#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
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
#include "driver/gpio.h"

#include "esp_camera.h"
#include "esp_http_client.h"

static const char *TAG = "ESP_CAM_WS";

// ===== Configuration Variables =====
#define CONFIG_FILE_PATH "/sdcard/config.txt"
char wifiSSID[64] = {0};
char wifiPASS[64] = {0};
char serverURI[128] = {0};

// ===== ESP32-CAM Pin Definitions (AI Thinker) =====
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
#define FLASH_GPIO_NUM    4 

// ===== Camera Queue =====
#define QUEUE_SIZE 10
#define MAX_FRAME_SIZE (60 * 1024)  // 60KB
#define FLASH_DELAY_MS 25           

// Structure for passing frames
typedef struct {
    uint8_t *data;
    size_t len;
    uint32_t frame_number;
} frame_t;

static QueueHandle_t cameraQueue;
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

// Statistics
static uint32_t total_frames_captured = 0;
static uint32_t total_frames_sent = 0;
static uint32_t total_frames_dropped = 0;

// ===== Read config.txt from SD card =====
bool read_config_from_sd(void)
{
    FILE *f = fopen(CONFIG_FILE_PATH, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open config.txt");
        return false;
    }

    memset(wifiSSID, 0, sizeof(wifiSSID));
    memset(wifiPASS, 0, sizeof(wifiPASS));
    memset(serverURI, 0, sizeof(serverURI));

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }

        if (strncmp(line, "WIFI_SSID=", 10) == 0) {
            strncpy(wifiSSID, line + 10, sizeof(wifiSSID) - 1);
        } else if (strncmp(line, "WIFI_PASS=", 10) == 0) {
            strncpy(wifiPASS, line + 10, sizeof(wifiPASS) - 1);
        } else if (strncmp(line, "SERVER_URI=", 11) == 0) {
            strncpy(serverURI, line + 11, sizeof(serverURI) - 1);
        }
    }

    fclose(f);
    
    if (strlen(wifiSSID) == 0 || strlen(wifiPASS) == 0 || strlen(serverURI) == 0) {
        ESP_LOGE(TAG, "Missing required parameters in config");
        return false;
    }
    
    ESP_LOGI(TAG, "Config loaded: SSID='%s', URI='%s'", wifiSSID, serverURI);
    return true;
}

// ===== Initialize SD card =====
esp_err_t init_sd(void)
{
    ESP_LOGI(TAG, "Initializing SD card...");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;

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

// ===== Wi-Fi Event Handlers =====
static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                              int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Reconnecting to Wi-Fi...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void got_ip_handler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "Wi-Fi connected, IP obtained: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
}

// ===== Wi-Fi Initialization =====
static void wifi_init(void)
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

    ESP_LOGI(TAG, "Connecting to: %s", wifiSSID);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, 
                                          WIFI_CONNECTED_BIT, 
                                          pdFALSE, 
                                          pdTRUE, 
                                          pdMS_TO_TICKS(30000));
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi connected successfully");
    } else {
        ESP_LOGE(TAG, "Wi-Fi connection failed - timeout");
    }
}

// ===== Camera Initialization =====
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
        .frame_size     = FRAMESIZE_SVGA,
        .jpeg_quality   = 12,             
        .fb_count       = 1,
        .grab_mode      = CAMERA_GRAB_WHEN_EMPTY
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed: 0x%x", err);
        return err;
    }
    ESP_LOGI(TAG, "Camera initialized successfully");
    return ESP_OK;
}

// ===== HTTP Photo Upload with Retry =====
esp_err_t send_photo_http_with_retry(uint8_t *image_data, size_t image_len)
{
    if (strlen(serverURI) == 0) {
        ESP_LOGE(TAG, "Server URL is not set");
        return ESP_FAIL;
    }

    const int max_retries = 2;
    esp_err_t err = ESP_FAIL;

    for (int attempt = 0; attempt < max_retries; attempt++) {
        if (attempt > 0) {
            ESP_LOGW(TAG, "Retrying photo upload (%d/%d)", attempt, max_retries);
            vTaskDelay(pdMS_TO_TICKS(100 * attempt));
        }

        esp_http_client_config_t config = {
            .url = serverURI,
            .transport_type = HTTP_TRANSPORT_OVER_TCP,
            .timeout_ms = 10000,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "HTTP client init failed");
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
                    ESP_LOGI(TAG, "Upload succeeded after %d attempts: %d bytes", 
                            attempt + 1, (int)image_len);
                }
                break;
            } else {
                ESP_LOGE(TAG, "HTTP error: %d (attempt %d)", status_code, attempt + 1);
                err = ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG, "Upload failed: %s (attempt %d)", 
                    esp_err_to_name(err), attempt + 1);
        }

        esp_http_client_cleanup(client);
    }

    return err;
}

// ===== Check Server Connection =====
void check_server_connection(void)
{
    ESP_LOGI(TAG, "Checking server connection: %s", serverURI);
    
    char test_uri[128];
    strncpy(test_uri, serverURI, sizeof(test_uri));
    char *upload_pos = strstr(test_uri, "/upload/");
    if (upload_pos) {
        *upload_pos = '\0';
    }
    
    esp_http_client_config_t config = {
        .url = test_uri,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client) {
        esp_http_client_set_method(client, HTTP_METHOD_GET);
        
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG, "Server available, status: %d", status);
        } else {
            ESP_LOGE(TAG, "Server unavailable: %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);
    }
}

