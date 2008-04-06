#!/bin/bash

# Usage: ./make [ARCH]
#
# With no arguments, builds all targets.  Else build target(s) listed on
# command line.  Special target "native" builds a native compiler.

# Set "DEBUG=echo" to view the commands instead of running them.

source ./configure

TINYCC_VERSION=1.0.0-pre3

DOLOCAL="-B. -I./include -I."

# Add a compiler define to the cc argument list, set to a string containing
# the value of the environment variable of the same name.
function DEF()
{
  local DATA=$(eval "echo \$$1")
  if [ -z "$DEBUG" ]
  then
    # Add a -DSYMBOL="STRING" argument.  The shell eats some of the characters
    # but this gives the compiler the right arguments.
    echo -D$1=\""$DATA"\"
  else
    # Add an extra layer of quotes so that even after the shell eats one,
    # "echo" produces something that works if we cut and paste it.
    echo -D$1=\\\""$DATA"\\\"
  fi
}

# Invoke the compiler with all the appropriate arguments

function compile_tinycc()
{
  OUTFILE=$1
  shift
  TINYCC_TARGET="$ARCH"
  $DEBUG $CC $@ -o $OUTFILE $CFLAGS $LIBS \
    -DTINYCC_TARGET_$(echo $ARCH | tr a-z A-Z) \
    $(DEF TINYCC_TARGET) $(DEF TINYCC_VERSION) $(DEF TINYCC_INSTALLDIR) \
    $(DEF CC_CRTDIR) $(DEF CC_LIBPATH) $(DEF CC_HEADERPATH) \
    $(DEF CC_DYNAMIC_LINKER)
}

# Show command line before running it.
function show_verbose()
{
  SHOWIT=echo
  [ "$1" == "compile_tinycc" ] && SHOWIT=""
  [ ! -z "$VERBOSE" ] && DEBUG=echo $SHOWIT "$@"
  if [ "$VERBOSE" != "debug" ]
  then
    "$@"
  fi
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

  ARCH=$1 CC_LIBPATH="$L" show_verbose compile_tinycc $1-tinycc_unstripped \
    tcc.c options.c &&
  show_verbose $STRIP $1-tinycc_unstripped -o $1-tinycc

  [ $? -ne 0 ] && exit 1

  # If the compiler we just built works as a native compiler for this host,
  # create a "tinycc" symlink pointing to it

  if [ "$1" == "$HOST" ]
  then
    show_verbose rm -f tinycc
    show_verbose ln -s $1-tinycc tinycc || exit 1
  fi

  # Compile tinycc engine as a shared library.

  ARCH=$1 show_verbose compile_tinycc libtinycc-$1.so -shared -fPIC -DLIBTCC \
    tcc.c &&

  # If we're building a tinycc binary we can't run, build one we _can_ run
  # that outputs binaries for the same target.  We need it to build a target
  # version of the runtime library (libtinyccrt) with.

  if [ "$CC" != "$HOSTCC" ]
  then
    LIBCC=host-$1-tinycc
    ARCH=$1 CC="$HOSTCC" CC_LIBPATH="$L" show_verbose compile_tinycc $LIBCC \
      tcc.c options.c
  else
    LIBCC=$1-tinycc
  fi

  # Build libtinyccrt-$ARCH.a (which compiled programs link against).  This
  # contains support code such as alloca, bounds checking (if necessary),
  # and 64 bit math on 32 bit platforms.

  # XXX build this on all platforms

  if [ -f $1/alloca.S ]
  then
    show_verbose mkdir -p lib/$1
    show_verbose ./$LIBCC $DOLOCAL -o libtinycc1-$1.o -c libtinycc1.c &&
    show_verbose ./$LIBCC $DOLOCAL -o alloca-$1.o -c $1/alloca.S &&
    show_verbose ./$LIBCC $DOLOCAL -o bound-alloca-$1.o -c $1/bound-alloca.S &&
    show_verbose $AR rcs libtinyccrt-$1.a {libtinycc1,alloca,bound-alloca}-$1.o
  fi
}

# Figure out what target(s) to build for.

[ $# -ne 0 ] && TARGETS="$@"
[ "$TARGETS" == "native" ] && TARGETS="$HOST"
[ "$TARGETS" == "all" ] && TARGETS="i386 arm" #c67 win32
if [ -z "$TARGETS" ]
then
  echo "Usage: make.sh TARGET [TARGET...]" >&2
  echo "Targets: i386 arm (all native)" >&2
  exit 1
fi

# Build each architecture

for TARGET in $TARGETS
do
  echo Building for target: "$TARGET"
  build $TARGET || exit 1
done
