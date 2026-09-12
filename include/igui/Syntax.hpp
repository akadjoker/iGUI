#pragma once

// Shared lexical syntax-highlighting layer, usable from both the retained
// widget toolkit (ig::retained::CodeEditor) and the immediate-mode Context
// (ig::Context::codeEditor). It depends only on ct:: containers and ig::Color
// - nothing from ig::retained - so either mode can link it standalone.

#include <ct/vector.hpp>
#include <ct/hashset.hpp>
#include <ct/string.hpp>
#include <ct/function.hpp>

#include "Color.hpp"

namespace ig { namespace syntax
{

using String = ct::String;

// ═════════════════════════════════════════════════════════════════════════════
//  SyntaxHighlighter — abstract base for language-specific highlighting
//
//  Subclass this to create a highlighter for any language.
//  It scans text char-by-char using a state machine to produce Spans.
//
//  Multi-line state (e.g. block comments, triple-quoted strings) is tracked
//  via an opaque int passed between lines.
//
//  Fold regions are also defined here — each language decides what opens/closes
//  a fold (braces for C++, indentation for Python, tags for HTML, etc.).
//
//  Usage:
//      class MyLangHighlighter : public SyntaxHighlighter {
//          HighlightResult highlightLine(int line, const String& text,
//                                        int prevState) override;
//          int foldDelta(int line, const String& text) override;
//      };
// ═════════════════════════════════════════════════════════════════════════════

class SyntaxHighlighter
{
public:
    virtual ~SyntaxHighlighter() = default;

    // ── Token types ───────────────────────────────────────────────────────
    /// @brief Semantic token categories used by the lexer.
    enum class TokenType
    {
        Default,        ///< Plain text / identifiers.
        Keyword,        ///< Language keywords (if, for, class, ...).
        Type,           ///< Built-in types (int, float, string, ...).
        String,         ///< String literals ("..." or '...').
        Number,         ///< Numeric literals (42, 3.14, 0xFF).
        Comment,        ///< Single-line and multi-line comments.
        Preprocessor,   ///< Preprocessor directives (#include, #define).
        Operator,       ///< Operators (+, -, *, ==, ...).
        Function,       ///< Function/method names at call sites.
        Constant,       ///< Named constants (true, false, nullptr, None).
        Attribute,      ///< Decorators/attributes (@decorator, [[attr]]).
        Bracket,        ///< Brackets and braces ({, }, (, ), [, ]).
        MatchedBracket, ///< Highlighted matching bracket pair.
        Error,          ///< Lexer error (unclosed string, etc.).
    };

    static constexpr int TokenTypeCount = 14;

    /// @brief A colored span within a single line of text.
    struct Span
    {
        int       startCol;   ///< Start column (byte offset in line).
        int       endCol;     ///< End column (exclusive).
        TokenType type;       ///< Token category.
    };

    /// @brief Result of highlighting a single line.
    struct HighlightResult
    {
        ct::Vector<Span> spans;   ///< Colored spans for this line.
        int               state;   ///< Lexer state to pass to the next line.
    };

    // ── Core interface (override in subclasses) ───────────────────────────

    /// @brief Highlight a single line of text.
    /// @param lineIndex  Zero-based line number.
    /// @param text       The line text (no trailing newline).
    /// @param prevState  Lexer state from the previous line (0 = start of file).
    /// @return Colored spans and the new lexer state for the next line.
    virtual HighlightResult highlightLine(int lineIndex,
                                          const String& text,
                                          int prevState) const = 0;

    /// @brief Compute the fold level delta for a line.
    ///
    /// Return +1 for lines that open a fold region (e.g. `{` in C++),
    /// -1 for lines that close one (e.g. `}`), 0 otherwise.
    /// For Python, this would be based on indentation changes.
    /// @param lineIndex  Zero-based line number.
    /// @param text       The line text.
    /// @return Fold level change: +N opens N regions, -N closes N.
    virtual int foldDelta(int lineIndex, const String& text) const = 0;

    // ── Theme colors ──────────────────────────────────────────────────────

    /// @brief Get the color for a token type.
    Color colorFor(TokenType type) const;

    /// @brief Override the color for a specific token type.
    void setColor(TokenType type, const Color& c);

