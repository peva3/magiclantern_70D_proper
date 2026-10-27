#!/usr/bin/env bash
#
# test_qemu.sh — Test any camera model in QEMU: build ML, launch, capture boot log
#
# Usage:
#   ./test_qemu.sh 70D               # Quick test 70D (10s)
#   ./test_qemu.sh EOSM              # Quick test EOS M (10s)
#   ./test_qemu.sh 70D --boot        # Boot from ML on SD card
#   ./test_qemu.sh EOSM --full       # Full boot attempt (60s)
#   ./test_qemu.sh 70D --gdb         # With GDB server
#   ./test_qemu.sh EOSM --debugmsg   # With DebugMsg logging
#
# ROM dumps must be in roms/<CAMERA>/ (ROM0.BIN, ROM1.BIN, SFDATA.BIN)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QEMU_DIR="$SCRIPT_DIR/qemu-eos"
QEMU_BIN="$QEMU_DIR/build/arm-softmmu/qemu-system-arm"
ROM_DIR="$SCRIPT_DIR/roms"
LOG_DIR="$SCRIPT_DIR/logs"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

CAMERA=""
TIMEOUT=10
GDB_MODE=0
BOOT_TRACE=0
BOOT_MODE=0
NO_BUILD=0
D_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --list) LIST_MODE=1; shift ;;
        -h|--help) HELP_MODE=1; shift ;;
        -*) echo "Unknown option: $1" >&2; exit 1 ;;
        *)
            if [ -z "$CAMERA" ] && [[ "$1" =~ ^[0-9A-Za-z]+$ ]]; then
                CAMERA="$1"; shift
            else
                echo "Unknown argument: $1" >&2; exit 1
            fi ;;
        --gdb) GDB_MODE=1; TIMEOUT=120; shift ;;
        --boot-trace) GDB_MODE=1; BOOT_TRACE=1; TIMEOUT=120; shift ;;
        --full) TIMEOUT=60; shift ;;
        --mpu) D_ARGS+=("mpu"); shift ;;
        --debugmsg) D_ARGS+=("debugmsg"); shift ;;
        --io) D_ARGS+=("io_quick"); shift ;;
        --tasks) D_ARGS+=("tasks"); shift ;;
        --calls) D_ARGS+=("calls"); shift ;;
         --boot) BOOT_MODE=1; TIMEOUT=180; shift ;;
         --no-build) NO_BUILD=1; shift ;;
         --timeout) TIMEOUT="$2"; shift; shift ;;
    esac
done

