#!/bin/sh

aclocal
autoconf
autoheader
libtoolize --automake --force
automake --add-missing
./configure --host=aarch64-hisiv610-linux --prefix=$(pwd)/../output/ CFLAGS='-mcpu=cortex-a53' CXXFlAGS='-mcpu=cortex-a53'
make clean
make
make install

