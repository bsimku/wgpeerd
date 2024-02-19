#include "args.h"

#include <stddef.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "memory.h"
#include "net.h"

const char *Usage =
    "[option...] address\n"
    "\n"
    "Options:\n"
    "  -h, --help                            show this help message and exit\n"
    "  -v, --verbose                         enable verbose logging\n"
    "  -i, --interface  <interface>          wireguard interface\n"
    "  -p, --port       <port>               port to connect\n"
    "  -w, --public-key <key>                public key of WireGuard device\n"
    "  -P, --peer       <peer,endpoint>      peer's default endpoint\n"
    "  -f, --forward    <port,peer,endpoint> forward peer's traffic\n";

const struct option c_long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"verbose", no_argument, NULL, 'v'},
    {"interface", required_argument, NULL, 'i'},
    {"public-key", required_argument, NULL, 'w'},
    {"peer", required_argument, NULL, 'P'},
    {"forward", required_argument, NULL, 'l'},
    {"port", required_argument, NULL, 'p'},
    {}
};


static void print_usage(const char *exec) {
    fprintf(stderr, "Usage: %s %s", exec, Usage);
}

args_t args_get_defaults() {
    args_t args = {
        .port = DEFAULT_PORT,
        .address = NULL,
        .interface = NULL,
        .public_key = NULL,
        .fwds = NULL,
        .nfwds = 0
    };

    return args;
}

bool parse_fwd(const char *fwd_str, args_fwd_t *fwd) {
    char *str = strdup(fwd_str);

    char *listen_port = strtok(str, ",");
    char *peer_key = strtok(NULL, ",");
    char *endpoint = strtok(NULL, ",");

    if (!listen_port || !peer_key || !endpoint)
        goto error;

    fwd->port  = atoi(listen_port);
    fwd->peer_key = strdup(peer_key);
    fwd->endpoint = strdup(endpoint);

    bool ret = true;

cleanup:
    free(str);

    return ret;
error:
    ret = false;
    goto cleanup;
}

bool parse_peer(const char *peer_str, args_peer_t *peer) {
    char *str = strdup(peer_str);

    fprintf(stderr, "str = '%s'\n", str);

    char *peer_key = strtok(str, ",");
    char *endpoint = strtok(NULL, ",");

    if (!peer_key || !endpoint)
        goto error;

    peer->public_key = strdup(peer_key);
    peer->endpoint = strdup(endpoint);

    bool ret = true;

cleanup:
    free(str);

    return ret;
error:
    ret = false;
    goto cleanup;

}

int args_parse(int argc, char *argv[], args_t *args) {
    int ch, option_index = 0;

    while ((ch = getopt_long(argc, argv, "hvi:P:w:f:p:", c_long_options, &option_index)) != -1) {
        if (ch == 'P') {
            args->npeers++;
        }
        else if (ch == 'f') {
            args->nfwds++;
        }
    }

    if (args->nfwds) {
        args->fwds = safe_alloc(args->nfwds * sizeof(args_fwd_t));
    }

    if (args->npeers) {
        args->peers = safe_alloc(args->npeers * sizeof(args_peer_t));
    }

    int fwd_idx = 0;
    int peer_idx = 0;

    optind = 1;

    while ((ch = getopt_long(argc, argv, "hvi:w:P:f:p:", c_long_options, &option_index)) != -1) {
        switch (ch) {
            default:
                print_usage(argv[0]);
                goto error;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            case 'v':
                g_log_level = DEBUG;
                break;
            case 'i':
                args->interface = optarg;
                break;
            case 'w':
                args->public_key = optarg;
                break;
            case 'P':
                if (!parse_peer(optarg, &args->peers[peer_idx++])) {
                    fprintf(stderr, "failed to parse peer string\n");
                    print_usage(argv[0]);
                    goto error;
                }

                break;
            case 'f':
                if (!parse_fwd(optarg, &args->fwds[fwd_idx++])) {
                    fprintf(stderr, "failed to parse forward string\n");
                    print_usage(argv[0]);
                    goto error;
                }

                break;
            case 'p':
                args->port = atoi(optarg);
                break;
        }
    }

    if (optind + 1 != argc) {
        print_usage(argv[0]);
        goto error;
    }

    args->address = argv[optind];

    return 0;

error:
    if (args->peers) {
        free(args->peers);
    }

    if (args->fwds) {
        free(args->fwds);
    }

    return -1;
}
