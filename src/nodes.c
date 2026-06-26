#include "nodes.h"
#include "common.h"
#include "mem.h"

#include <stdlib.h>

void nodes_dispose(Node *nodes, uint32_t n) {
    if (!nodes) return;
    for (uint32_t i = 0; i < n; i++) {
        tsb_free(nodes[i].children);
        tsb_free(nodes[i].props);
    }
    tsb_free(nodes);
}

int nodes_load(Node **out, uint32_t *outn,
               const uint8_t *data, size_t off, size_t len,
               uint32_t atom_count, uint32_t value_count) {
    *out = NULL;
    *outn = 0;

    cur_t c;
    cur_init(&c, data, off, len);
    uint32_t n = cur_u32(&c);
    if (!cur_ok(&c)) return TSB_E_BOUNDS;
    if (n > NODES_MAX) return TSB_E_SECTION;

    Node *nodes = n ? tsb_calloc(n, sizeof(Node)) : NULL;
    if (n && !nodes) return TSB_E_NOMEM;

    int rc = TSB_OK;
    uint32_t i = 0;
    for (; i < n; i++) {
        Node *nd = &nodes[i];
        nd->kind = cur_u8(&c);
        nd->name_id = cur_u32(&c);
        uint16_t cc = cur_u16(&c);
        if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
        if (nd->name_id >= atom_count) { rc = TSB_E_NODE; goto fail; }

        if (cc) {
            nd->children = tsb_malloc((size_t)cc * sizeof(uint32_t));
            if (!nd->children) { rc = TSB_E_NOMEM; goto fail; }
            nd->child_count = cc;
            for (uint16_t k = 0; k < cc; k++) {
                uint32_t cid = cur_u32(&c);
                if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
                if (cid >= n) { rc = TSB_E_NODE; goto fail; }
                nd->children[k] = cid;
            }
        }

        uint16_t pc = cur_u16(&c);
        if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
        if (pc) {
            nd->props = tsb_malloc((size_t)pc * sizeof(Prop));
            if (!nd->props) { rc = TSB_E_NOMEM; goto fail; }
            nd->prop_count = pc;
            for (uint16_t k = 0; k < pc; k++) {
                uint32_t pn = cur_u32(&c);
                uint8_t  dt = cur_u8(&c);
                uint32_t vi = cur_u32(&c);
                if (!cur_ok(&c)) { rc = TSB_E_BOUNDS; goto fail; }
                if (pn >= atom_count) { rc = TSB_E_NODE; goto fail; }
                if (dt < VAL_MIN || dt > VAL_MAX) { rc = TSB_E_NODE; goto fail; }
                if (vi >= value_count) { rc = TSB_E_NODE; goto fail; }
                nd->props[k].name_atom = pn;
                nd->props[k].decl_type = dt;
                nd->props[k].val_index = vi;
            }
        }
    }

    *out = nodes;
    *outn = n;
    return TSB_OK;

fail:
    nodes_dispose(nodes, i + 1); /* dispose tolerates the zeroed tail */
    return rc;
}

int validate_bindings(const Node *nodes, uint32_t node_count,
                      const Value *values, uint32_t value_count) {
    for (uint32_t i = 0; i < node_count; i++) {
        const Node *nd = &nodes[i];
        for (uint16_t k = 0; k < nd->prop_count; k++) {
            uint32_t vi = nd->props[k].val_index;
            if (vi >= value_count) return TSB_E_BINDING;
            if (values[vi].kind != nd->props[k].decl_type) return TSB_E_BINDING;
        }
    }
    return TSB_OK;
}
