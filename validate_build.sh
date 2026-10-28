#!/usr/bin/env bash
# Validate the 70D build without requiring QEMU execution.
# Exits non-zero if anything is wrong, so it is usable in a CI chain.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/platform/70D.112/build"
MODULES_DIR="$BUILD_DIR/modules"

# The real Magic Lantern budget on this body is what src/boot-d45-am.c reserves:
#   0x4E0000 - RESTARTSTART(0x44C100) = 0x93F00 = 605952 bytes
# Older notes quoting "656KB" are wrong; 656KB is the AllocateMemory pool extent
# (0x4E0000..0xD3C000), not the payload budget.
MAX_SIZE=605952

filesize() {
    stat --format="%s" "$1" 2>/dev/null || stat -f "%z" "$1" 2>/dev/null || echo 0
}

FAILED=0
fail() { echo "✗ $*"; FAILED=1; }

echo "=== Build Validation Report ==="
echo "Date: $(date)"
echo "Build Dir: $BUILD_DIR"
echo ""

# --- autoexec.bin ---------------------------------------------------------
if [[ -f "$BUILD_DIR/autoexec.bin" ]]; then
    SIZE=$(filesize "$BUILD_DIR/autoexec.bin")
    echo "✓ autoexec.bin: $((SIZE / 1024))KB (${SIZE} bytes)"
    if [[ $SIZE -lt $MAX_SIZE ]]; then
        echo "  Size OK: $((MAX_SIZE - SIZE)) bytes of headroom (budget ${MAX_SIZE})"
    else
        fail "autoexec.bin is ${SIZE} bytes, over the ${MAX_SIZE} byte budget"
    fi
else
    fail "autoexec.bin: NOT FOUND"
fi
echo ""

# --- modules --------------------------------------------------------------
echo "=== Modules ==="
for mod in crop_rec dual_iso sd_clock mlv_lite ptptun wifisrv wifitst ramdump; do
    if [[ -f "$MODULES_DIR/${mod}.mo" ]]; then
        echo "✓ ${mod}.mo: $(filesize "$MODULES_DIR/${mod}.mo") bytes"
    else
        fail "${mod}.mo: NOT FOUND"
    fi
done
echo ""

# --- symbol file ----------------------------------------------------------
if [[ -f "$BUILD_DIR/70D_112.sym" ]]; then
    echo "✓ Symbol file: 70D_112.sym"
else
    fail "Symbol file: NOT FOUND"
fi
echo ""

# --- module sources -------------------------------------------------------
echo "=== Source Validation ==="
for src in modules/ptptun/ptptun.c modules/wifisrv/wifisrv.c \
           modules/wifitst/wifitst.c modules/ramdump/ramdump.c; do
    if [[ -f "$SCRIPT_DIR/$src" ]]; then
        echo "✓ $src"
    else
        fail "$src: NOT FOUND"
    fi
done

# hw_test is deprecated but must stay buildable on demand, so the source has to
# be present even though nothing builds it by default.
if [[ -f "$SCRIPT_DIR/modules/hw_test/hw_test.c" ]]; then
    echo "  ✓ modules/hw_test/hw_test.c kept (deprecated, not built)"
else
    fail "modules/hw_test/hw_test.c missing (should stay in tree)"
fi
echo ""

# --- modules.included -----------------------------------------------------
echo "=== modules.included ==="
for mod in ptptun wifisrv wifitst ramdump sd_clock; do
    if grep -qx "$mod" "$SCRIPT_DIR/platform/70D.112/modules.included"; then
        echo "✓ $mod"
    else
        fail "$mod NOT in modules.included"
    fi
done
echo ""

# --- deprecations ---------------------------------------------------------
echo "=== Deprecated modules ==="
if [[ -f "$MODULES_DIR/hw_test.mo" ]]; then
    fail "hw_test.mo was built - it is deprecated and must not be a default module"
else
    echo "✓ hw_test not built"
fi
if grep -qx "hw_test" "$SCRIPT_DIR/platform/70D.112/modules.included"; then
    fail "hw_test still listed in platform/70D.112/modules.included"
else
    echo "✓ hw_test not shipped"
fi
echo ""

# --- no leftover renames --------------------------------------------------
echo "=== Renamed modules ==="
for old in wifi_test wifi_enable sd_uhs; do
    if [[ -e "$SCRIPT_DIR/modules/$old" ]]; then
        fail "modules/$old still exists (renamed/removed)"
    else
        echo "✓ modules/$old gone"
    fi
done
echo ""

echo "=== Register Addresses (from crop_rec.c) ==="
grep -io "0x26B54\|0x2684C\|0xFF2BC6C4" "$SCRIPT_DIR/modules/crop_rec/crop_rec.c" | sort -u | head -3 || echo "Not found in crop_rec.c"
echo ""

if [[ $FAILED -eq 0 ]]; then
    echo "=== Validation PASSED ==="
    echo "Next: copy build/70d-latest/autoexec.bin to the card, or run ./deploy.sh"
    echo "Then ML menu -> Debug -> 'Dump RAM + serial flash' (hw_test is deprecated)."
else
    echo "=== Validation FAILED ==="
    exit 1
fi
