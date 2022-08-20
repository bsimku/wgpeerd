#include "packets.h"

#include <stdlib.h>
#include <stdio.h>

#include "log.h"

packet_t *packet_allocate(const uint16_t type) {
    packet_t *packet = malloc(sizeof(packet_t));

    if (!packet) {
        LOG(ERROR, "failed to allocate packet.");
        return NULL;
    }

    packet->header.version = PROTOCOL_VERSION;
    packet->header.type = type;
    packet->header.size = packet_get_size(type);

    return packet;
}

uint32_t packet_get_size(const uint16_t type) {
    switch (type) {
        case PACKET_TYPE_ENDPOINT_INFO_REQ:
            return sizeof(packet_endpoint_info_req);
        case PACKET_TYPE_ENDPOINT_INFO_RES:
            return sizeof(packet_endpoint_info_res);
    }

    return 0;
}


int packet_parse_header(packet_header *header) {
    header->version = ntohs(header->version);
    header->type = ntohs(header->type);
    header->size = ntohl(header->size);

    if (header->version != PROTOCOL_VERSION) {
        LOG(ERROR, "error parsing packet: protocol mismatch.");
        return -1;
    }

    LOG(DEBUG, "type = %x", header->type);
    LOG(DEBUG, "size = %d", header->size);

    if (packet_get_size(header->type) != header->size) {
        LOG(ERROR, "error parsing packet: size mismatch.");
        return -1;
    }

    return 0;
}
