#pragma once

// Immediate-mode code editor: Context::codeEditor(). Builds on the shared
// lexical layer in Syntax.hpp (ig::syntax) to add per-line incremental
// syntax highlighting, a real (line, column) cursor with selection, full
// undo/redo, and a keyword/identifier autocomplete popup on top of a plain
// line buffer - things ig::Context::inputTextMultiline() does not attempt.
//
// State lives in an application-owned CodeEditorState, following the same
// pattern as FileDialogState: the app keeps it alongside the source text and
// passes it back in every frame.

#include <ct/vector.hpp>

#include "Syntax.hpp"
#include "Types.hpp"

namespace ig
{

// A single edit for the undo/redo stack. Insert and Delete both store the
// affected text so either direction can be replayed; Replace (used when
// autocomplete substitutes a partial identifier) stores both.
struct CodeEditorEdit
{
    enum Kind : uint8_t { Insert, Delete, Replace };

    Kind kind = Insert;
    int line = 0;               // start line of the edit
    int column = 0;              // start column (byte offset) of the edit
    String removedText;          // text that was there before (Delete/Replace)
    String insertedText;         // text that is there after (Insert/Replace)
    int cursorLineBefore = 0, cursorColumnBefore = 0;
    int cursorLineAfter = 0, cursorColumnAfter = 0;
    // Coalescing key: consecutive plain single-character insertions (regular
    // typing) merge into one undo step instead of one step per keystroke.
    // Any other edit, or a gap in typing, starts a new group.
    uint64_t group = 0;
};

struct CodeEditorOptions
{
    // File name used only to auto-pick a highlighter (by extension) the
    // first time this state is drawn; ignored afterwards. Leave empty and
    // call CodeEditorState::setLanguage/setHighlighterForFile explicitly to
    // control the language yourself.
    StringView autoDetectFileName;
    bool showLineNumbers = true;
    bool autoIndent = true;
    // Keyword/identifier popup triggered after 2+ identifier characters.
    bool autocomplete = true;
    // Fold gutter (chevrons for regions the highlighter's foldDelta opens/
    // closes, e.g. matching braces). Requires a highlighter to be set;
    // otherwise nothing is foldable and the gutter draws nothing.
    bool showFolding = true;
    // VSCode-style whitespace rendering: a centered dot for each space, an
    // arrow spanning each tab stop. Off by default, like most editors.
    bool showWhitespace = false;
    // When true (the default, matching most editors' "insert spaces" mode),
    // pressing Tab inserts tabSize spaces. When false, it inserts one real
    // '\t' byte instead - tabSize then only controls how wide that '\t'
    // renders (its column advance and the visual span the whitespace arrow
    // covers), not how many bytes are inserted.
    bool insertSpacesForTab = true;
    int tabSize = 4;
};

// One foldable region, from a line that opens it (foldDelta +1, e.g. a line
// ending in "{") to the line that closes it (foldDelta -1, e.g. "}").
struct CodeEditorFoldRange
{
    int startLine = 0, endLine = 0;
    bool collapsed = false;
};

// One secondary caret+selection added by Ctrl+D ("add next occurrence").
// The primary cursor (CodeEditorState::cursorLine/cursorColumn/selectionAnchor*)
// is never stored here - this only holds the EXTRA ones.
struct CodeEditorCursor
{
    int line = 0, column = 0;
    int anchorLine = 0, anchorColumn = 0;
};

// Autocomplete popup state, rebuilt whenever the identifier under the cursor
// changes. Kept separate from CodeEditorState's edit history so accepting a
// suggestion is just another Replace edit.
struct CodeEditorCompletion
{
    ct::Vector<String> items;
    int selected = 0;
    int line = -1;                       // line the popup applies to
    int wordStart = -1, wordEnd = -1;    // byte range of the identifier being completed
    bool visible = false;
    // True once the user has pressed Up/Down to browse this popup. Enter and
    // Tab only accept the highlighted item when this is set - otherwise they
    // keep their normal meaning (newline, indent) so an unrequested popup
    // (opened just because the word being typed also occurs elsewhere in the
    // document) never blocks normal editing.
    bool navigated = false;
};

// Application-owned editor buffer and interaction state. One instance per
// on-screen editor; construct once and keep it alive across frames, exactly
// like FileDialogState. All mutation goes through this class so every path
// (typing, autocomplete acceptance, undo/redo) keeps the highlight cache and
// undo history consistent - Context::codeEditor() never touches lines_
// directly.
class CodeEditorState
{
public:
    CodeEditorState();
    ~CodeEditorState();
    CodeEditorState(const CodeEditorState&) = delete;
    CodeEditorState& operator=(const CodeEditorState&) = delete;

