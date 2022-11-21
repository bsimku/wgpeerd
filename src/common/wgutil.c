#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "wireguard.h"

#include "log.h"
#include "memory.h"

char *wgutil_choose_device(const char *interface) {
    char *deviceNames = wg_list_device_names();

    if (!deviceNames) {
        LOG(ERROR, "wg_list_device_names() failed: %s", strerror(errno));
        return NULL;
    }

    char *deviceName, *chosenDevice = NULL;
    size_t len;

    wg_for_each_device_name(deviceNames, deviceName, len) {
        if (interface && strcmp(deviceName, interface) == 0 || !interface && !chosenDevice) {
            if (chosenDevice) {
                free(chosenDevice);
            }

            chosenDevice = safe_alloc(len + 1);
            strcpy(chosenDevice, deviceName);
        }
    }

    free(deviceNames);

    return chosenDevice;
}

bool wgutil_key_matches(wg_key a, wg_key b) {
    return memcmp(a, b, 32) == 0;
}
