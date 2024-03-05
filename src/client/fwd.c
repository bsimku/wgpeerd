#include "fwd.h"

#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

#include "mem.h"
#include "net.h"
#include "wgutil.h"
#include "socket.h"
#include "log.h"

#define BUFFER_LEN 4096

bool fwd_init(fwd_t *fwd, int nfwds) {
    fwd->fwds = mem_alloc(nfwds * sizeof(struct fwd));
    fwd->nfwds = nfwds;

    if ((fwd->connect_sock_fd = socket_create_udp()) == -1)
        return false;

    return true;
}

bool fwd_add(fwd_t *fwd, int i_fwd, const char *peer_key, const char *endpoint, unsigned short listen_port) {
    struct fwd *entry = &fwd->fwds[i_fwd];

    if (!wgutil_key_from_base64(entry->peer_key, peer_key))
        return false;

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

    entry->curr_endpoint = entry->default_endpoint;

    return true;
}

void fwd_set_endpoint(fwd_t *fwd, int i_fwd, struct sockaddr_in *addr) {
    if (net_addr_and_port_matches(&fwd->fwds[i_fwd].curr_endpoint, addr))
        return;

    char old_addr[ADDR_MAX_LEN], new_addr[ADDR_MAX_LEN];

    LOG(INFO, "%s:%d -> %s:%d", net_addr_to_str(&fwd->fwds[i_fwd].curr_endpoint, old_addr),
                                ntohs(fwd->fwds[i_fwd].curr_endpoint.sin_port),
                                net_addr_to_str(addr, new_addr),
                                ntohs(addr->sin_port));

    fwd->fwds[i_fwd].curr_endpoint = *addr;

}

void fwd_setup_poll_connect(fwd_t *fwd, struct pollfd *fd) {
    fd->fd = fwd->connect_sock_fd;
    fd->events = POLLIN;
    fd->revents = 0;
}

void fwd_setup_poll_listen(fwd_t *fwd, int i_fwd, struct pollfd *fd) {
    fd->fd = fwd->fwds[i_fwd].listen_sock_fd;
    fd->events = POLLIN;
    fd->revents = 0;
}

static bool forward_packet(fwd_t *fwd, int fd_recv, int fd_send, struct sockaddr_in *dest_addr, struct sockaddr_in *raddr) {
    char buffer[BUFFER_LEN];

    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    const ssize_t sz = recvfrom(fd_recv, buffer, BUFFER_LEN, 0, (struct sockaddr *)&addr, &addr_len);

    if (sz == -1) {
        LOG(ERROR, "recvfrom() failed: %s", strerror(errno));
        return false;
    }

    if (fd_send == -1) {
        for (int i = 0; i < fwd->nfwds; i++) {
            if (net_addr_and_port_matches(&addr, &fwd->fwds[i].curr_endpoint)) {
                fd_send = fwd->fwds[i].listen_sock_fd;
                break;
            }
        }

        if (fd_send == -1) {
            char buf[ADDR_MAX_LEN];

            LOG(DEBUG, "failed to forward packet from %s:%d", net_addr_to_str(&addr, buf),
                                                              ntohs(addr.sin_port));

            return true;
        }
    }

    const ssize_t ret = sendto(fd_send, buffer, sz, 0, (struct sockaddr *)dest_addr, sizeof(*dest_addr));

    if (ret == -1) {
        LOG(ERROR, "sendto() failed: %s", strerror(errno));
    }

    if (raddr) {
        *raddr = addr;
    }

    return true;
}

bool fwd_check_poll_connect(fwd_t *fwd, struct pollfd *fd) {
    if (fd->revents & POLLIN) {
        fd->revents &= ~POLLIN;

        return forward_packet(fwd, fwd->connect_sock_fd, -1, &fwd->host, NULL);
    }

    return true;
}

bool fwd_check_poll_listen(fwd_t *fwd, int i_fwd, struct pollfd *fd) {
    if (fd->revents & POLLIN) {
        fd->revents &= ~POLLIN;

        struct fwd *entry = &fwd->fwds[i_fwd];

        return forward_packet(fwd, entry->listen_sock_fd,
                              fwd->connect_sock_fd, &entry->curr_endpoint, &fwd->host);
    }

    return true;
}
