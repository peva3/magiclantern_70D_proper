#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <config.h>
#include <property.h>
#include <battery.h>

/*
 * wifisrv — WiFi TCP Server for Canon 70D
 *
 * Uses RAM-loaded socket functions (validated by hw_test v15 on physical 70D).
 * Canon firmware initializes the networking stack at boot; socket APIs are
 * resident at fixed RAM addresses (0x0005xxxx).
 *
 * Camera listens on port 5555 (default), accepts connections, and responds to
 * single-byte commands. Companion: camremote.py
 */

#define MODNAME "wifisrv"
#define DEFAULT_PORT 5555
#define MAX_PAYLOAD 1024
#define BACKLOG 1

/* ── RAM-loaded socket function addresses (70D specific, validated) ── */
#define SOCKET_CREATE_ADDR   0x00059AF8
#define SOCKET_BIND_ADDR     0x00059E94
#define SOCKET_LISTEN_ADDR   0x0005A9D0
#define SOCKET_RECV_ADDR     0x00059CE8
#define SOCKET_SEND_ADDR     0x0005A09C
#define SOCKET_SETSOCKOPT    0x0005A810

static int (*p_socket_create)(int domain, int type, int protocol);
static int (*p_socket_bind)(int fd, void *addr, int addrlen);
static int (*p_socket_listen)(int fd, int backlog);
static int (*p_socket_recv)(int fd, void *buf, int len, int flags);
static int (*p_socket_send)(int fd, const void *buf, int len, int flags);
static void (*p_socket_setsockopt)(int fd, int level, int optname, const void *optval, int optlen);

extern int ptpip_sock_accept(int server_fd);
extern int socket_close_if_valid(int fd);   /* ROM1 0xFF7AF380: close(fd) unless fd == -1 */
extern int socket_close_caller(int converted_socket);
extern int remote_shot_flag;                /* src/shoot.c:4935 declares this as int */
extern void schedule_remote_shot(void);
extern int lv;                              /* src/propvalues.h:38 */

/* ── IPv4 struct ── */
struct sockaddr_in {
    int16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    char sin_zero[8];
};

#define SOCK_FAMILY_IPv4 0x100
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SO_REUSEADDR 2

/* ── Helpers ── */

static unsigned short htons_ml(unsigned short port)
{
    return ((port & 0xFF) << 8) | ((port >> 8) & 0xFF);
}

/* Accept the common ARM prologues. The old test only matched
 * `stmdb sp!, {...}` with condition AL implicit in a 0x0FFF mask, which
 * (a) accepted conditional pushes that never execute and (b) rejected perfect
 * leaf functions that start with `sub sp, sp, #n` or `str lr, [sp, #-4]!`. */
static int is_valid_code(uint32_t addr)
{
    if (addr < 0x1000 || addr >= 0xFFFF0000) return 0;
    volatile uint32_t *p = (uint32_t *)(uintptr_t)addr;
    uint32_t word = *p;
    if ((word & 0xFFFF0000) == 0xE92D0000) return 1;   /* stmdb sp!, {...}        */
    if ((word & 0xFFFFF000) == 0xE24DD000) return 1;   /* sub sp, sp, #imm        */
    if ((word & 0xFFFFF000) == 0xE52DE000) return 1;   /* str lr, [sp, #-4]!      */
    return 0;
}

/* ── Read config ── */

static int read_config(int *port)
{
    char filename[FIO_MAX_PATH_LENGTH];
    snprintf(filename, sizeof(filename), "%sWIFISRV.CFG", get_config_dir());
    *port = DEFAULT_PORT;
    FILE *f = FIO_OpenFile(filename, O_RDONLY | O_SYNC);
    if (!f) return 0;
    char buf[64];
    memset(buf, 0, sizeof(buf));
    int n = FIO_ReadFile(f, buf, sizeof(buf) - 1);
    FIO_CloseFile(f);
    if (n <= 0) return 0;
    int val = 0;
    for (char *p = buf; *p; p++) {
        if (*p >= '0' && *p <= '9') val = val * 10 + (*p - '0');
        else break;
    }
    if (val > 0 && val <= 65535) *port = val;
    return 1;
}

