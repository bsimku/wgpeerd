#ifndef WGUTIL_H
#define WGUTIL_H

#include "wireguard.h"

char *wgutil_choose_device(const char *interface);
bool wgutil_key_matches(wg_key a, wg_key b);

#endif
