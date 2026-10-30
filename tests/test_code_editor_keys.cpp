// The retained CodeEditor's keyboard: undo groups and the recorded replace,
// the inline bars (find, replace, go to line, go to symbol), the commands,
// the extra cursors, bookmarks, the jump list and the document formatter.

#include <igui/widgets/CodeEditor.hpp>
#include <igui/widgets/WidgetApp.hpp>

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

struct Bench
{
    WidgetApp& app = WidgetApp::instance();
    IO io;
    DrawData data;
    Widget* stage = nullptr;
    CodeEditor* editor = nullptr;

    explicit Bench(const char* text, const char* file = "test.py")
    {
        io.displayWidth = 800.0f;
        io.displayHeight = 600.0f;
        io.deltaTime = 1.0f / 60.0f;
        stage = app.addStage("keys");
        app.setStage("keys", TransitionType::None);
        editor = stage->createChild<CodeEditor>();
        editor->setRect({ 0.0f, 0.0f, 600.0f, 300.0f });
        editor->setHighlighterForFile(file);
        editor->setText(text);
        app.setFocused(editor);
        frame();
    }
    ~Bench()
    {
        app.removeStage("keys");
    }
    void frame()
    {
        app.update(io);
        app.paint(data);
        io.keyEvents.clear();
        io.inputChars.clear();
    }
    void key(int code, bool ctrl = false, bool shift = false, bool alt = false)
    {
        IO::KeyEvent e;
        e.key = code;
        e.ctrl = ctrl;
        e.shift = shift;
        e.alt = alt;
        e.down = true;
        io.keyEvents.push_back(e);
        frame();
    }
    void type(const char* text)
    {
        for (const char* p = text; *p; ++p)
        {
            io.inputChars.push_back(static_cast<uint32_t>(static_cast<unsigned char>(*p)));
            frame();
        }
    }
    const String& line(int n) const
    {
        return editor->lineAt(n);
    }
    int cursorLine() const
    {
        return editor->cursorPos().line;
    }
    int cursorCol() const
    {
        return editor->cursorPos().col;
    }
};

void testUndoKeysAndGroups()
{
    Bench b("one\ntwo\nthree\n");
    b.editor->setCursorPos(0, 3);
    b.type("!");
    check(b.line(0) == "one!", "typing");
    b.key(Key::Z, true);
    check(b.line(0) == "one", "Ctrl+Z undoes it");
    b.key(Key::Y, true);
    check(b.line(0) == "one!", "Ctrl+Y redoes it");
    b.key(Key::Z, true);
    b.key(Key::Z, true, true);
    check(b.line(0) == "one!", "Ctrl+Shift+Z redoes too");
    b.key(Key::Z, true);
    check(b.line(0) == "one", "back to the start");

    b.editor->setSelection({ 0, 0 }, { 2, 0 });
    b.key(Key::Tab);
    check(b.line(0) == "\tone" && b.line(1) == "\ttwo" && b.line(2) == "three", "Tab with lines selected indents those lines only");
    check(b.editor->selectionStart().line == 0 && b.editor->selectionEnd().line == 2, "and keeps the selection over them");
    b.key(Key::Z, true);
    check(b.line(0) == "one" && b.line(1) == "two", "one Ctrl+Z takes the whole indent back");
    b.key(Key::Y, true);
    check(b.line(0) == "\tone" && b.line(1) == "\ttwo", "and one Ctrl+Y brings it back");
    b.editor->setSelection({ 0, 0 }, { 2, 0 });
    b.key(Key::Tab, false, true);
    check(b.line(0) == "one" && b.line(1) == "two", "Shift+Tab unindents them");
    b.editor->clearSelection();
    b.editor->setCursorPos(2, 2);
    b.key(Key::Tab, false, true);
    check(b.line(2) == "three", "Shift+Tab on an unindented line changes nothing");
    b.editor->setText("    four\n");
    b.editor->setCursorPos(0, 6);
    b.key(Key::Tab, false, true);
    check(b.line(0) == "four" && b.cursorCol() == 2, "Shift+Tab takes one level of spaces and keeps the cursor on its character");
    b.key(Key::Z, true);
    check(b.line(0) == "    four", "an unindent is undoable");
}

