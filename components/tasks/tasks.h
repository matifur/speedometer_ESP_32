#pragma once
/*  Wspólny nagłówek wszystkich zadań modułu „tasks” */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- wiadomości od ENCDERA ---------- */
typedef enum {
    ENC_EVT_ROTATE,
    ENC_EVT_BUTTON
} encoder_evt_t;

typedef struct {
    encoder_evt_t type;
    int8_t        delta;      /* +1 / –1  dla ROTATE, 0 dla BUTTON */
} encoder_msg_t;

/* ---------- wiadomości do / z  WYŚWIETLACZA ---------- */
typedef enum {
    DISP_MENU_ROTATE,
    DISP_MENU_BUTTON,
    DISP_METRICS_UPDATE
} display_msg_type_t;

typedef struct {
    display_msg_type_t type;
    union {
        int8_t dir;           /* MENU_ROTATE / BUTTON            */
        struct {
            float speed_kmh;
            float avg_kmh;
            float dist_km;
        } metrics;            /* METRICS_UPDATE                  */
    } d;
} display_msg_t;

/* ---------- wiadomości z  CZUJNIKA HALLa ---------- */
typedef struct {
    uint64_t pulse_dt_us;     /* odstęp czasu między impulsami    */
} hall_msg_t;

/* ---------- API widoczne dla reszty projektu -------- */
void tasks_init(void);   /* tworzy wszystkie kolejki           */
void tasks_start(void);  /* uruchamia wszystkie xTaskCreate    */

/* gettery kolejki – bez ujawniania zmiennych globalnych */
QueueHandle_t tasks_get_display_q(void);
QueueHandle_t tasks_get_encoder_q(void);
QueueHandle_t tasks_get_hall_q(void);
QueueHandle_t tasks_get_ble_q(void);

#ifdef __cplusplus
}
#endif
