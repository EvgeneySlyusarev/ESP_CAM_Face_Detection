#include "camera.h"
#include "common.h"
#include "esp_camera.h"

// Пины камеры (AI Thinker)
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

QueueHandle_t cameraQueue = NULL;

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
        .frame_size     = FRAMESIZE_SVGA,
        .jpeg_quality   = 12,
        .fb_count       = 1,
        .grab_mode      = CAMERA_GRAB_WHEN_EMPTY
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE("CAMERA", "Camera initialization failed: 0x%x", err);
        return err;
    }
    ESP_LOGI("CAMERA", "Camera initialized successfully");
    return ESP_OK;
}

void clear_camera_queue(void)
{
    frame_t *frame;
    while (xQueueReceive(cameraQueue, &frame, 0) == pdTRUE) {
        if (frame) {
            free(frame->data);
            free(frame);
            total_frames_dropped++;
        }
    }
}

void camera_capture_task(void *pvParameters)
{
    const TickType_t xDelay = pdMS_TO_TICKS(200);
    uint32_t frame_counter = 0;
    uint32_t log_counter = 0;

    while (1) {
        if (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) {
            // Flash ON
            gpio_set_level(FLASH_GPIO_NUM, 1);
            vTaskDelay(pdMS_TO_TICKS(FLASH_DELAY_MS));

            camera_fb_t *fb = esp_camera_fb_get();
            gpio_set_level(FLASH_GPIO_NUM, 0);

            if (!fb) {
                ESP_LOGE("CAMERA", "Camera capture failed");
                vTaskDelay(xDelay);
                continue;
            }

            if (fb->len > MAX_FRAME_SIZE) {
                ESP_LOGW("CAMERA", "Frame too large (%d > %d), skipping", fb->len, MAX_FRAME_SIZE);
                esp_camera_fb_return(fb);
                vTaskDelay(xDelay);
                continue;
            }

            // Allocate frame
            frame_t *frame = malloc(sizeof(frame_t));
            if (!frame) {
                ESP_LOGE("CAMERA", "Failed to allocate frame memory");
                esp_camera_fb_return(fb);
                vTaskDelay(xDelay);
                continue;
            }

            frame->data = malloc(fb->len);
            if (!frame->data) {
                ESP_LOGE("CAMERA", "Failed to allocate frame data");
                free(frame);
                esp_camera_fb_return(fb);
                vTaskDelay(xDelay);
                continue;
            }

            frame->len = fb->len;
            frame->frame_number = frame_counter++;
            memcpy(frame->data, fb->buf, fb->len);

            total_frames_captured++;

            // Check queue
            if (uxQueueMessagesWaiting(cameraQueue) >= QUEUE_SIZE - 1) {
                ESP_LOGW("CAMERA", "Queue full, clearing old frames");
                clear_camera_queue();
            }

            if (xQueueSend(cameraQueue, &frame, pdMS_TO_TICKS(50)) != pdTRUE) {
                ESP_LOGW("CAMERA", "Failed to enqueue frame %u, dropped", frame->frame_number);
                free(frame->data);
                free(frame);
                total_frames_dropped++;
            }

            if (++log_counter % 10 == 0) {
                ESP_LOGI("CAMERA", "Stats: Captured %u, Dropped %u",
                         total_frames_captured, total_frames_dropped);
            }

            esp_camera_fb_return(fb);
        } else {
            if (log_counter % 20 == 0) {
                ESP_LOGW("CAMERA", "Waiting for Wi-Fi connection...");
            }
            log_counter++;
        }

        vTaskDelay(xDelay);
    }
}
