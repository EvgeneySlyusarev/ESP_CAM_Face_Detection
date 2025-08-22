#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

static const char *TAG = "ESP_CAM_WS";

// ===== Config variables =====
#define CONFIG_FILE_PATH "/sdcard/config.txt"
char wifiSSID[64] = {0};
char wifiPASS[64] = {0};
char wsURI[128] = {0};

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
bool read_config_from_sd(void)
{
    FILE *f = fopen(CONFIG_FILE_PATH, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open config.txt");
        return false;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *pos = strchr(line, '\n');
        if (pos) *pos = '\0';

        if (strncmp(line, "WIFI_SSID=", 10) == 0) {
            strncpy(wifiSSID, line + 10, sizeof(wifiSSID) - 1);
        } else if (strncmp(line, "WIFI_PASS=", 10) == 0) {
            strncpy(wifiPASS, line + 10, sizeof(wifiPASS) - 1);
        } else if (strncmp(line, "WS_URI=", 7) == 0) {
            strncpy(wsURI, line + 7, sizeof(wsURI) - 1);
        }
    }

    fclose(f);
    ESP_LOGI(TAG, "Config loaded: SSID='%s', WS='%s'", wifiSSID, wsURI);
    return true;
}

// ===== Initialize SD card =====
esp_err_t init_sd(void)
{
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
    return ESP_OK;
}

// ===== Wi-Fi event handler =====
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting...");
        esp_wifi_connect();
    }
}

// ===== Wi-Fi Init =====
static void wifi_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, &instance_any_id));

    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, wifiSSID, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, wifiPASS, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    int retries = 0;
    while (esp_wifi_sta_get_ap_info(NULL) != ESP_OK && retries < 20) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        retries++;
    }
    if (retries == 20) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi");
    } else {
        ESP_LOGI(TAG, "Wi-Fi connected");
    }
}

// ===== Camera Init =====
static esp_err_t camera_init(void)
{
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
        .jpeg_quality   = 10,
        .fb_count       = 1,
        .grab_mode      = CAMERA_GRAB_WHEN_EMPTY
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return err;
    }
    ESP_LOGI(TAG, "Camera initialized");
    return ESP_OK;
}

// ===== HTTP/WebSocket send function =====
esp_err_t send_photo_ws(uint8_t *image_data, size_t image_len)
{
    esp_http_client_config_t config = {
        .url = wsURI,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_FAIL;

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "image/jpeg");
    esp_http_client_set_post_field(client, (const char*)image_data, image_len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Sent %d bytes to WS server", (int)image_len);
    } else {
        ESP_LOGE(TAG, "Send failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

// ===== Camera sending task =====
void camera_send_task(void *pvParameters)
{
    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "Captured %d bytes, sending...", fb->len);
        send_photo_ws(fb->buf, fb->len);

        esp_camera_fb_return(fb);
        vTaskDelay(5000 / portTICK_PERIOD_MS); // каждые 5 секунд фото
    }
}

// ===== Main =====
void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-CAM HTTP/WebSocket Stream Start");

    if (init_sd() != ESP_OK) {
        ESP_LOGE(TAG, "SD init failed");
        return;
    }

    if (!read_config_from_sd()) {
        ESP_LOGE(TAG, "Config read failed");
        return;
    }

    wifi_init();
    ESP_ERROR_CHECK(camera_init());

    xTaskCreate(camera_send_task, "camera_send_task", 8192, NULL, 5, NULL);
}
