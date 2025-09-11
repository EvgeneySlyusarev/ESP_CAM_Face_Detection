idf_component_register(
    SRCS "main.c"
         "src/config.c"
         "src/network.c"
         "src/camera.c"
         "src/servo.c"
         "src/webserver.c"
         "src/globals.c"
    INCLUDE_DIRS "include"
    REQUIRES esp32-camera esp_http_server esp_http_client fatfs esp_netif esp_event esp_wifi nvs_flash mdns
)

# Оптимизации для уменьшения IRAM использования
target_compile_options(${COMPONENT_LIB} PRIVATE
    "-ffunction-sections"
    "-fdata-sections"
    "-fno-ira-loop-pressure"
    "-fno-ira-share-save-slots"
    "-fno-ira-share-spill-slots"
)

target_link_options(${COMPONENT_LIB} PRIVATE
    "-Wl,--gc-sections"
)