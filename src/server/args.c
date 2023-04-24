#include "args.h"

#include <stddef.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "net.h"

const char *Usage =
    "[option...]\n"
    "\n"
    "Options:\n"
    "  -h, --help       show this help message and exit\n"
    "  -v, --verbose    enable verbose logging\n"
    "  -i, --interface  wireguard interface\n"
    "  -p, --port       port to listen\n";

const struct option LongOptions[] = {
    {"help", no_argument, NULL, 'h'},
    {"verbose", no_argument, NULL, 'v'},
    {"interface", required_argument, NULL, 'i'},
    {"port", required_argument, NULL, 'p'},
    {}
};

args_t args_get_defaults() {
    args_t args = {
        .port = DEFAULT_PORT
    };

    return args;
}

static void print_usage(const char *exec) {
    fprintf(stderr, "Usage: %s %s", exec, Usage);
}

int args_parse(int argc, char *argv[], args_t *args) {
    int ch, optionIndex = 0;

    while ((ch = getopt_long(argc, argv, "hvi:p:", LongOptions, &optionIndex)) != -1) {
        switch (ch) {
            default:
                print_usage(argv[0]);
                return -1;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            case 'v':
                g_log_level = DEBUG;
                break;
            case 'i':
                args->interface = optarg;
                break;
            case 'p':
                args->port = atoi(optarg);
                break;
        }
    }

    return 0;
}

