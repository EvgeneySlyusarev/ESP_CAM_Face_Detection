#include "network.h"
#include "common.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "NETWORK";

static esp_netif_t *sta_netif = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Wi-Fi started, trying to connect...");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
    ESP_LOGW(TAG,
        "Disconnected from Wi-Fi, retrying...");
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    esp_wifi_connect();
    break;
        default:
            break;
        }
    }
}

static void got_ip_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "GOT IP: " IPSTR "  GW: " IPSTR "  MASK: " IPSTR,
                 IP2STR(&event->ip_info.ip),
                 IP2STR(&event->ip_info.gw),
                 IP2STR(&event->ip_info.netmask));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
        ESP_LOGW(TAG, "Lost IP address");
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_init(void)
{
    // Event group
    wifi_event_group = xEventGroupCreate();
    if (!wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create wifi_event_group");
        return ESP_FAIL;
    }

    // Handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &got_ip_handler, NULL, NULL));

    // 1) create default STA netif BEFORE wifi init/start
    sta_netif = esp_netif_create_default_wifi_sta();
    if (!sta_netif) {
        ESP_LOGE(TAG, "Failed to create default WIFI STA netif");
        return ESP_FAIL;
    }

    // Optional but helpful: set hostname and ensure DHCP client is running
    ESP_ERROR_CHECK(esp_netif_set_hostname(sta_netif, "esp32cam"));
    // Stop/start DHCP to be 100% sure client is active
    esp_netif_dhcpc_stop(sta_netif);
    esp_err_t dhcp_ret = esp_netif_dhcpc_start(sta_netif);
    if (dhcp_ret != ESP_OK && dhcp_ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_LOGW(TAG, "Failed to start DHCP client: %s", esp_err_to_name(dhcp_ret));
    } else {
        ESP_LOGI(TAG, "DHCP client started");
    }

    // 2) init Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // --- Scan APs ---
    wifi_scan_config_t scanConf = {0};
    scanConf.show_hidden = true;
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, true));

    uint16_t apCount = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&apCount));
    if (apCount == 0) {
        ESP_LOGW(TAG, "No Wi-Fi APs found!");
        return ESP_FAIL;
    }

    wifi_ap_record_t *apRecords = malloc(sizeof(wifi_ap_record_t) * apCount);
    if (!apRecords) {
        ESP_LOGE(TAG, "No memory for AP records");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, apRecords));
    ESP_LOGI(TAG, "Found %u APs", apCount);

    int best_rssi = -127;
    char best_ssid[64] = {0};
    char best_pass[64] = {0};

    for (int i = 0; i < wifi_count; i++) {
        for (int j = 0; j < apCount; j++) {
            if (strcmp((char *)apRecords[j].ssid, wifi_list[i].ssid) == 0) {
                ESP_LOGI(TAG, "Candidate: %s RSSI=%d", wifi_list[i].ssid, apRecords[j].rssi);
                if (apRecords[j].rssi > best_rssi) {
                    best_rssi = apRecords[j].rssi;
                    strncpy(best_ssid, wifi_list[i].ssid, sizeof(best_ssid) - 1);
                    strncpy(best_pass, wifi_list[i].pass, sizeof(best_pass) - 1);
                }
            }
        }
    }
    free(apRecords);

    if (best_ssid[0] == '\0') {
    ESP_LOGW(TAG, "No known networks available!");
    ESP_LOGW(TAG, "Scanned AP count: %u, Known Wi-Fi count: %u", apCount, wifi_count);
    return ESP_FAIL;
}


    ESP_LOGI(TAG, "Connecting to SSID: %s (RSSI=%d)", best_ssid, best_rssi);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, best_ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, best_pass, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Wait up to 30 sec for IP
    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(30000)
    );

    if (!(bits & WIFI_CONNECTED_BIT)) {
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(sta_netif, &ip) == ESP_OK) {
        if (ip.ip.addr != 0) {
            ESP_LOGW(TAG,
                "No GOT_IP event, but IP already assigned: " IPSTR,
                IP2STR(&ip.ip));
        } else {
            ESP_LOGW(TAG, "No IP assigned (DHCP failed or not completed)");
        }
    } else {
        ESP_LOGW(TAG, "Failed to read IP info from netif");
    }

    ESP_LOGE(TAG,
        "Failed to connect to Wi-Fi or acquire IP after timeout (30s)");
    return ESP_FAIL;
}


    return ESP_OK;
}