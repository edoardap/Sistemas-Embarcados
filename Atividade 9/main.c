#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "driver/i2c.h"

#include "esp_log.h"

#include "imu.h"

#define TAG "RTOS"

// ADC
#define POT_PIN ADC1_CHANNEL_0

// LED
#define LED_GPIO 2

// BUTTON
#define BUTTON_GPIO 4

// I2C
#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

QueueHandle_t adc_queue;
SemaphoreHandle_t button_semaphore;
SemaphoreHandle_t imu_mutex;

volatile int current_adc = 0;
volatile int current_mv = 0;
volatile int current_led_percent = 0;

bool hold_mode = false;

typedef struct {
    float x;
    float y;
    float z;
} imu_data_t;

imu_data_t imu_data;

// ================= ADC TASK =================
void potentiometer_task(void *pvParameters)
{
    while (1)
    {
        int adc_value = adc1_get_raw(POT_PIN);

        int pwm_value = adc_value * 2;

        current_adc = adc_value;

        // 0-4095 -> 0-3300mV
        current_mv = (adc_value * 3300) / 4095;

        xQueueSend(adc_queue, &pwm_value, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
// ================= LED TASK =================
void led_task(void *pvParameters)
{
    int pwm_value = 0;
    int last_pwm = 0;

    while (1)
    {
        if (xSemaphoreTake(button_semaphore, 0))
        {
            hold_mode = !hold_mode;
        }

        if (!hold_mode)
        {
            if (xQueueReceive(adc_queue, &pwm_value, pdMS_TO_TICKS(10)))
            {
                last_pwm = pwm_value;
            }
        }

        ledc_set_duty(
            LEDC_LOW_SPEED_MODE,
            LEDC_CHANNEL_0,
            last_pwm);

        ledc_update_duty(
            LEDC_LOW_SPEED_MODE,
            LEDC_CHANNEL_0);

        // 13 bits => 8191 max
        current_led_percent = (last_pwm * 100) / 8191;

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ================= BUTTON TASK =================
void button_task(void *pvParameters)
{
    int last_state = 1;

    while (1)
    {
        int state = gpio_get_level(BUTTON_GPIO);

        if (state == 0 && last_state == 1)
        {
            xSemaphoreGive(button_semaphore);
        }

        last_state = state;

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ================= IMU TASK =================
void imu_task(void *pvParameters)
{
    while (1)
    {
        float ax, ay, az;

        mpu6050_read_accel(&ax, &ay, &az);

        xSemaphoreTake(imu_mutex, portMAX_DELAY);

        imu_data.x = ax;
        imu_data.y = ay;
        imu_data.z = az;

        xSemaphoreGive(imu_mutex);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ================= CONSOLE TASK =================
void console_task(void *pvParameters)
{
    while (1)
    {
        float x, y, z;

        xSemaphoreTake(imu_mutex, portMAX_DELAY);

        x = imu_data.x;
        y = imu_data.y;
        z = imu_data.z;

        xSemaphoreGive(imu_mutex);

        printf("=====================================================\n");

        printf("STATUS: [%s] | POT: %d (%d mV) | LED: %d%%\n",
               hold_mode ? "HOLD" : "LIVE",
               current_adc,
               current_mv,
               current_led_percent);

        printf("IMU ACCEL (g): X: %.2f | Y: %.2f | Z: %.2f\n",
               x,
               y,
               z);

        printf("=====================================================\n\n");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
// ================= MAIN =================
void app_main(void)
{
    // ADC
    adc1_config_width(ADC_WIDTH_BIT_12);

    adc1_config_channel_atten(
        POT_PIN,
        ADC_ATTEN_DB_11);

    // BUTTON
    gpio_config_t btn_config = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };

    gpio_config(&btn_config);

    // PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK};

    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = LED_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0};

    ledc_channel_config(&ledc_channel);

    // I2C
    i2c_master_init();

    // MPU6050
    mpu6050_init();

    // Queue
    adc_queue = xQueueCreate(5, sizeof(int));

    // Semaphore
    button_semaphore = xSemaphoreCreateBinary();

    // Mutex
    imu_mutex = xSemaphoreCreateMutex();

    // TASKS
    xTaskCreate(potentiometer_task,
                "pot_task",
                2048,
                NULL,
                1,
                NULL);

    xTaskCreate(led_task,
                "led_task",
                2048,
                NULL,
                2,
                NULL);

    xTaskCreate(button_task,
                "button_task",
                2048,
                NULL,
                3,
                NULL);

    xTaskCreate(imu_task,
                "imu_task",
                4096,
                NULL,
                1,
                NULL);

    xTaskCreate(console_task,
                "console_task",
                4096,
                NULL,
                1,
                NULL);
}