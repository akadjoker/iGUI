// The basic retained widgets (CheckBox, Button, Label, ComboBox, ImageButton...): what they draw and the size they
// ask for, checked on the pixels of the software rasteriser and on their size hints. Driven through the real frame
// loop (tests/RetainedHarness.hpp). No standard library: ct and C only.

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

#include <igui/widgets/AppWidgets.hpp>
#include <igui/widgets/AudioWidgets.hpp>
#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/ComboBox.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/ScrollWidgets.hpp>
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

// The layout that asks for itself must not move what the application placed by hand: a widget directly on the stage with a
// rectangle of its own (a panel, an overlay), one inside a container nobody lays out, and a container with a rectangle of its
// own. Only a widget with no size at all (just added) takes the window.
void testAutomaticLayoutKeepsHandPlacedWidgets()
{
    Harness h(640.0f, 480.0f, false);
    Panel* placed = h.root()->createChild<Panel>();
    placed->setRect({ 50.0f, 60.0f, 200.0f, 100.0f });
    BoxLayout* overlay = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    overlay->setRect({ 300.0f, 40.0f, 150.0f, 90.0f });
    Label* inOverlay = overlay->createChild<Label>(String("um"));
    Widget* canvas = h.root()->createChild<Widget>();
    canvas->setRect({ 10.0f, 300.0f, 300.0f, 100.0f });
    Label* free = canvas->createChild<Label>(String("livre"));
    free->setRect({ 33.0f, 44.0f, 55.0f, 20.0f });
    h.frames(2); // the first layout of the window, as it always was: the children of the stage take the window
    placed->setRect({ 50.0f, 60.0f, 200.0f, 100.0f });
    overlay->setRect({ 300.0f, 40.0f, 150.0f, 90.0f });
    canvas->setRect({ 10.0f, 300.0f, 300.0f, 100.0f });
    free->setRect({ 33.0f, 44.0f, 55.0f, 20.0f });
    h.frames(2);

    // everything that asks for a layout by itself
    placed->createChild<Label>(String("filho"));
    h.frame();
    inOverlay->setText("um texto mais comprido");
    h.frame();
    inOverlay->setVisible(false);
    h.frame();
    inOverlay->setVisible(true);
    h.frame();
    overlay->setSpacing(9.0f);
    overlay->setPadding(7.0f);
    h.frame();
    Widget* late = h.root()->createChild<Widget>();
    h.frame();
    canvas->createChild<Label>(String("outro"));
    h.frame();
    check(placed->rect().x == 50.0f && placed->rect().y == 60.0f && placed->rect().w == 200.0f && placed->rect().h == 100.0f,
            "a Panel placed by hand on the stage keeps its rectangle");
    check(overlay->rect().x == 300.0f && overlay->rect().y == 40.0f && overlay->rect().w == 150.0f && overlay->rect().h == 90.0f,
            "so does a BoxLayout placed by hand");
    check(inOverlay->rect().x >= 7.0f && inOverlay->rect().x < 150.0f && inOverlay->rect().w <= 150.0f,
            "while its children are laid out again inside it (padding 7)");
    check(canvas->rect().x == 10.0f && canvas->rect().w == 300.0f, "a plain widget placed by hand keeps its rectangle");
    check(free->rect().x == 33.0f && free->rect().y == 44.0f && free->rect().w == 55.0f && free->rect().h == 20.0f,
            "a child of a widget that lays nothing out keeps the rectangle it was given");
    check(late->rect().w == 640.0f && late->rect().h == 480.0f, "a widget just added to the stage takes the window");
}

// ---- Label and ComboBox ask for the width of their text as measured ----

