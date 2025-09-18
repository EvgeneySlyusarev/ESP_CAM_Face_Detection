#include "esp_http_server.h"
#include "common.h"
#include "servo.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include "webserver.h"

static const char *TAG = "WEB_SERVER";
httpd_handle_t server = NULL;

// --- Stream Task ---
void stream_task(void *arg)
{
    frame_t *frame;
    char part_buf[64];
    static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
    static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

    start_webserver(); 

    while (true) {
        if (xQueueReceive(cameraQueue, &frame, portMAX_DELAY) == pdTRUE) {
            if (mjpeg_client.connected && mjpeg_client.req) {
                int hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, frame->len);
                esp_err_t res;
                res = httpd_resp_send_chunk(mjpeg_client.req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
                if (res == ESP_OK) res = httpd_resp_send_chunk(mjpeg_client.req, part_buf, hlen);
                if (res == ESP_OK) res = httpd_resp_send_chunk(mjpeg_client.req, (const char*)frame->data, frame->len);

                if (res == ESP_OK) total_frames_sent++;
                else mjpeg_client.connected = 0; // клиент отключился
            }

            free(frame->data);
            free(frame);
        }
    }
}

// --- MJPEG Handler ---
static esp_err_t stream_handler(httpd_req_t *req)
{
    mjpeg_client.connected = 1;
    mjpeg_client.req = req;
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");

    while(mjpeg_client.connected) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return ESP_OK;
}

// --- Servo Handler ---
static esp_err_t servo_handler(httpd_req_t *req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret < 0) buf[0] = 0;
    buf[ret] = 0;

    int angle1 = 90, angle2 = 45;
    char *p;
    if ((p = strstr(buf, "angle1="))) angle1 = atoi(p+7);
    if ((p = strstr(buf, "angle2="))) angle2 = atoi(p+7);

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
    char json[256];
    snprintf(json, sizeof(json),
        "{\"frames_captured\":%lu,\"frames_sent\":%lu,\"frames_dropped\":%lu}",
        total_frames_captured, total_frames_sent, total_frames_dropped);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

// --- Start Server ---
void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);

        httpd_uri_t stream_uri = { .uri="/stream", .method=HTTP_GET, .handler=stream_handler };
        httpd_uri_t servo_uri  = { .uri="/servo", .method=HTTP_POST, .handler=servo_handler };
        httpd_uri_t status_uri = { .uri="/status", .method=HTTP_GET, .handler=status_handler };

        httpd_register_uri_handler(server, &stream_uri);
        httpd_register_uri_handler(server, &servo_uri);
        httpd_register_uri_handler(server, &status_uri);
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}
