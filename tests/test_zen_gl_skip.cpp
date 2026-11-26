// The OpenGL path of the zen adapter does not draw and swap a frame whose draw data has not changed (Config::skipUnchangedGl):
// an unchanged UI is skipped, a changed widget, a texture updated in place, an invalidateGl() and a resized window are drawn, a
// second window has its own signature, readWindowPixels after skipped frames still returns the picture, and the switch turns it
// off. Real windows on the throwaway Xvfb of tools/xvfb-test.sh (ThrowawayDisplay.hpp); 77 (skipped) without OpenGL.

#include "ThrowawayDisplay.hpp"

#include <igui_zen/ZenApp.hpp>

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/Theme.hpp>

#include <platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void frames(ig::zen::App& zen, int n)
{
    for (int i = 0; i < n; ++i) zen.pump();
}

static size_t countColour(const ct::Vector<uint32_t>& px, uint32_t rgb)
{
    size_t n = 0;
    for (size_t i = 0; i < px.size(); ++i)
        if ((px[i] & 0x00ffffffu) == rgb) ++n;
    return n;
}

static void fill(unsigned char* rgba, int w, int h, unsigned r, unsigned g, unsigned b)
{
    for (int i = 0; i < w * h; ++i)
    {
        rgba[i * 4 + 0] = static_cast<unsigned char>(r);
        rgba[i * 4 + 1] = static_cast<unsigned char>(g);
        rgba[i * 4 + 2] = static_cast<unsigned char>(b);
        rgba[i * 4 + 3] = 255;
    }
}

static bool makeApp(ig::zen::App& zen, bool skip)
{
    ig::zen::App::Config config;
    config.title = "iGUI gl skip test";
    config.width = 200;
    config.height = 120;
    config.preferOpenGL = true;
    config.vsync = false;
    config.showStats = false;
    config.skipUnchangedGl = skip;
    return zen.create(config) && zen.usesOpenGL();
}

