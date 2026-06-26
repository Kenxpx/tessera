#include "wbuf.h"
#include "mem.h"

#include <string.h>

void wbuf_init(WBuf *w) {
    w->data = NULL;
    w->len = 0;
    w->cap = 0;
    w->err = 0;
}

void wbuf_free(WBuf *w) {
    if (!w) return;
    tsb_free(w->data);
    wbuf_init(w);
}

int wbuf_ok(const WBuf *w) { return !w->err; }

size_t wbuf_pos(const WBuf *w) { return w->len; }

/* Grow to hold `need` more bytes. Doubling keeps appends amortized O(1). */
static int reserve(WBuf *w, size_t need) {
    if (w->err) return -1;
    if (w->len + need <= w->cap) return 0;
    size_t nc = w->cap ? w->cap : 64;
    while (nc < w->len + need) {
        size_t doubled = nc << 1;
        if (doubled < nc) { w->err = 1; return -1; } /* size_t overflow */
        nc = doubled;
    }
    uint8_t *nd = tsb_realloc(w->data, nc);
    if (!nd) { w->err = 1; return -1; }
    w->data = nd;
    w->cap = nc;
    return 0;
}

int wbuf_put(WBuf *w, const void *p, size_t n) {
    if (n == 0) return w->err ? -1 : 0;
    if (reserve(w, n) != 0) return -1;
    memcpy(w->data + w->len, p, n);
    w->len += n;
    return 0;
}

int wbuf_u8(WBuf *w, uint8_t v) {
    return wbuf_put(w, &v, 1);
}

int wbuf_u16(WBuf *w, uint16_t v) {
    uint8_t t[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
    return wbuf_put(w, t, 2);
}

int wbuf_u32(WBuf *w, uint32_t v) {
    uint8_t t[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    return wbuf_put(w, t, 4);
}

int wbuf_i64(WBuf *w, int64_t v) {
    uint64_t u = (uint64_t)v;
    uint8_t t[8];
    for (int i = 0; i < 8; i++) t[i] = (uint8_t)(u >> (8 * i));
    return wbuf_put(w, t, 8);
}

int wbuf_f64(WBuf *w, double v) {
    uint8_t t[8];
    memcpy(t, &v, 8);
    return wbuf_put(w, t, 8);
}

int wbuf_patch_u32(WBuf *w, size_t at, uint32_t v) {
    if (w->err) return -1;
    if (at + 4 > w->len) return -1;
    w->data[at + 0] = (uint8_t)v;
    w->data[at + 1] = (uint8_t)(v >> 8);
    w->data[at + 2] = (uint8_t)(v >> 16);
    w->data[at + 3] = (uint8_t)(v >> 24);
    return 0;
}

uint8_t *wbuf_detach(WBuf *w, size_t *len_out) {
    if (w->err) { wbuf_free(w); if (len_out) *len_out = 0; return NULL; }
    uint8_t *d = w->data;
    if (len_out) *len_out = w->len;
    wbuf_init(w);
    return d;
}
