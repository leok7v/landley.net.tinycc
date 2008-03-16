#!/bin/bash

# Usage: ./make [ARCH]
#
# With no arguments, builds all targets.  Else build target(s) listed on
# command line.  Special target "native" builds a native compiler.

# Set "DEBUG=echo" to view the commands instead of running them.

source ./configure

TINYCC_VERSION=1.0.0-pre3

DOLOCAL="-B. -I./include -I."

# Invoke the compiler with all the appropriate arguments

function compile_tinycc()
{
  OUTFILE=$1
  shift
  $DEBUG $CC $@ -o $OUTFILE $CFLAGS $LIBS \
    -DTINYCC_TARGET_$(echo $ARCH | tr a-z A-Z) \
    -DTINYCC_TARGET='"'$ARCH'"' \
    -DTINYCC_VERSION='"'$TINYCC_VERSION'"' \
    -DTINYCC_INSTALLDIR='"'$TINYCC_INSTALLDIR'"' \
    -DCC_CRTDIR='"'$CC_CRTDIR'"' \
    -DCC_LIBPATH='"'$CC_LIBPATH'"' \
    -DCC_HEADERPATH='"'$CC_HEADERPATH'"' \
    -DCC_DYNAMIC_LINKER='"'$CC_DYNAMIC_LINKER'"'
}


function build()
{
  # The path at which the new compiler should search for system libraries is,
  # alas, target dependent.

  if [ -z "$CC_LIBPATH" ]
  then
    L="lib"
    [ "$HOST" == "x86_64" ] && [ TARGET="i386" ] && L="lib32"
    L="/usr/local/$L:/usr/$L:/$L"
  else
    L="$CC_LIBPATH"
  fi

  # Build tinycc with a specific architecture and search paths.

  ARCH=$1 CC_LIBPATH="$L" compile_tinycc $1-tinycc_unstripped tcc.c options.c &&
  $DEBUG $STRIP $1-tinycc_unstripped -o $1-tinycc

  [ $? -ne 0 ] && exit 1

  # If this would be a native compiler for this host, create "tinycc" symlink
  if [ "$1" == "$HOST" ]
  then
    $DEBUG rm -f tinycc
    $DEBUG ln -s $1-tinycc tinycc
  fi

  # Compile tinycc as a shared library.

  ARCH=$1 compile_tinycc libtinycc-$1.so -shared -fPIC -DLIBTCC tcc.c &&

  # Build libtinyccrt-$ARCH.a (which compiled programs link against)

  if [ -f $1/alloca.S ]
  then
    $DEBUG mkdir -p lib/$1
    $DEBUG ./$1-tinycc $DOLOCAL -o libtinycc1-$1.o -c libtinycc1.c &&
    $DEBUG ./$1-tinycc $DOLOCAL -o alloca-$1.o -c $1/alloca.S &&
    $DEBUG ./$1-tinycc $DOLOCAL -o bound-alloca-$1.o -c $1/bound-alloca.S &&
    $DEBUG $AR rcs libtinyccrt-$1.a {libtinycc1,alloca,bound-alloca}-$1.o
  fi
}

# Figure out what target(s) to build for.

[ $# -ne 0 ] && TARGETS="$@"
[ "$TARGETS" == "native" ] && TARGETS="$HOST"
[ "$TARGETS" == "all" ] && TARGETS="i386 arm c67 win32"
if [ -z "$TARGETS" ]
then
  echo "Usage: make.sh TARGET [TARGET...]" >&2
  echo "Targets: i386 arm c67 win32 (all native)" >&2
  exit 1
fi

# Build each architecture

for TARGET in $TARGETS
do
  echo Building for target: "$TARGET"
  build $TARGET || exit 1
done
