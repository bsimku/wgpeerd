#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 9742
#define ADDR_MAX_LEN 20

bool net_addr_matches(struct sockaddr_in *a, struct sockaddr_in *b);
bool net_addr_and_port_matches(struct sockaddr_in *a, struct sockaddr_in *b);
bool net_resolve_host(const char *host, struct sockaddr_in *addr);
bool net_parse_addr(struct sockaddr_in *raddr, const char *saddr);
char *net_addr_to_str(struct sockaddr_in *addr, char buf[ADDR_MAX_LEN]);

#endif
