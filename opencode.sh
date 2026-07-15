#!/bin/bash
# OpenCode wrapper: monitors opencode progress on the ESP32 display
#
# Usage:
#   ./opencode.sh "your prompt here"
#   ./opencode.sh --no-monitor "your prompt"
#
# Prerequisites:
#   python3 mac_host.py running in another terminal (or auto-started)

SOCKET="/tmp/esp32-ai-monitor.sock"
BRIDGE="opencode_monitor.py"

send_event() {
    python3 "$(dirname "$0")/$BRIDGE" --cmd "$1" 2>/dev/null
}

# Check if BLE host is listening
if [ ! -S "$SOCKET" ]; then
    echo "[opencode.sh] BLE host not running. Start it first:"
    echo "  python3 mac_host.py &"
    echo ""
fi

MONITOR=true
OPTS=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-monitor)
            MONITOR=false
            shift
            ;;
        -*)
            OPTS="$OPTS $1"
            shift
            ;;
        *)
            break
            ;;
    esac
done

if [ "$MONITOR" = true ] && [ -S "$SOCKET" ]; then
    send_event '{"cmd":"agent","value":"OpenCode"}'
    send_event '{"cmd":"state","value":2}'
    send_event '{"cmd":"preview","value":"'"$*"'"}'
    
    # Run opencode with JSON output
    opencode run "$@" $OPTS --format json 2>/dev/null | \
        python3 "$(dirname "$0")/$BRIDGE" --pipe --socket "$SOCKET"
    
    EXIT_CODE=$?
    send_event '{"cmd":"state","value":1}'
    send_event '{"cmd":"stats","codex":100,"agy":0}'
    
    echo "[opencode.sh] Done (exit code: $EXIT_CODE)"
else
    opencode run "$@" $OPTS
fi
