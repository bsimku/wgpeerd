#include "net.h"

#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "log.h"

bool net_addr_matches(struct sockaddr_in *a, struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr;
}
bool net_addr_and_port_matches(struct sockaddr_in *a, struct sockaddr_in *b) {
    return net_addr_matches(a, b) && a->sin_port == b->sin_port;
}
bool net_resolve_host(const char *host, struct sockaddr_in *addr) {
    struct addrinfo *info;

    int ret = getaddrinfo(host, NULL, NULL, &info);

    if (ret != 0) {
        LOG(ERROR, "getaddrinfo() for '%s' failed: %s", host, gai_strerror(ret));
        return false;
    }

    for (struct addrinfo *res = info; res != NULL; res = res->ai_next) {
        if (res->ai_family != AF_INET)
            continue;

        *addr = *(struct sockaddr_in *)res->ai_addr;

        break;
    }

    freeaddrinfo(info);

    return true;
}
bool net_parse_addr(struct sockaddr_in *raddr, const char *saddr) {
    LOG(DEBUG, "saddr = %s", saddr);
    char *str = strdup(saddr);

    char *addr = strtok(str, ":");
    char *port = strtok(NULL, ":");

    if (!addr || !port) {
        LOG(ERROR, "failed to parse address '%s'", saddr);
        goto error;
    }

    if (!net_resolve_host(addr, raddr))
        goto error;

    raddr->sin_port = htons(atoi(port));

    free(str);

    return true;

error:
    free(str);
    return false;
}
char *net_addr_to_str(struct sockaddr_in *addr, char buf[ADDR_MAX_LEN]) {
    if (inet_ntop(AF_INET, &addr->sin_addr, buf, ADDR_MAX_LEN) == NULL) {
        LOG(ERROR, "inet_ntop() failed: %s", strerror(errno));
        buf[0] = '\0';
    }

    return buf;
}
