#pragma once

#include "TextInputWidgets.hpp"
#include <ct/function.hpp>
#include <ct/hashset.hpp>
#include <ct/vector.hpp>
#include <igui/Syntax.hpp>
#include <igui/widgets/String.hpp>

namespace ig
{
namespace retained
{

// The lexical highlighting layer (SyntaxHighlighter, SyntaxLanguage, and the
// built-in per-language lexers) lives in ig::syntax so it can be shared with
// the immediate-mode Context::codeEditor() without depending on the retained
// widget toolkit. Pull the names in here so existing retained code and users
// of this header keep referring to them unqualified, as ig::retained::*.
using ig::syntax::BlitzHighlighter;
using ig::syntax::CppHighlighter;
using ig::syntax::DefinedHighlighter;
using ig::syntax::GlslHighlighter;
using ig::syntax::JavaHighlighter;
using ig::syntax::JsHighlighter;
using ig::syntax::LuaHighlighter;
using ig::syntax::PythonHighlighter;
using ig::syntax::SyntaxHighlighter;
using ig::syntax::SyntaxLanguage;

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
    IG_WIDGET_CLASS(CodeEditor, TextEdit);

    CodeEditor();
    ~CodeEditor() override;

    // ── Highlighter ───────────────────────────────────────────────────────

    /// @brief Create and set a highlighter by type.
    /// @code editor->setHighlighter<CppHighlighter>(); @endcode
    template<typename T, typename... Args>
    T* setHighlighter(Args&&... args)
    {
        auto* hl = new T(std::forward<Args>(args)...);
        setHighlighterRaw(hl);
        return hl;
    }

    /// @brief Set a pre-created highlighter (CodeEditor takes ownership).
    void setHighlighterRaw(SyntaxHighlighter* hl);

    /// @brief Get the current syntax highlighter (nullptr if none).
    SyntaxHighlighter* highlighter() const
    {
        return highlighter_;
    }

    /// @brief Auto-detect and create highlighter from file extension.
    /// @param filename  File name or path (e.g. "main.cpp").
    /// @return The created highlighter, or nullptr if no match.
    SyntaxHighlighter* setHighlighterForFile(const String& filename);
    /// Install a copied declarative language definition on this editor.
    DefinedHighlighter* setLanguage(const SyntaxLanguage& language)
    {
        return setHighlighter<DefinedHighlighter>(language);
    }
    /// Register/replace a definition by name for automatic extension detection.
    /// Call on the UI thread; custom definitions take precedence over built-ins.
    static void registerLanguage(const SyntaxLanguage& language);

    // ── Breakpoints ───────────────────────────────────────────────────────

    /// @brief A gutter column for breakpoints: a click there toggles the
    ///        line's, and breakpointToggled says which (lines from 0). They
    ///        move with the text as lines come and go.
    void setShowBreakpoints(bool show)
    {
        showBreakpoints_ = show;
        markDirty();
    }
    bool showBreakpoints() const
    {
        return showBreakpoints_;
    }
    void setBreakpoint(int line, bool on);
    bool hasBreakpoint(int line) const;
    void clearBreakpoints()
    {
        breakpoints_.clear();
        markDirty();
    }
    const ct::Vector<int>& breakpoints() const
    {
        return breakpoints_;
    }
    /// @brief The line a debugger stopped at (-1 for none): an arrow in the
    ///        gutter and the line tinted.
    void setExecutionLine(int line)
    {
        executionLine_ = line;
        markDirty();
    }
    int executionLine() const
    {
        return executionLine_;
    }

    // ── Code folding ──────────────────────────────────────────────────────

    /// @brief Enable or disable fold gutter icons.
    void setShowFolding(bool show)
    {
        showFolding_ = show;
        markDirty();
    }
    /// @brief Check if fold gutter is visible.
    bool showFolding() const
    {
        return showFolding_;
    }

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
    void setAutoIndent(bool ai)
    {
        autoIndent_ = ai;
    }
    /// @brief Check if auto-indent is enabled.
    bool autoIndent() const
    {
        return autoIndent_;
    }

    // ── Bracket matching ──────────────────────────────────────────────────

