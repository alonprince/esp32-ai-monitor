# implement stitch UI dashboards

## Goal

Implement the UI dashboards designed in the Stitch mockups on the ESP32-C6 AI Monitor. The UI must dynamically switch between three states based on the BLE connection status and the task status:
1. **蓝牙待连接 (Bluetooth Disconnected)**: Show "WAITING FOR CONNECTION..." with a large Bluetooth icon when BLE is not connected.
2. **Empty State AI Dashboard**: Show "No Active Tasks" in a large orange container when BLE is connected but there are no active tasks.
3. **Refined AI Dashboard**: Show a high-density list of active tasks (matching the IMAGE_9 Style) when BLE is connected and tasks are active.
Additionally, show the date and synchronized system clock in the top-left corner of the screen across the dashboards.

## Requirements

### 1. Visual Aesthetics & Theme (Stitch / Nexus-OS Style)
- **Background**: `#000000` (OLED Pure Black).
- **Core Accent Color**: `#FF4500` (Orange / `nexus-orange`) for status boxes and badges.
- **Secondary Accent Color**: `#00F0FF` (Cyan / `nexus-blue`) for progress indicators and highlights.
- **Fonts**:
  - Montserrat/Inter for dates, headers, and labels.
  - Large, thin digits for the clock.
  - Monospace font for task IDs, times, and device names.
- **Edge Progress**: 1px thin border at 12px offset, with a progress arc showing overall system/agent state.

### 2. Dashboard State Flow
- **Disconnected state** (`BLE_STATE_DISCONNECTED`):
  - Automatically loads the `guide_screen` (waiting for connection screen).
  - Displays a large Bluetooth icon, "WAITING FOR CONNECTION..." label, and device name (e.g. `Device: NEXUS-01`).
- **Idle state** (`BLE_STATE_IDLE`):
  - Automatically loads the dashboard screen in `Empty State`.
  - Displays "No Active Tasks" with `task_alt` icon in the orange container.
  - Displays battery percentage (e.g. `85%`) and the current date/version sync pill at the bottom.
- **Working state** (`BLE_STATE_WORKING` / `BLE_STATE_WAIT_QUESTION`):
  - Automatically loads the dashboard screen in `Refined AI Dashboard` state.
  - Displays `CURRENT TASK (X)` and a vertical list of tasks inside the orange container.
  - Displays battery percentage and date/version sync pill at the bottom.

### 3. Date & Clock Header
- **Position**: Top-left corner of the screens.
- **Contents**:
  - Date label: `Tuesday 17` (dynamically updated in English).
  - Clock label: `10:40` (large, thin digits).
  - Seconds label: `45` (smaller digits next to the clock).
- **Time Synchronization**:
  - Automatically synchronize the ESP32 internal RTC when the host sends `sync_time` (Unix timestamp) via BLE.
  - A background timer updates the time/date labels every second.

### 4. Delimiter-Based Task String Format
To support multiple tasks over the single BLE `active_tool` string, we use:
- **Task Separator**: `|` (splits the string into individual tasks, max 3)
- **Field Separator**: `,` (splits a single task into its attributes: `Name,ID,Time,Status`)
- **Example**: `Model Training,99x-A,03:32,working|Data Indexing,102-B,01:15,done|Syncing Codex,044-C,00:42,paused`

## Acceptance Criteria

- [ ] The ESP32 boot/disconnected screen matches the `蓝牙待连接` mockup design.
- [ ] Connecting the host via BLE transitions the display to `Empty State AI Dashboard` (showing "No Active Tasks").
- [ ] Starting a task via `opencode` or `mac_host.py` transitions the display to `Refined AI Dashboard` (showing task list).
- [ ] The top-left corner dynamically displays the current date, hour, minute, and seconds.
- [ ] Sending a time sync packet from the host sets the ESP32 system clock, and the UI reflects this time accurately.
- [ ] No compilation/build errors on ESP-IDF v6.0.1.

## GSTACK REVIEW REPORT

| Review | Trigger | Why | Runs | Status | Findings |
|--------|---------|-----|------|--------|----------|
| CEO Review | `/plan-ceo-review` | Scope & strategy | 1 | DONE | SELECTIVE_EXPANSION mode selected. Outer Edge Arc progress bezel accepted. Timezone alignment optimized via host local representation epoch (0 code/0 TZ env config on ESP32, works worldwide out of the box). |
| Codex Review | `/codex review` | Independent 2nd opinion | 0 | — | — |
| Eng Review | `/plan-eng-review` | Architecture & tests | 1 | DONE | Mutex-based thread safety, settimeofday time sync, statically pre-created widgets for Smooth 60fps, BLE response=True for long packet handling |
| Design Review | `/plan-design-review` | UI/UX gaps | 0 | — | — |
| DX Review | `/plan-devex-review` | Developer experience gaps | 0 | — | — |

**VERDICT:** BOTH ENG & CEO REVIEWS DONE AND PASSED. READY FOR PHASE 2 IMPLEMENTATION.
