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

#include "log.h"
#include "mem.h"
#include "packets.h"
#include "socket.h"

#define EPOLL_TIMEOUT 2000 // ms

server_t *server_new() {
    server_t *server = mem_alloc(sizeof(server_t));

    server->fd = -1;
    server->epoll_fd = -1;
    server->nclients = 0;

    return server;
}

int server_init(server_t *server) {
    if (!server)
        return -1;

    server->revent_idx = 0;
    server->nrevents = 0;

    server->epoll_fd = epoll_create1(0);

    if (server->epoll_fd == -1) {
        LOG(ERROR, "epoll_create1() failed: %s", strerror(errno));
        return -1;
    }

    server->fd = socket_create_tcp();

    if (server->fd == -1)
        return -1;

    if (socket_set_non_blocking(server->fd) == -1)
        return -1;

    struct epoll_event event = {
        .data.fd = server->fd,
        .events = EPOLLIN | EPOLLRDHUP
    };

    if (epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, server->fd, &event) == -1) {
        LOG(ERROR, "epoll_ctl() failed: %s", strerror(errno));
        return -1;
    }

    const int opt = 1;

    if (setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        LOG(ERROR, "setsockopt() failed: %s", strerror(errno));
        return -1;
    }

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

static client_t *add_client(server_t *server, int fd) {
    struct epoll_event event = {
        .data.fd = fd,
        .events = EPOLLIN
    };

    if (epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        LOG(ERROR, "epoll_ctl() failed: %s", strerror(errno));
        return NULL;
    }

    client_t *client = &server->clients[server->nclients++];

    client->fd = fd;

    return client;
}

static void remove_client(server_t *server, client_t *client) {
    if (epoll_ctl(server->epoll_fd, EPOLL_CTL_DEL, client->fd, NULL) == -1) {
        LOG(ERROR, "epoll_ctl() failed: %s", strerror(errno));
    }

    for (int i = 0; i < server->nclients; i++) {
        if (server->clients[i].fd != client->fd)
            continue;

        close(server->clients[i].fd);

        for (int j = i + 1; j < server->nclients; j++) {
            server->clients[j - 1] = server->clients[j];
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

    const int fd = socket_accept(server->fd);

    if (fd == -1)
        return -1;

    if (socket_set_non_blocking(fd) == -1)
        return -1;

    *client = add_client(server, fd);

    LOG(DEBUG, "accepted client (fd = %d)", fd);

    return 0;
}

static client_t *find_client(server_t *server, int fd) {
    for (int i = 0; i < server->nclients; i++) {
        if (server->clients[i].fd == fd)
            return &server->clients[i];
    }

    return NULL;
}

poll_status server_handle_poll_revents(server_t *server, client_t **client) {
    if (server->revent_idx >= server->nrevents)
        return POLL_TIMEOUT;

    const struct epoll_event *revent = &server->revents[server->revent_idx++];

    if (revent->events & EPOLLERR)
        return POLL_ERROR;

    if (revent->data.fd == server->fd) {
        if (server_accept(server, client) == -1)
            return POLL_ERROR;

        return POLL_NEW_CONNECTION;
    }

    LOG(DEBUG, "revent->fd = %d", revent->data.fd);

    *client = find_client(server, revent->data.fd);

    if (*client == NULL)
        return POLL_ERROR;

    if (revent->events & EPOLLRDHUP) {
        remove_client(server, *client);

        return POLL_DISCONNECT;
    }

    if (revent->events & EPOLLIN)
        return POLL_RECEIVED_DATA;

    return POLL_ERROR;
}

poll_status server_poll(server_t *server, client_t **client) {
    LOG(DEBUG, "server_poll()");

    if (!server || !client)
        return POLL_ERROR;

    poll_status status;

    if ((status = server_handle_poll_revents(server, client)) != POLL_TIMEOUT)
        return status;

    int ret;

    while ((ret = epoll_wait(server->epoll_fd, server->revents, SERVER_MAX_REVENTS, EPOLL_TIMEOUT)) < 0) {
        if (errno == EINTR)
            continue;

        LOG(ERROR, "epoll_wait() failed: %s", strerror(errno));

        return POLL_ERROR;
    }

    if (ret == 0)
        return POLL_TIMEOUT;

    server->nrevents = ret;
    server->revent_idx = 0;

    return server_handle_poll_revents(server, client);
}

int server_read_packet(server_t *server, client_t *client, packet_t **packet) {
    if (!server || !client)
        return -1;

    const int ret = socket_read_packet(client->fd, &server->packet);

    if (ret != SOCK_OK) {
        if (ret == SOCK_DISCONNECTED) {
            remove_client(server, client);
        }

        return -1;
    }

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
    close(server->epoll_fd);

    free(server);
}