    /// @brief Enable or disable bracket matching highlight.
    void setShowBracketMatch(bool s)
    {
        showBracketMatch_ = s;
        markDirty();
    }
    /// @brief Check if bracket matching is enabled.
    bool showBracketMatch() const
    {
        return showBracketMatch_;
    }

    // ── Visual ────────────────────────────────────────────────────────────

    /// @brief Enable or disable current-line highlight.
    void setHighlightCurrentLine(bool h)
    {
        highlightCurrentLine_ = h;
        markDirty();
    }
    /// @brief Check if current-line highlight is enabled.
    bool highlightCurrentLine() const
    {
        return highlightCurrentLine_;
    }

    /// @brief Enable or disable indent guide lines.
    void setShowIndentGuides(bool s)
    {
        showIndentGuides_ = s;
        markDirty();
    }
    /// @brief Check if indent guides are visible.
    bool showIndentGuides() const
    {
        return showIndentGuides_;
    }

    /// @brief Enable or disable whitespace rendering (dots for spaces, arrows for tabs).
    void setShowWhitespace(bool s)
    {
        showWhitespace_ = s;
        markDirty();
    }
    /// @brief Check if whitespace rendering is enabled.
    bool showWhitespace() const
    {
        return showWhitespace_;
    }

    /// @brief Enable or disable fold scope lines (vertical lines from { to }).
    void setShowScopeLines(bool s)
    {
        showScopeLines_ = s;
        markDirty();
    }
    /// @brief Check if fold scope lines are visible.
    bool showScopeLines() const
    {
        return showScopeLines_;
    }

    // ── Minimap ───────────────────────────────────────────────────────────

    /// @brief Enable or disable the code minimap on the right side.
    void setShowMinimap(bool s)
    {
        showMinimap_ = s;
        markDirty();
    }
    /// @brief Check if minimap is visible.
    bool showMinimap() const
    {
        return showMinimap_;
    }
    /// @brief Set minimap width in pixels (default 80).
    void setMinimapWidth(float w)
    {
        minimapWidth_ = w;
        markDirty();
    }
    /// @brief Get minimap width.
    float minimapWidth() const
    {
        return minimapWidth_;
    }

    // ── Zoom ──────────────────────────────────────────────────────────────

    /// @brief Zoom in by one step (10%).
    void zoomIn()
    {
        setFontScale(fontScale() + 0.1f);
    }
    /// @brief Zoom out by one step (10%).
    void zoomOut()
    {
        setFontScale(fontScale() - 0.1f);
    }
    /// @brief Reset zoom to 100%.
    void zoomReset()
    {
        setFontScale(1.0f);
    }

    // ── Search / Replace ──────────────────────────────────────────────────

    /// @brief Open the search bar (Ctrl+F).
    void showSearchBar();
    /// @brief Open the search & replace bar (Ctrl+H).
    void showReplaceBar();
    /// @brief Close the search/replace bar.
    void hideSearchBar();
    /// @brief Check if the search bar is visible.
    bool isSearchBarVisible() const { return bar_ == Bar::Find || bar_ == Bar::Replace; }

    /// @brief Find and select the next occurrence of the search term.
    /// @param text       The text to search for, or a pattern when useRegex is set.
    /// @param caseSensitive  Whether the search is case-sensitive.
    /// @param wholeWord  Only match whole words. Ignored when useRegex is set.
    /// @param useRegex   Treat text as a ct::Regex pattern (Python re syntax)
    ///                   instead of a literal substring. An invalid pattern
    ///                   matches nothing rather than falling back to literal.
    /// @return true if a match was found.
    bool findNext(const String& text, bool caseSensitive = false, bool wholeWord = false,
            bool useRegex = false);

    /// @brief Find the previous occurrence.
    bool findPrev(const String& text, bool caseSensitive = false, bool wholeWord = false,
            bool useRegex = false);

    /// @brief Replace the current selection (if it matches) and find next.
    bool replaceNext(const String& find, const String& replace, bool caseSensitive = false,
            bool useRegex = false);

    /// @brief Replace all occurrences. Returns the number of replacements.
    /// When useRegex is set, replace may reference capture groups as \1, \2, ...
    int replaceAll(const String& find, const String& replace, bool caseSensitive = false,
            bool useRegex = false);

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
    int cursorCount() const
    {
        return 1 + static_cast<int>(extraCursors_.size());
    }
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

