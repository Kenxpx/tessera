/* bundle.h - the in-memory bundle aggregate and the finisher entry points. */
#ifndef TESSERA_BUNDLE_H
#define TESSERA_BUNDLE_H

#include "atoms.h"
#include "values.h"
#include "nodes.h"
#include "refs.h"

typedef struct {
    Atoms    atoms;
    Value   *values;
    uint32_t value_count;
    Node    *nodes;
    uint32_t node_count;
    Ref     *refs;
    uint32_t ref_count;

    /* Finisher outputs. Each finisher records a small observable result here so
     * the unit tests can pin its behavior on valid input (and so a "fix" can't
     * silently delete the work that produces a crash). */
    uint64_t flat_digest;     /* flatten: FNV fold over byte slots          */
    uint32_t pack_export_len; /* pack:   total exported byte length         */
    uint64_t mat_digest;      /* materialize: FNV fold over emitted props   */
} Bundle;

void bundle_free(Bundle *b);

/* Shared load pipeline (header + atoms + values, plus nodes/refs when
 * with_graph). Exposed for the unit tests, which drive the finishers directly
 * against a loaded bundle. */
int load_core(Bundle *b, const uint8_t *data, size_t size, int with_graph);

int flatten_run(Bundle *b);
int pack_run(Bundle *b);
int materialize_run(Bundle *b);

#endif /* TESSERA_BUNDLE_H */
