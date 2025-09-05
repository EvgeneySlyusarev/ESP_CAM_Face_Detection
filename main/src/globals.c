#include "common.h"

// --- Последний кадр ---
frame_t last_frame = { .data = NULL, .len = 0, .frame_number = 0 };
SemaphoreHandle_t frame_mutex = NULL;

// --- Статистика ---
uint32_t total_frames_captured = 0;
uint32_t total_frames_sent = 0;
uint32_t total_frames_dropped = 0;

// --- Wi-Fi ---
EventGroupHandle_t wifi_event_group;
const EventBits_t WIFI_CONNECTED_BIT = BIT0;

// --- Wi-Fi данные ---
char wifiSSID[64] = {0};
char wifiPASS[64] = {0};