    // ── Inline bars: find, replace, go to line, go to symbol ──────────────
    //
    //  One bar at a time, at the top right of the editor, with a field the
    //  keyboard types into while it is open (Escape closes it). Find (Ctrl+F)
    //  searches as you type from where the cursor was, Return and Shift+Return
    //  step through the matches, F3/Shift+F3 do the same with the bar closed,
    //  Alt+C/W/R toggle case, whole word and regex. Replace (Ctrl+H) adds a
    //  second field (Tab switches), Return replaces the match and finds the
    //  next, Ctrl+Return replaces every match as one undo step. Go to line
    //  (Ctrl+G) takes a number. Go to symbol (Ctrl+Shift+O) filters the
    //  symbols of the text as you type, Up/Down pick, Return reveals.

    enum class Bar
    {
        None,
        Find,
        Replace,
        GotoLine,
        GotoSymbol,
    };
    void showBar(Bar bar);
    void hideBar();
    Bar activeBar() const { return bar_; }
    /// @brief What is typed in the bar: the search term, the line, the filter.
    const String& barText() const { return barText_; }
    const String& barReplaceText() const { return barReplace_; }
    /// @brief The symbols the go-to-symbol bar matches, best first.
    const ct::Vector<int>& symbolMatches() const { return paletteMatches_; }
    int symbolMatchSelected() const { return paletteSelected_; }

    // ── Commands ──────────────────────────────────────────────────────────
    //  Each one undo step where it changes the text (see shortcuts()).

    /// @brief Replaces [start, end) with text as one recorded edit, the way
    /// the keyboard would: undoable, reported by textChanged, the cursor
    /// after the text. A host editing the buffer (a tool, an assistant) goes
    /// through here. Nothing happens when the text is already there.
    void replaceRange(TextPos start, TextPos end, const String& text);

    /// @brief Ctrl+L: the whole line; pressed again, the next line too.
    void selectLine();
    /// @brief Ctrl+J: the next line onto this one, one space between.
    void joinLines();
    /// @brief Ctrl+M: cursor to the bracket matching the one at the cursor
    /// (or just before it); with select, selects through it.
    bool jumpToMatchingBracket(bool select = false);
    /// @brief Ctrl+B: reveal the symbol the word at the cursor names. False if none.
    bool gotoDefinition();
    /// @brief Ctrl+Alt+Up / Ctrl+Alt+Down: another cursor on the line above / below.
    void addCursorAbove();
    void addCursorBelow();
    /// @brief Ctrl+F2 / F2 / Shift+F2: a bookmark marker on the line, the next one, the previous one.
    void toggleBookmark();
    bool nextBookmark();
    bool previousBookmark();
    /// @brief Ctrl+Shift+U / Ctrl+U: the selection in upper / lower case (ASCII).
    void transformSelection(bool upper);
    /// @brief Tab / Shift+Tab with a selection over several lines: every line one level.
    void indentSelection();
    void unindentSelection();
    /// @brief Alt+Left / Alt+Right: back to where the cursor was before a
    /// reveal, a go-to or a find; and forward again (vim's Ctrl+O / Ctrl+I).
    bool jumpBack();
    bool jumpForward();

    // ── Formatting ────────────────────────────────────────────────────────
    //  Ctrl+Shift+F: the whole text, as one undo step. Every language loses
    //  its trailing spaces, runs of blank lines shrink to one and the text
    //  ends with a newline. An indentation language (Python) is reindented by
    //  block levels: each level becomes the indent unit, however the levels
    //  were written. A brace language is reindented by the depth a line
    //  starts at, a closing brace one less; lines inside block comments and
    //  preprocessor lines are left where they are. Lua and Blitz keep their
    //  indentation.

    /// @brief Spaces per indent level the formatter writes; 0 means a tab. Default 4.
    void setIndentUnit(int spaces)
    {
        indentUnit_ = spaces < 0 ? 0 : spaces;
    }
    int indentUnit() const { return indentUnit_; }
    /// @brief The text as formatDocument would leave it.
    String formattedText() const;
    void formatDocument();

