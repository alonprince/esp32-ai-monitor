# ESP32 AI 助手桌面监控屏 (ESP32 AI Monitor)

将 [微雪 (WaveShare) ESP32-C6 Touch AMOLED 2.16"](https://www.waveshare.com/esp32-c6-touch-amoled-2.16.htm) 开发板打造为桌面 AI 编程助手的物理监控屏。通过低功耗蓝牙 (BLE) 连接到 Mac 主机，在 AMOLED 触摸屏上实时显示 AI Agent 的运行状态、当前任务列表、额度进度等遥测数据。支持通过屏幕触摸手势发送“同意/拒绝”交互操作回传至主机。

---

## 硬件支持

- [微雪 ESP32-C6 Touch AMOLED 2.16"](https://www.waveshare.com/esp32-c6-touch-amoled-2.16.htm) 开发板（CO5300 QSPI 屏幕、CST9220 触摸芯片、ES8311 音频解码芯片、AXP2101 电源管理芯片 PMU）

---

## 功能特性

- **Agent 状态实时监测**：可显示 **空闲 (Idle)** / **工作中 (Working)** / **等待审批 (Awaiting Approval)** / **提问中 (Asking a Question)** 四种状态。
- **任务详情列表**：最多支持展示 3 个并发任务，包含任务名称、ID、运行时长和运行状态。
- **Codex 额度显示**：直接从本地 Codex 会话数据中解析出剩余周额度百分比，并在屏幕上展示。
- **实时时钟与同步**：日期和时间由 Mac 主机在建立连接时自动同步。
- **触摸交互控制**：可通过屏幕触摸点击“Approve（同意）”或“Deny（拒绝）”工具调用请求，数据通过 BLE 实时返回给主机。
- **电池与充电指示**：通过 AXP2101 PMU 实时获取开发板电池电量及充电状态（⚡/🔋），并显示在右上角。
- **音频事件提示**：连接、断开连接以及收到审批请求时会播放提示音效（基于 ES8311 解码芯片）。
- **中文原生显示**：支持原生中文字符渲染（采用内置的 `Source Han Sans SC CJK 16` 字体）。

---

## 系统架构

```
┌──────────────────┐     BLE      ┌──────────────────────┐
│   ESP32-C6       │◄────────────►│   Mac Host            │
│   (Buddy-AMOLED) │              │   mac_host.py         │
│                  │              │                      │
│  • LVGL 界面 UI  │              │  • BLE 客户端         │
│  • NimBLE 蓝牙   │              │  • Unix Socket 服务端 │
│  • 屏幕触摸输入  │              │  • Codex 会话 DB 监听 │
│  • 扬声器音效播放│              │  • OpenCode 桥接脚本  │
└──────────────────┘              └──────────────────────┘
```

Mac 主机脚本（`mac_host.py`）会自动扫描并连接名为 "Buddy-AMOLED" 的 BLE 监控屏设备，连接成功后将作为桥梁转发以下数据：
- **Codex**：轮询监测 SQLite 本地会话数据库及 JSONL 会话日志，实时获取额度和运行任务。
- **OpenCode**：通过 `opencode_monitor.py` 解析 JSON 输出并转发 Agent 执行事件。

---

## 固件 (Firmware)

详细环境搭建步骤请参阅仓库中的编译环境说明文档 [docs/mac_setup_guide.md](file:///Users/wangheng/workspace/pico/esp32-ai-monitor/docs/mac_setup_guide.md)。

### 前提条件
- [ESP-IDF v6.0.1](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/)
- 目标芯片 (Target): `esp32c6`

### 编译与烧录
```bash
# 1. 激活 ESP-IDF 环境变量
. /Users/wangheng/.espressif/v6.0.1/esp-idf/export.sh

# 2. 编译固件
idf.py build

# 3. 烧录固件并开启串口监控 (根据实际情况替换串口端口名)
idf.py -p /dev/cu.usbmodem101 flash monitor
```

或者使用项目提供的快捷脚本进行烧录：
```bash
./flash.sh /dev/cu.usbmodem101
```

---

## Mac 主机端配置 (Mac Host)

### 1. 安装 Python 依赖库
```bash
pip install bleak pypinyin
```

### 2. 运行主机脚本
```bash
python3 mac_host.py
```
运行后，脚本会自动搜索并连接 "Buddy-AMOLED" 开发板，同时在 `/tmp/esp32-ai-monitor.sock` 创建一个 Unix socket 服务端，用以接收本地客户端的控制指令。

### 3. 交互式终端指令
连接成功后，你可以在主机终端中输入以下测试指令来手动调试显示内容：

| 指令 | 作用描述 |
| :--- | :--- |
| `state i` | 将 Agent 状态设置为 **空闲 (Idle)** |
| `state w` | 将 Agent 状态设置为 **工作中 (Working)** |
| `state a` | 将 Agent 状态设置为 **等待审批 (Awaiting Approval)** |
| `state q` | 将 Agent 状态设置为 **提问中 (Asking a Question)** |
| `agent <name>` | 修改当前运行的 Agent 名称 |
| `workspace <name>` | 修改显示的工作空间目录名 |
| `tool <cmd>` | 更新当前正在运行的工具名称 |
| `tasks name,id,time,status\|...` | 更新任务列表 |
| `preview <text>` | 显示一行消息预览文本（支持中文） |
| `stats <percentage>` | 设置 Codex 剩余额度百分比 (0-100) |
| `quota_reset <time>` | 设置额度重置时间（例如: "23 Jul 13:27"） |
| `sound <bright> <vol>` | 设置显示屏亮度与音量 (0-100) |
| `sync_time` | 手动同步 Mac 主机时间到开发板 RTC |
| `exit` | 断开连接并退出程序 |

### 4. OpenCode 集成调用
```bash
./opencode.sh "explain this codebase"
```
该封装脚本会自动以 JSON 模式运行 `opencode`，通过 `opencode_monitor.py` 实时将编译/运行等步骤遥测信息推送到 ESP32 显示屏上。

---

## BLE 协议格式

Mac 主机通过指定的 **Write 特征值** 向 ESP32 发送 TLV (Type-Length-Value) 格式的二进制数据包：

| 类型代码 (Type) | 属性名称 (Name) | 数据格式 (Format) |
| :--- | :--- | :--- |
| `0x01` | Status (Agent 状态) | 1 字节：0=未连接, 1=空闲, 2=工作, 3=等待审批, 4=提问中 |
| `0x02` | Agent Name (Agent 名字) | UTF-8 字符串 (最大 32 字节) |
| `0x03` | Workspace (工作区名称) | UTF-8 字符串 (最大 64 字节) |
| `0x04` | Active Tool / Tasks (活跃工具) | UTF-8 字符串 (最大 255 字节)，多任务以 `|` 分隔 |
| `0x05` | Message Preview (消息预览) | UTF-8 字符串 (最大 255 字节) |
| `0x06` | Stats (额度百分比) | 1 字节：[codex%] |
| `0x07` | Time Sync (时钟同步) | 4 字节：UTC Unix 时间戳 (大端序 Big-Endian) |
| `0x08` | Sound & Light (音量与亮度) | 2 字节：[brightness, volume] |
| `0x09` | Quota Reset (重置时间) | UTF-8 字符串 (最大 32 字节) |

ESP32 通过 **Notify 特征值** 向主机实时返回触摸交互事件：格式为 `[0x81, 0x01, event_code]`，其中 `event_code` 值为 `1` 代表同意 (Approve)，值为 `2` 代表拒绝 (Deny)。

---

## 硬件管脚映射 (Pin Assignments)

| 外设/功能 | GPIO 管脚 |
| :--- | :--- |
| I2C SDA | 8 |
| I2C SCL | 7 |
| QSPI CLK | 0 |
| QSPI D0–D3 | 1–4 |
| LCD CS | 15 |
| Touch INT (触摸中断) | 5 |
| Touch RST (触摸复位) | 11 |
| I2S MCLK | 19 |
| I2S BCLK | 20 |
| I2S LRCLK | 22 |
| I2S DOUT (音频输出) | 23 |
| I2S DIN | 21 |

---

## 单元测试

运行 Python 端主机遥测解析和 JSON 命令控制的单元测试：
```bash
python3 -m unittest discover -s test
```

---

## 项目工程结构

```
esp32-ai-monitor/
├── main/                  # ESP32 固件 C 源码
│   ├── main.c             # 应用程序入口、硬件初始化
│   ├── ble_server.c/h     # NimBLE 蓝牙 GATT 服务器 (实现 TLV 协议解析)
│   ├── ui.c/h             # LVGL 界面布局 (面板设计、交互审批页面)
│   ├── audio.c/h          # I2S 驱动配置及 ES8311 音频芯片初始化
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── docs/
│   └── mac_setup_guide.md # ESP-IDF macOS 开发环境搭建指南
├── test/
│   ├── test_telemetry.py  # Python 端遥测解析单元测试
│   └── test_firmware.c    # 固件 C 端 Mock 单元测试
├── mac_host.py            # Mac BLE 主机驱动 + Unix socket 转发服务器
├── opencode_monitor.py    # OpenCode 终端 JSON 遥测桥接脚本
├── opencode.sh            # OpenCode 快捷调用包装脚本
├── flash.sh               # 快捷烧录执行脚本
├── CMakeLists.txt
├── sdkconfig
└── sdkconfig.defaults     # 固件基础 Kconfig 配置默认项
```

---

## 开源协议

MIT
