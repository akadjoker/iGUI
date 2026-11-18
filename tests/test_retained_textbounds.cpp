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

// Icons are drawn when the application paints with no icon atlas (the zen adapter passes none): the vector
// icons need no atlas. Widgets that drew their icons only with one never showed them there.
void testIconsAreDrawnWithoutAnAtlas()
{
    {
        const SweepCase plain = { "Outliner",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                Outliner* w = b->createChild<Outliner>();
                w->addRoot(t);
                return w;
            } };
        const SweepCase iconed = { "Outliner",
            [](BoxLayout* b, const String& t) -> Widget*
            {
                Outliner* w = b->createChild<Outliner>();
                w->addRoot(t)->setIconId(IconId::File);
                return w;
            } };
        Rect r;
        const Shot a = sweepShot(plain, "No", &r);
        const Shot b = sweepShot(iconed, "No", &r);
        const int n = differing(a, b, static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.x) + 80, static_cast<int>(r.y) + 30);
        if (n < 10) printf("    Outliner: only %d pixels differ where the icon goes\n", n);
        check(n >= 10, "the icon of an Outliner node is drawn with no icon atlas");
    }
}

// The pixels of a rectangle that are not the colour `fill` (0xAARRGGBB).
int notColour(const Shot& s, int x0, int y0, int x1, int y1, uint32_t fill)
{
    int n = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (s.px[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)] != fill) ++n;
    return n;
}

// The AssetBrowser draws the icons of its view buttons and of its items with no icon atlas.
void testAssetBrowserIconsAreDrawnWithoutAnAtlas()
{
    const SweepCase c = { "AssetBrowser",
        [](BoxLayout* b, const String&) -> Widget*
        {
            AssetBrowser* w = b->createChild<AssetBrowser>();
            AssetItem item;
            item.name = "clip";
            item.type = AssetType::Folder;
            w->addItem(item);
            return w;
        } };
    Rect r;
    const Shot shot = sweepShot(c, "", &r);
    const int bx = static_cast<int>(r.x + r.w - 52.0f), by = static_cast<int>(r.y) + 3;
    // the first button (the grid view is on): 60,120,200 under the icon; the second: 40,44,52
    const uint32_t on = 0xff000000u | (60u << 16) | (120u << 8) | 200u;
    const uint32_t off = 0xff000000u | (40u << 16) | (44u << 8) | 52u;
    const int a = notColour(shot, bx + 3, by + 3, bx + 17, by + 17, on);
    const int b = notColour(shot, bx + 29, by + 3, bx + 43, by + 17, off);
    if (a < 8 || b < 8) printf("    pixels of the icons of the view buttons: %d, %d\n", a, b);
    check(a >= 8, "the grid icon of the AssetBrowser is drawn with no icon atlas");
    check(b >= 8, "the list icon of the AssetBrowser is drawn with no icon atlas");
    // the list view: without an atlas the row drew a solid square of the type's colour where the icon
    // goes (12 pixels, from 5 pixels under the top of the row); the icon is strokes, not a solid square
    const SweepCase list = { "AssetBrowser list",
        [](BoxLayout* b, const String&) -> Widget*
        {
            AssetBrowser* w = b->createChild<AssetBrowser>();
            w->setViewMode(AssetBrowser::ViewMode::List);
            AssetItem item;
            item.name = "clip";
            item.type = AssetType::Folder;
            w->addItem(item);
            return w;
        } };
    const Shot rows = sweepShot(list, "", &r);
    const int sx = static_cast<int>(r.x) + 6, sy = static_cast<int>(r.y) + 27 + 5;
    const uint32_t centre = rows.px[static_cast<size_t>(sy + 6) * static_cast<size_t>(rows.w) + static_cast<size_t>(sx + 6)];
    const int solid = 144 - notColour(rows, sx, sy, sx + 12, sy + 12, centre);
    if (solid >= 100) printf("    %d of the 144 pixels where the icon goes are one colour\n", solid);
    check(solid < 100, "the icon of a row of the AssetBrowser is drawn, not a solid square, with no icon atlas");
}

