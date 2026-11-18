// Round 6 (docs/ROUND6.md): the scroll bar room that painting reads from the bar and not from the theme, the piano roll's
// notes bounded by the pixels on screen, widgets that delete themselves in their own signals, the list box of the zen_ui
// adapter, the tool bar ids of the serializer, the float casts. Headless: the real frame loop (tests/RetainedHarness.hpp),
// the synthetic input of WidgetApp (feedMouseMove/Button/Wheel) where a mouse is involved, pixels of the software
// rasteriser where a picture is the point. No standard library: ct and C only.

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

#include <unistd.h>

#include <igui/widgets/AudioWidgets.hpp>
#include <igui/widgets/ConsoleWidget.hpp>
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
struct ThemeScope
{
    ThemeScope() : saved(Theme::instance()) {}
    ~ThemeScope()
    {
        Theme::instance() = saved;
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

} // namespace

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testPaintTakesTheBarsRoomFromTheBar);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
