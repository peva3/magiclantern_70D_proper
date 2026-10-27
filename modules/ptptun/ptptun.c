#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <property.h>
#include <mem.h>
#include <config.h>
#include "ptptun.h"

#define PTP_RC_OK               0x2001
#define PTP_RC_GeneralError     0x2002
#define PTP_RC_ParameterNotSupported 0x2006
#define BUF_SIZE 128 * 1024

struct ptp_msg {
    uint32_t id;
    uint32_t session;
    uint32_t transaction;
    uint32_t param_count;
    uint32_t param[5];
} __attribute__((packed));

struct ptp_context {
    struct ptp_handle *handle;
    int (*send_data)(struct ptp_handle *handle, void *buf, int part_size, int total_size, int, int, int);
    int (*recv_data)(struct ptp_handle *handle, void *buf, size_t len, void (*callback)(void *cb_priv, int status), void *cb_priv);
    int (*send_resp)(struct ptp_handle *handle, struct ptp_msg *msg);
    int (*get_data_size)(struct ptp_handle *handle);
    void *off_0x14;
    void *off_0x18;
    void *off_0x1c;
};

extern uint32_t ptp_register_handler(uint32_t id, void *handler, void *priv);
extern int take_screenshot(char *filename, uint32_t mode);

/* Addresses below 0x80000000 that are safe to dereference from the tunnel.
 * Anything outside these windows will fault the camera, so reads are refused
 * rather than blindly dereferenced. */
static int address_is_readable(uint32_t addr)
{
    if (addr >= 0xC0000000u && addr <= 0xDFFFFFFFu) return 1;   /* MMIO / ENGIO */
    if (addr >= 0x40000000u && addr <= 0x5FFFFFFFu) return 1;   /* RAM          */
    if (addr >= 0xF0000000u) return 1;                          /* ROM1 / boot  */
    return 0;
}

/* ENGIO writes can permanently break sensor/video timing, so they are gated.
 * The palette registers are always writable (ML itself owns them); anything
 * else requires the operator to drop ML/SETTINGS/PTP_UNSAFE.WR on the card. */
#define PALETTE_LO 0xC0F14080u
#define PALETTE_HI 0xC0F140BCu
static int unsafe_writes_enabled;
static int engio_write_allowed(uint32_t addr)
{
    if (addr >= PALETTE_LO && addr <= PALETTE_HI) return 1;
    return unsafe_writes_enabled;
}

static int send_data(struct ptp_context *ctx, const char *buf, int size)
{
    int tmp = size;
    while (size >= BUF_SIZE) {
        if (ctx->send_data(ctx->handle, (void *)buf, BUF_SIZE, tmp, 0, 0, 0))
            return 0;
        tmp = 0; size -= BUF_SIZE; buf += BUF_SIZE;
    }
    if (size && ctx->send_data(ctx->handle, (void *)buf, size, tmp, 0, 0, 0))
        return 0;
    return 1;
}

static int recv_data(struct ptp_context *ctx, char *buf, int size)
{
    while (size >= BUF_SIZE) {
        ctx->recv_data(ctx->handle, buf, BUF_SIZE, 0, 0);
        size -= BUF_SIZE; buf += BUF_SIZE;
    }
    if (size) ctx->recv_data(ctx->handle, buf, size, 0, 0);
    return 1;
}

static void send_ok(struct ptp_context *ctx, uint32_t session, uint32_t transaction, uint32_t v0, uint32_t v1)
{
    struct ptp_msg m = { .id = PTP_RC_OK, .session = session, .transaction = transaction,
        .param_count = 2, .param = { v0, v1, 0, 0, 0 } };
    ctx->send_resp(ctx->handle, &m);
}

static void send_err(struct ptp_context *ctx, uint32_t session, uint32_t transaction)
{
    struct ptp_msg m = { .id = PTP_RC_GeneralError, .session = session, .transaction = transaction };
    ctx->send_resp(ctx->handle, &m);
}

