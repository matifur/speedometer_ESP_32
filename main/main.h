#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 *  Protokół komunikacji pomiędzy zadaniami – wiadomości przesyłane w kolejce
 * --------------------------------------------------------------------------*/

typedef enum {
    DISP_MENU_ROTATE = 0,   /* obrót enkodera: d.dir = ±1           */
    DISP_MENU_BUTTON,       /* przycisk enkodera: d.idx (opcjonalnie)*/
    // DISP_METRICS,           /* ogólna aktualizacja metryk            */
    DISP_METRICS_UPDATE     /* szczegółowa aktualizacja z hall_task  */
} disp_msg_type_t;

/* ładunek wiadomości – różne pola w zależności od typu */
typedef struct {
    float speed_kmh;
    float avg_kmh;
    float dist_km;
} disp_metrics_t;

typedef union {
    int8_t         dir;   /* do DISP_MENU_ROTATE */
    // int            idx;   /* do DISP_MENU_BUTTON */
    disp_metrics_t m;     /* do metryk           */
} disp_payload_t;

/* element kolejki */
typedef struct {
    disp_msg_type_t type;
    disp_payload_t  d;
} disp_msg_t;

/* --------------------------------------------------------------------------
 *  Deklaracje zadań (definicje znajdują się w main.c)
 * --------------------------------------------------------------------------*/
void display_task(void *pv);
void encoder_task(void *pv);
void hall_task(void *pv);

/* --------------------------------------------------------------------------
 *  Funkcje pomocnicze z menu_task.c używane w main.c / hall_task.c
 * --------------------------------------------------------------------------*/

QueueHandle_t display_get_queue(void);
QueueHandle_t hall_get_queue(void);


bool  menu_is_running(void);
float menu_get_radius_inch(void);

#ifdef __cplusplus
}
#endif

#endif