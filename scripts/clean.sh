#!/usr/bin/env bash
# mini-AOSP clean — removes build artifacts
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== mini-AOSP Clean ==="
rm -rf "$ROOT_DIR/out"
echo "Removed out/"
