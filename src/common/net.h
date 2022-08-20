#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 9742

bool net_addr_matches(struct sockaddr_in *a, struct sockaddr_in *b);
bool net_addr_and_port_matches(struct sockaddr_in *a, struct sockaddr_in *b);

#endif
