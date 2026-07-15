idf_component_register(SRCS "main.c" "ble_server.c" "ui.c" "audio.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES nvs_flash bt esp_driver_i2c esp_driver_i2s esp_lcd espressif__esp_lcd_co5300 lvgl__lvgl waveshare__esp_lcd_touch_cst9217 espressif__esp_codec_dev)
