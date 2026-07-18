#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

// Mock the structures needed
typedef enum {
    BLE_STATE_DISCONNECTED = 0,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTED,
    BLE_STATE_IDLE,
    BLE_STATE_WORKING,
    BLE_STATE_APPROVAL
} ble_monitor_state_t;

typedef struct {
    ble_monitor_state_t state;
    char agent_name[32];
    char workspace[64];
    char active_tool[128];
    char message_preview[128];
    uint8_t progress_current;
    uint8_t progress_total;
    uint32_t sync_time;
    uint8_t brightness;
    uint8_t volume;
    char quota_reset_time[32];
    bool updated;
} ble_telemetry_data_t;

// Test variables representing static state in ble_server.c
static ble_telemetry_data_t s_telemetry;

// Mock Semaphore APIs
typedef void* SemaphoreHandle_t;
#define pdTRUE 1
#define portMAX_DELAY 0xFFFF
static int mock_semaphore_taken = 0;
static int mock_semaphore_given = 0;
bool xSemaphoreTake(SemaphoreHandle_t sem, uint32_t delay) {
    mock_semaphore_taken++;
    return true;
}
void xSemaphoreGive(SemaphoreHandle_t sem) {
    mock_semaphore_given++;
}

// Mock logging
void mock_log(const char *level, const char *tag, const char *fmt, ...) {
    // printf("[%s] %s: ", level, tag);
    // va_list args;
    // va_start(args, fmt);
    // vprintf(fmt, args);
    // va_end(args);
    // printf("\n");
}
#define ESP_LOGI(tag, fmt, ...) mock_log("I", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) mock_log("W", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) mock_log("E", tag, fmt, ##__VA_ARGS__)

// Copy of ble_server_parse_tlv logic to test
static void test_parse_tlv(const uint8_t *data, uint16_t len) {
    uint16_t offset = 0;
    while (offset + 2 <= len) {
        uint8_t type = data[offset];
        uint8_t tlv_len = data[offset + 1];
        if (offset + 2 + tlv_len > len) {
            break;
        }
        const uint8_t *val = &data[offset + 2];
        
        switch (type) {
            case 0x01: // Status Update
                if (tlv_len >= 1) {
                    s_telemetry.state = (ble_monitor_state_t)val[0];
                    s_telemetry.updated = true;
                }
                break;
            case 0x02:
                {
                    int copy_len = tlv_len < sizeof(s_telemetry.agent_name) - 1 ? tlv_len : sizeof(s_telemetry.agent_name) - 1;
                    memcpy(s_telemetry.agent_name, val, copy_len);
                    s_telemetry.agent_name[copy_len] = '\0';
                    s_telemetry.updated = true;
                }
                break;
            case 0x09: // Quota Reset Time
                {
                    int copy_len = tlv_len < (int)sizeof(s_telemetry.quota_reset_time) - 1
                                 ? tlv_len
                                 : (int)sizeof(s_telemetry.quota_reset_time) - 1;
                    memcpy(s_telemetry.quota_reset_time, val, copy_len);
                    s_telemetry.quota_reset_time[copy_len] = '\0';
                    s_telemetry.updated = true;
                }
                break;
            default:
                break;
        }
        offset += 2 + tlv_len;
    }
}

// UI test mocks
typedef struct {
    char text[64];
} MockLvLabel;
static MockLvLabel mock_battery_lbl;

#define LV_SYMBOL_BATTERY_FULL "F"
#define LV_SYMBOL_BATTERY_3    "3"
#define LV_SYMBOL_BATTERY_2    "2"
#define LV_SYMBOL_BATTERY_1    "1"
#define LV_SYMBOL_BATTERY_EMPTY "E"
#define LV_SYMBOL_CHARGE       "C"

void lv_label_set_text(MockLvLabel *lbl, const char *txt) {
    if (lbl) {
        strncpy(lbl->text, txt, sizeof(lbl->text) - 1);
        lbl->text[sizeof(lbl->text)-1] = '\0';
    }
}

// ui_update_device_battery logic to test
void test_ui_update_device_battery(uint8_t percentage, bool is_charging) {
    const char *batt_sym = LV_SYMBOL_BATTERY_FULL;
    if (is_charging) {
        batt_sym = LV_SYMBOL_CHARGE;
    } else {
        if      (percentage > 75) batt_sym = LV_SYMBOL_BATTERY_FULL;
        else if (percentage > 50) batt_sym = LV_SYMBOL_BATTERY_3;
        else if (percentage > 25) batt_sym = LV_SYMBOL_BATTERY_2;
        else if (percentage > 10) batt_sym = LV_SYMBOL_BATTERY_1;
        else                      batt_sym = LV_SYMBOL_BATTERY_EMPTY;
    }

    char batt_buf[16];
    snprintf(batt_buf, sizeof(batt_buf), "%s %d%%", batt_sym, percentage);
    lv_label_set_text(&mock_battery_lbl, batt_buf);
}

// Tests
void run_tests() {
    // 1. Test Parse TLV for Quota Reset (Type 0x09)
    memset(&s_telemetry, 0, sizeof(s_telemetry));
    uint8_t packet[] = { 0x09, 0x0c, '2', '2', ' ', 'J', 'u', 'l', ' ', '1', '4', ':', '0', '0' }; // len 12
    test_parse_tlv(packet, sizeof(packet));
    assert(strcmp(s_telemetry.quota_reset_time, "22 Jul 14:00") == 0);
    assert(s_telemetry.updated == true);
    printf("C TEST: Parse TLV type 0x09 passed!\n");

    // 2. Test ui_update_device_battery logic
    // Case 1: Charging
    test_ui_update_device_battery(45, true);
    assert(strcmp(mock_battery_lbl.text, "C 45%") == 0);

    // Case 2: Discharging, >75%
    test_ui_update_device_battery(80, false);
    assert(strcmp(mock_battery_lbl.text, "F 80%") == 0);

    // Case 3: Discharging, <=10%
    test_ui_update_device_battery(5, false);
    assert(strcmp(mock_battery_lbl.text, "E 5%") == 0);

    printf("C TEST: ui_update_device_battery logic passed!\n");
}

int main() {
    run_tests();
    printf("ALL C TESTS PASSED\n");
    return 0;
}