static void handle_CallByName(struct ptp_context *ctx, uint32_t session, uint32_t transaction, uint32_t param2)
{
    int size = ctx->get_data_size(ctx->handle);
    /* smallest valid payload is a 1-char name plus NUL: 2 bytes */
    if (size < 2) { send_err(ctx, session, transaction); return; }

    char *buf = fio_malloc(size + 1);
    if (!buf) { send_err(ctx, session, transaction); return; }
    if (!recv_data(ctx, buf, size)) { fio_free(buf); send_err(ctx, session, transaction); return; }
    buf[size] = 0;

    int arg_count = param2 & 0xFF;
    if (arg_count > 6) arg_count = 6;

    uint32_t args[6] = {0};
    char *func_name = buf;
    unsigned int name_len = strlen(func_name);
    if (name_len + 1 + (unsigned int)arg_count * 4 > (unsigned int)size) {
        fio_free(buf); send_err(ctx, session, transaction); return;
    }
    char *arg_ptr = func_name + name_len + 1;
    for (int i = 0; i < arg_count; i++)
        args[i] = *(uint32_t *)(arg_ptr + i * 4);

    int result = 0;
    switch (arg_count) {
        case 0: result = call(func_name); break;
        case 1: result = call(func_name, args[0]); break;
        case 2: result = call(func_name, args[0], args[1]); break;
        case 3: result = call(func_name, args[0], args[1], args[2]); break;
        case 4: result = call(func_name, args[0], args[1], args[2], args[3]); break;
        case 5: result = call(func_name, args[0], args[1], args[2], args[3], args[4]); break;
        case 6: result = call(func_name, args[0], args[1], args[2], args[3], args[4], args[5]); break;
    }

    fio_free(buf);
    send_ok(ctx, session, transaction, (uint32_t)result, 0);
}

/* dumpf() always writes log0000.log (see src/debug.c). Remove any leftover
 * copy first, so the file read back afterwards is guaranteed to come from
 * this call rather than from an earlier session. The log is left on the card
 * (it gets overwritten by the next capture) - deleting it here used to destroy
 * the only debug output the user had. */
#define CONSOLE_LOG_PATH "log0000.log"

static void handle_ConsoleCapture(struct ptp_context *ctx, uint32_t session, uint32_t transaction)
{
    FIO_RemoveFile(CONSOLE_LOG_PATH);
    int dump_ret = call("dumpf");
    if (dump_ret < 0) { send_err(ctx, session, transaction); return; }
    msleep(500);

    const char *path = CONSOLE_LOG_PATH;
    uint32_t sz = 0;
    if (FIO_GetFileSize((char *)path, &sz) != 0 || sz == 0) {
        send_err(ctx, session, transaction); return;
    }
    if (sz > 128 * 1024) sz = 128 * 1024;

    FILE *f = FIO_OpenFile((char *)path, 0);
    if (!f) { send_err(ctx, session, transaction); return; }

    char *data = fio_malloc(sz);
    if (!data) { FIO_CloseFile(f); send_err(ctx, session, transaction); return; }
    int nread = FIO_ReadFile(f, data, sz);
    FIO_CloseFile(f);

    if (nread <= 0) { fio_free(data); send_err(ctx, session, transaction); return; }
    send_data(ctx, data, nread);

    struct ptp_msg m = { .id = PTP_RC_OK, .session = session, .transaction = transaction,
        .param_count = 1, .param = { (uint32_t)nread, 0, 0, 0, 0 } };
    ctx->send_resp(ctx->handle, &m);
    fio_free(data);
}

