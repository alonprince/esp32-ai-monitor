import asyncio
import sys
import json
import os
import argparse
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
            tool_val = str(kwargs.get("value", ""))[:64]
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
        
        if payload:
            await self.client.write_gatt_char(WRITE_CHAR_UUID, payload, response=False)
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
    print("  tool <command>           - Change active tool command")
    print("  preview <text>           - Change message preview snippet")
    print("  stats <codex> <agy>      - Change Codex (0-100) and Antigravity (0-100) usage arcs")
    print("  sound <bright> <vol>     - Change brightness and volume (0-100)")
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
            
            parts = line.strip().split(maxsplit=2)
            if not parts:
                continue
                
            cmd = parts[0].lower()
            if cmd == "exit":
                break
            
            kwargs = {}
            
            if cmd == "state":
                if len(parts) >= 2:
                    kwargs["value"] = int(parts[1])
                else:
                    print("Usage: state <1-4>")
                    continue
            elif cmd == "agent":
                if len(parts) >= 2:
                    kwargs["value"] = parts[1]
                else:
                    print("Usage: agent <name>")
                    continue
            elif cmd == "workspace":
                if len(parts) >= 2:
                    kwargs["value"] = parts[1]
                else:
                    print("Usage: workspace <name>")
                    continue
            elif cmd == "tool":
                if len(parts) >= 2:
                    kwargs["value"] = parts[1] + (" " + parts[2] if len(parts) > 2 else "")
                else:
                    print("Usage: tool <command>")
                    continue
            elif cmd == "preview":
                if len(parts) >= 2:
                    kwargs["value"] = parts[1] + (" " + parts[2] if len(parts) > 2 else "")
                else:
                    print("Usage: preview <text>")
                    continue
            elif cmd == "stats":
                subparts = parts[1].split() if len(parts) >= 2 else []
                if len(parts) >= 3:
                    kwargs["codex"] = int(parts[1])
                    kwargs["agy"] = int(parts[2])
                elif len(subparts) == 2:
                    kwargs["codex"] = int(subparts[0])
                    kwargs["agy"] = int(subparts[1])
                else:
                    print("Usage: stats <codex (0-100)> <agy (0-100)>")
                    continue
            elif cmd == "sound":
                subparts = parts[1].split() if len(parts) >= 2 else []
                if len(parts) >= 3:
                    kwargs["bright"] = int(parts[1])
                    kwargs["vol"] = int(parts[2])
                elif len(subparts) == 2:
                    kwargs["bright"] = int(subparts[0])
                    kwargs["vol"] = int(subparts[1])
                else:
                    print("Usage: sound <brightness (0-100)> <volume (0-100)>")
                    continue
            else:
                print(f"Unknown command: {cmd}")
                continue

            print(f"[Host -> ESP32] {cmd}")
            await processor.process_command(cmd, **kwargs)
                
        except Exception as e:
            print(f"Error executing command: {e}")


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
        
        if args.socket:
            socket_task = asyncio.create_task(
                unix_socket_listener(processor, args.socket)
            )
        else:
            socket_task = None
        
        await interactive_shell(processor)
        
        if socket_task:
            socket_task.cancel()
            try:
                await socket_task
            except asyncio.CancelledError:
                pass
        
        print("Unsubscribing...")
        await client.stop_notify(NOTIFY_CHAR_UUID)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nExited.")
