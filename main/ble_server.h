#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include <stdint.h>
#include <stdbool.h>

// BLE states
typedef enum {
    BLE_STATE_DISCONNECTED = 0,
    BLE_STATE_IDLE,
    BLE_STATE_WORKING,
    BLE_STATE_WAIT_APPROVAL,
    BLE_STATE_WAIT_QUESTION
} ble_monitor_state_t;

// Struct to store parsed telemetry data from host
typedef struct {
    ble_monitor_state_t state;
    char agent_name[32];
    char workspace[64];
    char active_tool[256];
    char message_preview[256];
    uint8_t progress_current;
    uint8_t progress_total;
    uint32_t sync_time;
    uint8_t brightness;
    uint8_t volume;
    char quota_reset_time[32]; // Quota reset time string e.g. "23 Jul 13:27"
    bool updated; // Flag indicating UI needs redraw
} ble_telemetry_data_t;

// Initialize NimBLE stack and GATT services
void ble_server_init(void);

// Send interaction event back to Mac host (e.g. touch approve/deny)
// Event code: 1 = Approve, 2 = Deny
void ble_server_send_interaction(uint8_t event_code);

// Getter for telemetry data
void ble_server_get_telemetry(ble_telemetry_data_t *out_data);

// Reset update flag
void ble_server_clear_update_flag(void);

// Check if BLE is connected
bool ble_server_is_connected(void);

#endif // BLE_SERVER_H
