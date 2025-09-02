#ifndef NETWORK_H
#define NETWORK_H

#include "common.h"

// Глобальный хэндл EventGroup уже объявлен в common.h
// Бит успешного соединения
extern const EventBits_t WIFI_CONNECTED_BIT;

// Инициализация Wi-Fi
void wifi_init(void);

// Функция отправки фото через HTTP
esp_err_t send_photo_http(const uint8_t *image_data, size_t image_len);

#endif
