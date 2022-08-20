#include "server.h"

#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "socket.h"
#include "packets.h"
#include "log.h"

server_t *server_new() {
    server_t *server = malloc(sizeof(server_t));

    memset(server->fds, 0, sizeof(server->fds));

    server->fd = -1;
    server->nfds = 0;
    server->nclients = 0;

    return server;
}

int server_init(server_t *server) {
    if (!server)
        return -1;

    server->fd = socket_create();

    if (server->fd == -1)
        return -1;

    const int opt = 1;

    if (setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        LOG(ERROR, "setsockopt() failed: %s", strerror(errno));
        return -1;
    }

    struct timeval timeout = {
        .tv_sec = 1,
        .tv_usec = 0
    };

    if (setsockopt(server->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        LOG(ERROR, "setsockopt() failed: %s", strerror(errno));
        return -1;
    }

    server->fds[0].fd = server->fd;
    server->fds[0].events = POLLIN;

    server->nfds = 1;

    return 0;
}

int server_listen(server_t *server, unsigned short port) {
    if (!server)
        return -1;

    const struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr = {INADDR_ANY},
        .sin_port = htons(port)
    };

    if (bind(server->fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        LOG(ERROR, "bind() failed: %s", strerror(errno));
        return -1;
    }

    if (listen(server->fd, 0) == -1) {
        LOG(ERROR, "listen() failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

client_t *server_add_client(server_t *server, int fd) {
    client_t *client = &server->clients[server->nclients++];

    client->fd = fd;

    server->fds[server->nfds].fd = fd;
    server->fds[server->nfds].revents = POLLIN;

    server->nfds++;

    return client;
}

void server_remove_client(server_t *server, client_t *client) {
    for (int i = 0; i < server->nclients; i++) {
        if (server->clients[i].fd != client->fd)
            continue;

        close(server->clients[i].fd);

        for (int j = i + 1; j < server->nclients; j++) {
            server->clients[j - 1] = server->clients[j];
        }

        for (int j = i + 2; j < server->nfds; j++) {
            server->fds[j - 1] = server->fds[j];
        }

        server->nclients--;

        break;
    }
}

int server_accept(server_t *server, client_t **client) {
    if (!server || !client)
        return -1;

    if (server->nclients >= SERVER_MAX_CLIENTS) {
        LOG(ERROR, "can't accept connection, maximum client count reached.");
        return -1;
    }

    struct sockaddr_in addr;
    socklen_t size = sizeof(addr);

    const int fd = accept(server->fd, (struct sockaddr *)&addr, &size);

    if (fd < 0) {
        LOG(ERROR, "accept() failed: %s", strerror(errno));
        return -1;
    }

    *client = server_add_client(server, fd);

    LOG(DEBUG, "accepted client.");

    return 0;
}

poll_status server_handle_poll_revents(server_t *server, client_t **client) {
    if (server->fds[0].revents) {
        server->fds[0].revents = 0;

        if (server_accept(server, client) == -1)
            return POLL_ERROR;

        return POLL_NEW_CONNECTION;
    }

    for (nfds_t i = 1; i < server->nfds; i++) {
        const short revents = server->fds[i].revents;

        if (!revents)
            continue;

        server->fds[i].revents = 0;

        *client = &server->clients[i - 1];

        if (revents == POLLHUP) {
            server_remove_client(server, *client);

            return POLL_DISCONNECT;
        }

        if (revents != POLLIN) {
            LOG(ERROR, "unexpected poll() result.");
            return POLL_ERROR;
        }

        for (size_t j = 0; j < server->nclients; j++) {
            if (server->fds[i].fd != server->clients[j].fd)
                continue;

            *client = &server->clients[j];

            return POLL_RECEIVED_DATA;
        }

        return POLL_ERROR;
    }

    return 0;
}

poll_status server_poll(server_t *server, client_t **client) {
    if (!server || !client)
        return POLL_ERROR;

    poll_status status;

    if ((status = server_handle_poll_revents(server, client)))
        return status;

    LOG(DEBUG, "poll: %d", server->nfds);

    const int ret = poll(server->fds, server->nfds, 5000);

    if (ret < 0) {
        LOG(ERROR, "poll() failed: %s", strerror(errno));
        return POLL_ERROR;
    }
    else if (ret == 0)
        return POLL_TIMEOUT;

    return server_handle_poll_revents(server, client);
}

int server_read_packet(server_t *server, client_t *client, packet_t **packet) {
    if (!server || !client)
        return -1;

    if (socket_read_packet(client->fd, &server->packet) == -1)
        return -1;

    *packet = &server->packet;

    return 0;
}

int server_send_packet(server_t *server, client_t *client, packet_t *packet) {
    if (!server || !client)
        return -1;

    return socket_send_packet(client->fd, packet);
}

void server_close(server_t *server) {
    if (!server)
        return;

    close(server->fd);

    free(server);
}

