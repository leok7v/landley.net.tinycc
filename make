#!/bin/bash

# Set lots of environment variables to default values, all of these are
# overridden by existing local variables.

[ -z "$CC" ] && CC=cc
[ -z "$AR" ] && AR=ar
[ -z "$STRIP" ] && STRIP=strip
[ -z "$LIBSUF" ] && LIBSUF=.a
[ -z "$EXESUF" ] && EXESUF=
[ -z "$CFLAGS" ] && CFLAGS="-g -Wall -fsigned-char -Os -fno-strict-aliasing"
[ -z "$LIBS" ] && LIBS="-lm -ldl"
[ -z "$ARCH" ] && ARCH="i386 arm c67 win32"

TINYCC_VERSION=1.2.3

DOLOCAL="-B. -I./include -I."

# Print help for any argument we don't recognize.

if [ "$#" -gt 0 ] && [ "$1" != "--fast" ] && [ "$1" != "--clean" ] &&
   [ "$1" != "--install" ] && [ "$1" != "--test" ]
then
  echo "Usage: ./make [--options]"
  echo "	--help		Display this help."
  echo "	--fast		Just build native tinycc."
  echo "	--clean		Remove temporary files."
  echo "	--test		Rebuild and run test suite."
  echo "	--install $TO	Install (must specify destination)."	
  exit 0
fi

# Handle --clean

if [ "$1" == "--clean" ]
then
  # Need to figure out how much of this is needed...
  rm -f *~ *.o *.a tinycc tinycc_unstripped tinycc.1 tcct tcc_g tcctest.ref
        *.bin *.i ex2 core gmon.out test.out test.ref a.out tcc_p \
           *.exe *.lib tcc.pod libtcc_test i386/*.o \
           tcctest[1234] test[1234].out tcc win32/lib/*.o
  exit 0
fi

# Build each architecture

for i in $ARCH
do
  # A --fast build skips everything but native compiler

  [ "$1" == "--fast" ] && [ "$i" != "$HOST" ] && continue

  $CC $CFLAGS $LIBS -DTCC_TARGET_$i '-DCC_LIB_PATH="/blah"' \
    -DTINYCC_VERSION='"'$TINYCC_VERSION'"' \
    -o ${i}-tinycc_unstripped tcc.c &&
  $STRIP ${i}-tinycc_unstripped -o ${i}-tinycc
  [ $? -ne 0 ] && exit 1

  # If this would be a native compiler for this host, create "tinycc" symlink
  #if [ "$i" == "$HOST" ]
  #then
  #  cp ${i}-tinycc tinycc
  #fi

  # Build libtcc1.a

  if [ -f $i/alloca.S ]
  then
    ./$i-tinycc TCC=./tcc -B. -I./include -I. -c -o libtinycc1.o libtinycc1.c &&
    $CC -c -o alloca.o $i/alloca.S &&
    $CC -c -o bound-alloca.o $i/bound-alloca.S &&
    $AR rcs libtcc-${i}.a libtcc1.o alloca.o bound-alloca.o
  fi
done

exit 0

# This is what the rest of the build did, remove this later when I'm sure
# it's doing it right.

gcc -O2 -Wall -c -o libtcc1.o libtcc1.c &&
gcc -c -o alloca.o $ARCH/alloca.S &&
gcc -c -o bound-alloca.o i386/bound-alloca.S &&
ar rcs libtcc1.a libtcc1.o alloca.o bound-alloca.o



gcc -O2 -g -Wall -fsigned-char -Os -fno-strict-aliasing -DTCC_TARGET_ARM -DTCC_ARM_EABI -o arm-tcc tcc.c -lm -ldl &&
gcc -O2 -g -Wall -fsigned-char -Os -fno-strict-aliasing -DTCC_TARGET_C67 -o c67-tcc tcc.c -lm -ldl &&
gcc -O2 -g -Wall -fsigned-char -Os -fno-strict-aliasing -DTCC_TARGET_PE -o i386-win32-tcc tcc.c -lm -ldl &&

gcc -O2 -Wall -c -o libtcc1.o libtcc1.c &&
gcc -c -o i386/alloca86.o i386/alloca86.S &&
gcc -c -o i386/bound-alloca86.o i386/bound-alloca86.S &&
ar rcs libtcc1.a libtcc1.o i386/alloca86.o i386/bound-alloca86.o &&

gcc -O2 -Wall -c -o bcheck.o bcheck.c &&
gcc -O2 -g -Wall -fsigned-char -Os -fno-strict-aliasing -DLIBTCC -c -o libtcc.o tcc.c &&
ar rcs libtcc.a libtcc.o &&
gcc -O2 -g -Wall -fsigned-char -Os -fno-strict-aliasing -o libtcc_test tests/libtcc_test.c libtcc.a -lm -ldl &&
texi2html -monolithic -number tcc-doc.texi &&
./texi2pod.pl tcc-doc.texi tcc.pod &&
pod2man --section=1 --center=" " --release=" " tcc.pod > tcc.1

