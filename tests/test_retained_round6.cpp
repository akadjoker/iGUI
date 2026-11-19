// Round 6 (docs/ROUND6.md): the scroll bar room that painting reads from the bar and not from the theme, the piano roll's
// notes bounded by the pixels on screen, widgets that delete themselves in their own signals, the list box of the zen_ui
// adapter, the tool bar ids of the serializer, the float casts. Headless: the real frame loop (tests/RetainedHarness.hpp),
// the synthetic input of WidgetApp (feedMouseMove/Button/Wheel) where a mouse is involved, pixels of the software
// rasteriser where a picture is the point. No standard library: ct and C only.

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"
#include "StageHarness.hpp"

#include <unistd.h>

#include <igui/widgets/AudioWidgets.hpp>
#include <igui/widgets/AssetBrowser.hpp>
#include <igui/widgets/AutomotiveWidgets.hpp>
#include <igui/widgets/ChartWidget.hpp>
#include <igui/widgets/ConsoleWidget.hpp>
#include <igui/widgets/InputWidgets.hpp>
#include <igui/widgets/MenuWidgets.hpp>
#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/DataWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/ScrollWidgets.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/Theme.hpp>
#include <igui/widgets/ThumbnailGrid.hpp>
#include <igui/widgets/Timeline.hpp>
#include <igui/widgets/ToolBar.hpp>
#include <igui/widgets/TreePropertyColorWidgets.hpp>
#include <igui/widgets/WidgetSerializer.hpp>

using namespace rh;

// ---- the ceiling on what a frame may ask of the heap (the hostile cases, below) ----
//
// The heap requests of a frame are counted through the wrapped malloc, calloc and realloc (ct allocates with them; the
// link options of the target say so) and a frame of a hostile case that asks for more than kMaxFrameBytes stops the
// program there, naming the case: a loop that does not stop or a size that is not bounded fails here, small and fast,
// and never by taking the memory of the machine. Counting is on only while a hostile frame is being made.

namespace
{

constexpr unsigned long long kMaxFrameBytes = 64ull << 20;
unsigned long long gAskedBytes = 0;
bool gCounting = false;
const char* gCurrentCase = "(none)";

[[noreturn]] void frameAskedTooMuch(unsigned long long bytes)
{
    gCounting = false;
    char text[256];
    const int n = snprintf(text, sizeof text, "\n    FAIL %s: one frame asked the heap for more than %llu MiB (%llu so far): a loop that does not stop\n",
            gCurrentCase, kMaxFrameBytes >> 20, bytes >> 20);
    if (n > 0)
    {
        const ssize_t written = write(2, text, static_cast<size_t>(n < static_cast<int>(sizeof text) ? n : static_cast<int>(sizeof text) - 1));
        (void) written;
    }
    _exit(3);
}

void asked(size_t bytes)
{
    if (!gCounting) return;
    gAskedBytes += bytes;
    if (gAskedBytes > kMaxFrameBytes) frameAskedTooMuch(gAskedBytes);
}

} // namespace

extern "C" void* __real_malloc(size_t);
extern "C" void* __real_calloc(size_t, size_t);
extern "C" void* __real_realloc(void*, size_t);
extern "C" void* __wrap_malloc(size_t n)
{
    asked(n);
    return __real_malloc(n);
}
extern "C" void* __wrap_calloc(size_t a, size_t b)
{
    asked(a * b);
    return __real_calloc(a, b);
}
extern "C" void* __wrap_realloc(void* p, size_t n)
{
    asked(n);
    return __real_realloc(p, n);
}

namespace
{

// The theme as it was, put back at the end of a case that changes it.
// (and a theme written by hand is not looked for by update() meanwhile: these cases are about what painting does when a theme
// changes and nothing laid out again, which update() would otherwise do for it, WidgetApp::watchTheme)
struct ThemeScope
{
    ThemeScope() : saved(Theme::instance())
    {
        WidgetApp::instance().watchTheme(false);
    }
    ~ThemeScope()
    {
        Theme::instance() = saved;
        WidgetApp::instance().watchTheme(true);
    }
    Theme saved;
};

// A widget that asks for a size and fills its rectangle with a colour (so a clip can be seen in the pixels).
class Solid : public Widget
{
public:
    IG_WIDGET_CLASS(Solid, Widget);
    Solid(float w, float h) : w_(w), h_(h) {}
    Vec2f sizeHint() const override
    {
        return { w_, h_ };
    }
    void paint(PaintContext& ctx) override
    {
        const Rect r = absoluteRect();
        ctx.fill.SetColor(200, 60, 60, 255);
        ctx.fillRect(r.x, r.y, r.w, r.h);
    }
    float w_, h_;
};

ScrollBar* findBar(Widget* w, ScrollBarOrientation o)
{
    for (Widget* c: w->children())
        if (ScrollBar* bar = widget_cast<ScrollBar>(c))
            if (bar->orientation() == o) return bar;
    return nullptr;
}

uint32_t pixelAt(const Shot& s, int x, int y)
{
    return s.px[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)];
}

// The last column (+1) of a row of pixels that is not the same in two shots; 0 when they are equal along it.
int lastDifferingColumn(const Shot& a, const Shot& b, int y, int x0, int x1)
{
    int last = 0;
    for (int x = x0; x < x1; ++x)
        if (pixelAt(a, x, y) != pixelAt(b, x, y)) last = x + 1;
    return last;
}