void testFindBar()
{
    Bench b("alpha beta\ngamma alpha\nALPHA delta\n");
    b.key(Key::F, true);
    check(b.editor->activeBar() == CodeEditor::Bar::Find && b.editor->barText().empty(), "Ctrl+F opens an empty find bar");
    b.type("alp");
    check(b.editor->barText() == "alp" && b.editor->hasSelection() && b.editor->selectionStart() == TextEdit::TextPos{ 0, 0 },
          "typing searches from where the cursor was and selects the first match");
    b.type("ha");
    check(b.editor->selectionStart() == TextEdit::TextPos{ 0, 0 } && b.editor->selectedText() == "alpha", "more letters narrow in place, the match does not run ahead");
    b.key(Key::Return);
    check(b.editor->selectionStart() == TextEdit::TextPos{ 1, 6 }, "Return goes to the next match");
    b.key(Key::Return);
    check(b.editor->selectionStart() == TextEdit::TextPos{ 2, 0 }, "case does not matter by default");
    b.key(Key::C, false, false, true);
    check(b.editor->selectionStart() == TextEdit::TextPos{ 0, 0 }, "Alt+C makes it match case, from the origin again");
    b.key(Key::Return);
    b.key(Key::Return);
    check(b.editor->selectionStart() == TextEdit::TextPos{ 0, 0 }, "and wraps around past the last match");
    b.key(Key::Return, false, true);
    check(b.editor->selectionStart() == TextEdit::TextPos{ 1, 6 }, "Shift+Return goes back");
    b.key(Key::Backspace);
    b.key(Key::Backspace);
    check(b.editor->barText() == "alp", "Backspace edits the term");
    b.key(Key::Escape);
    check(b.editor->activeBar() == CodeEditor::Bar::None, "Escape closes the bar");
    b.key(Key::F3);
    check(b.editor->hasSelection() && b.editor->selectedText() == "alp", "F3 finds the next with the bar closed");
    b.key(Key::F3, false, true);
    check(b.editor->selectionStart() == TextEdit::TextPos{ 0, 0 }, "Shift+F3 the previous one");
    b.editor->setSelection({ 0, 6 }, { 0, 10 });
    b.key(Key::F, true);
    check(b.editor->barText() == "beta", "Ctrl+F over a selection looks for it");
    b.key(Key::Escape);
}

void testReplaceBar()
{
    Bench b("cat dog cat\ncat\n");
    b.key(Key::H, true);
    check(b.editor->activeBar() == CodeEditor::Bar::Replace, "Ctrl+H opens the replace bar");
    b.type("cat");
    b.key(Key::Tab);
    b.type("cow");
    check(b.editor->barText() == "cat" && b.editor->barReplaceText() == "cow", "Tab switches to the replace field");
    b.key(Key::Return);
    check(b.line(0) == "cow dog cat" && b.editor->selectedText() == "cat", "Return replaces the match and selects the next");
    b.key(Key::Return, true);
    check(b.line(0) == "cow dog cow" && b.line(1) == "cow", "Ctrl+Return replaces every match");
    b.key(Key::Escape);
    b.key(Key::Z, true);
    check(b.line(0) == "cow dog cat" && b.line(1) == "cat", "a replace-all is one undo step");
    b.key(Key::Z, true);
    check(b.line(0) == "cat dog cat", "and the single replace another");
    b.key(Key::Y, true);
    b.key(Key::Y, true);
    check(b.line(0) == "cow dog cow" && b.line(1) == "cow", "redo brings both back");
}

void testGotoLineAndSymbol()
{
    String text;
    for (int i = 0; i < 100; ++i)
    {
        char buf[64];
        snprintf(buf, sizeof buf, "def func_%02d():\n    return %d\n", i, i);
        text += buf;
    }
    Bench b(text.c_str());
    b.key(Key::G, true);
    b.type("5x1");
    check(b.editor->activeBar() == CodeEditor::Bar::GotoLine && b.editor->barText() == "51", "the go-to-line field takes digits only");
    b.key(Key::Return);
    check(b.editor->activeBar() == CodeEditor::Bar::None && b.cursorLine() == 50, "Return goes to that line (1-based)");
    b.frame();
    check(b.editor->firstVisibleLine() > 30 && b.editor->firstVisibleLine() <= 50, "and scrolls it into view");

    b.key(Key::O, true, true);
    check(b.editor->activeBar() == CodeEditor::Bar::GotoSymbol && b.editor->symbolMatches().size() == 100, "Ctrl+Shift+O lists every symbol");
    b.type("f7");
    check(b.editor->symbolMatches().size() == 19, "typing filters them (fuzzy: f7 matches func_07, func_17, ... and func_70, func_71, ...; func_77 once)");
    const ct::Vector<ig::syntax::Symbol>& symbols = b.editor->symbols();
    check(symbols[static_cast<size_t>(b.editor->symbolMatches()[0])].name == "func_07", "with no prefix or word-start match, text order");
    b.key(Key::Down);
    b.key(Key::Down);
    check(b.editor->symbolMatchSelected() == 2, "Down picks");
    b.key(Key::Return);
    check(b.editor->activeBar() == CodeEditor::Bar::None && b.cursorLine() == symbols[static_cast<size_t>(ig::syntax::findSymbol(symbols, "func_27"))].line,
          "Return reveals the picked symbol");
    b.key(Key::Left, false, false, true);
    check(b.cursorLine() == 50, "Alt+Left goes back to where the cursor was before");
    b.key(Key::Left, false, false, true);
    check(b.cursorLine() == 0, "and before that, the start");
    b.key(Key::Right, false, false, true);
    check(b.cursorLine() == 50, "Alt+Right forward again");
}

