/* materialize.c - the materialize finisher.
 *
 * "Materialize" applies the scoped property rebinds (OP_REBIND_PROP) and atom
 * bindings (OP_BIND_ATOM) and then emits every node's properties into a digest,
 * dispatching on each property's declared type.
 *
 * Property/slot type agreement is established once at load time by
 * validate_bindings(); OP_REBIND_PROP re-checks it when it points a property at
 * a different slot.
 */
#include "bundle.h"
#include "common.h"
#include "atoms.h"
#include "mem.h"

#include <string.h>
#include <stdlib.h>

#define FNV64_OFF 1469598103934665603ULL
#define FNV64_PRM 1099511628211ULL

int materialize_run(Bundle *b) {
    uint32_t *scope = NULL;
    uint32_t depth = 0, scap = 0;
    int rc = TSB_OK;

    for (uint32_t i = 0; i < b->ref_count; i++) {
        Ref *r = &b->refs[i];
        switch (r->op) {
        case OP_OPEN_SCOPE:
            if (depth == scap) {
                uint32_t nc = scap ? scap * 2 : 8;
                uint32_t *ns = tsb_realloc(scope, (size_t)nc * sizeof(uint32_t));
                if (!ns) { rc = TSB_E_NOMEM; goto done; }
                scope = ns; scap = nc;
            }
            scope[depth++] = r->a;
            break;

        case OP_CLOSE_SCOPE:
            if (depth > 0) depth--;
            break;

        case OP_REBIND_PROP: {
            if (depth == 0) { rc = TSB_E_SEMANTIC; goto done; }
            Node *nd = &b->nodes[scope[depth - 1]];
            uint32_t sel = r->a;        /* scope-relative prop selector */
            uint32_t slot = r->b;       /* new value slot, checked at load */
            if (sel >= nd->prop_count) { rc = TSB_E_SEMANTIC; goto done; }
            /* A rebind must keep the property's declared type. */
            if (b->values[slot].kind != nd->props[sel].decl_type) {
                rc = TSB_E_BINDING; goto done;
            }
            nd->props[sel].val_index = slot;
            break;
        }

        case OP_BIND_ATOM: {
            uint32_t slot = r->a;       /* value slot, checked at load */
            uint32_t atom = r->b;       /* atom id, checked at load   */
            Value *v = &b->values[slot];
            if (v->kind == VAL_BYTES && v->owns) tsb_free(v->u.bytes.ptr);
            else if (v->kind == VAL_LIST)        tsb_free(v->u.list.items);
            v->kind = VAL_BYTES;
            v->owns = 0;
            v->u.bytes.ptr = (uint8_t *)atoms_get(&b->atoms, atom);
            v->u.bytes.len = atoms_len(&b->atoms, atom);
            break;
        }

        default:
            break; /* not materialize's op family */
        }
    }

    /* Emit: fold each property into the digest by its declared type. */
    uint64_t digest = FNV64_OFF;
    for (uint32_t i = 0; i < b->node_count; i++) {
        Node *nd = &b->nodes[i];
        for (uint16_t k = 0; k < nd->prop_count; k++) {
            const Value *v = &b->values[nd->props[k].val_index];
            switch (nd->props[k].decl_type) {
            case VAL_INT:
                digest = (digest ^ (uint64_t)v->u.i64) * FNV64_PRM;
                break;
            case VAL_FLOAT: {
                uint64_t bits;
                memcpy(&bits, &v->u.f64, 8);
                digest = (digest ^ bits) * FNV64_PRM;
                break;
            }
            case VAL_BYTES:
                for (uint32_t j = 0; j < v->u.bytes.len; j++) {
                    digest ^= v->u.bytes.ptr[j];
                    digest *= FNV64_PRM;
                }
                break;
            case VAL_LIST:
                for (uint32_t j = 0; j < v->u.list.count; j++)
                    digest = (digest ^ v->u.list.items[j]) * FNV64_PRM;
                break;
            case VAL_REF:
                digest = (digest ^ v->u.ref) * FNV64_PRM;
                break;
            case VAL_U128:
                for (int j = 0; j < 16; j++) {
                    digest ^= v->u.u128[j];
                    digest *= FNV64_PRM;
                }
                break;
            }
        }
    }
    b->mat_digest = digest;

done:
    tsb_free(scope);
    return rc;
}
