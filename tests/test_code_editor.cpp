// codeEditor with multi-byte UTF-8: the caret, Backspace and Delete move and
// erase whole code points, so the buffer stays valid UTF-8 whatever is typed.

#include "InteractionHarness.hpp"

#include <igui/CodeEditor.hpp>

#include <stdio.h>
#include <string.h>

namespace
{

int gFailures = 0;

void check(bool condition, const char *what)
{
    if (!condition)
    {
        ++gFailures;
        printf("FAIL %s\n", what);
    }
}

bool validUtf8(const ig::String &text)
{
    size_t i = 0u;
    while (i < text.size())
    {
        const unsigned char first = static_cast<unsigned char>(text[i]);
        size_t length = first < 0x80u ? 1u : (first >= 0xC2u && first <= 0xDFu) ? 2u
                      : (first >= 0xE0u && first <= 0xEFu) ? 3u : (first >= 0xF0u && first <= 0xF4u) ? 4u : 0u;
        if (length == 0u || i + length > text.size())
            return false;
        for (size_t k = 1u; k < length; ++k)
            if ((static_cast<unsigned char>(text[i + k]) & 0xC0u) != 0x80u)
                return false;
        i += length;
    }
    return true;
}

struct Editor
{
    ig::TestBackend backend;
    ig::Context context;
    ig::Harness harness;
    ig::CodeEditorState state;

    explicit Editor(const char *text) : backend(), context(backend), harness(context), state()
    {
        state.setText(text);
        frame();
        // Click below the text: caret at the end of the last line.
        harness.click(480.0f, 200.0f, [this](ig::Context &c) { build(c); });
    }

    void build(ig::Context &c)
    {
        c.beginMainWindow("main");
        c.codeEditor("code", state, ig::Rect(10.0f, 10.0f, 500.0f, 200.0f));
        c.endWindow();
    }

    void frame() { harness.frame([this](ig::Context &c) { build(c); }); }

    void key(ig::KeyCode code, bool control = false, bool shift = false)
    {
        context.pushEvent(ig::Event::keyDown(code, control, shift));
        frame();
    }

    void type(const char *text)
    {
        context.pushEvent(ig::Event::textInput(text));
        frame();
    }
};

void expectText(Editor &editor, const char *expected, const char *what)
{
    const ig::String text = editor.state.text();
    if (text != expected)
    {
        ++gFailures;
        printf("FAIL %s: got '%s', expected '%s'\n", what, text.c_str(), expected);
    }
}

void test_caret_and_erase_by_code_point()
{
    {
        Editor e("a\xC3\xA9");
        e.key(ig::KeyCode::Left);
        e.type("X");
        expectText(e, "aX\xC3\xA9", "Left steps over a 2-byte character");
    }
    {
        Editor e("a\xF0\x9F\x98\x80" "b");
        e.key(ig::KeyCode::Home);
        e.key(ig::KeyCode::Right);
        e.key(ig::KeyCode::Right);
        e.type("X");
        expectText(e, "a\xF0\x9F\x98\x80" "Xb", "Right steps over a 4-byte character");
    }
    {
        Editor e("a\xC3\xA9");
        e.key(ig::KeyCode::Backspace);
        expectText(e, "a", "Backspace erases a whole 2-byte character");
    }
    {
        Editor e("\xE2\x82\xAC" "b");
        e.key(ig::KeyCode::Home);
        e.key(ig::KeyCode::Delete);
        expectText(e, "b", "Delete erases a whole 3-byte character");
    }
    {
        // Caret at byte 1 of "ab", Up into "éé": byte 1 is inside the first
        // é, so the caret must land on a character boundary.
        Editor e("\xC3\xA9\xC3\xA9\nab");
        e.key(ig::KeyCode::Left);
        e.key(ig::KeyCode::Up);
        e.type("X");
        check(validUtf8(e.state.text()), "Up into a line of 2-byte characters splits one");
    }
    {
        // Undo restores exactly what was erased.
        Editor e("a\xC3\xA9");
        e.key(ig::KeyCode::Backspace);
        e.key(ig::KeyCode::Z, true);
        expectText(e, "a\xC3\xA9", "Undo after erasing a 2-byte character");
    }
}

// Random editing, multi-cursor included: the buffer must stay valid UTF-8.
void test_random_editing_keeps_utf8_valid()
{
    const char *const texts[] = {"a", "\xC3\xA9", "\xE2\x82\xAC", "\xF0\x9F\x98\x80", " ", "\n", "x\xC3\xA7y"};
    struct Key
    {
        ig::KeyCode code;
        bool control;
        bool shift;
    };
    const Key keys[] = {
        {ig::KeyCode::Left, false, false}, {ig::KeyCode::Right, false, false}, {ig::KeyCode::Up, false, false},
        {ig::KeyCode::Down, false, false}, {ig::KeyCode::Left, false, true}, {ig::KeyCode::Right, false, true},
        {ig::KeyCode::Home, false, false}, {ig::KeyCode::End, false, false}, {ig::KeyCode::Backspace, false, false},
        {ig::KeyCode::Delete, false, false}, {ig::KeyCode::Enter, false, false}, {ig::KeyCode::D, true, false},
        {ig::KeyCode::A, true, false}, {ig::KeyCode::Z, true, false}, {ig::KeyCode::Y, true, false},
        {ig::KeyCode::Escape, false, false},
    };
    uint32_t seed = 7u;
    for (int run = 0; run < 20; ++run)
    {
        Editor e("\xC3\xA9t\xC3\xA9 \xE2\x82\xAC\xE2\x82\xAC\n\xF0\x9F\x98\x80 \xC3\xA9t\xC3\xA9\nplain");
        for (int step = 0; step < 300; ++step)
        {
            seed = seed * 1664525u + 1013904223u;
            const uint32_t r = seed >> 8;
            if (r % 3u == 0u)
                e.type(texts[(r / 3u) % (sizeof(texts) / sizeof(texts[0]))]);
            else
            {
                const Key &k = keys[(r / 3u) % (sizeof(keys) / sizeof(keys[0]))];
                if (k.code == ig::KeyCode::Escape)
                    e.harness.click(480.0f, 200.0f, [&e](ig::Context &c) { e.build(c); }); // refocus after Escape
                else
                    e.key(k.code, k.control, k.shift);
            }
            if (!validUtf8(e.state.text()))
            {
                char message[96];
                snprintf(message, sizeof(message), "random editing produced invalid UTF-8 (run %d, step %d)", run, step);
                check(false, message);
                return;
            }
        }
    }
}

} // namespace

int main()
{
    test_caret_and_erase_by_code_point();
    test_random_editing_keeps_utf8_valid();
    if (gFailures != 0)
    {
        printf("test_code_editor: %d failure(s)\n", gFailures);
        return 1;
    }
    printf("test_code_editor: all tests passed\n");
    return 0;
}
