#pragma once

#include "TextInputWidgets.hpp"
#include <igui/widgets/String.hpp>
#include <igui/Syntax.hpp>
#include <ct/vector.hpp>
#include <ct/hashset.hpp>
#include <ct/function.hpp>

namespace ig { namespace retained
{

// The lexical highlighting layer (SyntaxHighlighter, SyntaxLanguage, and the
// built-in per-language lexers) lives in ig::syntax so it can be shared with
// the immediate-mode Context::codeEditor() without depending on the retained
// widget toolkit. Pull the names in here so existing retained code and users
// of this header keep referring to them unqualified, as ig::retained::*.
using ig::syntax::SyntaxHighlighter;
using ig::syntax::SyntaxLanguage;
using ig::syntax::DefinedHighlighter;
using ig::syntax::CppHighlighter;
using ig::syntax::PythonHighlighter;
using ig::syntax::JsHighlighter;
using ig::syntax::LuaHighlighter;
using ig::syntax::GlslHighlighter;
using ig::syntax::JavaHighlighter;
using ig::syntax::BlitzHighlighter;

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
    SyntaxHighlighter* setHighlighterForFile(const String& filename);
    /// Install a copied declarative language definition on this editor.
    DefinedHighlighter* setLanguage(const SyntaxLanguage& language)
    { return setHighlighter<DefinedHighlighter>(language); }
    /// Register/replace a definition by name for automatic extension detection.
    /// Call on the UI thread; custom definitions take precedence over built-ins.
    static void registerLanguage(const SyntaxLanguage& language);

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
    /// @param text       The text to search for, or a pattern when useRegex is set.
    /// @param caseSensitive  Whether the search is case-sensitive.
    /// @param wholeWord  Only match whole words. Ignored when useRegex is set.
    /// @param useRegex   Treat text as a ct::Regex pattern (Python re syntax)
    ///                   instead of a literal substring. An invalid pattern
    ///                   matches nothing rather than falling back to literal.
    /// @return true if a match was found.
    bool findNext(const String& text, bool caseSensitive = false,
                  bool wholeWord = false, bool useRegex = false);

    /// @brief Find the previous occurrence.
    bool findPrev(const String& text, bool caseSensitive = false,
                  bool wholeWord = false, bool useRegex = false);

    /// @brief Replace the current selection (if it matches) and find next.
    bool replaceNext(const String& find, const String& replace,
                     bool caseSensitive = false, bool useRegex = false);

    /// @brief Replace all occurrences. Returns the number of replacements.
    /// When useRegex is set, replace may reference capture groups as \1, \2, ...
    int replaceAll(const String& find, const String& replace,
                   bool caseSensitive = false, bool useRegex = false);

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
    mutable ct::Vector<int> lineStates_;     // lexer state after each line
    mutable bool             hlDirty_ = true; // full re-highlight needed
    void reHighlight() const;
    void reHighlightFrom(int line) const;

    // Bridge to TextEdit's SyntaxCallback
    void installSyntaxCallback();

    // ── Folding ──────────────────────────────────────────────────────────
    bool showFolding_ = true;
    struct FoldRange { int startLine, endLine; bool collapsed; };
    ct::Vector<FoldRange> foldRanges_;
    void rebuildFoldRanges();
    bool isLineHidden(int line) const;
    int  foldGutterWidth() const;
    void paintFoldGutter(PaintContext& ctx, const Rect& abs);

    // ── Undo / Redo ──────────────────────────────────────────────────────
    struct EditAction {
        enum Type { Insert, Delete, Replace };
        Type type;
        TextPos pos;               // start position of the edit
        String oldText;       // text before (for undo)
        String newText;       // text after (for redo)
        TextPos oldCursor;         // cursor before
        TextPos newCursor;         // cursor after
    };
    ct::Vector<EditAction> undoStack_;
    ct::Vector<EditAction> redoStack_;
    bool   recording_ = false;     // prevents recursive recording
    void recordInsert(TextPos pos, const String& text,
                      TextPos oldCursor, TextPos newCursor);
    void recordDelete(TextPos pos, const String& text,
                      TextPos oldCursor, TextPos newCursor);
    void applyAction(const EditAction& action, bool isUndo);
    void clearRedoStack();

    // ── Auto-indent ──────────────────────────────────────────────────────
    bool autoIndent_ = true;
    String getLineIndent(int line) const;
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
    const ig::retained::Font* lastPaintFont_ = nullptr;  // cached during paint
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
    String searchTerm_;
    String replaceTerm_;
    bool        searchCaseSensitive_ = false;
    bool        searchWholeWord_     = false;
    bool        searchIsRegex_       = false;
    ct::Vector<TextPos> searchMatches_;
    // Match byte length at searchMatches_[i], parallel array. Always
    // searchTerm_.size() for a literal search; varies per match for regex
    // (e.g. "a+" matches "a", "aa", ...).
    ct::Vector<int> searchMatchLengths_;
    int                  searchMatchIdx_ = -1;
    void rebuildSearchMatches();
    void paintSearchHighlights(PaintContext& ctx, const Rect& abs);
    void paintSearchBar(PaintContext& ctx, const Rect& abs);

    // ── Multi-cursor ─────────────────────────────────────────────────────
    ct::Vector<CursorState> extraCursors_;
    void mergeCursorsIfNeeded();
    void applyToCursors(ct::Function<void(TextPos& pos, TextPos& anchor)> fn);
    void paintExtraCursors(PaintContext& ctx, const Rect& abs);
    void paintExtraSelections(PaintContext& ctx, const Rect& abs);

    // ── Word utilities (internal) ─────────────────────────────────────────
    bool isWordChar(char c) const;
    String commentPrefix() const;
};

} // namespace retained
} // namespace ig