// A TextEdit draws a tab as the spaces up to the next tab stop (4), and a text with tabs and accents the same
// pixels as the text with the spaces written out; focused, with the cursor in the middle of it, too.
Shot tabShot(const String& text, bool focus, bool select, int cursorCol)
{
    Screen screen(kWindowW, kWindowH);
    BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(10.0f);
    TextEdit* editor = box->createChild<TextEdit>(text);
    editor->setStretch(1.0f);
    screen.h.relayout();
    if (focus)
    {
        screen.h.clickWidget(editor);
        editor->setCursorPos(0, cursorCol);
        if (select) editor->selectAll();
        // the cursor blinks: a frame with it drawn
        screen.h.frames(2);
    }
    screen.h.frames(2);
    return screen.take();
}

void testTextEditTabsAreSpaces()
{
    const String tabs = "\ta\tb\t\xC3\xA7\xC3\xA3\nlinha dois\tcom tab\n";
    const String spaces = "    a   b   \xC3\xA7\xC3\xA3\nlinha dois  com tab\n";
    for (int mode = 0; mode < 3; ++mode)
    {
        const Shot a = tabShot(tabs, mode > 0, mode == 2, 2); // after the tab and the a
        const Shot b = tabShot(spaces, mode > 0, mode == 2, 5);
        const int n = differing(a, b, 0, 0, a.w, a.h);
        if (n) printf("    mode %d: %d pixels differ\n", mode, n);
        check(n == 0, "a text with tabs is drawn as the text with the spaces written out (plain, focused, selected)");
    }
}

// The text of a SplitButton is cut where a Button's is: the main button of a SplitButton is a button of its own width
// (the arrow takes the rest), and a text in it that is wider than the width less two paddings on each side is cut with
// "..." in the main button as in a Button, so a text is whole or cut in both for every width. Counted in the vertices of
// the frame: the same text in a Button of the width of the main button and in the SplitButton differ by what the
// SplitButton has more (its arrow and divider), the same for every length of text, as long as both cut at the same width.
size_t verticesOfAWidget(float width, const String& text, bool split)
{
    Harness h(width, 60.0f, false);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    if (split) box->createChild<SplitButton>(text);
    else
        box->createChild<Button>(text);
    h.relayout();
    h.frames(2);
    return GetDrawList().vertices().size();
}

void testSplitButtonCutsWhereAButtonDoes()
{
    const float mainWidth = 150.0f;
    const float arrowWidth = 22.0f; // SplitButton::dropArrowW
    String text;
    const size_t base = verticesOfAWidget(mainWidth + arrowWidth, "M", true) - verticesOfAWidget(mainWidth, "M", false);
    int mismatches = 0;
    int cuts = 0;
    for (int n = 1; n <= 24; ++n)
    {
        text += "M";
        const size_t split = verticesOfAWidget(mainWidth + arrowWidth, text, true);
        const size_t button = verticesOfAWidget(mainWidth, text, false);
        if (split - button != base)
        {
            ++mismatches;
            printf("    %d letters: %u vertices in the SplitButton, %u in the Button (%u more at one letter)\n", n,
                    static_cast<unsigned>(split), static_cast<unsigned>(button), static_cast<unsigned>(base));
        }
        if (split < verticesOfAWidget(mainWidth + arrowWidth, "M", true) + 4 * static_cast<size_t>(n - 1)) ++cuts;
    }
    check(cuts > 0, "the sweep reaches texts that are cut");
    check(mismatches == 0, "a text is cut at the same length in a SplitButton and in a Button as wide as its main button");
}

// A tab past the end of the bar has no room (the tabs are cut short at the end of the bar and the ones after the last
// have width 0): nothing of it is drawn (it was its background, and with close buttons a close button at its left
// edge, on the last tab), and no click reaches it. What main did: the tab went on past the end of the bar, drawn
// outside the widget over what is next to it, and no click reached it either (a click outside the bar is not in the bar).
size_t verticesOfTabs(int tabs, bool closable)
{
    Harness h(300.0f, 200.0f, false);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    TabLayout* tabsWidget = box->createChild<TabLayout>();
    tabsWidget->setStretch(1.0f);
    tabsWidget->setTabsClosable(closable);
    for (int i = 0; i < tabs; ++i) tabsWidget->addTab<Panel>(String("T") + String::number(i % 10));
    h.relayout();
    h.frames(2);
    return GetDrawList().vertices().size();
}

