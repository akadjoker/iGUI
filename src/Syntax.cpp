#include "igui/Syntax.hpp"

namespace ig { namespace syntax
{

namespace
{
// ASCII-only character classification, matching the immediate-mode core's
// no-cctype contract (see cmake/CheckNoStd.cmake). Source code identifiers
// are always ASCII/UTF-8 here, so a locale-aware classifier would only add
// risk (undefined behavior on a negative char) for no benefit.
inline bool asciiIsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool asciiIsDigit(char c) { return c >= '0' && c <= '9'; }
inline bool asciiIsAlnum(char c) { return asciiIsAlpha(c) || asciiIsDigit(c); }
inline bool asciiIsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }
inline char asciiToLower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
inline bool isOneOf(char c, const char* set) { for (; *set; ++set) if (*set == c) return true; return false; }
} // anonymous namespace

// ═════════════════════════════════════════════════════════════════════════════
//  SyntaxHighlighter — base class
// ═════════════════════════════════════════════════════════════════════════════

Color SyntaxHighlighter::colorFor(TokenType type) const
{
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < TokenTypeCount) return colors_[idx];
    return colors_[0];
}

void SyntaxHighlighter::setColor(TokenType type, const Color& c)
{
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < TokenTypeCount) colors_[idx] = c;
}

bool SyntaxHighlighter::isKeyword(const String& word) const
{ return keywords_.contains(word); }

bool SyntaxHighlighter::isType(const String& word) const
{ return types_.contains(word); }

bool SyntaxHighlighter::isConstant(const String& word) const
{ return constants_.contains(word); }

void SyntaxHighlighter::setKeywords(const ct::Vector<const char*>& words)
{ keywords_.clear(); for (auto* w : words) keywords_.insert(w); }

void SyntaxHighlighter::setTypes(const ct::Vector<const char*>& words)
{ types_.clear(); for (auto* w : words) types_.insert(w); }

void SyntaxHighlighter::setConstants(const ct::Vector<const char*>& words)
{ constants_.clear(); for (auto* w : words) constants_.insert(w); }

// ═════════════════════════════════════════════════════════════════════════════
//  C-like lexer helpers (shared by C++, JS, GLSL, Java)
// ═════════════════════════════════════════════════════════════════════════════

