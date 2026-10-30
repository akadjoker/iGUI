#include "igui/Symbols.hpp"

#include <string.h>

namespace ig
{
namespace syntax
{

namespace
{

bool isIdentStart(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool isIdentChar(char c)
{
    return isIdentStart(c) || (c >= '0' && c <= '9');
}

bool isBlank(char c)
{
    return c == ' ' || c == '\t';
}

char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

size_t skipBlanks(const String& line, size_t i)
{
    while (i < line.size() && isBlank(line[i]))
        ++i;
    return i;
}

int indentOf(const String& line)
{
    int indent = 0;
    for (size_t i = 0; i < line.size(); ++i)
    {
        if (line[i] == ' ')
            ++indent;
        else if (line[i] == '\t')
            indent += 4;
        else
            break;
    }
    return indent;
}

// The word at i equals `word` and ends there (so "define" is not "def").
bool wordAt(const String& line, size_t i, const char* word, bool caseless = false)
{
    const size_t n = strlen(word);
    if (i + n > line.size())
        return false;
    for (size_t k = 0; k < n; ++k)
    {
        const char c = caseless ? lower(line[i + k]) : line[i + k];
        if (c != word[k])
            return false;
    }
    return i + n == line.size() || !isIdentChar(line[i + n]);
}

String identifierAt(const String& line, size_t& i)
{
    if (i >= line.size() || !isIdentStart(line[i]))
        return String();
    const size_t start = i;
    while (i < line.size() && isIdentChar(line[i]))
        ++i;
    return line.substr(start, i - start);
}

// "(a, b)" from the "(" at i to its ")" on the same line; cut with "...)"
// when the list goes on to the next line.
String parenText(const String& line, size_t i)
{
    if (i >= line.size() || line[i] != '(')
        return String();
    int depth = 0;
    for (size_t k = i; k < line.size(); ++k)
    {
        if (line[k] == '(')
            ++depth;
        else if (line[k] == ')' && --depth == 0)
            return line.substr(i, k - i + 1);
    }
    String open = line.substr(i);
    while (!open.empty() && (open[open.size() - 1] == ' ' || open[open.size() - 1] == ','))
        open.resize(open.size() - 1);
    return open + " ...)";
}

Symbol make(SymbolKind kind, const String& name, int line, int indent, int parent)
{
    Symbol s;
    s.kind = kind;
    s.name = name;
    s.line = line;
    s.endLine = line;
    s.indent = indent;
    s.parent = parent;
    return s;
}

bool sameName(const String& a, StringView b)
{
    return a.size() == b.size() && (b.size() == 0 || memcmp(a.c_str(), b.data(), b.size()) == 0);
}

// ── Indent (Python) ─────────────────────────────────────────────────────

// Walks the quotes of one line and leaves in `open` the triple-quote
// delimiter still open at its end (0, '"' or '\''), so a "def" inside a
// docstring is not a declaration.
void trackTripleQuotes(const String& line, char& open)
{
    const size_t n = line.size();
    size_t i = 0;
    while (i < n)
    {
        const char c = line[i];
        if (open)
        {
            if (c == '\\')
            {
                i += 2;
                continue;
            }
            if (c == open && i + 2 < n && line[i + 1] == open && line[i + 2] == open)
            {
                open = 0;
                i += 3;
                continue;
            }
            ++i;
            continue;
        }
        if (c == '#')
            return;
        if (c == '"' || c == '\'')
        {
            if (i + 2 < n && line[i + 1] == c && line[i + 2] == c)
            {
                open = c;
                i += 3;
                continue;
            }
            ++i;
            while (i < n && line[i] != c)
            {
                if (line[i] == '\\')
                    ++i;
                ++i;
            }
            ++i;
            continue;
        }
        ++i;
    }
}

// "self.name =" (also "+=" and friends, not "=="): a field of the class the
// method belongs to, listed once.
void scanSelfAssignments(const String& line, int lineIndex, int classIndex, ct::Vector<Symbol>& out)
{
    const size_t n = line.size();
    size_t i = 0;
    while (i + 5 < n)
    {
        if (strncmp(line.c_str() + i, "self.", 5) != 0 || (i > 0 && isIdentChar(line[i - 1])))
        {
            ++i;
            continue;
        }
        size_t j = i + 5;
        const String name = identifierAt(line, j);
        i = j;
        if (name.empty())
            continue;
        size_t k = skipBlanks(line, j);
        if (k < n && (line[k] == '+' || line[k] == '-' || line[k] == '*' || line[k] == '/'))
            ++k;
        if (k >= n || line[k] != '=' || (k + 1 < n && line[k + 1] == '='))
            continue;
        bool seen = false;
        for (size_t s = 0; s < out.size() && !seen; ++s)
            seen = out[s].parent == classIndex && out[s].kind == SymbolKind::Variable && out[s].name == name;
        if (!seen)
            out.push_back(make(SymbolKind::Variable, name, lineIndex, out[static_cast<size_t>(classIndex)].indent + 4, classIndex));
    }
}

ct::Vector<Symbol> scanIndent(const ct::Vector<String>& lines)
{
    ct::Vector<Symbol> out;
    ct::Vector<int> open; // the blocks the current line is inside
    int lastCodeLine = -1;
    char quote = 0;
    for (size_t li = 0; li < lines.size(); ++li)
    {
        const String& line = lines[li];
        const int current = static_cast<int>(li);
        const bool insideString = quote != 0;
        trackTripleQuotes(line, quote);
        if (insideString)
            continue;
        const size_t first = skipBlanks(line, 0);
        if (first >= line.size() || line[first] == '#')
            continue; // blank lines and comments neither open nor close a block
        const int indent = indentOf(line);
        while (!open.empty() && out[static_cast<size_t>(open.back())].indent >= indent)
        {
            out[static_cast<size_t>(open.back())].endLine = lastCodeLine;
            open.pop_back();
        }
        lastCodeLine = current;
        const int parent = open.empty() ? -1 : open.back();
        size_t i = first;
        if (wordAt(line, i, "class"))
        {
            i = skipBlanks(line, i + 5);
            const String name = identifierAt(line, i);
            if (name.empty())
                continue;
            out.push_back(make(SymbolKind::Class, name, current, indent, parent));
            open.push_back(static_cast<int>(out.size()) - 1);
        }
        else if (wordAt(line, i, "def") || wordAt(line, i, "async"))
        {
            if (line[i] == 'a')
            {
                i = skipBlanks(line, i + 5);
                if (!wordAt(line, i, "def"))
                    continue;
            }
            i = skipBlanks(line, i + 3);
            const String name = identifierAt(line, i);
            if (name.empty())
                continue;
            const bool inClass = parent >= 0 && out[static_cast<size_t>(parent)].kind == SymbolKind::Class;
            out.push_back(make(inClass ? SymbolKind::Method : SymbolKind::Function, name, current, indent, parent));
            out.back().signature = parenText(line, skipBlanks(line, i));
            open.push_back(static_cast<int>(out.size()) - 1);
        }
        else if (wordAt(line, i, "import") || wordAt(line, i, "from"))
        {
            // "import a, b" names each module; "from m import x" names m.
            const bool from = line[i] == 'f';
            i = skipBlanks(line, i + (from ? 4 : 6));
            while (i < line.size())
            {
                String name = identifierAt(line, i);
                if (name.empty())
                    break;
                while (i < line.size() && line[i] == '.')
                {
                    ++i;
                    const String part = identifierAt(line, i);
                    if (part.empty())
                        break;
                    name += ".";
                    name += part;
                }
                out.push_back(make(SymbolKind::Import, name, current, indent, parent));
                if (from)
                    break;
                i = skipBlanks(line, i);
                if (wordAt(line, i, "as"))
                {
                    i = skipBlanks(line, i + 2);
                    identifierAt(line, i);
                    i = skipBlanks(line, i);
                }
                if (i < line.size() && line[i] == ',')
                    i = skipBlanks(line, i + 1);
                else
                    break;
            }
        }
        else if (parent >= 0 && out[static_cast<size_t>(parent)].kind == SymbolKind::Method)
        {
            const int classIndex = out[static_cast<size_t>(parent)].parent;
            if (classIndex >= 0 && line.find("self.") != String::npos)
                scanSelfAssignments(line, current, classIndex, out);
        }
    }
    while (!open.empty())
    {
        out[static_cast<size_t>(open.back())].endLine = lastCodeLine;
        open.pop_back();
    }
    return out;
}

// ── Blitz ───────────────────────────────────────────────────────────────

// A Blitz identifier may end in a type sigil: a$, b#, c%.
String blitzIdentifierAt(const String& line, size_t& i)
{
    String name = identifierAt(line, i);
    if (!name.empty() && i < line.size() && (line[i] == '$' || line[i] == '#' || line[i] == '%'))
        name += line[i++];
    return name;
}

// "Global a, b$ = 1, c# = f(2)": one symbol per name; an initializer is
// skipped to the next comma outside brackets.
void scanBlitzNames(const String& line, size_t i, int lineIndex, int indent, int parent, SymbolKind kind, ct::Vector<Symbol>& out)
{
    while (i < line.size())
    {
        i = skipBlanks(line, i);
        const String name = blitzIdentifierAt(line, i);
        if (name.empty())
            break;
        out.push_back(make(kind, name, lineIndex, indent, parent));
        int depth = 0;
        bool more = false;
        while (i < line.size())
        {
            const char c = line[i];
            if (c == '(' || c == '[')
                ++depth;
            else if (c == ')' || c == ']')
                --depth;
            else if (c == ',' && depth <= 0)
            {
                ++i;
                more = true;
                break;
            }
            else if (c == ';')
                return; // a comment: nothing more declared here
            else if (c == '"')
            {
                // A string in the initializer: a comma inside it separates nothing.
                ++i;
                while (i < line.size() && line[i] != '"')
                    ++i;
            }
            ++i;
        }
        if (!more)
            break;
    }
}

ct::Vector<Symbol> scanBlitz(const ct::Vector<String>& lines)
{
    ct::Vector<Symbol> out;
    int open = -1; // the Function or Type block the line is inside
    for (size_t li = 0; li < lines.size(); ++li)
    {
        const String& line = lines[li];
        const int current = static_cast<int>(li);
        size_t i = skipBlanks(line, 0);
        if (i >= line.size() || line[i] == ';')
            continue;
        const int indent = indentOf(line);
        if (line[i] == '.')
        {
            ++i;
            const String name = identifierAt(line, i);
            if (!name.empty())
                out.push_back(make(SymbolKind::Label, name, current, indent, open));
        }
        else if (wordAt(line, i, "end", true) || wordAt(line, i, "endfunction", true) || wordAt(line, i, "endtype", true))
        {
            size_t j = skipBlanks(line, i + 3);
            const bool closes = wordAt(line, i, "end", true) ? (wordAt(line, j, "function", true) || wordAt(line, j, "type", true)) : true;
            if (closes && open >= 0)
            {
                out[static_cast<size_t>(open)].endLine = current;
                open = -1;
            }
        }
        else if (wordAt(line, i, "function", true))
        {
            i = skipBlanks(line, i + 8);
            const String name = blitzIdentifierAt(line, i);
            if (name.empty())
                continue;
            out.push_back(make(SymbolKind::Function, name, current, indent, -1));
            out.back().signature = parenText(line, skipBlanks(line, i));
            open = static_cast<int>(out.size()) - 1;
        }
        else if (wordAt(line, i, "type", true))
        {
            i = skipBlanks(line, i + 4);
            const String name = identifierAt(line, i);
            if (name.empty())
                continue;
            out.push_back(make(SymbolKind::Struct, name, current, indent, -1));
            open = static_cast<int>(out.size()) - 1;
        }
        else if (wordAt(line, i, "field", true))
        {
            if (open >= 0 && out[static_cast<size_t>(open)].kind == SymbolKind::Struct)
                scanBlitzNames(line, i + 5, current, indent, open, SymbolKind::Variable, out);
        }
        else if (wordAt(line, i, "global", true))
            scanBlitzNames(line, i + 6, current, indent, -1, SymbolKind::Variable, out);
        else if (wordAt(line, i, "const", true))
            scanBlitzNames(line, i + 5, current, indent, -1, SymbolKind::Constant, out);
    }
    if (open >= 0)
        out[static_cast<size_t>(open)].endLine = static_cast<int>(lines.size()) - 1;
    return out;
}

// ── Lua ─────────────────────────────────────────────────────────────────

ct::Vector<Symbol> scanLua(const ct::Vector<String>& lines)
{
    ct::Vector<Symbol> out;
    struct Block
    {
        int symbol; // -1 for if/for/while/do blocks
    };
    ct::Vector<Block> blocks;
    bool longBracket = false; // inside --[[ ]] or [[ ]]
    for (size_t li = 0; li < lines.size(); ++li)
    {
        const String& line = lines[li];
        const int current = static_cast<int>(li);
        const size_t n = line.size();
        const int indent = indentOf(line);
        size_t i = 0;
        String previous;      // the identifier or keyword before the current one
        String beforePrevious;
        bool lineStart = true;
        while (i < n)
        {
            if (longBracket)
            {
                const size_t close = line.find("]]", i);
                if (close == String::npos)
                    break;
                longBracket = false;
                i = close + 2;
                continue;
            }
            const char c = line[i];
            if (c == '-' && i + 1 < n && line[i + 1] == '-')
            {
                if (i + 3 < n && line[i + 2] == '[' && line[i + 3] == '[')
                {
                    longBracket = true;
                    i += 4;
                    continue;
                }
                break; // a comment to the end of the line
            }
            if (c == '[' && i + 1 < n && line[i + 1] == '[')
            {
                longBracket = true;
                i += 2;
                continue;
            }
            if (c == '"' || c == '\'')
            {
                ++i;
                while (i < n && line[i] != c)
                {
                    if (line[i] == '\\')
                        ++i;
                    ++i;
                }
                ++i;
                previous.clear();
                continue;
            }
            if (!isIdentStart(c))
            {
                if (!isBlank(c))
                {
                    // "a.b", "a:b" keep the chain as one name.
                    if (c == '.' || c == ':')
                    {
                        if (!previous.empty() && i + 1 < n && isIdentStart(line[i + 1]))
                        {
                            ++i;
                            previous += c;
                            previous += identifierAt(line, i);
                            continue;
                        }
                    }
                    else if (c == '=' && !(i + 1 < n && line[i + 1] == '='))
                    {
                        beforePrevious = previous;
                        previous = "=";
                        ++i;
                        continue;
                    }
                    beforePrevious.clear();
                    previous.clear();
                    lineStart = false;
                }
                ++i;
                continue;
            }
            const String word = identifierAt(line, i);
            const bool atStart = lineStart;
            lineStart = false;
            if (word == "function")
            {
                // "function a.b:c(", "local function f(", "x = function(".
                size_t j = skipBlanks(line, i);
                String name;
                if (j < n && isIdentStart(line[j]))
                {
                    name = identifierAt(line, j);
                    while (j < n && (line[j] == '.' || line[j] == ':') && j + 1 < n && isIdentStart(line[j + 1]))
                    {
                        name += line[j++];
                        name += identifierAt(line, j);
                    }
                    i = j;
                }
                else if (previous == "=")
                    name = beforePrevious;
                int symbol = -1;
                if (!name.empty())
                {
                    int parent = -1;
                    for (size_t b = blocks.size(); b > 0 && parent < 0; --b)
                        parent = blocks[b - 1].symbol;
                    const bool method = name.find('.') != String::npos || name.find(':') != String::npos;
                    out.push_back(make(method ? SymbolKind::Method : SymbolKind::Function, name, current, indent, parent));
                    out.back().signature = parenText(line, skipBlanks(line, i));
                    symbol = static_cast<int>(out.size()) - 1;
                }
                blocks.push_back({symbol});
                beforePrevious.clear();
                previous.clear();
                continue;
            }
            if (word == "if" || word == "for" || word == "while" || word == "repeat" || (word == "do" && atStart))
                blocks.push_back({-1});
            else if (word == "end" || word == "until")
            {
                if (!blocks.empty())
                {
                    if (blocks.back().symbol >= 0)
                        out[static_cast<size_t>(blocks.back().symbol)].endLine = current;
                    blocks.pop_back();
                }
            }
            if (word == "local" || word == "then" || word == "do")
                continue; // transparent: "local x = function" keeps x
            beforePrevious = previous;
            previous = word;
        }
    }
    for (size_t b = 0; b < blocks.size(); ++b)
        if (blocks[b].symbol >= 0)
            out[static_cast<size_t>(blocks[b].symbol)].endLine = static_cast<int>(lines.size()) - 1;
    return out;
}

// ── Braces (C, C++, Java, JavaScript, GLSL) ─────────────────────────────

enum class TokenType
{
    None,
    Ident,
    Number,
    String,
    Punct,
};

struct Token
{
    TokenType type = TokenType::None;
    String text;
    int line = 0;
    size_t col = 0; // byte offset in the line
};

// Words that are never a function's name, whatever precedes the "(".
bool isReservedName(const String& w)
{
    static const char* const words[] = {
        "if", "for", "while", "switch", "catch", "return", "sizeof", "alignof", "alignas", "typeid",
        "decltype", "static_assert", "new", "delete", "throw", "case", "else", "do", "using", "typedef",
        "template", "operator", "defined", "co_return", "co_await", "co_yield", "require", "synchronized",
        "assert", "super", "this", "function", "await", "yield", "typeof", "instanceof", "void", "int",
        "float", "double", "char", "bool", "auto", "unsigned", "signed", "long", "short", "const",
        "static", "virtual", "inline", "explicit", "public", "private", "protected", "class", "struct",
        "enum", "union", "namespace", "interface", "extends", "implements", "throws", "import", "export",
        "package", "var", "let", "elif", "in", "of", "goto", "break", "continue", "default", "try",
        "finally", "noexcept", "constexpr", "consteval", "constinit", "override", "final", "volatile",
        "mutable", "register", "extern", "friend", "typename", "requires", "concept", "layout", "uniform",
        "varying", "attribute", "precision", "__attribute__", "__declspec", "Q_OBJECT",
    };
    for (size_t k = 0; k < sizeof(words) / sizeof(words[0]); ++k)
        if (w == words[k])
            return true;
    return false;
}

// What may stand before a declared name: its type, a specifier, or the end
// of the previous declaration.
bool canPrecedeName(const Token& t)
{
    if (t.type == TokenType::None)
        return true; // start of file
    if (t.type == TokenType::Ident)
        return t.text != "return" && t.text != "new" && t.text != "throw" && t.text != "case" && t.text != "delete" && t.text != "goto" && t.text != "typeof" && t.text != "await" && t.text != "in" && t.text != "of";
    if (t.type != TokenType::Punct)
        return false;
    return t.text == ";" || t.text == "{" || t.text == "}" || t.text == ":" || t.text == ">" || t.text == "*" || t.text == "&" || t.text == "&&" || t.text == "]";
}

bool isContainerWord(const String& w, SymbolKind& kind)
{
    if (w == "class") kind = SymbolKind::Class;
    else if (w == "struct" || w == "union") kind = SymbolKind::Struct;
    else if (w == "enum") kind = SymbolKind::Enum;
    else if (w == "interface" || w == "@interface") kind = SymbolKind::Interface;
    else if (w == "namespace") kind = SymbolKind::Namespace;
    else return false;
    return true;
}

bool isTwoCharPunct(char a, char b)
{
    static const char* const pairs[] = {"::", "->", "=>", "&&", "||", "==", "!=", "<=", ">=", "++", "--", "<<", ">>", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^="};
    for (size_t k = 0; k < sizeof(pairs) / sizeof(pairs[0]); ++k)
        if (pairs[k][0] == a && pairs[k][1] == b)
            return true;
    return false;
}

class BraceScanner
{
public:
    ct::Vector<Symbol> run(const ct::Vector<String>& lines)
    {
        for (size_t li = 0; li < lines.size(); ++li)
            scanLine(lines[li], static_cast<int>(li));
        const int last = static_cast<int>(lines.size()) - 1;
        while (!scopes_.empty())
        {
            if (scopes_.back().symbol >= 0)
                out_[static_cast<size_t>(scopes_.back().symbol)].endLine = last;
            scopes_.pop_back();
        }
        return out_;
    }

private:
    enum class ScopeKind
    {
        Namespace, // the top level, a namespace, extern "C", a top-level if: declarations are scanned
        Container, // class, struct, enum, interface: members are scanned
        Opaque,    // a function body, an initializer, an unknown brace: nothing inside is scanned
    };
    struct Scope
    {
        ScopeKind kind;
        int symbol;
    };
    enum class Phase
    {
        None,
        InParens,    // between the "(" after a name and its ")"
        AfterParens, // waiting for "{" (a definition) or ";" (a declaration)
    };

    ct::Vector<Symbol> out_;
    ct::Vector<Scope> scopes_;
    Token prev_;  // the token before the current one
    Token prev2_; // and the one before that
    String lastAssigned_; // the identifier before the last "=": JavaScript's "name = function"
    bool blockComment_ = false;
    bool templateString_ = false;
    bool lineContinues_ = false; // a "#" line ended in "\"
    // A "class X" seen, waiting for its "{" (a ";" makes it a forward declaration).
    bool pendingContainer_ = false;
    SymbolKind pendingKind_ = SymbolKind::Class;
    String pendingName_;
    int pendingLine_ = 0;
    int pendingIndent_ = 0;
    // A "name(" seen.
    Phase phase_ = Phase::None;
    String candidate_;
    int candidateLine_ = 0;
    int candidateIndent_ = 0;
    int parenDepth_ = 0;
    bool typed_ = false;     // a type stood before the name: "void f()", not "f()"
    bool arrow_ = false;     // JavaScript "name = (...) =>"
    bool initList_ = false;  // past the ":" of a constructor's initializer list
    int groupDepth_ = 0;     // nested ( ) [ ] after the parameter list
    bool assigned_ = false;  // "= ..." after the parameter list
    String assignedValue_;   // "0", "default" or "delete" keep the declaration

    ScopeKind scopeKind() const
    {
        return scopes_.empty() ? ScopeKind::Namespace : scopes_.back().kind;
    }

    int scopeSymbol() const
    {
        for (size_t s = scopes_.size(); s > 0; --s)
            if (scopes_[s - 1].symbol >= 0)
                return scopes_[s - 1].symbol;
        return -1;
    }

    bool scanning() const
    {
        return scopeKind() != ScopeKind::Opaque;
    }

    void dropCandidate()
    {
        phase_ = Phase::None;
        candidate_.clear();
    }

    void declare(bool definition, int line)
    {
        const int parent = scopeSymbol();
        const bool member = parent >= 0 && scopeKind() == ScopeKind::Container &&
                            out_[static_cast<size_t>(parent)].kind != SymbolKind::Namespace;
        out_.push_back(make(member ? SymbolKind::Method : SymbolKind::Function, candidate_, candidateLine_, candidateIndent_, parent));
        out_.back().signature = signatureHead_;
        if (definition)
            scopes_.push_back({ScopeKind::Opaque, static_cast<int>(out_.size()) - 1});
        else
            out_.back().endLine = line;
        dropCandidate();
        pendingContainer_ = false;
    }

    // The parameter list, from the "(" that started the candidate.
    String signatureHead_;
    int signatureLine_ = 0;

    void startCandidate(const String& name, int line, int indent, bool typed, bool arrow)
    {
        signatureHead_ = currentLine_ ? parenText(*currentLine_, currentCol_) : String();
        signatureLine_ = currentTokenLine_;
        candidate_ = name;
        candidateLine_ = line;
        candidateIndent_ = indent;
        parenDepth_ = 1;
        typed_ = typed;
        arrow_ = arrow;
        initList_ = false;
        groupDepth_ = 0;
        assigned_ = false;
        assignedValue_.clear();
        phase_ = Phase::InParens;
    }

    void openBrace(const Token& t)
    {
        if (phase_ == Phase::AfterParens && (!arrow_ || prev_.text == "=>") && !assigned_)
            declare(true, t.line);
        else if (pendingContainer_ && !pendingName_.empty())
        {
            out_.push_back(make(pendingKind_, pendingName_, pendingLine_, pendingIndent_, scopeSymbol()));
            const ScopeKind kind = pendingKind_ == SymbolKind::Namespace ? ScopeKind::Namespace : ScopeKind::Container;
            scopes_.push_back({kind, static_cast<int>(out_.size()) - 1});
            pendingContainer_ = false;
        }
        else
        {
            // extern "C" {, namespace {, a top-level if (...) {: still declarations
            // inside. After "=", "," or "(" it is an initializer: opaque.
            const bool initializer = prev_.text == "=" || prev_.text == "," || prev_.text == "(" || prev_.text == "return" || prev_.text == "=>";
            const bool transparent = scopeKind() == ScopeKind::Namespace && !initializer;
            dropCandidate();
            pendingContainer_ = false;
            scopes_.push_back({transparent ? ScopeKind::Namespace : ScopeKind::Opaque, -1});
        }
    }

    void closeBrace(const Token& t)
    {
        dropCandidate();
        pendingContainer_ = false;
        if (scopes_.empty())
            return;
        if (scopes_.back().symbol >= 0)
            out_[static_cast<size_t>(scopes_.back().symbol)].endLine = t.line;
        scopes_.pop_back();
    }

    // Between the ")" of the parameters and the "{" or ";" that decides.
    void afterParens(const Token& t)
    {
        if (groupDepth_ > 0)
        {
            if (t.type == TokenType::Punct)
            {
                if (t.text == "(" || t.text == "[")
                    ++groupDepth_;
                else if (t.text == ")" || t.text == "]")
                    --groupDepth_;
            }
            return;
        }
        if (assigned_)
        {
            if (t.text == ";")
            {
                const bool pure = assignedValue_ == "0" || assignedValue_ == "default" || assignedValue_ == "delete";
                if (pure && !arrow_)
                    declare(false, t.line);
                else
                    dropCandidate();
            }
            else if (assignedValue_.empty())
                assignedValue_ = t.text;
            else
                dropCandidate(); // more than one token: an initializer, not "= 0"
            return;
        }
        if (t.type != TokenType::Punct)
            return; // const, override, noexcept, throws X, a return type
        if (t.text == ";")
        {
            if (!arrow_ && (typed_ || scopeKind() == ScopeKind::Container))
                declare(false, t.line);
            else
                dropCandidate();
        }
        else if (t.text == "=")
            assigned_ = true;
        else if (t.text == ":")
            initList_ = !arrow_;
        else if (t.text == "[")
            ++groupDepth_;
        else if (t.text == "(")
        {
            const bool allowed = initList_ || prev_.text == "noexcept" || prev_.text == "requires" || prev_.text == "throw" ||
                                 prev_.text == "decltype" || prev_.text == "alignas" || prev_.text == "__attribute__" || prev_.text == "__declspec";
            if (allowed)
                ++groupDepth_;
            else
                dropCandidate();
        }
        else if (t.text == "=>")
        {
            if (!arrow_)
                dropCandidate();
        }
        else if (t.text == "," || t.text == ")" || t.text == "]" || t.text == "?")
        {
            if (!initList_)
                dropCandidate();
        }
        // "->", "&", "&&", "*", "<", ">", "::", ".", "|": a trailing return type,
        // a ref-qualifier, "throws a.b": they decide nothing.
    }

    void feed(const Token& t, int indent)
    {
        if (t.type == TokenType::Punct && t.text == "{")
            openBrace(t);
        else if (t.type == TokenType::Punct && t.text == "}")
            closeBrace(t);
        else if (phase_ == Phase::InParens)
        {
            if (t.type == TokenType::Punct)
            {
                if (t.text == "(")
                    ++parenDepth_;
                else if (t.text == ")" && --parenDepth_ == 0)
                {
                    phase_ = Phase::AfterParens;
                    // On one line parenText already has it whole; over several
                    // lines it ends in "...)".
                    (void)t;
                }
            }
        }
        else if (phase_ == Phase::AfterParens)
            afterParens(t);
        else if (!scanning())
        {
            // inside a body: nothing to find
        }
        else if (t.type == TokenType::Punct)
        {
            if (t.text == "(")
            {
                if (prev_.type == TokenType::Ident && !isReservedName(prev_.text) && prev_.text[0] != '@' && canPrecedeName(prev2_))
                {
                    const bool typed = prev2_.type == TokenType::Ident || prev2_.text == ">" || prev2_.text == "*" || prev2_.text == "&" || prev2_.text == "&&" || prev2_.text == "]";
                    startCandidate(prev_.text, prev_.line, indent, typed, false);
                }
                else if (prev_.text == "function" && prev2_.text == "=" && !lastAssigned_.empty())
                    startCandidate(lastAssigned_, prev_.line, indent, true, false); // foo.bar = function(
                else if ((prev_.text == "=" && prev2_.type == TokenType::Ident && !isReservedName(prev2_.text)) || (prev_.text == "async" && prev2_.text == "="))
                    startCandidate(prev_.text == "=" ? prev2_.text : lastAssigned_, prev_.line, indent, true, true); // name = (a) =>
                pendingContainer_ = false;
            }
            else if (t.text == ";" || t.text == "=" || t.text == ",")
                pendingContainer_ = false; // "class X;", "class T = Foo"
        }
        else if (t.type == TokenType::Ident)
        {
            SymbolKind kind;
            if (isContainerWord(t.text, kind) && !(prev_.text == "enum" && t.text == "class"))
            {
                pendingContainer_ = true;
                pendingKind_ = kind;
                pendingName_.clear();
                pendingLine_ = t.line;
                pendingIndent_ = indent;
            }
            else if (pendingContainer_ && pendingName_.empty() && !isReservedName(t.text) && t.text[0] != '@')
                pendingName_ = t.text;
        }
        if (t.type == TokenType::Punct && t.text == "=" && prev_.type == TokenType::Ident)
            lastAssigned_ = prev_.text;
        prev2_ = prev_;
        prev_ = t;
    }

    const String* currentLine_ = nullptr; // the line being tokenized, for signatures
    size_t currentCol_ = 0;               // where the token being fed starts
    int currentTokenLine_ = 0;

    void scanLine(const String& line, int lineIndex)
    {
        const size_t n = line.size();
        size_t i = 0;
        const int indent = indentOf(line);
        currentLine_ = &line;
        currentTokenLine_ = lineIndex;
        if (!blockComment_ && !templateString_)
        {
            const size_t first = skipBlanks(line, 0);
            const bool directive = lineContinues_ || (first < n && line[first] == '#');
            if (directive)
            {
                lineContinues_ = n > 0 && line[n - 1] == '\\';
                return; // a preprocessor line, and the lines a "\" continues it on
            }
        }
        Token t;
        t.line = lineIndex;
        while (i < n)
        {
            if (blockComment_)
            {
                const size_t close = line.find("*/", i);
                if (close == String::npos)
                    return;
                blockComment_ = false;
                i = close + 2;
                continue;
            }
            if (templateString_)
            {
                while (i < n && line[i] != '`')
                    i += line[i] == '\\' ? 2 : 1;
                if (i >= n)
                    return;
                templateString_ = false;
                ++i;
                t.type = TokenType::String;
                t.text.clear();
                feed(t, indent);
                continue;
            }
            const char c = line[i];
            if (isBlank(c))
            {
                ++i;
                continue;
            }
            if (c == '/' && i + 1 < n && line[i + 1] == '/')
                return;
            if (c == '/' && i + 1 < n && line[i + 1] == '*')
            {
                blockComment_ = true;
                i += 2;
                continue;
            }
            if (c == '"' || c == '\'' || c == '`')
            {
                ++i;
                while (i < n && line[i] != c)
                    i += line[i] == '\\' ? 2 : 1;
                if (i < n)
                    ++i;
                else if (c == '`')
                    templateString_ = true;
                t.type = TokenType::String;
                t.text.clear();
                feed(t, indent);
                continue;
            }
            if (isIdentStart(c) || c == '@' || c == '$' || (c == '~' && i + 1 < n && isIdentStart(line[i + 1])))
            {
                const size_t start = i;
                if (c == '@' || c == '$' || c == '~')
                    ++i;
                while (i < n && (isIdentChar(line[i]) || line[i] == '$'))
                    ++i;
                // "A::B::~C" is one name.
                while (i + 2 < n && line[i] == ':' && line[i + 1] == ':' && (isIdentStart(line[i + 2]) || line[i + 2] == '~'))
                {
                    i += 2;
                    if (line[i] == '~')
                        ++i;
                    while (i < n && isIdentChar(line[i]))
                        ++i;
                }
                t.type = TokenType::Ident;
                t.text = line.substr(start, i - start);
                feed(t, indent);
                continue;
            }
            if (c >= '0' && c <= '9')
            {
                const size_t start = i;
                while (i < n && (isIdentChar(line[i]) || line[i] == '.'))
                    ++i;
                t.type = TokenType::Number;
                t.text = line.substr(start, i - start);
                feed(t, indent);
                continue;
            }
            t.type = TokenType::Punct;
            t.col = i;
            currentCol_ = i;
            if (i + 1 < n && isTwoCharPunct(c, line[i + 1]))
            {
                t.text = line.substr(i, 2);
                i += 2;
            }
            else
            {
                t.text = String(&c, 1);
                ++i;
            }
            feed(t, indent);
        }
    }
};

} // namespace

// ── API ─────────────────────────────────────────────────────────────────

ct::Vector<Symbol> scanSymbols(const ct::Vector<String>& lines, SymbolSyntax syntax)
{
    switch (syntax)
    {
    case SymbolSyntax::Braces: return BraceScanner().run(lines);
    case SymbolSyntax::Indent: return scanIndent(lines);
    case SymbolSyntax::Lua: return scanLua(lines);
    case SymbolSyntax::Blitz: return scanBlitz(lines);
    case SymbolSyntax::None: break;
    }
    return ct::Vector<Symbol>();
}

ct::Vector<Symbol> scanSymbols(const String& text, SymbolSyntax syntax)
{
    ct::Vector<String> lines;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i)
    {
        if (i < text.size() && text[i] != '\n')
            continue;
        size_t n = i - start;
        if (n > 0 && text[start + n - 1] == '\r')
            --n;
        lines.push_back(text.substr(start, n));
        start = i + 1;
    }
    return scanSymbols(lines, syntax);
}

int enclosingSymbol(const ct::Vector<Symbol>& symbols, int line)
{
    int found = -1;
    for (size_t i = 0; i < symbols.size(); ++i)
    {
        const Symbol& s = symbols[i];
        if (s.kind == SymbolKind::Import || s.kind == SymbolKind::Variable || s.kind == SymbolKind::Constant || s.kind == SymbolKind::Label)
            continue;
        // Children follow their parent and lie inside it: the last block
        // holding the line is the innermost.
        if (s.line <= line && line <= s.endLine)
            found = static_cast<int>(i);
    }
    return found;
}

int findSymbol(const ct::Vector<Symbol>& symbols, StringView name)
{
    for (size_t i = 0; i < symbols.size(); ++i)
        if (sameName(symbols[i].name, name))
            return static_cast<int>(i);
    return -1;
}

bool isContainerKind(SymbolKind kind)
{
    return kind == SymbolKind::Namespace || kind == SymbolKind::Class || kind == SymbolKind::Struct || kind == SymbolKind::Enum || kind == SymbolKind::Interface;
}

const char* symbolKindName(SymbolKind kind)
{
    switch (kind)
    {
    case SymbolKind::Namespace: return "namespace";
    case SymbolKind::Class: return "class";
    case SymbolKind::Struct: return "struct";
    case SymbolKind::Enum: return "enum";
    case SymbolKind::Interface: return "interface";
    case SymbolKind::Function: return "function";
    case SymbolKind::Method: return "method";
    case SymbolKind::Variable: return "variable";
    case SymbolKind::Constant: return "constant";
    case SymbolKind::Label: return "label";
    case SymbolKind::Import: return "import";
    }
    return "";
}

} // namespace syntax
} // namespace ig
