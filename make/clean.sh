#!/bin/bash

source configure

# Need to figure out how much of this is needed...
$DEBUG rm -f *~ *.o *.a tinycc *-tinycc *-tinycc_unstripped tinycc.1 \
      tcctest.ref *.bin *.i ex2 core gmon.out test.out test.ref a.out \
      *.exe *.lib libtcc_test tcctest[1234] test[1234].out tcc win32/lib/*.o
