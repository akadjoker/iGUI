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

// A widget that counts the moves it is given.
class MoveCounter : public Widget
{
public:
    Vec2f sizeHint() const override { return { 200.0f, 100.0f }; }
    void onMouseMove(MouseEvent& e) override
    {
        ++moves;
        lastX = e.x;
        relSum += e.relX;
    }
    int moves = 0;
    float lastX = 0.0f;
    float relSum = 0.0f;
};

// The moves of a frame that no press or release is between are one move to where the pointer ends: a mouse
// that reports 8000 times a second must not be hit tested 130 times in a frame, and the widgets see the
// whole distance anyway (relX is the sum).
void testMovesOfAFrameAreOne()
{
    Harness h(400.0f, 200.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    MoveCounter* counter = box->createChild<MoveCounter>();
    h.relayout();
    const Rect r = counter->absoluteRect();
    const float y = r.y + 10.0f;
    h.io().mouseX = r.x + 5.0f;
    h.io().mouseY = y;
    h.frame();
    counter->moves = 0;
    counter->relSum = 0.0f;
    for (int i = 1; i <= 130; ++i) h.io().addMouseMoveEvent(r.x + 5.0f + static_cast<float>(i), y);
    check(h.io().mouseEvents.size() == 1, "130 moves in a row are one event in the IO");
    h.io().mouseX = r.x + 135.0f;
    h.frame();
    check(counter->moves == 1, "and one move for the widgets");
    check(counter->lastX == r.x + 135.0f && counter->relSum == 130.0f, "to where the pointer ended, with all the distance");
    // a press in the middle keeps the two stretches apart
    h.io().addMouseMoveEvent(r.x + 10.0f, y);
    h.io().addMouseMoveEvent(r.x + 20.0f, y);
    h.io().addMouseButtonEvent(r.x + 20.0f, y, 0, true);
    h.io().addMouseMoveEvent(r.x + 30.0f, y);
    h.io().addMouseMoveEvent(r.x + 40.0f, y);
    check(h.io().mouseEvents.size() == 3, "a press between moves keeps them apart: a move, the press, a move");
    h.io().mouseX = r.x + 40.0f;
    h.io().mouseDown[0] = true;
    h.frame();
    check(counter->moves == 3, "the widgets get the move before the press, and the move after it");
}

// Text of several lines put into a document in the middle, at the start, at the end and over a selection: the
// lines are where they belong (insertTextAtCursor moves what follows once, not once for each new line).
void testPasteOfManyLines()
{
    Harness h(400.0f, 300.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    TextEdit* editor = box->createChild<TextEdit>("a0\na1\na2\na3");
    editor->setStretch(1.0f);
    h.relayout();
    h.clickWidget(editor);
    // a paste: what the clipboard gives goes in at the cursor
    auto paste = [&](const String& text)
    {
        clipboardText() = text;
        h.key(Key::V, false, true);
    };
    // in the middle of a line: the line splits, the pasted lines go between the halves
    editor->setCursorPos(1, 1);
    paste("X\nb0\nb1\nY");
    check(editor->lineCount() == 7, "three new lines in the middle of a document");
    check(editor->lineAt(0) == "a0" && editor->lineAt(1) == "a" + String("X") && editor->lineAt(2) == "b0" &&
                    editor->lineAt(3) == "b1" && editor->lineAt(4) == "Y1" && editor->lineAt(5) == "a2" && editor->lineAt(6) == "a3",
            "with the lines before and after them where they were");
    check(editor->cursorPos().line == 4 && editor->cursorPos().col == 1, "and the cursor after the pasted text");
    // at the very end of the document
    editor->setCursorPos(6, 2);
    paste("\nz0\nz1");
    check(editor->lineCount() == 9 && editor->lineAt(6) == "a3" && editor->lineAt(7) == "z0" && editor->lineAt(8) == "z1", "at the end");
    // at the start of the document, over a selection
    editor->setCursorPos(0, 0);
    paste("s0\ns1\n");
    check(editor->lineCount() == 11 && editor->lineAt(0) == "s0" && editor->lineAt(1) == "s1" && editor->lineAt(2) == "a0", "at the start");
    // two lines only (no middle ones)
    editor->setCursorPos(2, 1);
    paste("p\nq");
    check(editor->lineCount() == 12 && editor->lineAt(2) == "ap" && editor->lineAt(3) == "q0", "a pasted line break and nothing else");
    // many lines, then the document is what was put in
    String many;
    for (int i = 0; i < 3000; ++i) many += String("linha ") + String::number(i) + "\n";
    editor->setText("inicio\nfim");
    editor->setCursorPos(0, 6);
    paste(many);
    check(editor->lineCount() == 3002, "three thousand lines in one paste");
    check(editor->lineAt(0) == "iniciolinha 0" && editor->lineAt(1) == "linha 1" && editor->lineAt(2999) == "linha 2999" &&
                    editor->lineAt(3000) == "" && editor->lineAt(3001) == "fim",
            "in order, with the rest of the document after them");
    h.frames(2);
}

// A widget that counts the presses and releases it is given, and where the last release was.
class ClickCounter : public Widget
{
public:
    Vec2f sizeHint() const override { return { 200.0f, 60.0f }; }
    void onMousePress(MouseEvent& e) override
    {
        ++presses;
        e.consumed = true;
    }
    void onMouseRelease(MouseEvent& e) override
    {
        ++releases;
        lastReleaseX = e.x;
    }
    int presses = 0, releases = 0;
    float lastReleaseX = 0.0f;
};

// The widget a click lands on, which runs a nested loop of its own from the click: what a modal dialog does when
// it opens from a button (ZenApp::pump(true) in a loop, from inside the handler). The nested frames put the
// events of the dialog's window into the same IO and the first of them draws and renders, which clears it; the
// events the outer frame was dispatching are not the nested frame's to dispatch again, and the ones the nested
// frame was given are not the outer frame's.
class NestingButton : public ClickCounter
{
public:
    void onMousePress(MouseEvent& e) override
    {
        ClickCounter::onMousePress(e);
        if (nested) return;
        nested = true;
        // the dialog's window: 200 events (more than the vector of the IO has room for), alternating press and release
        for (int i = 0; i < 200; ++i) harness->io().addMouseButtonEvent(targetX, targetY, 0, (i & 1) == 0);
        harness->frame();
    }
    Harness* harness = nullptr;
    bool nested = false;
    float targetX = 0.0f, targetY = 0.0f;
};

// A handler that runs a nested loop does not change what the outer frame walks: the events the outer frame
// had are dispatched once, each, and so are the ones the nested frame had. Before, WidgetApp::update walked the
// vector of mouse events of the IO with a range-for while the handler added 200 to it (the vector moved: a
// use of freed memory) and the nested frame walked, and dispatched again, the outer frame's events too.
void testNestedLoopFromAMouseHandler()
{
    Harness h(400.0f, 200.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    NestingButton* a = box->createChild<NestingButton>();
    ClickCounter* b = box->createChild<ClickCounter>();
    h.relayout();
    a->harness = &h;
    float ax = 0, ay = 0, bx = 0, by = 0;
    Harness::centre(a, ax, ay);
    Harness::centre(b, bx, by);
    a->targetX = bx;
    a->targetY = by;
    h.io().mouseX = ax;
    h.io().mouseY = ay;
    h.io().addMouseMoveEvent(ax, ay);
    h.io().addMouseButtonEvent(ax, ay, 0, true);
    h.io().addMouseButtonEvent(ax, ay, 0, false);
    h.frame();
    check(a->presses == 1, "the press that started the nested loop is dispatched once");
    check(a->releases == 1, "and the release after it, which the outer frame had, still comes once the nested loop is over");
    check(a->lastReleaseX == ax, "at the place it happened");
    check(b->presses == 100 && b->releases == 100, "the 200 events of the nested frame are dispatched once, by that frame (100 clicks)");
    if (b->presses != 100 || b->releases != 100) printf("    got %d presses and %d releases\n", b->presses, b->releases);
    check(h.io().mouseEvents.empty(), "nothing is left in the IO");
    h.frames(2);
    check(a->presses == 1 && a->releases == 1 && b->presses == 100 && b->releases == 100, "and nothing comes again in the next frames");
}

// A widget that records what it is given from the keyboard: keys as K<code>, typed text as T<text>.
class KeyLog : public Widget
{
public:
    Vec2f sizeHint() const override { return { 200.0f, 60.0f }; }
    void onKeyPress(KeyEvent& e) override
    {
        char item[16];
        snprintf(item, sizeof item, "K%d ", e.key);
        log += item;
        ++keys;
        e.consumed = true;
        if (e.key == 1001 && !nested)
        {
            nested = true;
            // the dialog's window: a letter and then 200 keys, the first of them with the letter before it
            harness->io().addInputCharacter('n');
            for (int i = 0; i < 200; ++i) harness->io().addKeyEvent(2000 + i, 2000 + i, false, false, false, true);
            harness->frame();
        }
    }
    void onTextInput(KeyEvent& e) override
    {
        log += "T";
        log += e.text;
        log += " ";
        e.consumed = true;
    }
    Harness* harness = nullptr;
    bool nested = false;
    int keys = 0;
    String log;
};

// The same for the keys and the letters: the key that starts the nested loop, the letter the outer frame
// typed before the next key, and that key are dispatched in order around what the nested frame had.
void testNestedLoopFromAKeyHandler()
{
    Harness h(400.0f, 200.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    KeyLog* w = box->createChild<KeyLog>();
    h.relayout();
    w->harness = &h;
    h.app.setFocused(w);
    h.io().addKeyEvent(1001, 1001, false, false, false, true);
    h.io().addInputCharacter('x');
    h.io().addKeyEvent(1002, 1002, false, false, false, true);
    h.frame();
    String expected = "K1001 Tn ";
    for (int i = 0; i < 200; ++i)
    {
        char item[16];
        snprintf(item, sizeof item, "K%d ", 2000 + i);
        expected += item;
    }
    expected += "Tx K1002 ";
    check(w->log == expected, "the keys and letters of both frames are dispatched once each and in order: the nested frame's between the outer frame's");
    if (w->log != expected) printf("    got %d keys, the log starts \"%.60s\"\n", w->keys, w->log.c_str());
    check(h.io().keyEvents.empty() && h.io().inputChars.empty(), "nothing is left in the IO");
    h.frames(2);
    check(w->keys == 202, "and nothing comes again in the next frames");
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
    RUN(testMovesOfAFrameAreOne);
    RUN(testPasteOfManyLines);
    RUN(testNestedLoopFromAMouseHandler);
    RUN(testNestedLoopFromAKeyHandler);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
