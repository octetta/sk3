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

#else

#ifndef _WIN32
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
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
#endif

#define SK_HTTP_RX_BUF_SIZE (32768)
#define SK_HTTP_TX_BUF_SIZE (65536)
#define SK_HTTP_IDLE_TIMEOUT_SEC (60)

typedef enum {
    SK_HTTP_STATE_READING_HEADERS = 0,
    SK_HTTP_STATE_READING_BODY,
    SK_HTTP_STATE_SENDING_RESPONSE,
    SK_HTTP_STATE_CLOSED
} sk_http_conn_state_t;

typedef struct {
    SOCKET sock;
    struct sockaddr_in addr;
    skode_t ctx;                  /* Dedicated per-connection sk3/skode context */
    int in_use;
    sk_http_conn_state_t state;

    char rx_buf[SK_HTTP_RX_BUF_SIZE];
    int rx_len;

    char tx_buf[SK_HTTP_TX_BUF_SIZE];
    int tx_len;
    int tx_sent;

    int content_length;
    int body_offset;
    int keep_alive;
    uint32_t last_activity_sec;
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
    c->state = SK_HTTP_STATE_CLOSED;
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

/* Execute command body in client's context and prepare HTTP response */
static void client_execute_and_respond(sk_http_client_t *c, const char *body, int body_len) {
    atomic_fetch_add_uint64(&metric_requests, 1);

    /* Reset client's log buffer to capture only this request's output */
    c->ctx.log[0] = '\0';
    c->ctx.log_len = 0;
    c->ctx.log_head = 0;
    c->ctx.log_count = 0;
    c->ctx.log_pending_len = 0;
    c->ctx.log_pending[0] = '\0';
    c->ctx.log_enable = 1;

    /* Execute each line in client's dedicated context */
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

    /* Format response */
    const char *resp_body = c->ctx.log_len > 0 ? c->ctx.log : "OK\n";
    int resp_len = (int)strlen(resp_body);

    const char *conn_header = c->keep_alive ? "keep-alive" : "close";

    c->tx_len = snprintf(c->tx_buf, sizeof(c->tx_buf),
        "HTTP/1.1 200 OK\r\n"
        "Server: sk3-http\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: POST, OPTIONS, GET\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: %s\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        conn_header, resp_len, resp_body);

    if (c->tx_len >= (int)sizeof(c->tx_buf)) {
        c->tx_len = (int)sizeof(c->tx_buf) - 1;
    }
    c->tx_sent = 0;
    c->state = SK_HTTP_STATE_SENDING_RESPONSE;
}

/* Respond to CORS OPTIONS preflight */
static void client_respond_cors(sk_http_client_t *c) {
    const char *conn_header = c->keep_alive ? "keep-alive" : "close";
    c->tx_len = snprintf(c->tx_buf, sizeof(c->tx_buf),
        "HTTP/1.1 204 No Content\r\n"
        "Server: sk3-http\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: POST, OPTIONS, GET\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Access-Control-Max-Age: 86400\r\n"
        "Connection: %s\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        conn_header);
    c->tx_sent = 0;
    c->state = SK_HTTP_STATE_SENDING_RESPONSE;
}

/* Process incoming data on client socket */
static void client_read(sk_http_client_t *c) {
    int space = sizeof(c->rx_buf) - 1 - c->rx_len;
    if (space <= 0) {
        /* Buffer overflow - reset */
        atomic_fetch_add_uint64(&metric_errors, 1);
        client_close(c);
        return;
    }

    int n = recv(c->sock, c->rx_buf + c->rx_len, space, 0);
    if (n == 0) {
        /* Clean disconnect by client */
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
        /* Socket error or disconnect */
        client_close(c);
        return;
    }

    c->rx_len += n;
    c->rx_buf[c->rx_len] = '\0';

    if (c->state == SK_HTTP_STATE_READING_HEADERS) {
        char *hdr_end = strstr(c->rx_buf, "\r\n\r\n");
        if (!hdr_end) {
            /* Try Unix line endings \n\n */
            hdr_end = strstr(c->rx_buf, "\n\n");
            if (hdr_end) {
                c->body_offset = (int)(hdr_end - c->rx_buf) + 2;
            }
        } else {
            c->body_offset = (int)(hdr_end - c->rx_buf) + 4;
        }

        if (hdr_end) {
            *hdr_end = '\0'; /* Null-terminate headers string */

            /* Parse keep-alive */
            const char *conn_val = find_header(c->rx_buf, "Connection");
            if (conn_val) {
                c->keep_alive = (strncasecmp(conn_val, "close", 5) != 0);
            } else {
                c->keep_alive = 1; /* HTTP/1.1 default */
            }

            /* Parse Content-Length */
            const char *cl_val = find_header(c->rx_buf, "Content-Length");
            c->content_length = cl_val ? atoi(cl_val) : 0;
            if (c->content_length < 0) c->content_length = 0;

            /* Extract URL path from request line: "METHOD /path HTTP/1.x" */
            char path[128] = "/";
            const char *sp1 = strchr(c->rx_buf, ' ');
            if (sp1) {
                sp1++;
                const char *sp2 = strchr(sp1, ' ');
                if (sp2 && (sp2 - sp1) < (int)sizeof(path)) {
                    memcpy(path, sp1, sp2 - sp1);
                    path[sp2 - sp1] = '\0';
                }
            }

            /* Check method */
            if (strncmp(c->rx_buf, "OPTIONS", 7) == 0) {
                client_respond_cors(c);
                return;
            } else if (strncmp(c->rx_buf, "GET", 3) == 0) {
                char get_body[512];
                const char *content_type = "text/plain; charset=utf-8";

                if (strcmp(path, "/version") == 0) {
                    snprintf(get_body, sizeof(get_body), "%s\n", skred_version());
                } else if (strcmp(path, "/status") == 0 || strcmp(path, "/health") == 0 || strcmp(path, "/metrics") == 0) {
                    content_type = "application/json; charset=utf-8";
                    skred_http_metrics_t m;
                    skred_http_metrics(&m);
                    snprintf(get_body, sizeof(get_body),
                        "{\"status\":\"ok\",\"version\":\"%s\",\"port\":%d,\"active_connections\":%d,\"total_requests\":%" PRIu64 ",\"total_commands\":%" PRIu64 ",\"errors\":%" PRIu64 "}\n",
                        skred_version(), m.port, m.active_connections, m.total_requests, m.total_commands, m.errors);
                } else {
                    snprintf(get_body, sizeof(get_body),
                        "sk3 v%s HTTP endpoint ready\n"
                        "Endpoints:\n"
                        "  POST /skode (or POST /)   Execute Skode Forth commands\n"
                        "  GET  /status              JSON engine & HTTP metrics\n"
                        "  GET  /version             Plaintext version string\n",
                        skred_version());
                }

                int len = (int)strlen(get_body);
                c->tx_len = snprintf(c->tx_buf, sizeof(c->tx_buf),
                    "HTTP/1.1 200 OK\r\n"
                    "Server: sk3-http\r\n"
                    "Content-Type: %s\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Connection: %s\r\n"
                    "Content-Length: %d\r\n"
                    "\r\n"
                    "%s",
                    content_type, c->keep_alive ? "keep-alive" : "close", len, get_body);
                c->tx_sent = 0;
                c->state = SK_HTTP_STATE_SENDING_RESPONSE;
                return;
            } else if (strncmp(c->rx_buf, "POST", 4) == 0) {
                c->state = SK_HTTP_STATE_READING_BODY;
            } else {
                /* Unsupported method */
                const char *bad = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
                c->tx_len = (int)strlen(bad);
                memcpy(c->tx_buf, bad, c->tx_len);
                c->tx_sent = 0;
                c->state = SK_HTTP_STATE_SENDING_RESPONSE;
                return;
            }
        }
    }

    if (c->state == SK_HTTP_STATE_READING_BODY) {
        int body_available = c->rx_len - c->body_offset;
        if (body_available >= c->content_length) {
            /* Full body received */
            const char *body = c->rx_buf + c->body_offset;
            client_execute_and_respond(c, body, c->content_length);

            /* Shift any remaining pipeline bytes */
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

/* Write pending response data to client socket */
static void client_write(sk_http_client_t *c) {
    if (c->state != SK_HTTP_STATE_SENDING_RESPONSE) return;

    int remaining = c->tx_len - c->tx_sent;
    if (remaining <= 0) {
        /* All sent */
        if (c->keep_alive) {
            c->state = SK_HTTP_STATE_READING_HEADERS;
            c->content_length = 0;
            c->body_offset = 0;
            c->tx_len = 0;
            c->tx_sent = 0;
            /* If pipelined bytes exist, process them */
            if (c->rx_len > 0) {
                client_read(c);
            }
        } else {
            client_close(c);
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
        if (c->keep_alive) {
            c->state = SK_HTTP_STATE_READING_HEADERS;
            c->content_length = 0;
            c->body_offset = 0;
            c->tx_len = 0;
            c->tx_sent = 0;
            if (c->rx_len > 0) {
                client_read(c);
            }
        } else {
            client_close(c);
        }
    }
}

/* Single multiplexed tick using select() across listening socket and all active clients */
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
            if (clients[i].state == SK_HTTP_STATE_READING_HEADERS ||
                clients[i].state == SK_HTTP_STATE_READING_BODY) {
                FD_SET(s, &read_fds);
            }
            if (clients[i].state == SK_HTTP_STATE_SENDING_RESPONSE) {
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

            /* Find slot */
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
                c->state = SK_HTTP_STATE_READING_HEADERS;
                c->keep_alive = 1;

                /* Initialize dedicated skode context for this connection */
                skode_init(&c->ctx);
                c->ctx.log_enable = 1;
            } else {
                /* Table full: reject gracefully */
                const char *busy = "HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\nServer Busy\n";
                send(client_sock, busy, (int)strlen(busy), 0);
                CLOSE_SOCKET(client_sock);
            }
        }
    }

    /* Process existing clients */
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
        return -1;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    set_nonblocking(server_sock);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        CLOSE_SOCKET(server_sock);
        server_sock = INVALID_SOCKET;
        return -1;
    }

    if (listen(server_sock, 32) < 0) {
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

    /* Close all active clients and free their contexts */
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

    int active = 0;
    for (int i = 0; i < SK_HTTP_MAX_CLIENTS; i++) {
        if (clients[i].in_use) active++;
    }
    out->active_connections = active;
    return 0;
}

#endif /* !__EMSCRIPTEN__ */
