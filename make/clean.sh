#!/bin/bash

source configure

# Need to figure out how much of this is needed...
$DEBUG rm -f *~ *.o *.a *.so tinycc *-tinycc *-tinycc_unstripped a.out \
      test[0-9] test[0-9].out \
      libtcc_test tinycc.1 *.bin *.i ex2 core gmon.out *.exe *.lib win32/lib/*.o
