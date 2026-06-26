/* encode.c - the .tsb writer (see include/tessera/builder.h).
 *
 * Records are kept in plain growable arrays as they are added, then serialized
 * in one pass: each of the four sections is rendered into its own WBuf, and the
 * final image is header + section table (with computed offsets) + bodies, laid
 * out in section-id order exactly as fmt_open() expects to find them.
 */
#include "tessera/builder.h"
#include "tessera/tessera.h"
#include "tessera/format.h"
#include "common.h"   /* SYNTH_MAX and the other on-disk caps */
#include "wbuf.h"
#include "mem.h"

#include <string.h>

typedef struct { uint8_t *bytes; uint32_t len; } BAtom;

typedef struct {
    uint8_t  tag;   /* VAL_* */
    uint8_t  sub;   /* VAL_BYTES: BYTES_INLINE | BYTES_ATOM */
    union {
        int64_t  i64;
        double   f64;
        struct { uint8_t *ptr; uint32_t len; } inl;  /* owned inline copy */
        uint32_t atom;                               /* bytes-atom id, or ref */
        struct { uint32_t *items; uint32_t n; } list;
        uint8_t  u128[16];
    } u;
} BValue;

typedef struct { uint32_t name_atom; uint8_t decl_type; uint32_t val_index; } BProp;

typedef struct {
    uint8_t   kind;
    uint32_t  name_id;
    uint32_t *children; uint32_t child_count, child_cap;
    BProp    *props;    uint32_t prop_count,  prop_cap;
} BNode;

typedef struct {
    uint8_t  op;
    uint32_t a, b, c;
    uint8_t *data; uint32_t data_len;
} BRef;

struct TsbBuilder {
    uint16_t flags;
    BAtom   *atoms;  uint32_t natoms,  catoms;
    BValue  *values; uint32_t nvalues, cvalues;
    BNode   *nodes;  uint32_t nnodes,  cnodes;
    BRef    *refs;   uint32_t nrefs,   crefs;
    int      err;
};

/* Grow *arr (element size elem) so that count+1 fits; updates *cap. */
static int ensure(void **arr, uint32_t *cap, uint32_t count, size_t elem) {
    if (count < *cap) return 0;
    uint32_t nc = *cap ? *cap * 2 : 8;
    void *na = tsb_realloc(*arr, (size_t)nc * elem);
    if (!na) return -1;
    *arr = na;
    *cap = nc;
    return 0;
}

TsbBuilder *tsb_builder_new(void) {
    TsbBuilder *b = tsb_calloc(1, sizeof(*b));
    return b;
}

void tsb_builder_free(TsbBuilder *b) {
    if (!b) return;
    for (uint32_t i = 0; i < b->natoms; i++) tsb_free(b->atoms[i].bytes);
    tsb_free(b->atoms);
    for (uint32_t i = 0; i < b->nvalues; i++) {
        if (b->values[i].tag == VAL_BYTES && b->values[i].sub == BYTES_INLINE)
            tsb_free(b->values[i].u.inl.ptr);
        else if (b->values[i].tag == VAL_LIST)
            tsb_free(b->values[i].u.list.items);
    }
    tsb_free(b->values);
    for (uint32_t i = 0; i < b->nnodes; i++) {
        tsb_free(b->nodes[i].children);
        tsb_free(b->nodes[i].props);
    }
    tsb_free(b->nodes);
    for (uint32_t i = 0; i < b->nrefs; i++) tsb_free(b->refs[i].data);
    tsb_free(b->refs);
    tsb_free(b);
}

void tsb_builder_set_flags(TsbBuilder *b, uint16_t flags) { b->flags = flags; }

/* ---- atoms --------------------------------------------------------------- */

int tsb_builder_atom(TsbBuilder *b, const void *bytes, uint32_t len) {
    if (b->err) return TSB_E_NOMEM;
    if (ensure((void **)&b->atoms, &b->catoms, b->natoms, sizeof(BAtom)) != 0)
        { b->err = 1; return TSB_E_NOMEM; }
    uint8_t *copy = tsb_malloc(len ? len : 1);
    if (!copy) { b->err = 1; return TSB_E_NOMEM; }
    if (len) memcpy(copy, bytes, len);
    b->atoms[b->natoms].bytes = copy;
    b->atoms[b->natoms].len = len;
    return (int)b->natoms++;
}

