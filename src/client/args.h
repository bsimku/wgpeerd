#ifndef ARGS_H
#define ARGS_H

typedef struct {
    unsigned short port;
    char *address;
    char *interface;
} args_t;

args_t args_get_defaults();
int args_parse(int argc, char *argv[], args_t *args);

#endif
