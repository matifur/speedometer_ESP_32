#include "hall_sensor.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uart.h" // For uart_printf, if you have a custom UART implementation
#include "timer_utils.h"
#include "bike_metrics.h"

#define HALL_TIMER_GROUP  TIMER_GROUP_0
#define HALL_TIMER_IDX    TIMER_0
static const char *TAG = "hall_sensor";

static bike_metrics_t s_bike; 
/*
 * Adjust this constant to match the GPIO connected to UGN3144 output.
 */
static const gpio_num_t HALL_SENSOR_GPIO = GPIO_NUM_6;

// static const gpio_num_t HALL_INDICATOR_LED   = GPIO_NUM_8;  /* On‑board LED */

static volatile bool s_led_state = false;
static volatile uint32_t s_pulse_count = 0;

/* ----------- Sekcja zmiennych ----------- */
static volatile uint64_t s_prev_time_us   = 0;   /* poprzedni timestamp */
static volatile uint64_t s_interval_us    = 0;   /* Δt aktualne        */

static TaskHandle_t s_task_handle = NULL;

static void IRAM_ATTR hall_isr_handler(void *arg)
{   
    uint64_t now = timer_utils_get_us(HALL_TIMER_GROUP, HALL_TIMER_IDX);
    if (s_prev_time_us != 0) {                 // pomijamy pierwszy impuls
        s_interval_us = now - s_prev_time_us;  // Δt = różnica
    }
    s_prev_time_us = now; 

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_task_handle) {
        vTaskNotifyGiveFromISR(s_task_handle, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
esp_err_t hall_sensor_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask  = 1ULL << HALL_SENSOR_GPIO,
        .mode          = GPIO_MODE_INPUT,
        .pull_up_en    = GPIO_PULLUP_ENABLE,
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,
        .intr_type     = GPIO_INTR_NEGEDGE /* UGN3144 pulls low when magnet approaches */
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    /* Install ISR service (ignore error if already installed by other code) */
    (void)gpio_install_isr_service(0);
    ESP_ERROR_CHECK(gpio_isr_handler_add(HALL_SENSOR_GPIO, hall_isr_handler, NULL));

    ESP_ERROR_CHECK(timer_utils_init_us(HALL_TIMER_GROUP, HALL_TIMER_IDX));
    return ESP_OK;
}

uint32_t hall_sensor_get_pulse_count(void)
{
    return s_pulse_count;
}

void hall_sensor_reset_pulse_count(void)
{
    s_pulse_count = 0;
}

uint64_t hall_sensor_get_last_dt_us(void)
{
    return s_interval_us;               /* można używać w innych modułach */
}

void hall_sensor_task(void *pvParameter)
{
    // const uint32_t flash_ms = (uint32_t)pvParameter ?: 50;  /* długość błysku */

    s_task_handle = xTaskGetCurrentTaskHandle();
    // static double speed = 0.0;
    // static double average_speed = 0.0;
    // static double distance = 0.0;
    bike_metrics_init(&s_bike, /*średnica w calach → podamy z app_main*/ 27.0f);
    for (;;) {
        /* Blokuj do czasu notyfikacji z ISR */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* Pierwszy impuls po starcie ma Δt==0 – pomijamy */
        uint64_t dt = s_interval_us;
        uart_printf("duration = %llu us\r\n", (unsigned long long)dt);
        float v_kmh = bike_metrics_update(&s_bike, dt);
        float v_avg = bike_metrics_get_avg_kmh(&s_bike);
        float dist  = bike_metrics_get_distance_km(&s_bike);

        uart_printf("v=%.2f km/h | avg=%.2f | dist=%.3f km\r\n", v_kmh, v_avg, dist);    
        // if (dt == 0) {
        //     continue;
        // }

        /* ───── LED flash (opcjonalny) ───── */
        // gpio_set_level(LED_GPIO, 1);
        // vTaskDelay(pdMS_TO_TICKS(flash_ms));
        // gpio_set_level(LED_GPIO, 0);

        /* ───── Log / dalsze obliczenia ──── */
        // uart_printf("duration1 = %llu us\r\n", (unsigned long long)dt);
        /* tu możesz policzyć prędkość, zapisać do kolejki itp. */
    }
}