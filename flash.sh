#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

echo "========================================="
echo "  ESP32-C6 AI Monitor Flash Tool"
echo "  Target Port: /dev/cu.usbmodem101"
echo "========================================="

# 1. Source ESP-IDF v6.0.1 environment
IDF_EXPORT_PATH="/Users/wangheng/.espressif/v6.0.1/esp-idf/export.sh"

if [ -f "$IDF_EXPORT_PATH" ]; then
    echo "Activating ESP-IDF environment..."
    # Sourcing export.sh in bash/zsh
    . "$IDF_EXPORT_PATH"
else
    echo "Error: ESP-IDF export script not found at: $IDF_EXPORT_PATH"
    exit 1
fi

# 2. Build and flash the project, then open serial monitor
echo "Building and Flashing to /dev/cu.usbmodem101..."
idf.py -p /dev/cu.usbmodem101 flash monitor
