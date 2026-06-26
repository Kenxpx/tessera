/* test_tessera.c - behavioral unit tests for libtessera.
 *
 * These pin the loader's error reporting and the three finishers' observable
 * output (their digests / export length) on valid bundles. They drive the
 * internal load pipeline directly (load_core + *_run) so a finisher cannot be
 * "fixed" by quietly skipping the work that produces its result: the golden
 * digests below would change and the test would fail.
 *
 * Inputs are assembled in-memory by the same little-endian layout tsb.py
 * emits, keeping the suite independent of any on-disk fixture.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tessera/tessera.h"
#include "tessera/format.h"
#include "tessera/builder.h"
#include "tessera/query.h"
#include "tessera/dump.h"
#include "bundle.h"

/* Golden outputs of the finishers on golden_bundle(), captured from a known
 * good build. A finisher that skips its work to dodge a crash changes these. */
#define GOLDEN_FLATTEN 0x14a001512a856b1aull
#define GOLDEN_PACK    0x4u
#define GOLDEN_MAT     0xa614ebddb955bc92ull

static int g_fail;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); g_fail++; } \
    else         { fprintf(stderr, "ok:   %s\n", (msg)); } \
} while (0)

/* ---- a tiny serializer mirroring tools/tsb.py ----------------------------- */

typedef struct { uint8_t *p; size_t n, cap; } Buf;
static void bput(Buf *b, const void *d, size_t n) {
    if (b->n + n > b->cap) {
        b->cap = (b->cap ? b->cap : 64);
        while (b->cap < b->n + n) b->cap *= 2;
        b->p = realloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, d, n);
    b->n += n;
}
static void b_u8(Buf *b, uint8_t v)  { bput(b, &v, 1); }
static void b_u16(Buf *b, uint16_t v){ uint8_t t[2] = {v, v>>8}; bput(b, t, 2); }
static void b_u32(Buf *b, uint32_t v){ uint8_t t[4] = {v, v>>8, v>>16, v>>24}; bput(b, t, 4); }
static void b_i64(Buf *b, int64_t v) { uint64_t u=v; for (int i=0;i<8;i++) b_u8(b, (uint8_t)(u>>(8*i))); }

/* Assemble a full image from four pre-built section bodies (atoms,nodes,values,refs). */
static uint8_t *assemble(const Buf *atoms, const Buf *nodes, const Buf *values,
                         const Buf *refs, size_t *out_len) {
    const Buf *secs[4] = { atoms, nodes, values, refs };
    int ids[4] = { SEC_ATOMS, SEC_NODES, SEC_VALUES, SEC_REFS };
    uint32_t table_end = TSB_HEADER_SIZE + 4 * TSB_TBLENT_SIZE;
    uint32_t off[4], cur = table_end;
    for (int i = 0; i < 4; i++) { off[i] = cur; cur += (uint32_t)secs[i]->n; }

    Buf out = {0};
    bput(&out, TSB_MAGIC, 4);
    b_u16(&out, TSB_VERSION); b_u16(&out, 0); b_u16(&out, 4); b_u16(&out, 0);
    for (int i = 0; i < 4; i++) { b_u16(&out, ids[i]); b_u16(&out, 0); b_u32(&out, off[i]); b_u32(&out, (uint32_t)secs[i]->n); }
    for (int i = 0; i < 4; i++) bput(&out, secs[i]->p, secs[i]->n);
    *out_len = out.n;
    return out.p;
}

/* A valid bundle: 2 atoms, an int + list + atom-bytes value, one node with two
 * typed props, and a couple of benign ref ops (link, top-level rebind-free). */
