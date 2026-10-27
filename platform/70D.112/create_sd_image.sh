#!/usr/bin/env bash
#
# create_sd_image.sh — Create a QEMU SD card image with Magic Lantern for the 70D
#
# Usage: ./create_sd_image.sh [--no-clean]
#
#   --no-clean   keep the previous image contents instead of wiping ML/ first
#
# Produces build/sd.qcow2, suitable for QEMU emulation with the ML autoexec.bin
# and modules.  The 70D is SD-only; no CF drive is needed.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
ML_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
QEMU_DIR="$ML_DIR/qemu-eos"
BASE_SD="$QEMU_DIR/magiclantern/disk_images/sd.qcow2"
BASE_SD_XZ="$QEMU_DIR/magiclantern/disk_images/sd.qcow2.xz"
PARTITION_OFFSET=50688
SD_IMG_TARGET="$BUILD_DIR/sd.qcow2"

NO_CLEAN=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-clean) NO_CLEAN=1; shift ;;
        -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; echo "Usage: $0 [--no-clean]" >&2; exit 1 ;;
    esac
done

if [[ ! -f "$BUILD_DIR/autoexec.bin" ]]; then
    echo "ERROR: No autoexec.bin found. Run 'make' first."
    exit 1
fi

echo "=== Creating QEMU SD card image for 70D ==="

QEMU_IMG="$QEMU_DIR/build/qemu-img"
if [[ ! -x "$QEMU_IMG" ]]; then
    echo "ERROR: qemu-img not found at $QEMU_IMG"
    echo "Build QEMU first: cd $QEMU_DIR && ./configure && make"
    exit 1
fi

# mtools is a hard dependency: without it every mcopy/mdel/mmd/mdir below fails
# and, because of `set -e`, the script dies with a bare "command not found" far
# from the real cause.
for tool in mcopy mdel mmd mdir mdir; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: '$tool' not found. Install mtools:" >&2
        echo "  apt-get install mtools    /    brew install mtools" >&2
        exit 1
    fi
done

WORK_DIR=$(mktemp -d)
trap 'rm -rf "$WORK_DIR"' EXIT

if [[ ! -f "$BASE_SD" ]]; then
    if [[ -f "$BASE_SD_XZ" ]]; then
        echo "Decompressing base SD image..."
        # Decompress into the work dir first: a partial write straight onto
        # $BASE_SD would be reused forever afterwards, because the existence
        # check above would then succeed.
        if ! xz -dc "$BASE_SD_XZ" > "$WORK_DIR/base.qcow2"; then
            echo "ERROR: failed to decompress $BASE_SD_XZ" >&2
            exit 1
        fi
        mv "$WORK_DIR/base.qcow2" "$BASE_SD"
    else
        echo "ERROR: No base SD image found at $BASE_SD or $BASE_SD_XZ"
        exit 1
    fi
fi

echo "Converting base image to raw..."
"$QEMU_IMG" convert -f qcow2 -O raw "$BASE_SD" "$WORK_DIR/sd.raw"

IMG=("$WORK_DIR/sd.raw@@$PARTITION_OFFSET")

if [[ "$NO_CLEAN" -eq 0 ]]; then
    # Wipe the whole ML tree, not just two files: a stale .mo left behind is
    # still loaded at boot, which is exactly the S3.1 "stale SD image" failure.
    echo "Removing old ML files from image..."
    mdel -i "${IMG[0]}" ::AUTOEXEC.BIN 2>/dev/null || true
    if mdir -i "${IMG[0]}" ::ML >/dev/null 2>&1; then
        mdeltree -i "${IMG[0]}" ::ML >/dev/null 2>&1 || true
    fi
else
    echo "Keeping existing image contents (--no-clean)"
fi

echo "Installing ML build to SD image..."
mcopy -i "${IMG[0]}" "$BUILD_DIR/autoexec.bin" ::AUTOEXEC.BIN

# Copy from build/zip/ML (standard location) or build/ML (legacy)
ML_SRC=""
if [[ -d "$BUILD_DIR/zip/ML" ]]; then
    ML_SRC="$BUILD_DIR/zip/ML"
elif [[ -d "$BUILD_DIR/ML" ]]; then
    ML_SRC="$BUILD_DIR/ML"
fi

if [[ -n "$ML_SRC" ]]; then
    # FAT is case-insensitive in hardware but QEMU's emulation may not be, so
    # ship the module directory in upper case.
    ML_TMP="$WORK_DIR/ML"
    cp -r "$ML_SRC" "$ML_TMP"
    if [[ -d "$ML_TMP/modules" ]]; then
        mv "$ML_TMP/modules" "$ML_TMP/MODULES"
    fi

    echo "Copying ML directory from $ML_TMP..."
    mmd -i "${IMG[0]}" ::ML 2>/dev/null || true

    # Copy regular files at the ML root; directories are handled below with
    # explicit recursion (-s).  A bare "mcopy dir/*" refuses directory operands.
    find "$ML_TMP" -maxdepth 1 -type f -print0 |
        xargs -0 -r mcopy -i "${IMG[0]}" -t ::ML/

    for sub in MODULES SETTINGS FONTS DATA DOC SCRIPTS; do
        if [[ -d "$ML_TMP/$sub" ]]; then
            echo "Copying $sub..."
            mcopy -s -i "${IMG[0]}" "$ML_TMP/$sub" "::ML/$sub"
        fi
    done
else
    echo "WARNING: No ML directory found, modules won't be available"
fi

if [[ -f "$BUILD_DIR/MAGIC.CFG" ]]; then
    mcopy -i "${IMG[0]}" "$BUILD_DIR/MAGIC.CFG" ::ML/SETTINGS/MAGIC.CFG 2>/dev/null || true
fi

echo "Verifying installed files..."
mdir -i "${IMG[0]}" ::

echo "Converting to qcow2..."
"$QEMU_IMG" convert -f raw -O qcow2 "$WORK_DIR/sd.raw" "$SD_IMG_TARGET"

filesize() { stat --format="%s" "$1" 2>/dev/null || stat -f "%z" "$1" 2>/dev/null || echo 0; }
SD_SIZE=$(filesize "$SD_IMG_TARGET")
AUTOEXEC_SIZE=$(filesize "$BUILD_DIR/autoexec.bin")

echo ""
echo "=== Done ==="
echo "  sd.qcow2: $(( SD_SIZE / 1024 ))KB ($SD_SIZE bytes)"
echo "  autoexec.bin: $(( AUTOEXEC_SIZE / 1024 ))KB"
echo "  modules: $(ls "$BUILD_DIR/zip/ML/modules"/*.mo 2>/dev/null | wc -l)"
echo ""
echo "  Run QEMU with: ./test_70d_qemu.sh --boot --no-build"
