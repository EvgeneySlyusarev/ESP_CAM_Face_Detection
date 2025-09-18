#include "network.h"
#include "common.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "mdns.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NETWORK";

// --- mDNS ---
static void start_mdns_service(const char *hostname)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return;
    }
    mdns_hostname_set(hostname);
    mdns_instance_name_set("ESP32-CAM Device");
    ESP_LOGI(TAG, "mDNS started: http://%s.local", hostname);
}

// --- Wi-Fi события ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void got_ip_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t* event = (const ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Wi-Fi connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        start_mdns_service("esp32cam");
    }
}

// --- Wi-Fi init ---
void wifi_init(void)
{
    // Создаем EventGroup
    wifi_event_group = xEventGroupCreate();

    // Регистрируем обработчики событий
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_handler, NULL, NULL));

    // Инициализация Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_netif_set_hostname(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), "esp32cam"));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Перебираем все сети из SD
    for (int i = 0; i < wifi_count; i++) {
        wifi_config_t wifi_config = {0};
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char*)wifi_config.sta.ssid, wifi_list[i].ssid, sizeof(wifi_config.sta.ssid)-1);
        strncpy((char*)wifi_config.sta.password, wifi_list[i].pass, sizeof(wifi_config.sta.password)-1);
        wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

        ESP_LOGI(TAG, "Trying to connect to SSID: %s", wifi_list[i].ssid);

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_connect());

        // Ждем подключения 5 секунд
        EventBits_t bits = xEventGroupWaitBits(
            wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(5000)
        );

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected to %s", wifi_list[i].ssid);
            return; // Подключились, выходим
        } else {
            ESP_LOGW(TAG, "Failed to connect to %s, trying next...", wifi_list[i].ssid);
            ESP_ERROR_CHECK(esp_wifi_stop());
        }
    }

    ESP_LOGE(TAG, "Could not connect to any configured Wi-Fi network");
}
