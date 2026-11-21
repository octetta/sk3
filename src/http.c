#include "http.h"
#include "skred.h"
#include "skode.h"
#include "api.h"
#include "portable_atomic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#if defined(__EMSCRIPTEN__) || defined(SK_NO_NETWORK)

/* Stub implementations for WASM / non-networking builds so it never breaks */
int skred_http_start(int port) { (void)port; return -1; }
void skred_http_stop(void) {}
int skred_http_info(void) { return 0; }
int skred_http_poll(int timeout_ms) { (void)timeout_ms; return 0; }
int skred_http_metrics(skred_http_metrics_t *out) {
    if (out) memset(out, 0, sizeof(*out));
    return 0;
}
int skred_tcp_status(char *out, int max_len) {
    if (out && max_len > 0) out[0] = '\0';
    return 0;
}

#else

#ifndef _WIN32
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define CLOSE_SOCKET close
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include "portable_win.h"
typedef int socklen_t;
#define CLOSE_SOCKET closesocket
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#endif

#define SK_HTTP_RX_BUF_SIZE (32768)
#define SK_HTTP_TX_BUF_SIZE (65536)
#define SK_HTTP_IDLE_TIMEOUT_SEC (60)
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

typedef enum {
    SK_HTTP_SUBSTATE_READING_HEADERS = 0,
    SK_HTTP_SUBSTATE_READING_BODY,
    SK_HTTP_SUBSTATE_SENDING_RESPONSE
} sk_http_substate_t;

typedef struct {
    SOCKET sock;
    struct sockaddr_in addr;
    skode_t ctx;                  /* Dedicated per-client Forth context */
    int in_use;

    sk_proto_mode_t proto;        /* DETECT, RAW_TCP, HTTP, or WEBSOCKET */
    sk_http_substate_t http_state;

    char rx_buf[SK_HTTP_RX_BUF_SIZE];
    int rx_len;

    char tx_buf[SK_HTTP_TX_BUF_SIZE];
    int tx_len;
    int tx_sent;

    int content_length;
    int body_offset;
    int keep_alive;
    char path[128];
} sk_http_client_t;

static SOCKET server_sock = INVALID_SOCKET;
static int http_port = 0;
static volatile int http_running = 0;
static sk_http_client_t clients[SK_HTTP_MAX_CLIENTS];

#ifndef _WIN32
static pthread_t http_thread;
#else
static HANDLE http_thread = NULL;
#endif

static atomic_int_t http_thread_active;
static atomic_uint64_t metric_requests;
static atomic_uint64_t metric_commands;
static atomic_uint64_t metric_errors;

/* -------------------------------------------------------------------------
 * Pure C SHA-1 implementation (RFC 3174) for WebSocket Handshake
 * ------------------------------------------------------------------------- */
typedef struct {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
} sk_sha1_ctx_t;

static inline uint32_t sha1_rol(uint32_t value, uint32_t bits) {
    return (value << bits) | (value >> (32 - bits));
}