void testCommands()
{
    Bench b("first line\n    second\nthird (a[b]) end\n");
    b.editor->setCursorPos(0, 3);
    b.key(Key::L, true);
    check(b.editor->selectionStart() == TextEdit::TextPos{ 0, 0 } && b.editor->selectionEnd() == TextEdit::TextPos{ 1, 0 }, "Ctrl+L selects the line");
    b.key(Key::L, true);
    check(b.editor->selectionEnd() == TextEdit::TextPos{ 2, 0 }, "again, the next line too");
    b.editor->clearSelection();
    b.editor->setCursorPos(0, 2);
    b.key(Key::J, true);
    check(b.line(0) == "first line second" && b.cursorCol() == 10, "Ctrl+J joins the next line, its indentation dropped, one space between");
    b.key(Key::Z, true);
    check(b.line(0) == "first line" && b.line(1) == "    second", "a join is undoable");

    b.editor->setCursorPos(2, 7); // after "third (" : on 'a'
    b.editor->setCursorPos(2, 6); // on '('
    b.key(Key::M, true);
    check(b.cursorLine() == 2 && b.cursorCol() == 12, "Ctrl+M on '(' jumps after its ')'");
    b.key(Key::M, true);
    check(b.cursorCol() == 6, "and from after ')' back to '('");
    b.key(Key::M, true, true);
    check(b.editor->hasSelection() && b.editor->selectedText() == "(a[b])", "Ctrl+Shift+M selects through the pair");

    b.editor->setSelection({ 0, 0 }, { 0, 5 });
    b.key(Key::U, true, true);
    check(b.line(0) == "FIRST line" && b.editor->selectedText() == "FIRST", "Ctrl+Shift+U upper-cases the selection and keeps it");
    b.key(Key::U, true);
    check(b.line(0) == "first line", "Ctrl+U lower-cases it");
    b.key(Key::Z, true);
    b.key(Key::Z, true);
    check(b.line(0) == "first line", "both are undoable");
}

void testCursorsBookmarksDefinition()
{
    Bench b("x = 1\ny = 2\nz = 3\n\ndef helper():\n    return x\n\nhelper()\n");
    b.editor->setCursorPos(0, 4);
    b.key(Key::Down, true, false, true);
    b.key(Key::Down, true, false, true);
    check(b.editor->cursorCount() == 3, "Ctrl+Alt+Down twice: three cursors");
    b.type("0");
    check(b.line(0) == "x = 01" && b.line(1) == "y = 02" && b.line(2) == "z = 03", "typing goes to every cursor");
    b.key(Key::Escape);
    check(b.editor->cursorCount() == 1, "Escape leaves one");
    b.key(Key::Up, true, false, true);
    check(b.editor->cursorCount() == 1, "Ctrl+Alt+Up on the first line has nowhere to go");
    b.editor->setCursorPos(2, 1);
    b.key(Key::Up, true, false, true);
    check(b.editor->cursorCount() == 2 && b.editor->cursorPosAt(1).line == 1 && b.editor->cursorPosAt(1).col == 1, "Ctrl+Alt+Up adds one above, same column");
    b.key(Key::Escape);

    b.editor->setCursorPos(7, 2);
    b.key(Key::B, true);
    check(b.cursorLine() == 4, "Ctrl+B on a call goes to the def");
    b.key(Key::Left, false, false, true);
    check(b.cursorLine() == 7, "Alt+Left comes back");

    b.editor->setCursorPos(1, 0);
    b.key(Key::F2, true);
    b.editor->setCursorPos(5, 0);
    b.key(Key::F2, true);
    check(b.editor->hasMarker(1, TextEdit::MarkerType::Bookmark) && b.editor->hasMarker(5, TextEdit::MarkerType::Bookmark), "Ctrl+F2 sets bookmarks");
    b.editor->setCursorPos(0, 0);
    b.key(Key::F2);
    check(b.cursorLine() == 1, "F2 goes to the next bookmark");
    b.key(Key::F2);
    check(b.cursorLine() == 5, "and the next");
    b.key(Key::F2);
    check(b.cursorLine() == 1, "wrapping around");
    b.key(Key::F2, false, true);
    check(b.cursorLine() == 5, "Shift+F2 the previous one, wrapping too");
    b.key(Key::F2, true);
    check(!b.editor->hasMarker(5, TextEdit::MarkerType::Bookmark), "Ctrl+F2 again clears it");
}

