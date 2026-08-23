#pragma once

#include "Widget.hpp"
#include "Theme.hpp"
#include "Utf8.hpp"
#include <string>
#include <functional>
#include <vector>


namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  TextInput - single-line text input field
//  Modes: Normal, Password (•), NumberOnly (digits/-.+), ReadOnly
// ═════════════════════════════════════════════════════════════════════════════

class TextInput : public Widget
{
public:
    enum class Mode { Normal, Password, NumberOnly, ReadOnly };

    explicit TextInput(const std::string& text = "", Mode mode = Mode::Normal);

    /// @brief Set the text content.
    void setText(const std::string& t);
    /// @brief Get the text content.
    const std::string& text() const { return text_; }

    /// @brief Set placeholder text shown when empty.
    void setPlaceholder(const std::string& p) { placeholder_ = p; markDirty(); }
    /// @brief Get the placeholder text.
    const std::string& placeholder() const    { return placeholder_; }

    /// @brief Set the input mode (Normal, Password, NumberOnly, ReadOnly).
    void setMode(Mode m) { mode_ = m; markDirty(); }
    /// @brief Get the input mode.
    Mode mode() const    { return mode_; }

    /// @brief Select all text.
    void selectAll();
    /// @brief Clear the text selection.
    void clearSelection();
    /// @brief Check if any text is selected.
    bool hasSelection() const { return selStart_ != selEnd_; }
    /// @brief Get the selected text.
    std::string selectedText() const;

    /// @brief Get the cursor position (character index).
    int cursorPos() const { return cursor_; }
    /// @brief Set the cursor position.
    void setCursorPos(int pos);

    /// @brief Emitted when the text content changes.
    Signal<const std::string&> textChanged;
    /// @brief Emitted when Enter is pressed.
    Signal<const std::string&> submitted;

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onKeyPress(KeyEvent& e) override;
    void onTextInput(KeyEvent& e) override;

private:
    std::string text_;
    std::string placeholder_;
    Mode mode_ = Mode::Normal;

    int   cursor_   = 0;
    int   selStart_ = 0;
    int   selEnd_   = 0;
    float scrollX_  = 0;
    float blinkTimer_ = 0;
    bool  dragging_   = false;

    std::string displayText() const;
    int   hitTestChar(const BuGUI::Font* font, float localX) const;
    float cursorXOffset(const BuGUI::Font* font, int pos) const;
    void  deleteSelection();
    void  insertText(const std::string& t);
    void  ensureCursorVisible(const BuGUI::Font* font);
    void  clampCursor();
    bool  isNumberChar(char c) const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  TextEdit - multi-line text editor
//  Features: line numbers, cursor 2D, selection, scroll, word wrap,
//            tab-to-spaces, clipboard, syntax highlight callback
// ═════════════════════════════════════════════════════════════════════════════

class TextEdit : public Widget
{
public:
    struct TextPos
    {
        int line = 0;
        int col  = 0;

        bool operator==(const TextPos& o) const { return line == o.line && col == o.col; }
        bool operator!=(const TextPos& o) const { return !(*this == o); }
        bool operator<(const TextPos& o) const  { return line < o.line || (line == o.line && col < o.col); }
        bool operator>(const TextPos& o) const  { return o < *this; }
        bool operator<=(const TextPos& o) const { return !(o < *this); }
        bool operator>=(const TextPos& o) const { return !(*this < o); }
    };

    struct ColorSpan
    {
        int startCol, endCol;
        Color color;
    };

    using SyntaxCallback = std::function<std::vector<ColorSpan>(int lineIndex, const std::string& lineText)>;

    explicit TextEdit(const std::string& text = "");

    /// @brief Set the entire text content.
    void setText(const std::string& text);
    /// @brief Get all text as a single string.
    std::string text() const;
    /// @brief Get the number of lines.
    int lineCount() const { return static_cast<int>(lines_.size()); }
    /// @brief Get the text of line n.
    const std::string& lineAt(int n) const;

    /// @brief Get the cursor position (line, col).
    TextPos cursorPos() const { return cursor_; }
    /// @brief Set the cursor position.
    void setCursorPos(TextPos pos);
    /// @brief Set the cursor by line and column.
    void setCursorPos(int line, int col);

    /// @brief Select all text.
    void selectAll();
    /// @brief Select an entire line.
    void selectLine(int n);
    /// @brief Set selection by start and end positions.
    void setSelection(TextPos start, TextPos end);
    /// @brief Clear the selection.
    void clearSelection() { selAnchor_ = cursor_; markDirty(); }
    /// @brief Check if there is a selection.
    bool hasSelection() const { return selAnchor_ != cursor_; }
    /// @brief Get the selected text.
    std::string selectedText() const;
    /// @brief Get the start of the selection.
    TextPos selectionStart() const { return cursor_ < selAnchor_ ? cursor_ : selAnchor_; }
    /// @brief Get the end of the selection.
    TextPos selectionEnd()   const { return cursor_ > selAnchor_ ? cursor_ : selAnchor_; }

    /// @brief Set read-only mode.
    void setReadOnly(bool r) { readOnly_ = r; }
    /// @brief Check if in read-only mode.
    bool readOnly() const    { return readOnly_; }

