#!/usr/bin/env bash
# mini-AOSP deploy — login to teleport, find pod by short name, tar, copy, extract
# Usage: ./scripts/deploy.sh <pod-name>
#   e.g. ./scripts/deploy.sh o1
#   Resolves "o1" → "merchant-financial-service-web-sandbox-o1-6b597c97ff-jcchf"
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REMOTE_DIR="/tmp/mini-AOSP"
TAR_FILE="/tmp/mini-aosp.tar.gz"

POD_NAME="${1:-}"
NS="${MINI_AOSP_NS:-merchant-financial-service-sandbox}"
PROXY="${MINI_AOSP_PROXY:-proxy.doordash-int.com:443}"
KUBE_CLUSTER="${MINI_AOSP_KUBE:-sandbox-01.prod-us-west-2}"

if [ -z "$POD_NAME" ]; then
    echo "Usage: ./scripts/deploy.sh <pod-name>"
    echo "  e.g. ./scripts/deploy.sh o1"
    exit 1
fi

# ── Step 0: Teleport login ──────────────────────────────────────────

echo "=== mini-AOSP Deploy ==="
echo ""

if ! kubectl get ns "$NS" &>/dev/null; then
    echo "[deploy] Not logged in. Running teleport login..."
    tsh login --proxy="$PROXY" --auth=okta
    tsh kube login "$KUBE_CLUSTER"
    echo "[deploy] Logged into $KUBE_CLUSTER"
else
    echo "[deploy] Already logged in to Kubernetes."
fi

# ── Step 1: Find pod by short name ──────────────────────────────────

echo "[deploy] Finding pod matching '$POD_NAME' in namespace $NS..."
POD=$(kubectl -n "$NS" get pods --no-headers -o custom-columns=":metadata.name" | grep -- "-${POD_NAME}-" | head -1)
if [ -z "$POD" ]; then
    echo "ERROR: No pod found matching '-${POD_NAME}-' in namespace '$NS'"
    echo "  Available pods:"
    kubectl -n "$NS" get pods --no-headers 2>/dev/null | awk '{print "    " $1}' || true
    exit 1
fi

echo ""
echo "  Source:    $ROOT_DIR"
echo "  Pod:       $POD"
echo "  Namespace: $NS"
echo "  Remote:    $REMOTE_DIR"
echo ""

# ── Step 2: Create tarball ──────────────────────────────────────────

echo "[deploy] Creating tarball..."
tar czf "$TAR_FILE" -C "$ROOT_DIR" --exclude='out' --exclude='.git' .
SIZE=$(du -h "$TAR_FILE" | cut -f1)
echo "[deploy] Tarball created: $TAR_FILE ($SIZE)"

# ── Step 3: Copy to pod ────────────────────────────────────────────

echo "[deploy] Copying to pod..."
kubectl -n "$NS" cp "$TAR_FILE" "$POD:$TAR_FILE"
echo "[deploy] Copy complete."

# ── Step 4: Extract on pod ──────────────────────────────────────────

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
