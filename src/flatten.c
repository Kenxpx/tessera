/* flatten.c - the flatten finisher.
 *
 * "Flatten" resolves the graph's synthesized atoms into the pool and then folds
 * every byte-valued slot into a single digest - the flattened image of the
 * bundle. It is the read-only export used by callers that only want a content
 * hash of a loaded bundle.
 *
 * Two passes:
 *   A. Apply the synthesis op family (OP_SYNTH, OP_CONCAT). Each interns a new
 *      atom and renames the target node to it.
 *   B. Re-resolve node names and fold the byte slots into the digest.
 *
 * Byte slots cache their resolved view at decode time (Value.u.bytes.ptr) so
 * the fold is a tight loop over a ready pointer rather than an id lookup per
 * slot.
 */
#include "bundle.h"
#include "common.h"
#include "atoms.h"
#include "mem.h"

#include <string.h>
#ifdef TSB_VERIFY
#include <stdio.h>
#include <stdlib.h>
#endif

#define FNV64_OFF 1469598103934665603ULL
#define FNV64_PRM 1099511628211ULL

/* intern atom_b ++ atom_c as a fresh atom; returns the new id or -1 */
static int intern_concat(Atoms *a, uint32_t b, uint32_t c) {
    uint32_t lb = atoms_len(a, b), lc = atoms_len(a, c);
    uint64_t tot = (uint64_t)lb + lc;
    if (tot > 0xffffffffULL) return -1;
    uint8_t *tmp = tsb_malloc((size_t)tot ? (size_t)tot : 1);
    if (!tmp) return -1;
    if (lb) memcpy(tmp, atoms_get(a, b), lb);
    if (lc) memcpy(tmp + lb, atoms_get(a, c), lc);
    int id = atoms_intern(a, tmp, (uint32_t)tot);
    tsb_free(tmp);
    return id;
}

int flatten_run(Bundle *b) {
    /* Pass A: synthesize. Interning may grow the pool past its load-time
     * capacity, which relocates the backing buffer. */
    for (uint32_t i = 0; i < b->ref_count; i++) {
        Ref *r = &b->refs[i];
        if (r->op == OP_SYNTH) {
            int id = atoms_intern(&b->atoms, r->data, r->data_len);
            if (id < 0) return TSB_E_NOMEM;
            b->nodes[r->a].name_id = (uint32_t)id;
        } else if (r->op == OP_CONCAT) {
            int id = intern_concat(&b->atoms, r->b, r->c);
            if (id < 0) return TSB_E_NOMEM;
            b->nodes[r->a].name_id = (uint32_t)id;
        }
    }

    /* Pass B: node names are re-resolved against the current pool, so they are
     * always valid. */
    for (uint32_t i = 0; i < b->node_count; i++)
        b->nodes[i].name = atoms_get(&b->atoms, b->nodes[i].name_id);

    /* Fold every byte slot into the digest, reading through the view cached at
     * decode time. */
    uint64_t digest = FNV64_OFF;
    for (uint32_t i = 0; i < b->value_count; i++) {
        Value *v = &b->values[i];
        if (v->kind != VAL_BYTES) continue;
        const uint8_t *p = v->u.bytes.ptr;
        uint32_t n = v->u.bytes.len;
        for (uint32_t k = 0; k < n; k++) {
            digest ^= p[k];
            digest *= FNV64_PRM;
        }
    }

    b->flat_digest = digest;
    return TSB_OK;
}
