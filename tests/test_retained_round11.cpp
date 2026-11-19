// Round 11 (docs/ROUND11.md): the numbers a person types and the numbers shown to one, whatever LC_NUMERIC the program set (ct's
// to_float / to_double read only '.', and printf writes the locale's separator); the clock that keeps counting while the loop sleeps
// (IO::idleSeconds); the key that carries Super; the hook that wakes a sleeping loop. No standard library: ct and C only.
//
// The comma locale is pt_PT.UTF-8 (or de_DE, pt_BR): if none is installed the checks that need one are not run, and the program says so.

#include "RetainedHarness.hpp"

#include <igui/widgets/NumberText.hpp>
#include <igui/widgets/PlotChart.hpp>
#include <igui/widgets/SpinSlider.hpp>
#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/InputWidgets.hpp>
#include <igui/widgets/TreePropertyColorWidgets.hpp>
#include <igui/widgets/CodeEditor.hpp>
#include <string.h>

#include <locale.h>
#include <stdlib.h>

using namespace rh;
namespace nt = ig::retained::numtext;

namespace
{

// A locale whose decimal separator is ',' (printf("%.1f", 1.5) writes "1,5" in it); false if there is none on this machine.
bool commaLocale()
{
    static const char* const names[] = { "pt_PT.UTF-8", "pt_PT.utf8", "de_DE.UTF-8", "de_DE.utf8", "pt_BR.UTF-8", "pt_BR.utf8", "fr_FR.UTF-8" };
    for (const char* name : names)
    {
        if (!setlocale(LC_NUMERIC, name)) continue;
        char probe[16];
        snprintf(probe, sizeof probe, "%.1f", 1.5);
        if (probe[1] == ',') return true;
    }
    setlocale(LC_NUMERIC, "C");
    return false;
}

bool near(double a, double b)
{
    const double d = a - b;
    return (d < 0 ? -d : d) <= 1e-9 * (1.0 + (b < 0 ? -b : b));
}

bool parses(const char* text, double expected)
{
    double v = -12345.0;
    return nt::parseDecimal(text, strlen(text), v) && near(v, expected);
}

bool refuses(const char* text)
{
    double v = -12345.0;
    return !nt::parseDecimal(text, strlen(text), v) && v == -12345.0;
}

void testParseRule()
{
    check(parses("12", 12), "12");
    check(parses("  -1.5  ", -1.5), "blanks around, a sign, a point");
    check(parses("\x01" "8\x02", 8), "control bytes around the number are trimmed like blanks");
    check(parses("1,5", 1.5), "a comma is the decimal point");
    check(parses("-1,5", -1.5), "a signed comma number");
    check(parses("+2,25", 2.25), "a plus sign");
    check(parses(".5", 0.5), ".5");
    check(parses(",5", 0.5), ",5");
    check(parses("5.", 5), "5.");
    check(parses("5,", 5), "5,");
    check(parses("1e3", 1000), "an exponent");
    check(parses("2,5E-2", 0.025), "a comma and an exponent");
    check(parses("0.1", 0.1), "0.1 is exactly the double 0.1");
    {
        double v = 0.0;
        nt::parseDecimal("0,1", 3, v);
        check(v == 0.1, "0,1 is exactly the double 0.1 too");
    }
    check(parses("123456789012345678", 123456789012345678.0), "more digits than an int");
    // not numbers: the field keeps what it had
    check(refuses(""), "empty");
    check(refuses("   "), "blanks");
    check(refuses("-"), "a sign alone");
    check(refuses("."), "a point alone");
    check(refuses(","), "a comma alone");
    check(refuses("1,000.5"), "two separators are not read (a text that can be read two ways)");
    check(refuses("1.000,5"), "two separators, the other way");
    check(refuses("1.2.3"), "two points");
    check(refuses("1,2,3"), "two commas");
    check(refuses("1x"), "trailing text");
    check(refuses("1 2"), "a space inside");
    check(refuses("12 kg"), "a unit");
    check(refuses("inf"), "inf");
    check(refuses("-inf"), "-inf");
    check(refuses("nan"), "nan");
    check(refuses("0x10"), "hexadecimal");
    check(refuses("1e"), "an exponent with no digits");
    check(refuses("1e+"), "an exponent with a sign only");
    check(refuses("e5"), "an exponent alone");
    check(refuses("1e999"), "a double that is not finite");
    check(refuses("--1"), "two signs");
    check(refuses("1e5,5"), "a separator in the exponent");
    char longText[200];
    memset(longText, '1', sizeof longText - 1);
    longText[sizeof longText - 1] = 0;
    check(refuses(longText), "a hundred digits and more");
}

void testClamp()
{
    double v = 0.0;
    bool clamped = false;
    check(nt::parseClamped("5,5", 0.0, 10.0, v, &clamped) && v == 5.5 && !clamped, "inside the range: as typed, not clamped");
    check(nt::parseClamped("500", 0.0, 10.0, v, &clamped) && v == 10.0 && clamped, "above: the maximum, and it says so");
    check(nt::parseClamped("-500", 0.0, 10.0, v, &clamped) && v == 0.0 && clamped, "below: the minimum");
    check(nt::parseClamped("1e300", -5.0, 5.0, v, &clamped) && v == 5.0 && clamped, "1e300 is the maximum");
    check(!nt::parseClamped("abc", 0.0, 10.0, v, &clamped) && !clamped, "text is not clamped to anything");
    long long n = 0;
    check(nt::parseInteger("12,7", 0, 100, n, &clamped) && n == 12 && !clamped, "an integer is cut toward zero");
    check(nt::parseInteger("-12.7", -100, 100, n, &clamped) && n == -12, "toward zero for a negative too");
    check(nt::parseInteger("99999999999", 0, 100, n, &clamped) && n == 100 && clamped, "past an int: the maximum, never a wrap");
    check(nt::parseInteger("-99999999999", -100, 100, n, &clamped) && n == -100 && clamped, "past an int below: the minimum");
    check(nt::parseInteger("99999999999999999999999", -2147483647LL - 1, 2147483647LL, n, &clamped) && n == 2147483647LL && clamped,
            "past a long long: the limit of the field");
    check(nt::parseInteger("1e10", 0, 2147483647LL, n, &clamped) && n == 2147483647LL, "a power of ten past the int");
}

// the fast conversion is the same text as snprintf, in the C locale
void testFixedAgainstSnprintf()
{
    setlocale(LC_NUMERIC, "C");
    Rng rng(12345u);
    int different = 0;
    int total = 0;
    char a[400], b[400];
    auto compare = [&](double v, int d) {
        ++total;
        const int na = nt::fixedInto(a, sizeof a, v, d);
        const int nb = snprintf(b, sizeof b, "%.*f", d, v);
        if (na != nb || strcmp(a, b) != 0)
        {
            if (++different <= 5) printf("  fixed(%.17g, %d): '%s' (%d) but snprintf '%s' (%d)\n", v, d, a, na, b, nb);
        }
    };
    static const double special[] = { 0.0, -0.0, 0.5, 1.5, 2.5, 0.125, 0.375, 1e-5, 0.00049999, 0.0005, 0.05, 0.95, 0.995, 9.995, 9.9949999,
        99.5, 999.9999, 1e14, 123456789012345.0, 0.1, 0.2, 0.3, 0.1 + 0.2, 1.0 / 3.0, 2.0 / 3.0, 1e-7, 123.456, -123.456, -0.001, -0.00001, 5e-324,
        1.7976931348623157e308, 4503599627370496.5, 0.30000000000000004 };
    for (double v : special)
        for (int d = 0; d <= 10; ++d) compare(v, d);
    for (int i = 0; i < 400000; ++i)
    {
        const int kind = rng.below(5);
        double v;
        if (kind == 0) v = (rng.below(2000001) - 1000000) / 1000.0;                 // thousandths: what a tick is
        else if (kind == 1) v = (rng.below(20001) - 10000) * 0.1;                  // k * 0.1: 0.30000000000000004
        else if (kind == 2) v = rng.range(-1000.0f, 1000.0f) * 0.37;               // anything
        else if (kind == 3) v = static_cast<double>(rng.next()) / 4294967296.0 * 1e-3 * (rng.below(9) + 1); // small
        else v = static_cast<double>(rng.next()) * static_cast<double>(rng.next()) / 16.0;                  // big
        compare(v, rng.below(10));
    }
    check(different == 0, "fixedInto writes what snprintf(\"%.*f\") writes, 400000 random values and the edge cases");
    printf("  fixedInto: %d values compared, %d different\n", total, different);

    // %g
    different = 0;
    for (int i = 0; i < 200000; ++i)
    {
        const double v = static_cast<double>(static_cast<int>(rng.next())) * 1.0e-4 * (rng.below(4) == 0 ? 1.0e6 : 1.0);
        const int p = 1 + rng.below(12);
        const int na = nt::generalInto(a, sizeof a, v, p);
        const int nb = snprintf(b, sizeof b, "%.*g", p, v);
        if (na != nb || strcmp(a, b) != 0)
        {
            if (++different <= 5) printf("  general(%.17g, %d): '%s' but snprintf '%s'\n", v, p, a, b);
        }
    }
    check(different == 0, "generalInto writes what snprintf(\"%.*g\") writes");
    // a buffer that is too small: the same return value and a cut, terminated text
    char small[6];
    check(nt::fixedInto(small, sizeof small, 123.456, 2) == 6 && strcmp(small, "123.4") == 0, "fixedInto into a small buffer: the full length, a cut text");
    check(nt::fixedInto(small, sizeof small, 1e20, 2) == snprintf(nullptr, 0, "%.2f", 1e20), "fixedInto past 1e15 is snprintf's");
}

void testLocaleIndependence()
{
    if (!commaLocale())
    {
        printf("  (no locale with a comma for the decimal point on this machine: the comma-locale checks are not run)\n");
        return;
    }
    char probe[32];
    snprintf(probe, sizeof probe, "%.2f", 1.5);
    check(strcmp(probe, "1,50") == 0, "(the test's locale: printf writes a comma)");
    // what is typed
    check(parses("1,5", 1.5), "locale: 1,5");
    check(parses("1.5", 1.5), "locale: 1.5 (strtod read it as 1 here)");
    check(parses("0.1", 0.1), "locale: 0.1");
    check(refuses("1,000.5"), "locale: two separators");
    // what is shown
    char b[64];
    check(nt::fixedInto(b, sizeof b, 1.5, 2) == 4 && strcmp(b, "1.50") == 0, "locale: fixedInto writes 1.50");
    check(nt::fixedInto(b, sizeof b, 1.5e20, 2) > 0 && strchr(b, ',') == nullptr, "locale: the fallback has no comma either");
    check(nt::generalInto(b, sizeof b, 0.25, 6) == 4 && strcmp(b, "0.25") == 0, "locale: generalInto writes 0.25");
    nt::cformat(b, sizeof b, "%s, %.3f, %g", "a,b", 2.5, 0.5);
    check(strcmp(b, "a,b, 2.500, 0.5") == 0, "locale: cformat changes the numbers and leaves a comma in a string alone");
    check(nt::shortest(0.1) == "0.1" && nt::shortest(0.1f) == "0.1", "locale: shortest");
    check(nt::fixed(-2.5, 1) == "-2.5", "locale: fixed as a String");
    // what a cell is
    double v = 0.0;
    check(nt::leadingNumber("10.5 kg", v) && v == 10.5, "locale: a cell that starts with 10.5 sorts as 10.5 (strtod gave 10)");
    check(!nt::leadingNumber("abc", v), "locale: a cell with no number");
    // the labels of the chart's axes
    check(plot::formatTime(0.25, 0.05) == "0.25", "locale: an axis label of the plot");
    check(plot::formatValue(1.5, 0.5) == "1.5", "locale: a value label of the plot");
    // widgets that show a number
    {
        Harness h;
        Widget* root = h.root();
        auto* slider = root->createChild<Slider>();
        slider->setRect({ 10, 50, 200, 22 });
        h.relayout();
        check(slider->valueText(2.5f).find('.') != String::npos, "locale: the Slider's readout has a point");
        check(slider->valueText(0.5f).find(',') == String::npos, "locale: the Slider's readout has no comma");
    }
    setlocale(LC_NUMERIC, "C");
}

// A SpinSlider with a range, its field opened by a click, text typed, Enter.
double typedInto(Harness& h, SpinSlider* s, const char* text)
{
    s->setValue(0.0);
    h.frames(2);
    h.click(100.0f, 34.0f);
    h.type(text);
    h.key(Key::Return);
    h.frames(2);
    return s->value();
}

void testSpinSliderTyping(bool withCommaLocale)
{
    Harness h;
    auto* s = h.root()->createChild<SpinSlider>();
    s->setRect({ 20, 20, 160, 28 });
    s->setMin(-100.0);
    s->setMax(100.0);
    s->setStep(0.01);
    h.relayout();
    h.frames(2);
    const char* where = withCommaLocale ? " (comma locale)" : "";
    char what[160];
    auto expect = [&](const char* text, double expected, const char* why) {
        const double v = typedInto(h, s, text);
        snprintf(what, sizeof what, "SpinSlider%s: '%s' %s (got %g)", where, text, why, v);
        check(near(v, expected), what);
    };
    expect("1.5", 1.5, "is 1.5");
    expect("1,5", 1.5, "is 1.5");
    expect("-2,25", -2.25, "is -2.25");
    expect("0,1", 0.1, "is 0.1");
    expect("1e1", 10.0, "is 10");
    expect("500", 100.0, "is the maximum, not wrapped");
    expect("-500,5", -100.0, "is the minimum");
    expect("1e300", 100.0, "is the maximum");
    expect("99999999999999999999", 100.0, "is the maximum");
    // a text that is not a number leaves the value as it was
    s->setValue(7.0);
    h.frames(2);
    h.click(100.0f, 34.0f);
    h.type("1,000.5");
    h.key(Key::Return);
    h.frames(2);
    snprintf(what, sizeof what, "SpinSlider%s: '1,000.5' leaves the value (got %g)", where, s->value());
    check(near(s->value(), 7.0), what);
    // and what it shows is with a point
    s->setValue(1.5);
    check(s->textValue().find(',') == String::npos, "SpinSlider: the shown value has a point");
}

// A SpinSlider of integers: a number past the int does not wrap.
void testSpinSliderIntegers()
{
    Harness h;
    auto* s = h.root()->createChild<SpinSlider>();
    s->setRect({ 20, 20, 160, 28 });
    s->setMin(-50.0);
    s->setMax(50.0);
    s->setStep(1.0);
    h.relayout();
    h.frames(2);
    check(near(typedInto(h, s, "4294967297"), 50.0), "SpinSlider integers: 2^32 + 1 is the maximum, not 1");
    check(near(typedInto(h, s, "-4294967297"), -50.0), "SpinSlider integers: -(2^32 + 1) is the minimum");
    check(near(typedInto(h, s, "2147483648"), 50.0), "SpinSlider integers: INT_MAX + 1 is the maximum");
    check(near(typedInto(h, s, "12"), 12.0), "SpinSlider integers: 12");
}

struct ClickProbe : Widget
{
    int presses = 0;
    int lastCount = 0;
    void onMousePress(MouseEvent& e) override
    {
        ++presses;
        lastCount = e.clickCount;
        e.consumed = true;
    }
};

// The clock: what the loop slept is for the clock, the tooltip and the time between two clicks, not for animations.
void testIdleSeconds()
{
    Harness h;
    WidgetApp& app = h.app;
    const double c0 = app.clockSeconds();
    h.io().deltaTime = 0.001f;
    h.io().idleSeconds = 2.5f;
    h.frame();
    check(near(app.clockSeconds() - c0, 2.501), "the clock counts the seconds the loop slept");
    check(h.io().idleSeconds == 0.0f, "update() takes the sleep: it is not counted twice");
    h.io().deltaTime = 1.0f / 60.0f;
    h.frame();
    check(near(app.clockSeconds() - c0, 2.501 + 1.0 / 60.0), "and goes on with the frames' own time");
    check(app.deltaTime() < 0.02f, "deltaTime (what animations get) is the frame's, not the sleep");

    // a long sleep between two clicks is not a double click (the clock that tells them apart counted only the 0.1 s a frame is capped at)
    auto* probe = h.root()->createChild<ClickProbe>();
    probe->setRect({ 10, 10, 100, 30 });
    h.relayout();
    h.click(50.0f, 25.0f);
    h.click(50.0f, 25.0f);
    check(probe->presses == 2 && probe->lastCount == 2, "two clicks one after the other are a double click");
    h.click(50.0f, 25.0f);
    h.io().deltaTime = 0.1f; // the longest frame the adapter hands over
    h.io().idleSeconds = 30.0f;
    h.frame();
    h.click(50.0f, 25.0f);
    check(probe->presses == 4 && probe->lastCount == 1, "two clicks with a long sleep between them are two single clicks");
}

// A widget that says what it got
struct KeyProbe : Widget
{
    int keys = 0;
    bool super = false, ctrl = false;
    KeyProbe() { acceptsFocus_ = true; }
    void onKeyPress(KeyEvent& e) override
    {
        ++keys;
        super = e.super;
        ctrl = e.ctrl;
        e.consumed = true;
    }
};

void testSuperKey()
{
    Harness h;
    auto* probe = h.root()->createChild<KeyProbe>();
    probe->setRect({ 10, 10, 100, 30 });
    h.relayout();
    h.app.setFocused(probe);
    h.io().addKeyEvent('a', 'a', false, false, false, true, true);
    h.io().addKeyEvent('a', 'a', false, false, false, false, true);
    h.frame();
    check(probe->keys == 1 && probe->super && !probe->ctrl, "a key pressed with Super held reaches the widget with super set");
    h.io().addKeyEvent('b', 'b', false, true, false, true);
    h.frame();
    check(probe->keys == 2 && !probe->super && probe->ctrl, "and one without it has super false (the argument is optional)");
}

int gWakes = 0;
void wakeCounter()
{
    __atomic_fetch_add(&gWakes, 1, __ATOMIC_RELAXED);
}

void testWakeHook()
{
    Harness h;
    WidgetApp& app = h.app;
    gWakes = 0;
    app.requestFrameFromAnyThread(); // no hook yet: the flag is set, nothing is called
    check(gWakes == 0, "no hook: nothing to call");
    h.frame();
    h.io().wakeLoop = &wakeCounter;
    h.frame(); // update() reads the hook
    app.requestFrameFromAnyThread();
    check(gWakes == 1, "requestFrameFromAnyThread calls IO::wakeLoop, from the thread that called it");
    check(app.needsFrame(), "and the frame is needed");
    app.requestFrame();
    check(gWakes == 1, "requestFrame (the thread that drives the window) does not wake anything");
    h.io().wakeLoop = nullptr;
    h.frame();
    h.frame();
    app.requestFrameFromAnyThread();
    check(gWakes == 1, "a host that has no hook: no call");
}


// A TreeView toggle (an eye, a padlock) is drawn in the theme's iconColor by default, and follows a theme change; setToggleColor tints it.
int verticesOfColor(const Color& c, float xFrom)
{
    int n = 0;
    for (const DrawPass& pass: GetDrawData()->passes)
        if (pass.list)
            for (const DrawVertex& v: pass.list->vertices())
                if (v.x >= xFrom && v.color.r == c.r && v.color.g == c.g && v.color.b == c.b) ++n;
    return n;
}

void testToggleTint()
{
    Harness h;
    Theme::instance() = Theme::byName("dark");
    auto* tree = h.root()->createChild<TreeView>();
    tree->setRect({ 10, 10, 300, 200 });
    TreeNode* node = tree->addRoot("row");
    const int eye = node->addToggle(IconId::Eye, IconId::EyeOff, true, "hide", "show");
    h.relayout();
    h.frames(2);
    const float slot = 250.0f; // the toggles are at the right edge of the row
    const Color dark = Theme::instance().iconColor;
    check(verticesOfColor(dark, slot) > 0, "a toggle with no tint is drawn in the theme's iconColor");
    check(node->toggleColor(eye).a == 0, "and says it has none of its own");
    // a theme change: the toggle follows
    Theme::instance() = Theme::byName("light");
    h.frames(2);
    const Color light = Theme::instance().iconColor;
    check(!(light.r == dark.r && light.g == dark.g && light.b == dark.b), "(the light preset's icon colour is another one)");
    check(verticesOfColor(light, slot) > 0 && verticesOfColor(dark, slot) == 0, "after a theme change the toggle is in the new iconColor");
    // its own tint
    node->setToggleColor(eye, Color(200, 40, 40, 255));
    h.frames(2);
    check(verticesOfColor(Color(200, 40, 40, 255), slot) > 0 && verticesOfColor(light, slot) == 0, "setToggleColor: the toggle is in that colour");
    node->setToggleState(eye, false);
    h.frames(2);
    check(verticesOfColor(Color(200, 40, 40, 255), slot) > 0, "and still when it is off");
    node->clearToggleColor(eye);
    h.frames(2);
    check(verticesOfColor(light, slot) > 0 && verticesOfColor(Color(200, 40, 40, 255), slot) == 0, "clearToggleColor: back to the theme's");
    const int second = node->addToggle(IconId::Lock, IconId::None, true, "", "", Color(30, 160, 80, 255));
    h.frames(2);
    check(node->toggleColor(second).g == 160 && verticesOfColor(Color(30, 160, 80, 255), slot) > 0, "addToggle takes the tint too");
    Theme::instance() = Theme::byName("dark");
}

// scrollToLine by the row the line is at: with folds above the line (hidden lines have no row) the view used to go past the last row: blank
struct EditorProbe : CodeEditor
{
    int rowOf(int line) const
    {
        return logicalToVisualRow(line);
    }
    int rows() const
    {
        return logicalToVisualRow(lineCount());
    }
};

void testScrollToLineWithFolds()
{
    Harness h;
    auto* ed = h.root()->createChild<EditorProbe>();
    ed->setRect({ 0, 0, 500, 240 });
    String text;
    for (int k = 0; k < 100; ++k)
    {
        text += String("void f") + String::number(k) + "() {\n    int a = " + String::number(k) + ";\n    int b = a;\n}\n";
    }
    ed->setHighlighter<ig::syntax::CppHighlighter>(); // (the highlighter says what opens and closes a fold)
    ed->setText(text);
    ed->setShowFolding(true);
    h.relayout();
    h.frames(3);
    check(ed->rows() == ed->lineCount(), "(unfolded: a row for each line)");
    ed->foldAll();
    h.frames(3);
    const int rows = ed->rows();
    check(rows < ed->lineCount() / 2, "(everything folded: far fewer rows than lines)");
    ed->scrollToLine(120); // line 120 is the head of the 31st function; above it, 30 folded ones (30 rows, not 120)
    h.frames(2);
    const int first = ed->firstVisibleLine();
    const int want = ed->rowOf(120);
    char what[160];
    snprintf(what, sizeof what, "scrollToLine(120) with folds above: the first row is the row of that line (%d, want %d, %d rows in all)", first, want, rows);
    check(first == want && first < rows, what);
    ed->scrollToLine(0);
    h.frames(2);
    check(ed->firstVisibleLine() == 0, "scrollToLine(0) goes to the top");
    ed->scrollToLine(100000);
    h.frames(2);
    check(ed->firstVisibleLine() < rows, "a line past the end: the last page, not past the last row");
    ed->unfoldAt(0);
    h.frames(2);
    ed->scrollToLine(60);
    h.frames(2);
    check(ed->firstVisibleLine() == ed->rowOf(60) && ed->rowOf(60) > 14, "with one fold open: still by row");
}

} // namespace

int main()
{
    testParseRule();
    testClamp();
    testFixedAgainstSnprintf();
    testSpinSliderTyping(false);
    testSpinSliderIntegers();
    testSuperKey();
    testWakeHook();
    testToggleTint();
    testScrollToLineWithFolds();
    testIdleSeconds();
    testLocaleIndependence();
    if (commaLocale())
    {
        testSpinSliderTyping(true);
        setlocale(LC_NUMERIC, "C");
    }
    shutdown();
    if (failures())
    {
        printf("round11: %d failure(s)\n", failures());
        return 1;
    }
    printf("round11: all checks passed\n");
    return 0;
}
