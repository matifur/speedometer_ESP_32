#ifndef HALL_SENSOR_H
#define HALL_SENSOR_H

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise Hall‑effect sensor GPIO and ISR (pin defined in source).
 *
 * Call this once before starting the task via xTaskCreate().
 */
esp_err_t hall_sensor_init(void);

/**
 * @brief Get raw pulse count since last reset.
 */
uint32_t hall_sensor_get_pulse_count(void);

/**
 * @brief Reset pulse counter to zero.
 */
void hall_sensor_reset_pulse_count(void);

/**
 * @brief FreeRTOS task entry for Hall‑effect sampling.
 *
 * Pass the sampling window in milliseconds as the pvParameter when calling
 * xTaskCreate()/xTaskCreatePinnedToCore().
 *
 * Example:
 *     xTaskCreate(hall_sensor_task, "hall", 2048, (void*)1000, 5, NULL);
 */
void hall_sensor_task(void *pvParameter);

/**
 * @brief Get the last time difference in microseconds.
 *
 * This function returns the time difference between the last two pulses
 * detected by the Hall sensor.
 *
 * @return Time difference in microseconds.
 */
uint64_t hall_sensor_get_last_dt_us(void);


#ifdef __cplusplus
}
#endif

#endif // HALL_SENSOR_H