    /// @brief Other occurrences of the word at the cursor (or of a one-line
    /// selection) painted behind the text, like VS Code. On by default.
    void setHighlightOccurrences(bool on)
    {
        highlightOccurrences_ = on;
        markDirty();
    }
    bool highlightOccurrences() const { return highlightOccurrences_; }

    /// @brief The keyboard, for a host's help page: {"Ctrl+D", "..."}.
    struct Shortcut
    {
        const char* keys;
        const char* action;
    };
    static Span<const Shortcut> shortcuts();

    // ── Symbols: outline, go-to-symbol ────────────────────────────────────

    /// @brief The declarations of the text (classes, methods, functions,
    /// imports), found by the scan the highlighter names (see Symbols.hpp).
    /// Scanned again only after an edit, when next asked.
    const ct::Vector<syntax::Symbol>& symbols() const;
    /// @brief Cursor to the first symbol with that name, revealed. False if none.
    bool gotoSymbol(StringView name);
    /// @brief Cursor to (line, col), and the line scrolled into the upper
    /// third of the view: now, or on the first painted frame when the editor
    /// does not know its line height yet.
    void reveal(int line, int col = 0);
    /// @brief TextEdit::setText, and the symbols know the text changed.
    void setText(const String& text);

    // ── Completion ────────────────────────────────────────────────────────
    //
    //  A popup at the cursor with what the word being typed may be. It opens
    //  by itself after two letters, at once after a "." or Ctrl+Space, and
    //  closes on Escape or when the cursor leaves the word. Up/Down browse,
    //  Tab accepts; Return accepts a popup that was asked for or browsed, and
    //  stays a newline otherwise. The built-in items are the language's words
    //  (from the highlighter) and the words of the text; a host adds its own
    //  with a provider (an engine's API, a form's widgets).

    struct CompletionItem
    {
        String text;   // what is inserted
        String detail; // shown dimmed at the right: a type, "method", "keyword"
    };
    /// Where the cursor is when items are asked for. Columns are code points
    /// on the line, as TextPos counts them; the word ends at the cursor.
    struct CompletionContext
    {
        String receiver; // the dotted name before a ".": "self", "self.display"; empty for a bare word
        String prefix;   // the word typed so far
        int line = 0;
        int wordStart = 0;
        int wordEnd = 0;
        bool requested = false; // by a "." or Ctrl+Space, not by typing letters
    };
    using CompletionProvider = ct::Function<void(const CompletionContext&, ct::Vector<CompletionItem>&)>;

    void setAutocompleteEnabled(bool on);
    bool autocompleteEnabled() const { return autocomplete_; }
    /// @brief The host's items come first, in the order it gives them (what
    /// the prefix rules out is dropped here); the built-in ones follow unless
    /// setBuiltinCompletions(false).
    void setCompletionProvider(CompletionProvider provider);
    void setBuiltinCompletions(bool on) { builtinCompletions_ = on; }
    /// @brief Open the popup for the word at the cursor (Ctrl+Space).
    void requestCompletion();
    bool completionVisible() const { return completionVisible_; }
    const ct::Vector<CompletionItem>& completionItems() const { return completionItems_; }
    int completionSelected() const { return completionSelected_; }
    /// @brief Insert the selected item, as typed text: one undo step.
    bool acceptCompletion();
    void closeCompletion();

    // ── Signals ───────────────────────────────────────────────────────────

    /// @brief Emitted when undo/redo availability changes.
    Signal<bool, bool> undoRedoChanged; // (canUndo, canRedo)

    /// @brief Emitted when a fold region is toggled (line, isFolded).
    Signal<int, bool> foldToggled;
    Signal<int, bool> breakpointToggled; // (line, on)

    // ── Overrides ─────────────────────────────────────────────────────────
    void paint(PaintContext& ctx) override;
    void onKeyPress(KeyEvent& e) override;
    void onTextInput(KeyEvent& e) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;

private:
    // ── Highlighter ──────────────────────────────────────────────────────
    SyntaxHighlighter* highlighter_ = nullptr; // owned, deleted in dtor

    // Cached highlight state per line (invalidated on edit)
    mutable ct::Vector<int> lineStates_; // lexer state after each line
    mutable bool hlDirty_ = true;        // full re-highlight needed
    mutable ct::Vector<int> hlStarts_;   // lines an edit changed: highlight again from there
    void reHighlight() const;
    void reHighlightStale() const;

