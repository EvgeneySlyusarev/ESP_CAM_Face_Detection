#include "config.h"
#include <stdio.h>
#include <string.h>
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "common.h"
#include "esp_log.h"


bool read_config_from_sd(void)
{
    FILE *f = fopen(CONFIG_FILE_PATH, "r");
    if (!f) {
        ESP_LOGE("CONFIG", "Failed to open %s", CONFIG_FILE_PATH);
        return false;
    }

    wifi_entry_count = 0;
    char line[256];
    my_wifi_entry_t current = {0};

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;  // убрать \n

        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }

        if (strncmp(line, "WIFI_SSID=", 10) == 0) {
            strncpy(current.ssid, line + 10, sizeof(current.ssid) - 1);
            current.ssid[sizeof(current.ssid) - 1] = '\0';
        } else if (strncmp(line, "WIFI_PASS=", 10) == 0) {
            strncpy(current.pass, line + 10, sizeof(current.pass) - 1);
            current.pass[sizeof(current.pass) - 1] = '\0';

            // закончили пару — сохраняем
            if (wifi_entry_count < MAX_WIFI_ENTRIES) {
                wifi_entries[wifi_entry_count++] = current;
                memset(&current, 0, sizeof(current));
            }
        }
    }

    fclose(f);

    if (wifi_entry_count == 0) {
        ESP_LOGE("CONFIG", "No WiFi entries found in config");
        return false;
    }

    ESP_LOGI("CONFIG", "Loaded %d WiFi entries", wifi_entry_count);
    for (int i = 0; i < wifi_entry_count; i++) {
        ESP_LOGI("CONFIG", "[%d] SSID='%s' PASS='%s'",
                 i, wifi_entries[i].ssid, wifi_entries[i].pass);
    }

    return true;
}

esp_err_t init_sd(void)
{
    ESP_LOGI("CONFIG", "Initializing SD card (SDMMC 1-bit mode)...");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;   // ESP32-CAM поддерживает только 1-битный режим

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 3,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE("CONFIG", "Failed to mount SD card: %s", esp_err_to_name(ret));
        return ret;
    }

    sdmmc_card_print_info(stdout, card);
    return ESP_OK;
}
