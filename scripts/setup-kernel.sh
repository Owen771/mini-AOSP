#!/usr/bin/env bash
# mini-AOSP kernel setup — checks for kernel features needed in later stages
set -euo pipefail

echo "=== mini-AOSP Kernel Check ==="
echo ""

# Stage 0 requirements (all available on any modern kernel)
echo "--- Stage 0 Requirements ---"
echo "  Unix domain sockets: always available ✓"
echo "  fork/exec:           always available ✓"
echo "  SO_PEERCRED:         Linux-only (not on macOS) — degraded on macOS"
echo ""

# Stage 2+ requirements
echo "--- Future Stage Requirements ---"

# Check for binder
if [ -e /dev/binder ] || [ -e /dev/binderfs/binder ]; then
    echo "  Binder device:  found ✓"
elif lsmod 2>/dev/null | grep -q binder; then
    echo "  Binder module:  loaded ✓ (but no /dev/binder — may need binderfs)"
else
    echo "  Binder module:  not loaded"
    echo "    Stage 2 will need the binder kernel module."
    echo "    On Ubuntu: sudo apt install linux-modules-extra-\$(uname -r)"
    echo "    Then: sudo modprobe binder_linux"
    echo "    Or mount binderfs: sudo mkdir -p /dev/binderfs && sudo mount -t binder binder /dev/binderfs"
    echo ""
    echo "    NOTE: macOS does not support binder. Stage 2+ requires Linux."
fi

echo ""

# Check for /proc filesystem
if [ -d /proc/self ]; then
    echo "  /proc filesystem: available ✓"
else
    echo "  /proc filesystem: not available (some features degraded)"
fi

# Check for cgroups
if [ -d /sys/fs/cgroup ]; then
    if [ -f /sys/fs/cgroup/cgroup.controllers ]; then
        echo "  cgroups:          v2 ✓"
    else
        echo "  cgroups:          v1 ✓"
    fi
else
    echo "  cgroups:          not available (resource limiting won't work)"
fi

echo ""
echo "=== Stage 0 is ready to build on any POSIX system (Linux or macOS) ==="
