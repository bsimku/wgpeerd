#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <stddef.h>

#include "packets.h"

typedef struct {
    int fd;
    bool connect_error;
    packet_t packet;
} client_t;

client_t *client_new();
int client_init(client_t *client);
int client_connect(client_t *client, const char *host, unsigned short port);
int client_send_packet(client_t *client, packet_t *packet);
int client_read_packet(client_t *client, packet_t **packet);
void client_close(client_t *client);
void client_free(client_t *client);

#endif
