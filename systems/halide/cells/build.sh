#!/bin/bash
# Build the Halide cells: generators (AOT, one .a per schedule variant) and the C++ drivers.
#   systems/halide/cells/build.sh [p1|p2|p3 ...]   (default: all)
set -euo pipefail
H=${HALIDE_ROOT:-/home/exedev/tools/halide/v21}
here=$(cd "$(dirname "$0")" && pwd)
B=${BUILD:-build}; F=${FEAT:-}  # FEAT=-profile: Halide profiler build (into BUILD=build-prof)
mkdir -p "$here/$B"; cd "$here/$B"
export LD_LIBRARY_PATH=$H/lib
CXX="g++ -std=c++17 -O2"
probs=${*:-p1 p2 p3}

# gen NAME GENERATOR FUNC [params...]: one AOT variant, appended to the variant table of problem NAME
gen() {
    local p=$1 g=$2 f=$3; shift 3
    ./gen_$p -g $g -f $f -o . target=host-no_runtime$F "$@"
    echo "#include \"$f.h\"" >> ${p}_variants.inc
    echo "$f" >> ${p}_$g.list
}
table() {  # table PROBLEM GENERATOR TYPENAME: the C table of variants of one generator
    local p=$1 g=$2 t=$3
    echo "static struct { const char *name; decltype(&$(head -1 ${p}_$g.list)) fn; } $t[] = {"
    while read f; do echo "    {\"$f\", $f},"; done < ${p}_$g.list
    echo "};"
    echo "static const int n${t^} = sizeof $t / sizeof $t[0];"
}

for p in $probs; do
    $CXX -fno-rtti -I$H/include "$here/gen_$p.cpp" $H/lib/libHalide_GenGen.a -L$H/lib -lHalide -lpthread -ldl -Wl,-rpath,$H/lib -o gen_$p
    rm -f ${p}_variants.inc ${p}_*.list
    case $p in
    p3)
        for c in 0 1 2 3; do for w in 0 1; do gen p3 layout layout_c${c}_w$w csched=$c wsched=$w; done; done
        for rf in 0 1; do for par in false true; do for vec in 0 32; do
            gen p3 relocate reloc_f${rf}_p${par:0:1}_v$vec rform=$rf par=$par vec=$vec; done; done; done
        { cat p3_variants.inc; table p3 layout layouts; table p3 relocate relocs; } > p3_variants.h
        ;;
    esac
    # one Halide runtime for all variants of this problem
    ./gen_$p -r runtime_$p -o . target=host$F
    $CXX -march=native -I. -I$H/include "$here/$p.cpp" $(sed 's/.*"\(.*\)\.h"/\1.a/' ${p}_variants.inc) runtime_$p.a -lpthread -ldl -o $p
    echo "built $p"
done