// ---- item 1: painting asks the bar, not the theme ----
//
// The room a scroll bar takes from its list or view is the bar's own rectangle, which the layout set to the theme's thickness.
// Painting used to ask the theme again at every paint (ScrollBar::thickness(), a call per widget per frame); a theme that
// changes with no layout in between now leaves the picture as the layout made it, content and bar agreeing, until the layout
// takes the new thickness.
void testPaintTakesTheBarsRoomFromTheBar()
{
    const ThemeScope scope;
    Theme::instance().scrollbarWidth = 14.0f;

    // ListBox: the highlight of the selected row ends 16 pixels (the bar and its 2 pixels) before the right edge
    {
        Screen screen(400.0f, 260.0f);
        Harness& h = screen.h;
        Widget* holder = h.root()->createChild<Widget>();
        ListBox* list = holder->createChild<ListBox>();
        list->setRect({ 10.0f, 10.0f, 240.0f, 160.0f });
        for (int i = 0; i < 40; ++i) list->addItem(String("item ") + String::number(i));
        h.relayout();
        ScrollBar* bar = findBar(list, ScrollBarOrientation::Vertical);
        check(bar && bar->rect().w == 14.0f, "ListBox: the bar was laid out 14 thick");
        const Shot plain = screen.take();
        list->setSelectedIndex(2);
        h.frame();
        const Shot selected = screen.take();
        const float ih = Theme::instance().fontSize + Theme::instance().padding * 2.0f; // the height of a row
        const int y = static_cast<int>(10.0f + 2.5f * ih);                            // inside row 2
        const int before = lastDifferingColumn(plain, selected, y, 10, 400);
        check(before == 10 + 240 - 16, "ListBox: the selected row ends where the bar's room begins (16 from the right edge)");
        if (before != 10 + 240 - 16) printf("    the highlight ends at %d, wanted %d\n", before, 10 + 240 - 16);
        // the theme moves on with no layout: what is painted stays as laid out
        Theme::instance().scrollbarWidth = 20.0f;
        h.frame();
        const Shot after = screen.take();
        const int now = lastDifferingColumn(plain, after, y, 10, 400);
        check(now == 10 + 240 - 16, "ListBox: a theme changed with no layout leaves the highlight where the bar is (it does not ask the theme at paint)");
        if (now != 10 + 240 - 16) printf("    the highlight ends at %d, wanted %d\n", now, 10 + 240 - 16);
        Rect item;
        list->itemRect(2, item);
        check(item.x + item.w == 10.0f + 240.0f - 16.0f, "ListBox: itemRect agrees with the bar's rectangle too");
        h.relayout();
        check(bar && bar->rect().w == 20.0f, "ListBox: and the next layout takes the new thickness");
        Theme::instance().scrollbarWidth = 14.0f;
    }

    // ListWidget: the selected row's highlight
    {
        Screen screen(400.0f, 260.0f);
        Harness& h = screen.h;
        Widget* holder = h.root()->createChild<Widget>();
        ListWidget* list = holder->createChild<ListWidget>();
        list->setRect({ 10.0f, 10.0f, 240.0f, 160.0f });
        for (int i = 0; i < 20; ++i) list->addRow<Label>(String("row ") + String::number(i));
        h.relayout();
        ScrollBar* bar = findBar(list, ScrollBarOrientation::Vertical);
        check(bar && bar->rect().w == 14.0f, "ListWidget: the bar was laid out 14 thick");
        const Shot plain = screen.take();
        list->setSelectedIndex(1);
        h.frame();
        const Shot selected = screen.take();
        const Rect row = list->children().size() > 2 ? list->children()[1]->absoluteRect() : Rect();
        const int y = static_cast<int>(row.y + row.h * 0.5f);
        const int ends = lastDifferingColumn(plain, selected, y, 10, 400);
        Theme::instance().scrollbarWidth = 20.0f;
        h.frame();
        const Shot after = screen.take();
        const int now = lastDifferingColumn(plain, after, y, 10, 400);
        check(ends > 0 && now == ends, "ListWidget: a theme changed with no layout leaves the highlight where it was laid out");
        if (now != ends) printf("    the highlight ended at %d, now at %d\n", ends, now);
        Theme::instance().scrollbarWidth = 14.0f;
    }

    // ScrollView: the content is clipped where the vertical bar begins, the horizontal one's room is taken from the bottom
    {
        Screen screen(400.0f, 300.0f);
        Harness& h = screen.h;
        Widget* holder = h.root()->createChild<Widget>();
        ScrollView* view = holder->createChild<ScrollView>();
        view->setRect({ 10.0f, 10.0f, 200.0f, 150.0f });
        view->setContent<Solid>(500.0f, 600.0f);
        h.relayout();
        ScrollBar* vbar = findBar(view, ScrollBarOrientation::Vertical);
        ScrollBar* hbar = findBar(view, ScrollBarOrientation::Horizontal);
        check(vbar && hbar && vbar->rect().w == 14.0f && hbar->rect().h == 14.0f, "ScrollView: both bars were laid out 14 thick");
        const int row = 60;
        const Shot s1 = screen.take();
        int lastRed = 0;
        for (int x = 10; x < 220; ++x)
            if (pixelAt(s1, x, row) == 0xffc83c3cu) lastRed = x + 1;
        check(lastRed == 10 + 200 - 16, "ScrollView: the content is clipped 16 pixels before the right edge (the bar and the gap)");
        int lastRedRow = 0;
        for (int y = 10; y < 170; ++y)
            if (pixelAt(s1, 60, y) == 0xffc83c3cu) lastRedRow = y + 1;
        check(lastRedRow == 10 + 150 - 16, "ScrollView: and 16 pixels before the bottom edge");
        Theme::instance().scrollbarWidth = 20.0f;
        h.frame();
        const Shot s2 = screen.take();
        int lastRed2 = 0, lastRedRow2 = 0;
        for (int x = 10; x < 220; ++x)
            if (pixelAt(s2, x, row) == 0xffc83c3cu) lastRed2 = x + 1;
        for (int y = 10; y < 170; ++y)
            if (pixelAt(s2, 60, y) == 0xffc83c3cu) lastRedRow2 = y + 1;
        check(lastRed2 == lastRed && lastRedRow2 == lastRedRow, "ScrollView: a theme changed with no layout leaves the clip where the bars are");
        if (lastRed2 != lastRed || lastRedRow2 != lastRedRow) printf("    clip right %d -> %d, bottom %d -> %d\n", lastRed, lastRed2, lastRedRow, lastRedRow2);
        Theme::instance().scrollbarWidth = 14.0f;
    }

    // TextEdit, ConsoleWidget and TreeView draw their bar themselves: they take the thickness at layout too (they used to read
    // the theme at every paint), so a theme changed with no layout leaves what they draw as it was, and the next layout moves it
    {
        Screen screen(500.0f, 300.0f);
        Widget* holder = screen.h.root()->createChild<Widget>();
        TextEdit* edit = holder->createChild<TextEdit>();
        edit->setRect({ 10.0f, 10.0f, 220.0f, 120.0f });
        String text;
        for (int i = 0; i < 60; ++i) text = text + "line " + String::number(i) + "\n";
        edit->setText(text);
        ConsoleWidget* console = holder->createChild<ConsoleWidget>();
        console->setRect({ 250.0f, 10.0f, 240.0f, 120.0f });
        for (int i = 0; i < 60; ++i) console->log(LogLevel::Info, String("message ") + String::number(i));
        TreeView* tree = holder->createChild<TreeView>();
        tree->setRect({ 10.0f, 150.0f, 220.0f, 120.0f });
        for (int i = 0; i < 40; ++i) tree->addRoot(String("node ") + String::number(i));
        screen.h.relayout();
        screen.h.frames(2);
        const Shot laidOut = screen.take();
        Theme::instance().scrollbarWidth = 20.0f;
        screen.h.frames(2);
        const Shot noLayout = screen.take();
        screen.h.relayout();
        screen.h.frames(2);
        const Shot relaid = screen.take();
        struct Drawn
        {
            const char* name;
            Widget* widget;
        };
        const Drawn drawn[] = { { "TextEdit", edit }, { "ConsoleWidget", console }, { "TreeView", tree } };
        for (const Drawn& d: drawn)
        {
            const Rect r = d.widget->absoluteRect();
            const int x0 = static_cast<int>(r.x), y0 = static_cast<int>(r.y), x1 = static_cast<int>(r.x + r.w), y1 = static_cast<int>(r.y + r.h);
            const int moved = differing(laidOut, noLayout, x0, y0, x1, y1);
            check(moved == 0, (String(d.name) + ": a theme changed with no layout leaves its bar as it was laid out").c_str());
            if (moved) printf("    %d pixels differ\n", moved);
            check(differing(laidOut, relaid, x0, y0, x1, y1) > 0, (String(d.name) + ": and the next layout takes the new thickness").c_str());
        }
        Theme::instance().scrollbarWidth = 14.0f;
    }
}

