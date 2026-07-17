import asyncio
import sys
import json
import os
import argparse
import datetime
import glob
import re
import time
from bleak import BleakScanner, BleakClient

# GATT Service & Characteristic UUIDs
SERVICE_UUID = "0000cafe-0000-1000-8000-00805f9b34fb"
WRITE_CHAR_UUID = "0001cafe-0000-1000-8000-00805f9b34fb"
NOTIFY_CHAR_UUID = "0002cafe-0000-1000-8000-00805f9b34fb"

# Default socket path
DEFAULT_SOCKET = "/tmp/esp32-ai-monitor.sock"

BLUETOOTH = "\U0001F5F2"

# TLV Type Codes
TYPE_STATUS = 0x01
TYPE_AGENT = 0x02
TYPE_WORKSPACE = 0x03
TYPE_TOOL = 0x04
TYPE_PREVIEW = 0x05
TYPE_STATS = 0x06
TYPE_SYNC_TIME = 0x07
TYPE_SOUND_LIGHT = 0x08

# States
STATE_DISCONNECTED = 0
STATE_IDLE = 1
STATE_WORKING = 2
STATE_WAIT_APPROVAL = 3
STATE_WAIT_QUESTION = 4

def encode_tlv(type_code: int, value: bytes) -> bytes:
    """Pack Type, Length, and Value into a TLV binary frame."""
    length = len(value)
    if length > 255:
        print(f"[Warning] Value for type 0x{type_code:02X} is too long ({length} bytes), truncating to 255.")
        value = value[:255]
        length = 255
    return bytes([type_code, length]) + value

def notification_handler(sender, data):
    """Callback for BLE notifications received from ESP32."""
    if len(data) >= 3 and data[0] == 0x81:
        event_len = data[1]
        event_code = data[2]
        if event_code == 1:
            print(f"\n{BLUETOOTH} [ESP32 -> Mac] TOUCH APPROVED!")
        elif event_code == 2:
            print(f"\n{BLUETOOTH} [ESP32 -> Mac] TOUCH DENIED!")
        else:
            print(f"\n{BLUETOOTH} [ESP32 -> Mac] Unknown interaction code: {event_code}")
    else:
        print(f"\n{BLUETOOTH} [ESP32 -> Mac] Received raw notification: {data.hex()}")

class BLECommandProcessor:
    """Processes commands and sends TLV frames over BLE write characteristic."""
    
    def __init__(self, client: BleakClient):
        self.client = client
    
    async def process_command(self, cmd: str, **kwargs):
        """Process a named command with keyword arguments. Returns True if a frame was sent."""
        payload = None
        
        if cmd == "state":
            state_val = kwargs.get("value", 1)
            payload = encode_tlv(TYPE_STATUS, bytes([state_val]))
            print(f"  -> STATE: {state_val}")
        elif cmd == "agent":
            agent_val = str(kwargs.get("value", "unknown"))[:16]
            payload = encode_tlv(TYPE_AGENT, agent_val.encode('utf-8'))
            print(f"  -> AGENT: {agent_val}")
        elif cmd == "workspace":
            ws_val = str(kwargs.get("value", ""))[:32]
            payload = encode_tlv(TYPE_WORKSPACE, ws_val.encode('utf-8'))
            print(f"  -> WORKSPACE: {ws_val}")
        elif cmd == "tool":
            # Extended active_tool buffer can take up to 255 bytes (TLV max length)
            tool_val = str(kwargs.get("value", ""))[:255]
            payload = encode_tlv(TYPE_TOOL, tool_val.encode('utf-8'))
            print(f"  -> TOOL: {tool_val}")
        elif cmd == "preview":
            preview_val = str(kwargs.get("value", ""))[:128]
            payload = encode_tlv(TYPE_PREVIEW, preview_val.encode('utf-8'))
            print(f"  -> PREVIEW: {preview_val[:60]}...")
        elif cmd == "stats":
            codex = min(max(kwargs.get("codex", 0), 0), 100)
            agy = min(max(kwargs.get("agy", 0), 0), 100)
            payload = encode_tlv(TYPE_STATS, bytes([codex, agy]))
            print(f"  -> STATS: Codex={codex}% Agy={agy}%")
        elif cmd == "sound":
            bright = min(max(kwargs.get("bright", 50), 0), 100)
            vol = min(max(kwargs.get("vol", 50), 0), 100)
            payload = encode_tlv(TYPE_SOUND_LIGHT, bytes([bright, vol]))
            print(f"  -> SOUND: bright={bright}% vol={vol}%")
        elif cmd == "sync_time":
            # Send current host local time cast as UTC Unix timestamp
            now = datetime.datetime.now()
            local_epoch = int(now.replace(tzinfo=datetime.timezone.utc).timestamp())
            payload = encode_tlv(TYPE_SYNC_TIME, local_epoch.to_bytes(4, byteorder='big'))
            print(f"  -> SYNC_TIME: local_epoch={local_epoch} ({now.strftime('%Y-%m-%d %H:%M:%S')})")
        
        if payload:
            # Set response=True to support write request for packets exceeding default MTU size
            await self.client.write_gatt_char(WRITE_CHAR_UUID, payload, response=True)
            return True
        return False


