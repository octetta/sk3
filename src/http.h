#ifndef SKRED_HTTP_H
#define SKRED_HTTP_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SK_HTTP_DEFAULT_PORT (60440)
#define SK_TCP_DEFAULT_PORT  (60440)
#define SK_HTTP_MAX_CLIENTS  (64)

typedef enum {
    SK_PROTO_DETECT = 0,    /* Probing first bytes to auto-detect protocol */
    SK_PROTO_RAW_TCP,       /* Raw ASCII / Skode line streaming */
    SK_PROTO_HTTP,          /* HTTP/1.1 REST */
    SK_PROTO_WEBSOCKET,     /* RFC 6455 full-duplex WebSocket */
    SK_PROTO_CLOSED
} sk_proto_mode_t;

typedef struct {
    int port;
    int running;
    int active_connections;
    int raw_clients;
    int http_clients;
    int ws_clients;
    uint64_t total_requests;
    uint64_t total_commands;
    uint64_t errors;
} skred_http_metrics_t;

/* Start the TCP server on specified port (spawns 1 single-thread multiplexed I/O loop).
 * Automatically supports Raw TCP, HTTP/1.1, and RFC 6455 WebSocket.
 * Returns the listening port on success, or -1 on error. */
int skred_http_start(int port);
static inline int skred_tcp_start(int port) { return skred_http_start(port); }

/* Stop the server, close all active client connections, and free their contexts */
void skred_http_stop(void);
static inline void skred_tcp_stop(void) { skred_http_stop(); }

/* Returns listening port if running, or 0 if stopped */
int skred_http_info(void);
static inline int skred_tcp_info(void) { return skred_http_info(); }

/* Single-tick multiplexing loop using select() non-blocking I/O.
 * Can be called synchronously by single-threaded hosts with timeout_ms.
 * Returns the number of events processed, or -1 on error. */
int skred_http_poll(int timeout_ms);
static inline int skred_tcp_poll(int timeout_ms) { return skred_http_poll(timeout_ms); }

/* Retrieve live metrics from the TCP server */
int skred_http_metrics(skred_http_metrics_t *out);
static inline int skred_tcp_metrics(skred_http_metrics_t *out) { return skred_http_metrics(out); }

/* Format human-readable TCP server and client status for /s command */
int skred_tcp_status(char *out, int max_len);

#ifdef __cplusplus
}
#endif

#endif /* SKRED_HTTP_H */
