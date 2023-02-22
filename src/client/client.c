#include "client.h"

#include <stdlib.h>
#include <sys/unistd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include "log.h"
#include "memory.h"
#include "packets.h"
#include "socket.h"

client_t *client_new() {
    return safe_alloc(sizeof(client_t));
}

int client_init(client_t *client) {
    if (!client)
        return -1;

    client->fd = socket_create();

    if (client->fd == -1)
        return -1;

    return 0;
}

int client_connect(client_t *client, const char *host, unsigned short port) {
    if (!client)
        return -1;

    struct addrinfo hints, *info;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(host, NULL, &hints, &info);

    if (ret != 0) {
        LOG(ERROR, "getaddrinfo() for %s failed: %s", host, gai_strerror(ret));
        return -1;
    }

    struct sockaddr_in addr;

    for (struct addrinfo *res = info; res != NULL; res = res->ai_next) {
        if (res->ai_family != AF_INET)
            continue;

        addr = *(struct sockaddr_in *)res->ai_addr;

        break;
    }

    freeaddrinfo(info);

    addr.sin_port = htons(port);

    if (connect(client->fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        if (!client->connect_error) {
            LOG(ERROR, "connect() failed: %s", strerror(errno));
            client->connect_error = true;
        }

        return -1;
    }

    LOG(INFO, "connected to server.");

    client->connect_error = false;

    return 0;
}

int client_send_packet(client_t *client, packet_t *packet) {
    if (!client)
        return -1;

    return socket_send_packet(client->fd, packet);
}

int client_read_packet(client_t *client, packet_t **packet) {
    if (!client || !packet)
        return -1;

    const int ret = socket_read_packet(client->fd, &client->packet);

    if (ret != SOCK_OK) {
        if (ret == SOCK_DISCONNECTED) {
            LOG(ERROR, "lost connection to server.");
        }

        return -1;
    }

    *packet = &client->packet;

    return 0;
}

void client_close(client_t *client) {
    if (!client)
        return;

    close(client->fd);
}

void client_free(client_t *client) {
    if (!client)
        return;

    free(client);
}