/* ── Initialize function pointers ── */

/* Only install a pointer once its target has been validated, otherwise a
 * rejected address stays in the pointer and gets called anyway. */
static int init_socket_ptrs(void)
{
    p_socket_create = is_valid_code(SOCKET_CREATE_ADDR) ? (void*)SOCKET_CREATE_ADDR : 0;
    p_socket_bind   = is_valid_code(SOCKET_BIND_ADDR)   ? (void*)SOCKET_BIND_ADDR   : 0;
    p_socket_listen = is_valid_code(SOCKET_LISTEN_ADDR) ? (void*)SOCKET_LISTEN_ADDR : 0;
    p_socket_recv   = is_valid_code(SOCKET_RECV_ADDR)   ? (void*)SOCKET_RECV_ADDR   : 0;
    p_socket_send   = is_valid_code(SOCKET_SEND_ADDR)   ? (void*)SOCKET_SEND_ADDR   : 0;
    /* setsockopt is optional: the server works without it */
    p_socket_setsockopt = is_valid_code(SOCKET_SETSOCKOPT) ? (void*)SOCKET_SETSOCKOPT : 0;
    return p_socket_create && p_socket_bind && p_socket_listen &&
           p_socket_recv && p_socket_send;
}

/* ── Send response: 2-byte length prefix + payload ── */

/* TCP send() may accept fewer bytes than asked; the old code treated that as
 * fatal and left the peer's length-prefixed reader permanently out of sync. */
static int send_all(int fd, const void *buf, int len)
{
    const char *p = buf;
    while (len > 0) {
        int n = p_socket_send(fd, p, len, 0);
        if (n <= 0) return -1;
        p += n;
        len -= n;
    }
    return 0;
}

static int send_resp(int fd, const char *data, int len)
{
    uint16_t net_len = htons_ml((uint16_t)len);
    if (send_all(fd, &net_len, 2) < 0) return -1;
    if (len > 0 && send_all(fd, data, len) < 0) return -1;
    return 0;
}

/* ── Handle one client (read-eval loop) ── */

static int srv_stop;
static int srv_sock = -1;

static int do_command(int client_fd, char cmd)
{
    char resp[2048];
    int rlen;

    switch (cmd) {
    case 'P':
        rlen = snprintf(resp, sizeof(resp), "PONG");
        return send_resp(client_fd, resp, rlen);

    case 'S':
        rlen = snprintf(resp, sizeof(resp),
            "STAT b=%d s=%d t=%d lv=%d rmt=%d cam=70D fw=1.1.2",
            GetBatteryLevel(), shutter_count, efic_temp, lv, remote_shot_flag);
        return send_resp(client_fd, resp, rlen);

    case 'R': {
        /* FA_RemoteRelease is not guaranteed to exist; schedule_remote_shot()
         * drives ML's own verified remote-release path and always works. */
        call("FA_RemoteRelease");
        schedule_remote_shot();
        rlen = snprintf(resp, sizeof(resp), "REMOTE_OK");
        return send_resp(client_fd, resp, rlen);
    }

    case 'L':
        /* `lv` is ML's own LiveView flag. The old code read 0x7CFEC, which is
         * the EVF *state-object pointer* slot - non-NULL from early boot - so
         * it always reported LV on and could only ever stop LiveView. */
        if (lv)
            call("FA_StopLiveView");
        else
            call("FA_StartLiveView");
        rlen = snprintf(resp, sizeof(resp), "LV_TOGGLE");
        return send_resp(client_fd, resp, rlen);

    case 'B':
        rlen = snprintf(resp, sizeof(resp), "ERR_UNSAFE_CMD");
        return send_resp(client_fd, resp, rlen);

    case 'Q':
        return -2;   /* caller closes */

    default:
        rlen = snprintf(resp, sizeof(resp), "ERR_UNKNOWN_CMD");
        return send_resp(client_fd, resp, rlen);
    }
}