if [ -n "${LIST_MODE:-}" ] || [ -n "${HELP_MODE:-}" ]; then
    echo "Usage: $0 <CAMERA> [options]"
    echo ""
    echo "Options: --boot  --no-build  --timeout N  --gdb  --boot-trace  --full"
    echo "         --mpu --debugmsg --io --tasks --calls  --list"
    echo ""
    echo "Cameras supported by this QEMU build:"
    "$QEMU_BIN" -M help 2>/dev/null | awk 'NR>1 && NF {print "  " $1}' | grep -Ei '^  (70D|EOSM|EOSM2|100D|200D|5D3|6D|700D|650D|600D|60D|80D|7D|7D2|550D|500D|50D|5D2|1100D|1000D|450D|750D|760D|77D|800D|850D|90D|R|RP|M50|M6II)$' || echo "  (is qemu-system-arm built?)"
    echo ""
    echo "Models with an ML build in this tree:"
    ls -d "$SCRIPT_DIR"/platform/*.* 2>/dev/null | xargs -n1 basename | sed 's/^/  /'
    exit 0
fi

if [ -z "$CAMERA" ]; then
    echo "Usage: $0 <CAMERA> [options]   (--list to see what is available)"
    exit 1
fi

# Validate the camera against what this QEMU binary actually implements instead of
# against a hand-maintained list (the old list both rejected supported models and
# accepted ones QEMU cannot run).
if [ -x "$QEMU_BIN" ] && ! "$QEMU_BIN" -M help 2>/dev/null | awk '{print $1}' | grep -qxF "$CAMERA"; then
    echo "✗ $CAMERA is not a machine in $QEMU_BIN" >&2
    echo "  Run '$0 --list' to see the available models." >&2
    exit 1
fi

mkdir -p "$LOG_DIR"

CAM_ROM_DIR="$ROM_DIR/$CAMERA"
SERIAL_LOG="$LOG_DIR/serial_${CAMERA}_${TIMESTAMP}.log"

# ── check ROMs ────────────────────────────────────────────────────────────────
if [ ! -f "$CAM_ROM_DIR/ROM1.BIN" ]; then
    echo "WARNING: No ROMs found for $CAMERA at $CAM_ROM_DIR"
    echo "Creating placeholder ROMs for basic model testing..."
    mkdir -p "$CAM_ROM_DIR"
    python3 -c "
import struct
data = bytearray(16 * 1024 * 1024)
data[0:4] = struct.pack('<I', 0xE12FFF1E)  # BX LR
data[0x100:0x110] = b'DRYOS 2.3 r51'
data[0x200:0x210] = ('${CAMERA}_PLACEHOLDER'.encode() + b' ' * 16)[:16]
with open('${CAM_ROM_DIR}/ROM1.BIN', 'wb') as f: f.write(data)
data0 = bytearray(8 * 1024 * 1024)
data0[0:8] = ('${CAMERA}_ASST'.encode() + b' ' * 8)[:8]
with open('${CAM_ROM_DIR}/ROM0.BIN', 'wb') as f: f.write(data0)
_sf = bytearray(16 * 1024 * 1024)
_sf[0:10] = ('${CAMERA}_SFD'.encode() + b' ' * 10)[:10]
with open('${CAM_ROM_DIR}/SFDATA.BIN', 'wb') as f: f.write(_sf)
print('Placeholder ROMs created')
"
fi

# ── build debug args ──────────────────────────────────────────────────────────
DEBUG_OPTS=()
if [ ${#D_ARGS[@]} -gt 0 ]; then
    DEBUG_OPTS=("-d" "$(IFS=,; echo "${D_ARGS[*]}")")
fi

# ── QEMU command ──────────────────────────────────────────────────────────────
QEMU_CMD=("$QEMU_BIN" "-M" "$CAMERA" "-nographic" "-display" "none"
    "${DEBUG_OPTS[@]}"
    "-drive" "if=sd,format=qcow2,file=$QEMU_DIR/sd.qcow2,cache=unsafe")

# ── QEMU_EOS_WORKDIR for ROM path resolution ────────────────────────────────────
export QEMU_EOS_WORKDIR="$ROM_DIR"

# ── QEMU_EOS_DEBUGMSG for DebugMsg logging ─────────────────────────────────────
if [[ " ${D_ARGS[*]} " =~ " debugmsg " ]]; then
    case "$CAMERA" in
        70D)  export QEMU_EOS_DEBUGMSG="0xFFA50C3C" ;;
        EOSM) export QEMU_EOS_DEBUGMSG="0xFF1BE4AC" ;;
        *)    echo "DebugMsg address not known for $CAMERA" ;;
    esac
fi

if [ "$BOOT_MODE" -eq 1 ]; then
    # Find the platform directory that actually holds a build.  The old code kept a
    # hand-maintained camera->firmware map that fell through to a bogus "202"
    # default, producing platform/<cam>.202/build paths that never exist.
    ML_BUILD_DIR=""
    for cand in "$SCRIPT_DIR/platform/${CAMERA}."*/build; do
        [ -f "$cand/autoexec.bin" ] || continue
        ML_BUILD_DIR="$cand"
        break
    done
    if [ -n "$ML_BUILD_DIR" ] && [ -f "$ML_BUILD_DIR/autoexec.bin" ]; then
        echo "Booting $CAMERA from ML build..."
        SD_IMG="$QEMU_DIR/sd_${CAMERA}.qcow2"
        if [ ! -f "$SD_IMG" ]; then
            # Prefer the platform script: it knows the image format, the ml build
            # contents and the correct mcopy invocation for this body.
            MK_SD="$SCRIPT_DIR/platform/$(basename "$(dirname "$(dirname "$ML_BUILD_DIR")")")/create_sd_image.sh"
            if [ -x "$MK_SD" ]; then
                echo "Building SD image via $MK_SD ..."
                "$MK_SD" || { echo "✗ create_sd_image.sh failed" >&2; exit 1; }
            fi
        fi
        if [ ! -f "$SD_IMG" ]; then
            echo "✗ No SD image at $SD_IMG and no platform create_sd_image.sh to make one." >&2
            echo "  Run without --boot to test the model only." >&2
            exit 1
        fi
        # The image is qcow2, so it must be declared as qcow2 here; the old code
        # created a raw file but passed format=qcow2, which QEMU refuses to open.
        SD_FMT=qcow2
        case "$SD_IMG" in
            *.raw|*.img) SD_FMT=raw ;;
        esac
        # Only add a CF drive when this model actually declares a CF interrupt;
        # grepping the whole file always matched some other camera.
        CF_ARGS=()
        CF_IMG="$QEMU_DIR/cf_${CAMERA}.raw"
        if awk "/MODEL_NAME_${CAMERA}\"/,/\},/" "$QEMU_DIR/hw/eos/model_list.c" 2>/dev/null | grep -q "cf_driver_interrupt *= *0x" ; then
            [ -f "$CF_IMG" ] || qemu-img create -f raw "$CF_IMG" 64M >/dev/null 2>&1
            CF_ARGS=("-drive" "if=ide,format=raw,file=$CF_IMG,cache=unsafe")
        fi
        # NOTE: no "-serial file:$SERIAL_LOG" here -- the run below is also piped
        # through "tee $SERIAL_LOG", and two writers to the same path interleave
        # and truncate each other.
        QEMU_CMD=("$QEMU_BIN" "-M" "$CAMERA" "-nographic"
            "-drive" "if=sd,format=$SD_FMT,file=$SD_IMG,cache=unsafe"
            "${CF_ARGS[@]}"
            "${DEBUG_OPTS[@]}"
            "-display" "none")
    else
        echo "No ML build found under platform/${CAMERA}.*/build"
        echo "Build one first, or run without --boot to test the model only."
        exit 1
    fi
    echo "ML build: $ML_BUILD_DIR"