    // Replace the whole buffer (e.g. after loading a file). Clears undo
    // history, resets the cursor, and re-highlights every line.
    void setText(const String& text);
    // Concatenates lines with '\n'. O(n) in document size - cache the result
    // if calling this every frame.
    String text() const;

    int lineCount() const { return static_cast<int>(lines_.size()); }
    const String& lineAt(int line) const { return lines_[static_cast<size_t>(line)]; }

    // Installs a highlighter (buffer content and undo history are kept;
    // only cached highlight spans are invalidated). Passing nullptr to
    // setHighlighter disables highlighting.
    void setHighlighter(syntax::SyntaxHighlighter* highlighter);
    void setLanguage(const syntax::SyntaxLanguage& language);
    syntax::SyntaxHighlighter* setHighlighterForFile(const String& filename);
    syntax::SyntaxHighlighter* highlighter() const { return highlight_.highlighter(); }
    // Colored spans for one line, using the cached previous-line lexer
    // state. Empty (no spans) when there is no highlighter installed.
    syntax::SyntaxHighlighter::HighlightResult highlightSpans(int line) const
    { return highlight_.highlightLine(line, lineAt(line)); }

    // ── Editing (all undo-tracked) ───────────────────────────────────────
    // Insert text at (line, column); moves the cursor to the end of it.
    // Newlines in text split lines as expected.
    void insertText(int line, int column, const String& text);
    // Erase [start, end) and move the cursor to start. Returns the removed text.
    String eraseRange(int startLine, int startColumn, int endLine, int endColumn);
    // Erase the current selection, if any; returns true if something was removed.
    bool eraseSelection();

    // ── Undo / redo ──────────────────────────────────────────────────────
    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }
    void undo();
    void redo();
    void clearUndoHistory();
    // Ends coalescing so the next insertText starts a new undo group (call
    // after moving the cursor without typing, e.g. a mouse click).
    void breakUndoCoalescing() { lastEditWasTyping_ = false; ++editGroup_; }

    // ── Cursor / selection (line/column, both zero-based) ───────────────
    int cursorLine = 0, cursorColumn = 0;
    int selectionAnchorLine = 0, selectionAnchorColumn = 0;
    bool hasSelection() const
    { return selectionAnchorLine != cursorLine || selectionAnchorColumn != cursorColumn; }
    void clearSelection() { selectionAnchorLine = cursorLine; selectionAnchorColumn = cursorColumn; }
    // Selection endpoints in document order (a <= b).
    void orderedSelection(int& startLine, int& startColumn, int& endLine, int& endColumn) const;
    String selectedText() const;

    // ── Multi-cursor (Ctrl+D "add selection to next find match") ───────────
    // Extra carets beyond the primary cursor/selection above. Empty in the
    // common single-cursor case - most call sites can ignore this entirely.
    int extraCursorCount() const { return static_cast<int>(extraCursors_.size()); }
    const CodeEditorCursor& extraCursorAt(int i) const { return extraCursors_[static_cast<size_t>(i)]; }
    bool hasMultipleCursors() const { return !extraCursors_.empty(); }
    // Ctrl+D: with no selection, selects the word under the primary cursor.
    // With a selection, adds a new cursor+selection at the next occurrence of
    // the selected text (wrapping around the document), skipping any
    // occurrence already covered by an existing cursor. No-op if nothing else
    // matches.
    void addNextOccurrenceCursor();
    // Drops every extra cursor, keeping only the primary. Called on Escape or
    // any plain (non-extending) cursor movement.
    void clearExtraCursors() { extraCursors_.clear(); }

    // Applies the same insert/erase every plain typing/Backspace/Delete/Enter
    // path already does, but at the primary cursor AND every extra cursor at
    // once, as a single undo step (one CodeEditorEdit per cursor, sharing the
    // same group). With no extra cursors these behave exactly like the
    // single-cursor primitives above.
    void insertTextAtAllCursors(const String& text);
    bool eraseSelectionsAtAllCursors(); // true if anything had a selection to erase
    void backspaceAtAllCursors();
    void deleteAtAllCursors();

