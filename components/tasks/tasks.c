/*  Implementacja „szkieletu” */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "sh1106.h"
#include "encoder.h"

#include "tasks.h"     /* nasze API */

/* nazwa modułu do logów */
static const char *TAG = "TASKS";

/* ------- prywatne kolejki ------- */
static QueueHandle_t q_display = NULL;
static QueueHandle_t q_encoder = NULL;
static QueueHandle_t q_hall    = NULL;
static QueueHandle_t q_ble     = NULL;

/* ------- gettery ------- */
QueueHandle_t tasks_get_display_q(void) { return q_display; }
QueueHandle_t tasks_get_encoder_q(void) { return q_encoder; }
QueueHandle_t tasks_get_hall_q   (void) { return q_hall;    }
QueueHandle_t tasks_get_ble_q    (void) { return q_ble;     }

/* =====================================================
 *  MOCK-TASKI  – pętle nieskończone z prostym logiem
 *  ===================================================*/
/* ===================  konfiguracja HW  ======================= */
#define ENCODER_PIN_A   GPIO_NUM_7
#define ENCODER_PIN_B   GPIO_NUM_3
#define ENCODER_BTN     GPIO_NUM_0

#define DISP_Q_LEN      12         /* elementów w kolejce */
#define FPS_LIMIT_MS    100        /* ~10 FPS odświeżania */

/* ===================  zmienne wspólne  ======================= */
static QueueHandle_t s_disp_q = NULL;
QueueHandle_t tasks_get_disp_q(void) { return s_disp_q; }

/* ========  stan menu + danych jazdy (w zadaniu display)  ===== */
typedef enum { MENU_START_STOP, MENU_RESET, MENU_SET_RADIUS, MENU_COUNT } menu_item_t;

/* ============================================================= */
/* ======================  DISPLAY TASK  ======================= */
/* ============================================================= */

static void draw_screen(menu_item_t idx, bool running,
                        float radius_in, float spd, float avg, float dist)
{
    char buf[128];
    size_t off = 0;
    static const char *items[MENU_COUNT] = {
        "Start / Stop", "Reset", "Set radius"
    };

    for (int i = 0; i < MENU_COUNT; ++i) {
        off += snprintf(buf+off, sizeof(buf)-off, "%c %s\n",
                        (i==idx ? '>' : ' '), items[i]);
    }
    off += snprintf(buf+off, sizeof(buf)-off,
                    "[%s]  R=%.1f\"\n", running ? "RUN" : "STOP", radius_in);

    snprintf(buf+off, sizeof(buf)-off,
             "S%.1f  A%.1f  D%.2f\n", spd, avg, dist);

    task_sh1106_display_clear(NULL);
    task_sh1106_display_text((const void*)buf);
}

static void display_task(void *pv)
{
    /* --- stan wewnętrzny ------------------- */
    menu_item_t menu_idx   = MENU_START_STOP;
    bool        running    = false;
    float       radius_in  = 12.0f;
    float       spd=0, avg=0, dist=0;

    display_msg_t evt;
    TickType_t last = 0;
    bool dirty = true;

    for (;;)
    {
        /* karm watchdog maks co 50 ms */
        if (xQueueReceive(s_disp_q, &evt, pdMS_TO_TICKS(50))) {
            switch (evt.type) {
                case DISP_MENU_ROTATE:
                    menu_idx = (menu_item_t)(((int)menu_idx + evt.d.dir + MENU_COUNT)%MENU_COUNT);
                    dirty = true;
                    break;

                case DISP_MENU_BUTTON:
                    switch (menu_idx) {
                        case MENU_START_STOP: running = !running;                 break;
                        case MENU_RESET:      
                            spd = avg = dist = 0.0f;
                            rad_in = 12.0f;  
                            break;
                        case MENU_SET_RADIUS:
                            radius_in += 0.5f;
                            if (radius_in > 30.0f) radius_in = 10.0f;
                            break;
                        default: break;
                    }
                    dirty = true;
                    break;

                case DISP_METRICS_UPDATE:
                    if (running) {
                        spd  = evt.d.metrics.speed_kmh;
                        avg  = evt.d.metrics.avg_kmh;
                        dist = evt.d.metrics.dist_km;
                        dirty = true;
                    }
                    break;

                default: break;
            }
        }

        TickType_t now = xTaskGetTickCount();
        if (dirty && (now-last) >= pdMS_TO_TICKS(FPS_LIMIT_MS)) {
            draw_screen(menu_idx, running, radius_in, spd, avg, dist);
            dirty = false;
            last  = now;
        }

        // vTaskDelay(pdMS_TO_TICKS(5));
        vTaskDelay(1);
    }
}