async def handle_json_command(processor: BLECommandProcessor, json_str: str):
    """Parse a JSON command string and process it."""
    try:
        msg = json.loads(json_str)
        cmd = msg.get("cmd", "")
        if not cmd:
            print(f"[Socket] Invalid command, missing 'cmd' field: {json_str}")
            return
        await processor.process_command(cmd, **{k: v for k, v in msg.items() if k != "cmd"})
    except json.JSONDecodeError as e:
        print(f"[Socket] JSON parse error: {e}")
    except Exception as e:
        print(f"[Socket] Error processing command: {e}")


async def unix_socket_listener(processor: BLECommandProcessor, socket_path: str):
    """Async Unix domain socket server that accepts JSON commands."""
    if os.path.exists(socket_path):
        os.unlink(socket_path)
    
    async def handle_client(reader, writer):
        print(f"[Socket] Client connected")
        try:
            while True:
                line = await reader.readline()
                if not line:
                    break
                line_str = line.decode('utf-8').strip()
                if line_str:
                    await handle_json_command(processor, line_str)
        except Exception as e:
            print(f"[Socket] Client error: {e}")
        finally:
            writer.close()
            await writer.wait_closed()
            print(f"[Socket] Client disconnected")
    
    server = await asyncio.start_unix_server(handle_client, path=socket_path)
    print(f"[Socket] Listening on {socket_path}")
    async with server:
        await server.serve_forever()


