#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_touch_cst9217.h"
#include "lvgl.h"
#include "ble_server.h"
#include "ui.h"
#include "audio.h"

static const char *TAG = "main";

#define I2C_BUS_PORT            0
#define I2C_SDA_PIN             8
#define I2C_SCL_PIN             7
#define AXP2101_I2C_ADDR        0x34

#define LCD_HOST                SPI2_HOST
#define LCD_PCLK_PIN            0
#define LCD_DATA0_PIN           1
#define LCD_DATA1_PIN           2
#define LCD_DATA2_PIN           3
#define LCD_DATA3_PIN           4
#define LCD_CS_PIN              15

#define TOUCH_INT_PIN           5
#define TOUCH_RST_PIN           11

#define LCD_H_RES               480
#define LCD_V_RES               480
#define LVGL_BUFFER_LINES       20

i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t axp_dev_handle;
static SemaphoreHandle_t refresh_finish_sem = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;

typedef struct {
    esp_lcd_panel_handle_t panel;
    SemaphoreHandle_t sem;
} lcd_flush_ctx_t;

static lcd_flush_ctx_t flush_ctx;

static const co5300_lcd_init_cmd_t custom_lcd_init_cmds[] = {
    {0xFE, (uint8_t []){0x00}, 0, 0},
    {0xC4, (uint8_t []){0x80}, 1, 0},
    {0x35, (uint8_t []){0x00}, 0, 10},
    {0x36, (uint8_t []){0xA0}, 1, 0}, // Hardware rotation: MV=1, MY=1 (Swap XY, Mirror Y)
    {0x53, (uint8_t []){0x20}, 1, 10},
    {0x51, (uint8_t []){0xFF}, 1, 10},
    {0x63, (uint8_t []){0xFF}, 1, 10},
    {0x2A, (uint8_t []){0x00, 0x06, 0x01, 0xDD}, 4, 0},
    {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0xD1}, 4, 0},
    {0x11, (uint8_t []){0x00}, 0, 60},
    {0x29, (uint8_t []){0x00}, 0, 0},
};

static esp_err_t axp_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};
    esp_err_t err = i2c_master_transmit(axp_dev_handle, data, 2, 100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write AXP2101 reg 0x%02X: %s", reg, esp_err_to_name(err));
    }
    return err;
}

static void axp_display_power_cycle(void)
{
    ESP_LOGI(TAG, "Power-cycling display via AXP2101 ALDO3...");
    axp_write_reg(0x94, 0x00);
    vTaskDelay(pdMS_TO_TICKS(100));
    axp_write_reg(0x94, 0x1C);
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void init_pmu(void)
{
    ESP_LOGI(TAG, "Initializing I2C Master Bus...");
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_BUS_PORT,
        .scl_io_num = I2C_SCL_PIN,
        .sda_io_num = I2C_SDA_PIN,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_I2C_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &axp_dev_handle));

    ESP_LOGI(TAG, "Configuring AXP2101 Power Rails...");

    axp_write_reg(0x94, 0x1C);
    axp_write_reg(0x93, 0x1C);
    axp_write_reg(0x92, 0x1C);
    axp_write_reg(0x95, 0x0D);
    axp_write_reg(0x96, 0x1C);
    axp_write_reg(0x97, 0x17);
    axp_write_reg(0x99, 0x1C);
    axp_write_reg(0x9A, 0x1C);
    axp_write_reg(0x82, 0x12);
    axp_write_reg(0x86, 0x15);
    axp_write_reg(0x80, 0x1F);
    axp_write_reg(0x90, 0xFF);
    axp_write_reg(0x91, 0x01);

    ESP_LOGI(TAG, "PMU initialized. Power rails enabled.");
    vTaskDelay(pdMS_TO_TICKS(100));
}

IRAM_ATTR static bool notify_refresh_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;
    xSemaphoreGiveFromISR(refresh_finish_sem, &need_yield);
    return (need_yield == pdTRUE);
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    lcd_flush_ctx_t *ctx = lv_display_get_driver_data(disp);
    esp_lcd_panel_draw_bitmap(ctx->panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    xSemaphoreTake(ctx->sem, portMAX_DELAY);
    lv_display_flush_ready(disp);
}

