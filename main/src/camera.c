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
        .fb_location = CAMERA_FB_IN_PSRAM,
        .jpeg_quality   = 12,
        .fb_count       = 2,
        .grab_mode      = CAMERA_GRAB_LATEST
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return err;
    }

    ESP_LOGI(TAG, "Camera initialized successfully");
    return ESP_OK;
}

void camera_capture_task(void *pvParameters)
{
    camera_pins_t *cfg = pvParameters ? (camera_pins_t*)pvParameters : &default_pins;

    gpio_reset_pin(cfg->flash_gpio);
    gpio_set_direction(cfg->flash_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(cfg->flash_gpio, 0);

    while (1) {
        if (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) {
            camera_fb_t* fb = esp_camera_fb_get();
            if (!fb) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }

          volatile uint8_t idx = frame_buffer.write_index;

        if (frame_buffer.ready[idx] && frame_buffer.fb[idx]) {
            esp_camera_fb_return(frame_buffer.fb[idx]);
            }

            frame_buffer.fb[idx] = fb;
            frame_buffer.ready[idx] = true;

            __asm__ volatile ("" ::: "memory");

            frame_buffer.write_index = idx ^ 1;

            total_frames_captured++;
        }
        taskYIELD();
    }
}
