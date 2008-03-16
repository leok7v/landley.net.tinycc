#!/bin/bash

source ./configure

if [ -z "$TINYCC_INSTALLDIR" ]
then
  echo 'No $TINYCC_INSTALLDIR' >&2
  exit 1
fi

# Install libraries and headers

$DEBUG mkdir -p "$TINYCC_INSTALLDIR"/{lib,include} &&
$DEBUG cp libtinyccrt-*.a libtinycc-*.so "$TINYCC_INSTALLDIR"/lib &&
$DEBUG cp include/* "$TINYCC_INSTALLDIR"/include &&

# Install binaries
$DEBUG cp *-tinycc "$PREFIX"/bin
