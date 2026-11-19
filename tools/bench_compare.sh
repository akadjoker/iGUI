#!/bin/bash
# iGUI against Dear ImGui, FLTK and GTK 3, the same six scenes in each (docs/COMPARACAO.md has the method and the results):
#
#   tools/bench_compare.sh [options]
#
#   A empty window   B form of 200 widgets   C list of 100000 rows   D tree of 10000 nodes
#   E text editor with a 5 MB file           F 50 overlapping windows or panels
#
# What it runs, per library (igui-soft, igui-gl, imgui, fltk, gtk) and scene, every program on an Xvfb of its own (invisible,
# -noreset, no window manager), in a memory-capped scope, with HOME and XDG_* redirected, no sound:
#   frames   startup to the first frame presented, memory (RSS, PSS, heap), and the cost of a frame idle / with the mouse moving /
#            scrolling / clicking / typing / expanding the tree; the median of RUNS runs
#   idlecpu  (scene B) the library's own main loop for 10 s with nothing happening: CPU, wake-ups, frames
#   latency  (scene B) an injected click until the widget it changed is drawn and presented
#   icount   the user-space instructions of the GUI side of one frame (single-stepped: the perf counters are closed to a user here)
#   sizes    the stripped minimal application of each library, and the library
# then writes $OUT/tables.md (the tables of the document) next to the raw outputs.
#
# The programs are built with  cmake -DIGUI_BUILD_ZEN_ADAPTER=ON -DIGUI_BUILD_COMPARE=ON -DIGUI_COMPARE_IMGUI_DIR=<Dear ImGui>
#   -DIGUI_COMPARE_FLTK_CONFIG=<fltk-config> -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE=-O2 ...  (one -O2 for all of them);
# bench/compare is outside the no-std rule (the programs of the other libraries use their C++ library); see bench/compare/CMakeLists.txt.
#
# Options:
#   --build DIR      the build tree with bench/compare/cmp_* (default $IGUI_COMPARE_BUILD, else <repo>/../igui-compare-build)
#   --out DIR        raw outputs and tables.md (default <build>/compare-out)
#   --runs N         runs per library and scene for the medians (default 5)
#   --n N            frames per phase in each run (default 100)
#   --libs "..."     which libraries (default "igui-soft igui-gl imgui fltk gtk")
#   --scenes "..."   which scenes (default "A B C D E F")
#   --skip STAGE     leave out frames, idle, latency, icount or sizes (repeatable)
#   --jobs N         instruction counts in parallel (default 2; they do not depend on the machine's noise)
set -u

repo=$(cd "$(dirname "$0")/.." && pwd)
build=${IGUI_COMPARE_BUILD:-$repo/../igui-compare-build}
out=""
runs=5
nframes=100
libs="igui-soft igui-gl imgui fltk gtk"
scenes="A B C D E F"
skip=" "
jobs=2
while [ $# -gt 0 ]; do
    case "$1" in
        --build) build=$2; shift 2 ;;
        --out) out=$2; shift 2 ;;
        --runs) runs=$2; shift 2 ;;
        --n) nframes=$2; shift 2 ;;
        --libs) libs=$2; shift 2 ;;
        --scenes) scenes=$2; shift 2 ;;
        --skip) skip="$skip$2 "; shift 2 ;;
        --jobs) jobs=$2; shift 2 ;;
        -h|--help) sed -n '2,/^set -u/p' "$0" | sed '$d'; exit 0 ;;
        *) echo "bench_compare.sh: unknown option $1" >&2; exit 2 ;;
    esac
done
build=$(cd "$build" && pwd) || exit 2
[ -n "$out" ] || out=$build/compare-out
mkdir -p "$out/raw" "$out/home" "$out/xdg/cfg" "$out/xdg/cache" || exit 2
out=$(cd "$out" && pwd)
bin=$build/bench/compare
preload=$bin/libcmp_mcount.so
[ -f "$preload" ] || { echo "bench_compare.sh: $preload not found: build with -DIGUI_BUILD_COMPARE=ON (see the top of this script)" >&2; exit 2; }

cap_args=(--user --scope --quiet -p MemoryMax=4G -p MemorySwapMax=0)
if command -v systemd-run > /dev/null 2>&1 && systemd-run "${cap_args[@]}" -- true > /dev/null 2>&1; then have_scope=1; else have_scope=0
    echo "bench_compare.sh: WARNING: systemd-run --user --scope is not usable: running without the memory cap" >&2; fi
# one program on a private Xvfb (xvfb-run -a takes a free display number, never the desktop's)
xrun()
{
    local pre=()
    [ $have_scope -eq 1 ] && pre=(systemd-run "${cap_args[@]}" --)
    "${pre[@]}" env -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR -u DISPLAY -u XAUTHORITY -u XDG_SESSION_TYPE \
        HOME="$out/home" XDG_DATA_HOME="$out/xdg" XDG_CONFIG_HOME="$out/xdg/cfg" XDG_CACHE_HOME="$out/xdg/cache" \
        LIBGL_ALWAYS_SOFTWARE=1 LC_ALL=C SDL_AUDIODRIVER=dummy \
        xvfb-run -a -s "-screen 0 1280x800x24 -noreset" "$@"
}
command -v xvfb-run > /dev/null 2>&1 || { echo "bench_compare.sh: xvfb-run (Xvfb) is needed: nothing here opens a window on a real display" >&2; exit 2; }

