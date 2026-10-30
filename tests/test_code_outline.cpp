// The retained CodeEditor's symbols, reveal and completion popup, and the
// SymbolOutline beside it, driven through WidgetApp the way a window does.

#include <igui/widgets/CodeEditor.hpp>
#include <igui/widgets/ConsoleWidget.hpp>
#include <igui/widgets/SymbolOutline.hpp>
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

const char* kScript =
    "class MainForm(Form):\n"            // 0
    "    def on_create(self):\n"         // 1
    "        self.acc = 0.0\n"           // 2
    "        self.display.text = \"0\"\n"
    "\n"
    "    def on_digit(self, sender):\n"  // 5
    "        self.display.text = sender.text\n"
    "\n"
    "def helper(value):\n"               // 8
    "    return value\n";

// A stage with an editor and an outline side by side, pumped like a window.
struct Bench
{
    WidgetApp& app = WidgetApp::instance();
    IO io;
    DrawData data;
    Widget* stage = nullptr;
    CodeEditor* editor = nullptr;
    SymbolOutline* outline = nullptr;

    Bench()
    {
        io.displayWidth = 800.0f;
        io.displayHeight = 600.0f;
        io.deltaTime = 1.0f / 60.0f;
        stage = app.addStage("outline");
        app.setStage("outline", TransitionType::None);
        editor = stage->createChild<CodeEditor>();
        editor->setRect({ 0.0f, 0.0f, 500.0f, 300.0f });
        editor->setHighlighter<PythonHighlighter>();
        outline = stage->createChild<SymbolOutline>();
        outline->setRect({ 520.0f, 0.0f, 260.0f, 400.0f });
        outline->setEditor(editor);
    }
    ~Bench()
    {
        app.removeStage("outline");
    }
    void frame()
    {
        app.update(io);
        app.paint(data);
        io.keyEvents.clear();
        io.inputChars.clear();
    }
    void key(int code, bool ctrl = false, bool shift = false)
    {
        IO::KeyEvent e;
        e.key = code;
        e.ctrl = ctrl;
        e.shift = shift;
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
    void click(float x, float y)
    {
        io.mouseX = x;
        io.mouseY = y;
        frame();
        io.mouseDown[0] = true;
        frame();
        io.mouseDown[0] = false;
        frame();
    }
};

int indexOf(const CodeEditor& editor, const char* name)
{
    return ig::syntax::findSymbol(editor.symbols(), name);
}

void testSymbolsFollowEdits()
{
    Bench b;
    b.editor->setText(kScript);
    const ct::Vector<ig::syntax::Symbol>& symbols = b.editor->symbols();
    check(symbols.size() == 5, "the script has a class, two methods, a field and a function");
    check(indexOf(*b.editor, "MainForm") == 0 && symbols[0].kind == ig::syntax::SymbolKind::Class, "the class comes first");
    const int digit = indexOf(*b.editor, "on_digit");
    check(digit >= 0 && symbols[static_cast<size_t>(digit)].parent == 0 && symbols[static_cast<size_t>(digit)].line == 5, "a method under the class, on its line");
    check(&b.editor->symbols() == &symbols && b.editor->symbols().data() == symbols.data(), "asking again without an edit scans nothing");
    b.editor->insertTextAt({ 0, 0 }, "import json\n");
    check(b.editor->symbols().size() == 6 && indexOf(*b.editor, "json") == 0, "an edit is seen on the next ask");
    check(b.editor->symbols()[static_cast<size_t>(indexOf(*b.editor, "on_digit"))].line == 6, "lines moved with the insertion");
    check(b.editor->gotoSymbol("helper") && b.editor->cursorPos().line == 9 && b.editor->cursorPos().col == 0, "gotoSymbol puts the cursor on the declaration");
    check(!b.editor->gotoSymbol("nowhere"), "an unknown name is reported");
}

void testRevealScrollsOncePainted()
{
    Bench b;
    String text;
    for (int i = 0; i < 200; ++i)
    {
        char line[32];
        snprintf(line, sizeof line, "line_%d = %d\n", i, i);
        text += line;
    }
    b.editor->setText(text);
    b.editor->reveal(150, 2);
    check(b.editor->cursorPos().line == 150 && b.editor->cursorPos().col == 2, "reveal moves the cursor at once");
    b.frame();
    b.frame();
    const int first = b.editor->firstVisibleLine();
    check(first > 100 && first <= 150, "once painted the line is in view, in the upper part");
    b.editor->reveal(3);
    b.frame();
    check(b.editor->firstVisibleLine() == 0, "a line near the top scrolls back to the top");
}

void testBuiltinCompletion()
{
    Bench b;
    b.editor->setText("alpha_one = 1\nalpha_two = 2\n");
    b.app.setFocused(b.editor);
    b.editor->moveBottom();
    b.type("al");
    check(b.editor->completionVisible(), "two letters open the popup when the text has matching words");
    const ct::Vector<CodeEditor::CompletionItem>& items = b.editor->completionItems();
    check(items.size() == 2 && items[0].text == "alpha_one" && items[1].text == "alpha_two", "the words of the text, sorted, without the one being typed");
    b.key(Key::Return);
    check(!b.editor->completionVisible() && b.editor->lineCount() == 4, "Return on a popup nobody asked for is still a newline");
    b.key(Key::Backspace);
    check(b.editor->lineCount() == 3 && b.editor->lineAt(2) == "al", "and the popup closed");
    b.type("p");
    check(b.editor->completionVisible() && b.editor->completionSelected() == 0, "typing on reopens it");
    b.key(Key::Down);
    check(b.editor->completionSelected() == 1, "Down browses");
    b.key(Key::Return);
    check(!b.editor->completionVisible() && b.editor->lineAt(2) == "alpha_two", "Return after browsing accepts the highlighted word");
    check(b.editor->canUndo(), "an accepted completion is undoable");
    b.editor->undo();
    check(b.editor->lineAt(2) == "alp", "as one step, back to what was typed");
    b.type("h");
    b.key(Key::Escape);
    check(!b.editor->completionVisible(), "Escape closes it");
    b.key(Key::Space, true);
    check(b.editor->completionVisible(), "Ctrl+Space asks for it");
    b.key(Key::Tab);
    check(b.editor->lineAt(2) == "alpha_one", "Tab accepts the first item");
    b.editor->setText("x = 1\n");
    b.editor->moveBottom();
    b.type("de");
    bool keyword = false;
    for (const CodeEditor::CompletionItem& item: b.editor->completionItems())
        keyword = keyword || (item.text == "def" && item.detail == "keyword");
    check(keyword, "the language's keywords are offered, marked as such");
}

void testProviderCompletion()
{
    Bench b;
    b.editor->setText(kScript);
    b.app.setFocused(b.editor);
    ct::Vector<String> receivers;
    b.editor->setCompletionProvider([&](const CodeEditor::CompletionContext& ctx, ct::Vector<CodeEditor::CompletionItem>& items) {
        receivers.push_back(ctx.receiver);
        if (ctx.receiver == "self")
        {
            items.push_back({ "display", "Edit", "" });
            items.push_back({ "message", "method", "A message box of the system." });
        }
    });
    b.editor->moveBottom();
    b.type("    self.");
    check(b.editor->completionVisible() && !receivers.empty() && receivers.back() == "self", "a dot asks the provider with the name before it");
    const ct::Vector<CodeEditor::CompletionItem>& items = b.editor->completionItems();
    check(items.size() >= 2 && items[0].text == "display" && items[0].detail == "Edit" && items[1].text == "message", "the host's items come first, in its order");
    check(items[1].doc == "A message box of the system.", "with their line of documentation");
    bool textWord = false;
    for (const CodeEditor::CompletionItem& item: items)
        textWord = textWord || item.text == "sender";
    check(textWord, "the words of the text follow after a dot, the keywords do not");
    b.type("m");
    check(b.editor->completionVisible() && b.editor->completionItems()[0].text == "message", "typing narrows to the matching items");
    b.key(Key::Return);
    check(b.editor->lineAt(b.editor->lineCount() - 1) == "    self.message", "Return accepts a popup that a dot asked for");
    b.editor->setBuiltinCompletions(false);
    b.type(".");
    check(!b.editor->completionVisible(), "nothing from the host and no built-ins: no popup");
}

void testOutline()
{
    Bench b;
    b.editor->setText(kScript);
    b.frame();
    b.frame();
    const ct::Vector<TreeNode*>& roots = b.outline->roots();
    check(roots.size() == 2 && roots[0]->text() == "MainForm" && roots[1]->text() == "helper", "a root per top-level declaration");
    check(roots[0]->children().size() == 3 && roots[0]->children()[0]->text() == "on_create", "the class holds its methods and field");
    const int digit = indexOf(*b.editor, "on_digit");
    TreeNode* digitNode = b.outline->nodeOf(digit);
    check(digitNode && b.outline->symbolOf(digitNode) == digit, "rows map to symbols both ways");

    // Rows are 24 px: MainForm, on_create, acc, on_digit, helper.
    int activated = -1;
    b.outline->activated.connect([&](int index) { activated = index; });
    const Rect o = b.outline->absoluteRect();
    b.click(o.x + 100.0f, o.y + 3 * 24.0f + 12.0f);
    check(activated == digit && b.editor->cursorPos().line == 5 && b.editor->cursorPos().col == 4, "a press on a row puts the cursor on its declaration");
    check(b.app.focused() == b.editor, "and the editor takes the focus to type there");
    activated = -1;
    b.click(o.x + 100.0f, o.y + 3 * 24.0f + 12.0f);
    check(activated == digit, "a press on the selected row navigates again");

    b.editor->setCursorPos(2, 0);
    b.frame();
    check(b.outline->selectedNode() == b.outline->nodeOf(indexOf(*b.editor, "on_create")), "the selection follows the cursor into a method");
    b.editor->setCursorPos(9, 0);
    b.frame();
    check(b.outline->selectedNode() == b.outline->nodeOf(indexOf(*b.editor, "helper")), "and into a function");

    // Typing inside a body changes lines, not the outline: the rows stay.
    TreeNode* before = b.outline->nodeOf(digit);
    b.editor->insertTextAt({ 2, 0 }, "        pass\n");
    b.frame();
    check(b.outline->nodeOf(digit + 0) == before && b.editor->symbols()[static_cast<size_t>(digit)].line == 6, "an edit inside a method keeps the rows and moves the line");
    b.editor->insertTextAt({ 0, 0 }, "def first():\n    pass\n\n");
    b.frame();
    check(b.outline->roots().size() == 3 && b.outline->roots()[0]->text() == "first", "a new declaration rebuilds the rows");

    b.outline->setEditor(nullptr);
    check(b.outline->roots().empty(), "without an editor the outline is empty");
}

void testConsoleClick()
{
    WidgetApp& app = WidgetApp::instance();
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    io.deltaTime = 1.0f / 60.0f;
    DrawData data;
    Widget* stage = app.addStage("console");
    app.setStage("console", TransitionType::None);
    auto* console = stage->createChild<ConsoleWidget>();
    console->setRect({ 0.0f, 0.0f, 600.0f, 300.0f });
    console->log(LogLevel::Info, "run: starting");
    console->log(LogLevel::Error, "run: main.py:7: name 'x' is not defined");
    String activated;
    console->entryActivated.connect([&](const LogEntry& entry) { activated = entry.message; });
    app.update(io);
    app.paint(data);
    // Rows of 18 px under a 24 px tool bar: the second line.
    io.mouseX = 100.0f;
    io.mouseY = 24.0f + 18.0f + 9.0f;
    app.update(io);
    app.paint(data);
    io.mouseDown[0] = true;
    app.update(io);
    app.paint(data);
    io.mouseDown[0] = false;
    app.update(io);
    check(activated == "run: main.py:7: name 'x' is not defined", "a press on a console line reports that entry");
    check(console->selectedEntry() && console->selectedEntry()->level == LogLevel::Error, "and it is the selected one");
    app.removeStage("console");
}

void testEditorDeletedFirst()
{
    // The stage deletes its children in order: the editor goes before the
    // outline, which must not touch it afterwards.
    Bench b;
    b.editor->setText(kScript);
    b.frame();
    check(!b.outline->roots().empty(), "rows before the editor goes");
    b.stage->removeChild(b.editor);
    b.editor = nullptr;
    check(b.outline->editor() == nullptr && b.outline->roots().empty(), "the outline drops a deleted editor and its rows");
    b.frame();
}

} // namespace

int main()
{
    testSymbolsFollowEdits();
    testRevealScrollsOncePainted();
    testBuiltinCompletion();
    testProviderCompletion();
    testOutline();
    testConsoleClick();
    testEditorDeletedFirst();
    if (gFailures)
        printf("%d failure(s)\n", gFailures);
    else
        printf("code outline: all checks passed\n");
    return gFailures == 0 ? 0 : 1;
}
