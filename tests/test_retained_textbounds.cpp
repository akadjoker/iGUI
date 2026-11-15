// Where widgets put their text: a long text stays inside its own rectangle and never runs over the
// part of the widget next to it (the value column of a property, the arrow of a combo box, the
// shortcut of a menu item). Checked on the pixels the software rasteriser draws: a window is drawn
// with a long text and again with a short one, and the pixels that belong to the part next to the
// text must be the same in both. Driven through the real frame loop (tests/RetainedHarness.hpp). No
// standard library: ct and C only.

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

#include <igui/widgets/AppWidgets.hpp>
#include <igui/widgets/AssetBrowser.hpp>
#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/ComboBox.hpp>
#include <igui/widgets/ConsoleWidget.hpp>
#include <igui/widgets/DockPanel.hpp>
#include <igui/widgets/InputWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/ScrollWidgets.hpp>
#include <igui/widgets/SpinSlider.hpp>
#include <igui/widgets/Timeline.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/TreePropertyColorWidgets.hpp>
#include <igui_zen/SoftRaster.hpp>

using namespace rh;

namespace
{

const float kWindowW = 640.0f;
const float kWindowH = 320.0f;

int ceilInt(float v)
{
    const int i = static_cast<int>(v);
    return static_cast<float>(i) < v ? i + 1 : i;
}

// The elided text a widget should show in a room of `room` pixels, as the font measures it.
String elided(const String& text, float room)
{
    Harness h(kWindowW, kWindowH, false);
    DrawList list;
    PaintContext ctx(list, h.font());
    ctx.font.SetFontSize(Theme::instance().fontSize);
    return ctx.fitText(text, room);
}

String longText()
{
    return "Entrevista com Ação e muitas outras coisas bem mais compridas do que a coluna";
}

// A text with ink all along its width (no gaps between the letters for a check to fall in)
String wideText()
{
    String s;
    for (int i = 0; i < 70; ++i) s += "M";
    return s;
}

// ---- PropertyGrid ----

enum PropRows
{
    kRowSection,
    kRowString,
    kRowFloat,
    kRowInt,
    kRowBool,
    kRowColor,
    kRowCombo,
    kRowVec3,
    kRowButton,
    kRowRange,
    kRowCount
};

const float kPropRowH = 24.0f;

struct PropSpec
{
    String name;          // the name of every row
    String stringValue = "valor";
    String comboOption = "Opcao";
    float width = kWindowW;
    float labelRatio = 0.4f;
};

// A window with a PropertyGrid of one row of each kind, drawn once.
Shot propShot(const PropSpec& spec, Rect* gridRect = nullptr)
{
    Screen screen(spec.width, kWindowH);
    BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    PropertyGrid* grid = box->createChild<PropertyGrid>();
    grid->setStretch(1.0f);
    grid->addSection("Secao");
    grid->addString(spec.name, spec.stringValue);
    grid->addFloat(spec.name, 0.5f, 0.0f, 1.0f);
    grid->addInt(spec.name, 3, 0, 10);
    grid->addBool(spec.name, true);
    grid->addColor(spec.name, Color(200, 40, 40, 255));
    ct::Vector<String> options;
    options.push_back(spec.comboOption);
    grid->addCombo(spec.name, options, 0);
    grid->addVec3(spec.name, 1.0f, 2.0f, 3.0f, 0.0f, 10.0f);
    grid->addButton(spec.name);
    grid->addRange(spec.name, 2.0f, 6.0f, 0.0f, 10.0f);
    screen.h.relayout();
    screen.h.frames(2);
    if (gridRect) *gridRect = grid->absoluteRect();
    return screen.take();
}

// The pixels of a row, from `fromX` to the right edge of the grid, that differ between two shots.
int rowPixels(const Shot& a, const Shot& b, const Rect& grid, int row, int fromX, int toX)
{
    const int y0 = static_cast<int>(grid.y) + static_cast<int>(row * kPropRowH) + 1;
    return differing(a, b, fromX, y0, toX, y0 + static_cast<int>(kPropRowH) - 2);
}

void testPropertyGridNameStaysInItsColumn()
{
    PropSpec shortSpec;
    shortSpec.name = "n";
    PropSpec longSpec = shortSpec;
    longSpec.name = longText();
    Rect grid;
    const Shot shortShot = propShot(shortSpec, &grid);
    const Shot longShot = propShot(longSpec);
    const int valueX = ceilInt(grid.x + grid.w * 0.4f) + 1;
    const int right = static_cast<int>(grid.x + grid.w) - 1;
    static const char* names[kRowCount] = { "section", "string", "float", "int", "bool", "color", "combo", "vec3",
        "button", "range" };
    int total = 0;
    for (int row = kRowString; row < kRowCount; ++row)
    {
        if (row == kRowButton) continue; // the name is the caption of its button: checked below
        const int n = rowPixels(shortShot, longShot, grid, row, valueX, right);
        if (n) printf("    %s row: %d pixels right of the name column differ\n", names[row], n);
        total += n;
        check(n == 0, "a long property name does not reach the value column");
    }
    (void) total;

    // the caption of a button stays in the button: its margins (4 pixels each side) are not drawn on
    const int buttonValueX = ceilInt(grid.x + grid.w * 0.4f);
    check(rowPixels(shortShot, longShot, grid, kRowButton, buttonValueX + 1, buttonValueX + 4) == 0,
            "a long button caption does not pass the left of its button");
    check(rowPixels(shortShot, longShot, grid, kRowButton, right - 3, right) == 0,
            "a long button caption does not pass the right of its button");

    // the name ends in "..." in its column: the same pixels as a grid whose name was cut by hand
    PropSpec cutSpec = shortSpec;
    cutSpec.name = elided(longText(), grid.w * 0.4f - 8.0f - 4.0f);
    const Shot cutShot = propShot(cutSpec);
    int labelDiffs = 0;
    for (int row = kRowString; row < kRowCount; ++row)
    {
        if (row == kRowButton) continue;
        labelDiffs += rowPixels(longShot, cutShot, grid, row, static_cast<int>(grid.x), valueX - 1);
    }
    if (labelDiffs) printf("    %d pixels of the name column differ from the name cut by hand\n", labelDiffs);
    check(labelDiffs == 0, "a long property name ends in ... in its column");
}

void testPropertyGridValueStaysInItsBox()
{
    PropSpec shortSpec;
    shortSpec.name = "n";
    PropSpec longSpec = shortSpec;
    longSpec.stringValue = wideText();
    longSpec.comboOption = wideText();
    Rect grid;
    const Shot shortShot = propShot(shortSpec, &grid);
    const Shot longShot = propShot(longSpec);
    const int right = static_cast<int>(grid.x + grid.w) - 1;
    // the box of a string ends 4 pixels before the grid does: nothing of the text past it
    const int n = rowPixels(shortShot, longShot, grid, kRowString, right - 3, right);
    if (n) printf("    %d pixels past the box of the string differ\n", n);
    check(n == 0, "a long string value does not pass its box");
    // the arrow of a combo box has its room: the text ends before it
    const int arrowFrom = right - 4 - 14;
    const int c = rowPixels(shortShot, longShot, grid, kRowCombo, arrowFrom, right);
    if (c) printf("    %d pixels at the arrow of the combo differ\n", c);
    check(c == 0, "a long combo option does not reach the arrow");
}

// The pixels outside a rectangle that differ between two shots: the four bands around it.
int outside(const Shot& a, const Shot& b, const Rect& r)
{
    const int x0 = static_cast<int>(r.x), y0 = static_cast<int>(r.y);
    const int x1 = ceilInt(r.x + r.w), y1 = ceilInt(r.y + r.h);
    return differing(a, b, 0, 0, a.w, y0) + differing(a, b, 0, y1, a.w, a.h) + differing(a, b, 0, y0, x0, y1) +
           differing(a, b, x1, y0, a.w, y1);
}

// The description panel of a PropertyGrid (the name and the text of the selected row) is inside the grid.
Shot propDescShot(const String& name, const String& description, Rect* gridRect)
{
    Screen screen(kWindowW, kWindowH);
    BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(20.0f, 290.0f, 20.0f, 100.0f); // the grid is 250 by 280, with room around it
    PropertyGrid* grid = box->createChild<PropertyGrid>();
    grid->setStretch(1.0f);
    grid->setDescHeight(60.0f);
    grid->addBool(name, true, nullptr, description);
    screen.h.relayout();
    *gridRect = grid->absoluteRect();
    screen.h.click(gridRect->x + 20.0f, gridRect->y + kPropRowH * 0.5f); // selects the row
    screen.h.frames(2);
    return screen.take();
}

void testPropertyGridDescriptionStaysInTheGrid()
{
    Rect grid;
    const Shot shortShot = propDescShot("n", "d", &grid);
    const Shot longShot = propDescShot(wideText(), wideText(), &grid);
    const int n = outside(shortShot, longShot, grid);
    check(grid.w == 250.0f && grid.x == 100.0f, "the grid of the description test has the size the test counts on");
    if (n) printf("    %d pixels outside the grid differ\n", n);
    check(n == 0, "the name and the description of the selected row stay inside the grid");
}

// A string being edited: the text stays in its box.
Shot propEditShot(const String& typed, Rect* gridRect)
{
    Screen screen(kWindowW, kWindowH);
    BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    PropertyGrid* grid = box->createChild<PropertyGrid>();
    grid->setStretch(1.0f);
    grid->addString("Nome", "");
    screen.h.relayout();
    *gridRect = grid->absoluteRect();
    const float vx = gridRect->x + gridRect->w * 0.4f + 40.0f;
    screen.h.click(vx, gridRect->y + kPropRowH * 0.5f); // starts the edit
    screen.h.type(typed.c_str());
    screen.h.frames(2);
    return screen.take();
}

void testPropertyGridEditedValueStaysInItsBox()
{
    Rect grid;
    const Shot shortShot = propEditShot("ab", &grid);
    const Shot longShot = propEditShot(wideText(), &grid);
    const int right = static_cast<int>(grid.x + grid.w) - 1;
    // the box ends 4 pixels before the grid does
    const int n = rowPixels(shortShot, longShot, grid, 0, right - 3, right);
    if (n) printf("    %d pixels past the box differ\n", n);
    check(n == 0, "the text of a string being edited does not pass its box");
}

// ---- widgets that print one text in a rectangle of their own ----

struct SweepCase
{
    const char* name;
    Widget* (*make)(BoxLayout* box, const String& text);
};

// The widgets are 320 pixels wide in a window of 640: a long text has a place to go, in the window,
// and a widget that does not keep it in its rectangle is seen.
const SweepCase kSweep[] = {
    { "Label", [](BoxLayout* b, const String& t) -> Widget* { return b->createChild<Label>(t); } },
    { "Button", [](BoxLayout* b, const String& t) -> Widget* { return b->createChild<Button>(t); } },
    { "CheckBox", [](BoxLayout* b, const String& t) -> Widget* { return b->createChild<CheckBox>(t); } },
    { "RadioButton", [](BoxLayout* b, const String& t) -> Widget* { return b->createChild<RadioButton>(t); } },
    { "Switch", [](BoxLayout* b, const String& t) -> Widget* { return b->createChild<Switch>(t); } },
    { "TextInput", [](BoxLayout* b, const String& t) -> Widget* { return b->createChild<TextInput>(t); } },
    { "TextInput placeholder",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                TextInput* w = b->createChild<TextInput>("");
                w->setPlaceholder(t);
                return w;
            } },
    { "TextEdit", [](BoxLayout* b, const String& t) -> Widget* { return b->createChild<TextEdit>(t); } },
    { "SearchBar",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                SearchBar* w = b->createChild<SearchBar>();
                w->setText(t);
                return w;
            } },
    { "SearchBar placeholder",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                SearchBar* w = b->createChild<SearchBar>();
                w->setPlaceholder(t);
                return w;
            } },
    { "SplitButton", [](BoxLayout* b, const String& t) -> Widget* { return b->createChild<SplitButton>(t); } },
    { "ComboBox",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                ComboBox* w = b->createChild<ComboBox>();
                w->addItem(t);
                w->setSelectedIndex(0);
                return w;
            } },
    { "Collapsible", [](BoxLayout* b, const String& t) -> Widget* { return b->createChild<Collapsible>(t, false); } },
    { "StatusBar",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                StatusBar* w = b->createChild<StatusBar>();
                w->setText(t);
                return w;
            } },
    { "ListBox",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                ListBox* w = b->createChild<ListBox>();
                w->addItem(t);
                return w;
            } },
    { "SpinBox",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                SpinBox* w = b->createChild<SpinBox>();
                w->setSuffix(t);
                return w;
            } },
    { "Breadcrumbs",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                Breadcrumbs* w = b->createChild<Breadcrumbs>();
                ct::Vector<String> path;
                path.push_back(t);
                w->setPath(path);
                return w;
            } },
    { "TreeView",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                TreeView* w = b->createChild<TreeView>();
                w->addRoot(t);
                return w;
            } },
    { "ProgressBar",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                ProgressBar* w = b->createChild<ProgressBar>();
                w->setText(t);
                return w;
            } },
    { "SpinSlider label",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                SpinSlider* w = b->createChild<SpinSlider>();
                w->setLabel(t);
                return w;
            } },
    { "SpinSlider suffix",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                SpinSlider* w = b->createChild<SpinSlider>();
                w->setSuffix(t);
                return w;
            } },
    { "TabLayout",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                TabLayout* w = b->createChild<TabLayout>();
                w->addTab<Panel>(t);
                return w;
            } },
    { "DockPanel",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                DockPanel* w = b->createChild<DockPanel>();
                w->addPanel<Panel>(t);
                return w;
            } },
    { "Timeline track",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                Timeline* w = b->createChild<Timeline>();
                w->addTrack(t);
                return w;
            } },
    { "AssetBrowser path",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                AssetBrowser* w = b->createChild<AssetBrowser>();
                w->setPath(t);
                return w;
            } },
    { "AssetBrowser grid name",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                AssetBrowser* w = b->createChild<AssetBrowser>();
                AssetItem item;
                item.name = t;
                w->addItem(item);
                return w;
            } },
    { "AssetBrowser list name",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                AssetBrowser* w = b->createChild<AssetBrowser>();
                w->setViewMode(AssetBrowser::ViewMode::List);
                AssetItem item;
                item.name = t;
                w->addItem(item);
                return w;
            } },
    { "ConsoleWidget message",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                ConsoleWidget* w = b->createChild<ConsoleWidget>();
                w->log(LogLevel::Info, t);
                return w;
            } },
    { "Outliner",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                Outliner* w = b->createChild<Outliner>();
                w->addRoot(t);
                return w;
            } },
};

