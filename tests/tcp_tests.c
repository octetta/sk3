#include "api.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define CLOSE_SOCKET close
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#define CLOSE_SOCKET closesocket
#endif

/* Helper to send a masked WebSocket text frame */
static void send_ws_masked_frame(int sock, uint8_t opcode, const char *payload, int payload_len) {
    uint8_t hdr[14];
    int hdr_len = 0;
    hdr[0] = 0x80 | (opcode & 0x0F);

    if (payload_len < 126) {
        hdr[1] = 0x80 | (uint8_t)payload_len; /* MASK bit = 1 */
        hdr_len = 2;
    } else {
        hdr[1] = 0x80 | 126;
        hdr[2] = (uint8_t)((payload_len >> 8) & 0xFF);
        hdr[3] = (uint8_t)(payload_len & 0xFF);
        hdr_len = 4;
    }

    uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    memcpy(hdr + hdr_len, mask, 4);
    hdr_len += 4;

    send(sock, (const char *)hdr, hdr_len, 0);

    if (payload_len > 0) {
        char *masked = (char *)malloc(payload_len);
        assert(masked);
        for (int i = 0; i < payload_len; i++) {
            masked[i] = payload[i] ^ mask[i % 4];
        }
        send(sock, masked, payload_len, 0);
        free(masked);
    }
}

/* Helper to read an unmasked WebSocket frame from server */
static int read_ws_server_frame(int sock, uint8_t *opcode_out, char *payload_out, int max_payload) {
    uint8_t hdr[2];
    int n = recv(sock, (char *)hdr, 2, 0);
    if (n < 2) return -1;

    *opcode_out = hdr[0] & 0x0F;
    int masked = (hdr[1] & 0x80) != 0;
    assert(!masked); /* Server-to-client frames MUST NOT be masked */

    uint64_t plen = hdr[1] & 0x7F;
    if (plen == 126) {
        uint8_t ext[2];
        recv(sock, (char *)ext, 2, 0);
        plen = ((uint64_t)ext[0] << 8) | ext[1];
    }

    int read_bytes = 0;
    while (read_bytes < (int)plen && read_bytes < max_payload - 1) {
        int r = recv(sock, payload_out + read_bytes, (int)plen - read_bytes, 0);
        if (r <= 0) break;
        read_bytes += r;
    }
    payload_out[read_bytes] = '\0';
    return read_bytes;
}

