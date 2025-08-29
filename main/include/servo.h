#ifndef SERVO_H
#define SERVO_H

#include "common.h"

void init_servo_pwm(void);
void set_servo(int pin, int angle, int max_angle);
void start_servo_server(void);

#endif