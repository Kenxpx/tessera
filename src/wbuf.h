/* wbuf.h - a growable little-endian output buffer.
 *
 * The writer side (encode.c) and the human-readable dumper (dump.c) both build
 * up a byte image incrementally; wbuf is the shared sink. Every put routine is
 * a no-op once the buffer has latched an error (an allocation failure), so a
 * caller can append a whole record and check wbuf_ok() once at the end instead
 * of testing every field.
 */
#ifndef TESSERA_WBUF_H
#define TESSERA_WBUF_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
    int      err;   /* sticky: set on OOM, never cleared */
} WBuf;

void   wbuf_init(WBuf *w);
void   wbuf_free(WBuf *w);
int    wbuf_ok(const WBuf *w);
size_t wbuf_pos(const WBuf *w);

/* Append raw bytes; returns 0 on success, -1 once the buffer is in error. */
int wbuf_put(WBuf *w, const void *p, size_t n);
int wbuf_u8(WBuf *w, uint8_t v);
int wbuf_u16(WBuf *w, uint16_t v);
int wbuf_u32(WBuf *w, uint32_t v);
int wbuf_i64(WBuf *w, int64_t v);
int wbuf_f64(WBuf *w, double v);

/* Overwrite a previously reserved 4-byte little-endian slot at `at`. Used to
 * backpatch section offsets/lengths once the body sizes are known. */
int wbuf_patch_u32(WBuf *w, size_t at, uint32_t v);

/* Hand the finished buffer to the caller; the WBuf is reset to empty and the
 * caller owns the returned allocation (free with the library allocator). */
uint8_t *wbuf_detach(WBuf *w, size_t *len_out);

#endif /* TESSERA_WBUF_H */
