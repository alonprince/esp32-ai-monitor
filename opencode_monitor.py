#!/usr/bin/env python3
"""
OpenCode Bridge for ESP32 AI Monitor
Parses opencode JSON output and sends status updates via Unix socket.

Usage:
    opencode run "prompt" --format json 2>&1 | python3 opencode_monitor.py

or in daemon mode (listens on socket):
    python3 opencode_monitor.py --daemon

or send a single command:
    python3 opencode_monitor.py --cmd '{"cmd":"agent","value":"OpenCode"}'
"""

import sys
import json
import os
import socket
import argparse
import time
import re

DEFAULT_SOCKET = "/tmp/esp32-ai-monitor.sock"

def send_to_socket(cmd_obj: dict, socket_path: str = DEFAULT_SOCKET):
    """Send a JSON command to the BLE host's Unix socket."""
    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(2.0)
        sock.connect(socket_path)
        line = json.dumps(cmd_obj) + "\n"
        sock.sendall(line.encode('utf-8'))
        sock.close()
        return True
    except (socket.error, FileNotFoundError, ConnectionRefusedError) as e:
        print(f"[Bridge] Cannot connect to {socket_path}: {e}", file=sys.stderr)
        return False

def parse_opencode_event(line: str) -> list:
    """
    Parse a line of opencode JSON output and extract relevant events.
    Returns a list of command dicts to send.
    
    OpenCode JSON format examples (from --format json):
    {"type":"user","message":{"role":"user","content":"do something"}}
    {"type":"assistant","message":{"role":"assistant","content":[...]}}
    {"type":"tool_use","tool":"grep","input":{...}}
    {"type":"tool_result","tool_use_id":"...","content":"..."}
    {"type":"result","subtype":"success","result":"..."}
    """
    commands = []
    
    try:
        data = json.loads(line.strip())
    except json.JSONDecodeError:
        return commands
    
    event_type = data.get("type", "")
    
    if event_type == "user":
        content = ""
        msg = data.get("message", {})
        if isinstance(msg.get("content"), str):
            content = msg["content"]
        elif isinstance(msg.get("content"), list):
            for block in msg["content"]:
                if isinstance(block, dict) and block.get("type") == "text":
                    content = block.get("text", "")
                    break
        
        if content:
            commands.append({"cmd": "preview", "value": content[:100]})
            commands.append({"cmd": "state", "value": 2})
    
    elif event_type == "tool_use":
        tool_name = data.get("tool", data.get("name", "unknown"))
        tool_input = data.get("input", {})
        desc = tool_name
        if isinstance(tool_input, dict):
            args = " ".join(f"{k}:{str(v)[:20]}" for k, v in list(tool_input.items())[:2])
            if args:
                desc = f"{tool_name} {args}"
        
        commands.append({"cmd": "tool", "value": desc})
        commands.append({"cmd": "state", "value": 2})
    
    elif event_type == "tool_result":
        commands.append({"cmd": "state", "value": 1})
    
    elif event_type == "assistant":
        msg = data.get("message", {})
        content_list = msg.get("content", [])
        text_parts = []
        for block in content_list:
            if isinstance(block, dict) and block.get("type") == "text":
                text_parts.append(block.get("text", ""))
        if text_parts:
            preview = " ".join(text_parts)[:100]
            commands.append({"cmd": "preview", "value": preview})
    
    elif event_type == "result":
        commands.append({"cmd": "state", "value": 1})
        commands.append({"cmd": "stats", "codex": 100, "agy": 0})
    
    elif event_type == "system":
        subtype = data.get("subtype", "")
        if "init" in subtype:
            commands.append({"cmd": "agent", "value": "OpenCode"})
            commands.append({"cmd": "state", "value": 1})
    
    return commands


async def pipe_monitor(socket_path: str = DEFAULT_SOCKET):
    """
    Read opencode JSON lines from stdin and forward events to the socket.
    Uses asyncio for non-blocking I/O.
    """
    import asyncio
    
    print(f"[Bridge] Monitoring stdin for opencode events, forwarding to {socket_path}")
    print(f"[Bridge] Press Ctrl+C to stop")
    
    state = "idle"
    last_stats = {"codex": 0, "agy": 0}
    
    async def reader():
        loop = asyncio.get_event_loop()
        while True:
            line = await loop.run_in_executor(None, sys.stdin.readline)
            if not line:
                break
            
            line = line.strip()
            if not line:
                continue
            
            commands = parse_opencode_event(line)
            for cmd in commands:
                if cmd["cmd"] == "state":
                    new_state = "working" if cmd["value"] == 2 else "idle"
                    if new_state != state:
                        state = new_state
                
                if cmd["cmd"] == "stats":
                    last_stats = {"codex": cmd.get("codex", 0), "agy": cmd.get("agy", 0)}
                
                send_to_socket(cmd, socket_path)
                await asyncio.sleep(0.05)
        
        # Send idle on EOF
        send_to_socket({"cmd": "state", "value": 1}, socket_path)
    
    try:
        await reader()
    except KeyboardInterrupt:
        print("\n[Bridge] Stopped")
    except Exception as e:
        print(f"[Bridge] Error: {e}", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description="OpenCode Bridge for ESP32 AI Monitor")
    parser.add_argument("--socket", default=DEFAULT_SOCKET, help=f"Unix socket path (default: {DEFAULT_SOCKET})")
    parser.add_argument("--cmd", help="Send a single JSON command to the socket and exit")
    parser.add_argument("--pipe", action="store_true", help="Read JSON lines from stdin (for piping opencode output)")
    parser.add_argument("--connect", action="store_true", help="Send initial connect event")
    parser.add_argument("--disconnect", action="store_true", help="Send disconnect event")
    parser.add_argument("--request", help="OpenCode permission request text")
    parser.add_argument("--done", action="store_true", help="Signal task completion")
    args = parser.parse_args()
    
    if args.cmd:
        try:
            cmd_obj = json.loads(args.cmd)
        except json.JSONDecodeError:
            print(f"[Bridge] Invalid JSON: {args.cmd}", file=sys.stderr)
            sys.exit(1)
        if send_to_socket(cmd_obj, args.socket):
            print(f"[Bridge] Sent: {args.cmd}")
        else:
            sys.exit(1)
        return
    
    if args.connect:
        send_to_socket({"cmd": "agent", "value": "OpenCode"}, args.socket)
        send_to_socket({"cmd": "state", "value": 1}, args.socket)
        print("[Bridge] Connected")
        return
    
    if args.disconnect:
        send_to_socket({"cmd": "state", "value": 0}, args.socket)
        print("[Bridge] Disconnected")
        return
    
    if args.request:
        send_to_socket({"cmd": "state", "value": 3}, args.socket)
        send_to_socket({"cmd": "tool", "value": args.request[:64]}, args.socket)
        print(f"[Bridge] Approval request sent: {args.request}")
        return
    
    if args.done:
        send_to_socket({"cmd": "state", "value": 1}, args.socket)
        print("[Bridge] Task complete")
        return
    
    if args.pipe:
        import asyncio
        asyncio.run(pipe_monitor(args.socket))
    else:
        import asyncio
        asyncio.run(pipe_monitor(args.socket))


if __name__ == "__main__":
    main()
