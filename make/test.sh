#!/bin/bash

source ./configure

$DEBUG $CC $CFLAGS -I. -Iinclude -o test1 tests/tcctest.c &&
$DEBUG ./test1 > test1.out &&
$DEBUG ./tinycc -I. -L. -Iinclude -o test2 tests/tcctest.c &&
$DEBUG ./test2 > test2.out &&
$DEBUG diff -u test1.out test2.out
if [ $? -eq 0 ]
then
  echo Tinycc test passed.
else
  exit 1
fi

$DEBUG $CC $CFLAGS -Iinclude -L . -ltinycc-i386 tests/libtcc_test.c -o libtcc_test &&
LD_LIBRARY_PATH=. $DEBUG ./libtcc_test
[ $? -eq 0 ] && echo libtinycc test passed.
