#include "pch.hpp"
#include "TextInputWidgets.hpp"
#include "WidgetApp.hpp"
#include "Utf8.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  TextInput - single-line text input
// ─────────────────────────────────────────────────────────────────────────────

TextInput::TextInput(const String& text, Mode mode)
    : text_(text), mode_(mode)
{
    acceptsFocus_ = true;
    setCursor(CursorType::IBeam);
    cursor_ = utf8Length(text_);
    selStart_ = selEnd_ = cursor_;
}

void TextInput::setText(const String& t)
{
    if (text_ == t) return;
    text_ = t;
    int len = utf8Length(text_);
    if (cursor_ > len) cursor_ = len;
    selStart_ = selEnd_ = cursor_;
    scrollX_ = 0;
    markDirty();
    textChanged.emit(text_);
}

void TextInput::selectAll()
{
    selStart_ = 0;
    selEnd_   = utf8Length(text_);
    cursor_   = selEnd_;
    markDirty();
}

void TextInput::clearSelection()
{
    selStart_ = selEnd_ = cursor_;
    markDirty();
}

String TextInput::selectedText() const
{
    if (selStart_ == selEnd_) return {};
    int lo = std::min(selStart_, selEnd_);
    int hi = std::max(selStart_, selEnd_);
    return utf8Substr(text_, lo, hi);
}

void TextInput::setCursorPos(int pos)
{
    int len = utf8Length(text_);
    cursor_ = std::max(0, std::min(pos, len));
    selStart_ = selEnd_ = cursor_;
    markDirty();
}

Widget::Vec2f TextInput::sizeHint() const
{
    const auto& t = Theme::instance();
    return {120.0f, t.inputHeight};
}

String TextInput::displayText() const
{
    if (mode_ == Mode::Password)
    {
        int len = utf8Length(text_);
        String result;
        result.reserve(static_cast<size_t>(len) * 3);
        for (int i = 0; i < len; ++i)
            result += "\xe2\x80\xa2";  // UTF-8 bullet •
        return result;
    }
    return text_;
}

int TextInput::hitTestChar(const ig::retained::Font* font, float localX) const
{
    auto& wa = WidgetApp::instance();
    String disp = displayText();
    int len = utf8Length(text_);
    float pad = Theme::instance().padding;
    float x = pad - scrollX_;

    for (int i = 0; i < len; ++i)
    {
        String ch = utf8Substr(disp, i, i + 1);
        float cw = wa.textWidth(ch.c_str(), font);
        if (localX < x + cw * 0.5f) return i;
        x += cw;
    }
    return len;
}

float TextInput::cursorXOffset(const ig::retained::Font* font, int pos) const
{
    String disp = displayText();
    String sub = utf8Substr(disp, 0, pos);
    return WidgetApp::instance().textWidth(sub.c_str(), font);
}

void TextInput::deleteSelection()
{
    if (selStart_ == selEnd_) return;
    int lo = std::min(selStart_, selEnd_);
    int hi = std::max(selStart_, selEnd_);
    size_t byteStart = utf8ByteOffset(text_, lo);
    size_t byteEnd   = utf8ByteOffset(text_, hi);
    text_.erase(byteStart, byteEnd - byteStart);
    cursor_   = lo;
    selStart_ = selEnd_ = cursor_;
}

void TextInput::insertText(const String& t)
{
    if (mode_ == Mode::ReadOnly) return;
    if (mode_ == Mode::NumberOnly)
        for (char c : t) if (!isNumberChar(c)) return;

    deleteSelection();
    size_t bytePos = utf8ByteOffset(text_, cursor_);
    text_.insert(bytePos, t);
    cursor_ += utf8Length(t);
    selStart_ = selEnd_ = cursor_;
    markDirty();
    textChanged.emit(text_);
}

void TextInput::ensureCursorVisible(const ig::retained::Font* font)
{
    if (!font) return;
    float pad    = Theme::instance().padding;
    float fieldW = rect_.w - pad * 2;
    float cx     = cursorXOffset(font, cursor_);
    if (cx - scrollX_ < 0)        scrollX_ = cx;
    else if (cx - scrollX_ > fieldW) scrollX_ = cx - fieldW;
}

void TextInput::clampCursor()
{
    int len = utf8Length(text_);
    if (cursor_ < 0)   cursor_ = 0;
    if (cursor_ > len) cursor_ = len;
}

bool TextInput::isNumberChar(char c) const
{
    return (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+';
}

void TextInput::paint(PaintContext& ctx)
{
    if (!visible_) return;
    const auto& t = Theme::instance();
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;
    Rect clipped;
    float pad = t.padding;

    blinkTimer_ += WidgetApp::instance().deltaTime();
    if (blinkTimer_ > 1.0f) blinkTimer_ -= 1.0f;

    const bool enabled = isEnabledInTree();

    // Background
    Color bg = !enabled ? t.panelColor : ((isFocused() || isHovered()) ? t.inputBgHover : t.inputBg);
    if (ctx.clipRectIntersect(abs, clipped))
    {
        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fill.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                   static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                   t.borderRadius, 6, true);
    }

    // Border
    Color bc = isFocused() ? t.focusColor : ((enabled && isHovered()) ? t.inputBorderHover : t.inputBorder);
    if (ctx.clipRectIntersect(abs, clipped))
    {
        ctx.line.SetColor(bc.r, bc.g, bc.b, bc.a);
        ctx.line.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                   static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                   t.borderRadius, 6, false);
    }

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);

    if (overrideFont_) ctx.pushFont(overrideFont_);

    const ig::retained::Font* font = ctx.buFont;
    ensureCursorVisible(font);

    Rect textArea = {abs.x + pad, abs.y, abs.w - pad * 2, abs.h};
    float textX   = textArea.x - scrollX_;
    float asc     = ctx.font.GetAscender();
    float textY   = abs.y + (abs.h + asc) * 0.5f;

    ctx.pushClip(textArea);
    if (text_.empty() && !isFocused() && !placeholder_.empty())
    {
        Color pc = t.textDisabled;
        pc.a = 128;
        ctx.font.SetColor(pc);
        ctx.font.Print(placeholder_.c_str(), textX, textY);
    }
    else
    {
        // Selection highlight
        if (hasSelection() && isFocused() && font)
        {
            int lo  = std::min(selStart_, selEnd_);
            int hi  = std::max(selStart_, selEnd_);
            float sx1 = textArea.x + cursorXOffset(font, lo) - scrollX_;
            float sx2 = textArea.x + cursorXOffset(font, hi) - scrollX_;
            float cx1 = std::max(sx1, textArea.x);
            float cx2 = std::min(sx2, textArea.x + textArea.w);
            if (cx1 < cx2)
            {
                ctx.fill.SetColor(t.selectionColor.r, t.selectionColor.g,
                                  t.selectionColor.b, t.selectionColor.a);
                ctx.fill.Rectangle(static_cast<int>(cx1), static_cast<int>(abs.y + 2),
                                    static_cast<int>(cx2 - cx1), static_cast<int>(abs.h - 4), true);
            }
        }

        // Text
        ctx.font.SetColor(enabled ? t.textColor : t.textDisabled);
        ctx.font.Print(displayText().c_str(), textX, textY);

        // Cursor
        if (isFocused() && blinkTimer_ < 0.5f && mode_ != Mode::ReadOnly && font)
        {
            float cx = textArea.x + cursorXOffset(font, cursor_) - scrollX_;
            if (cx >= textArea.x && cx <= textArea.x + textArea.w)
            {
                ctx.line.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
                ctx.line.Line2D(static_cast<int>(cx), static_cast<int>(abs.y + 3),
                                static_cast<int>(cx), static_cast<int>(abs.y + abs.h - 3));
            }
        }
    }
    ctx.popClip();
    if (overrideFont_) ctx.popFont();
}

void TextInput::onMousePress(MouseEvent& e)
{
    if (!isEnabledInTree()) return;
    const ig::retained::Font* font = overrideFont_ ? overrideFont_ : WidgetApp::instance().font();
    cursor_   = hitTestChar(font, e.localX);
    selStart_ = selEnd_ = cursor_;
    dragging_ = true;
    blinkTimer_ = 0;
    markDirty();
    e.consumed = true;
}

