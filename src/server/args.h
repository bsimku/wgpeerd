#ifndef ARGS_H
#define ARGS_H

typedef struct {
    char *interface;
    unsigned short port;
} args_t;

args_t args_get_defaults();
int args_parse(int argc, char *argv[], args_t *args);

#endif
