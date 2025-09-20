#include "esp_http_server.h"
#include "common.h"
#include "servo.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include "webserver.h"


static const char *TAG = "WEB_SERVER";

// --- Stream Task ---
void stream_task(void *pvParameters)
{
    char part_buf[512];
    static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
    static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

    ESP_LOGI("TASK", "Started: %s, free stack=%d", pcTaskGetName(NULL), uxTaskGetStackHighWaterMark(NULL));
    uint32_t frame_number = 0;

    while (1) {
        // Ждём Wi-Fi
        if (!(xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT)) {
            ESP_LOGW("STREAM", "Wi-Fi disconnected, waiting...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Ждём подключение клиента
        if (!mjpeg_client.connected || !mjpeg_client.req) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        // Захватываем кадр
        if (xSemaphoreTake(frame_mutex, portMAX_DELAY) == pdTRUE) {
            if (current_frame && current_frame->len > 4) {
                if (current_frame->data[0] == 0xFF && current_frame->data[1] == 0xD8 &&
                    current_frame->data[current_frame->len-2] == 0xFF &&
                    current_frame->data[current_frame->len-1] == 0xD9) {

                    int hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, current_frame->len);
                    esp_err_t res1 = httpd_resp_send_chunk(mjpeg_client.req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
                    esp_err_t res2 = httpd_resp_send_chunk(mjpeg_client.req, part_buf, hlen);
                    esp_err_t res3 = httpd_resp_send_chunk(mjpeg_client.req, (const char*)current_frame->data, current_frame->len);

                    if (res1 == ESP_OK && res2 == ESP_OK && res3 == ESP_OK) {
                        total_frames_sent++;
                        frame_number++;
                        ESP_LOGI("STREAM", "Frame #%u sent, size=%u bytes", frame_number, current_frame->len);
                    } else {
                        ESP_LOGW("STREAM", "Client disconnected, resetting");
                        mjpeg_client.connected = 0;
                        mjpeg_client.req = NULL;
                    }
                }

                free(current_frame->data);
                free(current_frame);
                current_frame = NULL;
            }
            xSemaphoreGive(frame_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// --- MJPEG Handler ---
static esp_err_t stream_handler(httpd_req_t *req)
{
    ESP_LOGI("MJPEG", "Client connected");
    while (!(xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT)) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    mjpeg_client.connected = 1;
    mjpeg_client.req = req;
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");

    while (mjpeg_client.connected) {
        if (httpd_req_to_sockfd(req) < 0) {
            mjpeg_client.connected = 0;
            mjpeg_client.req = NULL;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI("MJPEG", "Client disconnected");
    return ESP_OK;
}

// --- Servo Handler ---
static esp_err_t servo_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret < 0) buf[0] = 0;
    buf[ret] = 0;

    int angle1 = 90, angle2 = 45;
    char *p;
    if ((p = strstr(buf, "angle1="))) angle1 = atoi(p+7);
    if ((p = strstr(buf, "angle2="))) angle2 = atoi(p+7);

    ESP_LOGI("SERVO", "Received servo command: angle1=%d, angle2=%d", angle1, angle2);

    if (angle1 < 0) angle1 = 0; 
    if (angle1 > 180) angle1 = 180;
    if (angle2 < 0) angle2 = 0;
    if (angle2 > 90) angle2 = 90;

    servo_cmd_t cmd = { angle1, angle2 };
    if (xQueueSend(servoQueue, &cmd, 0) != pdTRUE) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_sendstr(req, "{\"status\":\"queue_full\"}");
        return ESP_FAIL;
    }

    char resp[80];
    snprintf(resp, sizeof(resp), "{\"servo1\":%d,\"servo2\":%d,\"status\":\"queued\"}", angle1, angle2);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// --- Status Handler ---
static esp_err_t status_handler(httpd_req_t *req)
{
    char json[512];
    snprintf(json, sizeof(json),
        "{\"frames_captured\":%lu,\"frames_sent\":%lu,\"frames_dropped\":%lu}",
        total_frames_captured, total_frames_sent, total_frames_dropped);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

// --- Start servers ---
// port '81' stream, port `80` control
void start_webserver(void)
{
    // --- Stream Server ---
    httpd_config_t stream_config = HTTPD_DEFAULT_CONFIG();
    stream_config.server_port = 81;
    stream_config.ctrl_port = 8081; 
    stream_config.max_open_sockets = 4;

    esp_err_t res_stream = httpd_start(&stream_server, &stream_config);
    if (res_stream == ESP_OK) {
        ESP_LOGI(TAG, "[STREAM] Server started successfully on port %d", stream_config.server_port);
        httpd_uri_t stream_uri = {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = stream_handler
        };
        httpd_register_uri_handler(stream_server, &stream_uri);
    } else {
        ESP_LOGE(TAG, "[STREAM] Failed to start server on port %d, err=0x%x", stream_config.server_port, res_stream);
    }

    // Небольшая задержка между запуском серверов
    vTaskDelay(pdMS_TO_TICKS(100));

    // --- Control Server ---
    httpd_config_t control_config = HTTPD_DEFAULT_CONFIG();
    control_config.server_port = 80;
    control_config.ctrl_port = 8080; 
    control_config.max_open_sockets = 4;

    esp_err_t res_control = httpd_start(&control_server, &control_config);
    if (res_control == ESP_OK) {
        ESP_LOGI(TAG, "[CONTROL] Server started successfully on port %d", control_config.server_port);
        httpd_uri_t servo_uri  = { .uri="/servo",  .method=HTTP_POST, .handler=servo_handler };
        httpd_uri_t status_uri = { .uri="/status", .method=HTTP_GET,  .handler=status_handler };
        httpd_register_uri_handler(control_server, &servo_uri);
        httpd_register_uri_handler(control_server, &status_uri);
    } else {
        ESP_LOGE(TAG, "[CONTROL] Failed to start server on port %d, err=0x%x", control_config.server_port, res_control);
    }
}

