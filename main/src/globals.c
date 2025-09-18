// globals.c
#include "common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Wi-Fi конфигурация
char wifiSSID[64] = {0};
char wifiPASS[64] = {0};

// Статистика
volatile uint32_t total_frames_captured = 0;
volatile uint32_t total_frames_sent = 0;
volatile uint32_t total_frames_dropped = 0;

// Wi-Fi
EventGroupHandle_t wifi_event_group = NULL;
const EventBits_t WIFI_CONNECTED_BIT = BIT0;

// Многозадачность
QueueHandle_t servo_queue = NULL;
volatile int current_angle1 = 90;
volatile int current_angle2 = 45;