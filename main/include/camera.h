#ifndef CAMERA_H
#define CAMERA_H

#include "common.h"

extern QueueHandle_t cameraQueue;

esp_err_t camera_init(void);
void camera_capture_task(void *pvParameters);
void camera_send_task(void *pvParameters);
void clear_camera_queue(void);

#endif