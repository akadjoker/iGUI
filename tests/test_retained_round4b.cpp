// Round 4, second half (docs/ROUND4B.md), the parts that are not menus: the synthetic input API of WidgetApp, the
// SideTabBar and ToolBar with an application's own icons, the timeline's bars and beats, the piano roll's velocities and
// note signals, the chart's double data and axis titles, the slider's readout. Headless: the real frame loop
// (tests/RetainedHarness.hpp), pixels of the software rasteriser where a picture is the point. No standard library: ct and C
// only.

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

#include <igui/widgets/AudioWidgets.hpp>
#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/SideTabBar.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/Timeline.hpp>
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

// ---- item 19: the bars and beats ruler of the Timeline ----

// The pixel at (x, y) is this colour exactly.
bool isColour(const Shot& s, int x, int y, int r, int g, int b)
{
    const Pixel p = pixelAt(s, x, y);
    return p.r == r && p.g == g && p.b == b;
}

// The pixels of a w by h window of `a` at (xa, y) are those of `b` at (xb, y).
bool sameCrop(const Shot& a, int xa, const Shot& b, int xb, int y, int w, int h)
{
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
        {
            const Pixel p = pixelAt(a, xa + i, y + j), q = pixelAt(b, xb + i, y + j);
            if (p.r != q.r || p.g != q.g || p.b != q.b) return false;
        }
    return true;
}