fi

echo "============================================"
echo " QEMU $CAMERA Test | Timeout: ${TIMEOUT}s"
echo " Log: $SERIAL_LOG"
echo "============================================"

# ── GDB mode ─────────────────────────────────────────────────────────────────
if [ "$GDB_MODE" -eq 1 ]; then
    CMD_GDB="$QEMU_DIR/magiclantern/cam_config/$CAMERA"
    if [ "$BOOT_TRACE" -eq 1 ] && [ -f "$CMD_GDB/boot.gdb" ]; then
        GDB_SCRIPT="$CMD_GDB/boot.gdb"
    elif [ -f "$CMD_GDB/debugmsg.gdb" ]; then
        GDB_SCRIPT="$CMD_GDB/debugmsg.gdb"
    elif [ -f "$CMD_GDB/patches.gdb" ]; then
        GDB_SCRIPT="$CMD_GDB/patches.gdb"
    else
        echo "No GDB scripts for $CAMERA at $CMD_GDB"
        exit 1
    fi
    echo "GDB script: $GDB_SCRIPT"
    "${QEMU_CMD[@]}" -s -S &
    QEMU_PID=$!
    sleep 1
    arm-none-eabi-gdb -batch -x "$GDB_SCRIPT" \
        -ex "target remote :1234" \
        -ex "echo --- done ---\n" 2>&1 | tee "$SERIAL_LOG" || true
    kill $QEMU_PID 2>/dev/null || true
    wait $QEMU_PID 2>/dev/null || true
else
    timeout "$TIMEOUT" "${QEMU_CMD[@]}" 2>&1 | tee "$SERIAL_LOG" || true
fi

echo ""
echo "=== Analysis ==="
if grep -q "startupInitializeComplete\|K325 READY\|ML started\|ml_started" "$SERIAL_LOG" 2>/dev/null; then
    echo "  Firmware booted successfully"
elif grep -q "PLACEHOLDER\|E12FFF1E" "$SERIAL_LOG" 2>/dev/null; then
    echo "  Placeholder ROMs (no real firmware)"
elif [ -s "$SERIAL_LOG" ]; then
    echo "  Model loaded (check log for details)"
else
    echo "  No output captured"
fi
