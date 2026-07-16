#!/bin/bash
# flash.sh - Quick script to flash the ESP32 AI Monitor firmware

PORT=${1:-/dev/cu.usbmodem101}

echo "Sourcing ESP-IDF v6.0.1 environment..."
if [ -f "/Users/wangheng/.espressif/v6.0.1/esp-idf/export.sh" ]; then
    . /Users/wangheng/.espressif/v6.0.1/esp-idf/export.sh
else
    echo "Error: ESP-IDF export script not found at standard path."
    exit 1
fi

echo "Flashing firmware to port: $PORT..."
idf.py -p "$PORT" flash
