#include "servo.h"
#include "common.h"
#include "driver/ledc.h"
#include "esp_http_server.h"

static int current_angle1 = 90;
static int current_angle2 = 45;
static const int max_angle1 = 180;
static const int max_angle2 = 90;

void set_servo(int pin, int angle, int max_angle) {
    if (angle < 0) angle = 0;
    if (angle > max_angle) angle = max_angle;

    int duty_us = 500 + (angle * (2500 - 500) / max_angle);
    int channel = (pin == SERVO_PIN_1 ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1);

    uint32_t duty = (duty_us * (1 << 16)) / 20000;
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, channel);
}

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

    set_servo(SERVO_PIN_1, current_angle1, max_angle1);
    set_servo(SERVO_PIN_2, current_angle2, max_angle2);
}

static esp_err_t servo_handler(httpd_req_t *req)
{
    char query[64];
    int angle1 = current_angle1;
    int angle2 = current_angle2;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char param[8];
        if (httpd_query_key_value(query, "x", param, sizeof(param)) == ESP_OK) {
            angle1 = atoi(param);
        }
        if (httpd_query_key_value(query, "y", param, sizeof(param)) == ESP_OK) {
            angle2 = atoi(param);
        }
    }

    // Apply to servos
    current_angle1 = (angle1 < 0) ? 0 : (angle1 > max_angle1 ? max_angle1 : angle1);
    current_angle2 = (angle2 < 0) ? 0 : (angle2 > max_angle2 ? max_angle2 : angle2);

    set_servo(SERVO_PIN_1, current_angle1, max_angle1);
    set_servo(SERVO_PIN_2, current_angle2, max_angle2);

    // Return JSON response
    char resp[64];
    snprintf(resp, sizeof(resp),
             "{\"servo1\":%d,\"servo2\":%d,\"status\":\"ok\"}",
             current_angle1, current_angle2);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}

void start_servo_server(httpd_handle_t server)
{
    httpd_uri_t servo_uri = {
        .uri       = "/servo",
        .method    = HTTP_POST,
        .handler   = servo_handler,
        .user_ctx  = NULL
    };

    httpd_register_uri_handler(server, &servo_uri);
    ESP_LOGI("SERVO", "Servo endpoint registered");
}