void testTabsPastTheEndOfTheBarAreNotDrawnOrHit()
{
    for (int closable = 0; closable < 2; ++closable)
    {
        // 300 pixels hold five tabs of 60: with 12 or with 40, what is drawn is the same
        const size_t few = verticesOfTabs(12, closable != 0), many = verticesOfTabs(40, closable != 0);
        if (few != many) printf("    %u vertices with 12 tabs, %u with 40\n", static_cast<unsigned>(few), static_cast<unsigned>(many));
        check(few == many, closable ? "tabs with no room draw nothing, close buttons included" : "tabs with no room draw nothing");
    }
    // and a click anywhere along the bar lands on a tab that has room
    Harness h(300.0f, 200.0f, false);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    TabLayout* tabs = box->createChild<TabLayout>();
    tabs->setStretch(1.0f);
    for (int i = 0; i < 40; ++i) tabs->addTab<Panel>(String("T") + String::number(i % 10));
    h.relayout();
    h.frames(2);
    const Rect r = tabs->absoluteRect();
    int most = 0;
    for (float x = r.x + 1.0f; x < r.x + r.w; x += 3.0f)
    {
        h.click(x, r.y + 10.0f);
        if (tabs->currentIndex() > most) most = tabs->currentIndex();
    }
    if (most > 6) printf("    a click along the bar selected tab %d\n", most);
    check(most >= 3 && most <= 6, "a click along the bar selects only the tabs that have room (the first five or six)");
}

// An icon with no drawing of its own, no atlas to take it from and a glyph the font does not have draws nothing: the
// font puts a '?' in place of a glyph it lacks, and a question mark says nothing about what the icon was. The font that
// ships with the library has all of these glyphs, so the test takes the ones of the system (fonts of a few common
// families, wherever they are: none of them, or none that lacks a glyph, and it says so and checks nothing).
bool readFile(const char* path, ct::Vector<unsigned char>& out)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    unsigned char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0)
        for (size_t i = 0; i < n; ++i) out.push_back(chunk[i]);
    fclose(f);
    return !out.empty();
}

void testIconWithoutAGlyphIsNotAQuestionMark()
{
    const char* candidates[] = { "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf", "/usr/share/fonts/truetype/droid/DroidSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf", "/usr/share/fonts/TTF/LiberationSans-Regular.ttf",
        "C:\\Windows\\Fonts\\arial.ttf", "/Library/Fonts/Arial.ttf" };
    int fonts = 0, missingSomewhere = 0, marks = 0;
    for (const char* path: candidates)
    {
        ct::Vector<unsigned char> data;
        if (!readFile(path, data)) continue;
        FontAtlas atlas;
        if (!atlas.buildFromTTFMemory(data.data(), static_cast<int>(data.size()), 16.0f)) continue;
        atlas.setTexture(handle(9));
        ++fonts;
        // what this font draws for '?': the texture coordinates of its glyph
        DrawList reference;
        PaintContext refCtx(reference, &atlas.defaultFont());
        refCtx.font.SetFontSize(16.0f);
        refCtx.drawText(0.0f, 20.0f, "?");
        ct::Vector<Vec2> question;
        for (size_t i = 0; i < reference.vertices().size(); ++i) question.push_back({ reference.vertices()[i].u, reference.vertices()[i].v });
        check(!question.empty(), "the font draws a question mark");
        int nothing = 0, here = 0;
        for (int i = 1; i < static_cast<int>(IconId::COUNT); ++i)
        {
            const IconId id = static_cast<IconId>(i);
            if (DrawList::hasVectorIcon(id)) continue;
            DrawList list;
            PaintContext ctx(list, &atlas.defaultFont());
            ctx.drawIcon(id, 10.0f, 10.0f, 24.0f);
            if (list.vertices().empty())
            {
                ++nothing;
                continue;
            }
            bool mark = false;
            for (size_t v = 0; v < list.vertices().size() && !mark; ++v)
                for (size_t q = 0; q < question.size(); ++q)
                    if (list.vertices()[v].u == question[q].x && list.vertices()[v].v == question[q].y) mark = true;
            if (mark) ++here;
        }
        if (here) printf("    %s: %d icons drew a question mark (%d drew nothing)\n", path, here, nothing);
        marks += here;
        if (nothing > 0) ++missingSomewhere;
    }
    if (!missingSomewhere && marks == 0)
    {
        printf("    %s\n", fonts ? "no system font here lacks an icon glyph: nothing to check" : "no system font found: nothing to check");
        return;
    }
    check(marks == 0, "an icon whose glyph the font lacks is not drawn as a question mark");
}

