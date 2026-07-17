# ESP32-C6 AI Coding Agent Monitor

## Goal

Build an ESP-IDF firmware for the ESP32-C6 (Waveshare ESP32-C6-Touch-AMOLED-2.16 dev board) that acts as a physical desk monitor for AI coding agents (primarily Codex/Antigravity). The device receives real-time status data from the Mac host via BLE and displays it on a 2.16" AMOLED touch screen.

User value: at a glance, see whether your AI agent is working, waiting for approval, finished, or paused — without switching windows on your Mac.

## Background

### Hardware Target

- **Board**: Waveshare ESP32-C6-Touch-AMOLED-2.16
- **MCU**: ESP32-C6 (RISC-V single-core 160MHz, Wi-Fi 6 + BLE 5 + 802.15.4)
- **Memory**: 512KB HP SRAM, 16KB LP SRAM, 320KB ROM, 16MB external Flash
- **Display**: 2.16" AMOLED capacitive touch screen, **480×480** resolution, 16.7M colors, 600 cd/m²
- **Display Driver**: **CO5300** (QSPI interface)
- **Touch Controller**: **CST9220** (I2C interface)
- **IMU**: QMI8658 (6-axis)
- **RTC**: PCF85063
- **Audio**: Low-power codec with dual microphone support
- **Power Management**: AXP2101 (battery charge/discharge)
- **Connector**: USB Type-C, MX1.25 lithium battery header
- **ESP-IDF**: v6.0.1 (installed at `~/.espressif/v6.0.1/esp-idf/`)
- **Waveshare official examples**: https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16 (C-based, ESP-IDF)

### Key Differences from CodeIsland Buddy (ESP32-C6-LCD-1.47)

| Aspect | CodeIsland Buddy (1.47") | Our Board (2.16") |
|--------|-------------------------|-------------------|
| Display | 172×320 IPS LCD (ST7789, SPI) | 480×480 AMOLED (CO5300, QSPI) |
| Touch | None | CST9220 capacitive touch (I2C) |
| Framework | Arduino | ESP-IDF native |
| Resolution | Low (pixel art mascots) | High (room for rich UI / LVGL) |
| Extra sensors | None | IMU, RTC, audio codec, battery mgmt |
| Rendering | Adafruit GFX | LVGL or direct framebuffer |

### CodeIsland Reference

