#ifndef FWD_H
#define FWD_H

#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>

#include "wireguard.h"

typedef struct {
    struct sockaddr_in host;
    int bind_port;
    int pipe_fds[2];
    struct fwd {
        struct sockaddr_in default_endpoint;
        struct sockaddr_in curr_endpoint;
        wg_key peer_key;
        int listen_sock_fd;
        int connect_sock_fd;
        bool listener_connected;
    } *fwds;
    int nfwds;
} fwd_t;

bool fwd_init(fwd_t *fwd, int bind_port, int nfwds);
bool fwd_add(fwd_t *fwd, int i_fwd, const char *peer_key, const char *endpoint, unsigned short listen_port);
void fwd_set_endpoint(fwd_t *fwd, int i_fwd, struct sockaddr_in *addr);
void fwd_setup_poll_connect(fwd_t *fwd, int i_fwd, struct pollfd *fd);
void fwd_setup_poll_listen(fwd_t *fwd, int i_fwd, struct pollfd *fd);
bool fwd_check_poll_connect(fwd_t *fwd, int i_fwd, struct pollfd *fd);
bool fwd_check_poll_listen(fwd_t *fwd, int i_fwd, struct pollfd *fd);

#endif
