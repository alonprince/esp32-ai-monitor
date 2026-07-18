# ESP32 macOS 开发环境搭建与编译烧录指南

本指南详细介绍了如何在 macOS 环境下搭建 ESP32（特别是本项目的 ESP32-C6 芯片）开发环境，以及如何对固件进行编译、烧录和监控。

---

## 一、 开发环境搭建 (macOS)

为了编译本项目所采用的 ESP-IDF 固件，推荐安装并配置 **ESP-IDF v6.0.1** 版本。

### 1. 安装基础依赖
在 macOS 上，首先需要安装命令行工具以及 `Homebrew` 包管理器。
打开终端，执行以下命令安装必备的依赖工具（如 CMake、Ninja、Python、ccache 等）：

```bash
# 安装 Xcode 命令行工具 (已安装则忽略)
xcode-select --install

# 使用 Homebrew 安装基础构建依赖
brew install cmake ninja dfu-util ccache
```

### 2. 克隆 ESP-IDF 代码库
建议在用户目录下创建 `.espressif` 目录或直接将 IDF 克隆到指定目录：

```bash
mkdir -p ~/.espressif/v6.0.1
cd ~/.espressif/v6.0.1

# 克隆 ESP-IDF 官方仓库 (v6.0.1 版本)
git clone -b v6.0.1 --recursive https://github.com/espressif/esp-idf.git
```

### 3. 安装编译器和工具链
进入克隆下来的 `esp-idf` 目录，运行安装脚本以拉取对应 ESP32 芯片（特别是本项目使用的 ESP32-C6）的工具链及 Python 虚拟环境依赖：

```bash
cd esp-idf
# 运行安装脚本 (会自动识别系统架构，支持 Apple Silicon M1/M2/M3 及 Intel 芯片)
./install.sh esp32c6
```

### 4. 配置环境变量快捷方式 (Zsh)
在 macOS 默认的 `zsh` 中，为了避免每次打开终端都需要手动激活环境，建议在 `~/.zshrc` 中添加一个快捷别名（Alias）：

```bash
# 打开 shell 配置文件
nano ~/.zshrc
```
在文件末尾添加以下行：
```bash
# ESP-IDF v6.0.1 环境变量激活快捷别名
alias get_idf=". /Users/wangheng/.espressif/v6.0.1/esp-idf/export.sh"
```
保存并退出（在 nano 中按 `Ctrl+O` 写入，`Ctrl+X` 退出），然后刷新当前 shell：
```bash
source ~/.zshrc
```

---

## 二、 固件编译 (Build)

1. 进入本项目的工作空间目录：
   ```bash
   cd /Users/wangheng/workspace/pico/esp32-ai-monitor
   ```
2. 激活 ESP-IDF 编译环境：
   ```bash
   get_idf
   ```
   *(如果成功激活，终端会输出包含 `IDF_PATH` 且带有 "Go to the project directory and run: idf.py build" 的提示)*

3. 设置目标芯片为 **ESP32-C6**（仅在首次编译或需要切换芯片时执行）：
   ```bash
   idf.py set-target esp32c6
   ```

4. 执行编译：
   ```bash
   idf.py build
   ```
   *编译成功后，生成的二进制文件 `esp32_ai_monitor.bin` 等将存放在 `build/` 目录下。*

---

## 三、 固件烧录 (Flash) 与调试监控 (Monitor)

### 1. 识别板载串口
将 Waveshare ESP32-C6-Touch-AMOLED-2.16 开发板通过 USB 数据线连接至 Mac 电脑。
在终端运行以下命令，查找识别到的串口名称：

```bash
ls -l /dev/cu.usb* /dev/cu.usbmodem*
```
* **CDC 设备（如 ESP32-C6 内置 USB）**：通常显示为 `/dev/cu.usbmodemXXXX`（例如 `/dev/cu.usbmodem101`）。
* **外部串口芯片（如 CH340 等）**：通常显示为 `/dev/cu.usbserial-XXXX`。

### 2. 一键烧录与监控
使用 `idf.py` 烧录固件，并在烧录完成后立即启动串口监视器，查看日志输出：

```bash
# 激活环境后执行 (请将 /dev/cu.usbmodem101 替换为实际查看到的端口名)
idf.py -p /dev/cu.usbmodem101 flash monitor
```

* 💡 **项目内置脚本快捷烧录**：
  本项目根目录下提供了一个快捷烧录脚本 [flash.sh](file:///Users/wangheng/workspace/pico/esp32-ai-monitor/flash.sh)。你只需执行：
  ```bash
  # 默认烧录至 /dev/cu.usbmodem101 端口
  ./flash.sh /dev/cu.usbmodem101
  ```

### 3. 调试控制快捷键
在控制台监控模式下 (`idf.py monitor`)，可使用以下快捷键：
* `Ctrl + ]`：退出串口监视器。
* `Ctrl + T` 接着按 `Ctrl + H`：查看串口监视器的帮助菜单（支持日志过滤、复位重启等指令）。

---

## 四、 常见问题与排除 (macOS)

1. **Python 环境冲突**
   * 如果激活 `export.sh` 时报错提示 Python 版本或 venv 错误，可运行 `rm -rf ~/.espressif/python_env` 清除缓存，然后在 `esp-idf` 目录下重新运行 `./install.sh`。
2. **串口无权限或无法打开**
   * 请确保数据线支持数据传输，而非仅充电线。如果是 M1/M2/M3 Mac 且使用扩展坞，尝试直接连接至 Mac 主板 USB-C 接口。
3. **内存/分区不足**
   * 本项目由于启用了大容量中文字体，必须使用自定义的分区表。如果遇到空间不足错误，请确保 [sdkconfig.defaults](file:///Users/wangheng/workspace/pico/esp32-ai-monitor/sdkconfig.defaults#L26-L28) 中配置了正确的自定义分区表配置：
     ```ini
     CONFIG_PARTITION_TABLE_CUSTOM=y
     CONFIG_PARTITION_TABLE_FILENAME="partitions_singleapp_large.csv"
     ```
