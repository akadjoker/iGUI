// Round 10, the clipboard of the widgets that take text (docs/ROUND10.md, section 7): Shift+Insert pastes, Ctrl+Insert copies and Shift+Delete cuts next to
// Ctrl+V, Ctrl+C and Ctrl+X; the context menu of the right button (Cut, Copy, Paste, Delete, Select All, each disabled where it does not apply); the
// middle button pastes at the click, from the PRIMARY selection when the adapter has one and from the clipboard when it has not, and a selection made by the
// person becomes the PRIMARY one. Driven by injected input through the headless frame loop, for TextInput, TextEdit and CodeEditor (undo), the search field,
// the console input line, the cells of the grids and the property grid, and the bar of the code editor. With ROUND10_SHOTS=<directory> it writes a sheet of the
// context menu of each preset (PPM). No standard library: ct and C only.

#include "RasterScreen.hpp"

#include <igui/widgets/AppWidgets.hpp>
#include <igui/widgets/CodeEditor.hpp>
#include <igui/widgets/ConsoleWidget.hpp>
#include <igui/widgets/DataWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/MenuWidgets.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/TreePropertyColorWidgets.hpp>

#include <stdlib.h>

using namespace rh;

namespace
{

String& primaryText()
{
    static String text;
    return text;
}

// An adapter with a PRIMARY selection (the IO hooks the zen adapter keeps for it): what a selection sets and a middle click reads.
void withPrimary(Harness& h)
{
    h.io().setPrimaryText = [](const char* text) { primaryText() = String(text ? text : ""); };
    h.io().getPrimaryText = []() { return primaryText(); };
    h.frame();
}

// A scene of one text input on a 500 x 200 window, focused.
struct Field
{
    Harness h;
    Widget* holder;
    TextInput* input;
    int changes = 0;
    explicit Field(const char* text, TextInput::Mode mode = TextInput::Mode::Normal) : h(500.0f, 200.0f)
    {
        holder = h.root()->createChild<Widget>();
        holder->setRect({ 0, 0, 500, 200 });
        input = holder->createChild<TextInput>(String(text), mode);
        input->setRect({ 20, 20, 300, 28 });
        h.relayout();
        h.clickWidget(input);
        input->textChanged.connect([this](const String&) { ++changes; });
        clipboardText() = "";
        primaryText() = "";
    }
    // the characters [a, b) selected, the way a person does it
    void select(int a, int b)
    {
        input->setCursorPos(a);
        for (int i = a; i < b; ++i) h.key(Key::Right, true);
    }
    // a click inside the first character, and one far to the right of the text
    float insideX() const
    {
        return input->absoluteRect().x + 10.0f;
    }
    float outsideX() const
    {
        return input->absoluteRect().x + input->absoluteRect().w - 6.0f;
    }
    float midY() const
    {
        return input->absoluteRect().y + input->absoluteRect().h * 0.5f;
    }
};

// ── the keys ──────────────────────────────────────────────────────────────────────────────────────────────────────────

void testInputKeys()
{
    printf("-- TextInput: Ctrl+C / Ctrl+Insert, Ctrl+X / Shift+Delete, Ctrl+V / Shift+Insert\n");
    {
        Field f("hello world");
        f.select(6, 11);
        f.h.key(Key::C, false, true);
        check(clipboardText() == "world" && f.input->text() == "hello world", "Ctrl+C copies the selection and leaves the text");
        clipboardText() = "";
        f.h.key(Key::Insert, false, true);
        check(clipboardText() == "world" && f.input->text() == "hello world", "Ctrl+Insert copies too");
        f.h.key(Key::X, false, true);
        check(clipboardText() == "world" && f.input->text() == "hello " && f.changes == 1, "Ctrl+X cuts: on the clipboard, out of the text, textChanged once");
    }
    {
        Field f("hello world");
        f.select(6, 11);
        f.h.key(Key::Delete, true, false);
        check(clipboardText() == "world" && f.input->text() == "hello " && f.changes == 1, "Shift+Delete cuts the same way");
        check(!f.input->hasSelection() && f.input->cursorPos() == 6, "and leaves the cursor where the selection was");
        // a plain Delete and a plain Insert are what they were
        clipboardText() = "keep";
        f.input->setCursorPos(0);
        f.h.key(Key::Delete);
        check(f.input->text() == "ello " && clipboardText() == "keep", "a Delete with no Shift deletes a character and touches no clipboard");
        f.h.key(Key::Insert);
        check(f.input->text() == "ello ", "an Insert alone does nothing to the text");
    }
    {
        Field f("hello world");
        clipboardText() = "XY";
        f.select(0, 5);
        f.h.key(Key::V, false, true);
        check(f.input->text() == "XY world" && f.input->cursorPos() == 2 && !f.input->hasSelection() && f.changes == 1, "Ctrl+V replaces the selection");
        f.input->setText("hello world");
        f.select(0, 5);
        f.changes = 0;
        f.h.key(Key::Insert, true, false);
        check(f.input->text() == "XY world" && f.changes == 1, "Shift+Insert replaces the selection too");
        f.input->setCursorPos(2);
        f.h.key(Key::Insert, true, false);
        check(f.input->text() == "XYXY world", "with nothing selected it goes in at the cursor");
        clipboardText() = "a\r\nb\nc";
        f.input->setText("");
        f.h.key(Key::Insert, true, false);
        check(f.input->text() == "abc", "the line breaks of what is pasted are dropped in a one line field");
        clipboardText() = "";
        f.input->setText("same");
        f.input->setCursorPos(4);
        f.h.key(Key::Insert, true, false);
        check(f.input->text() == "same", "an empty clipboard pastes nothing");
    }
    {
        // read only: copy is allowed, cut and paste are refused (and consumed: Shift+Delete is not a Delete)
        Field f("locked", TextInput::Mode::ReadOnly);
        f.input->selectAll();
        f.h.key(Key::C, false, true);
        check(clipboardText() == "locked", "read only: Ctrl+C copies");
        clipboardText() = "";
        f.h.key(Key::Insert, false, true);
        check(clipboardText() == "locked", "read only: Ctrl+Insert copies");
        clipboardText() = "keep";
        f.h.key(Key::X, false, true);
        f.h.key(Key::Delete, true, false);
        check(f.input->text() == "locked" && clipboardText() == "keep" && f.changes == 0, "read only: Ctrl+X and Shift+Delete cut nothing");
        f.h.key(Key::V, false, true);
        f.h.key(Key::Insert, true, false);
        check(f.input->text() == "locked" && f.changes == 0, "read only: Ctrl+V and Shift+Insert paste nothing");
    }
    {
        // a password: nothing leaves the field, a paste goes in
        Field f("secret", TextInput::Mode::Password);
        clipboardText() = "keep";
        f.input->selectAll();
        f.h.key(Key::C, false, true);
        f.h.key(Key::Insert, false, true);
        check(clipboardText() == "keep", "password: Ctrl+C and Ctrl+Insert copy nothing");
        f.h.key(Key::X, false, true);
        f.h.key(Key::Delete, true, false);
        check(clipboardText() == "keep" && f.input->text() == "secret" && f.changes == 0, "password: Ctrl+X and Shift+Delete cut nothing");
        clipboardText() = "new";
        f.h.key(Key::Insert, true, false);
        check(f.input->text() == "new", "password: Shift+Insert pastes over the selection");
    }
    {
        Field f("", TextInput::Mode::NumberOnly);
        clipboardText() = "12a";
        f.h.key(Key::Insert, true, false);
        check(f.input->text().empty(), "number only: a paste with a letter in it is refused");
        clipboardText() = "123";
        f.h.key(Key::Insert, true, false);
        check(f.input->text() == "123", "number only: digits go in");
    }
    {
        Field f("hello world");
        f.select(0, 5);
        f.h.key(Key::A, false, true);
        check(f.input->selectedText() == "hello world", "Ctrl+A selects all");
    }
}

// ── the context menu ──────────────────────────────────────────────────────────────────────────────────────────────────

Menu* openMenu(Harness& h, float x, float y)
{
    h.click(x, y, 1);
    h.frames(14); // the popup opens in a tenth of a second
    Widget* popup = WidgetApp::instance().popup();
    return popup ? static_cast<Menu*>(popup) : nullptr;
}

void clickItem(Harness& h, Menu* menu, int index)
{
    const Rect r = menu->actionRect(index);
    h.click(r.x + r.w * 0.5f, r.y + r.h * 0.5f, 0);
    h.frames(14);
}

// the enabled state of the five items: Cut, Copy, Paste, Delete, Select All (the separator is item 4)
struct Flags
{
    bool cut, copy, paste, del, all;
};
bool flagsAre(Menu* menu, const Flags& f)
{
    return menu && menu->actionCount() == 6 && menu->action(0)->isEnabled() == f.cut && menu->action(1)->isEnabled() == f.copy &&
           menu->action(2)->isEnabled() == f.paste && menu->action(3)->isEnabled() == f.del && menu->action(5)->isEnabled() == f.all;
}

void testInputMenu()
{
    printf("-- TextInput: the context menu\n");
    {
        Field f("hello world");
        f.select(0, 5);
        clipboardText() = "P";
        Menu* menu = openMenu(f.h, f.insideX(), f.midY());
        check(menu != nullptr, "a right click opens a menu");
        check(menu && WidgetApp::instance().popupOwner() == f.input, "the field is its owner");
        check(menu && menu->actionCount() == 6 && menu->action(0)->text() == "Cut" && menu->action(1)->text() == "Copy" && menu->action(2)->text() == "Paste" &&
                        menu->action(3)->text() == "Delete" && menu->action(4)->isSeparator() && menu->action(5)->text() == "Select All",
                "Cut, Copy, Paste, Delete, a separator and Select All");
        check(f.input->selectedText() == "hello", "a right click inside the selection keeps it");
        check(flagsAre(menu, { true, true, true, true, true }), "with a selection and a clipboard every item is enabled");
        clickItem(f.h, menu, 2);
        check(f.input->text() == "P world" && WidgetApp::instance().popup() == nullptr, "Paste replaces the selection and closes the menu");
    }
    {
        Field f("hello world");
        f.select(0, 5);
        clipboardText() = "";
        Menu* menu = openMenu(f.h, f.insideX(), f.midY());
        check(flagsAre(menu, { true, true, false, true, true }), "an empty clipboard disables Paste");
        clickItem(f.h, menu, 0);
        check(clipboardText() == "hello" && f.input->text() == " world" && f.changes == 1, "Cut puts the selection on the clipboard and takes it out");
    }
    {
        Field f("hello world");
        f.select(0, 5);
        Menu* menu = openMenu(f.h, f.insideX(), f.midY());
        clickItem(f.h, menu, 1);
        check(clipboardText() == "hello" && f.input->text() == "hello world" && f.changes == 0, "Copy copies");
        f.select(0, 5);
        clipboardText() = "keep";
        menu = openMenu(f.h, f.insideX(), f.midY());
        clickItem(f.h, menu, 3);
        check(f.input->text() == " world" && clipboardText() == "keep" && f.changes == 1, "Delete takes the selection out and keeps the clipboard");
        menu = openMenu(f.h, f.insideX(), f.midY());
        check(flagsAre(menu, { false, false, true, false, true }), "with no selection Cut, Copy and Delete are disabled");
        clickItem(f.h, menu, 5);
        check(f.input->selectedText() == " world", "Select All selects everything");
        check(primaryText() == "", "(and with no PRIMARY hook nothing is kept for one)");
    }
    {
        // a right click outside the selection puts the cursor there and drops the selection
        Field f("hello world");
        f.select(0, 5);
        Menu* menu = openMenu(f.h, f.outsideX(), f.midY());
        check(menu != nullptr && !f.input->hasSelection() && f.input->cursorPos() == 11, "a right click outside the selection moves the cursor there");
        check(flagsAre(menu, { false, false, false, false, true }), "and Cut, Copy and Delete are disabled");
        f.h.click(30.0f, 190.0f, 0);
        f.h.frames(14);
        check(WidgetApp::instance().popup() == nullptr, "a click outside closes the menu");
    }
    {
        Field f("locked", TextInput::Mode::ReadOnly);
        f.input->selectAll();
        clipboardText() = "P";
        Menu* menu = openMenu(f.h, f.insideX(), f.midY());
        check(flagsAre(menu, { false, true, false, false, true }), "read only: Copy and Select All only");
    }
    {
        Field f("secret", TextInput::Mode::Password);
        f.input->selectAll();
        clipboardText() = "P";
        Menu* menu = openMenu(f.h, f.insideX(), f.midY());
        check(flagsAre(menu, { false, false, true, true, true }), "password: no Cut and no Copy (Delete takes nothing out of the field: it stays)");
        clickItem(f.h, menu, 2);
        check(f.input->text() == "P", "password: Paste works");
    }
    {
        // a field with no text has nothing to select
        Field f("");
        Menu* menu = openMenu(f.h, f.insideX(), f.midY());
        check(flagsAre(menu, { false, false, false, false, false }), "an empty field: nothing is enabled with an empty clipboard");
    }
}

// the widget goes while the menu is open, or from the handler of the paste (meant to run under the address sanitizer)
void testMenuAndDeletion()
{
    printf("-- the menu and a widget that goes\n");
    {
        Field f("hello world");
        f.select(0, 5);
        Menu* menu = openMenu(f.h, f.insideX(), f.midY());
        check(menu != nullptr && WidgetApp::instance().popup() == menu, "the menu is open");
        f.holder->removeChild(f.input);
        f.h.frames(4);
        check(WidgetApp::instance().popup() == nullptr, "a field taken out of the window while its menu is open closes the menu");
        f.h.frames(14);
    }
    {
        Field f("hello world");
        f.select(0, 5);
        clipboardText() = "P";
        Widget* holder = f.holder;
        TextInput* input = f.input;
        input->textChanged.connect([holder, input](const String&) { holder->removeChild(input); });
        Menu* menu = openMenu(f.h, f.insideX(), f.midY());
        clickItem(f.h, menu, 2);
        f.h.frames(4);
        check(WidgetApp::instance().popup() == nullptr, "a handler of textChanged that removes the field from Paste: nothing is left open");
    }
    {
        Field f("hello world");
        f.select(0, 5);
        Widget* holder = f.holder;
        TextInput* input = f.input;
        input->textChanged.connect([holder, input](const String&) { holder->removeChild(input); });
        Menu* menu = openMenu(f.h, f.insideX(), f.midY());
        clickItem(f.h, menu, 3);
        f.h.frames(4);
        check(WidgetApp::instance().popup() == nullptr, "... and from Delete");
    }
    {
        Field f("hello world");
        f.select(0, 5);
        Widget* holder = f.holder;
        TextInput* input = f.input;
        input->textChanged.connect([holder, input](const String&) { holder->removeChild(input); });
        f.h.key(Key::X, false, true);
        f.h.frames(3);
        check(true, "... and from Ctrl+X");
    }
}

// ── PRIMARY and the middle button ─────────────────────────────────────────────────────────────────────────────────────

void testInputMiddleAndPrimary()
{
    printf("-- TextInput: the middle button and the PRIMARY selection\n");
    {
        Field f("hello world");
        withPrimary(f.h);
        primaryText() = "PRIM";
        clipboardText() = "CLIP";
        f.h.click(f.insideX() - 6.0f, f.midY(), 2); // at the very start of the text
        check(f.input->text() == "PRIMhello world" && f.changes == 1, "a middle click pastes the PRIMARY selection at the click");
        check(clipboardText() == "CLIP", "and does not touch the clipboard");
        f.h.click(f.outsideX(), f.midY(), 2);
        check(f.input->text() == "PRIMhello worldPRIM", "at the end of the text when the click is there");
    }
    {
        // no PRIMARY (the default): the clipboard is pasted
        Field f("abc");
        clipboardText() = "CLIP";
        f.h.click(f.outsideX(), f.midY(), 2);
        check(f.input->text() == "abcCLIP", "with no PRIMARY a middle click pastes the clipboard");
    }
    {
        Field f("locked", TextInput::Mode::ReadOnly);
        clipboardText() = "CLIP";
        f.h.click(f.outsideX(), f.midY(), 2);
        check(f.input->text() == "locked" && f.changes == 0, "read only: a middle click pastes nothing");
    }
    {
        // selecting sets the PRIMARY selection: a drag of the mouse, Ctrl+A, the menu's Select All; a password sets nothing
        Field f("hello world");
        withPrimary(f.h);
        clipboardText() = "CLIP";
        const Rect r = f.input->absoluteRect();
        f.h.moveTo(r.x + 8.0f, f.midY());
        f.h.down(0);
        f.h.moveTo(r.x + 40.0f, f.midY());
        check(primaryText() == "", "while the mouse is still down nothing is published");
        f.h.up(0);
        check(f.input->hasSelection() && primaryText() == f.input->selectedText() && primaryText().size() >= 3, "a selection by the mouse is the PRIMARY selection");
        check(clipboardText() == "CLIP", "and not the clipboard's");
        primaryText() = "";
        f.h.key(Key::A, false, true);
        check(primaryText() == "hello world", "Ctrl+A too");
        Field p("secret", TextInput::Mode::Password);
        withPrimary(p.h);
        p.h.key(Key::A, false, true);
        check(primaryText() == "", "a password never sets it");
    }
}

// ── TextEdit and CodeEditor ───────────────────────────────────────────────────────────────────────────────────────────

struct Pad
{
    Harness h;
    Widget* holder;
    TextEdit* edit;
    int changes = 0;
    explicit Pad(const char* text, bool code = false) : h(500.0f, 300.0f)
    {
        holder = h.root()->createChild<Widget>();
        holder->setRect({ 0, 0, 500, 300 });
        edit = code ? static_cast<TextEdit*>(holder->createChild<CodeEditor>()) : holder->createChild<TextEdit>();
        edit->setShowLineNumbers(false);
        edit->setText(String(text));
        edit->setRect({ 20, 20, 400, 200 });
        h.relayout();
        h.clickWidget(edit);
        edit->textChanged.connect([this] { ++changes; });
        clipboardText() = "";
        primaryText() = "";
    }
    float leftX() const
    {
        return edit->absoluteRect().x + 8.0f;
    }
    float rightX() const
    {
        return edit->absoluteRect().x + edit->absoluteRect().w - 20.0f;
    }
    float rowY(int row) const
    {
        return edit->absoluteRect().y + 4.0f + (static_cast<float>(row) + 0.5f) * 16.0f;
    }
};

void testPadKeys(bool code)
{
    printf("-- %s: the keys, the menu and the middle button\n", code ? "CodeEditor" : "TextEdit");
    {
        Pad p("one two\nthree four", code);
        p.edit->setSelection({ 0, 4 }, { 1, 5 });
        p.h.key(Key::C, false, true);
        check(clipboardText() == "two\nthree" && p.edit->text() == "one two\nthree four", "Ctrl+C copies a selection of two lines");
        clipboardText() = "";
        p.h.key(Key::Insert, false, true);
        check(clipboardText() == "two\nthree", "Ctrl+Insert too");
        p.h.key(Key::Delete, true, false);
        check(clipboardText() == "two\nthree" && p.edit->text() == "one  four" && p.changes >= 1, "Shift+Delete cuts it");
    }
    {
        Pad p("one two\nthree four", code);
        clipboardText() = "X\nY";
        p.edit->setSelection({ 0, 0 }, { 0, 3 });
        p.h.key(Key::Insert, true, false);
        check(p.edit->text() == "X\nY two\nthree four", "Shift+Insert replaces the selection with two lines");
        check(p.edit->cursorPos().line == 1 && p.edit->cursorPos().col == 1 && !p.edit->hasSelection(), "and puts the cursor after them");
        if (code)
        {
            CodeEditor* editor = static_cast<CodeEditor*>(p.edit);
            editor->undo();
            check(p.edit->text() == "one two\nthree four", "CodeEditor: undo restores what the paste replaced");
            editor->redo();
            check(p.edit->text() == "X\nY two\nthree four", "CodeEditor: redo pastes again");
            editor->undo();
            p.edit->setSelection({ 0, 0 }, { 0, 3 });
            p.h.key(Key::X, false, true);
            check(p.edit->text() == " two\nthree four" && clipboardText() == "one", "CodeEditor: Ctrl+X cuts");
            editor->undo();
            check(p.edit->text() == "one two\nthree four", "CodeEditor: undo puts the cut text back");
        }
    }
    {
        Pad p("locked text", code);
        p.edit->setReadOnly(true);
        p.edit->selectAll();
        clipboardText() = "";
        p.h.key(Key::Insert, false, true);
        check(clipboardText() == "locked text", "read only: Ctrl+Insert copies");
        clipboardText() = "keep";
        p.h.key(Key::Delete, true, false);
        p.h.key(Key::X, false, true);
        p.h.key(Key::Insert, true, false);
        p.h.key(Key::V, false, true);
        check(p.edit->text() == "locked text" && clipboardText() == "keep" && p.changes == 0, "read only: cut and paste are refused");
        Menu* menu = openMenu(p.h, p.leftX(), p.rowY(0));
        check(flagsAre(menu, { false, true, false, false, true }), "read only: the menu has Copy and Select All only");
        p.h.click(480.0f, 290.0f, 0);
        p.h.frames(14);
    }
    {
        Pad p("one two\nthree four", code);
        p.edit->setSelection({ 0, 0 }, { 0, 3 });
        clipboardText() = "P";
        Menu* menu = openMenu(p.h, p.leftX(), p.rowY(0));
        check(menu != nullptr && WidgetApp::instance().popupOwner() == p.edit && p.edit->selectedText() == "one", "a right click inside the selection opens the menu and keeps it");
        check(flagsAre(menu, { true, true, true, true, true }), "every item is enabled");
        clickItem(p.h, menu, 0);
        check(clipboardText() == "one" && p.edit->text() == " two\nthree four", "Cut");
        menu = openMenu(p.h, p.leftX() + 200.0f, p.rowY(5));
        check(flagsAre(menu, { false, false, true, false, true }), "a right click below the text drops the selection: only Paste and Select All");
        clickItem(p.h, menu, 2);
        check(p.edit->lineAt(1) == "three fourone" && p.edit->lineAt(0) == " two", "Paste (what Cut put on the clipboard) goes in at the click, the end of the last line");
        menu = openMenu(p.h, p.leftX(), p.rowY(0));
        clickItem(p.h, menu, 5);
        check(p.edit->selectedText() == p.edit->text(), "Select All selects the whole text");
    }
    {
        Pad p("one two\nthree four", code);
        withPrimary(p.h);
        primaryText() = "PRIM";
        clipboardText() = "CLIP";
        p.h.click(p.leftX() - 6.0f, p.rowY(1), 2);
        check(p.edit->lineAt(1) == "PRIMthree four" && p.edit->lineAt(0) == "one two", "a middle click pastes the PRIMARY selection at the click, on the line clicked");
        check(clipboardText() == "CLIP", "and leaves the clipboard");
        // a double click selects a word: PRIMARY
        primaryText() = "";
        p.h.click(p.leftX() + 4.0f, p.rowY(0), 0);
        p.h.click(p.leftX() + 4.0f, p.rowY(0), 0);
        check(primaryText() == "one", "a selection made with the mouse (a double click on a word) is the PRIMARY selection");
    }
    {
        Pad p("abc", code);
        clipboardText() = "CLIP";
        p.h.click(p.rightX(), p.rowY(0), 2);
        check(p.edit->lineAt(0) == "abcCLIP", "with no PRIMARY a middle click pastes the clipboard");
    }
    {
        // the handler of textChanged takes the editor out: nothing is touched after it
        Pad p("one two", code);
        Widget* holder = p.holder;
        TextEdit* edit = p.edit;
        edit->textChanged.connect([holder, edit] { holder->removeChild(edit); });
        edit->setSelection({ 0, 0 }, { 0, 3 });
        clipboardText() = "P";
        p.h.key(Key::Insert, true, false);
        p.h.frames(3);
        check(true, "a handler that removes the editor on Shift+Insert");
    }
}

// ── pictures ──────────────────────────────────────────────────────────────────────────────────────────────────────────

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

// the context menu of a text field in each preset: an input with a selection and the menu open, and the same in a disabled state (a password)
void testPictures(const char* dir)
{
    printf("-- the context menu of each preset, into %s\n", dir);
    const char* presets[] = { "dark", "light", "xp", "macos", "macos-dark" };
    for (int i = 0; i < 5; ++i)
    {
        Theme::instance() = Theme::byName(presets[i]);
        Shot sheet;
        sheet.w = 2 * 300;
        sheet.h = 190;
        sheet.px.resize(static_cast<size_t>(sheet.w) * static_cast<size_t>(sheet.h));
        for (size_t k = 0; k < sheet.px.size(); ++k)
            sheet.px[k] = 0xff000000u | (static_cast<uint32_t>(Theme::instance().bgColor.r) << 16) | (static_cast<uint32_t>(Theme::instance().bgColor.g) << 8) |
                          Theme::instance().bgColor.b;
        for (int pass = 0; pass < 2; ++pass)
        {
            Screen screen(1280.0f, 720.0f);
            screen.useDynamicPage();
            Harness& h = screen.h;
            Widget* holder = h.root()->createChild<Widget>();
            holder->setRect({ 0, 0, 1280, 720 });
            TextInput* input = holder->createChild<TextInput>(String(pass == 0 ? "Some text to select" : "a password"),
                    pass == 0 ? TextInput::Mode::Normal : TextInput::Mode::Password);
            input->setRect({ 20, 20, 260, Theme::instance().inputHeight });
            h.relayout();
            h.clickWidget(input);
            input->setCursorPos(5);
            for (int k = 0; k < 7; ++k) h.key(Key::Right, true);
            if (pass == 1) input->selectAll();
            clipboardText() = "clipboard";
            h.click(input->absoluteRect().x + 40.0f, input->absoluteRect().y + 8.0f, 1);
            h.frames(14);
            const Shot shot = screen.take();
            for (int y = 0; y < 190; ++y)
                for (int x = 0; x < 300; ++x)
                    sheet.px[static_cast<size_t>(y) * static_cast<size_t>(sheet.w) + static_cast<size_t>(pass * 300 + x)] =
                            shot.px[static_cast<size_t>(10 + y) * static_cast<size_t>(shot.w) + static_cast<size_t>(10 + x)];
            WidgetApp::instance().closePopup();
            h.frames(3);
        }
        char name[64];
        snprintf(name, sizeof name, "textmenu-%s", presets[i]);
        writePicture(dir, name, sheet);
    }
    Theme::instance() = Theme::dark();
}

} // namespace

int main()
{
    testInputKeys();
    testInputMenu();
    testMenuAndDeletion();
    testInputMiddleAndPrimary();
    testPadKeys(false);
    testPadKeys(true);
    if (const char* dir = getenv("ROUND10_SHOTS")) testPictures(dir);
    Theme::instance() = Theme::dark();
    shutdown();
    if (failures() != 0)
    {
        printf("FAILED: %d\n", failures());
        return 1;
    }
    printf("PASS\n");
    return 0;
}