void testLabelAndComboHintsAreMeasured()
{
    Harness h(640.0f, 480.0f); // with dynamic fonts: the text height changes what is measured
    h.frames(2);
    const Theme& t = Theme::instance();
    WidgetApp& app = h.app;
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    String narrow, wide, longText;
    for (int i = 0; i < 24; ++i) narrow += "i";
    for (int i = 0; i < 12; ++i) wide += "W";
    for (int i = 0; i < 400; ++i) longText += "Entrevista com Acao ";
    Label* a = box->createChild<Label>(narrow);
    Label* b = box->createChild<Label>(wide);
    Label* elided = box->createChild<Label>(longText);
    elided->setElide(true);
    Label* shortElided = box->createChild<Label>(String("ok"));
    shortElided->setElide(true);
    h.relayout();
    const float narrowReal = app.textWidth(narrow.c_str()), wideReal = app.textWidth(wide.c_str());
    check(narrowReal < narrow.size() * t.fontSize * 0.6f - 20.0f, "(the narrow text really is narrower than its estimate)");
    check(fabsf(a->sizeHint().x - (narrowReal + t.padding * 2)) < 0.01f, "a Label asks for the measured width of a narrow text");
    check(fabsf(b->sizeHint().x - (wideReal + t.padding * 2)) < 0.01f, "and of a wide one");
    const float dots = app.textWidth("...") + t.padding * 2;
    check(fabsf(elided->sizeHint().x - dots) < 0.01f, "an eliding Label with a long text asks for the width of its dots");
    check(shortElided->sizeHint().x <= dots + 0.01f && shortElided->sizeHint().x > t.padding * 2, "an eliding Label never asks for more than its text");
    // a scroll area's content is not widened by an eliding label (the hint of a column of them is the hint of the widest
    // thing that does not elide)
    check(box->sizeHint().x < 4.0f * t.fontSize * 12.0f, "a column with an eliding long label stays narrow");

    ComboBox* combo = box->createChild<ComboBox>();
    for (int i = 0; i < 3; ++i) combo->addItem(narrow);
    const float narrowCombo = combo->sizeHint().x;
    check(narrowCombo > 100.0f && fabsf(narrowCombo - (narrowReal + t.padding * 3 + 16.0f)) < 0.01f,
            "a ComboBox of narrow items asks for the measured width of the widest");
    combo->addItem(wide + wide + wide);
    const float wideReal3 = app.textWidth((wide + wide + wide).c_str());
    check(fabsf(combo->sizeHint().x - (wideReal3 + t.padding * 3 + 16.0f)) < 0.01f, "an item added afterwards is in the hint");
    combo->clear();
    check(combo->sizeHint().x == 100.0f, "an emptied ComboBox asks for 100");
    Theme saved = Theme::instance();
    combo->addItem(wide);
    const float before = combo->sizeHint().x;
    Theme::instance().fontSize = t.fontSize * 1.5f;
    check(combo->sizeHint().x > before, "the hint follows the text height");
    Theme::instance() = saved;
    // a ComboBox with a font of its own is measured with it, and the remembered width follows a change of font
    const float withTheme = combo->sizeHint().x;
    const Font* big = h.font()->atHeight(t.fontSize * 2.0f);
    combo->setFont(big);
    const float bigWidth = app.textWidth(wide.c_str(), big) + t.padding * 3 + 16.0f;
    check(bigWidth > withTheme && fabsf(combo->sizeHint().x - bigWidth) < 0.01f, "a ComboBox measures its items with its own font");
    combo->setFont(nullptr);
    check(fabsf(combo->sizeHint().x - withTheme) < 0.01f, "and with the theme's again when the font is taken off");
}

// ---- ScrollView: a horizontal bar that does not stick, and content as wide as it asks for ----

// A widget whose hint is the width it has (never less than 50): the kind that held a ScrollView's content at its widest.
class AsWideAsItIs : public Widget
{
public:
    IG_WIDGET_CLASS(AsWideAsItIs, Widget);
    Vec2f sizeHint() const override
    {
        return { rect_.w > 50.0f ? rect_.w : 50.0f, 400.0f };
    }
};

// A widget that asks for a fixed width and height.
class Wants : public Widget
{
public:
    IG_WIDGET_CLASS(Wants, Widget);
    Wants(float w, float h) : w_(w), h_(h) {}
    Vec2f sizeHint() const override
    {
        return { w_, h_ };
    }
    float w_, h_;
};

ScrollBar* barOf(ScrollView* view, ScrollBarOrientation o)
{
    for (Widget* c: view->children())
        if (ScrollBar* bar = widget_cast<ScrollBar>(c))
            if (bar->orientation() == o) return bar;
    return nullptr;
}

void testScrollViewHorizontalBar()
{
    Harness h(700.0f, 400.0f, false);
    Widget* holder = h.root()->createChild<Widget>();
    ScrollView* view = holder->createChild<ScrollView>();
    view->setRect({ 10.0f, 10.0f, 200.0f, 100.0f });
    AsWideAsItIs* content = view->setContent<AsWideAsItIs>();
    h.relayout();
    check(content->rect().w <= 200.0f, "content that asks for the width it has fits a view of 200");
    view->setRect({ 10.0f, 10.0f, 600.0f, 100.0f });
    h.relayout();
    view->setRect({ 10.0f, 10.0f, 200.0f, 100.0f });
    h.relayout();
    if (content->rect().w > 200.0f) printf("    the content is %.0f wide in a view of 200\n", static_cast<double>(content->rect().w));
    check(content->rect().w <= 200.0f, "and after the view was 600 wide it fits 200 again (the horizontal bar does not stay)");
    ScrollBar* hbar = barOf(view, ScrollBarOrientation::Horizontal);
    check(hbar && !hbar->isVisible(), "the horizontal bar is not shown for it");

    // content wider than the view: it is as wide as it asks for, and the bar scrolls over it
    ScrollView* wide = holder->createChild<ScrollView>();
    wide->setRect({ 10.0f, 200.0f, 200.0f, 100.0f });
    Wants* wants = wide->setContent<Wants>(500.0f, 60.0f);
    h.relayout();
    check(wants->rect().w == 500.0f, "content that asks for 500 in a view of 200 is laid out 500 wide");
    ScrollBar* wideBar = barOf(wide, ScrollBarOrientation::Horizontal);
    check(wideBar && wideBar->isVisible(), "and the horizontal bar is shown");
    wide->setRect({ 10.0f, 200.0f, 800.0f, 100.0f });
    h.relayout();
    check(wants->rect().w == 800.0f && wideBar && !wideBar->isVisible(), "in a view of 800 it takes the view and the bar goes");
}

