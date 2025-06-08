#ifndef ENCODER_H_
#define ENCODER_H_

#include "driver/gpio.h"
#include <stdbool.h>
#include <stdint.h>
#include "uart.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ENCODER_EVENT_ROTATE,
    ENCODER_EVENT_BUTTON
} encoder_event_type_t;

typedef struct {
    encoder_event_type_t type;
    int32_t position;   // always in range 0–19
    int8_t direction;   // +1 (CW), -1 (CCW), 0 for button
} encoder_event_t;

typedef struct {
    gpio_num_t pin_a;
    gpio_num_t pin_b;
    gpio_num_t pin_btn;         // GPIO_NUM_NC if unused
    bool flip_direction;
} encoder_config_t;

void encoder_init(const encoder_config_t *config);
QueueHandle_t encoder_get_event_queue(void);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_H_ */