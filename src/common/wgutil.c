#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "wireguard.h"

#include "log.h"
#include "mem.h"

char *wgutil_choose_device(const char *interface) {
    char *device_names = wg_list_device_names();

    if (!device_names) {
        LOG(ERROR, "wg_list_device_names() failed: %s", strerror(errno));
        return NULL;
    }

    char *device_name, *chosen_device = NULL;
    size_t len;

    wg_for_each_device_name(device_names, device_name, len) {
        if (interface && strcmp(device_name, interface) == 0 || !interface && !chosen_device) {
            if (chosen_device) {
                free(chosen_device);
            }

            chosen_device = mem_alloc(len + 1);
            strcpy(chosen_device, device_name);
        }
    }

    free(device_names);

    return chosen_device;
}

bool wgutil_key_matches(wg_key a, wg_key b) {
    return memcmp(a, b, 32) == 0;
}

bool wgutil_key_from_base64(wg_key key, const char *b64str) {
    const int ret = wg_key_from_base64(key, b64str);

    if (ret < 0) {
        LOG(ERROR, "failed to parse public key '%s': %s", b64str, strerror(ret));
        return false;
    }

    return true;
}
