#include "esp_http_server.h"
#include "esp_camera.h"
#include "common.h"
#include "servo.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WEB_SERVER";

// Global HTTP server handle (declared in header or common)
httpd_handle_t server = NULL;

// === MJPEG Stream Handler ===
static esp_err_t stream_handler(httpd_req_t *req)
{
    esp_err_t res = ESP_OK;
    char part_buf[64];

    static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
    static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
    static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    while (true) {
        frame_t *frame = NULL;

        // Берём кадр из очереди (блокирующе, но с таймаутом)
        if (xQueueReceive(cameraQueue, &frame, pdMS_TO_TICKS(500)) != pdTRUE) {
            // Нет кадра в очереди, можно подождать или выйти
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (!frame) continue; // на всякий случай

        // Формируем заголовок части MJPEG
        size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, frame->len);

        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)frame->data, frame->len);

        // Освобождаем память кадра после отправки
        free(frame->data);
        free(frame);

        if (res != ESP_OK) break;

        // Optional: небольшой yield, чтобы другие задачи работали
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return res;
}


// === Servo Control Handler (enqueue only) ===
static esp_err_t servo_handler(httpd_req_t *req)
{
    // Read body or query - accept both ways
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret < 0) {
        // Maybe no body; try query string
        buf[0] = 0;
        if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
            // ok
            ret = strlen(buf);
        } else {
            httpd_resp_send_408(req);
            return ESP_FAIL;
        }
    }
    buf[ret] = 0;

    // parse angle1 and angle2 from either body "angle1=..&angle2=.." or query
    int angle1 = 90, angle2 = 45;
    char *p;

    p = strstr(buf, "angle1=");
    if (p) angle1 = atoi(p + 7);
    p = strstr(buf, "angle2=");
    if (p) angle2 = atoi(p + 7);

    // clip
    if (angle1 < 0) angle1 = 0;
    if (angle1 > 180) angle1 = 180;
    if (angle2 < 0) angle2 = 0;
    if (angle2 > 90) angle2 = 90;

    // Enqueue command (non-blocking)
    servo_cmd_t cmd;
    cmd.angle1 = angle1;
    cmd.angle2 = angle2;

    BaseType_t ok = xQueueSend(servoQueue, &cmd, 0);
    if (ok != pdTRUE) {
        // queue full
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_sendstr(req, "{\"status\":\"queue_full\"}");
        return ESP_FAIL;
    }

    // Respond that command queued
    char resp[80];
    snprintf(resp, sizeof(resp), "{\"servo1\":%d,\"servo2\":%d,\"status\":\"queued\"}", angle1, angle2);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}

// === Status Handler (JSON) ===
static esp_err_t status_handler(httpd_req_t *req)
{
    char json[256];

    // Build JSON
    snprintf(json, sizeof(json),
        "{"
        "\"wifi\":\"%s\","
        "\"frames_captured\":%lu,"
        "\"frames_sent\":%lu,"
        "\"frames_dropped\":%lu"
        "}",
        wifiSSID, total_frames_captured, total_frames_sent, total_frames_dropped);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

// === Start HTTP Server ===
void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);

        // Register URIs
        httpd_uri_t stream_uri = {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = stream_handler,
            .user_ctx = NULL
        };
        httpd_uri_t servo_uri = {
            .uri = "/servo",
            .method = HTTP_POST,
            .handler = servo_handler,
            .user_ctx = NULL
        };
        httpd_uri_t status_uri = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = status_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(server, &stream_uri);
        httpd_register_uri_handler(server, &servo_uri);
        httpd_register_uri_handler(server, &status_uri);
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}
