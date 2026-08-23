#pragma once

#include "TextInputWidgets.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>

namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  SyntaxHighlighter — abstract base for language-specific highlighting
//
//  Subclass this to create a highlighter for any language.
//  It scans text char-by-char using a state machine to produce ColorSpans.
//
//  Multi-line state (e.g. block comments, triple-quoted strings) is tracked
//  via an opaque int passed between lines.
//
//  Fold regions are also defined here — each language decides what opens/closes
//  a fold (braces for C++, indentation for Python, tags for HTML, etc.).
//
//  Usage:
//      class MyLangHighlighter : public SyntaxHighlighter {
//          HighlightResult highlightLine(int line, const std::string& text,
//                                        int prevState) override;
//          int foldDelta(int line, const std::string& text) override;
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
        std::vector<Span> spans;   ///< Colored spans for this line.
        int               state;   ///< Lexer state to pass to the next line.
    };

    // ── Core interface (override in subclasses) ───────────────────────────

    /// @brief Highlight a single line of text.
    /// @param lineIndex  Zero-based line number.
    /// @param text       The line text (no trailing newline).
    /// @param prevState  Lexer state from the previous line (0 = start of file).
    /// @return Colored spans and the new lexer state for the next line.
    virtual HighlightResult highlightLine(int lineIndex,
                                          const std::string& text,
                                          int prevState) const = 0;

    /// @brief Compute the fold level delta for a line.
    ///
    /// Return +1 for lines that open a fold region (e.g. `{` in C++),
    /// -1 for lines that close one (e.g. `}`), 0 otherwise.
    /// For Python, this would be based on indentation changes.
    /// @param lineIndex  Zero-based line number.
    /// @param text       The line text.
    /// @return Fold level change: +N opens N regions, -N closes N.
    virtual int foldDelta(int lineIndex, const std::string& text) const = 0;

    // ── Theme colors ──────────────────────────────────────────────────────

    /// @brief Get the color for a token type.
    Color colorFor(TokenType type) const;

    /// @brief Override the color for a specific token type.
    void setColor(TokenType type, const Color& c);

    /// @brief Get the language display name (e.g. "C++", "Python").
    virtual const char* languageName() const = 0;

    /// @brief Get file extensions this highlighter handles (e.g. {"cpp","hpp","h"}).
    virtual std::vector<std::string> extensions() const = 0;

protected:
    // ── Helpers for subclass lexers ───────────────────────────────────────

    /// @brief Check if a word is in the keyword set.
    bool isKeyword(const std::string& word) const;

    /// @brief Check if a word is in the type set.
    bool isType(const std::string& word) const;

    /// @brief Check if a word is a known constant.
    bool isConstant(const std::string& word) const;

    /// @brief Set the keyword list for this language.
    void setKeywords(std::initializer_list<const char*> words);

    /// @brief Set the built-in type list for this language.
    void setTypes(std::initializer_list<const char*> words);

    /// @brief Set the constant list for this language.
    void setConstants(std::initializer_list<const char*> words);

    std::unordered_set<std::string> keywords_;
    std::unordered_set<std::string> types_;
    std::unordered_set<std::string> constants_;

    /// @brief Default colors per token type (dark theme).
    Color colors_[14] = {
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
    HighlightResult highlightLine(int lineIndex, const std::string& text,
                                  int prevState) const override;
    int foldDelta(int lineIndex, const std::string& text) const override;
    const char* languageName() const override { return "C++"; }
    std::vector<std::string> extensions() const override
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
    HighlightResult highlightLine(int lineIndex, const std::string& text,
                                  int prevState) const override;
    int foldDelta(int lineIndex, const std::string& text) const override;
    const char* languageName() const override { return "Python"; }
    std::vector<std::string> extensions() const override
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
    HighlightResult highlightLine(int lineIndex, const std::string& text,
                                  int prevState) const override;
    int foldDelta(int lineIndex, const std::string& text) const override;
    const char* languageName() const override { return "JavaScript"; }
    std::vector<std::string> extensions() const override
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
    HighlightResult highlightLine(int lineIndex, const std::string& text,
                                  int prevState) const override;
    int foldDelta(int lineIndex, const std::string& text) const override;
    const char* languageName() const override { return "Lua"; }
    std::vector<std::string> extensions() const override
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
    HighlightResult highlightLine(int lineIndex, const std::string& text,
                                  int prevState) const override;
    int foldDelta(int lineIndex, const std::string& text) const override;
    const char* languageName() const override { return "GLSL"; }
    std::vector<std::string> extensions() const override
    { return {"glsl", "vert", "frag", "geom", "comp", "tesc", "tese"}; }
};