// A window with the widget of a case (320 wide, the rest of the window free), drawn once.
Shot sweepShot(const SweepCase& c, const String& text, Rect* widgetRect)
{
    Screen screen(kWindowW, kWindowH);
    BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(40.0f, 280.0f, 80.0f, 40.0f);
    Widget* w = c.make(box, text);
    w->setLayoutAlign(Align::Fill);
    screen.h.relayout();
    // a widget with no height of its own (a browser, a console) is given the rest of the box
    if (w->absoluteRect().h < 1.0f)
    {
        w->setStretch(1.0f);
        screen.h.relayout();
    }
    screen.h.frames(2);
    *widgetRect = w->absoluteRect();
    return screen.take();
}

void testSweepTextStaysInItsWidget()
{
    for (const SweepCase& c: kSweep)
    {
        Rect shortRect, longRect;
        const Shot shortShot = sweepShot(c, "x", &shortRect);
        const Shot longShot = sweepShot(c, wideText(), &longRect);
        String what = String(c.name) + ": a long text stays inside the widget";
        const bool sameRect = shortRect.x == longRect.x && shortRect.y == longRect.y && shortRect.w == longRect.w &&
                              shortRect.h == longRect.h;
        if (!sameRect) printf("    %s: the size of the widget follows the text (%g x %g, %g x %g)\n", c.name,
                                      shortRect.w, shortRect.h, longRect.w, longRect.h);
        const int n = outside(shortShot, longShot, longRect);
        if (n) printf("    %s: %d pixels outside the widget differ\n", c.name, n);
        check(sameRect && n == 0, what.c_str());
    }
}

