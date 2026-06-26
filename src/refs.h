/* refs.h - the pass-2 fixup/op list.
 *
 * refs_load() reads and range-checks the op stream but does not apply it;
 * each finisher applies only the op family it understands. The generic fields
 * a/b/c carry op-specific ids (their meaning depends on `op`); `data` holds the
 * literal bytes for OP_SYNTH.
 */
#ifndef TESSERA_REFS_H
#define TESSERA_REFS_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t  op;
    uint32_t a, b, c;
    uint8_t *data;
    uint32_t data_len;
} Ref;

int  refs_load(Ref **out, uint32_t *outn,
               const uint8_t *data, size_t off, size_t len,
               uint32_t node_count, uint32_t value_count, uint32_t atom_count);
void refs_dispose(Ref *refs, uint32_t n);

#endif /* TESSERA_REFS_H */
