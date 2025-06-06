#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ble_server.h"

static void sensor_task(void *arg)
{
    float v = 10.0f;      /* km/h */
    float sum = 0.0f;
    float dist = 0.0f;    /* km   */
    uint32_t n = 0;

    while (1) {
        notify_speed(v);

        dist += v / 3600.0f;     /* +kilometr = v(km/h) * 1 s */
        sum  += v;
        n++;

        notify_distance(dist);
        notify_avg_speed(sum / n);

        /* prosta symulacja */
        v += 1.5f;
        if (v > 35.0f) v = 10.0f;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ble_server_init();
    xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL);
}
