#ifndef PACKETS_H
#define PACKETS_H

#include "wireguard.h"

#define PACKET_ATTR __attribute__((__packed__)) 

#define PROTOCOL_VERSION 1

#define PACKET_TYPE_ENDPOINT_INFO_REQ 0x3E
#define PACKET_TYPE_ENDPOINT_INFO_RES 0x3F

typedef struct PACKET_ATTR {
    uint16_t version;
    uint16_t type;
    uint32_t size;
} packet_header;

typedef struct PACKET_ATTR {
    wg_key public_key;
} packet_endpoint_info_req;

typedef struct PACKET_ATTR {
    wg_key public_key;
    unsigned long addr;
    unsigned short port;
} packet_endpoint_info_res;

typedef struct PACKET_ATTR {
    packet_header header;

    union {
        void *data;
        packet_endpoint_info_req endpoint_info_req;
        packet_endpoint_info_res endpoint_info_res;
    };
} packet_t;

packet_t *packet_allocate(const uint16_t type);
uint32_t packet_get_size(const uint16_t type);

#define PACKET_NEW(packet) packet_allocate(PACKET_TYPE_##packet)

int packet_parse_header(packet_header *header);

#endif
