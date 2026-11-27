// The retained NodeEditor in edit request mode: the editor changes nothing itself and tells
// the application, which may link pins of different types.

#include <igui/widgets/NodeEditor.hpp>
#include <igui/widgets/WidgetApp.hpp>

#include <stdio.h>

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

struct Bench
{
    WidgetApp& app = WidgetApp::instance();
    IO io;
    DrawData data;
    NodeEditor* editor = nullptr;
    int source = -1;
    int target = -1;

    Bench()
    {
        io.displayWidth = 800.0f;
        io.displayHeight = 600.0f;
        io.deltaTime = 1.0f / 60.0f;
        Widget* stage = app.addStage("nodes");
        app.setStage("nodes", TransitionType::None);
        editor = stage->createChild<NodeEditor>();
        editor->setRect({ 0.0f, 0.0f, 700.0f, 500.0f });
        editor->setGridSnap(0.0f);
        source = editor->addNode("Source", 40.0f, 40.0f);
        editor->addPin(source, "Out", PinDir::Output, PinType::Vec3);
        target = editor->addNode("Target", 360.0f, 40.0f);
        editor->addPin(target, "In", PinDir::Input, PinType::Vec4);
        frame();
    }
    ~Bench()
    {
        app.removeStage("nodes");
    }
    void frame()
    {
        app.update(io);
        app.paint(data);
        io.keyEvents.clear();
    }
    void press(float x, float y)
    {
        io.mouseX = x;
        io.mouseY = y;
        frame();
        io.mouseDown[0] = true;
        frame();
    }
    void release(float x, float y)
    {
        io.mouseX = x;
        io.mouseY = y;
        frame();
        io.mouseDown[0] = false;
        frame();
    }
    void drag(float x0, float y0, float x1, float y1)
    {
        press(x0, y0);
        io.mouseX = (x0 + x1) * 0.5f;
        io.mouseY = (y0 + y1) * 0.5f;
        frame();
        release(x1, y1);
    }
};

const float kOutX = 40.0f + 140.0f;
const float kOutY = 40.0f + 24.0f + 10.0f;
const float kInX = 360.0f;
const float kInY = 40.0f + 24.0f + 10.0f;

void testTypesAreTheApplications()
{
    Bench b;
    check(!b.editor->canLink(b.source, 0, b.target, 0), "different types do not link by default");
    b.editor->setEditRequests(true);
    check(b.editor->editRequests(), "the mode is on");
    check(b.editor->canLink(b.source, 0, b.target, 0), "in request mode the type is not checked");
    check(!b.editor->canLink(b.target, 0, b.source, 0), "an input is still no source");
    check(b.editor->addLink(b.source, 0, b.target, 0) >= 0, "the application links the types");
    check(!b.editor->canLink(b.source, 0, b.target, 0), "an input takes one link");
}

void testLinkRequest()
{
    Bench b;
    b.editor->setEditRequests(true);
    int fromNode = -1, fromPin = -1, toNode = -1, toPin = -1;
    b.editor->onLinkRequested.connect(
            [&](int a, int p, int c, int q)
            {
                fromNode = a;
                fromPin = p;
                toNode = c;
                toPin = q;
            });
    b.drag(kOutX, kOutY, kInX, kInY);
    check(fromNode == b.source && fromPin == 0 && toNode == b.target && toPin == 0,
            "a pin dropped on a pin is a request");
    check(b.editor->linkCount() == 0, "the editor made no link itself");
}

void testDeleteRequest()
{
    Bench b;
    b.editor->setEditRequests(true);
    int deleted = -1;
    b.editor->onNodeDeleteRequested.connect([&](int node) { deleted = node; });
    b.press(400.0f, 52.0f);
    b.release(400.0f, 52.0f);
    check(b.editor->selectedNode() == b.target, "a click selects the node");
    IO::KeyEvent e;
    e.key = Key::Delete;
    e.down = true;
    b.io.keyEvents.push_back(e);
    b.frame();
    check(deleted == b.target, "Delete on the selected node is a request");
    check(b.editor->nodeCount() == 2, "the node is still there");
}

void testFitKeepsMinZoom()
{
    Bench b;
    const int far = b.editor->addNode("Far", 3000.0f, 40.0f);
    (void) far;
    b.editor->fitAll(0.8f);
    b.frame();
    check(b.editor->zoom() > 0.79f && b.editor->zoom() < 0.81f, "the zoom stops at the minimum");
    check(b.editor->panX() + 700.0f / b.editor->zoom() > 3140.0f, "the right end is in view");
    b.editor->fitAll();
    b.frame();
    check(b.editor->zoom() < 0.5f, "without a minimum everything is shown");
}

} // namespace

int main()
{
    testTypesAreTheApplications();
    testLinkRequest();
    testDeleteRequest();
    testFitKeepsMinZoom();
    if (gFailures == 0) printf("node editor requests: ok\n");
    return gFailures == 0 ? 0 : 1;
}
