#include "atoms.h"
#include "common.h"
#include "mem.h"

#include <stdlib.h>
#include <string.h>

static int index_reserve(Atoms *a, uint32_t need) {
    if (need <= a->index_cap) return 0;
    uint32_t nc = a->index_cap ? a->index_cap : 8;
    while (nc < need) nc <<= 1;
    AtomRef *ni = tsb_realloc(a->index, (size_t)nc * sizeof(AtomRef));
    if (!ni) return -1;
    a->index = ni;
    a->index_cap = nc;
    return 0;
}

/* Ensure the pool buffer can take `need` more bytes, growing (and relocating)
 * the backing allocation if it has to. */
static int pool_reserve(Atoms *a, size_t need) {
    if (a->used + need <= a->cap) return 0;
    size_t nc = a->cap ? a->cap : 16;
    while (nc < a->used + need) nc <<= 1;
    uint8_t *nb = tsb_realloc(a->buf, nc);
    if (!nb) return -1;
    a->buf = nb;
    a->cap = nc;
    return 0;
}

int atoms_intern(Atoms *a, const uint8_t *bytes, uint32_t len) {
    size_t foot = align_up4((size_t)len + 1);
    if (pool_reserve(a, foot) != 0) return -1;
    if (index_reserve(a, a->count + 1) != 0) return -1;
    uint32_t off = (uint32_t)a->used;
    if (len) memcpy(a->buf + off, bytes, len);
    a->buf[off + len] = 0;
    a->index[a->count].off = off;
    a->index[a->count].len = len;
    a->used += foot;
    return (int)a->count++;
}

const uint8_t *atoms_get(const Atoms *a, uint32_t id) {
    if (id >= a->count) return NULL;
    return a->buf + a->index[id].off;
}

uint32_t atoms_len(const Atoms *a, uint32_t id) {
    if (id >= a->count) return 0;
    return a->index[id].len;
}

void atoms_dispose(Atoms *a) {
    if (!a) return;
    tsb_free(a->buf);
    tsb_free(a->index);
    a->buf = NULL;
    a->index = NULL;
    a->used = a->cap = 0;
    a->count = a->index_cap = 0;
}

int atoms_load(Atoms *a, const uint8_t *data, size_t off, size_t len) {
    memset(a, 0, sizeof(*a));

    cur_t c;
    cur_init(&c, data, off, len);
    uint32_t count = cur_u32(&c);
    if (!cur_ok(&c)) return TSB_E_BOUNDS;
    if (count > ATOMS_MAX) return TSB_E_SECTION;

    /* Pass 1: validate framing and sum the in-pool footprints so we can size
     * the pool exactly once. round_up_pow2 then gives the load-time capacity. */
    size_t after_count = c.pos;
    uint64_t total = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t l = cur_u32(&c);
        if (!cur_ok(&c)) return TSB_E_BOUNDS;
        if (cur_take(&c, l) == NULL) return TSB_E_BOUNDS;
        total += align_up4((uint64_t)l + 1);
        if (total > 0x7fffffffULL) return TSB_E_SECTION;
    }

    size_t cap = (size_t)round_up_pow2(total ? total : 1);
    a->buf = tsb_malloc(cap);
    if (!a->buf) return TSB_E_NOMEM;
    a->cap = cap;
    a->used = 0;
    if (index_reserve(a, count ? count : 1) != 0) {
        atoms_dispose(a);
        return TSB_E_NOMEM;
    }

    /* Pass 2: append. Everything fits inside the pre-sized capacity, so no
     * relocation happens here. */
    cur_t c2;
    cur_init(&c2, data, off, len);
    c2.pos = after_count;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t l = cur_u32(&c2);
        const uint8_t *p = cur_take(&c2, l);
        if (!p) { atoms_dispose(a); return TSB_E_BOUNDS; }
        uint32_t o = (uint32_t)a->used;
        if (l) memcpy(a->buf + o, p, l);
        a->buf[o + l] = 0;
        a->index[a->count].off = o;
        a->index[a->count].len = l;
        a->used += align_up4((size_t)l + 1);
        a->count++;
    }
    return TSB_OK;
}
