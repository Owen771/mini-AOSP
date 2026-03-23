#!/usr/bin/env bash
# mini-AOSP run-test — build, start, wait for hello_app success, then shutdown
# Designed to run headless (no Ctrl+C needed). Exit code 0 = success.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RUNTIME_DIR="/tmp/mini-aosp"
TIMEOUT="${1:-30}"  # seconds to wait before giving up

# ── Step 1: Build ───────────────────────────────────────────────────

echo "[run-test] Building..."
make -C "$ROOT_DIR/build" all 2>&1

# ── Step 2: Clean previous run ──────────────────────────────────────

"$SCRIPT_DIR/stop.sh" 2>/dev/null || true
rm -f "$RUNTIME_DIR"/*.pid "$RUNTIME_DIR"/*.sock "$RUNTIME_DIR"/*.ready

# ── Step 3: Start in background ─────────────────────────────────────

echo "[run-test] Starting mini-AOSP..."
mkdir -p "$RUNTIME_DIR"

# start.sh uses exec, so we need to launch it as a subprocess
bash "$SCRIPT_DIR/start.sh" &>"$RUNTIME_DIR/output.log" &
INIT_WRAPPER_PID=$!

# ── Step 4: Wait for success marker or timeout ──────────────────────

echo "[run-test] Waiting for hello_app to complete (timeout: ${TIMEOUT}s)..."
ELAPSED=0
SUCCESS=0
while [ "$ELAPSED" -lt "$TIMEOUT" ]; do
    if grep -q "Full stack verified" "$RUNTIME_DIR/output.log" 2>/dev/null; then
        SUCCESS=1
        break
    fi
    # If init already exited with error, bail early
    if ! kill -0 "$INIT_WRAPPER_PID" 2>/dev/null; then
        break
    fi
    sleep 1
    ELAPSED=$((ELAPSED + 1))
done

# ── Step 5: Print output ────────────────────────────────────────────

echo ""
echo "─── Output ───"
cat "$RUNTIME_DIR/output.log" 2>/dev/null || true
echo "───────────────"
echo ""

# ── Step 6: Shutdown ────────────────────────────────────────────────

if [ -f "$RUNTIME_DIR/init.pid" ]; then
    INIT_PID=$(cat "$RUNTIME_DIR/init.pid")
    if kill -0 "$INIT_PID" 2>/dev/null; then
        kill "$INIT_PID" 2>/dev/null || true
        sleep 2
        kill -9 "$INIT_PID" 2>/dev/null || true
    fi
fi
wait "$INIT_WRAPPER_PID" 2>/dev/null || true
rm -f "$RUNTIME_DIR"/*.pid "$RUNTIME_DIR"/*.sock "$RUNTIME_DIR"/*.ready

# ── Result ──────────────────────────────────────────────────────────

if [ "$SUCCESS" -eq 1 ]; then
    echo "[run-test] PASS"
    exit 0
else
    echo "[run-test] FAIL — did not see success marker within ${TIMEOUT}s"
    exit 1
fi
