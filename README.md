# tessera

A small, dependency-free C library for reading **`.tsb`** bundles — a compact
little-endian container that stores an interned blob pool, a tagged-union value
stream, an object/node graph, and a stream of deferred "ref ops" that are
applied by one of three finishers.

The format grew out of a build-cache serializer: a tool needed to hand a graph
of named objects (with typed properties and cross-references) between processes
cheaply, resolving names lazily and sharing string storage through an interned
pool. `libtessera` is the reader half — it loads an image and runs one slice of
the parse pipeline.

## Layout

```
include/tessera/      public API (tessera.h) and on-disk constants (format.h)
src/                  loader + the three finishers
  format.c              header + section table
  atoms.c               interned blob pool
  values.c              tagged-union value stream
  nodes.c               object/node table + binding validation
  refs.c                ref-op stream (loaded, not yet applied)
  flatten.c             finisher: resolve synthesized atoms, fold a digest
  pack.c                finisher: export the graph, optionally adopt the pool
  materialize.c         finisher: apply property rebinds, emit a digest
fuzz/                 one libFuzzer harness per entry point
corpus/               per-target seed bundles
tests/                behavioral unit tests
tools/                tsb.py — a builder for the format (used to make seeds)
.clusterfuzzlite/     build.sh + project.yaml for ClusterFuzzLite
```

## The format

Every image is `magic("TSB1") | version | flags | section-count | reserved`,
followed by a section table and four core sections located by id:

| id | section | holds |
|----|---------|-------|
| 1  | atoms   | interned, NUL-terminated, 4-byte-aligned blobs |
| 2  | nodes   | objects: kind, name (atom id), children, typed props |
| 3  | values  | tagged-union slots: int/float/bytes/list/ref/u128 |
| 4  | refs    | deferred ops applied in pass 2 by a finisher |

A `bytes` value is either an owned inline copy or a *borrowed* view into the
atom pool. A node property carries a declared type that is checked against its
value slot's kind once, at load time. The ref-op stream is range-checked when
loaded but only *applied* by the finisher whose op family it belongs to
(synthesis for flatten, scopes/adoption for pack, rebinds/bindings for
materialize). See `include/tessera/format.h` for the exact constants.

## API

```c
int tsb_open_index(const uint8_t *data, size_t size);     /* header only */
int tsb_decode_values(const uint8_t *data, size_t size);  /* + atoms + values */
int tsb_flatten(const uint8_t *data, size_t size);        /* full load + flatten */
int tsb_pack(const uint8_t *data, size_t size);           /* full load + pack */
int tsb_materialize(const uint8_t *data, size_t size);    /* full load + materialize */
```

All entry points are read-only with respect to their input buffer and return a
`tsb_status` code; they never take ownership of `data`. Allocation can be
redirected with `tsb_set_allocator()`.

## Building and testing

The library is plain C99 with no dependencies:

```sh
cc -Iinclude -Isrc -c src/*.c
cc -Iinclude -Isrc tests/test_tessera.c src/*.c -o test_tessera && ./test_tessera
```

## Fuzzing

Five harnesses live in `fuzz/`, one per entry point. They build under
ClusterFuzzLite via `.clusterfuzzlite/build.sh`, which compiles the library
into an archive, links each harness against the configured engine, and packages
the per-target seed corpus from `corpus/`.
