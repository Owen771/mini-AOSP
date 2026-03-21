#!/usr/bin/env bash
# mini-AOSP status — show running processes, sockets, resource usage
set -euo pipefail

RUNTIME_DIR="/tmp/mini-aosp"

echo "=== mini-AOSP Status ==="
echo ""

# Check processes
echo "--- Processes ---"
any_running=false
for pidfile in "$RUNTIME_DIR"/*.pid; do
    [ -f "$pidfile" ] || continue
    name=$(basename "$pidfile" .pid)
    pid=$(cat "$pidfile")
    if kill -0 "$pid" 2>/dev/null; then
        # Get memory usage
        if [[ "$OSTYPE" == "darwin"* ]]; then
            mem=$(ps -o rss= -p "$pid" 2>/dev/null | awk '{printf "%.1f MB", $1/1024}')
        else
            mem=$(ps -o rss= -p "$pid" 2>/dev/null | awk '{printf "%.1f MB", $1/1024}')
        fi
        printf "  %-20s PID %-8s %s\n" "$name" "$pid" "$mem"
        any_running=true
    else
        printf "  %-20s PID %-8s (not running)\n" "$name" "$pid"
    fi
done

if [ "$any_running" = false ]; then
    echo "  No mini-AOSP processes running."
fi

echo ""

# Check sockets
echo "--- Unix Sockets ---"
any_sockets=false
for sock in "$RUNTIME_DIR"/*.sock; do
    [ -S "$sock" ] || continue
    echo "  $sock"
    any_sockets=true
done

if [ "$any_sockets" = false ]; then
    echo "  No active sockets."
fi

echo ""

# Runtime directory contents
echo "--- Runtime Directory ($RUNTIME_DIR) ---"
if [ -d "$RUNTIME_DIR" ]; then
    ls -la "$RUNTIME_DIR/" 2>/dev/null || echo "  (empty)"
else
    echo "  (does not exist)"
fi
