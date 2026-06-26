/* tessera/builder.h - a programmatic writer for the .tsb container format.
 *
 * The builder is the inverse of the loaders in libtessera: it accumulates
 * atoms, value slots, nodes, and ref ops in memory and serializes them into a
 * single little-endian image that the readers (tsb_flatten/pack/materialize
 * and friends) accept. It is the path callers use to produce bundles, and the
 * basis for the seed-corpus and round-trip fixtures.
 *
 * Every "add" routine that creates an entity returns its index (atom id, value
 * slot, or node id) so later entities can reference it; on allocation failure
 * it returns a negative tsb_status. Ids are assigned densely from zero in the
 * order added, matching the on-disk record order.
 *
 * The builder performs only local structural bookkeeping - it does not enforce
 * the cross-section invariants the loaders check (e.g. that a property's value
 * slot exists, or that a declared type matches its slot). That makes it usable
 * for emitting deliberately edge-case bundles; call tsb_builder_check() if you
 * want those invariants validated before emit.
 */
#ifndef TESSERA_BUILDER_H
#define TESSERA_BUILDER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TsbBuilder TsbBuilder;

TsbBuilder *tsb_builder_new(void);
void        tsb_builder_free(TsbBuilder *b);

/* Set the header flags word written into the image (default 0). */
void tsb_builder_set_flags(TsbBuilder *b, uint16_t flags);

/* atoms ------------------------------------------------------------------- */
int tsb_builder_atom(TsbBuilder *b, const void *bytes, uint32_t len);     /* -> atom id   */
int tsb_builder_atom_str(TsbBuilder *b, const char *s);                   /* -> atom id   */

/* values ------------------------------------------------------------------ */
int tsb_builder_val_int(TsbBuilder *b, int64_t v);                        /* -> value slot */
int tsb_builder_val_float(TsbBuilder *b, double v);
int tsb_builder_val_inline(TsbBuilder *b, const void *bytes, uint32_t len);
int tsb_builder_val_atom(TsbBuilder *b, uint32_t atom_id);
int tsb_builder_val_list(TsbBuilder *b, const uint32_t *items, uint32_t n);
int tsb_builder_val_ref(TsbBuilder *b, uint32_t ref);
int tsb_builder_val_u128(TsbBuilder *b, const uint8_t v[16]);

/* nodes ------------------------------------------------------------------- */
int tsb_builder_node(TsbBuilder *b, uint8_t kind, uint32_t name_id);      /* -> node id   */
int tsb_builder_node_child(TsbBuilder *b, uint32_t node, uint32_t child);
int tsb_builder_node_prop(TsbBuilder *b, uint32_t node,
                          uint32_t name_atom, uint8_t decl_type, uint32_t val_index);

/* ref ops ----------------------------------------------------------------- */
int tsb_builder_ref_link(TsbBuilder *b, uint32_t a, uint32_t bnode);
int tsb_builder_ref_synth(TsbBuilder *b, uint32_t node, const void *data, uint32_t len);
int tsb_builder_ref_concat(TsbBuilder *b, uint32_t node, uint32_t atom_a, uint32_t atom_b);
int tsb_builder_ref_open_scope(TsbBuilder *b, uint32_t node);
int tsb_builder_ref_close_scope(TsbBuilder *b);
int tsb_builder_ref_rebind(TsbBuilder *b, uint32_t sel, uint32_t slot);
int tsb_builder_ref_bind_atom(TsbBuilder *b, uint32_t slot, uint32_t atom);
int tsb_builder_ref_adopt(TsbBuilder *b, uint32_t slot);

/* Optional cross-section validation (mirrors the loaders' range checks).
 * Returns TSB_OK or the first violated tsb_status. */
int tsb_builder_check(const TsbBuilder *b);

/* Serialize. On success *out is a freshly allocated image of *out_len bytes,
 * owned by the caller (free with the library allocator); returns TSB_OK.
 * Returns a negative tsb_status on failure and leaves *out NULL. */
int tsb_builder_emit(TsbBuilder *b, uint8_t **out, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* TESSERA_BUILDER_H */