static void handle_Screenshot(struct ptp_context *ctx, uint32_t session, uint32_t transaction)
{
    char path[64];
    snprintf(path, sizeof(path), "ML/SETTINGS/PTP_SCRN.BMP");
    FIO_RemoveFile(path);

    int r = take_screenshot(path, 1);
    if (!r) { send_err(ctx, session, transaction); return; }
    msleep(100);

    uint32_t sz = 0;
    if (FIO_GetFileSize(path, &sz) != 0 || sz == 0) { send_err(ctx, session, transaction); return; }

    FILE *f = FIO_OpenFile(path, 0);
    if (!f) { send_err(ctx, session, transaction); return; }
    char *data = fio_malloc(sz);
    if (!data) { FIO_CloseFile(f); send_err(ctx, session, transaction); return; }
    int nread = FIO_ReadFile(f, data, sz);
    FIO_CloseFile(f);
    FIO_RemoveFile(path);

    if (nread <= 0) { fio_free(data); send_err(ctx, session, transaction); return; }
    send_data(ctx, data, nread);

    struct ptp_msg m = { .id = PTP_RC_OK, .session = session, .transaction = transaction,
        .param_count = 1, .param = { (uint32_t)nread, 0, 0, 0, 0 } };
    ctx->send_resp(ctx->handle, &m);
    fio_free(data);
}

static void handle_ExecuteLua(struct ptp_context *ctx, uint32_t session, uint32_t transaction)
{
    int size = ctx->get_data_size(ctx->handle);
    if (size < 1) { send_err(ctx, session, transaction); return; }

    char *script = fio_malloc(size + 1);
    if (!script) { send_err(ctx, session, transaction); return; }
    if (!recv_data(ctx, script, size)) { fio_free(script); send_err(ctx, session, transaction); return; }
    script[size] = 0;

    /* This only UPLOADS the script. Nothing auto-runs it: ML's lua module
     * loads ML/SCRIPTS/<name>.lua solely at startup, and only for files that carry
     * its AUTORUN flag. The file name is deliberately not CHDK_RUN.LUA, which
     * belongs to the CHDK PTP ExecuteScript path (src/ptp-chdk.c). */
    char path[64];
    snprintf(path, sizeof(path), "ML/SCRIPTS/TUNNEL.LUA");
    FIO_RemoveFile(path);

    FILE *f = FIO_CreateFile(path);
    if (!f) { fio_free(script); send_err(ctx, session, transaction); return; }
    int wrote = FIO_WriteFile(f, script, size);
    FIO_CloseFile(f);
    fio_free(script);

    if (wrote != size) { FIO_RemoveFile(path); send_err(ctx, session, transaction); return; }
    send_ok(ctx, session, transaction, (uint32_t)size, 0);
}

static void handle_EngioRead(struct ptp_context *ctx, uint32_t session, uint32_t transaction, uint32_t param2)
{
    /* shamem_read() only understands the ENGIO window; feeding it a RAM
     * address indexes out of range inside Canon's accessor. MEMX() routes
     * 0xCxxxxxxx to shamem_read and everything else to a plain load. */
    if (!address_is_readable(param2)) { send_err(ctx, session, transaction); return; }
    uint32_t val = MEMX(param2);
    send_ok(ctx, session, transaction, val, 0);
}

static void handle_EngioWrite(struct ptp_context *ctx, uint32_t session, uint32_t transaction, uint32_t param2, uint32_t param3)
{
    if (!engio_write_allowed(param2)) { send_err(ctx, session, transaction); return; }
    EngDrvOut(param2, param3);
    send_ok(ctx, session, transaction, 0, 0);
}

static void handle_ShamemRead(struct ptp_context *ctx, uint32_t session, uint32_t transaction, uint32_t param2)
{
    /* Kept for backwards compatibility with the original host tool, which
     * used this as a generic memory read. It never was shamem-specific. */
    if (!address_is_readable(param2)) { send_err(ctx, session, transaction); return; }
    uint32_t val = MEMX(param2);
    send_ok(ctx, session, transaction, val, 0);
}

static void handle_SetPropertyRaw(struct ptp_context *ctx, uint32_t session, uint32_t transaction, uint32_t param2)
{
    int size = ctx->get_data_size(ctx->handle);
    /* The length must match the property's real length exactly - Canon copies
     * that many bytes out of our buffer regardless of what we allocated. */
    if (size < 1 || size > 4096) { send_err(ctx, session, transaction); return; }

    char *data = fio_malloc(size);
    if (!data) { send_err(ctx, session, transaction); return; }
    if (!recv_data(ctx, data, size)) { fio_free(data); send_err(ctx, session, transaction); return; }

    /* prop_request_change() is fire-and-forget and returns void; the _wait
     * variant tells us whether the property was actually accepted. */
    int r = prop_request_change_wait(param2, data, size, 1000);
    fio_free(data);
    if (r != 0) { send_err(ctx, session, transaction); return; }
    send_ok(ctx, session, transaction, 0, 0);
}

