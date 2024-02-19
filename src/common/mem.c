#include "mem.h"

#include <memory.h>
#include <stdlib.h>

#include "log.h"

void *mem_alloc(size_t size) {
    void *ptr = malloc(size);

    if (!ptr && size) {
        LOG(ERROR, "failed to allocate memory.");
        abort();
    }

    return ptr;
}
