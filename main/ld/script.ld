/* Пользовательский скрипт линковки для выноса функций из IRAM */
SECTIONS {
    .non_iram : {
        *(.text.non_iram)
        *(.text.process_frame)
        *(.text.camera_capture_task)
        *(.text.esp_camera_*)
        *(.text.gpio_*)
        *(.text.httpd_*)
        . = ALIGN(4);
    } > iram0_0_text
}