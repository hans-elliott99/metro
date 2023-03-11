#!/usr/bin/bash

mkdir -p obj

set -x

CC=gcc

MAIN="main.c"
EXE="main"

PKGS=""
CFLAGS="-Wall -Wextra -std=c11 -pedantic -ggdb"
LIBS=-lm #include <math.h>
SRC=""


$CC $CFLAGS -o obj/$EXE $MAIN $SRC $LIBS
#$CC $CFLAGS `pkg-config --cflags $PKGS` -o $EXE $MAIN $SRC $LIBS `pkg-config --libs $PKGS`

./obj/$EXE
