/* atoms.h - the interned blob pool.
 *
 * Atoms live in one contiguous, growable heap buffer. Each atom is stored
 * NUL-terminated and 4-byte aligned, so its in-pool footprint is
 * align4(len + 1). The pool is pre-sized at load time to hold every literal
 * atom; later interning (during a finisher) may grow it.
 */
#ifndef TESSERA_ATOMS_H
#define TESSERA_ATOMS_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t off;
    uint32_t len;
} AtomRef;

typedef struct {
    uint8_t *buf;
    size_t   used;
    size_t   cap;
    AtomRef *index;
    uint32_t count;
    uint32_t index_cap;
} Atoms;

int            atoms_load(Atoms *a, const uint8_t *data, size_t off, size_t len);
int            atoms_intern(Atoms *a, const uint8_t *bytes, uint32_t len); /* -> id or -1 */
const uint8_t *atoms_get(const Atoms *a, uint32_t id);
uint32_t       atoms_len(const Atoms *a, uint32_t id);
void           atoms_dispose(Atoms *a);

#endif /* TESSERA_ATOMS_H */
