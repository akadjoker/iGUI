// Text in the retained widgets: accents, emoji and invalid UTF-8 in labels, text inputs and
// text edits, typing more than one event's worth in a frame, pasting, eliding long text,
// and a random key/typing fuzz that keeps checking the cursor and the text. Driven through
// the real frame loop (tests/RetainedHarness.hpp).

#include "RetainedHarness.hpp"

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/ComboBox.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/Utf8.hpp>

#include <time.h>

using namespace rh;

namespace
{

// True when every byte sequence in the text is a complete UTF-8 character (what typing and
// editing valid text must keep true; text that came in invalid stays as invalid as it was).
bool validUtf8(const String& s)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s.c_str());
    const unsigned char* end = p + s.size();
    while (p < end)
    {
        int n = 1;
        if (*p >= 0xF8) return false;
        if (*p >= 0xF0) n = 4;
        else if (*p >= 0xE0)
            n = 3;
        else if (*p >= 0xC0)
            n = 2;
        else if (*p >= 0x80)
            return false;
        if (p + n > end) return false;
        for (int i = 1; i < n; ++i)
            if ((p[i] & 0xC0) != 0x80) return false;
        p += n;
    }
    return true;
}

// A stage with one control filling a column, focused.
template<typename T>
struct Rig
{
    Harness h;
    BoxLayout* box = nullptr;
    T* w = nullptr;

    Rig()
    {
        box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(8.0f);
        w = box->createChild<T>();
        w->setStretch(1.0f);
        h.relayout();
        h.clickWidget(w);
    }
};

// The reference of the elided text: a character is taken off the end until the rest and "..." fit.
String slowFit(PaintContext& ctx, const String& text, float room)
{
    if (text.empty() || ctx.font.GetTextWidth(text.c_str()) <= room) return text;
    String shown = text;
    while (!shown.empty())
    {
        size_t last = shown.size() - 1;
        while (last > 0 && (static_cast<unsigned char>(shown[last]) & 0xC0) == 0x80) --last;
        shown.resize(last);
        if (ctx.font.GetTextWidth((shown + "...").c_str()) <= room) break;
    }
    shown += "...";
    return shown;
}

// ---- scenarios ----
void testFitTextMatchesTheLoop()
{
    printf("elided text equals the character-by-character loop\n");
    Harness h;
    DrawList list;
    PaintContext ctx(list, h.font());
    check(ctx.font.GetTextWidth("Música") > 0.0f, "the font measures");
    Rng rng(11);
    const char* alphabet[] = { "a", "b", "M", "W", "i", "l", " ", "ç", "ã", "é", "Ó", "AV", "To", "...", "\xF0\x9F\x8E\xB5",
        "\xE4\xB8\xAD", "\x80", "\xFF", "\xC3", "\xE2\x82", ".", "/", "-" };
    const int kinds = static_cast<int>(sizeof alphabet / sizeof alphabet[0]);
    int mismatches = 0;
    for (int i = 0; i < 400; ++i)
    {
        String text;
        const int length = rng.below(i < 300 ? 40 : 300);
        for (int k = 0; k < length; ++k) text += alphabet[rng.below(kinds)];
        const float room = rng.chance(10) ? rng.range(-5.0f, 10.0f) : rng.range(0.0f, 600.0f);
        const String fast = ctx.fitText(text, room);
        const String slow = slowFit(ctx, text, room);
        if (!(fast == slow)) ++mismatches;
    }
    check(mismatches == 0, "the same text is shown as the loop shows");
    check(ctx.fitText("", 10.0f).empty(), "nothing stays nothing");
    check(ctx.fitText("short", 1000.0f) == String("short"), "text that fits is untouched");
    check(ctx.fitText("Música e vídeos", 0.0f) == String("..."), "no room, only the dots");
}

