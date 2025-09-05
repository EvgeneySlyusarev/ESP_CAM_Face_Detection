#include "common.h"
#include "config.h"
#include "network.h"
#include "camera.h"
#include "servo.h"
#include "webserver.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    ESP_LOGI("MAIN", "ESP32-CAM Streaming (modular)");

    // --- NVS ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // --- Network init ---
    ESP_ERROR_CHECK(esp_netif_init());
    ret = esp_event_loop_create_default();
    ESP_ERROR_CHECK(ret);

    // --- SD Card & Config ---
    ret = init_sd(); // init_sd должен возвращать esp_err_t
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "SD card init failed");
        return;
    }

    if (!read_config_from_sd()) { // read_config_from_sd должен возвращать bool
        ESP_LOGE("MAIN", "Failed to read config from SD");
        return;
    }

    // --- Wi-Fi ---
    if (wifi_init() != ESP_OK) {
        ESP_LOGE("MAIN", "Wi-Fi init failed");
        return;
    } else {
        esp_netif_ip_info_t ip_info;
        esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
            ESP_LOGI("MAIN", "Device IP address: " IPSTR, IP2STR(&ip_info.ip));
        }
    }


    // --- Frame mutex ---
    frame_mutex = xSemaphoreCreateMutex();
    if (!frame_mutex) {
        ESP_LOGE("MAIN", "Frame mutex creation failed");
        return;
    }

    // --- Camera ---
    if (camera_init() != ESP_OK) {
        ESP_LOGE("MAIN", "Camera init failed");
        return;
    }

    // --- Flash LED ---
    gpio_reset_pin(FLASH_GPIO_NUM);
    gpio_set_direction(FLASH_GPIO_NUM, GPIO_MODE_OUTPUT);
    gpio_set_level(FLASH_GPIO_NUM, 0);

    // --- Servo ---
    init_servo_pwm();

    // --- Camera capture task ---
    if (xTaskCreate(camera_capture_task, "camera_capture_task", 15360, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE("MAIN", "Camera capture task creation failed");
        return;
    }

    // --- Start webserver ---
    start_webserver();

    ESP_LOGI("MAIN", "Application started successfully");
}
