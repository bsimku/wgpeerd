#include <netinet/in.h>
#include <stdio.h>
#include <net/if.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#include <wireguard.h>

#include "fwd.h"
#include "memory.h"
#include "socket.h"
#include "wgutil.h"
#include "args.h"
#include "net.h"
#include "client.h"
#include "log.h"
#include "packets.h"

#define RECONNECT_INTERVAL 5

#define POLL_TIMEOUT 5000
#define POLL_FD_IDX_CLIENT 0
#define POLL_FD_IDX_FWD_CONNECT 1
#define POLL_FD_IDX_FWD_LISTEN_BASE 2

typedef struct {
    args_t *args;
    client_t *client;
    wg_device *device;
    wg_key public_key;
    struct sockaddr_in host;
    nfds_t nfds;
    struct pollfd *fds;
    bool fwd_mode;
    fwd_t fwd;
} client_ctx_t;

static int send_public_key(client_t *client, wg_key key) {
    packet_t *packet = PACKET_NEW(ENDPOINT_INFO_REQ);

    if (!packet)
        return -1;

    memcpy(packet->endpoint_info_req.public_key, key, 32);

    wg_key_b64_string b64;
    wg_key_to_base64(b64, packet->endpoint_info_req.public_key);

    LOG(DEBUG, "public_key = %s", b64);

    int ret = 0;

    if (client_send_packet(client, packet) == -1) {
        ret = -1;
        goto cleanup;
    }

cleanup:
    free(packet);
    return ret;
}

static void update_endpoint_fwd(client_ctx_t *ctx, wg_key public_key, struct sockaddr_in *addr) {
    for (int i = 0; i < ctx->fwd.nfwds; i++) {
        if (wgutil_key_matches(ctx->fwd.fwds[i].peer_key, public_key)) {
            if (net_addr_matches(addr, &ctx->host)) {
                fwd_set_endpoint(&ctx->fwd, i, &ctx->fwd.fwds[i].default_endpoint);
            }
            else if (net_addr_and_port_matches(&ctx->fwd.fwds[i].curr_endpoint, addr)) {
                LOG(DEBUG, "peer endpoint address matches, skipping..");
            }
            else {
                fwd_set_endpoint(&ctx->fwd, i, addr);
            }

            return;
        }
    }
}

static void update_endpoint(client_ctx_t *ctx, wg_key public_key, struct sockaddr_in *addr) {
    wg_device *device = ctx->device;

    if (wg_get_device(&ctx->device, device->name) < 0) {
        LOG(ERROR, "failed to get device %s: %s.", ctx->device->name, strerror(errno));
        return;
    }

    wg_free_device(device);

    wg_peer *peer;

    wg_for_each_peer(ctx->device, peer) {
        if (!wgutil_key_matches(peer->public_key, public_key))
            continue;

        if (net_addr_matches(addr, &ctx->host)) {
            LOG(DEBUG, "peer and host address match, skipping..");
            return;
        }

        if (net_addr_and_port_matches(&peer->endpoint.addr4, addr)) {
            LOG(DEBUG, "peer endpoint address matches, skipping..");
            return;
        }

        LOG(DEBUG, "found peer, updating.");

        {
            char addr[20];
            inet_ntop(AF_INET, &peer->endpoint.addr4.sin_addr, addr, 20);
            LOG(DEBUG, "%s:%d", addr, ntohs(peer->endpoint.addr4.sin_port));
        }

        peer->endpoint.addr4 = *addr;

        {
            char addr[20];
            inet_ntop(AF_INET, &peer->endpoint.addr4.sin_addr, addr, 20);
            LOG(DEBUG, "%s:%d", addr, ntohs(peer->endpoint.addr4.sin_port));
        }

        wg_set_device(ctx->device);

        return;
    }
}

static void handle_endpoint_info_res(client_ctx_t *ctx, packet_endpoint_info_res *packet) {
    LOG(DEBUG, "PACKET_TYPE_ENDPOINT_INFO_RES");

    struct sockaddr_in in = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = packet->addr,
        .sin_port = packet->port
    };

    {
        char addr[20];
        inet_ntop(AF_INET, &in.sin_addr, addr, 20);
        LOG(DEBUG, "%s:%d", addr, ntohs(in.sin_port));
    }

    {
        wg_key_b64_string key;

        wg_key_to_base64(key, ctx->public_key);
        LOG(DEBUG, "ctx->device->public_key = %s", key);

        wg_key_to_base64(key, packet->public_key);
        LOG(DEBUG, "packet->public_key = %s", key);

    }

    if (wgutil_key_matches(ctx->public_key, packet->public_key)) {
        LOG(DEBUG, "got host address.");

        ctx->host = in;

        if (ctx->fwd_mode) {
            for (int i = 0; i < ctx->fwd.nfwds; i++) {
                if (send_public_key(ctx->client, ctx->fwd.fwds[i].peer_key) == -1)
                    return;
            }
        }
        else {
            wg_peer *peer;

            wg_for_each_peer(ctx->device, peer) {
                if (send_public_key(ctx->client, peer->public_key) == -1)
                    return;
            }
        }

        return;
    }

    if (ctx->fwd_mode) {
        update_endpoint_fwd(ctx, packet->public_key, &in);
    }
    else {
        update_endpoint(ctx, packet->public_key, &in);
    }
}

