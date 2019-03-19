#!bin/bash/
export CC=aarch64-linux-gnu-gcc
make clean -w
./configure
make
cd examples
./build.sh
cd ..
