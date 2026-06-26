/* values.h - the tagged-union value stream.
 *
 * A value slot carries a kind (one of VAL_*) and, for VAL_BYTES, an `owns`
 * flag distinguishing a heap-copied blob from a borrowed view into the atom
 * pool. `decl_type` on a node property is checked against `kind` once, at load
 * time, by validate_bindings().
 */
#ifndef TESSERA_VALUES_H
#define TESSERA_VALUES_H

#include <stdint.h>
#include <stddef.h>

#include "atoms.h"

typedef struct Value {
    uint8_t kind;
    uint8_t owns;
    union {
        int64_t  i64;
        double   f64;
        struct { uint8_t *ptr; uint32_t len; } bytes;
        struct { uint32_t *items; uint32_t count; } list;
        uint32_t ref;
        uint8_t  u128[16];
    } u;
} Value;

int  decode_values(Value **out, uint32_t *outn,
                   const uint8_t *data, size_t off, size_t len,
                   const Atoms *atoms);
void values_dispose(Value *v, uint32_t n);

#endif /* TESSERA_VALUES_H */
