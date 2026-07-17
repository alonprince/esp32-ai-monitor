# ESP32-C6 AI Coding Agent Monitor - Implementation Plan

This plan outlines the sequential steps required to build and integrate the native ESP-IDF firmware for the ESP32-C6 AMOLED board and the companion Mac Python BLE host.

---

## Step 1: ESP-IDF Skeleton Setup [COMPLETED]
*   **Goal**: Initialize a clean ESP-IDF v6.0.1 CMake project targeting `esp32c6` and verify compilation.
*   **Tasks**:
    *   Create project directory structure (`main/`, `CMakeLists.txt`, `sdkconfig.defaults`).
    *   Set up baseline `main.c` with boot messages.
    *   Set compiler flags for optimization and clean warning output.
*   **Verification**: Run `idf.py build` to ensure the project compiles successfully without any missing header errors.

## Step 2: NimBLE BLE Server & Advertising [COMPLETED]
*   **Goal**: Initialize the NimBLE stack on the ESP32-C6, advertise as `Buddy-AMOLED`, and configure custom GATT service/characteristics.
*   **Tasks**:
    *   Initialize NVS flash and NimBLE controller/host stack.
    *   Register GATT service `0000beef-0000-1000-8000-00805f9b34fb`.
    *   Add characteristics: Write (`0000beef-0001-...`) and Notify (`0000beef-0002-...`).
    *   Set up BLE connection state callbacks (start advertising on disconnect, handle MTU negotiation).
*   **Verification**: Verify `Buddy-AMOLED` is visible on macOS Bluetooth scanners and can pair successfully.

## Step 3: Mac Python BLE client (bleak) [COMPLETED]
*   **Goal**: Implement a lightweight Python host program that scans, connects to `Buddy-AMOLED`, and handles GATT operations.
*   **Tasks**:
    *   Create `mac_host.py` and install dependencies (`bleak`, `asyncio`).
    *   Write a scanning loop that filters by the target Service UUID.
    *   Handle connection monitoring, auto-reconnection, and GATT discovery.
    *   Subscribe to Notify characteristic to listen for touch/input events.
*   **Verification**: Run the Python script; verify it discovers, connects, and stays connected to the ESP32-C6.

## Step 4: Custom TLV Frame Protocol & Parser
*   **Goal**: Implement the custom TLV (Type-Length-Value) packet frame encoder in Python and parser in ESP32 C code.
*   **Tasks**:
    *   Python: Create helper methods to pack variables (status, agent, workspace, active tool, message preview, stats) into TLV binary frames.
    *   ESP32: Implement an incoming packet buffer, decode TLV headers, and dispatch data to status variables. Add console printing of state.
    *   Implement keep-alive timer (host sends ping frame every 15s; ESP32 resets inactivity timer).
*   **Verification**: Python script sends status updates; ESP32 console prints:
    `[BLE] Decoded Agent: Codex, State: WORKING, Tool: run_command`

## Step 5: PMU (AXP2101) & AMOLED Display (CO5300 QSPI) Driver
*   **Goal**: Turn on display power rails via AXP2101 PMU, initialize the CO5300 driver via QSPI, and display simple colors/patterns.
*   **Tasks**:
    *   Configure shared I2C bus on the board.
    *   Add AXP2101 initialization commands to enable display LDOs/DCDC rails.
    *   Include Espressif `esp_lcd_co5300` driver component.
    *   Configure QSPI interface (CS, SCLK, D0-D3, RST) and initialize panel.
    *   Implement direct frame flushing to test red/green/blue screen fills.
*   **Verification**: Screen turns on and displays solid solid red, green, blue color fills on boot.

## Step 6: LVGL Port & Dashboard UI Pages
*   **Goal**: Initialize LVGL v9, register the CO5300 display flush callback, and design the modern circular dashboard UI pages.
*   **Tasks**:
    *   Add LVGL component to the project.
    *   Configure double buffering for smooth 480×480 AMOLED rendering.
    *   Design the dark-themed screens:
        *   **Guide/Advertising screen** (Glowing BLE logo, connection steps).
        *   **Dashboard screen** (Workspace card, code console widget, scrolling preview card, neomorphic pulsing state ring).
        *   **Approval Prompt screen** (Approve/Deny overlays).
*   **Verification**: Board boots up showing the beautiful dark-themed "Waiting for connection..." guide screen.

## Step 7: CST9220 I2C Touch Driver integration
*   **Goal**: Initialize the CST9220 touch controller via I2C, poll coordinates, and register them as an LVGL input device.
*   **Tasks**:
    *   Add I2C touch reading routine (reading coordinates from register offsets on `0x1A` or `0x5A` depending on scanner discovery).
    *   Configure the falling-edge interrupt pin for touch detection.
    *   Register touch coordinates as an `lv_indev_t` input pointer in LVGL.
    *   Hook "Approve" and "Deny" touch buttons to trigger BLE event notifications (`0x81`).
*   **Verification**: Pressing the touch buttons triggers the Python client console to print `[Host] User approved permission!` or `[Host] User denied permission!`.

## Step 8: Audio Codec & 8-bit Sound Effects
*   **Goal**: Initialize the ES8311 audio codec and play 8-bit chimes for connection/permission events.
*   **Tasks**:
    *   Configure PMU to enable codec power rails.
    *   Initialize ES8311 codec via I2C.
    *   Set up ESP-IDF I2S driver for DAC audio output.
    *   Create synthesized 8-bit sound effects (frequency/amplitude profiles).
    *   Trigger sounds on BLE connect, disconnect, and permission requests.
*   **Verification**: The device plays a clean high-pitched chime when the BLE connection completes.

## Step 9: Codex & Antigravity (agy) Hook Integrations
*   **Goal**: Connect the Python client to Codex (JSON-RPC) and Antigravity hook outputs, completing the end-to-end physical monitor loop.
*   **Tasks**:
    *   Python: Connect to `/tmp/codeisland-<uid>.sock` or interface directly with `codex app-server` to fetch JSON-RPC frames.
    *   Python: Intercept Antigravity CLI hook outputs.
    *   Wire these event feeds into the BLE packet sender.
*   **Verification**: Run an agent command in terminal (e.g. `agy "list files"`); verify the ESP32 screen immediately displays "ANTIGRAVITY", pulses cyan, and displays `list_dir` in the console console output area.