void TextInput::onMouseRelease(MouseEvent& e)
{
    if (dragging_) { dragging_ = false; e.consumed = true; }
}

void TextInput::onMouseMove(MouseEvent& e)
{
    if (!isEnabledInTree()) return;
    if (dragging_)
    {
        const ig::retained::Font* font =
                overrideFont_ ? overrideFont_ : WidgetApp::instance().font();
        cursor_ = hitTestChar(font, e.localX);
        selEnd_ = cursor_;
        blinkTimer_ = 0;
        markDirty();
        e.consumed = true;
    }
}

void TextInput::onKeyPress(KeyEvent& e)
{
    if (!isEnabledInTree()) return;
    if (mode_ == Mode::ReadOnly && !(e.ctrl && e.key == ig::retained::Key::C)) return;

    int len = utf8Length(text_);
    blinkTimer_ = 0;

    if (e.ctrl)
    {
        switch (e.key)
        {
        case ig::retained::Key::A: selectAll(); e.consumed = true; return;
        case ig::retained::Key::C:
        {
            String sel = selectedText();
            if (!sel.empty()) WidgetApp::instance().setClipboardText(sel.c_str());
            e.consumed = true; return;
        }
        case ig::retained::Key::X:
        {
            if (mode_ == Mode::ReadOnly) { e.consumed = true; return; }
            String sel = selectedText();
            if (!sel.empty()) { WidgetApp::instance().setClipboardText(sel.c_str()); deleteSelection(); markDirty(); textChanged.emit(text_); }
            e.consumed = true; return;
        }
        case ig::retained::Key::V:
        {
            if (mode_ == Mode::ReadOnly) { e.consumed = true; return; }
            String clip = WidgetApp::instance().getClipboardText();
            if (!clip.empty()) {
                clip.erase(std::remove(clip.begin(), clip.end(), '\n'), clip.end());
                clip.erase(std::remove(clip.begin(), clip.end(), '\r'), clip.end());
                insertText(clip);
            }
            e.consumed = true; return;
        }
        default: break;
        }
    }

    switch (e.key)
    {
    case ig::retained::Key::Left:
        if (cursor_ > 0) {
            if (e.shift) { cursor_--; selEnd_ = cursor_; }
            else { cursor_ = hasSelection() ? std::min(selStart_, selEnd_) : cursor_ - 1; selStart_ = selEnd_ = cursor_; }
        } else if (!e.shift) selStart_ = selEnd_ = cursor_;
        markDirty(); e.consumed = true; break;

    case ig::retained::Key::Right:
        if (cursor_ < len) {
            if (e.shift) { cursor_++; selEnd_ = cursor_; }
            else { cursor_ = hasSelection() ? std::max(selStart_, selEnd_) : cursor_ + 1; selStart_ = selEnd_ = cursor_; }
        } else if (!e.shift) selStart_ = selEnd_ = cursor_;
        markDirty(); e.consumed = true; break;

    case ig::retained::Key::Home:
        cursor_ = 0;
        if (e.shift) selEnd_ = cursor_; else selStart_ = selEnd_ = cursor_;
        markDirty(); e.consumed = true; break;

    case ig::retained::Key::End:
        cursor_ = len;
        if (e.shift) selEnd_ = cursor_; else selStart_ = selEnd_ = cursor_;
        markDirty(); e.consumed = true; break;

    case ig::retained::Key::Backspace:
        if (mode_ == Mode::ReadOnly) { e.consumed = true; break; }
        if (hasSelection()) { deleteSelection(); }
        else if (cursor_ > 0) {
            size_t byteEnd   = utf8ByteOffset(text_, cursor_);
            cursor_--;
            size_t byteStart = utf8ByteOffset(text_, cursor_);
            text_.erase(byteStart, byteEnd - byteStart);
            selStart_ = selEnd_ = cursor_;
        }
        markDirty(); textChanged.emit(text_); e.consumed = true; break;

    case ig::retained::Key::Delete:
        if (mode_ == Mode::ReadOnly) { e.consumed = true; break; }
        if (hasSelection()) { deleteSelection(); }
        else if (cursor_ < len) {
            size_t byteStart = utf8ByteOffset(text_, cursor_);
            size_t byteEnd   = utf8ByteOffset(text_, cursor_ + 1);
            text_.erase(byteStart, byteEnd - byteStart);
            selStart_ = selEnd_ = cursor_;
        }
        markDirty(); textChanged.emit(text_); e.consumed = true; break;

    case ig::retained::Key::Return:
    case ig::retained::Key::KPEnter:
        submitted.emit(text_); e.consumed = true; break;

    case ig::retained::Key::Escape:
        clearSelection(); break;

    default: break;
    }
}

