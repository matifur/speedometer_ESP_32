#include "timer_utils.h"
#include "esp_err.h"
#include "esp_log.h"

esp_err_t timer_utils_init_us(timer_group_t group, timer_idx_t idx)
{
    const char *TAG = "timer_utils";

    timer_config_t cfg = {
        .divider     = 80,            // 80 MHz / 80 = 1 MHz  → 1 tick = 1 µs
        .counter_dir = TIMER_COUNT_UP,
        .counter_en  = TIMER_PAUSE,
        .alarm_en    = TIMER_ALARM_DIS,
        .auto_reload = false,
    };
    ESP_ERROR_CHECK(timer_init(group, idx, &cfg));
    ESP_ERROR_CHECK(timer_set_counter_value(group, idx, 0));
    ESP_ERROR_CHECK(timer_start(group, idx));
    return ESP_OK;
}