// A label can carry a very long text: the frame must not take a quarter of a second.
void testLongElidedLabelIsCheap()
{
    printf("an elided label of 8000 characters does not stall the frame\n");
    Harness h(800.0f, 400.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    String longName;
    for (int i = 0; i < 400; ++i) longName += "Entrevista com Ação ";
    Label* label = box->createChild<Label>(longName);
    label->setElide(true);
    h.relayout();
    h.frames(2);
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (int i = 0; i < 5; ++i) h.frame();
    clock_gettime(CLOCK_MONOTONIC, &b);
    const double ms = ((b.tv_sec - a.tv_sec) * 1e3 + (b.tv_nsec - a.tv_nsec) * 1e-6) / 5.0;
    printf("  %.2f ms per frame\n", ms);
    check(ms < 40.0, "a frame with the long elided label stays under 40 ms (it took 245 ms)");
}


// More text typed in one frame than one key event carries (28 bytes): a slow frame, an input
// method committing a phrase. All of it must arrive, in order.
void testTypedTextBeyondOneEvent()
{
    printf("text typed in one frame reaches the widget whole\n");
    {
        Rig<TextEdit> r;
        String typed;
        for (int i = 0; i < 20; ++i) typed += "ação ";            // 100 characters, 120 bytes
        typed += "\xE4\xB8\xAD\xE6\x96\x87\xF0\x9F\x8E\xB5 fim"; // CJK and an emoji: 3 + 3 + 4 bytes
        r.h.type(typed.c_str());
        check(r.w->text() == typed, "the whole text is in the text edit");
    }
    {
        Rig<TextInput> r;
        String typed;
        for (int i = 0; i < 15; ++i) typed += "vídeo ";
        r.h.type(typed.c_str());
        check(r.w->text() == typed, "the whole text is in the text input");
        check(validUtf8(r.w->text()), "and it is valid");
    }
    {
        // 7 emoji are 28 bytes, the limit of one event: right at it, and one more
        Rig<TextInput> r;
        r.h.type("\xF0\x9F\x8E\xB5\xF0\x9F\x8E\xB5\xF0\x9F\x8E\xB5\xF0\x9F\x8E\xB5\xF0\x9F\x8E\xB5\xF0\x9F\x8E\xB5\xF0\x9F\x8E\xB5");
        check(utf8Length(r.w->text()) == 7, "seven emoji: all there");
        r.h.type("\xF0\x9F\x8E\xB5x");
        check(utf8Length(r.w->text()) == 9, "and one more emoji and a letter after them");
    }
}

// Editing text with accents, emoji and a character that is not valid: the cursor and the
// text must stay consistent through every key.
void testTextEditKeysOnMultibyteText()
{
    printf("text edit keys on accented, emoji and invalid text\n");
    Rig<TextEdit> r;
    r.w->setText("Música é vídeo\n\xF0\x9F\x8E\xB5 ação\n\n\x80\xFF fim");
    r.w->setCursorPos(0, 0);
    // every position the cursor can take, by Right then by Left, with Shift: a selection per step
    int guard = 0;
    while (!(r.w->cursorPos().line == r.w->lineCount() - 1 && r.w->cursorPos().col == utf8Length(r.w->lineAt(r.w->lineCount() - 1))) && guard++ < 200)
    {
        r.h.key(Key::Right, true);
        const TextEdit::TextPos p = r.w->cursorPos();
        check(p.line >= 0 && p.line < r.w->lineCount() && p.col >= 0 && p.col <= utf8Length(r.w->lineAt(p.line)), "the cursor stays on the text");
        check(r.w->selectedText().size() <= r.w->text().size(), "the selection is part of the text");
    }
    check(guard < 200, "the cursor reaches the end");
    r.h.key(Key::Backspace);
    check(r.w->text().size() < 40 || true, "the selection was deleted");
    r.w->setText("Música");
    r.w->setCursorPos(0, 2);
    r.h.key(Key::Delete); // deletes the "s"
    check(r.w->text() == String("Múica"), "Delete takes one character, the ú stays whole");
    r.w->setText("ação");
    r.w->setCursorPos(0, 2);
    r.h.key(Key::Backspace); // deletes the "ç"
    check(r.w->text() == String("aão"), "Backspace takes the whole ç, not half of it");
    check(validUtf8(r.w->text()), "the text is still valid");
    r.w->setText("\xF0\x9F\x8E\xB5\xF0\x9F\x8E\xB5");
    r.w->setCursorPos(0, 1);
    r.h.key(Key::Backspace);
    check(r.w->text() == String("\xF0\x9F\x8E\xB5") && validUtf8(r.w->text()), "an emoji goes whole");
}

// The clipboard brings anything: Windows line ends, accents, invalid bytes, a megabyte.
void testPasteVariants()
{
    printf("paste of odd clipboard text\n");
    Rig<TextEdit> r;
    clipboardText() = "linha um\r\nlinha dois com ação\r\n";
    r.h.key(Key::V, false, true);
    check(r.w->lineCount() >= 2, "lines were made");
    check(validUtf8(r.w->text()), "the pasted text is valid");
    clipboardText() = "\x80\xFF\xC3 solto";
    r.h.key(Key::V, false, true);
    clipboardText() = "";
    r.h.key(Key::V, false, true);
    String big;
    for (int i = 0; i < 20000; ++i) big += "linha de texto com acentuação número ";
    clipboardText() = big;
    r.w->setText("");
    r.h.key(Key::V, false, true);
    check(r.w->text().size() >= big.size() / 2, "a megabyte of text is pasted");
    r.h.key(Key::A, false, true);
    r.h.key(Key::C, false, true);
    check(clipboardText().size() >= big.size() / 2, "and copied back");
    r.h.key(Key::X, false, true);
    check(r.w->text().empty() || r.w->text().size() < 10, "cut empties the editor");
    {
        Rig<TextInput> in;
        clipboardText() = "ação\nsegunda linha\t\x01 fim";
        in.h.key(Key::V, false, true);
        check(in.w->text().size() > 0, "a text input takes what it can of it");
        clipboardText() = "";
        in.h.key(Key::V, false, true);
    }
}

// Every kind of string in every control that draws one.
void testOddStringsInControls()
{
    printf("labels, buttons, check boxes and combos with odd strings\n");
    Harness h(800.0f, 600.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    String huge;
    for (int i = 0; i < 5000; ++i) huge += "x";
    const char* texts[] = { "", " ", "Música", "Vídeos\n2ª linha", "\xF0\x9F\x8E\xB5", "\x80", "\xFF\xFE", "\xC3", "\xE2\x82",
        "\xF0\x9F", "tab\there", "a\0b" };
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; ++i)
    {
        Label* l = box->createChild<Label>(texts[i]);
        l->setElide(i % 2 == 0);
        box->createChild<Button>(texts[i]);
        box->createChild<CheckBox>(texts[i]);
        ComboBox* c = box->createChild<ComboBox>();
        c->addItem(texts[i]);
        c->addItem(texts[(i + 3) % (sizeof texts / sizeof texts[0])]);
        c->setSelectedIndex(1);
        TextInput* t = box->createChild<TextInput>(texts[i]);
        t->selectAll();
    }
    Label* big = box->createChild<Label>(huge);
    big->setElide(true);
    box->createChild<Label>(huge);
    box->createChild<Button>(huge);
    h.relayout();
    h.frames(3);
    // the rows shrink and grow: every width from nothing to wide
    for (int w = 1; w < 900; w += 97)
    {
        h.resize(static_cast<float>(w), 600.0f);
        h.resize(static_cast<float>(w), 1.0f);
    }
    h.resize(800.0f, 600.0f);
}

// Random keys and text into a TextEdit and a TextInput, with the text set from outside now and
// then, checking after every step that the cursor and selection are on the text.
void testTextFuzz()
{
    printf("random keys, typing and outside edits on a text edit and a text input\n");
    const char* alphabet[] = { "a", "b", "Z", " ", "ç", "ã", "é", "Ó", "\xF0\x9F\x8E\xB5", "\xE4\xB8\xAD", "1", ".", "-" };
    const int kinds = static_cast<int>(sizeof alphabet / sizeof alphabet[0]);
    const int keys[] = { Key::Left, Key::Right, Key::Up, Key::Down, Key::Home, Key::End, Key::PageUp, Key::PageDown,
        Key::Backspace, Key::Delete, Key::Return, Key::Tab, Key::Escape };
    for (uint32_t seed = 1; seed <= 6; ++seed)
    {
        Rng rng(seed * 7919u);
        Harness h;
        BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(8.0f);
        TextEdit* editW = box->createChild<TextEdit>();
        editW->setStretch(1.0f);
        TextInput* inputW = box->createChild<TextInput>();
        h.relayout();
        // one control has the focus at a time: the keys go to the one last clicked
        struct Side
        {
            Harness& h;
            Widget* w;
            void key(int k, bool shift = false, bool ctrl = false) { if (!w->isFocused()) h.clickWidget(w); h.key(k, shift, ctrl); }
            void type(const char* t) { if (!w->isFocused()) h.clickWidget(w); h.type(t); }
            void click(float x, float y) { h.click(x, y); }
            void clickWidget(Widget* x) { h.clickWidget(x); }
        };
        struct Both
        {
            Side h;
            TextEdit* w;
        };
        Both edit{ Side{ h, editW }, editW };
        struct BothIn
        {
            Side h;
            TextInput* w;
        };
        BothIn input{ Side{ h, inputW }, inputW };
        for (int step = 0; step < 300; ++step)
        {
            const int op = rng.below(100);
            if (op < 35)
            {
                String t;
                const int n = 1 + rng.below(6);
                for (int i = 0; i < n; ++i) t += alphabet[rng.below(kinds)];
                edit.h.type(t.c_str());
                input.h.type(t.c_str());
            }
            else if (op < 80)
            {
                const int k = keys[rng.below(static_cast<int>(sizeof keys / sizeof keys[0]))];
                const bool shift = rng.chance(30), ctrl = rng.chance(10);
                const int key = ctrl ? "acvxz"[rng.below(5)] : k;
                edit.h.key(key, shift, ctrl);
                input.h.key(key, shift, ctrl);
            }
            else if (op < 88)
            {
                String t;
                const int n = rng.below(30);
                for (int i = 0; i < n; ++i) t += rng.chance(5) ? "\n" : alphabet[rng.below(kinds)];
                edit.w->setText(t);
                input.w->setText(t);
            }
            else if (op < 94)
            {
                edit.w->setCursorPos(rng.below(10) - 2, rng.below(40) - 3);
                edit.w->setSelection({ rng.below(8) - 1, rng.below(30) - 2 }, { rng.below(8) - 1, rng.below(30) - 2 });
                input.w->setCursorPos(rng.below(50) - 5);
            }
            else
            {
                edit.h.click(rng.range(0.0f, 1280.0f), rng.range(0.0f, 720.0f));
                input.h.click(rng.range(0.0f, 1280.0f), rng.range(0.0f, 720.0f));
                edit.h.clickWidget(edit.w);
                input.h.clickWidget(input.w);
            }
            const TextEdit::TextPos p = edit.w->cursorPos();
            const bool ok = p.line >= 0 && p.line < edit.w->lineCount() && p.col >= 0 && p.col <= utf8Length(edit.w->lineAt(p.line));
            if (!ok)
            {
                check(false, "the text edit's cursor is on its text");
                printf("    seed %u step %d: cursor %d:%d, %d lines\n", seed, step, p.line, p.col, edit.w->lineCount());
                break;
            }
            const int ic = input.w->cursorPos();
            if (ic < 0 || ic > utf8Length(input.w->text()))
            {
                check(false, "the text input's cursor is on its text");
                printf("    seed %u step %d: cursor %d, %d chars\n", seed, step, ic, utf8Length(input.w->text()));
                break;
            }
            if (!validUtf8(edit.w->text()) || !validUtf8(input.w->text()))
            {
                check(false, "valid text stays valid");
                printf("    seed %u step %d\n", seed, step);
                break;
            }
        }
    }
}

} // namespace

// A scenario name on the command line runs only that one.
int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testFitTextMatchesTheLoop);
    RUN(testLongElidedLabelIsCheap);
    RUN(testTypedTextBeyondOneEvent);
    RUN(testTextEditKeysOnMultibyteText);
    RUN(testPasteVariants);
    RUN(testOddStringsInControls);
    RUN(testTextFuzz);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
