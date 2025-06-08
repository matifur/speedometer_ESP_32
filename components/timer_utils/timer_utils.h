#ifndef TIMER_UTILS_H
#define TIMER_UTILS_H

#include "driver/timer.h"   /* <- TUTAJ był brak! */
#include "esp_err.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Inicjalizuje licznik w wybranej grupie / kanale z rozdzielczością 1 µs.
 * @param  group   TIMER_GROUP_0 lub TIMER_GROUP_1
 * @param  idx     TIMER_0 lub TIMER_1
 * @return esp_err_t
 */
esp_err_t timer_utils_init_us(timer_group_t group, timer_idx_t idx);

/**
 * @brief  Odczytuje aktualną wartość licznika (µs) — *można* wołać w ISR.
 */
static inline uint64_t timer_utils_get_us(timer_group_t group, timer_idx_t idx)
{
    uint64_t val;
    timer_get_counter_value(group, idx, &val);
    return val;
}

/**
 * @brief  Zeruje licznik — *można* wołać w ISR.
 */
static inline void timer_utils_reset(timer_group_t group, timer_idx_t idx)
{
    timer_set_counter_value(group, idx, 0);
}

#ifdef __cplusplus
}
#endif

#endif // TIMER_UTILS_H