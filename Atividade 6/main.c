#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h" // Biblioteca de filas citada nos slides
#include "driver/gpio.h"
#include "esp_timer.h"

#define LED_GPIO            GPIO_NUM_2
#define BUTTON_GPIO         GPIO_NUM_4
#define DEBOUNCE_TIME_MS    50
#define AUTO_OFF_TIME_MS    10000
#define FORCE_OFF_TIME_MS   2000

// Fila para enviar o tempo do evento da ISR para o Main
QueueHandle_t button_queue; 

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    static int64_t last_interrupt_time = 0;
    int64_t now = esp_timer_get_time() / 1000;

    if ((now - last_interrupt_time) > DEBOUNCE_TIME_MS) {
        // Em vez de mudar uma variável global, "postamos" o tempo na fila
        // xQueueSendFromISR é a versão específica para usar dentro de interrupções
        xQueueSendFromISR(button_queue, &now, NULL);
        last_interrupt_time = now;
    }
}

void app_main(void) {
    // Configurações de GPIO permanecem as mesmas
    gpio_config_t io_conf = {.pin_bit_mask = (1ULL << LED_GPIO), .mode = GPIO_MODE_OUTPUT};
    gpio_config(&io_conf);
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    gpio_config(&io_conf);

    // Criamos uma fila para até 10 mensagens do tipo int64_t
    button_queue = xQueueCreate(10, sizeof(int64_t));

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, NULL);

    bool led_state = false;
    int64_t led_on_time = 0;
    int64_t button_press_time = 0;
    int64_t event_time;
    bool long_press_detected = false;

    while (1) {
        int64_t now = esp_timer_get_time() / 1000;
        bool button_pressed = (gpio_get_level(BUTTON_GPIO) == 0);

        // xQueueReceive espera por uma mensagem da fila. 
        // Se não houver nada, ele segue em frente (timeout 0)
        if (xQueueReceive(button_queue, &event_time, 0)) {
            if (button_pressed) {
                button_press_time = event_time;
                long_press_detected = false;
            } else {
                if ((event_time - button_press_time) >= FORCE_OFF_TIME_MS) {
                    long_press_detected = true;
                } else if (!long_press_detected) {
                    led_state = true;
                    led_on_time = event_time;
                }
            }
        }

        // Lógica de controle do LED e tempo
        if (led_state) {
            gpio_set_level(LED_GPIO, 1);
            if ((now - led_on_time) >= AUTO_OFF_TIME_MS || 
               (button_pressed && (now - button_press_time) >= FORCE_OFF_TIME_MS)) {
                led_state = false;
                if (button_pressed) long_press_detected = true;
            }
        } else {
            gpio_set_level(LED_GPIO, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}