#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <config.h>

#ifdef CONFIG_EOSM
#define RAM_START    0x40000000
#define RAM_END      0x50000000
#define RAM_LABEL    "EOS M (256MB)"
#define SF_SIZE      0x00800000
#else
#define RAM_START    0x40000000
#define RAM_END      0x60000000
#define RAM_LABEL    "70D (512MB)"
#define SF_SIZE      0x00800000
#endif

static void dump_progress(const char *label, uint32_t offset, uint32_t total,
                          uint32_t written, uint32_t skipped)
{
    int pct = (int)((uint64_t)offset * 100 / total);
    bmp_printf(FONT_MED, 20, 140, "%s: %dMB/%dMB (%d%%)  ", label,
               offset >> 20, total >> 20, pct);
    bmp_printf(FONT_MED, 20, 160, "Written: %dMB  Skipped: %d pages  ",
               written >> 20, skipped);
}

static int dump_region(const char *filename, const char *label,
                       uint32_t start, uint32_t size)
{
    uint32_t chunk = 262144;
    uint8_t *buf = fio_malloc(chunk);
    if (!buf) return 0;

    FILE *fp = FIO_CreateFile(filename);
    if (!fp) { fio_free(buf); return 0; }

    uint32_t total_written = 0, skipped = 0, offset = 0;
    int ok = 1;

    while (offset < size && ok) {
        uint32_t src = start + offset;
        uint32_t probe = *(volatile uint32_t *)(uintptr_t)src;
        if (probe == 0xFFFFFFFF || (probe == 0x55555555)) {
            offset += chunk;
            skipped++;
            continue;
        }
        memcpy(buf, (void *)(uintptr_t)src, chunk);
        int w = FIO_WriteFile(fp, buf, chunk);
        if (w <= 0) { ok = 0; break; }
        total_written += w;
        offset += chunk;

        if ((offset & 0x3FFFFF) == 0)
            dump_progress(label, offset, size, total_written, skipped);
    }

    fio_free(buf);
    FIO_CloseFile(fp);
    return ok;
}

static int dump_serial_flash(const char *filename)
{
    bmp_printf(FONT_MED, 20, 100, "Dumping serial flash (%dMB)...",
               SF_SIZE >> 20);
    return dump_region(filename, "Serial Flash", 0x00000000, SF_SIZE);
}

static void ramdump_auto(void)
{
    extern int GetBatteryLevel(void);
    int batt = GetBatteryLevel();
    if (batt < 50) {
        bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
        bmp_printf(FONT_LARGE, 20, 100, "Low battery (%d%%)!", batt);
        bmp_printf(FONT_MED, 20, 140, "Charge battery before dumping.");
        msleep(5000);
        return;
    }

    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    bmp_printf(FONT_LARGE, 20, 30, "RAM + Serial Flash Dump");
    bmp_printf(FONT_MED, 20, 60, "Camera: %s", RAM_LABEL);
    bmp_printf(FONT_MED, 20, 80, "Dumping RAM 0x%08x-0x%08x",
               RAM_START, RAM_END - 1);
    bmp_printf(FONT_MED, 20, 100, "Do NOT power off or remove battery!");
    msleep(2000);

    int all_ok = 1;

    bmp_printf(FONT_MED, 20, 140, "Dumping RAM...");
    all_ok = dump_region("ML/LOGS/RAM_DUMP.BIN", "RAM",
                         RAM_START, RAM_END - RAM_START);

    if (all_ok) {
        bmp_printf(FONT_MED, 20, 140, "RAM dump complete. Dumping SF...");
        all_ok = dump_serial_flash("ML/LOGS/SF_DUMP.BIN");
    }

    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    if (all_ok) {
        bmp_printf(FONT_LARGE, 20, 30, "Dump Complete!");
        bmp_printf(FONT_MED, 20, 60, "Files on SD card:");
        bmp_printf(FONT_MED, 20, 80, "  ML/LOGS/RAM_DUMP.BIN");
        bmp_printf(FONT_MED, 20, 100, "  ML/LOGS/SF_DUMP.BIN");
        bmp_printf(FONT_SMALL, 20, 140, "Copy to PC for QEMU emulation.");
    } else {
        bmp_printf(FONT_LARGE, 20, 30, "Dump FAILED");
        bmp_printf(FONT_MED, 20, 60, "Check SD card space.");
    }

    msleep(15000);
    bmp_off();
}

static unsigned int ramdump_init(void)
{
    if (config_flag_file_setting_load("ML/SETTINGS/RAMDUMP.RUN"))
    {
        ramdump_auto();
        config_flag_file_setting_save("ML/SETTINGS/RAMDUMP.RUN", 0);
    }
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(ramdump_init)
MODULE_INFO_END()
