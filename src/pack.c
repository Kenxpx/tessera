/* pack.c - the pack finisher.
 *
 * "Pack" walks the ref op stream and exports the graph, optionally adopting the
 * atom pool into a value slot (OP_ADOPT) so a caller can take the interned blob
 * storage without a copy. Adoption transfers ownership of the pool buffer from
 * the Atoms struct to the value slot.
 *
 * Adoption inside an open scope is deferred to the matching OP_CLOSE_SCOPE, so
 * that an abandoned scope can leave the pool untouched; a top-level adoption
 * hands the buffer over immediately.
 */
#include "bundle.h"
#include "common.h"
#include "atoms.h"
#include "mem.h"

#include <stdlib.h>

typedef struct {
    uint32_t node;
    long     adopt_slot; /* value slot with a deferred adoption, or -1 */
} PFrame;

/* Detach the pool buffer from the Atoms struct: the new owner (a value slot)
 * keeps the pointer, so the bundle teardown must not free it again. */
static void pool_handoff(Bundle *b) {
    b->atoms.buf = NULL;
    b->atoms.cap = 0;
    b->atoms.used = 0;
}

int pack_run(Bundle *b) {
    PFrame *stack = NULL;
    uint32_t depth = 0, cap = 0;
    int rc = TSB_OK;

    for (uint32_t i = 0; i < b->ref_count; i++) {
        Ref *r = &b->refs[i];
        switch (r->op) {
        case OP_OPEN_SCOPE:
            if (depth == cap) {
                uint32_t nc = cap ? cap * 2 : 8;
                PFrame *ns = tsb_realloc(stack, (size_t)nc * sizeof(PFrame));
                if (!ns) { rc = TSB_E_NOMEM; goto done; }
                stack = ns; cap = nc;
            }
            stack[depth].node = r->a;
            stack[depth].adopt_slot = -1;
            depth++;
            break;

        case OP_CLOSE_SCOPE:
            if (depth == 0) break;
            depth--; /* pop the closing frame */
            /* commit a deferred adoption from the enclosing scope */
            if (depth > 0 && stack[depth - 1].adopt_slot >= 0)
                pool_handoff(b);
            break;

        case OP_ADOPT: {
            uint32_t a = r->a; /* value slot, range-checked at load */
            Value *v = &b->values[a];
            if (v->kind == VAL_BYTES && v->owns) tsb_free(v->u.bytes.ptr);
            else if (v->kind == VAL_LIST)        tsb_free(v->u.list.items);
            v->kind = VAL_BYTES;
            v->owns = 1;
            v->u.bytes.ptr = b->atoms.buf;
            v->u.bytes.len = (uint32_t)b->atoms.used;
            if (depth == 0) {
                pool_handoff(b);            /* top level: hand off now */
            } else {
                stack[depth - 1].adopt_slot = (long)a; /* defer to CLOSE_SCOPE */
            }
            break;
        }

        default:
            break; /* not pack's op family */
        }
    }

    /* Export size: the resolved length of every node name plus the adopted
     * blobs. Lengths come from the atom index, which survives a hand-off. */
    uint32_t total = 0;
    for (uint32_t i = 0; i < b->node_count; i++)
        total += atoms_len(&b->atoms, b->nodes[i].name_id);
    for (uint32_t i = 0; i < b->value_count; i++)
        if (b->values[i].kind == VAL_BYTES && b->values[i].owns)
            total += b->values[i].u.bytes.len;
    b->pack_export_len = total;

done:
    tsb_free(stack);
    return rc;
}
