#include "network.h"
#include "common.h"
#include "esp_http_client.h"

EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                              int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI("NETWORK", "Reconnecting to Wi-Fi...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void got_ip_handler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI("NETWORK", "Wi-Fi connected, IP obtained: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
}

void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &got_ip_handler,
                                                        NULL,
                                                        NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    strncpy((char*)wifi_config.sta.ssid, wifiSSID, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, wifiPASS, sizeof(wifi_config.sta.password));

    ESP_LOGI("NETWORK", "Connecting to: %s", wifiSSID);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, 
                                          WIFI_CONNECTED_BIT, 
                                          pdFALSE, 
                                          pdTRUE, 
                                          pdMS_TO_TICKS(30000));
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI("NETWORK", "Wi-Fi connected successfully");
    } else {
        ESP_LOGE("NETWORK", "Wi-Fi connection failed - timeout");
    }
}

esp_err_t send_photo_http_with_retry(uint8_t *image_data, size_t image_len)
{
    if (strlen(serverURI) == 0) {
        ESP_LOGE("NETWORK", "Server URL is not set");
        return ESP_FAIL;
    }

    const int max_retries = 2;
    esp_err_t err = ESP_FAIL;

    for (int attempt = 0; attempt < max_retries; attempt++) {
        if (attempt > 0) {
            ESP_LOGW("NETWORK", "Retrying photo upload (%d/%d)", attempt, max_retries);
            vTaskDelay(pdMS_TO_TICKS(100 * attempt));
        }

        esp_http_client_config_t config = {
            .url = serverURI,
            .transport_type = HTTP_TRANSPORT_OVER_TCP,
            .timeout_ms = 10000,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE("NETWORK", "HTTP client init failed");
            continue;
        }

        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", "image/jpeg");
        esp_http_client_set_header(client, "Connection", "close");
        esp_http_client_set_header(client, "User-Agent", "ESP32-CAM");
        
        esp_http_client_set_post_field(client, (const char*)image_data, image_len);

        err = esp_http_client_perform(client);
        
        if (err == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            
            if (status_code == 200) {
                if (attempt > 0) {
                    ESP_LOGI("NETWORK", "Upload succeeded after %d attempts: %d bytes", 
                            attempt + 1, (int)image_len);
                }
                break;
            } else {
                ESP_LOGE("NETWORK", "HTTP error: %d (attempt %d)", status_code, attempt + 1);
                err = ESP_FAIL;
            }
        } else {
            ESP_LOGE("NETWORK", "Upload failed: %s (attempt %d)", 
                    esp_err_to_name(err), attempt + 1);
        }

        esp_http_client_cleanup(client);
    }

    return err;
}