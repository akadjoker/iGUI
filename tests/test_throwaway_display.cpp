// The guard of the tests that open windows or inject input (tests/ThrowawayDisplay.hpp) says yes only on the
// Xvfb that tools/xvfb-test.sh starts, and no in every other case: no token, a token that is not the display, a
// Wayland session in the environment, a display that is a host, a display nothing listens on, a server that is
// not the one named. Run through tools/xvfb-test.sh (the ctest entry does), where the real conditions are the
// right ones; each case changes the environment of this process and puts it back. It never names a display
// that exists (":987654" and the like): the cases that must be refused are refused before any socket is
// touched, or on a socket nobody listens on. No standard library: C only.

#include "ThrowawayDisplay.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{

int gFailures = 0;
void check(bool ok, const char* what)
{
    if (!ok)
    {
        ++gFailures;
        printf("  FAIL %s\n", what);
    }
}

// The environment variables the guard reads, as they are on entry, to put back after each case.
struct Saved
{
    char* token = nullptr;
    char* display = nullptr;
    char* wayland = nullptr;
    char* runtime = nullptr;
    static char* copy(const char* value)
    {
        return value ? strdup(value) : nullptr;
    }
    Saved()
    {
        token = copy(getenv("IGUI_TEST_THROWAWAY_DISPLAY"));
        display = copy(getenv("DISPLAY"));
        wayland = copy(getenv("WAYLAND_DISPLAY"));
        runtime = copy(getenv("XDG_RUNTIME_DIR"));
    }
    static void put(const char* name, const char* value)
    {
        if (value) setenv(name, value, 1);
        else
            unsetenv(name);
    }
    void restore() const
    {
        put("IGUI_TEST_THROWAWAY_DISPLAY", token);
        put("DISPLAY", display);
        put("WAYLAND_DISPLAY", wayland);
        put("XDG_RUNTIME_DIR", runtime);
    }
    ~Saved()
    {
        restore();
        free(token);
        free(display);
        free(wayland);
        free(runtime);
    }
};

} // namespace

int main()
{
#if defined(IGUI_ZEN_FAKE_PLATFORM) || !defined(__linux__)
    printf("test_throwaway_display: no X server involved here: skipped\n");
    return 77;
#else
    Saved saved;
    // Under tools/xvfb-test.sh: the guard says yes. Anywhere else this test is not what is wanted, and says so.
    if (!saved.token || !saved.display)
    {
        printf("test_throwaway_display: not started by tools/xvfb-test.sh (ctest does): skipped\n");
        return 77;
    }
    if (!igui_test::onThrowawayDisplay("test_throwaway_display (the real conditions)")) return 77;
    printf("test_throwaway_display: running on the Xvfb %s\n", saved.display);

    // Refused: there is no token
    unsetenv("IGUI_TEST_THROWAWAY_DISPLAY");
    check(!igui_test::onThrowawayDisplay("no token"), "no token: refused");
    setenv("IGUI_TEST_THROWAWAY_DISPLAY", "", 1);
    check(!igui_test::onThrowawayDisplay("empty token"), "an empty token: refused");
    saved.restore();

    // Refused: the token is not the display the process is on (a token left over in the environment of a desktop session)
    setenv("IGUI_TEST_THROWAWAY_DISPLAY", ":987654", 1);
    check(!igui_test::onThrowawayDisplay("another token"), "a token that is not DISPLAY: refused");
    saved.restore();
    unsetenv("DISPLAY");
    check(!igui_test::onThrowawayDisplay("no display"), "no DISPLAY: refused");
    saved.restore();

    // Refused: a Wayland session that a client would open its window in, whatever DISPLAY says
    setenv("WAYLAND_DISPLAY", "wayland-0", 1);
    check(!igui_test::onThrowawayDisplay("wayland"), "WAYLAND_DISPLAY set: refused");
    saved.restore();
    setenv("XDG_RUNTIME_DIR", "/run/user/1000", 1);
    check(!igui_test::onThrowawayDisplay("runtime dir"), "XDG_RUNTIME_DIR set: refused");
    saved.restore();

    // Refused: a display that is not a local one, or that nothing listens on (the token matches in both)
    const char* odd[] = { "somehost:0", "tcp/somehost:6000", ":", ":x", ":987654", ":987654.0", "unix:", ":1e3", ":99999999999" };
    for (const char* display: odd)
    {
        setenv("DISPLAY", display, 1);
        setenv("IGUI_TEST_THROWAWAY_DISPLAY", display, 1);
        char what[96];
        snprintf(what, sizeof what, "DISPLAY \"%s\"", display);
        char claim[128];
        snprintf(claim, sizeof claim, "%s: refused", what);
        check(!igui_test::onThrowawayDisplay(what), claim);
    }
    saved.restore();

    // Refused: a server that is not the one asked for. This process is on an Xvfb, so asking for any other
    // program (what a desktop runs is Xorg or Xwayland) is what the guard does on a real display.
    check(!igui_test::onThrowawayDisplay("an Xorg server wanted", "Xorg"), "the server is an Xvfb, not an Xorg: refused");
    check(!igui_test::onThrowawayDisplay("an Xwayland server wanted", "Xwayland"), "the server is an Xvfb, not an Xwayland: refused");

    // and with everything back as it was, yes again
    saved.restore();
    check(igui_test::onThrowawayDisplay("the real conditions, again"), "the real conditions: accepted");

    if (gFailures) printf("%d checks failed\n", gFailures);
    else
        printf("ok\n");
    return gFailures ? 1 : 0;
#endif
}