int tsb_builder_atom_str(TsbBuilder *b, const char *s) {
    return tsb_builder_atom(b, s, (uint32_t)strlen(s));
}

/* ---- values -------------------------------------------------------------- */

static BValue *new_value(TsbBuilder *b, int *slot_out) {
    if (b->err) return NULL;
    if (ensure((void **)&b->values, &b->cvalues, b->nvalues, sizeof(BValue)) != 0)
        { b->err = 1; return NULL; }
    BValue *v = &b->values[b->nvalues];
    memset(v, 0, sizeof(*v));
    *slot_out = (int)b->nvalues++;
    return v;
}

int tsb_builder_val_int(TsbBuilder *b, int64_t x) {
    int s; BValue *v = new_value(b, &s); if (!v) return TSB_E_NOMEM;
    v->tag = VAL_INT; v->u.i64 = x; return s;
}
int tsb_builder_val_float(TsbBuilder *b, double x) {
    int s; BValue *v = new_value(b, &s); if (!v) return TSB_E_NOMEM;
    v->tag = VAL_FLOAT; v->u.f64 = x; return s;
}
int tsb_builder_val_inline(TsbBuilder *b, const void *bytes, uint32_t len) {
    int s; BValue *v = new_value(b, &s); if (!v) return TSB_E_NOMEM;
    uint8_t *copy = tsb_malloc(len ? len : 1);
    if (!copy) { b->err = 1; return TSB_E_NOMEM; }
    if (len) memcpy(copy, bytes, len);
    v->tag = VAL_BYTES; v->sub = BYTES_INLINE; v->u.inl.ptr = copy; v->u.inl.len = len;
    return s;
}
int tsb_builder_val_atom(TsbBuilder *b, uint32_t atom_id) {
    int s; BValue *v = new_value(b, &s); if (!v) return TSB_E_NOMEM;
    v->tag = VAL_BYTES; v->sub = BYTES_ATOM; v->u.atom = atom_id; return s;
}
int tsb_builder_val_list(TsbBuilder *b, const uint32_t *items, uint32_t n) {
    int s; BValue *v = new_value(b, &s); if (!v) return TSB_E_NOMEM;
    uint32_t *copy = n ? tsb_malloc((size_t)n * sizeof(uint32_t)) : NULL;
    if (n && !copy) { b->err = 1; return TSB_E_NOMEM; }
    if (n) memcpy(copy, items, (size_t)n * sizeof(uint32_t));
    v->tag = VAL_LIST; v->u.list.items = copy; v->u.list.n = n; return s;
}
int tsb_builder_val_ref(TsbBuilder *b, uint32_t ref) {
    int s; BValue *v = new_value(b, &s); if (!v) return TSB_E_NOMEM;
    v->tag = VAL_REF; v->u.atom = ref; return s;
}
int tsb_builder_val_u128(TsbBuilder *b, const uint8_t x[16]) {
    int s; BValue *v = new_value(b, &s); if (!v) return TSB_E_NOMEM;
    v->tag = VAL_U128; memcpy(v->u.u128, x, 16); return s;
}

/* ---- nodes --------------------------------------------------------------- */

int tsb_builder_node(TsbBuilder *b, uint8_t kind, uint32_t name_id) {
    if (b->err) return TSB_E_NOMEM;
    if (ensure((void **)&b->nodes, &b->cnodes, b->nnodes, sizeof(BNode)) != 0)
        { b->err = 1; return TSB_E_NOMEM; }
    BNode *nd = &b->nodes[b->nnodes];
    memset(nd, 0, sizeof(*nd));
    nd->kind = kind;
    nd->name_id = name_id;
    return (int)b->nnodes++;
}

int tsb_builder_node_child(TsbBuilder *b, uint32_t node, uint32_t child) {
    if (b->err) return TSB_E_NOMEM;
    if (node >= b->nnodes) return TSB_E_NODE;
    BNode *nd = &b->nodes[node];
    if (ensure((void **)&nd->children, &nd->child_cap, nd->child_count, sizeof(uint32_t)) != 0)
        { b->err = 1; return TSB_E_NOMEM; }
    nd->children[nd->child_count++] = child;
    return TSB_OK;
}