/* ============================================================= */
/* ======================  ENCODER TASK  ======================= */
/* ============================================================= */

static void encoder_task(void *pv)
{
    QueueHandle_t enc_q = encoder_get_event_queue();

    if (!enc_q || !s_disp_q) {
        ESP_LOGE("encoder", "queues missing!");
        vTaskDelete(NULL);
        return;
    }

    encoder_event_t enc;
    display_msg_t      evt;

    for (;;) {
        if (xQueueReceive(enc_q, &enc, portMAX_DELAY)) {
            switch (enc.type) {
                case ENCODER_EVENT_ROTATE:
                    evt.type  = DISP_MENU_ROTATE;
                    evt.d.dir = (int8_t)enc.direction;
                    xQueueSend(s_disp_q, &evt, 0);
                    break;

                case ENCODER_EVENT_BUTTON:
                    evt.type = DISP_MENU_BUTTON;
                    xQueueSend(s_disp_q, &evt, 0);
                    break;

                default: break;
            }
        }
    }
}

/* ============================================================= */
/* ============  PUBLIC: init + start zadań  ==================== */
/* ============================================================= */

// static void encoder_task(void *pv)
// {
//     encoder_msg_t evt;
//     for (;;) {
//         if (xQueueReceive(q_encoder, &evt, portMAX_DELAY)) {
//             /* TODO:  tutaj logika dekodowania menu itp.       */
//         }
//     }
// }

// static void hall_task(void *pv)
// {
//     hall_msg_t pulse;
//     for (;;) {
//         if (xQueueReceive(q_hall, &pulse, portMAX_DELAY)) {
//             /* TODO:  obliczenia prędkości / dystansu          */
//         }
//     }
// }

// static void ble_task(void *pv)
// {
//     /* np. czekanie na notyfikacje i wysyłanie metryk */
//     for (;;) {
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }

/* =====================================================
 *                PUBLIC  API
 * ===================================================*/
void tasks_init(void)
{
    s_disp_q = xQueueCreate(DISP_Q_LEN, sizeof(display_msg_t));

    /* 2. sprzęt: OLED + enkoder */
    i2c_master_init();
    sh1106_init();
    task_sh1106_display_clear(NULL);

    encoder_config_t cfg = {
        .pin_a = ENCODER_PIN_A,
        .pin_b = ENCODER_PIN_B,
        .pin_btn = ENCODER_BTN,
        .flip_direction = false
    };
    encoder_init(&cfg);

    /* Wypisz do logu: */
    ESP_LOGI(TAG, "queues ready");
}

void tasks_start(void)
{   
    if (!s_disp_q) { ESP_LOGE("tasks", "call tasks_init() first!"); return; }
    /* Kolejność / priorytety możesz zmienić później */
    xTaskCreate(display_task, "display", 4096, NULL, 5, NULL);
    xTaskCreate(encoder_task, "encoder", 2048, NULL, 4, NULL);
    // xTaskCreatePinnedToCore(display_task, "display", 4096, NULL, 4, NULL, 0);
    // xTaskCreatePinnedToCore(encoder_task, "encoder", 2048, NULL, 5, NULL, 0);
    // xTaskCreatePinnedToCore(hall_task,    "hall",    3072, NULL, 5, NULL, 0);
    // xTaskCreatePinnedToCore(ble_task,     "ble",     4096, NULL, 3, NULL, 0);

    ESP_LOGI(TAG, "tasks started");
}
