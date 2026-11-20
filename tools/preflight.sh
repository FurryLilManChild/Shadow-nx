#!/bin/bash
# preflight.sh: gate 2, run in the cloud before a Switch build goes to the tester. Each check prints PASS or FAIL and
# the script ends with PREFLIGHT PASSED or PREFLIGHT FAILED. Builds the NRO from the checked out commit (Docker image
# ghcr.io/danfromtico/switch-dev-legacy-vulkan, /home/claude/switch_build.sh) unless SKIP_BUILD=1.
set -u
cd "$(dirname "$0")/.."
fail=0
check() { if eval "$2"; then echo "PASS  $1"; else echo "FAIL  $1"; fail=1; fi; }
check "fetched origin" "git fetch -q origin"
check "on main" '[ "$(git rev-parse --abbrev-ref HEAD)" = main ]'
check "no uncommitted changes to tracked files" '[ -z "$(git status --porcelain --untracked-files=no)" ]'
check "HEAD is origin/main (gate 1: only the latest pushed commit is built)" '[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ]'
check "no game data or derived files tracked" '! git ls-files | grep -qiE "^(assets/|game/assets/|game/generated/|out/|installer/release/shadercache/)|\.(xbe|iso|xiso|spv|dk2|dksh)$|(^|/)(shadercache/|prof[.0-9]*\.bin$)"'
t=switch/shadow-settings.toml
for kv in "profile = false" "wait_monitor = false" "trace_threads = false" "self_test = false" "screenshot_button = false" "guest_one_core = true"; do
    check "settings: $kv" "grep -qx '$kv' $t"
done
check "version 0.5.0 in the build line and the NRO" 'grep -q "switch-0.5.0" switch/CMakeLists.txt && grep -q "VERSION \"0.5.0\"" switch/CMakeLists.txt'
check "LICENSES.txt present" '[ -s LICENSES.txt ]'
if [ "${SKIP_BUILD:-0}" != 1 ]; then
    rm -f build-switch/shadow.nro
    sudo docker run --rm -v "$PWD":/src -v /home/claude/switch_build.sh:/b.sh ghcr.io/danfromtico/switch-dev-legacy-vulkan bash /b.sh > /tmp/preflight_build.log 2>&1
    rc=$?
    check "NRO built without errors (exit $rc)" '[ $rc = 0 ] && ! grep -qE "error:|FAILED:|CMake Error" /tmp/preflight_build.log && [ -s build-switch/shadow.nro ]'
    c=$(git rev-parse --short=12 HEAD)
    check "NRO stamped with commit $c (not dirty)" 'strings -n 8 build-switch/shadow.nro | grep -q "commit $c" && ! strings -n 8 build-switch/shadow.nro | grep -q "commit $c-dirty"'
    check "NRO says switch-0.5.0" 'strings -n 8 build-switch/shadow.nro | grep -q "switch-0.5.0"'
fi
[ $fail = 0 ] && echo "PREFLIGHT PASSED" || echo "PREFLIGHT FAILED"
exit $fail