    // Bridge to TextEdit's SyntaxCallback
    void installSyntaxCallback();

    // ── Folding ──────────────────────────────────────────────────────────
    bool showFolding_ = true;
    struct FoldRange
    {
        int startLine, endLine;
        bool collapsed;
    };
    ct::Vector<FoldRange> foldRanges_;
    ct::Vector<int> foldDeltas_; // what each line opens or closes, measured once
    bool foldsStale_ = false;
    int lineFoldDelta(int line);
    void rebuildFoldRanges();
    void onLinesChanged(int line, int removed, int inserted) override;
    bool isLineHidden(int line) const;
    mutable ct::Vector<unsigned char> hiddenLines_; // per line, rebuilt when the folds change
    mutable bool hiddenDirty_ = true;
    int foldGutterWidth() const;
    void paintFoldGutter(PaintContext& ctx, const Rect& abs);
    int breakpointGutterWidth() const
    {
        return showBreakpoints_ ? 16 : 0;
    }
    void paintBreakpoints(PaintContext& ctx, const Rect& abs);
    int lineAtVisualRow(int vRow);
    bool showBreakpoints_ = false;
    ct::Vector<int> breakpoints_;
    int executionLine_ = -1;

    // ── Undo / Redo ──────────────────────────────────────────────────────
    struct EditAction
    {
        enum Type
        {
            Insert,
            Delete,
            Replace
        };
        Type type;
        TextPos pos;       // start position of the edit
        String oldText;    // text before (for undo)
        String newText;    // text after (for redo)
        TextPos oldCursor; // cursor before
        TextPos newCursor; // cursor after
        int group = 0;     // actions sharing a nonzero group undo together
    };
    int undoGroup_ = 0;  // the group being recorded, 0 for none
    int nextGroup_ = 1;
    void beginUndoGroup();
    void endUndoGroup();
    void recordReplace(TextPos pos, const String& oldText, const String& newText, TextPos oldCursor, TextPos newCursor);
    static TextPos advance(TextPos pos, const String& text);
    ct::Vector<EditAction> undoStack_;
    ct::Vector<EditAction> redoStack_;
    bool recording_ = false; // prevents recursive recording
    void recordInsert(TextPos pos, const String& text, TextPos oldCursor, TextPos newCursor);
    void recordDelete(TextPos pos, const String& text, TextPos oldCursor, TextPos newCursor);
    void applyAction(const EditAction& action, bool isUndo);
    // Drop extra cursors whose line the buffer no longer has.
    void dropStaleExtraCursors();
    void clearRedoStack();

    // ── Auto-indent ──────────────────────────────────────────────────────
    bool autoIndent_ = true;
    String getLineIndent(int line) const;

    // ── Bracket matching ─────────────────────────────────────────────────
    bool showBracketMatch_ = true;
    struct BracketPair
    {
        TextPos open, close;
    };
    BracketPair findMatchingBracket(TextPos pos) const;
    void paintBracketMatch(PaintContext& ctx, const Rect& abs);

    // ── Visual ───────────────────────────────────────────────────────────
    bool highlightCurrentLine_ = true;
    bool showIndentGuides_ = true;
    bool showWhitespace_ = false;
    bool showScopeLines_ = true;
    const ig::retained::Font* lastPaintFont_ = nullptr; // cached during paint
    float colToPixelX(int line, int col) const;
    float colCharWidth(int line, int col) const;
    void paintCurrentLineHighlight(PaintContext& ctx, const Rect& abs);
    void paintIndentGuides(PaintContext& ctx, const Rect& abs);
    void paintWhitespace(PaintContext& ctx, const Rect& abs);
    void paintScopeLines(PaintContext& ctx, const Rect& abs);

    // ── Minimap ──────────────────────────────────────────────────────────
    bool showMinimap_ = false;
    float minimapWidth_ = 80.0f;
    bool minimapDrag_ = false; // mouse dragging the minimap slider
    void paintMinimap(PaintContext& ctx, const Rect& abs);
    float minimapEffectiveWidth() const; // 0 when hidden

