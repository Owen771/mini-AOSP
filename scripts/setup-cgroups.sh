#!/usr/bin/env bash
# mini-AOSP cgroup setup — creates resource-limited cgroup for mini-AOSP processes
# Limits: 4 GB memory, 4 CPU cores, 200 max processes
# Requires sudo on Linux. Not supported on macOS (informational only).
set -euo pipefail

CGROUP_NAME="mini-aosp"

echo "=== mini-AOSP Cgroup Setup ==="

if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "macOS does not support cgroups."
    echo "Resource limiting is handled differently on macOS (launchd resource limits)."
    echo "For development purposes, this is not required."
    exit 0
fi

# Check for cgroup v2
if [ -d /sys/fs/cgroup/cgroup.controllers ]; then
    echo "cgroup v2 detected."
    CGROUP_PATH="/sys/fs/cgroup/$CGROUP_NAME"

    sudo mkdir -p "$CGROUP_PATH"

    # Memory limit: 4 GB
    echo "4294967296" | sudo tee "$CGROUP_PATH/memory.max" > /dev/null
    echo "  Memory limit: 4 GB"

    # CPU limit: 4 cores (400000/100000 = 4.0)
    echo "400000 100000" | sudo tee "$CGROUP_PATH/cpu.max" > /dev/null
    echo "  CPU limit: 4 cores"

    # Process limit: 200
    echo "200" | sudo tee "$CGROUP_PATH/pids.max" > /dev/null
    echo "  Process limit: 200"

    echo ""
    echo "Cgroup created at: $CGROUP_PATH"
    echo "To run processes in this cgroup:"
    echo "  echo \$PID | sudo tee $CGROUP_PATH/cgroup.procs"

elif [ -d /sys/fs/cgroup/memory ]; then
    echo "cgroup v1 detected."
    CGROUP_PATH="/sys/fs/cgroup"

    # Memory
    sudo mkdir -p "$CGROUP_PATH/memory/$CGROUP_NAME"
    echo "4294967296" | sudo tee "$CGROUP_PATH/memory/$CGROUP_NAME/memory.limit_in_bytes" > /dev/null
    echo "  Memory limit: 4 GB"

    # CPU
    sudo mkdir -p "$CGROUP_PATH/cpu/$CGROUP_NAME"
    echo "400000" | sudo tee "$CGROUP_PATH/cpu/$CGROUP_NAME/cpu.cfs_quota_us" > /dev/null
    echo "100000" | sudo tee "$CGROUP_PATH/cpu/$CGROUP_NAME/cpu.cfs_period_us" > /dev/null
    echo "  CPU limit: 4 cores"

    # PIDs
    sudo mkdir -p "$CGROUP_PATH/pids/$CGROUP_NAME"
    echo "200" | sudo tee "$CGROUP_PATH/pids/$CGROUP_NAME/pids.max" > /dev/null
    echo "  Process limit: 200"

    echo ""
    echo "Cgroup created. Add processes with:"
    echo "  echo \$PID | sudo tee /sys/fs/cgroup/{memory,cpu,pids}/$CGROUP_NAME/cgroup.procs"
else
    echo "No cgroup filesystem found. Ensure cgroups are enabled in kernel."
    exit 1
fi

echo ""
echo "=== Cgroup setup complete ==="
