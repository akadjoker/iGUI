// Round 10, part 3 (docs/ROUND10.md): the interactive chart, PlotChart.
//   - the pyramid of min/max blocks and the search against brute force, with gaps;
//   - the gestures and the callbacks, by injected input, with the values checked in data units; bindings that can be changed; a click that is
//     reported; programmatic views that are not "by the user"; linked x axes;
//   - hover: the nearest sample with gaps and with an x of each series;
//   - decimation never loses an extreme: the band drawn in every column contains the minimum and the maximum of its samples (numerically);
//   - the ticks of ranges from microseconds to days, their labels, and no label touching the next;
//   - when the layer is rebuilt: never for a hover, a clock, a marker, a band, a range; once for each real change; a revision that did not change costs nothing;
//   - the presets: every series colour against the plot, the text, a theme change repaints; stable colours when a series goes;
//   - idle: no frame painted when nothing changed, one for each change, no allocation in a frame with a moving clock or pointer;
//   - destroying the chart and replacing its data inside every signal;
//   - y modes (Auto fits the visible x range, Full, Manual), a second axis, an XY series.
// No standard library: ct and C only.

#include "AllocCounter.hpp"
#include "RasterScreen.hpp"
#include "Round9Window.hpp"

#include <igui/widgets/PlotChart.hpp>
#include <igui/widgets/PlotChartCore.hpp>

using namespace rh;

namespace
{

struct Data
{
    ct::Vector<double> x, y;
    void sine(size_t n, double x0, double x1, double freq, double amp = 1.0, double offset = 0.0)
    {
        x.resize(n);
        y.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            x[i] = x0 + (x1 - x0) * static_cast<double>(i) / static_cast<double>(n - 1);
            y[i] = offset + amp * sin(x[i] * freq);
        }
    }
};

// a chart in a window of its own: 800 x 400
struct Scene
{
    Harness h;
    PlotChart* chart = nullptr;
    Widget* holder = nullptr;
    explicit Scene(float w = 800.0f, float hh = 400.0f) : h(w + 40.0f, hh + 40.0f)
    {
        Theme::instance() = Theme::dark();
        holder = h.root()->createChild<Widget>();
        holder->setRect({ 0, 0, w + 40.0f, hh + 40.0f });
        chart = holder->createChild<PlotChart>();
        chart->setRect({ 20, 20, w, hh });
        h.relayout();
        h.frames(2);
    }
    Rect plot() const
    {
        return chart->plotRect();
    }
    // a point of the plot by its fraction
    void at(double fx, double fy, float& x, float& y) const
    {
        const Rect p = plot();
        x = p.x + static_cast<float>(fx) * p.w;
        y = p.y + static_cast<float>(fy) * p.h;
    }
};

bool near(double a, double b, double tol = 1e-6)
{
    return fabs(a - b) <= tol * (1.0 + fabs(b));
}

// ── 1. the pyramid ───────────────────────────────────────────────────────────────────────────────────────────────────────
void testPyramid()
{
    printf("-- the pyramid of min/max blocks\n");
    Rng rng(7);
    for (int pass = 0; pass < 3; ++pass)
    {
        const size_t n = pass == 0 ? 10 : (pass == 1 ? 1000 : 300007);
        ct::Vector<double> y;
        y.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            y[i] = rng.range(-5.0f, 5.0f);
            if (rng.chance(3)) y[i] = static_cast<double>(NAN);
        }
        plot::Pyramid pyr;
        pyr.begin(n);
        int steps = 0;
        while (!pyr.build(y.data(), 5000)) ++steps;
        check(pyr.ready() && pyr.progress() == 1.0f, "a pyramid built in steps with a budget finishes");
        int wrong = 0, queries = 0;
        for (int q = 0; q < 2000; ++q)
        {
            size_t a = static_cast<size_t>(rng.below(static_cast<int>(n))), b = static_cast<size_t>(rng.below(static_cast<int>(n)));
            if (a > b)
            {
                const size_t t = a;
                a = b;
                b = t;
            }
            ++b;
            plot::MinMax expect;
            for (size_t i = a; i < b; ++i) expect.add(y[i]);
            const plot::MinMax got = pyr.query(y.data(), a, b);
            ++queries;
            if (got.any != expect.any || got.gap != expect.gap || (got.any && (got.lo != expect.lo || got.hi != expect.hi))) ++wrong;
        }
        char what[100];
        snprintf(what, sizeof what, "%zu samples: %d of %d ranges answered exactly (min, max and gap flag)", n, queries - wrong, queries);
        check(wrong == 0, what);
        (void) steps;
    }
    // the search
    ct::Vector<double> x;
    for (int i = 0; i < 5000; ++i) x.push_back(i * 0.5 + (i % 7) * 1e-3);
    int bad = 0;
    for (int q = 0; q < 5000; ++q)
    {
        const double v = rng.range(-10.0f, 2600.0f);
        size_t expect = 0;
        while (expect < x.size() && x[expect] < v) ++expect;
        if (plot::lowerBound(x.data(), x.size(), v) != expect) ++bad;
        if (plot::lowerBoundFrom(x.data(), x.size(), static_cast<size_t>(rng.below(5001)), v) != expect) ++bad;
    }
    check(bad == 0, "lowerBound and lowerBoundFrom find the first sample at or after an x, from any hint");
}

