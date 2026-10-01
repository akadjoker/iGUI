// Caret movement and editing keys in inputText / inputTextMultiline, with
// multi-byte UTF-8 (2-, 3- and 4-byte code points) under the caret.

#include "InteractionHarness.hpp"

#include <igui/CodeEditor.hpp>

#include <stdio.h>
#include <string.h>

namespace
{

int gFailures = 0;

class ClipboardBackend : public ig::TestBackend
{
public:
    ig::String clipboard;
    bool setClipboardText(ig::StringView text) override
    {
        clipboard = ig::String(text.data(), text.size());
        return true;
    }
    ig::String clipboardText() override { return clipboard; }
};

// Keys: "<L>" "<R>" "<U>" "<D>" "<HOME>" "<END>" "<BS>" "<DEL>" "<ENTER>"
// "<PASTE>"; anything else is typed as text.
ig::String edit(const char *initial, const char *const *keys, int count, bool multiline,
                const char *clipboard = "")
{
    ClipboardBackend backend;
    backend.clipboard = clipboard;
    ig::Context context(backend);
    ig::Harness harness(context);
    ig::String value(initial);
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        if (multiline)
            c.inputTextMultiline("text", value, ig::Rect(10.0f, 10.0f, 300.0f, 120.0f));
        else
            c.inputText("text", value, ig::Rect(10.0f, 10.0f, 300.0f, 28.0f));
        c.endWindow();
    };
    harness.frame(build);
    harness.click(150.0f, 24.0f, build); // focus: caret at the end
    struct Named
    {
        const char *name;
        ig::KeyCode key;
        bool control;
    };
    const Named named[] = {
        {"<L>", ig::KeyCode::Left, false}, {"<R>", ig::KeyCode::Right, false},
        {"<U>", ig::KeyCode::Up, false}, {"<D>", ig::KeyCode::Down, false},
        {"<HOME>", ig::KeyCode::Home, false}, {"<END>", ig::KeyCode::End, false},
        {"<BS>", ig::KeyCode::Backspace, false}, {"<DEL>", ig::KeyCode::Delete, false},
        {"<ENTER>", ig::KeyCode::Enter, false}, {"<PASTE>", ig::KeyCode::V, true},
    };
    for (int k = 0; k < count; ++k)
    {
        bool isKey = false;
        for (size_t n = 0; n < sizeof(named) / sizeof(named[0]); ++n)
        {
            if (strcmp(keys[k], named[n].name) == 0)
            {
                context.pushEvent(ig::Event::keyDown(named[n].key, named[n].control));
                isKey = true;
            }
        }
        if (!isKey)
            context.pushEvent(ig::Event::textInput(keys[k]));
        harness.frame(build);
    }
    return value;
}

void expect(const char *what, const ig::String &actual, const char *expected)
{
    if (actual != expected)
    {
        ++gFailures;
        printf("FAIL %s: got '%s', expected '%s'\n", what, actual.c_str(), expected);
    }
}

void test_single_line()
{
    const char *left[] = {"<L>", "X"};
    expect("Left then type", edit("abc", left, 2, false), "abXc");
    const char *leftBack[] = {"<L>", "<L>", "<BS>"};
    expect("Left x2 then Backspace", edit("abc", leftBack, 3, false), "bc");
    const char *overTwoByte[] = {"<L>", "X"};
    expect("Left over a 2-byte char", edit("a\xC3\xA7", overTwoByte, 2, false), "aX\xC3\xA7");
    const char *overThreeByte[] = {"<L>", "<L>", "X"};
    expect("Left over a 3-byte char", edit("a\xE2\x82\xAC" "b", overThreeByte, 3, false), "aX\xE2\x82\xAC" "b");
    const char *overFourByte[] = {"<HOME>", "<R>", "<R>", "X"};
    expect("Right over a 4-byte char", edit("a\xF0\x9F\x98\x80" "b", overFourByte, 4, false),
           "a\xF0\x9F\x98\x80" "Xb");
    const char *del[] = {"<HOME>", "<DEL>"};
    expect("Home then Delete", edit("abc", del, 2, false), "bc");
    const char *delMulti[] = {"<HOME>", "<DEL>"};
    expect("Delete a 2-byte char", edit("\xC3\xA7" "b", delMulti, 2, false), "b");
    const char *delEnd[] = {"<DEL>"};
    expect("Delete at the end does nothing", edit("ab", delEnd, 1, false), "ab");
    const char *leftStart[] = {"<HOME>", "<L>", "X"};
    expect("Left at the start stays", edit("ab", leftStart, 3, false), "Xab");
    const char *rightEnd[] = {"<R>", "X"};
    expect("Right at the end stays", edit("ab", rightEnd, 2, false), "abX");
    const char *newline[] = {"line\nbreak"};
    expect("Single line drops newlines from typed text", edit("a", newline, 1, false), "alinebreak");
    const char *paste[] = {"<HOME>", "<PASTE>"};
    expect("Single line drops newlines from pasted text", edit("a", paste, 2, false, "x\r\ny"), "xya");
}