// ---- item 2: the piano roll's notes are bounded by the pixels on screen ----

// The roll of these tests: 400x200 at (20, 20) in a 480x260 window, 12 rows of 12 pixels (pitches 48 to 59, the top row is 59),
// the keys 36 pixels wide so the grid starts at x = 56 and 8 beats take 364 pixels, 45.5 a beat.
struct RollRig
{
    explicit RollRig(float rollHeight = 200.0f)
            : screen(480.0f, 260.0f),
              h(screen.h)
    {
        roll = h.root()->createChild<PianoRoll>();
        roll->setRect(Rect(20.0f, 20.0f, 400.0f, rollHeight));
        roll->setPitchRange(48, 60);
        roll->setTimeRange(0.0f, 8.0f);
        h.frame();
    }
    static float xBeat(float beat)
    {
        return 56.0f + beat / 8.0f * 364.0f;
    }
    static float yPitch(int pitch)
    {
        return 20.0f + static_cast<float>(59 - pitch) * 12.0f + 6.0f;
    }
    size_t vertices()
    {
        h.frame();
        return GetDrawList().vertices().size();
    }
    Screen screen;
    Harness& h;
    PianoRoll* roll = nullptr;
};

// How many vertices one note adds to a frame (all six rectangles of it).
size_t verticesOfOneNote()
{
    RollRig rig;
    const size_t none = rig.vertices();
    rig.roll->addNote(55, 2.0f, 1.0f);
    return rig.vertices() - none;
}

// An ordinary roll draws every note, one by one, as it always did: ten notes are ten times what one is.
void testPianoRollOrdinaryRollDrawsEveryNote()
{
    const size_t one = verticesOfOneNote();
    check(one > 0, "(one note adds vertices)");
    RollRig rig;
    const size_t none = rig.vertices();
    for (int i = 0; i < 10; ++i) rig.roll->addNote(50 + i % 5, 0.5f * static_cast<float>(i), 0.4f);
    check(rig.vertices() - none == 10 * one, "ten notes in view are ten times the vertices of one: an ordinary roll is drawn note by note");
    // and a thousand notes (under half of the 4368 cells of this roll's view) still are
    RollRig rig2;
    const size_t none2 = rig2.vertices();
    for (int i = 0; i < 1000; ++i) rig2.roll->addNote(48 + i % 12, 8.0f * static_cast<float>(i) / 1000.0f, 0.05f);
    check(rig2.vertices() - none2 == 1000 * one, "a thousand notes in view, under half the cells of the view, are drawn one by one");
}

// The notes that do not reach the visible part are not drawn: a roll too short for its rows draws nothing for the rows that
// are past its bottom edge.
void testPianoRollDrawsOnlyNotesThatReachTheView()
{
    const size_t one = verticesOfOneNote();
    RollRig rig(50.0f); // the rows from the fifth (the top four are 48 pixels) are below the bottom edge of the roll
    const size_t none = rig.vertices();
    for (int i = 0; i < 20; ++i) rig.roll->addNote(48 + i % 6, 0.2f * static_cast<float>(i), 0.1f); // pitches 48 to 53: the rows from the 7th
    check(rig.vertices() == none, "notes in rows past the bottom of the roll add no vertices (they are drawn nowhere)");
    for (int i = 0; i < 4; ++i) rig.roll->addNote(59 - i % 2, 0.2f * static_cast<float>(i), 0.1f); // the top two rows
    check(rig.vertices() - none == 4 * one, "and the notes in the rows that show are drawn");
    // notes past the left and right edges of the view
    RollRig rig2;
    const size_t none2 = rig2.vertices();
    for (int i = 0; i < 50; ++i) rig2.roll->addNote(55, 20.0f + static_cast<float>(i), 1.0f);
    for (int i = 0; i < 50; ++i) rig2.roll->addNote(55, -60.0f + static_cast<float>(i), 1.0f);
    check(rig2.vertices() == none2, "notes before and after the view add no vertices");
}

// 36000 notes of a tenth of a pixel in nine rows (4000 in each, the first four beats) are nine runs: a note for each row, from
// where the first starts to where the last one ends.
void testPianoRollManyNotesAreDrawnAsWhatTheyCover()
{
    const size_t one = verticesOfOneNote();
    const int rows = 9;
    RollRig rig;
    const size_t none = rig.vertices();
    const Shot empty = rig.screen.take();
    for (int k = 0; k < 4000; ++k)
        for (int r = 0; r < rows; ++r) rig.roll->addNote(50 + r, static_cast<float>(k) * 0.001f, 0.00005f);
    const size_t dense = rig.vertices();
    check(dense - none == static_cast<size_t>(rows) * one, "36000 notes of tiny length in nine rows are nine notes' worth of vertices (a run in each row)");
    if (dense - none != static_cast<size_t>(rows) * one) printf("    %zu vertices more than without notes, one note is %zu\n", dense - none, one);
    const Shot shot = rig.screen.take();
    // what they cover: each row from the first start (x = 56) to 4 pixels past the last (x = 56 + 3.999 beats of 45.5 pixels)
    bool contiguous = true;
    int firstCol[16] = {}, lastCol[16] = {};
    for (int r = 0; r < rows; ++r)
    {
        const int y = static_cast<int>(RollRig::yPitch(50 + r));
        int first = -1, last = -1;
        for (int x = 56; x < 420; ++x)
        {
            const bool covered = pixelAt(shot, x, y) != pixelAt(empty, x, y);
            if (covered)
            {
                if (first < 0) first = x;
                if (last >= 0 && x != last + 1) contiguous = false;
                last = x;
            }
        }
        firstCol[r] = first;
        lastCol[r] = last;
    }
    check(contiguous, "each row of them is drawn as one stretch, without a gap");
    const int wantFirst = 56, wantLast = static_cast<int>(RollRig::xBeat(3.999f)) + 4;
    bool ends = true;
    for (int r = 0; r < rows; ++r)
        if (firstCol[r] < wantFirst || firstCol[r] > wantFirst + 1 || lastCol[r] < wantLast - 1 || lastCol[r] > wantLast + 1) ends = false;
    check(ends, "from the start of the first note to the end of the last");
    if (!ends) printf("    row 0 covers %d to %d, wanted %d to %d\n", firstCol[0], lastCol[0], wantFirst, wantLast);
    // the rows without notes are as they were
    const int y = static_cast<int>(RollRig::yPitch(49));
    int changed = 0;
    for (int x = 56; x < 420; ++x)
        if (pixelAt(shot, x, y) != pixelAt(empty, x, y)) ++changed;
    check(changed == 0, "and the rows without notes are not touched");
}

