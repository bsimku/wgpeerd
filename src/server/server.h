#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>
#include <stdint.h>

#include <sys/epoll.h>

#include "packets.h"

#define SERVER_MAX_CLIENTS 32
#define SERVER_MAX_REVENTS 4

typedef struct {
    int fd;
} client_t;

typedef struct {
    int fd;
    int epoll_fd;
    client_t clients[SERVER_MAX_CLIENTS];
    struct epoll_event revents[SERVER_MAX_REVENTS];
    int revent_idx;
    int nrevents;
    size_t nclients;
    packet_t packet;
} server_t;

typedef enum {
    POLL_NEW_CONNECTION,
    POLL_RECEIVED_DATA,
    POLL_TIMEOUT,
    POLL_DISCONNECT,
    POLL_ERROR
} poll_status;

server_t *server_new();
int server_init(server_t *server);
int server_listen(server_t *server, unsigned short port);
int server_accept(server_t *server, client_t **client);
poll_status server_poll(server_t *server, client_t **client);
int server_read_packet(server_t *server, client_t *client, packet_t **packet);
int server_send_packet(server_t *server, client_t *client, packet_t *packet);
void server_close(server_t *server);

#endif
