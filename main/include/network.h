#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include "esp_err.h"


#define MAX_WIFI_ENTRIES 5

typedef struct {
    char ssid[64];
    char pass[64];
} wifi_entry_t;

extern wifi_entry_t wifi_entries[MAX_WIFI_ENTRIES];
extern int wifi_entry_count;


bool read_config_from_sd(void);
esp_err_t init_sd(void);

#endif
