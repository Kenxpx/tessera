#include "mem.h"
#include "tessera/tessera.h"

#include <stdlib.h>
#include <string.h>

static void *def_alloc(size_t n, void *ud)              { (void)ud; return malloc(n); }
static void *def_realloc(void *p, size_t n, void *ud)   { (void)ud; return realloc(p, n); }
static void  def_free(void *p, void *ud)                { (void)ud; free(p); }

static tsb_allocator g_alloc = { def_alloc, def_realloc, def_free, NULL };

void tsb_set_allocator(const tsb_allocator *a) {
    if (a && a->alloc && a->realloc && a->free) {
        g_alloc = *a;
    } else {
        g_alloc.alloc = def_alloc;
        g_alloc.realloc = def_realloc;
        g_alloc.free = def_free;
        g_alloc.ud = NULL;
    }
}

void *tsb_malloc(size_t n) {
    return g_alloc.alloc(n ? n : 1, g_alloc.ud);
}

void *tsb_realloc(void *p, size_t n) {
    return g_alloc.realloc(p, n ? n : 1, g_alloc.ud);
}

void tsb_free(void *p) {
    if (p) g_alloc.free(p, g_alloc.ud);
}

void *tsb_calloc(size_t count, size_t size) {
    size_t tot = count * size;
    if (size && tot / size != count) return NULL; /* overflow */
    void *p = g_alloc.alloc(tot ? tot : 1, g_alloc.ud);
    if (p) memset(p, 0, tot ? tot : 1);
    return p;
}