    /// @brief Get the language display name (e.g. "C++", "Python").
    virtual const char* languageName() const = 0;

    /// @brief Get file extensions this highlighter handles (e.g. {"cpp","hpp","h"}).
    virtual ct::Vector<String> extensions() const = 0;

    /// @brief Keywords known to this highlighter (used for autocomplete).
    const ct::HashSet<String>& keywordSet() const { return keywords_; }
    /// @brief Built-in types known to this highlighter (used for autocomplete).
    const ct::HashSet<String>& typeSet() const { return types_; }
    /// @brief Named constants known to this highlighter (used for autocomplete).
    const ct::HashSet<String>& constantSet() const { return constants_; }

protected:
    // ── Helpers for subclass lexers ───────────────────────────────────────

    /// @brief Check if a word is in the keyword set.
    bool isKeyword(const String& word) const;

    /// @brief Check if a word is in the type set.
    bool isType(const String& word) const;

    /// @brief Check if a word is a known constant.
    bool isConstant(const String& word) const;

    /// @brief Set the keyword list for this language.
    void setKeywords(const ct::Vector<const char*>& words);

    /// @brief Set the built-in type list for this language.
    void setTypes(const ct::Vector<const char*>& words);

    /// @brief Set the constant list for this language.
    void setConstants(const ct::Vector<const char*>& words);

    ct::HashSet<String> keywords_;
    ct::HashSet<String> types_;
    ct::HashSet<String> constants_;

