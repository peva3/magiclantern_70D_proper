/* RAM + serial flash dumper for offline reverse engineering.
 *
 * Two very different address spaces are handled here, and it matters that they
 * are not confused:
 *
 *   RAM   0x40000000-0x60000000 on the 70D is ordinary memory.  Some of it is
 *         uninitialised heap filled with 0xFFFFFFFF or 0xAAAAAAAA, so those
 *         chunks are skipped -- but a skipped chunk is recorded in the .MAP
 *         sidecar file so the dump stays address-mappable.  Without the map a
 *         512 MB region that skips 3 MB of holes produces a file that cannot be
 *         mapped back to addresses, which defeats the purpose.
 *
 *   Serial flash is NOT memory mapped.  Reading 0x00000000 as if it were returns
 *         DryOS module code (that is where loaded modules live), not calibration
 *         data.  It must be read through the SPI stubs, exactly as
 *         modules/dev_tools/sf_dump/sf_dump.c does.
 *
 * Everything runs in a separate task: a full RAM dump is ~500 MB of writes and
 * can take minutes, which must not block the module loader.
 */

#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <config.h>
#include <menu.h>
#include <fio-ml.h>

#ifdef CONFIG_EOSM
#define RAM_START    0x40000000
#define RAM_END      0x50000000
#define RAM_LABEL    "EOS M (256MB)"
#else
#define RAM_START    0x40000000
#define RAM_END      0x60000000
#define RAM_LABEL    "70D (512MB)"
#endif

#define CHUNK        262144        /* 256 KB per read/write */
#define RAM_FILE     "ML/LOGS/RAM_DUMP.BIN"
#define RAM_MAP      "ML/LOGS/RAM_DUMP.MAP"
#define SF_FILE      "ML/LOGS/SF_DUMP.BIN"

/* 70D serial flash: 16 MB, i.e. the same size as the SFDATA.BIN dumps taken
 * from the camera.  Addresses are the ones sf_dump.c uses for this body. */
#define SF_SIZE      0x01000000

static int (*SF_CreateSerial)(void) = NULL;
static int (*SF_readSerialFlash)(int src, void *dest, int size) = NULL;
static int (*SF_Destroy)(void) = NULL;

static void set_sf_stubs(void)
{
    if (is_camera("70D", "1.1.2"))
    {
        SF_CreateSerial    = (void *) 0xFF144D2C;
        SF_readSerialFlash = (void *) 0xFF144CD4;
        SF_Destroy         = (void *) 0xFF14771C;
    }
    else if (is_camera("EOSM", "2.0.2"))
    {
        SF_CreateSerial    = (void *) 0xFF1385AC;
        SF_readSerialFlash = (void *) 0xFF138554;
        SF_Destroy         = (void *) 0xFF13AF5C;
    }
}

/* Write exactly len bytes, retrying on short writes.
 * FIO_WriteFile() returns the number of bytes written and FAT does return short
 * counts; a single call that writes 4 KB of a 256 KB chunk would otherwise
 * silently shift every subsequent byte by the difference. */
static int write_all(FILE *fp, uint8_t *buf, uint32_t len)
{
    uint32_t done = 0;
    while (done < len)
    {
        int w = FIO_WriteFile(fp, buf + done, (int)(len - done));
        if (w <= 0) return 0;
        done += (uint32_t) w;
    }
    return 1;
}

static void dump_progress(const char *label, uint32_t offset, uint32_t total,
                          uint32_t written, uint32_t skipped)
{
    int pct = (int)((uint64_t) offset * 100 / total);
    bmp_printf(FONT_MED, 20, 140, "%s: %dMB/%dMB (%d%%)  ", label,
               offset >> 20, total >> 20, pct);
    /* `skipped` counts 256 KB chunks, not 4 KB pages -- label it accordingly. */
    bmp_printf(FONT_MED, 20, 160, "Written: %dMB  Skipped: %d chunks  ",
               written >> 20, skipped);
}

static int dump_region(const char *filename, const char *mapname, const char *label,
                       uint32_t start, uint32_t size)
{
    uint8_t *buf = fio_malloc(CHUNK);
    if (!buf) return 0;

    FILE *fp = FIO_CreateFile(filename);
    if (!fp) { fio_free(buf); return 0; }

    FILE *mf = FIO_CreateFile(mapname);
    /* The map is a convenience, not a requirement -- carry on without it. */

    uint32_t total_written = 0, skipped = 0, offset = 0;
    int ok = 1;

    while (offset < size && ok)
    {
        /* Clamp the tail so the final chunk never reads past the region. */
        uint32_t remaining = size - offset;
        uint32_t n = (remaining < CHUNK) ? remaining : CHUNK;
        uint32_t src = start + offset;

        /* Probe both ends of the chunk: an erased block has the same pattern at
         * the start and at the end, whereas a block that merely begins with a
         * zero-ish word is real data. */
        uint32_t first = *(volatile uint32_t *)(uintptr_t) src;
        uint32_t last  = *(volatile uint32_t *)(uintptr_t)(src + n - 4);
        int unmapped =
            (first == 0xFFFFFFFF && last == 0xFFFFFFFF) ||
            (first == 0xAAAAAAAA && last == 0xAAAAAAAA);

        if (unmapped)
        {
            if (mf) FIO_WriteFile(mf, (uint8_t *) "0", 1);
            offset += n;
            skipped++;
            continue;
        }

        if (mf) FIO_WriteFile(mf, (uint8_t *) "1", 1);

        memcpy(buf, (void *)(uintptr_t) src, n);

        if (!write_all(fp, buf, n)) { ok = 0; break; }
        total_written += n;
        offset += n;

        if ((offset & 0x3FFFFF) == 0)
            dump_progress(label, offset, size, total_written, skipped);
    }

    if (mf) { FIO_WriteFile(mf, (uint8_t *) "\n", 1); FIO_CloseFile(mf); }
    fio_free(buf);
    FIO_CloseFile(fp);

    /* A region only counts as dumped if every byte made it to the card. */
    return ok && total_written == size;
}

