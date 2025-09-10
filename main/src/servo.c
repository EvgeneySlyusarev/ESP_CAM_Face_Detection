#include "servo.h"
#include "esp_log.h"
#include "common.h"

static const char *TAG = "SERVO";

void init_servo_pwm(void) {
    // Настройка таймера
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_16_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    // Канал 1
    ledc_channel_config_t ch1 = {
        .gpio_num = SERVO_PIN_1,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ch1);

    // Канал 2
    ledc_channel_config_t ch2 = {
        .gpio_num = SERVO_PIN_2,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ch2);

    ESP_LOGI(TAG, "Servo PWM initialized");

    // Стартовые значения
    set_servo(SERVO_PIN_1, current_angle1, 180);
    set_servo(SERVO_PIN_2, current_angle2, 90);
}

void set_servo(int pin, int angle, int max_angle) {
    if (angle < 0) angle = 0;
    if (angle > max_angle) angle = max_angle;

    float min_us = 500.0f;
    float max_us = 2500.0f;
    float us = min_us + ((float)angle / (float)max_angle) * (max_us - min_us);

    uint32_t duty = (uint32_t)((us / 20000.0f) * ((1 << 16) - 1));

    int channel = (pin == SERVO_PIN_1) ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);

    // Обновляем глобальные переменные
    if (pin == SERVO_PIN_1) current_angle1 = angle;
    else current_angle2 = angle;

    ESP_LOGI(TAG, "Set servo pin %d -> angle %d (duty %u)", pin, angle, duty);
}
