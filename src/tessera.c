/* tessera.c - the public entry points and the shared load pipeline.
 *
 * Every entry point validates the header, loads the atom pool and value
 * stream, and (for the graph entry points) the node table and ref ops, before
 * dispatching to a finisher. The load steps are bounds-checked and return an
 * error rather than faulting on malformed input.
 */
#include "common.h"
#include "bundle.h"

#include <string.h>

int load_core(Bundle *b, const uint8_t *data, size_t size,
              int with_graph) {
    FmtHeader h;
    int rc = fmt_open(data, size, &h);
    if (rc) return rc;

    rc = atoms_load(&b->atoms, data, h.off[SEC_ATOMS], h.len[SEC_ATOMS]);
    if (rc) return rc;

    rc = decode_values(&b->values, &b->value_count,
                       data, h.off[SEC_VALUES], h.len[SEC_VALUES], &b->atoms);
    if (rc) return rc;

    if (with_graph) {
        rc = nodes_load(&b->nodes, &b->node_count,
                        data, h.off[SEC_NODES], h.len[SEC_NODES],
                        b->atoms.count, b->value_count);
        if (rc) return rc;

        rc = validate_bindings(b->nodes, b->node_count, b->values, b->value_count);
        if (rc) return rc;

        rc = refs_load(&b->refs, &b->ref_count,
                       data, h.off[SEC_REFS], h.len[SEC_REFS],
                       b->node_count, b->value_count, b->atoms.count);
        if (rc) return rc;
    }
    return TSB_OK;
}

int tsb_decode_values(const uint8_t *data, size_t size) {
    Bundle b;
    memset(&b, 0, sizeof(b));
    int rc = load_core(&b, data, size, 0);
    bundle_free(&b);
    return rc;
}

int tsb_flatten(const uint8_t *data, size_t size) {
    Bundle b;
    memset(&b, 0, sizeof(b));
    int rc = load_core(&b, data, size, 1);
    if (!rc) rc = flatten_run(&b);
    bundle_free(&b);
    return rc;
}

int tsb_pack(const uint8_t *data, size_t size) {
    Bundle b;
    memset(&b, 0, sizeof(b));
    int rc = load_core(&b, data, size, 1);
    if (!rc) rc = pack_run(&b);
    bundle_free(&b);
    return rc;
}

int tsb_materialize(const uint8_t *data, size_t size) {
    Bundle b;
    memset(&b, 0, sizeof(b));
    int rc = load_core(&b, data, size, 1);
    if (!rc) rc = materialize_run(&b);
    bundle_free(&b);
    return rc;
}
