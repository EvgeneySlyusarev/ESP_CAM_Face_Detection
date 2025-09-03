#include "network.h"
#include "common.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "mdns.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "NETWORK";

// --- mDNS service ---
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

// --- Wi-Fi events ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) 
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected from Wi-Fi, retrying...");
        if ((xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) == 0) {
            esp_wifi_connect();
        }
    }
}

static void got_ip_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            const ip_event_got_ip_t* event = (const ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "Wi-Fi connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            start_mdns_service("esp32cam");
        } else if (event_id == IP_EVENT_STA_LOST_IP) {
            ESP_LOGW(TAG, "Lost IP address");
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

// --- Wi-Fi init with automatic network selection ---
void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();
    if (!wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create Wi-Fi event group");
        return;
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &got_ip_handler, NULL, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Set device hostname
    esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif) {
        ESP_ERROR_CHECK(esp_netif_set_hostname(sta_netif, "esp32cam"));
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // --- Scan available APs ---
    wifi_scan_config_t scanConf = {0};
    scanConf.show_hidden = true;
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, true));

    uint16_t apCount = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&apCount));
    if (apCount == 0) {
        ESP_LOGW(TAG, "No Wi-Fi APs found!");
        return;
    }

    wifi_ap_record_t *apRecords = malloc(sizeof(wifi_ap_record_t) * apCount);
    if (!apRecords) {
        ESP_LOGE(TAG, "Failed to allocate memory for AP records");
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, apRecords));
    ESP_LOGI(TAG, "Found %d APs", apCount);

    // --- Choose best known network ---
    int best_rssi = -127;
    memset(wifiSSID, 0, sizeof(wifiSSID));
    memset(wifiPASS, 0, sizeof(wifiPASS));

    for (int i = 0; i < wifi_entry_count; i++) {
        for (int j = 0; j < apCount; j++) {
            if (strcmp((char*)apRecords[j].ssid, wifi_entries[i].ssid) == 0) {
                ESP_LOGI(TAG, "Candidate: %s RSSI=%d", wifi_entries[i].ssid, apRecords[j].rssi);
                if (apRecords[j].rssi > best_rssi) {
                    best_rssi = apRecords[j].rssi;
                    strncpy(wifiSSID, wifi_entries[i].ssid, sizeof(wifiSSID) - 1);
                    strncpy(wifiPASS, wifi_entries[i].pass, sizeof(wifiPASS) - 1);
                }
            }
        }
    }

    // --- Log all detected APs ---
    for (int j = 0; j < apCount; j++) {
        ESP_LOGI(TAG, "Detected AP: '%s', RSSI=%d", apRecords[j].ssid, apRecords[j].rssi);
    }

    free(apRecords);

    if (strlen(wifiSSID) == 0) {
        ESP_LOGW(TAG, "No known networks available for connection!");
        return;
    }

    ESP_LOGI(TAG, "Connecting to SSID: %s (RSSI=%d)", wifiSSID, best_rssi);

    // --- Disconnect old connection if any ---
    ESP_ERROR_CHECK(esp_wifi_disconnect());

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, wifiSSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, wifiPASS, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_connect());

    // --- Wait for connection with retries, проверка текущего подключения ---
    int retries = 5;
    EventBits_t bits = 0;
    while (retries--) {
        bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                                   pdFALSE, pdTRUE, pdMS_TO_TICKS(5000));

        if(bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Wi-Fi successfully connected!");
            break;
        }

        // Проверяем текущее состояние STA
        wifi_ap_record_t current_ap;
        if (esp_wifi_sta_get_ap_info(&current_ap) == ESP_OK) {
            ESP_LOGI(TAG, "Already connected to SSID: %s", current_ap.ssid);
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        }

        ESP_LOGW(TAG, "Retrying Wi-Fi connection...");
        esp_wifi_connect();
    }

    if (!(bits & WIFI_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi after retries!");
    }
}
