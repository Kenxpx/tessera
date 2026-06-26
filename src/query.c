/* query.c - the resident-bundle read API (see include/tessera/query.h).
 *
 * A TsbBundle is just an owned Bundle plus the load pipeline run with the graph
 * sections. All accessors are bounds-checked reads against it; none mutate, so
 * the cached byte views established at decode time stay valid for the handle's
 * lifetime (no finisher runs to relocate the atom pool).
 */
#include "tessera/query.h"
#include "tessera/tessera.h"
#include "tessera/format.h"
#include "bundle.h"
#include "atoms.h"
#include "mem.h"

#include <string.h>

struct TsbBundle {
    Bundle b;
};

int tsb_bundle_open(const uint8_t *data, size_t size, TsbBundle **out) {
    *out = NULL;
    TsbBundle *h = tsb_calloc(1, sizeof(*h));
    if (!h) return TSB_E_NOMEM;
    int rc = load_core(&h->b, data, size, 1);
    if (rc != TSB_OK) {
        bundle_free(&h->b);
        tsb_free(h);
        return rc;
    }
    *out = h;
    return TSB_OK;
}

void tsb_bundle_close(TsbBundle *h) {
    if (!h) return;
    bundle_free(&h->b);
    tsb_free(h);
}

uint32_t tsb_bundle_atom_count(const TsbBundle *h)  { return h->b.atoms.count; }
uint32_t tsb_bundle_value_count(const TsbBundle *h) { return h->b.value_count; }
uint32_t tsb_bundle_node_count(const TsbBundle *h)  { return h->b.node_count; }
uint32_t tsb_bundle_ref_count(const TsbBundle *h)   { return h->b.ref_count; }

const uint8_t *tsb_bundle_atom(const TsbBundle *h, uint32_t id, uint32_t *len_out) {
    if (id >= h->b.atoms.count) return NULL;
    if (len_out) *len_out = atoms_len(&h->b.atoms, id);
    return atoms_get(&h->b.atoms, id);
}

int tsb_node_kind(const TsbBundle *h, uint32_t node, uint8_t *kind_out) {
    if (node >= h->b.node_count) return TSB_E_NODE;
    if (kind_out) *kind_out = h->b.nodes[node].kind;
    return TSB_OK;
}

const uint8_t *tsb_node_name(const TsbBundle *h, uint32_t node, uint32_t *len_out) {
    if (node >= h->b.node_count) return NULL;
    uint32_t id = h->b.nodes[node].name_id;
    if (len_out) *len_out = atoms_len(&h->b.atoms, id);
    return atoms_get(&h->b.atoms, id);
}

int tsb_node_find(const TsbBundle *h, const char *name, uint32_t *node_out) {
    size_t nlen = strlen(name);
    for (uint32_t i = 0; i < h->b.node_count; i++) {
        uint32_t id = h->b.nodes[i].name_id;
        uint32_t alen = atoms_len(&h->b.atoms, id);
        const uint8_t *bytes = atoms_get(&h->b.atoms, id);
        if (bytes && alen == nlen && memcmp(bytes, name, nlen) == 0) {
            if (node_out) *node_out = i;
            return TSB_OK;
        }
    }
    return TSB_E_SEMANTIC;
}

uint32_t tsb_node_child_count(const TsbBundle *h, uint32_t node) {
    if (node >= h->b.node_count) return 0;
    return h->b.nodes[node].child_count;
}

int tsb_node_child(const TsbBundle *h, uint32_t node, uint32_t i, uint32_t *child_out) {
    if (node >= h->b.node_count) return TSB_E_NODE;
    if (i >= h->b.nodes[node].child_count) return TSB_E_NODE;
    if (child_out) *child_out = h->b.nodes[node].children[i];
    return TSB_OK;
}

uint32_t tsb_node_prop_count(const TsbBundle *h, uint32_t node) {
    if (node >= h->b.node_count) return 0;
    return h->b.nodes[node].prop_count;
}

int tsb_node_prop(const TsbBundle *h, uint32_t node, uint32_t i,
                  uint32_t *name_atom_out, uint8_t *decl_type_out, uint32_t *val_index_out) {
    if (node >= h->b.node_count) return TSB_E_NODE;
    if (i >= h->b.nodes[node].prop_count) return TSB_E_NODE;
    const Prop *p = &h->b.nodes[node].props[i];
    if (name_atom_out) *name_atom_out = p->name_atom;
    if (decl_type_out) *decl_type_out = p->decl_type;
    if (val_index_out) *val_index_out = p->val_index;
    return TSB_OK;
}

static const Value *value_at(const TsbBundle *h, uint32_t slot) {
    if (slot >= h->b.value_count) return NULL;
    return &h->b.values[slot];
}

uint8_t tsb_value_kind(const TsbBundle *h, uint32_t slot) {
    const Value *v = value_at(h, slot);
    return v ? v->kind : 0;
}

int tsb_value_int(const TsbBundle *h, uint32_t slot, int64_t *out) {
    const Value *v = value_at(h, slot);
    if (!v || v->kind != VAL_INT) return TSB_E_VALUE;
    if (out) *out = v->u.i64;
    return TSB_OK;
}

int tsb_value_float(const TsbBundle *h, uint32_t slot, double *out) {
    const Value *v = value_at(h, slot);
    if (!v || v->kind != VAL_FLOAT) return TSB_E_VALUE;
    if (out) *out = v->u.f64;
    return TSB_OK;
}

int tsb_value_bytes(const TsbBundle *h, uint32_t slot, const uint8_t **ptr_out, uint32_t *len_out) {
    const Value *v = value_at(h, slot);
    if (!v || v->kind != VAL_BYTES) return TSB_E_VALUE;
    if (ptr_out) *ptr_out = v->u.bytes.ptr;
    if (len_out) *len_out = v->u.bytes.len;
    return TSB_OK;
}

uint32_t tsb_value_list_count(const TsbBundle *h, uint32_t slot) {
    const Value *v = value_at(h, slot);
    if (!v || v->kind != VAL_LIST) return 0;
    return v->u.list.count;
}

int tsb_value_list_item(const TsbBundle *h, uint32_t slot, uint32_t i, uint32_t *item_out) {
    const Value *v = value_at(h, slot);
    if (!v || v->kind != VAL_LIST) return TSB_E_VALUE;
    if (i >= v->u.list.count) return TSB_E_VALUE;
    if (item_out) *item_out = v->u.list.items[i];
    return TSB_OK;
}

int tsb_value_ref(const TsbBundle *h, uint32_t slot, uint32_t *ref_out) {
    const Value *v = value_at(h, slot);
    if (!v || v->kind != VAL_REF) return TSB_E_VALUE;
    if (ref_out) *ref_out = v->u.ref;
    return TSB_OK;
}
