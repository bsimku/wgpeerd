#include <stdio.h>
#include <net/if.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <wireguard.h>

#include "wgutil.h"
#include "args.h"
#include "net.h"
#include "client.h"
#include "log.h"
#include "packets.h"

#define RECONNECT_INTERVAL 3 * 1000 * 1000

typedef struct {
    args_t *args;
    client_t *client;
    wg_device *device;
    struct sockaddr_in host;
} client_ctx;

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

static void update_endpoint(client_ctx *ctx, wg_key public_key, struct sockaddr_in *addr) {
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

static void handle_endpoint_info_res(client_ctx *ctx, packet_endpoint_info_res *packet) {
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

        wg_key_to_base64(key, ctx->device->public_key);
        LOG(DEBUG, "ctx->device->public_key = %s", key);

        wg_key_to_base64(key, packet->public_key);
        LOG(DEBUG, "packet->public_key = %s", key);

    }

    if (wgutil_key_matches(ctx->device->public_key, packet->public_key)) {
        LOG(DEBUG, "got host address.");

        wg_peer *peer;

        wg_for_each_peer(ctx->device, peer) {
            if (send_public_key(ctx->client, peer->public_key) == -1)
                return;
        }

        ctx->host = in;

        return;
    }

    update_endpoint(ctx, packet->public_key, &in);
}

static int client_loop(client_ctx *ctx) {
    packet_t *packet;

    if (client_read_packet(ctx->client, &packet) == -1)
        return -1;

    switch (packet->header.type) {
        default:
            LOG(DEBUG, "unknown packet type: 0x%x.", packet->header.type);
            break;
        case PACKET_TYPE_ENDPOINT_INFO_RES: {
            handle_endpoint_info_res(ctx, &packet->endpoint_info_res);
            break;
        }
    }

    return 0;
}

static void connect_loop(client_ctx *ctx) {
    if (client_init(ctx->client) == -1)
        return;

    if (client_connect(ctx->client, ctx->args->address, ctx->args->port) == -1)
        goto error;

    if (send_public_key(ctx->client, ctx->device->public_key) == -1)
        goto error;

    while (true) {
        if (client_loop(ctx) == -1)
            goto error;
    }

error:
    client_close(ctx->client);
}

int main(int argc, char *argv[]) {
    args_t args = args_get_defaults();

    if (args_parse(argc, argv, &args) == -1)
        return -1;

    LOG(DEBUG, "Interface: %s", args.interface);
    LOG(DEBUG, "Address: %s", args.address);
    LOG(DEBUG, "Port: %d", args.port);

    const char *deviceName = wgutil_choose_device(args.interface);

    if (!deviceName) {
        LOG(ERROR, "No suitable wireguard device found.");
        return 2;
    }

    LOG(INFO, "Using device: %s", deviceName);

    wg_device *device;

    if (wg_get_device(&device, deviceName) < 0) {
        LOG(ERROR, "Failed to get device %s: %s.", deviceName, strerror(errno));
        return 3;
    }

    wg_key_b64_string key;
    wg_key_to_base64(key, device->public_key);

    LOG(DEBUG, "public_key = %s", key);

    client_t *client = client_new();

    int ret = 0;

    if (!client) {
        ret = 4;
        goto cleanup;
    }

    client_ctx ctx = {
        .args = &args,
        .client = client,
        .device = device,
        .host.sin_port = 0
    };

    while (true) {
        connect_loop(&ctx);
        usleep(RECONNECT_INTERVAL);
    }

cleanup:
    if (client) {
        client_free(client);
    }

    return ret;
}