lv_obj_t *touch_indicator = NULL;

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    static uint16_t last_x = 0;
    static uint16_t last_y = 0;

    esp_lcd_touch_point_data_t point_data[1];
    uint8_t touch_cnt = 0;

    esp_lcd_touch_read_data(touch_handle);
    esp_lcd_touch_get_data(touch_handle, point_data, &touch_cnt, 1);

    if (touch_cnt > 0) {
        last_x = point_data[0].x;
        last_y = point_data[0].y;
        data->point.x = point_data[0].x;
        data->point.y = point_data[0].y;
        data->state = LV_INDEV_STATE_PRESSED;
        if (touch_indicator) {
            lv_obj_set_style_bg_color(touch_indicator, lv_color_hex(0x00FF00), 0);
            lv_obj_set_style_bg_opa(touch_indicator, LV_OPA_COVER, 0);
        }
    } else {
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_RELEASED;
        if (touch_indicator) {
            lv_obj_set_style_bg_color(touch_indicator, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_bg_opa(touch_indicator, LV_OPA_30, 0);
        }
    }
}

static void init_touch(void)
{
    ESP_LOGI(TAG, "Initializing CST9220 touch controller...");

    gpio_config_t int_conf = {
        .pin_bit_mask = BIT64(TOUCH_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&int_conf);

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST9217_CONFIG();
    tp_io_config.scl_speed_hz = 100000;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_handle, &tp_io_config, &tp_io_handle));

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = TOUCH_RST_PIN,
        .int_gpio_num = TOUCH_INT_PIN,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 1,
            .mirror_x = 0,
            .mirror_y = 1,
        },
    };

    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst9217(tp_io_handle, &tp_cfg, &touch_handle));
    ESP_LOGI(TAG, "CST9220 touch controller ready");
}

static void lvgl_init(void)
{
    ESP_LOGI(TAG, "Initializing LVGL...");
    lv_init();

    lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);

    size_t buf_sz = LCD_H_RES * LVGL_BUFFER_LINES * sizeof(lv_color_t);
    lv_color_t *buf1 = heap_caps_malloc(buf_sz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_color_t *buf2 = heap_caps_malloc(buf_sz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(buf1 && buf2);

    lv_display_set_buffers(disp, buf1, buf2, buf_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_driver_data(disp, &flush_ctx);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);

    lv_timer_handler();
    ESP_LOGI(TAG, "LVGL initialized with display and touch drivers");
}

static void lvgl_task(void *arg)
{
    ui_init();

    uint32_t last_telemetry_check = 0;
    while (1) {
        uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
        lv_tick_inc(5);

        uint32_t sleep_ms = lv_timer_handler();
        if (sleep_ms > 5) sleep_ms = 5;
        if (sleep_ms < 1) sleep_ms = 1;

        if (now - last_telemetry_check >= 200) {
            ble_telemetry_data_t data;
            ble_server_get_telemetry(&data);
            if (data.updated) {
                ui_update_telemetry(&data);
                ble_server_clear_update_flag();
            }
            last_telemetry_check = now;
        }

        vTaskDelay(pdMS_TO_TICKS(sleep_ms));
    }
}

void app_main(void)
{
    init_pmu();

    ESP_LOGI(TAG, "Initializing QSPI bus...");
    const spi_bus_config_t buscfg = CO5300_PANEL_BUS_QSPI_CONFIG(
        LCD_PCLK_PIN,
        LCD_DATA0_PIN,
        LCD_DATA1_PIN,
        LCD_DATA2_PIN,
        LCD_DATA3_PIN,
        LCD_H_RES * LVGL_BUFFER_LINES * sizeof(uint16_t)
    );
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Installing LCD panel IO...");
    refresh_finish_sem = xSemaphoreCreateBinary();
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = CO5300_PANEL_IO_QSPI_CONFIG(
        LCD_CS_PIN,
        notify_refresh_ready,
        NULL
    );
    io_config.pclk_hz = 20 * 1000 * 1000; // Lower QSPI clock from 40MHz to 20MHz to prevent signal reflections and slanted/italicized display distortion
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Installing CO5300 display driver...");
    const co5300_vendor_config_t vendor_config = {
        .init_cmds = custom_lcd_init_cmds,
        .init_cmds_size = sizeof(custom_lcd_init_cmds) / sizeof(co5300_lcd_init_cmd_t),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = (void *)&vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(io_handle, &panel_config, &panel_handle));

    axp_display_power_cycle();

    ESP_LOGI(TAG, "Initializing display...");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    vTaskDelay(pdMS_TO_TICKS(100));

    flush_ctx.panel = panel_handle;
    flush_ctx.sem = refresh_finish_sem;

    ESP_LOGI(TAG, "Display initialized successfully!");

    init_touch();

    lvgl_init();

    audio_init();

    ESP_LOGI(TAG, "Starting BLE server...");
    ble_server_init();

    ESP_LOGI(TAG, "Starting LVGL UI task...");
    xTaskCreate(lvgl_task, "lvgl_ui", 10240, NULL, 5, NULL);
}
