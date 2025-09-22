#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"
#include "esp_err.h"

bool read_config_from_sd(void);
esp_err_t init_sd(void);   // <- эта строка обязательна

#endif
