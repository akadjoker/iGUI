// Round 4, second half (docs/ROUND4B.md), the parts that are not menus: the synthetic input API of WidgetApp, the
// SideTabBar and ToolBar with an application's own icons, the timeline's bars and beats, the piano roll's velocities and
// note signals, the chart's double data and axis titles, the slider's readout. Headless: the real frame loop
// (tests/RetainedHarness.hpp), pixels of the software rasteriser where a picture is the point. No standard library: ct and C
// only.

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/SideTabBar.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/ToolBar.hpp>

using namespace rh;

namespace
{

// ---- item 16: synthetic input ----

// Records what reaches it.
class InputProbe : public Widget
{
public:
    IG_WIDGET_CLASS(InputProbe, Widget);

    InputProbe()
    {
        acceptsFocus_ = true;
    }
    struct Press
    {
        int button;
        bool shift, ctrl, alt;
        int clicks;
        float x, y;
    };
    ct::Vector<Press> presses;
    ct::Vector<int> releases;
    int moves = 0;
    float lastX = -1.0f, lastY = -1.0f;
    float scrollX = 0.0f, scrollY = 0.0f;
    int scrolls = 0;
    struct Key
    {
        int key;
        bool down, shift, ctrl, alt;
    };
    ct::Vector<Key> keys;
    String typed;

