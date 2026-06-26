#include <stdint.h>
#include <stddef.h>

#include "tessera/tessera.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    tsb_flatten(data, size);
    return 0;
}
