#!/bin/bash
# Deploy build artifacts to the local deployment folder.
# Usage: ./deploy.sh [build_dir]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-$SCRIPT_DIR/platform/70D.112/build}"
DEPLOY_DIR="$SCRIPT_DIR/build/70d-latest"

echo "=== Deploying to $DEPLOY_DIR ==="

if [[ ! -f "$BUILD_DIR/autoexec.bin" ]]; then
    echo "✗ $BUILD_DIR/autoexec.bin not found - build first" >&2
    exit 1
fi

mkdir -p "$DEPLOY_DIR"

cp "$BUILD_DIR/autoexec.bin"      "$DEPLOY_DIR/"
cp "$BUILD_DIR/magiclantern.bin"  "$DEPLOY_DIR/"
cp "$BUILD_DIR/magiclantern.zip"  "$DEPLOY_DIR/"
if [[ -f "$BUILD_DIR/70D_112.sym" ]]; then
    cp "$BUILD_DIR/70D_112.sym" "$DEPLOY_DIR/"
else
    echo "  (no 70D_112.sym - skipping)"
fi

unzip -o -q "$DEPLOY_DIR/magiclantern.zip" -d "$DEPLOY_DIR/"

echo "✓ Deployed:"
ls -lh "$DEPLOY_DIR/autoexec.bin" "$DEPLOY_DIR/magiclantern.bin" "$DEPLOY_DIR/magiclantern.zip"
# Compare against the zip rather than the directory: unzip overwrites but never
# deletes, so a module removed or renamed in the build would otherwise linger
# here forever and still be loaded at boot.
N_ZIP=$(unzip -l "$DEPLOY_DIR/magiclantern.zip" | grep -c 'ML/modules/.*\.mo' || true)
N_DIR=$(ls "$DEPLOY_DIR"/ML/modules/*.mo 2>/dev/null | wc -l)
echo "✓ Modules: ${N_ZIP} in package, ${N_DIR} on disk"
if [[ "$N_ZIP" -ne "$N_DIR" ]]; then
    echo "  ⚠ stale modules on disk from an earlier deploy:"
    comm -13 <(unzip -Z1 "$DEPLOY_DIR/magiclantern.zip" | grep 'ML/modules/.*\.mo' | xargs -n1 basename | sort) \
             <(ls "$DEPLOY_DIR"/ML/modules/*.mo | xargs -n1 basename | sort) | sed 's/^/     /'
    echo "  remove them, or delete $DEPLOY_DIR/ML/modules and re-run"
fi

if [[ -f "$DEPLOY_DIR/ML/modules/ptptun.mo" ]]; then
    echo "✓ PTP Tunnel module: $(ls -lh "$DEPLOY_DIR/ML/modules/ptptun.mo" | awk '{print $5}')"
fi