// The same picture as a roll with the nine long notes would draw.
void testPianoRollManyNotesLookLikeLongNotes()
{
    const int rows = 9;
    RollRig dense;
    for (int k = 0; k < 4000; ++k)
        for (int r = 0; r < rows; ++r) dense.roll->addNote(50 + r, static_cast<float>(k) * 0.001f, 0.00005f);
    dense.h.frame();
    const Shot a = dense.screen.take();
    RollRig plain;
    // one note for a row from 56 to 4 pixels past the last start: a width of (x of 3.999 beats - 56) + 4, so a length of that plus 1 pixel
    const float width = RollRig::xBeat(3.999f) - 56.0f + 4.0f;
    for (int r = 0; r < rows; ++r) plain.roll->addNote(50 + r, 0.0f, (width + 1.0f) / 45.5f);
    plain.h.frame();
    const Shot b = plain.screen.take();
    const int n = differing(a, b, 56, 20, 420, 220);
    check(n < 60, "the picture of 36000 tiny notes is the picture of nine long ones (a few pixels of rounding at the ends apart)");
    if (n >= 60) printf("    %d pixels differ\n", n);
}

// The lane: more than two notes for each pixel column is one bar for each column, the tallest.
void testPianoRollLaneDrawsOneBarForEachColumn()
{
    // 3000 notes in the 364 pixels of the lane (eight in each column), velocities that differ, the lane on
    const size_t one = verticesOfOneNote();
    const int notes = 3000;
    RollRig rig;
    rig.roll->setShowVelocityLane(true);
    rig.h.frame();
    const size_t laneOnly = rig.vertices();
    const Shot bare = rig.screen.take();
    ct::Vector<float> starts, vels;
    for (int i = 0; i < notes; ++i)
    {
        const float start = 8.0f * static_cast<float>(i) / static_cast<float>(notes);
        const float vel = static_cast<float>((i * 37) % 100) / 100.0f;
        starts.push_back(start);
        vels.push_back(vel);
        rig.roll->addNote(50 + i % 9, start, 0.00005f, vel);
    }
    const size_t with = rig.vertices();
    // the notes area: nine runs; the lane: a bar (two rectangles, 8 vertices) for each column that has a note, 364 of them at most
    check(with - laneOnly <= 9 * one + 364 * 8, "3000 notes with the lane on add at most nine runs and a bar for each pixel column");
    if (with - laneOnly > 9 * one + 364 * 8) printf("    %zu vertices added\n", with - laneOnly);
    // every note's own column has a bar at least as tall as the note's velocity: look for the bar's colour at the height of each
    const Shot shot = rig.screen.take();
    const float laneTop = 20.0f + 200.0f - 60.0f, foot = laneTop + 60.0f - 2.0f, usable = 60.0f - 2.0f - 4.0f;
    int missing = 0;
    for (int i = 0; i < notes; ++i)
    {
        const float x = 56.0f + starts[static_cast<size_t>(i)] / 8.0f * 364.0f;
        const float h = Math::max2(2.0f, vels[static_cast<size_t>(i)] * usable);
        if (x < 58.0f || x >= 418.0f) continue; // (the playhead is drawn over the first two columns, the roll's border over the last)
        const int px = static_cast<int>(x), py = static_cast<int>(Math::min2(foot - 1.0f, foot - h + 1.5f)); // inside a bar of that height
        if (pixelAt(shot, px, py) == pixelAt(bare, px, py))
        {
            if (++missing <= 4) printf("    note %d at x %g, velocity %g: pixel (%d, %d) is still the background\n", i, static_cast<double>(x), static_cast<double>(vels[static_cast<size_t>(i)]), px, py);
        }
    }
    check(missing == 0, "each of the 3000 notes has a bar at its start at least as tall as its velocity");
    if (missing) printf("    %d notes have no bar under them\n", missing);
}

// ---- item 3: a handler that deletes the widget it runs in ----
//
// The library's way to get rid of a widget is parent->removeChild(widget): it deletes the widget when the frame phase that is
// running (update(), paint()) is over, or at once when none is (WidgetApp::deleteLater); `delete` of a widget that is still
// in its parent leaves a pointer to freed memory in the parent and is not a way. What a widget does after a signal whose
// handler took it out is the point of these cases. Each runs twice: IN A FRAME (the mouse events come from
// WidgetApp::feedMouse*, so the deletion waits for the end of update() and the memory is still there when the widget goes on)
// and OUTSIDE ONE (the widget's own method called by the program, as an application does with setSelectedIndex() or
// setValue(): the removal deletes at once, and a widget that touches itself after the signal reads freed memory: AddressSanitizer
// stops the program there). The widget must go no further than the signal (WidgetApp::hasGone): nothing more is emitted, nothing
// of it is read.

// The piano roll in the holder: 400x200 at (20, 20), keys 36 wide, 12 rows of 12 pixels (48 to 59), 8 beats over 364 pixels.
struct RollStage : Stage
{
    explicit RollStage(int mode_) : Stage(mode_)
    {
        roll = holder->createChild<PianoRoll>();
        roll->setRect(Rect(20.0f, 20.0f, 400.0f, 200.0f));
        roll->setPitchRange(48, 60);
        roll->setTimeRange(0.0f, 8.0f);
        h.frame();
    }
    // a note put with a click and a drag: onNoteAdded, then onNoteCreated
    void putNote()
    {
        move(RollRig::xBeat(2.0f), RollRig::yPitch(55));
        down(roll);
        drag(roll, RollRig::xBeat(3.0f), RollRig::yPitch(55));
        up(roll);
    }
    void takeOut()
    {
        holder->removeChild(roll);
    }
    PianoRoll* roll = nullptr;
};

