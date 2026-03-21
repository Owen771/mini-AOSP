#!/usr/bin/env bash
# mini-AOSP start — generates init.rc with absolute paths, then launches init
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RUNTIME_DIR="/tmp/mini-aosp"

# Check if already running
if [ -f "$RUNTIME_DIR/init.pid" ]; then
    pid=$(cat "$RUNTIME_DIR/init.pid")
    if kill -0 "$pid" 2>/dev/null; then
        echo "mini-AOSP is already running (init PID $pid)"
        echo "Run ./scripts/stop.sh first."
        exit 1
    fi
fi

# Ensure binaries exist
if [ ! -f "$ROOT_DIR/out/bin/init" ]; then
    echo "Build artifacts not found. Run: make -C build"
    exit 1
fi

# Create runtime directory
mkdir -p "$RUNTIME_DIR"

# Generate init.rc with resolved absolute paths
GENERATED_RC="$RUNTIME_DIR/init.rc"
sed "s|\${MINI_AOSP_ROOT}|$ROOT_DIR|g" "$ROOT_DIR/system/core/rootdir/init.rc" > "$GENERATED_RC"

echo "=== mini-AOSP Starting ==="
echo "Root: $ROOT_DIR"
echo "Runtime: $RUNTIME_DIR"
echo ""

# Launch init (runs in foreground so you see output)
exec "$ROOT_DIR/out/bin/init" "$GENERATED_RC"
