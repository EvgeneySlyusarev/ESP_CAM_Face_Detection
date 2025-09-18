#include "common.h"

char wifiSSID[64] = {0};
char wifiPASS[64] = {0};

wifi_cred_t wifi_list[MAX_WIFI];
int wifi_count = 0;

volatile uint32_t total_frames_captured = 0;
volatile uint32_t total_frames_sent = 0;
volatile uint32_t total_frames_dropped = 0;

EventGroupHandle_t wifi_event_group = NULL;
const EventBits_t WIFI_CONNECTED_BIT = BIT0;
// MJPEG client
mjpeg_client_t mjpeg_client = {0};

QueueHandle_t cameraQueue = NULL;
QueueHandle_t servoQueue = NULL;

volatile int current_angle1 = 90;
volatile int current_angle2 = 45;
