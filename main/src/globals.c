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

// Servo queue and positions
QueueHandle_t servoQueue = NULL;
volatile int current_angleX = 90;
volatile int current_angleY = 45;

// Двойной буфер для кадров камеры
frame_double_buffer_t frame_buffer = {
    .fb = { NULL, NULL },
    .write_index = 0,
    .read_index  = 1,
    .ready = { false, false }
};

// HTTP server handles
httpd_handle_t stream_server = NULL;
httpd_handle_t control_server = NULL;
