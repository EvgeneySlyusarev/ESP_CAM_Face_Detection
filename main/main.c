#include "common.h"
#include "config.h"
#include "network.h"
#include "camera.h"
#include "servo.h"

// Глобальные переменные
char wifiSSID[64] = {0};
char wifiPASS[64] = {0};
char serverURI[128] = {0};

uint32_t total_frames_captured = 0;
uint32_t total_frames_sent = 0;
uint32_t total_frames_dropped = 0;

void app_main(void)
{
    ESP_LOGI("MAIN", "ESP32-CAM Streaming (modular version)");
    ESP_LOGI("MAIN", "Free heap: %d bytes", esp_get_free_heap_size());
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    if (init_sd() != ESP_OK) {
        ESP_LOGE("MAIN", "Failed to initialize SD card");
        return;
    }
    
    if (!read_config_from_sd()) {
        ESP_LOGE("MAIN", "Failed to read configuration");
        return;
    }

    esp_netif_create_default_wifi_sta();
    wifi_init();

    if (camera_init() != ESP_OK) {
        ESP_LOGE("MAIN", "Failed to initialize camera");
        return;
    }

    gpio_reset_pin(FLASH_GPIO_NUM);
    gpio_set_direction(FLASH_GPIO_NUM, GPIO_MODE_OUTPUT);
    gpio_set_level(FLASH_GPIO_NUM, 0);

    init_servo_pwm();

    cameraQueue = xQueueCreate(QUEUE_SIZE, sizeof(frame_t*));
    if (!cameraQueue) {
        ESP_LOGE("MAIN", "Failed to create camera queue");
        return;
    }

    if (xTaskCreate(camera_capture_task, "camera_capture_task", 15360, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE("MAIN", "Failed to create camera capture task");
        return;
    }
    
    if (xTaskCreate(camera_send_task, "camera_send_task", 15360, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE("MAIN", "Failed to create camera send task");
        return;
    }
    
    start_servo_server();

    ESP_LOGI("MAIN", "Application started successfully");
}