namespace {

using TT = SyntaxHighlighter::TokenType;
using Span = SyntaxHighlighter::Span;

inline bool isIdStart(char c) { return asciiIsAlpha(c) || c == '_'; }
inline bool isIdChar(char c)  { return asciiIsAlnum(c) || c == '_'; }
inline bool isDigit(char c)   { return c >= '0' && c <= '9'; }
inline bool isHexDigit(char c)
{
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
inline bool isOperatorChar(char c)
{
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
           c == '=' || c == '!' || c == '<' || c == '>' || c == '&' ||
           c == '|' || c == '^' || c == '~' || c == '?' || c == ':';
}
inline bool isBracket(char c)
{
    return c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']';
}

// State constants for multi-line constructs
enum LexState {
    StateNormal    = 0,
    StateBlockComment = 1,   // inside /* ... */
    StateTripleDQ  = 2,      // inside """ ... """ (Python)
    StateTripleSQ  = 3,      // inside ''' ... ''' (Python)
    StateLuaBlock  = 4,      // inside --[[ ... ]]
    StateLuaString = 5,      // inside [[ ... ]]
};

// Scan a number literal (int, float, hex, bin, oct)
int scanNumber(const String& text, int i)
{
    int n = static_cast<int>(text.size());
    if (i >= n) return i;
    if (text[i] == '0' && i + 1 < n) {
        char next = text[i + 1];
        if (next == 'x' || next == 'X') { // hex
            i += 2;
            while (i < n && isHexDigit(text[i])) i++;
            return i;
        }
        if (next == 'b' || next == 'B') { // bin
            i += 2;
            while (i < n && (text[i] == '0' || text[i] == '1')) i++;
            return i;
        }
    }
    while (i < n && isDigit(text[i])) i++;
    if (i < n && text[i] == '.') {
        i++;
        while (i < n && isDigit(text[i])) i++;
    }
    if (i < n && (text[i] == 'e' || text[i] == 'E')) {
        i++;
        if (i < n && (text[i] == '+' || text[i] == '-')) i++;
        while (i < n && isDigit(text[i])) i++;
    }
    // Suffixes: f, u, l, ll, etc.
    while (i < n && (text[i] == 'f' || text[i] == 'F' || text[i] == 'u' ||
                     text[i] == 'U' || text[i] == 'l' || text[i] == 'L'))
        i++;
    return i;
}

// Count leading whitespace bytes (for syntax highlighter char-index comparisons)
int leadingSpaceBytes(const String& text)
{
    int i = 0, n = static_cast<int>(text.size());
    while (i < n && (text[i] == ' ' || text[i] == '\t')) i++;
    return i;
}

// Count net brace delta (for C-like languages)
int braceDelta(const String& text)
{
    int delta = 0;
    bool inStr = false;
    char strCh = 0;
    for (int i = 0; i < static_cast<int>(text.size()); i++) {
        char c = text[i];
        if (inStr) {
            if (c == '\\') { i++; continue; }
            if (c == strCh) inStr = false;
            continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; continue; }
        if (c == '/' && i + 1 < static_cast<int>(text.size())) {
            if (text[i+1] == '/') break;         // line comment — stop
            if (text[i+1] == '*') { i++; continue; } // block comment start
        }
        if (c == '{') delta++;
        if (c == '}') delta--;
    }
    return delta;
}

} // anonymous namespace

// ═════════════════════════════════════════════════════════════════════════════
//  CppHighlighter
// ═════════════════════════════════════════════════════════════════════════════

CppHighlighter::CppHighlighter()
{
    setKeywords({
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand",
        "bitor", "break", "case", "catch", "class", "compl", "concept",
        "const", "consteval", "constexpr", "constinit", "const_cast",
        "continue", "co_await", "co_return", "co_yield", "decltype",
        "default", "delete", "do", "dynamic_cast", "else", "enum",
        "explicit", "export", "extern", "for", "friend", "goto", "if",
        "inline", "mutable", "namespace", "new", "noexcept", "not",
        "not_eq", "operator", "or", "or_eq", "private", "protected",
        "public", "register", "reinterpret_cast", "requires", "return",
        "sizeof", "static", "static_assert", "static_cast", "struct",
        "switch", "template", "this", "throw", "try", "typedef",
        "typeid", "typename", "union", "using", "virtual", "volatile",
        "while", "xor", "xor_eq", "override", "final",
    });
    setTypes({
        "bool", "char", "char8_t", "char16_t", "char32_t", "double",
        "float", "int", "long", "short", "signed", "unsigned", "void",
        "wchar_t", "int8_t", "int16_t", "int32_t", "int64_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t", "size_t",
        "ptrdiff_t", "intptr_t", "uintptr_t", "string", "vector",
        "map", "set", "array", "pair", "tuple", "optional", "variant",
        "unique_ptr", "shared_ptr", "weak_ptr", "function",
    });
    setConstants({
        "true", "false", "nullptr", "NULL", "EOF",
    });
}

SyntaxHighlighter::HighlightResult
CppHighlighter::highlightLine(int /*lineIndex*/, const String& text,
                               int prevState) const
{
    HighlightResult result;
    int n = static_cast<int>(text.size());
    int i = 0;
    int state = prevState;

    auto push = [&](int start, int end, TT type) {
        if (start < end)
            result.spans.push_back({start, end, type});
    };

    // Continue block comment from previous line
    if (state == StateBlockComment) {
        while (i < n) {
            if (text[i] == '*' && i + 1 < n && text[i+1] == '/') {
                i += 2;
                push(0, i, TT::Comment);
                state = StateNormal;
                break;
            }
            i++;
        }
        if (state == StateBlockComment) {
            push(0, n, TT::Comment);
            result.state = StateBlockComment;
            return result;
        }
    }

    while (i < n) {
        char c = text[i];

        // Whitespace
        if (c == ' ' || c == '\t') { i++; continue; }

        // Preprocessor directive
        if (c == '#' && leadingSpaceBytes(text) == i) {
            push(i, n, TT::Preprocessor);
            i = n;
            continue;
        }

        // Single-line comment
        if (c == '/' && i + 1 < n && text[i+1] == '/') {
            push(i, n, TT::Comment);
            i = n;
            continue;
        }

        // Block comment start
        if (c == '/' && i + 1 < n && text[i+1] == '*') {
            int start = i;
            i += 2;
            while (i < n) {
                if (text[i] == '*' && i + 1 < n && text[i+1] == '/') { i += 2; break; }
                i++;
            }
            bool closed = (i <= n && i >= 2 && text[i-2] == '*' && text[i-1] == '/');
            push(start, i, TT::Comment);
            if (!closed) {
                state = StateBlockComment;
                result.state = state;
                return result;
            }
            continue;
        }

        // String / char literal
        if (c == '"' || c == '\'') {
            int start = i;
            char quote = c;
            i++;
            while (i < n) {
                if (text[i] == '\\') { i += 2; continue; }
                if (text[i] == quote) { i++; break; }
                i++;
            }
            push(start, i, TT::String);
            continue;
        }

        // Number
        if (isDigit(c) || (c == '.' && i + 1 < n && isDigit(text[i+1]))) {
            int start = i;
            i = scanNumber(text, i);
            push(start, i, TT::Number);
            continue;
        }

        // Identifier / keyword / type / function
        if (isIdStart(c)) {
            int start = i;
            while (i < n && isIdChar(text[i])) i++;
            String word = text.substr(start, i - start);
            TT type = TT::Default;
            if (isKeyword(word))       type = TT::Keyword;
            else if (isType(word))     type = TT::Type;
            else if (isConstant(word)) type = TT::Constant;
            else {
                // Check if followed by '(' → function
                int j = i;
                while (j < n && text[j] == ' ') j++;
                if (j < n && text[j] == '(') type = TT::Function;
            }
            push(start, i, type);
            continue;
        }

        // Brackets
        if (isBracket(c)) {
            push(i, i + 1, TT::Bracket);
            i++;
            continue;
        }

        // Operators
        if (isOperatorChar(c)) {
            int start = i;
            while (i < n && isOperatorChar(text[i])) i++;
            push(start, i, TT::Operator);
            continue;
        }

        // Anything else (semicolons, commas, dots...)
        i++;
    }

    result.state = StateNormal;
    return result;
}

int CppHighlighter::foldDelta(int /*lineIndex*/, const String& text) const
{
    return braceDelta(text);
}

// ═════════════════════════════════════════════════════════════════════════════
//  PythonHighlighter
// ═════════════════════════════════════════════════════════════════════════════

PythonHighlighter::PythonHighlighter()
{
    setKeywords({
        "False", "None", "True", "and", "as", "assert", "async", "await",
        "break", "class", "continue", "def", "del", "elif", "else",
        "except", "finally", "for", "from", "global", "if", "import",
        "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise",
        "return", "try", "while", "with", "yield",
    });
    setTypes({
        "int", "float", "str", "bool", "list", "dict", "set", "tuple",
        "bytes", "bytearray", "complex", "frozenset", "range", "type",
        "object", "Exception", "BaseException",
    });
    setConstants({
        "True", "False", "None", "__name__", "__file__", "__doc__",
    });
}

SyntaxHighlighter::HighlightResult
PythonHighlighter::highlightLine(int /*lineIndex*/, const String& text,
                                  int prevState) const
{
    HighlightResult result;
    int n = static_cast<int>(text.size());
    int i = 0;
    int state = prevState;

    auto push = [&](int start, int end, TT type) {
        if (start < end)
            result.spans.push_back({start, end, type});
    };

    // Continue multi-line string from previous line
    if (state == StateTripleDQ || state == StateTripleSQ) {
        const char* delim = (state == StateTripleDQ) ? "\"\"\"" : "'''";
        while (i < n) {
            if (text[i] == '\\') { i += 2; continue; }
            if (i + 2 < n && text[i] == delim[0] && text[i+1] == delim[0] && text[i+2] == delim[0]) {
                i += 3;
                push(0, i, TT::String);
                state = StateNormal;
                break;
            }
            i++;
        }
        if (state != StateNormal) {
            push(0, n, TT::String);
            result.state = state;
            return result;
        }
    }

    while (i < n) {
        char c = text[i];

        if (c == ' ' || c == '\t') { i++; continue; }

        // Comment
        if (c == '#') {
            push(i, n, TT::Comment);
            i = n;
            continue;
        }

        // Decorator
        if (c == '@' && leadingSpaceBytes(text) == i) {
            int start = i;
            i++;
            while (i < n && isIdChar(text[i])) i++;
            push(start, i, TT::Attribute);
            continue;
        }

        // Triple-quoted string
        if ((c == '"' || c == '\'') && i + 2 < n && text[i+1] == c && text[i+2] == c) {
            int start = i;
            char q = c;
            int tripleState = (q == '"') ? StateTripleDQ : StateTripleSQ;
            i += 3;
            bool closed = false;
            while (i < n) {
                if (text[i] == '\\') { i += 2; continue; }
                if (i + 2 < n && text[i] == q && text[i+1] == q && text[i+2] == q) {
                    i += 3; closed = true; break;
                }
                i++;
            }
            push(start, i, TT::String);
            if (!closed) {
                result.state = tripleState;
                return result;
            }
            continue;
        }

        // Single/double string
        if (c == '"' || c == '\'') {
            int start = i;
            char q = c;
            // Handle f-string prefix
            if (start > 0 && (text[start-1] == 'f' || text[start-1] == 'F' ||
                              text[start-1] == 'r' || text[start-1] == 'R' ||
                              text[start-1] == 'b' || text[start-1] == 'B')) {
                // prefix already consumed as identifier; just handle quote
            }
            i++;
            while (i < n) {
                if (text[i] == '\\') { i += 2; continue; }
                if (text[i] == q) { i++; break; }
                i++;
            }
            push(start, i, TT::String);
            continue;
        }

        // Number
        if (isDigit(c) || (c == '.' && i + 1 < n && isDigit(text[i+1]))) {
            int start = i;
            i = scanNumber(text, i);
            push(start, i, TT::Number);
            continue;
        }

        // Identifier / keyword
        if (isIdStart(c)) {
            int start = i;
            while (i < n && isIdChar(text[i])) i++;
            String word = text.substr(start, i - start);
            TT type = TT::Default;
            if (isKeyword(word))       type = TT::Keyword;
            else if (isType(word))     type = TT::Type;
            else if (isConstant(word)) type = TT::Constant;
            else {
                int j = i;
                while (j < n && text[j] == ' ') j++;
                if (j < n && text[j] == '(') type = TT::Function;
            }
            push(start, i, type);
            continue;
        }

        if (isBracket(c)) { push(i, i + 1, TT::Bracket); i++; continue; }
        if (isOperatorChar(c)) {
            int start = i;
            while (i < n && isOperatorChar(text[i])) i++;
            push(start, i, TT::Operator);
            continue;
        }
        i++;
    }

    result.state = StateNormal;
    return result;
}

int PythonHighlighter::foldDelta(int /*lineIndex*/, const String& text) const
{
    // Python folds by trailing colon (def, class, if, for, etc.)
    // A line ending with ':' opens a fold; closing is implicit
    // (handled by indentation decrease in next non-empty line).
    // Simplified: +1 if line ends with ':', 0 otherwise.
    int n = static_cast<int>(text.size());
    int last = n - 1;
    while (last >= 0 && (text[last] == ' ' || text[last] == '\t')) last--;
    if (last >= 0 && text[last] == ':') return +1;
    return 0;
}

// ═════════════════════════════════════════════════════════════════════════════
//  JsHighlighter
// ═════════════════════════════════════════════════════════════════════════════

JsHighlighter::JsHighlighter()
{
    setKeywords({
        "async", "await", "break", "case", "catch", "class", "const",
        "continue", "debugger", "default", "delete", "do", "else",
        "export", "extends", "finally", "for", "from", "function", "if",
        "import", "in", "instanceof", "let", "new", "of", "return",
        "static", "super", "switch", "this", "throw", "try", "typeof",
        "var", "void", "while", "with", "yield",
        // TypeScript
        "abstract", "as", "declare", "enum", "implements", "interface",
        "module", "namespace", "private", "protected", "public",
        "readonly", "type",
    });
    setTypes({
        "Array", "Boolean", "Date", "Error", "Function", "Map", "Number",
        "Object", "Promise", "RegExp", "Set", "String", "Symbol",
        "WeakMap", "WeakSet",
        // TypeScript types
        "any", "boolean", "never", "number", "string", "unknown", "void",
    });
    setConstants({
        "true", "false", "null", "undefined", "NaN", "Infinity",
    });
}

SyntaxHighlighter::HighlightResult
JsHighlighter::highlightLine(int /*lineIndex*/, const String& text,
                              int prevState) const
{
    HighlightResult result;
    int n = static_cast<int>(text.size());
    int i = 0;
    int state = prevState;

    auto push = [&](int start, int end, TT type) {
        if (start < end)
            result.spans.push_back({start, end, type});
    };

    if (state == StateBlockComment) {
        while (i < n) {
            if (text[i] == '*' && i + 1 < n && text[i+1] == '/') {
                i += 2;
                push(0, i, TT::Comment);
                state = StateNormal;
                break;
            }
            i++;
        }
        if (state == StateBlockComment) {
            push(0, n, TT::Comment);
            result.state = StateBlockComment;
            return result;
        }
    }

    while (i < n) {
        char c = text[i];
        if (c == ' ' || c == '\t') { i++; continue; }

        if (c == '/' && i + 1 < n && text[i+1] == '/') {
            push(i, n, TT::Comment);
            i = n; continue;
        }
        if (c == '/' && i + 1 < n && text[i+1] == '*') {
            int start = i; i += 2;
            while (i < n) {
                if (text[i] == '*' && i + 1 < n && text[i+1] == '/') { i += 2; break; }
                i++;
            }
            bool closed = (i >= 2 && text[i-2] == '*' && text[i-1] == '/');
            push(start, i, TT::Comment);
            if (!closed) { result.state = StateBlockComment; return result; }
            continue;
        }

        // Template literal
        if (c == '`') {
            int start = i; i++;
            while (i < n && text[i] != '`') {
                if (text[i] == '\\') { i += 2; continue; }
                i++;
            }
            if (i < n) i++; // skip closing `
            push(start, i, TT::String);
            continue;
        }

        if (c == '"' || c == '\'') {
            int start = i; char q = c; i++;
            while (i < n) {
                if (text[i] == '\\') { i += 2; continue; }
                if (text[i] == q) { i++; break; }
                i++;
            }
            push(start, i, TT::String);
            continue;
        }

        if (isDigit(c) || (c == '.' && i + 1 < n && isDigit(text[i+1]))) {
            int start = i; i = scanNumber(text, i);
            push(start, i, TT::Number);
            continue;
        }

        if (isIdStart(c) || c == '$') {
            int start = i;
            while (i < n && (isIdChar(text[i]) || text[i] == '$')) i++;
            String word = text.substr(start, i - start);
            TT type = TT::Default;
            if (isKeyword(word))       type = TT::Keyword;
            else if (isType(word))     type = TT::Type;
            else if (isConstant(word)) type = TT::Constant;
            else {
                int j = i;
                while (j < n && text[j] == ' ') j++;
                if (j < n && text[j] == '(') type = TT::Function;
            }
            push(start, i, type);
            continue;
        }

        if (c == '=' && i + 1 < n && text[i+1] == '>') {
            push(i, i + 2, TT::Operator); i += 2; continue;
        }

        if (isBracket(c)) { push(i, i + 1, TT::Bracket); i++; continue; }
        if (isOperatorChar(c)) {
            int start = i;
            while (i < n && isOperatorChar(text[i])) i++;
            push(start, i, TT::Operator);
            continue;
        }
        i++;
    }

    result.state = StateNormal;
    return result;
}

int JsHighlighter::foldDelta(int /*lineIndex*/, const String& text) const
{
    return braceDelta(text);
}

// ═════════════════════════════════════════════════════════════════════════════
//  LuaHighlighter
// ═════════════════════════════════════════════════════════════════════════════

LuaHighlighter::LuaHighlighter()
{
    setKeywords({
        "and", "break", "do", "else", "elseif", "end", "for", "function",
        "goto", "if", "in", "local", "not", "or", "repeat", "return",
        "then", "until", "while",
    });
    setTypes({
        "string", "table", "math", "io", "os", "coroutine", "debug",
        "package", "utf8",
    });
    setConstants({
        "true", "false", "nil", "_G", "_VERSION",
    });
}

SyntaxHighlighter::HighlightResult
LuaHighlighter::highlightLine(int /*lineIndex*/, const String& text,
                               int prevState) const
{
    HighlightResult result;
    int n = static_cast<int>(text.size());
    int i = 0;
    int state = prevState;

    auto push = [&](int start, int end, TT type) {
        if (start < end) result.spans.push_back({start, end, type});
    };

    // Continue multi-line block comment --[[ ... ]]
    if (state == StateLuaBlock) {
        while (i < n) {
            if (text[i] == ']' && i + 1 < n && text[i+1] == ']') {
                i += 2;
                push(0, i, TT::Comment);
                state = StateNormal;
                break;
            }
            i++;
        }
        if (state == StateLuaBlock) {
            push(0, n, TT::Comment);
            result.state = StateLuaBlock;
            return result;
        }
    }

    // Continue multi-line string [[ ... ]]
    if (state == StateLuaString) {
        while (i < n) {
            if (text[i] == ']' && i + 1 < n && text[i+1] == ']') {
                i += 2;
                push(0, i, TT::String);
                state = StateNormal;
                break;
            }
            i++;
        }
        if (state == StateLuaString) {
            push(0, n, TT::String);
            result.state = StateLuaString;
            return result;
        }
    }

    while (i < n) {
        char c = text[i];
        if (c == ' ' || c == '\t') { i++; continue; }

        // Block comment --[[ ... ]]
        if (c == '-' && i + 3 < n && text[i+1] == '-' && text[i+2] == '[' && text[i+3] == '[') {
            int start = i; i += 4;
            while (i < n) {
                if (text[i] == ']' && i + 1 < n && text[i+1] == ']') { i += 2; break; }
                i++;
            }
            bool closed = (i >= 2 && text[i-2] == ']' && text[i-1] == ']');
            push(start, i, TT::Comment);
            if (!closed) { result.state = StateLuaBlock; return result; }
            continue;
        }

        // Line comment --
        if (c == '-' && i + 1 < n && text[i+1] == '-') {
            push(i, n, TT::Comment); i = n; continue;
        }

        // Multi-line string [[ ... ]]
        if (c == '[' && i + 1 < n && text[i+1] == '[') {
            int start = i; i += 2;
            while (i < n) {
                if (text[i] == ']' && i + 1 < n && text[i+1] == ']') { i += 2; break; }
                i++;
            }
            bool closed = (i >= 2 && text[i-2] == ']' && text[i-1] == ']');
            push(start, i, TT::String);
            if (!closed) { result.state = StateLuaString; return result; }
            continue;
        }

        if (c == '"' || c == '\'') {
            int start = i; char q = c; i++;
            while (i < n) {
                if (text[i] == '\\') { i += 2; continue; }
                if (text[i] == q) { i++; break; }
                i++;
            }
            push(start, i, TT::String);
            continue;
        }

        if (isDigit(c) || (c == '.' && i + 1 < n && isDigit(text[i+1]))) {
            int start = i; i = scanNumber(text, i);
            push(start, i, TT::Number);
            continue;
        }

        if (isIdStart(c)) {
            int start = i;
            while (i < n && isIdChar(text[i])) i++;
            String word = text.substr(start, i - start);
            TT type = TT::Default;
            if (isKeyword(word))       type = TT::Keyword;
            else if (isType(word))     type = TT::Type;
            else if (isConstant(word)) type = TT::Constant;
            else {
                int j = i;
                while (j < n && text[j] == ' ') j++;
                if (j < n && (text[j] == '(' || text[j] == '{')) type = TT::Function;
            }
            push(start, i, type);
            continue;
        }

        if (isBracket(c)) { push(i, i + 1, TT::Bracket); i++; continue; }
        if (isOperatorChar(c) || c == '.' || c == '#') {
            int start = i;
            while (i < n && (isOperatorChar(text[i]) || text[i] == '.' || text[i] == '#')) i++;
            push(start, i, TT::Operator);
            continue;
        }
        i++;
    }

    result.state = StateNormal;
    return result;
}

int LuaHighlighter::foldDelta(int /*lineIndex*/, const String& text) const
{
    // Scan for opening/closing keywords
    int delta = 0;
    int n = static_cast<int>(text.size());
    int i = 0;
    while (i < n) {
        if (!isIdStart(text[i])) { i++; continue; }
        int start = i;
        while (i < n && isIdChar(text[i])) i++;
        String word = text.substr(start, i - start);
        if (word == "function" || word == "do" || word == "then" ||
            word == "repeat" || word == "if")
            delta++;
        else if (word == "end" || word == "until")
            delta--;
    }
    return delta;
}

// ═════════════════════════════════════════════════════════════════════════════
//  GlslHighlighter
// ═════════════════════════════════════════════════════════════════════════════

GlslHighlighter::GlslHighlighter()
{
    setKeywords({
        "attribute", "break", "case", "centroid", "const", "continue",
        "default", "discard", "do", "else", "flat", "for", "highp", "if",
        "in", "inout", "invariant", "layout", "lowp", "mediump",
        "noperspective", "out", "precision", "return", "smooth", "struct",
        "subroutine", "switch", "uniform", "varying", "while",
        "buffer", "coherent", "readonly", "restrict", "volatile",
        "writeonly", "shared",
    });
    setTypes({
        "void", "bool", "int", "uint", "float", "double",
        "vec2", "vec3", "vec4", "bvec2", "bvec3", "bvec4",
        "ivec2", "ivec3", "ivec4", "uvec2", "uvec3", "uvec4",
        "dvec2", "dvec3", "dvec4",
        "mat2", "mat3", "mat4", "mat2x2", "mat2x3", "mat2x4",
        "mat3x2", "mat3x3", "mat3x4", "mat4x2", "mat4x3", "mat4x4",
        "sampler1D", "sampler2D", "sampler3D", "samplerCube",
        "sampler2DShadow", "samplerCubeShadow",
        "sampler1DArray", "sampler2DArray", "isampler2D", "usampler2D",
        "image2D", "iimage2D", "uimage2D",
    });
    setConstants({
        "true", "false",
        "gl_Position", "gl_FragColor", "gl_FragCoord", "gl_VertexID",
        "gl_InstanceID", "gl_PointSize", "gl_FrontFacing",
    });
}

SyntaxHighlighter::HighlightResult
GlslHighlighter::highlightLine(int lineIndex, const String& text,
                                int prevState) const
{
    // GLSL uses the same comment/string/number syntax as C++
    // Reuse CppHighlighter's logic (we just have different keywords/types)
    // But since we can't call Cpp's method, we duplicate the core scanner.
    // (In a production codebase we'd factor out a shared C-like scanner.)

    HighlightResult result;
    int n = static_cast<int>(text.size());
    int i = 0;
    int state = prevState;

    auto push = [&](int start, int end, TT type) {
        if (start < end) result.spans.push_back({start, end, type});
    };

    if (state == StateBlockComment) {
        while (i < n) {
            if (text[i] == '*' && i + 1 < n && text[i+1] == '/') { i += 2; push(0, i, TT::Comment); state = StateNormal; break; }
            i++;
        }
        if (state == StateBlockComment) { push(0, n, TT::Comment); result.state = StateBlockComment; return result; }
    }

    while (i < n) {
        char c = text[i];
        if (c == ' ' || c == '\t') { i++; continue; }

        if (c == '#' && leadingSpaceBytes(text) == i) { push(i, n, TT::Preprocessor); i = n; continue; }

        if (c == '/' && i + 1 < n && text[i+1] == '/') { push(i, n, TT::Comment); i = n; continue; }
        if (c == '/' && i + 1 < n && text[i+1] == '*') {
            int start = i; i += 2;
            while (i < n) { if (text[i] == '*' && i + 1 < n && text[i+1] == '/') { i += 2; break; } i++; }
            bool closed = (i >= 2 && text[i-2] == '*' && text[i-1] == '/');
            push(start, i, TT::Comment);
            if (!closed) { result.state = StateBlockComment; return result; }
            continue;
        }

        if (c == '"' || c == '\'') {
            int start = i; char q = c; i++;
            while (i < n) { if (text[i] == '\\') { i += 2; continue; } if (text[i] == q) { i++; break; } i++; }
            push(start, i, TT::String); continue;
        }

        if (isDigit(c) || (c == '.' && i + 1 < n && isDigit(text[i+1]))) {
            int start = i; i = scanNumber(text, i); push(start, i, TT::Number); continue;
        }

        if (isIdStart(c)) {
            int start = i;
            while (i < n && isIdChar(text[i])) i++;
            String word = text.substr(start, i - start);
            TT type = TT::Default;
            if (isKeyword(word))       type = TT::Keyword;
            else if (isType(word))     type = TT::Type;
            else if (isConstant(word)) type = TT::Constant;
            else {
                int j = i; while (j < n && text[j] == ' ') j++;
                if (j < n && text[j] == '(') type = TT::Function;
            }
            push(start, i, type); continue;
        }

        if (isBracket(c)) { push(i, i + 1, TT::Bracket); i++; continue; }
        if (isOperatorChar(c)) {
            int start = i; while (i < n && isOperatorChar(text[i])) i++;
            push(start, i, TT::Operator); continue;
        }
        i++;
    }

    result.state = StateNormal;
    (void)lineIndex;
    return result;
}

int GlslHighlighter::foldDelta(int /*lineIndex*/, const String& text) const
{
    return braceDelta(text);
}

// ═════════════════════════════════════════════════════════════════════════════
//  JavaHighlighter
//
//  Same lexical family as C++ (braces, // and /* */ comments, "..."/'...'
//  literals, C-style numbers) but its own keyword/type/constant sets and no
//  preprocessor directives or raw/triple-quoted strings. Annotations
//  (@Override, @FunctionalInterface, ...) highlight like Python decorators.
// ═════════════════════════════════════════════════════════════════════════════

JavaHighlighter::JavaHighlighter()
{
    setKeywords({
        "abstract", "assert", "break", "case", "catch", "class", "const",
        "continue", "default", "do", "else", "enum", "extends", "final",
        "finally", "for", "goto", "if", "implements", "import", "instanceof",
        "interface", "native", "new", "package", "private", "protected",
        "public", "return", "static", "strictfp", "super", "switch",
        "synchronized", "this", "throw", "throws", "transient", "try",
        "var", "volatile", "while", "yield", "record", "sealed", "permits",
        "non-sealed",
    });
    setTypes({
        "boolean", "byte", "char", "double", "float", "int", "long", "short",
        "void", "String", "Integer", "Long", "Short", "Byte", "Double",
        "Float", "Boolean", "Character", "Object", "List", "ArrayList",
        "Map", "HashMap", "Set", "HashSet", "Optional", "Exception",
        "RuntimeException", "Thread", "Number",
    });
    setConstants({
        "true", "false", "null",
    });
}

SyntaxHighlighter::HighlightResult
JavaHighlighter::highlightLine(int /*lineIndex*/, const String& text,
                                int prevState) const
{
    HighlightResult result;
    int n = static_cast<int>(text.size());
    int i = 0;
    int state = prevState;

    auto push = [&](int start, int end, TT type) {
        if (start < end) result.spans.push_back({start, end, type});
    };

    // Continue block comment from previous line
    if (state == StateBlockComment) {
        while (i < n) {
            if (text[i] == '*' && i + 1 < n && text[i+1] == '/') {
                i += 2;
                push(0, i, TT::Comment);
                state = StateNormal;
                break;
            }
            i++;
        }
        if (state == StateBlockComment) {
            push(0, n, TT::Comment);
            result.state = StateBlockComment;
            return result;
        }
    }

    while (i < n) {
        char c = text[i];
        if (c == ' ' || c == '\t') { i++; continue; }

        // Single-line comment
        if (c == '/' && i + 1 < n && text[i+1] == '/') {
            push(i, n, TT::Comment);
            i = n;
            continue;
        }

        // Block comment start (also covers /** ... */ Javadoc)
        if (c == '/' && i + 1 < n && text[i+1] == '*') {
            int start = i;
            i += 2;
            while (i < n) {
                if (text[i] == '*' && i + 1 < n && text[i+1] == '/') { i += 2; break; }
                i++;
            }
            bool closed = (i >= 2 && text[i-2] == '*' && text[i-1] == '/');
            push(start, i, TT::Comment);
            if (!closed) {
                result.state = StateBlockComment;
                return result;
            }
            continue;
        }

        // Annotation (@Override, @FunctionalInterface, ...)
        if (c == '@') {
            int start = i;
            i++;
            while (i < n && isIdChar(text[i])) i++;
            push(start, i, TT::Attribute);
            continue;
        }

        // String / char literal (no escapes beyond backslash-skip, no text blocks)
        if (c == '"' || c == '\'') {
            int start = i;
            char quote = c;
            i++;
            while (i < n) {
                if (text[i] == '\\') { i += 2; continue; }
                if (text[i] == quote) { i++; break; }
                i++;
            }
            push(start, i, TT::String);
            continue;
        }

        // Number
        if (isDigit(c) || (c == '.' && i + 1 < n && isDigit(text[i+1]))) {
            int start = i;
            i = scanNumber(text, i);
            push(start, i, TT::Number);
            continue;
        }

        // Identifier / keyword / type / function
        if (isIdStart(c)) {
            int start = i;
            while (i < n && isIdChar(text[i])) i++;
            String word = text.substr(start, i - start);
            TT type = TT::Default;
            if (isKeyword(word))       type = TT::Keyword;
            else if (isType(word))     type = TT::Type;
            else if (isConstant(word)) type = TT::Constant;
            else {
                int j = i;
                while (j < n && text[j] == ' ') j++;
                if (j < n && text[j] == '(') type = TT::Function;
            }
            push(start, i, type);
            continue;
        }

        if (isBracket(c)) { push(i, i + 1, TT::Bracket); i++; continue; }
        if (isOperatorChar(c)) {
            int start = i;
            while (i < n && isOperatorChar(text[i])) i++;
            push(start, i, TT::Operator);
            continue;
        }
        i++;
    }

    result.state = StateNormal;
    return result;
}

int JavaHighlighter::foldDelta(int /*lineIndex*/, const String& text) const
{
    return braceDelta(text);
}

// ═════════════════════════════════════════════════════════════════════════════
//  BlitzHighlighter
//
//  BASIC-family dialect (BlitzBasic/BlitzMax-style): case-insensitive
//  keywords, REM/' line comments (no block comments), "..." strings with no
//  backslash escapes (a doubled quote "" is the escape), $ (hex) and % (bin)
//  numeric prefixes as well as decimal/float, and type sigils (%, #, $) that
//  may trail an identifier (Count%, X#, Name$) - those are scanned as part
//  of the identifier rather than as separate operators.
// ═════════════════════════════════════════════════════════════════════════════

namespace {

inline char lower(char c) { return asciiToLower(c); }

String toLowerCopy(const String& s)
{
    String out = s;
    for (char& c : out) c = lower(c);
    return out;
}

} // anonymous namespace

BlitzHighlighter::BlitzHighlighter()
{
    // Stored lowercase; highlightLine folds identifiers to lowercase before
    // lookup so the language reads as case-insensitive, matching BlitzBasic.
    setKeywords({
        "and", "or", "not", "xor", "mod", "shl", "shr", "sar",
        "if", "then", "else", "elseif", "endif", "end if",
        "for", "to", "step", "next", "each", "in",
        "while", "wend", "repeat", "until", "forever",
        "select", "case", "default", "end select",
        "function", "end function", "return",
        "type", "end type", "field", "new", "delete", "insert", "before", "after",
        "global", "local", "const", "dim", "redim",
        "include", "import", "module",
        "true", "false", "null",
        "goto", "gosub", "exit", "continue",
    });
    setTypes({
        "int", "float", "double", "string", "byte", "short", "long",
        "object", "array",
    });
    setConstants({
        "true", "false", "null", "pi",
    });
}

SyntaxHighlighter::HighlightResult
BlitzHighlighter::highlightLine(int /*lineIndex*/, const String& text,
                                 int /*prevState*/) const
{
    // Blitz has no multi-line comments or strings, so there is no
    // cross-line state to propagate - every line lexes independently.
    HighlightResult result;
    result.state = StateNormal;
    int n = static_cast<int>(text.size());
    int i = 0;

    auto push = [&](int start, int end, TT type) {
        if (start < end) result.spans.push_back({start, end, type});
    };

    while (i < n) {
        char c = text[i];
        if (c == ' ' || c == '\t') { i++; continue; }

        // Line comment: ; ... , REM ... or ' ...
        if (c == ';' || c == '\'') { push(i, n, TT::Comment); i = n; continue; }
        if ((c == 'r' || c == 'R') && i + 2 < n &&
            lower(text[i+1]) == 'e' && lower(text[i+2]) == 'm' &&
            (i + 3 >= n || !isIdChar(text[i+3])) &&
            (i == 0 || !isIdChar(text[i-1])))
        {
            push(i, n, TT::Comment);
            i = n;
            continue;
        }

        // String literal: "..."; "" inside is an escaped quote, not a close
        if (c == '"') {
            int start = i;
            i++;
            while (i < n) {
                if (text[i] == '"') {
                    if (i + 1 < n && text[i+1] == '"') { i += 2; continue; }
                    i++;
                    break;
                }
                i++;
            }
            push(start, i, TT::String);
            continue;
        }

        // Hex number: $FF
        if (c == '$' && i + 1 < n && isHexDigit(text[i+1])) {
            int start = i;
            i++;
            while (i < n && isHexDigit(text[i])) i++;
            push(start, i, TT::Number);
            continue;
        }

        // Binary number: %1010
        if (c == '%' && i + 1 < n && (text[i+1] == '0' || text[i+1] == '1')) {
            int start = i;
            i++;
            while (i < n && (text[i] == '0' || text[i] == '1')) i++;
            push(start, i, TT::Number);
            continue;
        }

        // Decimal / float number
        if (isDigit(c) || (c == '.' && i + 1 < n && isDigit(text[i+1]))) {
            int start = i;
            i = scanNumber(text, i);
            push(start, i, TT::Number);
            continue;
        }

        // Identifier / keyword / type, with an optional trailing type sigil
        if (isIdStart(c)) {
            int start = i;
            while (i < n && isIdChar(text[i])) i++;
            if (i < n && (text[i] == '%' || text[i] == '#' || text[i] == '$'))
                i++; // type sigil is part of the identifier
            String word = toLowerCopy(text.substr(start, i - start));
            TT type = TT::Default;
            if (isKeyword(word))       type = TT::Keyword;
            else if (isType(word))     type = TT::Type;
            else if (isConstant(word)) type = TT::Constant;
            else {
                int j = i;
                while (j < n && text[j] == ' ') j++;
                if (j < n && text[j] == '(') type = TT::Function;
            }
            push(start, i, type);
            continue;
        }

        if (isBracket(c)) { push(i, i + 1, TT::Bracket); i++; continue; }
        if (isOperatorChar(c) || c == '.') {
            int start = i;
            while (i < n && (isOperatorChar(text[i]) || text[i] == '.')) i++;
            push(start, i, TT::Operator);
            continue;
        }
        i++;
    }

    return result;
}

int BlitzHighlighter::foldDelta(int /*lineIndex*/, const String& text) const
{
    int delta = 0;
    int n = static_cast<int>(text.size());
    int i = 0;
    while (i < n) {
        if (text[i] == '\'') break; // rest of line is a comment
        if (!isIdStart(text[i])) { i++; continue; }
        int start = i;
        while (i < n && isIdChar(text[i])) i++;
        String word = toLowerCopy(text.substr(start, i - start));
        if (word == "rem" && start == 0) break;
        if (word == "function" || word == "type" || word == "if" ||
            word == "for" || word == "while" || word == "repeat" ||
            word == "select")
            delta++;
        else if (word == "end" || word == "endif" || word == "next" ||
                 word == "wend" || word == "forever")
            delta--;
    }
    return delta;
}

// ═════════════════════════════════════════════════════════════════════════════
//  DefinedHighlighter — declarative language definitions
// ═════════════════════════════════════════════════════════════════════════════

namespace {

String syntaxWord(String word, bool sensitive)
{
    if (!sensitive) for (char& c : word) c = lower(c);
    return word;
}

ct::Vector<SyntaxLanguage>& syntaxLanguages()
{
    static ct::Vector<SyntaxLanguage> languages = [] {
        ct::Vector<SyntaxLanguage> result;
        SyntaxLanguage json;
        json.name = "JSON";
        json.constants = {"true", "false", "null"}; json.quotes = "\"";
        json.lineComment.clear(); json.blockCommentStart.clear(); json.blockCommentEnd.clear();
        json.extensions = {"json"}; result.push_back(json);
        json.name = "JSONC"; json.extensions = {"jsonc"}; json.lineComment = "//";
        json.blockCommentStart = "/*"; json.blockCommentEnd = "*/"; result.push_back(json);
        SyntaxLanguage sql;
        sql.name = "SQL"; sql.extensions = {"sql"}; sql.lineComment = "--"; sql.caseSensitive = false;
        sql.keywords = {"select", "from", "where", "join", "left", "right", "inner", "outer", "on", "as", "insert", "into", "values", "update", "set", "delete", "create", "table", "drop", "alter", "group", "by", "order", "having", "limit", "distinct", "union", "and", "or", "not", "in", "is", "like", "case", "when", "then", "else", "end", "with"};
        sql.types = {"int", "integer", "text", "varchar", "boolean", "date", "timestamp", "decimal"};
        sql.constants = {"null", "true", "false"}; result.push_back(sql);
        SyntaxLanguage go;
        go.name = "Go"; go.extensions = {"go"}; go.rawQuote = '`';
        go.keywords = {"package", "import", "func", "var", "const", "type", "struct", "interface", "map", "chan", "go", "defer", "select", "case", "default", "if", "else", "for", "range", "return", "break", "continue", "switch", "fallthrough"};
        go.types = {"string", "bool", "byte", "rune", "int", "int64", "uint", "float32", "float64", "error"};
        go.constants = {"true", "false", "nil", "iota"}; result.push_back(go);
        return result;
    }();
    return languages;
}

} // anonymous namespace

DefinedHighlighter::DefinedHighlighter(const SyntaxLanguage& language) : language_(language)
{
    for (const auto& w : language.keywords) keywords_.insert(syntaxWord(w, language.caseSensitive));
    for (const auto& w : language.types) types_.insert(syntaxWord(w, language.caseSensitive));
    for (const auto& w : language.constants) constants_.insert(syntaxWord(w, language.caseSensitive));
}

SyntaxHighlighter::HighlightResult DefinedHighlighter::highlightLine(int, const String& text, int prevState) const
{
    HighlightResult result; result.state = prevState == 1 || (prevState == 2 && language_.rawQuote) ? prevState : 0;
    int i = 0, n = static_cast<int>(text.size());
    auto matches = [&](const String& token) {
        return !token.empty() && token.size() <= text.size() - static_cast<size_t>(i) &&
            text.compare(static_cast<size_t>(i), token.size(), token.c_str()) == 0;
    };
    while (i < n) {
        const int start = i;
        TokenType type = TokenType::Default;
        if (result.state == 2 || (language_.rawQuote && text[i] == language_.rawQuote)) {
            if (result.state != 2) ++i;
            result.state = 2;
            while (i < n && text[i] != language_.rawQuote) ++i;
            if (i < n) { ++i; result.state = 0; }
            type = TokenType::String;
        } else if (result.state == 1 || (!language_.blockCommentEnd.empty() && matches(language_.blockCommentStart))) {
            if (result.state != 1) i += static_cast<int>(language_.blockCommentStart.size());
            result.state = 1;
            while (i < n && !matches(language_.blockCommentEnd)) ++i;
            if (i < n) { i += static_cast<int>(language_.blockCommentEnd.size()); result.state = 0; }
            type = TokenType::Comment;
        } else if (matches(language_.lineComment)) {
            i = n; type = TokenType::Comment;
        } else if (language_.quotes.find(text[i]) != String::npos) {
            const char quote = text[i++]; type = TokenType::String;
            while (i < n) {
                if (text[i] == '\\' && i + 1 < n) { i += 2; continue; }
                if (text[i++] == quote) break;
            }
        } else if (isIdStart(text[i])) {
            ++i;
            while (i < n && isIdChar(text[i])) ++i;
            const String word = syntaxWord(text.substr(start, i - start), language_.caseSensitive);
            if (isKeyword(word)) type = TokenType::Keyword;
            else if (isType(word)) type = TokenType::Type;
            else if (isConstant(word)) type = TokenType::Constant;
            else { int next = i; while (next < n && asciiIsSpace(text[next])) ++next;
                if (next < n && text[next] == '(') type = TokenType::Function; }
        } else if (asciiIsDigit(text[i])) {
            ++i; while (i < n && (isIdChar(text[i]) || text[i] == '.')) ++i;
            type = TokenType::Number;
        } else {
            const char c = text[i++];
            if (isOneOf(c, "{}[]()")) type = TokenType::Bracket;
            else if (isOneOf(c, "+-*/%=!<>|&^~?:")) type = TokenType::Operator;
        }
        result.spans.push_back({start, i, type});
    }
    return result;
}

int DefinedHighlighter::foldDelta(int line, const String& text) const
{
    int delta = 0;
    for (const auto& span : highlightLine(line, text, 0).spans)
        if (span.type == TokenType::Bracket) {
            if (text[span.startCol] == '{' || text[span.startCol] == '[') ++delta;
            if (text[span.startCol] == '}' || text[span.startCol] == ']') --delta;
        }
    return delta;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Registry / factory
// ═════════════════════════════════════════════════════════════════════════════

void registerLanguage(const SyntaxLanguage& language)
{
    for (auto& existing : syntaxLanguages())
        if (existing.name == language.name) { existing = language; return; }
    syntaxLanguages().push_back(language);
}

SyntaxHighlighter* makeHighlighterForFile(const String& filename)
{
    auto dot = filename.rfind('.');
    if (dot == String::npos) return nullptr;
    String ext = filename.substr(dot + 1);
    for (auto& c : ext) c = lower(c);

    for (auto it = syntaxLanguages().rbegin(); it != syntaxLanguages().rend(); ++it)
    {
        const auto& language = *it;
        for (const auto& suffix : language.extensions)
            if (syntaxWord(suffix, false) == ext) return new DefinedHighlighter(language);
    }

    struct Factory {
        ct::Vector<String> exts;
        SyntaxHighlighter* (*create)();
    };
    Factory factories[] = {
        { {"c","cpp","cc","cxx","h","hpp","hxx","inl"}, []() -> SyntaxHighlighter* { return new CppHighlighter(); } },
        { {"py","pyw","pyi"}, []() -> SyntaxHighlighter* { return new PythonHighlighter(); } },
        { {"js","jsx","ts","tsx","mjs"}, []() -> SyntaxHighlighter* { return new JsHighlighter(); } },
        { {"lua"}, []() -> SyntaxHighlighter* { return new LuaHighlighter(); } },
        { {"glsl","vert","frag","geom","comp","tesc","tese"}, []() -> SyntaxHighlighter* { return new GlslHighlighter(); } },
        { {"java"}, []() -> SyntaxHighlighter* { return new JavaHighlighter(); } },
        { {"blitz","bb","bmx","decls"}, []() -> SyntaxHighlighter* { return new BlitzHighlighter(); } },
    };
    for (auto& f : factories)
        for (auto& e : f.exts)
            if (e == ext) return f.create();

    return nullptr;
}

// ═════════════════════════════════════════════════════════════════════════════
//  LineHighlightCache
// ═════════════════════════════════════════════════════════════════════════════

void LineHighlightCache::setHighlighter(SyntaxHighlighter* hl)
{
    if (highlighter_ == hl) return;
    delete highlighter_;
    highlighter_ = hl;
    lineStates_.clear();
    dirty_ = true;
}

SyntaxHighlighter* LineHighlightCache::setHighlighterForFile(const String& filename)
{
    SyntaxHighlighter* hl = makeHighlighterForFile(filename);
    setHighlighter(hl);
    return hl;
}

} // namespace syntax
} // namespace ig
