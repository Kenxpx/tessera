/* tessera/query.h - a resident, read-only view over a loaded bundle.
 *
 * The top-level entry points in tessera.h load a bundle, run one slice of the
 * pipeline, and tear it down - they answer "does this parse?" but hand nothing
 * back. The query API instead keeps a bundle resident behind an opaque handle
 * so a caller can walk the node graph, read typed property values, and resolve
 * atoms after a successful load.
 *
 * A handle borrows nothing from the original input buffer: tsb_bundle_open()
 * copies everything it needs, so `data` may be freed immediately after it
 * returns. Accessors are pure reads and never fail on a valid handle; they
 * report out-of-range ids through their return code.
 */
#ifndef TESSERA_QUERY_H
#define TESSERA_QUERY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TsbBundle TsbBundle;

/* Load a bundle (header + atoms + values + nodes + refs, range-checked) and
 * keep it resident. Returns TSB_OK and sets *out, or a tsb_status on failure
 * (with *out left NULL). Does NOT run any finisher. */
int  tsb_bundle_open(const uint8_t *data, size_t size, TsbBundle **out);
void tsb_bundle_close(TsbBundle *h);

/* section sizes */
uint32_t tsb_bundle_atom_count(const TsbBundle *h);
uint32_t tsb_bundle_value_count(const TsbBundle *h);
uint32_t tsb_bundle_node_count(const TsbBundle *h);
uint32_t tsb_bundle_ref_count(const TsbBundle *h);

/* atoms: returns the interned bytes (NUL-terminated) or NULL if id is out of
 * range; *len_out, when non-NULL, receives the byte length. */
const uint8_t *tsb_bundle_atom(const TsbBundle *h, uint32_t id, uint32_t *len_out);

/* nodes */
int      tsb_node_kind(const TsbBundle *h, uint32_t node, uint8_t *kind_out);
const uint8_t *tsb_node_name(const TsbBundle *h, uint32_t node, uint32_t *len_out);
int      tsb_node_find(const TsbBundle *h, const char *name, uint32_t *node_out);
uint32_t tsb_node_child_count(const TsbBundle *h, uint32_t node);
int      tsb_node_child(const TsbBundle *h, uint32_t node, uint32_t i, uint32_t *child_out);
uint32_t tsb_node_prop_count(const TsbBundle *h, uint32_t node);
int      tsb_node_prop(const TsbBundle *h, uint32_t node, uint32_t i,
                       uint32_t *name_atom_out, uint8_t *decl_type_out, uint32_t *val_index_out);

/* values: tsb_value_kind returns one of VAL_* (or 0 if slot is out of range).
 * The typed getters return TSB_OK only when the slot holds that kind. */
uint8_t tsb_value_kind(const TsbBundle *h, uint32_t slot);
int     tsb_value_int(const TsbBundle *h, uint32_t slot, int64_t *out);
int     tsb_value_float(const TsbBundle *h, uint32_t slot, double *out);
int     tsb_value_bytes(const TsbBundle *h, uint32_t slot, const uint8_t **ptr_out, uint32_t *len_out);
uint32_t tsb_value_list_count(const TsbBundle *h, uint32_t slot);
int     tsb_value_list_item(const TsbBundle *h, uint32_t slot, uint32_t i, uint32_t *item_out);
int     tsb_value_ref(const TsbBundle *h, uint32_t slot, uint32_t *ref_out);

#ifdef __cplusplus
}
#endif

#endif /* TESSERA_QUERY_H */
