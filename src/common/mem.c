#include "mem.h"

#include <errno.h>
#include <memory.h>
#include <stdlib.h>

#include "log.h"

void *mem_alloc(size_t size) {
    void *ptr = malloc(size);

    if (!ptr && size) {
        LOG(ERROR, "malloc() failed: %s", strerror(errno));
        abort();
    }

    return ptr;
}
