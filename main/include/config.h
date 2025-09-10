#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include "esp_err.h"

#define MAX_WIFI_ENTRIES 5
#define CONFIG_FILE_PATH "/sdcard/config.txt"

typedef struct {
    char ssid[64];
    char pass[64];
} my_wifi_entry_t;

// --- Конфигурация Wi-Fi ---
extern my_wifi_entry_t wifi_entries[MAX_WIFI_ENTRIES];
extern int wifi_entry_count;

// --- SD & Config ---
esp_err_t init_sd(void);         // возвращает ESP_OK/ESP_FAIL
bool read_config_from_sd(void);  // true/false

#endif