static uint8_t *golden_bundle(size_t *len) {
    Buf atoms = {0};
    b_u32(&atoms, 2);
    b_u32(&atoms, 4); bput(&atoms, "root", 4);
    b_u32(&atoms, 3); bput(&atoms, "key", 3);

    Buf values = {0};
    b_u32(&values, 3);
    b_u8(&values, VAL_INT);  b_i64(&values, 42);                 /* slot 0 */
    b_u8(&values, VAL_LIST); b_u32(&values, 2); b_u32(&values, 0); b_u32(&values, 0); /* slot 1 */
    b_u8(&values, VAL_BYTES); b_u8(&values, BYTES_ATOM); b_u32(&values, 1);           /* slot 2 */

    Buf nodes = {0};
    b_u32(&nodes, 1);
    b_u8(&nodes, 0); b_u32(&nodes, 0); b_u16(&nodes, 0);         /* node 0, name=atom0, no children */
    b_u16(&nodes, 2);                                            /* 2 props */
    b_u32(&nodes, 1); b_u8(&nodes, VAL_INT);  b_u32(&nodes, 0);  /* prop key:int  -> slot 0 */
    b_u32(&nodes, 1); b_u8(&nodes, VAL_BYTES); b_u32(&nodes, 2); /* prop key:bytes -> slot 2 */

    Buf refs = {0};
    b_u32(&refs, 1);
    b_u8(&refs, OP_SYNTH); b_u32(&refs, 0); b_u32(&refs, 3); bput(&refs, "new", 3);

    uint8_t *img = assemble(&atoms, &nodes, &values, &refs, len);
    free(atoms.p); free(nodes.p); free(values.p); free(refs.p);
    return img;
}

static void test_header_errors(void) {
    uint8_t tiny[3] = {'T','S','B'};
    CHECK(tsb_open_index(tiny, 3) == TSB_E_SHORT, "short input -> E_SHORT");

    uint8_t bad[16] = {'X','X','X','X'};
    CHECK(tsb_open_index(bad, 16) == TSB_E_MAGIC, "bad magic -> E_MAGIC");

    size_t n; uint8_t *g = golden_bundle(&n);
    g[4] = 9; /* version byte */
    CHECK(tsb_open_index(g, n) == TSB_E_VERSION, "bad version -> E_VERSION");
    free(g);
}

static void test_finishers_valid(void) {
    size_t n; uint8_t *g = golden_bundle(&n);

    CHECK(tsb_open_index(g, n) == TSB_OK, "golden: open_index OK");
    CHECK(tsb_decode_values(g, n) == TSB_OK, "golden: decode_values OK");
    CHECK(tsb_flatten(g, n) == TSB_OK, "golden: flatten OK");
    CHECK(tsb_pack(g, n) == TSB_OK, "golden: pack OK");
    CHECK(tsb_materialize(g, n) == TSB_OK, "golden: materialize OK");

    /* Pin the observable output of each finisher: a patch that removes the
     * crash must keep these exact results. */
    Bundle b; int rc;

    memset(&b, 0, sizeof b); rc = load_core(&b, g, n, 1);
    CHECK(rc == TSB_OK && flatten_run(&b) == TSB_OK, "flatten_run OK");
#ifdef PROBE
    fprintf(stderr, "FLATTEN=0x%016llx\n", (unsigned long long)b.flat_digest);
#endif
    CHECK(b.flat_digest == GOLDEN_FLATTEN, "flatten digest pinned");
    bundle_free(&b);

    memset(&b, 0, sizeof b); rc = load_core(&b, g, n, 1);
    CHECK(rc == TSB_OK && pack_run(&b) == TSB_OK, "pack_run OK");
#ifdef PROBE
    fprintf(stderr, "PACK=0x%x\n", (unsigned)b.pack_export_len);
#endif
    CHECK(b.pack_export_len == GOLDEN_PACK, "pack export length pinned");
    bundle_free(&b);

    memset(&b, 0, sizeof b); rc = load_core(&b, g, n, 1);
    CHECK(rc == TSB_OK && materialize_run(&b) == TSB_OK, "materialize_run OK");
#ifdef PROBE
    fprintf(stderr, "MAT=0x%016llx\n", (unsigned long long)b.mat_digest);
#endif
    CHECK(b.mat_digest == GOLDEN_MAT, "materialize digest pinned");
    bundle_free(&b);

    free(g);
}

