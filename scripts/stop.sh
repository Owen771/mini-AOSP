#!/usr/bin/env bash
# mini-AOSP stop — gracefully shuts down all mini-AOSP processes
set -euo pipefail

RUNTIME_DIR="/tmp/mini-aosp"

echo "=== mini-AOSP Stopping ==="

# Send SIGTERM to init, which will cascade to children
if [ -f "$RUNTIME_DIR/init.pid" ]; then
    pid=$(cat "$RUNTIME_DIR/init.pid")
    if kill -0 "$pid" 2>/dev/null; then
        echo "Sending SIGTERM to init (PID $pid)..."
        kill "$pid"
        # Wait up to 5 seconds for graceful shutdown
        for i in $(seq 1 50); do
            if ! kill -0 "$pid" 2>/dev/null; then
                echo "init stopped."
                break
            fi
            sleep 0.1
        done
        # Force kill if still alive
        if kill -0 "$pid" 2>/dev/null; then
            echo "Force killing init..."
            kill -9 "$pid" 2>/dev/null || true
        fi
    else
        echo "init (PID $pid) not running."
    fi
    rm -f "$RUNTIME_DIR/init.pid"
fi

# Clean up any orphaned child processes
for pidfile in "$RUNTIME_DIR"/*.pid; do
    [ -f "$pidfile" ] || continue
    pid=$(cat "$pidfile")
    if kill -0 "$pid" 2>/dev/null; then
        name=$(basename "$pidfile" .pid)
        echo "Killing orphaned $name (PID $pid)..."
        kill "$pid" 2>/dev/null || true
        sleep 0.5
        kill -9 "$pid" 2>/dev/null || true
    fi
    rm -f "$pidfile"
done

# Clean up socket files
rm -f "$RUNTIME_DIR"/*.sock

echo "=== mini-AOSP Stopped ==="
