#include "refs.h"
#include "common.h"
#include "mem.h"

#include <stdlib.h>
#include <string.h>

void refs_dispose(Ref *refs, uint32_t n) {
    if (!refs) return;
    for (uint32_t i = 0; i < n; i++) tsb_free(refs[i].data);
    tsb_free(refs);
}

int refs_load(Ref **out, uint32_t *outn,
              const uint8_t *data, size_t off, size_t len,
              uint32_t node_count, uint32_t value_count, uint32_t atom_count) {
    *out = NULL;
    *outn = 0;

    cur_t c;
    cur_init(&c, data, off, len);
    uint32_t n = cur_u32(&c);
    if (!cur_ok(&c)) return TSB_E_BOUNDS;
    if (n > REFS_MAX) return TSB_E_SECTION;

    Ref *refs = n ? tsb_calloc(n, sizeof(Ref)) : NULL;
    if (n && !refs) return TSB_E_NOMEM;

    int rc = TSB_OK;
    uint32_t i = 0;
    for (; i < n; i++) {
        Ref *r = &refs[i];
        uint8_t op = cur_u8(&c);
        if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
        if (op < OP_MIN || op > OP_MAX) { rc = TSB_E_REF; goto fail; }
        r->op = op;

        switch (op) {
        case OP_LINK:
            r->a = cur_u32(&c);
            r->b = cur_u32(&c);
            if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
            if (r->a >= node_count || r->b >= node_count) { rc = TSB_E_REF; goto fail; }
            break;
        case OP_SYNTH: {
            r->a = cur_u32(&c);
            uint32_t dl = cur_u32(&c);
            const uint8_t *p = cur_take(&c, dl);
            if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
            if (r->a >= node_count) { rc = TSB_E_REF; goto fail; }
            if (dl > SYNTH_MAX) { rc = TSB_E_REF; goto fail; }
            r->data = tsb_malloc(dl ? dl : 1);
            if (!r->data) { rc = TSB_E_NOMEM; goto fail; }
            if (dl) memcpy(r->data, p, dl);
            r->data_len = dl;
            break;
        }
        case OP_CONCAT:
            r->a = cur_u32(&c);
            r->b = cur_u32(&c);
            r->c = cur_u32(&c);
            if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
            if (r->a >= node_count) { rc = TSB_E_REF; goto fail; }
            if (r->b >= atom_count || r->c >= atom_count) { rc = TSB_E_REF; goto fail; }
            break;
        case OP_OPEN_SCOPE:
            r->a = cur_u32(&c);
            if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
            if (r->a >= node_count) { rc = TSB_E_REF; goto fail; }
            break;
        case OP_CLOSE_SCOPE:
            break;
        case OP_REBIND_PROP:
            r->a = cur_u32(&c); /* scope-relative prop selector */
            r->b = cur_u32(&c); /* new value slot */
            if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
            if (r->b >= value_count) { rc = TSB_E_REF; goto fail; }
            break;
        case OP_BIND_ATOM:
            r->a = cur_u32(&c); /* value slot to promote */
            r->b = cur_u32(&c); /* atom to bind */
            if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
            if (r->a >= value_count || r->b >= atom_count) { rc = TSB_E_REF; goto fail; }
            break;
        case OP_ADOPT:
            r->a = cur_u32(&c); /* value slot that adopts the pool */
            if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
            if (r->a >= value_count) { rc = TSB_E_REF; goto fail; }
            break;
        }
    }

    *out = refs;
    *outn = n;
    return TSB_OK;

fail:
    refs_dispose(refs, i + 1);
    return rc;
}