void TextInput::onTextInput(KeyEvent& e)
{
    if (!isEnabledInTree()) return;
    if (mode_ == Mode::ReadOnly) { e.consumed = true; return; }
    String t(e.text);
    if (t.empty()) return;
    insertText(t);
    blinkTimer_ = 0;
    e.consumed = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  TextEdit - multi-line text editor
// ─────────────────────────────────────────────────────────────────────────────

const String TextEdit::emptyLine_;

TextEdit::TextEdit(const String& text)
{
    acceptsFocus_ = true;
    setCursor(CursorType::IBeam);
    setText(text);
}

// ── Gutter markers ───────────────────────────────────────────────────────────

void TextEdit::setMarker(int line, MarkerType type)
{
    markers_[line].insert(static_cast<int>(type));
    markDirty();
}

void TextEdit::clearMarker(int line)
{
    markers_.erase(line);
    markDirty();
}

void TextEdit::clearMarker(int line, MarkerType type)
{
    ct::HashSet<int>* lineMarkers = markers_.find(line);
    if (lineMarkers) {
        lineMarkers->erase(static_cast<int>(type));
        if (lineMarkers->empty()) markers_.erase(line);
    }
    markDirty();
}

void TextEdit::clearAllMarkers() { markers_.clear(); markDirty(); }

bool TextEdit::hasMarker(int line) const
{
    return markers_.contains(line);
}

bool TextEdit::hasMarker(int line, MarkerType type) const
{
    const ct::HashSet<int>* lineMarkers = markers_.find(line);
    return lineMarkers && lineMarkers->contains(static_cast<int>(type));
}

void TextEdit::setText(const String& text)
{
    lines_.clear();
    String line;
    for (char c : text)
    {
        if (c == '\n')   { lines_.push_back(std::move(line)); line.clear(); }
        else if (c != '\r') line += c;
    }
    lines_.push_back(std::move(line));
    cursor_    = {0, 0};
    selAnchor_ = cursor_;
    scrollX_ = scrollY_ = 0;
    wrapRows_.clear();
    wrapRows_.resize(lines_.size());
    for (int i = 0; i < static_cast<int>(lines_.size()); ++i)
        wrapRows_[i].push_back({ 0, lineColCount(i) });
    rowStartsDirty_ = true;
    wrapDirty_ = true;
    markDirty();
}

String TextEdit::text() const
{
    String result;
    for (size_t i = 0; i < lines_.size(); ++i)
    {
        if (i > 0) result += '\n';
        result += lines_[i];
    }
    return result;
}

const String& TextEdit::lineAt(int n) const
{
    if (n < 0 || n >= static_cast<int>(lines_.size())) return emptyLine_;
    return lines_[n];
}

void TextEdit::setCursorPos(TextPos pos) { clampPos(pos); cursor_ = pos; selAnchor_ = cursor_; markDirty(); cursorMoved.emit(cursor_); }
void TextEdit::setCursorPos(int line, int col) { setCursorPos({line, col}); }

void TextEdit::selectAll()
{
    selAnchor_ = {0, 0};
    int last = static_cast<int>(lines_.size()) - 1;
    cursor_ = {last, lineColCount(last)};
    markDirty();
}

void TextEdit::selectLine(int n)
{
    if (n < 0 || n >= static_cast<int>(lines_.size())) return;
    selAnchor_ = {n, 0};
    cursor_    = {n, lineColCount(n)};
    markDirty();
}

void TextEdit::setSelection(TextPos start, TextPos end)
{
    clampPos(start);
    clampPos(end);
    selAnchor_ = start;
    cursor_    = end;
    markDirty();
}

void TextEdit::copy()
{
    String sel = selectedText();
    if (!sel.empty())
        WidgetApp::instance().setClipboardText(sel.c_str());
}

void TextEdit::cut()
{
    if (readOnly_) return;
    String sel = selectedText();
    if (!sel.empty()) {
        WidgetApp::instance().setClipboardText(sel.c_str());
        deleteSelection();
        markDirty();
        textChanged.emit();
        cursorMoved.emit(cursor_);
    }
}

void TextEdit::paste()
{
    if (readOnly_) return;
    String clip = WidgetApp::instance().getClipboardText();
    if (!clip.empty())
        insertTextAtCursor(clip);
}

void TextEdit::insertTextAt(TextPos pos, const String& text)
{
    if (readOnly_) return;
    clampPos(pos);
    // Save cursor, move to pos, insert, restore cursor
    TextPos savedCursor = cursor_;
    TextPos savedAnchor = selAnchor_;
    cursor_ = pos;
    selAnchor_ = pos;
    insertTextAtCursor(text);
    // Adjust saved cursor if it was after the insertion point
    TextPos newCur = cursor_;
    cursor_ = savedCursor;
    selAnchor_ = savedAnchor;
    // If original cursor was on or after insertion, shift it
    if (savedCursor > pos || savedCursor == pos) {
        // Simple case: if insert was single line and same line
        // For now, just leave cursor where it was — callers can adjust
    }
    markDirty();
}

String TextEdit::selectedText() const
{
    if (!hasSelection()) return {};
    TextPos s = selectionStart(), e = selectionEnd();
    if (s.line == e.line) return utf8Substr(lines_[s.line], s.col, e.col);
    String result = utf8Substr(lines_[s.line], s.col, lineColCount(s.line));
    result += '\n';
    for (int i = s.line + 1; i < e.line; ++i) { result += lines_[i]; result += '\n'; }
    result += utf8Substr(lines_[e.line], 0, e.col);
    return result;
}

float TextEdit::lineHeight() const
{
    const auto& t = Theme::instance();
    return (t.fontSize + t.lineSpacing) * fontScale_;
}

int TextEdit::firstVisibleLine() const
{
    if (lineHeight() <= 0) return 0;
    return static_cast<int>(scrollY_ / lineHeight());
}

int TextEdit::visibleLineCount() const
{
    if (lineHeight() <= 0) return 0;
    return static_cast<int>(rect_.h / lineHeight()) + 2;
}

void TextEdit::scrollToLine(int n)
{
    if (lineHeight() <= 0) return;
    scrollY_ = n * lineHeight();
    float maxY = std::max(0.0f, static_cast<int>(lines_.size()) * lineHeight() - rect_.h);
    scrollY_ = std::max(0.0f, std::min(scrollY_, maxY));
    markDirty();
}

// ── Helpers ──────────────────────────────────────────────────────────────────

String TextEdit::expandTabs(const String& text) const
{
    return expandTabs(text, 0);
}

String TextEdit::expandTabs(const String& text, int startVisCol) const
{
    String out;
    out.reserve(text.size());
    int visCol = startVisCol;
    for (size_t i = 0; i < text.size(); ) {
        if (text[i] == '\t') {
            int spaces = tabSize_ - (visCol % tabSize_);
            out.append(spaces, ' ');
            visCol += spaces;
            i++;
        } else {
            int seqLen = utf8CharLen(text.c_str() + i);
            for (int k = 0; k < seqLen && i + k < text.size(); k++)
                out += text[i + k];
            visCol++;
            i += seqLen;
        }
    }
    return out;
}

String TextEdit::expandTabsVisible(const String& text, int startVisCol) const
{
    // Like expandTabs but replaces spaces with · and tab first pos with ·
    String out;
    out.reserve(text.size() * 2);
    int visCol = startVisCol;
    for (size_t i = 0; i < text.size(); ) {
        if (text[i] == '\t') {
            int spaces = tabSize_ - (visCol % tabSize_);
            // First position: ·, rest: normal spaces
            out += "\xC2\xB7";  // · U+00B7
            if (spaces > 1) out.append(spaces - 1, ' ');
            visCol += spaces;
            i++;
        } else if (text[i] == ' ') {
            out += "\xC2\xB7";  // · U+00B7
            visCol++;
            i++;
        } else {
            int seqLen = utf8CharLen(text.c_str() + i);
            for (int k = 0; k < seqLen && i + k < text.size(); k++)
                out += text[i + k];
            visCol++;
            i += seqLen;
        }
    }
    return out;
}

int TextEdit::lineColCount(int line) const
{
    if (line < 0 || line >= static_cast<int>(lines_.size())) return 0;
    return utf8Length(lines_[line]);
}

int TextEdit::visColAt(int line, int col) const
{
    if (line < 0 || line >= static_cast<int>(lines_.size())) return 0;
    int visCol = 0;
    int len = std::min(col, lineColCount(line));
    for (int i = 0; i < len; ++i)
    {
        String ch = utf8Substr(lines_[line], i, i + 1);
        if (ch[0] == '\t') visCol += tabSize_ - (visCol % tabSize_);
        else visCol++;
    }
    return visCol;
}

void TextEdit::clampPos(TextPos& p) const
{
    int lc = static_cast<int>(lines_.size());
    p.line = std::max(0, std::min(p.line, lc - 1));
    p.col  = std::max(0, std::min(p.col, lineColCount(p.line)));
}

float TextEdit::colToX(PaintContext& ctx, int line, int col) const
{
    if (line < 0 || line >= static_cast<int>(lines_.size())) return 0;
    String sub = expandTabs(utf8Substr(lines_[line], 0, col));
    return ctx.font.GetTextWidth(sub.c_str());
}

int TextEdit::xToCol(PaintContext& ctx, int line, float x) const
{
    if (line < 0 || line >= static_cast<int>(lines_.size())) return 0;
    int len = lineColCount(line);
    float cx = 0;
    int visCol = 0;
    for (int i = 0; i < len; ++i)
    {
        String ch = utf8Substr(lines_[line], i, i + 1);
        float cw;
        if (ch[0] == '\t') {
            int spaces = tabSize_ - (visCol % tabSize_);
            cw = spaces * ctx.font.GetTextWidth(" ");
            visCol += spaces;
        } else {
            cw = ctx.font.GetTextWidth(ch.c_str());
            visCol++;
        }
        if (x < cx + cw * 0.5f) return i;
        cx += cw;
    }
    return len;
}

TextEdit::TextPos TextEdit::hitTestMouse(float localX, float localY) const
{
    auto& wa = WidgetApp::instance();
    if (lineHeight() <= 0) return { 0, 0 };
    int vRow = static_cast<int>((localY + scrollY_) / lineHeight());
    int tvr  = totalVisualRows();
    vRow = std::max(0, std::min(vRow, tvr - 1));
    if (vRow < 0) return {0, 0};

    auto vp     = visualToLogical(vRow);
    int  line   = vp.line;
    int  rowIdx = vp.row;
    if (line < 0 || line >= static_cast<int>(lines_.size())) return {0, 0};
    auto& rows = wrapRows_[line];
    if (rowIdx < 0 || rowIdx >= static_cast<int>(rows.size())) return {line, 0};

    int startCol = rows[rowIdx].startCol;
    int endCol   = rows[rowIdx].endCol;
    float textX  = localX - gutterW_ + scrollX_ - Theme::instance().textEditPadding;
    float cx = 0;
    int visCol = 0;
    // Compute visual col of startCol
    for (int k = 0; k < startCol; k++) {
        String ch = utf8Substr(lines_[line], k, k + 1);
        if (ch[0] == '\t') visCol += tabSize_ - (visCol % tabSize_);
        else visCol++;
    }
    for (int i = 0; i < endCol - startCol; ++i)
    {
        String ch = utf8Substr(lines_[line], startCol + i, startCol + i + 1);
        float cw;
        if (ch[0] == '\t') {
            int spaces = tabSize_ - (visCol % tabSize_);
            cw = spaces * wa.textWidth(" ", overrideFont_, fontScale_);
            visCol += spaces;
        } else {
            cw = wa.textWidth(ch.c_str(), overrideFont_, fontScale_);
            visCol++;
        }
        if (textX < cx + cw * 0.5f) return {line, startCol + i};
        cx += cw;
    }
    return {line, endCol};
}

TextEdit::TextPos TextEdit::hitTest(PaintContext& ctx, float localX, float localY) const
{
    int vRow = static_cast<int>((localY + scrollY_) / lineHeight());
    int tvr  = totalVisualRows();
    vRow = std::max(0, std::min(vRow, tvr - 1));
    if (vRow < 0) return {0, 0};

    auto vp     = visualToLogical(vRow);
    int  line   = vp.line;
    int  rowIdx = vp.row;
    if (line < 0 || line >= static_cast<int>(lines_.size())) return {0, 0};
    auto& rows = wrapRows_[line];
    if (rowIdx < 0 || rowIdx >= static_cast<int>(rows.size())) return {line, 0};

    int startCol = rows[rowIdx].startCol;
    int endCol   = rows[rowIdx].endCol;
    float textX  = localX - gutterW_ + scrollX_ - Theme::instance().textEditPadding;
    float cx = 0;
    int visCol = 0;
    for (int k = 0; k < startCol; k++) {
        String ch = utf8Substr(lines_[line], k, k + 1);
        if (ch[0] == '\t') visCol += tabSize_ - (visCol % tabSize_);
        else visCol++;
    }
    for (int i = 0; i < endCol - startCol; ++i)
    {
        String ch = utf8Substr(lines_[line], startCol + i, startCol + i + 1);
        float cw;
        if (ch[0] == '\t') {
            int spaces = tabSize_ - (visCol % tabSize_);
            cw = spaces * ctx.font.GetTextWidth(" ");
            visCol += spaces;
        } else {
            cw = ctx.font.GetTextWidth(ch.c_str());
            visCol++;
        }
        if (textX < cx + cw * 0.5f) return {line, startCol + i};
        cx += cw;
    }
    return {line, endCol};
}

void TextEdit::deleteSelection()
{
    if (!hasSelection()) return;
    TextPos s = selectionStart(), e = selectionEnd();
    if (s.line == e.line)
    {
        size_t bs = utf8ByteOffset(lines_[s.line], s.col);
        size_t be = utf8ByteOffset(lines_[s.line], e.col);
        lines_[s.line].erase(bs, be - bs);
    }
    else
    {
        String head = utf8Substr(lines_[s.line], 0, s.col);
        String tail = utf8Substr(lines_[e.line], e.col, lineColCount(e.line));
        lines_[s.line] = head + tail;
        lines_.erase(lines_.begin() + s.line + 1, lines_.begin() + e.line + 1);
    }
    cursor_ = s; selAnchor_ = cursor_;
    linesChanged(s.line, e.line - s.line + 1, 1);
}

void TextEdit::insertTextAtCursor(const String& t)
{
    if (readOnly_) return;
    deleteSelection();
    ct::Vector<String> insertLines;
    String line;
    for (char c : t) {
        if (c == '\n') { insertLines.push_back(std::move(line)); line.clear(); }
        else if (c != '\r') line += c;
    }
    insertLines.push_back(std::move(line));
    const int firstLine = cursor_.line;
    if (insertLines.size() == 1)
    {
        size_t bytePos = utf8ByteOffset(lines_[cursor_.line], cursor_.col);
        lines_[cursor_.line].insert(bytePos, insertLines[0]);
        cursor_.col += utf8Length(insertLines[0]);
    }
    else
    {
        String tail = utf8Substr(lines_[cursor_.line], cursor_.col, lineColCount(cursor_.line));
        lines_[cursor_.line] = utf8Substr(lines_[cursor_.line], 0, cursor_.col) + insertLines[0];
        for (size_t i = 1; i < insertLines.size() - 1; ++i)
            lines_.insert(lines_.begin() + cursor_.line + static_cast<int>(i), insertLines[i]);
        int lastIdx = cursor_.line + static_cast<int>(insertLines.size()) - 1;
        lines_.insert(lines_.begin() + lastIdx, insertLines.back() + tail);
        cursor_.line = lastIdx;
        cursor_.col  = utf8Length(insertLines.back());
    }
    selAnchor_ = cursor_;
    linesChanged(firstLine, 1, static_cast<int>(insertLines.size()));
    markDirty();
    textChanged.emit();
    cursorMoved.emit(cursor_);
}

void TextEdit::splitLine()
{
    if (readOnly_) return;
    deleteSelection();
    String tail = utf8Substr(lines_[cursor_.line], cursor_.col, lineColCount(cursor_.line));
    lines_[cursor_.line] = utf8Substr(lines_[cursor_.line], 0, cursor_.col);
    lines_.insert(lines_.begin() + cursor_.line + 1, tail);
    cursor_.line++;
    cursor_.col = 0;
    selAnchor_ = cursor_;
    linesChanged(cursor_.line - 1, 1, 2);
    markDirty();
    textChanged.emit();
    cursorMoved.emit(cursor_);
}

void TextEdit::mergeWithPrevLine()
{
    if (cursor_.line <= 0) return;
    int prevCol = lineColCount(cursor_.line - 1);
    lines_[cursor_.line - 1] += lines_[cursor_.line];
    lines_.erase(lines_.begin() + cursor_.line);
    cursor_.line--;
    cursor_.col = prevCol;
    selAnchor_ = cursor_;
    linesChanged(cursor_.line, 2, 1);
}

void TextEdit::mergeWithNextLine()
{
    if (cursor_.line >= static_cast<int>(lines_.size()) - 1) return;
    lines_[cursor_.line] += lines_[cursor_.line + 1];
    lines_.erase(lines_.begin() + cursor_.line + 1);
    linesChanged(cursor_.line, 2, 1);
}

float TextEdit::computeGutterWidth(const ig::retained::Font& font) const
{
    if (!showLineNumbers_) return 0;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", std::max(static_cast<int>(lines_.size()), 99));
    return WidgetApp::instance().textWidth(buf, &font) + Theme::instance().gutterPadding * 2;
}

float TextEdit::computeLineHeight(const ig::retained::Font& font) const
{
    (void)font;
    return Theme::instance().fontSize + Theme::instance().lineSpacing;
}

// ── Word-wrap ─────────────────────────────────────────────────────────────────

TextEdit::WrapLine TextEdit::measureLine(PaintContext& ctx, const String& line, float textW) const
{
    WrapLine out;
    out.width = ctx.font.GetTextWidth(expandTabs(line).c_str());

    ct::Vector<float> widths;
    ct::Vector<bool> spaces;
    char glyph[8];
    for (size_t b = 0; b < line.size();)
    {
        const int n = utf8CharLen(line.c_str() + b);
        for (int k = 0; k < n; ++k) glyph[k] = line[b + k];
        glyph[n] = '\0';
        widths.push_back(ctx.font.GetTextWidth(glyph));
        spaces.push_back(line[b] == ' ');
        b += static_cast<size_t>(n);
    }
    const int len = static_cast<int>(widths.size());
    if (len == 0 || !wordWrap_ || textW <= 0) {
        out.rows.push_back({0, len});
        return out;
    }

    int start = 0;
    while (start < len)
    {
        int end = start, lastSpace = -1;
        float w = 0;
        while (end < len)
        {
            if (w + widths[end] > textW && end > start) break;
            if (spaces[end]) lastSpace = end;
            w += widths[end]; end++;
        }
        if (end < len && lastSpace > start) end = lastSpace + 1;
        out.rows.push_back({start, end});
        start = end;
    }
    return out;
}

void TextEdit::rebuildWrap(PaintContext& ctx)
{
    float textW = rect_.w - gutterW_ - Theme::instance().textEditPadding * 2;
    WrapKey key;
    key.width   = wordWrap_ ? textW : 0.0f;
    key.font    = ctx.buFont;
    key.scale   = ctx.font.scale;
    key.tabSize = tabSize_;
    key.wrap    = wordWrap_;
    if (!(key == wrapKey_)) {
        wrapCache_.clear();
        wrapKey_ = key;
    }

    ct::HashMap<String, WrapLine> cache;
    cache.reserve(lines_.size());
    wrapRows_.clear();
    wrapRows_.resize(lines_.size());
    lineWidths_.clear();
    lineWidths_.resize(lines_.size());
    staleWrapLines_.clear();
    widestLineGone_ = false;
    wrapWidth_   = textW;
    maxContentW_ = 0;

    for (int i = 0; i < static_cast<int>(lines_.size()); ++i)
    {
        const String& line = lines_[i];
        WrapLine reused;
        WrapLine* entry = nullptr;
        if (wrapCache_.erase(line, reused))
            entry = &cache.put(line, std::move(reused));
        else if (!(entry = cache.find(line)))
            entry = &cache.put(line, measureLine(ctx, line, textW));
        lineWidths_[i] = entry->width;
        if (entry->width > maxContentW_) maxContentW_ = entry->width;

        // If a line filter hides this line, give it 0 visual rows
        if (lineFilterCb_ && !lineFilterCb_(i)) continue;
        wrapRows_[i] = entry->rows;
    }
    wrapCache_ = std::move(cache);
    wrapDirty_ = false;
    rowStartsDirty_ = true;
}

bool TextEdit::wrapNeedsRebuild(const PaintContext& ctx, float textW) const
{
    if (wrapDirty_ || wrapRows_.size() != lines_.size()) return true;
    if (wrapKey_.font != ctx.buFont || wrapKey_.scale != ctx.font.scale ||
            wrapKey_.tabSize != tabSize_ || wrapKey_.wrap != wordWrap_)
        return true;
    return wordWrap_ && std::abs(textW - wrapWidth_) > 1.0f;
}

void TextEdit::linesChanged(int line, int removed, int inserted)
{
    if (wrapDirty_) return;
    const int count = static_cast<int>(wrapRows_.size());
    if (line < 0 || removed < 0 || inserted < 0 || line + removed > count ||
            count - removed + inserted != static_cast<int>(lines_.size()))
    {
        wrapDirty_ = true;
        return;
    }

    int kept = 0;
    for (int stale: staleWrapLines_)
    {
        if (stale >= line + removed) staleWrapLines_[kept++] = stale + inserted - removed;
        else if (stale < line)
            staleWrapLines_[kept++] = stale;
    }
    staleWrapLines_.resize(kept);
    for (int i = line; i < line + removed; ++i)
        if (lineWidths_[i] >= maxContentW_) widestLineGone_ = true;

    const int delta = inserted - removed;
    if (delta > 0)
    {
        wrapRows_.resize(count + delta);
        lineWidths_.resize(count + delta);
        for (int i = count - 1; i >= line + removed; --i)
        {
            wrapRows_[i + delta] = std::move(wrapRows_[i]);
            lineWidths_[i + delta] = lineWidths_[i];
        }
    }
    else if (delta < 0)
    {
        for (int i = line + removed; i < count; ++i)
        {
            wrapRows_[i + delta] = std::move(wrapRows_[i]);
            lineWidths_[i + delta] = lineWidths_[i];
        }
        wrapRows_.resize(count + delta);
        lineWidths_.resize(count + delta);
    }
    for (int i = line; i < line + inserted; ++i)
    {
        wrapRows_[i].clear();
        wrapRows_[i].push_back({ 0, lineColCount(i) });
        lineWidths_[i] = 0.0f;
        staleWrapLines_.push_back(i);
    }
    rowStartsDirty_ = true;
}

void TextEdit::measureStaleLines(PaintContext& ctx)
{
    for (int line: staleWrapLines_)
    {
        WrapLine measured = measureLine(ctx, lines_[line], wrapWidth_);
        lineWidths_[line] = measured.width;
        if (measured.width > maxContentW_) maxContentW_ = measured.width;
        if (lineFilterCb_ && !lineFilterCb_(line)) wrapRows_[line].clear();
        else
            wrapRows_[line] = std::move(measured.rows);
    }
    staleWrapLines_.clear();
    rowStartsDirty_ = true;
    if (widestLineGone_)
    {
        maxContentW_ = 0;
        for (float width: lineWidths_)
            if (width > maxContentW_) maxContentW_ = width;
        widestLineGone_ = false;
    }
}

const ct::Vector<int>& TextEdit::rowStarts() const
{
    const int lines = static_cast<int>(wrapRows_.size());
    if (rowStartsDirty_ || static_cast<int>(rowStarts_.size()) != lines + 1)
    {
        rowStarts_.resize(lines + 1);
        int row = 0;
        for (int i = 0; i < lines; ++i)
        {
            rowStarts_[i] = row;
            row += static_cast<int>(wrapRows_[i].size());
        }
        rowStarts_[lines] = row;
        rowStartsDirty_ = false;
    }
    return rowStarts_;
}

int TextEdit::totalVisualRows() const
{
    const ct::Vector<int>& starts = rowStarts();
    return starts[starts.size() - 1];
}

TextEdit::VisualPos TextEdit::visualToLogical(int visualRow) const
{
    const ct::Vector<int>& starts = rowStarts();
    const int lines = static_cast<int>(wrapRows_.size());
    if (lines == 0) return { 0, 0 };
    if (visualRow < 0) return { 0, visualRow };
    if (visualRow >= starts[lines])
        return { lines - 1, static_cast<int>(wrapRows_[lines - 1].size()) - 1 };
    int low = 0, high = lines - 1;
    while (low < high)
    {
        const int mid = (low + high + 1) / 2;
        if (starts[mid] <= visualRow) low = mid;
        else
            high = mid - 1;
    }
    return { low, visualRow - starts[low] };
}

int TextEdit::logicalToVisualRow(int line, int row) const
{
    const ct::Vector<int>& starts = rowStarts();
    const int lines = static_cast<int>(wrapRows_.size());
    return starts[std::max(0, std::min(line, lines))] + row;
}

void TextEdit::ensureCursorVisible()
{
    if (lineHeight() <= 0 || rect_.h <= 0) return;

    int cursorRowInLine = 0;
    if (!wrapRows_.empty() && cursor_.line < static_cast<int>(wrapRows_.size()))
    {
        auto& rows = wrapRows_[cursor_.line];
        for (int r = 0; r < static_cast<int>(rows.size()); ++r)
        {
            if (cursor_.col >= rows[r].startCol &&
                (cursor_.col <= rows[r].endCol || r == static_cast<int>(rows.size()) - 1))
            { cursorRowInLine = r; break; }
        }
    }
    int cursorVRow = logicalToVisualRow(cursor_.line, cursorRowInLine);
    float cy = cursorVRow * lineHeight();

    if (cy < scrollY_) scrollY_ = cy;
    else if (cy + lineHeight() > scrollY_ + rect_.h)
        scrollY_ = cy + lineHeight() - rect_.h;

    float maxY = std::max(0.0f, totalVisualRows() * lineHeight() - rect_.h);
    scrollY_ = std::max(0.0f, std::min(scrollY_, maxY));

    if (wordWrap_ || cursor_.line < 0 || cursor_.line >= static_cast<int>(lines_.size()))
        return;
    const String before = expandTabs(utf8Substr(lines_[cursor_.line], 0, cursor_.col));
    const float cx = WidgetApp::instance().textWidth(before.c_str(), overrideFont_, fontScale_);
    const float view = textViewWidth();
    const float margin = std::min(view * 0.25f, 20.0f);
    if (cx < scrollX_) scrollX_ = cx - margin;
    else if (cx > scrollX_ + view) scrollX_ = cx - view + margin;
    scrollX_ = std::max(0.0f, scrollX_);
}

void TextEdit::updateGutterWidth(PaintContext& ctx)
{
    if (ctx.buFont)
        gutterW_ = computeGutterWidth(*ctx.buFont) * fontScale_ + extraGutterW_;
    else
        gutterW_ = (showLineNumbers_ ? 36.0f * fontScale_ : 0.0f) + extraGutterW_;
}

float TextEdit::textViewWidth() const
{
    return rect_.w - gutterW_ - Theme::instance().textEditPadding * 2;
}

float TextEdit::maxScrollX() const
{
    return wordWrap_ ? 0.0f : std::max(0.0f, maxContentW_ - textViewWidth());
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void TextEdit::paint(PaintContext& ctx)
{
    if (!visible_) return;
    const auto& t = Theme::instance();
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;
    Rect clipped;

    blinkTimer_ += WidgetApp::instance().deltaTime();
    if (blinkTimer_ > 1.0f) blinkTimer_ -= 1.0f;

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.scale = fontScale_;

    if (overrideFont_) ctx.pushFont(overrideFont_);

    updateGutterWidth(ctx);

    float newWrapW = abs.w - gutterW_ - t.textEditPadding * 2;
    if (wrapNeedsRebuild(ctx, newWrapW)) rebuildWrap(ctx);
    else if (!staleWrapLines_.empty() || widestLineGone_)
        measureStaleLines(ctx);
    scrollX_ = std::max(0.0f, std::min(scrollX_, maxScrollX()));

    float pad = t.textEditPadding;
    Rect textArea = {abs.x + gutterW_, abs.y, abs.w - gutterW_, abs.h};
    Rect fullArea = {abs.x, abs.y, abs.w, abs.h};

    int totalVRows = totalVisualRows();
    const bool enabled = isEnabledInTree();
    const Color textColor = enabled ? t.textColor : t.textDisabled;

    // Background
    const Color bg = enabled ? t.inputBg : t.panelColor;
    if (ctx.clipRectIntersect(abs, clipped))
    {
        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fill.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                   static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                   t.borderRadius, 6, true);
    }

    // Gutter
    if (showLineNumbers_ && gutterW_ > 0)
    {
        Rect gutterRect = {abs.x, abs.y, gutterW_, abs.h};
        if (ctx.clipRectIntersect(gutterRect, clipped))
        {
            ctx.fill.SetColor(t.gutterBg.r, t.gutterBg.g, t.gutterBg.b, t.gutterBg.a);
            ctx.fill.Rectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                static_cast<int>(clipped.w), static_cast<int>(clipped.h), true);
        }
    }

    ctx.font.SetBatch(&ctx.text);

    int firstVRow = std::max(0, static_cast<int>(scrollY_ / lineHeight()));
    int lastVRow = std::min(firstVRow + static_cast<int>(abs.h / lineHeight()) + 2, totalVRows - 1);

    // Cursor visual row
    int cursorVRow = 0, cursorRowInLine = 0;
    if (!wrapRows_.empty() && cursor_.line < static_cast<int>(wrapRows_.size()))
    {
        auto& rows = wrapRows_[cursor_.line];
        for (int r = 0; r < static_cast<int>(rows.size()); ++r)
        {
            if (cursor_.col >= rows[r].startCol &&
                (cursor_.col <= rows[r].endCol || r == static_cast<int>(rows.size()) - 1))
            { cursorRowInLine = r; break; }
        }
        cursorVRow = logicalToVisualRow(cursor_.line, cursorRowInLine);
    }

    ctx.pushClip(abs);
    for (int vr = firstVRow; showLineNumbers_ && vr <= lastVRow; ++vr)
    {
        auto vp     = visualToLogical(vr);
        int  i      = vp.line;
        int  rowIdx = vp.row;
        if (i < 0 || i >= static_cast<int>(lines_.size())) continue;
        float ly = abs.y + vr * lineHeight() - scrollY_;
        float lty = ly + lineHeight() - t.lineSpacing;
        if (ly + lineHeight() < abs.y || ly > abs.y + abs.h) continue;
        if (rowIdx == 0)
        {
            char numBuf[16];
            snprintf(numBuf, sizeof(numBuf), "%d", i + 1);
            float numW = ctx.font.GetTextWidth(numBuf);
            Color numColor = (i == cursor_.line && isFocused()) ? t.lineNumberActive : t.lineNumberColor;
            ctx.font.SetColor(numColor);
            float numGutterW = gutterW_ - extraGutterW_;
            ctx.font.Print(numBuf, abs.x + numGutterW - numW - t.gutterPadding, lty);

            // Gutter markers (left of line numbers)
            ct::HashSet<int>* lineMarkers = markers_.find(i);
            if (lineMarkers) {
                float markerR = lineHeight() * 0.3f;
                float mx = abs.x + markerR + 3.0f;
                float my = ly + lineHeight() * 0.5f;
                // Pick highest-priority marker color
                Color mc = {100, 100, 100, 255}; // default grey
                if (lineMarkers->contains(static_cast<int>(MarkerType::Error)))
                    mc = {220, 50, 50, 255};
                else if (lineMarkers->contains(static_cast<int>(MarkerType::Breakpoint)))
                    mc = {200, 60, 60, 255};
                else if (lineMarkers->contains(static_cast<int>(MarkerType::Warning)))
                    mc = {220, 180, 40, 255};
                else if (lineMarkers->contains(static_cast<int>(MarkerType::Bookmark)))
                    mc = {60, 120, 220, 255};
                else if (lineMarkers->contains(static_cast<int>(MarkerType::Info)))
                    mc = {80, 180, 80, 255};
                ctx.fill.SetColor(mc.r, mc.g, mc.b, mc.a);
                ctx.fillCircle(mx, my, markerR);
            }
        }
    }
    ctx.popClip();

    ctx.pushClip(textArea);

    for (int vr = firstVRow; vr <= lastVRow; ++vr)
    {
        auto vp     = visualToLogical(vr);
        int  i      = vp.line;
        int  rowIdx = vp.row;
        if (i < 0 || i >= static_cast<int>(lines_.size())) continue;
        auto& rows = wrapRows_[i];
        if (rowIdx < 0 || rowIdx >= static_cast<int>(rows.size())) continue;

        int startCol = rows[rowIdx].startCol;
        int endCol   = rows[rowIdx].endCol;
        int startVisCol = visColAt(i, startCol);
        String rowTextRaw = utf8Substr(lines_[i], startCol, endCol);
        String rowText = expandTabs(rowTextRaw, startVisCol);

        float ly = abs.y + vr * lineHeight() - scrollY_;
        float lty = ly + lineHeight() - t.lineSpacing; // baseline

        if (ly + lineHeight() < abs.y || ly > abs.y + abs.h) continue;

        // Selection highlight
        if (isFocused() && hasSelection())
        {
            TextPos s = selectionStart(), e = selectionEnd();
            if ((i > s.line || (i == s.line && endCol > s.col)) &&
                (i < e.line || (i == e.line && startCol < e.col)))
            {
                int selS = (i == s.line) ? std::max(startCol, s.col) : startCol;
                int selE = (i == e.line) ? std::min(endCol, e.col) : endCol;
                if (i > s.line && i < e.line) { selS = startCol; selE = endCol; }
                if (selS < selE)
                {
                    String pre = expandTabs(utf8Substr(lines_[i], startCol, selS), startVisCol);
                    String sel = expandTabs(utf8Substr(lines_[i], startCol, selE), startVisCol);
                    float sx1 = textArea.x + pad + ctx.font.GetTextWidth(pre.c_str()) - scrollX_;
                    float sx2 = textArea.x + pad + ctx.font.GetTextWidth(sel.c_str()) - scrollX_;
                    if (i < e.line && selE == endCol) sx2 += 6;
                    float cx1 = std::max(sx1, textArea.x);
                    float cx2 = std::min(sx2, textArea.x + textArea.w);
                    if (cx1 < cx2)
                    {
                        ctx.fill.SetColor(t.selectionColor.r, t.selectionColor.g,
                                          t.selectionColor.b, t.selectionColor.a);
                        ctx.fill.Rectangle(static_cast<int>(cx1), static_cast<int>(ly),
                                static_cast<int>(cx2 - cx1), static_cast<int>(lineHeight()), true);
                    }
                }
            }
        }

        // Current-line highlight
        if (isFocused() && !hasSelection() && i == cursor_.line)
        {
            Rect lineRect = { textArea.x, ly, textArea.w, lineHeight() };
            Rect lc2;
            if (ctx.clipRectIntersect(lineRect, lc2))
            {
                ctx.fill.SetColor(t.currentLineHighlight.r, t.currentLineHighlight.g,
                                  t.currentLineHighlight.b, t.currentLineHighlight.a);
                ctx.fill.Rectangle(static_cast<int>(lc2.x), static_cast<int>(lc2.y),
                                   static_cast<int>(lc2.w), static_cast<int>(lc2.h), true);
            }
        }

        // Row text (with optional syntax highlighting)
        float tx = textArea.x + pad - scrollX_;
        if (syntaxCb_ && rowIdx == 0 && !lines_[i].empty()) {
            auto spans = syntaxCb_(i, lines_[i]);
            if (spans.empty()) {
                ctx.font.SetColor(textColor);
                ctx.font.Print(rowText.c_str(), tx, lty);
            } else {
                // Render text span by span, tracking visual column for correct tab expansion
                float cx = tx;
                int prevCol = startCol;
                int curVisCol = 0;
                for (const auto& sp : spans) {
                    int s0 = std::max(sp.startCol, startCol);
                    int s1 = std::min(sp.endCol, endCol);
                    if (s0 >= s1) continue;
                    // Gap before span (default color)
                    if (s0 > prevCol) {
                        String gap = expandTabs(utf8Substr(lines_[i], prevCol, s0), curVisCol);
                        ctx.font.SetColor(textColor);
                        ctx.font.Print(gap.c_str(), cx, lty);
                        cx += ctx.font.GetTextWidth(gap.c_str());
                        curVisCol += utf8Length(gap);
                    }
                    // Colored span
                    String seg = expandTabs(utf8Substr(lines_[i], s0, s1), curVisCol);
                    ctx.font.SetColor(enabled ? sp.color : textColor);
                    ctx.font.Print(seg.c_str(), cx, lty);
                    cx += ctx.font.GetTextWidth(seg.c_str());
                    curVisCol += utf8Length(seg);
                    prevCol = s1;
                }
                // Trailing text after last span
                if (prevCol < endCol) {
                    String tail = expandTabs(utf8Substr(lines_[i], prevCol, endCol), curVisCol);
                    ctx.font.SetColor(textColor);
                    ctx.font.Print(tail.c_str(), cx, lty);
                }
            }
        } else {
            ctx.font.SetColor(textColor);
            ctx.font.Print(rowText.c_str(), tx, lty);
        }
    }

    // Cursor line
    if (isFocused() && blinkTimer_ < 0.5f && !readOnly_)
    {
        float cy = abs.y + cursorVRow * lineHeight() - scrollY_;
        int rowStart = 0;
        if (cursor_.line < static_cast<int>(wrapRows_.size()) &&
            cursorRowInLine < static_cast<int>(wrapRows_[cursor_.line].size()))
            rowStart = wrapRows_[cursor_.line][cursorRowInLine].startCol;

        int rowStartVisCol = visColAt(cursor_.line, rowStart);
        String pre = expandTabs(utf8Substr(lines_[cursor_.line], rowStart, cursor_.col), rowStartVisCol);
        float cx = textArea.x + pad + ctx.font.GetTextWidth(pre.c_str()) - scrollX_;

        if (cx >= textArea.x && cx <= textArea.x + textArea.w && cy >= abs.y &&
                cy + lineHeight() <= abs.y + abs.h)
        {
            ctx.line.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
            ctx.line.Line2D(static_cast<int>(cx), static_cast<int>(cy + 1), static_cast<int>(cx),
                    static_cast<int>(cy + lineHeight() - 1));
        }
    }

    // Scrollbar — inside same clip so it merges
    float barW   = t.scrollbarWidth;
    float totalH = totalVRows * lineHeight();
    bool needVBar = totalH > abs.h;
    if (needVBar)
    {
        float trackH = abs.h;
        float thumbH = std::max(t.scrollbarMinThumb, trackH * (trackH / totalH));
        float maxSc  = totalH - trackH;
        float thumbY = (maxSc > 0) ? abs.y + (scrollY_ / maxSc) * (trackH - thumbH) : abs.y;
        ctx.fill.SetColor(t.scrollbarThumb.r, t.scrollbarThumb.g, t.scrollbarThumb.b, t.scrollbarThumb.a);
        ctx.fill.Rectangle(static_cast<int>(abs.x + abs.w - barW), static_cast<int>(thumbY),
                           static_cast<int>(barW), static_cast<int>(thumbH), true);
    }
    const float maxSx = maxScrollX();
    if (maxSx > 0.0f)
    {
        float trackW = textArea.w - (needVBar ? barW : 0.0f);
        float thumbW = std::max(t.scrollbarMinThumb, trackW * (trackW / (trackW + maxSx)));
        float thumbX = textArea.x + (scrollX_ / maxSx) * (trackW - thumbW);
        ctx.fill.SetColor(t.scrollbarThumb.r, t.scrollbarThumb.g, t.scrollbarThumb.b, t.scrollbarThumb.a);
        ctx.fill.Rectangle(static_cast<int>(thumbX), static_cast<int>(abs.y + abs.h - barW),
                           static_cast<int>(thumbW), static_cast<int>(barW), true);
    }

    ctx.popClip();

    // Border — outside clip so it draws over the content edge cleanly
    Color bc = isFocused() ? t.focusColor : t.inputBorder;
    if (ctx.clipRectIntersect(abs, clipped))
    {
        ctx.line.SetColor(bc.r, bc.g, bc.b, bc.a);
        ctx.line.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                   static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                   t.borderRadius, 6, false);
    }

    if (overrideFont_) ctx.popFont();
    ctx.font.scale = 1.0f;  // restore default scale
}