static void handle_client(int client_fd)
{
    for (;;) {
        uint8_t buf[512];
        int n = p_socket_recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;

        /* Process every received byte, not just buf[0]: a client that
         * pipelines "PS" in one segment used to lose the 'S'. */
        for (int i = 0; i < n; i++) {
            if (srv_stop) return;
            int r = do_command(client_fd, (char)buf[i]);
            if (r == -2) return;     /* 'Q' */
            if (r < 0) return;       /* peer gone */
        }
    }
}

/* ── Server task ── */

static void server_task(void *unused)
{
    (void)unused;

    if (!init_socket_ptrs()) {
        printf("[%s] Socket function pointers not all valid, aborting\n", MODNAME);
        bmp_printf(FONT_MED, 0, 30, "[%s] socket API not available", MODNAME);
        return;
    }
    printf("[%s] Socket ptrs validated (setsockopt %s)\n",
        MODNAME, p_socket_setsockopt ? "ok" : "unavailable");

    int port = DEFAULT_PORT;
    read_config(&port);
    printf("[%s] Starting server on port %d\n", MODNAME, port);

    int sock = p_socket_create(1, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("[%s] socket_create failed: %d\n", MODNAME, sock);
        return;
    }
    printf("[%s] socket=%d\n", MODNAME, sock);

    int reuse = 1;
    if (p_socket_setsockopt)
        p_socket_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = SOCK_FAMILY_IPv4;
    sa.sin_port = htons_ml((uint16_t)port);
    sa.sin_addr = 0;

    int ret = p_socket_bind(sock, &sa, sizeof(sa));
    if (ret < 0) {
        printf("[%s] bind failed: %d\n", MODNAME, ret);
        socket_close_if_valid(sock);
        return;
    }
    printf("[%s] bind OK\n", MODNAME);

    ret = p_socket_listen(sock, BACKLOG);
    if (ret < 0) {
        printf("[%s] listen failed: %d\n", MODNAME, ret);
        socket_close_if_valid(sock);
        return;
    }
    printf("[%s] listen OK, waiting for connections...\n", MODNAME);
    bmp_printf(FONT_MED, 0, 30, "[%s] Listening on :%d", MODNAME, port);

    srv_sock = sock;

    TASK_LOOP {
        if (srv_stop) break;

        int client = ptpip_sock_accept(sock);
        if (client < 0) {
            if (srv_stop) break;
            printf("[%s] accept failed: %d\n", MODNAME, client);
            msleep(1000);
            continue;
        }

        printf("[%s] Client connected\n", MODNAME);
        bmp_printf(FONT_MED, 0, 50, "[%s] Client connected", MODNAME);

        handle_client(client);
        socket_close_if_valid(client);
        printf("[%s] Client disconnected\n", MODNAME);
        bmp_printf(FONT_MED, 0, 50, "[%s] Waiting for client...", MODNAME);

        msleep(100);
    }

    /* Reached only when srv_stop was set from deinit. Release the listening
     * socket so the port is not left bound after the module is unloaded. */
    socket_close_if_valid(sock);
    srv_sock = -1;
    printf("[%s] Server stopped\n", MODNAME);
}

/* ── Module lifecycle ── */

static unsigned int wifisrv_init(void)
{
    printf("[%s] Module loading\n", MODNAME);
    srv_stop = 0;
    task_create(MODNAME, 0x1d, 0x2000, server_task, 0);
    return 0;
}

static unsigned int wifisrv_deinit(void)
{
    printf("[%s] Module unloading\n", MODNAME);
    /* Ask the task to leave its loop and unblock accept() by closing the
     * listening socket. ML cannot safely kill a task that may be inside
     * Canon code (src/module.c warns about exactly this), so the flag plus
     * the closed socket is the least-bad shutdown. */
    srv_stop = 1;
    if (srv_sock >= 0) {
        int s = srv_sock;
        srv_sock = -1;
        socket_close_if_valid(s);
    }
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(wifisrv_init)
    MODULE_DEINIT(wifisrv_deinit)
MODULE_INFO_END()
