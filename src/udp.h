#ifndef SKRED_UDP_H
#define SKRED_UDP_H

#include <stdint.h>

#define UDP_PORT (60440)

typedef struct {
  int port;
  int running;
  uint64_t packets;
  uint64_t commands;
  uint64_t errors;
} skred_udp_metrics_t;

/* Canonical skred_udp_* API */
int skred_udp_start(int port);
void skred_udp_stop(void);
int skred_udp_info(void);
int skred_udp_metrics(skred_udp_metrics_t *out);

/* Legacy aliases for backwards compatibility */
int udp_start(int port);
void udp_stop(void);
int udp_info(void);
int udp_metrics(skred_udp_metrics_t *out);

#endif /* SKRED_UDP_H */
