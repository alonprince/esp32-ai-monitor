#ifndef UI_H
#define UI_H

#include "ble_server.h"

void ui_init(void);
void ui_update_telemetry(const ble_telemetry_data_t *data);
void ui_update_device_battery(uint8_t percentage, bool is_charging);

#endif