void testHandlerDeletesThePianoRoll()
{
    for (int mode = 0; mode < 2; ++mode)
    {
        {
            // onNoteAdded, the first signal of a note that was put: the roll stops there (nothing of it is touched, and
            // onNoteCreated, which comes after, is not emitted by a roll that is gone)
            RollStage r(mode);
            int added = 0, created = 0;
            r.roll->onNoteAdded.connect([&](int, float, float) { ++added; r.takeOut(); });
            r.roll->onNoteCreated.connect([&](int) { ++created; });
            r.putNote();
            r.h.frames(2);
            const bool ok = added == 1 && created == 0 && r.gone();
            check(ok, say("PianoRoll: onNoteAdded taking the roll out ends the gesture there", mode).c_str());
            if (!ok) printf("    added %d, created %d, gone %d\n", added, created, r.gone());
        }
        {
            // onNoteCreated, the last signal
            RollStage r(mode);
            int created = 0;
            r.roll->onNoteCreated.connect([&](int) { ++created; r.takeOut(); });
            r.putNote();
            r.h.frames(2);
            check(created == 1 && r.gone(), say("PianoRoll: onNoteCreated taking the roll out", mode).c_str());
        }
        {
            // a note picked up and moved
            RollStage r(mode);
            r.roll->addNote(55, 2.0f, 2.0f);
            int moved = 0;
            r.roll->onNoteMoved.connect([&](int) { ++moved; r.takeOut(); });
            r.h.frame();
            r.move(RollRig::xBeat(3.0f), RollRig::yPitch(55)); // the middle of the note
            r.down(r.roll);
            r.drag(r.roll, RollRig::xBeat(4.0f), RollRig::yPitch(56));
            r.up(r.roll);
            r.h.frames(2);
            check(moved == 1 && r.gone(), say("PianoRoll: onNoteMoved taking the roll out", mode).c_str());
        }
        {
            // a note resized by its end
            RollStage r(mode);
            r.roll->addNote(55, 2.0f, 2.0f);
            int resized = 0;
            r.roll->onNoteResized.connect([&](int) { ++resized; r.takeOut(); });
            r.h.frame();
            r.move(RollRig::xBeat(4.0f) - 2.0f, RollRig::yPitch(55)); // the end of the note
            r.down(r.roll);
            r.drag(r.roll, RollRig::xBeat(5.0f), RollRig::yPitch(55));
            r.up(r.roll);
            r.h.frames(2);
            check(resized == 1 && r.gone(), say("PianoRoll: onNoteResized taking the roll out", mode).c_str());
        }
        {
            // onNoteClicked, at the press: the button is still down when the roll goes; the release that follows finds no roll
            RollStage r(mode);
            r.roll->addNote(55, 2.0f, 2.0f);
            int clicked = 0;
            r.roll->onNoteClicked.connect([&](int) { ++clicked; r.takeOut(); });
            r.h.frame();
            r.move(RollRig::xBeat(3.0f), RollRig::yPitch(55));
            r.down(r.roll);
            r.up(r.roll);
            r.h.frames(2);
            check(clicked == 1 && r.gone(), say("PianoRoll: onNoteClicked taking the roll out", mode).c_str());
        }
        {
            // onNoteRemoved, emitted BEFORE the note goes and followed by code that works on the note and on the roll:
            // onNoteDeleted does not come from a roll that is gone
            RollStage r(mode);
            r.roll->addNote(55, 2.0f, 2.0f);
            int removed = 0, deleted = 0;
            r.roll->onNoteRemoved.connect([&](int) { ++removed; r.takeOut(); });
            r.roll->onNoteDeleted.connect([&](int, const PianoNote&) { ++deleted; });
            r.h.frame();
            r.move(RollRig::xBeat(3.0f), RollRig::yPitch(55));
            r.down(r.roll, 1);
            r.up(r.roll, 1);
            r.h.frames(2);
            const bool ok = removed == 1 && deleted == 0 && r.gone();
            check(ok, say("PianoRoll: onNoteRemoved taking the roll out", mode).c_str());
            if (!ok) printf("    removed %d, deleted %d, gone %d\n", removed, deleted, r.gone());
        }
        {
            // onNoteDeleted, the last
            RollStage r(mode);
            r.roll->addNote(55, 2.0f, 2.0f);
            int deleted = 0;
            r.roll->onNoteDeleted.connect([&](int, const PianoNote&) { ++deleted; r.takeOut(); });
            r.h.frame();
            r.move(RollRig::xBeat(3.0f), RollRig::yPitch(55));
            r.down(r.roll, 1);
            r.up(r.roll, 1);
            r.h.frames(2);
            check(deleted == 1 && r.gone(), say("PianoRoll: onNoteDeleted taking the roll out", mode).c_str());
        }
        {
            // the velocity lane: one signal for each note that changed, from the last; the first handler takes the roll out and
            // the rest are not told
            RollStage r(mode);
            r.roll->setShowVelocityLane(true);
            for (int i = 0; i < 5; ++i) r.roll->addNote(50 + i, 0.5f + static_cast<float>(i), 0.5f, 0.5f);
            int told = 0;
            r.roll->onNoteVelocityChanged.connect([&](int) { ++told; r.takeOut(); });
            r.h.frame();
            r.move(RollRig::xBeat(0.5f), 200.0f);
            r.down(r.roll);
            r.drag(r.roll, RollRig::xBeat(5.0f), 170.0f);
            r.up(r.roll);
            r.h.frames(2);
            const bool ok = told == 1 && r.gone();
            check(ok, say("PianoRoll: onNoteVelocityChanged taking the roll out in the first of five signals", mode).c_str());
            if (!ok) printf("    told %d, gone %d\n", told, r.gone());
        }
        {
            // a handler that takes some OTHER widget out leaves the roll to finish what it was doing: the note still goes, and says so
            RollStage r(mode);
            r.roll->addNote(55, 2.0f, 2.0f);
            Widget* other = r.holder->createChild<Widget>();
            int removed = 0, deleted = 0;
            r.roll->onNoteRemoved.connect([&](int) { ++removed; r.holder->removeChild(other); });
            r.roll->onNoteDeleted.connect([&](int, const PianoNote&) { ++deleted; });
            r.h.frame();
            r.move(RollRig::xBeat(3.0f), RollRig::yPitch(55));
            r.down(r.roll, 1);
            r.up(r.roll, 1);
            r.h.frames(2);
            check(removed == 1 && deleted == 1 && r.roll->notes().empty(), say("PianoRoll: a handler that takes another widget out does not stop the roll (the note goes, onNoteDeleted comes)", mode).c_str());
        }
    }
}

