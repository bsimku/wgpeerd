#ifndef SOCKET_H
#define SOCKET_H

#include "packets.h"

#define SOCK_OK 0
#define SOCK_ERROR -1
#define SOCK_DISCONNECTED -2
#define SOCK_AGAIN -3

int socket_create();
int socket_set_non_blocking(const int fd);
int socket_accept(const int fd);
int socket_send(const int fd, const void *data, const size_t size);
int socket_read(const int fd, void *data, const size_t size);
int socket_send_packet(const int fd, packet_t *packet);
int socket_read_packet(const int fd, packet_t *packet);

#endif