int main()
{
    if (!igui_test::onThrowawayDisplay("test_zen_gl_skip")) return 77;

    {
        ig::zen::App zen;
        if (!makeApp(zen, true))
        {
            fprintf(stderr, "test_zen_gl_skip: no OpenGL context on this X server: skipped\n");
            return 77;
        }
        Widget* root = zen.widgets().addStage("main");
        auto* column = root->createChild<BoxLayout>(LayoutDir::Vertical);
        column->setPadding(4);
        auto* label = column->createChild<Label>(String("one"));
        auto* image = column->createChild<ImageButton>();
        image->setStretch(1);
        unsigned char rgba[16 * 16 * 4];
        fill(rgba, 16, 16, 0, 255, 0);
        const TextureHandle tex = zen.gl().createTexture(16, 16, rgba);
        image->setTexture(tex, 16, 16);
        image->setSrcRect(FloatRect(0, 0, 16, 16));
        zen.widgets().setTransition(TransitionType::None, 0.0f);
        zen.widgets().setStage("main");

        // what the application sees every frame, drawn or not: the picture the window shows
        ct::Vector<uint32_t> seen;
        int seenWidth = 0, seenHeight = 0, callbacks = 0;
        zen.setAfterRenderCallback([&] {
            ++callbacks;
            zen.readWindowPixels(seen, seenWidth, seenHeight);
        });

        frames(zen, 12);
        check(zen.glFramesDrawn() >= 1, "the first frames are drawn");
        check(zen.glFramesSkipped() >= 1, "an unchanged UI is skipped after the first frames");
        uint64_t drawn = zen.glFramesDrawn(), skipped = zen.glFramesSkipped();
        const int cb = callbacks;
        frames(zen, 20);
        check(zen.glFramesDrawn() == drawn, "no frame is drawn while nothing changes");
        check(zen.glFramesSkipped() == skipped + 20, "twenty unchanged frames are twenty skips");
        check(callbacks == cb + 20, "the after-render callback runs on every frame, skipped or not");
        const size_t green = countColour(seen, 0x00ff00u);
        check(green > 200, "the picture read in a skipped frame has the image");

        // a changed widget
        drawn = zen.glFramesDrawn();
        label->setText(String("two"));
        frames(zen, 6);
        check(zen.glFramesDrawn() > drawn, "a changed label is drawn");
        drawn = zen.glFramesDrawn();
        frames(zen, 6);
        check(zen.glFramesDrawn() == drawn, "and then skipped again");

        // a texture updated in place: same handle, same widget, new pixels
        fill(rgba, 16, 16, 255, 0, 0);
        check(zen.gl().updateTexture(tex, rgba, 16, 16), "the texture is updated in place");
        frames(zen, 1);
        check(zen.glFramesDrawn() == drawn + 1, "a texture updated in place makes the next frame drawn");
        check(countColour(seen, 0xff0000u) > 200 && countColour(seen, 0x00ff00u) == 0, "and the new pixels are on screen");
        const ct::Vector<uint32_t> reference = seen;
        skipped = zen.glFramesSkipped();
        frames(zen, 5);
        check(zen.glFramesSkipped() == skipped + 5, "unchanged again: skipped");
        check(seen.size() == reference.size() && memcmp(seen.data(), reference.data(), reference.size() * sizeof(uint32_t)) == 0,
                "readWindowPixels in skipped frames returns the picture of the last drawn one");
        ct::Vector<uint32_t> after;
        int w = 0, h = 0;
        check(zen.readWindowPixels(after, w, h) && after.size() == reference.size() &&
                        memcmp(after.data(), reference.data(), reference.size() * sizeof(uint32_t)) == 0,
                "readWindowPixels after pump() returns the picture too");
        drawn = zen.glFramesDrawn();
        frames(zen, 3);
        check(zen.glFramesDrawn() == drawn, "a read does not make the next frame drawn");

        // a pending capture
        bool captured = false;
        zen.requestCapture([&](const uint32_t* p, int cw, int ch) { captured = p && cw == 200 && ch == 120; });
        frames(zen, 1);
        check(captured, "a requested capture is drawn and delivered in a skippable frame");

        // invalidate
        drawn = zen.glFramesDrawn();
        zen.invalidateGl();
        frames(zen, 1);
        check(zen.glFramesDrawn() == drawn + 1, "invalidateGl() draws the next frame");

        // a resize
        drawn = zen.glFramesDrawn();
        window_set_size(zen.window(), 240, 150);
        frames(zen, 30);
        int fw = 0, fh = 0;
        window_get_framebuffer_size(zen.window(), &fw, &fh);
        check(fw == 240 && fh == 150, "the window was resized");
        check(zen.glFramesDrawn() > drawn, "a resized window is drawn");
        drawn = zen.glFramesDrawn();
        frames(zen, 5);
        check(zen.glFramesDrawn() == drawn, "and skipped again at its new size");
        check(seenWidth == 240 && seenHeight == 150, "the picture read has the new size");

        // a second window: its own signature
        ig::zen::App::Window* previous = zen.current();
        ig::zen::App::WindowDesc desc;
        desc.title = "second";
        desc.width = 160;
        desc.height = 100;
        ig::zen::App::Window* second = zen.openWindow(desc);
        check(second != nullptr, "a second window opens");
        if (second)
        {
            Label* secondLabel = nullptr;
            Widget* sroot = zen.widgets().addStage("second");
            secondLabel = sroot->createChild<Label>(String("window two"));
            zen.widgets().setStage("second", TransitionType::None);
            zen.makeCurrent(previous);
            frames(zen, 12);
            drawn = zen.glFramesDrawn();
            skipped = zen.glFramesSkipped();
            frames(zen, 5);
            check(zen.glFramesDrawn() == drawn && zen.glFramesSkipped() == skipped + 10, "both windows are skipped, five frames each");
            zen.makeCurrent(second);
            secondLabel->setText(String("changed"));
            zen.makeCurrent(previous);
            drawn = zen.glFramesDrawn();
            skipped = zen.glFramesSkipped();
            frames(zen, 1);
            check(zen.glFramesDrawn() == drawn + 1 && zen.glFramesSkipped() == skipped + 1,
                    "a change in the second window draws only the second window");
            zen.closeWindow(second);
            frames(zen, 2);
        }
    }

    { // the switch
        ig::zen::App zen;
        if (!makeApp(zen, false)) return 77;
        zen.widgets().addStage("main")->createChild<Label>(String("static"));
        zen.widgets().setTransition(TransitionType::None, 0.0f);
        zen.widgets().setStage("main");
        frames(zen, 20);
        check(zen.glFramesSkipped() == 0 && zen.glFramesDrawn() == 20, "with skipUnchangedGl = false every frame is drawn");
    }

    if (failures)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_zen_gl_skip: ok\n");
    return 0;
}
