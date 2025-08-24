#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#include "esp_camera.h"
#include "esp_http_client.h"
#include "mdns.h"

static const char *TAG = "ESP_CAM_HTTP";

// ===== Config variables =====
#define CONFIG_FILE_PATH "/sdcard/config.txt"
char wifiSSID[64] = {0};
char wifiPASS[64] = {0};
char uploadURL[128] = {0};

// ===== Semaphore for Wi-Fi connection =====
static SemaphoreHandle_t wifi_connected_semaphore;

// ===== ESP32-CAM Pins (AI Thinker) =====
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

// ===== Read config.txt from SD =====
static void readConfig(void) {
    FILE* f = fopen(CONFIG_FILE_PATH, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open config file");
        return;
    }

    char line[192];
    while (fgets(line, sizeof(line), f)) {
        char *pos;
        if ((pos = strchr(line, '\n')) != NULL) *pos = 0;
        if ((pos = strchr(line, '\r')) != NULL) *pos = 0;

        if (strncmp(line, "WIFI_SSID=", 10) == 0) {
            strncpy(wifiSSID, line + 10, sizeof(wifiSSID)-1);
        } else if (strncmp(line, "WIFI_PASS=", 10) == 0) {
            strncpy(wifiPASS, line + 10, sizeof(wifiPASS)-1);
        } else if (strncmp(line, "UPLOAD_URL=", 11) == 0) {
            strncpy(uploadURL, line + 11, sizeof(uploadURL)-1);
        }
    }

    fclose(f);
    ESP_LOGI(TAG, "Config loaded: SSID=%s, UPLOAD_URL=%s", wifiSSID, uploadURL);
}

// ===== Initialize SD card =====
static esp_err_t init_sd(void) {
    ESP_LOGI(TAG, "Initializing SD card...");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1; // 1-bit mode for ESP32-CAM

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 3,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        return ret;
    }

    sdmmc_card_print_info(stdout, card);

    // Auto unmount on reset
    esp_register_shutdown_handler((shutdown_handler_t)esp_vfs_fat_sdcard_unmount);

    return ESP_OK;
}

// ===== mDNS init =====
void init_mdns(void) {
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("esp32"));
    ESP_ERROR_CHECK(mdns_instance_name_set("ESP32-CAM"));
    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, NULL, 0));
    ESP_LOGI(TAG, "mDNS responder started as esp32.local");
}

// ===== Wi-Fi event handler =====
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: %s", ip_str);
        xSemaphoreGive(wifi_connected_semaphore);
    }
}

// ===== Wi-Fi Init =====
static void wifi_init(void) {
    wifi_connected_semaphore = xSemaphoreCreateBinary();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL, NULL));

    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, wifiSSID, sizeof(wifi_config.sta.ssid)-1);
    strncpy((char*)wifi_config.sta.password, wifiPASS, sizeof(wifi_config.sta.password)-1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
}

// ===== Camera Init =====
static esp_err_t camera_init(void) {
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
        .frame_size     = FRAMESIZE_QVGA,
        .jpeg_quality   = 15,
        .fb_count       = 1,
        .grab_mode      = CAMERA_GRAB_WHEN_EMPTY
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s) s->set_special_effect(s, 2); // grayscale

    ESP_LOGI(TAG, "Camera initialized (JPEG+Grayscale, QVGA)");
    return ESP_OK;
}

// ===== HTTP POST =====
static esp_err_t send_photo_http(const uint8_t *image_data, size_t image_len) {
    if (uploadURL[0] == '\0') {
        ESP_LOGE(TAG, "UPLOAD_URL is empty");
        return ESP_ERR_INVALID_ARG;
    }

    esp_http_client_config_t config = {
        .url = uploadURL,
        .timeout_ms = 3000,
        .keep_alive_enable = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_FAIL;

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "image/jpeg");
    esp_http_client_set_post_field(client, (const char*)image_data, image_len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200) {
            ESP_LOGI(TAG, "Photo sent successfully (%d bytes)", (int)image_len);
        } else {
            ESP_LOGW(TAG, "Server responded with status %d", status);
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

// ===== Camera sending task =====
static void camera_send_task(void* arg) {
    const TickType_t frame_delay_ticks = 30000 / portTICK_PERIOD_MS; // every 30s
    while (1) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        send_photo_http(fb->buf, fb->len);
        esp_camera_fb_return(fb);
        vTaskDelay(frame_delay_ticks);
    }
}

// ===== Main =====
void app_main(void) {
    ESP_LOGI(TAG, "ESP32-CAM HTTP Uploader start");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    if (init_sd() != ESP_OK) {
        ESP_LOGE(TAG, "SD init failed");
        return;
    }

    readConfig();
    wifi_init();
    xSemaphoreTake(wifi_connected_semaphore, portMAX_DELAY);

    ESP_ERROR_CHECK(camera_init());
    init_mdns();

    xTaskCreate(camera_send_task, "camera_send_task", 8192, NULL, 5, NULL);
}