void test_multiline()
{
    const char *up[] = {"<U>", "X"};
    expect("Up keeps the column", edit("abc\nde", up, 2, true), "abXc\nde");
    const char *upShort[] = {"<U>", "X"};
    expect("Up clamps to a shorter line", edit("ab\ncdef", upShort, 2, true), "abX\ncdef");
    const char *down[] = {"<HOME>", "<U>", "<D>", "X"};
    expect("Down from the first line", edit("ab\ncd", down, 4, true), "ab\nXcd");
    const char *home[] = {"<HOME>", "X"};
    expect("Home goes to the line start", edit("ab\ncd", home, 2, true), "ab\nXcd");
    const char *end[] = {"<U>", "<HOME>", "<END>", "X"};
    expect("End goes to the line end", edit("ab\ncd", end, 4, true), "abX\ncd");
    const char *upFirst[] = {"<U>", "<U>", "X"};
    expect("Up on the first line goes to its start", edit("ab\ncd", upFirst, 3, true), "Xab\ncd");
    const char *downLast[] = {"<D>", "X"};
    expect("Down on the last line goes to its end", edit("ab\ncd", downLast, 2, true), "ab\ncdX");
    const char *upUtf8[] = {"<U>", "X"};
    expect("Up keeps the column in code points", edit("\xC3\xA7\xC3\xA7\xC3\xA7\nab", upUtf8, 2, true),
           "\xC3\xA7\xC3\xA7X\xC3\xA7\nab");
    const char *joinLines[] = {"<U>", "<END>", "<DEL>"};
    expect("Delete at a line end joins the lines", edit("ab\ncd", joinLines, 3, true), "abcd");
    const char *enter[] = {"<L>", "<ENTER>", "x"};
    expect("Enter splits at the caret", edit("ab", enter, 3, true), "a\nxb");
}

// Ctrl+Z / Ctrl+Y belong to the text field that has the keyboard, not to the
// application's undo history: typing in a field and pressing Ctrl+Z must not
// silently revert an unrelated application action (and in the code editor,
// which has its own undo, it must not undo twice).
void test_undo_shortcut_belongs_to_focused_text_field()
{
    for (int widget = 0; widget < 3; ++widget)
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        ig::CodeEditorState code;
        code.setText("abc");
        ig::String text("abc");
        int appValue = 2;
        int *value = &appValue;
        auto build = [&](ig::Context &c)
        {
            c.beginMainWindow("main");
            if (widget == 0)
                c.codeEditor("code", code, ig::Rect(10.0f, 10.0f, 400.0f, 200.0f));
            else if (widget == 1)
                c.inputText("text", text, ig::Rect(10.0f, 10.0f, 400.0f, 28.0f));
            else
                c.inputTextMultiline("text", text, ig::Rect(10.0f, 10.0f, 400.0f, 100.0f));
            c.button("button", ig::Rect(10.0f, 300.0f, 100.0f, 28.0f));
            c.endWindow();
        };
        harness.frame(build);
        context.pushUndo("set 2", [value]() { *value = 1; }, [value]() { *value = 2; });
        harness.click(100.0f, 20.0f, build); // focus the text widget
        context.pushEvent(ig::Event::textInput("X"));
        harness.frame(build);
        context.pushEvent(ig::Event::keyDown(ig::KeyCode::Z, true));
        harness.frame(build);
        const char *names[] = {"codeEditor", "inputText", "inputTextMultiline"};
        if (appValue != 2)
        {
            ++gFailures;
            printf("FAIL Ctrl+Z in a focused %s undid an application action\n", names[widget]);
        }
        if (widget == 0 && code.text() != "abc")
        {
            ++gFailures;
            printf("FAIL Ctrl+Z in the code editor did not undo its own typing\n");
        }

        // With no text field focused the application history gets it.
        harness.click(50.0f, 314.0f, build); // the button takes focus
        context.pushEvent(ig::Event::keyDown(ig::KeyCode::Z, true));
        harness.frame(build);
        if (appValue != 1)
        {
            ++gFailures;
            printf("FAIL Ctrl+Z with no text field focused did not reach the application (%s)\n", names[widget]);
        }
    }
}

} // namespace

int main()
{
    test_single_line();
    test_multiline();
    test_undo_shortcut_belongs_to_focused_text_field();
    if (gFailures != 0)
    {
        printf("test_text_editing: %d failure(s)\n", gFailures);
        return 1;
    }
    printf("test_text_editing: all tests passed\n");
    return 0;
}