void testHandlerDeletesTheButton()
{
    for (int mode = 0; mode < 2; ++mode)
    {
        Stage st(mode);
        Button* button = st.holder->createChild<Button>("close");
        button->setRect(Rect(20.0f, 20.0f, 100.0f, 30.0f));
        st.h.frame();
        int clicks = 0, released = 0;
        button->released.connect([&](int) { ++released; });
        button->clicked.connect([&]() { ++clicks; st.holder->removeChild(button); });
        st.move(60.0f, 35.0f);
        st.down(button);
        st.up(button);
        st.h.frames(2);
        check(clicks == 1 && st.gone(), say("Button: clicked taking the button out", mode).c_str());
    }
}

void testHandlerDeletesTheMenu()
{
    // a menu is not in a parent: it is shown as a popup (exec) and its owner deletes it, from the handler of one of its actions too
    for (int mode = 0; mode < 2; ++mode)
    {
        Harness h(300.0f, 200.0f, false);
        Menu* menu = new Menu();
        MenuAction* act = menu->addAction("Go");
        menu->addAction("Other");
        int fired = 0;
        act->triggered.connect([&]()
                {
                    ++fired;
                    if (mode == 0) menu->clearActions(); // the action that is running goes
                    else
                        delete menu; // the menu goes, and the action with it
                });
        menu->exec(10.0f, 10.0f);
        h.frames(12);
        const Rect row = menu->actionRect(0);
        WidgetApp& app = WidgetApp::instance();
        app.feedMouseMove(row.x + row.w * 0.5f, row.y + row.h * 0.5f);
        app.feedMouseButton(0, true);
        app.feedMouseButton(0, false);
        h.frames(3);
        check(fired == 1, (String("Menu: an action's handler that ") + (mode == 0 ? "clears the actions (the running action goes)" : "deletes the menu") + " runs once and nothing reads what is gone").c_str());
        if (mode == 0) delete menu;
    }
}

void testHandlerDeletesTheListBox()
{
    for (int mode = 0; mode < 2; ++mode)
    {
        // the selection by a click, by the arrow key (in a frame) and by the program (outside one)
        for (int how = 0; how < 3; ++how)
        {
            if ((how == 2) != (mode == 1)) continue; // setSelectedIndex is the program's call; the click and the key are events
            Stage st(mode);
            ListBox* list = st.holder->createChild<ListBox>();
            list->setRect(Rect(10.0f, 10.0f, 200.0f, 150.0f));
            for (int i = 0; i < 40; ++i) list->addItem(String("item ") + String::number(i));
            st.h.relayout();
            int changed = 0;
            list->selectionChanged.connect([&](int) { ++changed; st.holder->removeChild(list); });
            if (how == 2) list->setSelectedIndex(3);
            else if (how == 0)
            {
                st.move(60.0f, 10.0f + Theme::instance().fontSize + Theme::instance().padding * 3.0f);
                st.down(list);
                st.up(list);
            }
            else
            {
                st.app.setFocused(list);
                st.app.feedKey(Key::Down, true);
                st.app.feedKey(Key::Down, false);
            }
            st.h.frames(2);
            const char* names[] = { "a click", "the arrow key", "setSelectedIndex" };
            const bool ok = changed == 1 && st.gone();
            check(ok, say((String("ListBox: selectionChanged taking the list out, selection by ") + names[how]).c_str(), mode).c_str());
            if (!ok) printf("    changed %d, gone %d\n", changed, st.gone());
        }
        {
            Stage st(mode);
            ListWidget* list = st.holder->createChild<ListWidget>();
            list->setRect(Rect(10.0f, 10.0f, 200.0f, 150.0f));
            for (int i = 0; i < 20; ++i) list->addRow<Label>(String("row ") + String::number(i));
            st.h.relayout();
            int changed = 0;
            list->selectionChanged.connect([&](int) { ++changed; st.holder->removeChild(list); });
            if (mode == 1) list->setSelectedIndex(2);
            else
            {
                st.move(60.0f, 10.0f + 40.0f);
                st.down(list);
                st.up(list);
            }
            st.h.frames(2);
            check(changed == 1 && st.gone(), say("ListWidget: selectionChanged taking the list out", mode).c_str());
        }
    }
}

void testHandlerDeletesTheSlider()
{
    for (int mode = 0; mode < 2; ++mode)
    {
        for (int how = 0; how < 2; ++how)
        {
            if ((how == 0) != (mode == 1)) continue; // setValue is the program's call, the press on the track is an event
            Stage st(mode);
            Slider* slider = st.holder->createChild<Slider>(0.0f, 1.0f, 0.5f);
            slider->setRect(Rect(20.0f, 20.0f, 200.0f, 24.0f));
            st.h.relayout();
            int changed = 0;
            slider->onValueChanged.connect([&](float) { ++changed; st.holder->removeChild(slider); });
            if (how == 0) slider->setValue(0.9f);
            else
            {
                st.move(30.0f, 32.0f);
                st.down(slider);
                st.drag(slider, 40.0f, 32.0f);
                st.up(slider);
            }
            st.h.frames(2);
            const bool ok = changed == 1 && st.gone();
            check(ok, say(how == 0 ? "Slider: onValueChanged taking the slider out, setValue" : "Slider: onValueChanged taking the slider out, a press on the track", mode).c_str());
            if (!ok) printf("    changed %d, gone %d\n", changed, st.gone());
        }
    }
}

// ---- item 6: a loaded tool bar keeps the ids it was saved with ----

ToolBar* firstToolBar(Widget* loaded)
{
    if (!loaded) return nullptr;
    for (Widget* c: loaded->children())
        if (ToolBar* bar = widget_cast<ToolBar>(c)) return bar;
    return nullptr;
}

// What an application keeps of a bar: the id of each of its items by what they say (the tooltip).
int idOf(const ToolBar& bar, const char* tooltip)
{
    for (int i = 0; i < bar.itemCount(); ++i)
        if (bar.itemTooltip(bar.itemIdAt(i)) == String(tooltip)) return bar.itemIdAt(i);
    return -1;
}

