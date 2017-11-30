#!/bin/sh

aclocal
autoconf
autoheader
libtoolize --automake
automake --add-missing
./configure --host=aarch64-hisiv610-linux --prefix=$(pwd)/../output/A73/ CFLAGS='-g -O2 -mcpu=cortex-a73.cortex-a53' CXXFLAGS='-g -O2 -mcpu=cortex-a73.cortex-a53'
make clean
make
make install