int tsb_builder_node_prop(TsbBuilder *b, uint32_t node,
                          uint32_t name_atom, uint8_t decl_type, uint32_t val_index) {
    if (b->err) return TSB_E_NOMEM;
    if (node >= b->nnodes) return TSB_E_NODE;
    BNode *nd = &b->nodes[node];
    if (ensure((void **)&nd->props, &nd->prop_cap, nd->prop_count, sizeof(BProp)) != 0)
        { b->err = 1; return TSB_E_NOMEM; }
    nd->props[nd->prop_count].name_atom = name_atom;
    nd->props[nd->prop_count].decl_type = decl_type;
    nd->props[nd->prop_count].val_index = val_index;
    nd->prop_count++;
    return TSB_OK;
}

/* ---- ref ops ------------------------------------------------------------- */

static BRef *new_ref(TsbBuilder *b, uint8_t op) {
    if (b->err) return NULL;
    if (ensure((void **)&b->refs, &b->crefs, b->nrefs, sizeof(BRef)) != 0)
        { b->err = 1; return NULL; }
    BRef *r = &b->refs[b->nrefs++];
    memset(r, 0, sizeof(*r));
    r->op = op;
    return r;
}

int tsb_builder_ref_link(TsbBuilder *b, uint32_t a, uint32_t bnode) {
    BRef *r = new_ref(b, OP_LINK); if (!r) return TSB_E_NOMEM;
    r->a = a; r->b = bnode; return TSB_OK;
}
int tsb_builder_ref_synth(TsbBuilder *b, uint32_t node, const void *data, uint32_t len) {
    BRef *r = new_ref(b, OP_SYNTH); if (!r) return TSB_E_NOMEM;
    r->a = node;
    r->data = tsb_malloc(len ? len : 1);
    if (!r->data) { b->err = 1; return TSB_E_NOMEM; }
    if (len) memcpy(r->data, data, len);
    r->data_len = len;
    return TSB_OK;
}
int tsb_builder_ref_concat(TsbBuilder *b, uint32_t node, uint32_t atom_a, uint32_t atom_b) {
    BRef *r = new_ref(b, OP_CONCAT); if (!r) return TSB_E_NOMEM;
    r->a = node; r->b = atom_a; r->c = atom_b; return TSB_OK;
}
int tsb_builder_ref_open_scope(TsbBuilder *b, uint32_t node) {
    BRef *r = new_ref(b, OP_OPEN_SCOPE); if (!r) return TSB_E_NOMEM;
    r->a = node; return TSB_OK;
}
int tsb_builder_ref_close_scope(TsbBuilder *b) {
    BRef *r = new_ref(b, OP_CLOSE_SCOPE); if (!r) return TSB_E_NOMEM;
    return TSB_OK;
}
int tsb_builder_ref_rebind(TsbBuilder *b, uint32_t sel, uint32_t slot) {
    BRef *r = new_ref(b, OP_REBIND_PROP); if (!r) return TSB_E_NOMEM;
    r->a = sel; r->b = slot; return TSB_OK;
}
int tsb_builder_ref_bind_atom(TsbBuilder *b, uint32_t slot, uint32_t atom) {
    BRef *r = new_ref(b, OP_BIND_ATOM); if (!r) return TSB_E_NOMEM;
    r->a = slot; r->b = atom; return TSB_OK;
}
int tsb_builder_ref_adopt(TsbBuilder *b, uint32_t slot) {
    BRef *r = new_ref(b, OP_ADOPT); if (!r) return TSB_E_NOMEM;
    r->a = slot; return TSB_OK;
}

/* ---- validation ---------------------------------------------------------- */

