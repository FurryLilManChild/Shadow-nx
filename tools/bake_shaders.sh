#!/bin/bash
# bake_shaders.sh <out dir> <shadercache dir>...: the release's shader cache. Every SPIR-V module from the given
# caches (a console's sdmc:/switch/shadow/shadercache, a PC's build-pc/shadercache) through the same SPIR-V to GLSL
# (switch/dk_glsl.cpp) and uam the console uses, written as <hash>_2.spv and <hash>_2.dk2 pairs: the console's
# start pass (deko.cpp Precompile) then has nothing left to compile. A .dk2 a console made is kept as it is and
# compared with the baked one (the report says how many match). Runs on Linux (the cloud's Docker image has gcc).
# Game derived: the output stays out of git (installer/release/shadercache is ignored).
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
out=$1; shift
build=${BAKE_BUILD:-$here/build-dk-linux}
cmake -S "$here/tools/dk_shaders" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
ninja -C "$build" dk_shaders >/dev/null
work=$(mktemp -d)
mkdir -p "$work/spv" "$work/baked" "$out"
for d in "$@"; do
    [ -d "$d" ] || continue
    for k in "$d"/*.spv "$d"/*.dk2; do
        [ -e "$k" ] || continue
        [ -e "$work/spv/$(basename "$k")" ] || cp "$k" "$work/spv/"
    done
done
"$build/dk_shaders" "$work/spv" "$work/baked" > "$work/report.txt" || true
tail -4 "$work/report.txt"
same=0; differ=0; baked=0; failed=0
for s in "$work"/spv/*.spv; do
    n=$(basename "$s" .spv)
    cp "$s" "$out/$n.spv"
    if [ -e "$work/spv/$n.dk2" ]; then
        if [ -e "$work/baked/$n.dksh" ] && cmp -s "$work/spv/$n.dk2" "$work/baked/$n.dksh"; then same=$((same+1)); else differ=$((differ+1)); fi
        cp "$work/spv/$n.dk2" "$out/$n.dk2"
    elif [ -e "$work/baked/$n.dksh" ]; then
        cp "$work/baked/$n.dksh" "$out/$n.dk2"; baked=$((baked+1))
    else
        failed=$((failed+1)); echo "no DKSH for $n"
    fi
done
echo "bake: $(ls "$out"/*.spv | wc -l) shaders; console-made kept $((same+differ)) (byte-identical to the bake: $same, different: $differ); baked here $baked; failed $failed"
rm -rf "$work"
[ "$failed" = 0 ]
