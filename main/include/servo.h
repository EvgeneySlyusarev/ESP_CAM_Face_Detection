#pragma once
#include "driver/ledc.h"

// Глобальные переменные для хранения текущих углов
extern int current_angle1;
extern int current_angle2;

void init_servo_pwm(void);
void set_servo(int pin, int angle, int max_angle);
