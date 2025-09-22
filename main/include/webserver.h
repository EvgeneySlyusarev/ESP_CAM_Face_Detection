#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "common.h" 

// Запуск HTTP-сервера с поддержкой многозадачности
void start_webserver(void);

// Остановка HTTP-сервера и освобождение ресурсов
void stop_webserver(void);
#endif // WEBSERVER_H
