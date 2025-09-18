#include "common.h"

char wifiSSID[64] = {0};
char wifiPASS[64] = {0};

volatile uint32_t total_frames_captured = 0;
volatile uint32_t total_frames_sent = 0;
volatile uint32_t total_frames_dropped = 0;

EventGroupHandle_t wifi_event_group = NULL;
const EventBits_t WIFI_CONNECTED_BIT = BIT0;

QueueHandle_t cameraQueue = NULL;
QueueHandle_t servoQueue = NULL;

volatile int current_angle1 = 90;
volatile int current_angle2 = 45;
