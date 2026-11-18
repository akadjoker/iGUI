// Round 4, second half (docs/ROUND4B.md), the parts that are not menus: the synthetic input API of WidgetApp, the
// SideTabBar and ToolBar with an application's own icons, the timeline's bars and beats, the piano roll's velocities and
// note signals, the chart's double data and axis titles, the slider's readout. Headless: the real frame loop
// (tests/RetainedHarness.hpp), pixels of the software rasteriser where a picture is the point. No standard library: ct and C
// only.

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/TextInputWidgets.hpp>

using namespace rh;

namespace
{

// ---- item 16: synthetic input ----

// Records what reaches it.
class InputProbe : public Widget
{
public:
    IG_WIDGET_CLASS(InputProbe, Widget);

    InputProbe()
    {
        acceptsFocus_ = true;
    }
    struct Press
    {
        int button;
        bool shift, ctrl, alt;
        int clicks;
        float x, y;
    };
    ct::Vector<Press> presses;
    ct::Vector<int> releases;
    int moves = 0;
    float lastX = -1.0f, lastY = -1.0f;
    float scrollX = 0.0f, scrollY = 0.0f;
    int scrolls = 0;
    struct Key
    {
        int key;
        bool down, shift, ctrl, alt;
    };
    ct::Vector<Key> keys;
    String typed;