// ═════════════════════════════════════════════════════════════════════════════
//  CodeEditor — full-featured code editor widget
//
//  Extends TextEdit with features expected in a code editor:
//    - Syntax highlighting (via pluggable SyntaxHighlighter)
//    - Code folding (collapse/expand regions)
//    - Undo/Redo stack
//    - Auto-indent (Enter inherits previous line's indent)
//    - Bracket matching (highlights matching {}, (), [])
//    - Current line highlight
//    - Indent guides (vertical dotted lines at tab stops)
//    - Search/Replace bar (plain text and regex)
//
//  Usage:
//      auto* editor = parent->createChild<CodeEditor>();
//      editor->setHighlighter<CppHighlighter>();
//      editor->setText(sourceCode);
//      editor->setShowFolding(true);
//
//  The highlighter can be swapped at any time. Without a highlighter,
//  the editor behaves like a plain TextEdit.
// ═════════════════════════════════════════════════════════════════════════════

class CodeEditor : public TextEdit
{
public:
    CodeEditor();
    ~CodeEditor() override;

    // ── Highlighter ───────────────────────────────────────────────────────

    /// @brief Create and set a highlighter by type.
    /// @code editor->setHighlighter<CppHighlighter>(); @endcode
    template <typename T, typename... Args>
    T* setHighlighter(Args&&... args)
    {
        auto* hl = new T(std::forward<Args>(args)...);
        setHighlighterRaw(hl);
        return hl;
    }

    /// @brief Set a pre-created highlighter (CodeEditor takes ownership).
    void setHighlighterRaw(SyntaxHighlighter* hl);

    /// @brief Get the current syntax highlighter (nullptr if none).
    SyntaxHighlighter* highlighter() const { return highlighter_; }

    /// @brief Auto-detect and create highlighter from file extension.
    /// @param filename  File name or path (e.g. "main.cpp").
    /// @return The created highlighter, or nullptr if no match.
    SyntaxHighlighter* setHighlighterForFile(const std::string& filename);

    // ── Code folding ──────────────────────────────────────────────────────

    /// @brief Enable or disable fold gutter icons.
    void setShowFolding(bool show) { showFolding_ = show; markDirty(); }
    /// @brief Check if fold gutter is visible.
    bool showFolding() const       { return showFolding_; }

    /// @brief Collapse a fold region starting at the given line.
    void foldAt(int line);
    /// @brief Expand a previously folded region at the given line.
    void unfoldAt(int line);
    /// @brief Toggle fold state at the given line.
    void toggleFoldAt(int line);
    /// @brief Check if a line is the head of a collapsed fold.
    bool isFolded(int line) const;

    /// @brief Collapse all foldable regions.
    void foldAll();
    /// @brief Expand all folded regions.
    void unfoldAll();

    // ── Undo / Redo ───────────────────────────────────────────────────────

    /// @brief Undo the last edit operation.
    void undo();
    /// @brief Redo the last undone operation.
    void redo();
    /// @brief Check if undo is available.
    bool canUndo() const;
    /// @brief Check if redo is available.
    bool canRedo() const;
    /// @brief Clear the undo/redo history.
    void clearUndoHistory();

