// The basic retained widgets (CheckBox, Button, Label, ComboBox, ImageButton...): what they draw and the size they
// ask for, checked on the pixels of the software rasteriser and on their size hints. Driven through the real frame
// loop (tests/RetainedHarness.hpp). No standard library: ct and C only.

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

#include <igui/widgets/AppWidgets.hpp>
#include <igui/widgets/AudioWidgets.hpp>
#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/ViewWidgets.hpp>

using namespace rh;

namespace
{

float distance2(uint32_t px, const Color& c)
{
    const float r = static_cast<float>((px >> 16) & 255u) - static_cast<float>(c.r);
    const float g = static_cast<float>((px >> 8) & 255u) - static_cast<float>(c.g);
    const float b = static_cast<float>(px & 255u) - static_cast<float>(c.b);
    return r * r + g * g + b * b;
}

// Whether the pixel at (x, y) is nearer to the colour of the mark than to the one of the box.
bool marked(const Shot& s, int x, int y, const Color& mark, const Color& bg)
{
    const uint32_t px = s.px[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)];
    return distance2(px, mark) < distance2(px, bg);
}

// How much of the inside of a box the mark covers (0..1), and how far it is from being the same when the box is turned
// left to right (0 for a square in the middle, large for a tick).
struct MarkReport
{
    float covered = 0.0f;
    float asymmetry = 0.0f;
};

MarkReport markInBox(const Shot& s, const Rect& box, const Color& mark, const Color& bg)
{
    MarkReport r;
    const int x0 = static_cast<int>(box.x) + 1, y0 = static_cast<int>(box.y) + 1;
    const int x1 = static_cast<int>(box.x + box.w) - 1, y1 = static_cast<int>(box.y + box.h) - 1;
    int total = 0, hit = 0, differ = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
        {
            ++total;
            const bool here = marked(s, x, y, mark, bg);
            if (here) ++hit;
            if (here != marked(s, x0 + (x1 - 1 - x), y, mark, bg)) ++differ;
        }
    r.covered = total > 0 ? static_cast<float>(hit) / static_cast<float>(total) : 0.0f;
    r.asymmetry = hit > 0 ? static_cast<float>(differ) / static_cast<float>(hit) : 0.0f;
    return r;
}

// A check mark is a tick, not a filled square: it covers a part of the inside of the box, and it leans.
bool looksLikeATick(const MarkReport& r)
{
    return r.covered >= 0.08f && r.covered <= 0.50f && r.asymmetry >= 0.35f;
}

void reportMark(const char* what, const MarkReport& r)
{
    if (!looksLikeATick(r))
        printf("    %s: covers %.0f%% of the box, asymmetry %.2f\n", what, static_cast<double>(r.covered) * 100.0,
                static_cast<double>(r.asymmetry));
}

// ---- CheckBox ----

Rect checkBoxBox(const CheckBox* c)
{
    const Theme& t = Theme::instance();
    const Rect abs = c->absoluteRect();
    return { abs.x + t.padding, abs.y + (abs.h - t.fontSize) * 0.5f, t.fontSize, t.fontSize };
}

void checkBoxScene(bool light, float fontSize, bool hovered)
{
    const Theme saved = Theme::instance();
    Theme::instance() = light ? Theme::light() : Theme::dark();
    Theme::instance().fontSize = fontSize;
    {
        Screen s(260.0f, 120.0f);
        BoxLayout* box = s.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(6.0f);
        CheckBox* off = box->createChild<CheckBox>("off");
        CheckBox* on = box->createChild<CheckBox>("on");
        on->setChecked(true);
        Collapsible* group = box->createChild<Collapsible>("Group", true);
        group->setCheckable(true);
        group->setChecked(true);
        s.h.relayout();
        const Theme& t = Theme::instance();
        char what[160];
        const char* mode = light ? "light" : "dark";
        if (hovered) s.h.moveTo(checkBoxBox(on).x + 2.0f, checkBoxBox(on).y + 2.0f);
        s.h.frames(2);
        const Shot shot = s.take();
        const Color& bg = hovered ? t.inputBgHover : t.inputBg;
        const MarkReport onMark = markInBox(shot, checkBoxBox(on), t.checkMark, bg);
        snprintf(what, sizeof what, "%s theme, font %.0f, %s: a checked CheckBox", mode, static_cast<double>(fontSize),
                hovered ? "hovered" : "plain");
        reportMark(what, onMark);
        check(looksLikeATick(onMark), "a checked CheckBox draws a tick, not a filled square");
        const MarkReport offMark = markInBox(shot, checkBoxBox(off), t.checkMark, t.inputBg);
        check(offMark.covered < 0.03f, "an unchecked CheckBox draws no tick");
        if (!hovered)
        {
            const MarkReport groupMark = markInBox(shot, group->checkRect(), t.checkMark, t.inputBg);
            snprintf(what, sizeof what, "%s theme, font %.0f: the header check box of a Collapsible", mode,
                    static_cast<double>(fontSize));
            reportMark(what, groupMark);
            check(looksLikeATick(groupMark), "a checked Collapsible header draws a tick, not a filled square");
        }
    }
    Theme::instance() = saved;
}

