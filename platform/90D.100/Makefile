MODEL = 90D
FW_VERSION = 111

TOP_DIR = ../..

AUTOEXEC_BASE = 0x800000
FIR_BASE = 0x800120

#CANON_FIR  = 90D00111.FIR
FIRMWARE_ID = 0x8000043D
MAIN_FIRMWARE_ADDR = 0xE0040000

# user_mem_start from cstart struct (TODO: find from firmware RE)
RESTARTSTART = 0x200000

# Cortex A9, binaries loaded as Thumb
PLATFORM_ARCH = armv7-a
CFLAGS_PLATFORM += -mthumb -mlong-calls

ML_BOOT_OBJ = boot-d678.o
ML_SRC_EXTRA_OBJS += $(BUILD_DIR)/function_overrides.o

include ../Makefile
.DEFAULT_GOAL := $(BUILD_DIR)/cam_complete