    /// @brief Default colors per token type (dark theme).
    Color colors_[TokenTypeCount] = {
        {220, 220, 220, 255},   // Default      — light gray
        {198, 120, 221, 255},   // Keyword       — purple
        { 86, 182, 194, 255},   // Type          — cyan
        {152, 195, 121, 255},   // String        — green
        {209, 154, 102, 255},   // Number        — orange
        {106, 115, 125, 255},   // Comment       — dim gray
        {224, 108,  97, 255},   // Preprocessor  — red
        {220, 220, 170, 255},   // Operator      — yellow-ish
        { 97, 175, 239, 255},   // Function      — blue
        {209, 154, 102, 255},   // Constant      — orange (like numbers)
        {152, 195, 121, 255},   // Attribute     — green (like strings)
        {220, 220, 220, 255},   // Bracket       — same as default
        {255, 210,  80, 255},   // MatchedBracket — bright yellow
        {244,  71,  71, 255},   // Error         — bright red
    };
};

// ═════════════════════════════════════════════════════════════════════════════
//  Declarative language definition (no subclassing needed)
// ═════════════════════════════════════════════════════════════════════════════

// Declarative lexer for languages with identifiers, quoted strings and comments.
// Definitions are copied; callers may release or modify the original afterwards.
struct SyntaxLanguage
{
    String name;
    ct::Vector<String> extensions, keywords, types, constants;
    String lineComment = "//";
    String blockCommentStart = "/*", blockCommentEnd = "*/";
    String quotes = "\"'";
    // Optional single-byte delimiter for raw, multiline strings (e.g. Go `).
    char rawQuote = '\0';
    bool caseSensitive = true;
};

class DefinedHighlighter : public SyntaxHighlighter
{
public:
    explicit DefinedHighlighter(const SyntaxLanguage& language);
    HighlightResult highlightLine(int, const String&, int prevState) const override;
    int foldDelta(int, const String&) const override;
    const char* languageName() const override { return language_.name.c_str(); }
    ct::Vector<String> extensions() const override { return language_.extensions; }
private:
    SyntaxLanguage language_;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Built-in highlighters
// ═════════════════════════════════════════════════════════════════════════════

/// @brief C/C++ syntax highlighter.
///
/// Handles: keywords, types, preprocessor directives, single/multi-line
/// comments, string/char literals, numbers (hex/oct/bin/float), operators.
/// Fold regions: `{` opens, `}` closes.
class CppHighlighter : public SyntaxHighlighter
{
public:
    CppHighlighter();
    HighlightResult highlightLine(int lineIndex, const String& text,
                                  int prevState) const override;
    int foldDelta(int lineIndex, const String& text) const override;
    const char* languageName() const override { return "C++"; }
    ct::Vector<String> extensions() const override
    { return {"c", "cpp", "cc", "cxx", "h", "hpp", "hxx", "inl"}; }
};

/// @brief Python syntax highlighter.
///
/// Handles: keywords, built-in types/functions, decorators, single/multi-line
/// strings (triple quotes), comments (#), f-strings, numbers.
/// Fold regions: indentation-based (+1 when indent increases, -1 when decreases).
class PythonHighlighter : public SyntaxHighlighter
{
public:
    PythonHighlighter();
    HighlightResult highlightLine(int lineIndex, const String& text,
                                  int prevState) const override;
    int foldDelta(int lineIndex, const String& text) const override;
    const char* languageName() const override { return "Python"; }
    ct::Vector<String> extensions() const override
    { return {"py", "pyw", "pyi"}; }
};

/// @brief JavaScript/TypeScript syntax highlighter.
///
/// Handles: keywords, template literals (`${}`), single/multi-line comments,
/// string literals, regex literals, numbers, arrow functions.
/// Fold regions: `{` opens, `}` closes.
class JsHighlighter : public SyntaxHighlighter
{
public:
    JsHighlighter();
    HighlightResult highlightLine(int lineIndex, const String& text,
                                  int prevState) const override;
    int foldDelta(int lineIndex, const String& text) const override;
    const char* languageName() const override { return "JavaScript"; }
    ct::Vector<String> extensions() const override
    { return {"js", "jsx", "ts", "tsx", "mjs"}; }
};

/// @brief Lua syntax highlighter.
///
/// Handles: keywords, multi-line strings/comments ([[...]]), numbers.
/// Fold regions: function/do/then/repeat open, end/until close.
class LuaHighlighter : public SyntaxHighlighter
{
public:
    LuaHighlighter();
    HighlightResult highlightLine(int lineIndex, const String& text,
                                  int prevState) const override;
    int foldDelta(int lineIndex, const String& text) const override;
    const char* languageName() const override { return "Lua"; }
    ct::Vector<String> extensions() const override
    { return {"lua"}; }
};

/// @brief GLSL shader syntax highlighter.
///
/// Handles: GLSL keywords, types (vec2/mat4/sampler2D), preprocessor,
/// single/multi-line comments, numbers, swizzle operators.
/// Fold regions: `{` opens, `}` closes.
class GlslHighlighter : public SyntaxHighlighter
{
public:
    GlslHighlighter();
    HighlightResult highlightLine(int lineIndex, const String& text,
                                  int prevState) const override;
    int foldDelta(int lineIndex, const String& text) const override;
    const char* languageName() const override { return "GLSL"; }
    ct::Vector<String> extensions() const override
    { return {"glsl", "vert", "frag", "geom", "comp", "tesc", "tese"}; }
};

/// @brief Java syntax highlighter.
///
/// Handles: keywords, built-in types, annotations (@Override), single/multi-line
/// comments, string/char literals (no triple/raw strings, no templates), numbers.
/// Fold regions: `{` opens, `}` closes.
class JavaHighlighter : public SyntaxHighlighter
{
public:
    JavaHighlighter();
    HighlightResult highlightLine(int lineIndex, const String& text,
                                  int prevState) const override;
    int foldDelta(int lineIndex, const String& text) const override;
    const char* languageName() const override { return "Java"; }
    ct::Vector<String> extensions() const override
    { return {"java"}; }
};

/// @brief Blitz(Max/3D) BASIC-dialect syntax highlighter.
///
/// Handles: keywords, built-in types (suffix-based: i%, f#, s$ sigils are
/// scanned as part of the identifier), REM/' line comments, single/double
/// quoted strings (no escapes, BASIC strings double the quote to escape),
/// numbers (hex with $, binary with %). Case-insensitive, like the language.
/// Fold regions: block openers (Function/Type/If/For/While/Repeat/Select)
/// open, matching End*/Next/Wend/Forever close.
class BlitzHighlighter : public SyntaxHighlighter
{
public:
    BlitzHighlighter();
    HighlightResult highlightLine(int lineIndex, const String& text,
                                  int prevState) const override;
    int foldDelta(int lineIndex, const String& text) const override;
    const char* languageName() const override { return "Blitz"; }
    ct::Vector<String> extensions() const override
    { return {"blitz", "bb", "bmx", "decls"}; }
};

// ═════════════════════════════════════════════════════════════════════════════
//  LineHighlightCache — per-line incremental highlight cache
//
//  Shared bookkeeping used by any line-oriented editor (retained CodeEditor
//  and the immediate-mode codeEditor()). Wraps a SyntaxHighlighter and caches
//  the opaque lexer state after each line, so re-highlighting after an edit
//  only walks forward from the changed line and stops as soon as the lexer
//  state converges with what was already cached (see reHighlightFrom).
// ═════════════════════════════════════════════════════════════════════════════

class LineHighlightCache
{
public:
    // Takes ownership of a highlighter created for a given language/file
    // (see makeHighlighterForExtension). Pass nullptr to disable highlighting.
    void setHighlighter(SyntaxHighlighter* hl);
    SyntaxHighlighter* highlighter() const { return highlighter_; }