# the program and flags of a library
prog() { case "$1" in igui-soft|igui-gl) echo "$bin/cmp_igui" ;; imgui) echo "$bin/cmp_imgui" ;; fltk) echo "$bin/cmp_fltk" ;; gtk) echo "$bin/cmp_gtk" ;; esac; }
flags() { case "$1" in igui-soft) echo "--soft" ;; igui-gl) echo "--gl" ;; *) echo "" ;; esac; }
# the idle and latency stages run the library as an application would: frames in which nothing changed are not painted (--idleskip)
loopflags() { case "$1" in igui-soft) echo "--soft --idleskip" ;; igui-gl) echo "--gl --idleskip" ;; *) echo "" ;; esac; }
have() { [ -x "$(prog "$1")" ]; }
want() { case "$skip" in *" $1 "*) return 1 ;; esac; return 0; }

libs_ok=""
for l in $libs; do if have "$l"; then libs_ok="$libs_ok $l"; else echo "bench_compare.sh: no program for $l, left out"; fi; done

echo "bench_compare.sh: $(nproc) CPUs, load $(cut -d' ' -f1-3 /proc/loadavg); the timings are the medians of $runs runs, the instruction counts do not depend on the load"

if want frames; then
    for sc in $scenes; do for l in $libs_ok; do
        mkdir -p "$out/raw/$l"
        for r in $(seq 1 "$runs"); do
            xrun env LD_PRELOAD="$preload" "$(prog "$l")" "$sc" frames $(flags "$l") --n="$nframes" \
                > "$out/raw/$l/frames.$sc.$r.txt" 2> "$out/raw/$l/frames.$sc.$r.err"
        done
        echo "frames $l $sc done"
    done; done
fi

if want idle; then
    for l in $libs_ok; do
        for r in 1 2 3; do
            xrun env LD_PRELOAD="$preload" "$(prog "$l")" B idlecpu $(loopflags "$l") --seconds=10 \
                > "$out/raw/$l/idlecpu.B.$r.txt" 2> "$out/raw/$l/idlecpu.B.$r.err"
        done
        echo "idlecpu $l done"
    done
fi

if want latency; then
    for l in $libs_ok; do
        for r in 1 2 3; do
            xrun env LD_PRELOAD="$preload" "$(prog "$l")" B latency $(loopflags "$l") --n=40 \
                > "$out/raw/$l/latency.B.$r.txt" 2> "$out/raw/$l/latency.B.$r.err"
        done
        echo "latency $l done"
    done
fi

if want icount; then
    # which phases exist in which scene; no preload (its malloc wrapper would be counted)
    phases_of() { case "$1" in A) echo "idle,hover" ;; B) echo "idle,hover,click" ;; C) echo "idle,hover,scroll" ;; D) echo "idle,hover,scroll,expand" ;; E) echo "idle,hover,scroll,typing" ;; F) echo "idle,hover" ;; esac; }
    running=0
    for sc in $scenes; do for l in $libs_ok; do
        (
            xrun env ICOUNT=1 "$(prog "$l")" "$sc" frames $(flags "$l") --n=1 --phases="$(phases_of "$sc")" \
                > "$out/raw/$l/icount.$sc.txt" 2> "$out/raw/$l/icount.$sc.err"
            echo "icount $l $sc done"
        ) &
        running=$((running + 1))
        if [ $running -ge "$jobs" ]; then wait -n; running=$((running - 1)); fi
    done; done
    wait
fi

if want sizes; then
    # bytes of the stripped binary, the number of shared libraries it links, and the bytes of those beyond the C runtime (libc, libm,
    # libstdc++, libgcc, the loader): what has to be installed or shipped besides the binary
    : > "$out/raw/sizes.txt"
    for f in "$bin"/min_*; do
        [ -x "$f" ] || continue
        s="$out/raw/$(basename "$f").stripped"
        cp "$f" "$s" && strip "$s"
        extra=0
        for so in $(ldd "$f" 2>/dev/null | awk '/=>/ {print $3}'); do
            case "$so" in */libc.so*|*/libm.so*|*/libstdc++.so*|*/libgcc_s.so*|*/libpthread*|*/libdl*|*/librt*) continue ;; esac
            extra=$((extra + $(stat -L -c %s "$so" 2>/dev/null || echo 0)))
        done
        echo "$(basename "$f") $(stat -c %s "$s") $(ldd "$f" 2>/dev/null | grep -c '=>') $extra" >> "$out/raw/sizes.txt"
    done
    for a in "$build"/libigui_widgets.a "$build"/libigui_core.a "$build"/libigui_zen.a "$build"/zen_plataform/libplatform.a; do
        [ -f "$a" ] && echo "$(basename "$a") $(stat -c %s "$a") 0 0" >> "$out/raw/sizes.txt"
    done
fi

python3 -I "$repo/tools/bench_compare_table.py" "$out/raw" > "$out/tables.md" && echo "bench_compare.sh: tables in $out/tables.md"