static void sha1_transform(uint32_t state[5], const uint8_t *buffer) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)buffer[i * 4] << 24) |
               ((uint32_t)buffer[i * 4 + 1] << 16) |
               ((uint32_t)buffer[i * 4 + 2] << 8) |
               ((uint32_t)buffer[i * 4 + 3]);
    }
    for (int i = 16; i < 80; i++) {
        w[i] = sha1_rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t temp = sha1_rol(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = sha1_rol(b, 30);
        b = a;
        a = temp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

static void sha1_init(sk_sha1_ctx_t *ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count = 0;
}

static void sha1_update(sk_sha1_ctx_t *ctx, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ctx->buffer[(ctx->count >> 3) & 63] = data[i];
        ctx->count += 8;
        if ((ctx->count & (63 << 3)) == 0) {
            sha1_transform(ctx->state, ctx->buffer);
        }
    }
}

static void sha1_final(sk_sha1_ctx_t *ctx, uint8_t digest[20]) {
    uint8_t finalcount[8];
    for (int i = 0; i < 8; i++) {
        finalcount[i] = (uint8_t)((ctx->count >> ((7 - i) * 8)) & 0xFF);
    }
    uint8_t c = 0x80;
    sha1_update(ctx, &c, 1);
    uint8_t zero = 0;
    while ((ctx->count & (63 << 3)) != (56 << 3)) {
        sha1_update(ctx, &zero, 1);
    }
    sha1_update(ctx, finalcount, 8);
    for (int i = 0; i < 20; i++) {
        digest[i] = (uint8_t)((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 0xFF);
    }
}

/* -------------------------------------------------------------------------
 * Base64 Encoding
 * ------------------------------------------------------------------------- */
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode(const uint8_t *in, int in_len, char *out, int out_max) {
    int out_len = 4 * ((in_len + 2) / 3);
    if (out_max < out_len + 1) return -1;
    int o = 0;
    for (int i = 0; i < in_len; i += 3) {
        int rem = in_len - i;
        uint32_t octet_a = in[i];
        uint32_t octet_b = rem > 1 ? in[i + 1] : 0;
        uint32_t octet_c = rem > 2 ? in[i + 2] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out[o++] = b64_table[(triple >> 18) & 0x3F];
        out[o++] = b64_table[(triple >> 12) & 0x3F];
        out[o++] = (rem > 1) ? b64_table[(triple >> 6) & 0x3F] : '=';
        out[o++] = (rem > 2) ? b64_table[triple & 0x3F] : '=';
    }
    out[o] = '\0';
    return o;
}

/* Set socket to non-blocking */
static int set_nonblocking(SOCKET sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

/* Close a single client connection and clean up its dedicated skode context */
static void client_close(sk_http_client_t *c) {
    if (!c || !c->in_use) return;
    skode_free(&c->ctx);
    if (c->sock != INVALID_SOCKET) {
        CLOSE_SOCKET(c->sock);
        c->sock = INVALID_SOCKET;
    }
    c->in_use = 0;
    c->proto = SK_PROTO_CLOSED;
}

/* Find case-insensitive header in raw HTTP header string */
static const char *find_header(const char *headers, const char *name) {
    if (!headers || !name) return NULL;
    size_t name_len = strlen(name);
    const char *p = headers;
    while (*p) {
        if (strncasecmp(p, name, name_len) == 0 && p[name_len] == ':') {
            const char *val = p + name_len + 1;
            while (*val == ' ' || *val == '\t') val++;
            return val;
        }
        p = strstr(p, "\r\n");
        if (!p) break;
        p += 2;
    }
    return NULL;
}

/* Case-insensitive substring match */
static int str_contains_case(const char *haystack, const char *needle) {
    if (!haystack || !needle) return 0;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncasecmp(haystack, needle, nlen) == 0) return 1;
        haystack++;
    }
    return 0;
}

/* Matches endpoint command names accommodating leading '/', '-', or '.' */
static inline int path_match(const char *path, const char *name) {
    if (!path || !name) return 0;
    while (*path == '/') path++;
    if (*path == '-' || *path == '.') path++;
    return strcmp(path, name) == 0;
}

/* Forward declarations */
static void client_read_websocket(sk_http_client_t *c);

/* -------------------------------------------------------------------------
 * Output Buffering / Sending Helper
 * ------------------------------------------------------------------------- */
static void queue_outgoing_data(sk_http_client_t *c, const void *data, int len) {
    if (!c || c->sock == INVALID_SOCKET || len <= 0) return;

    /* Compact tx_buf if unsent data was shifted */
    if (c->tx_sent > 0) {
        int unsent = c->tx_len - c->tx_sent;
        if (unsent > 0) {
            memmove(c->tx_buf, c->tx_buf + c->tx_sent, unsent);
        }
        c->tx_len = unsent;
        c->tx_sent = 0;
    }

    /* If tx buffer is completely empty, try immediate non-blocking send */
    if (c->tx_len == 0) {
        int n = send(c->sock, (const char *)data, len, 0);
        if (n == len) {
            /* All sent with zero latency */
            return;
        }
        if (n > 0) {
            int rem = len - n;
            if (rem <= (int)sizeof(c->tx_buf)) {
                memcpy(c->tx_buf, (const char *)data + n, rem);
                c->tx_len = rem;
                c->tx_sent = 0;
            }
            return;
        }
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            client_close(c);
            return;
        }
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            client_close(c);
            return;
        }
#endif
    }

    /* Buffer remaining data in tx_buf */
    if (c->tx_len + len <= (int)sizeof(c->tx_buf)) {
        memcpy(c->tx_buf + c->tx_len, data, len);
        c->tx_len += len;
    } else {
        atomic_fetch_add_uint64(&metric_errors, 1);
    }
}

/* -------------------------------------------------------------------------
 * Raw TCP Mode: direct ASCII line-based streaming (Netcat, socat, Tcl)
 * ------------------------------------------------------------------------- */
static void raw_tcp_send(sk_http_client_t *c, const char *msg, int len) {
    queue_outgoing_data(c, msg, len);
}

