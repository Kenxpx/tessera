# Build libtessera, the tsbtool CLI, and the test suite.
#
# This is for local development; the fuzzers are built separately by
# .clusterfuzzlite/build.sh under ClusterFuzzLite.

CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -g -Wall -Wextra
CPPFLAGS = -Iinclude -Isrc

LIB_SRC := $(wildcard src/*.c)
LIB_OBJ := $(LIB_SRC:.c=.o)

.PHONY: all test clean

all: libtessera.a tsbtool

libtessera.a: $(LIB_OBJ)
	$(AR) rcs $@ $(LIB_OBJ)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

tsbtool: tools/tsbtool.c libtessera.a
	$(CC) $(CFLAGS) $(CPPFLAGS) $< libtessera.a -o $@

test: tests/test_tessera.c libtessera.a
	$(CC) $(CFLAGS) $(CPPFLAGS) $< libtessera.a -o run_tests
	./run_tests

clean:
	rm -f $(LIB_OBJ) libtessera.a tsbtool run_tests
