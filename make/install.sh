#!/bin/bash

source ./configure

# Install libraries

mkdir -p "$TINYCC_LIBDIR"
cp libtinycc-*.a "$TINYCC_LIBDIR"

# Install headers

mkdir -p "$TINYCC_HEADERDIR"
cp include/* "$TINYCC_HEADERDIR"

# Install binaries
cp *-tinycc /usr/local/bin
