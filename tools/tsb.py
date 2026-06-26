"""tsb.py - a builder for the .tsb container format.

Used to generate proof-of-concept inputs and seed-corpus bundles. Mirrors the
on-disk layout documented in include/tessera/format.h. Everything is
little-endian.
"""
import struct

# section ids
SEC_ATOMS, SEC_NODES, SEC_VALUES, SEC_REFS = 1, 2, 3, 4

# value tags
VAL_INT, VAL_FLOAT, VAL_BYTES, VAL_LIST, VAL_REF, VAL_U128 = 1, 2, 3, 4, 5, 6
BYTES_INLINE, BYTES_ATOM = 0, 1

# ref ops
OP_LINK, OP_SYNTH, OP_CONCAT, OP_OPEN_SCOPE = 1, 2, 3, 4
OP_CLOSE_SCOPE, OP_REBIND_PROP, OP_BIND_ATOM, OP_ADOPT = 5, 6, 7, 8

MAGIC = b"TSB1"
VERSION = 1
HEADER_SIZE = 12
TBLENT_SIZE = 12


def _u16(x): return struct.pack("<H", x)
def _u32(x): return struct.pack("<I", x)
def _i64(x): return struct.pack("<q", x)
def _f64(x): return struct.pack("<d", x)


def atoms_section(atoms):
    """atoms: list[bytes]"""
    out = _u32(len(atoms))
    for a in atoms:
        out += _u32(len(a)) + a
    return out


def values_section(values):
    """values: list of tuples describing each slot:
       ('int', n) | ('float', x) | ('inline', b'..') | ('atom', aid)
       ('list', [idx,..]) | ('ref', r) | ('u128', b'16bytes')
    """
    out = _u32(len(values))
    for v in values:
        kind = v[0]
        if kind == 'int':
            out += bytes([VAL_INT]) + _i64(v[1])
        elif kind == 'float':
            out += bytes([VAL_FLOAT]) + _f64(v[1])
        elif kind == 'inline':
            out += bytes([VAL_BYTES, BYTES_INLINE]) + _u32(len(v[1])) + v[1]
        elif kind == 'atom':
            out += bytes([VAL_BYTES, BYTES_ATOM]) + _u32(v[1])
        elif kind == 'list':
            items = v[1]
            out += bytes([VAL_LIST]) + _u32(len(items))
            for it in items:
                out += _u32(it)
        elif kind == 'ref':
            out += bytes([VAL_REF]) + _u32(v[1])
        elif kind == 'u128':
            assert len(v[1]) == 16
            out += bytes([VAL_U128]) + v[1]
        else:
            raise ValueError(kind)
    return out


def nodes_section(nodes):
    """nodes: list of dicts {kind, name_id, children:[..], props:[(name_atom, decl_type, val_index)]}"""
    out = _u32(len(nodes))
    for nd in nodes:
        out += bytes([nd.get('kind', 0)])
        out += _u32(nd['name_id'])
        ch = nd.get('children', [])
        out += _u16(len(ch))
        for c in ch:
            out += _u32(c)
        props = nd.get('props', [])
        out += _u16(len(props))
        for (na, dt, vi) in props:
            out += _u32(na) + bytes([dt]) + _u32(vi)
    return out


def refs_section(refs):
    """refs: list of tuples, op-specific:
       ('link', a, b) ('synth', a, data) ('concat', a, b, c)
       ('open', a) ('close',) ('rebind', sel, slot) ('bind', slot, atom) ('adopt', slot)
    """
    out = _u32(len(refs))
    for r in refs:
        op = r[0]
        if op == 'link':
            out += bytes([OP_LINK]) + _u32(r[1]) + _u32(r[2])
        elif op == 'synth':
            out += bytes([OP_SYNTH]) + _u32(r[1]) + _u32(len(r[2])) + r[2]
        elif op == 'concat':
            out += bytes([OP_CONCAT]) + _u32(r[1]) + _u32(r[2]) + _u32(r[3])
        elif op == 'open':
            out += bytes([OP_OPEN_SCOPE]) + _u32(r[1])
        elif op == 'close':
            out += bytes([OP_CLOSE_SCOPE])
        elif op == 'rebind':
            out += bytes([OP_REBIND_PROP]) + _u32(r[1]) + _u32(r[2])
        elif op == 'bind':
            out += bytes([OP_BIND_ATOM]) + _u32(r[1]) + _u32(r[2])
        elif op == 'adopt':
            out += bytes([OP_ADOPT]) + _u32(r[1])
        else:
            raise ValueError(op)
    return out


def build(atoms=(), values=(), nodes=(), refs=(), flags=0):
    """Assemble a full .tsb image with the four core sections laid out
    sequentially after the section table."""
    secs = {
        SEC_ATOMS: atoms_section(list(atoms)),
        SEC_VALUES: values_section(list(values)),
        SEC_NODES: nodes_section(list(nodes)),
        SEC_REFS: refs_section(list(refs)),
    }
    sec_count = len(secs)
    table_end = HEADER_SIZE + sec_count * TBLENT_SIZE

    # lay sections out in id order right after the table
    order = [SEC_ATOMS, SEC_NODES, SEC_VALUES, SEC_REFS]
    offsets = {}
    cur = table_end
    for sid in order:
        offsets[sid] = cur
        cur += len(secs[sid])

    header = MAGIC + _u16(VERSION) + _u16(flags) + _u16(sec_count) + _u16(0)
    table = b""
    for sid in order:
        table += _u16(sid) + _u16(0) + _u32(offsets[sid]) + _u32(len(secs[sid]))

    body = b""
    for sid in order:
        body += secs[sid]

    return header + table + body
