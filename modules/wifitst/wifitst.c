/* WiFi / socket diagnostics for Canon 70D (DIGIC V)
 *
 * Renamed from "wifi_test": module names must be <= 8 characters because the
 * card is FAT-formatted, and 8.3 truncation breaks TCC symbol lookup (exactly
 * why ptp_tunnel had to become ptptun).
 *
 * This module is READ-ONLY.
 *
 *   - verifies the addresses of the RAM-loaded socket library (0x0005xxxx)
 *     and of the ROM1 PTPIP wrappers (0xFF7Axxxx) by inspecting their first
 *     instruction word; nothing is executed during that pass;
 *   - optionally (from the menu) opens one listening socket to prove the API
 *     is reachable, then closes it again.
 *
 * It deliberately does NOT call any WiFi bootstrap eventproc. The project's
 * own hardware notes (AGENTS.md, Sprint 42) record that this family of names
 * -- NwLimeInit, NwLimeOn, wlanpoweron, wlanup, wlanchk, wlanipset, nif_up,
 * nif_start, dhcpc_setup, dnsc_setup, ipset, LughConnect -- could not be
 * resolved on the 70D, and platform/70D.112/stubs.S states outright that
 * NwLimeInit/NwLimeOn were NOT found in ROM1. call() on a missing name just
 * returns -1, and bringing up SIO/WiFi blind has frozen this body before.
 *
 * The names are still listed so the log records what was considered -- but
 * they are reported, never invoked.
 */

#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <menu.h>
#include <ml_socket.h>

static void wifitst_task(void *unused);

static unsigned int wifitst_init(void);

MODULE_INFO_START()
MODULE_INIT(wifitst_init)
MODULE_INFO_END()

static struct menu_entry wifitst_menu[] =
{
    {
        .name = "WiFi / socket diagnostics",
        .select = run_in_separate_task,
        .priv = wifitst_task,
        .help = "Verify the RAM-loaded socket library and the ROM1 PTPIP wrappers.",
        .help2 = "Read-only address checks, then one listening socket to prove the API works.",
    },
};

#ifndef SOCK_STREAM
#define SOCK_STREAM 1
#endif

/* 70D RAM-loaded socket function addresses (resolved from firmware module
 * space at boot; cross-checked against the eventproc/symbol dumps). */
#define SOCKET_CREATE_ADDR   0x00059AF8
#define SOCKET_BIND_ADDR     0x00059E94
#define SOCKET_CONNECT_ADDR  0x00059DDC
#define SOCKET_LISTEN_ADDR   0x0005A9D0
#define SOCKET_SETSOCKOPT    0x0005A810
#define SOCKET_RECV_ADDR     0x00059CE8
#define SOCKET_SEND_ADDR     0x0005A09C

/* ROM1 PTPIP wrappers, verified for existence only. */
static const uint32_t ptpip_addrs[] =
{
    0xFF7AF220,   /* ptpip_sock_create */
    0xFF7AEE80,   /* ptpip_open_server */
    0xFF7AF2CC,   /* ptpip_create_client */
    0xFF7AF38C,   /* ptpip_set_keepalive */
    0xFF7AEFCC,   /* ptpip_listen_close */
    0xFF7AF344,   /* ptpip_close_server */
    0xFF7AF160,   /* ptpip_sock_accept */
    0xFF7AF380,   /* socket_close_if_valid */
};

static const char *const ptpip_names[] =
{
    "ptpip_sock_create", "ptpip_open_server", "ptpip_create_client",
    "ptpip_set_keepalive", "ptpip_listen_close", "ptpip_close_server",
    "ptpip_sock_accept", "socket_close_if_valid",
};

/* Names that the port has historically tried to invoke. Kept only so the log
 * shows what was considered; none of them is callable via call() on the 70D. */
static const char *const absent_names[] =
{
    "NwLimeInit", "NwLimeOn", "wlanpoweron", "wlanup", "wlanchk",
    "wlanipset", "nif_up", "nif_start", "dhcpc_setup", "dnsc_setup", "ipset",
};

typedef int (*socket_create_fn)(int domain, int type, int protocol);
typedef int (*socket_bind_fn)(int fd, struct sockaddr_in *addr, int addrlen);
typedef int (*socket_connect_fn)(int fd, struct sockaddr_in *addr, int addrlen);
typedef int (*socket_listen_fn)(int fd, int backlog);
typedef void (*socket_setsockopt_fn)(int fd, int level, int optname, const void *optval, int optlen);
typedef int (*socket_recv_fn)(int fd, void *buf, int len, int flags);
typedef int (*socket_send_fn)(int fd, const void *buf, int len, int flags);

