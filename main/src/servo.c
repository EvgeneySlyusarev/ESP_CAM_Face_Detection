#include "servo.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "common.h"

static const char *TAG = "SERVO";

static const int max_angle1 = 180;
static const int max_angle2 = 90;

// Delay between steps for smooth movement (in ms)
static const int step_delay_ms = 15;  

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

// smoothly move servo to target angle
void move_servo_smooth(int pin, int *current_angle, int target_angle, int max_angle)
{
    if (target_angle < 0) target_angle = 0;
    if (target_angle > max_angle) target_angle = max_angle;

    while (*current_angle != target_angle) {
        if (*current_angle < target_angle) (*current_angle)++;
        else if (*current_angle > target_angle) (*current_angle)--;

        set_servo(pin, *current_angle, max_angle);
        vTaskDelay(pdMS_TO_TICKS(step_delay_ms)); //task delay
    }
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
    set_servo(SERVO_PIN_1, current_angle1, max_angle1);
    set_servo(SERVO_PIN_2, current_angle2, max_angle2);
}

// Task to handle servo commands
void servo_task(void *pvParameters)
{
    servo_cmd_t cmd;
    ESP_LOGI(TAG, "Servo task started");

    while (1) {
        if (xQueueReceive(servoQueue, &cmd, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Target servo angles: %d, %d", cmd.angle1, cmd.angle2);

            // Плавно доехать до новых углов
            move_servo_smooth(SERVO_PIN_1, &current_angle1, cmd.angle1, max_angle1);
            move_servo_smooth(SERVO_PIN_2, &current_angle2, cmd.angle2, max_angle2);

            ESP_LOGI(TAG, "Applied servo angles: %d, %d", current_angle1, current_angle2);
        }
    }
}
