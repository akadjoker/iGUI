// The CodeEditor's tools: the recorded range edit the commands are built on,
// the inline bars (find, replace, go to line, go to symbol), the commands
// behind the shortcuts, the jump list and the occurrence highlight. The
// editing core (text, cursors, folding, painting the text) is CodeEditor.cpp.
#include "CodeEditor.hpp"
#include "Utf8.hpp"
#include "WidgetApp.hpp"
#include "pch.hpp"
#include <algorithm>
#include <ct/sort.hpp>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace
{

constexpr int kListRows = 10;
constexpr size_t kMaxJumps = 100;

bool isWord(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// Every character of `pattern` appears in `text` in order (a fuzzy filter):
// "ond" matches "on_digit". Case does not matter.
bool fuzzyMatches(const String& pattern, const String& text)
{
    size_t p = 0;
    for (size_t i = 0; i < text.size() && p < pattern.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(text[i])) == std::tolower(static_cast<unsigned char>(pattern[p]))) ++p;
    return p == pattern.size();
}

// Lower is better: a prefix first, then a word start, then anywhere.
int fuzzyRank(const String& pattern, const String& text)
{
    if (pattern.empty()) return 2;
    if (text.size() >= pattern.size() &&
            strncasecmp(text.c_str(), pattern.c_str(), pattern.size()) == 0)
        return 0;
    for (size_t i = 1; i + pattern.size() <= text.size(); ++i)
        if ((text[i - 1] == '_' || text[i - 1] == '.' || text[i - 1] == ':') &&
                strncasecmp(text.c_str() + i, pattern.c_str(), pattern.size()) == 0)
            return 1;
    return 2;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
//  Recorded edits
// ═════════════════════════════════════════════════════════════════════════════

TextEdit::TextPos CodeEditor::advance(TextPos pos, const String& text)
{
    for (size_t i = 0; i < text.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '\n')
        {
            pos.line++;
            pos.col = 0;
        }
        else if ((c & 0xC0) != 0x80)
            pos.col++;
    }
    return pos;
}

void CodeEditor::beginUndoGroup()
{
    undoGroup_ = nextGroup_++;
}

void CodeEditor::endUndoGroup()
{
    undoGroup_ = 0;
}

void CodeEditor::recordReplace(TextPos pos, const String& oldText, const String& newText, TextPos oldCursor, TextPos newCursor)
{
    if (recording_) return;
    clearRedoStack();
    undoStack_.push_back({ EditAction::Replace, pos, oldText, newText, oldCursor, newCursor, undoGroup_ });
    undoRedoChanged.emit(canUndo(), canRedo());
}

void CodeEditor::replaceRange(TextPos start, TextPos end, const String& text)
{
    if (readOnly_) return;
    clampPos(start);
    clampPos(end);
    if (end < start) std::swap(start, end);
    const TextPos oldCursor = cursor_;
    setSelection(start, end);
    const String old = selectedText();
    if (old == text)
    {
        cursor_ = oldCursor;
        selAnchor_ = cursor_;
        return;
    }
    if (hasSelection()) deleteSelection();
    insertTextAtCursor(text);
    recordReplace(start, old, text, oldCursor, cursor_);
    hlDirty_ = true;
    invalidateWrap();
    markDirty();
    textChanged.emit();
    cursorMoved.emit(cursor_);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Bars
// ═════════════════════════════════════════════════════════════════════════════

void CodeEditor::showBar(Bar bar)
{
    closeCompletion();
    if (bar_ == Bar::None) barOrigin_ = hasSelection() ? selectionStart() : cursor_;
    const Bar previous = bar_;
    bar_ = bar;
    barField_ = 0;
    if (bar == Bar::Find || bar == Bar::Replace)
    {
        // A one-line selection is what the user means to look for.
        if (hasSelection() && selectionStart().line == selectionEnd().line)
        {
            barText_ = selectedText();
            barOrigin_ = selectionStart();
        }
        else if (previous != Bar::Find && previous != Bar::Replace)
            barText_ = searchTerm_;
        liveFind();
    }
    else
    {
        barText_.clear();
        if (bar == Bar::GotoSymbol) refilterPalette();
    }
    markDirty();
}

void CodeEditor::hideBar()
{
    if (bar_ == Bar::None) return;
    bar_ = Bar::None;
    paletteMatches_.clear();
    markDirty();
}

void CodeEditor::barInput(const char* text)
{
    if (!text || !text[0]) return;
    if (bar_ == Bar::GotoLine)
    {
        for (const char* p = text; *p; ++p)
            if (*p >= '0' && *p <= '9') barText_ += *p;
    }
    else if (bar_ == Bar::Replace && barField_ == 1)
        barReplace_ += text;
    else
        barText_ += text;
    barChanged();
}

void CodeEditor::barBackspace()
{
    String& field = bar_ == Bar::Replace && barField_ == 1 ? barReplace_ : barText_;
    if (field.empty()) return;
    // Whole code points: a multi-byte character goes in one press.
    size_t n = field.size();
    do
        --n;
    while (n > 0 && (static_cast<unsigned char>(field[n]) & 0xC0) == 0x80);
    field.resize(n);
    barChanged();
}

void CodeEditor::barChanged()
{
    if (bar_ == Bar::Find || (bar_ == Bar::Replace && barField_ == 0)) liveFind();
    else if (bar_ == Bar::GotoSymbol) refilterPalette();
    markDirty();
}

// The first match at or after where the bar opened; typing more letters
// narrows it in place instead of running ahead through the text.
void CodeEditor::liveFind()
{
    if (barText_.empty())
    {
        searchTerm_.clear();
        searchMatches_.clear();
        searchMatchIdx_ = -1;
        cursor_ = barOrigin_;
        clampPos(cursor_);
        selAnchor_ = cursor_;
        return;
    }
    cursor_ = barOrigin_;
    clampPos(cursor_);
    selAnchor_ = cursor_;
    if (!findNext(barText_, searchCaseSensitive_, searchWholeWord_, searchIsRegex_))
    {
        searchTerm_ = barText_;
        cursor_ = barOrigin_;
        clampPos(cursor_);
        selAnchor_ = cursor_;
    }
}

void CodeEditor::refilterPalette()
{
    paletteMatches_.clear();
    const ct::Vector<syntax::Symbol>& list = symbols();
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].kind != syntax::SymbolKind::Import && fuzzyMatches(barText_, list[i].name))
            paletteMatches_.push_back(static_cast<int>(i));
    // Prefix matches first, word starts next, the rest in text order.
    ct::stable_sort(paletteMatches_.begin(), paletteMatches_.end(), [&](int a, int b)
    {
        return fuzzyRank(barText_, list[static_cast<size_t>(a)].name) < fuzzyRank(barText_, list[static_cast<size_t>(b)].name);
    });
    paletteSelected_ = 0;
    paletteFirst_ = 0;
}

