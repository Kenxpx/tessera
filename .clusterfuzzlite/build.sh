#!/bin/bash -eu
#
# ClusterFuzzLite build entry point for libtessera.
#
# Compiles the reader library into a static archive, then links each fuzz
# harness in fuzz/ against it with the configured fuzzing engine and
# sanitizer. Every harness binary is written into $OUT, and each one gets its
# per-target seed corpus packaged next to it.
#
# Paths are resolved from $SRC (the repository root, also the working
# directory), so the build is relocatable inside the CFL image.

INC="-I${SRC}/include -I${SRC}/src"
BUILD="${WORK:-${SRC}}/build"
mkdir -p "${BUILD}"

# --- library -> static archive ------------------------------------------------
LIB_OBJS=()
for c in "${SRC}"/src/*.c; do
    obj="${BUILD}/$(basename "${c%.c}").o"
    $CC $CFLAGS $INC -c "$c" -o "$obj"
    LIB_OBJS+=("$obj")
done
ar rcs "${BUILD}/libtessera.a" "${LIB_OBJS[@]}"

# --- harnesses ---------------------------------------------------------------
TARGETS="open_index decode_values flatten pack materialize"
for t in $TARGETS; do
    $CC $CFLAGS $INC -c "${SRC}/fuzz/fuzz_${t}.c" -o "${BUILD}/fuzz_${t}.o"
    $CXX $CXXFLAGS "${BUILD}/fuzz_${t}.o" "${BUILD}/libtessera.a" \
        $LIB_FUZZING_ENGINE -o "${OUT}/${t}_fuzzer"
done

# --- per-target seed corpora -------------------------------------------------
# Package corpus/<target>/ as <harness>_seed_corpus.zip (OSS-Fuzz convention).
for t in $TARGETS; do
    if [ -d "${SRC}/corpus/${t}" ]; then
        (cd "${SRC}/corpus/${t}" && zip -q -r "${OUT}/${t}_fuzzer_seed_corpus.zip" .)
    fi
done

# --- optional dictionary -----------------------------------------------------
if [ -f "${SRC}/fuzz/dictionary.txt" ]; then
    for t in $TARGETS; do
        cp "${SRC}/fuzz/dictionary.txt" "${OUT}/${t}_fuzzer.dict"
    done
fi
