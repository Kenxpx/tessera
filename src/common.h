/* common.h - internal helpers shared by the loaders: a bounded little-endian
 * cursor, a couple of size helpers, and the parsed-header struct.
 *
 * Every byte read during parsing goes through one of the cur_* readers, which
 * refuse to read past the section limit. That is the single invariant the
 * "no malformed input crashes a loader" guarantee rests on.
 */
#ifndef TESSERA_COMMON_H
#define TESSERA_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "tessera/tessera.h"
#include "tessera/format.h"

/* Bounds on per-section counts so a hostile header can't ask us to allocate an
 * absurd table before we've even seen the records. */
#define ATOMS_MAX   (1u << 20)
#define NODES_MAX   (1u << 20)
#define VALUES_MAX  (1u << 20)
#define REFS_MAX    (1u << 20)
#define LIST_MAX    (1u << 20)
#define SYNTH_MAX   (1u << 16)

typedef struct {
    const uint8_t *base;
    size_t pos;
    size_t end;
    int    err;
} cur_t;

static inline void cur_init(cur_t *c, const uint8_t *data, size_t off, size_t len) {
    c->base = data + off;
    c->pos  = 0;
    c->end  = len;
    c->err  = 0;
}
static inline int    cur_ok(const cur_t *c)   { return !c->err; }
static inline size_t cur_left(const cur_t *c) { return c->err ? 0 : c->end - c->pos; }

static inline uint8_t cur_u8(cur_t *c) {
    if (c->err || c->pos + 1 > c->end) { c->err = 1; return 0; }
    return c->base[c->pos++];
}
static inline uint16_t cur_u16(cur_t *c) {
    if (c->err || c->pos + 2 > c->end) { c->err = 1; return 0; }
    uint16_t v = (uint16_t)(c->base[c->pos] | ((uint16_t)c->base[c->pos + 1] << 8));
    c->pos += 2;
    return v;
}
static inline uint32_t cur_u32(cur_t *c) {
    if (c->err || c->pos + 4 > c->end) { c->err = 1; return 0; }
    uint32_t v = (uint32_t)c->base[c->pos]
               | ((uint32_t)c->base[c->pos + 1] << 8)
               | ((uint32_t)c->base[c->pos + 2] << 16)
               | ((uint32_t)c->base[c->pos + 3] << 24);
    c->pos += 4;
    return v;
}
static inline int64_t cur_i64(cur_t *c) {
    if (c->err || c->pos + 8 > c->end) { c->err = 1; return 0; }
    uint64_t v = 0;
    for (size_t i = 0; i < 8; i++) v |= (uint64_t)c->base[c->pos + i] << (8u * i);
    c->pos += 8;
    return (int64_t)v;
}
static inline double cur_f64(cur_t *c) {
    if (c->err || c->pos + 8 > c->end) { c->err = 1; return 0.0; }
    double d;
    memcpy(&d, c->base + c->pos, 8);
    c->pos += 8;
    return d;
}
/* hand back a pointer to n raw bytes and advance; NULL if it would overrun */
static inline const uint8_t *cur_take(cur_t *c, size_t n) {
    if (c->err || c->pos + n > c->end) { c->err = 1; return NULL; }
    const uint8_t *p = c->base + c->pos;
    c->pos += n;
    return p;
}

static inline size_t align_up4(size_t x) { return (x + 3u) & ~(size_t)3u; }

static inline uint64_t round_up_pow2(uint64_t x) {
    if (x <= 1) return 1;
    x--;
    x |= x >> 1;  x |= x >> 2;  x |= x >> 4;
    x |= x >> 8;  x |= x >> 16; x |= x >> 32;
    return x + 1;
}

/* Result of fmt_open: version/flags plus the located sections, indexed by id. */
typedef struct {
    uint16_t version;
    uint16_t flags;
    uint32_t off[SEC_MAX_ID + 1];
    uint32_t len[SEC_MAX_ID + 1];
} FmtHeader;

int fmt_open(const uint8_t *data, size_t size, FmtHeader *out);

#endif /* TESSERA_COMMON_H */