void testTimelineBarRuler()
{
    // 160 pixels of header, 16 of padding each side and 400 of time: 50 pixels for each second, so every tick is on a pixel
    Screen screen(592.0f, 120.0f);
    Timeline* tl = screen.h.root()->createChild<Timeline>();
    tl->setRect(Rect(0.0f, 0.0f, 592.0f, 120.0f));
    tl->setTimeRange(0.0f, 8.0f);
    tl->setPlayhead(100.0f); // out of the view: at 0 it would be drawn over the first tick
    screen.h.frames(2);
    const int rowMajor = 15, rowMinor = 21; // a major tick covers the last 10 rows of the 24 of the ruler, a minor one the last 5
    const int bgR = 38, bgG = 40, bgB = 46;
    const Shot seconds = screen.take();

    check(!tl->barRuler() && tl->barRulerBeats() == 0, "the ruler is in seconds by default");
    tl->setBarRuler(120.0f, 4); // a bar is 2 seconds: 100 pixels
    check(tl->barRuler() && tl->barRulerTempo() == 120.0f && tl->barRulerBeats() == 4, "setBarRuler turns the bars and beats ruler on");
    screen.h.frames(2);
    const Shot bars = screen.take();
    check(differing(seconds, bars, 160, 0, 592, 24) > 100, "and the ruler is drawn differently");
    check(differing(seconds, bars, 0, 25, 592, 120) == 0 && differing(seconds, bars, 0, 0, 160, 25) == 0, "nothing else changes");
    // a major tick at each bar line (0, 2, 4, 6, 8 seconds: x 176, 276, 376, 476, 576), none between
    bool majors = true;
    for (int bar = 0; bar <= 4; ++bar) majors = majors && isColour(bars, 176 + bar * 100, rowMajor, 80, 82, 90);
    check(majors, "a major tick at each bar line");
    check(isColour(bars, 226, rowMajor, bgR, bgG, bgB) && isColour(bars, 326, rowMajor, bgR, bgG, bgB), "and none at the beats");
    // a small tick at each beat (half a second: 25 pixels): 201, 226, 251, then the same in the next bar
    bool beats = true;
    for (int k = 1; k < 4; ++k) beats = beats && isColour(bars, 176 + k * 25, rowMinor, 55, 57, 63) && isColour(bars, 276 + k * 25, rowMinor, 55, 57, 63);
    check(beats, "a small tick at each beat of a bar");
    check(isColour(bars, 189, rowMinor, bgR, bgG, bgB), "and none anywhere else");
    // the label is the number of the bar, counted from 1: the tick of 4 seconds is the third bar, and its label is the
    // same picture as the label of the tick of 4 seconds in a view that starts there
    tl->setTimeRange(4.0f, 12.0f);
    screen.h.frames(2);
    const Shot shifted = screen.take();
    check(isColour(shifted, 176, rowMajor, 80, 82, 90), "(the bar of 4 seconds is at the left edge of the shifted view)");
    check(sameCrop(bars, 376 + 2, shifted, 176 + 2, 3, 12, 11), "the label of the third bar is the same wherever the bar is");
    check(!sameCrop(bars, 176 + 2, shifted, 176 + 2, 3, 12, 11), "and it is not the label of the first");
    tl->setTimeRange(0.0f, 8.0f);

    // a faster tempo and another time signature move the bar lines: 60 bpm, 4 beats is 4 seconds a bar; 120 bpm, 3 beats 1.5 s
    tl->setBarRuler(60.0f, 4);
    screen.h.frames(2);
    const Shot slow = screen.take();
    check(isColour(slow, 376, rowMajor, 80, 82, 90) && isColour(slow, 276, rowMajor, bgR, bgG, bgB) && isColour(slow, 576, rowMajor, 80, 82, 90),
            "at 60 bpm a bar is 4 seconds: the major ticks are at 0, 4 and 8");
    tl->setBarRuler(120.0f, 3);
    screen.h.frames(2);
    const Shot waltz = screen.take();
    check(isColour(waltz, 251, rowMajor, 80, 82, 90) && isColour(waltz, 326, rowMajor, 80, 82, 90) && isColour(waltz, 276, rowMajor, bgR, bgG, bgB),
            "in 3 beats to the bar the bar is 1.5 seconds: the major ticks are at 0, 1.5, 3 ...");

    // zoomed out so that the bars are 25 pixels apart: a label for every second bar, the bars between as small ticks, no beats
    tl->setBarRuler(120.0f, 4);
    tl->setTimeRange(0.0f, 32.0f);
    screen.h.frames(2);
    const Shot far = screen.take();
    check(isColour(far, 176, rowMajor, 80, 82, 90) && isColour(far, 226, rowMajor, 80, 82, 90) && isColour(far, 276, rowMajor, 80, 82, 90),
            "zoomed out, the major ticks are at every second bar (4 seconds)");
    check(isColour(far, 201, rowMajor, bgR, bgG, bgB) && isColour(far, 201, rowMinor, 55, 57, 63), "the bar between is a small tick");
    check(isColour(far, 189, rowMinor, bgR, bgG, bgB), "and the beats are left out");
    tl->setTimeRange(0.0f, 8.0f);

    // zoomed in so that there is room for the beats to be numbered: the beat 2 of the first bar is "1.2"
    tl->setTimeRange(0.0f, 2.0f);
    screen.h.frames(2);
    const Shot near = screen.take();
    // (a beat is 100 pixels: a small tick at 276 and a label right of it)
    check(isColour(near, 176 + 100, rowMinor, 55, 57, 63), "zoomed in, the beats are small ticks 100 pixels apart");
    check(differing(near, bars, 176 + 100 + 2, 3, 176 + 100 + 30, 14) > 10, "and are numbered");
    tl->setTimeRange(0.0f, 8.0f);

    // back to seconds: the picture is the first one, exactly
    tl->clearBarRuler();
    check(!tl->barRuler(), "clearBarRuler turns it off");
    screen.h.frames(2);
    check(differing(screen.take(), seconds, 0, 0, 592, 120) == 0, "and the ruler is as it was");

    // a tempo that is not a number, or not positive, is no bar ruler; the others are brought into range
    tl->setBarRuler(120.0f, 4);
    tl->setBarRuler(0.0f, 4);
    check(!tl->barRuler(), "a tempo of 0 turns it off");
    tl->setBarRuler(120.0f, 4);
    tl->setBarRuler(-5.0f, 4);
    check(!tl->barRuler(), "so does a negative one");
    tl->setBarRuler(120.0f, 4);
    tl->setBarRuler(NAN, 4);
    check(!tl->barRuler(), "so does NaN");
    tl->setBarRuler(INFINITY, 4);
    check(!tl->barRuler(), "and infinity");
    tl->setBarRuler(5000.0f, 100);
    check(tl->barRuler() && tl->barRulerTempo() == 999.0f && tl->barRulerBeats() == 64, "a tempo of 5000 and 100 beats are brought to 999 and 64");
    tl->setBarRuler(0.001f, 0);
    check(tl->barRulerTempo() == 1.0f && tl->barRulerBeats() == 1, "a tempo under 1 and no beats are brought to 1 and 1");
    screen.h.frames(2);
}