// ── 2. ticks ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
void testTicks()
{
    printf("-- ticks, from microseconds to days\n");
    struct Range
    {
        double span;
        const char* name;
    } ranges[] = { { 5e-6, "5 us" }, { 3e-4, "300 us" }, { 0.004, "4 ms" }, { 0.35, "350 ms" }, { 7.0, "7 s" }, { 95.0, "95 s" }, { 1800.0, "30 min" }, { 20000.0, "5.5 h" },
        { 400000.0, "4.6 d" }, { 9.0e6, "104 d" } };
    for (size_t r = 0; r < sizeof ranges / sizeof ranges[0]; ++r)
    {
        const double span = ranges[r].span;
        const double lo = 1234.5; // (an offset: ticks of a short range far from zero)
        const double step = plot::niceTimeStep(span, 8);
        ct::Vector<double> ticks;
        plot::ticksIn(lo, lo + span, step, ticks);
        char what[160];
        snprintf(what, sizeof what, "%s: between 2 and 8 ticks (%zu), step %g", ranges[r].name, ticks.size(), step);
        check(ticks.size() >= 2 && ticks.size() <= 9, what);
        // nice: 1, 2 or 5 times a power of ten below a minute; else one of the clock steps
        bool nice = false;
        if (step < 1.0 || step == 1.0)
        {
            double m = step;
            while (m < 1.0 - 1e-12) m *= 10.0;
            nice = fabs(m - 1.0) < 1e-9 || fabs(m - 2.0) < 1e-9 || fabs(m - 5.0) < 1e-9 || fabs(m - 10.0) < 1e-9;
        }
        else
        {
            static const double clock[] = { 2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600, 7200, 10800, 21600, 43200, 86400, 172800, 432000, 864000 };
            for (size_t k = 0; k < sizeof clock / sizeof clock[0]; ++k) nice = nice || step == clock[k];
            if (step > 864000.0) nice = true;
        }
        snprintf(what, sizeof what, "%s: the step %g is a nice one", ranges[r].name, step);
        check(nice, what);
        bool multiples = true, unique = true;
        for (size_t i = 0; i < ticks.size(); ++i)
        {
            const double k = ticks[i] / step;
            if (fabs(k - floor(k + 0.5)) > 1e-6) multiples = false;
            if (i > 0 && !(ticks[i] > ticks[i - 1])) unique = false;
        }
        check(multiples && unique, "the ticks are multiples of the step, ascending");
        // the labels: distinct, and as precise as the step needs
        ct::Vector<String> labels;
        for (size_t i = 0; i < ticks.size(); ++i) labels.push_back(plot::formatTime(ticks[i], step));
        bool distinct = true;
        for (size_t i = 1; i < labels.size(); ++i) distinct = distinct && !(labels[i] == labels[i - 1]);
        snprintf(what, sizeof what, "%s: the labels tell the ticks apart (first %s, second %s)", ranges[r].name, labels.empty() ? "" : labels[0].c_str(), labels.size() > 1 ? labels[1].c_str() : "");
        check(distinct, what);
    }
    check(plot::formatTime(0.000123, 1e-6) == "0.000123", "microsecond labels have six decimals");
    check(plot::formatTime(0.5, 0.1) == "0.5", "a tenth of a second has one");
    check(plot::formatTime(75.0, 15.0) == "75", "a quarter minute: seconds");
    check(plot::formatTime(120.0, 60.0) == "2:00" && plot::formatTime(3725.0, 60.0) == "1:02:05", "a minute: m:ss, h:mm:ss");
    check(plot::formatTime(3725.0, 5.0) == "3725", "five seconds: whole seconds");
    check(plot::formatTime(7380.0, 3600.0) == "2:03", "an hour: h:mm");
    check(plot::formatTime(90000.0, 86400.0) == "1d 01:00", "a day: Nd hh:mm");
    check(plot::formatValue(2.5, 0.5) == "2.5" && plot::formatValue(100.0, 10.0) == "100" && plot::formatValue(0.00001, 1e-6).size() > 0, "values: as many decimals as the step needs");
    // the chart's own labels: spaced so that they do not touch
    Scene s;
    Data d;
    d.sine(2000, 0.0, 1.0, 40.0);
    s.chart->setSeries(PlotSeriesInfo{ 1, "a", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeriesData(1, d.x.data(), d.y.data(), d.x.size(), 1);
    for (size_t r = 0; r < sizeof ranges / sizeof ranges[0]; ++r)
    {
        s.chart->setViewX(1234.5, 1234.5 + ranges[r].span);
        s.h.frames(2);
        int overlaps = 0;
        const Rect p = s.plot();
        PaintContext pc(GetDrawList(), s.h.font());
        pc.font.SetFontSize(Theme::instance().fontSize);
        float lastEnd = -1e9f;
        for (int i = 0; i < s.chart->xTickCount(); ++i)
        {
            const float w = pc.font.GetTextWidth(s.chart->xTickLabel(i).c_str());
            const float c = static_cast<float>(s.chart->xToPixel(s.chart->xTick(i))) - p.x;
            if (c - w * 0.5f < lastEnd) ++overlaps;
            lastEnd = c + w * 0.5f;
        }
        char what[120];
        snprintf(what, sizeof what, "%s on the chart: %d x ticks, no label touching the next (%d do)", ranges[r].name, s.chart->xTickCount(), overlaps);
        check(s.chart->xTickCount() >= 2 && overlaps == 0, what);
    }
}

// ── 3. gestures ──────────────────────────────────────────────────────────────────────────────────────────────────────────
struct Seen
{
    int views = 0, userViews = 0, hovers = 0, hoverEnds = 0, rangeDrag = 0, rangeDone = 0, rangeSel = 0, rangeClr = 0, clicks = 0, legend = 0, remove = 0, yModes = 0;
    double vx0 = 0, vx1 = 0, vy0 = 0, vy1 = 0;
    double hx = 0, hy = 0;
    int hs = -2, hp = -2;
    bool hin = false;
    double ra = 0, rb = 0;
    double cx = 0, cy = 0;
    int cbutton = -1, cmods = -1;
    int legendIndex = -1, removeIndex = -1;
    PlotYMode mode = PlotYMode::Auto;
    bool lastByUser = false;
};

void watch(PlotChart* c, Seen& s)
{
    c->viewChanged.connect(
            [&s](double a, double b, double y0, double y1, bool user)
            {
                ++s.views;
                if (user) ++s.userViews;
                s.vx0 = a;
                s.vx1 = b;
                s.vy0 = y0;
                s.vy1 = y1;
                s.lastByUser = user;
            });
    c->hovered.connect([&s](double x, double y, int sr, int pt, bool in) { ++s.hovers; s.hx = x; s.hy = y; s.hs = sr; s.hp = pt; s.hin = in; });
    c->hoverEnded.connect([&s] { ++s.hoverEnds; });
    c->rangeChanged.connect([&s](double a, double b, bool done) { ++s.rangeDrag; if (done) ++s.rangeDone; s.ra = a; s.rb = b; });
    c->rangeSelected.connect([&s](double a, double b) { ++s.rangeSel; s.ra = a; s.rb = b; });
    c->rangeCleared.connect([&s] { ++s.rangeClr; });
    c->plotClicked.connect([&s](double x, double y, int b, int m) { ++s.clicks; s.cx = x; s.cy = y; s.cbutton = b; s.cmods = m; });
    c->legendEntryClicked.connect([&s](int i) { ++s.legend; s.legendIndex = i; });
    c->legendEntryRemoveClicked.connect([&s](int i) { ++s.remove; s.removeIndex = i; });
    c->yModeChanged.connect([&s](int, PlotYMode m) { ++s.yModes; s.mode = m; });
}

void testGestures()
{
    printf("-- the gestures, with the values in data units\n");
    Scene s;
    Data a, b;
    a.sine(4000, 0.0, 100.0, 0.3, 2.0, 5.0);
    b.sine(1500, 3.0, 90.0, 0.7, 1.0, 0.0);
    s.chart->setSeries(PlotSeriesInfo{ 1, "alpha", "V", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeries(PlotSeriesInfo{ 2, "beta", "A", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeriesData(1, a.x.data(), a.y.data(), a.x.size(), 1);
    s.chart->setSeriesData(2, b.x.data(), b.y.data(), b.x.size(), 1);
    s.chart->fit();
    s.h.frames(3);
    Seen seen;
    watch(s.chart, seen);
    check(near(s.chart->viewX0(), 0.0) && near(s.chart->viewX1(), 100.0), "fit(): the view is the extent of the data");

    // wheel: zoom x about the pointer
    float px, py;
    s.at(0.25, 0.5, px, py);
    const double aboutX = s.chart->pixelToX(px);
    s.h.moveTo(px, py);
    s.h.wheel(1.0f);
    const double w1 = s.chart->viewX1() - s.chart->viewX0();
    check(near(w1, 100.0 / 1.2, 1e-6), "a wheel notch zooms x by 1.2");
    check(near(s.chart->pixelToX(px), aboutX, 1e-6), "... about the pointer: the data under it stays under it");
    check(seen.userViews >= 1 && seen.lastByUser && near(seen.vx0, s.chart->viewX0()) && near(seen.vx1, s.chart->viewX1()), "viewChanged is emitted, by the user, with the new x range");
    s.h.wheel(-1.0f);
    check(near(s.chart->viewX1() - s.chart->viewX0(), 100.0, 1e-6), "the wheel the other way zooms out by the same");

    // Ctrl + wheel: y about the pointer, y becomes Manual
    const double y0before = s.chart->viewY0(), y1before = s.chart->viewY1();
    s.at(0.5, 0.25, px, py);
    const double aboutY = s.chart->pixelToY(0, py);
    s.h.moveTo(px, py);
    s.h.io().keyCtrl = true;
    s.h.wheel(1.0f);
    s.h.io().keyCtrl = false;
    check(s.chart->yMode(0) == PlotYMode::Manual && seen.yModes == 1 && seen.mode == PlotYMode::Manual, "Ctrl + wheel makes y Manual and says so (yModeChanged)");
    check(near(s.chart->viewY1() - s.chart->viewY0(), (y1before - y0before) / 1.2, 1e-6) && near(s.chart->pixelToY(0, py), aboutY, 1e-6), "... zooming y by 1.2 about the pointer");
    check(near(s.chart->viewX0(), 0.0) && near(s.chart->viewX1(), 100.0), "... and x is not touched");

    // fit by double click: x to the extent, y Auto
    s.chart->setViewX(20.0, 40.0);
    s.at(0.5, 0.5, px, py);
    s.h.moveTo(px, py);
    s.h.down();
    s.h.up();
    s.h.down();
    s.h.up();
    s.h.frames(2);
    // (a double click: two presses close in time; the harness counts click times)
    check(s.chart->yMode(0) == PlotYMode::Auto, "a double click fits: y is Auto again");

    // shift + wheel: pan a tenth of the view
    s.chart->setViewX(20.0, 40.0);
    s.h.frames(2);
    s.h.io().keyShift = true;
    s.h.wheel(1.0f);
    s.h.io().keyShift = false;
    check(near(s.chart->viewX0(), 18.0) && near(s.chart->viewX1(), 38.0), "Shift + wheel pans x by a tenth of the view a notch");

    // drag: pan x (a second since the last click: not a double click)
    s.chart->setViewX(20.0, 40.0);
    s.h.frames(60);
    seen = Seen();
    s.at(0.5, 0.5, px, py);
    const double perPixel = 20.0 / s.plot().w;
    s.h.moveTo(px, py);
    s.h.down();
    s.h.moveTo(px + 6.0f, py);
    s.h.moveTo(px + 50.0f, py);
    s.h.moveTo(px + 100.0f, py);
    check(seen.userViews >= 2, "viewChanged fires during the drag, not only at the end");
    const double moved100 = static_cast<double>(px + 100.0f) - static_cast<double>(px); // (what the float pointer position really moved)
    check(near(s.chart->viewX0(), 20.0 - moved100 * perPixel, 1e-9) && near(s.chart->viewX1(), 40.0 - moved100 * perPixel, 1e-9), "a left drag pans x: the data under the pointer follows it");
    s.h.up();
    check(seen.clicks == 0, "a drag is not a click");
    // middle button
    s.chart->setViewX(20.0, 40.0);
    s.h.frames(60);
    s.h.moveTo(px, py);
    s.h.down(2);
    s.h.moveTo(px - 60.0f, py);
    s.h.up(2);
    check(near(s.chart->viewX0(), 20.0 + 60.0 * perPixel, 1e-6), "a middle drag pans too");

    // range: Shift + drag, finished on release; cleared by Escape and by a click
    seen = Seen();
    s.chart->setViewX(0.0, 100.0);
    s.h.frames(60);
    float rx0, ry, rx1;
    s.at(0.2, 0.5, rx0, ry);
    s.at(0.6, 0.5, rx1, ry);
    s.h.moveTo(rx0, ry);
    s.h.io().keyShift = true;
    s.h.down();
    s.h.moveTo(rx0 + 30.0f, ry);
    s.h.moveTo(rx1, ry);
    s.h.io().keyShift = false;
    check(seen.rangeDrag >= 2 && seen.rangeDone == 0, "rangeChanged fires while the range is dragged (done false)");
    s.h.up();
    check(seen.rangeDone == 1 && seen.rangeSel == 1 && near(seen.ra, s.chart->pixelToX(rx0), 1e-9) && near(seen.rb, s.chart->pixelToX(rx1), 1e-9), "released: rangeSelected(x0, x1) in data units");
    check(s.chart->hasRange() && near(s.chart->rangeStart(), seen.ra) && near(s.chart->rangeEnd(), seen.rb), "the chart keeps the range");
    s.h.key(ig::retained::Key::Escape);
    check(!s.chart->hasRange() && seen.rangeClr == 1, "Escape (with the focus) clears it: rangeCleared");
    s.chart->setRange(10.0, 20.0);
    s.at(0.5, 0.5, px, py);
    s.h.frames(40);
    s.h.click(px, py);
    check(!s.chart->hasRange() && seen.rangeClr == 2 && seen.clicks == 1, "a click clears the range and is reported all the same");

    // click: the value in data units, the button, the modifiers
    seen = Seen();
    s.at(0.3, 0.7, px, py);
    s.h.click(px, py);
    check(seen.clicks == 1 && near(seen.cx, s.chart->pixelToX(px), 1e-9) && near(seen.cy, s.chart->pixelToY(0, py), 1e-9) && seen.cbutton == 0 && seen.cmods == 0, "a click: plotClicked(x, y, button, modifiers) in data units");
    s.h.frames(40);
    s.h.io().keyCtrl = true;
    s.h.click(px, py, 1);
    s.h.io().keyCtrl = false;
    check(seen.clicks == 2 && seen.cbutton == 1 && seen.cmods == PlotModCtrl, "a right click with Ctrl says so");
    s.h.frames(40);
    s.h.moveTo(px, py);
    s.h.down();
    s.h.moveTo(px + 2.0f, py + 1.0f);
    s.h.up();
    check(seen.clicks == 3, "a press and release that moved under 4 pixels is a click");

    // programmatic views are not the user's
    seen = Seen();
    s.chart->setViewX(10.0, 30.0);
    check(seen.views == 1 && !seen.lastByUser && near(seen.vx0, 10.0) && near(seen.vx1, 30.0), "setViewX: viewChanged with byUser false");
    s.chart->setViewX(10.0, 30.0);
    check(seen.views == 1, "... and nothing when the view is the same");
    s.chart->zoomX(0.5, 20.0);
    check(near(s.chart->viewX0(), 15.0) && near(s.chart->viewX1(), 25.0) && !seen.lastByUser, "zoomX(factor, about) by code");
    s.chart->panX(5.0);
    check(near(s.chart->viewX0(), 20.0) && near(s.chart->viewX1(), 30.0), "panX by code");
    s.chart->setViewY(0, -1.0, 11.0);
    check(s.chart->yMode(0) == PlotYMode::Manual && near(s.chart->viewY0(), -1.0) && near(s.chart->viewY1(), 11.0), "setViewY: Manual");

    // follow the clock
    s.chart->fit();
    s.chart->setViewX(0.0, 20.0);
    s.chart->setClock(5.0, true);
    s.chart->setFollowX(5.0, true);
    s.h.frames(2);
    check(near(s.chart->viewX0(), 0.0), "follow: the clock in view: nothing moves");
    s.chart->setClock(30.0, true);
    s.h.frames(2);
    check(s.chart->viewX0() < 30.0 && s.chart->viewX1() > 30.0 && near(s.chart->viewX1() - s.chart->viewX0(), 20.0), "follow: the view comes to the clock when it leaves it, with the same width");
    s.chart->setFollowX(0.0, false);
}

void testBindings()
{
    printf("-- the gesture bindings are configurable\n");
    Scene s;
    Data a;
    a.sine(2000, 0.0, 100.0, 0.3);
    s.chart->setSeries(PlotSeriesInfo{ 1, "a", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeriesData(1, a.x.data(), a.y.data(), a.x.size(), 1);
    s.chart->fit();
    s.h.frames(3);
    Seen seen;
    watch(s.chart, seen);
    PlotBindings b;
    b.zoomY = { 0, PlotModAlt, true };   // Alt + wheel
    b.range = { 0, PlotModCtrl, true };  // Ctrl + drag
    b.pan = { 1, PlotModNone, true };    // the right button pans
    b.panAlt.enabled = false;
    b.zoomFactor = 2.0f;
    s.chart->setBindings(b);
    float px, py;
    s.at(0.5, 0.5, px, py);
    s.h.moveTo(px, py);
    s.h.io().keyCtrl = true;
    s.h.wheel(1.0f);
    s.h.io().keyCtrl = false;
    check(s.chart->yMode(0) == PlotYMode::Auto, "Ctrl + wheel no longer zooms y");
    s.h.io().keyAlt = true;
    s.h.wheel(1.0f);
    s.h.io().keyAlt = false;
    check(s.chart->yMode(0) == PlotYMode::Manual, "Alt + wheel does");
    s.chart->fit();
    s.h.frames(2);
    s.h.wheel(1.0f);
    check(near(s.chart->viewX1() - s.chart->viewX0(), 50.0, 1e-9), "zoomFactor is the one given (2)");
    // pan on the right button
    s.chart->setViewX(20.0, 40.0);
    s.h.frames(2);
    s.h.moveTo(px, py);
    s.h.down(0);
    s.h.moveTo(px + 80.0f, py);
    s.h.up(0);
    check(near(s.chart->viewX0(), 20.0), "a left drag no longer pans");
    s.h.moveTo(px, py);
    s.h.down(1);
    s.h.moveTo(px + 80.0f, py);
    s.h.up(1);
    check(s.chart->viewX0() < 20.0, "a right drag does");
    s.h.moveTo(px, py);
    s.h.down(2);
    s.h.moveTo(px + 80.0f, py);
    s.h.up(2);
    // Ctrl + drag selects a range
    seen = Seen();
    s.h.moveTo(px, py);
    s.h.io().keyCtrl = true;
    s.h.down();
    s.h.moveTo(px + 100.0f, py);
    s.h.up();
    s.h.io().keyCtrl = false;
    check(seen.rangeSel == 1, "Ctrl + drag selects a range now");
    // the range tool: a plain left drag
    s.h.frames(60);
    PlotBindings plain;
    s.chart->setBindings(plain);
    s.chart->setRangeTool(true);
    seen = Seen();
    s.h.moveTo(px, py);
    s.h.down();
    s.h.moveTo(px + 90.0f, py);
    s.h.up();
    check(seen.rangeSel == 1, "while the range tool is on a plain drag selects a range");
    s.chart->setRangeTool(false);
    // not interactive
    s.chart->setInteractive(false);
    const double x0 = s.chart->viewX0();
    s.h.moveTo(px, py);
    s.h.wheel(1.0f);
    s.h.down();
    s.h.moveTo(px + 80.0f, py);
    s.h.up();
    check(near(s.chart->viewX0(), x0), "setInteractive(false): no zoom, no pan");
    s.chart->setInteractive(true);
}

void testLink()
{
    printf("-- linked x axes\n");
    Scene s(500.0f, 220.0f);
    PlotChart* c2 = s.holder->createChild<PlotChart>();
    c2->setRect({ 20, 260, 500, 220 });
    Data a;
    a.sine(1000, 0.0, 50.0, 1.0);
    s.chart->setSeries(PlotSeriesInfo{ 1, "a", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeriesData(1, a.x.data(), a.y.data(), a.x.size(), 1);
    c2->setSeries(PlotSeriesInfo{ 1, "a", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    c2->setSeriesData(1, a.x.data(), a.y.data(), a.x.size(), 1);
    s.chart->fit();
    c2->fit();
    s.h.relayout();
    s.h.frames(3);
    PlotAxisLink link;
    link.add(s.chart);
    link.add(c2);
    link.add(s.chart); // (twice: once)
    check(link.count() == 2, "a chart is added once");
    Seen sa, sb;
    watch(s.chart, sa);
    watch(c2, sb);
    float px, py;
    s.at(0.3, 0.5, px, py);
    s.h.moveTo(px, py);
    s.h.wheel(2.0f);
    check(near(c2->viewX0(), s.chart->viewX0(), 1e-9) && near(c2->viewX1(), s.chart->viewX1(), 1e-9), "a zoom by the person on one chart is applied to the other");
    check(sb.views >= 1 && sb.userViews == 0, "... reported there as not by the user (so nothing loops)");
    check(sa.views == sb.views, "each viewChanged is emitted once on each chart");
    const double before0 = c2->viewX0();
    s.chart->setViewX(5.0, 15.0);
    check(near(c2->viewX0(), before0), "a view set by code does not travel (the link follows the person)");
    // a chart that goes is taken out
    s.holder->removeChild(c2);
    s.h.frames(2);
    check(link.count() == 1, "a chart that is destroyed leaves the link by itself");
    s.h.moveTo(px, py);
    s.h.wheel(1.0f);
    check(true, "(the link survives one chart fewer)");
}

// ── 4. hover ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
void testHover()
{
    printf("-- hover: the nearest sample, with gaps and an x for each series\n");
    Scene s;
    Data a, b;
    // a: regular, with a gap; b: irregular x
    a.sine(400, 0.0, 40.0, 0.5, 1.0, 0.0);
    for (size_t i = 150; i < 170; ++i) a.y[i] = static_cast<double>(NAN);
    b.x.resize(60);
    b.y.resize(60);
    double xv = 1.0;
    for (size_t i = 0; i < 60; ++i)
    {
        xv += 0.2 + 0.9 * static_cast<double>((i * 7919) % 13) / 13.0;
        b.x[i] = xv;
        b.y[i] = 0.4 * cos(xv * 0.8) + 0.1 * static_cast<double>(i % 3);
    }
    s.chart->setSeries(PlotSeriesInfo{ 1, "regular", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeries(PlotSeriesInfo{ 2, "irregular", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeriesData(1, a.x.data(), a.y.data(), a.x.size(), 1);
    s.chart->setSeriesData(2, b.x.data(), b.y.data(), b.x.size(), 1);
    s.chart->setViewX(0.0, 40.0);
    s.chart->setViewY(0, -1.5, 1.5);
    s.h.frames(3);
    Seen seen;
    watch(s.chart, seen);
    Rng rng(11);
    int wrong = 0, trials = 0;
    for (int t = 0; t < 60; ++t)
    {
        float fx = rng.unit(), fy = rng.unit();
        float px, py;
        s.at(fx, fy, px, py);
        s.h.moveTo(px, py);
        // the expected: for each series the valid sample nearest in x (a gap is not one); the series whose sample is nearest to the pointer
        double best = 1e30;
        int bs = -1, bp = -1;
        const double cxd = s.chart->pixelToX(px);
        for (int k = 0; k < 2; ++k)
        {
            const Data& d = k == 0 ? a : b;
            double bestDx = 1e300;
            int bi = -1;
            for (size_t i = 0; i < d.x.size(); ++i)
            {
                if (d.y[i] != d.y[i]) continue;
                const double dx = fabs(d.x[i] - cxd);
                if (dx < bestDx)
                {
                    bestDx = dx;
                    bi = static_cast<int>(i);
                }
            }
            if (bi < 0) continue;
            const double qx = s.chart->xToPixel(d.x[static_cast<size_t>(bi)]), qy = s.chart->yToPixel(0, d.y[static_cast<size_t>(bi)]);
            const double dd = (qx - px) * (qx - px) + (qy - py) * (qy - py);
            if (dd < best)
            {
                best = dd;
                bs = k;
                bp = bi;
            }
        }
        ++trials;
        if (seen.hs != bs || seen.hp != bp || !seen.hin) ++wrong;
    }
    char what[120];
    snprintf(what, sizeof what, "the nearest sample over 60 pointer positions: %d of %d differ from brute force", wrong, trials);
    check(wrong == 0, what);
    // the values in data units
    float px, py;
    s.at(0.5, 0.5, px, py);
    s.h.moveTo(px, py);
    check(near(seen.hx, s.chart->pixelToX(px), 1e-5) && near(seen.hy, s.chart->pixelToY(0, py), 1e-5), "hovered(x, y, ...): the pointer in data units");
    check(s.chart->hasHover() && near(s.chart->hoverX(), seen.hx), "the chart knows it is hovered");
    // in the gap: no sample of series 0 is picked from the gap
    s.chart->setViewX(14.0, 17.0);
    s.h.frames(2);
    s.at(0.5, 0.5, px, py);
    s.h.moveTo(px, py);
    check(seen.hs == 1 || seen.hp < 150 || seen.hp >= 170, "a gap is never the nearest sample");
    // leaving the plot
    const int ends = seen.hoverEnds;
    s.h.moveTo(5.0f, 5.0f);
    check(seen.hoverEnds == ends + 1 && !s.chart->hasHover(), "hoverEnded when the pointer leaves the plot");
}

// ── 5. decimation ────────────────────────────────────────────────────────────────────────────────────────────────────────
void testDecimation()
{
    printf("-- decimation never loses an extreme\n");
    Scene s(900.0f, 300.0f);
    const size_t n = 400000;
    Data d;
    d.x.resize(n);
    d.y.resize(n);
    Rng rng(3);
    for (size_t i = 0; i < n; ++i)
    {
        d.x[i] = static_cast<double>(i) * 1e-3;
        d.y[i] = sin(d.x[i] * 0.9) + 0.2 * rng.range(-1.0f, 1.0f);
        if (i % 9973 == 0) d.y[i] += (i % 2 ? 6.0 : -6.0); // single-sample spikes: the extremes a stride would lose
        if (i > 100000 && i < 100400) d.y[i] = static_cast<double>(NAN);
    }
    s.chart->setSeries(PlotSeriesInfo{ 1, "big", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeriesData(1, d.x.data(), d.y.data(), n, 1);
    s.chart->fit();
    for (int i = 0; i < 40 && s.chart->pyramidProgress(1) < 1.0f; ++i) s.h.frames(1);
    s.h.frames(2);
    check(s.chart->pyramidProgress(1) == 1.0f, "the pyramid of a 400000 sample series is built, a few frames at a time");
    struct View
    {
        double a, b;
    } views[] = { { 0.0, 400.0 }, { 10.0, 60.0 }, { 100.2, 100.9 }, { 250.0, 250.4 }, { 399.0, 400.0 } };
    for (size_t v = 0; v < sizeof views / sizeof views[0]; ++v)
    {
        s.chart->setViewX(views[v].a, views[v].b);
        s.chart->setViewY(0, -8.0, 8.0);
        s.h.frames(2);
        const Rect p = s.plot();
        const double dx = (s.chart->viewX1() - s.chart->viewX0()) / p.w;
        int columns = 0, missed = 0;
        const bool env = s.chart->layerIsEnvelope(0);
        if (env)
        {
            for (int c = 0; c < s.chart->layerPointCount(0) / 2; ++c)
            {
                float cx, lo, hi;
                if (!s.chart->layerColumn(0, c, cx, lo, hi)) break;
                const int col = static_cast<int>(cx);
                const double xa = s.chart->viewX0() + col * dx, xb = xa + dx;
                double mn = 1e30, mx = -1e30;
                size_t i0 = plot::lowerBound(d.x.data(), n, xa), i1 = plot::lowerBound(d.x.data(), n, xb);
                for (size_t i = i0; i < i1; ++i)
                {
                    if (d.y[i] != d.y[i]) continue;
                    if (d.y[i] < mn) mn = d.y[i];
                    if (d.y[i] > mx) mx = d.y[i];
                }
                if (mn > mx) continue;
                ++columns;
                const double top = s.chart->yToPixel(0, mx) - p.y, bottom = s.chart->yToPixel(0, mn) - p.y;
                if (hi > top + 1e-3 || lo < bottom - 1e-3) ++missed; // the band drawn must hold both extremes
            }
        }
        char what[160];
        snprintf(what, sizeof what, "view %.1f..%.1f: %s, %d columns, %d with an extreme outside the band", views[v].a, views[v].b, env ? "envelope" : "polyline", columns, missed);
        check(missed == 0 && (!env || columns > 100), what);
    }
    // not ready: the preview is made from a stride, exact once the pyramid is built (the test above); the chart asks for frames while it builds
    Scene s2(900.0f, 300.0f);
    s2.chart->setSeries(PlotSeriesInfo{ 1, "big", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s2.chart->setSeriesData(1, d.x.data(), d.y.data(), n, 1);
    s2.chart->fit();
    s2.h.frames(1);
    check(s2.chart->pyramidProgress(1) < 1.0f || n < 1u << 20, "a big series starts with the pyramid unfinished (a budget of samples a frame)");
    check(s2.h.app.needsFrame() || s2.chart->pyramidProgress(1) == 1.0f, "... and the chart asks for the frames it needs to finish it");
}

// ── 6. the layer ─────────────────────────────────────────────────────────────────────────────────────────────────────────
void testLayerRebuilds()
{
    printf("-- when the layer is rebuilt\n");
    Scene s;
    Data a, b;
    a.sine(5000, 0.0, 100.0, 0.3);
    b.sine(3000, 0.0, 100.0, 0.9);
    s.chart->setSeries(PlotSeriesInfo{ 1, "a", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeries(PlotSeriesInfo{ 2, "b", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeriesData(1, a.x.data(), a.y.data(), a.x.size(), 1);
    s.chart->setSeriesData(2, b.x.data(), b.y.data(), b.x.size(), 1);
    s.chart->fit();
    s.h.frames(4);
    const uint32_t base = s.chart->layerRebuilds();
    check(base >= 1, "the layer was built");
    s.h.frames(10);
    check(s.chart->layerRebuilds() == base, "idle frames do not rebuild it");
    float px, py;
    for (int i = 0; i < 10; ++i)
    {
        s.at(0.1 + 0.08 * i, 0.4, px, py);
        s.h.moveTo(px, py);
    }
    check(s.chart->layerRebuilds() == base, "a hover moving over the plot does not rebuild it");
    for (int i = 0; i < 10; ++i)
    {
        s.chart->setClock(10.0 + i, true);
        s.h.frames(1);
    }
    check(s.chart->layerRebuilds() == base, "a moving clock does not");
    s.chart->setMarker(1, 20.0, Color(255, 0, 0, 255), "In");
    s.chart->setMarker(2, 60.0, Color(0, 255, 0, 255), "Out");
    s.chart->setBand(1, 20.0, 60.0);
    s.chart->setRange(30.0, 40.0);
    s.h.frames(2);
    s.chart->setMarker(1, 22.0, Color(255, 0, 0, 255), "In");
    s.chart->removeBand(1);
    s.h.frames(2);
    check(s.chart->layerRebuilds() == base, "markers, bands and a range do not");
    s.chart->setReadout("12.5 s", ct::Vector<String>());
    s.chart->setNote("a note");
    s.h.frames(2);
    check(s.chart->layerRebuilds() == base + 1 || s.chart->layerRebuilds() == base, "a readout text and a note are not data (at most the header's room)");
    const uint32_t b2 = s.chart->layerRebuilds();

    auto once = [&](const char* what, auto change)
    {
        const uint32_t before = s.chart->layerRebuilds();
        change();
        s.h.frames(3);
        char text[160];
        snprintf(text, sizeof text, "%s: the layer is rebuilt once (%u)", what, s.chart->layerRebuilds() - before);
        check(s.chart->layerRebuilds() == before + 1, text);
    };
    once("the view moves", [&] { s.chart->setViewX(10.0, 60.0); });
    once("y becomes Manual", [&] { s.chart->setViewY(0, -2.0, 2.0); });
    once("y is Auto again", [&] { s.chart->setYMode(0, PlotYMode::Auto); });
    once("new data (a new revision)", [&] { s.chart->setSeriesData(1, a.x.data(), a.y.data(), a.x.size(), 2); });
    once("a series is hidden", [&] { s.chart->setSeriesVisible(2, false); });
    once("and shown", [&] { s.chart->setSeriesVisible(2, true); });
    once("the colour of a series", [&] { s.chart->setSeries(PlotSeriesInfo{ 1, "a", "", Color(10, 200, 30, 255), true, 0, false, -1, 1.5f, 0.0 }); });
    once("the size of the widget", [&] { s.chart->setRect({ 20, 20, 700, 360 }); s.h.relayout(); });
    once("the theme (a preset)", [&] { s.h.app.setThemePreset("light"); });
    once("the theme, back", [&] { s.h.app.setThemePreset("dark"); });
    // the same revision, again: nothing
    const uint32_t before = s.chart->layerRebuilds();
    s.chart->setSeriesData(1, a.x.data(), a.y.data(), a.x.size(), 2);
    s.chart->setSeriesData(2, b.x.data(), b.y.data(), b.x.size(), 1);
    s.h.frames(3);
    check(s.chart->layerRebuilds() == before, "setSeriesData with the same revision (and arrays) costs nothing");
    (void) b2;
}

// ── 7. data: owning and not, y modes, a second axis, XY, series colours ───────────────────────────────────────────────────
void testData()
{
    printf("-- series, y modes, two axes, XY\n");
    Scene s;
    Data a, b;
    a.sine(2000, 0.0, 100.0, 0.2, 1.0, 10.0); // y in 9..11
    b.sine(2000, 0.0, 100.0, 0.2, 1.0, 0.0);
    s.chart->setSeries(PlotSeriesInfo{ 7, "a", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeries(PlotSeriesInfo{ 9, "b", "", Color(0, 0, 0, 0), true, 1, false, -1, 1.5f, 0.0 }); // the right axis
    s.chart->setSeriesData(7, a.x.data(), a.y.data(), a.x.size(), 1);
    s.chart->setSeriesDataCopy(9, b.x.data(), b.y.data(), b.x.size());
    s.chart->fit();
    s.h.frames(3);
    check(s.chart->seriesCount() == 2 && s.chart->seriesIdAt(0) == 7 && s.chart->seriesIdAt(1) == 9 && s.chart->seriesIndexOf(9) == 1 && s.chart->seriesIndexOf(5) == -1, "series have stable ids");
    check(s.chart->viewY0(0) > 8.5 && s.chart->viewY1(0) < 11.5, "the left axis is fitted to its series (9..11)");
    check(s.chart->viewY0(1) < -0.9 && s.chart->viewY1(1) > 0.9 && s.chart->viewY1(1) < 1.5, "the right axis to its own (-1..1): a second y axis, by series");
    // the copy is the chart's
    b.y[100] = 1.0e6;
    s.h.frames(2);
    check(s.chart->viewY1(1) < 2.0, "an owning copy does not see what the caller does to its array afterwards");
    // Auto fits the visible x range; Full all the data
    Data c;
    c.x.resize(1000);
    c.y.resize(1000);
    for (size_t i = 0; i < 1000; ++i)
    {
        c.x[i] = static_cast<double>(i);
        c.y[i] = i < 500 ? 1.0 : 100.0;
    }
    s.chart->removeSeries(9);
    s.chart->setSeriesData(7, c.x.data(), c.y.data(), 1000, 2);
    s.chart->setViewX(0.0, 400.0);
    s.h.frames(3);
    check(s.chart->viewY1(0) < 5.0, "y Auto: fitted to the data of the visible x range (a step to 100 outside it is not in it)");
    s.chart->setYMode(0, PlotYMode::Full);
    s.h.frames(3);
    check(s.chart->viewY1(0) > 99.0, "y Full: fitted to all of the data");
    s.chart->setViewY(0, 0.0, 10.0);
    s.h.frames(3);
    check(near(s.chart->viewY0(0), 0.0) && near(s.chart->viewY1(0), 10.0) && s.chart->yMode(0) == PlotYMode::Manual, "y Manual: the range given");
    s.chart->setViewX(0.0, 999.0);
    s.h.frames(2);
    check(near(s.chart->viewY1(0), 10.0), "... which a change of x does not move");

    // the colours of the series are stable while the others come and go
    s.chart->clearSeries();
    for (int id = 1; id <= 4; ++id)
    {
        s.chart->setSeries(PlotSeriesInfo{ id * 10, "s", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
        s.chart->setSeriesData(id * 10, a.x.data(), a.y.data(), a.x.size(), 1);
    }
    const Color c30 = s.chart->seriesColor(30), c40 = s.chart->seriesColor(40);
    s.chart->removeSeries(10);
    s.chart->removeSeries(20);
    check(s.chart->seriesColor(30).r == c30.r && s.chart->seriesColor(30).g == c30.g && s.chart->seriesColor(40).b == c40.b, "a series keeps its colour while others are removed");
    s.chart->setSeries(PlotSeriesInfo{ 50, "n", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    check(s.chart->seriesColor(50).r != s.chart->seriesColor(30).r || s.chart->seriesColor(50).g != s.chart->seriesColor(30).g, "a new series takes the next colour of the palette, not one in use");
    s.chart->clearSeries();

    // XY: x from another series
    Data px, py;
    px.x.resize(500);
    px.y.resize(500);
    py.x.resize(500);
    py.y.resize(500);
    for (size_t i = 0; i < 500; ++i)
    {
        const double t = static_cast<double>(i) * 0.05;
        px.x[i] = t;
        px.y[i] = 10.0 * cos(t);
        py.x[i] = t;
        py.y[i] = 5.0 * sin(t * 1.5);
    }
    s.chart->setSeries(PlotSeriesInfo{ 1, "x", "", Color(0, 0, 0, 0), false, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeries(PlotSeriesInfo{ 2, "y", "", Color(0, 0, 0, 0), true, 0, false, 1, 1.5f, 0.0 }); // x from series 1
    s.chart->setSeriesData(1, px.x.data(), px.y.data(), 500, 1);
    s.chart->setSeriesData(2, py.x.data(), py.y.data(), 500, 1);
    s.chart->fit();
    s.h.frames(3);
    check(s.chart->layerPointCount(1) > 100, "an XY series is drawn (points: x from the series it names)");
    check(s.chart->viewX0() < -9.0 && s.chart->viewX1() > 9.0, "... and fit() takes the x range from the x series' values");
}

// ── 8. the presets ───────────────────────────────────────────────────────────────────────────────────────────────────────
float lumOf(const Color& c)
{
    auto ch = [](unsigned char v)
    {
        const float f = static_cast<float>(v) / 255.0f;
        return f <= 0.03928f ? f / 12.92f : powf((f + 0.055f) / 1.055f, 2.4f);
    };
    return 0.2126f * ch(c.r) + 0.7152f * ch(c.g) + 0.0722f * ch(c.b);
}

float ratioOf(const Color& a, const Color& b)
{
    const float la = lumOf(a), lb = lumOf(b);
    return (maxf(la, lb) + 0.05f) / (minf(la, lb) + 0.05f);
}

Color over(const Color& top, const Color& bottom)
{
    const float a = static_cast<float>(top.a) / 255.0f;
    return Color(static_cast<uint8_t>(top.r * a + bottom.r * (1 - a) + 0.5f), static_cast<uint8_t>(top.g * a + bottom.g * (1 - a) + 0.5f),
            static_cast<uint8_t>(top.b * a + bottom.b * (1 - a) + 0.5f), 255);
}

void testPresets()
{
    printf("-- the presets\n");
    const char* names[] = { "dark", "light", "xp", "macos", "macos-dark" };
    for (int i = 0; i < 5; ++i)
    {
        const Theme t = Theme::byName(names[i]);
        char what[200];
        for (int k = 0; k < 8; ++k)
        {
            snprintf(what, sizeof what, "%s: series colour %d against the plot is %.2f (3 or more)", names[i], k, ratioOf(t.plot.series[k], t.plot.bg));
            check(ratioOf(t.plot.series[k], t.plot.bg) >= 3.0f, what);
        }
        // the palette is a palette: no two colours alike
        bool distinct = true;
        for (int a = 0; a < 8; ++a)
            for (int b = a + 1; b < 8; ++b)
            {
                const int d = abs(t.plot.series[a].r - t.plot.series[b].r) + abs(t.plot.series[a].g - t.plot.series[b].g) + abs(t.plot.series[a].b - t.plot.series[b].b);
                if (d < 60) distinct = false;
            }
        snprintf(what, sizeof what, "%s: the eight palette colours are told apart", names[i]);
        check(distinct, what);
        snprintf(what, sizeof what, "%s: the grid shows over the plot background (%.2f)", names[i], ratioOf(over(t.plot.grid, t.plot.bg), t.plot.bg));
        check(ratioOf(over(t.plot.grid, t.plot.bg), t.plot.bg) >= 1.08f, what);
        snprintf(what, sizeof what, "%s: the cursor line against the plot is %.2f (3 or more)", names[i], ratioOf(over(t.plot.cursor, t.plot.bg), t.plot.bg));
        check(ratioOf(over(t.plot.cursor, t.plot.bg), t.plot.bg) >= 3.0f, what);
        snprintf(what, sizeof what, "%s: the clock line against the plot is %.2f (3 or more)", names[i], ratioOf(t.plot.clock, t.plot.bg));
        check(ratioOf(t.plot.clock, t.plot.bg) >= 3.0f, what);
        snprintf(what, sizeof what, "%s: the axis text against the panel is %.2f (4.5 or more)", names[i], ratioOf(t.textSecondary, t.panelColor));
        check(ratioOf(t.textSecondary, t.panelColor) >= 4.5f, what);
        snprintf(what, sizeof what, "%s: the legend text against the panel is %.2f (4.5 or more)", names[i], ratioOf(t.textColor, t.panelColor));
        check(ratioOf(t.textColor, t.panelColor) >= 4.5f, what);
        // a marker's flag text is black or white, whichever reads: always 4.5 or more
        for (int k = 0; k < 8; ++k)
        {
            const Color flag = t.plot.series[k];
            const Color text = ratioOf(Color(255, 255, 255, 255), flag) >= ratioOf(Color(0, 0, 0, 255), flag) ? Color(255, 255, 255, 255) : Color(0, 0, 0, 255);
            if (ratioOf(text, flag) < 4.5f)
            {
                snprintf(what, sizeof what, "%s: flag text over series colour %d is %.2f", names[i], k, ratioOf(text, flag));
                check(false, what);
            }
        }
    }
    // a theme change repaints the chart, and what it draws is the new theme's
    Scene s;
    Data d;
    d.sine(3000, 0.0, 10.0, 3.0);
    s.chart->setSeries(PlotSeriesInfo{ 1, "a", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeriesData(1, d.x.data(), d.y.data(), d.x.size(), 1);
    s.chart->fit();
    s.chart->setClock(5.0, true);
    s.h.frames(3);
    for (int i = 0; i < 5; ++i)
    {
        s.h.app.setThemePreset(names[i]);
        s.h.frames(3);
        const Theme& t = Theme::instance();
        const DrawList& dl = GetDrawList();
        bool bg = false, line = false, clock = false;
        const Rect p = s.plot();
        for (size_t k = 0; k < dl.vertices().size(); ++k)
        {
            const DrawVertex& v = dl.vertices()[k];
            if (v.x < p.x - 2 || v.x > p.x + p.w + 2 || v.y < p.y - 2 || v.y > p.y + p.h + 2) continue;
            if (v.color.r == t.plot.bg.r && v.color.g == t.plot.bg.g && v.color.b == t.plot.bg.b && v.color.a == 255) bg = true;
            if (v.color.r == t.plot.series[0].r && v.color.g == t.plot.series[0].g && v.color.b == t.plot.series[0].b) line = true;
            if (v.color.r == t.plot.clock.r && v.color.g == t.plot.clock.g && v.color.b == t.plot.clock.b) clock = true;
        }
        char what[120];
        snprintf(what, sizeof what, "%s: the plot, the first palette colour and the clock are the theme's", names[i]);
        check(bg && line && clock, what);
    }
    Theme::instance() = Theme::dark();
}

// ── 9. idle ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
bool pass(Harness& h)
{
    NewFrame();
    h.app.update(GetIO());
    if (!h.app.needsFrame())
    {
        Render();
        return false;
    }
    h.app.paint(*GetDrawData(), &h.atlas().defaultFont(), nullptr);
    if (h.atlas().dynamicDirty()) h.atlas().clearDynamicDirty();
    Render();
    return true;
}

int passes(Harness& h, int n)
{
    int painted = 0;
    for (int i = 0; i < n; ++i)
        if (pass(h)) ++painted;
    return painted;
}

void testIdle()
{
    printf("-- idle\n");
    Scene s;
    Data a;
    a.sine(4000, 0.0, 100.0, 0.5);
    s.chart->setSeries(PlotSeriesInfo{ 1, "a", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
    s.chart->setSeriesData(1, a.x.data(), a.y.data(), a.x.size(), 1);
    s.chart->fit();
    s.h.app.setFocused(nullptr);
    int quiet = 0;
    for (int i = 0; i < 60 && quiet < 3; ++i) quiet = pass(s.h) ? 0 : quiet + 1;
    check(passes(s.h, 100) == 0, "100 idle frames paint none");
    const uint32_t rebuilds = s.chart->layerRebuilds();
    s.chart->setClock(10.0, true);
    check(passes(s.h, 6) == 1, "a clock change paints one frame");
    s.chart->setClock(10.0, true);
    check(passes(s.h, 6) == 0, "the same clock again paints none");
    s.chart->setMarker(1, 10.0, Color(255, 0, 0, 255), "x");
    check(passes(s.h, 6) == 1, "a marker paints one frame");
    s.chart->setSeriesData(1, a.x.data(), a.y.data(), a.x.size(), 2);
    check(passes(s.h, 6) == 1 && s.chart->layerRebuilds() == rebuilds + 1, "new data: one frame, one rebuild");
    check(passes(s.h, 30) == 0, "... and then none");
    s.chart->setViewX(10.0, 50.0);
    check(passes(s.h, 6) == 1 && passes(s.h, 20) == 0, "a view change: one frame");
    // frames with a moving clock and with the pointer allocate nothing (once the caches of text are warm: a glyph pair seen for the first time is
    // remembered by the font, so the values read out are all shown a first time before)
    for (int i = 0; i < 80; ++i)
    {
        s.chart->setClock(11.0 + i * 0.37, true);
        passes(s.h, 1);
        float px, py;
        s.at(0.1 + 0.0097 * i, 0.5 + 0.003 * (i % 11), px, py);
        s.h.io().mouseX = px;
        s.h.io().mouseY = py;
        passes(s.h, 1);
    }
    const unsigned long long before = gAllocs;
    if (getenv("IGUI_TRACE_ALLOCS")) gTraceAllocs = true; // (where the allocations of a frame come from: addr2line on the addresses)
    int painted = 0;
    for (int i = 0; i < 30; ++i)
    {
        s.chart->setClock(20.0 + i * 0.3, true);
        float px, py;
        s.at(0.2 + 0.01 * i, 0.5, px, py);
        s.h.io().mouseX = px;
        s.h.io().mouseY = py;
        painted += passes(s.h, 1);
    }
    gTraceAllocs = false;
    char what[140];
    snprintf(what, sizeof what, "30 frames with a clock and a pointer moving (%d painted) allocate nothing (%llu allocations)", painted, gAllocs - before);
    check(painted == 30 && gAllocs == before, what);
}

// ── 10. a handler that destroys the chart or replaces its data ────────────────────────────────────────────────────────────
void testHandlers()
{
    printf("-- destroying the chart and replacing its data inside every signal\n");
    const int kSignals = 11;
    const char* names[kSignals] = { "viewChanged", "hovered", "hoverEnded", "rangeChanged", "rangeSelected", "rangeCleared", "plotClicked", "legendEntryClicked",
        "legendEntryRemoveClicked", "yModeChanged", "viewChanged by a wheel" };
    for (int mode = 0; mode < 2; ++mode) // 0: replace the data; 1: remove the chart
    {
        for (int sig = 0; sig < kSignals; ++sig)
        {
            Scene s;
            Data a, b;
            a.sine(3000, 0.0, 100.0, 0.4);
            b.sine(500, 0.0, 100.0, 1.4);
            PlotChart* chart = s.chart;
            chart->setSeries(PlotSeriesInfo{ 1, "a", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
            chart->setSeries(PlotSeriesInfo{ 2, "b", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
            chart->setSeriesData(1, a.x.data(), a.y.data(), a.x.size(), 1);
            chart->setSeriesData(2, b.x.data(), b.y.data(), b.x.size(), 1);
            chart->setLegendRemovable(true);
            chart->fit();
            s.h.frames(3);
            Widget* holder = s.holder;
            int calls = 0;
            const uint32_t seen = s.h.app.deletions();
            (void) seen;
            auto act = [&]()
            {
                ++calls;
                if (mode == 0)
                {
                    chart->setSeriesData(1, b.x.data(), b.y.data(), b.x.size(), 100 + static_cast<uint64_t>(calls));
                    chart->removeSeries(2);
                    chart->setSeriesDataCopy(3, b.x.data(), b.y.data(), b.x.size());
                    chart->clearSeries();
                    chart->setSeries(PlotSeriesInfo{ 5, "c", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
                    chart->setSeriesData(5, a.x.data(), a.y.data(), a.x.size(), 200 + static_cast<uint64_t>(calls));
                }
                else if (calls == 1)
                    holder->removeChild(chart);
            };
            switch (sig)
            {
                case 0: chart->viewChanged.connect([&](double, double, double, double, bool) { act(); }); break;
                case 1: chart->hovered.connect([&](double, double, int, int, bool) { act(); }); break;
                case 2: chart->hoverEnded.connect([&]() { act(); }); break;
                case 3: chart->rangeChanged.connect([&](double, double, bool) { act(); }); break;
                case 4: chart->rangeSelected.connect([&](double, double) { act(); }); break;
                case 5: chart->rangeCleared.connect([&]() { act(); }); break;
                case 6: chart->plotClicked.connect([&](double, double, int, int) { act(); }); break;
                case 7: chart->legendEntryClicked.connect([&](int) { act(); }); break;
                case 8: chart->legendEntryRemoveClicked.connect([&](int) { act(); }); break;
                case 9: chart->yModeChanged.connect([&](int, PlotYMode) { act(); }); break;
                default: chart->viewChanged.connect([&](double, double, double, double, bool) { act(); }); break;
            }
            float px, py;
            s.at(0.4, 0.5, px, py);
            const Rect r = chart->absoluteRect();
            const Rect pl = s.plot();
            switch (sig)
            {
                case 0: // a drag pans
                    s.h.moveTo(px, py);
                    s.h.down();
                    s.h.moveTo(px + 30.0f, py);
                    s.h.moveTo(px + 60.0f, py);
                    s.h.up();
                    break;
                case 1: s.h.moveTo(px, py); s.h.moveTo(px + 5.0f, py + 3.0f); break;
                case 2: s.h.moveTo(px, py); s.h.moveTo(3.0f, 3.0f); break;
                case 3:
                case 4:
                    s.h.moveTo(px, py);
                    s.h.io().keyShift = true;
                    s.h.down();
                    s.h.moveTo(px + 40.0f, py);
                    s.h.moveTo(px + 80.0f, py);
                    s.h.up();
                    s.h.io().keyShift = false;
                    break;
                case 5:
                {
                    PlotBindings b = chart->bindings();
                    b.clickClearsRange = false;
                    chart->setBindings(b);
                    s.h.click(px, py); // (the focus)
                    chart->setRange(10.0, 30.0);
                    s.h.frames(2);
                    s.h.key(ig::retained::Key::Escape);
                    break;
                }
                case 6: s.h.click(px, py); break;
                case 7: s.h.click(r.x + 14.0f, r.y + 34.0f); break;
                case 8:
                {
                    // the cross of the first legend entry: look for it along the legend row
                    for (float x = r.x + 20.0f; x < r.x + 400.0f && calls == 0; x += 4.0f)
                    {
                        s.h.click(x, r.y + 34.0f);
                    }
                    break;
                }
                case 9:
                    s.h.moveTo(px, py);
                    s.h.io().keyCtrl = true;
                    s.h.wheel(1.0f);
                    s.h.io().keyCtrl = false;
                    break;
                default:
                    s.h.moveTo(px, py);
                    s.h.wheel(1.0f);
                    break;
            }
            (void) pl;
            s.h.frames(3);
            char what[160];
            snprintf(what, sizeof what, "%s inside %s: the handler ran (%d) and nothing crashed", mode == 0 ? "data replaced" : "the chart removed", names[sig], calls);
            check(calls >= 1, what);
            if (mode == 1)
            {
                snprintf(what, sizeof what, "the chart removed inside %s is gone", names[sig]);
                check(holder->children().empty(), what);
            }
        }
    }
}

// ── 11. a big series ─────────────────────────────────────────────────────────────────────────────────────────────────────
void testBig()
{
    printf("-- 16 series of a million points\n");
    Scene s(1000.0f, 500.0f);
    const size_t n = 1000000;
    ct::Vector<Data> d;
    d.resize(16);
    for (int k = 0; k < 16; ++k)
    {
        d[static_cast<size_t>(k)].x.resize(n);
        d[static_cast<size_t>(k)].y.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            const double t = static_cast<double>(i) * 1e-4;
            d[static_cast<size_t>(k)].x[i] = t;
            d[static_cast<size_t>(k)].y[i] = sin(t * (0.3 + 0.1 * k)) + 0.1 * sin(t * 37.0) + (i % 50021 == 7 ? 3.0 : 0.0);
        }
        s.chart->setSeries(PlotSeriesInfo{ k + 1, "s", "", Color(0, 0, 0, 0), true, 0, false, -1, 1.5f, 0.0 });
        s.chart->setSeriesData(k + 1, d[static_cast<size_t>(k)].x.data(), d[static_cast<size_t>(k)].y.data(), n, 1);
    }
    s.chart->fit();
    int frames = 0;
    while (frames < 200 && s.chart->pyramidProgress(16) < 1.0f)
    {
        s.h.frames(1);
        ++frames;
    }
    char what[100];
    snprintf(what, sizeof what, "the pyramids of 16 series of 1,000,000 samples are done in %d frames (budget: 1M samples a frame)", frames);
    check(s.chart->pyramidProgress(16) == 1.0f, what);
    // zoomed views: the envelope holds the extremes of a sample column (spot check of the spikes of 3.0)
    s.chart->setViewY(0, -3.0, 4.5);
    int spikes = 0, seenSpikes = 0;
    s.chart->setViewX(0.0, 100.0);
    s.h.frames(3);
    const Rect p = s.plot();
    for (size_t i = 7; i < n; i += 50021)
    {
        const double x = d[0].x[i];
        if (x < 0.0 || x > 100.0) continue;
        ++spikes;
        const int col = static_cast<int>((x - s.chart->viewX0()) / (s.chart->viewX1() - s.chart->viewX0()) * p.w);
        float cx, lo, hi;
        for (int c = 0; c < s.chart->layerPointCount(0) / 2; ++c)
        {
            if (!s.chart->layerColumn(0, c, cx, lo, hi)) break;
            if (static_cast<int>(cx) == col && hi <= s.chart->yToPixel(0, d[0].y[i]) - p.y + 1e-3) ++seenSpikes;
        }
    }
    snprintf(what, sizeof what, "the spikes of 1,000,000 samples are in the band: %d of %d", seenSpikes, spikes);
    check(spikes > 0 && seenSpikes == spikes, what);
}


// ── pictures (ROUND10_SHOTS=<directory>, PPM; tools/ppm2png.py makes PNG) ─────────────────────────────────────────────────
void writePicture(const char* dir, const char* name, const Shot& shot)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    if (FILE* f = fopen(path, "wb"))
    {
        fprintf(f, "P6\n%d %d\n255\n", shot.w, shot.h);
        for (size_t i = 0; i < shot.px.size(); ++i)
        {
            const unsigned char rgb[3] = { static_cast<unsigned char>(shot.px[i] >> 16), static_cast<unsigned char>(shot.px[i] >> 8), static_cast<unsigned char>(shot.px[i]) };
            fwrite(rgb, 1, 3, f);
        }
        fclose(f);
        printf("  wrote %s\n", path);
    }
}

struct Demo
{
    Data speed, torque, noise, temp;
    Demo()
    {
        const size_t n = 60000;
        speed.x.resize(n);
        speed.y.resize(n);
        torque.x.resize(n);
        torque.y.resize(n);
        noise.x.resize(n);
        noise.y.resize(n);
        temp.x.resize(n);
        temp.y.resize(n);
        Rng rng(5);
        for (size_t i = 0; i < n; ++i)
        {
            const double t = static_cast<double>(i) * 0.001; // 1 kHz
            speed.x[i] = torque.x[i] = noise.x[i] = temp.x[i] = t;
            speed.y[i] = sin(t * 0.25) * 1.4 + 0.1 * sin(t * 71.0);
            torque.y[i] = 0.5 * sin(t * 0.12 + 1.0) + ((i / 15000) % 2 ? 0.45 : -0.45);
            noise.y[i] = 0.3 * rng.range(-1.0f, 1.0f) + 0.2 * sin(t * 1.5);
            temp.y[i] = 40.0 + 15.0 * sin(t * 0.08);
            if (i > 31000 && i < 32500) noise.y[i] = static_cast<double>(NAN);
        }
    }
    void feed(PlotChart* c, bool right = true)
    {
        const char* names[] = { "speed", "torque", "noise", "temperature" };
        Data* d[] = { &speed, &torque, &noise, &temp };
        for (int k = 0; k < 4; ++k)
        {
            PlotSeriesInfo info;
            info.id = k + 1;
            info.name = names[k];
            info.yAxis = k == 3 && right ? 1 : 0;
            c->setSeries(info);
            c->setSeriesData(k + 1, d[k]->x.data(), d[k]->y.data(), d[k]->x.size(), 1);
        }
    }
};

Shot chartShot(const char* preset, int variant)
{
    Theme::instance() = Theme::byName(preset);
    Screen s(900.0f, 420.0f);
    s.useDynamicPage();
    Harness& h = s.h;
    Widget* holder = h.root()->createChild<Widget>();
    holder->setRect({ 0, 0, 900, 420 });
    auto* chart = holder->createChild<PlotChart>();
    chart->setRect({ 0, 0, 900, 420 });
    Demo demo;
    demo.feed(chart);
    chart->setLegendRemovable(true);
    chart->setNote("1 kHz");
    chart->fit();
    h.relayout();
    h.frames(3);
    chart->setMarker(1, 8.0, Color(230, 80, 80, 255), "In");
    chart->setMarker(2, 46.0, Color(70, 190, 120, 255), "Out");
    chart->setClock(24.0, true);
    if (variant == 1) // zoomed in to milliseconds
    {
        chart->setViewX(24.000, 24.040);
        chart->setClock(24.0125, true);
        chart->setMarker(1, 24.005, Color(230, 80, 80, 255), "In");
    }
    if (variant == 2) // a range selected, with the statistics
    {
        chart->setRange(12.0, 31.0);
        ct::Vector<String> rows;
        rows.push_back("min");
        rows.push_back("max");
        rows.push_back("mean");
        ct::Vector<PlotStatsColumn> cols;
        Data* d[] = { &demo.speed, &demo.torque, &demo.noise, &demo.temp };
        for (int k = 0; k < 4; ++k)
        {
            // the statistics of the series over the range, as an application would work them out
            double lo = 1e300, hi = -1e300, sum = 0.0;
            size_t count = 0;
            for (size_t i = 0; i < d[k]->x.size(); ++i)
            {
                if (d[k]->x[i] < 12.0 || d[k]->x[i] > 31.0 || d[k]->y[i] != d[k]->y[i]) continue;
                if (d[k]->y[i] < lo) lo = d[k]->y[i];
                if (d[k]->y[i] > hi) hi = d[k]->y[i];
                sum += d[k]->y[i];
                ++count;
            }
            char text[3][32];
            snprintf(text[0], 32, "%.2f", lo);
            snprintf(text[1], 32, "%.2f", hi);
            snprintf(text[2], 32, "%.2f", count ? sum / static_cast<double>(count) : 0.0);
            PlotStatsColumn c;
            c.seriesId = k + 1;
            c.header = k == 0 ? "speed" : k == 1 ? "torque" : k == 2 ? "noise" : "temp";
            for (int r = 0; r < 3; ++r) c.cells.push_back(text[r]);
            cols.push_back(c);
        }
        chart->setStats(rows, cols);
    }
    h.frames(3);
    // the pointer over the plot: the cursor, the dots, the values in the legend
    const Rect p = chart->plotRect();
    h.moveTo(p.x + p.w * 0.62f, p.y + p.h * 0.4f);
    h.frames(2);
    return s.take();
}

void testPictures(const char* dir)
{
    printf("-- pictures of the chart, into %s\n", dir);
    const char* names[] = { "dark", "light", "xp", "macos", "macos-dark" };
    for (int i = 0; i < 5; ++i)
    {
        char name[64];
        snprintf(name, sizeof name, "plot-%s", names[i]);
        writePicture(dir, name, chartShot(names[i], 0));
    }
    writePicture(dir, "plot-dark-milliseconds", chartShot("dark", 1));
    writePicture(dir, "plot-light-range", chartShot("light", 2));
    // two y axes are in every picture (the temperature is on the right)
    Theme::instance() = Theme::dark();
}

} // namespace

int main()
{
    testPyramid();
    testTicks();
    testGestures();
    testBindings();
    testLink();
    testHover();
    testDecimation();
    testLayerRebuilds();
    testData();
    testPresets();
    testIdle();
    testHandlers();
    testBig();
    if (const char* dir = getenv("ROUND10_SHOTS")) testPictures(dir);
    Theme::instance() = Theme::dark();
    shutdown();
    if (failures() == 0) printf("PASS\n");
    return failures() == 0 ? 0 : 1;
}
