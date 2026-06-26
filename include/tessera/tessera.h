/* tessera.h - public API for libtessera, a reader for the .tsb bundle format.
 *
 * Each entry point takes a raw .tsb image and runs a slice of the parse
 * pipeline. The lower layers (header, atoms, values, nodes, refs) are shared;
 * the three "finisher" entry points (flatten/pack/materialize) each apply a
 * different family of ref ops on top of the loaded bundle.
 *
 * All functions are read-only with respect to their input buffer and return a
 * status code from tsb_status; they never take ownership of `data`.
 */
#ifndef TESSERA_H
#define TESSERA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Optional memory hooks. Set a custom allocator to embed tessera in an
 * environment with its own memory management (an arena, a tracking allocator,
 * a sandbox). Pass NULL to restore the default malloc-based allocator. Not
 * reentrant; configure it once before use. */
typedef struct {
    void *(*alloc)(size_t size, void *ud);
    void *(*realloc)(void *ptr, size_t size, void *ud);
    void  (*free)(void *ptr, void *ud);
    void  *ud;
} tsb_allocator;

void tsb_set_allocator(const tsb_allocator *a);

enum tsb_status {
    TSB_OK = 0,
    TSB_E_SHORT,     /* input shorter than the header                 */
    TSB_E_MAGIC,     /* bad magic                                     */
    TSB_E_VERSION,   /* unsupported version                           */
    TSB_E_SECTION,   /* malformed section table                       */
    TSB_E_BOUNDS,    /* a record ran past its section                 */
    TSB_E_VALUE,     /* malformed value record                        */
    TSB_E_NODE,      /* malformed node record                         */
    TSB_E_REF,       /* malformed ref op                              */
    TSB_E_BINDING,   /* a property's declared type != its slot kind   */
    TSB_E_NOMEM,     /* allocation failure                            */
    TSB_E_SEMANTIC   /* a structurally valid op failed a runtime rule */
};

/* header + section table only */
int tsb_open_index(const uint8_t *data, size_t size);

/* header + atoms + value stream */
int tsb_decode_values(const uint8_t *data, size_t size);

/* full load + flatten finisher (resolves synthesized atoms into the graph) */
int tsb_flatten(const uint8_t *data, size_t size);

/* full load + pack finisher (exports the graph, may adopt the atom pool) */
int tsb_pack(const uint8_t *data, size_t size);

/* full load + materialize finisher (applies property rebinds and emits) */
int tsb_materialize(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* TESSERA_H */
