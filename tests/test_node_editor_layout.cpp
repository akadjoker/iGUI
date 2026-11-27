// The retained NodeEditor sizes each node from its texts (title, detail and the widest input
// label plus the widest output label of a row) and draws links as smooth bands that start and end
// exactly on the pins. Driven through the real frame loop with a real font.

#include "RetainedHarness.hpp"

#include <igui/widgets/NodeEditor.hpp>

#include <math.h>

using namespace rh;

namespace
{

void testNodeWidth()
{
    printf("a node is as wide as its texts need\n");
    Harness h(900.0f, 500.0f);
    check(h.ok(), "the font atlas builds");
    NodeEditor* editor = h.root()->createChild<NodeEditor>();
    editor->setRect({ 0.0f, 0.0f, 800.0f, 400.0f });

    const int small = editor->addNode("A", 10.0f, 10.0f);
    editor->addPin(small, "In", PinDir::Input, PinType::Float);
    editor->addPin(small, "Out", PinDir::Output, PinType::Float);

    const int titled = editor->addNode("A Very Long Node Title For Measuring", 10.0f, 100.0f);

    const int pins = editor->addNode("Cubemap Sample", 10.0f, 200.0f);
    editor->addPin(pins, "Cubemap", PinDir::Input, PinType::Float);
    editor->addPin(pins, "Direction", PinDir::Input, PinType::Vec3);
    editor->addPin(pins, "Color", PinDir::Output, PinType::Vec3);
    editor->addPin(pins, "RGBA", PinDir::Output, PinType::Vec4);

    const int wide = editor->addNode("W", 400.0f, 10.0f);
    editor->addPin(wide, "A Very Long Input Label", PinDir::Input, PinType::Float);
    editor->addPin(wide, "A Very Long Output Label", PinDir::Output, PinType::Float);
    h.relayout();
    h.frames(2);

    WidgetApp& app = h.app;
    check(editor->node(small).w == editor->node(small).minW,
            "a small node keeps the minimum width");
    check(editor->node(titled).w >= app.textWidth("A Very Long Node Title For Measuring") + 16.0f,
            "the title fits inside the node");
    check(editor->node(titled).w > editor->node(small).w, "width grows with the title");
    const Node& n = editor->node(pins);
    check(n.w >= app.textWidth("Cubemap") + app.textWidth("Color") + 18.0f,
            "input and output labels of a row do not overlap");
    check(n.w >= app.textWidth("Direction") + app.textWidth("RGBA") + 18.0f,
            "second row does not overlap");
    check(n.w >= app.textWidth("Cubemap Sample") + 16.0f, "title of the pin node fits");
    const Node& w = editor->node(wide);
    check(w.w >= app.textWidth("A Very Long Input Label") +
                            app.textWidth("A Very Long Output Label") + 18.0f,
            "long labels widen the node");

    editor->node(wide).pins[0].name = "x";
    editor->node(wide).pins[1].name = "y";
    h.frames(2);
    check(editor->node(wide).w == editor->node(wide).minW, "width follows the text back down");
}

void testCurve()
{
    printf("the link curve is exact at the pins and smooth\n");
    ct::Vector<Vec2> a;
    buildLinkCurve(100.0f, 50.0f, 400.0f, 200.0f, 1.0f, a);
    check(a.size() >= 8, "enough points");
    check(a[0].x == 100.0f && a[0].y == 50.0f, "starts on the output pin");
    check(a[a.size() - 1].x == 400.0f && a[a.size() - 1].y == 200.0f, "ends on the input pin");

    for (float zoom: { 0.3f, 1.0f, 1.5f, 3.0f })
    {
        ct::Vector<Vec2> c;
        buildLinkCurve(10.0f, 20.0f, 300.0f * zoom, 90.0f * zoom, zoom, c);
        const size_t last = c.size() - 1;
        check(c[0].x == 10.0f && c[0].y == 20.0f, "exact start at any zoom");
        check(c[last].x == 300.0f * zoom && c[last].y == 90.0f * zoom, "exact end at any zoom");
        const float startSlope = fabsf(c[1].y - c[0].y) / (c[1].x - c[0].x);
        const float endSlope = fabsf(c[last].y - c[last - 1].y) / (c[last].x - c[last - 1].x);
        check(c[1].x > c[0].x && startSlope < 0.35f, "horizontal tangent at the start");
        check(c[last].x > c[last - 1].x && endSlope < 0.35f, "horizontal tangent at the end");
        float longest = 0.0f;
        for (size_t i = 1; i < c.size(); ++i)
        {
            const float dx = c[i].x - c[i - 1].x;
            const float dy = c[i].y - c[i - 1].y;
            const float len = sqrtf(dx * dx + dy * dy);
            if (len > longest) longest = len;
        }
        check(longest < 12.0f * (zoom > 1.0f ? zoom : 1.0f), "segments stay short");
    }

    ct::Vector<Vec2> shortCurve, longCurve;
    buildLinkCurve(0.0f, 0.0f, 60.0f, 0.0f, 1.0f, shortCurve);
    buildLinkCurve(0.0f, 0.0f, 1200.0f, 300.0f, 1.0f, longCurve);
    check(longCurve.size() > shortCurve.size(), "longer links get more points");

    ct::Vector<Vec2> backwards;
    buildLinkCurve(500.0f, 100.0f, 100.0f, 160.0f, 1.0f, backwards);
    check(backwards[0].x == 500.0f && backwards[backwards.size() - 1].x == 100.0f,
            "a link that goes backwards keeps its ends");

    check(linkThickness(0.1f) == 1.5f, "thickness has a minimum of 1.5");
    check(linkThickness(1.0f) > 2.0f && linkThickness(1.0f) < 2.5f, "about 2.2 at zoom 1");
    check(linkThickness(1.5f) > linkThickness(1.0f), "thickness grows with the zoom");
    check(linkThickness(100.0f) <= 5.0f, "thickness is capped");
}

void testImageAndError()
{
    printf("a node holds an image under its pins and can be marked as failed\n");
    Harness h(900.0f, 500.0f);
    check(h.ok(), "the font atlas builds");
    NodeEditor* editor = h.root()->createChild<NodeEditor>();
    editor->setRect({ 0.0f, 0.0f, 800.0f, 400.0f });

    const int a = editor->addNode("A", 10.0f, 10.0f);
    editor->addPin(a, "In", PinDir::Input, PinType::Float);
    editor->addPin(a, "Out", PinDir::Output, PinType::Float);
    h.relayout();
    h.frames(2);
    const float plainHeight = editor->node(a).h;
    const float plainWidth = editor->node(a).w;

    editor->setNodeImage(a, handle(7), 180.0f, 96.0f);
    h.frames(2);
    check(editor->node(a).h >= plainHeight + 96.0f, "the node grows by the image height");
    check(editor->node(a).w >= 180.0f + 16.0f, "the node is wide enough for the image");
    check(editor->node(a).w > plainWidth, "a narrow node widens for the image");
    check(editor->node(a).image.value == 7, "the texture is kept");

    editor->setNodeImage(a, handle(0), 96.0f, 96.0f);
    h.frames(2);
    check(editor->node(a).h == plainHeight, "removing the image gives the height back");

    check(editor->node(a).error.empty(), "no error at first");
    editor->setNodeError(a, "undeclared identifier");
    editor->setNodeImage(a, handle(7), 96.0f, 96.0f);
    h.frames(2);
    check(editor->node(a).error == String("undeclared identifier"), "the message is kept");
    editor->setNodeError(a, String());
    check(editor->node(a).error.empty(), "an empty message clears the error");
    editor->setNodeError(a, "again");
    editor->clearNodeErrors();
    check(editor->node(a).error.empty(), "clearNodeErrors clears every node");
    editor->setNodeError(99, "x");
    editor->setNodeImage(-1, handle(1), 1.0f, 1.0f);
    h.frames(1);
}

} // namespace

int main()
{
    testNodeWidth();
    testCurve();
    testImageAndError();
    shutdown();
    if (failures()) printf("%d failure(s)\n", failures());
    else
        printf("node editor layout: all checks passed\n");
    return failures() == 0 ? 0 : 1;
}
