#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <poll.h>
#include <time.h>

#include "packets.h"

#define CLIENT_OK 0
#define CLIENT_ERROR -1
#define CLIENT_CONNECTED 1
#define CLIENT_RECEIVED_PACKET 2

typedef struct {
    int fd;
    time_t last_conn;
    bool connect_failed;
    bool connected;
    packet_t packet;
} client_t;

client_t *client_new();
int client_init(client_t *client);
void client_setup_poll(client_t *client, struct pollfd *fd);
int client_connect(client_t *client, const char *host, unsigned short port);
int client_send_packet(client_t *client, packet_t *packet);
int client_read_packet(client_t *client, packet_t **packet);
int client_check_poll(client_t *client, struct pollfd *fd);
void client_close(client_t *client);
void client_free(client_t *client);

#endif
