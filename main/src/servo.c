#include "servo.h"
#include "common.h"
#include "driver/ledc.h"
#include "esp_http_server.h"

void set_servo(int pin, int angle, int max_angle)
{
    if(angle < 0) angle = 0;
    if(angle > max_angle) angle = max_angle;

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

    set_servo(SERVO_PIN_1, 90, 180);
    set_servo(SERVO_PIN_2, 45, 90);
}

static esp_err_t servo_handler(httpd_req_t *req)
{
    char buf[32];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = 0;

    int angle1 = 90, angle2 = 45;
    sscanf(buf, "angle1=%d&angle2=%d", &angle1, &angle2);

    if(angle1 < 0) angle1 = 0;
    if(angle1 > 180) angle1 = 180;
    if(angle2 < 0) angle2 = 0;
    if(angle2 > 90) angle2 = 90;

    set_servo(SERVO_PIN_1, angle1, 180);
    set_servo(SERVO_PIN_2, angle2, 90);

    const char* resp = "OK";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

void start_servo_server(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    httpd_uri_t servo_uri = {
        .uri       = "/servo",
        .method    = HTTP_POST,
        .handler   = servo_handler,
        .user_ctx  = NULL
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &servo_uri);
        ESP_LOGI("SERVO", "Servo control server started on port %d", config.server_port);
    }
}