static bool client_try_connect(client_ctx_t *ctx) {
    if (ctx->client->connect_failed) {
        if (time(NULL) - ctx->client->last_conn < RECONNECT_INTERVAL)
            return false;

        client_close(ctx->client);

        if (client_init(ctx->client) == -1)
            return false;

        client_setup_poll(ctx->client, &ctx->fds[POLL_FD_IDX_CLIENT]);
    }

    if (client_connect(ctx->client, ctx->args->address, ctx->args->port) == -1)
        return false;

    if (send_public_key(ctx->client, ctx->public_key) == -1)
        return false;

    return true;
}

static bool handle_client_received_packet(client_ctx_t *ctx) {
    packet_t *packet;

    if (client_read_packet(ctx->client, &packet) == -1)
        return false;

    switch (packet->header.type) {
        default:
            LOG(DEBUG, "unknown packet type: 0x%x.", packet->header.type);
            break;
        case PACKET_TYPE_ENDPOINT_INFO_RES: {
            handle_endpoint_info_res(ctx, &packet->endpoint_info_res);
            break;
        }
    }

    return true;
}

int main(int argc, char *argv[]) {
    args_t args = args_get_defaults();

    if (args_parse(argc, argv, &args) == -1)
        return -1;

    LOG(DEBUG, "Interface: %s", args.interface);
    LOG(DEBUG, "Address: %s", args.address);
    LOG(DEBUG, "Port: %d", args.port);

    int ret = 0;

    client_t *client = client_new();

    client_ctx_t ctx = {
        .args = &args,
        .client = client,
        .device = NULL,
        .host.sin_port = 0,
        .fwd_mode = args.nfwds
    };

    if (ctx.fwd_mode) {
        if (!wgutil_key_from_base64(ctx.public_key, args.public_key))
            goto error;

        if (!fwd_init(&ctx.fwd, args.nfwds))
            goto error;

        for (int i = 0; i < args.nfwds; i++) {
            fwd_add(&ctx.fwd, i, args.fwds[i].peer_key, args.fwds[i].endpoint, args.fwds[i].port);
        }
    }
    else {
        const char *device_name = wgutil_choose_device(args.interface);

        if (!device_name) {
            LOG(ERROR, "No suitable wireguard device found.");
            goto error;
        }

        LOG(INFO, "Using device: %s", device_name);

        if (wg_get_device(&ctx.device, device_name) < 0) {
            LOG(ERROR, "Failed to get device %s: %s.", device_name, strerror(errno));
            goto error;
        }

        memcpy(ctx.public_key, ctx.device->public_key, sizeof(wg_key));
    }

    if (ctx.fwd_mode) {
        ctx.nfds = 2 + args.nfwds;
    }
    else {
        ctx.nfds = 1;
    }

    ctx.fds = safe_alloc(ctx.nfds * sizeof(struct pollfd));

    if (client_init(client) == -1)
        goto error;

    client_setup_poll(client, &ctx.fds[POLL_FD_IDX_CLIENT]);

    if (ctx.fwd_mode) {
        fwd_setup_poll_connect(&ctx.fwd, &ctx.fds[POLL_FD_IDX_FWD_CONNECT]);

        for (int i = 0; i < args.nfwds; i++) {
            fwd_setup_poll_listen(&ctx.fwd, i, &ctx.fds[POLL_FD_IDX_FWD_LISTEN_BASE + i]);
        }
    }

    client_try_connect(&ctx);

    while (true) {
        int ret;

        if (client->connect_failed) {
            ret = poll(ctx.fds + 1, ctx.nfds - 1, POLL_TIMEOUT);
        }
        else {
            ret = poll(ctx.fds, ctx.nfds, POLL_TIMEOUT);
        }

        if (ret == -1) {
            LOG(ERROR, "poll() failed: %s", strerror(errno));
            goto error;
        }
        else if (ret == 0)
            continue;

        const int status = client_check_poll(client, &ctx.fds[POLL_FD_IDX_CLIENT]);

        if (status == CLIENT_RECEIVED_PACKET) {
            handle_client_received_packet(&ctx);
        }

        if (!ctx.fwd_mode)
            continue;

        if (!fwd_check_poll_connect(&ctx.fwd, &ctx.fds[POLL_FD_IDX_FWD_CONNECT]))
            goto error;

        for (int i = 0; i < args.nfwds; i++) {
            struct pollfd *fd = &ctx.fds[POLL_FD_IDX_FWD_LISTEN_BASE + i];

            if (!fwd_check_poll_listen(&ctx.fwd, i, fd))
                goto error;
        }

        if (ctx.client->connect_failed) {
            client_try_connect(&ctx);
        }
    }

cleanup:
    free(ctx.fds);

    if (client) {
        client_free(client);
    }

    return ret;

error:
    ret = 1;
    goto cleanup;
}
