#!/bin/bash

# ESP32 прошивка на скорости 115200 bps
cd "$(dirname "$0")" || exit 1

export IDF_PATH=/home/yevhens/esp-idf-v5.5
. "$IDF_PATH/export.sh"

idf.py -p /dev/ttyUSB0 -b 115200 flash

echo "Прошивка завершена"
