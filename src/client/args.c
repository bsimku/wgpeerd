#include "args.h"

#include <stddef.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "net.h"

const char *Usage =
    "[option...] address\n"
    "\n"
    "Options:\n"
    "  -h, --help       show this help message and exit\n"
    "  -v, --verbose    enable verbose logging\n"
    "  -i, --interface  wireguard interface\n"
    "  -p, --port       port to connect\n";

const struct option LongOptions[] = {
    {"help", no_argument, NULL, 'h'},
    {"verbose", no_argument, NULL, 'v'},
    {"interface", required_argument, NULL, 'i'},
    {"port", required_argument, NULL, 'p'},
    {}
};


void args_print_usage(const char *exec) {
    fprintf(stderr, "Usage: %s %s", exec, Usage);
}

args_t args_get_defaults() {
    args_t args = {
        .port = DEFAULT_PORT
    };

    return args;
}

int args_parse(int argc, char *argv[], args_t *args) {
    int ch, optionIndex = 0;

    while ((ch = getopt_long(argc, argv, "hvi:p:", LongOptions, &optionIndex)) != -1) {
        switch (ch) {
            default:
                args_print_usage(argv[0]);
                return -1;
            case 'h':
                args_print_usage(argv[0]);
                exit(0);
            case 'v':
                g_log_level = DEBUG;
                break;
            case 'i':
                args->interface = optarg; // TODO: BUG?
                break;
            case 'p':
                args->port = atoi(optarg);
                break;
        }
    }

    if (optind + 1 != argc) {
        args_print_usage(argv[0]);
        return -1;
    }

    args->address = argv[optind];

    return 0;
}
