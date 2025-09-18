#ifndef COMMON_H
#define COMMON_H

// Standard
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

// ESP-IDF common
#include "esp_err.h"
#include "esp_event.h"

// SD / FS (если используешь SD)
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"


#include "esp_http_server.h"

// -----------------------------------------------------------------------------
// Global configuration / shared variables (ONLY declarations - use extern here)
// Actual definitions must be in a single .c file (e.g. globals.c)
// -----------------------------------------------------------------------------

// Wi-Fi credentials (filled from config)
extern char wifiSSID[64];
extern char wifiPASS[64];

// Queues (создаются в app/main и определяются в globals.c)
extern QueueHandle_t cameraQueue;
extern QueueHandle_t servoQueue;

// Servo positions (shared state)
extern volatile int current_angle1;
extern volatile int current_angle2;

// Statistics (использовать volatile если изменяются из разных тасков/ISR)
extern volatile uint32_t total_frames_captured;
extern volatile uint32_t total_frames_sent;
extern volatile uint32_t total_frames_dropped;

// Wi-Fi event group
extern EventGroupHandle_t wifi_event_group;
extern const EventBits_t WIFI_CONNECTED_BIT;

// -----------------------------------------------------------------------------
// Types
// -----------------------------------------------------------------------------
typedef struct {
    uint8_t *data;        // pointer to JPEG buffer (heap)
    size_t    len;        // length in bytes
    uint32_t  frame_number;
} frame_t;

// Servo command type (если используешь очередь команд серво)
typedef struct {
    int angle1;
    int angle2;
} servo_cmd_t;

// MJPEG client type (для stream_task)
typedef struct {
    httpd_req_t *req;
    int connected;
} mjpeg_client_t;

// -----------------------------------------------------------------------------
// GPIO / constants
// -----------------------------------------------------------------------------
#define FLASH_GPIO_NUM    4
#define SERVO_PIN_1       12
#define SERVO_PIN_2       13

#define QUEUE_SIZE        10
#define MAX_FRAME_SIZE    (60 * 1024)   // максимальный размер кадра (байт)
#define FLASH_DELAY_MS    25
#define CONFIG_FILE_PATH  "/sdcard/config.txt"

// Tasks
void camera_capture_task(void *arg);
void servo_task(void *arg);
void stream_task(void *arg);

#endif // COMMON_H