// ── Mouse events ──────────────────────────────────────────────────────────────

void TextEdit::onMousePress(MouseEvent& e)
{
    if (!isEnabledInTree()) return;
    // Gutter click → toggle breakpoint / emit markerClicked
    if (showLineNumbers_ && e.localX < gutterW_) {
        int clickedLine = static_cast<int>((e.localY + scrollY_) / lineHeight());
        if (clickedLine >= 0 && clickedLine < static_cast<int>(lines_.size())) {
            // Find the highest-priority marker on this line
            ct::HashSet<int>* lineMarkers = markers_.find(clickedLine);
            if (lineMarkers && !lineMarkers->empty()) {
                MarkerType mt = static_cast<MarkerType>(*lineMarkers->begin());
                markerClicked.emit(clickedLine, mt);
            } else {
                // No marker yet → emit as Breakpoint toggle
                markerClicked.emit(clickedLine, MarkerType::Breakpoint);
            }
        }
        e.consumed = true;
        return;
    }

    cursor_    = hitTestMouse(e.localX, e.localY);
    clampPos(cursor_);
    selAnchor_ = cursor_;

    // Double-click → select word, triple-click → select line
    if (e.button == 0 && e.clickCount >= 2) {
        int line = cursor_.line;
        if (line >= 0 && line < static_cast<int>(lines_.size())) {
            if (e.clickCount == 2) {
                const auto& text = lines_[line];
                ct::Vector<size_t> starts;
                for (size_t b = 0; b < text.size(); b += utf8CharLen(text.c_str() + b))
                    starts.push_back(b);
                int len = static_cast<int>(starts.size());
                int col = cursor_.col;
                if (col > len) col = len;
                // Letters outside ASCII count as word characters.
                auto isWord = [&text, &starts](int c) {
                    const unsigned char ch = static_cast<unsigned char>(text[starts[c]]);
                    return ch >= 0x80 || std::isalnum(ch) || ch == '_';
                };
                int start = col, end = col;
                if (col < len && isWord(col)) {
                    while (start > 0   && isWord(start - 1)) --start;
                    while (end   < len && isWord(end))       ++end;
                } else if (col > 0 && isWord(col - 1)) {
                    start = col - 1;
                    end   = col;
                    while (start > 0   && isWord(start - 1)) --start;
                    while (end   < len && isWord(end))       ++end;
                }
                selAnchor_ = { line, start };
                cursor_    = { line, end };
            } else {
                // Triple-click → select entire line
                selAnchor_ = { line, 0 };
                cursor_    = { line, lineColCount(line) };
            }
        }
    }

    dragging_  = true;
    blinkTimer_ = 0;
    ensureCursorVisible();
    markDirty();
    e.consumed = true;
}