void testToolBarKeepsItsIdsAfterRemovalsAndDrags()
{
    WidgetSerializer::registerBuiltinTypes();
    {
        // items removed: the ids that are left are not 1, 2, 3 in the order of the bar, and the application's handlers know them
        BoxLayout root(LayoutDir::Vertical);
        ToolBar* bar = root.createChild<ToolBar>();
        const int a = bar->addButton(IconId::Gear, "A");
        const int b = bar->addButton(IconId::Folder, "B");
        const int c = bar->addToggle(IconId::File, "C", true);
        const int d = bar->addButton(3, "D");
        bar->addSeparator();
        const int f = bar->addToggle(IconId::Gear, "F");
        bar->removeItem(b);
        bar->removeItem(d);
        const int g = bar->addButton(IconId::Folder, "G");
        const json saved = WidgetSerializer::save(&root);
        BoxLayout destination(LayoutDir::Vertical);
        ToolBar* copy = firstToolBar(WidgetSerializer::load(saved, &destination));
        check(copy != nullptr && copy->itemCount() == bar->itemCount(), "(the bar with items removed is loaded)");
        if (!copy) return;
        bool same = true;
        for (int i = 0; i < bar->itemCount(); ++i)
            if (copy->itemIdAt(i) != bar->itemIdAt(i)) same = false;
        check(same, "a bar that had items removed is loaded with the ids it was saved with");
        if (!same)
            for (int i = 0; i < copy->itemCount(); ++i) printf("    item %d: id %d, saved with %d\n", i, copy->itemIdAt(i), bar->itemIdAt(i));
        check(idOf(*copy, "A") == a && idOf(*copy, "C") == c && idOf(*copy, "F") == f && idOf(*copy, "G") == g, "so the id an application kept for \"C\" is still the id of \"C\"");
        check(copy->itemType(c) == ToolBar::ItemType::Toggle && copy->isItemChecked(c), "(and the item that has it is the toggle that was checked)");
        // the next one added to the loaded bar does not take an id that is in use
        const int next = copy->addButton(IconId::Gear, "new");
        bool free = true;
        for (int i = 0; i < copy->itemCount(); ++i)
            if (copy->itemIdAt(i) == next && copy->itemTooltip(next) != String("new")) free = false;
        check(free && next > g, "and the next item added to the loaded bar gets an id past all of them");
    }
    {
        // dragged: the order changed and each id went with its item
        Harness h(480.0f, 120.0f, false);
        ToolBar* bar = h.root()->createChild<ToolBar>();
        bar->setRect(Rect(0.0f, 0.0f, 300.0f, 34.0f));
        const int a = bar->addButton(IconId::Gear, "A");
        const int b = bar->addButton(IconId::Folder, "B");
        const int c = bar->addButton(IconId::File, "C");
        h.relayout();
        Rect ra, rb;
        check(bar->itemRect(a, ra) && bar->itemRect(b, rb), "(the items have rectangles)");
        WidgetApp& app = h.app;
        app.feedMouseMove(ra.x + ra.w * 0.5f, ra.y + ra.h * 0.5f);
        app.feedMouseButton(0, true);
        app.feedMouseMove(rb.x + rb.w * 0.5f + 6.0f, rb.y + rb.h * 0.5f);
        app.feedMouseMove(rb.x + rb.w * 0.5f + 12.0f, rb.y + rb.h * 0.5f);
        app.feedMouseButton(0, false);
        check(bar->itemIdAt(0) == b && bar->itemIdAt(1) == a && bar->itemIdAt(2) == c, "(the drag put B first: the ids are 2, 1, 3)");
        const json saved = WidgetSerializer::save(h.root());
        BoxLayout destination(LayoutDir::Vertical);
        Widget* loaded = WidgetSerializer::load(saved, &destination);
        ToolBar* copy = firstToolBar(loaded);
        check(copy && copy->itemIdAt(0) == b && copy->itemIdAt(1) == a && copy->itemIdAt(2) == c, "a bar that was dragged is loaded with the ids 2, 1, 3, each with its item");
        check(copy && copy->itemTooltip(copy->itemIdAt(0)) == String("B") && copy->itemTooltip(copy->itemIdAt(1)) == String("A"), "(B and A, in that order)");
    }
}

void testToolBarLoadsFilesWithoutIdsAndWithBadOnes()
{
    WidgetSerializer::registerBuiltinTypes();
    // a file saved before the ids were in it: given again from 1, in the order of the list
    const char* old = "{\"type\":\"BoxLayout\",\"children\":[{\"type\":\"ToolBar\",\"items\":[{\"type\":\"Button\",\"icon\":1,\"tooltip\":\"A\"},"
                      "{\"type\":\"Separator\"},{\"type\":\"Toggle\",\"cell\":2,\"tooltip\":\"C\",\"checked\":true}]}]}";
    json::Error error;
    const json oldFile = json::parse(String(old), &error);
    check(!error, "(the old file parses)");
    BoxLayout destination(LayoutDir::Vertical);
    ToolBar* bar = firstToolBar(WidgetSerializer::load(oldFile, &destination));
    check(bar && bar->itemCount() == 3 && bar->itemIdAt(0) == 1 && bar->itemIdAt(1) == 2 && bar->itemIdAt(2) == 3, "a file saved before round 6, with no ids, loads with the ids 1, 2, 3 as it always did");
    check(bar && bar->itemType(3) == ToolBar::ItemType::Toggle && bar->isItemChecked(3) && bar->itemCell(3) == 2, "(and every other thing of the items as it was)");
    // a file made by hand: ids that are repeated, zero, negative, enormous, not numbers, and one that is fine
    const char* odd = "{\"type\":\"BoxLayout\",\"children\":[{\"type\":\"ToolBar\",\"items\":[{\"id\":5,\"tooltip\":\"one\"},{\"id\":5,\"tooltip\":\"two\"},"
                      "{\"id\":0,\"tooltip\":\"three\"},{\"id\":-3,\"tooltip\":\"four\"},{\"id\":99999999999,\"tooltip\":\"five\"},{\"id\":\"x\",\"tooltip\":\"six\"},"
                      "{\"id\":2,\"type\":\"Spacer\"},{\"id\":1.5,\"tooltip\":\"seven\"}]}]}";
    const json oddFile = json::parse(String(odd), &error);
    check(!error, "(the odd file parses)");
    BoxLayout destination2(LayoutDir::Vertical);
    ToolBar* odd2 = firstToolBar(WidgetSerializer::load(oddFile, &destination2));
    check(odd2 && odd2->itemCount() == 8, "every item of a file with bad ids is an item");
    if (!odd2) return;
    bool distinct = true, positive = true;
    for (int i = 0; i < odd2->itemCount(); ++i)
    {
        if (odd2->itemIdAt(i) < 1) positive = false;
        for (int j = i + 1; j < odd2->itemCount(); ++j)
            if (odd2->itemIdAt(i) == odd2->itemIdAt(j)) distinct = false;
    }
    check(distinct && positive, "and their ids are all different and from 1 up (the repeated, zero, negative, enormous and not numbers get the next one)");
    check(odd2->itemIdAt(0) == 5 && odd2->itemIdAt(6) == 2, "the ones that were fine (5, and 2 on the spacer) are kept");
    // the new API on its own
    ToolBar bar2;
    check(bar2.addItemWithId(7, ToolBar::ItemType::Button, -1, IconId::Gear, "seven") == 7, "addItemWithId gives the id asked for");
    check(bar2.addButton(IconId::Folder, "next") == 8, "and the next id of addButton goes on past it");
    check(bar2.addItemWithId(7, ToolBar::ItemType::Toggle, 3, IconId::None, "again", true) == 9, "an id that is taken gives the next one");
    check(bar2.addItemWithId(0, ToolBar::ItemType::Separator) == 10 && bar2.addItemWithId(-4, ToolBar::ItemType::Spacer) == 11, "zero and a negative one give the next one too");
    check(bar2.itemType(9) == ToolBar::ItemType::Toggle && bar2.isItemChecked(9) && bar2.itemCell(9) == 3 && bar2.itemTooltip(7) == String("seven"), "(with what the kind has: cell, tooltip, checked)");
}

