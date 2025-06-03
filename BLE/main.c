#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ble_server.h"       // nasze API

static void sensor_task(void *arg)
{
    float speed_kmh = 10.0f;      // startowa prędkość
    while (1) {
        notify_speed(speed_kmh);  // wyślij przez BLE

        // ***** SYMULACJA ***** – zmieniaj prędkość sinusoidalnie
        speed_kmh += 1.5f;
        if (speed_kmh > 35.0f) speed_kmh = 10.0f;

        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 Hz
    }
}

void app_main(void)
{
    ble_server_init();             // uruchom BLE (def. w ble_server.c)

    xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL);
}
