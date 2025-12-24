#include "servo.h"
#include "common.h"
#include "driver/ledc.h"
#include "esp_http_server.h"

static const char *TAG = "SERVO";

#define SERVO_STEP 1          // шаг в градусах
#define SERVO_DELAY_MS 20     // задержка между шагами

static const int max_angleX = 180;
static const int max_angleY = 90;

// task for processing servo commands
void servo_task(void *pvParameters)
{
    while (1) {

        if (current_angleX < target_angleX) current_angleX += SERVO_STEP;
        else if (current_angleX > target_angleX) current_angleX -= SERVO_STEP;

        if (current_angleY < target_angleY) current_angleY += SERVO_STEP;
        else if (current_angleY > target_angleY) current_angleY -= SERVO_STEP;

        set_servo(SERVO_PIN_1, current_angleX, max_angleX);
        set_servo(SERVO_PIN_2, current_angleY, max_angleY);

        vTaskDelay(pdMS_TO_TICKS(SERVO_DELAY_MS));
    }
    ESP_LOGI(TAG, "Servo target updated: X=%d Y=%d",
         target_angleX, target_angleY);
}

void set_servo(int pin, int angle, int max_angle)
{
    if (angle < 0) angle = 0;
    if (angle > max_angle) angle = max_angle;

    int duty_us = 500 + (angle * (2500 - 500) / max_angle);
    int channel = (pin == SERVO_PIN_1 ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1);

    uint32_t duty = (duty_us * (1 << 16)) / 20000; // 20ms period for 50Hz
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

    set_servo(SERVO_PIN_1, current_angleX, max_angleX);
    set_servo(SERVO_PIN_2, current_angleY, max_angleY);
}

static esp_err_t servo_handler(httpd_req_t *req)
{
    char query[64];
    int x = target_angleX;
    int y = target_angleY;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char param[8];
        if (httpd_query_key_value(query, "x", param, sizeof(param)) == ESP_OK)
            x = atoi(param);
        if (httpd_query_key_value(query, "y", param, sizeof(param)) == ESP_OK)
            y = atoi(param);
    }

    if (x < 0) x = 0;
    if (x > max_angleX) x = max_angleX;
    if (y < 0) y = 0;
    if (y > max_angleY) y = max_angleY;

    target_angleX = x;
    target_angleY = y;

    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
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
