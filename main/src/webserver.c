#include "esp_http_server.h"
#include "esp_camera.h"
#include "common.h"
#include "servo.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "WEB_SERVER";

// Глобальный HTTP сервер
httpd_handle_t server = NULL;

// === MJPEG Stream Handler ===
static esp_err_t stream_handler(httpd_req_t *req) {
    esp_err_t res = ESP_OK;
    char part_buf[64];

    static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
    static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
    static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    while (true) {
        if (xSemaphoreTake(frame_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (last_frame.data && last_frame.len > 0) {
                size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, last_frame.len);

                res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
                if (res == ESP_OK) res = httpd_resp_send_chunk(req, part_buf, hlen);
                if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)last_frame.data, last_frame.len);
            }
            xSemaphoreGive(frame_mutex);
        }

        if (res != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(200)); // ограничиваем FPS ~5
    }

    return res;
}


// === Servo Control Handler ===
static esp_err_t servo_handler(httpd_req_t *req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret <= 0) {
        httpd_resp_send_408(req);
        return ESP_FAIL;
    }
    buf[ret] = 0;

    int angle1 = 90, angle2 = 45;
    sscanf(buf, "angle1=%d&angle2=%d", &angle1, &angle2);

    if (angle1 < 0) angle1 = 0;
    if (angle1 > 180) angle1 = 180;
    if (angle2 < 0) angle2 = 0;
    if (angle2 > 90) angle2 = 90;

    set_servo(SERVO_PIN_1, angle1, 180);
    set_servo(SERVO_PIN_2, angle2, 90);

    char resp[64];
    snprintf(resp, sizeof(resp), "{\"servo1\":%d,\"servo2\":%d,\"status\":\"ok\"}", angle1, angle2);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}

// === Status Handler (JSON) ===
static esp_err_t status_handler(httpd_req_t *req) {
    char json[256];
    sprintf(json,
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
void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);

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
