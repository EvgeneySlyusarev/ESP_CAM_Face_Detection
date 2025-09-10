// globals.c
#include "common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

// Wi-Fi конфигурация
char wifiSSID[64] = {0};
char wifiPASS[64] = {0};

// Статистика
uint32_t total_frames_captured = 0;
uint32_t total_frames_sent = 0;
uint32_t total_frames_dropped = 0;

// Кадры
frame_t last_frame = { .data = NULL, .len = 0, .frame_number = 0 };
SemaphoreHandle_t frame_mutex = NULL;

// Wi-Fi
EventGroupHandle_t wifi_event_group = NULL;
const EventBits_t WIFI_CONNECTED_BIT = BIT0;

// Многозадачность
QueueHandle_t servo_queue = NULL;
TaskHandle_t video_task_handle = NULL;
TaskHandle_t servo_task_handle = NULL;
int current_angle1 = 90;
int current_angle2 = 45;
SemaphoreHandle_t camera_mutex = NULL;

// Конфигурация Wi-Fi
my_wifi_entry_t wifi_entries[MAX_WIFI_ENTRIES] = {0};
int wifi_entry_count = 0;