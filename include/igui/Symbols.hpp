#pragma once

// ═════════════════════════════════════════════════════════════════════════════
//  ig::syntax symbols — the declarations of a source text, for an outline
//
//  What an editor's "functions / types" pane and its go-to-symbol need:
//  classes, their methods, functions, imports, found by a lexical scan that
//  knows how each family of languages nests (braces, indentation, Lua's
//  function...end, Blitz's Function...End Function). Deliberately not a
//  parser: it looks for declaration keywords and the identifier that follows,
//  and tracks nesting, which is enough to navigate a file and is cheap enough
//  to run after every keystroke.
//
//  Which scan applies comes from the SyntaxHighlighter of the language
//  (SyntaxHighlighter::symbolSyntax), so a host asks the editor for its
//  symbols without knowing the language; the free functions below are for
//  hosts without an editor widget.
// ═════════════════════════════════════════════════════════════════════════════

#include <igui/Types.hpp>
#include <ct/vector.hpp>

namespace ig
{
namespace syntax
{

enum class SymbolKind
{
    Namespace,
    Class,
    Struct,
    Enum,
    Interface,
    Function,   // free, or nested in another function
    Method,     // a function inside a class, struct or interface
    Variable,   // a field (Blitz Field, Python self.x), a Global
    Constant,
    Label,      // Blitz .label
    Import,
};

struct Symbol
{
    SymbolKind kind = SymbolKind::Function;
    String name;
    int line = 0;      // zero-based line of the declaration
    int endLine = 0;   // last line of its block; the declaration's own for a leaf
    int indent = 0;    // columns before the declaration (a tab counts 4)
    int parent = -1;   // index of the enclosing symbol, -1 at the top level
    String signature;  // a function's or method's "(a, b=2)" as written; empty otherwise
};

// How a language nests its declarations.
enum class SymbolSyntax
{
    None,     // no outline for this language
    Braces,   // C, C++, Java, JavaScript, GLSL, C#, ...
    Indent,   // Python and its relatives
    Lua,      // function ... end
    Blitz,    // Function ... End Function, Type ... End Type
};

// One pass over the lines; symbols come in source order, a parent before
// its children.
ct::Vector<Symbol> scanSymbols(const ct::Vector<String>& lines, SymbolSyntax syntax);
// The same over a whole text, split at '\n' ('\r' is dropped).
ct::Vector<Symbol> scanSymbols(const String& text, SymbolSyntax syntax);

// The innermost class, function or method whose block holds the line, or
// -1 when the line is at the top level.
int enclosingSymbol(const ct::Vector<Symbol>& symbols, int line);

// The first symbol with that name, or -1.
int findSymbol(const ct::Vector<Symbol>& symbols, StringView name);

// Namespaces, classes, structs, enums and interfaces: what holds members.
bool isContainerKind(SymbolKind kind);

// "class", "method", ... for tools and tests.
const char* symbolKindName(SymbolKind kind);

} // namespace syntax
} // namespace ig
