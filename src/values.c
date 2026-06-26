#include "values.h"
#include "common.h"
#include "mem.h"

#include <stdlib.h>
#include <string.h>

void values_dispose(Value *v, uint32_t n) {
    if (!v) return;
    for (uint32_t i = 0; i < n; i++) {
        if (v[i].kind == VAL_BYTES && v[i].owns) tsb_free(v[i].u.bytes.ptr);
        if (v[i].kind == VAL_LIST) tsb_free(v[i].u.list.items);
    }
    tsb_free(v);
}

int decode_values(Value **out, uint32_t *outn,
                   const uint8_t *data, size_t off, size_t len,
                   const Atoms *atoms) {
    *out = NULL;
    *outn = 0;

    cur_t c;
    cur_init(&c, data, off, len);
    uint32_t n = cur_u32(&c);
    if (!cur_ok(&c)) return TSB_E_BOUNDS;
    if (n > VALUES_MAX) return TSB_E_SECTION;

    Value *vs = n ? tsb_calloc(n, sizeof(Value)) : NULL;
    if (n && !vs) return TSB_E_NOMEM;

    int rc = TSB_OK;
    uint32_t i = 0;
    for (; i < n; i++) {
        Value *v = &vs[i];
        uint8_t tag = cur_u8(&c);
        if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
        if (tag < VAL_MIN || tag > VAL_MAX) { rc = TSB_E_VALUE; goto fail; }
        v->kind = tag;

        switch (tag) {
        case VAL_INT:
            v->u.i64 = cur_i64(&c);
            if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
            break;
        case VAL_FLOAT:
            v->u.f64 = cur_f64(&c);
            if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
            break;
        case VAL_BYTES: {
            uint8_t sub = cur_u8(&c);
            if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
            if (sub == BYTES_INLINE) {
                uint32_t bl = cur_u32(&c);
                const uint8_t *p = cur_take(&c, bl);
                if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
                uint8_t *copy = tsb_malloc(bl ? bl : 1);
                if (!copy) { rc = TSB_E_NOMEM; goto fail; }
                if (bl) memcpy(copy, p, bl);
                v->owns = 1;
                v->u.bytes.ptr = copy;
                v->u.bytes.len = bl;
            } else if (sub == BYTES_ATOM) {
                uint32_t aid = cur_u32(&c);
                if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
                if (aid >= atoms->count) { rc = TSB_E_VALUE; goto fail; }
                v->owns = 0;
                v->u.bytes.ptr = (uint8_t *)atoms_get(atoms, aid);
                v->u.bytes.len = atoms_len(atoms, aid);
            } else {
                rc = TSB_E_VALUE;
                goto fail;
            }
            break;
        }
        case VAL_LIST: {
            uint32_t lc = cur_u32(&c);
            if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
            if (lc > LIST_MAX) { rc = TSB_E_VALUE; goto fail; }
            uint32_t *items = lc ? tsb_malloc((size_t)lc * sizeof(uint32_t)) : NULL;
            if (lc && !items) { rc = TSB_E_NOMEM; goto fail; }
            for (uint32_t k = 0; k < lc; k++) {
                uint32_t idx = cur_u32(&c);
                if (!cur_ok(&c) || idx >= n) {
                    tsb_free(items);
                    rc = TSB_E_VALUE;
                    goto fail;
                }
                items[k] = idx;
            }
            v->u.list.items = items;
            v->u.list.count = lc;
            break;
        }
        case VAL_REF: {
            uint32_t r = cur_u32(&c);
            if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
            v->u.ref = r; /* opaque until a BIND op consumes it */
            break;
        }
        case VAL_U128: {
            const uint8_t *p = cur_take(&c, 16);
            if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
            memcpy(v->u.u128, p, 16);
            break;
        }
        }
    }

    *out = vs;
    *outn = n;
    return TSB_OK;

fail:
    values_dispose(vs, i); /* slots [0,i) are fully built; slot i owns nothing */
    return rc;
}
