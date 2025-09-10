#include "camera.h"
#include "common.h"
#include "esp_camera.h"

#define PWDN_GPIO_NUM     32
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

static const char *TAG = "CAMERA";


esp_err_t camera_init(void)
{
    camera_config_t config = {
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
        .frame_size     = FRAMESIZE_320X320,
        .jpeg_quality   = 12,
        .fb_count       = 2,
        .grab_mode      = CAMERA_GRAB_LATEST
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed: 0x%x", err);
        return err;
    }
    ESP_LOGI(TAG, "Camera initialized successfully");
    return ESP_OK;
}

// Сохраняем кадр безопасно
void process_frame(camera_fb_t *fb, uint32_t frame_number)
{
    if (!fb) return;

    ESP_LOGI(TAG, "Processing frame %u (%u bytes)", frame_number, (unsigned)fb->len);

    // Возвращаем предыдущий fb драйверу
    if (last_frame.fb) {
        esp_camera_fb_return(last_frame.fb);
        last_frame.fb = NULL;
    }

    // Сохраняем новый fb
    last_frame.fb = fb;
    last_frame.data = fb->buf;
    last_frame.len = fb->len;
    last_frame.frame_number = frame_number;
}

void camera_capture_task(void *pvParameters)
{
    const TickType_t frameDelay = pdMS_TO_TICKS(30); // ~33 FPS
    uint32_t frame_counter = 0;
    uint32_t log_counter = 0;

    while (1) {
        if (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) {

            // Включаем подсветку
            gpio_set_level(FLASH_GPIO_NUM, 1);
            vTaskDelay(pdMS_TO_TICKS(50)); // делаем паузу, чтобы глаз видел вспышку

            // Снимаем кадр
            camera_fb_t *fb = esp_camera_fb_get();

            if (!fb) {
                ESP_LOGE(TAG, "Camera capture failed");
                gpio_set_level(FLASH_GPIO_NUM, 0); // выключаем вспышку даже при ошибке
                total_frames_dropped++;
                vTaskDelay(frameDelay);
                continue;
            }

            // Кадр снят — выключаем подсветку
            gpio_set_level(FLASH_GPIO_NUM, 0);

            total_frames_captured++;
            frame_counter++;

            if (++log_counter % 10 == 0) {
                ESP_LOGI(TAG, "Stats: Captured %u, Dropped %u", total_frames_captured, total_frames_dropped);
            }

            ESP_LOGI(TAG, "Frame %u captured, size=%u bytes", frame_counter, (unsigned)fb->len);

            process_frame(fb, frame_counter);

        } else {
            if (log_counter % 20 == 0) {
                ESP_LOGW(TAG, "Waiting for Wi-Fi connection...");
            }
            log_counter++;
        }

        vTaskDelay(frameDelay);
    }
}