async def interactive_shell(processor: BLECommandProcessor):
    """Interactive loop to manually test BLE telemetry frames."""
    print("\n--- ESP32-C6 AI Monitor BLE Interactive Shell ---")
    print("Commands:")
    print("  state <1-4>              - Change agent state (1: Idle, 2: Working, 3: Confirm, 4: Question)")
    print("  agent <name>             - Change agent name")
    print("  workspace <name>         - Change workspace folder name")
    print("  tool <command>           - Change active tool command (supports multiple separated by '|')")
    print("  preview <text>           - Change message preview snippet")
    print("  stats <codex> <agy>      - Change Codex (0-100) and Antigravity (0-100) usage arcs")
    print("  sound <bright> <vol>     - Change brightness and volume (0-100)")
    print("  sync_time                - Synchronize current host local time to ESP32 RTC")
    print("  exit                     - Disconnect and exit")
    print("-------------------------------------------------")
    if DEFAULT_SOCKET in sys.argv:
        print(f"[Note] Also accepting commands on {DEFAULT_SOCKET}")
    
    loop = asyncio.get_event_loop()
    
    while True:
        try:
            sys.stdout.write("ble-host> ")
            sys.stdout.flush()
            line = await loop.run_in_executor(None, sys.stdin.readline)
            if not line:
                break
            
            parts = line.strip().split(maxsplit=1)
            if not parts:
                continue
                
            cmd = parts[0].lower()
            if cmd == "exit":
                break
            
            kwargs = {}
            arg = parts[1] if len(parts) > 1 else ""
            
            if cmd == "state":
                if arg:
                    kwargs["value"] = int(arg)
                else:
                    print("Usage: state <1-4>")
                    continue
            elif cmd == "agent":
                if arg:
                    kwargs["value"] = arg
                else:
                    print("Usage: agent <name>")
                    continue
            elif cmd == "workspace":
                if arg:
                    kwargs["value"] = arg
                else:
                    print("Usage: workspace <name>")
                    continue
            elif cmd == "tool":
                if arg:
                    kwargs["value"] = arg
                else:
                    print("Usage: tool <command>")
                    continue
            elif cmd == "preview":
                if arg:
                    kwargs["value"] = arg
                else:
                    print("Usage: preview <text>")
                    continue
            elif cmd == "stats":
                subparts = arg.split()
                if len(subparts) == 2:
                    kwargs["codex"] = int(subparts[0])
                    kwargs["agy"] = int(subparts[1])
                else:
                    print("Usage: stats <codex (0-100)> <agy (0-100)>")
                    continue
            elif cmd == "sound":
                subparts = arg.split()
                if len(subparts) == 2:
                    kwargs["bright"] = int(subparts[0])
                    kwargs["vol"] = int(subparts[1])
                else:
                    print("Usage: sound <brightness (0-100)> <volume (0-100)>")
                    continue
            elif cmd == "sync_time":
                pass
            else:
                print(f"Unknown command: {cmd}")
                continue

            print(f"[Host -> ESP32] {cmd}")
            await processor.process_command(cmd, **kwargs)
                
        except Exception as e:
            print(f"Error executing command: {e}")


async def get_latest_transcript_data():
    brain_dir = os.path.expanduser("~/.gemini/antigravity-cli/brain")
    if not os.path.exists(brain_dir):
        return None, 0
    
    files = []
    for root, dirs, filenames in os.walk(brain_dir):
        if "transcript.jsonl" in filenames:
            files.append(os.path.join(root, "transcript.jsonl"))
            
    if not files:
        return None, 0
        
    # Get the latest modified file
    files.sort(key=lambda x: os.path.getmtime(x), reverse=True)
    latest_file = files[0]
    mtime = os.path.getmtime(latest_file)
    
    try:
        with open(latest_file, "r", encoding="utf-8") as f:
            lines = f.readlines()
            for line in reversed(lines):
                line = line.strip()
                if line:
                    return json.loads(line), mtime
    except Exception:
        pass
        
    return None, 0

async def auto_watcher_loop(processor: BLECommandProcessor):
    print("[AutoWatcher] Started background watcher for Codex tasks...")
    
    last_step_index = -1
    last_state = None
    
    while True:
        try:
            data, mtime = await get_latest_transcript_data()
            now = time.time()
            
            # If the file was updated in the last 15 seconds, we assume Codex is active
            is_active = (now - mtime) < 15.0
            
            if is_active and data:
                step_idx = data.get("step_index", 0)
                
                # Check if it's a new step or state change
                if step_idx != last_step_index or last_state != STATE_WORKING:
                    last_step_index = step_idx
                    last_state = STATE_WORKING
                    
                    # Determine active tool or preview
                    tool_desc = "Working"
                    if "tool_calls" in data and data["tool_calls"]:
                        tool_desc = data["tool_calls"][0].get("name", "working")
                    elif data.get("type") == "RUN_COMMAND":
                        tool_desc = "run_command"
                        
                    # Send state working (2)
                    await processor.process_command("state", value=STATE_WORKING)
                    # Send tool description
                    await processor.process_command("tool", value=tool_desc)
                    # Send step preview
                    preview_txt = f"Step {step_idx}: {tool_desc}"
                    await processor.process_command("preview", value=preview_txt)
            else:
                # Idle state: BLE_STATE_IDLE (1)
                if last_state != STATE_IDLE:
                    last_state = STATE_IDLE
                    await processor.process_command("state", value=STATE_IDLE)
                    await processor.process_command("tool", value="None")
                    await processor.process_command("preview", value="Idle")
                    
        except Exception as e:
            print(f"[AutoWatcher] Error: {e}")
            
        await asyncio.sleep(1.0)

