#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
// #include "driver/uart.h"  
//user
#include "main.h"
#include "ble_server.h"
#include "sh1106.h"
#include "encoder.h"
#include "uart.h"
#include "hall_sensor.h"
#include "timer_utils.h"
#include "bike_metrics.h"

#include "tasks.h"


// #include "display_task.h"
// #include "encoder_task.h"
// #include "hall_task.h"

static const char *TAG = "APP";   //

//timer
#define HALL_TIMER_GROUP  TIMER_GROUP_0
#define HALL_TIMER_IDX    TIMER_0

#define ENCODER_PIN_A   GPIO_NUM_7
#define ENCODER_PIN_B   GPIO_NUM_3
#define ENCODER_BTN     GPIO_NUM_0

/* ---------------- konfiguracja --------------- */
#define DISP_Q_LEN      12
#define FPS_LIMIT_MS    100     /* ~10 FPS  */

/* ---------------- zmienne wewn. -------------- */
static QueueHandle_t s_disp_q = NULL;
static bool          s_is_running = false;
static float         s_radius_in  = 12.0f;

#ifndef HALL_GPIO
#define HALL_GPIO   GPIO_NUM_6      /* dowolny pin zewnętrzny – dostosuj do HW */
#endif

/* …include'y… */
#define FPS_MS      100
#define Q_LEN       12

static QueueHandle_t disp_q;
static bool   s_running = false;
static float  s_radius  = 12.0f;
static int8_t s_menu    = 0;

/* metryki */
static disp_metrics_t s_m = {0};

static const char *item_txt[] = { "Start / Stop", "Reset", "Set radius" };

QueueHandle_t display_get_queue(void)   /* <-- GLOBALNY symbol */
{
    return s_disp_q;
}

static void draw_screen(void)
{
    char buf[192];
    size_t off = 0;

    /* menu */
    for (int i = 0; i < 3; ++i)
        off += snprintf(buf+off,sizeof(buf)-off,"%c %s\n",
                        i==s_menu?'>':' ', item_txt[i]);

    /* status + promień */
    off += snprintf(buf+off,sizeof(buf)-off,"[%s]  R=%.1f in\n\n",
                    s_running?"RUN":"STOP", s_radius);

    /* metryki zawsze pod spodem */
    snprintf(buf+off,sizeof(buf)-off,
             "v=%.1f km/h  avg=%.1f\nDist: %.2f km\n",
             s_m.speed_kmh, s_m.avg_kmh, s_m.dist_km);

    task_sh1106_display_clear(NULL);
    task_sh1106_display_text(buf);
}

