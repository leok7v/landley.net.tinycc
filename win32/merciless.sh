#!/bin/bash

# Download and install a prebuilt binary mingw toolchain, suitable for running
# under wine to test win32 output and win32 host.

mkdir mongo
cd mongo
wget http://downloads.sf.net/mingw/binutils-2.17.50-20060824-1.tar.gz
wget http://downloads.sf.net/mingw/gcc-core-3.4.5-20060117-1.tar.gz
wget http://downloads.sf.net/mingw/mingw-runtime-3.14.tar.gz
wget http://downloads.sf.net/mingw/w32api-3.10.tar.gz
tar xvzf binutils-*.tar.gz
tar xvzf gcc-core-*.tar.gz
tar xvzf mingw-runtime-*.tar.gz
tar xvzf w32api-*.tar.gz

# Now run it with something like the following
#   wine cmd
#   path %path%;z:\home\landley\mongo\bin
#   gcc hello.c
