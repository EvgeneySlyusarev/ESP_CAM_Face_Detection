#ifndef SERVO_H
#define SERVO_H

#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define SERVO_PIN_1 12
#define SERVO_PIN_2 13

void init_servo_pwm(void);
void set_servo(int pin, int angle, int max_angle);
void servo_task(void *pvParameters);

#endif
