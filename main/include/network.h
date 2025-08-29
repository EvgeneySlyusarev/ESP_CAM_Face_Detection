#ifndef NETWORK_H
#define NETWORK_H

#include "common.h"

extern EventGroupHandle_t wifi_event_group;
extern const int WIFI_CONNECTED_BIT;

void wifi_init(void);
esp_err_t send_photo_http_with_retry(uint8_t *image_data, size_t image_len);

#endif