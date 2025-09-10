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
#include "freertos/semphr.h"
#include "freertos/queue.h"

void app_main(void)
{
    ESP_LOGI("MAIN", "ESP32-CAM Streaming with multitasking");

    // Инициализация NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Инициализация сетевого стека
    ESP_ERROR_CHECK(esp_netif_init());
    ret = esp_event_loop_create_default();
    ESP_ERROR_CHECK(ret);

    // Инициализация объектов FreeRTOS для многозадачности
    frame_mutex = xSemaphoreCreateMutex();
    camera_mutex = xSemaphoreCreateMutex();
    servo_queue = xQueueCreate(10, sizeof(servo_command_t));
    wifi_event_group = xEventGroupCreate();

    // Проверка создания объектов
    if (!frame_mutex || !camera_mutex || !servo_queue || !wifi_event_group) {
        ESP_LOGE("MAIN", "Failed to create FreeRTOS objects");
        return;
    }

    // Инициализация SD карты
    ret = init_sd();
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "SD card init failed");
        return;
    }

    // Чтение конфигурации
    if (!read_config_from_sd()) {
        ESP_LOGE("MAIN", "Failed to read config from SD");
        return;
    }

    // Инициализация Wi-Fi
    if (wifi_init() != ESP_OK) {
        ESP_LOGE("MAIN", "Wi-Fi init failed");
        return;
    } else {
        // Ожидание подключения Wi-Fi
        EventBits_t bits = xEventGroupWaitBits(wifi_event_group, 
                                             WIFI_CONNECTED_BIT,
                                             pdFALSE, pdTRUE, 10000 / portTICK_PERIOD_MS);
        
        if (bits & WIFI_CONNECTED_BIT) {
            esp_netif_ip_info_t ip_info;
            esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
                ESP_LOGI("MAIN", "Device IP address: " IPSTR, IP2STR(&ip_info.ip));
            }
        } else {
            ESP_LOGE("MAIN", "Wi-Fi connection timeout");
            return;
        }
    }

    // Инициализация камеры
    if (camera_init() != ESP_OK) {
        ESP_LOGE("MAIN", "Camera init failed");
        return;
    }

    // Инициализация сервомоторов
    init_servo_pwm();

    // Запуск веб-сервера
    start_webserver();

    ESP_LOGI("MAIN", "Application started successfully");

    // Бесконечный цикл для мониторинга
    while (1) {
        ESP_LOGI("MAIN", "Stats: Captured: %u, Sent: %u, Dropped: %u",
                total_frames_captured, total_frames_sent, total_frames_dropped);
        vTaskDelay(5000 / portTICK_PERIOD_MS); // Каждые 5 секунд
    }
}