bool CodeEditor::barKey(KeyEvent& e)
{
    namespace Key = ig::retained::Key;
    switch (e.key)
    {
        case Key::Escape:
            hideBar();
            e.consumed = true;
            return true;
        case Key::Backspace:
            if (e.ctrl)
            {
                (bar_ == Bar::Replace && barField_ == 1 ? barReplace_ : barText_).clear();
                barChanged();
            }
            else
                barBackspace();
            e.consumed = true;
            return true;
        case Key::Tab:
            if (bar_ == Bar::Replace)
            {
                barField_ = 1 - barField_;
                markDirty();
            }
            e.consumed = true;
            return true;
        case Key::Up:
        case Key::Down:
            if (bar_ == Bar::GotoSymbol && !paletteMatches_.empty())
            {
                const int n = static_cast<int>(paletteMatches_.size());
                paletteSelected_ = e.key == Key::Up ? (paletteSelected_ + n - 1) % n : (paletteSelected_ + 1) % n;
                if (paletteSelected_ < paletteFirst_) paletteFirst_ = paletteSelected_;
                if (paletteSelected_ >= paletteFirst_ + kListRows) paletteFirst_ = paletteSelected_ - kListRows + 1;
                markDirty();
                e.consumed = true;
                return true;
            }
            return false;
        case Key::Return:
        case Key::KPEnter:
            e.consumed = true;
            if (bar_ == Bar::Find)
            {
                if (barText_.empty()) return true;
                if (e.shift) findPrev(barText_, searchCaseSensitive_, searchWholeWord_, searchIsRegex_);
                else findNext(barText_, searchCaseSensitive_, searchWholeWord_, searchIsRegex_);
                pushJump();
            }
            else if (bar_ == Bar::Replace)
            {
                if (barText_.empty()) return true;
                if (e.ctrl) replaceAll(barText_, barReplace_, searchCaseSensitive_, searchIsRegex_);
                else replaceNext(barText_, barReplace_, searchCaseSensitive_, searchIsRegex_);
            }
            else if (bar_ == Bar::GotoLine)
            {
                const int line = std::atoi(barText_.c_str());
                hideBar();
                if (line >= 1) reveal(std::min(line, lineCount()) - 1);
            }
            else if (bar_ == Bar::GotoSymbol)
            {
                if (paletteSelected_ >= 0 && paletteSelected_ < static_cast<int>(paletteMatches_.size()))
                {
                    const syntax::Symbol& s = symbols()[static_cast<size_t>(paletteMatches_[static_cast<size_t>(paletteSelected_)])];
                    hideBar();
                    reveal(s.line, s.indent);
                }
                else
                    hideBar();
            }
            return true;
        case Key::F3:
            if (bar_ == Bar::Find || bar_ == Bar::Replace)
            {
                if (e.shift) findPrev(barText_, searchCaseSensitive_, searchWholeWord_, searchIsRegex_);
                else findNext(barText_, searchCaseSensitive_, searchWholeWord_, searchIsRegex_);
                e.consumed = true;
                return true;
            }
            return false;
        default:
            break;
    }
    // Alt+C / Alt+W / Alt+R: the find flags, as in most editors.
    if (e.alt && !e.ctrl && (bar_ == Bar::Find || bar_ == Bar::Replace))
    {
        bool flag = true;
        if (e.key == Key::C) searchCaseSensitive_ = !searchCaseSensitive_;
        else if (e.key == Key::W) searchWholeWord_ = !searchWholeWord_;
        else if (e.key == Key::R) searchIsRegex_ = !searchIsRegex_;
        else flag = false;
        if (flag)
        {
            liveFind();
            markDirty();
            e.consumed = true;
            return true;
        }
    }
    // Ctrl+F / Ctrl+H / Ctrl+G again switch bars; other keys reach the editor.
    return false;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Commands
// ═════════════════════════════════════════════════════════════════════════════

void CodeEditor::selectLine()
{
    const int last = lineCount() - 1;
    TextPos start = hasSelection() ? selectionStart() : cursor_;
    TextPos end = hasSelection() ? selectionEnd() : cursor_;
    // Already whole lines: one more line.
    const bool wholeLines = hasSelection() && start.col == 0 && end.col == 0 && end.line > start.line;
    int lastLine = wholeLines ? end.line : end.line;
    if (wholeLines && end.line <= last) lastLine = end.line;
    else lastLine = end.line;
    TextPos from = { start.line, 0 };
    TextPos to = lastLine < last ? TextPos{ lastLine + 1, 0 } : TextPos{ last, lineColCount(last) };
    setSelection(from, to);
    ensureCursorVisible();
    cursorMoved.emit(cursor_);
}

void CodeEditor::joinLines()
{
    if (readOnly_) return;
    const int ln = hasSelection() ? selectionStart().line : cursor_.line;
    if (ln + 1 >= lineCount()) return;
    const String& next = lineAt(ln + 1);
    int lead = 0;
    while (lead < static_cast<int>(next.size()) && (next[static_cast<size_t>(lead)] == ' ' || next[static_cast<size_t>(lead)] == '\t')) ++lead;
    const String& line = lineAt(ln);
    const bool spaceNeeded = !line.empty() && lead < static_cast<int>(next.size()) &&
                             line[line.size() - 1] != ' ' && line[line.size() - 1] != '\t';
    const TextPos joinAt = { ln, lineColCount(ln) };
    replaceRange(joinAt, { ln + 1, lead }, spaceNeeded ? String(" ") : String());
    cursor_ = joinAt;
    selAnchor_ = cursor_;
    cursorMoved.emit(cursor_);
}

bool CodeEditor::jumpToMatchingBracket(bool select)
{
    // The bracket at the cursor, or the one just before it (the cursor
    // usually sits after a closing bracket).
    BracketPair pair = findMatchingBracket(cursor_);
    TextPos at = cursor_;
    if (pair.close.line < 0 && cursor_.col > 0)
    {
        at = { cursor_.line, cursor_.col - 1 };
        pair = findMatchingBracket(at);
    }
    if (pair.close.line < 0) return false;
    pushJump();
    // To the other end: after a closing bracket, before an opening one.
    const TextPos other = pair.close;
    const char c = lineAt(other.line)[static_cast<size_t>(other.col)];
    TextPos target = other;
    if (c == ')' || c == ']' || c == '}') target.col += 1;
    if (select)
    {
        TextPos from = at;
        const char here = lineAt(at.line)[static_cast<size_t>(at.col)];
        if (here == ')' || here == ']' || here == '}') from.col += 1;
        setSelection(from, target);
    }
    else
        setCursorPos(target);
    ensureCursorVisible();
    return true;
}

String CodeEditor::wordAtCursor(TextPos& start, TextPos& end) const
{
    if (hasSelection())
    {
        start = selectionStart();
        end = selectionEnd();
        if (start.line != end.line) return String();
        return selectedText();
    }
    start = findWordStart(cursor_);
    end = findWordEnd(cursor_);
    if (start == end) return String();
    return utf8Substr(lineAt(start.line), start.col, end.col);
}

bool CodeEditor::gotoDefinition()
{
    TextPos start, end;
    const String word = wordAtCursor(start, end);
    if (word.empty()) return false;
    // "A.b" or "A::b": the last part names the method; the whole name first.
    if (gotoSymbol(ig::StringView(word.data(), word.size()))) return true;
    size_t cut = word.size();
    while (cut > 0 && isWord(word[cut - 1])) --cut;
    if (cut == 0 || cut == word.size()) return false;
    return gotoSymbol(ig::StringView(word.data() + cut, word.size() - cut));
}

void CodeEditor::addCursorAbove()
{
    int line = cursor_.line;
    for (const CursorState& c: extraCursors_) line = std::min(line, c.pos.line);
    if (line <= 0) return;
    addCursor(line - 1, std::min(cursor_.col, lineColCount(line - 1)));
}

void CodeEditor::addCursorBelow()
{
    int line = cursor_.line;
    for (const CursorState& c: extraCursors_) line = std::max(line, c.pos.line);
    if (line + 1 >= lineCount()) return;
    addCursor(line + 1, std::min(cursor_.col, lineColCount(line + 1)));
}

void CodeEditor::toggleBookmark()
{
    if (hasMarker(cursor_.line, MarkerType::Bookmark)) clearMarker(cursor_.line, MarkerType::Bookmark);
    else setMarker(cursor_.line, MarkerType::Bookmark);
    markDirty();
}

bool CodeEditor::nextBookmark()
{
    const int lc = lineCount();
    for (int step = 1; step <= lc; ++step)
    {
        const int line = (cursor_.line + step) % lc;
        if (hasMarker(line, MarkerType::Bookmark))
        {
            reveal(line, 0);
            return true;
        }
    }
    return false;
}

bool CodeEditor::previousBookmark()
{
    const int lc = lineCount();
    for (int step = 1; step <= lc; ++step)
    {
        const int line = (cursor_.line - step + lc) % lc;
        if (hasMarker(line, MarkerType::Bookmark))
        {
            reveal(line, 0);
            return true;
        }
    }
    return false;
}

void CodeEditor::transformSelection(bool upper)
{
    if (readOnly_ || !hasSelection()) return;
    const TextPos start = selectionStart(), end = selectionEnd();
    String text = selectedText();
    for (size_t i = 0; i < text.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) text[i] = static_cast<char>(upper ? std::toupper(c) : std::tolower(c));
    }
    replaceRange(start, end, text);
    setSelection(start, advance(start, text));
}

