# ESP32-C6 AI Coding Agent Monitor - Design Specification

This document details the visual design, screen layout, state transitions, and custom BLE communication protocol for the ESP32-C6 AI Coding Agent Monitor. The design is inspired by the modern dashboard/smartwatch UI style requested by the user: dark mode, glassmorphism, glowing neomorphic rings, and clean typography.

## 1. Visual Design & Theme

### Color Palette (AMOLED Optimized)
To take advantage of the 480×480 AMOLED display and save power, the UI is built on a pure black background with vibrant high-contrast accents.

*   **Background**: `#000000` (AMOLED off, pure black)
*   **Card Backgrounds (Glassmorphic)**: `#121212` with a semi-transparent white border (`rgba(255,255,255,0.05)`)
*   **Text (Primary)**: `#F3F4F6` (Cool gray/white)
*   **Text (Secondary)**: `#9CA3AF` (Muted gray)
*   **Accents**:
    *   **Working/Processing**: `#00F0FF` / `#0088FF` (Neon Cyan/Blue gradient) - Pulsing glow
    *   **Idle/Connected**: `#10B981` (Emerald Green) - Solid glow
    *   **Wait Approval**: `#F59E0B` (Amber Orange) - Flashing glow
    *   **Disconnected/Warning**: `#EF4444` (Coral Red)

### Typography
*   Use a clean sans-serif typeface (e.g., **Inter** or **Outfit**) converted to LVGL binary font format.
*   **Font Sizes**:
    *   Huge (Metrics/Time): 36px / 48px
    *   Large (Status Titles): 24px
    *   Medium (Labels, Workspace): 18px
    *   Small (Tool details, timestamp): 14px

---

## 2. Screen Layout (480×480 AMOLED)

```
        +---------------+  <- (Top-Right Arc: Codex Usage)
       /                 \
      /   [BT] Connected  \
     /                     \
    |   CURRENT RUNNING TASK|
    |   --------------------|
    |   $ agy --run compile |
    |   Compiling ESP-IDF   |
    |   project skeleton... |
    |                       |
    |     Bar Chart (Usage) |
    |     || || || |#| ||   |
    |     S  M  T  W  T  F  S  |
     \                     /
      \                   /
       \-----------------/ <- (Bottom-Left Arc: Antigravity Usage)
```

### Layout Elements

1.  **Status Bar (Top-Center)**:
    *   BLE status icon (glowing blue when connected, gray/red warning when disconnected).
    *   Current local time (synchronized from Mac host).
2.  **Telemetry Border Arcs (Glow Borders)**:
    *   **Top-Right Border Arc**: Renders a rounded glowing white/cyan line along the top-right corner. The length or brightness of this arc represents the **Codex usage** (0-100% of daily/session limit).
    *   **Bottom-Left Border Arc**: Renders a rounded glowing white/green line along the bottom-left corner. The length or brightness of this arc represents the **Antigravity usage** (0-100%).
3.  **Center Console (Primary Focus)**:
    *   Shows the **currently running task** description or command in high-contrast crisp text (Inter/Outfit font).
    *   Displays current execution state: "WORKING", "IDLE", "WAITING APPROVAL", "WAITING QUESTION".
4.  **Daily Activity History Bar Chart (Bottom)**:
    *   Displays a clean 7-column bar chart representing AI usage over the last week. The column matching the current day is highlighted with a gradient/glow.
5.  **Interactive Overlays**:
    *   Approve / Deny touch-sensitive overlay cards that appear in full screen when an action requires approval.

---

## 3. UI States & Animations

### 3.1 Guide / Disconnected State
*   **Background**: `#000000`
*   **Visuals**:
    *   A large glowing red/gray Bluetooth icon in the center.
    *   Text: "Waiting for connection..." and the device advertising name `Buddy-AMOLED`.
    *   Both border arcs are dimmed to 10% opacity.
*   **Sound**: Single low-pitched tone on boot, double beep on BLE disconnect.

### 3.2 Idle State
*   **Background**: `#000000`
*   **Visuals**:
    *   Center text: "AGENT IDLE" (Breathe animation).
    *   Border arcs reflect last known usage stats.
