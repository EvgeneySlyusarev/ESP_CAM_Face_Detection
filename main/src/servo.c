#include "servo.h"
#include "common.h"
#include "driver/ledc.h"
#include "esp_http_server.h"

static const char *TAG = "SERVO";

static const int max_angleX = 180;
static const int max_angleY = 90;

// task for processing servo commands
void servo_task(void *pvParameters) {
    servo_cmd_t cmd;
    
    while (1) {

        if (xQueueReceive(servoQueue, &cmd, portMAX_DELAY) == pdTRUE) {
            set_servo(SERVO_PIN_1, cmd.angleX, 180);
            set_servo(SERVO_PIN_2, cmd.angleY, 90);
            
            ESP_LOGI(TAG, "Servo angles set: %d, %d", cmd.angleX, cmd.angleY);
        }
    }
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
    int angleX = current_angleX;
    int angleY = current_angleY;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char param[8];
        if (httpd_query_key_value(query, "x", param, sizeof(param)) == ESP_OK) {
            angleX = atoi(param);
        }
        if (httpd_query_key_value(query, "y", param, sizeof(param)) == ESP_OK) {
            angleY = atoi(param);
        }
    }

    // Ограничение значений
    angleX = (angleX < 0) ? 0 : (angleX > max_angleX ? max_angleX : angleX);
    angleY = (angleY < 0) ? 0 : (angleY > max_angleY ? max_angleY : angleY);

    current_angleX = angleX;
    current_angleY = angleY;

    // Отправляем команду в очередь
    servo_cmd_t cmd = { angleX, angleY };
    if (xQueueSend(servoQueue, &cmd, 0) != pdTRUE) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_sendstr(req, "{\"status\":\"queue_full\"}");
        return ESP_FAIL;
    }

    // Ответ клиенту
    char resp[64];
    snprintf(resp, sizeof(resp),
             "{\"servoX\":%d,\"servoY\":%d,\"status\":\"ok\"}",
             angleX, angleY);
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
