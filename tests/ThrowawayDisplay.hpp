// The guard of the tests that open windows or inject keyboard and mouse events (XTEST): they do it only on a
// throwaway X server, never on the display of whoever runs ctest. onThrowawayDisplay() is the first thing their
// main() calls, and when it says no the test exits 77 (skipped) before it has touched any display.
//
// It says yes only when everything below holds (tools/xvfb-test.sh, which ctest runs these tests through,
// arranges all of it):
//   1. IGUI_TEST_THROWAWAY_DISPLAY is set and is the same as DISPLAY: the script that started an Xvfb says which
//      display it started. Nothing else sets it; a desktop session does not have it. Without it nothing is
//      connected to, not even to look.
//   2. DISPLAY is a local display (":N" or ":N.S", or "unix:N"), not a host: a network display is somebody's.
//   3. WAYLAND_DISPLAY and XDG_RUNTIME_DIR are not set: with them a Wayland client opens its window on the
//      session of the user running it, whatever DISPLAY says.
//   4. The program on the other end of the display's unix socket is an Xvfb: the kernel says who listens there
//      (SO_PEERCRED of a connection that sends nothing, then /proc/<pid>/exe). An Xorg or an Xwayland, which is
//      what a desktop runs, is not. The test connects to the socket and closes it; no X request is made.
// Linux only (no Xvfb, no /proc and no SO_PEERCRED anywhere else): elsewhere the answer is no. With the fake
// backend of zen_plataform (IGUI_ZEN_FAKE_PLATFORM, no window and no display at all) it is yes.
// No standard library: C only.
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace igui_test
{

namespace detail
{

#if defined(__linux__)

inline bool nonEmpty(const char* value)
{
    return value && *value;
}

// The display's number out of ":N", ":N.S" or "unix:N" (-1 for anything else: a host name, a protocol, text).
inline long displayNumber(const char* display)
{
    if (!display) return -1;
    if (!strncmp(display, "unix:", 5)) display += 5;
    else if (display[0] == ':')
        display += 1;
    else
        return -1;
    if (*display < '0' || *display > '9') return -1;
    long n = 0;
    for (; *display >= '0' && *display <= '9'; ++display)
    {
        n = n * 10 + (*display - '0');
        if (n > 1000000) return -1;
    }
    if (*display == '.')
    {
        ++display;
        if (*display < '0' || *display > '9') return -1;
        while (*display >= '0' && *display <= '9') ++display;
    }
    return *display == 0 ? n : -1;
}

// The name of the program listening on the unix socket of X display `number`, through the socket of the file
// system (abstract false) or the abstract one with the same name. connected is false when nothing listens there,
// and the answer is empty. Nothing is written to the socket.
inline bool serverProgram(long number, bool abstractName, char* program, size_t size, bool& connected)
{
    connected = false;
    program[0] = 0;
    sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    char name[64];
    snprintf(name, sizeof name, "/tmp/.X11-unix/X%ld", number);
    socklen_t len;
    if (abstractName)
    {
        // an abstract name starts with a zero byte and has no terminator in its length
        const size_t n = strlen(name);
        if (n + 1 > sizeof addr.sun_path) return false;
        memcpy(addr.sun_path + 1, name, n);
        len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1 + n);
    }
    else
    {
        strncpy(addr.sun_path, name, sizeof addr.sun_path - 1);
        len = static_cast<socklen_t>(sizeof addr);
    }
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), len) != 0)
    {
        close(fd);
        return false;
    }
    connected = true;
    ucred cred;
    socklen_t credLen = sizeof cred;
    bool ok = false;
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &credLen) == 0 && cred.pid > 0)
    {
        char link[64];
        snprintf(link, sizeof link, "/proc/%d/exe", static_cast<int>(cred.pid));
        char target[512];
        const ssize_t n = readlink(link, target, sizeof target - 1);
        if (n > 0)
        {
            target[n] = 0;
            const char* base = strrchr(target, '/');
            base = base ? base + 1 : target;
            strncpy(program, base, size - 1);
            program[size - 1] = 0;
            ok = true;
        }
    }
    close(fd);
    return ok;
}

#endif // __linux__

} // namespace detail

// True when the process is on a throwaway X server (see the top of the file). On false it has said why on stdout
// ("<test>: refused ...: skipped") and the caller returns 77. `server` is the program that must be on the other
// end of the display (Xvfb; a test of this function can name another to see it refuse).
inline bool onThrowawayDisplay(const char* test, const char* server = "Xvfb")
{
#if defined(IGUI_ZEN_FAKE_PLATFORM)
    (void) test;
    (void) server;
    return true;
#elif !defined(__linux__)
    (void) server;
    printf("%s: refused: no throwaway display is known on this system: skipped\n", test);
    return false;
#else
    const char* token = getenv("IGUI_TEST_THROWAWAY_DISPLAY");
    const char* display = getenv("DISPLAY");
    if (!detail::nonEmpty(token))
    {
        printf("%s: refused: not started by tools/xvfb-test.sh (ctest does), so there is no proof that the display is "
               "a throwaway one: skipped\n",
                test);
        return false;
    }
    if (!detail::nonEmpty(display) || strcmp(token, display) != 0)
    {
        printf("%s: refused: DISPLAY is not the one the Xvfb of tools/xvfb-test.sh was started on: skipped\n", test);
        return false;
    }
    if (detail::nonEmpty(getenv("WAYLAND_DISPLAY")) || detail::nonEmpty(getenv("XDG_RUNTIME_DIR")))
    {
        printf("%s: refused: WAYLAND_DISPLAY or XDG_RUNTIME_DIR is set, a Wayland client would open its window on the "
               "session of the user running this: skipped\n",
                test);
        return false;
    }
    const long number = detail::displayNumber(display);
    if (number < 0)
    {
        printf("%s: refused: DISPLAY \"%s\" is not a local display: skipped\n", test, display);
        return false;
    }
    bool connectedAny = false;
    for (int variant = 0; variant < 2; ++variant)
    {
        bool connected = false;
        char program[128];
        const bool known = detail::serverProgram(number, variant == 1, program, sizeof program, connected);
        if (!connected) continue;
        connectedAny = true;
        if (!known || strcmp(program, server) != 0)
        {
            printf("%s: refused: the server of display %s is \"%s\", not %s: skipped\n", test, display,
                    known ? program : "an unknown program", server);
            return false;
        }
    }
    if (!connectedAny)
    {
        printf("%s: refused: nothing listens on the socket of display %s: skipped\n", test, display);
        return false;
    }
    return true;
#endif
}

} // namespace igui_test
