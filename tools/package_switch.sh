#!/bin/bash
# package_switch.sh <out.zip> [shadercache dir]: the Switch release zip. switch/shadow/ with shadow.nro, the default
# settings, LICENSES.txt, an empty game/ folder with a note, and the baked shader cache (tools/bake_shaders.sh) when
# given; README.md at the top. Nothing of the game goes in except the shader cache made from it.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
zip_out=$(realpath -m "$1"); cache=${2:-}
work=$(mktemp -d); d="$work/switch/shadow"
mkdir -p "$d/game"
cp "$here/build-switch/shadow.nro" "$here/switch/shadow-settings.toml" "$here/LICENSES.txt" "$d/"
printf 'Put your Shadow the Hedgehog disc (Xbox, USA) here: an .iso image of it (a trimmed XISO works),\nor its files (default.xbe with all its folders).\n' > "$d/game/PUT-THE-GAME-HERE.txt"
[ -n "$cache" ] && mkdir -p "$d/shadercache" && cp "$cache"/*.spv "$cache"/*.dk2 "$d/shadercache/"
cp "$here/README.md" "$work/"
rm -f "$zip_out"; (cd "$work" && zip -qr "$zip_out" README.md switch)
unzip -l "$zip_out" | tail -1; rm -rf "$work"