    void onMousePress(MouseEvent& e) override
    {
        presses.push_back({ e.button, e.shift, e.ctrl, e.alt, e.clickCount, e.x, e.y });
        e.consumed = true;
    }
    void onMouseRelease(MouseEvent& e) override
    {
        releases.push_back(e.button);
        e.consumed = true;
    }
    void onMouseMove(MouseEvent& e) override
    {
        ++moves;
        lastX = e.x;
        lastY = e.y;
    }
    void onMouseScroll(MouseEvent& e) override
    {
        ++scrolls;
        scrollX += e.scrollX;
        scrollY += e.scrollY;
        e.consumed = true;
    }
    void onKeyPress(KeyEvent& e) override
    {
        keys.push_back({ e.key, true, e.shift, e.ctrl, e.alt });
        e.consumed = true;
    }
    void onKeyRelease(KeyEvent& e) override
    {
        keys.push_back({ e.key, false, e.shift, e.ctrl, e.alt });
        e.consumed = true;
    }
    void onTextInput(KeyEvent& e) override
    {
        typed += String(e.text);
        e.consumed = true;
    }
};

void testSyntheticInput()
{
    Harness h(420.0f, 300.0f, false);
    WidgetApp& app = WidgetApp::instance();
    InputProbe* probe = h.root()->createChild<InputProbe>();
    probe->setRect(Rect(40.0f, 40.0f, 200.0f, 120.0f));
    Button* button = h.root()->createChild<Button>("Click");
    button->setRect(Rect(260.0f, 40.0f, 100.0f, 30.0f));
    int clicks = 0;
    button->clicked.connect([&]() { ++clicks; });
    h.frame(); // one frame lays the window out; the events below come with no frame in between

    // the pointer: a move reaches the widget under it, at the point given
    app.feedMouseMove(100.0f, 80.0f);
    check(probe->moves == 1 && probe->lastX == 100.0f && probe->lastY == 80.0f, "feedMouseMove moves the pointer: the widget under it hears it");
    check(probe->isHovered(), "and is hovered");
    check(app.mouseX() == 100.0f && app.mouseY() == 80.0f, "the application's pointer is there");

    // a click: press and release of the left button, where the pointer is, with no frame between them
    app.feedMouseButton(0, true);
    check(probe->presses.size() == 1 && probe->presses[0].button == 0 && probe->presses[0].x == 100.0f && probe->presses[0].y == 80.0f &&
                    !probe->presses[0].shift && !probe->presses[0].ctrl && !probe->presses[0].alt && probe->presses[0].clicks == 1,
            "feedMouseButton(0, true) presses the left button where the pointer is");
    check(probe->releases.size() == 0, "and nothing is released yet");
    app.feedMouseButton(0, false);
    check(probe->releases.size() == 1 && probe->releases[0] == 0, "feedMouseButton(0, false) releases it");
    // pressing what is down does nothing, as in update()
    app.feedMouseButton(0, false);
    app.feedMouseButton(1, true);
    app.feedMouseButton(1, true);
    app.feedMouseButton(1, false);
    check(probe->presses.size() == 2 && probe->presses[1].button == 1 && probe->releases.size() == 2 && probe->releases[1] == 1,
            "a release of a button that is not down, and a second press of one that is, do nothing; button 1 is the right one");
    // modifiers go with the event
    app.feedMouseButton(2, true, true, true, false);
    app.feedMouseButton(2, false);
    check(probe->presses.size() == 3 && probe->presses[2].button == 2 && probe->presses[2].shift && probe->presses[2].ctrl && !probe->presses[2].alt,
            "the modifiers given are those of the press (and button 2 is the middle one)");
    // out of range buttons are ignored
    app.feedMouseButton(7, true);
    app.feedMouseButton(-1, true);
    check(probe->presses.size() == 3, "a button that does not exist is ignored");
    // time does not pass: two clicks on the same spot, with no frame between, are a double click
    app.feedMouseButton(0, true);
    app.feedMouseButton(0, false);
    app.feedMouseButton(0, true);
    app.feedMouseButton(0, false);
    check(probe->presses.size() == 5 && probe->presses[4].clicks == 2, "no time passes in a feed: the second click is a double click");
    h.frames(30); // half a second of frames
    app.feedMouseButton(0, true);
    app.feedMouseButton(0, false);
    check(probe->presses.size() == 6 && probe->presses[5].clicks == 1, "a click after the frames is a single click again");

    // the wheel goes where the pointer is
    app.feedMouseWheel(0.0f, 2.0f);
    app.feedMouseWheel(1.5f, -1.0f);
    check(probe->scrolls == 2 && probe->scrollX == 1.5f && probe->scrollY == 1.0f, "feedMouseWheel turns the wheel under the pointer, in the sign given");

    // a real frame after the events finds the state they left: the pointer stays, nothing is pressed or released again
    const int pressesBefore = static_cast<int>(probe->presses.size()), releasesBefore = static_cast<int>(probe->releases.size());
    h.frames(5);
    check(probe->isHovered() && app.mouseX() == 100.0f && app.mouseY() == 80.0f, "a frame after a feed leaves the pointer where the feed put it");
    check(static_cast<int>(probe->presses.size()) == pressesBefore && static_cast<int>(probe->releases.size()) == releasesBefore,
            "and does not repeat a press or a release");
    check(h.io().mouseEvents.empty() && h.io().keyEvents.empty() && h.io().inputChars.empty(), "the backend's IO has no events of the feed in it");

    // the keys: to the widget that has the focus
    app.feedMouseButton(0, true); // the probe takes the focus (it accepts it) with a click
    app.feedMouseButton(0, false);
    check(app.focused() == probe, "(the probe has the focus)");
    app.feedKey(ig::retained::Key::A, true, true, false, false);
    app.feedKey(ig::retained::Key::A, false);
    app.feedKey(ig::retained::Key::Return);
    check(probe->keys.size() == 3, "feedKey sends the key down and up events to the focused widget");
    if (probe->keys.size() == 3)
    {
        check(probe->keys[0].key == ig::retained::Key::A && probe->keys[0].down && probe->keys[0].shift && !probe->keys[0].ctrl &&
                        !probe->keys[0].alt,
                "with the modifiers given");
        check(probe->keys[1].key == ig::retained::Key::A && !probe->keys[1].down && !probe->keys[1].shift,
                "the release is a release, and has no modifiers unless given");
        check(probe->keys[2].key == ig::retained::Key::Return && probe->keys[2].down, "down is the default");
    }

    // the text: UTF-8, each character as it was typed
    app.feedText("ab");
    app.feedText("Mus\xC3\xADica \xE2\x82\xAC \xF0\x9F\x8E\xB5");
    check(probe->typed == "abMus\xC3\xADica \xE2\x82\xAC \xF0\x9F\x8E\xB5", "feedText delivers the typed text, accents, the euro sign and an emoji included");
    app.feedText("");
    app.feedText(nullptr);

    // widgets work from the feed as they do from a window: a Button is clicked, a TextInput typed into
    app.feedMouseMove(300.0f, 55.0f);
    app.feedMouseButton(0, true);
    check(clicks == 0, "a Button is not clicked by the press");
    app.feedMouseButton(0, false);
    check(clicks == 1, "a Button is clicked by a press and a release on it");
    TextInput* input = h.root()->createChild<TextInput>();
    input->setRect(Rect(40.0f, 200.0f, 200.0f, 26.0f));
    h.frame();
    app.feedMouseMove(100.0f, 213.0f);
    app.feedMouseButton(0, true);
    app.feedMouseButton(0, false);
    check(app.focused() == input, "a click on a TextInput focuses it");
    app.feedText("Ol\xC3\xA1");
    check(input->text() == "Ol\xC3\xA1", "text fed to a focused TextInput is typed in it");
    app.feedKey(ig::retained::Key::Backspace);
    check(input->text() == "Ol", "and a key reaches it too");
}

// ---- item 17: SideTabBar and ToolBar with the application's own icons, and Theme values ----

// A headless window drawn by the software rasteriser, like rh::Screen, that can also make textures of its own: the
// application's icons.
class IconScreen
{
public:
    IconScreen(float width, float height)
            : h(width, height, false),
              w_(static_cast<int>(width)),
              h_(static_cast<int>(height))
    {
        raster_.setSamples(1);
        atlasTexture_ = raster_.createTexture(h.atlas().width(), h.atlas().height(), h.atlas().pixels());
        h.atlas().setTexture(atlasTexture_);
    }
    ~IconScreen()
    {
        raster_.destroyTexture(atlasTexture_);
        raster_.destroyTexture(iconTexture_);
    }
    IconScreen(const IconScreen&) = delete;
    IconScreen& operator=(const IconScreen&) = delete;