void CodeEditor::indentLines(int first, int last, bool indent)
{
    if (readOnly_) return;
    const TextPos keepCursor = cursor_, keepAnchor = selAnchor_;
    int shiftFirst = 0, shiftLast = 0;
    beginUndoGroup();
    for (int ln = first; ln <= last; ++ln)
    {
        int delta = 0;
        if (indent)
        {
            if (lineAt(ln).empty()) continue; // blank lines stay blank
            replaceRange({ ln, 0 }, { ln, 0 }, "\t");
            delta = 1;
        }
        else
        {
            const String& line = lineAt(ln);
            int removed = 0;
            if (!line.empty() && line[0] == '\t') removed = 1;
            else
                while (removed < tabSize_ && removed < static_cast<int>(line.size()) && line[static_cast<size_t>(removed)] == ' ') removed++;
            if (removed == 0) continue;
            replaceRange({ ln, 0 }, { ln, removed }, "");
            delta = -removed;
        }
        if (ln == first) shiftFirst = delta;
        if (ln == last) shiftLast = delta;
    }
    endUndoGroup();
    // The selection keeps covering the same lines.
    TextPos cursor = keepCursor, anchor = keepAnchor;
    TextPos& lower = cursor < anchor ? cursor : anchor;
    TextPos& higher = cursor < anchor ? anchor : cursor;
    // A selection from the start of a line keeps the line's new indentation
    // inside it; one starting mid-line stays on its character.
    if (lower.col > 0) lower.col = std::max(0, lower.col + (lower.line == first ? shiftFirst : 0));
    if (higher.col > 0) higher.col = std::max(0, higher.col + (higher.line == last ? shiftLast : 0));
    selAnchor_ = anchor;
    cursor_ = cursor;
    clampPos(selAnchor_);
    clampPos(cursor_);
    markDirty();
}