int tsb_builder_check(const TsbBuilder *b) {
    if (b->err) return TSB_E_NOMEM;
    for (uint32_t i = 0; i < b->nvalues; i++) {
        const BValue *v = &b->values[i];
        if (v->tag == VAL_BYTES && v->sub == BYTES_ATOM && v->u.atom >= b->natoms)
            return TSB_E_VALUE;
        if (v->tag == VAL_LIST)
            for (uint32_t k = 0; k < v->u.list.n; k++)
                if (v->u.list.items[k] >= b->nvalues) return TSB_E_VALUE;
    }
    for (uint32_t i = 0; i < b->nnodes; i++) {
        const BNode *nd = &b->nodes[i];
        if (nd->child_count > 0xffff || nd->prop_count > 0xffff) return TSB_E_NODE;
        if (nd->name_id >= b->natoms) return TSB_E_NODE;
        for (uint32_t k = 0; k < nd->child_count; k++)
            if (nd->children[k] >= b->nnodes) return TSB_E_NODE;
        for (uint32_t k = 0; k < nd->prop_count; k++) {
            const BProp *p = &nd->props[k];
            if (p->name_atom >= b->natoms) return TSB_E_NODE;
            if (p->decl_type < VAL_MIN || p->decl_type > VAL_MAX) return TSB_E_NODE;
            if (p->val_index >= b->nvalues) return TSB_E_NODE;
            if (b->values[p->val_index].tag != p->decl_type) return TSB_E_BINDING;
        }
    }
    for (uint32_t i = 0; i < b->nrefs; i++) {
        const BRef *r = &b->refs[i];
        switch (r->op) {
        case OP_LINK:        if (r->a >= b->nnodes || r->b >= b->nnodes) return TSB_E_REF; break;
        case OP_SYNTH:       if (r->a >= b->nnodes || r->data_len > SYNTH_MAX) return TSB_E_REF; break;
        case OP_CONCAT:      if (r->a >= b->nnodes || r->b >= b->natoms || r->c >= b->natoms) return TSB_E_REF; break;
        case OP_OPEN_SCOPE:  if (r->a >= b->nnodes) return TSB_E_REF; break;
        case OP_CLOSE_SCOPE: break;
        case OP_REBIND_PROP: if (r->b >= b->nvalues) return TSB_E_REF; break;
        case OP_BIND_ATOM:   if (r->a >= b->nvalues || r->b >= b->natoms) return TSB_E_REF; break;
        case OP_ADOPT:       if (r->a >= b->nvalues) return TSB_E_REF; break;
        default:             return TSB_E_REF;
        }
    }
    return TSB_OK;
}

/* ---- emit ---------------------------------------------------------------- */

static int emit_atoms(const TsbBuilder *b, WBuf *w) {
    wbuf_u32(w, b->natoms);
    for (uint32_t i = 0; i < b->natoms; i++) {
        wbuf_u32(w, b->atoms[i].len);
        wbuf_put(w, b->atoms[i].bytes, b->atoms[i].len);
    }
    return wbuf_ok(w) ? TSB_OK : TSB_E_NOMEM;
}

static int emit_values(const TsbBuilder *b, WBuf *w) {
    wbuf_u32(w, b->nvalues);
    for (uint32_t i = 0; i < b->nvalues; i++) {
        const BValue *v = &b->values[i];
        wbuf_u8(w, v->tag);
        switch (v->tag) {
        case VAL_INT:   wbuf_i64(w, v->u.i64); break;
        case VAL_FLOAT: wbuf_f64(w, v->u.f64); break;
        case VAL_BYTES:
            wbuf_u8(w, v->sub);
            if (v->sub == BYTES_INLINE) {
                wbuf_u32(w, v->u.inl.len);
                wbuf_put(w, v->u.inl.ptr, v->u.inl.len);
            } else {
                wbuf_u32(w, v->u.atom);
            }
            break;
        case VAL_LIST:
            wbuf_u32(w, v->u.list.n);
            for (uint32_t k = 0; k < v->u.list.n; k++) wbuf_u32(w, v->u.list.items[k]);
            break;
        case VAL_REF:   wbuf_u32(w, v->u.atom); break;
        case VAL_U128:  wbuf_put(w, v->u.u128, 16); break;
        default:        return TSB_E_VALUE;
        }
    }
    return wbuf_ok(w) ? TSB_OK : TSB_E_NOMEM;
}