    // The application's icon: a 16 by 16 picture of one colour, registered as an icon every widget that takes an IconId shows.
    IconId solidIcon(unsigned r, unsigned g, unsigned b)
    {
        unsigned char px[16 * 16 * 4];
        for (int i = 0; i < 16 * 16; ++i)
        {
            px[i * 4 + 0] = static_cast<unsigned char>(r);
            px[i * 4 + 1] = static_cast<unsigned char>(g);
            px[i * 4 + 2] = static_cast<unsigned char>(b);
            px[i * 4 + 3] = 255;
        }
        iconTexture_ = raster_.createTexture(16, 16, px);
        return registerImageIcon(iconTexture_, Rect(0.0f, 0.0f, 1.0f, 1.0f));
    }
    Shot take()
    {
        Shot shot;
        shot.w = w_;
        shot.h = h_;
        shot.px.resize(static_cast<size_t>(w_) * static_cast<size_t>(h_));
        raster_.render(*GetDrawData(), shot.px.data(), w_, h_, w_, 0xff000000u);
        return shot;
    }
    Harness h;

private:
    ig::zen::SoftRaster raster_;
    TextureHandle atlasTexture_, iconTexture_;
    int w_, h_;
};

struct Pixel
{
    int r, g, b;
};

Pixel pixelAt(const Shot& s, int x, int y)
{
    const uint32_t v = s.px[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)];
    return { static_cast<int>((v >> 16) & 255u), static_cast<int>((v >> 8) & 255u), static_cast<int>(v & 255u) };
}

// The reddest pixel of a rectangle: where a red icon is, whatever the rectangle holds around it.
Pixel reddest(const Shot& s, const Rect& r)
{
    Pixel best = { 0, 255, 255 };
    for (int y = static_cast<int>(r.y); y < static_cast<int>(r.y + r.h); ++y)
        for (int x = static_cast<int>(r.x); x < static_cast<int>(r.x + r.w); ++x)
        {
            const Pixel p = pixelAt(s, x, y);
            if (p.r - p.g > best.r - best.g) best = p;
        }
    return best;
}