static int ptp_tunnel_handler(void *priv, struct ptp_context *ctx,
    uint32_t opcode, uint32_t session, uint32_t transaction,
    uint32_t param1, uint32_t param2, uint32_t param3,
    uint32_t param4, uint32_t param5)
{
    (void)priv; (void)opcode; (void)param4; (void)param5;

    switch (param1) {
        case PTP_TUNNEL_CallByName:
            handle_CallByName(ctx, session, transaction, param2);
            break;
        case PTP_TUNNEL_ConsoleCapture:
            handle_ConsoleCapture(ctx, session, transaction);
            break;
        case PTP_TUNNEL_Screenshot:
            handle_Screenshot(ctx, session, transaction);
            break;
        case PTP_TUNNEL_ExecuteLua:
            handle_ExecuteLua(ctx, session, transaction);
            break;
        case PTP_TUNNEL_EngioRead:
            handle_EngioRead(ctx, session, transaction, param2);
            break;
        case PTP_TUNNEL_EngioWrite:
            handle_EngioWrite(ctx, session, transaction, param2, param3);
            break;
        case PTP_TUNNEL_ShamemRead:
            handle_ShamemRead(ctx, session, transaction, param2);
            break;
        case PTP_TUNNEL_SetPropertyRaw:
            handle_SetPropertyRaw(ctx, session, transaction, param2);
            break;
        default: {
            struct ptp_msg m = { .id = PTP_RC_ParameterNotSupported,
                .session = session, .transaction = transaction };
            ctx->send_resp(ctx->handle, &m);
        }
    }
    return 0;
}

/* --- Self-test for QEMU verification --- */

