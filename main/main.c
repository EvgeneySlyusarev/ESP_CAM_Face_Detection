#include <stdio.h>
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_system.h"

static const char *TAG = "CAM_TEST";

// AI Thinker ESP32-CAM PIN Map
#define PWDN_GPIO_NUM     32   // Обычно 32 для AI-Thinker, не -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void app_main(void)
{
    camera_config_t camera_config = {
        .pin_pwdn       = PWDN_GPIO_NUM,
        .pin_reset      = RESET_GPIO_NUM,
        .pin_xclk       = XCLK_GPIO_NUM,
        .pin_sccb_sda   = SIOD_GPIO_NUM,
        .pin_sccb_scl   = SIOC_GPIO_NUM,
        .pin_d7         = Y9_GPIO_NUM,
        .pin_d6         = Y8_GPIO_NUM,
        .pin_d5         = Y7_GPIO_NUM,
        .pin_d4         = Y6_GPIO_NUM,
        .pin_d3         = Y5_GPIO_NUM,
        .pin_d2         = Y4_GPIO_NUM,
        .pin_d1         = Y3_GPIO_NUM,
        .pin_d0         = Y2_GPIO_NUM,
        .pin_vsync      = VSYNC_GPIO_NUM,
        .pin_href       = HREF_GPIO_NUM,
        .pin_pclk       = PCLK_GPIO_NUM,
        .xclk_freq_hz   = 20000000,
        .ledc_timer     = LEDC_TIMER_0,
        .ledc_channel   = LEDC_CHANNEL_0,
        .pixel_format   = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_VGA,   // или FRAMESIZE_SVGA, FRAMESIZE_XGA
        .jpeg_quality = 12,            // качество JPEG (чем меньше — тем лучше качество)
        .fb_count = 2,                 // можно 2 буфера, если PSRAM есть
        .grab_mode      = CAMERA_GRAB_WHEN_EMPTY
    };

    // Инициализация камеры
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return;
    }

    // Получаем сенсор
    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        ESP_LOGE(TAG, "Camera sensor not found");
        return;
    }

    // Проверяем PID
    if (s->id.PID != OV2640_PID) {
        ESP_LOGE(TAG, "Detected camera not supported (PID=%d)", s->id.PID);
        return;
    }

    ESP_LOGI(TAG, "Camera OV2640 detected and initialized");

    // Захват тестового кадра
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        return;
    }
    ESP_LOGI(TAG, "Captured frame: %dx%d", fb->width, fb->height);

    // Освобождение буфера кадра
    esp_camera_fb_return(fb);
}
