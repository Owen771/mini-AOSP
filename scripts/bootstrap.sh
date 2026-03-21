#!/usr/bin/env bash
# mini-AOSP bootstrap — install deps, build, and run (for Linux servers)
# Usage: ./scripts/bootstrap.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
KOTLIN_VERSION="2.0.21"
KOTLIN_DIR="/opt/kotlin/kotlinc"

echo "==========================================="
echo "  mini-AOSP Bootstrap"
echo "==========================================="
echo "  Root: $ROOT_DIR"
echo ""

# ─── Step 1: System packages ───────────────────
echo "--- [1/4] Installing system packages ---"

need_apt=false
for cmd in g++ make java; do
    if ! command -v "$cmd" &>/dev/null; then
        need_apt=true
        break
    fi
done

if [ "$need_apt" = true ]; then
    echo "[bootstrap] Running apt-get update..."
    apt-get update -qq

    echo "[bootstrap] Installing g++, make, openjdk-17, unzip..."
    apt-get install -y -qq g++ make openjdk-17-jdk unzip curl > /dev/null 2>&1
    echo "[bootstrap] System packages installed."
else
    echo "[bootstrap] g++, make, java already installed. Skipping."
fi

# ─── Step 2: Kotlin compiler ───────────────────
echo ""
echo "--- [2/4] Installing Kotlin compiler ---"

if command -v kotlinc &>/dev/null; then
    echo "[bootstrap] kotlinc already available: $(kotlinc -version 2>&1 | head -1)"
elif [ -x "$KOTLIN_DIR/bin/kotlinc" ]; then
    echo "[bootstrap] kotlinc found at $KOTLIN_DIR, adding to PATH."
    export PATH="$KOTLIN_DIR/bin:$PATH"
else
    echo "[bootstrap] Downloading Kotlin $KOTLIN_VERSION..."
    curl -fsSL "https://github.com/JetBrains/kotlin/releases/download/v${KOTLIN_VERSION}/kotlin-compiler-${KOTLIN_VERSION}.zip" -o /tmp/kotlin.zip
    echo "[bootstrap] Extracting to /opt/kotlin..."
    mkdir -p /opt/kotlin
    unzip -qo /tmp/kotlin.zip -d /opt/kotlin
    rm -f /tmp/kotlin.zip
    export PATH="$KOTLIN_DIR/bin:$PATH"
    echo "[bootstrap] Kotlin $KOTLIN_VERSION installed."
fi

# Ensure kotlinc is on PATH for make
export PATH="$KOTLIN_DIR/bin:$PATH"

# ─── Step 3: Verify tools ──────────────────────
echo ""
echo "--- [3/4] Verifying tools ---"

echo "  g++:     $(g++ --version | head -1)"
echo "  java:    $(java -version 2>&1 | head -1)"
echo "  kotlinc: $(kotlinc -version 2>&1 | head -1)"
echo "  make:    $(make --version | head -1)"

# ─── Step 4: Build ─────────────────────────────
echo ""
echo "--- [4/4] Building mini-AOSP ---"

make -C "$ROOT_DIR/build" ROOT="$ROOT_DIR" all

echo ""
echo "==========================================="
echo "  Bootstrap complete!"
echo "==========================================="
echo ""
echo "Run the system:"
echo "  cd $ROOT_DIR"
echo "  ./scripts/start.sh"
echo ""
echo "Other commands:"
echo "  ./scripts/status.sh   — check running processes"
echo "  ./scripts/stop.sh     — stop everything"
echo "  make -C build clean   — remove build artifacts"
