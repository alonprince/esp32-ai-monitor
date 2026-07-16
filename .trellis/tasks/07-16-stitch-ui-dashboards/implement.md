# Execution Plan - Stitch UI Dashboards

This document outlines the step-by-step checklist to implement the new UI dashboards on the ESP32-C6.

## Implementation Steps

### Phase 1: Telemetry & Time Sync Backend
- [ ] In [main/ble_server.h](file:///Users/wangheng/workspace/pico/esp32-ai-monitor/main/ble_server.h), change `active_tool` size from 128 to 256.
- [ ] In [main/ble_server.c](file:///Users/wangheng/workspace/pico/esp32-ai-monitor/main/ble_server.c), under `TYPE_SYNC_TIME` packet handler, set the ESP32 system clock using `settimeofday()`.
- [ ] In [main/ble_server.c](file:///Users/wangheng/workspace/pico/esp32-ai-monitor/main/ble_server.c), ensure logging shows the correct synchronized time.

### Phase 2: Configuration & Large Font
- [ ] In [sdkconfig.defaults](file:///Users/wangheng/workspace/pico/esp32-ai-monitor/sdkconfig.defaults), append `CONFIG_LV_FONT_MONTSERRAT_48=y`.
- [ ] Run `idf.py reconfigure` to merge the defaults and verify Montserrat-48 font exists.

### Phase 3: LVGL UI Redesign
- [ ] In [main/ui.c](file:///Users/wangheng/workspace/pico/esp32-ai-monitor/main/ui.c), add `time.h` and helper function to format the English date (e.g. `Tuesday 17`) and clock.
- [ ] Create a 1-second LVGL timer `clock_timer_cb` that fetches the local time, updates the header clock labels, and updates the working status character animation frame.
- [ ] Redesign `create_guide_screen()` to match the "蓝牙待连接" mockup (large bluetooth icon, uppercase text, device name).
- [ ] Redesign `create_dashboard_screen()`:
  - Add top-left date, clock, and seconds labels on both screens (guide & dashboard) for visual continuity.
  - Add the dynamic outer progress arc bezel using `lv_arc_create()`, bound to `progress_current`.
  - Add top-right WiFi and Bluetooth status icons.
  - Add the dynamic layout switcher:
    - **Empty State**: Large orange container showing `task_alt` icon, "No Active Tasks" text, and system idle status.
    - **Refined State**: A vertical layout container that lists up to 3 parsed tasks.
  - Implement thread-safe copying in `ui_update_telemetry()` using `ble_server_get_telemetry()` to clone telemetry data to local stack memory under mutex before parsing.
  - Implement the split-string parser using `strtok_r` for safe re-entrant parsing. Use null-guards and fallback strings (`Name`: "Unknown Task", `ID`: "---", `Time`: "00:00", `Status`: "idle") to handle truncated/malformed inputs.
  - Create the task list layout, displaying the name, ID, remaining time, and status icon for each parsed task.
  - Implement a cycling character text animation (`|`, `/`, `-`, `\`) updated via the 1s timer for `working` task status to avoid heavy spinner widget overhead.
  - Update the footer to display the progress value (e.g. `85%`) in Montserrat-48 on the bottom-left and the timestamp pill on the bottom-right.
- [ ] Modify `ui_update_telemetry()` to trigger the layout switcher and parse tasks when the telemetry updates.

### Phase 4: Mac Host Script Update
- [ ] In [mac_host.py](file:///Users/wangheng/workspace/pico/esp32-ai-monitor/mac_host.py), add automatic calculation and transmission of local epoch time (local wall clock time cast as UTC) in `TYPE_SYNC_TIME` packet upon connection.
- [ ] In [mac_host.py](file:///Users/wangheng/workspace/pico/esp32-ai-monitor/mac_host.py), set write characteristic packet transmissions to use `response=True` for long packet fragmentation.
- [ ] In [mac_host.py](file:///Users/wangheng/workspace/pico/esp32-ai-monitor/mac_host.py), update the interactive shell and socket command handler to document and support multi-task inputs.

## Verification & Validation

### 1. Build Verification
Verify the firmware builds successfully with ESP-IDF v6.0.1:
```bash
idf.py build
```

### 2. Live Telemetry Verification
Run the host script and write test commands to verify the UI updates correctly:
```bash
# Connect host and verify clock syncs automatically
python3 mac_host.py --address "/dev/cu.usbmodem101"

# In the interactive shell:
# 1. Transition to idle state (Empty State)
ble-host> state 1

# 2. Transition to working state with 3 active tasks
ble-host> tool Model Training,99x-A,03:32,working|Data Indexing,102-B,01:15,done|Syncing Codex,044-C,00:42,paused
ble-host> state 2

# 3. Transition to working state with 1 active task
ble-host> tool Model Training,99x-A,03:32,working
ble-host> state 2
```