    // ── Auto-indent ───────────────────────────────────────────────────────

    /// @brief Enable or disable auto-indentation on Enter.
    void setAutoIndent(bool ai) { autoIndent_ = ai; }
    /// @brief Check if auto-indent is enabled.
    bool autoIndent() const     { return autoIndent_; }

    // ── Bracket matching ──────────────────────────────────────────────────

    /// @brief Enable or disable bracket matching highlight.
    void setShowBracketMatch(bool s) { showBracketMatch_ = s; markDirty(); }
    /// @brief Check if bracket matching is enabled.
    bool showBracketMatch() const    { return showBracketMatch_; }

    // ── Visual ────────────────────────────────────────────────────────────

    /// @brief Enable or disable current-line highlight.
    void setHighlightCurrentLine(bool h) { highlightCurrentLine_ = h; markDirty(); }
    /// @brief Check if current-line highlight is enabled.
    bool highlightCurrentLine() const    { return highlightCurrentLine_; }

    /// @brief Enable or disable indent guide lines.
    void setShowIndentGuides(bool s) { showIndentGuides_ = s; markDirty(); }
    /// @brief Check if indent guides are visible.
    bool showIndentGuides() const    { return showIndentGuides_; }

    /// @brief Enable or disable whitespace rendering (dots for spaces, arrows for tabs).
    void setShowWhitespace(bool s) { showWhitespace_ = s; markDirty(); }
    /// @brief Check if whitespace rendering is enabled.
    bool showWhitespace() const    { return showWhitespace_; }

    /// @brief Enable or disable fold scope lines (vertical lines from { to }).
    void setShowScopeLines(bool s) { showScopeLines_ = s; markDirty(); }
    /// @brief Check if fold scope lines are visible.
    bool showScopeLines() const    { return showScopeLines_; }

    // ── Minimap ───────────────────────────────────────────────────────────

    /// @brief Enable or disable the code minimap on the right side.
    void setShowMinimap(bool s) { showMinimap_ = s; markDirty(); }
    /// @brief Check if minimap is visible.
    bool showMinimap() const    { return showMinimap_; }
    /// @brief Set minimap width in pixels (default 80).
    void setMinimapWidth(float w) { minimapWidth_ = w; markDirty(); }
    /// @brief Get minimap width.
    float minimapWidth() const  { return minimapWidth_; }

    // ── Zoom ──────────────────────────────────────────────────────────────

    /// @brief Zoom in by one step (10%).
    void zoomIn()    { setFontScale(fontScale() + 0.1f); }
    /// @brief Zoom out by one step (10%).
    void zoomOut()   { setFontScale(fontScale() - 0.1f); }
    /// @brief Reset zoom to 100%.
    void zoomReset() { setFontScale(1.0f); }

    // ── Search / Replace ──────────────────────────────────────────────────

    /// @brief Open the search bar (Ctrl+F).
    void showSearchBar();
    /// @brief Open the search & replace bar (Ctrl+H).
    void showReplaceBar();
    /// @brief Close the search/replace bar.
    void hideSearchBar();
    /// @brief Check if the search bar is visible.
    bool isSearchBarVisible() const { return searchVisible_; }

    /// @brief Find and select the next occurrence of the search term.
    /// @param text       The text to search for.
    /// @param caseSensitive  Whether the search is case-sensitive.
    /// @param wholeWord  Only match whole words.
    /// @return true if a match was found.
    bool findNext(const std::string& text, bool caseSensitive = false,
                  bool wholeWord = false);

    /// @brief Find the previous occurrence.
    bool findPrev(const std::string& text, bool caseSensitive = false,
                  bool wholeWord = false);

    /// @brief Replace the current selection (if it matches) and find next.
    bool replaceNext(const std::string& find, const std::string& replace,
                     bool caseSensitive = false);

    /// @brief Replace all occurrences. Returns the number of replacements.
    int replaceAll(const std::string& find, const std::string& replace,
                   bool caseSensitive = false);