    // ── Find / replace ───────────────────────────────────────────────────
    // Each match is independent per line: a regex pattern's ^/$ anchor to
    // the line, and no match spans a line break (mirroring the retained
    // CodeEditor's line-oriented search). An invalid regex pattern matches
    // nothing rather than silently falling back to a literal search.
    //
    // Selects the next/previous match (wrapping around the document) and
    // moves the cursor there. Returns false when the pattern is empty,
    // invalid (when useRegex), or matches nothing.
    bool findNext(const String& pattern, bool caseSensitive = false, bool useRegex = false);
    bool findPrev(const String& pattern, bool caseSensitive = false, bool useRegex = false);
    // Replaces every match with replacement (or, when useRegex, with a
    // template that may reference capture groups as \1, \2, ...). One undo
    // group per call. Returns the number of replacements made.
    int replaceAll(const String& pattern, const String& replacement,
                   bool caseSensitive = false, bool useRegex = false);

    // ── Folding ────────────────────────────────────────────────────────────
    // Recomputed after every edit and highlighter change from the installed
    // highlighter's foldDelta (nesting-based: a stack of open regions).
    // Collapsed state persists across a rebuild by (startLine, endLine).
    const ct::Vector<CodeEditorFoldRange>& foldRanges() const { return foldRanges_; }
    // True for the header line of a fold region (collapsed or not) - the
    // line the gutter chevron is drawn next to.
    bool isFoldHeader(int line) const;
    // True for a line hidden because it is inside a collapsed ancestor fold
    // (but not the header line itself, which stays visible and shows "...").
    bool isLineHidden(int line) const;
    void toggleFoldAt(int line);
    void foldAll();
    void unfoldAll();
    // Cached visual row map, rebuilt only after text or folding changes.
    const ct::Vector<int>& visibleLines() const;

    // ── Zoom ───────────────────────────────────────────────────────────────
    // Multiplies CodeEditorOptions/theme font size for this editor only.
    // Persists in the state (like scrollLine), so it survives across frames
    // without the application tracking it separately.
    float fontScale() const { return fontScale_; }
    void setFontScale(float scale) { fontScale_ = scale < 0.25f ? 0.25f : (scale > 4.0f ? 4.0f : scale); }
    void zoomIn() { setFontScale(fontScale_ + 0.1f); }
    void zoomOut() { setFontScale(fontScale_ - 0.1f); }
    void zoomReset() { fontScale_ = 1.0f; }

    // Autocomplete popup and first visible line: Context::codeEditor() drives
    // both, but they are kept as public state (rather than hidden behind
    // more friend surface) so an application can inspect or reset them
    // (e.g. dismiss the popup on focus loss, or scroll to a line).
    CodeEditorCompletion completion;
    int scrollLine = 0;

private:
    ct::Vector<String> lines_;
    syntax::LineHighlightCache highlight_;
    ct::Vector<CodeEditorEdit> undoStack_;
    ct::Vector<CodeEditorEdit> redoStack_;
    uint64_t editGroup_ = 0;      // bumped whenever typing stops being contiguous
    bool lastEditWasTyping_ = false;
    ct::Vector<CodeEditorFoldRange> foldRanges_;
    mutable ct::Vector<int> visibleLines_;
    mutable bool visibleLinesDirty_ = true;
    float fontScale_ = 1.0f;
    ct::Vector<CodeEditorCursor> extraCursors_;

    void recordEdit(CodeEditorEdit::Kind kind, int line, int column,
                    const String& removed, const String& inserted,
                    int beforeLine, int beforeColumn, bool coalesce);
    void insertTextImpl(int line, int column, const String& text, bool bumpGroup);
    String eraseRangeImpl(int startLine, int startColumn, int endLine, int endColumn, bool bumpGroup);
    // Writes each per-cursor post-edit (line, column) back to extraCursors_
    // (collapsing that cursor's selection) or, for the primary, to
    // cursorLine/cursorColumn. AllCursorsResult is defined in
    // CodeEditorImmediate.cpp; declared here only as an opaque template so
    // the type doesn't need a public home.
    template <typename ResultVector> void applyMultiCursorResults(const ResultVector& results);
    void applyEditForward(const CodeEditorEdit& edit);
    void applyEditBackward(const CodeEditorEdit& edit);
    void rehighlightFrom(int line);
    void rebuildFoldRanges();
    void clampCursor();
};

} // namespace ig
