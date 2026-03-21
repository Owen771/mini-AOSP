#!/usr/bin/env bash
# mini-AOSP deploy — tar, copy to k8s pod, extract
# Usage: ./scripts/deploy.sh [pod-name] [namespace]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REMOTE_DIR="/tmp/mini-AOSP"
TAR_FILE="/tmp/mini-aosp.tar.gz"

# Defaults — override with args or env vars
POD="${1:-${MINI_AOSP_POD:-merchant-financial-service-web-sandbox-o1-779495fc67-nj8jg}}"
NS="${2:-${MINI_AOSP_NS:-merchant-financial-service-sandbox}}"

echo "=== mini-AOSP Deploy ==="
echo "  Source:    $ROOT_DIR"
echo "  Pod:       $POD"
echo "  Namespace: $NS"
echo "  Remote:    $REMOTE_DIR"
echo ""

# Step 1: Create tarball (exclude build artifacts and git)
echo "[deploy] Creating tarball..."
tar czf "$TAR_FILE" -C "$ROOT_DIR" --exclude='out' --exclude='.git' .
SIZE=$(du -h "$TAR_FILE" | cut -f1)
echo "[deploy] Tarball created: $TAR_FILE ($SIZE)"

# Step 2: Copy to pod
echo "[deploy] Copying to pod..."
kubectl -n "$NS" cp "$TAR_FILE" "$POD:$TAR_FILE"
echo "[deploy] Copy complete."

# Step 3: Extract on pod
echo "[deploy] Extracting on pod..."
kubectl -n "$NS" exec "$POD" -- bash -c "mkdir -p $REMOTE_DIR && cd $REMOTE_DIR && tar xzf $TAR_FILE && rm $TAR_FILE"
echo "[deploy] Extracted to $REMOTE_DIR"

# Cleanup local tarball
rm -f "$TAR_FILE"

echo ""
echo "=== Deploy complete ==="
echo ""
echo "Next steps:"
echo "  kubectl -n $NS exec -it $POD -- bash"
echo "  cd $REMOTE_DIR"
echo "  ./scripts/bootstrap.sh"
