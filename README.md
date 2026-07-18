# ESP32 AI Monitor

Turn a [WaveShare ESP32-C6 Touch AMOLED 2.16"](https://www.waveshare.com/esp32-c6-touch-amoled-2.16.htm) into a desktop AI coding companion monitor. Connects via Bluetooth Low Energy (BLE) to a Mac host and displays real-time AI agent telemetry — status, active tasks, progress, and quota usage — on the AMOLED touchscreen. Supports touch-based approve/deny interactions sent back to the host.

## Hardware

- [ESP32-C6 Touch AMOLED 2.16"](https://www.waveshare.com/esp32-c6-touch-amoled-2.16.htm) (CO5300 QSPI display, CST9220 touch, ES8311 audio codec, AXP2101 PMU)

## Features

- Real-time display of AI agent state: **Idle** / **Working** / **Awaiting Approval** / **Asking a Question**
- Active task list (up to 3 concurrent tasks) with name, ID, time remaining, and status
- Progress percentage and quota usage read from Codex session data
- Clock with date/time synchronized from the Mac host
- Touch gestures to approve or deny tool-use requests — sent back to the host via BLE
- Audio chimes for connect/disconnect/notification events (ES8311 audio codec)

## Architecture

```
┌──────────────────┐     BLE      ┌──────────────────────┐
│   ESP32-C6       │◄────────────►│   Mac Host            │
│   (Buddy-AMOLED) │              │   mac_host.py         │
│                  │              │                      │
│  • LVGL UI       │              │  • BLE client         │
│  • NimBLE GATT   │              │  • Unix socket server │
│  • Touch input    │              │  • Codex DB polling   │
│  • Audio chimes   │              │  • OpenCode bridge    │
└──────────────────┘              └──────────────────────┘
```

The Mac host (`mac_host.py`) scans for the "Buddy-AMOLED" BLE device, connects, and bridges:
- **Codex** — polls SQLite session DB and JSONL usage files
- **OpenCode** — `opencode_monitor.py` parses JSON output and forwards telemetry

## Firmware

### Prerequisites

- [ESP-IDF v6.0.1](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/)
- Target: `esp32c6`

### Build & Flash

```bash
# Source ESP-IDF environment
. ~/esp/esp-idf/export.sh

# Build
idf.py build

# Flash (and monitor)
idf.py -p /dev/cu.usbmodem101 flash monitor
```

Or use the shortcut:

```bash
./flash.sh /dev/cu.usbmodem101
```

## Mac Host

### Install Python dependencies

```bash
pip install bleak pypinyin
```

### Run the host

```bash
python3 mac_host.py
```

This starts the BLE client (scans and connects to "Buddy-AMOLED") and a Unix socket server at `/tmp/esp32-ai-monitor.sock`.

### Interactive commands

Once connected, type commands in the host terminal:

| Command | Description |
|---------|-------------|
| `state i` | Set agent to Idle |
| `state w` | Set agent to Working |
| `state a` | Set agent to Awaiting Approval |
| `state q` | Set agent to Asking a Question |
| `tasks name,id,time,status\|...` | Update task list |
| `preview message text` | Show a message preview |
| `progress 75 50` | Set progress: codex% agy% |
| `time` | Sync current time |
| `approve` | Simulate a touch-approve event |
| `disconnect` | Disconnect |

### OpenCode integration

```bash
./opencode.sh "explain this codebase"
```

This wrapper launches `opencode` with JSON output piped through `opencode_monitor.py`, sending telemetry to the ESP32 display in real time.

## BLE Protocol

The Mac host sends TLV-encoded frames to the ESP32 via a Write characteristic:

| Type | Name | Format |
|------|------|--------|
| `0x01` | Status | 1 byte: 0=disconnected, 1=idle, 2=working, 3=wait_approval, 4=wait_question |
| `0x02` | Agent Name | UTF-8 string (max 32 bytes) |
| `0x03` | Workspace | UTF-8 string (max 64 bytes) |
| `0x04` | Active Tool / Tasks | UTF-8 string (max 255 bytes), pipe-delimited |
| `0x05` | Message Preview | UTF-8 string (max 255 bytes) |
| `0x06` | Progress | 2 bytes: [codex%, agy%] |
| `0x07` | Time Sync | 4 bytes: Unix timestamp (big-endian) |
| `0x08` | Sound & Light | 2 bytes: [brightness, volume] |
| `0x09` | Quota Reset | UTF-8 string (max 64 bytes) |

The ESP32 sends interaction events back via a Notify characteristic: `[0x81, 0x01, event_code]` where `1=Approve`, `2=Deny`.

## Pin Assignments

| Function | GPIO |
|----------|------|
| I2C SDA | 8 |
| I2C SCL | 7 |
| QSPI CLK | 0 |
| QSPI D0–D3 | 1–4 |
| LCD CS | 15 |
| Touch INT | 5 |
| Touch RST | 11 |
| I2S MCLK | 19 |
| I2S BCLK | 20 |
| I2S LRCLK | 22 |
| I2S DOUT | 23 |
| I2S DIN | 21 |

## Testing

```bash
python3 -m unittest discover -s test
```

## Project Structure

```
esp32-ai-monitor/
├── main/                  # ESP32 firmware (C)
│   ├── main.c             # App entry point, hardware init
│   ├── ble_server.c/h     # NimBLE GATT server (TLV protocol)
│   ├── ui.c/h             # LVGL UI (dashboard, approval screens)
│   ├── audio.c/h          # I2S + ES8311 audio codec
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── mac_host.py            # Mac BLE host + Unix socket server
├── opencode_monitor.py    # OpenCode JSON output bridge
├── opencode.sh            # OpenCode wrapper script
├── flash.sh               # Quick flash helper
├── test/
│   └── test_telemetry.py  # Task parser unit tests
├── CMakeLists.txt
├── sdkconfig
└── sdkconfig.defaults
```

## License

MIT
