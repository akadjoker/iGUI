#!/bin/bash
# One command that checks the retained toolkit before a commit is handed in:
#
#   tools/check.sh [options]
#
#   1. nostd    no new `std::` and no new `#include <...>` in what src/, include/ and adapters/ gained
#               since the base revision (the project uses its own `ct` containers)
#   2. release  Release build, every test (ctest, one at a time)
#   3. asan     Debug build with AddressSanitizer + UndefinedBehaviorSanitizer, every test
#   4. hash     the checksum of what the bench draws for the editor window and for the piano roll (igui_retained_bench --hash)
#               against the reference in bench/retained/draw.hash: a change that is only meant to make
#               drawing faster, or to fix something outside the editor window, must not move it
#
# Every program and test runs through an invisible X server and scratch settings, never the real display:
#   env -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR XDG_DATA_HOME=... XDG_CONFIG_HOME=... xvfb-run -a -s "-screen 0 1280x800x24" <command>
# (unsetting WAYLAND_DISPLAY alone is not enough: with XDG_RUNTIME_DIR set, libwayland falls back to the
# session of the user running this).
#
# One summary line per stage ("PASS <stage>: ..." or "FAIL <stage>: ..."), the output of each stage in
# $BUILD/<stage>.log, and a last line with the totals; the exit status is not zero when any stage failed.
#
# Options (environment variables in capitals):
#   --base REV          the revision the work is compared with for `nostd` (default origin/main)
#   --build DIR         where the build trees and logs go (default $IGUI_CHECK_BUILD, else <repo>/../igui-check-build)
#   --skip STAGE        leave a stage out (nostd, release, asan, hash); repeatable
#   --jobs N            compiler jobs (default: half of the cores); the tests always run one at a time
#   --update-hash       write bench/retained/draw.hash with the checksum of this tree instead of comparing
#   IGUI_CONTAINERS_DIR, IGUI_ZEN_PLATFORM_DIR   the dependencies, as the CMake options of the same name
#                       (without them CMake uses third_party/containers and ../zen_plataform)
set -u

repo=$(cd "$(dirname "$0")/.." && pwd)
base=origin/main
build=${IGUI_CHECK_BUILD:-$repo/../igui-check-build}
jobs=$(( $(nproc 2>/dev/null || echo 2) / 2 ))
[ "$jobs" -lt 1 ] && jobs=1
skip=" "
update_hash=0
while [ $# -gt 0 ]; do
    case "$1" in
        --base) base=$2; shift 2 ;;
        --build) build=$2; shift 2 ;;
        --skip) skip="$skip$2 "; shift 2 ;;
        --jobs) jobs=$2; shift 2 ;;
        --update-hash) update_hash=1; shift ;;
        -h|--help) sed -n '2,32p' "$0"; exit 0 ;;
        *) echo "check.sh: unknown option $1" >&2; exit 2 ;;
    esac
done
mkdir -p "$build" "$build/xdg/cfg" || exit 2
build=$(cd "$build" && pwd)

failed=0
summary=()

# a program or a test, through an invisible X server and scratch settings
xrun()
{
    env -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR XDG_DATA_HOME="$build/xdg" XDG_CONFIG_HOME="$build/xdg/cfg" \
        xvfb-run -a -s "-screen 0 1280x800x24" "$@"
}

cmake_deps=()
[ -n "${IGUI_CONTAINERS_DIR:-}" ] && cmake_deps+=("-DIGUI_CONTAINERS_DIR=$IGUI_CONTAINERS_DIR")
[ -n "${IGUI_ZEN_PLATFORM_DIR:-}" ] && cmake_deps+=("-DIGUI_ZEN_PLATFORM_DIR=$IGUI_ZEN_PLATFORM_DIR")
common=(-G Ninja -DIGUI_BUILD_TESTS=ON -DIGUI_BUILD_ZEN_ADAPTER=ON -DIGUI_BUILD_ZEN_DEMO=OFF -DIGUI_BUILD_RAYLIB_BACKEND=OFF
        -DIGUI_BUILD_RETAINED_BENCH=ON "${cmake_deps[@]}")

report()
{
    local status=$1 stage=$2 text=$3
    echo "$status $stage: $text"
    summary+=("$status $stage")
    [ "$status" = FAIL ] && failed=$((failed + 1))
}

