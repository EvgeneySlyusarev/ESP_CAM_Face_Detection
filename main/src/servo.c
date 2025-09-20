#include "servo.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "common.h"

static const char *TAG = "SERVO";

static const int max_angle1 = 180;
static const int max_angle2 = 90;

void set_servo(int pin, int angle, int max_angle)
{
    // Ensure bounds
    if (angle < 0) angle = 0;
    if (angle > max_angle) angle = max_angle;

    // Convert angle to pulse width (microseconds)
    int duty_us = 500 + (angle * (2500 - 500) / max_angle);
    int channel = (pin == SERVO_PIN_1 ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1);

    // Convert microseconds to duty for LEDC_TIMER_16_BIT and 20ms period
    uint32_t duty = (duty_us * (1 << 16)) / 20000;
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, channel);
}

void init_servo_pwm()
{
    // Configure timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_16_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 50,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Channel 1
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

    // Channel 2
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
    set_servo(SERVO_PIN_1, current_angle1, max_angle1);
    set_servo(SERVO_PIN_2, current_angle2, max_angle2);
}

/*
 * ServoTask:
 * Waits on servoQueue for servo_cmd_t items and applies them.
 */
void servo_task(void *pvParameters)
{
    servo_cmd_t cmd;
    ESP_LOGI(TAG, "Servo task started");

    ESP_LOGI("TASK", "Started: %s, free stack=%d", pcTaskGetName(NULL), uxTaskGetStackHighWaterMark(NULL));
    
    while (1) {
        if (xQueueReceive(servoQueue, &cmd, portMAX_DELAY) == pdTRUE) {
            // Clip and apply
            if (cmd.angle1 < 0) cmd.angle1 = 0;
            if (cmd.angle1 > max_angle1) cmd.angle1 = max_angle1;
            if (cmd.angle2 < 0) cmd.angle2 = 0;
            if (cmd.angle2 > max_angle2) cmd.angle2 = max_angle2;

            current_angle1 = cmd.angle1;
            current_angle2 = cmd.angle2;

            set_servo(SERVO_PIN_1, current_angle1, max_angle1);
            set_servo(SERVO_PIN_2, current_angle2, max_angle2);

            ESP_LOGI(TAG, "Applied servo angles: %d, %d", current_angle1, current_angle2);
        }
    }
}