void CodeEditor::indentSelection()
{
    const TextPos start = hasSelection() ? selectionStart() : cursor_;
    const TextPos end = hasSelection() ? selectionEnd() : cursor_;
    // A selection ending at the start of a line does not include that line.
    const int last = end.col == 0 && end.line > start.line ? end.line - 1 : end.line;
    indentLines(start.line, last, true);
}

void CodeEditor::unindentSelection()
{
    const TextPos start = hasSelection() ? selectionStart() : cursor_;
    const TextPos end = hasSelection() ? selectionEnd() : cursor_;
    const int last = end.col == 0 && end.line > start.line ? end.line - 1 : end.line;
    indentLines(start.line, last, false);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Jump list
// ═════════════════════════════════════════════════════════════════════════════

void CodeEditor::pushJump()
{
    // What was ahead of the current place is dropped, as in a browser.
    if (jumpIndex_ < static_cast<int>(jumps_.size())) jumps_.resize(static_cast<size_t>(jumpIndex_));
    if (!jumps_.empty() && jumps_.back() == cursor_) return;
    jumps_.push_back(cursor_);
    if (jumps_.size() > kMaxJumps) jumps_.erase(jumps_.begin());
    jumpIndex_ = static_cast<int>(jumps_.size());
}

bool CodeEditor::jumpBack()
{
    if (jumps_.empty()) return false;
    if (jumpIndex_ >= static_cast<int>(jumps_.size()))
    {
        // Leaving the newest place: remember it, so forward comes back here.
        if (jumps_.back() != cursor_)
        {
            jumps_.push_back(cursor_);
            if (jumps_.size() > kMaxJumps) jumps_.erase(jumps_.begin());
        }
        jumpIndex_ = static_cast<int>(jumps_.size()) - 1;
    }
    if (jumpIndex_ <= 0) return false;
    jumpIndex_--;
    TextPos to = jumps_[static_cast<size_t>(jumpIndex_)];
    clampPos(to);
    setCursorPos(to);
    ensureCursorVisible();
    return true;
}

bool CodeEditor::jumpForward()
{
    if (jumpIndex_ + 1 >= static_cast<int>(jumps_.size())) return false;
    jumpIndex_++;
    TextPos to = jumps_[static_cast<size_t>(jumpIndex_)];
    clampPos(to);
    setCursorPos(to);
    ensureCursorVisible();
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Painting: occurrences, the bar, the lists
// ═════════════════════════════════════════════════════════════════════════════

void CodeEditor::paintOccurrences(PaintContext& ctx, const Rect& abs)
{
    if (!highlightOccurrences_ || lineHeight() <= 0.0f) return;
    TextPos start, end;
    const String word = wordAtCursor(start, end);
    if (word.size() < 2 || word.size() > 64) return;
    const bool wholeWord = !hasSelection();
    const float lh = lineHeight();
    const float textX = abs.x + gutterW_ + Theme::instance().textEditPadding;
    const int first = firstVisibleLine();
    const int rows = visibleLineCount();
    // Only the lines on screen: the cursor moves every frame, the text is long.
    int row = 0;
    for (int line = 0; line < lineCount() && row < first + rows; ++line)
    {
        if (isLineHidden(line)) continue;
        const int vRow = row++;
        if (vRow < first) continue;
        const String& text = lineAt(line);
        size_t at = 0;
        while ((at = text.find(word, at)) != String::npos)
        {
            const size_t after = at + word.size();
            const bool boundary = !wholeWord || ((at == 0 || !isWord(text[at - 1])) && (after >= text.size() || !isWord(text[after])));
            if (boundary && !(line == start.line && static_cast<int>(at) == static_cast<int>(utf8ByteOffset(text, start.col))))
            {
                int col = 0;
                for (size_t i = 0; i < at; ++i)
                    if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) ++col;
                const int endCol = col + utf8Length(word);
                const float x = textX + colToPixelX(line, col) - scrollX_;
                const float w = colToPixelX(line, endCol) - colToPixelX(line, col);
                const float y = abs.y + vRow * lh - scrollY_;
                ctx.fill.SetColor(120, 150, 200, 50);
                ctx.fill.Rectangle(x, y, w, lh, true);
            }
            at = after;
        }
    }
}

void CodeEditor::paintList(PaintContext& ctx, const Rect& abs, const ct::Vector<ListRow>& rows, int selected, int first, float x, float y, Rect& outRect, float& outRowHeight)
{
    const float lh = lineHeight();
    if (rows.empty() || lh <= 0.0f) return;
    const Theme& t = Theme::instance();
    const int shown = std::min(kListRows, static_cast<int>(rows.size()) - first);
    float width = 0.0f;
    for (int i = 0; i < shown; ++i)
    {
        const ListRow& row = rows[static_cast<size_t>(first + i)];
        const float detail = row.detail.empty() ? 0.0f : ctx.font.GetTextWidth(row.detail.c_str()) + 24.0f;
        width = std::max(width, ctx.font.GetTextWidth(row.text.c_str()) + detail);
    }
    width = std::min(abs.w - 8.0f, std::max(160.0f, width + 20.0f));
    const float height = shown * lh + 8.0f;
    x = std::max(abs.x, std::min(x, abs.x + abs.w - width));
    if (y + height > abs.y + abs.h && y - lh - height - 4.0f >= abs.y) y = y - lh - height - 4.0f;
    outRect = { x, y, width, height };
    outRowHeight = lh;

    ctx.pushClip(abs);
    ctx.fill.SetColor(t.menuBg);
    ctx.fill.RoundedRectangle(x, y, width, height, 4, 6, true);
    ctx.line.SetColor(t.borderColor);
    ctx.line.RoundedRectangle(x, y, width, height, 4, 6, false);
    const float textOffset = (lh - ctx.font.GetFontSize()) * 0.5f;
    for (int i = 0; i < shown; ++i)
    {
        const int index = first + i;
        const ListRow& row = rows[static_cast<size_t>(index)];
        const float rowY = y + 4.0f + i * lh;
        if (index == selected)
        {
            ctx.fill.SetColor(t.selectionColor);
            ctx.fill.Rectangle(x + 2.0f, rowY, width - 4.0f, lh, true);
        }
        ctx.font.SetColor(t.textColor);
        ctx.font.Print(row.text.c_str(), x + 8.0f, rowY + textOffset);
        if (!row.detail.empty())
        {
            ctx.font.SetColor(t.textDisabled);
            const float dw = ctx.font.GetTextWidth(row.detail.c_str());
            ctx.font.Print(row.detail.c_str(), x + width - dw - 8.0f, rowY + textOffset);
        }
    }
    ctx.popClip();
}

void CodeEditor::paintBar(PaintContext& ctx, const Rect& abs)
{
    const Theme& t = Theme::instance();
    const float rowH = 24.0f;
    const int rows = bar_ == Bar::Replace ? 2 : 1;
    const float barH = rows * rowH + 8.0f;
    const float barW = std::min(460.0f, abs.w - 20.0f);
    const float barX = abs.x + abs.w - barW - 10.0f;
    const float barY = abs.y + 4.0f;
    barRect_ = { barX, barY, barW, barH };

    ctx.pushClip(abs);
    ctx.fill.SetColor(t.menuBg);
    ctx.fill.RoundedRectangle(barX, barY, barW, barH, 4, 6, true);
    ctx.line.SetColor(t.borderColor);
    ctx.line.RoundedRectangle(barX, barY, barW, barH, 4, 6, false);

    const char* label = bar_ == Bar::Find ? "Find" : bar_ == Bar::Replace ? "Find" : bar_ == Bar::GotoLine ? "Go to line" : "Symbol";
    char buffer[256];
    const float textY = barY + 4.0f + (rowH - ctx.font.GetFontSize()) * 0.5f;
    ctx.font.SetColor(t.textDisabled);
    snprintf(buffer, sizeof buffer, "%s:", label);
    ctx.font.Print(buffer, barX + 8.0f, textY);
    const float labelW = ctx.font.GetTextWidth("Go to line:") + 8.0f;
    const float fieldX = barX + 8.0f + labelW;
    // The field, with a caret after what is typed.
    ctx.font.SetColor(t.textColor);
    ctx.font.Print(barText_.c_str(), fieldX, textY);
    const bool caretOnFirst = !(bar_ == Bar::Replace && barField_ == 1);
    if (caretOnFirst)
    {
        const float cx = fieldX + ctx.font.GetTextWidth(barText_.c_str()) + 1.0f;
        ctx.line.SetColor(t.textColor);
        ctx.line.Line2D(cx, textY, cx, textY + ctx.font.GetFontSize());
    }
    // The right side: flags and the count for a find, a hint otherwise.
    String right;
    if (bar_ == Bar::Find || bar_ == Bar::Replace)
    {
        if (!barText_.empty())
        {
            if (searchMatches_.empty()) right = "no match";
            else
            {
                snprintf(buffer, sizeof buffer, "%d of %d", searchMatchIdx_ + 1, static_cast<int>(searchMatches_.size()));
                right = buffer;
            }
        }
        snprintf(buffer, sizeof buffer, "%s  %s%s%s", right.c_str(), searchCaseSensitive_ ? "[Aa]" : "[aa]", searchWholeWord_ ? "[W]" : "[w]", searchIsRegex_ ? "[.*]" : "[..]");
        right = buffer;
    }
    else if (bar_ == Bar::GotoLine)
    {
        snprintf(buffer, sizeof buffer, "1-%d", lineCount());
        right = buffer;
    }
    else
    {
        snprintf(buffer, sizeof buffer, "%d", static_cast<int>(paletteMatches_.size()));
        right = buffer;
    }
    ctx.font.SetColor(t.textDisabled);
    ctx.font.Print(right.c_str(), barX + barW - ctx.font.GetTextWidth(right.c_str()) - 8.0f, textY);
    if (bar_ == Bar::Replace)
    {
        const float y2 = textY + rowH;
        ctx.font.SetColor(t.textDisabled);
        ctx.font.Print("Replace:", barX + 8.0f, y2);
        ctx.font.SetColor(t.textColor);
        ctx.font.Print(barReplace_.c_str(), fieldX, y2);
        if (!caretOnFirst)
        {
            const float cx = fieldX + ctx.font.GetTextWidth(barReplace_.c_str()) + 1.0f;
            ctx.line.SetColor(t.textColor);
            ctx.line.Line2D(cx, y2, cx, y2 + ctx.font.GetFontSize());
        }
        ctx.font.SetColor(t.textDisabled);
        const char* hint = "Enter one, Ctrl+Enter all, Tab field";
        ctx.font.Print(hint, barX + barW - ctx.font.GetTextWidth(hint) - 8.0f, y2);
    }
    ctx.popClip();

    if (bar_ == Bar::GotoSymbol && !paletteMatches_.empty())
    {
        const ct::Vector<syntax::Symbol>& list = symbols();
        ct::Vector<ListRow> rowsOut;
        rowsOut.reserve(paletteMatches_.size());
        for (int index: paletteMatches_)
        {
            const syntax::Symbol& s = list[static_cast<size_t>(index)];
            snprintf(buffer, sizeof buffer, "%s  %d", syntax::symbolKindName(s.kind), s.line + 1);
            rowsOut.push_back({ s.name, String(buffer) });
        }
        paintList(ctx, abs, rowsOut, paletteSelected_, paletteFirst_, barX, barY + barH + 2.0f, paletteRect_, paletteRowHeight_);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Signature help
// ═════════════════════════════════════════════════════════════════════════════

void CodeEditor::setSignatureProvider(SignatureProvider provider)
{
    signatureProvider_ = std::move(provider);
    sigKey_ = SigKey();
    markDirty();
}

void CodeEditor::dismissSignature()
{
    if (!sigVisible_) return;
    sigDismissed_ = true;
    sigDismissedLine_ = sigLine_;
    sigDismissedCol_ = sigOpenCol_;
    sigVisible_ = false;
    markDirty();
}

// Walks the cursor's line up to the cursor with a stack of open brackets;
// the innermost "(" still open is the call, the commas after it at its
// depth count the parameter, the identifiers and dots before it name the
// callee. Strings are skipped.
bool CodeEditor::callAtCursor(String& callee, int& openCol, int& parameter) const
{
    if (cursor_.line < 0 || cursor_.line >= lineCount()) return false;
    const String& line = lineAt(cursor_.line);
    const size_t end = utf8ByteOffset(line, cursor_.col);
    struct Open
    {
        char bracket;
        int col;
        int commas;
    };
    Open stack[32];
    int depth = 0;
    for (size_t i = 0; i < end && i < line.size(); ++i)
    {
        const char c = line[i];
        if (c == '"' || c == '\'')
        {
            ++i;
            while (i < end && line[i] != c) i += line[i] == '\\' ? 2 : 1;
            continue;
        }
        if (c == '#') return false; // a comment: no call
        if (c == '(' || c == '[' || c == '{')
        {
            if (depth < 32) stack[depth] = { c, static_cast<int>(i), 0 };
            ++depth;
        }
        else if (c == ')' || c == ']' || c == '}')
        {
            if (depth > 0) --depth;
        }
        else if (c == ',' && depth > 0 && depth <= 32)
            ++stack[depth - 1].commas;
    }
    while (depth > 0 && depth <= 32 && stack[depth - 1].bracket != '(') --depth;
    if (depth <= 0 || depth > 32) return false;
    const Open& call = stack[depth - 1];
    int nameEnd = call.col;
    while (nameEnd > 0 && (line[static_cast<size_t>(nameEnd - 1)] == ' ' || line[static_cast<size_t>(nameEnd - 1)] == '\t')) --nameEnd;
    int nameStart = nameEnd;
    while (nameStart > 0 && (isWord(line[static_cast<size_t>(nameStart - 1)]) || line[static_cast<size_t>(nameStart - 1)] == '.')) --nameStart;
    if (nameStart == nameEnd) return false;
    callee = line.substr(static_cast<size_t>(nameStart), static_cast<size_t>(nameEnd - nameStart));
    if (callee[callee.size() - 1] == '.' || callee[0] == '.') return false;
    openCol = call.col;
    parameter = call.commas;
    return true;
}

void CodeEditor::updateSignature()
{
    String callee;
    int openCol = 0, parameter = 0;
    const bool wasVisible = sigVisible_;
    sigVisible_ = false;
    if (callAtCursor(callee, openCol, parameter))
    {
        if (sigDismissed_ && sigDismissedLine_ == cursor_.line && sigDismissedCol_ == openCol)
        {
            // Dismissed for this very call: it stays away.
        }
        else
        {
            sigDismissed_ = false;
            String signature, doc;
            bool found = signatureProvider_ && signatureProvider_(callee, signature, doc);
            if (!found)
            {
                // The text's own functions and methods: the last part of the name.
                size_t cut = callee.size();
                while (cut > 0 && callee[cut - 1] != '.') --cut;
                const String name = callee.substr(cut);
                const ct::Vector<syntax::Symbol>& list = symbols();
                for (size_t i = 0; i < list.size() && !found; ++i)
                    if (list[i].name == name && !list[i].signature.empty() &&
                            (list[i].kind == syntax::SymbolKind::Function || list[i].kind == syntax::SymbolKind::Method))
                    {
                        signature = name + list[i].signature;
                        found = true;
                    }
            }
            if (found)
            {
                sigVisible_ = true;
                sigCallee_ = callee;
                sigText_ = signature;
                sigDoc_ = doc;
                sigParam_ = parameter;
                sigOpenCol_ = openCol;
                sigLine_ = cursor_.line;
            }
        }
    }
    if (wasVisible != sigVisible_) markDirty();
}

void CodeEditor::paintSignature(PaintContext& ctx, const Rect& abs)
{
    const float lh = lineHeight();
    if (lh <= 0.0f || sigLine_ < 0 || sigLine_ >= lineCount()) return;
    const Theme& t = Theme::instance();
    const float rows = sigDoc_.empty() ? 1.0f : 2.0f;
    const float height = rows * lh + 6.0f;
    const float width = std::min(abs.w - 8.0f, std::max(ctx.font.GetTextWidth(sigText_.c_str()), ctx.font.GetTextWidth(sigDoc_.c_str())) + 16.0f);
    const String& line = lineAt(sigLine_);
    int col = 0;
    for (int i = 0; i < sigOpenCol_ && i < static_cast<int>(line.size()); ++i)
        if ((static_cast<unsigned char>(line[static_cast<size_t>(i)]) & 0xC0) != 0x80) ++col;
    const float lineY = abs.y + t.textEditPadding + logicalToVisualRow(sigLine_) * lh - scrollY_;
    float x = abs.x + gutterW_ + t.textEditPadding + colToPixelX(sigLine_, col) - scrollX_ - 6.0f;
    x = std::max(abs.x, std::min(x, abs.x + abs.w - width));
    float y = lineY - height - 2.0f;
    if (y < abs.y) y = lineY + lh + 2.0f;

    ctx.pushClip(abs);
    ctx.fill.SetColor(t.menuBg);
    ctx.fill.RoundedRectangle(x, y, width, height, 4, 6, true);
    ctx.line.SetColor(t.borderColor);
    ctx.line.RoundedRectangle(x, y, width, height, 4, 6, false);
    const float textY = y + 3.0f + (lh - ctx.font.GetFontSize()) * 0.5f;
    // The active parameter: the n-th comma-separated piece inside the parens.
    const size_t open = sigText_.find('(');
    if (open != String::npos)
    {
        size_t start = open + 1, pos = start;
        int index = 0, depth = 0;
        size_t pieceStart = start, pieceEnd = String::npos;
        for (; pos < sigText_.size(); ++pos)
        {
            const char c = sigText_[pos];
            if (c == '(' || c == '[') ++depth;
            else if ((c == ')' && depth == 0) || (c == ',' && depth == 0))
            {
                if (index == sigParam_)
                {
                    pieceEnd = pos;
                    break;
                }
                ++index;
                pieceStart = pos + 1;
                while (pieceStart < sigText_.size() && sigText_[pieceStart] == ' ') ++pieceStart;
                if (c == ')') break;
            }
            else if (c == ')' || c == ']') --depth;
        }
        if (pieceEnd != String::npos && pieceEnd > pieceStart)
        {
            const String before = sigText_.substr(0, pieceStart);
            const String piece = sigText_.substr(pieceStart, pieceEnd - pieceStart);
            const float px = x + 8.0f + ctx.font.GetTextWidth(before.c_str());
            ctx.fill.SetColor(t.selectionColor);
            ctx.fill.Rectangle(px - 1.0f, y + 3.0f, ctx.font.GetTextWidth(piece.c_str()) + 2.0f, lh, true);
        }
    }
    ctx.font.SetColor(t.textColor);
    ctx.font.Print(sigText_.c_str(), x + 8.0f, textY);
    if (!sigDoc_.empty())
    {
        ctx.font.SetColor(t.textDisabled);
        ctx.font.Print(sigDoc_.c_str(), x + 8.0f, textY + lh);
    }
    ctx.popClip();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Formatting
// ═════════════════════════════════════════════════════════════════════════════

namespace
{

// Columns of indentation, a tab counting to the next multiple of 4.
int measureIndent(const String& line, size_t& firstChar)
{
    int columns = 0;
    size_t i = 0;
    for (; i < line.size(); ++i)
    {
        if (line[i] == ' ') ++columns;
        else if (line[i] == '\t') columns += 4 - columns % 4;
        else break;
    }
    firstChar = i;
    return columns;
}

String indentString(int depth, int unit)
{
    String s;
    if (unit > 0) s.resize(static_cast<size_t>(depth * unit), ' ');
    else s.resize(static_cast<size_t>(depth), '\t');
    return s;
}

String trimmedRight(const String& line, size_t from)
{
    size_t end = line.size();
    while (end > from && (line[end - 1] == ' ' || line[end - 1] == '\t' || line[end - 1] == '\r')) --end;
    return line.substr(from, end - from);
}

// The brace depth change a line makes, outside strings, char literals and
// comments; `inComment` carries a block comment over the lines. `leading`
// is how many closing braces open the line.
int braceDelta(const String& line, bool& inComment, int& leading)
{
    int delta = 0;
    leading = 0;
    bool countingLeading = true;
    const size_t n = line.size();
    size_t i = 0;
    while (i < n)
    {
        const char c = line[i];
        if (inComment)
        {
            const size_t close = line.find("*/", i);
            if (close == String::npos) return delta;
            inComment = false;
            i = close + 2;
            continue;
        }
        if (c == ' ' || c == '\t')
        {
            ++i;
            continue;
        }
        if (c == '/' && i + 1 < n && line[i + 1] == '/') return delta;
        if (c == '/' && i + 1 < n && line[i + 1] == '*')
        {
            inComment = true;
            i += 2;
            countingLeading = false;
            continue;
        }
        if (c == '"' || c == '\'' || c == '`')
        {
            ++i;
            while (i < n && line[i] != c) i += line[i] == '\\' ? 2 : 1;
            ++i;
            countingLeading = false;
            continue;
        }
        if (c == '{') ++delta;
        else if (c == '}')
        {
            --delta;
            if (countingLeading) ++leading;
        }
        if (c != '}') countingLeading = false;
        ++i;
    }
    return delta;
}

} // namespace

String CodeEditor::formattedText() const
{
    const syntax::SymbolSyntax style = highlighter_ ? highlighter_->symbolSyntax() : syntax::SymbolSyntax::None;
    ct::Vector<String> out;
    out.reserve(lines_.size());
    ct::Vector<int> levels; // Indent: the indentation each open block was written with
    int depth = 0;
    bool inComment = false;
    int blankRun = 0;
    for (const String& line: lines_)
    {
        size_t first = 0;
        const int indent = measureIndent(line, first);
        const String body = trimmedRight(line, first);
        if (body.empty())
        {
            if (!out.empty() && blankRun == 0) out.push_back(String());
            ++blankRun;
            continue;
        }
        blankRun = 0;
        if (style == syntax::SymbolSyntax::Indent)
        {
            while (!levels.empty() && levels.back() > indent) levels.pop_back();
            if (levels.empty() ? indent > 0 : indent > levels.back()) levels.push_back(indent);
            out.push_back(indentString(static_cast<int>(levels.size()), indentUnit_) + body);
        }
        else if (style == syntax::SymbolSyntax::Braces)
        {
            const bool wasInComment = inComment;
            int leading = 0;
            const int delta = braceDelta(body, inComment, leading);
            if (wasInComment || body[0] == '#')
                out.push_back(trimmedRight(line, 0)); // a comment's or a directive's own layout
            else
                out.push_back(indentString(std::max(0, depth - leading), indentUnit_) + body);
            depth = std::max(0, depth + delta);
        }
        else
            out.push_back(trimmedRight(line, 0));
    }
    // Blank lines at the end go; one final newline stays.
    while (!out.empty() && out.back().empty()) out.pop_back();
    String text;
    for (const String& line: out)
    {
        text += line;
        text += '\n';
    }
    return text;
}

void CodeEditor::formatDocument()
{
    if (readOnly_) return;
    const String after = formattedText();
    const int keepLine = cursor_.line;
    const int last = lineCount() - 1;
    replaceRange({ 0, 0 }, { last, lineColCount(last) }, after);
    cursor_ = { std::min(keepLine, lineCount() - 1), 0 };
    clampPos(cursor_);
    selAnchor_ = cursor_;
    ensureCursorVisible();
    cursorMoved.emit(cursor_);
}

// ═════════════════════════════════════════════════════════════════════════════
//  The keyboard
// ═════════════════════════════════════════════════════════════════════════════

ig::Span<const CodeEditor::Shortcut> CodeEditor::shortcuts()
{
    static const Shortcut table[] = {
        { "Ctrl+Z / Ctrl+Y", "Undo / redo (a replace-all or a block indent is one step)" },
        { "Ctrl+X / Ctrl+C / Ctrl+V", "Cut, copy, paste; with no selection, the line" },
        { "Ctrl+A", "Select all" },
        { "Ctrl+L", "Select the line; again, the next one too" },
        { "Ctrl+D", "Select the word; again, add the next occurrence as a cursor" },
        { "Ctrl+Shift+L", "A cursor on every occurrence of the selection" },
        { "Ctrl+Alt+Up / Down", "A cursor on the line above / below" },
        { "Alt+click", "A cursor there" },
        { "Escape", "Back to one cursor; close a bar or the completion" },
        { "Alt+Up / Alt+Down", "Move the line up / down" },
        { "Ctrl+Shift+D", "Duplicate the line" },
        { "Ctrl+Shift+K", "Delete the line" },
        { "Ctrl+J", "Join the next line onto this one" },
        { "Ctrl+/", "Comment or uncomment the line" },
        { "Tab / Shift+Tab", "Indent / unindent; a selection over lines moves them all" },
        { "Ctrl+U / Ctrl+Shift+U", "Selection to lower / upper case" },
        { "Ctrl+M / Ctrl+Shift+M", "Jump to the matching bracket / select to it" },
        { "Ctrl+Space", "Completion; '.' opens it too. Tab or Enter accept" },
        { "Ctrl+Shift+F", "Format the document: indentation by levels, no trailing spaces" },
        { "Ctrl+F / F3 / Shift+F3", "Find as you type; next / previous match" },
        { "Ctrl+H", "Replace: Enter one, Ctrl+Enter all, Tab switches field" },
        { "Alt+C / Alt+W / Alt+R", "In the find bar: match case, whole word, regex" },
        { "Ctrl+G", "Go to line" },
        { "Ctrl+Shift+O", "Go to symbol: type to filter, Enter reveals" },
        { "Ctrl+B", "Go to the definition of the word at the cursor" },
        { "Alt+Left / Alt+Right", "Back to where the cursor was / forward again" },
        { "Ctrl+F2 / F2 / Shift+F2", "Toggle a bookmark; next / previous bookmark" },
        { "Ctrl+Home / Ctrl+End", "Start / end of the text" },
        { "Ctrl+Left / Ctrl+Right", "By words" },
        { "Ctrl+wheel", "Zoom" },
    };
    return ig::Span<const Shortcut>(table, sizeof table / sizeof table[0]);
}
