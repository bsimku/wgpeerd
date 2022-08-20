#ifndef SOCKET_H
#define SOCKET_H

#include "packets.h"

int socket_create();
int socket_send(const int fd, const void *data, const size_t size);
int socket_read(const int fd, void *data, const size_t size);
int socket_send_packet(const int fd, packet_t *packet);
int socket_read_packet(const int fd, packet_t *packet);

#endif
