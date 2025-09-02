#ifndef SERVO_H
#define SERVO_H

#include "esp_http_server.h"  // здесь уже есть httpd_handle_t

#define SERVO_PIN_1 12
#define SERVO_PIN_2 13

void init_servo_pwm(void);
void set_servo(int pin, int angle, int max_angle);
void start_servo_server(httpd_handle_t server);  // используем тип из esp_http_server.h

#endif