static int emit_nodes(const TsbBuilder *b, WBuf *w) {
    wbuf_u32(w, b->nnodes);
    for (uint32_t i = 0; i < b->nnodes; i++) {
        const BNode *nd = &b->nodes[i];
        if (nd->child_count > 0xffff || nd->prop_count > 0xffff) return TSB_E_NODE;
        wbuf_u8(w, nd->kind);
        wbuf_u32(w, nd->name_id);
        wbuf_u16(w, (uint16_t)nd->child_count);
        for (uint32_t k = 0; k < nd->child_count; k++) wbuf_u32(w, nd->children[k]);
        wbuf_u16(w, (uint16_t)nd->prop_count);
        for (uint32_t k = 0; k < nd->prop_count; k++) {
            wbuf_u32(w, nd->props[k].name_atom);
            wbuf_u8(w,  nd->props[k].decl_type);
            wbuf_u32(w, nd->props[k].val_index);
        }
    }
    return wbuf_ok(w) ? TSB_OK : TSB_E_NOMEM;
}

static int emit_refs(const TsbBuilder *b, WBuf *w) {
    wbuf_u32(w, b->nrefs);
    for (uint32_t i = 0; i < b->nrefs; i++) {
        const BRef *r = &b->refs[i];
        wbuf_u8(w, r->op);
        switch (r->op) {
        case OP_LINK:        wbuf_u32(w, r->a); wbuf_u32(w, r->b); break;
        case OP_SYNTH:       wbuf_u32(w, r->a); wbuf_u32(w, r->data_len); wbuf_put(w, r->data, r->data_len); break;
        case OP_CONCAT:      wbuf_u32(w, r->a); wbuf_u32(w, r->b); wbuf_u32(w, r->c); break;
        case OP_OPEN_SCOPE:  wbuf_u32(w, r->a); break;
        case OP_CLOSE_SCOPE: break;
        case OP_REBIND_PROP: wbuf_u32(w, r->a); wbuf_u32(w, r->b); break;
        case OP_BIND_ATOM:   wbuf_u32(w, r->a); wbuf_u32(w, r->b); break;
        case OP_ADOPT:       wbuf_u32(w, r->a); break;
        default:             return TSB_E_REF;
        }
    }
    return wbuf_ok(w) ? TSB_OK : TSB_E_NOMEM;
}

int tsb_builder_emit(TsbBuilder *b, uint8_t **out, size_t *out_len) {
    *out = NULL;
    if (out_len) *out_len = 0;
    if (b->err) return TSB_E_NOMEM;

    /* Render the four section bodies (order: atoms, nodes, values, refs). */
    WBuf bodies[4];
    int (*emit_fn[4])(const TsbBuilder *, WBuf *) = { emit_atoms, emit_nodes, emit_values, emit_refs };
    int ids[4] = { SEC_ATOMS, SEC_NODES, SEC_VALUES, SEC_REFS };
    int rc = TSB_OK;
    int built = 0;
    for (; built < 4; built++) {
        wbuf_init(&bodies[built]);
        rc = emit_fn[built](b, &bodies[built]);
        if (rc != TSB_OK) { built++; goto cleanup; }
    }

    uint32_t table_end = TSB_HEADER_SIZE + 4 * TSB_TBLENT_SIZE;
    uint32_t off[4], cur = table_end;
    uint64_t total = table_end;
    for (int i = 0; i < 4; i++) {
        off[i] = cur;
        cur += (uint32_t)bodies[i].len;
        total += bodies[i].len;
    }
    if (total > 0xffffffffULL) { rc = TSB_E_SECTION; goto cleanup; }

    WBuf img; wbuf_init(&img);
    wbuf_put(&img, TSB_MAGIC, TSB_MAGIC_LEN);
    wbuf_u16(&img, TSB_VERSION);
    wbuf_u16(&img, b->flags);
    wbuf_u16(&img, 4);   /* section count */
    wbuf_u16(&img, 0);   /* reserved */
    for (int i = 0; i < 4; i++) {
        wbuf_u16(&img, (uint16_t)ids[i]);
        wbuf_u16(&img, 0);
        wbuf_u32(&img, off[i]);
        wbuf_u32(&img, (uint32_t)bodies[i].len);
    }
    for (int i = 0; i < 4; i++) wbuf_put(&img, bodies[i].data, bodies[i].len);

    if (!wbuf_ok(&img)) { wbuf_free(&img); rc = TSB_E_NOMEM; goto cleanup; }

    size_t n;
    uint8_t *image = wbuf_detach(&img, &n);
    *out = image;
    if (out_len) *out_len = n;
    rc = TSB_OK;

cleanup:
    for (int i = 0; i < built; i++) wbuf_free(&bodies[i]);
    return rc;
}
