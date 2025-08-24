idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp32-camera
        esp_http_client
        esp_wifi
        nvs_flash
        driver
        fatfs
        vfs
        esp_netif
        mdns
)