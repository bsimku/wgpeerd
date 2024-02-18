#include "client.h"

#include <stdlib.h>
#include <unistd.h>
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
#include "net.h"
#include "packets.h"
#include "socket.h"

client_t *client_new() {
    client_t *client = safe_alloc(sizeof(client_t));

    client->connect_failed = false;
    client->connected = false;

    return client;
}

int client_init(client_t *client) {
    if ((client->fd = socket_create_tcp()) == -1)
        return -1;

    if (socket_set_non_blocking(client->fd) == -1)
        return -1;

    return 0;
}

void client_setup_poll(client_t *client, struct pollfd *fd) {
    fd->fd = client->fd;
    fd->events = POLLIN | POLLOUT;
    fd->revents = 0;
}

int client_connect(client_t *client, const char *host, unsigned short port) {
    client->connect_failed = false;
    client->last_conn = time(NULL);

    struct sockaddr_in addr;

    if (!net_resolve_host(host, &addr))
        return -1;

    addr.sin_port = htons(port);

    if (connect(client->fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        if (errno != EINPROGRESS) {
            LOG(ERROR, "connect() failed: %s", strerror(errno));
            client->connect_failed = true;
        }

        return -1;
    }

    LOG(INFO, "connected to server.");

    client->connected = true;

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
            client->connect_failed = true;
            client_close(client);
        }

        return -1;
    }

    *packet = &client->packet;

    return 0;
}

int client_check_poll(client_t *client, struct pollfd *fd) {
    if (fd->revents & POLLHUP || fd->revents & POLLERR) {
        client->connect_failed = true;
        client->connected = false;

        return CLIENT_ERROR;
    }

    if (!client->connected && fd->revents & POLLOUT) {
        fd->events &= ~POLLOUT;
        fd->revents &= ~POLLOUT;

        int err;
        socklen_t err_size = sizeof(err);

        if (getsockopt(client->fd, SOL_SOCKET, SO_ERROR, &err, &err_size) == -1) {
            client_close(client);
            LOG(ERROR, "getsockopt() failed: %s", strerror(errno));
            return CLIENT_ERROR;
        }

        if (err != 0) {
            if (err == EINPROGRESS)
                return CLIENT_OK;

            LOG(ERROR, "connect() failed: %s", strerror(errno));

            client->connect_failed = true;

            return CLIENT_ERROR;
        }

        LOG(INFO, "connected to server.");

        client->connected = true;

        return CLIENT_OK;
    }

    if (fd->revents & POLLIN) {
        fd->revents &= ~POLLIN;
        return CLIENT_RECEIVED_PACKET;
    }

    return CLIENT_OK;
}

void client_close(client_t *client) {
    close(client->fd);

    client->connected = false;
    client->fd = -1;
}

void client_free(client_t *client) {
    if (!client)
        return;

    free(client);
}
