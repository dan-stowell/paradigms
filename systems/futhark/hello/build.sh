#!/bin/bash
# Build C and multicore library variants + driver + generator. Needs futhark on PATH (~/.local/bin/futhark).
set -e
cd "$(dirname "$0")"
mkdir -p build
cd build
for be in c multicore; do
  futhark $be --library -o hello_$be ../hello.fut
  mkdir -p $be
  cp hello_$be.h $be/hello.h
  extra=""
  [ $be = multicore ] && extra="-DMULTICORE"
  gcc -O3 -march=native $extra -I$be -o driver_$be ../driver.c hello_$be.c -lm -lpthread
done
gcc -O2 -o gen ../gen.c