// Whatever the view and the tempo, painting the ruler ends and draws a bounded number of ticks (the number of vertices of a
// frame is counted: it must not grow with the range).
void testTimelineBarRulerHostile()
{
    Screen screen(592.0f, 120.0f);
    Timeline* tl = screen.h.root()->createChild<Timeline>();
    tl->setRect(Rect(0.0f, 0.0f, 592.0f, 120.0f));
    int worst = 0;
    const float ranges[][2] = { { 0.0f, 8.0f }, { 0.0f, 1.0e9f }, { -1.0e9f, 1.0e9f }, { 1.0e8f, 1.0e8f + 3.0f }, { 0.0f, 0.001f }, { 0.0f, 3600.0f * 100.0f } };
    const float tempos[] = { 1.0f, 60.0f, 120.0f, 999.0f };
    const int beatsPerBar[] = { 1, 4, 64 };
    for (const auto& r: ranges)
        for (float tempo: tempos)
            for (int beats: beatsPerBar)
            {
                tl->setTimeRange(r[0], r[1]);
                tl->setBarRuler(tempo, beats);
                screen.h.frames(2);
                const int vertices = static_cast<int>(GetDrawData()->passes.empty() ? 0 : GetDrawData()->passes[0].list->vertices().size());
                if (vertices > worst) worst = vertices;
            }
    // a bar of 64 beats at 1000 pixels is nothing to draw but 64 + 1 ticks and their labels; the ceiling is far from that
    if (worst >= 60000) printf("    a frame drew %d vertices\n", worst);
    check(worst > 0 && worst < 60000, "a frame with the bar ruler draws a bounded number of vertices for any range and tempo");
}

// ---- item 19: the piano roll's velocity, and the signals that say which note ----

struct RollLog
{
    char order[32] = {};
    int n = 0;
    int created = -1, moved = -1, deleted = -1, removed = -1;
    int createdCount = 0, movedCount = 0, deletedCount = 0, addedCount = 0;
    int sizeAtRemoved = -1, sizeAtDeleted = -1;
    PianoNote gone = { 0, 0.0f, 0.0f };
    void add(char c)
    {
        if (n < 31) order[n++] = c;
    }
};

