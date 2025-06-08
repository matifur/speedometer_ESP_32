#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float   wheel_circ_m;     /* obwód koła [m]             */
    float   distance_m;       /* dystans od startu [m]      */
    float   avg_kmh;          /* aktualna średnia [km/h]    */
} bike_metrics_t;

/**
 * @brief  Inicjalizacja (średnica w calach ").  Prędkość średnia = 0.
 */
void bike_metrics_init(bike_metrics_t *m, float diameter_in_inches);

/**
 * @brief  Aktualizacja po kolejnym impulsie.
 *
 * @param  dt_us   czas od poprzedniego impulsu [µs]
 * @return prędkość chwilowa [km/h]
 */
float bike_metrics_update(bike_metrics_t *m, uint64_t dt_us);

/* Gettery – przydatne w innych modułach */
float bike_metrics_get_distance_km(const bike_metrics_t *);
float bike_metrics_get_avg_kmh(const bike_metrics_t *);
void bike_metrics_reset(bike_metrics_t *);

#ifdef __cplusplus
}
#endif
