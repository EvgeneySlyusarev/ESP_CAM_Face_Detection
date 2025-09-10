#include "esp_http_server.h"
#include "esp_camera.h"
#include "common.h"
#include "servo.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "WEB_SERVER";

httpd_handle_t server = NULL;


// Структура для команд сервомотора
typedef struct {
    int angle1;
    int angle2;
} servo_command_t;

// === Задача для управления сервомоторами ===
static void servo_task(void *pvParameters) {
    servo_command_t cmd;
    
    while (1) {
        // Ждем команды из очереди
        if (xQueueReceive(servo_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            // Устанавливаем сервоприводы
            set_servo(SERVO_PIN_1, cmd.angle1, 180);
            set_servo(SERVO_PIN_2, cmd.angle2, 90);
            
            ESP_LOGI(TAG, "Servo angles set: %d, %d", cmd.angle1, cmd.angle2);
        }
    }
}

// === MJPEG Stream Handler ===
static esp_err_t stream_handler(httpd_req_t *req) {
    char part_buf[256];
    esp_err_t res = ESP_OK;
    camera_fb_t *fb = NULL;

    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");

    // Оптимизация: снижаем качество для уменьшения нагрузки
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_framesize(s, FRAMESIZE_VGA);  // 640x480 вместо 800x600
        s->set_quality(s, 12);               // Качество JPEG
    }

    while (true) {
        // Включаем подсветку
        gpio_set_level(FLASH_GPIO_NUM, 1);
        vTaskDelay(pdMS_TO_TICKS(FLASH_DELAY_MS));

        // Получаем кадр
        fb = esp_camera_fb_get();
        gpio_set_level(FLASH_GPIO_NUM, 0);

        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Формируем заголовок
        int header_len = snprintf(part_buf, sizeof(part_buf),
                                  "--frame\r\n"
                                  "Content-Type: image/jpeg\r\n"
                                  "Content-Length: %u\r\n\r\n",
                                  (unsigned int)fb->len);

        // Отправляем данные
        res = httpd_resp_send_chunk(req, part_buf, header_len);
        if (res != ESP_OK) break;

        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        if (res != ESP_OK) break;

        res = httpd_resp_send_chunk(req, "\r\n", 2);
        if (res != ESP_OK) break;

        total_frames_sent++;
        total_frames_captured++;

        // Освобождаем кадр
        esp_camera_fb_return(fb);
        fb = NULL;

        // Небольшая задержка для снижения FPS
        vTaskDelay(pdMS_TO_TICKS(50)); // ~20 FPS
    }

    if (fb) esp_camera_fb_return(fb);
    ESP_LOGI(TAG, "Client disconnected from stream");
    return res;
}

// === Servo Handler ===
static esp_err_t servo_handler(httpd_req_t *req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    
    if (ret <= 0) {
        httpd_resp_send_408(req);
        return ESP_FAIL;
    }
    buf[ret] = 0;

    int angle1 = current_angle1;
    int angle2 = current_angle2;
    
    // Парсим данные
    sscanf(buf, "angle1=%d&angle2=%d", &angle1, &angle2);

    // Ограничиваем значения
    angle1 = (angle1 < 0) ? 0 : (angle1 > 180) ? 180 : angle1;
    angle2 = (angle2 < 0) ? 0 : (angle2 > 90) ? 90 : angle2;

    // Отправляем команду в очередь (неблокирующе)
    servo_command_t cmd = {angle1, angle2};
    if (servo_queue != NULL) {
        xQueueSend(servo_queue, &cmd, 0);
    }

    // Немедленный ответ клиенту
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"servo1\":%d,\"servo2\":%d,\"status\":\"queued\"}", angle1, angle2);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}

// === Start HTTP Server ===
void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true; // Важно для освобождения соединений

    // Создаем очередь для команд сервомотора
    servo_queue = xQueueCreate(10, sizeof(servo_command_t));
    
    if (servo_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create servo queue");
        return;
    }

    // Запускаем задачу управления сервомоторами
    xTaskCreate(servo_task, "servo_task", 2048, NULL, 3, NULL); // Высокий приоритет

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);

        httpd_uri_t servo_uri = { 
            .uri = "/servo", 
            .method = HTTP_POST, 
            .handler = servo_handler, 
            .user_ctx = NULL 
        };
        httpd_register_uri_handler(server, &servo_uri);

        httpd_uri_t stream_uri = { 
            .uri = "/stream", 
            .method = HTTP_GET, 
            .handler = stream_handler, 
            .user_ctx = NULL 
        };
        httpd_register_uri_handler(server, &stream_uri);

    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        vQueueDelete(servo_queue);
        servo_queue = NULL;
    }
}

// === Stop HTTP Server ===
void stop_webserver(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
    if (servo_queue) {
        vQueueDelete(servo_queue);
        servo_queue = NULL;
    }
}