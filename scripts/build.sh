#!/usr/bin/env bash
# mini-AOSP build script — compiles C++ and Kotlin sources
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== mini-AOSP Build ==="
echo "Root: $ROOT_DIR"

make -C "$ROOT_DIR/build" ROOT="$ROOT_DIR" all
