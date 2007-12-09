#!/bin/bash

source ./configure

$CC $CFLAGS -I. -Iinclude -o test1 tests/tcctest.c &&
./test1 > test1.txt &&
./tinycc -I. -Iinclude -o test2 tests/tcctest.c &&
./test2 > test2.txt &&
diff -u test1.txt test2.txt

