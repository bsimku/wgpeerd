#include "socket.h"

#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>

#include "packets.h"
#include "log.h"

int socket_create() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        LOG(ERROR, "socket() failed: %s", strerror(errno));
        return -1;
    }

    return fd;
}

int socket_send(const int fd, const void *data, const size_t size) {
    ssize_t sent = 0;

    while (sent != size) {
        ssize_t bytes = send(fd, data + sent, size - sent, 0);

        if (bytes == -1) {
            LOG(ERROR, "send() failed: %s", strerror(errno));
            return SOCK_ERROR;
        }

        sent += bytes;
    }

    return SOCK_OK;
}

int socket_read(const int fd, void *data, const size_t size) {
    ssize_t bytesRead = 0;

    while (bytesRead != size) {
        ssize_t ret = read(fd, data + bytesRead, size - bytesRead);

        if (ret == 0)
            return SOCK_DISCONNECTED;

        if (ret < 0) {
            if (errno == EAGAIN)
                return SOCK_AGAIN;

            LOG(ERROR, "read() failed: %s", strerror(errno));

            return SOCK_ERROR;
        }

        bytesRead += ret;
    }

    return SOCK_OK;

}

int socket_send_packet(const int fd, packet_t *packet) {
    const uint32_t size = packet->header.size;

    packet->header.version = htons(packet->header.version);
    packet->header.type = htons(packet->header.type);
    packet->header.size = htonl(packet->header.size);

    return socket_send(fd, packet, sizeof(packet->header) + size);
}

int socket_read_packet(const int fd, packet_t *packet) {
    if (!packet)
        return -1;

    packet_header *header = &packet->header;

    int ret;

    if ((ret = socket_read(fd, header, sizeof(*header))) != SOCK_OK)
        return ret;

    if (packet_parse_header(header) == -1)
        return -1;

    if ((ret = socket_read(fd, &packet->data, header->size)) != SOCK_OK)
        return ret;

    return 0;
}

