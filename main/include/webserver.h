#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_http_server.h"

// Запуск HTTP-сервера с потоковой камерой, управлением серво и статусом
void start_webserver(void);

#endif // WEBSERVER_H