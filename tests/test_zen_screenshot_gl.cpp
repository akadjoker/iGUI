// ZEN_SCREENSHOT_BACKEND=gl: a screenshot drawn by the OpenGL backend into an offscreen framebuffer of a window that is never
// shown, and Config::defaultDynamicPage = false. The window is hidden and never mapped, but it is still a connection to an X
// server, so the rule of the other window tests applies: it runs only on the throwaway Xvfb that tools/xvfb-test.sh started for
// it (ctest runs it that way) and refuses, before it touches a display, anywhere else (ThrowawayDisplay.hpp). A display without
// OpenGL, or whose OpenGL cannot make the offscreen framebuffer, exits 77 (skipped) with the reason.
// Both applications draw text with the default atlas of ZenApp, whose RAM copy of the baked page is released as soon as the
// backend has the texture (FontAtlas::releasePixels): the first one with a baked glyph row, the second one, with the dynamic
// page, at a size that is not the baked one and with a codepoint the baked page does not have, both first used after the release.

#include "ThrowawayDisplay.hpp"

#include <igui_zen/ZenApp.hpp>

#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"

#include <igui/widgets/Theme.hpp>

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

// A second application: the default font with its dynamic page, a size that is not the baked one (22) and a codepoint the baked
// page does not have (Cyrillic Zhe and Ef), all first drawn after the baked page's RAM copy was released. Read back from the
// offscreen target of the window with readWindowPixels.
static void dynamicText()
{
    ig::zen::App::Config config;
    config.title = "iGUI dynamic text test";
    config.width = 320;
    config.height = 96;
    config.preferOpenGL = false;
    config.vsync = false;
    config.showStats = false;
    config.defaultDynamicPage = true;

    ig::zen::App zen;
    if (!zen.create(config))
    {
        fprintf(stderr, "test_zen_screenshot_gl: the second application could not be made, skipped\n");
        return;
    }
    Theme::instance().fontSize = 22.0f;
    Widget* root = zen.widgets().addStage("dyn");
    auto* column = root->createChild<BoxLayout>(LayoutDir::Vertical);
    column->setStretch(1);
    column->setSpacing(4);
    column->setPadding(6);
    column->createChild<Label>(String("Hamburgefonstiv"));
    column->createChild<Label>(String("\xd0\x96 \xd0\xa4 Q 0123"));
    zen.widgets().setTransition(TransitionType::None, 0.0f);
    zen.widgets().setStage("dyn");
    for (int frame = 0; frame < 10 && zen.pump(); ++frame)
    {
    }
    ct::Vector<uint32_t> px;
    int width = 0, height = 0;
    check(zen.readWindowPixels(px, width, height) && width == 320 && height == 96, "the offscreen target is read back at the window's size");
    if (px.size() == static_cast<size_t>(320) * 96)
    {
        // rows of text of 22 pixels: the first starts 6 pixels down, the second 4 pixels under it; the corner is the background
        const uint32_t background = px[0];
        int top = 0, bottom = 0;
        for (int y = 0; y < 96; ++y)
            for (int x = 0; x < 320; ++x)
                if (px[static_cast<size_t>(y) * 320 + x] != background) (y < 34 ? top : bottom) += 1;
        check(top > 150, "text of a size that is not the baked one is drawn from the dynamic page (released baked pixels)");
        check(bottom > 80, "and so is a codepoint the baked page never had, first used after the release");
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

    { // the first application is gone before the second one is made
        ig::zen::App zen;
        if (!zen.create(config))
        {
            fprintf(stderr, "test_zen_screenshot_gl: no OpenGL context, or no offscreen framebuffer, on this X server: skipped\n");
            remove(path);
            return 77;
        }
        check(zen.usesOpenGL(), "ZEN_SCREENSHOT_BACKEND=gl draws with OpenGL although preferOpenGL is false");
        Widget* root = zen.widgets().addStage("shot");
        auto* column = root->createChild<BoxLayout>(LayoutDir::Vertical);
        column->setStretch(1);
        column->setSpacing(0);
        column->setPadding(0);
        column->createChild<Label>(String("Hamburgefonstiv"));
        auto* body = column->createChild<BoxLayout>(LayoutDir::Horizontal);
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
        // the label on top: its text is anti-aliased, so many different colours in the top rows (a flat band has one or two)
        ct::Vector<uint32_t> seen;
        for (int y = 0; y < 14; ++y)
            for (int x = 0; x < 160; ++x)
            {
                const size_t i = (static_cast<size_t>(y) * 160 + x) * 3;
                const uint32_t colour = (static_cast<uint32_t>(rgb[i]) << 16) | (static_cast<uint32_t>(rgb[i + 1]) << 8) | rgb[i + 2];
                bool known = false;
                for (size_t k = 0; k < seen.size() && !known; ++k) known = seen[k] == colour;
                if (!known && seen.size() < 64) seen.push_back(colour);
            }
        check(seen.size() >= 6, "the baked text of a label is drawn (the atlas texture was made before its RAM copy was released)");
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
    dynamicText();
    if (failures)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_zen_screenshot_gl: ok\n");
    return 0;
}
