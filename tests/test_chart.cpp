// The retained Chart: pie, donut and bars, their legend, labels and hit
// testing, driven through WidgetApp the way a window does.

#include <igui/widgets/ChartWidget.hpp>
#include <igui/widgets/WidgetApp.hpp>

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace
{

using namespace ig::retained;

int gFailures = 0;

void check(bool condition, const char* what)
{
    if (!condition)
    {
        ++gFailures;
        printf("FAIL %s\n", what);
    }
}

ct::Vector<String> labels(const char* a, const char* b, const char* c, const char* d = nullptr)
{
    ct::Vector<String> v;
    v.push_back(String(a));
    v.push_back(String(b));
    v.push_back(String(c));
    if (d)
        v.push_back(String(d));
    return v;
}

ct::Vector<float> values(float a, float b)
{
    ct::Vector<float> v;
    v.push_back(a);
    v.push_back(b);
    return v;
}

ct::Vector<float> values(float a, float b, float c)
{
    ct::Vector<float> v = values(a, b);
    v.push_back(c);
    return v;
}

ct::Vector<float> values(float a, float b, float c, float d)
{
    ct::Vector<float> v = values(a, b, c);
    v.push_back(d);
    return v;
}

bool near(float a, float b, float eps = 0.5f)
{
    return fabsf(a - b) <= eps;
}

// A stage with one chart, pumped like a window.
struct Bench
{
    WidgetApp& app = WidgetApp::instance();
    IO io;
    DrawData data;
    Widget* stage = nullptr;
    Chart* chart = nullptr;

    Bench(float w = 480.0f, float h = 320.0f)
    {
        io.displayWidth = 800.0f;
        io.displayHeight = 600.0f;
        io.deltaTime = 1.0f / 60.0f;
        stage = app.addStage("chart");
        app.setStage("chart", TransitionType::None);
        chart = stage->createChild<Chart>();
        chart->setRect({ 10.0f, 20.0f, w, h });
    }
    ~Bench()
    {
        app.removeStage("chart");
    }
    void frame()
    {
        app.update(io);
        app.paint(data);
    }
    void move(float localX, float localY)
    {
        io.mouseX = 10.0f + localX;
        io.mouseY = 20.0f + localY;
        frame();
    }
    void click(float localX, float localY)
    {
        move(localX, localY);
        io.mouseDown[0] = true;
        frame();
        io.mouseDown[0] = false;
        frame();
    }
};

void testData()
{
    Bench b;
    Chart& c = *b.chart;
    c.pie(labels("a", "b", "c"), values(1.0f, 2.0f, 3.0f));
    check(c.kind() == ChartKind::Pie && c.categoryCount() == 3 && c.seriesCount() == 1, "pie() sets the kind, the labels and one series");
    check(near(c.total(0), 6.0f, 1e-4f), "the total adds the values");
    check(c.colorOf(0, 0).r == 78 && c.colorOf(0, 1).r == 242, "categories take the palette in order");
    c.setCategoryVisible(1, false);
    check(near(c.total(0), 4.0f, 1e-4f), "a hidden category leaves the total");
    c.bars(labels("x", "y", "z"), values(5.0f, -2.0f, 7.0f), String("sales"));
    check(c.kind() == ChartKind::Bars && c.seriesCount() == 1 && c.series(0).name == "sales", "bars() sets the kind and names the series");
    check(c.categoryVisible(1), "bars() starts with every category shown");
    const int second = c.addSeries(String("costs"), values(1.0f, 2.0f));
    check(second == 1 && near(c.value(1, 2), 0.0f, 1e-6f), "a short series reads 0 past its end");
    ct::Vector<float> longer;
    for (int i = 0; i < 5; ++i)
        longer.push_back(1.0f);
    c.addSeries(String("more"), longer);
    check(c.categoryCount() == 5 && c.categories()[4] == "5", "values past the labels get numbered labels");
    c.removeSeries(0);
    check(c.seriesCount() == 2 && c.series(0).name == "costs", "removeSeries drops the one asked");
    c.clear();
    check(c.seriesCount() == 0 && c.categoryCount() == 0, "clear empties both");

    char buf[32];
    c.formatValue(3.0f, buf, sizeof(buf));
    check(strcmp(buf, "3") == 0, "a whole value prints without decimals");
    c.formatValue(3.14159f, buf, sizeof(buf));
    check(strcmp(buf, "3.14") == 0, "a small value prints two decimals");
    c.formatValue(42.5f, buf, sizeof(buf));
    check(strcmp(buf, "42.5") == 0, "a medium value prints one decimal");
    c.setValueSuffix(String("%"));
    c.setValueDecimals(1);
    c.formatValue(7.0f, buf, sizeof(buf));
    check(strcmp(buf, "7.0%") == 0, "the suffix and fixed decimals apply");
}

void testPieGeometry()
{
    Bench b;
    Chart& c = *b.chart;
    c.pie(labels("a", "b", "c", "d"), values(25.0f, 25.0f, 25.0f, 25.0f));
    c.setLegendPosition(LegendPosition::Right);
    b.frame();
    const Widget::Vec2f center = c.pieCenter();
    const float r = c.pieRadius();
    check(r > 40.0f, "the pie has a radius worth looking at");
    check(c.plotRect().right() <= c.legendEntryRect(0).x, "the legend sits right of the plot");
    check(c.legendEntryCount() == 4, "the pie legend has one entry per category");

    // Quarter slices clockwise from the top: a at the right-top, b right-bottom, c left-bottom, d left-top.
    const float m = r * 0.6f;
    check(c.hitTest(center.x + m * 0.7f, center.y - m * 0.7f).index == 0, "the first slice is top right");
    check(c.hitTest(center.x + m * 0.7f, center.y + m * 0.7f).index == 1, "the second slice is bottom right");
    check(c.hitTest(center.x - m * 0.7f, center.y + m * 0.7f).index == 2, "the third slice is bottom left");
    check(c.hitTest(center.x - m * 0.7f, center.y - m * 0.7f).index == 3, "the fourth slice is top left");
    check(!c.hitTest(center.x + r + 20.0f, center.y).valid(), "outside the rim is nothing");
    check(c.hitTest(center.x, center.y).valid(), "a pie's centre belongs to a slice");

    c.setKind(ChartKind::Donut);
    b.frame();
    check(!c.hitTest(c.pieCenter().x, c.pieCenter().y).valid(), "a donut's hole is nothing");
    check(c.hitTest(c.pieCenter().x + c.pieRadius() * 0.85f, c.pieCenter().y - 1.0f).index == 0, "the ring still hits");

    // Hidden categories drop out of the slices and the hit test.
    c.setCategoryVisible(0, false);
    b.frame();
    const Chart::Item hit = c.hitTest(c.pieCenter().x + c.pieRadius() * 0.85f, c.pieCenter().y - 1.0f);
    check(hit.index == 1, "with the first category hidden the second starts at the top");
    c.setCategoryVisible(0, true);

    // Values of zero take no slice.
    c.pie(labels("a", "b", "c"), values(0.0f, 10.0f, 0.0f));
    b.frame();
    check(c.hitTest(c.pieCenter().x - c.pieRadius() * 0.5f, c.pieCenter().y).index == 1, "a full circle for the only value");

    // No data paints without a slice and hits nothing.
    c.clear();
    b.frame();
    check(!c.hitTest(c.pieCenter().x, c.pieCenter().y).valid(), "an empty pie hits nothing");
}

void testBarsGeometry()
{
    Bench b;
    Chart& c = *b.chart;
    c.bars(labels("jan", "feb", "mar"), values(12.0f, 30.0f, 7.0f));
    b.frame();
    check(near(c.axisMin(), 0.0f, 1e-4f) && near(c.axisMax(), 30.0f, 1e-4f) && near(c.axisStep(), 5.0f, 1e-4f), "a 1-2-5 axis: 0..30 by 5");
    check(c.legendEntryCount() == 0, "a single unnamed series has no legend");
    const Rect r0 = c.barRect(0, 0);
    const Rect r1 = c.barRect(0, 1);
    const Rect r2 = c.barRect(0, 2);
    check(r0.w > 10.0f && near(r0.w, r1.w) && near(r1.w, r2.w), "bars share a width");
    check(r0.x < r1.x && r1.x < r2.x, "bars follow the categories left to right");
    check(near(r1.h / r0.h, 30.0f / 12.0f, 0.05f), "bar heights follow the values");
    check(near(r0.bottom(), r1.bottom()) && near(r1.bottom(), r2.bottom()), "bars share the zero line");
    check(c.hitTest(r1.x + r1.w * 0.5f, r1.y + r1.h * 0.5f).index == 1, "the tallest bar is under its rect");
    check(!c.hitTest(r1.x + r1.w * 0.5f, r1.y - 4.0f).valid(), "above a bar is nothing");

    // Negative values: the axis includes them and the bar hangs below zero.
    c.setSeriesValues(0, values(12.0f, -8.0f, 7.0f));
    b.frame();
    check(c.axisMin() < 0.0f, "the axis reaches the negative value");
    const Rect neg = c.barRect(0, 1);
    const Rect pos = c.barRect(0, 0);
    check(near(neg.y, pos.bottom()), "a negative bar starts at the zero line");

    // Two named series: a legend, grouped bars, legend press hides one.
    c.bars(labels("q1", "q2", "q3"), values(10.0f, 20.0f, 30.0f), String("2024"));
    c.addSeries(String("2025"), values(15.0f, 25.0f, 5.0f));
    b.frame();
    check(c.legendEntryCount() == 2, "two series make two legend entries");
    const Rect a = c.barRect(0, 0);
    const Rect bb = c.barRect(1, 0);
    check(a.right() <= bb.x + 0.01f && bb.x - a.right() < 6.0f, "the series sit side by side in the group");
    int toggled = -1;
    c.legendToggled.connect([&](int entry) { toggled = entry; });
    const Rect entry = c.legendEntryRect(1);
    b.click(entry.x + 4.0f, entry.y + entry.h * 0.5f);
    check(toggled == 1 && !c.series(1).visible, "pressing a legend entry hides its series");
    check(c.barRect(1, 0).w == 0.0f, "a hidden series has no bars");
    check(c.barRect(0, 0).w > a.w, "the remaining series takes the group width");
    b.click(entry.x + 4.0f, entry.y + entry.h * 0.5f);
    check(c.series(1).visible, "pressing again shows it");
}

void testHoverAndPress()
{
    Bench b;
    Chart& c = *b.chart;
    c.bars(labels("a", "b", "c"), values(1.0f, 2.0f, 3.0f));
    b.frame();
    int hoverCount = 0;
    Chart::Item last;
    c.hoverChanged.connect([&](Chart::Item item) { ++hoverCount; last = item; });
    int pressedSeries = -1, pressedIndex = -1;
    c.itemPressed.connect([&](int s, int i) { pressedSeries = s; pressedIndex = i; });

    const Rect r2 = c.barRect(0, 2);
    b.move(r2.x + r2.w * 0.5f, r2.y + r2.h * 0.5f);
    check(hoverCount == 1 && last.index == 2 && c.hoveredItem().index == 2, "moving onto a bar hovers it");
    b.move(r2.x + r2.w * 0.5f, r2.y + r2.h * 0.6f);
    check(hoverCount == 1, "moving inside the same bar changes nothing");
    b.click(r2.x + r2.w * 0.5f, r2.y + r2.h * 0.6f);
    check(pressedSeries == 0 && pressedIndex == 2, "pressing a bar reports it");
    b.move(r2.x + r2.w * 0.5f, r2.y - 10.0f);
    check(!c.hoveredItem().valid(), "leaving the bar clears the hover");
    b.move(-30.0f, -30.0f);
    check(!c.hoveredItem().valid(), "leaving the widget clears the hover");

    // The same on a pie: the slice under the mouse.
    c.pie(labels("a", "b", "c"), values(50.0f, 25.0f, 25.0f));
    b.frame();
    const Widget::Vec2f center = c.pieCenter();
    b.move(center.x + c.pieRadius() * 0.5f, center.y);
    check(c.hoveredItem().index == 0, "the half slice on the right hovers");
    b.click(center.x + c.pieRadius() * 0.5f, center.y);
    check(pressedIndex == 0, "pressing a slice reports it");
    const Rect entry = c.legendEntryRect(0);
    b.click(entry.x + 4.0f, entry.y + entry.h * 0.5f);
    check(!c.categoryVisible(0), "pressing a pie legend entry hides the category");
    b.frame();
    check(c.hitTest(center.x + c.pieRadius() * 0.5f, center.y).index != 0, "the hidden category has no slice");
}

void testLegendPlacement()
{
    {
        Bench wide(480.0f, 240.0f);
        wide.chart->pie(labels("a", "b", "c"), values(1.0f, 1.0f, 1.0f));
        wide.frame();
        const Rect e = wide.chart->legendEntryRect(0);
        check(e.x > 240.0f, "a wide chart puts the legend at the right");
    }
    {
        Bench tall(240.0f, 320.0f);
        tall.chart->pie(labels("a", "b", "c"), values(1.0f, 1.0f, 1.0f));
        tall.frame();
        const Rect e = tall.chart->legendEntryRect(0);
        check(e.y > 240.0f, "a tall chart puts the legend below");
        check(tall.chart->plotRect().bottom() <= e.y, "the plot stops above the legend");
        tall.chart->setShowLegend(false);
        tall.frame();
        check(tall.chart->legendEntryCount() == 0, "the legend can be hidden");
        check(tall.chart->plotRect().h > 300.0f - 20.0f, "without a legend the plot takes the height");
    }
    {
        Bench titled;
        titled.chart->setTitle(String("Sales"));
        titled.chart->bars(labels("a", "b", "c"), values(1.0f, 2.0f, 3.0f));
        titled.frame();
        check(titled.chart->plotRect().y > 20.0f, "a title takes a line above the plot");
    }
}

void testManyThinSlices()
{
    // Thirty slices with labels: the outside labels must not crash and the
    // hit test must still find each slice.
    Bench b;
    ct::Vector<String> names;
    ct::Vector<float> vals;
    char buf[16];
    for (int i = 0; i < 30; ++i)
    {
        snprintf(buf, sizeof(buf), "item %d", i);
        names.push_back(String(buf));
        vals.push_back(1.0f + static_cast<float>(i % 3));
    }
    b.chart->pie(names, vals);
    b.chart->setShowValues(true);
    b.frame();
    int found = 0;
    const Widget::Vec2f center = b.chart->pieCenter();
    const float r = b.chart->pieRadius();
    for (int k = 0; k < 360; k += 3)
    {
        const float a = static_cast<float>(k) * 3.14159265f / 180.0f;
        if (b.chart->hitTest(center.x + sinf(a) * r * 0.7f, center.y - cosf(a) * r * 0.7f).valid())
            ++found;
    }
    check(found == 120, "every angle lands on a slice");
    b.chart->setKind(ChartKind::Bars);
    b.frame();
    check(b.chart->barRect(0, 29).w >= 1.0f, "thirty bars still have a width");
}

} // namespace

int main()
{
    testData();
    testPieGeometry();
    testBarsGeometry();
    testHoverAndPress();
    testLegendPlacement();
    testManyThinSlices();
    if (gFailures == 0)
        printf("chart tests passed\n");
    return gFailures == 0 ? 0 : 1;
}
