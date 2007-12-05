#!/bin/bash

source configure

# Need to figure out how much of this is needed...
$DEBUG rm -f *~ *.o *.a tinycc *-tinycc *-tinycc_unstripped tinycc.1 tcct \
      tcc_g tcctest.ref *.bin *.i ex2 core gmon.out test.out test.ref a.out \
      tcc_p *.exe *.lib tcc.pod libtcc_test i386/*.o \
      tcctest[1234] test[1234].out tcc win32/lib/*.o

