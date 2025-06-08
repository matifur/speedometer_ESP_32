/* =========================================================
 *  main.c  –  punkt wejścia aplikacji
 * =======================================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ESP-IDF */
#include "esp_log.h"
#include "nvs_flash.h"

/* Sprzęt / sterowniki */
#include "driver/gpio.h"
#include "encoder.h"
#include "sh1106.h"
#include "ble_server.h"
#include "encoder.h"
#include "hall_sensor.h"
#include "timer_utils.h"
#include "bike_metrics.h"
#include "uart.h"

/* Wspólne kolejki + tasks */
#include "tasks.h"

/* tag do logów */
static const char *TAG = "MAIN";

/* ------------ konfiguracja HW ----------------- */
//timer
#define HALL_TIMER_GROUP  TIMER_GROUP_0
#define HALL_TIMER_IDX    TIMER_0


#define ENCODER_PIN_A   GPIO_NUM_7
#define ENCODER_PIN_B   GPIO_NUM_3
#define ENCODER_BTN     GPIO_NUM_0

#define HALL_GPIO       GPIO_NUM_6    /* czujnik HALLa */

/* =========================================================
 *  Inicjalizacja peryferiów
 * =======================================================*/
static void hw_init(void)
{
    /* 1. NVS (wymagane przez BLE) */
    ESP_ERROR_CHECK(nvs_flash_init());

    /* 2. I²C + OLED SH1106 */
    i2c_master_init();
    sh1106_init();
    task_sh1106_display_clear(NULL);

    /* 3. Enkoder – ISR i własna kolejka wewnątrz drivera */
    encoder_config_t cfg = {
        .pin_a          = ENCODER_PIN_A,
        .pin_b          = ENCODER_PIN_B,
        .pin_btn        = ENCODER_BTN,
        .flip_direction = false         /* ↺ / ↻ zgodnie z HW */
    };
    encoder_init(&cfg);

    /* 4. Czujnik HALLa – tylko GPIO + ISR
     *    (sam ISR kolejkuje pulse_dt_us do tasks_get_hall_q()) */
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << HALL_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    /* 5. BLE  (jeżeli nie używasz – usuń) */
    ble_server_init();

    ESP_LOGI(TAG, "hardware initialised");
}

/* =========================================================
 *                 app_main()
 * =======================================================*/
void app_main(void)
{
    /* Sprzęt */
    hw_init();

    /* Kolejki */
    tasks_init();

    /* Wystartuj zadania */
    tasks_start();

    /* -------- main loop --------
     * Nie potrzebujemy tu nic robić – całe życie aplikacji
     * toczy się w zadaniach; gdyby trzeba było dodać tryb
     * uśpienia itp. można to zrobić tutaj. */
    for (;;)
    {
        /* watchdog „żyje” dzięki innym zadaniom;
           tu wystarczy długo spać.                */
        vTaskDelay(pdMS_TO_TICKS(10 * 1000));
    }
}
