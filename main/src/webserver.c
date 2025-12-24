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
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Ждём подключённого MJPEG клиента
        if (!mjpeg_client.connected || !mjpeg_client.req) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        int idx = frame_buffer.read_index;

        // Если кадр готов
        if (frame_buffer.ready[idx] && frame_buffer.fb[idx]) {
            camera_fb_t *fb = frame_buffer.fb[idx];

            // Проверка JPEG
            if (fb->len > 4 && fb->buf[0] == 0xFF && fb->buf[1] == 0xD8 &&
                fb->buf[fb->len-2] == 0xFF && fb->buf[fb->len-1] == 0xD9) {

                int hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, fb->len);

                // Отправка данных клиенту
                if (httpd_resp_send_chunk(mjpeg_client.req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY)) != ESP_OK ||
                    httpd_resp_send_chunk(mjpeg_client.req, part_buf, hlen) != ESP_OK ||
                    httpd_resp_send_chunk(mjpeg_client.req, (const char*)fb->buf, fb->len) != ESP_OK) {
                    
                    ESP_LOGW("STREAM", "Client disconnected, resetting");
                    mjpeg_client.connected = 0;
                    mjpeg_client.req = NULL;
                } else {
                    total_frames_sent++;
                    frame_number++;
                    ESP_LOGI("STREAM", "Frame #%u sent, size=%u bytes", frame_number, fb->len);
                }
            }

            // Возвращаем буфер обратно в пул камеры
            esp_camera_fb_return(fb);
            frame_buffer.fb[idx] = NULL;
            frame_buffer.ready[idx] = false;

            // Переключаем индекс чтения
            frame_buffer.read_index ^= 1;
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
    char query[64];
    int x = target_angleX;
    int y = target_angleY;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char param[8];
        if (httpd_query_key_value(query, "x", param, sizeof(param)) == ESP_OK)
            x = atoi(param);
        if (httpd_query_key_value(query, "y", param, sizeof(param)) == ESP_OK)
            y = atoi(param);
    }

    // Ограничение
    if (x < 0) x = 0;
    if (x > 180) x = 180;
    if (y < 0) y = 0;
    if (y > 90) y = 90;

    // ✔ Просто обновляем цель
    target_angleX = x;
    target_angleY = y;

    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
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

    // Small delay to ensure the first server starts properly
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