void testPianoRollVelocityAndSignals()
{
    Harness h(480.0f, 260.0f, false);
    WidgetApp& app = WidgetApp::instance();
    PianoRoll* roll = h.root()->createChild<PianoRoll>();
    roll->setRect(Rect(20.0f, 20.0f, 400.0f, 200.0f));
    roll->setPitchRange(48, 60); // 12 rows of 12 pixels
    roll->setTimeRange(0.0f, 8.0f);
    h.frame();
    // the grid starts 36 pixels in (the keys): 8 beats in 364 pixels, 45.5 pixels a beat; the top row is pitch 59
    const float x2 = 56.0f + 91.0f + 3.0f; // beat 2, a little in
    const float y55 = 20.0f + 4 * 12.0f + 6.0f; // the row of pitch 55

    RollLog log;
    roll->onNoteAdded.connect([&](int, float, float) { log.add('A'); ++log.addedCount; });
    roll->onNoteCreated.connect([&](int i) { log.add('C'); log.created = i; ++log.createdCount; });
    roll->onNoteMoved.connect([&](int i) { log.add('M'); log.moved = i; ++log.movedCount; });
    roll->onNoteRemoved.connect([&](int i) { log.add('R'); log.removed = i; log.sizeAtRemoved = static_cast<int>(roll->notes().size()); });
    roll->onNoteDeleted.connect([&](int i, const PianoNote& note) { log.add('D'); log.deleted = i; ++log.deletedCount; log.gone = note; log.sizeAtDeleted = static_cast<int>(roll->notes().size()); });
    auto click = [&](float x, float y, int button = 0)
    {
        app.feedMouseMove(x, y);
        app.feedMouseButton(button, true);
        app.feedMouseButton(button, false);
    };

    // put a note: the left button on an empty place
    click(x2, y55);
    check(roll->notes().size() == 1 && roll->notes()[0].pitch == 55 && roll->notes()[0].start == 2.0f, "a click on an empty place puts a note (pitch 55, beat 2)");
    check(roll->notes()[0].velocity == 0.8f, "with the default velocity, 0.8");
    check(log.createdCount == 1 && log.created == 0 && log.movedCount == 0, "onNoteCreated says which note, once");
    check(log.n == 2 && log.order[0] == 'A' && log.order[1] == 'C', "after onNoteAdded, which is emitted as it always was");

    // a click on the note that does not move it: onNoteAdded as before, nothing new
    roll->setNoteVelocity(0, 0.3f);
    log = RollLog();
    click(x2, y55);
    check(log.addedCount == 1 && log.createdCount == 0 && log.movedCount == 0, "a click on a note emits onNoteAdded as it did, and no created or moved");
    // drag it to a quarter of a beat it is already on: still not moved
    log = RollLog();
    app.feedMouseMove(x2, y55);
    app.feedMouseButton(0, true);
    app.feedMouseMove(x2 + 3.0f, y55);
    app.feedMouseButton(0, false);
    check(log.movedCount == 0 && roll->notes()[0].start == 2.0f, "a drag that ends where the note was is not a move");
    // drag it two beats right and two rows up
    app.feedMouseMove(x2, y55);
    app.feedMouseButton(0, true);
    app.feedMouseMove(x2 + 91.0f, y55 - 24.0f);
    check(log.movedCount == 0, "(nothing is said while the button is down)");
    app.feedMouseButton(0, false);
    check(log.movedCount == 1 && log.moved == 0, "onNoteMoved says which note, once, when the button goes up");
    check(roll->notes()[0].start == 4.0f && roll->notes()[0].pitch == 57, "(it is at beat 4, pitch 57)");
    check(roll->notes()[0].velocity == 0.3f, "and keeps its velocity");
    check(log.createdCount == 0, "a move is not a creation");
    check(log.order[log.n - 1] == 'M', "(onNoteMoved comes last)");

    // a second note, to see that the index is the note's
    const float x6 = 56.0f + 6 * 45.5f + 3.0f, y50 = 20.0f + 9 * 12.0f + 6.0f; // beat 6, pitch 50
    log = RollLog();
    click(x6, y50);
    check(roll->notes().size() == 2 && log.created == 1, "the second note is index 1");
    log = RollLog();
    click(56.0f + 4 * 45.5f + 3.0f, 20.0f + 2 * 12.0f + 6.0f); // the first note again, at beat 4, pitch 57
    // right click on the first: removed before, deleted after, with the note
    log = RollLog();
    click(56.0f + 4 * 45.5f + 3.0f, 20.0f + 2 * 12.0f + 6.0f, 1);
    check(roll->notes().size() == 1, "(the first note is gone)");
    check(log.removed == 0 && log.sizeAtRemoved == 2, "onNoteRemoved is emitted before the note is gone (two notes then)");
    check(log.deleted == 0 && log.deletedCount == 1 && log.sizeAtDeleted == 1, "onNoteDeleted is emitted after it (one note then), with its index");
    check(log.gone.pitch == 57 && log.gone.start == 4.0f && log.gone.velocity == 0.3f, "and the note it was, with its velocity");
    check(log.n == 2 && log.order[0] == 'R' && log.order[1] == 'D', "removed, then deleted");
    check(roll->notes()[0].pitch == 50, "(what is left is the second note, now index 0)");

    // velocity
    roll->clearNotes();
    roll->addNote(60, 1.0f, 1.0f);
    roll->addNote(61, 1.0f, 1.0f, 0.5f);
    roll->addNote(62, 1.0f, 1.0f, 1.7f);
    roll->addNote(63, 1.0f, 1.0f, -3.0f);
    roll->addNote(64, 1.0f, 1.0f, NAN);
    roll->addNote(65, NAN, 1.0f, 0.5f);
    check(roll->notes().size() == 5, "a note with a start that is not a number is not added, with a velocity or without");
    check(roll->notes()[0].velocity == 0.8f && roll->notes()[1].velocity == 0.5f && roll->notes()[2].velocity == 1.0f &&
                    roll->notes()[3].velocity == 0.0f && roll->notes()[4].velocity == 0.8f,
            "addNote takes a velocity: 0.8 by default, kept, clamped to 1 and to 0, 0.8 for NaN");
    roll->setNoteVelocity(0, 0.25f);
    roll->setNoteVelocity(1, NAN);
    roll->setNoteVelocity(2, 9.0f);
    roll->setNoteVelocity(-1, 0.1f);
    roll->setNoteVelocity(77, 0.1f);
    check(roll->notes()[0].velocity == 0.25f && roll->notes()[1].velocity == 0.5f && roll->notes()[2].velocity == 1.0f && roll->notes().size() == 5,
            "setNoteVelocity sets it, clamps it, and ignores NaN and bad indices");
    // the aggregate of before still works
    PianoNote old = { 60, 1.0f, 2.0f };
    check(old.velocity == 0.8f, "a PianoNote made with three values has the default velocity");
}

