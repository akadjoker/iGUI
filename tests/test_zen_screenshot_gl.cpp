// ZEN_SCREENSHOT_BACKEND=gl: a screenshot drawn by the OpenGL backend into an offscreen framebuffer of a window that is never
// shown, and Config::defaultDynamicPage = false. The window is hidden and never mapped, but it is still a connection to an X
// server, so the rule of the other window tests applies: it runs only on the throwaway Xvfb that tools/xvfb-test.sh started for
// it (ctest runs it that way) and refuses, before it touches a display, anywhere else (ThrowawayDisplay.hpp). A display without
// OpenGL, or whose OpenGL cannot make the offscreen framebuffer, exits 77 (skipped) with the reason.

#include "ThrowawayDisplay.hpp"

#include <igui_zen/ZenApp.hpp>

#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace ig::retained;

static int failures = 0;
static void check(bool ok, const char* what)
{
    if (!ok)
    {
        fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

int main()
{
    // first of all: no window on a display that is not a throwaway one
    if (!igui_test::onThrowawayDisplay("test_zen_screenshot_gl")) return 77;
    char path[] = "/tmp/igui_zen_screenshot_gl_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return 77;
    close(fd);
    setenv("ZEN_SCREENSHOT", path, 1);
    setenv("ZEN_SCREENSHOT_BACKEND", getenv("IGUI_TEST_BACKEND") ? getenv("IGUI_TEST_BACKEND") : "gl", 1);
    setenv("ZEN_SCREENSHOT_FRAME", "12", 1);

    ig::zen::App::Config config;
    config.title = "iGUI screenshot test";
    config.width = 160;
    config.height = 96;
    config.preferOpenGL = false; // the environment asks for OpenGL anyway
    config.vsync = false;
    config.showStats = false;
    config.defaultDynamicPage = false;

    ig::zen::App zen;
    if (!zen.create(config))
    {
        fprintf(stderr, "test_zen_screenshot_gl: no window or no OpenGL (no display?), skipped\n");
        remove(path);
        return 77;
    }
    check(zen.usesOpenGL(), "ZEN_SCREENSHOT_BACKEND=gl draws with OpenGL although preferOpenGL is false");
    Widget* root = zen.widgets().addStage("shot");
    auto* body = root->createChild<BoxLayout>(LayoutDir::Horizontal);
    body->setStretch(1);
    body->setSpacing(0);
    body->setPadding(0);
    auto* red = body->createChild<Panel>();
    red->setStretch(1);
    red->setBgColor(Color(200, 30, 30, 255));
    zen.widgets().setTransition(TransitionType::None, 0.0f);
    zen.widgets().setStage("shot");
    for (int frame = 0; frame < 40 && zen.pump(); ++frame)
    {
    }

    FILE* file = fopen(path, "rb");
    check(file != nullptr, "the screenshot file was written");
    if (file)
    {
        int width = 0, height = 0, maxval = 0;
        check(fscanf(file, "P6 %d %d %d", &width, &height, &maxval) == 3 && width == 160 && height == 96 && maxval == 255, "a P6 image of the window's size");
        fgetc(file);
        unsigned char* rgb = static_cast<unsigned char*>(malloc(static_cast<size_t>(width) * height * 3));
        const size_t got = rgb ? fread(rgb, 1, static_cast<size_t>(width) * height * 3, file) : 0;
        check(got == static_cast<size_t>(width) * height * 3, "all its pixels");
        size_t red = 0;
        for (size_t i = 0; got && i + 2 < got; i += 3)
            if (rgb[i] == 200 && rgb[i + 1] == 30 && rgb[i + 2] == 30)
                ++red;
        check(red > 160 * 96 / 2, "the panel's colour fills most of the picture (it was drawn by the OpenGL backend and read back)");
        free(rgb);
        fclose(file);
    }
    if (!getenv("IGUI_TEST_KEEP"))
        remove(path);
    else
        fprintf(stderr, "kept %s\n", path);
    if (failures)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_zen_screenshot_gl: ok\n");
    return 0;
}
