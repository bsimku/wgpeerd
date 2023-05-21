#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "wireguard.h"

#include "args.h"
#include "wgutil.h"
#include "server.h"
#include "net.h"
#include "log.h"
#include "packets.h"

typedef struct {
    wg_key public_key;
    wg_endpoint endpoint;
} peer_state;

#define MAX_PEERS 32

typedef struct {
    server_t *server;
    wg_device *device;
    peer_state peers[MAX_PEERS];
    size_t npeers;
} server_ctx;

static void send_endpoint_info(server_t *server, client_t *client, wg_peer *peer) {
    packet_t *packet = PACKET_NEW(ENDPOINT_INFO_RES);

    if (!packet)
        return;

    memcpy(packet->endpoint_info_res.public_key, peer->public_key, 32);

    packet->endpoint_info_res.addr = peer->endpoint.addr4.sin_addr.s_addr;
    packet->endpoint_info_res.port = peer->endpoint.addr4.sin_port;

    if (server_send_packet(server, client, packet) == -1)
        goto cleanup;

    {
        char addr[20];
        inet_ntop(AF_INET, &peer->endpoint.addr4.sin_addr, addr, 20);
        LOG(DEBUG, "%s:%d", addr, ntohs(peer->endpoint.addr4.sin_port));
    }

cleanup:
    free(packet);
}

static void handle_new_connection(client_t *client) {
    LOG(DEBUG, "new connection.");
}

static void handle_endpoint_info_request(server_ctx *ctx, client_t *client, packet_t *packet) {
    for (int i = 0; i < 32; i++) {
        printf("%x", packet->endpoint_info_req.public_key[i]);
    }

    wg_key_b64_string key;
    wg_key_to_base64(key, packet->endpoint_info_req.public_key);

    LOG(DEBUG, "key = %s", key);

    wg_peer *peer;

    wg_for_each_peer(ctx->device, peer) {
        if (memcmp(peer->public_key, packet->endpoint_info_req.public_key, 32) != 0)
            continue;

        send_endpoint_info(ctx->server, client, peer);
    }
}

static int handle_packet(server_ctx *ctx, client_t *client, packet_t *packet) {
    switch (packet->header.type) {
        default:
            LOG(DEBUG, "unknown packet type: 0x%x.", packet->header.type);
            break;
        case PACKET_TYPE_ENDPOINT_INFO_REQ:
            handle_endpoint_info_request(ctx, client, packet);
            break;
    }

    return 0;
}

static int handle_received_data(server_ctx *ctx, client_t *client) {
    LOG(DEBUG, "received data.");

    packet_t *packet;

    while (true) {
        if (server_read_packet(ctx->server, client, &packet) == -1)
            break;

        if (handle_packet(ctx, client, packet) == -1)
            return -1;
    }

    return 0;
}

static void check_endpoint_details(server_ctx *ctx) {
    wg_device *device = ctx->device;

    if (wg_get_device(&ctx->device, ctx->device->name) < 0) {
        LOG(ERROR, "failed to get device %s: %s.", ctx->device->name, strerror(errno));
        return;
    }

    wg_peer *p1, *p2;

    wg_for_each_peer(ctx->device, p1) {
        wg_for_each_peer(device, p2) {
            if (!wgutil_key_matches(p1->public_key, p2->public_key))
                continue;

            if (net_addr_and_port_matches(&p1->endpoint.addr4, &p2->endpoint.addr4))
                continue;

            for (size_t i = 0; i < ctx->server->nclients; i++) {
                send_endpoint_info(ctx->server, &ctx->server->clients[i], p1);
            }

            LOG(DEBUG, "peer endpoint details changed");
        }
    }

    wg_free_device(device);
}

static void handle_timeout(server_ctx *ctx) {
    check_endpoint_details(ctx);
}

static int server_loop(server_ctx *ctx) {
    LOG(DEBUG, "server_loop()");
    client_t *client;

    poll_status status = server_poll(ctx->server, &client);

    switch (status) {
        case POLL_ERROR:
            LOG(DEBUG, "poll error.");
            return -1;
        case POLL_NEW_CONNECTION:
            handle_new_connection(client);
            break;
        case POLL_DISCONNECT:
            LOG(DEBUG, "disconnect.");
            break;
        case POLL_RECEIVED_DATA: {
            handle_received_data(ctx, client);
            break;
        }
        case POLL_TIMEOUT:
            LOG(DEBUG, "timeout.");
            handle_timeout(ctx);
            break;
    }

    LOG(DEBUG, "server_loop() end");

    return 0;
}

int main(int argc, char *argv[]) {
    args_t args = args_get_defaults();

    if (args_parse(argc, argv, &args) == -1)
        return -1;

    LOG(DEBUG, "Interface: %s", args.interface);
    LOG(DEBUG, "Port: %d", args.port);

    const char *deviceName = wgutil_choose_device(args.interface);

    if (!deviceName) {
        LOG(ERROR, "No suitable wireguard device found.");
        return -2;
    }

    LOG(INFO, "Using device: %s", deviceName);

    wg_device *device;

    if (wg_get_device(&device, deviceName) < 0) {
        LOG(ERROR, "Failed to get device %s: %s.", deviceName, strerror(errno));
        return -3;
    }

    wg_key_b64_string key;
    wg_key_to_base64(key, device->public_key);

    LOG(DEBUG, "public_key = %s", key);

    int ret = 0;

    server_t *net = server_new();

    if (!net)
        return -4;

    if (server_init(net) == -1) {
        ret = -5;
        goto cleanup;
    }

    if (server_listen(net, args.port) == -1) {
        ret = -5;
        goto cleanup;
    }

    server_ctx ctx = {
        .server = net,
        .device = device,
        .npeers = 0
    };

    while (true) {
        if (server_loop(&ctx) == -1) {
            ret = -6;
            goto cleanup;
        }

    }

cleanup:
    server_close(net);

    wg_free_device(ctx.device);

    return ret;
}