void testPianoRollShowVelocity()
{
    Screen screen(480.0f, 260.0f);
    PianoRoll* roll = screen.h.root()->createChild<PianoRoll>();
    roll->setRect(Rect(20.0f, 20.0f, 400.0f, 200.0f));
    roll->setPitchRange(48, 60);
    roll->setTimeRange(0.0f, 8.0f);
    roll->addNote(55, 2.0f, 2.0f, 1.0f);
    roll->addNote(52, 2.0f, 2.0f, 0.0f);
    screen.h.frames(2);
    const Shot plain = screen.take();
    check(!roll->showVelocity(), "the velocity is not shown by default");
    roll->setShowVelocity(true);
    check(roll->showVelocity(), "setShowVelocity(true) is kept");
    screen.h.frames(2);
    const Shot shown = screen.take();
    // the note of velocity 1 is drawn as it was; the one of velocity 0 is fainter
    const int xn = 56 + 91 + 20; // inside both notes, a good way from their edges
    const int yHigh = 20 + 4 * 12 + 6, yLow = 20 + 7 * 12 + 6;
    check(pixelAt(shown, xn, yHigh).r == pixelAt(plain, xn, yHigh).r && pixelAt(shown, xn, yHigh).b == pixelAt(plain, xn, yHigh).b,
            "a note of velocity 1 is drawn as without the velocity");
    check(pixelAt(shown, xn, yLow).b < pixelAt(plain, xn, yLow).b - 20, "a note of velocity 0 is drawn fainter");
    roll->setNoteVelocity(1, 1.0f);
    screen.h.frames(2);
    check(differing(screen.take(), plain, 0, 0, 480, 260) == 0, "with every velocity at 1 the picture is the one without the velocity");
    roll->setShowVelocity(false);
    roll->setNoteVelocity(1, 0.0f);
    screen.h.frames(2);
    check(differing(screen.take(), plain, 0, 0, 480, 260) == 0, "and with it off the velocity changes nothing in the picture");
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
    RUN(testTimelineBarRuler);
    RUN(testTimelineBarRulerHostile);
    RUN(testPianoRollVelocityAndSignals);
    RUN(testPianoRollShowVelocity);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
