/* tsbtool.c - a small command-line front end for libtessera.
 *
 *   tsbtool dump <file>      render a bundle as text
 *   tsbtool check <file>     run every loader entry point, report status
 *   tsbtool roundtrip        build a sample bundle, emit it, dump it
 *
 * It exists so the library has a real driver beyond the fuzz harnesses, and as
 * a handy way to inspect a corpus input or a candidate crashing file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tessera/tessera.h"
#include "tessera/format.h"
#include "tessera/builder.h"
#include "tessera/dump.h"

static uint8_t *slurp(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "tsbtool: cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)sz ? (size_t)sz : 1);
    if (buf && sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    *len_out = (size_t)sz;
    return buf;
}

static int cmd_dump(const char *path) {
    size_t n; uint8_t *buf = slurp(path, &n);
    if (!buf) return 2;
    int rc = tsb_dump(buf, n, stdout);
    free(buf);
    return rc == TSB_OK ? 0 : 1;
}

static int cmd_check(const char *path) {
    size_t n; uint8_t *buf = slurp(path, &n);
    if (!buf) return 2;
    struct { const char *name; int (*fn)(const uint8_t *, size_t); } entries[] = {
        { "open_index",    tsb_open_index },
        { "decode_values", tsb_decode_values },
        { "flatten",       tsb_flatten },
        { "pack",          tsb_pack },
        { "materialize",   tsb_materialize },
    };
    int worst = 0;
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        int rc = entries[i].fn(buf, n);
        printf("%-14s -> %d\n", entries[i].name, rc);
        if (rc != 0) worst = 1;
    }
    free(buf);
    return worst;
}

/* Build a small but complete bundle, emit it, and dump it back - exercises the
 * writer and the reader against each other. */
static int cmd_roundtrip(void) {
    TsbBuilder *b = tsb_builder_new();
    if (!b) return 2;

    int a_root = tsb_builder_atom_str(b, "root");
    int a_key  = tsb_builder_atom_str(b, "key");
    int a_hi   = tsb_builder_atom_str(b, "hello");

    int v_int  = tsb_builder_val_int(b, 1234);
    (void)tsb_builder_val_atom(b, (uint32_t)a_hi);
    uint32_t items[2] = { (uint32_t)v_int, (uint32_t)v_int };
    (void)tsb_builder_val_list(b, items, 2);

    int n0 = tsb_builder_node(b, 0, (uint32_t)a_root);
    tsb_builder_node_prop(b, (uint32_t)n0, (uint32_t)a_key, VAL_INT, (uint32_t)v_int);

    if (tsb_builder_check(b) != TSB_OK) {
        fprintf(stderr, "tsbtool: sample bundle failed validation\n");
        tsb_builder_free(b);
        return 1;
    }

    uint8_t *img = NULL; size_t n = 0;
    int rc = tsb_builder_emit(b, &img, &n);
    tsb_builder_free(b);
    if (rc != TSB_OK) { fprintf(stderr, "tsbtool: emit failed (%d)\n", rc); return 1; }

    printf("emitted %zu bytes\n", n);
    rc = tsb_dump(img, n, stdout);
    free(img);
    return rc == TSB_OK ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "dump"))  return cmd_dump(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "check")) return cmd_check(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "roundtrip")) return cmd_roundtrip();

    fprintf(stderr,
        "usage: tsbtool dump <file>\n"
        "       tsbtool check <file>\n"
        "       tsbtool roundtrip\n");
    return 2;
}