static void client_read_raw_tcp(sk_http_client_t *c) {
    char *newline;
    while (c->in_use && (newline = strchr(c->rx_buf, '\n')) != NULL) {
        *newline = '\0';
        int line_len = (int)(newline - c->rx_buf);
        if (line_len > 0 && c->rx_buf[line_len - 1] == '\r') {
            c->rx_buf[line_len - 1] = '\0';
        }

        char *line = c->rx_buf;
        while (*line == ' ' || *line == '\t') line++;

        if (*line) {
            atomic_fetch_add_uint64(&metric_requests, 1);
            atomic_fetch_add_uint64(&metric_commands, 1);

            if (path_match(line, "reset") || strcmp(line, "reset") == 0) {
                skode_free(&c->ctx);
                skode_init(&c->ctx);
                c->ctx.log_enable = 1;
                /* Silent reset */
            } else if (path_match(line, "version") || strcmp(line, "version") == 0) {
                char vmsg[64];
                snprintf(vmsg, sizeof(vmsg), "%s\n", skred_version());
                raw_tcp_send(c, vmsg, (int)strlen(vmsg));
            } else if (path_match(line, "status") || strcmp(line, "status") == 0) {
                char stat_msg[512];
                skred_http_metrics_t m;
                skred_http_metrics(&m);
                snprintf(stat_msg, sizeof(stat_msg),
                    "{\"status\":\"ok\",\"version\":\"%s\",\"port\":%d,\"active_connections\":%d,\"raw_clients\":%d,\"http_clients\":%d,\"ws_clients\":%d,\"total_requests\":%" PRIu64 ",\"total_commands\":%" PRIu64 ",\"errors\":%" PRIu64 "}\n",
                    skred_version(), m.port, m.active_connections, m.raw_clients, m.http_clients, m.ws_clients, m.total_requests, m.total_commands, m.errors);
                raw_tcp_send(c, stat_msg, (int)strlen(stat_msg));
            } else {
                c->ctx.log[0] = '\0';
                c->ctx.log_len = 0;
                c->ctx.log_head = 0;
                c->ctx.log_count = 0;
                c->ctx.log_pending_len = 0;
                c->ctx.log_pending[0] = '\0';
                c->ctx.log_enable = 1;

                skred_command_ctx(line, &c->ctx);

                /* Silence on success: only emit output if command generated log (queries, errors) */
                if (c->ctx.log_len > 0) {
                    raw_tcp_send(c, c->ctx.log, c->ctx.log_len);
                }
            }
        }

        int consumed = (int)(newline + 1 - c->rx_buf);
        int rem = c->rx_len - consumed;
        if (rem > 0) {
            memmove(c->rx_buf, c->rx_buf + consumed, rem);
            c->rx_len = rem;
            c->rx_buf[c->rx_len] = '\0';
        } else {
            c->rx_len = 0;
        }
    }
}

/* -------------------------------------------------------------------------
 * WebSocket Mode (RFC 6455): Frame creation and parsing
 * ------------------------------------------------------------------------- */
static void ws_send_frame(sk_http_client_t *c, uint8_t opcode, const void *payload, int payload_len) {
    if (!c || c->sock == INVALID_SOCKET) return;
    if (payload_len < 0) payload_len = 0;

    uint8_t hdr[10];
    int hdr_len = 0;
    hdr[0] = 0x80 | (opcode & 0x0F); /* FIN = 1 */

    if (payload_len < 126) {
        hdr[1] = (uint8_t)payload_len;
        hdr_len = 2;
    } else if (payload_len <= 65535) {
        hdr[1] = 126;
        hdr[2] = (uint8_t)((payload_len >> 8) & 0xFF);
        hdr[3] = (uint8_t)(payload_len & 0xFF);
        hdr_len = 4;
    } else {
        hdr[1] = 127;
        for (int i = 0; i < 8; i++) {
            hdr[2 + i] = (uint8_t)(((uint64_t)payload_len >> ((7 - i) * 8)) & 0xFF);
        }
        hdr_len = 10;
    }

    int total_len = hdr_len + payload_len;
    char *frame_buf = (char *)malloc(total_len);
    if (!frame_buf) return;

    memcpy(frame_buf, hdr, hdr_len);
    if (payload_len > 0 && payload) {
        memcpy(frame_buf + hdr_len, payload, payload_len);
    }

    queue_outgoing_data(c, frame_buf, total_len);
    free(frame_buf);
}