void display_task(void *pv)
{
    s_disp_q = xQueueCreate(DISP_Q_LEN, sizeof(disp_msg_t));
    if (!s_disp_q) { ESP_LOGE(TAG, "queue alloc"); vTaskDelete(NULL); }
    TickType_t last = 0;
    bool dirty = true;
    disp_msg_t msg;

    for(;;) {
        if (xQueueReceive(s_disp_q,&msg,pdMS_TO_TICKS(50))) {
            switch(msg.type) {
            case DISP_MENU_ROTATE:
                s_menu = (s_menu + msg.d.dir + 3) % 3;
                dirty = true; break;

            case DISP_MENU_BUTTON:
                if      (s_menu==0) s_running = !s_running;
                else if (s_menu==1) s_m = (disp_metrics_t){0};
                else                { s_radius += 0.5f; if(s_radius>30) s_radius=10; }
                dirty = true; break;

            case DISP_METRICS_UPDATE:
                s_m = msg.d.m;
                dirty = true;       /* zawsze odświeżamy */
                break;
            }
        }
        TickType_t now = xTaskGetTickCount();
        if (dirty && now-last>=pdMS_TO_TICKS(FPS_MS)) {
            draw_screen();
            dirty = false; last = now;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}



void encoder_task(void *pv)
{
    /* kolejki */
    QueueHandle_t enc_q  = encoder_get_event_queue();
    QueueHandle_t disp_q = display_get_queue();

    if (!enc_q || !disp_q) {
        vTaskDelete(NULL);
        return;
    }

    encoder_event_t evt;
    disp_msg_t     msg;

    for(;;) {
        if (!xQueueReceive(enc_q,&evt,portMAX_DELAY)) continue;

        msg.type = (evt.type==ENCODER_EVENT_ROTATE)?
                   DISP_MENU_ROTATE : DISP_MENU_BUTTON;
        msg.d.dir = evt.direction;
        xQueueSend(disp_q,&msg,portMAX_DELAY);
    }
}

/*************************
 *  Kolejka impulsów (ISR)
 *************************/
static QueueHandle_t s_pulse_q = NULL;    /* element = uint64_t dt_us */

QueueHandle_t hall_get_queue(void)
{
    return s_pulse_q;
}

/********************
 *  ISR czujnika HALLa
 *******************/

static TaskHandle_t s_task_handle = NULL;

static void IRAM_ATTR hall_isr(void *arg)
{
    static uint64_t s_last_us = 0;
    const uint64_t now_us = timer_utils_get_us(HALL_TIMER_GROUP, HALL_TIMER_IDX);

    if (s_last_us == 0) {
        s_last_us = now_us;      /* pierwszy impuls – brak pomiaru */
        return;
    }

    const uint64_t dt = now_us - s_last_us;
    s_last_us       = now_us;

    /* Prześlij do zadania w kontekście przerwania */
    if (s_pulse_q) {
        xQueueSendFromISR(s_pulse_q, &dt, NULL);
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_task_handle) {
        vTaskNotifyGiveFromISR(s_task_handle, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/********************
 *  Zadanie główne
 *******************/
void hall_task(void *pv)
{
    /*******************
     * Inicjalizacja HW
     ******************/
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << HALL_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE      /* magnes opuszcza czujnik – dowolna krawędź */
    };
    gpio_config(&io);

    /* Kolejka impulsów */
    s_pulse_q = xQueueCreate(32, sizeof(uint64_t));
    if (!s_pulse_q) {
        vTaskDelete(NULL);
        return;
    }

    /* ISR */
    gpio_install_isr_service(0);
    gpio_isr_handler_add(HALL_GPIO, hall_isr, NULL);

    /****************************
     *  Obiekty pomocnicze / kolejki
     ***************************/
    QueueHandle_t disp_q = display_get_queue();
    if (!disp_q) {
        vTaskDelete(NULL);
        return;
    }

    /***********************
     *  Dane jazdy / metryki
     ************************/
    bike_metrics_t metrics;
    // float diameter_in = menu_get_radius_inch() * 2.0f;
    bike_metrics_init(&metrics, 27.0f);

    for (;;) {
        uint64_t dt_us;
        if (xQueueReceive(s_pulse_q, &dt_us, portMAX_DELAY)) {
            if (!s_is_running) {
                /* Jeżeli licznik zatrzymany – ignoruj impulsy, ale zeruj referencję czasu
                   tak aby pierwszy impuls po wznowieniu nie dawał dt = duża wartość. */
                continue;
            }

            float v_inst_kmh = bike_metrics_update(&metrics, dt_us);

            disp_msg_t msg = {
                .type              = DISP_METRICS_UPDATE,
                .d.m.speed_kmh = v_inst_kmh,
                .d.m.avg_kmh   = bike_metrics_get_avg_kmh(&metrics),
                .d.m.dist_km = bike_metrics_get_distance_km(&metrics)
            };
            xQueueSend(disp_q, &msg, 0);
        }
    }
}


//OLED MENU
void app_main(void) {

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

    static bike_metrics_t metrics;
    bike_metrics_init(&metrics, 2.0f * 12.0f);

    xTaskCreate(display_task, "display", 4096, NULL, 5, NULL);
    xTaskCreate(encoder_task,  "encoder", 2048, NULL, 8, NULL);
    xTaskCreate(hall_task, "hall", 3072, NULL, 10, NULL);
}
