#ifndef ARGS_H
#define ARGS_H

typedef struct {
    char *peer_key;
    char *endpoint;
    unsigned short port;
} args_fwd_t;

typedef struct {
    unsigned short port;
    char *address;
    char *interface;
    char *public_key;
    args_fwd_t *fwds;
    int nfwds;
} args_t;

args_t args_get_defaults();
int args_parse(int argc, char *argv[], args_t *args);

#endif
