#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_event.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_http_server.h"
#include "esp_camera.h"

// ---------------- Wi-Fi ----------------
#define MAX_WIFI        5
#define SSID_MAX_LEN    32
#define PASS_MAX_LEN    64

typedef struct {
    char ssid[SSID_MAX_LEN];
    char pass[PASS_MAX_LEN];
} wifi_cred_t;

extern wifi_cred_t wifi_list[MAX_WIFI];
extern int wifi_count;


extern volatile int current_angleX;
extern volatile int current_angleY;
extern volatile int target_angleX;
extern volatile int target_angleY;

// ---------------- Frames (double buffer) ----------------
typedef struct {
    camera_fb_t *fb[2];   // два буфера
    int write_index;      // индекс записи
    int read_index;       // индекс чтения
    bool ready[2];        // готовность кадра
} frame_double_buffer_t;

extern frame_double_buffer_t frame_buffer;

extern volatile uint32_t total_frames_captured;
extern volatile uint32_t total_frames_sent;
extern volatile uint32_t total_frames_dropped;

// ---------------- MJPEG Client ----------------
typedef struct {
    int connected;
    httpd_req_t *req;
} mjpeg_client_t;

extern mjpeg_client_t mjpeg_client;

// ---------------- Wi-Fi Event ----------------
extern EventGroupHandle_t wifi_event_group;
extern const EventBits_t WIFI_CONNECTED_BIT;

// ---------------- GPIO ----------------
#define FLASH_GPIO_NUM  4
#define SERVO_PIN_1     12
#define SERVO_PIN_2     13
#define FLASH_DELAY_MS  25   

#define QUEUE_SIZE      2
#define MAX_FRAME_SIZE  (60 * 1024)
#define CONFIG_FILE_PATH "/sdcard/config.txt"

extern httpd_handle_t stream_server;
extern httpd_handle_t control_server;

// ---------------- Tasks ----------------
void camera_capture_task(void *arg);
void servo_task(void *arg);
void stream_task(void *arg);

#endif // COMMON_H
