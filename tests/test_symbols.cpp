// The symbol scan behind the outline and go-to-symbol: one sample per
// language family, checked for what it must find, where, and under what.

#include <igui/Symbols.hpp>
#include <igui/Syntax.hpp>

#include <stdio.h>
#include <string.h>

namespace
{

int gFailures = 0;

void check(bool condition, const char* what)
{
    if (!condition)
    {
        ++gFailures;
        printf("FAIL %s\n", what);
    }
}

using ig::syntax::Symbol;
using ig::syntax::SymbolKind;
using ig::syntax::SymbolSyntax;

// The symbol named `name` with that kind, or nullptr; prints the outline
// when it is missing so a failure says what was found instead.
const Symbol* find(const ct::Vector<Symbol>& symbols, const char* name, SymbolKind kind)
{
    for (size_t i = 0; i < symbols.size(); ++i)
        if (symbols[i].name == name && symbols[i].kind == kind)
            return &symbols[i];
    printf("  missing %s %s; found:\n", ig::syntax::symbolKindName(kind), name);
    for (size_t i = 0; i < symbols.size(); ++i)
        printf("    %-9s %-28s line %d-%d parent %d\n", ig::syntax::symbolKindName(symbols[i].kind),
               symbols[i].name.c_str(), symbols[i].line, symbols[i].endLine, symbols[i].parent);
    return nullptr;
}

bool has(const ct::Vector<Symbol>& symbols, const char* name)
{
    return ig::syntax::findSymbol(symbols, name) >= 0;
}

int parentOf(const ct::Vector<Symbol>& symbols, const Symbol* s)
{
    return s ? s->parent : -2;
}

int indexOf(const ct::Vector<Symbol>& symbols, const Symbol* s)
{
    return s ? static_cast<int>(s - symbols.data()) : -2;
}

void testPython()
{
    const char* source =
        "import json, os.path as p\n"               // 0
        "from math import sin\n"                    // 1
        "\n"
        "class MainForm(Form):\n"                   // 3
        "    def on_create(self):\n"                // 4
        "        self.acc = 0.0\n"                  // 5
        "        self.acc += 1\n"
        "        if self.acc == 2:\n"
        "            pass\n"
        "\n"
        "    # a comment at column 0 does not close the class\n"
        "    def on_digit(self, sender):\n"         // 11
        "        def helper(x):\n"                  // 12
        "            return x\n"
        "        return helper(1)\n"                // 14
        "\n"
        "    def doc(self):\n"                      // 16
        "        \"\"\"def fake(self):\n"
        "        class Fake: pass\"\"\"\n"
        "        pass\n"                            // 19
        "\n"
        "def top_level(a, b):\n"                    // 21
        "    return a + b\n"                        // 22
        "\n"
        "async def later():\n"                      // 24
        "    pass\n";
    const ct::Vector<Symbol> symbols = ig::syntax::scanSymbols(ig::String(source), SymbolSyntax::Indent);
    check(has(symbols, "json") && has(symbols, "os.path") && has(symbols, "math"), "python: imports, dotted and from");
    const Symbol* form = find(symbols, "MainForm", SymbolKind::Class);
    check(form && form->line == 3 && form->endLine == 19, "python: the class spans to its last method");
    const Symbol* create = find(symbols, "on_create", SymbolKind::Method);
    check(create && create->parent == indexOf(symbols, form) && create->endLine == 8, "python: a method under its class, ending before the next def");
    const Symbol* acc = find(symbols, "acc", SymbolKind::Variable);
    check(acc && acc->parent == indexOf(symbols, form), "python: self.acc is a field of the class, listed once");
    int accCount = 0;
    for (size_t i = 0; i < symbols.size(); ++i)
        accCount += symbols[i].name == "acc";
    check(accCount == 1, "python: '+=' and '==' add no second field");
    const Symbol* digit = find(symbols, "on_digit", SymbolKind::Method);
    const Symbol* helper = find(symbols, "helper", SymbolKind::Function);
    check(helper && helper->parent == indexOf(symbols, digit), "python: a nested def is a function under the method");
    check(!has(symbols, "fake") && !has(symbols, "Fake"), "python: a docstring declares nothing");
    const Symbol* top = find(symbols, "top_level", SymbolKind::Function);
    check(top && top->parent == -1 && top->line == 21 && top->endLine == 22, "python: a top-level function");
    check(find(symbols, "later", SymbolKind::Function) != nullptr, "python: async def");
    check(digit && digit->signature == "(self, sender)" && top && top->signature == "(a, b)", "python: a def keeps its parameter list");
    check(form && form->signature.empty(), "python: a class has no signature");
    check(ig::syntax::enclosingSymbol(symbols, 13) == indexOf(symbols, helper), "python: the innermost block holds the line");
    check(ig::syntax::enclosingSymbol(symbols, 14) == indexOf(symbols, digit), "python: after the nested def, back in the method");
    check(ig::syntax::enclosingSymbol(symbols, 23) == -1, "python: a blank line between functions is top level");
    (void)parentOf;
}

void testCpp()
{
    const char* source =
        "#include <x.h>\n"                                          // 0
        "#define MACRO(a) \\\n"
        "    do { a; } while (0)\n"
        "namespace ig {\n"                                          // 3
        "namespace retained {\n"                                    // 4
        "\n"
        "class CodeEditor : public TextEdit\n"                      // 6
        "{\n"
        "public:\n"
        "    CodeEditor();\n"                                       // 9
        "    ~CodeEditor() override;\n"                             // 10
        "    template <typename T, typename... Args>\n"
        "    T* setHighlighter(Args&&... args)\n"                   // 12
        "    {\n"
        "        auto* hl = new T(args...);\n"
        "        if (hl) call(hl);\n"
        "        return hl;\n"
        "    }\n"                                                   // 17
        "    virtual void paint(PaintContext& ctx) = 0;\n"          // 18
        "    void undo() = delete;\n"                               // 19
        "    bool operator==(const CodeEditor& o) const { return false; }\n"
        "    Signal<bool, bool> undoRedoChanged;\n"
        "    ct::Function<bool(int)> lineFilterCb_;\n"
        "    int values_[3] = { f(1), 2, 3 };\n"
        "    struct FoldRange { int startLine, endLine; };\n"       // 24
        "    enum class Mode { A = 0, B };\n"                       // 25
        "};\n"                                                      // 26
        "\n"
        "Widget::Vec2f CodeEditor::sizeHint() const noexcept\n"     // 28
        "{\n"
        "    if (x) { return {1, 2}; }\n"
        "    return {200.f, 200.f};\n"
        "}\n"                                                       // 32
        "\n"
        "CodeEditor::CodeEditor() : TextEdit(\"\"), count_(f(2))\n" // 34
        "{\n"
        "}\n"                                                       // 36
        "\n"
        "static int helper(int a) { return a; }\n"                  // 38
        "int result = helper(3);\n"                                 // 39
        "auto lambda = [](int a) { return a; };\n"
        "extern \"C\" {\n"                                          // 41
        "void exported(void);\n"                                    // 42
        "}\n"
        "} // namespace retained\n"                                 // 44
        "}\n";                                                      // 45
    const ct::Vector<Symbol> symbols = ig::syntax::scanSymbols(ig::String(source), SymbolSyntax::Braces);
    const Symbol* ns = find(symbols, "ig", SymbolKind::Namespace);
    const Symbol* inner = find(symbols, "retained", SymbolKind::Namespace);
    check(ns && inner && inner->parent == indexOf(symbols, ns) && ns->endLine == 45, "c++: nested namespaces");
    const Symbol* cls = find(symbols, "CodeEditor", SymbolKind::Class);
    check(cls && cls->parent == indexOf(symbols, inner) && cls->line == 6 && cls->endLine == 26, "c++: the class under its namespace, spanning to its closing brace");
    const Symbol* ctor = find(symbols, "CodeEditor", SymbolKind::Method);
    check(ctor && ctor->line == 9 && ctor->parent == indexOf(symbols, cls), "c++: a constructor declaration");
    check(find(symbols, "~CodeEditor", SymbolKind::Method) != nullptr, "c++: a destructor declaration");
    const Symbol* tmpl = find(symbols, "setHighlighter", SymbolKind::Method);
    check(tmpl && tmpl->line == 12 && tmpl->endLine == 17, "c++: a template method defined in the class");
    check(!has(symbols, "call") && !has(symbols, "T"), "c++: calls inside a body are not declarations");
    check(find(symbols, "paint", SymbolKind::Method) != nullptr, "c++: a pure virtual");
    check(find(symbols, "undo", SymbolKind::Method) != nullptr, "c++: a deleted method");
    check(!has(symbols, "f"), "c++: an initializer's call is not a declaration");
    const Symbol* range = find(symbols, "FoldRange", SymbolKind::Struct);
    check(range && range->parent == indexOf(symbols, cls), "c++: a nested struct");
    check(find(symbols, "Mode", SymbolKind::Enum) != nullptr, "c++: enum class");
    const Symbol* size = find(symbols, "CodeEditor::sizeHint", SymbolKind::Function);
    check(size && size->line == 28 && size->endLine == 32 && size->parent == indexOf(symbols, inner), "c++: an out-of-line definition with a qualified name");
    const Symbol* outCtor = find(symbols, "CodeEditor::CodeEditor", SymbolKind::Function);
    check(outCtor && outCtor->endLine == 36, "c++: a constructor with an initializer list");
    const Symbol* helperFn = find(symbols, "helper", SymbolKind::Function);
    check(helperFn && helperFn->signature == "(int a)", "c++: a function keeps its parameter list");
    check(ctor && ctor->signature == "()" && tmpl && tmpl->signature == "(Args&&... args)", "c++: constructors and templates too");
    check(!has(symbols, "lambda"), "c++: a lambda initializer is not a function");
    const Symbol* exported = find(symbols, "exported", SymbolKind::Function);
    check(exported && exported->parent == indexOf(symbols, inner), "c++: a declaration inside extern \"C\"");
    check(ig::syntax::enclosingSymbol(symbols, 30) == indexOf(symbols, size), "c++: a line inside a body");
    check(ig::syntax::enclosingSymbol(symbols, 15) == indexOf(symbols, tmpl), "c++: a line inside a method in the class");
}

void testJava()
{
    const char* source =
        "package demo;\n"
        "import java.util.List;\n"
        "\n"
        "public class Account extends Base implements Runnable {\n"   // 3
        "    private final List<String> items = new ArrayList<>();\n"
        "    @Override\n"
        "    public void run() throws IOException {\n"                 // 6
        "        items.add(\"x\");\n"
        "    }\n"                                                      // 8
        "    public <T> List<T> map(Function<T> f) {\n"                // 9
        "        return null;\n"
        "    }\n"
        "    static { init(); }\n"
        "    interface Listener { void changed(int v); }\n"            // 13
        "    enum Kind { A, B }\n"                                     // 14
        "}\n";                                                         // 15
    const ct::Vector<Symbol> symbols = ig::syntax::scanSymbols(ig::String(source), SymbolSyntax::Braces);
    const Symbol* cls = find(symbols, "Account", SymbolKind::Class);
    check(cls && cls->line == 3 && cls->endLine == 15, "java: the class with extends and implements");
    const Symbol* run = find(symbols, "run", SymbolKind::Method);
    check(run && run->line == 6 && run->endLine == 8 && run->parent == indexOf(symbols, cls), "java: a method with an annotation and throws");
    check(!has(symbols, "Override") && !has(symbols, "ArrayList") && !has(symbols, "add") && !has(symbols, "init"), "java: annotations, new and calls are not declarations");
    check(find(symbols, "map", SymbolKind::Method) != nullptr, "java: a generic method");
    const Symbol* listener = find(symbols, "Listener", SymbolKind::Interface);
    const Symbol* changed = find(symbols, "changed", SymbolKind::Method);
    check(listener && changed && changed->parent == indexOf(symbols, listener), "java: an interface and its method");
    check(find(symbols, "Kind", SymbolKind::Enum) != nullptr, "java: an enum");
}

void testJavaScript()
{
    const char* source =
        "import { x } from './x.js';\n"
        "const PI = 3.14;\n"
        "function top(a, b) {\n"                      // 2
        "    return a + b;\n"
        "}\n"                                         // 4
        "const arrow = (a) => {\n"                    // 5
        "    return a * 2;\n"
        "};\n"                                        // 7
        "const short = a => a * 2;\n"
        "obj.handler = function(e) {\n"               // 9
        "    log(e);\n"
        "};\n"
        "class Shape {\n"                             // 12
        "    constructor(w) { this.w = w; }\n"        // 13
        "    get area() { return this.w; }\n"         // 14
        "    static make() { return new Shape(1); }\n"
        "    async load() { await fetch(`${this.w}`); }\n"
        "}\n"                                         // 17
        "if (PI > 3) {\n"
        "    function nested() { }\n"                 // 19
        "}\n"
        "top(1, 2);\n";
    const ct::Vector<Symbol> symbols = ig::syntax::scanSymbols(ig::String(source), SymbolSyntax::Braces);
    const Symbol* top = find(symbols, "top", SymbolKind::Function);
    check(top && top->line == 2 && top->endLine == 4, "js: function declaration");
    int topCount = 0;
    for (size_t i = 0; i < symbols.size(); ++i)
        topCount += symbols[i].name == "top";
    check(topCount == 1, "js: a call at the top level is not a declaration");
    const Symbol* arrow = find(symbols, "arrow", SymbolKind::Function);
    check(arrow && arrow->line == 5 && arrow->endLine == 7, "js: an arrow function assigned to a const");
    check(!has(symbols, "short"), "js: an arrow without a block has no range to show");
    check(find(symbols, "handler", SymbolKind::Function) != nullptr, "js: obj.handler = function");
    const Symbol* shape = find(symbols, "Shape", SymbolKind::Class);
    const Symbol* ctor = find(symbols, "constructor", SymbolKind::Method);
    check(shape && ctor && ctor->parent == indexOf(symbols, shape) && ctor->line == 13, "js: a class and its constructor");
    check(find(symbols, "area", SymbolKind::Method) && find(symbols, "make", SymbolKind::Method) && find(symbols, "load", SymbolKind::Method), "js: getter, static and async methods");
    check(!has(symbols, "fetch") && !has(symbols, "log"), "js: calls in bodies are not declarations");
    check(find(symbols, "nested", SymbolKind::Function) != nullptr, "js: a function inside a top-level if");
}

void testGlsl()
{
    const char* source =
        "#version 450 core\n"
        "layout(location = 0) in vec3 position;\n"
        "uniform mat4 model;\n"
        "struct Light { vec3 color; float power; };\n"   // 3
        "float attenuate(float d) { return 1.0 / d; }\n" // 4
        "void main()\n"                                   // 5
        "{\n"
        "    gl_Position = model * vec4(position, 1.0);\n"
        "}\n";                                            // 8
    const ct::Vector<Symbol> symbols = ig::syntax::scanSymbols(ig::String(source), SymbolSyntax::Braces);
    check(find(symbols, "Light", SymbolKind::Struct) != nullptr, "glsl: a struct");
    check(find(symbols, "attenuate", SymbolKind::Function) != nullptr, "glsl: a one-line function");
    const Symbol* mainFn = find(symbols, "main", SymbolKind::Function);
    check(mainFn && mainFn->line == 5 && mainFn->endLine == 8, "glsl: main with its brace on the next line");
    check(!has(symbols, "layout") && !has(symbols, "vec4"), "glsl: layout(...) and constructors are not functions");
}

void testLua()
{
    const char* source =
        "local M = {}\n"
        "-- function commented()\n"
        "--[[ function inBlock()\n"
        "end ]]\n"
        "function M.new(x)\n"                 // 4
        "    local self = { x = x }\n"
        "    if x then print(x) end\n"
        "    return self\n"
        "end\n"                               // 8
        "function M:area()\n"                 // 9
        "    local function square(v) return v * v end\n"  // 10
        "    for i = 1, 3 do square(i) end\n"
        "    return square(self.x)\n"
        "end\n"                               // 13
        "local helper = function(a)\n"        // 14
        "    return a\n"
        "end\n"                               // 16
        "M.cb = function() end\n"             // 17
        "do\n"
        "    local z = 1\n"
        "end\n"
        "return M\n";
    const ct::Vector<Symbol> symbols = ig::syntax::scanSymbols(ig::String(source), SymbolSyntax::Lua);
    check(!has(symbols, "commented") && !has(symbols, "inBlock"), "lua: comments declare nothing");
    const Symbol* mnew = find(symbols, "M.new", SymbolKind::Method);
    check(mnew && mnew->line == 4 && mnew->endLine == 8, "lua: function M.new ... end, through an if ... end inside");
    const Symbol* area = find(symbols, "M:area", SymbolKind::Method);
    const Symbol* square = find(symbols, "square", SymbolKind::Function);
    check(area && square && square->parent == indexOf(symbols, area) && square->line == 10 && square->endLine == 10, "lua: a local function nested in a method, closed on its own line");
    check(area && area->endLine == 13, "lua: the method ends after the for ... end inside it");
    const Symbol* helper = find(symbols, "helper", SymbolKind::Function);
    check(helper && helper->line == 14 && helper->endLine == 16, "lua: local name = function");
    check(helper && helper->signature == "(a)" && mnew && mnew->signature == "(x)", "lua: the parameter lists");
    check(find(symbols, "M.cb", SymbolKind::Method) != nullptr, "lua: a.b = function");
    check(ig::syntax::enclosingSymbol(symbols, 19) == -1, "lua: a do block is not a symbol");
}

void testBlitz()
{
    const char* source =
        "; .fake\n"
        ".start\n"                               // 1
        "Global score, name$ = \"x\", pos#\n"    // 2
        "Const MAX = 10\n"                       // 3
        "Type Player\n"                          // 4
        "    Field x#, y#\n"                     // 5
        "    Field hp\n"
        "End Type\n"                             // 7
        "Function Update(dt#)\n"                 // 8
        "    If dt# > 1 Then\n"
        "        Return\n"
        "    EndIf\n"
        "End Function\n"                         // 12
        "Function Greet$(who$)\n"                // 13
        "    Return \"hi \" + who$\n"
        "End Function\n";                        // 15
    const ct::Vector<Symbol> symbols = ig::syntax::scanSymbols(ig::String(source), SymbolSyntax::Blitz);
    check(!has(symbols, "fake"), "blitz: a label in a comment does not count");
    const Symbol* label = find(symbols, "start", SymbolKind::Label);
    check(label && label->line == 1, "blitz: a .label");
    check(has(symbols, "score") && has(symbols, "name$") && has(symbols, "pos#"), "blitz: every Global name, with its sigil");
    check(find(symbols, "MAX", SymbolKind::Constant) != nullptr, "blitz: a Const");
    const Symbol* player = find(symbols, "Player", SymbolKind::Struct);
    const Symbol* hp = find(symbols, "hp", SymbolKind::Variable);
    check(player && hp && hp->parent == indexOf(symbols, player) && player->endLine == 7, "blitz: a Type with its Fields, to End Type");
    const Symbol* update = find(symbols, "Update", SymbolKind::Function);
    check(update && update->line == 8 && update->endLine == 12, "blitz: Function ... End Function, an EndIf inside does not close it");
    check(find(symbols, "Greet$", SymbolKind::Function) != nullptr, "blitz: a function with a return sigil");
    check(update && update->signature == "(dt#)", "blitz: the parameter list");
    check(ig::syntax::enclosingSymbol(symbols, 14) == indexOf(symbols, find(symbols, "Greet$", SymbolKind::Function)), "blitz: a line inside a function");
}

void testHighlightersPickTheScan()
{
    check(ig::syntax::PythonHighlighter().symbolSyntax() == SymbolSyntax::Indent, "python highlighter: indent scan");
    check(ig::syntax::CppHighlighter().symbolSyntax() == SymbolSyntax::Braces, "c++ highlighter: braces scan");
    check(ig::syntax::JavaHighlighter().symbolSyntax() == SymbolSyntax::Braces, "java highlighter: braces scan");
    check(ig::syntax::JsHighlighter().symbolSyntax() == SymbolSyntax::Braces, "js highlighter: braces scan");
    check(ig::syntax::GlslHighlighter().symbolSyntax() == SymbolSyntax::Braces, "glsl highlighter: braces scan");
    check(ig::syntax::LuaHighlighter().symbolSyntax() == SymbolSyntax::Lua, "lua highlighter: lua scan");
    check(ig::syntax::BlitzHighlighter().symbolSyntax() == SymbolSyntax::Blitz, "blitz highlighter: blitz scan");
    ig::syntax::SyntaxLanguage lang;
    lang.name = "zenx";
    lang.symbols = SymbolSyntax::Indent;
    check(ig::syntax::DefinedHighlighter(lang).symbolSyntax() == SymbolSyntax::Indent, "a declared language names its scan");
    check(ig::syntax::scanSymbols(ig::String("def x():\n    pass\n"), SymbolSyntax::None).empty(), "no scan: no symbols");
}

} // namespace

int main()
{
    testPython();
    testCpp();
    testJava();
    testJavaScript();
    testGlsl();
    testLua();
    testBlitz();
    testHighlightersPickTheScan();
    if (gFailures)
        printf("%d failure(s)\n", gFailures);
    else
        printf("symbols: all checks passed\n");
    return gFailures == 0 ? 0 : 1;
}
