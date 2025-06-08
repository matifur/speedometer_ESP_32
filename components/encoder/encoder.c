#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "encoder.h"

#define ENCODER_MAX_POSITIONS 80
#define PULSES_PER_STEP 4  // 4 pulses per step (A/B quadrature encoding)
#define DEBOUNCE_US 20000

static encoder_config_t s_cfg;
static volatile int32_t s_position = 0;
static volatile uint8_t s_last_state = 0;
static QueueHandle_t s_event_queue;
static portMUX_TYPE s_spinlock = portMUX_INITIALIZER_UNLOCKED;

static const int8_t quad_table[16] = {
     0,  1, -1,  0,
    -1,  0,  0,  1,
     1,  0,  0, -1,
     0, -1,  1,  0
};

static void IRAM_ATTR encoder_isr(void *arg) {
    uint32_t gpio = (uint32_t)arg;

    // rotation
    if (gpio == s_cfg.pin_a || gpio == s_cfg.pin_b) {
        uint8_t state = (gpio_get_level(s_cfg.pin_a) << 1) | gpio_get_level(s_cfg.pin_b);
        uint8_t idx = (s_last_state << 2) | state;
        int8_t delta = quad_table[idx];
        s_last_state = state;

        if (delta != 0) {
            if (s_cfg.flip_direction) delta = -delta;

            portENTER_CRITICAL_ISR(&s_spinlock);
            s_position = (s_position + delta + ENCODER_MAX_POSITIONS) % ENCODER_MAX_POSITIONS;
            int32_t pos = s_position;
            portEXIT_CRITICAL_ISR(&s_spinlock);

            // encoder_event_t evt = {
            //     .type = ENCODER_EVENT_ROTATE,
            //     .position = pos,
            //     .direction = delta
            // };
            // xQueueSendFromISR(s_event_queue, &evt, NULL);
            if (pos % PULSES_PER_STEP == 0) {
                encoder_event_t evt = {
                .type = ENCODER_EVENT_ROTATE,
                .position = pos / PULSES_PER_STEP,  // map to 0–19
                .direction = delta > 0 ? 1 : -1
                };
                xQueueSendFromISR(s_event_queue, &evt, NULL);
            }
        }
    }

    // button
    if (s_cfg.pin_btn != GPIO_NUM_NC && gpio == s_cfg.pin_btn) {
        static uint32_t last_us = 0;
        uint32_t now = esp_timer_get_time();
        if (now - last_us > DEBOUNCE_US) {
            last_us = now;

            portENTER_CRITICAL_ISR(&s_spinlock);
            int32_t pos = s_position;
            portEXIT_CRITICAL_ISR(&s_spinlock);

            encoder_event_t evt = {
                .type = ENCODER_EVENT_BUTTON,
                .position = pos / PULSES_PER_STEP,
                .direction = 0
            };
            xQueueSendFromISR(s_event_queue, &evt, NULL);
        }
    }
}

void encoder_init(const encoder_config_t *config) {
    s_cfg = *config;

    gpio_config_t io = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config_t btn = io;
    btn.intr_type = GPIO_INTR_NEGEDGE;

    gpio_config_t inputs[] = { io, io };
    inputs[0].pin_bit_mask = 1ULL << s_cfg.pin_a;
    inputs[1].pin_bit_mask = 1ULL << s_cfg.pin_b;
    // inputs[0].intr_type = GPIO_INTR_NEGEDGE;
    // inputs[1].intr_type = GPIO_INTR_NEGEDGE;

    gpio_config(&inputs[0]);
    gpio_config(&inputs[1]);

    if (s_cfg.pin_btn != GPIO_NUM_NC) {
        btn.pin_bit_mask = 1ULL << s_cfg.pin_btn;
        gpio_config(&btn);
    }

    gpio_install_isr_service(0);
    gpio_isr_handler_add(s_cfg.pin_a, encoder_isr, (void*)s_cfg.pin_a);
    gpio_isr_handler_add(s_cfg.pin_b, encoder_isr, (void*)s_cfg.pin_b);
    if (s_cfg.pin_btn != GPIO_NUM_NC) {
        gpio_isr_handler_add(s_cfg.pin_btn, encoder_isr, (void*)s_cfg.pin_btn);
    }

    s_last_state = (gpio_get_level(s_cfg.pin_a) << 1) | gpio_get_level(s_cfg.pin_b);
    s_event_queue = xQueueCreate(10, sizeof(encoder_event_t));
    if (!s_event_queue) {
        uart_printf("ERROR: Event queue not created!\r\n");
    }
    else{
        uart_printf("Encoder initialized: A=%d, B=%d, BTN=%d\r\n",
                    s_cfg.pin_a, s_cfg.pin_b, s_cfg.pin_btn);
    }
}

QueueHandle_t encoder_get_event_queue(void) {
    return s_event_queue;
}