// ---- polylines: the first and the last segment are as thick as the others ----

void testOpenPolylineHasTheSameWidthAtItsEnds()
{
    DrawList list;
    ct::Vector<Vec2> points;
    points.push_back({ 10.0f, 10.0f });
    points.push_back({ 60.0f, 10.0f });
    points.push_back({ 110.0f, 10.0f });
    list.setAntiAliasing(false);
    list.addPolyline(points, Color(255, 255, 255, 255), 4.0f, false);
    const ct::Vector<DrawVertex>& v = list.vertices();
    float far = 0.0f;
    for (size_t i = 0; i < v.size(); ++i)
    {
        const float d = v[i].y > 10.0f ? v[i].y - 10.0f : 10.0f - v[i].y;
        if (d > far) far = d;
    }
    if (far > 2.01f) printf("    a vertex is %.2f from the line of a 4 pixel stroke\n", static_cast<double>(far));
    check(far <= 2.01f, "an open polyline is as thick at its ends as along it");
    // a bent one: the ends are not wider than the half width either (the elbow may be, by its miter)
    points.clear();
    points.push_back({ 10.0f, 10.0f });
    points.push_back({ 60.0f, 40.0f });
    points.push_back({ 110.0f, 10.0f });
    DrawList list2;
    list2.setAntiAliasing(false);
    list2.addPolyline(points, Color(255, 255, 255, 255), 4.0f, false);
    const ct::Vector<DrawVertex>& w = list2.vertices();
    float endOffset = 0.0f;
    for (size_t i = 0; i < 2; ++i) // the two vertices of the first point
    {
        const float dx = w[i].x - 10.0f, dy = w[i].y - 10.0f;
        const float d = sqrtf(dx * dx + dy * dy);
        if (d > endOffset) endOffset = d;
    }
    check(endOffset <= 2.01f, "the first point of a bent polyline is half the width from its centre");
}

// ---- a change of text asks for a layout pass only when the size hint changes ----

