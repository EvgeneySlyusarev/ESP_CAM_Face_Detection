#include "camera.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "common.h" 

static const char *TAG = "CAMERA";

// === Default pins for AI Thinker ESP32-CAM ===
static camera_pins_t default_pins = {
    .pin_pwdn     = 32,
    .pin_reset    = -1,
    .pin_xclk     = 0,
    .pin_sccb_sda = 26,
    .pin_sccb_scl = 27,
    .pin_d7       = 35,
    .pin_d6       = 34,
    .pin_d5       = 39,
    .pin_d4       = 36,
    .pin_d3       = 21,
    .pin_d2       = 19,
    .pin_d1       = 18,
    .pin_d0       = 5,
    .pin_vsync    = 25,
    .pin_href     = 23,
    .pin_pclk     = 22,
    .flash_gpio   = 4
};

// === Camera init function ===
esp_err_t camera_init(const camera_pins_t *pins)
{
    const camera_pins_t *cfg = pins ? pins : &default_pins;
    camera_config_t config = {
        .pin_pwdn       = cfg->pin_pwdn,
        .pin_reset      = cfg->pin_reset,
        .pin_xclk       = cfg->pin_xclk,
        .pin_sccb_sda   = cfg->pin_sccb_sda,
        .pin_sccb_scl   = cfg->pin_sccb_scl,
        .pin_d7         = cfg->pin_d7,
        .pin_d6         = cfg->pin_d6,
        .pin_d5         = cfg->pin_d5,
        .pin_d4         = cfg->pin_d4,
        .pin_d3         = cfg->pin_d3,
        .pin_d2         = cfg->pin_d2,
        .pin_d1         = cfg->pin_d1,
        .pin_d0         = cfg->pin_d0,
        .pin_vsync      = cfg->pin_vsync,
        .pin_href       = cfg->pin_href,
        .pin_pclk       = cfg->pin_pclk,
        .xclk_freq_hz   = 20000000,
        .ledc_timer     = LEDC_TIMER_0,
        .ledc_channel   = LEDC_CHANNEL_0,
        .pixel_format   = PIXFORMAT_JPEG,
        .frame_size     = FRAMESIZE_VGA,
        .jpeg_quality   = 12,
        .fb_count       = 2,
        .grab_mode      = CAMERA_GRAB_LATEST
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return err;
    }

    // Create mutex for frame access
    if (!frame_mutex) frame_mutex = xSemaphoreCreateMutex();
    if (!frame_mutex) return ESP_FAIL;

    ESP_LOGI(TAG, "Camera initialized successfully");
    return ESP_OK;
}

void camera_capture_task(void *pvParameters)
{
    camera_pins_t *cfg = pvParameters ? (camera_pins_t*)pvParameters : &default_pins;

    gpio_reset_pin(cfg->flash_gpio);
    gpio_set_direction(cfg->flash_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(cfg->flash_gpio, 0);

    ESP_LOGI("TASK", "Started: %s, free stack=%d", pcTaskGetName(NULL), uxTaskGetStackHighWaterMark(NULL));
    
    while (1) {
        if (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) {
            //gpio_set_level(cfg->flash_gpio, 1);
            //vTaskDelay(pdMS_TO_TICKS(FLASH_DELAY_MS));

            camera_fb_t* fb = esp_camera_fb_get();
            //gpio_set_level(cfg->flash_gpio, 0);
            if (!fb) { vTaskDelay(pdMS_TO_TICKS(75)); continue; }

            if (fb->len <= 0 || fb->len > MAX_FRAME_SIZE) {
                esp_camera_fb_return(fb);
                total_frames_dropped++;
                vTaskDelay(pdMS_TO_TICKS(150));
                continue;
            }

            frame_t* new_frame = malloc(sizeof(frame_t));
            new_frame->data = malloc(fb->len);
            memcpy(new_frame->data, fb->buf, fb->len);
            new_frame->len = fb->len;
            new_frame->frame_number = total_frames_captured + 1;
            esp_camera_fb_return(fb);

            if (xSemaphoreTake(frame_mutex, 10) == pdTRUE) {
                if (current_frame) {
                    free(current_frame->data);
                    free(current_frame);
                    total_frames_dropped++;
                }
                current_frame = new_frame;
                total_frames_captured++;
                xSemaphoreGive(frame_mutex);
            } else {
                free(new_frame->data);
                free(new_frame);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}