    // ── Movement ──────────────────────────────────────────────────────────

    /// @brief Move cursor(s) up by the given amount of lines.
    void moveUp(int amount = 1, bool select = false);
    /// @brief Move cursor(s) down by the given amount of lines.
    void moveDown(int amount = 1, bool select = false);
    /// @brief Move cursor(s) left by one character (or one word if wordMode).
    void moveLeft(bool select = false, bool wordMode = false);
    /// @brief Move cursor(s) right by one character (or one word if wordMode).
    void moveRight(bool select = false, bool wordMode = false);
    /// @brief Move cursor(s) to the beginning of the line.
    void moveHome(bool select = false);
    /// @brief Move cursor(s) to the end of the line.
    void moveEnd(bool select = false);
    /// @brief Move cursor(s) to the beginning of the document.
    void moveTop(bool select = false);
    /// @brief Move cursor(s) to the end of the document.
    void moveBottom(bool select = false);

    // ── Line operations ───────────────────────────────────────────────────

    /// @brief Move the current line(s) up by one.
    void moveLineUp();
    /// @brief Move the current line(s) down by one.
    void moveLineDown();
    /// @brief Remove the current line(s).
    void removeLine();
    /// @brief Duplicate the current line(s) below.
    void duplicateLine();
    /// @brief Increase indentation of current line(s).
    void indent();
    /// @brief Decrease indentation of current line(s).
    void unindent();
    /// @brief Toggle line comment on current line(s).
    void toggleComment();

    // ── Multi-cursor ──────────────────────────────────────────────────────

    struct CursorState
    {
        TextPos pos;
        TextPos anchor;
    };

    /// @brief Add an extra cursor at the given position.
    void addCursor(int line, int col);
    /// @brief Add a cursor at the next occurrence of the current selection (Ctrl+D).
    void addCursorForNextOccurrence();
    /// @brief Select all occurrences of the current selection, each with its own cursor.
    void selectAllOccurrences();
    /// @brief Remove all extra cursors, keeping only the primary.
    void clearExtraCursors();
    /// @brief Get the total number of cursors (primary + extra).
    int  cursorCount() const { return 1 + static_cast<int>(extraCursors_.size()); }
    /// @brief Get the position of cursor at index (0 = primary).
    TextPos cursorPosAt(int idx) const;
    /// @brief Check if any cursor has a selection.
    bool anyCursorHasSelection() const;
    /// @brief Check if all cursors have a selection.
    bool allCursorsHaveSelection() const;

    // ── Word utilities ────────────────────────────────────────────────────

    /// @brief Find the start of the word at the given position.
    TextPos findWordStart(TextPos pos) const;
    /// @brief Find the end of the word at the given position.
    TextPos findWordEnd(TextPos pos) const;

    // ── Signals ───────────────────────────────────────────────────────────

    /// @brief Emitted when undo/redo availability changes.
    Signal<bool, bool> undoRedoChanged;   // (canUndo, canRedo)

    /// @brief Emitted when a fold region is toggled (line, isFolded).
    Signal<int, bool>  foldToggled;

    // ── Overrides ─────────────────────────────────────────────────────────
    void paint(PaintContext& ctx) override;
    void onKeyPress(KeyEvent& e) override;
    void onTextInput(KeyEvent& e) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;

private:
    // ── Highlighter ──────────────────────────────────────────────────────
    SyntaxHighlighter* highlighter_ = nullptr;   // owned, deleted in dtor

    // Cached highlight state per line (invalidated on edit)
    mutable std::vector<int> lineStates_;     // lexer state after each line
    mutable bool             hlDirty_ = true; // full re-highlight needed
    void reHighlight() const;
    void reHighlightFrom(int line) const;

    // Bridge to TextEdit's SyntaxCallback
    void installSyntaxCallback();