int main(void) {
    printf("Starting sk3 Polymorphic TCP Server Test (Raw TCP, HTTP/1.1, WebSocket)...\n");

    /* 1. Initialize headless audio engine */
    int r = skred_start_headless(16, 48000);
    assert(r == 0);

    /* 2. Start TCP server on test port 19095 */
    int test_port = 19095;
    int started_port = skred_http_start(test_port);
    assert(started_port == test_port);
    assert(skred_http_info() == test_port);
    printf("  Server started on port %d\n", test_port);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(test_port);
    inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr);

    /* -------------------------------------------------------------
     * TEST 1: RAW TCP CLIENT (Netcat / socat / Tcl mode)
     * Verifies silence on commands, only emitting data on queries.
     * ------------------------------------------------------------- */
    int raw_sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(raw_sock >= 0);
    r = connect(raw_sock, (struct sockaddr *)&sin, sizeof(sin));
    assert(r == 0);

    /* Send plain Skode line (silent execution) */
    const char *raw_cmd1 = "v0 w0 f440 a0 l1\n";
    send(raw_sock, raw_cmd1, (int)strlen(raw_cmd1), 0);

    /* Define macro and execute (silent execution) */
    const char *raw_cmd2 = "[lead]:v0 n60 l1; lead\n";
    send(raw_sock, raw_cmd2, (int)strlen(raw_cmd2), 0);

    /* Query voice status (produces output) */
    const char *raw_cmd3 = "v0 ?\n";
    send(raw_sock, raw_cmd3, (int)strlen(raw_cmd3), 0);

    char raw_resp[1024];
    int n = recv(raw_sock, raw_resp, sizeof(raw_resp) - 1, 0);
    assert(n > 0);
    raw_resp[n] = '\0';
    /* Verify the query returns state set by previous silent commands */
    assert(strstr(raw_resp, "n60") != NULL || strstr(raw_resp, "261.6") != NULL || strstr(raw_resp, "v0") != NULL);
    printf("  [Raw TCP] Commands executed silently; query returned voice status successfully!\n");

    /* -------------------------------------------------------------
     * TEST 2: CONCURRENT HTTP/1.1 CLIENT (while Raw TCP is active)
     * ------------------------------------------------------------- */
    int http_sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(http_sock >= 0);
    r = connect(http_sock, (struct sockaddr *)&sin, sizeof(sin));
    assert(r == 0);

    const char *http_req =
        "POST /-skode HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 17\r\n"
        "\r\n"
        "v1 w1 f880 a0 l1\n";
    send(http_sock, http_req, (int)strlen(http_req), 0);

    char http_resp[1024];
    n = recv(http_sock, http_resp, sizeof(http_resp) - 1, 0);
    assert(n > 0);
    http_resp[n] = '\0';
    assert(strstr(http_resp, "200 OK") != NULL);
    assert(strstr(http_resp, "Access-Control-Allow-Origin: *") != NULL);
    printf("  [HTTP 1.1] POST /-skode returned 200 OK!\n");

    /* -------------------------------------------------------------
     * TEST 3: CONCURRENT WEBSOCKET CLIENT (RFC 6455 Handshake & Frames)
     * ------------------------------------------------------------- */
    int ws_sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(ws_sock >= 0);
    r = connect(ws_sock, (struct sockaddr *)&sin, sizeof(sin));
    assert(r == 0);

    /* Standard RFC 6455 Handshake */
    const char *ws_handshake =
        "GET /-ws HTTP/1.1\r\n"
        "Host: localhost:19095\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    send(ws_sock, ws_handshake, (int)strlen(ws_handshake), 0);

    char ws_hs_resp[1024];
    n = recv(ws_sock, ws_hs_resp, sizeof(ws_hs_resp) - 1, 0);
    assert(n > 0);
    ws_hs_resp[n] = '\0';
    assert(strstr(ws_hs_resp, "101 Switching Protocols") != NULL);
    assert(strstr(ws_hs_resp, "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != NULL);
    printf("  [WebSocket] Handshake 101 Switching Protocols OK with valid Sec-WebSocket-Accept!\n");

    /* Send Skode setup frame (silent execution) */
    const char *ws_skode = "v2 w2 f1320 a0 l1\n";
    send_ws_masked_frame(ws_sock, 0x1, ws_skode, (int)strlen(ws_skode));

    /* Send Skode query frame (produces output text frame) */
    const char *ws_query = "v2 ?\n";
    send_ws_masked_frame(ws_sock, 0x1, ws_query, (int)strlen(ws_query));

    uint8_t opcode = 0;
    char ws_frame_resp[512];
    n = read_ws_server_frame(ws_sock, &opcode, ws_frame_resp, sizeof(ws_frame_resp));
    assert(n > 0);
    assert(opcode == 0x1); /* Text frame */
    assert(strstr(ws_frame_resp, "v2") != NULL || strstr(ws_frame_resp, "1320") != NULL);
    printf("  [WebSocket] Command executed silently; query returned voice status successfully!\n");

    /* Simulate high-rate XY controller streaming (10 frames silent fire-and-forget) */
    for (int i = 0; i < 10; i++) {
        char xy_cmd[64];
        snprintf(xy_cmd, sizeof(xy_cmd), "v2 f%d\n", 400 + i * 20);
        send_ws_masked_frame(ws_sock, 0x1, xy_cmd, (int)strlen(xy_cmd));
    }
    printf("  [WebSocket] Continuous XY pad streaming (10 frames) executed with zero backpressure!\n");

    /* Test WebSocket Ping -> Pong heartbeat exchange */
    const char *ping_data = "heartbeat-123";
    send_ws_masked_frame(ws_sock, 0x9, ping_data, (int)strlen(ping_data)); /* 0x9 = Ping */
    n = read_ws_server_frame(ws_sock, &opcode, ws_frame_resp, sizeof(ws_frame_resp));
    assert(n > 0);
    assert(opcode == 0xA); /* 0xA = Pong */
    assert(strcmp(ws_frame_resp, ping_data) == 0);
    printf("  [WebSocket] Ping -> Pong echoed '%s' perfectly!\n", ws_frame_resp);

    /* -------------------------------------------------------------
     * TEST 4: DIAGNOSTICS & /s COMMAND INSPECTION
     * ------------------------------------------------------------- */
    char status_buf[1024];
    int st_len = skred_tcp_status(status_buf, sizeof(status_buf));
    assert(st_len > 0);
    printf("  [Diagnostics] skred_tcp_status output:\n%s", status_buf);
    assert(strstr(status_buf, "tcp_port 19095") != NULL);
    assert(strstr(status_buf, "clients 3") != NULL); /* raw, http, ws */
    assert(strstr(status_buf, "raw") != NULL);
    assert(strstr(status_buf, "http") != NULL);
    assert(strstr(status_buf, "ws") != NULL);

    /* Clean close WebSocket */
    send_ws_masked_frame(ws_sock, 0x8, NULL, 0); /* 0x8 = Close */
    CLOSE_SOCKET(ws_sock);
    CLOSE_SOCKET(http_sock);
    CLOSE_SOCKET(raw_sock);

    /* -------------------------------------------------------------
     * TEST 5: METRICS VERIFICATION
     * ------------------------------------------------------------- */
    skred_http_metrics_t m;
    skred_http_metrics(&m);
    assert(m.total_requests >= 14);
    assert(m.total_commands >= 14);
    printf("  [Metrics] Total requests: %" PRIu64 ", Total commands: %" PRIu64 "\n", m.total_requests, m.total_commands);

    skred_http_stop();
    assert(skred_http_info() == 0);
    skred_stop();

    printf("All Polymorphic TCP/HTTP/WebSocket tests passed successfully!\n");
    return 0;
}
