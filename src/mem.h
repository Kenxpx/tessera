/* mem.h - internal allocation wrappers.
 *
 * Every allocation in the library goes through these so that an embedder can
 * redirect them via tsb_set_allocator(). They behave like the libc functions
 * except tsb_free(NULL) and zero sizes are always safe.
 */
#ifndef TESSERA_MEM_H
#define TESSERA_MEM_H

#include <stddef.h>

void *tsb_malloc(size_t n);
void *tsb_calloc(size_t count, size_t size);
void *tsb_realloc(void *p, size_t n);
void  tsb_free(void *p);

#endif /* TESSERA_MEM_H */
