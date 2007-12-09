#!/bin/bash

source configure

# Need to figure out how much of this is needed...
$DEBUG rm -f *~ *.o *.a tinycc *-tinycc *-tinycc_unstripped a.out \
      test? test?.out \
      libtcc_test tinycc.1 *.bin *.i ex2 core gmon.out *.exe *.lib win32/lib/*.o