    // ── Folding ──────────────────────────────────────────────────────────
    bool showFolding_ = true;
    struct FoldRange { int startLine, endLine; bool collapsed; };
    std::vector<FoldRange> foldRanges_;
    void rebuildFoldRanges();
    bool isLineHidden(int line) const;
    int  foldGutterWidth() const;
    void paintFoldGutter(PaintContext& ctx, const Rect& abs);

    // ── Undo / Redo ──────────────────────────────────────────────────────
    struct EditAction {
        enum Type { Insert, Delete, Replace };
        Type type;
        TextPos pos;               // start position of the edit
        std::string oldText;       // text before (for undo)
        std::string newText;       // text after (for redo)
        TextPos oldCursor;         // cursor before
        TextPos newCursor;         // cursor after
    };
    std::vector<EditAction> undoStack_;
    std::vector<EditAction> redoStack_;
    bool   recording_ = false;     // prevents recursive recording
    void recordInsert(TextPos pos, const std::string& text,
                      TextPos oldCursor, TextPos newCursor);
    void recordDelete(TextPos pos, const std::string& text,
                      TextPos oldCursor, TextPos newCursor);
    void applyAction(const EditAction& action, bool isUndo);
    void clearRedoStack();

    // ── Auto-indent ──────────────────────────────────────────────────────
    bool autoIndent_ = true;
    std::string getLineIndent(int line) const;
    bool lineEndsWith(int line, char c) const;

    // ── Bracket matching ─────────────────────────────────────────────────
    bool showBracketMatch_ = true;
    struct BracketPair { TextPos open, close; };
    BracketPair findMatchingBracket(TextPos pos) const;
    void paintBracketMatch(PaintContext& ctx, const Rect& abs);

    // ── Visual ───────────────────────────────────────────────────────────
    bool highlightCurrentLine_ = true;
    bool showIndentGuides_     = true;
    bool showWhitespace_       = false;
    bool showScopeLines_       = true;
    const BuGUI::Font* lastPaintFont_ = nullptr;  // cached during paint
    float colToPixelX(int line, int col) const;
    float colCharWidth(int line, int col) const;
    void paintCurrentLineHighlight(PaintContext& ctx, const Rect& abs);
    void paintIndentGuides(PaintContext& ctx, const Rect& abs);
    void paintWhitespace(PaintContext& ctx, const Rect& abs);
    void paintScopeLines(PaintContext& ctx, const Rect& abs);

    // ── Minimap ──────────────────────────────────────────────────────────
    bool  showMinimap_    = false;
    float minimapWidth_   = 80.0f;
    bool  minimapDrag_    = false;   // mouse dragging the minimap slider
    void  paintMinimap(PaintContext& ctx, const Rect& abs);
    float minimapEffectiveWidth() const;  // 0 when hidden

    // ── Search ───────────────────────────────────────────────────────────
    bool        searchVisible_ = false;
    bool        replaceVisible_ = false;
    std::string searchTerm_;
    std::string replaceTerm_;
    bool        searchCaseSensitive_ = false;
    bool        searchWholeWord_     = false;
    std::vector<TextPos> searchMatches_;
    int                  searchMatchIdx_ = -1;
    void rebuildSearchMatches();
    void paintSearchHighlights(PaintContext& ctx, const Rect& abs);
    void paintSearchBar(PaintContext& ctx, const Rect& abs);

    // ── Multi-cursor ─────────────────────────────────────────────────────
    std::vector<CursorState> extraCursors_;
    void mergeCursorsIfNeeded();
    void applyToCursors(std::function<void(TextPos& pos, TextPos& anchor)> fn);
    void paintExtraCursors(PaintContext& ctx, const Rect& abs);
    void paintExtraSelections(PaintContext& ctx, const Rect& abs);

    // ── Word utilities (internal) ─────────────────────────────────────────
    bool isWordChar(char c) const;
    std::string commentPrefix() const;
};

} // namespace BuGUI
