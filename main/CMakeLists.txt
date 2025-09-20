idf_component_register(
    SRCS "main.c"
         "src/globals.c"
         "src/config.c"
         "src/network.c"
         "src/camera.c"
         "src/servo.c"
         "src/webserver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp32-camera esp_http_server esp_http_client fatfs esp_netif esp_event esp_wifi nvs_flash mdns
)