*   **Sound**: Short pleasant high-pitched chirp when BLE connects.

### 3.3 Working / Processing State
*   **Background**: `#000000`
*   **Visuals**:
    *   Center shows the current tool running (e.g. `$ agy --run compile`) and the command text.
    *   The border arcs (Codex / Antigravity) glow brightly and pulse slowly while the corresponding agent is processing.
*   **Sound**: Low-volume click sound when a new tool starts executing.

### 3.4 Wait Approval State (Permission Required)
*   **Background**: `#000000` with subtle red/amber flashing corner borders.
*   **Visuals**:
    *   Bold Title: **CONFIRM COMMAND?**
    *   Displays the full command to be run (e.g., `rm -rf build/`).
    *   Two prominent touch buttons fill the lower half of the screen:
        *   `APPROVE` (Green Card, bottom-right)
        *   `DENY` (Red Card, bottom-left)
*   **Sound**: Intermittent chime pattern (every 3 seconds) until touch input is received or host cancels.

### 3.5 Wait Question State (User Input Needed)
*   **Background**: `#000000`
*   **Visuals**:
    *   Title: **QUESTION INCOMING**
    *   Shows the clarifying question asked by the agent.
    *   "Tap screen to dismiss after reading".


---

## 4. Custom BLE Communication Protocol

To allow richer details (longer text, status flags, and audio commands) on the 480×480 AMOLED display, we will use a custom BLE protocol with a larger MTU (e.g. 256 bytes) instead of restricting ourselves to CodeIsland's 20-byte chunks.

### Service UUIDs
*   **Main Service**: `0000beef-0000-1000-8000-00805f9b34fb` (Retained for legacy matching but updated with custom characteristics)
*   **Write/Control Characteristic (Host -> Device)**: `0000beef-0001-1000-8000-00805f9b34fb` (Supports WRITE without response)
*   **Notify/Event Characteristic (Device -> Host)**: `0000beef-0002-1000-8000-00805f9b34fb` (Supports NOTIFY)

### Data Frames (Host -> Device)
Frames are sent as TLV (Type-Length-Value) packets to easily support variable-length text like workspace name, tool command, and message previews.

#### Frame Format:
```
[ 1 Byte Type ] [ 1 Byte Length ] [ N Bytes Value ]
```

| Type Code | Name | Value Encoding | Description |
|-----------|------|----------------|-------------|
| `0x01` | **Status Update** | 1 byte state code | `0`: Disconnected, `1`: Idle, `2`: Working, `3`: Wait Approval, `4`: Wait Question |
| `0x02` | **Agent Name** | UTF-8 String (max 16 bytes) | E.g. "Codex", "Antigravity", "Claude" |
| `0x03` | **Workspace** | UTF-8 String (max 32 bytes) | Current active directory/project name |
| `0x04` | **Active Tool** | UTF-8 String (max 64 bytes) | Active tool name and arguments |
| `0x05` | **Message Preview**| UTF-8 String (max 128 bytes) | Latest user prompt or model response preview |
| `0x06` | **Stats / Progress**| 2 bytes (`[Current] [Total]`) | E.g. progress percentage, token usage stats |
| `0x07` | **Sync Time** | 4 bytes (Unix Timestamp) | Keeps the device RTC time in sync |
| `0x08` | **Sound & Light** | 2 bytes (`[Brightness 0-100] [Volume 0-100]`) | Control screen brightness and audio output |

### Event Frames (Device -> Host)
Sent via the Notify characteristic.

#### Event format:
```
[ 1 Byte Event Code ] [ 1 Byte Payload Length ] [ N Bytes Payload ]
```

| Event Code | Name | Payload | Description |
|------------|------|---------|-------------|
| `0x81` | **User Interaction** | 1 byte action | `1`: Permission Approved (Touch), `2`: Permission Denied (Touch), `3`: Screen Tapped, `4`: Double Tapped |
| `0x82` | **System Status** | 1 byte status | `1`: Ready, `2`: Battery Low, `3`: Going to Sleep |
