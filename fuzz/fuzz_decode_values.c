#include <stdint.h>
#include <stddef.h>

#include "tessera/tessera.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    tsb_decode_values(data, size);
    return 0;
}
