#include "servo.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "common.h"

static const char *TAG = "SERVO";

static const int max_angleX = 180;
static const int max_angleY = 90;

void set_servo(int pin, int angle, int max_angle)
{
    if (angle < 0) angle = 0;
    if (angle > max_angle) angle = max_angle;

    int duty_us = 500 + (angle * (2500 - 500) / max_angle);
    int channel = (pin == SERVO_PIN_1 ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1);

    uint32_t duty = (duty_us * (1 << 16)) / 20000; // 20ms period (50Hz)
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, channel);
}

// Initialize PWM for servos
void init_servo_pwm()
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_16_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 50,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel1 = {
        .gpio_num       = SERVO_PIN_1,
        .speed_mode     = LEDC_HIGH_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER_0,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel1);

    ledc_channel_config_t ledc_channel2 = {
        .gpio_num       = SERVO_PIN_2,
        .speed_mode     = LEDC_HIGH_SPEED_MODE,
        .channel        = LEDC_CHANNEL_1,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER_0,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel2);

   // Set initial positions
    set_servo(SERVO_PIN_1, current_angleX, max_angleX);
    set_servo(SERVO_PIN_2, current_angleY, max_angleY);
}

void servo_task(void *pvParameters)
{
    servo_cmd_t cmd;
    ESP_LOGI(TAG, "Servo task started");

    while (1) {
        if (xQueueReceive(servoQueue, &cmd, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Received servo angles from WPF: %d, %d", cmd.angleX, cmd.angleY);

            set_servo(SERVO_PIN_1, cmd.angleX, max_angleX);
            current_angleX = cmd.angleX;
            set_servo(SERVO_PIN_2, cmd.angleY, max_angleY);
            current_angleY = cmd.angleY;

            ESP_LOGI(TAG, "Applied servo angles: %d, %d", current_angleX, current_angleY);
        }
    }
}

