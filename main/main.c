#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sh1106.h"

// --- Definicje pinów ---
#define MAG_SENSOR_PIN    GPIO_NUM_2
#define ENCODER_A_PIN     GPIO_NUM_3
#define ENCODER_B_PIN     GPIO_NUM_4
#define ENCODER_BTN_PIN   GPIO_NUM_5

// --- Zmienne enkodera ---
volatile int encoder_dir = 0; // -1 = lewo, 1 = prawo, 0 = brak ruchu
volatile int encoder_btn = 1;

// --- ISR enkodera ---
static void IRAM_ATTR encoder_isr_handler(void* arg)
{
    int a = gpio_get_level(ENCODER_A_PIN);
    int b = gpio_get_level(ENCODER_B_PIN);

    if (a != b)
        encoder_dir = 1; // prawo
    else
        encoder_dir = -1; // lewo
}

// --- ISR przycisku ---
static void IRAM_ATTR button_isr_handler(void* arg)
{
    encoder_btn = gpio_get_level(ENCODER_BTN_PIN);
}

// --- Inicjalizacja GPIO ---
void init_gpio()
{
    gpio_set_direction(MAG_SENSOR_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(ENCODER_A_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(ENCODER_B_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(ENCODER_BTN_PIN, GPIO_MODE_INPUT);

    gpio_set_pull_mode(ENCODER_A_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(ENCODER_B_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(ENCODER_BTN_PIN, GPIO_PULLUP_ONLY);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(ENCODER_A_PIN, encoder_isr_handler, NULL);
    gpio_isr_handler_add(ENCODER_BTN_PIN, button_isr_handler, NULL);
}

void app_main(void)
{
    // Inicjalizacja
    init_gpio();
    i2c_master_init();   // z biblioteki SH1106
    sh1106_init();

    char line1[20];
    char line2[20];

    while (1)
    {
        int magnet = gpio_get_level(MAG_SENSOR_PIN);
        int btn = gpio_get_level(ENCODER_BTN_PIN);
        char* dir = (encoder_dir == 1) ? "P" : (encoder_dir == -1) ? "L" : "-";

        snprintf(line1, sizeof(line1), "MAG: %d  BTN: %d", magnet, btn);
        snprintf(line2, sizeof(line2), "ENC: %s", dir);

        sh1106_clear_screen();
        sh1106_display_text(line1, 0);
        sh1106_display_text(line2, 1);
        vTaskDelay(pdMS_TO_TICKS(200));

        encoder_dir = 0; // reset kierunku
    }
}
