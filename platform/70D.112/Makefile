MODEL = 70D
FW_VERSION = 112

TOP_DIR = ../..

AUTOEXEC_BASE = 0x800000
FIR_BASE = 0x800120

#CANON_FIR = 70D00112.FIR
MAIN_FIRMWARE_ADDR = 0xFF0C0000
PLATFORM_ARCH = armv5te

ML_SRC_EXTRA_OBJS = $(BUILD_DIR)/afma.o $(BUILD_DIR)/reloc.o

# The 70D uses the 5D3 bitrate implementation.  Note that bitrate/nitrate
# control is NOT disabled: FEATURE_NITRATE comes from all_features.h, and the
# original `#undef FEATURE_NITRATE` was dropped from features.h long ago.
ML_BITRATE_OBJ = bitrate-5d3.o

# Load ML at the beginning of the AllocateMemory pool
# default 0x44C000 - 0xD3C000, patched to 0x4E0000 - 0xD3C000 (592K for us).
RESTARTSTART = 0x44C100
ML_BOOT_OBJ = boot-d45-am.o

include ../Makefile

# QEMU builds: pass the flag as a make VARIABLE, i.e.
#     make CONFIG_QEMU=y
# Do NOT use an environment assignment (`CONFIG_QEMU=y make`): ../Makefile
# assigns CONFIG_QEMU = n unconditionally during the include above, and
# Makefile.globals has already evaluated it, so an environment value is
# silently ignored and you get a HARDWARE build.  A stray CONFIG_QEMU=y in a
# hardware build is how the red-LED hang happens, so there is deliberately no
# `?=` here - it cannot override anything and only misleads. */
# CFLAGS += -DCONFIG_QEMU  (handled by Makefile.globals when CONFIG_QEMU=y)

.DEFAULT_GOAL := $(BUILD_DIR)/cam_complete

# Deployment target - copies build to 70d-latest folder
.PHONY: deploy
deploy:
	@echo "=== Deploying to 70d-latest ==="
	@cp build/autoexec.bin ../../70d-latest/
	@cp build/magiclantern.bin ../../70d-latest/
	@cp build/magiclantern.zip ../../70d-latest/
	@cp build/70D_112.sym ../../70d-latest/
	@unzip -o ../../70d-latest/magiclantern.zip -d ../../70d-latest/ > /dev/null 2>&1
	@echo "✓ Deployed to 70d-latest/"
	@ls -lh ../../70d-latest/autoexec.bin
