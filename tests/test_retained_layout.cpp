// Layout and geometry under odd sizes: panes of a splitter at the limits, boxes with children
// that ask for NaN, infinite, negative or enormous sizes, windows of one pixel or a hundred
// thousand, sliders with no range, and the software rasteriser fed vertices that are not
// numbers. Driven through the real frame loop (tests/RetainedHarness.hpp).

#include "RetainedHarness.hpp"

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/InputWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/ScrollWidgets.hpp>
#include <igui/widgets/SpinSlider.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui_zen/SoftRaster.hpp>

using namespace rh;

namespace
{

bool finite(float v)
{
    return v == v && v - v == 0.0f;
}

bool near(float a, float b, float eps)
{
    return fabsf(a - b) <= eps;
}

bool rectFinite(const Rect& r)
{
    return finite(r.x) && finite(r.y) && finite(r.w) && finite(r.h);
}

// A widget that asks for any size, and keeps what layout gave it.
class Asker : public Widget
{
public:
    Asker(float w, float h) : w_(w), h_(h) {}
    Vec2f sizeHint() const override { return { w_, h_ }; }
    float w_, h_;
};

// Every rectangle of a tree finite, and (when asked) not negative.
bool treeSane(const Widget* w, bool nonNegative)
{
    if (!rectFinite(w->rect())) return false;
    if (nonNegative && (w->rect().w < 0.0f || w->rect().h < 0.0f)) return false;
    for (size_t i = 0; i < w->children().size(); ++i)
        if (!treeSane(w->children()[i], nonNegative)) return false;
    return true;
}

// ---- scenarios ----

// The handle of a splitter sits between its two panes, wherever the ratio is and however small
// the window: layout keeps each pane at least minSize wide, and the handle was drawn and hit at
// ratio * width, which is somewhere else once that rule has moved a pane.
void testSplitterHandleSitsBetweenThePanes()
{
    printf("the splitter's handle is where its panes meet\n");
    const float ratios[] = { 0.0f, 0.01f, 0.5f, 0.99f, 1.0f };
    for (int vertical = 0; vertical < 2; ++vertical)
        for (size_t ri = 0; ri < sizeof ratios / sizeof ratios[0]; ++ri)
        {
            Harness h(500.0f, 400.0f);
            auto* split = h.root()->createChild<Splitter>(vertical ? LayoutDir::Vertical : LayoutDir::Horizontal);
            Panel* first = split->createChild<Panel>();
            Panel* second = split->createChild<Panel>();
            split->setRatio(ratios[ri]);
            h.relayout();
            const Rect a = first->rect(), b = second->rect();
            const float gap = vertical ? b.y - (a.y + a.h) : b.x - (a.x + a.w);
            check(near(gap, split->handleSize(), 0.01f), "the panes leave the handle's width between them");
            // the handle is drawn as a rectangle of its own colour: find it
            const DrawList& dl = GetDrawList();
            float lo = 1e9f, hi = -1e9f;
            for (size_t i = 0; i < dl.vertices().size(); ++i)
            {
                const DrawVertex& v = dl.vertices()[i];
                if (v.color.r == 65 && v.color.g == 65 && v.color.b == 70 && v.color.a == 255)
                {
                    const float c = vertical ? v.y : v.x;
                    lo = minf(lo, c);
                    hi = maxf(hi, c);
                }
            }
            const Rect sr = split->absoluteRect();
            const float expected = (vertical ? a.y + a.h : a.x + a.w) + (vertical ? sr.y : sr.x);
            check(near(lo, expected, 1.01f), "the handle is drawn right after the first pane");
            (void) hi;
        }
}

void testSplitterNarrowWindowAndDragPastTheEnds()
{
    printf("splitter in a window narrower than its minimum, dragged past both ends\n");
    for (int vertical = 0; vertical < 2; ++vertical)
    {
        Harness h(500.0f, 400.0f);
        auto* split = h.root()->createChild<Splitter>(vertical ? LayoutDir::Vertical : LayoutDir::Horizontal);
        Panel* first = split->createChild<Panel>();
        Panel* second = split->createChild<Panel>();
        h.relayout();
        // a window smaller than two minimum panes and the handle
        const float widths[] = { 500.0f, 80.0f, 40.0f, 5.0f, 1.0f, 0.0f };
        for (size_t i = 0; i < sizeof widths / sizeof widths[0]; ++i)
        {
            h.resize(vertical ? 500.0f : widths[i], vertical ? widths[i] : 400.0f);
            h.relayout();
            check(first->rect().w >= 0.0f && first->rect().h >= 0.0f, "the first pane is not negative");
            check(second->rect().w >= 0.0f && second->rect().h >= 0.0f, "the second pane is not negative");
            check(rectFinite(first->rect()) && rectFinite(second->rect()), "and finite");
        }
        h.resize(500.0f, 400.0f);
        h.relayout();
        // drag the handle across the whole window and far beyond it
        const Rect r = split->absoluteRect();
        const float along = vertical ? r.y : r.x;
        const float handle = along + (vertical ? first->rect().h : first->rect().w) + split->handleSize() * 0.5f;
        const float cross = vertical ? r.x + 50.0f : r.y + 50.0f;
        auto at = [&](float p) { if (vertical) h.moveTo(cross, p); else h.moveTo(p, cross); };
        at(handle);
        h.down();
        const float targets[] = { 0.0f, -1000.0f, 100000.0f, 250.0f, -5.0f, 499.0f, 1e9f };
        for (size_t i = 0; i < sizeof targets / sizeof targets[0]; ++i)
        {
            at(targets[i]);
            check(split->ratio() >= split->minRatio() && split->ratio() <= split->maxRatio(), "the ratio stays in its limits");
            check(finite(split->ratio()), "and is a number");
            const float s = vertical ? first->rect().h : first->rect().w;
            const float total = (vertical ? 400.0f : 500.0f) - split->handleSize();
            check(s >= 0.0f && s <= total + 0.01f, "the first pane stays inside the splitter");
        }
        h.up();
    }
}

void testBoxLayoutOddSizes()
{
    printf("box layouts with children that ask for odd sizes\n");
    const float odd[] = { 0.0f, -5.0f, 1e30f, 1e-30f, nanf(""), INFINITY, -INFINITY, 3.0e9f };
    for (int vertical = 0; vertical < 2; ++vertical)
        for (size_t i = 0; i < sizeof odd / sizeof odd[0]; ++i)
            for (size_t j = 0; j < sizeof odd / sizeof odd[0]; ++j)
            {
                Harness h(400.0f, 300.0f);
                auto* box = h.root()->createChild<BoxLayout>(vertical ? LayoutDir::Vertical : LayoutDir::Horizontal);
                box->setPadding(vertical ? 6.0f : 0.0f);
                box->setSpacing(i % 3 == 0 ? -4.0f : 4.0f);
                Asker* a = box->createChild<Asker>(odd[i], odd[j]);
                Asker* b = box->createChild<Asker>(50.0f, 20.0f);
                Asker* c = box->createChild<Asker>(odd[j], odd[i]);
                b->setStretch(1.0f);
                c->setStretch(i == 4 ? nanf("") : (j == 5 ? -2.0f : 0.5f));
                (void) a;
                h.relayout();
                h.frames(2);
                // Finite sizes asked must give finite rectangles. (A NaN or infinite request
                // cannot: what must hold is that nothing crashes and the others stay finite.)
                if (finite(odd[i]) && finite(odd[j]))
                    check(treeSane(box, false), "finite requests give finite rectangles");
            }
}

void testWindowSizes()
{
    printf("windows of a pixel, nothing and a hundred thousand\n");
    Harness h(800.0f, 600.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->createChild<Label>("Música");
    box->createChild<Button>("OK");
    Slider* s = box->createChild<Slider>(0.0f, 1.0f, 0.5f);
    box->createChild<ProgressBar>(0.0f, 1.0f, 0.5f);
    auto* split = box->createChild<Splitter>(LayoutDir::Horizontal);
    split->createChild<Panel>();
    split->createChild<Panel>();
    h.relayout();
    const float sizes[][2] = { { 1.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 600.0f }, { 800.0f, 0.0f }, { 3.0f, 3.0f },
        { 100000.0f, 100000.0f }, { 1.0f, 100000.0f }, { 800.0f, 600.0f }, { 20000.0f, 20.0f } };
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; ++i)
    {
        h.resize(sizes[i][0], sizes[i][1]);
        h.relayout();
        h.moveTo(sizes[i][0] * 0.5f, sizes[i][1] * 0.5f);
        h.click(sizes[i][0] * 0.25f, sizes[i][1] * 0.25f);
        check(treeSane(box, false), "every rectangle is finite");
    }
    (void) s;
}

// Scroll views and bars with content smaller than the view, empty, NaN and enormous.
void testScrollOddSizes()
{
    printf("scroll bars and views with odd sizes\n");
    Harness h;
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    ScrollBar* bar = box->createChild<ScrollBar>();
    const float odd[] = { 0.0f, -5.0f, 1e30f, nanf(""), INFINITY, 1.0f, 100000.0f };
    h.relayout();
    for (size_t i = 0; i < sizeof odd / sizeof odd[0]; ++i)
        for (size_t j = 0; j < sizeof odd / sizeof odd[0]; ++j)
        {
            bar->setContentSize(odd[i]);
            bar->setViewSize(odd[j]);
            bar->setValue(odd[(i + j) % (sizeof odd / sizeof odd[0])]);
            h.frame();
            const Rect r = bar->absoluteRect();
            h.moveTo(r.x + 2.0f, r.y + 5.0f);
            h.down();
            h.moveTo(r.x + 2.0f, r.y + 500.0f);
            h.up();
        }
    auto* scroll = box->createChild<ScrollView>();
    scroll->setStretch(1.0f);
    for (int i = 0; i < 50; ++i) scroll->createChild<Label>("linha");
    h.relayout();
    for (int i = 0; i < 200; ++i) h.wheel(i % 2 ? 3.0f : -7.0f);
    h.resize(5.0f, 5.0f);
    h.resize(800.0f, 600.0f);
}

} // namespace

// A scenario name on the command line runs only that one.
int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testSplitterHandleSitsBetweenThePanes);
    RUN(testSplitterNarrowWindowAndDragPastTheEnds);
    RUN(testBoxLayoutOddSizes);
    RUN(testWindowSizes);
    RUN(testScrollOddSizes);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