void testSignatureHelp()
{
    Bench b("def helper(a, b=2):\n    return a\n\nx = \n");
    b.editor->setCursorPos(3, 4);
    b.type("helper(");
    check(b.editor->signatureVisible() && b.editor->signatureText() == "helper(a, b=2)" && b.editor->signatureParameter() == 0,
          "typing '(' after a function of the text shows its parameters, the first active");
    b.type("1, ");
    check(b.editor->signatureParameter() == 1, "a comma moves to the next parameter");
    b.type("[3, 4], ");
    check(b.editor->signatureParameter() == 2, "commas inside brackets do not count");
    b.key(Key::Escape);
    check(!b.editor->signatureVisible(), "Escape dismisses it");
    b.type("5");
    check(!b.editor->signatureVisible(), "and it stays away for this call");
    b.type(")");
    b.type(" + helper(");
    check(b.editor->signatureVisible() && b.editor->signatureParameter() == 0, "another call shows it again");
    b.editor->setCursorPos(3, 4);
    b.frame();
    check(!b.editor->signatureVisible(), "outside the parentheses there is nothing to show");

    b.editor->setSignatureProvider([](const String& callee, String& signature, String& doc) {
        if (callee != "self.message") return false;
        signature = "message(title, text, kind=\"info\")";
        doc = "A message box of the system; kind is info, warning or error.";
        return true;
    });
    b.editor->moveBottom();
    b.type("self.message(\"zenforms\", ");
    check(b.editor->signatureVisible() && b.editor->signatureCallee() == "self.message" && b.editor->signatureParameter() == 1 && !b.editor->signatureDoc().empty(),
          "the host's provider answers for its API, with a line of documentation; commas inside strings do not count");
    b.type("\"a, b\"");
    check(b.editor->signatureParameter() == 1, "a comma inside the string argument does not move on");
    b.type(", ");
    check(b.editor->signatureParameter() == 2, "the next one does");
}

void testFormat()
{
    Bench b("class A:\n  def f(self):\n\treturn 1   \n\n\n\n  def g(self):\n      if x:\n            pass\n      return 2\nprint(1)");
    b.key(Key::F, true, true);
    check(b.line(0) == "class A:" && b.line(1) == "    def f(self):" && b.line(2) == "        return 1", "Ctrl+Shift+F reindents python by levels, four spaces each, and trims the end");
    check(b.line(3) == "" && b.line(4) == "    def g(self):", "runs of blank lines shrink to one");
    check(b.line(5) == "        if x:" && b.line(6) == "            pass" && b.line(7) == "        return 2" && b.line(8) == "print(1)", "deeper and shallower blocks keep their levels");
    check(b.editor->lineCount() == 10 && b.line(9) == "", "the text ends with a newline");
    b.key(Key::Z, true);
    check(b.line(1) == "  def f(self):", "formatting is one undo step");

    Bench c("int main(){\nif (x) {\nreturn 1;\n   }\n  else {\n return 2; }\n}\n", "test.cpp");
    c.key(Key::F, true, true);
    check(c.line(0) == "int main(){" && c.line(1) == "    if (x) {" && c.line(2) == "        return 1;" && c.line(3) == "    }",
          "braces: a line is indented by the depth it starts at, a closing brace one less");
    check(c.line(4) == "    else {" && c.line(5) == "        return 2; }" && c.line(6) == "}", "a brace that closes at the end of the line does not pull the line out");
    c.editor->setIndentUnit(0);
    c.key(Key::F, true, true);
    check(c.line(1) == "\tif (x) {" && c.line(2) == "\t\treturn 1;", "an indent unit of 0 means tabs");
}

} // namespace

int main()
{
    testUndoKeysAndGroups();
    testFindBar();
    testReplaceBar();
    testGotoLineAndSymbol();
    testCommands();
    testCursorsBookmarksDefinition();
    testSignatureHelp();
    testFormat();
    if (gFailures)
        printf("%d failure(s)\n", gFailures);
    else
        printf("code editor keys: all checks passed\n");
    return gFailures == 0 ? 0 : 1;
}
