#include "bike_metrics.h"
#include <math.h>

#define INCH_TO_M  (0.0254f)
#define US_TO_S    (1e-6f)

void bike_metrics_init(bike_metrics_t *m, float diameter_in_inches)
{
    m->wheel_circ_m = (float)M_PI * diameter_in_inches * INCH_TO_M;
    m->distance_m   = 0.0f;
    m->avg_kmh      = 0.0f;
}

float bike_metrics_update(bike_metrics_t *m, uint64_t dt_us)
{
    if (dt_us == 0) return 0.0f;             /* pierwszy impuls – brak pomiaru */

    /* 1. prędkość chwilowa */
    float v_inst_kmh = (m->wheel_circ_m / (dt_us * US_TO_S)) * 3.6f;

    /* 2. średnia = (poprz. avg + v_inst) / 2  */
    m->avg_kmh = (m->avg_kmh + v_inst_kmh) * 0.5f;

    /* 3. dystans = dystans + obwód koła  */
    m->distance_m += m->wheel_circ_m;

    return v_inst_kmh;
}

float bike_metrics_get_avg_kmh(const bike_metrics_t *m)
{
    return m->avg_kmh;
}

float bike_metrics_get_distance_km(const bike_metrics_t *m)
{
    return m->distance_m / 1000.0f;
}

void bike_metrics_reset(bike_metrics_t *m)
{
    m->distance_m = 0.0f;
    m->avg_kmh    = 0.0f;
}