// The status text of a StatusBar is inside the bar: a short text drawn, and the pixels outside the bar the same
// as without it (the baseline was where the top of the text should have been, half of it was above the bar).
void testStatusBarTextIsInTheBar()
{
    Rect r1, r2;
    const SweepCase c = { "StatusBar",
        [](BoxLayout* b, const String& t) -> Widget*
        {
            StatusBar* w = b->createChild<StatusBar>();
            w->setText(t);
            return w;
        } };
    const Shot withText = sweepShot(c, "Pronto", &r1);
    const Shot without = sweepShot(c, "", &r2);
    const int n = outside(withText, without, r1);
    if (n) printf("    %d pixels of the status text are outside the bar\n", n);
    check(n == 0, "the status text of a StatusBar is inside the bar");
    const int inside = differing(withText, without, static_cast<int>(r1.x), static_cast<int>(r1.y), ceilInt(r1.x + r1.w),
            ceilInt(r1.y + r1.h));
    check(inside > 20, "the status text of a StatusBar is drawn");
}

// The path in the top bar of an AssetBrowser stops before the buttons that switch the view: in the gaps
// between and after them the pixels are the same with a long path as with a short one.
void testAssetBrowserPathStopsAtTheButtons()
{
    const SweepCase c = { "AssetBrowser path",
        [](BoxLayout* b, const String& t) -> Widget*
        {
            AssetBrowser* w = b->createChild<AssetBrowser>();
            w->setPath(t);
            return w;
        } };
    Rect r;
    const Shot shortShot = sweepShot(c, "x", &r);
    const Shot longShot = sweepShot(c, wideText(), &r);
    const int bx = static_cast<int>(r.x + r.w - 52.0f);
    const int y0 = static_cast<int>(r.y) + 3, y1 = static_cast<int>(r.y) + 23;
    const int between = differing(shortShot, longShot, bx + 22, y0, bx + 26, y1);
    const int after = differing(shortShot, longShot, bx + 48, y0, static_cast<int>(r.x + r.w) - 1, y1);
    if (between || after) printf("    %d pixels between and %d after the buttons differ\n", between, after);
    check(between == 0 && after == 0, "the path of an AssetBrowser stops before its view buttons");
}

} // namespace

// A scenario name on the command line runs only that one.
int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testPropertyGridNameStaysInItsColumn);
    RUN(testPropertyGridValueStaysInItsBox);
    RUN(testPropertyGridDescriptionStaysInTheGrid);
    RUN(testPropertyGridEditedValueStaysInItsBox);
    RUN(testSweepTextStaysInItsWidget);
    RUN(testStatusBarTextIsInTheBar);
    RUN(testAssetBrowserPathStopsAtTheButtons);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
