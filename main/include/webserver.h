#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "common.h" 

// Структура для команд сервомотора
typedef struct {
    int angle1;
    int angle2;
} servo_command_t;

// Запуск HTTP-сервера с поддержкой многозадачности
void start_webserver(void);

// Остановка HTTP-сервера и освобождение ресурсов
void stop_webserver(void);

// Задачи для многозадачной работы
void video_stream_task(void *pvParameters);
void servo_control_task(void *pvParameters);

// Обработчики HTTP запросов
esp_err_t stream_handler(httpd_req_t *req);
esp_err_t servo_handler(httpd_req_t *req);
esp_err_t status_handler(httpd_req_t *req);

// Утилиты для работы с очередями
BaseType_t send_servo_command(int angle1, int angle2, TickType_t timeout);

#endif // WEBSERVER_H