void testCheckBoxDrawsATick()
{
    for (int light = 0; light < 2; ++light)
        for (int hovered = 0; hovered < 2; ++hovered)
        {
            checkBoxScene(light != 0, 16.0f, hovered != 0);
            checkBoxScene(light != 0, 12.0f, hovered != 0);
            checkBoxScene(light != 0, 10.0f, hovered != 0);
        }
}

// ---- Button and SplitButton: a widget at its own size hint shows its whole label ----

// A widget of type W with each text, drawn at its size hint and again with room to spare: a label that is cut ends in
// dots and draws other glyphs, so the vertices differ.
template<typename W>
int hintsThatCut(Harness& h, const char* kind, const char* const* texts, int n)
{
    int bad = 0;
    for (int i = 0; i < n; ++i)
    {
        Widget* holder = h.root()->createChild<Widget>();
        W* w = holder->createChild<W>(String(texts[i]));
        const Widget::Vec2f hint = w->sizeHint();
        w->setRect({ 10.0f, 10.0f, hint.x, hint.y });
        h.frames(2);
        const size_t atHint = GetDrawList().vertices().size();
        w->setRect({ 10.0f, 10.0f, hint.x + 150.0f, hint.y });
        h.frames(2);
        const size_t wide = GetDrawList().vertices().size();
        h.root()->removeChild(holder);
        if (atHint != wide)
        {
            ++bad;
            printf("    %s \"%s\": %u vertices at its hint (%.1f wide), %u when wide\n", kind, texts[i],
                    static_cast<unsigned>(atHint), static_cast<double>(hint.x), static_cast<unsigned>(wide));
        }
    }
    return bad;
}

void testButtonAtItsHintShowsItsWholeLabel()
{
    const char* texts[] = { "W", "M", "i", "Wi", "WW", "MM", "mm", "Ok", "OK", "Gravar", "MMMM", "Ficheiro", "AVWXYZ" };
    const int n = static_cast<int>(sizeof texts / sizeof texts[0]);
    const float sizes[] = { 12.0f, 16.0f, 24.0f };
    for (float size: sizes)
    {
        const Theme saved = Theme::instance();
        Theme::instance().fontSize = size;
        {
            Harness h(480.0f, 120.0f, false);
            printf("  font %.0f\n", static_cast<double>(size));
            check(hintsThatCut<Button>(h, "Button", texts, n) == 0, "a Button at its size hint shows its whole label");
            check(hintsThatCut<SplitButton>(h, "SplitButton", texts, n) == 0,
                    "a SplitButton at its size hint shows its whole label");
        }
        Theme::instance() = saved;
    }
}

// ---- mouse buttons: the window delivers 0 (left), 1 (right) and 2 (middle) ----

void testPianoRollButtons()
{
    Harness h(520.0f, 360.0f, false);
    Widget* holder = h.root()->createChild<Widget>();
    PianoRoll* roll = holder->createChild<PianoRoll>();
    roll->setRect({ 20.0f, 20.0f, 460.0f, 300.0f });
    roll->setPitchRange(48, 72);
    roll->setTimeRange(0.0f, 8.0f);
    h.relayout();
    const float x = 20.0f + 36.0f + 150.0f, y = 20.0f + 150.0f;
    h.click(x, y, 0);
    check(roll->notes().size() == 1, "a left click in the grid of a PianoRoll adds a note");
    h.click(x + 2.0f, y, 1);
    check(roll->notes().size() == 0, "a right click on a note of a PianoRoll removes it");
    h.click(x, y, 2);
    check(roll->notes().size() == 0, "a middle click in the grid of a PianoRoll adds nothing");
    // a left click on a note drags it to another pitch
    h.click(x, y, 0);
    const int pitch = roll->notes().size() == 1 ? roll->notes()[0].pitch : -1;
    h.moveTo(x + 2.0f, y);
    h.down(0);
    h.moveTo(x + 2.0f, y - 36.0f);
    h.up(0);
    check(roll->notes().size() == 1 && roll->notes()[0].pitch != pitch, "a left drag on a note moves it");
}

