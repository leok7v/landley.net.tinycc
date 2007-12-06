#!/bin/bash

# Usage: ./make [ARCH]
#
# With no arguments, builds all targets.  Else build target(s) listed on
# command line.  Special target "native" builds a native compiler.

TINYCC_VERSION=0.9.25

DOLOCAL="-B. -I./include -I."

function build()
{
  source ./configure -v

  # Build tinycc with a specific architecture and search paths.

  $DEBUG $CC tcc.c -o ${TARGET}-tinycc_unstripped $CFLAGS $LIBS \
    -DTINYCC_TARGET_$1 \
    -DTINYCC_TARGET=$1 \
    -DTINYCC_VERSION=$TINYCC_VERSION \
    -DTINYCC_LIBDIR=$TINYCC_LIBDIR \
    -DCC_CRTDIR=$CC_CRTDIR \
    -DCC_LIBPATH=$CC_LIBPATH \
    -DCC_HEADERPATH=$CC_HEADERPATH &&
  $DEBUG $STRIP ${TARGET}-tinycc_unstripped -o ${TARGET}-tinycc
  [ $? -ne 0 ] && exit 1

  # If this would be a native compiler for this host, create "tinycc" symlink
  if [ "$1" == "$HOST" ]
  then
    $DEBUG rm -f tinycc
    $DEBUG ln -s ${TARGET}-tinycc tinycc
  fi

  # Build libtinycc1.a

  if [ -f $TARGET/alloca.S ]
  then
    $DEBUG ./$TARGET-tinycc $DOLOCAL -o libtinycc1.o -c libtinycc1.c &&
    $DEBUG ./$TARGET-tinycc $DOLOCAL -o alloca.o -c $TARGET/alloca.S &&
    $DEBUG ./$TARGET-tinycc $DOLOCAL -o bound-alloca.o -c $TARGET/bound-alloca.S &&
    $DEBUG $AR rcs libtinycc-${TARGET}.a libtinycc1.o alloca.o bound-alloca.o
  fi
}

# Figure out what target(s) to build for.

[ $# -ne 0 ] && TARGETS="$@"
[ "$TARGETS" == "native" ] && TARGETS="$HOST"
[ -z "$TARGETS" ] && TARGETS="i386 arm c67 win32"

# Build each architecture

for TARGET in $TARGETS
do
  build $TARGET || exit 1
done
