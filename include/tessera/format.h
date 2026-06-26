/* tessera/format.h - on-disk constants for the .tsb container format.
 *
 * The format is little-endian. A 12-byte header is followed by a section
 * table; the four core sections (atoms, nodes, values, refs) are located by
 * id. See README.md for the high-level layout.
 */
#ifndef TESSERA_FORMAT_H
#define TESSERA_FORMAT_H

#define TSB_MAGIC      "TSB1"
#define TSB_MAGIC_LEN  4
#define TSB_VERSION    1

/* header/table sizes on disk */
#define TSB_HEADER_SIZE   12
#define TSB_TBLENT_SIZE   12

/* section ids */
#define SEC_ATOMS   1
#define SEC_NODES   2
#define SEC_VALUES  3
#define SEC_REFS    4
#define SEC_MAX_ID  4

/* value tags (tagged-union arms) */
#define VAL_INT     1
#define VAL_FLOAT   2
#define VAL_BYTES   3
#define VAL_LIST    4
#define VAL_REF     5
#define VAL_U128    6
#define VAL_MIN     1
#define VAL_MAX     6

/* VAL_BYTES sub-encodings */
#define BYTES_INLINE  0   /* u32 len + raw bytes, heap-copied (owned) */
#define BYTES_ATOM    1   /* u32 atom id, a borrowed view into the pool */

/* ref ops, applied in pass 2 by an operation-specific finisher */
#define OP_LINK         1   /* node -> node link */
#define OP_SYNTH        2   /* intern a literal synthesized atom */
#define OP_CONCAT       3   /* intern atom_a ++ atom_b (length resolved on apply) */
#define OP_OPEN_SCOPE   4   /* push a node scope */
#define OP_CLOSE_SCOPE  5   /* pop a node scope */
#define OP_REBIND_PROP  6   /* rebind a scoped property to another value slot */
#define OP_BIND_ATOM    7   /* promote a ref slot to a bytes view of an atom */
#define OP_ADOPT        8   /* hand pool ownership to a bytes value slot */
#define OP_MIN          1
#define OP_MAX          8

/* 4-byte alignment used for in-pool atom storage */
#define TSB_ALIGN 4

#endif /* TESSERA_FORMAT_H */
