#ifndef SKRED_HTTP_H
#define SKRED_HTTP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SK_HTTP_DEFAULT_PORT (8080)
#define SK_HTTP_MAX_CLIENTS  (64)

typedef struct {
    int port;
    int running;
    int active_connections;
    uint64_t total_requests;
    uint64_t total_commands;
    uint64_t errors;
} skred_http_metrics_t;

/* Start the HTTP server on specified port (spawns 1 single-thread multiplexed I/O loop)
 * Returns the listening port on success, or -1 on error. */
int skred_http_start(int port);

/* Stop the HTTP server, close all active client connections, and free their contexts */
void skred_http_stop(void);

/* Returns listening port if running, or 0 if stopped */
int skred_http_info(void);

/* Single-tick multiplexing loop using select() non-blocking I/O.
 * Can be called synchronously by single-threaded hosts with timeout_ms (e.g. 0 for non-blocking).
 * Returns the number of events processed, or -1 on error. */
int skred_http_poll(int timeout_ms);

/* Retrieve live metrics from the HTTP server */
int skred_http_metrics(skred_http_metrics_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SKRED_HTTP_H */
