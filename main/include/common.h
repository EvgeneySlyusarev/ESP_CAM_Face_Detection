#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

// --- Конфигурационные переменные ---
extern char wifiSSID[64];
extern char wifiPASS[64];

// --- Статистика ---
extern uint32_t total_frames_captured;
extern uint32_t total_frames_sent;
extern uint32_t total_frames_dropped;

// --- Структура для кадров ---
typedef struct { 
    uint8_t *data;
    size_t len;
    uint32_t frame_number;
} frame_t;

// --- Глобальный буфер последнего кадра ---
extern frame_t last_frame;              // единый объект
extern SemaphoreHandle_t frame_mutex;   // мьютекс защиты доступа

// --- GPIO ---
#define FLASH_GPIO_NUM    4
#define SERVO_PIN_1       12
#define SERVO_PIN_2       13

// --- Константы ---
#define MAX_FRAME_SIZE (60 * 1024)
#define FLASH_DELAY_MS 25
#define CONFIG_FILE_PATH "/sdcard/config.txt"

// --- Wi-Fi Event Group ---
extern EventGroupHandle_t wifi_event_group;  
extern const EventBits_t WIFI_CONNECTED_BIT;

#endif