    void onMousePress(MouseEvent& e) override
    {
        presses.push_back({ e.button, e.shift, e.ctrl, e.alt, e.clickCount, e.x, e.y });
        e.consumed = true;
    }
    void onMouseRelease(MouseEvent& e) override
    {
        releases.push_back(e.button);
        e.consumed = true;
    }
    void onMouseMove(MouseEvent& e) override
    {
        ++moves;
        lastX = e.x;
        lastY = e.y;
    }
    void onMouseScroll(MouseEvent& e) override
    {
        ++scrolls;
        scrollX += e.scrollX;
        scrollY += e.scrollY;
        e.consumed = true;
    }
    void onKeyPress(KeyEvent& e) override
    {
        keys.push_back({ e.key, true, e.shift, e.ctrl, e.alt });
        e.consumed = true;
    }
    void onKeyRelease(KeyEvent& e) override
    {
        keys.push_back({ e.key, false, e.shift, e.ctrl, e.alt });
        e.consumed = true;
    }
    void onTextInput(KeyEvent& e) override
    {
        typed += String(e.text);
        e.consumed = true;
    }
};

void testSyntheticInput()
{
    Harness h(420.0f, 300.0f, false);
    WidgetApp& app = WidgetApp::instance();
    InputProbe* probe = h.root()->createChild<InputProbe>();
    probe->setRect(Rect(40.0f, 40.0f, 200.0f, 120.0f));
    Button* button = h.root()->createChild<Button>("Click");
    button->setRect(Rect(260.0f, 40.0f, 100.0f, 30.0f));
    int clicks = 0;
    button->clicked.connect([&]() { ++clicks; });
    h.frame(); // one frame lays the window out; the events below come with no frame in between

    // the pointer: a move reaches the widget under it, at the point given
    app.feedMouseMove(100.0f, 80.0f);
    check(probe->moves == 1 && probe->lastX == 100.0f && probe->lastY == 80.0f, "feedMouseMove moves the pointer: the widget under it hears it");
    check(probe->isHovered(), "and is hovered");
    check(app.mouseX() == 100.0f && app.mouseY() == 80.0f, "the application's pointer is there");

    // a click: press and release of the left button, where the pointer is, with no frame between them
    app.feedMouseButton(0, true);
    check(probe->presses.size() == 1 && probe->presses[0].button == 0 && probe->presses[0].x == 100.0f && probe->presses[0].y == 80.0f &&
                    !probe->presses[0].shift && !probe->presses[0].ctrl && !probe->presses[0].alt && probe->presses[0].clicks == 1,
            "feedMouseButton(0, true) presses the left button where the pointer is");
    check(probe->releases.size() == 0, "and nothing is released yet");
    app.feedMouseButton(0, false);
    check(probe->releases.size() == 1 && probe->releases[0] == 0, "feedMouseButton(0, false) releases it");
    // pressing what is down does nothing, as in update()
    app.feedMouseButton(0, false);
    app.feedMouseButton(1, true);
    app.feedMouseButton(1, true);
    app.feedMouseButton(1, false);
    check(probe->presses.size() == 2 && probe->presses[1].button == 1 && probe->releases.size() == 2 && probe->releases[1] == 1,
            "a release of a button that is not down, and a second press of one that is, do nothing; button 1 is the right one");
    // modifiers go with the event
    app.feedMouseButton(2, true, true, true, false);
    app.feedMouseButton(2, false);
    check(probe->presses.size() == 3 && probe->presses[2].button == 2 && probe->presses[2].shift && probe->presses[2].ctrl && !probe->presses[2].alt,
            "the modifiers given are those of the press (and button 2 is the middle one)");
    // out of range buttons are ignored
    app.feedMouseButton(7, true);
    app.feedMouseButton(-1, true);
    check(probe->presses.size() == 3, "a button that does not exist is ignored");
    // time does not pass: two clicks on the same spot, with no frame between, are a double click
    app.feedMouseButton(0, true);
    app.feedMouseButton(0, false);
    app.feedMouseButton(0, true);
    app.feedMouseButton(0, false);
    check(probe->presses.size() == 5 && probe->presses[4].clicks == 2, "no time passes in a feed: the second click is a double click");
    h.frames(30); // half a second of frames
    app.feedMouseButton(0, true);
    app.feedMouseButton(0, false);
    check(probe->presses.size() == 6 && probe->presses[5].clicks == 1, "a click after the frames is a single click again");

    // the wheel goes where the pointer is
    app.feedMouseWheel(0.0f, 2.0f);
    app.feedMouseWheel(1.5f, -1.0f);
    check(probe->scrolls == 2 && probe->scrollX == 1.5f && probe->scrollY == 1.0f, "feedMouseWheel turns the wheel under the pointer, in the sign given");

    // a real frame after the events finds the state they left: the pointer stays, nothing is pressed or released again
    const int pressesBefore = static_cast<int>(probe->presses.size()), releasesBefore = static_cast<int>(probe->releases.size());
    h.frames(5);
    check(probe->isHovered() && app.mouseX() == 100.0f && app.mouseY() == 80.0f, "a frame after a feed leaves the pointer where the feed put it");
    check(static_cast<int>(probe->presses.size()) == pressesBefore && static_cast<int>(probe->releases.size()) == releasesBefore,
            "and does not repeat a press or a release");
    check(h.io().mouseEvents.empty() && h.io().keyEvents.empty() && h.io().inputChars.empty(), "the backend's IO has no events of the feed in it");

    // the keys: to the widget that has the focus
    app.feedMouseButton(0, true); // the probe takes the focus (it accepts it) with a click
    app.feedMouseButton(0, false);
    check(app.focused() == probe, "(the probe has the focus)");
    app.feedKey(ig::retained::Key::A, true, true, false, false);
    app.feedKey(ig::retained::Key::A, false);
    app.feedKey(ig::retained::Key::Return);
    check(probe->keys.size() == 3, "feedKey sends the key down and up events to the focused widget");
    if (probe->keys.size() == 3)
    {
        check(probe->keys[0].key == ig::retained::Key::A && probe->keys[0].down && probe->keys[0].shift && !probe->keys[0].ctrl &&
                        !probe->keys[0].alt,
                "with the modifiers given");
        check(probe->keys[1].key == ig::retained::Key::A && !probe->keys[1].down && !probe->keys[1].shift,
                "the release is a release, and has no modifiers unless given");
        check(probe->keys[2].key == ig::retained::Key::Return && probe->keys[2].down, "down is the default");
    }

    // the text: UTF-8, each character as it was typed
    app.feedText("ab");
    app.feedText("Mus\xC3\xADica \xE2\x82\xAC \xF0\x9F\x8E\xB5");
    check(probe->typed == "abMus\xC3\xADica \xE2\x82\xAC \xF0\x9F\x8E\xB5", "feedText delivers the typed text, accents, the euro sign and an emoji included");
    app.feedText("");
    app.feedText(nullptr);

    // widgets work from the feed as they do from a window: a Button is clicked, a TextInput typed into
    app.feedMouseMove(300.0f, 55.0f);
    app.feedMouseButton(0, true);
    check(clicks == 0, "a Button is not clicked by the press");
    app.feedMouseButton(0, false);
    check(clicks == 1, "a Button is clicked by a press and a release on it");
    TextInput* input = h.root()->createChild<TextInput>();
    input->setRect(Rect(40.0f, 200.0f, 200.0f, 26.0f));
    h.frame();
    app.feedMouseMove(100.0f, 213.0f);
    app.feedMouseButton(0, true);
    app.feedMouseButton(0, false);
    check(app.focused() == input, "a click on a TextInput focuses it");
    app.feedText("Ol\xC3\xA1");
    check(input->text() == "Ol\xC3\xA1", "text fed to a focused TextInput is typed in it");
    app.feedKey(ig::retained::Key::Backspace);
    check(input->text() == "Ol", "and a key reaches it too");
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testSyntheticInput);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