// The RAM copy of the baked page is freed once the texture is made (ZenApp does it with its default atlas): the same
// window, drawn with the pixels released, comes out pixel for pixel as it does without. Text of the baked ranges (Latin-1,
// an arrow, a star), a button, a text box being edited and a bigger font; every glyph is first drawn after the release.
Shot releasedShot(bool release, bool* hadPixels, int* inkPixels)
{
    Screen screen(kWindowW, kWindowH, false, release);
    *hadPixels = screen.h.atlas().pixels() != nullptr;
    BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(20.0f);
    box->createChild<Label>(String("Hamburgefonstiv 0123 \xc3\xa9\xc3\xa7 \xe2\x86\x92 \xe2\x98\x85 The quick brown fox"));
    box->createChild<Button>(String("A button with text"));
    TextEdit* edit = box->createChild<TextEdit>();
    edit->setText("edited text");
    screen.h.relayout();
    screen.h.frames(2);
    const Shot shot = screen.take();
    int ink = 0;
    for (size_t i = 0; i < shot.px.size(); ++i)
        if (shot.px[i] != shot.px[0]) ++ink;
    *inkPixels = ink;
    return shot;
}

void testTextIsDrawnWithTheAtlasPixelsReleased()
{
#ifdef IGUI_FONTATLAS_RELEASE_PIXELS
    bool keptPixels = false, releasedPixels = true;
    int keptInk = 0, releasedInk = 0;
    const Shot kept = releasedShot(false, &keptPixels, &keptInk);
    const Shot released = releasedShot(true, &releasedPixels, &releasedInk);
    check(keptPixels, "without the release the atlas has its pixels (the comparison is against something)");
    check(!releasedPixels, "after the release the atlas keeps no RAM copy of its page");
    check(keptInk > 1500, "the window has text in it (ink was counted)");
    check(releasedInk == keptInk && differing(kept, released, 0, 0, kept.w, kept.h) == 0,
            "text, a button and a text box are drawn pixel for pixel the same with the atlas pixels released");
#endif
}

// The default atlas of ZenApp has a dynamic page and every text of a widget is drawn from it (Font::atHeight), so a Label, a
// Button, a ComboBox and a TextInput measure their text for their size hints during layout, before the first paint, and that
// is when the glyphs go into the page. With the baked pixels already released that must change nothing: the glyphs below are
// in no baked range (Cyrillic Zhe and Ef, Greek Omega, Latin Extended-A) and are first measured and drawn after the release,
// and the window comes out pixel for pixel as when the pixels are kept.
struct DynamicReleaseRun
{
    Shot shot;
    bool released = false;
    bool dynamic = false;
    bool neverBaked = false;      // the codepoints are not in the baked font before the first measure
    bool madeByTheHint = false;   // the first sizeHint put them in the dynamic font, before any paint
    bool madeAfterTheHint = false; // ... and the frames did not leave the page dirty
    int ink = 0;
    int labelInk = 0; // the Label has no background: what is not the window's colour in its rectangle is glyph pixels
    float hintW[4] = { 0, 0, 0, 0 };
    float hintH[4] = { 0, 0, 0, 0 };
};

