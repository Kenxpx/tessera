#include "bundle.h"

#include <stdlib.h>

void bundle_free(Bundle *b) {
    if (!b) return;
    values_dispose(b->values, b->value_count);
    b->values = NULL;
    b->value_count = 0;
    nodes_dispose(b->nodes, b->node_count);
    b->nodes = NULL;
    b->node_count = 0;
    refs_dispose(b->refs, b->ref_count);
    b->refs = NULL;
    b->ref_count = 0;
    atoms_dispose(&b->atoms);
}
