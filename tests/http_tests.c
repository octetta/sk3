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

int main(void) {
    printf("Starting sk3 HTTP POST server test...\n");

    /* 1. Initialize headless engine */
    int r = skred_start_headless(16, 48000);
    assert(r == 0);

    /* 2. Start HTTP server on port 18080 */
    int test_port = 18080;
    int started_port = skred_http_start(test_port);
    assert(started_port == test_port);
    assert(skred_http_info() == test_port);

    /* 3. Connect Client 1 */
    int sock1 = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock1 >= 0);
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(test_port);
    inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr);

    r = connect(sock1, (struct sockaddr *)&sin, sizeof(sin));
    assert(r == 0);

    /* Send POST command */
    const char *req1 = 
        "POST /skode HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 12\r\n"
        "\r\n"
        "v0 440 F on\n";
    send(sock1, req1, (int)strlen(req1), 0);

    /* Read response */
    char resp1[1024];
    int n1 = recv(sock1, resp1, sizeof(resp1) - 1, 0);
    assert(n1 > 0);
    resp1[n1] = '\0';
    assert(strstr(resp1, "200 OK") != NULL);
    printf("  Client 1 received: 200 OK\n");

    /* Send second command on same connection (verifies persistent connection & context) */
    const char *req2 =
        "POST /skode HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 9\r\n"
        "\r\n"
        "v0 880 F\n";
    send(sock1, req2, (int)strlen(req2), 0);

    char resp2[1024];
    int n2 = recv(sock1, resp2, sizeof(resp2) - 1, 0);
    assert(n2 > 0);
    resp2[n2] = '\0';
    assert(strstr(resp2, "200 OK") != NULL);
    printf("  Client 1 keep-alive command 2 received: 200 OK\n");

    CLOSE_SOCKET(sock1);

    /* 4. Test OPTIONS preflight */
    int sock2 = socket(AF_INET, SOCK_STREAM, 0);
    r = connect(sock2, (struct sockaddr *)&sin, sizeof(sin));
    assert(r == 0);
    const char *opts = 
        "OPTIONS /skode HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n";
    send(sock2, opts, (int)strlen(opts), 0);
    char resp_opt[1024];
    int n_opt = recv(sock2, resp_opt, sizeof(resp_opt) - 1, 0);
    assert(n_opt > 0);
    resp_opt[n_opt] = '\0';
    assert(strstr(resp_opt, "204 No Content") != NULL);
    assert(strstr(resp_opt, "Access-Control-Allow-Origin: *") != NULL);
    printf("  Client 2 received CORS 204\n");
    CLOSE_SOCKET(sock2);

    /* 5. Query metrics */
    skred_http_metrics_t m;
    skred_http_metrics(&m);
    assert(m.total_requests >= 3);
    assert(m.total_commands >= 2);
    printf("  HTTP metrics: requests=%" PRIu64 ", commands=%" PRIu64 "\n", m.total_requests, m.total_commands);

    /* 6. Stop server */
    skred_http_stop();
    assert(skred_http_info() == 0);
    skred_stop();

    printf("All HTTP tests passed successfully!\n");
    return 0;
}
