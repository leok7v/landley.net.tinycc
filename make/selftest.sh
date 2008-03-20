#!/bin/bash

TARGET=$1
CCNAME=${1}-tinycc

if [ -z "$1" ]
then
  TARGET=native
  CCNAME=tinycc
fi

echo -e "\npass one: cc -o tinycc" &&
make/make.sh $TARGET &&
mv $CCNAME test1cc &&

echo -e "\npass two: tinycc -o tinycc" &&
CC=./test1cc make/make.sh $TARGET &&
mv $CCNAME test2cc &&

echo -e "\npass three: tinycc -o tinycc -o tinycc" &&
CC=./test2cc make/make.sh $TARGET &&

echo -e "\nSanity check." &&
./$CCNAME >/dev/null &&

echo success || echo test failed