static socket_create_fn     p_socket_create     = NULL;
static socket_bind_fn       p_socket_bind       = NULL;
static socket_connect_fn    p_socket_connect    = NULL;
static socket_listen_fn     p_socket_listen     = NULL;
static socket_setsockopt_fn p_socket_setsockopt = NULL;
static socket_recv_fn       p_socket_recv       = NULL;
static socket_send_fn       p_socket_send       = NULL;

extern int socket_close_if_valid(int socket);

static unsigned short htons_ml(unsigned short port)
{
    return ((port & 0xFF) << 8) | ((port >> 8) & 0xFF);
}

static uint32_t htonl_ml(uint32_t host)
{
    return ((host & 0x000000FFu) << 24)
         | ((host & 0x0000FF00u) << 8)
         | ((host & 0x00FF0000u) >> 8)
         | ((host & 0xFF000000u) >> 24);
}

/* A function prologue on ARMv5 is one of:
 *   stmdb sp!, {...}      E92Dxxxx
 *   sub   sp, sp, #imm    E24DDxxx
 *   str   lr, [sp, #-4]!  E52DE004
 * Anything else is "unknown", not necessarily invalid, but for these addresses
 * an unexpected first word is worth reporting.
 * Note the condition field must be part of the mask: 0x092D0000 is
 * "stmdbeq sp!" which is never executed in practice. */
static int looks_like_code(uint32_t addr)
{
    if (addr < 0x1000 || addr >= 0xFFFF0000) return 0;
    uint32_t word = *(volatile uint32_t *)(uintptr_t)addr;
    if ((word & 0xFFFF0000) == 0xE92D0000) return 1;   /* stmdb sp! */
    if ((word & 0xFFFFF000) == 0xE24DD000) return 1;   /* sub sp, sp, #imm */
    if ((word & 0xFFFFF000) == 0xE52DE000) return 1;   /* str lr, [sp, #-4]! */
    return 0;
}

static void *ptr_if_valid(uint32_t addr, const char *name)
{
    uint32_t word = *(volatile uint32_t *)(uintptr_t)addr;
    if (looks_like_code(addr))
    {
        printf("[WiFi] %-18s @ 0x%08X  ok  (0x%08X)\n", name, addr, word);
        return (void *)(uintptr_t)addr;
    }
    printf("[WiFi] %-18s @ 0x%08X  UNEXPECTED first word 0x%08X\n", name, addr, word);
    return NULL;
}

static int init_socket_ptrs(void)
{
    int ok = 0;
    p_socket_create     = (socket_create_fn)    ptr_if_valid(SOCKET_CREATE_ADDR,  "socket_create");
    p_socket_bind       = (socket_bind_fn)      ptr_if_valid(SOCKET_BIND_ADDR,    "socket_bind");
    p_socket_connect    = (socket_connect_fn)   ptr_if_valid(SOCKET_CONNECT_ADDR, "socket_connect");
    p_socket_listen     = (socket_listen_fn)    ptr_if_valid(SOCKET_LISTEN_ADDR,  "socket_listen");
    p_socket_setsockopt = (socket_setsockopt_fn)ptr_if_valid(SOCKET_SETSOCKOPT,   "socket_setsockopt");
    p_socket_recv       = (socket_recv_fn)      ptr_if_valid(SOCKET_RECV_ADDR,    "socket_recv");
    p_socket_send       = (socket_send_fn)      ptr_if_valid(SOCKET_SEND_ADDR,    "socket_send");

    if (p_socket_create)     ok++;
    if (p_socket_bind)       ok++;
    if (p_socket_connect)    ok++;
    if (p_socket_listen)     ok++;
    if (p_socket_setsockopt) ok++;
    if (p_socket_recv)       ok++;
    if (p_socket_send)       ok++;
    return ok;
}

static void show_status(const char *msg, int ok)
{
    bmp_printf(FONT_MED, 50, 50, "WiFi: %s [%s]", msg, ok ? "OK" : "FAIL");
}

/* --- Section 1: ROM1 PTPIP wrappers (existence only, nothing called) --- */

