#!/usr/bin/env bash
# Extract a frozen workload's flat files and oracles:  ref/flat/extract.sh minio  ->  data/minio/
# (frozen by bottom-up's link/freeze.sh into /home/exedev/targets/frozen/<name>; hlink copy in ref/hlink)
set -euo pipefail
here=$(cd "$(dirname "$0")/../.." && pwd); w=$1
d=${FROZEN:-/home/exedev/targets/frozen}/$w
[ -x "$here/build/hlink" ] || "$here/ref/hlink/build.sh"
mapfile -t F < "$d/link.flags"
mkdir -p "$here/data"; rm -rf "$here/data/$w"
out=$(mktemp -d)
"$here/build/hlink" -nodetach -fresh -o "$out/a.out" -importcfg "$d/importcfg.link" "${F[@]}" -s -w \
  -dumplive "$here/data/$w.live.txt" -dumpflat "$here/data/$w" "$d/main.a"
"$out/a.out" --version > /dev/null 2>&1 || "$out/a.out" --help > /dev/null 2>&1 || true
mv "$here/data/$w.live.txt" "$here/data/$w/oracle/live_names.txt"
rm -rf "$out"
cp "$here/ref/flat/README.md" "$here/data/$w/README.md"
du -sh "$here/data/$w"
