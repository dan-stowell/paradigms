#!/bin/bash
# Build the AOT generators and the driver against the official Halide 21.0.0 release tarball.
set -euo pipefail
H=${HALIDE_ROOT:-/home/exedev/tools/halide/v21}
cd "$(dirname "$0")"; mkdir -p build; cd build
export LD_LIBRARY_PATH=$H/lib
# 1. generator executable: gen.cpp + libHalide_GenGen.a (provides main)
g++ -std=c++17 -O2 -fno-rtti -I$H/include ../gen.cpp $H/lib/libHalide_GenGen.a -L$H/lib -lHalide -lpthread -ldl -Wl,-rpath,$H/lib -o gen
# 2. run it once per pipeline variant (AOT: emits NAME.a + NAME.h)
./gen -g gather -f gather -o . target=host
./gen -g hist   -f hist0  -o . target=host variant=0
./gen -g hist   -f hist1  -o . target=host variant=1
./gen -g hist   -f hist2  -o . target=host variant=2
./gen -g scan   -f scan0  -o . target=host variant=0
./gen -g scan   -f scan1  -o . target=host variant=1
# 3. driver + data generator
g++ -std=c++17 -O2 -I. -I$H/include ../driver.cpp gather.a hist0.a hist1.a hist2.a scan0.a scan1.a -lpthread -ldl -o driver
g++ -O2 ../mkdata.cpp -o mkdata
echo built
