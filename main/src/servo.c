#include "servo.h"
#include "common.h"
#include "driver/ledc.h"
#include "esp_http_server.h"

static const char *TAG = "SERVO";

/* ===== Servo tuning ===== */
#define SERVO_STEP        2     // degrees per step
#define SERVO_DELAY_MS    30    // delay between steps
#define SERVO_DEADBAND 2    // degrees within which no movement is made
#define LEDC_MAX_DUTY 1024

#define SERVO_MIN_US      600   // safe PWM range for most servos
#define SERVO_MAX_US      2400

static bool servoX_active = false;
static bool servoY_active = false;

static const int max_angleX = 180;
static const int max_angleY = 90;

/* ===== Internal state ===== */
static int last_sent_angleX = -1;
static int last_sent_angleY = -1;

/* ===== Servo task ===== */
void servo_task(void *pvParameters)
{
    while (1)
    {
        bool changed = false;

        /* ===== X axis ===== */
        if (abs(target_angleX - current_angleX) >= SERVO_DEADBAND)
        {
            if (current_angleX < target_angleX) {
                current_angleX += SERVO_STEP;
                if (current_angleX > target_angleX)
                    current_angleX = target_angleX;
                changed = true;
            }
            else if (current_angleX > target_angleX) {
                current_angleX -= SERVO_STEP;
                if (current_angleX < target_angleX)
                    current_angleX = target_angleX;
                changed = true;
            }
        }

        /* ===== Y axis ===== */
        if (abs(target_angleY - current_angleY) >= SERVO_DEADBAND)
        {
            if (current_angleY < target_angleY) {
                current_angleY += SERVO_STEP;
                if (current_angleY > target_angleY)
                    current_angleY = target_angleY;
                changed = true;
            }
            else if (current_angleY > target_angleY) {
                current_angleY -= SERVO_STEP;
                if (current_angleY < target_angleY)
                    current_angleY = target_angleY;
                changed = true;
            }
        }

        /* ===== Update PWM ONLY if angle really changed ===== */
        /* Update PWM ONLY if angle really changed */
if (changed)
{
    if (current_angleX != last_sent_angleX) {
        set_servo(SERVO_PIN_1, current_angleX, max_angleX);
        last_sent_angleX = current_angleX;
        servoX_active = true;
    }

    if (current_angleY != last_sent_angleY) {
        set_servo(SERVO_PIN_2, current_angleY, max_angleY);
        last_sent_angleY = current_angleY;
        servoY_active = true;
    }
}
else
{
    /* No movement — stop PWM to avoid noise */
    if (servoX_active) {
        ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
        servoX_active = false;
    }

    if (servoY_active) {
        ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0);
        servoY_active = false;
    }
}


        vTaskDelay(pdMS_TO_TICKS(SERVO_DELAY_MS));
    }
}

/* ===== PWM output ===== */
void set_servo(int pin, int angle, int max_angle)
{
    if (angle < 0) angle = 0;
    if (angle > max_angle) angle = max_angle;

    int duty_us =
        SERVO_MIN_US +
        (angle * (SERVO_MAX_US - SERVO_MIN_US) / max_angle);

    int channel = (pin == SERVO_PIN_1)
        ? LEDC_CHANNEL_0
        : LEDC_CHANNEL_1;

    uint32_t duty = (duty_us * LEDC_MAX_DUTY) / 20000;

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, channel);
}


/* ===== PWM init ===== */
void init_servo_pwm()
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_10_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 50,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel1 = {
        .gpio_num   = SERVO_PIN_1,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&ledc_channel1);

    ledc_channel_config_t ledc_channel2 = {
        .gpio_num   = SERVO_PIN_2,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel    = LEDC_CHANNEL_1,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&ledc_channel2);

    last_sent_angleX = current_angleX;
    last_sent_angleY = current_angleY;

    set_servo(SERVO_PIN_1, current_angleX, max_angleX);
    set_servo(SERVO_PIN_2, current_angleY, max_angleY);
}

/* ===== HTTP handler ===== */
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

    x = (x < 0) ? 0 : (x > max_angleX ? max_angleX : x);
    y = (y < 0) ? 0 : (y > max_angleY ? max_angleY : y);

    target_angleX = x;
    target_angleY = y;

    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* ===== Server ===== */
void start_servo_server(httpd_handle_t server)
{
    httpd_uri_t servo_uri = {
        .uri      = "/servo",
        .method   = HTTP_POST,
        .handler  = servo_handler,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(server, &servo_uri);
    ESP_LOGI(TAG, "Servo endpoint registered");
}