// What the apps do every frame during playback: a clock label whose digits change, a name that is cut with "...", a meter
// value. The digits all have the same width, so none of it moves anything and the tree must not be laid out again for it.
void testTextChangeAsksForLayoutOnlyWhenTheHintChanges()
{
    Harness h(900.0f, 400.0f);
    LayoutProbe* probe = h.root()->createChild<LayoutProbe>();
    BoxLayout* row = h.root()->createChild<BoxLayout>(LayoutDir::Horizontal);
    Label* clock = row->createChild<Label>(String("00:00:00:00"));
    Label* name = row->createChild<Label>(String("Parametro numero 1"));
    name->setElide(true);
    name->setStretch(1.0f);
    Label* last = row->createChild<Label>(String("end"));
    Button* play = row->createChild<Button>(String("Play"));
    CheckBox* check1 = row->createChild<CheckBox>(String("Loop"));
    h.frames(6);
    const float digitsWidth = clock->sizeHint().x;
    const int settled = probe->laidOut;
    check(settled > 0, "(the probe is laid out)");

    // the same width: no layout pass at all, however many times and however many frames
    for (int i = 1; i <= 20; ++i)
    {
        String t = String("00:00:") + String::number(10 + i % 40) + ":" + String::number(10 + (i * 7) % 40);
        clock->setText(t);
        name->setText(String("Parametro numero ") + String::number(i)); // an eliding label asks for the dots whatever it says
        play->setText(i % 2 ? "Stop" : "Play");                          // as many letters, the same hint
        check1->setText(i % 2 ? "Loop" : "Rows");
        h.frame();
    }
    check(clock->sizeHint().x == digitsWidth, "(the digits of the font all have one width)");
    if (probe->laidOut != settled) printf("    %d layout passes for texts of the same width\n", probe->laidOut - settled);
    check(probe->laidOut == settled, "a text that keeps the size hint asks for no layout pass");

    // the hint changes: the layout runs by itself, once, and the widget takes the new width
    const float before = clock->rect().w;
    clock->setText("00:00:00:00 and some more");
    h.frame();
    check(probe->laidOut == settled + 1, "a wider text takes one layout pass");
    check(clock->rect().w > before, "and the label is laid out at its new width");
    clock->setText("00:00:00:00");
    h.frame();
    check(probe->laidOut == settled + 2 && fabsf(clock->rect().w - before) < 0.01f, "the same for a narrower one");
    play->setText("Play something long");
    h.frame();
    check(probe->laidOut == settled + 3, "a Button whose text is longer takes one");
    check1->setText("Loop the whole project");
    h.frame();
    check(probe->laidOut == settled + 4, "and so does a CheckBox");

    // from nothing to something and back, in a label that does not cut and in one that does
    const int base = probe->laidOut;
    last->setText("");
    h.frame();
    check(probe->laidOut == base + 1, "a text that becomes empty takes one layout pass");
    last->setText("x");
    h.frame();
    check(probe->laidOut == base + 2, "and one that stops being empty");
    name->setText("");
    h.frame();
    check(probe->laidOut == base + 3, "an eliding label that becomes empty takes one");
    name->setText("Parametro numero 2");
    h.frame();
    check(probe->laidOut == base + 4, "and one that stops being empty");
    name->setText("Another parametro");
    h.frame();
    check(probe->laidOut == base + 4, "but not one that goes from a text to another");

    // cutting on and off: it moves the hint of a long text, and nothing of an empty one
    const int mid = probe->laidOut;
    last->setElide(true);
    h.frame();
    check(probe->laidOut == mid, "a one letter label that cuts asks for the same width (a letter is narrower than the dots)");
    last->setText("a long text that a label that cuts asks no room for");
    h.frame();
    const int widened = probe->laidOut;
    check(widened == mid + 1, "(a long text in a label that cuts takes its dots)");
    last->setElide(false);
    h.frame();
    check(probe->laidOut == widened + 1, "a label that stops cutting asks for the room of its whole text");
    last->setElide(true);
    h.frame();
    check(probe->laidOut == widened + 2, "and one that starts takes it back");
    last->setElide(true);
    h.frame();
    check(probe->laidOut == widened + 2, "cutting on again does nothing");
    last->setText("");
    h.frame();
    const int emptied = probe->laidOut;
    last->setElide(false);
    last->setElide(true);
    h.frame();
    check(probe->laidOut == emptied, "cutting on and off with no text asks for nothing");
}

// ---- the layout setters called again with the value they have ----

void testLayoutSettersWithTheirValueAskForNothing()
{
    Harness h(900.0f, 400.0f);
    LayoutProbe* probe = h.root()->createChild<LayoutProbe>();
    BoxLayout* row = h.root()->createChild<BoxLayout>(LayoutDir::Horizontal);
    Label* clock = row->createChild<Label>(String("00:00:00:00"));
    row->createChild<Label>(String("middle"));
    row->createChild<Label>(String("end"));
    h.frames(6);
    const int settled = probe->laidOut;
    check(settled > 0, "(the probe is laid out)");

    // the other setters that were asked for every frame with the value they already had
    const int steady = probe->laidOut;
    for (int i = 0; i < 5; ++i)
    {
        row->setStretch(row->stretch());
        row->setMargins(row->margins());
        row->setLayoutAlign(row->layoutAlign());
        row->setSpacing(row->spacing());
        row->setPadding(row->padding());
        row->setMainAlign(row->mainAlign());
        row->moveChild(clock, 0);
        h.frame();
    }
    if (probe->laidOut != steady) printf("    %d layout passes for setters with the value they had\n", probe->laidOut - steady);
    check(probe->laidOut == steady, "setters called again with the value they had ask for no layout pass");
    row->setSpacing(row->spacing() + 3.0f);
    h.frame();
    check(probe->laidOut == steady + 1, "and with another value, one");
    row->moveChild(clock, 2);
    h.frame();
    check(probe->laidOut == steady + 2, "a child that moves takes one");
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
    RUN(testAutomaticLayoutKeepsHandPlacedWidgets);
    RUN(testImageButtonStates);
    RUN(testLabelAndComboHintsAreMeasured);
    RUN(testScrollViewHorizontalBar);
    RUN(testOpenPolylineHasTheSameWidthAtItsEnds);
    RUN(testTextChangeAsksForLayoutOnlyWhenTheHintChanges);
    RUN(testLayoutSettersWithTheirValueAskForNothing);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