bool near(int a, int b, int slack)
{
    return a >= b - slack && a <= b + slack;
}

void testSideTabBarColoredIcons()
{
    IconScreen screen(300.0f, 300.0f);
    const IconId red = screen.solidIcon(255, 0, 0);
    SideTabBar* side = screen.h.root()->createChild<SideTabBar>(SideTabBar::Side::Left);
    side->setRect(Rect(0.0f, 0.0f, 300.0f, 300.0f));
    side->addPage<Widget>(red, "One");
    side->addPage<Widget>(red, "Two");
    side->addPage<Widget>(red, "Three");
    side->setPageEnabled(2, false);
    side->setCurrentIndex(0);
    screen.h.frames(2);
    Rect b0, b1, b2;
    check(side->buttonRect(0, b0) && side->buttonRect(1, b1) && side->buttonRect(2, b2), "(the three buttons have rectangles)");

    check(!side->coloredIcons(), "icons are tinted by default");
    Shot tinted = screen.take();
    const Pixel t0 = reddest(tinted, b0), t1 = reddest(tinted, b1);
    check(t0.r < 200 && t1.r < 200, "a registered icon is tinted with the colour of the button by default (it comes out dark)");
    const Shot beforeShot = tinted;

    side->setColoredIcons(true);
    check(side->coloredIcons(), "setColoredIcons(true) is kept");
    screen.h.frames(2);
    const Shot shot = screen.take();
    const Pixel c0 = reddest(shot, b0), c1 = reddest(shot, b1), c2 = reddest(shot, b2);
    if (!near(c0.r, 255, 1)) printf("    the open page's icon is %d %d %d\n", c0.r, c0.g, c0.b);
    check(near(c0.r, 255, 1) && c0.g <= 2 && c0.b <= 2, "the icon of the open page is drawn in its own colour, whole");
    if (!near(c1.r, 206, 8)) printf("    the idle icon is %d %d %d\n", c1.r, c1.g, c1.b);
    check(near(c1.r, 206, 8) && c1.g < 20 && c1.b < 20, "the icon of an idle page is a little fainter");
    if (!near(c2.r, 99, 10)) printf("    the disabled icon is %d %d %d\n", c2.r, c2.g, c2.b);
    check(near(c2.r, 99, 10), "the icon of a disabled page is much fainter");

    side->setColoredIcons(false);
    screen.h.frames(2);
    const Shot back = screen.take();
    check(differing(back, beforeShot, 0, 0, back.w, back.h) == 0, "and false is the picture of before");
}

