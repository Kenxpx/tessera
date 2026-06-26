/* nodes.h - the object/node table.
 *
 * A node has a kind, a name (an atom id), child node ids, and typed
 * properties. nodes_load() stores the name as an id, never as a pointer; a
 * finisher that wants the bytes resolves the id against the pool.
 */
#ifndef TESSERA_NODES_H
#define TESSERA_NODES_H

#include <stdint.h>
#include <stddef.h>

#include "values.h"

typedef struct {
    uint32_t name_atom;
    uint8_t  decl_type;
    uint32_t val_index;
} Prop;

typedef struct {
    uint8_t   kind;
    uint32_t  name_id;
    uint32_t *children;
    uint16_t  child_count;
    Prop     *props;
    uint16_t  prop_count;
    const uint8_t *name; /* finisher scratch: resolved name bytes */
} Node;

int  nodes_load(Node **out, uint32_t *outn,
                const uint8_t *data, size_t off, size_t len,
                uint32_t atom_count, uint32_t value_count);
int  validate_bindings(const Node *nodes, uint32_t node_count,
                       const Value *values, uint32_t value_count);
void nodes_dispose(Node *nodes, uint32_t n);

#endif /* TESSERA_NODES_H */