// ===== Clear Camera Queue (Drop Oldest Frames) =====
void clear_camera_queue(void)
{
    frame_t *frame;
    while (xQueueReceive(cameraQueue, &frame, 0) == pdTRUE) {
        if (frame) {
            free(frame->data);
            free(frame);
            total_frames_dropped++;
        }
    }
}

// ===== Camera Capture Task =====
void camera_capture_task(void *pvParameters)
{
    const TickType_t xDelay = pdMS_TO_TICKS(200);
    uint32_t frame_counter = 0;
    uint32_t log_counter = 0;
    
    while (1) {
        if (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) {
            gpio_set_level(FLASH_GPIO_NUM, 1);
            vTaskDelay(pdMS_TO_TICKS(FLASH_DELAY_MS));

            camera_fb_t *fb = esp_camera_fb_get();
            gpio_set_level(FLASH_GPIO_NUM, 0);

            if (!fb) {
                ESP_LOGE(TAG, "Camera capture failed");
                vTaskDelay(xDelay);
                continue;
            }

            if (fb->len > MAX_FRAME_SIZE) {
                ESP_LOGW(TAG, "Frame too large (%d > %d), skipping", fb->len, MAX_FRAME_SIZE);
                esp_camera_fb_return(fb);
                vTaskDelay(xDelay);
                continue;
            }

            frame_t *frame = malloc(sizeof(frame_t));
            if (!frame) {
                ESP_LOGE(TAG, "Failed to allocate frame memory");
                esp_camera_fb_return(fb);
                vTaskDelay(xDelay);
                continue;
            }

            frame->data = malloc(fb->len);
            if (!frame->data) {
                ESP_LOGE(TAG, "Failed to allocate frame data");
                free(frame);
                esp_camera_fb_return(fb);
                vTaskDelay(xDelay);
                continue;
            }

            frame->len = fb->len;
            frame->frame_number = frame_counter++;
            memcpy(frame->data, fb->buf, fb->len);
            
            total_frames_captured++;

            if (uxQueueMessagesWaiting(cameraQueue) >= QUEUE_SIZE - 1) {
                ESP_LOGW(TAG, "Queue full, clearing old frames");
                clear_camera_queue();
            }

            if (xQueueSend(cameraQueue, &frame, pdMS_TO_TICKS(50)) != pdTRUE) {
                ESP_LOGW(TAG, "Failed to enqueue frame %u, dropped", frame->frame_number);
                free(frame->data);
                free(frame);
                total_frames_dropped++;
            }

            if (++log_counter % 10 == 0) {
                ESP_LOGI(TAG, "Stats: Captured %u, Sent %u, Dropped %u",
                        total_frames_captured, total_frames_sent, total_frames_dropped);
            }
            
            esp_camera_fb_return(fb);
        } else {
            if (log_counter % 20 == 0) {
                ESP_LOGW(TAG, "Waiting for Wi-Fi connection...");
            }
            log_counter++;
        }
        
        vTaskDelay(xDelay);
    }
}

// ===== Camera Send Task =====
void camera_send_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Send task started");
    uint32_t log_counter = 0;
    
    while (1) {
        frame_t *frame = NULL;
        
        if (xQueueReceive(cameraQueue, &frame, portMAX_DELAY) == pdTRUE) {
            if (frame) {
                if (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) {
                    esp_err_t err = send_photo_http_with_retry(frame->data, frame->len);
                    if (err == ESP_OK) {
                        total_frames_sent++;
                        if (++log_counter % 5 == 0) {
                            ESP_LOGI(TAG, "Frame %u sent, size: %d bytes",
                                    frame->frame_number, frame->len);
                        }
                    }
                }
                
                free(frame->data);
                free(frame);
            }
        }
    }
}

// ===== Main Function =====
void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-CAM Streaming (enhanced version)");
    ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    if (init_sd() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SD card");
        return;
    }
    
    if (!read_config_from_sd()) {
        ESP_LOGE(TAG, "Failed to read configuration");
        return;
    }

    esp_netif_create_default_wifi_sta();
    wifi_init();
    check_server_connection();

    if (camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize camera");
        return;
    }

    gpio_reset_pin(FLASH_GPIO_NUM);
    gpio_set_direction(FLASH_GPIO_NUM, GPIO_MODE_OUTPUT);
    gpio_set_level(FLASH_GPIO_NUM, 0);

    cameraQueue = xQueueCreate(QUEUE_SIZE, sizeof(frame_t*));
    if (!cameraQueue) {
        ESP_LOGE(TAG, "Failed to create camera queue");
        return;
    }

    if (xTaskCreate(camera_capture_task, "camera_capture_task", 15360, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create camera capture task");
        return;
    }
    
    if (xTaskCreate(camera_send_task, "camera_send_task", 15360, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create camera send task");
        return;
    }
    
    ESP_LOGI(TAG, "Application started successfully");
}