[CodeIsland](https://github.com/wxtsky/CodeIsland) (v1.0.30) provides the BLE protocol and Mac-side infrastructure:
- **BLE Protocol** (`ESP32Protocol.swift`):
  - Service UUID: `0000beef-0000-1000-8000-00805f9b34fb`
  - Write characteristic (host→device): `0000beef-0001-...`
  - Notify characteristic (device→host): `0000beef-0002-...`
  - Compact ≤20-byte frames: agent status, workspace, message preview, brightness, orientation, model name, stats, subagent info, tool history, pairing
- **Hook bridge**: `codeisland-bridge` (Swift binary) receives AI tool hook events via stdin JSON, forwards to Unix socket
- **Codex integration**: `CodexAppServerClient.swift` speaks JSON-RPC 2.0 to `codex app-server`
- **Mac-side** CodeIsland app handles BLE scanning, connecting, and driving the device

## Requirements

### R1: ESP-IDF Project Scaffold
- Based on `hello_world` example structure (CMakeLists.txt + main/)
- Target: `esp32c6`
- Reference Waveshare official examples for CO5300 QSPI + CST9220 I2C init
- Components: NimBLE (BLE), QSPI (AMOLED), I2C (touch), LVGL (UI)

### R2: BLE GATT Server (NimBLE)
- Advertise as "Buddy" (CodeIsland compatible) or configurable device name
- Service UUID: `0000beef-0000-1000-8000-00805f9b34fb`
- Write characteristic for receiving host frames
- Notify characteristic for sending button/touch events back to host
- Parse CodeIsland's frame protocol (agent frame, workspace frame, message preview, brightness, orientation, stats, model, etc.)
- Support application-layer pairing protocol (0xE0/0xE1 markers)

### R3: AMOLED Display Driver
- CO5300 QSPI driver for the 2.16" 480×480 AMOLED
- CST9220 I2C touch controller driver
- Double-buffered rendering for smooth updates via LVGL
- Brightness control via BLE command

### R4: Status Display UI (LVGL)
- Show which AI tool is active (Codex, Claude, Gemini, Antigravity, etc.)
- Show current status (idle, processing, waitApproval, waitQuestion) with animated indicators
- Show workspace/project name
- Show current tool being called
- Show message preview (latest AI/user message snippet)
- Show model name being used
- Touch-driven approve/deny for permission requests
- Orientation flip support (up/down)

### R5: Touch & Button Interaction
- Touch-based approve/deny permission requests (replaces BOOT button for primary interaction)
- Touch to cycle through views or scroll message history
- Send touch/button events back to host via BLE notify

### R6: Connectivity Lifecycle
- BLE disconnection detection + auto-reconnect advertising
- Inactivity timeout (60s without writes → return to idle/guide screen)
- Guide screen when not connected (device name, instructions)

### R7: Mac-side BLE Host Program (Python + bleak)
- Lightweight Python CLI/daemon using `bleak` library as BLE Central
- Scans for and connects to the ESP32 device via BLE
- MVP supports **Codex** and **Antigravity (agy)** only; plugin architecture for future tool expansion
- Hooks into AI coding tools to capture events (reference CodeIsland's approach):
  - Hook events: session start/stop, tool calls, permission requests, questions, AI responses
  - Codex: reference `CodexAppServerClient.swift` (JSON-RPC 2.0 to `codex app-server`)
  - Antigravity: reference `codeisland-bridge` hook mechanism for agy CLI
- Custom BLE protocol (larger MTU for richer data on 480×480 AMOLED)
- Sends structured data to ESP32: agent name, status, workspace, tool name, message preview, model name, stats, etc.

### R8: 8-bit Sound Effects
- Use the onboard audio codec for 8-bit retro sound notifications
- Sound events: BLE connected, session start, permission request alert, task complete, error
- Volume control (mute/low/medium/high) via touch UI or BLE command

## Acceptance Criteria

- [ ] ESP-IDF project builds and flashes to ESP32-C6-Touch-AMOLED-2.16
- [ ] AMOLED displays UI after boot
- [ ] Mac-side Python program scans, discovers, and connects to ESP32 via BLE
- [ ] Codex hook events are captured and forwarded to ESP32
- [ ] Antigravity hook events are captured and forwarded to ESP32
- [ ] Agent status correctly parsed and displayed on AMOLED
- [ ] Touch approve/deny sends correct BLE notify events back to Mac
- [ ] 8-bit sound plays on key events (connect, permission request, task complete)
- [ ] Inactivity timeout returns to idle screen

1. **UI Layout**: How to map the reference image components (circular ring, daily bar chart, primary text) to AI monitor telemetry data.

## Decided

| Decision | Choice |
|----------|--------|
| Framework | ESP-IDF v6.0.1 native |
| Board | Waveshare ESP32-C6-Touch-AMOLED-2.16 (480×480 AMOLED) |
| Serial Port | `/dev/cu.usbmodem101` |
| Mac-side protocol | Custom (not CodeIsland compatible) |
| Mac-side tech | Python + bleak |
| MVP AI tools | Codex + Antigravity only |
| Extra sensors | 8-bit sound YES; IMU, RTC deferred |
| UI style | Modern Smartwatch / Neomorphic Dashboard (inspired by reference image) |

## Out of Scope (MVP)

- Wi-Fi connectivity / OTA updates
- IMU-based gestures (tilt/shake)
- RTC clock display
- Battery power management optimization
- Custom PCB design
- Apple Watch / iPhone companion
- AI tools beyond Codex + Antigravity
