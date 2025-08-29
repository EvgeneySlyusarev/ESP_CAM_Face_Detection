#include "config.h"
#include "common.h"

bool read_config_from_sd(void)
{
    FILE *f = fopen(CONFIG_FILE_PATH, "r");
    if (!f) {
        ESP_LOGE("CONFIG", "Failed to open config.txt");
        return false;
    }

    memset(wifiSSID, 0, sizeof(wifiSSID));
    memset(wifiPASS, 0, sizeof(wifiPASS));
    memset(serverURI, 0, sizeof(serverURI));

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }

        if (strncmp(line, "WIFI_SSID=", 10) == 0) {
            strncpy(wifiSSID, line + 10, sizeof(wifiSSID) - 1);
        } else if (strncmp(line, "WIFI_PASS=", 10) == 0) {
            strncpy(wifiPASS, line + 10, sizeof(wifiPASS) - 1);
        } else if (strncmp(line, "SERVER_URI=", 11) == 0) {
            strncpy(serverURI, line + 11, sizeof(serverURI) - 1);
        }
    }

    fclose(f);
    
    if (strlen(wifiSSID) == 0 || strlen(wifiPASS) == 0 || strlen(serverURI) == 0) {
        ESP_LOGE("CONFIG", "Missing required parameters in config");
        return false;
    }
    
    ESP_LOGI("CONFIG", "Config loaded: SSID='%s', URI='%s'", wifiSSID, serverURI);
    return true;
}

esp_err_t init_sd(void)
{
    ESP_LOGI("CONFIG", "Initializing SD card (SDMMC 1-bit mode)...");

    // SDMMC host
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // SDMMC slot config
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;   // ESP32-CAM поддерживает только 1-битный режим
    // если нужно – можно указать конкретные GPIO, но для ESP32-CAM дефолтные правильные

    // FAT FS mount config
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

    // Вывести инфу о карте
    sdmmc_card_print_info(stdout, card);

    // Авто-unmount при ресете (опционально)
    // esp_register_shutdown_handler((shutdown_handler_t)esp_vfs_fat_sdcard_unmount);

    return ESP_OK;
}

