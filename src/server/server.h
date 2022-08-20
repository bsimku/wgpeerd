#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>
#include <stdint.h>

#include <sys/poll.h>

#include "packets.h"

#define SERVER_MAX_CLIENTS 32

typedef struct {
    int fd;
} client_t;

typedef struct {
    int fd;
    struct pollfd fds[SERVER_MAX_CLIENTS + 1];
    nfds_t nfds;
    client_t clients[SERVER_MAX_CLIENTS];
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
