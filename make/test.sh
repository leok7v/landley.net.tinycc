#!/bin/bash

source ./configure

$CC $CFLAGS -I. -Iinclude -o test1 tests/tcctest.c &&
./test1 > test1.out &&
./tinycc -I. -Iinclude -o test2 tests/tcctest.c &&
./test2 > test2.out &&
diff -u test1.out test2.out
[ $? -eq 0 ] && echo Test passed.