void testWaveformViewButtons()
{
    Harness h(520.0f, 240.0f, false);
    Widget* holder = h.root()->createChild<Widget>();
    WaveformView* wave = holder->createChild<WaveformView>();
    wave->setRect({ 20.0f, 20.0f, 400.0f, 120.0f });
    float samples[64] = {};
    for (int i = 0; i < 64; ++i) samples[i] = (i & 1) ? 0.5f : -0.5f;
    wave->setSamples(samples, 64, 1, 44100.0f);
    wave->setViewRange(0.0f, 0.5f);
    h.relayout();
    h.click(220.0f, 80.0f, 0);
    check(wave->playhead() > 0.1f && wave->playhead() < 0.4f, "a left click on a WaveformView moves the playhead");
    const float before = wave->viewStart();
    h.moveTo(300.0f, 80.0f);
    h.down(1);
    h.moveTo(200.0f, 80.0f);
    h.up(1);
    check(wave->viewStart() != before, "a right drag on a WaveformView pans it");
}

// ---- FloatWindow: a move or a resize is in its rectangle at once ----

void testFloatWindowSettersLayOut()
{
    Harness h(640.0f, 480.0f, false);
    FloatWindow* fw = h.app.addFloat<FloatWindow>(String("Janela"));
    fw->setContent<Label>(String("conteudo"));
    fw->setFloatSize(300.0f, 200.0f);
    fw->setFloatPos(120.0f, 90.0f);
    // no frame yet: the rectangle already is where the window was put
    check(fw->rect().x == 120.0f && fw->rect().y == 90.0f, "setFloatPos puts the rectangle where it says, at once");
    check(fw->rect().w == 300.0f && fw->rect().h == 200.0f, "setFloatSize puts the size of the rectangle at once");
    check(fw->content() && fw->content()->rect().w > 200.0f, "setFloatSize lays the content out at once");
    fw->setFloatPos(300.0f, 220.0f);
    check(fw->absoluteRect().x == 300.0f && fw->absoluteRect().y == 220.0f, "a second setFloatPos moves it again");
}

// ---- Collapsible: its size hint does not depend on its own rectangle ----

void testCollapsibleHintIgnoresItsRect()
{
    Harness h(640.0f, 480.0f, false);
    Collapsible* c = h.root()->createChild<Collapsible>(String("Grupo"), true);
    c->createChild<Label>(String("conteudo"));
    const float narrow = c->sizeHint().x;
    c->setRect({ 0.0f, 0.0f, 500.0f, 100.0f });
    check(c->sizeHint().x == narrow, "a Collapsible asks for the same width whatever width it has");
    c->setRect({ 0.0f, 0.0f, 50.0f, 100.0f });
    check(c->sizeHint().x == narrow, "and when it is narrower than its hint");
}

// ---- layout asks for itself when what it depends on changes ----

// A widget that counts how many times it is laid out.
class LayoutProbe : public Widget
{
public:
    IG_WIDGET_CLASS(LayoutProbe, Widget);
    void layout() override
    {
        ++laidOut;
        Widget::layout();
    }
    int laidOut = 0;
};

void testLayoutRunsByItself()
{
    Harness h(640.0f, 480.0f, false);
    LayoutProbe* probe = h.root()->createChild<LayoutProbe>();
    h.frames(3);
    const int settled = probe->laidOut;
    h.frames(30);
    check(probe->laidOut == settled, "an idle window does no layout");

    // a child added after the first frame is laid out by the next one, with no requestLayout()
    Widget* late = h.root()->createChild<Widget>();
    h.frame();
    check(late->rect().w == 640.0f && late->rect().h == 480.0f, "a widget added later is laid out by the next frame");
    check(probe->laidOut == settled + 1, "and it took one layout pass");

    // visibility, text, spacing: a hidden child is skipped by a layout, a shown one is placed
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    Label* first = box->createChild<Label>(String("um"));
    Label* second = box->createChild<Label>(String("dois"));
    h.frame();
    const float yWithBoth = second->rect().y;
    first->setVisible(false);
    h.frame();
    check(second->rect().y < yWithBoth, "hiding a widget moves the ones after it (no requestLayout() needed)");
    first->setVisible(true);
    h.frame();
    check(second->rect().y == yWithBoth, "showing it moves them back");
    Button* b = box->createChild<Button>(String("a"));
    h.frame();
    const float narrow = b->sizeHint().x;
    b->setText("um texto bem comprido para um botao");
    h.frame();
    check(b->sizeHint().x > narrow, "the hint follows the text");
    const int before = probe->laidOut;
    b->setText("um texto bem comprido para um botao"); // the same text: nothing changed
    h.frames(3);
    check(probe->laidOut == before, "setting the same text again asks for no layout");
    box->removeChild(second);
    h.frame();
    check(probe->laidOut == before + 1, "removing a child takes one layout pass");
}

