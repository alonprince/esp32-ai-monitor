#include "audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include <string.h>
#include <math.h>

extern i2c_master_bus_handle_t i2c_bus_handle;

static const char *TAG = "audio";

#define I2S_PORT            I2S_NUM_0
#define SAMPLE_RATE         44100
#define ES8311_ADDR         0x18

#define I2S_MCLK_IO         19
#define I2S_BCLK_IO         20
#define I2S_LRCLK_IO        22
#define I2S_DOUT_IO         23
#define I2S_DIN_IO          21

static esp_codec_dev_handle_t codec_out = NULL;
static i2c_master_dev_handle_t es8311_i2c_dev = NULL;

static esp_err_t es8311_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};
    return i2c_master_transmit(es8311_i2c_dev, data, 2, 100);
}

static void es8311_init_manual(void)
{
    es8311_write_reg(0x00, 0x1F);
    vTaskDelay(pdMS_TO_TICKS(30));
    es8311_write_reg(0x00, 0x00);

    es8311_write_reg(0x01, 0x06);
    es8311_write_reg(0x02, 0x00);
    es8311_write_reg(0x03, 0xFC);
    es8311_write_reg(0x04, 0x01);
    es8311_write_reg(0x05, 0x71);

    es8311_write_reg(0x06, 0x00);

    es8311_write_reg(0x14, 0x20);
    es8311_write_reg(0x15, 0x20);
    es8311_write_reg(0x16, 0x00);
    es8311_write_reg(0x18, 0xC0);

    es8311_write_reg(0x19, 0x04);

    es8311_write_reg(0x2B, 0x00);

    es8311_write_reg(0x2C, 0x00);
    es8311_write_reg(0x2D, 0x00);

    ESP_LOGI(TAG, "ES8311 codec initialized manually");
}

static void write_pcm(const int16_t *samples, size_t num_samples)
{
    esp_codec_dev_write(codec_out, (void *)samples, num_samples * sizeof(int16_t));
}

static void play_tone(int freq, int duration_ms)
{
    int num_samples = SAMPLE_RATE * duration_ms / 1000;
    int total = num_samples * 2;

    int16_t *buf = malloc(total * sizeof(int16_t));
    if (!buf) return;

    for (int i = 0; i < num_samples; i++) {
        float t = (float)i / SAMPLE_RATE;
        float val = sinf(2.0f * (float)M_PI * freq * t);
        val *= 0.3f;
        int16_t sample = (int16_t)(val * 16384);
        buf[i * 2] = sample;
        buf[i * 2 + 1] = sample;
    }

    write_pcm(buf, total);
    free(buf);
}

void audio_init(void)
{
    ESP_LOGI(TAG, "Initializing I2S...");
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);
    i2s_chan_handle_t tx_handle = NULL;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCLK_IO,
            .bclk = I2S_BCLK_IO,
            .ws = I2S_LRCLK_IO,
            .dout = I2S_DOUT_IO,
            .din = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    ESP_LOGI(TAG, "Initializing ES8311 codec...");
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES8311_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &es8311_i2c_dev));
    es8311_init_manual();

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_PORT,
        .tx_handle = tx_handle,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

    esp_codec_dev_cfg_t dev_cfg2 = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .data_if = data_if,
        .codec_if = NULL,
    };
    codec_out = esp_codec_dev_new(&dev_cfg2);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = SAMPLE_RATE,
        .channel = 2,
        .bits_per_sample = 16,
    };
    esp_codec_dev_open(codec_out, &fs);
    esp_codec_dev_set_out_vol(codec_out, 80);

    ESP_LOGI(TAG, "Audio subsystem initialized");
}

void audio_play_connect(void)
{
    ESP_LOGI(TAG, "Playing connect chime");
    play_tone(523, 100);
    vTaskDelay(pdMS_TO_TICKS(50));
    play_tone(659, 100);
    vTaskDelay(pdMS_TO_TICKS(50));
    play_tone(784, 150);
}

void audio_play_disconnect(void)
{
    ESP_LOGI(TAG, "Playing disconnect tone");
    play_tone(784, 150);
    vTaskDelay(pdMS_TO_TICKS(50));
    play_tone(262, 250);
}

void audio_play_approval(void)
{
    ESP_LOGI(TAG, "Playing approval alert");
    for (int i = 0; i < 3; i++) {
        play_tone(1000, 80);
        vTaskDelay(pdMS_TO_TICKS(120));
    }
}

void audio_play_notify(void)
{
    ESP_LOGI(TAG, "Playing notification");
    play_tone(4000, 50);
}
