#include "net.h"

bool net_addr_matches(struct sockaddr_in *a, struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr;
}
bool net_addr_and_port_matches(struct sockaddr_in *a, struct sockaddr_in *b) {
    return net_addr_matches(a, b) && a->sin_port == b->sin_port;
}
