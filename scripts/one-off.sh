#!/usr/bin/env bash
# mini-AOSP one-off — run test on a k8s pod
# Usage:
#   ./scripts/one-off.sh o1 --first    # first time: deploy + bootstrap + test
#   ./scripts/one-off.sh o1            # subsequent: just run test
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REMOTE_DIR="/tmp/mini-AOSP"

POD_NAME="${1:-}"
FIRST=false
NS="${MINI_AOSP_NS:-merchant-financial-service-sandbox}"

if [ -z "$POD_NAME" ]; then
    echo "Usage: ./scripts/one-off.sh <pod-name> [--first]"
    echo "  ./scripts/one-off.sh o1 --first   # deploy + bootstrap + test"
    echo "  ./scripts/one-off.sh o1            # just re-run test"
    exit 1
fi

# Parse flags
shift
for arg in "$@"; do
    case "$arg" in
        --first) FIRST=true ;;
    esac
done

# ── Resolve pod name ────────────────────────────────────────────────

if $FIRST; then
    # deploy.sh handles login + pod resolution + copy
    "$SCRIPT_DIR/deploy.sh" "$POD_NAME"
fi

# Ensure we're logged in even without --first
if ! kubectl get ns "$NS" &>/dev/null; then
    echo "ERROR: Not logged in. Run with --first or login manually."
    exit 1
fi

POD=$(kubectl -n "$NS" get pods --no-headers -o custom-columns=":metadata.name" | grep -- "-${POD_NAME}-" | head -1)
if [ -z "$POD" ]; then
    echo "ERROR: No pod found matching '-${POD_NAME}-'"
    exit 1
fi
echo "[one-off] Pod: $POD"

# ── Bootstrap (first time only) ─────────────────────────────────────

if $FIRST; then
    echo ""
    echo "=== Bootstrapping on pod ==="
    kubectl -n "$NS" exec "$POD" -- bash -c "cd $REMOTE_DIR && ./scripts/bootstrap.sh"
fi

# ── Run test ────────────────────────────────────────────────────────

echo ""
echo "=== Running test on pod ==="
kubectl -n "$NS" exec "$POD" -- bash -c "cd $REMOTE_DIR && ./scripts/run-test.sh"