static void run_self_test(void *unused)
{
    (void)unused;
    msleep(2000);
    printf("\n========================================\n");
    printf("  PTP Tunnel Self-Test\n");
    printf("========================================\n");

    int pass = 0, fail = 0, total = 0;

    /* NOTE: the handler is registered once, in init_safe(). Re-registering the
     * same opcode here used to make this test fail unconditionally. */

    total++;
    printf("[TEST %d] MEMX(FPS_TIMER_A 0xC0F06008)... ", total);
    {
        uint32_t v = MEMX(0xC0F06008);
        if (v != 0 && v != 0xFFFFFFFF) { printf("OK (0x%08X)\n", v); pass++; }
        else { printf("FAIL (0x%08X)\n", v); fail++; }
    }

    total++;
    printf("[TEST %d] MEMX(FPS_TIMER_B 0xC0F06014)... ", total);
    {
        uint32_t v = MEMX(0xC0F06014);
        if (v != 0 && v != 0xFFFFFFFF) { printf("OK (0x%08X)\n", v); pass++; }
        else { printf("FAIL (0x%08X)\n", v); fail++; }
    }

    total++;
    printf("[TEST %d] MEMX(ENGIO 0xC0F06800)... ", total);
    {
        uint32_t v = MEMX(0xC0F06800);
        if (v != 0 && v != 0xFFFFFFFF) { printf("OK (0x%08X)\n", v); pass++; }
        else { printf("FAIL (0x%08X)\n", v); fail++; }
    }

    total++;
    printf("[TEST %d] MEMX(RAM ISO table 0x404E5664)... ", total);
    {
        uint32_t v = MEMX(0x404E5664);
        if (v != 0 && v != 0xFFFFFFFF && v != 0xAAAAAAAA) { printf("OK (0x%08X)\n", v); pass++; }
        else { printf("FAIL (0x%08X)\n", v); fail++; }
    }

    total++;
    printf("[TEST %d] address guard rejects 0x00000000... ", total);
    {
        if (!address_is_readable(0x00000000)) { printf("OK (refused)\n"); pass++; }
        else { printf("FAIL (guard let it through)\n"); fail++; }
    }

    total++;
    printf("[TEST %d] fio_malloc/free... ", total);
    {
        void *p = fio_malloc(1024);
        if (p) { fio_free(p); printf("OK\n"); pass++; }
        else { printf("FAIL\n"); fail++; }
    }

    total++;
    printf("[TEST %d] File write/read (ExecuteLua path)... ", total);
    {
        const char *test_path = "ML/SETTINGS/PTP_TEST.TMP";
        const char *test_data = "test_script_content";
        FIO_RemoveFile(test_path);
        FILE *f = FIO_CreateFile(test_path);
        if (f) {
            int w = FIO_WriteFile(f, test_data, strlen(test_data));
            FIO_CloseFile(f);
            if (w == (int)strlen(test_data)) {
                uint32_t sz = 0;
                FIO_GetFileSize(test_path, &sz);
                f = FIO_OpenFile(test_path, 0);
                if (f && sz > 0) {
                    char buf[128] = {0};
                    int r = FIO_ReadFile(f, buf, sizeof(buf)-1);
                    FIO_CloseFile(f);
                    if (r > 0 && strcmp(buf, test_data) == 0) {
                        printf("OK (%d bytes)\n", r); pass++;
                    } else { printf("FAIL (content mismatch)\n"); fail++; }
                } else { printf("FAIL (read)\n"); fail++; }
            } else { printf("FAIL (write)\n"); fail++; }
            FIO_RemoveFile(test_path);
        } else { printf("FAIL (create)\n"); fail++; }
    }

    total++;
    printf("[TEST %d] call('dumpf') console capture... ", total);
    {
        FIO_RemoveFile(CONSOLE_LOG_PATH);
        int r = call("dumpf");
        if (r >= 0) {
            uint32_t sz = 0;
            msleep(500);
            if (FIO_GetFileSize(CONSOLE_LOG_PATH, &sz) == 0 && sz > 0) {
                printf("OK (%s, %d bytes)\n", CONSOLE_LOG_PATH, sz); pass++;
            } else {
                printf("FAIL (dumpf returned %d but no %s)\n", r, CONSOLE_LOG_PATH); fail++;
            }
        } else { printf("FAIL (r=%d)\n", r); fail++; }
    }

    total++;
    printf("[TEST %d] EngDrvOut allow-list... ", total);
    {
        int pal_ok   = engio_write_allowed(0xC0F14080);
        int fps_deny = !engio_write_allowed(0xC0F06000);
        if (pal_ok && fps_deny) { printf("OK (palette allowed, FPS denied)\n"); pass++; }
        else { printf("FAIL (pal=%d fps_deny=%d)\n", pal_ok, fps_deny); fail++; }
    }

    printf("========================================\n");
    printf("  Results: %d/%d passed, %d failed\n", pass, total, fail);
    printf("========================================\n\n");
}

static void init_safe(void *unused)
{
    (void)unused;
    unsafe_writes_enabled = config_flag_file_setting_load("ML/SETTINGS/PTP_UNSAFE.WR");
    printf("[PTP] ENGIO writes beyond the palette: %s\n",
        unsafe_writes_enabled ? "ENABLED (PTP_UNSAFE.WR present)" : "blocked (drop ML/SETTINGS/PTP_UNSAFE.WR to allow)");
    printf("[PTP] Registering tunnel handler (opcode 0x%04X)\n", PTP_TUNNEL_CODE);
    unsigned long ptr = (unsigned long)ptp_tunnel_handler;
    uint32_t r = ptp_register_handler(PTP_TUNNEL_CODE, (void *)ptr, NULL);
    if (r == 0)
        printf("[PTP] Tunnel registration OK\n");
    else
        printf("[PTP] Tunnel registration FAILED: 0x%08X\n", r);

    run_self_test(NULL);
}

static unsigned int ptp_tunnel_init(void)
{
    printf("[PTP] Module loaded, creating init task\n");
    task_create("ptp_init", 0x1e, 0x2000, init_safe, 0);
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(ptp_tunnel_init)
MODULE_INFO_END()