    /// @brief Show or hide line numbers in the gutter.
    void setShowLineNumbers(bool s) { showLineNumbers_ = s; markDirty(); }
    /// @brief Check if line numbers are shown.
    bool showLineNumbers() const    { return showLineNumbers_; }

    /// @brief Enable or disable word wrap.
    void setWordWrap(bool w) { wordWrap_ = w; wrapDirty_ = true; markDirty(); }
    /// @brief Check if word wrap is enabled.
    bool wordWrap() const    { return wordWrap_; }

    /// @brief Set the tab stop width in spaces.
    void setTabSize(int n) { tabSize_ = n; }
    /// @brief Get the tab stop width.
    int  tabSize() const   { return tabSize_; }

    /// @brief Set font zoom scale (1.0 = 100%).
    void setFontScale(float s) { fontScale_ = max2(0.5f, min2(3.0f, s)); markDirty(); }
    /// @brief Get font zoom scale.
    float fontScale() const    { return fontScale_; }

    /// @brief Set a callback for syntax highlighting.
    void setSyntaxCallback(SyntaxCallback cb) { syntaxCb_ = std::move(cb); }

    // ── Clipboard ─────────────────────────────────────────────────────────

    /// @brief Copy selected text to clipboard.
    void copy();
    /// @brief Cut selected text to clipboard.
    void cut();
    /// @brief Paste text from clipboard at cursor.
    void paste();

    /// @brief Insert text at a specific position (does not move cursor).
    void insertTextAt(TextPos pos, const std::string& text);

    // ── Gutter markers ────────────────────────────────────────────────────
    enum class MarkerType { Breakpoint, Error, Warning, Bookmark, Info };

    /// @brief Set a gutter marker on a line.
    void setMarker(int line, MarkerType type);
    /// @brief Clear all markers on a line.
    void clearMarker(int line);
    /// @brief Clear a specific marker type on a line.
    void clearMarker(int line, MarkerType type);
    /// @brief Clear all markers.
    void clearAllMarkers();
    /// @brief Check if a line has any marker.
    bool hasMarker(int line) const;
    /// @brief Check if a line has a specific marker type.
    bool hasMarker(int line, MarkerType type) const;

    /// @brief Emitted when a gutter marker is clicked.
    Signal<int, MarkerType> markerClicked;

    /// @brief Get the first visible line index.
    int firstVisibleLine() const;
    /// @brief Get the number of visible lines.
    int visibleLineCount() const;
    /// @brief Scroll the view to show a given line.
    void scrollToLine(int n);

    /// @brief Emitted when text content changes.
    Signal<>         textChanged;
    /// @brief Emitted when the cursor moves.
    Signal<TextPos>  cursorMoved;

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onKeyPress(KeyEvent& e) override;
    void onTextInput(KeyEvent& e) override;

protected:
    static const std::string emptyLine_;

    std::vector<std::string> lines_;
    TextPos cursor_;
    TextPos selAnchor_;

    bool  readOnly_       = false;
    bool  showLineNumbers_ = true;
    bool  wordWrap_       = false;
    int   tabSize_        = 4;
    float fontScale_      = 1.0f;   // zoom: 1.0 = 100%

    float scrollX_ = 0, scrollY_ = 0;
    float blinkTimer_ = 0;
    bool  dragging_   = false;
    float extraGutterW_ = 0;   // extra gutter space (e.g. fold icons)

    SyntaxCallback syntaxCb_;

    /// @brief Optional filter: return false to hide a line (used by CodeEditor folding).
    std::function<bool(int)> lineFilterCb_;

    // Gutter markers: line → set of marker types
    std::unordered_map<int, std::unordered_set<int>> markers_;  // int = MarkerType

    // Computed each paint
    mutable float lineHeight_ = 0;
    mutable float gutterW_    = 0;

    // Helpers accessible to subclasses
    int  lineColCount(int line) const;
    void clampPos(TextPos& p) const;
    void deleteSelection();
    void insertTextAtCursor(const std::string& t);
    void splitLine();
    void mergeWithPrevLine();
    void mergeWithNextLine();
    void ensureCursorVisible();
    float computeGutterWidth(const BuGUI::Font& font) const;
    float computeLineHeight(const BuGUI::Font& font) const;
    void invalidateWrap() { wrapDirty_ = true; }
    int  logicalToVisualRow(int line, int row = 0) const;
    std::string expandTabs(const std::string& text) const;
    std::string expandTabs(const std::string& text, int startVisCol) const;
    std::string expandTabsVisible(const std::string& text, int startVisCol) const;
    int  visColAt(int line, int col) const;

private:
    // Word-wrap cache
    struct WrapRow { int startCol, endCol; };
    std::vector<std::vector<WrapRow>> wrapRows_;
    bool  wrapDirty_   = true;
    float wrapWidth_   = 0;
    float maxContentW_ = 0;

    // Internal helpers
    float colToX(PaintContext& ctx, int line, int col) const;
    int   xToCol(PaintContext& ctx, int line, float x) const;
    TextPos hitTest(PaintContext& ctx, float localX, float localY) const;
    TextPos hitTestMouse(float localX, float localY) const;
    void rebuildWrap(PaintContext& ctx);

    // Visual row helpers (for word-wrap)
    struct VisualPos { int line, row; };
    int        totalVisualRows() const;
    VisualPos  visualToLogical(int visualRow) const;
};

} // namespace BuGUI