void testToolBarColoredIconsAndTheme()
{
    IconScreen screen(300.0f, 120.0f);
    const IconId red = screen.solidIcon(255, 0, 0);
    ToolBar* bar = screen.h.root()->createChild<ToolBar>();
    bar->setRect(Rect(0.0f, 20.0f, 300.0f, 40.0f));
    const int a = bar->addButton(red, "plain");
    const int off = bar->addToggle(red, "off", false);
    const int on = bar->addToggle(red, "on", true);
    const int dis = bar->addButton(red, "disabled");
    bar->setItemEnabled(dis, false);
    screen.h.frames(2);
    Rect ra, roff, ron, rdis;
    check(bar->itemRect(a, ra) && bar->itemRect(off, roff) && bar->itemRect(on, ron) && bar->itemRect(dis, rdis), "(the four items have rectangles)");
    Theme& theme = Theme::instance();
    const Theme saved = theme;

    // the defaults are the colours that were fixed in the paint
    check(theme.toolbarChecked.r == 60 && theme.toolbarChecked.g == 100 && theme.toolbarChecked.b == 160, "the default checked background is the one it always was");
    check(!bar->coloredIcons() && bar->bottomBorder(), "icons are tinted and the border is drawn by default");
    const Shot plain = screen.take();
    const Pixel checkedBg = pixelAt(plain, static_cast<int>(ron.x) + 2, static_cast<int>(ron.y + ron.h * 0.5f));
    check(checkedBg.r == 60 && checkedBg.g == 100 && checkedBg.b == 160, "a checked toggle has the background of the theme (the default one)");
    const Pixel dot = pixelAt(plain, static_cast<int>(ron.x + ron.w) - 5, static_cast<int>(ron.y + ron.h) - 5);
    check(dot.r == 100 && dot.g == 200 && dot.b == 255, "and its dot (the default one)");
    const Pixel tintedA = reddest(plain, ra);
    check(near(tintedA.r, 220, 4) && tintedA.g < 10, "an icon is tinted with the toolbarIcon colour by default");

    // Theme values: the background and the dot of a checked toggle, the tint of the icons
    theme.toolbarChecked = Color(10, 200, 30, 255);
    theme.toolbarCheckedDot = Color(0, 0, 0, 0);
    theme.toolbarIcon = Color(120, 255, 255, 255);
    screen.h.frames(2);
    const Shot themed = screen.take();
    const Pixel bg2 = pixelAt(themed, static_cast<int>(ron.x) + 2, static_cast<int>(ron.y + ron.h * 0.5f));
    check(bg2.r == 10 && bg2.g == 200 && bg2.b == 30, "a checked toggle takes the toolbarChecked colour of the theme");
    const Pixel dot2 = pixelAt(themed, static_cast<int>(ron.x + ron.w) - 5, static_cast<int>(ron.y + ron.h) - 5);
    check(!(dot2.r == 100 && dot2.g == 200 && dot2.b == 255), "with no dot when the theme's dot has no alpha (the icon is where the dot was)");
    const Pixel tintedA2 = reddest(themed, ra);
    check(near(tintedA2.r, 120, 4), "the icons take the toolbarIcon tint of the theme");
    theme = saved;

    // coloured icons: their own colours, the state in how faint they are
    bar->setColoredIcons(true);
    check(bar->coloredIcons(), "setColoredIcons(true) is kept");
    screen.h.frames(2);
    const Shot colored = screen.take();
    const Pixel pa = reddest(colored, ra), poff = reddest(colored, roff), pon = reddest(colored, ron), pdis = reddest(colored, rdis);
    check(near(pa.r, 255, 1) && pa.g <= 2, "a coloured icon is drawn whole");
    if (!near(poff.r, 202, 8)) printf("    the idle toggle is %d %d %d\n", poff.r, poff.g, poff.b);
    check(near(poff.r, 202, 8), "a toggle that is off and not hovered is a little fainter");
    check(near(pon.r, 255, 1) && pon.g <= 2, "a checked toggle is whole, on the background of the theme");
    if (!near(pdis.r, 111, 10)) printf("    the disabled button is %d %d %d\n", pdis.r, pdis.g, pdis.b);
    check(near(pdis.r, 111, 10), "a disabled button is much fainter");
    bar->setColoredIcons(false);

    // the line along the bottom edge
    const Rect abs = bar->absoluteRect();
    const int lineY = static_cast<int>(abs.y + abs.h) - 1;
    screen.h.frames(2);
    const Shot withLine = screen.take();
    bar->setBottomBorder(false);
    check(!bar->bottomBorder(), "setBottomBorder(false) is kept");
    screen.h.frames(2);
    const Shot noLine = screen.take();
    const int row = differing(withLine, noLine, 0, lineY, withLine.w, lineY + 1);
    check(row > 200, "the line along the bottom edge is the bar's width, and goes away");
    // (the line is drawn a few pixels thick by the rasteriser, soft at its edges: a band around its row)
    const int above = differing(withLine, noLine, 0, 0, withLine.w, lineY - 4), below = differing(withLine, noLine, 0, lineY + 5, withLine.w, withLine.h);
    check(above == 0 && below == 0, "and nothing else changes, away from the line");
    bar->setBottomBorder(true);
    screen.h.frames(2);
    check(differing(screen.take(), withLine, 0, 0, withLine.w, withLine.h) == 0, "true brings it back");
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testSyntheticInput);
    RUN(testSideTabBarColoredIcons);
    RUN(testToolBarColoredIconsAndTheme);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