// ---- item 8: the casts of numbers that are not numbers (-fsanitize=float-cast-overflow) ----
//
// A float that is NaN, infinite or out of range cast to an int is undefined (the sanitizer says so; on x86 it gives INT_MIN, which
// the code after it mostly handles by luck). Math::toInt is the cast that says what it gives. Each widget below hit one of these
// with the geometry or the data of a hostile test; the cases run the widgets on exactly that, and under the sanitizer the program
// stops if one is left.

void testToIntCasts()
{
    const float nan = 0.0f / 0.0f, inf = 1.0f / 0.0f;
    check(Math::toInt(nan) == 0 && Math::toInt(nan, 7) == 7, "Math::toInt: not a number gives what was asked to keep");
    check(Math::toInt(inf) == 1000000000 && Math::toInt(-inf) == -1000000000, "the infinities give the ends of the default range");
    check(Math::toInt(3.0e30f, 0, 0, 10) == 10 && Math::toInt(-3.0e30f, 0, 0, 10) == 0 && Math::toInt(-3.0f, 0, 0, 10) == 0, "and what is outside the range is clamped to it");
    check(Math::toInt(2.9f) == 2 && Math::toInt(-2.9f) == -2 && Math::toInt(7.0f, 0, 0, 10) == 7, "what is inside is cut towards zero, as static_cast<int> does");
}

void testWidgetsMeetNumbersThatAreNotNumbers()
{
    const float nan = 0.0f / 0.0f, inf = 1.0f / 0.0f;
    Harness h(400.0f, 300.0f, false);
    Widget* holder = h.root()->createChild<Widget>();
    holder->setRect(Rect(0.0f, 0.0f, 400.0f, 300.0f));

    // a bar and a speed whose value is not a number (the text of the value is cast to an int)
    PowerBar* power = holder->createChild<PowerBar>(0.0f, 100.0f, nan);
    power->setRect(Rect(10.0f, 10.0f, 200.0f, 30.0f));
    DigitalSpeed* speed = holder->createChild<DigitalSpeed>(nan);
    speed->setRect(Rect(10.0f, 50.0f, 200.0f, 100.0f));
    // the divider of a splitter that has no rectangle (the handle is cast to pixels)
    Splitter* split = holder->createChild<Splitter>(LayoutDir::Horizontal);
    split->setRect(Rect(nan, nan, nan, nan));
    Splitter* vsplit = holder->createChild<Splitter>(LayoutDir::Vertical);
    vsplit->setRect(Rect(inf, 0.0f, nan, 50.0f));
    // the columns of the thumbnail grid and of the asset browser from a width that is not one
    ThumbnailGrid* grid = holder->createChild<ThumbnailGrid>();
    grid->setRect(Rect(220.0f, 10.0f, nan, 100.0f));
    for (int i = 0; i < 5; ++i) grid->addItem(String("t") + String::number(i));
    AssetBrowser* assets = holder->createChild<AssetBrowser>();
    assets->setRect(Rect(220.0f, 120.0f, nan, 100.0f));
    for (int i = 0; i < 5; ++i)
    {
        AssetItem item;
        item.name = String("asset") + String::number(i);
        assets->addItem(item);
    }
    // charts: values and samples that are not numbers (the number of ticks, of segments of a slice, the bin of a sample)
    Chart* bars = holder->createChild<Chart>();
    bars->setRect(Rect(10.0f, 160.0f, 120.0f, 120.0f));
    ct::Vector<String> names;
    ct::Vector<float> values;
    for (int i = 0; i < 4; ++i)
    {
        names.push_back(String("c") + String::number(i));
        values.push_back(i == 1 ? nan : (i == 2 ? inf : static_cast<float>(i)));
    }
    bars->bars(names, values, String("s"));
    Chart* pie = holder->createChild<Chart>();
    pie->setRect(Rect(140.0f, 160.0f, 120.0f, 120.0f));
    pie->pie(names, values);
    Chart* line = holder->createChild<Chart>();
    line->setRect(Rect(270.0f, 160.0f, 120.0f, 120.0f));
    ct::Vector<float> xs, ys;
    for (int i = 0; i < 6; ++i)
    {
        xs.push_back(i == 0 || i == 3 ? nan : static_cast<float>(i));
        ys.push_back(i == 2 ? nan : static_cast<float>(i * i));
    }
    line->plot(xs, ys, String("l"));
    h.relayout();
    h.frames(3);

    // a histogram counts every sample, one that is not a number in the first bin (as it always did), one past the top in the last
    Chart* hist = holder->createChild<Chart>();
    hist->setRect(Rect(270.0f, 10.0f, 120.0f, 100.0f));
    ct::Vector<float> samples;
    for (int i = 0; i < 20; ++i) samples.push_back(static_cast<float>(i % 7));
    samples.push_back(nan);
    samples.push_back(nan);
    const int series = hist->hist(samples, 5, String("h"));
    h.frames(2);
    double total = 0.0;
    for (int i = 0; i < hist->categoryCount(); ++i) total += hist->valueD(series, i);
    check(total == 22.0, "a histogram with samples that are not numbers still counts every sample once");
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testPaintTakesTheBarsRoomFromTheBar);
    RUN(testPianoRollOrdinaryRollDrawsEveryNote);
    RUN(testPianoRollDrawsOnlyNotesThatReachTheView);
    RUN(testPianoRollManyNotesAreDrawnAsWhatTheyCover);
    RUN(testPianoRollManyNotesLookLikeLongNotes);
    RUN(testPianoRollLaneDrawsOneBarForEachColumn);
    RUN(testHandlerDeletesThePianoRoll);
    RUN(testHandlerDeletesTheButton);
    RUN(testHandlerDeletesTheMenu);
    RUN(testHandlerDeletesTheListBox);
    RUN(testHandlerDeletesTheSlider);
    RUN(testToolBarKeepsItsIdsAfterRemovalsAndDrags);
    RUN(testToolBarLoadsFilesWithoutIdsAndWithBadOnes);
    RUN(testToIntCasts);
    RUN(testWidgetsMeetNumbersThatAreNotNumbers);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
