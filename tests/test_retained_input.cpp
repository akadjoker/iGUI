// The input of a frame reaches the widgets in the order it happened: a press and a release that fall
// between two frames are a click, a press happens where the pointer was when it was pressed (not where it
// is when the frame is made), and a key pressed between two letters of a frame acts between them. The
// zen adapter reads the platform's event stream into the IO (addMouseMoveEvent, addMouseButtonEvent,
// addKeyEvent, addInputCharacter), driven here through the headless frame loop (tests/RetainedHarness.hpp).
// No standard library: ct and C only.

#include "RetainedHarness.hpp"

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/TextInputWidgets.hpp>

using namespace rh;

namespace
{

// A click that falls inside one frame: the press and the release are both in the event stream of the frame,
// and the polled state, read when the frame is made, says the button is up (as it was before).
void testPressAndReleaseInOneFrameIsAClick()
{
    Harness h(400.0f, 200.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    Button* button = box->createChild<Button>("Ok");
    h.relayout();
    int clicks = 0;
    button->clicked.connect([&clicks] { ++clicks; });
    float x = 0, y = 0;
    Harness::centre(button, x, y);
    h.io().mouseX = x;
    h.io().mouseY = y;
    h.io().addMouseMoveEvent(x, y);
    h.io().addMouseButtonEvent(x, y, 0, true);
    h.io().addMouseButtonEvent(x, y, 0, false);
    h.frame();
    check(clicks == 1, "a press and a release in one frame are a click");
    h.frames(2);
    check(clicks == 1, "and only one");
    // two of them in a frame are two clicks
    h.io().addMouseButtonEvent(x, y, 0, true);
    h.io().addMouseButtonEvent(x, y, 0, false);
    h.io().addMouseButtonEvent(x, y, 0, true);
    h.io().addMouseButtonEvent(x, y, 0, false);
    h.frame();
    check(clicks == 3, "two clicks in a frame are two clicks");
    // the polled state alone, as before: a press in one frame, the release in another
    h.moveTo(x, y);
    h.down();
    h.up();
    check(clicks == 4, "the polled state still gives a click");
}

// The press lands where the pointer was, the release where it let go: a drag that starts and ends
// between two frames selects what it crossed.
void testPressIsWhereThePointerWas()
{
    Harness h(400.0f, 200.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    TextInput* input = box->createChild<TextInput>("0123456789");
    h.relayout();
    const Rect r = input->absoluteRect();
    const float y = r.y + r.h * 0.5f;
    h.click(r.x + 40.0f, y);
    h.frames(2);
    // press near the start of the text, drag to the middle, release: all in one frame
    const float x0 = r.x + 10.0f, x1 = r.x + 70.0f;
    h.io().mouseX = x1;
    h.io().mouseY = y;
    h.io().addMouseMoveEvent(x0, y);
    h.io().addMouseButtonEvent(x0, y, 0, true);
    h.io().addMouseMoveEvent(x1, y);
    h.io().addMouseButtonEvent(x1, y, 0, false);
    h.frame();
    check(input->hasSelection(), "a press and a drag in one frame select the text between them");
    check(input->selectedText().size() >= 3 && input->selectedText().size() <= 9, "and what they cross");
}

// A key between two letters of a frame acts between them: "a", Left, "b" is "ba".
void testKeysAndLettersKeepTheirOrder()
{
    Harness h(400.0f, 200.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    TextInput* input = box->createChild<TextInput>("");
    h.relayout();
    h.clickWidget(input);
    h.io().addInputCharacter('a');
    h.io().addKeyEvent(Key::Left, Key::Left, false, false, false, true);
    h.io().addKeyEvent(Key::Left, Key::Left, false, false, false, false);
    h.io().addInputCharacter('b');
    h.frame();
    check(input->text() == "ba", "a, Left, b in one frame is ba");
    if (input->text() != "ba") printf("    got \"%s\"\n", input->text().c_str());
    // Backspace between two letters
    input->setText("");
    h.io().addInputCharacter('x');
    h.io().addKeyEvent(Key::Backspace, Key::Backspace, false, false, false, true);
    h.io().addKeyEvent(Key::Backspace, Key::Backspace, false, false, false, false);
    h.io().addInputCharacter('y');
    h.frame();
    check(input->text() == "y", "x, Backspace, y in one frame is y");
    if (input->text() != "y") printf("    got \"%s\"\n", input->text().c_str());
    // the order of a frame with only letters, or only keys, does not change
    input->setText("");
    h.io().addInputCharacter('p');
    h.io().addInputCharacter('q');
    h.frame();
    check(input->text() == "pq", "letters alone are inserted in order");
    h.io().addKeyEvent(Key::Home, Key::Home, false, false, false, true);
    h.io().addKeyEvent(Key::Home, Key::Home, false, false, false, false);
    h.io().addInputCharacter('r');
    h.frame();
    check(input->text() == "rpq", "a key and a letter after it");
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testPressAndReleaseInOneFrameIsAClick);
    RUN(testPressIsWhereThePointerWas);
    RUN(testKeysAndLettersKeepTheirOrder);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