static void client_read_websocket(sk_http_client_t *c) {
    while (c->in_use && c->rx_len >= 2) {
        uint8_t *data = (uint8_t *)c->rx_buf;
        int opcode = data[0] & 0x0F;
        int masked = (data[1] & 0x80) != 0;
        uint64_t payload_len = data[1] & 0x7F;
        int hdr_len = 2;

        if (payload_len == 126) {
            if (c->rx_len < 4) return;
            payload_len = ((uint64_t)data[2] << 8) | (uint64_t)data[3];
            hdr_len = 4;
        } else if (payload_len == 127) {
            if (c->rx_len < 10) return;
            payload_len = 0;
            for (int i = 0; i < 8; i++) {
                payload_len = (payload_len << 8) | (uint64_t)data[2 + i];
            }
            hdr_len = 10;
        }

        if (payload_len > (uint64_t)(sizeof(c->rx_buf) - 64)) {
            atomic_fetch_add_uint64(&metric_errors, 1);
            ws_send_frame(c, 0x8, NULL, 0);
            client_close(c);
            return;
        }

        uint8_t mask_key[4] = {0, 0, 0, 0};
        if (masked) {
            if (c->rx_len < hdr_len + 4) return;
            memcpy(mask_key, data + hdr_len, 4);
            hdr_len += 4;
        }

        if ((uint64_t)c->rx_len < (uint64_t)hdr_len + payload_len) {
            return; /* Incomplete frame, wait for remainder */
        }

        uint8_t *payload = data + hdr_len;
        if (masked) {
            for (uint64_t i = 0; i < payload_len; i++) {
                payload[i] ^= mask_key[i % 4];
            }
        }

        if (opcode == 0x8) {
            /* Close frame */
            ws_send_frame(c, 0x8, payload, (int)payload_len);
            client_close(c);
            return;
        } else if (opcode == 0x9) {
            /* Ping frame: echo payload in Pong frame */
            ws_send_frame(c, 0xA, payload, (int)payload_len);
        } else if (opcode == 0xA) {
            /* Pong frame: do nothing */
        } else if (opcode == 0x1) {
            /* Text Skode command frame */
            atomic_fetch_add_uint64(&metric_requests, 1);

            char *buf = (char *)malloc(payload_len + 1);
            if (buf) {
                memcpy(buf, payload, payload_len);
                buf[payload_len] = '\0';

                c->ctx.log[0] = '\0';
                c->ctx.log_len = 0;
                c->ctx.log_head = 0;
                c->ctx.log_count = 0;
                c->ctx.log_pending_len = 0;
                c->ctx.log_pending[0] = '\0';
                c->ctx.log_enable = 1;

                char *line = buf;
                while (line && *line) {
                    char *next = strchr(line, '\n');
                    if (next) {
                        *next = '\0';
                        if (next > line && *(next - 1) == '\r') *(next - 1) = '\0';
                        next++;
                    }
                    while (*line == ' ' || *line == '\t') line++;
                    if (*line) {
                        atomic_fetch_add_uint64(&metric_commands, 1);
                        if (path_match(line, "reset") || strcmp(line, "reset") == 0) {
                            skode_free(&c->ctx);
                            skode_init(&c->ctx);
                            c->ctx.log_enable = 1;
                            /* Silent reset */
                        } else {
                            skred_command_ctx(line, &c->ctx);
                        }
                    }
                    line = next;
                }
                free(buf);

                /* Silence on success: only send frame if command generated log (queries, errors) */
                if (c->ctx.log_len > 0) {
                    ws_send_frame(c, 0x1, c->ctx.log, c->ctx.log_len);
                }
            }
        } else if (opcode == 0x2) {
            /* Binary Opcode Protocol (BOP) frame */
            atomic_fetch_add_uint64(&metric_requests, 1);
            int processed = 0;
            int r = skred_dispatch_batch(payload, (size_t)payload_len, &processed);
            if (processed > 0) {
                atomic_fetch_add_uint64(&metric_commands, (uint64_t)processed);
            }
            if (r < 0) {
                atomic_fetch_add_uint64(&metric_errors, 1);
            }
            /* Silence on success: zero reverse packet flood for binary streams */
        }

        int total_frame_len = hdr_len + (int)payload_len;
        int remaining = c->rx_len - total_frame_len;
        if (remaining > 0) {
            memmove(c->rx_buf, c->rx_buf + total_frame_len, remaining);
            c->rx_len = remaining;
            c->rx_buf[c->rx_len] = '\0';
        } else {
            c->rx_len = 0;
        }
    }
}

/* -------------------------------------------------------------------------
 * HTTP 1.1 Mode: REST, CORS, and WebSocket Upgrades
 * ------------------------------------------------------------------------- */
static int is_websocket_handshake(const char *headers, char *key_out, int key_max) {
    const char *upgrade = find_header(headers, "Upgrade");
    if (!upgrade || !str_contains_case(upgrade, "websocket")) return 0;

    const char *key = find_header(headers, "Sec-WebSocket-Key");
    if (!key) return 0;

    int i = 0;
    while (*key == ' ' || *key == '\t') key++;
    while (*key && *key != '\r' && *key != '\n' && *key != ' ' && *key != '\t' && i < key_max - 1) {
        key_out[i++] = *key++;
    }
    key_out[i] = '\0';
    return (i > 0);
}

static void handle_ws_handshake(sk_http_client_t *c, const char *key) {
    char combined[128];
    snprintf(combined, sizeof(combined), "%s%s", key, WS_GUID);

    sk_sha1_ctx_t sha;
    sha1_init(&sha);
    sha1_update(&sha, (const uint8_t *)combined, strlen(combined));
    uint8_t digest[20];
    sha1_final(&sha, digest);

    char accept_key[48];
    base64_encode(digest, 20, accept_key, sizeof(accept_key));

    char resp[512];
    int rlen = snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key);

    c->proto = SK_PROTO_WEBSOCKET;
    queue_outgoing_data(c, resp, rlen);

    /* Shift any pipelined frame data */
    int consumed = c->body_offset;
    int rem = c->rx_len - consumed;
    if (rem > 0) {
        memmove(c->rx_buf, c->rx_buf + consumed, rem);
        c->rx_len = rem;
        c->rx_buf[c->rx_len] = '\0';
        client_read_websocket(c);
    } else {
        c->rx_len = 0;
    }
}