void TextEdit::onMouseRelease(MouseEvent& e)
{
    if (dragging_) { dragging_ = false; e.consumed = true; }
}

void TextEdit::onMouseMove(MouseEvent& e)
{
    if (!isEnabledInTree()) return;
    if (!dragging_) return;
    cursor_ = hitTestMouse(e.localX, e.localY);
    clampPos(cursor_);
    blinkTimer_ = 0;
    markDirty();
    e.consumed = true;
}

void TextEdit::onMouseScroll(MouseEvent& e)
{
    if (!isEnabledInTree()) return;
    const float horizontal = (e.scrollX != 0.0f) ? e.scrollX : (e.shift ? e.scrollY : 0.0f);
    if (horizontal != 0.0f)
    {
        scrollX_ -= horizontal * lineHeight() * 3;
        scrollX_ = std::max(0.0f, std::min(scrollX_, maxScrollX()));
    }
    if (!e.shift)
    {
        scrollY_ -= e.scrollY * lineHeight() * 3;
        float maxY = std::max(0.0f, static_cast<float>(totalVisualRows()) * lineHeight() - rect_.h);
        scrollY_ = std::max(0.0f, std::min(scrollY_, maxY));
    }
    markDirty();
    e.consumed = true;
}

void TextEdit::onKeyPress(KeyEvent& e)
{
    if (!isEnabledInTree()) return;
    if (readOnly_ && !e.ctrl) return;
    blinkTimer_ = 0;
    int lc = static_cast<int>(lines_.size());

    if (e.ctrl)
    {
        switch (e.key)
        {
        case ig::retained::Key::A: selectAll(); e.consumed = true; return;
        case ig::retained::Key::C: copy(); e.consumed = true; return;
        case ig::retained::Key::X: cut(); e.consumed = true; return;
        case ig::retained::Key::V: paste(); e.consumed = true; return;
        case ig::retained::Key::Home: cursor_ = {0,0}; if (!e.shift) selAnchor_ = cursor_; ensureCursorVisible(); markDirty(); e.consumed = true; return;
        case ig::retained::Key::End: cursor_ = {lc-1, lineColCount(lc-1)}; if (!e.shift) selAnchor_ = cursor_; ensureCursorVisible(); markDirty(); e.consumed = true; return;
        default: break;
        }
    }

    switch (e.key)
    {
    case ig::retained::Key::Left:
        if (cursor_.col > 0) cursor_.col--;
        else if (cursor_.line > 0) { cursor_.line--; cursor_.col = lineColCount(cursor_.line); }
        if (!e.shift) selAnchor_ = cursor_;
        ensureCursorVisible(); markDirty(); cursorMoved.emit(cursor_); e.consumed = true; break;

    case ig::retained::Key::Right:
        if (cursor_.col < lineColCount(cursor_.line)) cursor_.col++;
        else if (cursor_.line < lc - 1) { cursor_.line++; cursor_.col = 0; }
        if (!e.shift) selAnchor_ = cursor_;
        ensureCursorVisible(); markDirty(); cursorMoved.emit(cursor_); e.consumed = true; break;

    case ig::retained::Key::Up:
        if (cursor_.line > 0) { cursor_.line--; cursor_.col = std::min(cursor_.col, lineColCount(cursor_.line)); }
        if (!e.shift) selAnchor_ = cursor_;
        ensureCursorVisible(); markDirty(); cursorMoved.emit(cursor_); e.consumed = true; break;

    case ig::retained::Key::Down:
        if (cursor_.line < lc - 1) { cursor_.line++; cursor_.col = std::min(cursor_.col, lineColCount(cursor_.line)); }
        if (!e.shift) selAnchor_ = cursor_;
        ensureCursorVisible(); markDirty(); cursorMoved.emit(cursor_); e.consumed = true; break;

    case ig::retained::Key::Home:
        cursor_.col = 0; if (!e.shift) selAnchor_ = cursor_;
        ensureCursorVisible(); markDirty(); cursorMoved.emit(cursor_); e.consumed = true; break;

    case ig::retained::Key::End:
        cursor_.col = lineColCount(cursor_.line); if (!e.shift) selAnchor_ = cursor_;
        ensureCursorVisible(); markDirty(); cursorMoved.emit(cursor_); e.consumed = true; break;

    case ig::retained::Key::PageUp: {
        int jump = std::max(1, visibleLineCount() - 1);
        cursor_.line = std::max(0, cursor_.line - jump);
        cursor_.col  = std::min(cursor_.col, lineColCount(cursor_.line));
        if (!e.shift) selAnchor_ = cursor_;
        ensureCursorVisible(); markDirty(); e.consumed = true; break;
    }
    case ig::retained::Key::PageDown: {
        int jump = std::max(1, visibleLineCount() - 1);
        cursor_.line = std::min(lc - 1, cursor_.line + jump);
        cursor_.col  = std::min(cursor_.col, lineColCount(cursor_.line));
        if (!e.shift) selAnchor_ = cursor_;
        ensureCursorVisible(); markDirty(); e.consumed = true; break;
    }

    case ig::retained::Key::Return: case ig::retained::Key::KPEnter:
        splitLine(); ensureCursorVisible(); e.consumed = true; break;

    case ig::retained::Key::Tab: {
        if (!readOnly_) { insertTextAtCursor("\t"); ensureCursorVisible(); }
        e.consumed = true; break;
    }

    case ig::retained::Key::Backspace:
        if (!readOnly_) {
            if (hasSelection()) { deleteSelection(); }
            else if (cursor_.col > 0) {
                size_t byteEnd   = utf8ByteOffset(lines_[cursor_.line], cursor_.col);
                cursor_.col--;
                size_t byteStart = utf8ByteOffset(lines_[cursor_.line], cursor_.col);
                lines_[cursor_.line].erase(byteStart, byteEnd - byteStart);
                selAnchor_ = cursor_;
                linesChanged(cursor_.line, 1, 1);
            }
            else { mergeWithPrevLine(); }
            ensureCursorVisible(); markDirty(); textChanged.emit(); cursorMoved.emit(cursor_);
        }
        e.consumed = true; break;

    case ig::retained::Key::Delete:
        if (!readOnly_) {
            if (hasSelection()) { deleteSelection(); }
            else if (cursor_.col < lineColCount(cursor_.line)) {
                size_t bs = utf8ByteOffset(lines_[cursor_.line], cursor_.col);
                size_t be = utf8ByteOffset(lines_[cursor_.line], cursor_.col + 1);
                lines_[cursor_.line].erase(bs, be - bs);
                selAnchor_ = cursor_;
                linesChanged(cursor_.line, 1, 1);
            }
            else { mergeWithNextLine(); }
            markDirty(); textChanged.emit(); cursorMoved.emit(cursor_);
        }
        e.consumed = true; break;

    case ig::retained::Key::Escape: clearSelection(); e.consumed = true; break;

    default: break;
    }
}

void TextEdit::onTextInput(KeyEvent& e)
{
    if (!isEnabledInTree()) return;
    if (readOnly_) { e.consumed = true; return; }
    String t(e.text);
    if (t.empty()) return;
    insertTextAtCursor(t);
    ensureCursorVisible();
    blinkTimer_ = 0;
    e.consumed = true;
}

Widget::Vec2f TextEdit::sizeHint() const { return {300.0f, 200.0f}; }