    // Auto-detect and install a highlighter from a file name's extension.
    // Returns the created highlighter, or nullptr if no match (and disables
    // highlighting). Ownership stays with the cache.
    SyntaxHighlighter* setHighlighterForFile(const String& filename);

    // Recompute every line from scratch (call after loading a new document).
    template <typename LineAccessor>
    void rehighlightAll(int lineCount, LineAccessor&& lineAt)
    {
        lineStates_.clear();
        if (!highlighter_) { dirty_ = false; return; }
        lineStates_.resize(lineCount, 0);
        int state = 0;
        for (int i = 0; i < lineCount; ++i)
        {
            auto result = highlighter_->highlightLine(i, lineAt(i), state);
            lineStates_[i] = result.state;
            state = result.state;
        }
        dirty_ = false;
    }

    // Recompute starting at `line`, reusing the cached state for lines before
    // it, and stopping early once the lexer state stops changing relative to
    // what was cached (multi-line constructs like block comments propagate
    // correctly; a plain edit on one line only re-highlights that line).
    template <typename LineAccessor>
    void rehighlightFrom(int line, int lineCount, LineAccessor&& lineAt)
    {
        if (!highlighter_) return;
        if (line < 0) line = 0;
        lineStates_.resize(lineCount, 0);
        int state = (line > 0 && line - 1 < static_cast<int>(lineStates_.size())) ? lineStates_[line - 1] : 0;
        for (int i = line; i < lineCount; ++i)
        {
            auto result = highlighter_->highlightLine(i, lineAt(i), state);
            int oldState = lineStates_[i];
            lineStates_[i] = result.state;
            state = result.state;
            if (i > line && oldState == result.state) break;
        }
    }

    // Spans for a single line, using the cached previous-line state. Does not
    // update the cache - call rehighlightFrom/rehighlightAll after edits.
    SyntaxHighlighter::HighlightResult highlightLine(int line, const String& text) const
    {
        if (!highlighter_) return SyntaxHighlighter::HighlightResult();
        int prevState = (line > 0 && line - 1 < static_cast<int>(lineStates_.size()))
            ? lineStates_[line - 1] : 0;
        return highlighter_->highlightLine(line, text, prevState);
    }

    void markDirty() { dirty_ = true; }
    bool dirty() const { return dirty_; }
    void clear() { lineStates_.clear(); dirty_ = true; }

private:
    SyntaxHighlighter* highlighter_ = nullptr; // owned
    ct::Vector<int> lineStates_;
    bool dirty_ = true;

public:
    ~LineHighlightCache() { delete highlighter_; }
};

// Registry of declarative languages (JSON, SQL, Go, plus anything the app
// registers via registerLanguage) used by setHighlighterForFile/makeHighlighter.
// Custom definitions take precedence over built-ins with the same extension.
void registerLanguage(const SyntaxLanguage& language);

// Create the best highlighter for a file name's extension: first checks the
// declarative registry (registerLanguage), then the built-in dedicated
// lexers (C++, Python, JS/TS, Lua, GLSL, Java, Blitz). Returns nullptr (and
// no allocation) when the extension is not recognized. Caller owns the result.
SyntaxHighlighter* makeHighlighterForFile(const String& filename);

} // namespace syntax
} // namespace ig
