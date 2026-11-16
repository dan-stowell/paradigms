#!/bin/bash
# Optional: build the compiler from source (needs cmake >= 3.27; the VM has none, so a pip venv provides it).
# Result: $TOOLS/build/goose (no gfx/audio/ui/physics packages: GOOSE_BUILTIN_LIBS=none).
set -e
TOOLS=${TOOLS:-/home/exedev/tools/goose}
[ -x "$TOOLS/cmakevenv/bin/cmake" ] || { python3 -m venv "$TOOLS/cmakevenv"; "$TOOLS/cmakevenv/bin/pip" install -q cmake; }
export PATH="$TOOLS/cmakevenv/bin:$PATH"
[ -d "$TOOLS/src" ] || git clone https://github.com/aardappel/goose "$TOOLS/src"
time {
  cmake -S "$TOOLS/src" -B "$TOOLS/build" -DCMAKE_BUILD_TYPE=Release -DGOOSE_BUILTIN_LIBS=none
  cmake --build "$TOOLS/build" -j8
}
"$TOOLS/build/goose" version