DynamicReleaseRun dynamicReleaseRun(bool release)
{
    DynamicReleaseRun run;
    Screen screen(kWindowW, kWindowH, false, release);
    run.dynamic = screen.useDynamicPage();
    run.released = screen.h.atlas().pixels() == nullptr;
    run.neverBaked = !screen.h.atlas().defaultFont().hasBakedGlyph(0x0416) && !screen.h.atlas().defaultFont().hasBakedGlyph(0x03a9) &&
                     !screen.h.atlas().defaultFont().hasBakedGlyph(0x0141);
    BoxLayout* box = screen.h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(20.0f);
    Label* label = box->createChild<Label>(String("\xd0\x96\xd0\xb0\xd0\xb1\xd0\xb0 \xd0\xa4\xd0\xb8\xd0\xbb\xd1\x8c\xd0\xbc \xce\xa9\xce\xbc\xce\xad\xce\xb3\xce\xb1 \xc5\x81\xc3\xb3" "d\xc5\xba"));
    Button* button = box->createChild<Button>(String("\xd0\x96 \xd0\xa4 \xce\xa9 \xc5\x81 button"));
    ComboBox* combo = box->createChild<ComboBox>();
    combo->addItem(String("\xd0\xa4\xd0\xb0\xd0\xb9\xd0\xbb"));
    combo->addItem(String("\xce\xa9\xce\xbc\xce\xad\xce\xb3\xce\xb1"));
    TextInput* input = box->createChild<TextInput>(String("\xd0\x96\xd1\x83\xd0\xba \xc5\x81\xc3\xb3" "d\xc5\xba"));
    // the hints, asked for before anything was painted
    const Widget::Vec2f hints[4] = { label->sizeHint(), button->sizeHint(), combo->sizeHint(), input->sizeHint() };
    for (int i = 0; i < 4; ++i)
    {
        run.hintW[i] = hints[i].x;
        run.hintH[i] = hints[i].y;
    }
    const Font* sized = screen.h.atlas().defaultFont().atHeight(Theme::instance().fontSize);
    run.madeByTheHint = sized && sized->isDynamic() && sized->hasBakedGlyph(0x0416) && sized->hasBakedGlyph(0x03a9) && sized->hasBakedGlyph(0x0141);
    screen.h.relayout();
    screen.h.frames(2);
    run.madeAfterTheHint = !screen.h.atlas().dynamicDirty();
    run.shot = screen.take();
    for (size_t i = 0; i < run.shot.px.size(); ++i)
        if (run.shot.px[i] != run.shot.px[0]) ++run.ink;
    const Rect lr = label->absoluteRect();
    for (int y = static_cast<int>(lr.y); y < static_cast<int>(lr.y + lr.h) && y < run.shot.h; ++y)
        for (int x = static_cast<int>(lr.x); x < static_cast<int>(lr.x + lr.w) && x < run.shot.w; ++x)
            if (run.shot.px[static_cast<size_t>(y) * static_cast<size_t>(run.shot.w) + static_cast<size_t>(x)] != run.shot.px[0]) ++run.labelInk;
    return run;
}

void testNewGlyphsAreMeasuredAndDrawnAfterTheAtlasPixelsAreReleased()
{
#ifdef IGUI_FONTATLAS_RELEASE_PIXELS
    const DynamicReleaseRun kept = dynamicReleaseRun(false);
    const DynamicReleaseRun released = dynamicReleaseRun(true);
    check(kept.dynamic && released.dynamic, "the dynamic page is a texture of the rasteriser (the comparison draws from it)");
    check(!kept.released && released.released, "the baked pixels are kept in one run and released in the other");
    check(kept.neverBaked && released.neverBaked, "the codepoints used are in no baked range of the atlas");
    check(kept.madeByTheHint && released.madeByTheHint, "the size hints rasterised the glyphs into the dynamic page, before any paint");
    check(kept.madeAfterTheHint && released.madeAfterTheHint, "the page was uploaded by the frames (nothing left dirty)");
    bool sameHints = true;
    for (int i = 0; i < 4; ++i)
        if (kept.hintW[i] != released.hintW[i] || kept.hintH[i] != released.hintH[i]) sameHints = false;
    check(sameHints, "a Label, a Button, a ComboBox and a TextInput hint the same size with the pixels released");
    check(kept.labelInk > 300 && released.labelInk == kept.labelInk,
            "the glyphs of the Label are drawn from the dynamic page (ink in its rectangle) and as many with the pixels released");
    check(kept.ink > 3000 && released.ink == kept.ink, "the window has as much ink with the pixels released");
    check(differing(kept.shot, released.shot, 0, 0, kept.shot.w, kept.shot.h) == 0,
            "glyphs first measured and drawn after the release come out pixel for pixel as with the pixels kept");
#endif
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
    RUN(testIconsAreDrawnWithoutAnAtlas);
    RUN(testTextEditTabsAreSpaces);
    RUN(testAssetBrowserIconsAreDrawnWithoutAnAtlas);
    RUN(testSplitButtonCutsWhereAButtonDoes);
    RUN(testTabsPastTheEndOfTheBarAreNotDrawnOrHit);
    RUN(testIconWithoutAGlyphIsNotAQuestionMark);
    RUN(testTextIsDrawnWithTheAtlasPixelsReleased);
    RUN(testNewGlyphsAreMeasuredAndDrawnAfterTheAtlasPixelsAreReleased);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