static void client_respond_cors(sk_http_client_t *c) {
    const char *conn_header = c->keep_alive ? "keep-alive" : "close";
    c->tx_len = snprintf(c->tx_buf, sizeof(c->tx_buf),
        "HTTP/1.1 204 No Content\r\n"
        "Server: sk3-tcp\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: POST, OPTIONS, GET\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n"
        "Access-Control-Allow-Private-Network: true\r\n"
        "Access-Control-Max-Age: 86400\r\n"
        "Connection: %s\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        conn_header);
    c->tx_sent = 0;
    c->http_state = SK_HTTP_SUBSTATE_SENDING_RESPONSE;
}

static void client_execute_and_respond(sk_http_client_t *c, const char *body, int body_len) {
    atomic_fetch_add_uint64(&metric_requests, 1);

    if (path_match(c->path, "reset")) {
        skode_free(&c->ctx);
        skode_init(&c->ctx);
        c->ctx.log_enable = 1;
        const char *resp_body = "OK reset\n";
        int resp_len = (int)strlen(resp_body);
        const char *conn_header = c->keep_alive ? "keep-alive" : "close";
        c->tx_len = snprintf(c->tx_buf, sizeof(c->tx_buf),
            "HTTP/1.1 200 OK\r\n"
            "Server: sk3-tcp\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: POST, OPTIONS, GET\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n"
            "Access-Control-Allow-Private-Network: true\r\n"
            "Connection: %s\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            conn_header, resp_len, resp_body);
        c->tx_sent = 0;
        c->http_state = SK_HTTP_SUBSTATE_SENDING_RESPONSE;
        return;
    }

    c->ctx.log[0] = '\0';
    c->ctx.log_len = 0;
    c->ctx.log_head = 0;
    c->ctx.log_count = 0;
    c->ctx.log_pending_len = 0;
    c->ctx.log_pending[0] = '\0';
    c->ctx.log_enable = 1;

    if (body && body_len > 0) {
        char *copy = (char *)malloc(body_len + 1);
        if (copy) {
            memcpy(copy, body, body_len);
            copy[body_len] = '\0';

            char *line = copy;
            while (line && *line) {
                char *next = strchr(line, '\n');
                if (next) {
                    *next = '\0';
                    if (next > line && *(next - 1) == '\r') *(next - 1) = '\0';
                    next++;
                }
                while (*line == ' ' || *line == '\t') line++;
                if (*line) {
                    atomic_fetch_add_uint64(&metric_commands, 1);
                    skred_command_ctx(line, &c->ctx);
                }
                line = next;
            }
            free(copy);
        }
    }

    const char *resp_body = c->ctx.log_len > 0 ? c->ctx.log : "OK\n";
    int resp_len = (int)strlen(resp_body);
    const char *conn_header = c->keep_alive ? "keep-alive" : "close";

    c->tx_len = snprintf(c->tx_buf, sizeof(c->tx_buf),
        "HTTP/1.1 200 OK\r\n"
        "Server: sk3-tcp\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: POST, OPTIONS, GET\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n"
        "Access-Control-Allow-Private-Network: true\r\n"
        "Connection: %s\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        conn_header, resp_len, resp_body);

    if (c->tx_len >= (int)sizeof(c->tx_buf)) {
        c->tx_len = (int)sizeof(c->tx_buf) - 1;
    }
    c->tx_sent = 0;
    c->http_state = SK_HTTP_SUBSTATE_SENDING_RESPONSE;
}

static void client_read_http(sk_http_client_t *c) {
    if (c->http_state == SK_HTTP_SUBSTATE_READING_HEADERS) {
        char *hdr_end = strstr(c->rx_buf, "\r\n\r\n");
        if (!hdr_end) {
            hdr_end = strstr(c->rx_buf, "\n\n");
            if (hdr_end) {
                c->body_offset = (int)(hdr_end - c->rx_buf) + 2;
            }
        } else {
            c->body_offset = (int)(hdr_end - c->rx_buf) + 4;
        }

        if (hdr_end) {
            *hdr_end = '\0';

            /* Check for RFC 6455 WebSocket Upgrade */
            char ws_key[64];
            if (is_websocket_handshake(c->rx_buf, ws_key, sizeof(ws_key))) {
                handle_ws_handshake(c, ws_key);
                return;
            }

            const char *conn_val = find_header(c->rx_buf, "Connection");
            if (conn_val) {
                c->keep_alive = (strncasecmp(conn_val, "close", 5) != 0);
            } else {
                c->keep_alive = 1;
            }

            const char *cl_val = find_header(c->rx_buf, "Content-Length");
            c->content_length = cl_val ? atoi(cl_val) : 0;
            if (c->content_length < 0) c->content_length = 0;

            c->path[0] = '/';
            c->path[1] = '\0';
            const char *sp1 = strchr(c->rx_buf, ' ');
            if (sp1) {
                sp1++;
                const char *sp2 = strchr(sp1, ' ');
                if (sp2 && (sp2 - sp1) < (int)sizeof(c->path)) {
                    memcpy(c->path, sp1, sp2 - sp1);
                    c->path[sp2 - sp1] = '\0';
                }
            }

            if (strncmp(c->rx_buf, "OPTIONS", 7) == 0) {
                client_respond_cors(c);
                return;
            } else if (strncmp(c->rx_buf, "GET", 3) == 0) {
                char get_body[512];
                const char *content_type = "text/plain; charset=utf-8";

                if (path_match(c->path, "version")) {
                    snprintf(get_body, sizeof(get_body), "%s\n", skred_version());
                } else if (path_match(c->path, "status") || path_match(c->path, "health") || path_match(c->path, "metrics")) {
                    content_type = "application/json; charset=utf-8";
                    skred_http_metrics_t m;
                    skred_http_metrics(&m);
                    snprintf(get_body, sizeof(get_body),
                        "{\"status\":\"ok\",\"version\":\"%s\",\"port\":%d,\"active_connections\":%d,\"raw_clients\":%d,\"http_clients\":%d,\"ws_clients\":%d,\"total_requests\":%" PRIu64 ",\"total_commands\":%" PRIu64 ",\"errors\":%" PRIu64 "}\n",
                        skred_version(), m.port, m.active_connections, m.raw_clients, m.http_clients, m.ws_clients, m.total_requests, m.total_commands, m.errors);
                } else {
                    snprintf(get_body, sizeof(get_body),
                        "sk3 v%s TCP server ready\n"
                        "Protocols supported on this port:\n"
                        "  WS   / (or /-ws, /-skode)   RFC 6455 bidirectional WebSocket for live streaming & Forth\n"
                        "  RAW  TCP socket streaming   Direct line-based ASCII Skode commands\n"
                        "  POST /                     Execute Skode commands in connection context\n"
                        "  POST /-skode (or /.skode)   Execute Skode commands\n"
                        "  POST /-reset (or /.reset)   Reset this connection's context\n"
                        "  GET  /-status (or /.status) JSON engine & TCP metrics\n"
                        "  GET  /-version (or /.version) Plaintext version string\n",
                        skred_version());
                }

                int len = (int)strlen(get_body);
                c->tx_len = snprintf(c->tx_buf, sizeof(c->tx_buf),
                    "HTTP/1.1 200 OK\r\n"
                    "Server: sk3-tcp\r\n"
                    "Content-Type: %s\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Connection: %s\r\n"
                    "Content-Length: %d\r\n"
                    "\r\n"
                    "%s",
                    content_type, c->keep_alive ? "keep-alive" : "close", len, get_body);
                c->tx_sent = 0;
                c->http_state = SK_HTTP_SUBSTATE_SENDING_RESPONSE;
                return;
            } else if (strncmp(c->rx_buf, "POST", 4) == 0) {
                c->http_state = SK_HTTP_SUBSTATE_READING_BODY;
            } else {
                const char *bad = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
                c->tx_len = (int)strlen(bad);
                memcpy(c->tx_buf, bad, c->tx_len);
                c->tx_sent = 0;
                c->http_state = SK_HTTP_SUBSTATE_SENDING_RESPONSE;
                return;
            }
        }
    }

    if (c->http_state == SK_HTTP_SUBSTATE_READING_BODY) {
        int body_available = c->rx_len - c->body_offset;
        if (body_available >= c->content_length) {
            const char *body = c->rx_buf + c->body_offset;
            client_execute_and_respond(c, body, c->content_length);

            int consumed = c->body_offset + c->content_length;
            int remaining = c->rx_len - consumed;
            if (remaining > 0) {
                memmove(c->rx_buf, c->rx_buf + consumed, remaining);
                c->rx_len = remaining;
                c->rx_buf[c->rx_len] = '\0';
            } else {
                c->rx_len = 0;
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * Socket I/O Multiplexing Handlers
 * ------------------------------------------------------------------------- */
static void client_read(sk_http_client_t *c) {
    int space = sizeof(c->rx_buf) - 1 - c->rx_len;
    if (space <= 0) {
        atomic_fetch_add_uint64(&metric_errors, 1);
        client_close(c);
        return;
    }

    int n = recv(c->sock, c->rx_buf + c->rx_len, space, 0);
    if (n == 0) {
        client_close(c);
        return;
    }
    if (n < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return;
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
#endif
        client_close(c);
        return;
    }

    c->rx_len += n;
    c->rx_buf[c->rx_len] = '\0';

    /* Protocol Detection on first received packet */
    if (c->proto == SK_PROTO_DETECT) {
        const char *p = c->rx_buf;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;

        if (strncmp(p, "GET ", 4) == 0 || strncmp(p, "POST ", 5) == 0 ||
            strncmp(p, "OPTIONS ", 8) == 0 || strncmp(p, "HEAD ", 5) == 0 ||
            strncmp(p, "PUT ", 4) == 0 || strncmp(p, "DELETE ", 7) == 0) {
            c->proto = SK_PROTO_HTTP;
            c->http_state = SK_HTTP_SUBSTATE_READING_HEADERS;
            client_read_http(c);
            return;
        } else if (strchr(c->rx_buf, '\n') != NULL || c->rx_len >= 8) {
            c->proto = SK_PROTO_RAW_TCP;
            client_read_raw_tcp(c);
            return;
        } else {
            /* Still ambiguous, wait for more bytes */
            return;
        }
    }

    if (c->proto == SK_PROTO_RAW_TCP) {
        client_read_raw_tcp(c);
    } else if (c->proto == SK_PROTO_HTTP) {
        client_read_http(c);
    } else if (c->proto == SK_PROTO_WEBSOCKET) {
        client_read_websocket(c);
    }
}

static void client_write(sk_http_client_t *c) {
    int remaining = c->tx_len - c->tx_sent;
    if (remaining <= 0) {
        if (c->proto == SK_PROTO_HTTP && c->http_state == SK_HTTP_SUBSTATE_SENDING_RESPONSE) {
            if (c->keep_alive) {
                c->http_state = SK_HTTP_SUBSTATE_READING_HEADERS;
                c->content_length = 0;
                c->body_offset = 0;
                c->tx_len = 0;
                c->tx_sent = 0;
                if (c->rx_len > 0) {
                    client_read_http(c);
                }
            } else {
                client_close(c);
            }
        } else {
            c->tx_len = 0;
            c->tx_sent = 0;
        }
        return;
    }

    int n = send(c->sock, c->tx_buf + c->tx_sent, remaining, 0);
    if (n < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return;
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
#endif
        client_close(c);
        return;
    }

    c->tx_sent += n;
    if (c->tx_sent >= c->tx_len) {
        c->tx_len = 0;
        c->tx_sent = 0;
        if (c->proto == SK_PROTO_HTTP && c->http_state == SK_HTTP_SUBSTATE_SENDING_RESPONSE) {
            if (c->keep_alive) {
                c->http_state = SK_HTTP_SUBSTATE_READING_HEADERS;
                c->content_length = 0;
                c->body_offset = 0;
                if (c->rx_len > 0) {
                    client_read_http(c);
                }
            } else {
                client_close(c);
            }
        }
    }
}

int skred_http_poll(int timeout_ms) {
    if (server_sock == INVALID_SOCKET) return 0;

    fd_set read_fds;
    fd_set write_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    FD_SET(server_sock, &read_fds);
    SOCKET max_fd = server_sock;

    for (int i = 0; i < SK_HTTP_MAX_CLIENTS; i++) {
        if (clients[i].in_use && clients[i].sock != INVALID_SOCKET) {
            SOCKET s = clients[i].sock;
            if (clients[i].proto == SK_PROTO_DETECT ||
                clients[i].proto == SK_PROTO_RAW_TCP ||
                clients[i].proto == SK_PROTO_WEBSOCKET ||
                (clients[i].proto == SK_PROTO_HTTP && clients[i].http_state != SK_HTTP_SUBSTATE_SENDING_RESPONSE)) {
                FD_SET(s, &read_fds);
            }
            if (clients[i].tx_len > clients[i].tx_sent) {
                FD_SET(s, &write_fds);
            }
            if (s > max_fd) max_fd = s;
        }
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int r = select((int)(max_fd + 1), &read_fds, &write_fds, NULL, &tv);
    if (r <= 0) return r;

    /* Accept new connection */
    if (FD_ISSET(server_sock, &read_fds)) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        SOCKET client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock != INVALID_SOCKET) {
            set_nonblocking(client_sock);

            /* Disable Nagle's algorithm for low-latency streaming */
            int nodelay = 1;
            setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));

            int slot = -1;
            for (int i = 0; i < SK_HTTP_MAX_CLIENTS; i++) {
                if (!clients[i].in_use) {
                    slot = i;
                    break;
                }
            }

            if (slot >= 0) {
                sk_http_client_t *c = &clients[slot];
                memset(c, 0, sizeof(*c));
                c->sock = client_sock;
                c->addr = client_addr;
                c->in_use = 1;
                c->proto = SK_PROTO_DETECT;
                c->http_state = SK_HTTP_SUBSTATE_READING_HEADERS;
                c->keep_alive = 1;

                skode_init(&c->ctx);
                c->ctx.log_enable = 1;
            } else {
                const char *busy = "HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\nServer Busy\n";
                send(client_sock, busy, (int)strlen(busy), 0);
                CLOSE_SOCKET(client_sock);
            }
        }
    }

    /* Process active clients */
    for (int i = 0; i < SK_HTTP_MAX_CLIENTS; i++) {
        if (!clients[i].in_use) continue;
        SOCKET s = clients[i].sock;

        if (FD_ISSET(s, &read_fds)) {
            client_read(&clients[i]);
        }
        if (clients[i].in_use && FD_ISSET(s, &write_fds)) {
            client_write(&clients[i]);
        }
    }

    return r;
}

#ifndef _WIN32
static void *http_thread_func(void *arg) {
    (void)arg;
    atomic_store_int(&http_thread_active, 1);
    while (http_running) {
        skred_http_poll(20);
    }
    atomic_store_int(&http_thread_active, 0);
    return NULL;
}
#else
static DWORD WINAPI http_thread_func(LPVOID arg) {
    (void)arg;
    atomic_store_int(&http_thread_active, 1);
    while (http_running) {
        skred_http_poll(20);
    }
    atomic_store_int(&http_thread_active, 0);
    return 0;
}
#endif

int skred_http_start(int port) {
    if (http_running) return http_port;
    if (port <= 0) port = SK_HTTP_DEFAULT_PORT;

#ifdef _WIN32
    static int wsa_init = 0;
    if (!wsa_init) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        wsa_init = 1;
    }
#endif

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCKET) {
        printf("# tcp thread cannot run on port %d\n", port);
        return -1;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif
#endif
    set_nonblocking(server_sock);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        printf("# tcp thread cannot run on port %d\n", port);
        CLOSE_SOCKET(server_sock);
        server_sock = INVALID_SOCKET;
        return -1;
    }

    if (listen(server_sock, 32) < 0) {
        printf("# tcp thread cannot run on port %d\n", port);
        CLOSE_SOCKET(server_sock);
        server_sock = INVALID_SOCKET;
        return -1;
    }

    http_port = port;
    http_running = 1;
    memset(clients, 0, sizeof(clients));

#ifndef _WIN32
    pthread_create(&http_thread, NULL, http_thread_func, NULL);
#else
    http_thread = CreateThread(NULL, 0, http_thread_func, NULL, 0, NULL);
#endif

    return http_port;
}

void skred_http_stop(void) {
    if (!http_running && server_sock == INVALID_SOCKET) return;
    http_running = 0;

#ifndef _WIN32
    if (atomic_load_int(&http_thread_active)) {
        pthread_join(http_thread, NULL);
    }
#else
    if (http_thread) {
        WaitForSingleObject(http_thread, 2000);
        CloseHandle(http_thread);
        http_thread = NULL;
    }
#endif

    for (int i = 0; i < SK_HTTP_MAX_CLIENTS; i++) {
        if (clients[i].in_use) {
            client_close(&clients[i]);
        }
    }

    if (server_sock != INVALID_SOCKET) {
        CLOSE_SOCKET(server_sock);
        server_sock = INVALID_SOCKET;
    }
    http_port = 0;
}

int skred_http_info(void) {
    return (http_running && server_sock != INVALID_SOCKET) ? http_port : 0;
}

int skred_http_metrics(skred_http_metrics_t *out) {
    if (!out) return -1;
    out->port = http_port;
    out->running = http_running;
    out->total_requests = atomic_load_uint64(&metric_requests);
    out->total_commands = atomic_load_uint64(&metric_commands);
    out->errors = atomic_load_uint64(&metric_errors);

    int active = 0, raw = 0, http = 0, ws = 0;
    for (int i = 0; i < SK_HTTP_MAX_CLIENTS; i++) {
        if (clients[i].in_use) {
            active++;
            if (clients[i].proto == SK_PROTO_RAW_TCP) raw++;
            else if (clients[i].proto == SK_PROTO_HTTP) http++;
            else if (clients[i].proto == SK_PROTO_WEBSOCKET) ws++;
        }
    }
    out->active_connections = active;
    out->raw_clients = raw;
    out->http_clients = http;
    out->ws_clients = ws;
    return 0;
}

int skred_tcp_status(char *out, int max_len) {
    if (!out || max_len <= 0) return 0;
    int port = skred_http_info();
    if (port <= 0) {
        return snprintf(out, max_len, "# tcp_port 0 (Off/Fail)\n");
    }

    int raw_count = 0, http_count = 0, ws_count = 0;
    for (int i = 0; i < SK_HTTP_MAX_CLIENTS; i++) {
        if (clients[i].in_use) {
            if (clients[i].proto == SK_PROTO_RAW_TCP) raw_count++;
            else if (clients[i].proto == SK_PROTO_HTTP) http_count++;
            else if (clients[i].proto == SK_PROTO_WEBSOCKET) ws_count++;
        }
    }
    int total = raw_count + http_count + ws_count;
    int o = snprintf(out, max_len, "# tcp_port %d (OK) clients %d (raw:%d http:%d ws:%d)\n",
                     port, total, raw_count, http_count, ws_count);

    for (int i = 0; i < SK_HTTP_MAX_CLIENTS && o < max_len - 64; i++) {
        if (clients[i].in_use) {
            const char *mode = "detect";
            if (clients[i].proto == SK_PROTO_RAW_TCP) mode = "raw";
            else if (clients[i].proto == SK_PROTO_HTTP) mode = "http";
            else if (clients[i].proto == SK_PROTO_WEBSOCKET) mode = "ws";

            char ip_str[INET_ADDRSTRLEN] = "unknown";
            inet_ntop(AF_INET, &clients[i].addr.sin_addr, ip_str, sizeof(ip_str));
            int client_port = ntohs(clients[i].addr.sin_port);

            o += snprintf(out + o, max_len - o, "#   client [%d] %s:%d %s\n",
                          i, ip_str, client_port, mode);
        }
    }
    return o;
}

#endif /* !__EMSCRIPTEN__ */