async def auto_stats_loop(processor: BLECommandProcessor):
    print("[AutoStats] Started background loop to monitor Codex usage limit...")
    while True:
        try:
            # Run "opencode stats" in subprocess
            proc = await asyncio.create_subprocess_exec(
                "opencode", "stats",
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            stdout, _ = await proc.communicate()
            output = stdout.decode('utf-8', errors='ignore')
            
            # Extract Total Cost
            cost_match = re.search(r"Total Cost\s+\$([0-9.]+)", output)
            if cost_match:
                total_cost = float(cost_match.group(1))
                # Map cost to percentage of a $10.00 daily/monthly limit
                budget = 10.00
                percentage = min(int((total_cost / budget) * 100), 100)
                
                # Send stats (Codex = percentage, Antigravity = 0)
                await processor.process_command("stats", codex=percentage, agy=0)
                print(f"[AutoStats] Token Quota Used: {percentage}% (Cost: ${total_cost:.2f} / Budget: ${budget:.2f})")
        except Exception as e:
            print(f"[AutoStats] Error: {e}")
            
        # Refresh every 30 seconds
        await asyncio.sleep(30.0)

async def main():
    parser = argparse.ArgumentParser(description="BLE host for ESP32 AI Monitor")
    parser.add_argument("--address", help="Device MAC or UUID address (optional)")
    parser.add_argument("--socket", default=DEFAULT_SOCKET, help=f"Unix socket path for JSON commands (default: {DEFAULT_SOCKET})")
    args = parser.parse_args()

    device = None
    
    if args.address:
        print(f"Connecting to address: {args.address}...")
        device = await BleakScanner.find_device_by_address(args.address, timeout=10.0)
        if not device:
            print(f"Failed to find device with address {args.address}")
            return
    else:
        print("Scanning for 'Buddy-AMOLED' or service UUID...")
        devices = await BleakScanner.discover(timeout=5.0)
        for d in devices:
            if d.name and "Buddy" in d.name:
                device = d
                break
        
        if not device:
            print("Failed to find 'Buddy-AMOLED' device in scan.")
            print("Found devices:")
            for d in devices:
                print(f"  {d.address} - {d.name}")
            return

    print(f"Found device: {device.name} [{device.address}]. Connecting...")
    
    async with BleakClient(device) as client:
        print(f"Connected to {device.name}!")
        
        print("\n=== Discovered BLE Services & Characteristics ===")
        for service in client.services:
            print(f"Service: {service.uuid}")
            for char in service.characteristics:
                print(f"  Char: {char.uuid} (Properties: {char.properties})")
        print("=================================================\n")
        
        print("Subscribing to notifications...")
        await client.start_notify(NOTIFY_CHAR_UUID, notification_handler)
        
        processor = BLECommandProcessor(client)
        
        # Automatically sync time upon connection
        print("Synchronizing device time...")
        try:
            await processor.process_command("sync_time")
        except Exception as e:
            print(f"Failed to auto-sync time: {e}")
        
        # Start background automatic watchers for tasks and token quota
        watcher_task = asyncio.create_task(auto_watcher_loop(processor))
        stats_task = asyncio.create_task(auto_stats_loop(processor))

        if args.socket:
            socket_task = asyncio.create_task(
                unix_socket_listener(processor, args.socket)
            )
        else:
            socket_task = None
        
        await interactive_shell(processor)
        
        if socket_task:
            socket_task.cancel()
        watcher_task.cancel()
        stats_task.cancel()
        
        try:
            if socket_task:
                await socket_task
            await watcher_task
            await stats_task
        except asyncio.CancelledError:
            pass
        
        print("Unsubscribing...")
        await client.stop_notify(NOTIFY_CHAR_UUID)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nExited.")
