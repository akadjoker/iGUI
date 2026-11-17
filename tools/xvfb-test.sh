#!/bin/sh
# Runs a test that opens windows or injects keyboard and mouse events on a throwaway X server (Xvfb), never on
# the display of whoever runs it:
#
#   tools/xvfb-test.sh <program> [arguments...]
#
# CMake registers these tests (igui_zen_input, igui_zen_lifecycle, igui_zen_sidetabs, igui_throwaway_display)
# through this script, so a bare `ctest` is safe: it starts its own Xvfb (xvfb-run -a takes a free display
# number, never the one the desktop is on), with no WAYLAND_DISPLAY, no XDG_RUNTIME_DIR (with it set, libwayland
# falls back to the session of the user running this) and scratch XDG_DATA_HOME and XDG_CONFIG_HOME.
# Where xvfb-run or Xvfb is missing it prints why and exits 77, which ctest reports as skipped (SKIP_RETURN_CODE).
#
# The test itself does not trust this: the script gives it IGUI_TEST_THROWAWAY_DISPLAY=<the DISPLAY of the Xvfb>,
# and the test refuses (77) unless that is what it finds, nothing points at Wayland, and the process on the
# other end of the display's socket is an Xvfb (tests/ThrowawayDisplay.hpp). Run by hand, without this
# script, those tests do nothing.

if [ $# -lt 1 ]; then
    echo "usage: xvfb-test.sh <program> [arguments...]" >&2
    exit 2
fi

if ! command -v xvfb-run > /dev/null 2>&1 || ! command -v Xvfb > /dev/null 2>&1; then
    echo "xvfb-test: xvfb-run or Xvfb is not installed: $1 is skipped (it opens windows, and only does so on a throwaway display)"
    exit 77
fi

scratch=$(mktemp -d "${TMPDIR:-/tmp}/igui-xvfb-test.XXXXXX") || exit 1
trap 'rm -rf "$scratch"' EXIT
trap 'exit 130' INT TERM HUP
mkdir -p "$scratch/data" "$scratch/cfg" "$scratch/cache" || exit 1

env -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR -u DISPLAY -u XAUTHORITY -u XDG_SESSION_TYPE \
    XDG_DATA_HOME="$scratch/data" XDG_CONFIG_HOME="$scratch/cfg" XDG_CACHE_HOME="$scratch/cache" \
    xvfb-run -a -s "-screen 0 1280x800x24" \
    sh -c 'IGUI_TEST_THROWAWAY_DISPLAY="$DISPLAY" exec "$@"' igui-xvfb-test "$@"
code=$?
exit $code
