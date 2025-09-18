#ifndef CAMERA_H
#define CAMERA_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "common.h" 

// === Camera pins config ===
typedef struct {
    int pin_pwdn;
    int pin_reset;
    int pin_xclk;
    int pin_sccb_sda;
    int pin_sccb_scl;
    int pin_d7;
    int pin_d6;
    int pin_d5;
    int pin_d4;
    int pin_d3;
    int pin_d2;
    int pin_d1;
    int pin_d0;
    int pin_vsync;
    int pin_href;
    int pin_pclk;
    int flash_gpio;
} camera_pins_t;


// === Functions ===
esp_err_t camera_init(const camera_pins_t *pins);  // Initialize camera with pins
void camera_capture_task(void *pv);                // Capture loop task
void clear_camera_queue(void);                     // Cleanup frames from queue

#endif // CAMERA_H