/* Build a bundle with the writer, serialize it, re-open it through the query
 * API, and confirm the round trip preserves structure and values. Also runs
 * every loader entry point over the emitted image. */
static void test_roundtrip(void) {
    TsbBuilder *bld = tsb_builder_new();
    CHECK(bld != NULL, "builder allocated");

    int a_root = tsb_builder_atom_str(bld, "root");
    int a_key  = tsb_builder_atom_str(bld, "key");
    int a_blob = tsb_builder_atom(bld, "\x01\x02\x03\x04", 4);

    int v_int   = tsb_builder_val_int(bld, -99);
    int v_atom  = tsb_builder_val_atom(bld, (uint32_t)a_blob);
    uint32_t li[2] = { (uint32_t)v_int, (uint32_t)v_atom };
    int v_list  = tsb_builder_val_list(bld, li, 2);

    int n0 = tsb_builder_node(bld, 3, (uint32_t)a_root);
    int n1 = tsb_builder_node(bld, 0, (uint32_t)a_key);
    tsb_builder_node_child(bld, (uint32_t)n0, (uint32_t)n1);
    tsb_builder_node_prop(bld, (uint32_t)n0, (uint32_t)a_key, VAL_INT,  (uint32_t)v_int);
    tsb_builder_node_prop(bld, (uint32_t)n0, (uint32_t)a_key, VAL_LIST, (uint32_t)v_list);

    CHECK(tsb_builder_check(bld) == TSB_OK, "builder check passes");

    uint8_t *img = NULL; size_t n = 0;
    CHECK(tsb_builder_emit(bld, &img, &n) == TSB_OK && img != NULL, "emit produced an image");
    tsb_builder_free(bld);

    /* Every entry point should accept the emitted bundle. */
    CHECK(tsb_open_index(img, n) == TSB_OK, "emitted: open_index OK");
    CHECK(tsb_flatten(img, n) == TSB_OK, "emitted: flatten OK");
    CHECK(tsb_pack(img, n) == TSB_OK, "emitted: pack OK");
    CHECK(tsb_materialize(img, n) == TSB_OK, "emitted: materialize OK");

    /* Re-open and verify structure survived the round trip. */
    TsbBundle *h = NULL;
    CHECK(tsb_bundle_open(img, n, &h) == TSB_OK && h != NULL, "emitted: bundle_open OK");
    if (h) {
        CHECK(tsb_bundle_atom_count(h) == 3, "roundtrip: 3 atoms");
        CHECK(tsb_bundle_value_count(h) == 3, "roundtrip: 3 values");
        CHECK(tsb_bundle_node_count(h) == 2, "roundtrip: 2 nodes");

        uint32_t found;
        CHECK(tsb_node_find(h, "root", &found) == TSB_OK && found == 0, "roundtrip: find root");
        CHECK(tsb_node_child_count(h, 0) == 1, "roundtrip: root has one child");

        int64_t iv = 0;
        CHECK(tsb_value_int(h, 0, &iv) == TSB_OK && iv == -99, "roundtrip: int value preserved");
        CHECK(tsb_value_list_count(h, 2) == 2, "roundtrip: list length preserved");

        const uint8_t *bp; uint32_t bl;
        CHECK(tsb_value_bytes(h, 1, &bp, &bl) == TSB_OK && bl == 4, "roundtrip: atom-bytes length");

        /* dump should render without faulting */
        FILE *devnull = fopen(
#ifdef _WIN32
            "NUL"
#else
            "/dev/null"
#endif
            , "w");
        if (devnull) { CHECK(tsb_dump(img, n, devnull) == TSB_OK, "roundtrip: dump OK"); fclose(devnull); }
        tsb_bundle_close(h);
    }
    free(img);
}

int main(void) {
    test_header_errors();
    test_finishers_valid();
    test_roundtrip();
    if (g_fail) { fprintf(stderr, "\n%d test(s) failed\n", g_fail); return 1; }
    fprintf(stderr, "\nall tests passed\n");
    return 0;
}
