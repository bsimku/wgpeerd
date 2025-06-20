#define _GNU_SOURCE

#include "fwd.h"

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#include "mem.h"
#include "net.h"
#include "wgutil.h"
#include "socket.h"
#include "log.h"

#define BUFFER_LEN 4096

bool fwd_init(fwd_t *fwd, int bind_port, int nfwds) {
    fwd->bind_port = bind_port;

    if (pipe(fwd->pipe_fds) == -1) {
        LOG(ERROR, "pipe() failed: %s", strerror(errno));
        return false;
    }

    fwd->fwds = mem_alloc(nfwds * sizeof(struct fwd));
    fwd->nfwds = nfwds;

    return true;
}

bool fwd_add(fwd_t *fwd, int i_fwd, const char *peer_key, const char *endpoint, unsigned short listen_port) {
    struct fwd *entry = &fwd->fwds[i_fwd];

    if (!wgutil_key_from_base64(entry->peer_key, peer_key))
        return false;

    if ((entry->connect_sock_fd = socket_create_udp()) == -1)
        return false;

    if (socket_set_reuseport(entry->connect_sock_fd) == -1)
        return false;

    const struct sockaddr_in bind_addr = {
        .sin_family = AF_INET,
        .sin_addr = INADDR_ANY,
        .sin_port = htons(fwd->bind_port)
    };

    if (bind(entry->connect_sock_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) == -1) {
        LOG(ERROR, "bind() failed: %s", strerror(errno));
        return false;
    }

    if ((entry->listen_sock_fd = socket_create_udp()) == -1)
        return false;

    const struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr = inet_addr("127.0.0.1"),
        .sin_port = htons(listen_port)
    };

    if (bind(entry->listen_sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        LOG(ERROR, "bind() failed: %s", strerror(errno));
        return false;
    }

    if (!net_parse_addr(&entry->default_endpoint, endpoint))
        return false;

    fwd_set_endpoint(fwd, i_fwd, &entry->default_endpoint);

    return true;
}

void fwd_set_endpoint(fwd_t *fwd, int i_fwd, struct sockaddr_in *addr) {
    struct fwd *entry = &fwd->fwds[i_fwd];

    if (net_addr_and_port_matches(&entry->curr_endpoint, addr))
        return;

    if (connect(entry->connect_sock_fd, (struct sockaddr *)addr, sizeof(*addr)) == -1) {
        LOG(ERROR, "connect() failed: %s", strerror(errno));
        return;
    }

    char old_addr[ADDR_MAX_LEN], new_addr[ADDR_MAX_LEN];

    LOG(INFO, "%s:%d -> %s:%d", net_addr_to_str(&entry->curr_endpoint, old_addr),
                                ntohs(entry->curr_endpoint.sin_port),
                                net_addr_to_str(addr, new_addr),
                                ntohs(addr->sin_port));

    entry->curr_endpoint = *addr;
}

void fwd_setup_poll_connect(fwd_t *fwd, int i_fwd, struct pollfd *fd) {
    fd->fd = fwd->fwds[i_fwd].connect_sock_fd;
    fd->events = POLLIN;
    fd->revents = 0;
}

void fwd_setup_poll_listen(fwd_t *fwd, int i_fwd, struct pollfd *fd) {
    fd->fd = fwd->fwds[i_fwd].listen_sock_fd;
    fd->events = POLLIN;
    fd->revents = 0;
}

static bool forward_packet(fwd_t *fwd, int fd_recv, int fd_send) {
    ssize_t ret;

    if ((ret = splice(fd_recv, NULL, fwd->pipe_fds[1], NULL, BUFFER_LEN, SPLICE_F_MOVE)) == -1) {
        LOG(ERROR, "splice() failed: %s", strerror(errno));
        return false;
    }


    if ((ret = splice(fwd->pipe_fds[0], NULL, fd_send, NULL, ret, SPLICE_F_MOVE)) == -1) {
        LOG(ERROR, "splice() failed: %s", strerror(errno));
        return false;
    }


    return true;
}

bool fwd_check_poll_connect(fwd_t *fwd, int i_fwd, struct pollfd *fd) {
    if (fd->revents & POLLIN) {
        fd->revents &= ~POLLIN;

        struct fwd *entry = &fwd->fwds[i_fwd];

        if (!entry->listener_connected)
            return true;

        if (!forward_packet(fwd, entry->connect_sock_fd, entry->listen_sock_fd)) {
            struct sockaddr_in addr = {
                .sin_family = AF_UNSPEC
            };

            if (connect(entry->listen_sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
                LOG(ERROR, "connect() failed: %s", strerror(errno));
                return false;
            }

            entry->listener_connected = false;
        }
    }

    return true;
}

bool fwd_check_poll_listen(fwd_t *fwd, int i_fwd, struct pollfd *fd) {
    if (fd->revents & POLLIN) {
        fd->revents &= ~POLLIN;

        struct fwd *entry = &fwd->fwds[i_fwd];

        if (!entry->listener_connected) {
            struct sockaddr_in addr;
            socklen_t addr_len = sizeof(addr);

            if (recvfrom(entry->listen_sock_fd, NULL, 0, MSG_PEEK, (struct sockaddr *)&addr, &addr_len) == -1) {
                LOG(ERROR, "recvfrom() failed: %s", strerror(errno));
                return false;
            }

            if (connect(entry->listen_sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
                LOG(ERROR, "connect() failed: %s", strerror(errno));
                return false;
            }

            entry->listener_connected = true;
        }

        return forward_packet(fwd, entry->listen_sock_fd, entry->connect_sock_fd);
    }

    return true;
}
