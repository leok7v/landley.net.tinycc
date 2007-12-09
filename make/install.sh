#!/bin/bash

source ./configure

if [ -z "$TINYCC_INSTALLDIR" ]
then
  No TINYCC_INSTALLDIR
  exit 1
fi

# Install libraries and headers

$DEBUG mkdir -p "$TINYCC_INSTALLDIR"/{lib,include} &&
$DEBUG cp libtinycc-*.a "$TINYCC_INSTALLDIR"/lib &&
$DEBUG cp include/* "$TINYCC_INSTALLDIR"/include &&

# Install binaries
$DEBUG cp *-tinycc /usr/local/bin
