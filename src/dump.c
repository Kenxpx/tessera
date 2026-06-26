/* dump.c - textual bundle rendering (see include/tessera/dump.h).
 *
 * Built entirely on the public query API so it stays a pure reader: open the
 * bundle, walk each section, print. Bytes are shown as a short printable/hex
 * preview rather than dumped in full.
 */
#include "tessera/dump.h"
#include "tessera/query.h"
#include "tessera/tessera.h"
#include "tessera/format.h"

#include <ctype.h>

static const char *kind_name(uint8_t k) {
    switch (k) {
    case VAL_INT:   return "int";
    case VAL_FLOAT: return "float";
    case VAL_BYTES: return "bytes";
    case VAL_LIST:  return "list";
    case VAL_REF:   return "ref";
    case VAL_U128:  return "u128";
    default:        return "?";
    }
}

/* Print up to `cap` bytes as text, escaping non-printables as \xNN. */
static void print_preview(FILE *out, const uint8_t *p, uint32_t len, uint32_t cap) {
    uint32_t n = len < cap ? len : cap;
    fputc('"', out);
    for (uint32_t i = 0; i < n; i++) {
        uint8_t c = p[i];
        if (isprint(c) && c != '"' && c != '\\') fputc(c, out);
        else fprintf(out, "\\x%02x", c);
    }
    fputc('"', out);
    if (len > cap) fprintf(out, "... (%u bytes)", len);
}

static void dump_atoms(FILE *out, const TsbBundle *h) {
    uint32_t n = tsb_bundle_atom_count(h);
    fprintf(out, "atoms: %u\n", n);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t len = 0;
        const uint8_t *p = tsb_bundle_atom(h, i, &len);
        fprintf(out, "  [%u] ", i);
        print_preview(out, p, len, 32);
        fputc('\n', out);
    }
}

static void dump_value(FILE *out, const TsbBundle *h, uint32_t i) {
    uint8_t k = tsb_value_kind(h, i);
    fprintf(out, "  [%u] %s", i, kind_name(k));
    switch (k) {
    case VAL_INT: {
        int64_t v; tsb_value_int(h, i, &v);
        fprintf(out, " = %lld", (long long)v);
        break;
    }
    case VAL_FLOAT: {
        double v; tsb_value_float(h, i, &v);
        fprintf(out, " = %g", v);
        break;
    }
    case VAL_BYTES: {
        const uint8_t *p; uint32_t len;
        tsb_value_bytes(h, i, &p, &len);
        fputc(' ', out);
        print_preview(out, p, len, 24);
        break;
    }
    case VAL_LIST: {
        uint32_t c = tsb_value_list_count(h, i);
        fprintf(out, " [");
        for (uint32_t j = 0; j < c; j++) {
            uint32_t it; tsb_value_list_item(h, i, j, &it);
            fprintf(out, "%s%u", j ? ", " : "", it);
        }
        fputc(']', out);
        break;
    }
    case VAL_REF: {
        uint32_t r; tsb_value_ref(h, i, &r);
        fprintf(out, " -> slot %u", r);
        break;
    }
    default:
        break;
    }
    fputc('\n', out);
}

static void dump_nodes(FILE *out, const TsbBundle *h) {
    uint32_t n = tsb_bundle_node_count(h);
    fprintf(out, "nodes: %u\n", n);
    for (uint32_t i = 0; i < n; i++) {
        uint8_t kind = 0; tsb_node_kind(h, i, &kind);
        uint32_t nlen = 0;
        const uint8_t *name = tsb_node_name(h, i, &nlen);
        fprintf(out, "  node[%u] kind=%u name=", i, kind);
        print_preview(out, name, nlen, 32);
        uint32_t cc = tsb_node_child_count(h, i);
        if (cc) {
            fprintf(out, " children=[");
            for (uint32_t c = 0; c < cc; c++) {
                uint32_t ch; tsb_node_child(h, i, c, &ch);
                fprintf(out, "%s%u", c ? ", " : "", ch);
            }
            fputc(']', out);
        }
        fputc('\n', out);
        uint32_t pc = tsb_node_prop_count(h, i);
        for (uint32_t p = 0; p < pc; p++) {
            uint32_t na, vi; uint8_t dt;
            tsb_node_prop(h, i, p, &na, &dt, &vi);
            fprintf(out, "    .prop name_atom=%u type=%s -> value[%u]\n",
                    na, kind_name(dt), vi);
        }
    }
}

int tsb_dump(const uint8_t *data, size_t size, FILE *out) {
    TsbBundle *h = NULL;
    int rc = tsb_bundle_open(data, size, &h);
    if (rc != TSB_OK) {
        fprintf(out, "tsb_dump: load failed (status %d)\n", rc);
        return rc;
    }

    fprintf(out, "tessera bundle: %u atoms, %u values, %u nodes, %u refs\n",
            tsb_bundle_atom_count(h), tsb_bundle_value_count(h),
            tsb_bundle_node_count(h), tsb_bundle_ref_count(h));
    dump_atoms(out, h);
    fprintf(out, "values: %u\n", tsb_bundle_value_count(h));
    for (uint32_t i = 0; i < tsb_bundle_value_count(h); i++) dump_value(out, h, i);
    dump_nodes(out, h);

    tsb_bundle_close(h);
    return TSB_OK;
}