static int dump_serial_flash(const char *filename)
{
    if (!SF_CreateSerial || !SF_readSerialFlash || !SF_Destroy)
        return 0;

    uint8_t *buf = fio_malloc(65536);
    if (!buf) return 0;

    FILE *fp = FIO_CreateFile(filename);
    if (!fp) { fio_free(buf); return 0; }

    bmp_printf(FONT_MED, 20, 100, "Dumping serial flash (%dMB)...", SF_SIZE >> 20);

    int ok = 1;
    SF_CreateSerial();

    for (uint32_t i = 0; i < SF_SIZE && ok; i += 65536)
    {
        uint32_t n = (SF_SIZE - i < 65536) ? (SF_SIZE - i) : 65536;
        SF_readSerialFlash(i, buf, (int) n);
        if (!write_all(fp, buf, n)) { ok = 0; break; }
        if ((i & 0xFFFFF) == 0)
            bmp_printf(FONT_MED, 20, 140, "Serial flash: %d%%  ",
                       (int)((uint64_t)(i + n) * 100 / SF_SIZE));
    }

    SF_Destroy();
    fio_free(buf);
    FIO_CloseFile(fp);
    return ok;
}

static void ramdump_task(void *unused)
{
    extern int GetBatteryLevel(void);
    int batt = GetBatteryLevel();
    if (batt < 50)
    {
        bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
        bmp_printf(FONT_LARGE, 20, 100, "Low battery (%d%%)!", batt);
        bmp_printf(FONT_MED, 20, 140, "Charge battery before dumping.");
        msleep(5000);
        return;
    }

    /* Refuse to start rather than half-write a 500 MB file and fill the card. */
    struct card_info *card = get_ml_card();
    uint32_t free_bytes = card ? (uint32_t) get_free_space_32k(card) * 32768 : 0;
    uint32_t need = (RAM_END - RAM_START) + SF_SIZE;
    if (!card || free_bytes < need)
    {
        bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
        bmp_printf(FONT_LARGE, 20, 100, "Not enough space");
        bmp_printf(FONT_MED, 20, 140, "Need %dMB, have %dMB",
                   need >> 20, free_bytes >> 20);
        msleep(5000);
        return;
    }

    set_sf_stubs();

    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    bmp_printf(FONT_LARGE, 20, 30, "RAM + Serial Flash Dump");
    bmp_printf(FONT_MED, 20, 60, "Camera: %s", RAM_LABEL);
    bmp_printf(FONT_MED, 20, 80, "Dumping RAM 0x%08x-0x%08x",
               RAM_START, RAM_END - 1);
    bmp_printf(FONT_MED, 20, 100, "Do NOT power off or remove battery!");
    msleep(2000);

    int ram_ok = 0, sf_ok = 0;

    bmp_printf(FONT_MED, 20, 140, "Dumping RAM...");
    ram_ok = dump_region(RAM_FILE, RAM_MAP, "RAM", RAM_START, RAM_END - RAM_START);

    if (ram_ok)
    {
        bmp_printf(FONT_MED, 20, 140, "RAM dump complete. Dumping SF...");
        sf_ok = dump_serial_flash(SF_FILE);
    }

    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    bmp_printf(FONT_LARGE, 20, 30, ram_ok && sf_ok ? "Dump Complete!" : "Dump INCOMPLETE");
    bmp_printf(FONT_MED, 20, 60, "Files on SD card:");
    bmp_printf(FONT_MED, 20, 80, "  %s %s", RAM_FILE, ram_ok ? "" : "(FAILED)");
    bmp_printf(FONT_MED, 20, 100, "  %s %s", RAM_MAP,
               ram_ok ? "(1 chunk = 1 byte: 1=present 0=skipped)" : "");
    bmp_printf(FONT_MED, 20, 120, "  %s %s", SF_FILE, sf_ok ? "" : "(FAILED)");
    bmp_printf(FONT_SMALL, 20, 160, "Use RAM_DUMP.MAP to map file offsets back to addresses.");

    msleep(15000);
}

static struct menu_entry ramdump_menu[] =
{
    {
        .name   = "Dump RAM + serial flash",
        .select = run_in_separate_task,
        .priv   = ramdump_task,
        .help   = "Writes ML/LOGS/RAM_DUMP.BIN (plus .MAP) and SF_DUMP.BIN. "
                  "Needs ~530MB free and a charged battery. Takes minutes.",
    }
};

static unsigned int ramdump_init(void)
{
    set_sf_stubs();
    menu_add("Debug", ramdump_menu, COUNT(ramdump_menu));

    if (config_flag_file_setting_load("ML/SETTINGS/RAMDUMP.RUN"))
    {
        /* Clear the flag *before* starting: a dump that never finishes must not
         * re-arm on the next boot (that would fill the card eventually). */
        config_flag_file_setting_save("ML/SETTINGS/RAMDUMP.RUN", 0);
        task_create("ramdump", 0x1c, 0x2000, ramdump_task, 0);
    }
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(ramdump_init)
MODULE_INFO_END()