static void verify_ptpip(void)
{
    int valid = 0;
    printf("\n=== PTPIP / socket-close ROM1 targets (read-only) ===\n");
    for (unsigned i = 0; i < COUNT(ptpip_addrs); i++)
    {
        uint32_t word = *(volatile uint32_t *)(uintptr_t)ptpip_addrs[i];
        int ok = looks_like_code(ptpip_addrs[i]);
        if (ok) valid++;
        printf("[PTPIP] %-22s @ 0x%08X  %s  (0x%08X)\n",
               ptpip_names[i], ptpip_addrs[i], ok ? "code" : "NOT code", word);
    }
    printf("[PTPIP] %d/%d targets look like ARM code\n", valid, (int)COUNT(ptpip_addrs));
}

/* --- Section 2: live socket API probe (one listening socket) --- */

static void probe_socket_api(void)
{
    printf("\n=== Socket API probe (RAM-loaded 0x0005xxxx) ===\n");

    if (!p_socket_create || !p_socket_bind || !p_socket_listen)
    {
        printf("[WiFi] create/bind/listen unavailable - skipping live probe\n");
        return;
    }

    int sock = p_socket_create(SOCK_FAMILY_IPv4, SOCK_STREAM, 0);
    printf("[WiFi] socket_create = %d\n", sock);
    if (sock < 0) return;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = SOCK_FAMILY_IPv4;   /* 0x100, network order 1 */
    addr.sin_port   = htons_ml(5555);
    addr.sin_addr   = 0;                  /* INADDR_ANY */

    int r = p_socket_bind(sock, &addr, sizeof(addr));
    printf("[WiFi] socket_bind   = %d\n", r);

    r = p_socket_listen(sock, 1);
    printf("[WiFi] socket_listen = %d\n", r);

    if (p_socket_setsockopt)
    {
        int reuse = 1;
        p_socket_setsockopt(sock, 0xFFFF /* SOL_SOCKET */, 0x0004 /* SO_REUSEADDR */,
                            &reuse, sizeof(reuse));
        printf("[WiFi] socket_setsockopt(SO_REUSEADDR) done\n");
    }

    /* A connect() on a socket that is already listening to its own port is
     * meaningless, so use a second socket and expect a failure (nothing is
     * accepting on loopback unless Canon's own server is up). The point is to
     * exercise the entry point, not to succeed. */
    if (p_socket_connect)
    {
        int cli = p_socket_create(SOCK_FAMILY_IPv4, SOCK_STREAM, 0);
        if (cli >= 0)
        {
            struct sockaddr_in peer;
            memset(&peer, 0, sizeof(peer));
            peer.sin_family = SOCK_FAMILY_IPv4;
            peer.sin_port   = htons_ml(5555);
            peer.sin_addr   = htonl_ml(0x7F000001);   /* 127.0.0.1 */
            r = p_socket_connect(cli, &peer, sizeof(peer));
            printf("[WiFi] socket_connect(127.0.0.1:5555) = %d (failure expected)\n", r);
            socket_close_if_valid(cli);
        }
    }

    /* socket_close_caller() expects a descriptor already run through
     * socket_convertfd(); that NSTUB does not exist for the 70D, so use the
     * ROM1 helper that takes the raw fd and validates it. */
    int cr = socket_close_if_valid(sock);
    printf("[WiFi] socket_close_if_valid(%d) = %d\n", sock, cr);
}

static void wifitst_task(void *unused)
{
    (void)unused;
    printf("\n========================================\n");
    printf("  WiFi / socket diagnostics - Canon 70D\n");
    printf("========================================\n");
    printf("[WiFi] Read-only: no WiFi bootstrap eventproc is invoked.\n");

    show_status("Probing", 0);

    printf("\n=== Address verification ===\n");
    int verified = init_socket_ptrs();
    printf("[WiFi] %d/7 socket function addresses look like ARM code\n", verified);

    verify_ptpip();
    probe_socket_api();

    printf("\n=== Names NOT callable on this body ===\n");
    for (unsigned i = 0; i < COUNT(absent_names); i++)
    {
        printf("[WiFi]   %-12s not resolved on 70D (see stubs.S) - not called\n",
               absent_names[i]);
    }

    printf("\n=== Summary ===\n");
    printf("[WiFi] Socket ptrs:      %d/7 verified\n", verified);
    printf("[WiFi] PTPIP targets:    verified above (existence only)\n");
    printf("[WiFi] WiFi bring-up:    NOT attempted (see comment in wifitst.c)\n");

    show_status("Complete", verified == 7);
    printf("========================================\n");
}

static unsigned int wifitst_init(void)
{
    menu_add("Debug", wifitst_menu, COUNT(wifitst_menu));
    bmp_printf(FONT_SMALL, 20, 400, "WiFi diagnostics: Debug menu > WiFi / socket diagnostics");
    return 0;
}
