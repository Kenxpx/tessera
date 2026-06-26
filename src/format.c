#include "common.h"

#include <string.h>

int fmt_open(const uint8_t *data, size_t size, FmtHeader *out) {
    memset(out, 0, sizeof(*out));
    if (size < TSB_HEADER_SIZE) return TSB_E_SHORT;
    if (memcmp(data, TSB_MAGIC, TSB_MAGIC_LEN) != 0) return TSB_E_MAGIC;

    cur_t c;
    cur_init(&c, data, 0, size);
    c.pos = TSB_MAGIC_LEN;
    uint16_t version   = cur_u16(&c);
    uint16_t flags     = cur_u16(&c);
    uint16_t sec_count = cur_u16(&c);
    (void)cur_u16(&c); /* reserved */
    if (!cur_ok(&c)) return TSB_E_SHORT;
    if (version != TSB_VERSION) return TSB_E_VERSION;
    if (sec_count == 0 || sec_count > 64) return TSB_E_SECTION;

    uint64_t table_end = (uint64_t)TSB_HEADER_SIZE + (uint64_t)sec_count * TSB_TBLENT_SIZE;
    if (table_end > size) return TSB_E_SECTION;

    out->version = version;
    out->flags = flags;

    int seen[SEC_MAX_ID + 1] = {0};
    cur_t t;
    cur_init(&t, data, TSB_HEADER_SIZE, (size_t)(table_end - TSB_HEADER_SIZE));
    for (uint16_t i = 0; i < sec_count; i++) {
        uint16_t id  = cur_u16(&t);
        (void)cur_u16(&t); /* pad */
        uint32_t off = cur_u32(&t);
        uint32_t len = cur_u32(&t);
        if (!cur_ok(&t)) return TSB_E_SECTION;
        if (id < 1 || id > SEC_MAX_ID) return TSB_E_SECTION;
        if (seen[id]) return TSB_E_SECTION;
        uint64_t end = (uint64_t)off + len;
        if (off < table_end || end > size) return TSB_E_SECTION;
        seen[id] = 1;
        out->off[id] = off;
        out->len[id] = len;
    }
    for (int id = 1; id <= SEC_MAX_ID; id++)
        if (!seen[id]) return TSB_E_SECTION;

    return TSB_OK;
}

int tsb_open_index(const uint8_t *data, size_t size) {
    FmtHeader h;
    return fmt_open(data, size, &h);
}