// ---- ImageButton has the states and the tooltip of an IconButton ----

// A window with one button of type B, drawn in a state: 0 plain, 1 hovered, 2 pressed, 3 disabled, 4 hovered long enough
// for its tooltip.
template<typename B>
Shot buttonInState(int state, Rect* where)
{
    Screen s(240.0f, 160.0f);
    Widget* holder = s.h.root()->createChild<Widget>();
    B* b = holder->createChild<B>();
    b->setRect({ 20.0f, 20.0f, 40.0f, 32.0f });
    b->setTooltip("uma dica");
    b->setTooltipDelay(0.1f);
    s.h.relayout();
    if (where) *where = b->absoluteRect();
    float x = 0, y = 0;
    Harness::centre(b, x, y);
    if (state == 3) b->setEnabled(false);
    if (state == 1 || state == 2 || state == 4) s.h.moveTo(x, y);
    if (state == 2)
    {
        s.h.io().mouseDown[0] = true;
        s.h.frame();
    }
    if (state == 4)
    {
        s.h.io().deltaTime = 0.5f;
        s.h.frames(3);
    }
    s.h.frames(2);
    return s.take();
}

template<typename B>
void buttonStates(const char* kind)
{
    Rect r;
    const Shot plain = buttonInState<B>(0, &r);
    const Shot hover = buttonInState<B>(1, nullptr);
    const Shot pressed = buttonInState<B>(2, nullptr);
    const Shot disabled = buttonInState<B>(3, nullptr);
    const Shot tip = buttonInState<B>(4, nullptr);
    const int x0 = static_cast<int>(r.x), y0 = static_cast<int>(r.y), x1 = x0 + static_cast<int>(r.w), y1 = y0 + static_cast<int>(r.h);
    char what[128];
    snprintf(what, sizeof what, "a hovered %s is drawn differently", kind);
    check(differing(plain, hover, x0, y0, x1, y1) > 0, what);
    snprintf(what, sizeof what, "a pressed %s is drawn differently from a hovered one", kind);
    check(differing(hover, pressed, x0, y0, x1, y1) > 0, what);
    snprintf(what, sizeof what, "a disabled %s is drawn differently", kind);
    check(differing(plain, disabled, x0, y0, x1, y1) > 0, what);
    snprintf(what, sizeof what, "a %s shows its tooltip after the delay", kind);
    check(differing(hover, tip, 0, y1 + 1, 240, 160) > 0, what);
}

// The alpha of the white (untinted) vertices of the last frame: 255 when an image is drawn as it is, less when faded; -1 when
// there are none.
int whiteImageAlpha(bool enabled)
{
    Harness h(160.0f, 100.0f, false);
    Widget* holder = h.root()->createChild<Widget>();
    ImageButton* b = holder->createChild<ImageButton>();
    b->setRect({ 20.0f, 20.0f, 40.0f, 32.0f });
    b->setTexture(handle(7), 16, 16);
    b->setSrcRect({ 0.0f, 0.0f, 16.0f, 16.0f });
    b->setEnabled(enabled);
    h.relayout();
    h.frames(2);
    int alpha = -1;
    const ct::Vector<DrawVertex>& v = GetDrawList().vertices();
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i].color.r == 255 && v[i].color.g == 255 && v[i].color.b == 255 && v[i].color.a > alpha) alpha = v[i].color.a;
    return alpha;
}

void testImageButtonStates()
{
    buttonStates<IconButton>("IconButton");
    buttonStates<ImageButton>("ImageButton");
    check(whiteImageAlpha(true) == 255, "an enabled ImageButton draws its image as it is");
    const int faded = whiteImageAlpha(false);
    check(faded > 0 && faded < 255, "a disabled ImageButton draws its image faded");
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testCheckBoxDrawsATick);
    RUN(testButtonAtItsHintShowsItsWholeLabel);
    RUN(testPianoRollButtons);
    RUN(testWaveformViewButtons);
    RUN(testFloatWindowSettersLayOut);
    RUN(testCollapsibleHintIgnoresItsRect);
    RUN(testLayoutRunsByItself);
    RUN(testImageButtonStates);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