skipped()
{
    case "$skip" in *" $1 "*) report SKIP "$1" "left out (--skip)"; return 0 ;; esac
    return 1
}

# Configures (when needed), builds and runs ctest, one test at a time, in a build tree.
# usage: ctest_stage <stage> <dir> <cmake args...>
ctest_stage()
{
    local stage=$1 dir=$2
    shift 2
    local log="$build/$stage.log"
    : > "$log"
    if ! cmake -S "$repo" -B "$dir" "${common[@]}" "$@" >> "$log" 2>&1; then
        report FAIL "$stage" "cmake did not configure (see $log)"
        return
    fi
    if ! cmake --build "$dir" -j "$jobs" >> "$log" 2>&1; then
        report FAIL "$stage" "the build failed (see $log)"
        return
    fi
    # no -j: the tests run one at a time
    xrun ctest --test-dir "$dir" --output-on-failure >> "$log" 2>&1
    local code=$?
    local line
    line=$(grep -E "tests passed, [0-9]+ tests failed out of" "$log" | tail -1)
    local skipped_n
    skipped_n=$(grep -cE "\*\*\*Skipped" "$log")
    [ "$skipped_n" -gt 0 ] && line="$line, $skipped_n skipped"
    if [ $code -eq 0 ]; then
        report PASS "$stage" "${line:-ctest passed}"
    else
        report FAIL "$stage" "${line:-ctest failed} (see $log)"
    fi
}

# ── 1. no new std ────────────────────────────────────────────────────────────
if ! skipped nostd; then
    if ! git -C "$repo" rev-parse --verify -q "$base" > /dev/null; then
        report FAIL nostd "the base revision $base does not exist (--base)"
    else
        hits=$(git -C "$repo" diff "$base" -- src include adapters | grep '^+' | grep -v '^+++' | grep -n 'std::\|#include <')
        if [ -z "$hits" ]; then
            report PASS nostd "nothing new with std:: or #include <...> in src, include and adapters since $base"
        else
            echo "$hits" > "$build/nostd.log"
            report FAIL nostd "$(echo "$hits" | wc -l) new lines (see $build/nostd.log)"
        fi
    fi
fi

# ── 2. Release ───────────────────────────────────────────────────────────────
if ! skipped release; then
    ctest_stage release "$build/release" -DCMAKE_BUILD_TYPE=Release
fi

# ── 3. sanitizers ────────────────────────────────────────────────────────────
if ! skipped asan; then
    san="-fsanitize=address,undefined -fno-omit-frame-pointer"
    ctest_stage asan "$build/asan" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="$san" -DCMAKE_C_FLAGS="$san" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
fi

# ── 4. what the bench draws ──────────────────────────────────────────────────
if ! skipped hash; then
    bench="$build/release/igui_retained_bench"
    ref="$repo/bench/retained/draw.hash"
    if [ ! -x "$bench" ]; then
        report FAIL hash "no bench to run ($bench: the release stage did not build it)"
    else
        xrun "$bench" --hash > "$build/hash.log" 2>&1
        # the checksums: one for each size of the editor window and one for the piano roll
        got=$(grep -o 'draw checksum [^:]*: [0-9a-f]* ([0-9]* vertices, [0-9]* indices, [0-9]* commands)' "$build/hash.log")
        if [ -z "$got" ]; then
            report FAIL hash "the bench printed no checksum (see $build/hash.log)"
        elif [ $update_hash -eq 1 ]; then
            echo "$got" > "$ref"
            report PASS hash "wrote $ref"
        elif [ ! -f "$ref" ]; then
            report SKIP hash "no reference ($ref): run with --update-hash to make one"
        elif [ "$got" = "$(cat "$ref")" ]; then
            report PASS hash "what the bench draws is what bench/retained/draw.hash says"
        else
            diff <(cat "$ref") <(echo "$got") > "$build/hash.diff"
            report FAIL hash "the draw data of the editor window changed (see $build/hash.diff; --update-hash if it is meant to)"
        fi
    fi
fi

echo "----"
echo "check.sh: ${#summary[@]} stages, $failed failed"
[ $failed -eq 0 ]