    // ── Search ───────────────────────────────────────────────────────────
    String searchTerm_;
    String replaceTerm_;
    bool searchCaseSensitive_ = false;
    bool searchWholeWord_ = false;
    bool searchIsRegex_ = false;
    ct::Vector<TextPos> searchMatches_;
    // Match byte length at searchMatches_[i], parallel array. Always
    // searchTerm_.size() for a literal search; varies per match for regex
    // (e.g. "a+" matches "a", "aa", ...).
    ct::Vector<int> searchMatchLengths_;
    int searchMatchIdx_ = -1;
    void rebuildSearchMatches();
    void paintSearchHighlights(PaintContext& ctx, const Rect& abs);
    // ── Bars ─────────────────────────────────────────────────────────────
    Bar bar_ = Bar::None;
    String barText_;
    String barReplace_;
    int barField_ = 0;        // Replace: 0 the find field, 1 the replace field
    TextPos barOrigin_;       // where the cursor was when the bar opened: a live find starts there
    ct::Vector<int> paletteMatches_;
    int paletteSelected_ = 0;
    int paletteFirst_ = 0;
    Rect barRect_{};
    Rect paletteRect_{};
    float paletteRowHeight_ = 0.0f;
    bool barKey(KeyEvent& e);       // the keys the open bar takes; true if it took one
    void barInput(const char* text);
    void barBackspace();
    void barChanged();              // after the field changed: live find, refilter
    void liveFind();
    void refilterPalette();
    void paintBar(PaintContext& ctx, const Rect& abs);
    // A list of rows (text, dimmed detail) at x, y; writes the rect it took
    // and its row height for hit tests.
    struct ListRow
    {
        String text;
        String detail;
    };
    void paintList(PaintContext& ctx, const Rect& abs, const ct::Vector<ListRow>& rows, int selected, int first, float x, float y, Rect& outRect, float& outRowHeight);
    // ── Occurrences, jumps ───────────────────────────────────────────────
    bool highlightOccurrences_ = true;
    int indentUnit_ = 4;
    void paintOccurrences(PaintContext& ctx, const Rect& abs);
    // The word at the cursor, or the one-line selection: what to highlight / look up.
    String wordAtCursor(TextPos& start, TextPos& end) const;
    ct::Vector<TextPos> jumps_;
    int jumpIndex_ = 0;             // jumps_[jumpIndex_ - 1] is where Alt+Left goes
    void pushJump();
    void indentLines(int first, int last, bool indent);

    // ── Multi-cursor ─────────────────────────────────────────────────────
    ct::Vector<CursorState> extraCursors_;
    void mergeCursorsIfNeeded();
    void applyToCursors(ct::Function<void(TextPos& pos, TextPos& anchor)> fn);
    void paintExtraCursors(PaintContext& ctx, const Rect& abs);
    void paintExtraSelections(PaintContext& ctx, const Rect& abs);

    // ── Word utilities (internal) ─────────────────────────────────────────
    // ── Symbols ──────────────────────────────────────────────────────────
    mutable ct::Vector<syntax::Symbol> symbols_;
    mutable bool symbolsDirty_ = true;
    int pendingRevealLine_ = -1;

    // ── Completion ───────────────────────────────────────────────────────
    bool autocomplete_ = true;
    bool builtinCompletions_ = true;
    CompletionProvider completionProvider_;
    ct::Vector<CompletionItem> completionItems_;
    CompletionContext completionContext_;
    bool completionVisible_ = false;
    bool completionNavigated_ = false; // Up/Down pressed: Return now accepts
    int completionSelected_ = 0;
    int completionFirstShown_ = 0;
    Rect completionRect_{};
    float completionRowHeight_ = 0.0f;
    // The key and text handlers as they were; the overrides wrap them with
    // the popup's keys and its refresh after typing.
    void handleKeyPress(KeyEvent& e);
    void handleTextInput(KeyEvent& e);
    bool completionContextAtCursor(CompletionContext& out) const;
    void updateCompletion(bool requested);
    void builtinCompletionItems(const CompletionContext& ctx, ct::Vector<CompletionItem>& items) const;
    void typeText(const char* text);
    void paintCompletion(PaintContext& ctx, const Rect& abs);

    bool isWordChar(char c) const;
    String commentPrefix() const;
};

} // namespace retained
} // namespace ig
