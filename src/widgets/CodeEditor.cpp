#include "pch.hpp"
#include "CodeEditor.hpp"
#include "WidgetApp.hpp"
#include "Utf8.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>
#include <ct/regex.hpp>

// The lexical highlighter classes (SyntaxHighlighter, SyntaxLanguage,
// DefinedHighlighter, CppHighlighter, PythonHighlighter, JsHighlighter,
// LuaHighlighter, GlslHighlighter, JavaHighlighter, BlitzHighlighter) now
// live in src/Syntax.cpp (ig::syntax) so they are shared with the
// immediate-mode Context::codeEditor(). Only the CodeEditor widget itself
// (folding, undo/redo, painting, ...) remains here, along with a couple of
// small presentation-only helpers (indent width, word-boundary check) that
// never belonged to the lexer interface.

namespace {

inline bool isIdChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// Count leading whitespace, expanding tabs to the next multiple of tabSize.
int leadingIndent(const String& text, int tabSize = 4)
{
    int visCol = 0;
    int n = static_cast<int>(text.size());
    for (int i = 0; i < n; i++) {
        if (text[i] == ' ') visCol++;
        else if (text[i] == '\t') visCol += tabSize - (visCol % tabSize);
        else break;
    }
    return visCol;
}

} // anonymous namespace

CodeEditor::CodeEditor()
{
    setShowLineNumbers(true);
    acceptsFocus_ = true;

    // Install line filter so TextEdit skips folded (hidden) lines
    lineFilterCb_ = [this](int line) -> bool {
        return !isLineHidden(line);
    };
}

void CodeEditor::registerLanguage(const SyntaxLanguage& language)
{
    ig::syntax::registerLanguage(language);
}

CodeEditor::~CodeEditor()
{
    delete highlighter_;
}

// ── Highlighter ──────────────────────────────────────────────────────────────

void CodeEditor::setHighlighterRaw(SyntaxHighlighter* hl)
{
    if (highlighter_ == hl) return;
    delete highlighter_;
    highlighter_ = hl;
    lineStates_.clear();
    hlDirty_ = true;
    installSyntaxCallback();
    markDirty();
}

SyntaxHighlighter* CodeEditor::setHighlighterForFile(const String& filename)
{
    SyntaxHighlighter* hl = ig::syntax::makeHighlighterForFile(filename);
    setHighlighterRaw(hl);
    return hl;
}

void CodeEditor::installSyntaxCallback()
{
    if (!highlighter_) {
        setSyntaxCallback(nullptr);
        return;
    }

    // Bridge: SyntaxHighlighter → TextEdit::SyntaxCallback
    setSyntaxCallback([this](int lineIndex, const String& lineText)
                      -> ct::Vector<TextEdit::ColorSpan>
    {
        // Get previous line's state
        int prevState = 0;
        if (lineIndex > 0 && lineIndex - 1 < static_cast<int>(lineStates_.size()))
            prevState = lineStates_[lineIndex - 1];

        auto result = highlighter_->highlightLine(lineIndex, lineText, prevState);

        // Cache state
        if (lineIndex >= static_cast<int>(lineStates_.size()))
            lineStates_.resize(lineIndex + 1, 0);
        lineStates_[lineIndex] = result.state;

        // Convert Span → ColorSpan
        ct::Vector<TextEdit::ColorSpan> out;
        out.reserve(result.spans.size());
        for (auto& sp : result.spans) {
            out.push_back({sp.startCol, sp.endCol,
                           highlighter_->colorFor(sp.type)});
        }
        return out;
    });
}

void CodeEditor::reHighlight() const
{
    if (!highlighter_) return;
    int lc = lineCount();
    lineStates_.resize(lc, 0);
    int state = 0;
    for (int i = 0; i < lc; i++) {
        auto result = highlighter_->highlightLine(i, lineAt(i), state);
        lineStates_[i] = result.state;
        state = result.state;
    }
    hlDirty_ = false;
}

void CodeEditor::reHighlightFrom(int line) const
{
    if (!highlighter_) return;
    int lc = lineCount();
    if (line < 0) line = 0;
    lineStates_.resize(lc, 0);
    int state = (line > 0) ? lineStates_[line - 1] : 0;
    for (int i = line; i < lc; i++) {
        auto result = highlighter_->highlightLine(i, lineAt(i), state);
        int oldState = lineStates_[i];
        lineStates_[i] = result.state;
        state = result.state;
        // If state didn't change, subsequent lines won't change either
        if (i > line && oldState == result.state) break;
    }
}

// ── Folding ──────────────────────────────────────────────────────────────────

void CodeEditor::rebuildFoldRanges()
{
    foldRanges_.clear();
    if (!highlighter_) return;

    int lc = lineCount();
    ct::Vector<int> foldStack; // stack of line indices that opened a fold

    for (int i = 0; i < lc; i++) {
        int delta = highlighter_->foldDelta(i, lineAt(i));
        for (int d = 0; d < delta; d++)
            foldStack.push_back(i);
        for (int d = 0; d > delta; d--) {
            if (!foldStack.empty()) {
                int startLine = foldStack.back();
                foldStack.pop_back();
                foldRanges_.push_back({startLine, i, false});
            }
        }
    }
    // Close any unclosed ranges at EOF
    while (!foldStack.empty()) {
        int startLine = foldStack.back();
        foldStack.pop_back();
        foldRanges_.push_back({startLine, lc - 1, false});
    }
}

void CodeEditor::foldAt(int line)
{
    for (auto& fr : foldRanges_) {
        if (fr.startLine == line && !fr.collapsed) {
            fr.collapsed = true;
            foldToggled.emit(line, true);
            invalidateWrap();
            markDirty();
            return;
        }
    }
}

void CodeEditor::unfoldAt(int line)
{
    for (auto& fr : foldRanges_) {
        if (fr.startLine == line && fr.collapsed) {
            fr.collapsed = false;
            foldToggled.emit(line, false);
            invalidateWrap();
            markDirty();
            return;
        }
    }
}

void CodeEditor::toggleFoldAt(int line)
{
    for (auto& fr : foldRanges_) {
        if (fr.startLine == line) {
            fr.collapsed = !fr.collapsed;
            foldToggled.emit(line, fr.collapsed);
            invalidateWrap();
            markDirty();
            return;
        }
    }
}

bool CodeEditor::isFolded(int line) const
{
    for (auto& fr : foldRanges_)
        if (fr.startLine == line) return fr.collapsed;
    return false;
}

void CodeEditor::foldAll()
{
    for (auto& fr : foldRanges_) fr.collapsed = true;
    invalidateWrap();
    markDirty();
}

void CodeEditor::unfoldAll()
{
    for (auto& fr : foldRanges_) fr.collapsed = false;
    invalidateWrap();
    markDirty();
}

bool CodeEditor::isLineHidden(int line) const
{
    for (auto& fr : foldRanges_) {
        if (fr.collapsed && line > fr.startLine && line <= fr.endLine)
            return true;
    }
    return false;
}

int CodeEditor::foldGutterWidth() const
{
    return showFolding_ ? 20 : 0;
}

void CodeEditor::paintFoldGutter(PaintContext& ctx, const Rect& abs)
{
    if (!showFolding_ || !highlighter_) return;

    float lh = lineHeight_;
    if (lh <= 0) return;
    // Fold icons sit between line numbers and code text
    float fgW = static_cast<float>(foldGutterWidth());
    float gutterX = abs.x + gutterW_ - fgW;

    ctx.pushClip(abs);

    for (auto& fr : foldRanges_) {
        int line = fr.startLine;
        if (isLineHidden(line)) continue;  // fold header itself hidden by parent fold

        int vRow = logicalToVisualRow(line);
        float y = abs.y + vRow * lh - scrollY_;

        // Skip if offscreen
        if (y + lh < abs.y || y > abs.y + abs.h) continue;

        float cx = gutterX + fgW * 0.5f;
        float cy = y + lh * 0.5f;
        float sz = 3.5f * fontScale_;

        if (fr.collapsed) {
            // Right-pointing chevron ›  (two lines forming >)
            ctx.line.SetColor(180, 180, 190, 220);
            ctx.line.Line2D(cx - sz * 0.5f, cy - sz, cx + sz * 0.5f, cy);
            ctx.line.Line2D(cx + sz * 0.5f, cy, cx - sz * 0.5f, cy + sz);
        } else {
            // Down-pointing chevron ˅  (two lines forming v)
            ctx.line.SetColor(150, 150, 160, 200);
            ctx.line.Line2D(cx - sz, cy - sz * 0.5f, cx, cy + sz * 0.5f);
            ctx.line.Line2D(cx, cy + sz * 0.5f, cx + sz, cy - sz * 0.5f);
        }
    }

    ctx.popClip();
}

// ── Undo / Redo ──────────────────────────────────────────────────────────────

void CodeEditor::recordInsert(TextPos pos, const String& text,
                               TextPos oldCursor, TextPos newCursor)
{
    if (recording_) return;
    clearRedoStack();
    undoStack_.push_back({EditAction::Insert, pos, "", text, oldCursor, newCursor});
    undoRedoChanged.emit(canUndo(), canRedo());
}

void CodeEditor::recordDelete(TextPos pos, const String& text,
                               TextPos oldCursor, TextPos newCursor)
{
    if (recording_) return;
    clearRedoStack();
    undoStack_.push_back({EditAction::Delete, pos, text, "", oldCursor, newCursor});
    undoRedoChanged.emit(canUndo(), canRedo());
}

void CodeEditor::clearRedoStack()
{
    if (!redoStack_.empty()) {
        redoStack_.clear();
        undoRedoChanged.emit(canUndo(), canRedo());
    }
}

bool CodeEditor::canUndo() const { return !undoStack_.empty(); }
bool CodeEditor::canRedo() const { return !redoStack_.empty(); }

void CodeEditor::clearUndoHistory()
{
    undoStack_.clear();
    redoStack_.clear();
    undoRedoChanged.emit(false, false);
}

void CodeEditor::undo()
{
    if (undoStack_.empty()) return;
    auto action = undoStack_.back();
    undoStack_.pop_back();

    recording_ = true;
    applyAction(action, true);
    recording_ = false;

    redoStack_.push_back(action);
    hlDirty_ = true;
    markDirty();
    undoRedoChanged.emit(canUndo(), canRedo());
}

void CodeEditor::redo()
{
    if (redoStack_.empty()) return;
    auto action = redoStack_.back();
    redoStack_.pop_back();

    recording_ = true;
    applyAction(action, false);
    recording_ = false;

    undoStack_.push_back(action);
    hlDirty_ = true;
    markDirty();
    undoRedoChanged.emit(canUndo(), canRedo());
}

void CodeEditor::applyAction(const EditAction& action, bool isUndo)
{
    if (isUndo) {
        if (action.type == EditAction::Insert) {
            // Undo insert = delete the text that was inserted
            cursor_ = action.pos;
            selAnchor_ = action.newCursor;
            // Select the inserted text, then delete
            if (cursor_ != selAnchor_) deleteSelection();
            cursor_ = action.oldCursor;
            selAnchor_ = cursor_;
        } else if (action.type == EditAction::Delete) {
            // Undo delete = re-insert the deleted text
            cursor_ = action.pos;
            selAnchor_ = cursor_;
            insertTextAtCursor(action.oldText);
            cursor_ = action.oldCursor;
            selAnchor_ = cursor_;
        }
    } else {
        if (action.type == EditAction::Insert) {
            cursor_ = action.pos;
            selAnchor_ = cursor_;
            insertTextAtCursor(action.newText);
            cursor_ = action.newCursor;
            selAnchor_ = cursor_;
        } else if (action.type == EditAction::Delete) {
            cursor_ = action.pos;
            selAnchor_ = action.pos;
            // Set selection to the text range and delete
            TextPos end = action.pos;
            // Calculate end position from old text
            for (char c : action.oldText) {
                if (c == '\n') { end.line++; end.col = 0; }
                else end.col++;
            }
            selAnchor_ = action.pos;
            cursor_ = end;
            deleteSelection();
            cursor_ = action.newCursor;
            selAnchor_ = cursor_;
        }
    }
    ensureCursorVisible();
    textChanged.emit();
    cursorMoved.emit(cursor_);
}

// ── Auto-indent ──────────────────────────────────────────────────────────────

String CodeEditor::getLineIndent(int line) const
{
    if (line < 0 || line >= lineCount()) return "";
    const auto& ln = lineAt(line);
    int i = 0;
    while (i < static_cast<int>(ln.size()) && (ln[i] == ' ' || ln[i] == '\t')) i++;
    return ln.substr(0, i);
}

bool CodeEditor::lineEndsWith(int line, char c) const
{
    if (line < 0 || line >= lineCount()) return false;
    const auto& ln = lineAt(line);
    int last = static_cast<int>(ln.size()) - 1;
    while (last >= 0 && (ln[last] == ' ' || ln[last] == '\t')) last--;
    return last >= 0 && ln[last] == c;
}

// ── Bracket matching ─────────────────────────────────────────────────────────

CodeEditor::BracketPair CodeEditor::findMatchingBracket(TextPos pos) const
{
    BracketPair result = {{-1, -1}, {-1, -1}};

    if (pos.line < 0 || pos.line >= lineCount()) return result;
    const auto& line = lineAt(pos.line);
    if (pos.col < 0 || pos.col >= static_cast<int>(line.size())) return result;

    char ch = line[pos.col];
    char match = 0;
    int dir = 0;
    switch (ch) {
        case '(': match = ')'; dir = +1; break;
        case ')': match = '('; dir = -1; break;
        case '{': match = '}'; dir = +1; break;
        case '}': match = '{'; dir = -1; break;
        case '[': match = ']'; dir = +1; break;
        case ']': match = '['; dir = -1; break;
        default: return result;
    }

    result.open = pos;
    int depth = 1;
    int curLine = pos.line;
    int curCol  = pos.col + dir;
    int lc = lineCount();

    while (curLine >= 0 && curLine < lc && depth > 0) {
        const auto& ln = lineAt(curLine);
        int len = static_cast<int>(ln.size());

        while (curCol >= 0 && curCol < len && depth > 0) {
            char c = ln[curCol];
            if (c == ch) depth++;
            else if (c == match) depth--;
            if (depth == 0) {
                result.close = {curLine, curCol};
                return result;
            }
            curCol += dir;
        }

        curLine += dir;
        if (curLine >= 0 && curLine < lc) {
            curCol = (dir > 0) ? 0 : static_cast<int>(lineAt(curLine).size()) - 1;
        }
    }

    result.open = {-1, -1};
    return result;
}

// Compute pixel X offset for column `col` in `line` using glyph advances.
// This matches addText's rendering exactly — no monospace assumption.
float CodeEditor::colToPixelX(int line, int col) const
{
    if (line < 0 || line >= lineCount()) return 0.0f;
    const auto* font = lastPaintFont_;
    if (!font) return 0.0f;

    float scale = fontScale_;
    const ig::retained::FontGlyph* spaceG = font->findGlyph(' ');
    float spaceAdv = spaceG ? spaceG->advanceX * scale : 8.0f;
    int ts = tabSize();
    if (ts <= 0) ts = 4;

    const String& text = lineAt(line);
    float penX = 0.0f;
    int visCol = 0;
    int curCol = 0;

    for (size_t i = 0; i < text.size() && curCol < col; ) {
        if (text[i] == '\t') {
            int tabW = ts - (visCol % ts);
            penX += tabW * spaceAdv;
            visCol += tabW;
            curCol++;
            i++;
        } else if (text[i] == ' ') {
            penX += spaceAdv;
            visCol++;
            curCol++;
            i++;
        } else {
            // Decode UTF-8 codepoint
            unsigned char uc = static_cast<unsigned char>(text[i]);
            int seqLen = 1;
            if      (uc >= 0xF0) seqLen = 4;
            else if (uc >= 0xE0) seqLen = 3;
            else if (uc >= 0xC0) seqLen = 2;

            uint32_t cp = uc;
            if (seqLen == 2 && i + 1 < text.size())
                cp = ((uc & 0x1F) << 6) | (text[i+1] & 0x3F);
            else if (seqLen == 3 && i + 2 < text.size())
                cp = ((uc & 0x0F) << 12) | ((text[i+1] & 0x3F) << 6) | (text[i+2] & 0x3F);
            else if (seqLen == 4 && i + 3 < text.size())
                cp = ((uc & 0x07) << 18) | ((text[i+1] & 0x3F) << 12) | ((text[i+2] & 0x3F) << 6) | (text[i+3] & 0x3F);

            const ig::retained::FontGlyph* g = font->findGlyph(cp);
            penX += g ? g->advanceX * scale : spaceAdv;
            visCol++;
            curCol++;
            i += seqLen;
        }
    }
    return penX;
}

// Width of one character at position col in line (for highlight rectangles)
float CodeEditor::colCharWidth(int line, int col) const
{
    if (line < 0 || line >= lineCount()) return 8.0f;
    const auto* font = lastPaintFont_;
    if (!font) return 8.0f;

    float scale = fontScale_;
    const ig::retained::FontGlyph* spaceG = font->findGlyph(' ');
    float spaceAdv = spaceG ? spaceG->advanceX * scale : 8.0f;

    const String& text = lineAt(line);
    int curCol = 0;
    for (size_t i = 0; i < text.size(); ) {
        if (curCol == col) {
            if (text[i] == '\t') {
                int ts = tabSize(); if (ts <= 0) ts = 4;
                int visC = visColAt(line, col);
                return (ts - (visC % ts)) * spaceAdv;
            } else if (text[i] == ' ') {
                return spaceAdv;
            } else {
                unsigned char uc = static_cast<unsigned char>(text[i]);
                int seqLen = 1;
                if      (uc >= 0xF0) seqLen = 4;
                else if (uc >= 0xE0) seqLen = 3;
                else if (uc >= 0xC0) seqLen = 2;
                uint32_t cp = uc;
                if (seqLen == 2 && i + 1 < text.size())
                    cp = ((uc & 0x1F) << 6) | (text[i+1] & 0x3F);
                else if (seqLen == 3 && i + 2 < text.size())
                    cp = ((uc & 0x0F) << 12) | ((text[i+1] & 0x3F) << 6) | (text[i+2] & 0x3F);
                else if (seqLen == 4 && i + 3 < text.size())
                    cp = ((uc & 0x07) << 18) | ((text[i+1] & 0x3F) << 12) | ((text[i+2] & 0x3F) << 6) | (text[i+3] & 0x3F);
                const ig::retained::FontGlyph* g = font->findGlyph(cp);
                return g ? g->advanceX * scale : spaceAdv;
            }
        }
        unsigned char uc = static_cast<unsigned char>(text[i]);
        int seqLen = 1;
        if      (uc >= 0xF0) seqLen = 4;
        else if (uc >= 0xE0) seqLen = 3;
        else if (uc >= 0xC0) seqLen = 2;
        i += seqLen;
        curCol++;
    }
    return spaceAdv;
}

void CodeEditor::paintBracketMatch(PaintContext& ctx, const Rect& abs)
{
    if (!showBracketMatch_) return;

    auto bp = findMatchingBracket(cursor_);
    if (bp.open.line < 0) {
        if (cursor_.col > 0) {
            bp = findMatchingBracket({cursor_.line, cursor_.col - 1});
        }
    }
    if (bp.open.line < 0 || bp.close.line < 0) return;

    float lh = lineHeight_;
    if (lh <= 0) return;
    float textX = abs.x + gutterW_;
    float pad = Theme::instance().textEditPadding;

    auto drawBracketHighlight = [&](TextPos p) {
        if (isLineHidden(p.line)) return;
        int vRow = logicalToVisualRow(p.line);
        float bx = textX + pad + colToPixelX(p.line, p.col) - scrollX_;
        float bw = colCharWidth(p.line, p.col);
        float by = abs.y + vRow * lh - scrollY_;
        if (by + lh < abs.y || by > abs.y + abs.h) return;
        ctx.fill.SetColor(255, 210, 80, 60);
        ctx.fill.Rectangle(bx, by, bw, lh, true);
        ctx.line.SetColor(255, 210, 80, 180);
        ctx.line.Rectangle(bx, by, bw, lh, false);
    };

    drawBracketHighlight(bp.open);
    drawBracketHighlight(bp.close);
}

// ── Visual: current line highlight ───────────────────────────────────────────

void CodeEditor::paintCurrentLineHighlight(PaintContext& ctx, const Rect& abs)
{
    if (!highlightCurrentLine_ || !isFocused()) return;
    if (isLineHidden(cursor_.line)) return;

    float lh = lineHeight_;
    if (lh <= 0) return;
    int vRow = logicalToVisualRow(cursor_.line);
    float y = abs.y + vRow * lh - scrollY_;

    if (y + lh < abs.y || y > abs.y + abs.h) return;

    ctx.fill.SetColor(45, 50, 60, 255);
    ctx.fill.Rectangle(abs.x + gutterW_, y,
                        abs.w - gutterW_, lh, true);
}

// ── Visual: indent guides ────────────────────────────────────────────────────

void CodeEditor::paintIndentGuides(PaintContext& ctx, const Rect& abs)
{
    if (!showIndentGuides_) return;

    float charW = ctx.font.GetTextWidth(" ");
    float lh = lineHeight_;
    if (lh <= 0) return;
    float textX = abs.x + gutterW_;
    int ts = tabSize();
    if (ts <= 0) ts = 4;

    ctx.line.SetColor(55, 60, 70, 120);

    int lc = lineCount();
    for (int line = 0; line < lc; line++) {
        if (isLineHidden(line)) continue;

        int indent = leadingIndent(lineAt(line));
        int vRow = logicalToVisualRow(line);
        float y = abs.y + vRow * lh - scrollY_;

        if (y + lh < abs.y) continue;
        if (y > abs.y + abs.h) break;

        for (int col = ts; col < indent; col += ts) {
            float x = textX + col * charW - scrollX_;
            float dashLen = 2.0f * fontScale_;
            float dashGap = 4.0f * fontScale_;
            for (float dy = dashLen; dy < lh - dashLen; dy += dashGap) {
                ctx.line.Line2D(static_cast<int>(x), static_cast<int>(y + dy),
                                static_cast<int>(x), static_cast<int>(y + dy + dashLen));
            }
        }
    }
}

// ── Visual: whitespace indicators ────────────────────────────────────────────

void CodeEditor::paintWhitespace(PaintContext& ctx, const Rect& abs)
{
    if (!showWhitespace_) return;

    const ig::retained::Font* font = ctx.buFont;
    if (!font) return;

    float lh = lineHeight_;
    if (lh <= 0) return;
    float scale = fontScale_;
    float textX = abs.x + gutterW_;
    float pad = Theme::instance().textEditPadding;
    int ts = tabSize();
    if (ts <= 0) ts = 4;

    // Space advance — same value addText uses for ' '
    const ig::retained::FontGlyph* spaceGlyph = font->findGlyph(' ');
    float spaceAdv = spaceGlyph ? spaceGlyph->advanceX * scale : 8.0f;

    ctx.fill.SetColor(100, 110, 130, 100);
    ctx.pushClip(abs);

    int lc = lineCount();
    for (int line = 0; line < lc; line++) {
        if (isLineHidden(line)) continue;

        int vRow = logicalToVisualRow(line);
        float y = abs.y + vRow * lh - scrollY_;

        if (y + lh < abs.y) continue;
        if (y > abs.y + abs.h) break;

        const String& text = lineAt(line);
        if (text.empty()) continue;

        String expanded = expandTabs(text);
        float midY = y + lh * 0.5f;

        // Walk original text, tracking visCol (same as expandTabs),
        // and penX via glyph advances on expanded string (same as addText).
        float penX = textX + pad - scrollX_;
        int visCol = 0;
        size_t ei = 0;

        for (size_t oi = 0; oi < text.size(); ) {
            float charX = penX;

            if (text[oi] == '\t') {
                // Compute tab width the SAME way expandTabs does
                int tabSpaces = ts - (visCol % ts);

                float tabEndX = penX + tabSpaces * spaceAdv;

                // Arrow: horizontal line + arrowhead
                float x1 = charX + spaceAdv * 0.3f;
                float x2 = tabEndX - spaceAdv * 0.3f;
                if (x2 > x1) {
                    float as = std::min(spaceAdv * 0.25f, 3.0f * scale);
                    ctx.fill.Line2D(x1, midY, x2, midY);
                    ctx.fill.Line2D(x2, midY, x2 - as, midY - as);
                    ctx.fill.Line2D(x2, midY, x2 - as, midY + as);
                }

                penX = tabEndX;
                ei += tabSpaces;
                visCol += tabSpaces;
                oi++;

            } else if (text[oi] == ' ') {
                penX += spaceAdv;
                ei++;

                float dotR = std::max(1.0f, 1.5f * scale);
                ctx.fillCircle(charX + spaceAdv * 0.5f, midY, dotR);

                visCol++;
                oi++;

            } else {
                // Regular char: decode from expanded to get exact glyph advance
                if (ei < expanded.size()) {
                    unsigned char uc = static_cast<unsigned char>(expanded[ei]);
                    int seqLen = 1;
                    if      (uc >= 0xF0) seqLen = 4;
                    else if (uc >= 0xE0) seqLen = 3;
                    else if (uc >= 0xC0) seqLen = 2;

                    uint32_t cp = uc;
                    if (seqLen == 2 && ei + 1 < expanded.size())
                        cp = ((uc & 0x1F) << 6) | (expanded[ei+1] & 0x3F);
                    else if (seqLen == 3 && ei + 2 < expanded.size())
                        cp = ((uc & 0x0F) << 12) | ((expanded[ei+1] & 0x3F) << 6) | (expanded[ei+2] & 0x3F);
                    else if (seqLen == 4 && ei + 3 < expanded.size())
                        cp = ((uc & 0x07) << 18) | ((expanded[ei+1] & 0x3F) << 12) | ((expanded[ei+2] & 0x3F) << 6) | (expanded[ei+3] & 0x3F);

                    const ig::retained::FontGlyph* g = font->findGlyph(cp);
                    penX += g ? g->advanceX * scale : spaceAdv;
                    ei += seqLen;
                }

                // Advance original text
                unsigned char ouc = static_cast<unsigned char>(text[oi]);
                int oseq = 1;
                if      (ouc >= 0xF0) oseq = 4;
                else if (ouc >= 0xE0) oseq = 3;
                else if (ouc >= 0xC0) oseq = 2;
                oi += oseq;
                visCol++;
            }
        }
    }

    ctx.popClip();
}

// ── Visual: fold scope lines ─────────────────────────────────────────────────

void CodeEditor::paintScopeLines(PaintContext& ctx, const Rect& abs)
{
    if (!showScopeLines_ || foldRanges_.empty()) return;

    float charW = ctx.font.GetTextWidth(" ");
    float lh = lineHeight_;
    if (lh <= 0 || charW <= 0) return;
    float textX = abs.x + gutterW_;
    float pad = 4.0f;

    // Colors: cycle through a palette for nested scopes (VS Code style)
    static const Color scopeColors[] = {
        { 90, 105, 180, 140},   // blue
        { 90, 170, 100, 140},   // green
        {190, 160,  70, 140},   // gold
        {180,  90,  90, 140},   // red
        { 90, 160, 180, 140},   // teal
        {160, 100, 180, 140},   // purple
    };
    constexpr int numColors = 6;

    ctx.pushClip(abs);

    // Sort ranges by startLine for consistent depth colouring
    int depth = 0;
    for (int ri = 0; ri < static_cast<int>(foldRanges_.size()); ri++) {
        auto& fr = foldRanges_[ri];
        if (fr.collapsed) continue;

        // Calculate indent of the opening line to position the vertical line
        const String& openLine = lineAt(fr.startLine);
        int indent = leadingIndent(openLine);

        // Determine nesting depth by counting how many ranges contain this one
        int nestDepth = 0;
        for (int rj = 0; rj < static_cast<int>(foldRanges_.size()); rj++) {
            if (rj == ri) continue;
            auto& other = foldRanges_[rj];
            if (other.startLine < fr.startLine && other.endLine > fr.endLine)
                nestDepth++;
        }

        const Color& sc = scopeColors[nestDepth % numColors];
        ctx.line.SetColor(sc.r, sc.g, sc.b, sc.a);

        float x = textX + pad + indent * charW - scrollX_ + charW * 0.5f;

        // Find visual rows for start+1 and end
        int startVis = -1, endVis = -1;
        for (int ln = fr.startLine + 1; ln <= fr.endLine; ln++) {
            if (isLineHidden(ln)) continue;
            if (startVis < 0) startVis = logicalToVisualRow(ln);
            endVis = logicalToVisualRow(ln);
        }
        if (startVis < 0) continue;

        float y1 = abs.y + startVis * lh - scrollY_;
        float y2 = abs.y + endVis * lh + lh - scrollY_;

        // Clip to visible area
        float cy1 = std::max(y1, abs.y);
        float cy2 = std::min(y2, abs.y + abs.h);
        if (cy1 >= cy2) continue;

        // Vertical scope line
        ctx.line.Line2D(x, cy1, x, cy2);
    }

    ctx.popClip();
}

// ── Search ───────────────────────────────────────────────────────────────────

void CodeEditor::showSearchBar()
{
    searchVisible_ = true;
    replaceVisible_ = false;
    markDirty();
}

void CodeEditor::showReplaceBar()
{
    searchVisible_ = true;
    replaceVisible_ = true;
    markDirty();
}

void CodeEditor::hideSearchBar()
{
    searchVisible_ = false;
    replaceVisible_ = false;
    searchMatches_.clear();
    searchMatchIdx_ = -1;
    markDirty();
}

void CodeEditor::rebuildSearchMatches()
{
    searchMatches_.clear();
    searchMatchLengths_.clear();
    searchMatchIdx_ = -1;
    if (searchTerm_.empty()) return;

    int lc = lineCount();

    if (searchIsRegex_) {
        // ct::Regex operates on a StringView per line - whole-line rather
        // than whole-document, so ^/$ anchor per line and matches never
        // span a line break (matching how the literal search already works).
        unsigned flags = searchCaseSensitive_ ? 0u : ct::Regex::IgnoreCase;
        ct::Regex::Error err;
        ct::Regex re = ct::Regex::compile(searchTerm_, flags, &err);
        if (!re.valid()) return;

        for (int line = 0; line < lc; line++) {
            const auto& ln = lineAt(line);
            ct::StringView view(ln.data(), ln.size());
            std::size_t pos = 0;
            bool mustAdvance = false;
            ct::Match m;
            while (pos <= view.size() && re.search(view, &m, pos)) {
                std::size_t start = m.start();
                std::size_t end = m.end();
                searchMatches_.push_back({line, static_cast<int>(start)});
                searchMatchLengths_.push_back(static_cast<int>(end - start));
                pos = end;
                if (start == end) ++pos; // avoid looping on a zero-width match
                (void)mustAdvance;
            }
        }
        return;
    }

    for (int line = 0; line < lc; line++) {
        const auto& ln = lineAt(line);
        size_t pos = 0;
        while (true) {
            size_t found;
            if (searchCaseSensitive_) {
                found = ln.find(searchTerm_, pos);
            } else {
                // Case-insensitive find
                auto it = std::search(ln.begin() + pos, ln.end(),
                                      searchTerm_.begin(), searchTerm_.end(),
                                      [](char a, char b) {
                                          return std::tolower(static_cast<unsigned char>(a)) ==
                                                 std::tolower(static_cast<unsigned char>(b));
                                      });
                found = (it != ln.end()) ? static_cast<size_t>(it - ln.begin()) : String::npos;
            }
            if (found == String::npos) break;

            if (searchWholeWord_) {
                int s = static_cast<int>(found);
                int e = s + static_cast<int>(searchTerm_.size());
                bool wordStart = (s == 0 || !isIdChar(ln[s - 1]));
                bool wordEnd = (e >= static_cast<int>(ln.size()) || !isIdChar(ln[e]));
                if (!wordStart || !wordEnd) { pos = found + 1; continue; }
            }

            searchMatches_.push_back({line, static_cast<int>(found)});
            searchMatchLengths_.push_back(static_cast<int>(searchTerm_.size()));
            pos = found + 1;
        }
    }
}

bool CodeEditor::findNext(const String& text, bool caseSensitive, bool wholeWord, bool useRegex)
{
    searchTerm_ = text;
    searchCaseSensitive_ = caseSensitive;
    searchWholeWord_ = wholeWord;
    searchIsRegex_ = useRegex;
    rebuildSearchMatches();
    if (searchMatches_.empty()) return false;

    // Find first match after cursor
    for (int i = 0; i < static_cast<int>(searchMatches_.size()); i++) {
        if (searchMatches_[i] >= cursor_) {
            searchMatchIdx_ = i;
            cursor_ = searchMatches_[i];
            selAnchor_ = cursor_;
            selAnchor_.col += searchMatchLengths_[i];
            // Swap so cursor is at end
            std::swap(cursor_, selAnchor_);
            ensureCursorVisible();
            markDirty();
            return true;
        }
    }
    // Wrap around
    searchMatchIdx_ = 0;
    cursor_ = searchMatches_[0];
    selAnchor_ = cursor_;
    cursor_.col += searchMatchLengths_[0];
    ensureCursorVisible();
    markDirty();
    return true;
}

bool CodeEditor::findPrev(const String& text, bool caseSensitive, bool wholeWord, bool useRegex)
{
    searchTerm_ = text;
    searchCaseSensitive_ = caseSensitive;
    searchWholeWord_ = wholeWord;
    searchIsRegex_ = useRegex;
    rebuildSearchMatches();
    if (searchMatches_.empty()) return false;

    // Find last match before cursor
    for (int i = static_cast<int>(searchMatches_.size()) - 1; i >= 0; i--) {
        if (searchMatches_[i] < cursor_) {
            searchMatchIdx_ = i;
            selAnchor_ = searchMatches_[i];
            cursor_ = selAnchor_;
            cursor_.col += searchMatchLengths_[i];
            ensureCursorVisible();
            markDirty();
            return true;
        }
    }
    // Wrap around
    int last = static_cast<int>(searchMatches_.size()) - 1;
    searchMatchIdx_ = last;
    selAnchor_ = searchMatches_[last];
    cursor_ = selAnchor_;
    cursor_.col += searchMatchLengths_[last];
    ensureCursorVisible();
    markDirty();
    return true;
}

bool CodeEditor::replaceNext(const String& find, const String& replace,
                              bool caseSensitive, bool useRegex)
{
    bool selectionMatches = false;
    if (hasSelection()) {
        const String selected = selectedText();
        if (useRegex) {
            ct::Regex::Error err;
            ct::Regex re = ct::Regex::compile(find, caseSensitive ? 0u : ct::Regex::IgnoreCase, &err);
            selectionMatches = re.valid() && re.fullmatch(ct::StringView(selected.data(), selected.size()));
            if (selectionMatches) {
                ct::StringView view(selected.data(), selected.size());
                const String replacement = re.sub(view, ct::StringView(replace.data(), replace.size()), 1);
                TextPos oldCur = cursor_;
                deleteSelection();
                TextPos afterDel = cursor_;
                insertTextAtCursor(replacement);
                recordInsert(afterDel, replacement, oldCur, cursor_);
                hlDirty_ = true;
            }
        } else {
            selectionMatches = selected == find;
            if (selectionMatches) {
                TextPos oldCur = cursor_;
                deleteSelection();
                TextPos afterDel = cursor_;
                insertTextAtCursor(replace);
                recordInsert(afterDel, replace, oldCur, cursor_);
                hlDirty_ = true;
            }
        }
    }
    return findNext(find, caseSensitive, false, useRegex);
}

int CodeEditor::replaceAll(const String& find, const String& replace,
                            bool caseSensitive, bool useRegex)
{
    searchTerm_ = find;
    searchCaseSensitive_ = caseSensitive;
    searchWholeWord_ = false;
    searchIsRegex_ = useRegex;
    rebuildSearchMatches();

    ct::Regex re;
    if (useRegex) {
        ct::Regex::Error err;
        re = ct::Regex::compile(find, caseSensitive ? 0u : ct::Regex::IgnoreCase, &err);
        if (!re.valid()) return 0;
    }

    int count = 0;
    // Replace from bottom to top so positions don't shift
    for (int i = static_cast<int>(searchMatches_.size()) - 1; i >= 0; i--) {
        cursor_ = searchMatches_[i];
        selAnchor_ = cursor_;
        cursor_.col += searchMatchLengths_[i];
        if (useRegex) {
            const String matched = selectedText();
            const String replacement = re.sub(ct::StringView(matched.data(), matched.size()),
                                              ct::StringView(replace.data(), replace.size()), 1);
            deleteSelection();
            insertTextAtCursor(replacement);
        } else {
            deleteSelection();
            insertTextAtCursor(replace);
        }
        count++;
    }

    hlDirty_ = true;
    markDirty();
    return count;
}

void CodeEditor::paintSearchHighlights(PaintContext& ctx, const Rect& abs)
{
    if (searchMatches_.empty()) return;

    float lh = lineHeight_;
    if (lh <= 0) return;
    float textX = abs.x + gutterW_;
    float pad = Theme::instance().textEditPadding;

    for (int idx = 0; idx < static_cast<int>(searchMatches_.size()); idx++) {
        auto& m = searchMatches_[idx];
        if (isLineHidden(m.line)) continue;

        int vRow = logicalToVisualRow(m.line);
        float x = textX + pad + colToPixelX(m.line, m.col) - scrollX_;
        float y = abs.y + vRow * lh - scrollY_;
        int matchEnd = m.col + searchMatchLengths_[idx];
        float w = colToPixelX(m.line, matchEnd) - colToPixelX(m.line, m.col);

        if (y + lh < abs.y || y > abs.y + abs.h) continue;

        if (idx == searchMatchIdx_) {
            ctx.fill.SetColor(200, 140, 40, 100);
        } else {
            ctx.fill.SetColor(120, 100, 40, 60);
        }
        ctx.fill.Rectangle(x, y, w, lh, true);
    }
}

// ── Minimap ──────────────────────────────────────────────────────────────────

float CodeEditor::minimapEffectiveWidth() const
{
    return showMinimap_ ? minimapWidth_ : 0.0f;
}

void CodeEditor::paintMinimap(PaintContext& ctx, const Rect& abs)
{
    if (!showMinimap_) return;

    // ── Layout constants ─────────────────────────────────────────────────
    const float mmW       = minimapWidth_;
    const float mmX       = abs.x + abs.w - mmW;
    const float mmY       = abs.y;
    const float mmH       = abs.h;
    const float charW     = 1.4f;    // px per visual column
    const float lineH     = 3.0f;    // px per line (2px content + 1px gap)
    const float contentH  = 2.0f;    // px of actual colored content per line
    const float leftPad   = 4.0f;    // left margin inside minimap
    const float rightPad  = 2.0f;    // right margin
    const int   maxVisCols = static_cast<int>((mmW - leftPad - rightPad) / charW);

    ctx.pushClip({mmX, mmY, mmW, mmH});

    // ── Background ───────────────────────────────────────────────────────
    ctx.fill.SetColor(25, 28, 34, 240);
    ctx.fill.Rectangle(mmX, mmY, mmW, mmH, true);

    // ── Count visible lines (fold-aware) ─────────────────────────────────
    int lc = lineCount();
    if (lc == 0) { ctx.popClip(); return; }

    // Build visible-line index for O(1) mapping
    ct::Vector<int> visLineMap;  // visLineMap[visIdx] → logical line
    visLineMap.reserve(lc);
    for (int i = 0; i < lc; i++) {
        if (!isLineHidden(i)) visLineMap.push_back(i);
    }
    int visLines = static_cast<int>(visLineMap.size());
    if (visLines == 0) { ctx.popClip(); return; }

    float totalMmContentH = visLines * lineH;

    // ── Scroll offset for minimap ────────────────────────────────────────
    // If total content fits, no scroll. Otherwise, track editor scroll.
    float mmScrollY = 0.0f;
    float totalEditorH = visLines * lineHeight_;
    if (totalMmContentH > mmH && totalEditorH > mmH) {
        float editorMaxScroll = totalEditorH - mmH;
        float mmMaxScroll     = totalMmContentH - mmH;
        float scrollRatio     = (editorMaxScroll > 0) ? scrollY_ / editorMaxScroll : 0.0f;
        scrollRatio = std::max(0.0f, std::min(1.0f, scrollRatio));
        mmScrollY = scrollRatio * mmMaxScroll;
    }

    // ── Viewport slider (behind content for layering) ────────────────────
    {
        float viewTopRatio = scrollY_ / (totalEditorH > 0 ? totalEditorH : 1.0f);
        float viewBotRatio = (scrollY_ + abs.h) / (totalEditorH > 0 ? totalEditorH : 1.0f);
        viewTopRatio = std::max(0.0f, std::min(1.0f, viewTopRatio));
        viewBotRatio = std::max(0.0f, std::min(1.0f, viewBotRatio));

        float sliderY = mmY + viewTopRatio * totalMmContentH - mmScrollY;
        float sliderH = (viewBotRatio - viewTopRatio) * totalMmContentH;
        sliderH = std::max(sliderH, 12.0f);

        // Clamp to minimap area
        if (sliderY < mmY) { sliderH -= (mmY - sliderY); sliderY = mmY; }
        if (sliderY + sliderH > mmY + mmH) sliderH = mmY + mmH - sliderY;

        if (sliderH > 0) {
            ctx.fill.SetColor(180, 195, 220, 30);
            ctx.fill.Rectangle(mmX, sliderY, mmW, sliderH, true);
            // Top/bottom border lines
            ctx.line.SetColor(140, 160, 190, 60);
            ctx.drawLine(mmX + 2, sliderY, mmX + mmW - 2, sliderY);
            ctx.drawLine(mmX + 2, sliderY + sliderH, mmX + mmW - 2, sliderY + sliderH);
        }
    }

    // ── Ensure highlight state is up-to-date ─────────────────────────────
    if (hlDirty_ && highlighter_) {
        reHighlight();
    }

    // ── Render lines ─────────────────────────────────────────────────────
    // Only draw lines visible in the minimap viewport
    int firstMmLine = static_cast<int>(mmScrollY / lineH);
    int lastMmLine  = static_cast<int>((mmScrollY + mmH) / lineH) + 1;
    firstMmLine = std::max(0, firstMmLine);
    lastMmLine  = std::min(visLines - 1, lastMmLine);

    for (int vi = firstMmLine; vi <= lastMmLine; vi++) {
        int logLine = visLineMap[vi];
        float ly = mmY + vi * lineH - mmScrollY;

        // Skip if off-screen
        if (ly + contentH < mmY || ly > mmY + mmH) continue;

        // Current-line highlight in minimap
        if (logLine == cursor_.line) {
            ctx.fill.SetColor(255, 255, 255, 15);
            ctx.fill.Rectangle(mmX, ly, mmW, lineH, true);
        }

        const String& text = lineAt(logLine);
        if (text.empty()) continue;

        // Get syntax spans
        ct::Vector<SyntaxHighlighter::Span> spans;
        if (highlighter_ && logLine < static_cast<int>(lineStates_.size())) {
            int prevState = (logLine > 0) ? lineStates_[logLine - 1] : 0;
            auto result = highlighter_->highlightLine(logLine, text, prevState);
            spans = std::move(result.spans);
        }

        // Walk characters, compute visual columns, draw colored blocks
        int byteIdx = 0;
        int visCol  = 0;
        int textLen = static_cast<int>(text.size());

        // Batch contiguous runs of same color
        Color  runColor(0, 0, 0, 0);
        float  runStartX = 0;
        int    runCols    = 0;

        auto flushRun = [&]() {
            if (runCols > 0) {
                float rx = mmX + leftPad + runStartX * charW;
                float rw = runCols * charW;
                // Clamp to minimap bounds
                if (rx < mmX + mmW - rightPad && rx + rw > mmX + leftPad) {
                    if (rx < mmX + leftPad) { rw -= (mmX + leftPad - rx); rx = mmX + leftPad; }
                    if (rx + rw > mmX + mmW - rightPad) rw = mmX + mmW - rightPad - rx;
                    ctx.fill.SetColor(runColor.r, runColor.g, runColor.b, runColor.a);
                    ctx.fill.Rectangle(rx, ly, rw, contentH, true);
                }
                runCols = 0;
            }
        };

        while (byteIdx < textLen && visCol < maxVisCols) {
            unsigned char ch = static_cast<unsigned char>(text[byteIdx]);

            if (ch == '\t') {
                // Flush current run before gap
                flushRun();
                int tabW = tabSize_ - (visCol % tabSize_);
                visCol += tabW;
                byteIdx++;
                continue;
            }

            if (ch == ' ') {
                // Spaces: flush run, skip
                flushRun();
                visCol++;
                byteIdx++;
                continue;
            }

            // Determine color from syntax spans
            Color c(170, 175, 185, 200);  // default
            if (highlighter_) {
                for (const auto& sp : spans) {
                    if (byteIdx >= sp.startCol && byteIdx < sp.endCol) {
                        c = highlighter_->colorFor(sp.type);
                        c.a = 200;
                        break;
                    }
                }
            }

            // Extend or start a new run
            if (runCols > 0 && c.r == runColor.r && c.g == runColor.g &&
                c.b == runColor.b && c.a == runColor.a) {
                runCols++;
            } else {
                flushRun();
                runColor  = c;
                runStartX = static_cast<float>(visCol);
                runCols   = 1;
            }

            // Advance past UTF-8 sequence
            int seqLen = 1;
            if (ch >= 0xC0 && ch < 0xE0) seqLen = 2;
            else if (ch >= 0xE0 && ch < 0xF0) seqLen = 3;
            else if (ch >= 0xF0) seqLen = 4;
            byteIdx += seqLen;
            visCol++;
        }
        flushRun();
    }

    // ── Separator line ───────────────────────────────────────────────────
    ctx.line.SetColor(50, 55, 65, 255);
    ctx.drawLine(mmX, mmY, mmX, mmY + mmH);

    ctx.popClip();
}

void CodeEditor::paintSearchBar(PaintContext& ctx, const Rect& abs)
{
    if (!searchVisible_) return;

    float barH = replaceVisible_ ? 56.0f : 28.0f;
    float barY = abs.y;
    float barW = std::min(400.0f, abs.w - 20.0f);
    float barX = abs.x + abs.w - barW - 10.0f;

    // Background
    ctx.fill.SetColor(40, 44, 52, 240);
    ctx.fill.RoundedRectangle(barX, barY, barW, barH, 4, 6, true);
    ctx.line.SetColor(80, 85, 95, 255);
    ctx.line.RoundedRectangle(barX, barY, barW, barH, 4, 6, false);

    // Search term
    ctx.font.SetColor(200, 200, 210, 255);
    char buf[256];
    if (searchTerm_.empty()) {
        ctx.font.Print("Search...", barX + 8, barY + 6);
    } else {
        snprintf(buf, sizeof(buf), "%sFind: %s  (%d matches)",
                 searchIsRegex_ ? ".*  " : "",
                 searchTerm_.c_str(), static_cast<int>(searchMatches_.size()));
        ctx.font.Print(buf, barX + 8, barY + 6);
    }

    if (replaceVisible_ && !replaceTerm_.empty()) {
        snprintf(buf, sizeof(buf), "Replace: %s", replaceTerm_.c_str());
        ctx.font.Print(buf, barX + 8, barY + 30);
    }
}

// ── Paint ────────────────────────────────────────────────────────────────────

void CodeEditor::paint(PaintContext& ctx)
{
    if (!visible_) return;

    // Rebuild highlight cache if needed
    if (hlDirty_ && highlighter_) {
        reHighlight();
        rebuildFoldRanges();
    }

    // Reserve gutter space for fold icons (between line numbers and code)
    extraGutterW_ = static_cast<float>(foldGutterWidth());

    // Shrink editor width to leave room for minimap
    float mmW = minimapEffectiveWidth();
    if (mmW > 0) rect_.w -= mmW;

    Rect abs = absoluteRect();

    // Cache font pointer for helper functions and set scale
    lastPaintFont_ = ctx.buFont;
    ctx.font.scale = fontScale_;

    // Paint extras before TextEdit paints
    paintCurrentLineHighlight(ctx, abs);
    paintIndentGuides(ctx, abs);
    paintSearchHighlights(ctx, abs);

    // Delegate to TextEdit for the main rendering
    TextEdit::paint(ctx);

    // Restore font scale for overlays (TextEdit::paint resets it)
    ctx.font.scale = fontScale_;

    // Paint overlays after
    ctx.pushClip(abs);
    paintExtraSelections(ctx, abs);
    paintExtraCursors(ctx, abs);
    ctx.popClip();
    paintWhitespace(ctx, abs);
    paintScopeLines(ctx, abs);
    paintBracketMatch(ctx, abs);
    paintFoldGutter(ctx, abs);

    // Restore default scale for other widgets
    ctx.font.scale = 1.0f;

    // Restore full width and paint minimap
    if (mmW > 0) {
        rect_.w += mmW;
        Rect fullAbs = absoluteRect();
        paintMinimap(ctx, fullAbs);
    }
}

// ── Key handling ─────────────────────────────────────────────────────────────

void CodeEditor::onKeyPress(KeyEvent& e)
{
    // Multi-cursor: Ctrl+D = add cursor for next occurrence
    if (e.ctrl && e.key == ig::retained::Key::D) {
        addCursorForNextOccurrence();
        e.consumed = true;
        return;
    }

    // Escape clears extra cursors
    if (e.key == ig::retained::Key::Escape && !extraCursors_.empty()) {
        clearExtraCursors();
        e.consumed = true;
        return;
    }

    // Ctrl+Shift+L = select all occurrences
    if (e.ctrl && e.shift && e.key == ig::retained::Key::L) {
        selectAllOccurrences();
        e.consumed = true;
        return;
    }

    // Alt+Up/Down = move line up/down
    if (e.alt && !e.ctrl && !e.shift) {
        if (e.key == ig::retained::Key::Up) { moveLineUp(); e.consumed = true; return; }
        if (e.key == ig::retained::Key::Down) { moveLineDown(); e.consumed = true; return; }
    }

    // Ctrl+Shift+K = remove line
    if (e.ctrl && e.shift && e.key == ig::retained::Key::K) {
        removeLine();
        e.consumed = true;
        return;
    }

    // Ctrl+Shift+D = duplicate line
    if (e.ctrl && e.shift && e.key == ig::retained::Key::D) {
        // Ctrl+D without shift is addCursorForNextOccurrence (handled above)
        duplicateLine();
        e.consumed = true;
        return;
    }

    // Ctrl+/ = toggle comment
    if (e.ctrl && e.key == ig::retained::Key::Slash) {
        toggleComment();
        e.consumed = true;
        return;
    }

    // ── Multi-cursor: intercept ALL movement/editing keys ────────────────
    // When extra cursors exist, we handle everything here instead of
    // falling through to TextEdit::onKeyPress (which only knows cursor_).
    if (!extraCursors_.empty()) {
        bool handled = true;
        switch (e.key) {
        case ig::retained::Key::Left:
            moveLeft(e.shift, e.ctrl); break;
        case ig::retained::Key::Right:
            moveRight(e.shift, e.ctrl); break;
        case ig::retained::Key::Up:
            moveUp(1, e.shift); break;
        case ig::retained::Key::Down:
            moveDown(1, e.shift); break;
        case ig::retained::Key::Home:
            if (e.ctrl) moveTop(e.shift);
            else moveHome(e.shift);
            break;
        case ig::retained::Key::End:
            if (e.ctrl) moveBottom(e.shift);
            else moveEnd(e.shift);
            break;
        case ig::retained::Key::Tab:
            if (!readOnly_) {
                // Insert tab at all cursors (reuse onTextInput path)
                KeyEvent tabE;
                std::strcpy(tabE.text, "\t");
                onTextInput(tabE);
            }
            break;
        case ig::retained::Key::Return: case ig::retained::Key::KPEnter:
            clearExtraCursors();
            handled = false; // fall through to normal Enter handler
            break;
        case ig::retained::Key::Backspace: case ig::retained::Key::Delete:
            handled = false; // fall through to multi-cursor backspace handler below
            break;
        case ig::retained::Key::PageUp:
            moveUp(std::max(1, visibleLineCount() - 1), e.shift); break;
        case ig::retained::Key::PageDown:
            moveDown(std::max(1, visibleLineCount() - 1), e.shift); break;
        default:
            // Ctrl+A = select all, clears extra cursors
            if (e.ctrl && e.key == ig::retained::Key::A) {
                clearExtraCursors();
                handled = false; break;
            }
            // Ctrl+C/V/X — fall through to TextEdit
            if (e.ctrl) { handled = false; break; }
            handled = false;
            break;
        }
        if (handled) {
            e.consumed = true;
            return;
        }
    }

    // Enter with auto-indent
    if (autoIndent_ && !readOnly_ &&
        (e.key == ig::retained::Key::Return || e.key == ig::retained::Key::KPEnter))
    {
        // Multi-cursor: clear extras for Enter (too complex to handle per-cursor)
        if (!extraCursors_.empty()) clearExtraCursors();

        TextPos oldCur = cursor_;
        String indent = getLineIndent(cursor_.line);

        // Extra indent after { or :
        if (lineEndsWith(cursor_.line, '{') || lineEndsWith(cursor_.line, ':'))
            indent += "\t";

        // Record for undo
        String insertedText = "\n" + indent;

        splitLine();

        // Insert indent on new line
        if (!indent.empty()) {
            insertTextAtCursor(indent);
        }

        recordInsert(oldCur, insertedText, oldCur, cursor_);
        hlDirty_ = true;
        ensureCursorVisible();
        markDirty();
        e.consumed = true;
        return;
    }

    // Multi-cursor backspace / delete
    if (!extraCursors_.empty() && !readOnly_ && !e.ctrl &&
        (e.key == ig::retained::Key::Backspace || e.key == ig::retained::Key::Delete))
    {
        struct CursorInfo { TextPos pos; TextPos anchor; int idx; };
        ct::Vector<CursorInfo> all;
        all.push_back({cursor_, selAnchor_, -1});
        for (int i = 0; i < static_cast<int>(extraCursors_.size()); i++)
            all.push_back({extraCursors_[i].pos, extraCursors_[i].anchor, i});

        // Sort bottom-to-top
        std::sort(all.begin(), all.end(), [](const CursorInfo& a, const CursorInfo& b) {
            TextPos aMax = a.pos > a.anchor ? a.pos : a.anchor;
            TextPos bMax = b.pos > b.anchor ? b.pos : b.anchor;
            return aMax > bMax;
        });

        for (auto& ci : all) {
            cursor_ = ci.pos;
            selAnchor_ = ci.anchor;

            if (hasSelection()) {
                deleteSelection();
            } else if (e.key == ig::retained::Key::Backspace) {
                if (cursor_.col > 0) {
                    size_t byteEnd   = utf8ByteOffset(lines_[cursor_.line], cursor_.col);
                    cursor_.col--;
                    size_t byteStart = utf8ByteOffset(lines_[cursor_.line], cursor_.col);
                    lines_[cursor_.line].erase(byteStart, byteEnd - byteStart);
                    selAnchor_ = cursor_;
                } else if (cursor_.line > 0) {
                    mergeWithPrevLine();
                }
            } else { // Delete
                if (cursor_.col < lineColCount(cursor_.line)) {
                    size_t bs = utf8ByteOffset(lines_[cursor_.line], cursor_.col);
                    size_t be = utf8ByteOffset(lines_[cursor_.line], cursor_.col + 1);
                    lines_[cursor_.line].erase(bs, be - bs);
                    selAnchor_ = cursor_;
                } else if (cursor_.line < lineCount() - 1) {
                    mergeWithNextLine();
                }
            }
            ci.pos = cursor_;
            ci.anchor = selAnchor_;
        }

        // Write back
        for (auto& ci : all) {
            if (ci.idx == -1) {
                cursor_ = ci.pos;
                selAnchor_ = ci.anchor;
            } else {
                extraCursors_[ci.idx].pos = ci.pos;
                extraCursors_[ci.idx].anchor = ci.anchor;
            }
        }

        hlDirty_ = true;
        invalidateWrap();
        ensureCursorVisible();
        markDirty();
        textChanged.emit();
        cursorMoved.emit(cursor_);
        e.consumed = true;
        return;
    }

    // Record edits for undo
    if (!readOnly_ && !e.ctrl) {
        TextPos before = cursor_;

        if (e.key == ig::retained::Key::Backspace || e.key == ig::retained::Key::Delete) {
            String deletedText;
            if (hasSelection()) {
                deletedText = selectedText();
            } else if (e.key == ig::retained::Key::Backspace && cursor_.col > 0) {
                deletedText = String(1, lineAt(cursor_.line)[cursor_.col - 1]);
            } else if (e.key == ig::retained::Key::Backspace && cursor_.line > 0) {
                deletedText = "\n";
            } else if (e.key == ig::retained::Key::Delete && cursor_.col < lineColCount(cursor_.line)) {
                deletedText = String(1, lineAt(cursor_.line)[cursor_.col]);
            } else if (e.key == ig::retained::Key::Delete && cursor_.line < lineCount() - 1) {
                deletedText = "\n";
            }

            TextEdit::onKeyPress(e);

            if (!deletedText.empty()) {
                recordDelete(cursor_, deletedText, before, cursor_);
                hlDirty_ = true;
            }
            return;
        }
    }

    TextEdit::onKeyPress(e);
}

void CodeEditor::onTextInput(KeyEvent& e)
{
    if (readOnly_) { e.consumed = true; return; }

    TextPos before = cursor_;
    String inputText = e.text;

    if (!extraCursors_.empty() && !inputText.empty()) {
        // Collect all cursor info as VALUES (not pointers)
        struct CursorInfo { TextPos pos; TextPos anchor; int idx; }; // idx: -1 = primary
        ct::Vector<CursorInfo> all;
        all.push_back({cursor_, selAnchor_, -1});
        for (int i = 0; i < static_cast<int>(extraCursors_.size()); i++)
            all.push_back({extraCursors_[i].pos, extraCursors_[i].anchor, i});

        // Sort bottom-to-top (by the END of selection, not just pos)
        std::sort(all.begin(), all.end(), [](const CursorInfo& a, const CursorInfo& b) {
            TextPos aMax = a.pos > a.anchor ? a.pos : a.anchor;
            TextPos bMax = b.pos > b.anchor ? b.pos : b.anchor;
            return aMax > bMax;
        });

        // Process each cursor bottom-to-top
        for (auto& ci : all) {
            cursor_ = ci.pos;
            selAnchor_ = ci.anchor;
            if (hasSelection()) deleteSelection();
            // Direct insert (no signal spam)
            size_t bytePos = utf8ByteOffset(lines_[cursor_.line], cursor_.col);
            lines_[cursor_.line].insert(bytePos, inputText);
            cursor_.col += utf8Length(inputText);
            selAnchor_ = cursor_;
            ci.pos = cursor_;
            ci.anchor = selAnchor_;
        }

        // Write back all positions
        for (auto& ci : all) {
            if (ci.idx == -1) {
                cursor_ = ci.pos;
                selAnchor_ = ci.anchor;
            } else {
                extraCursors_[ci.idx].pos = ci.pos;
                extraCursors_[ci.idx].anchor = ci.anchor;
            }
        }

        hlDirty_ = true;
        invalidateWrap();
        ensureCursorVisible();
        markDirty();
        textChanged.emit();
        cursorMoved.emit(cursor_);
        e.consumed = true;
        return;
    }

    TextEdit::onTextInput(e);

    if (!inputText.empty() && cursor_ != before) {
        recordInsert(before, inputText, before, cursor_);
        reHighlightFrom(before.line);
    }
}

void CodeEditor::onMousePress(MouseEvent& e)
{
    // Check minimap click — use full rect (before width shrink)
    if (showMinimap_) {
        Rect abs = absoluteRect();
        float mmW = minimapWidth_;
        float mmX = abs.x + abs.w - mmW;
        if (e.x >= mmX && e.x <= abs.x + abs.w) {
            minimapDrag_ = true;

            // Count visible lines and compute scroll
            int lc = lineCount();
            int visLines = 0;
            for (int i = 0; i < lc; i++) {
                if (!isLineHidden(i)) visLines++;
            }
            float totalEditorH = visLines * lineHeight_;
            float totalMmH     = visLines * 3.0f;  // lineH = 3.0f
            float maxScroll    = std::max(0.0f, totalEditorH - abs.h);

            // Map click Y → minimap content position → editor scroll
            float mmScrollY = 0.0f;
            if (totalMmH > abs.h && totalEditorH > abs.h) {
                float editorMaxScroll = totalEditorH - abs.h;
                float mmMaxScroll     = totalMmH - abs.h;
                float scrollRatio     = (editorMaxScroll > 0) ? scrollY_ / editorMaxScroll : 0.0f;
                scrollRatio = std::max(0.0f, std::min(1.0f, scrollRatio));
                mmScrollY = scrollRatio * mmMaxScroll;
            }

            // Click position in minimap content space
            float mmContentY = (e.y - abs.y) + mmScrollY;
            // Convert to line index
            int clickedVisLine = static_cast<int>(mmContentY / 3.0f);
            clickedVisLine = std::max(0, std::min(visLines - 1, clickedVisLine));

            // Scroll editor to center clicked line
            float targetScrollY = clickedVisLine * lineHeight_ - abs.h * 0.5f;
            scrollY_ = std::max(0.0f, std::min(maxScroll, targetScrollY));

            markDirty();
            e.consumed = true;
            return;
        }
    }

    // Check fold gutter click (fold icons sit between line numbers and code)
    if (showFolding_ && highlighter_) {
        Rect abs = absoluteRect();
        float fgW = static_cast<float>(foldGutterWidth());
        float fgX = abs.x + gutterW_ - fgW;
        if (e.x >= fgX && e.x < fgX + fgW) {
            float lh = lineHeight_;
            if (lh > 0) {
                int vRow = static_cast<int>((e.y - abs.y + scrollY_) / lh);
                int lc = lineCount();
                int curVRow = 0;
                int clickedLine = -1;
                for (int i = 0; i < lc; i++) {
                    if (isLineHidden(i)) continue;
                    if (curVRow == vRow) { clickedLine = i; break; }
                    curVRow++;
                }
                if (clickedLine >= 0 && clickedLine < lc) {
                    toggleFoldAt(clickedLine);
                    e.consumed = true;
                    return;
                }
            }
        }
    }

    // Alt+Click = add extra cursor
    if (e.alt) {
        // Use hitTest logic: compute line/col from click position
        Rect abs = absoluteRect();
        float localX = e.x - abs.x;
        float localY = e.y - abs.y;
        int vRow = static_cast<int>((localY + scrollY_) / lineHeight_);
        vRow = std::max(0, vRow);
        // Convert vRow to logical line
        int lc = lineCount();
        int curVRow = 0;
        int clickedLine = 0;
        for (int i = 0; i < lc; i++) {
            if (isLineHidden(i)) continue;
            if (curVRow == vRow) { clickedLine = i; break; }
            curVRow++;
            clickedLine = i;
        }
        // Approximate col from X using colToPixelX
        float textX = gutterW_ + Theme::instance().textEditPadding - scrollX_;
        int maxCol = lineColCount(clickedLine);
        int bestCol = maxCol;
        for (int c = 0; c <= maxCol; c++) {
            float px = textX + colToPixelX(clickedLine, c);
            if (px > localX) { bestCol = std::max(0, c - 1); break; }
        }
        addCursor(clickedLine, bestCol);
        e.consumed = true;
        return;
    }

    // Normal click clears extra cursors
    if (!extraCursors_.empty()) clearExtraCursors();

    TextEdit::onMousePress(e);
}

void CodeEditor::onMouseRelease(MouseEvent& e)
{
    if (minimapDrag_) {
        minimapDrag_ = false;
        e.consumed = true;
        return;
    }
    TextEdit::onMouseRelease(e);
}

void CodeEditor::onMouseMove(MouseEvent& e)
{
    if (minimapDrag_ && showMinimap_) {
        Rect abs = absoluteRect();

        int lc = lineCount();
        int visLines = 0;
        for (int i = 0; i < lc; i++) {
            if (!isLineHidden(i)) visLines++;
        }
        float totalEditorH = visLines * lineHeight_;
        float totalMmH     = visLines * 3.0f;
        float maxScroll    = std::max(0.0f, totalEditorH - abs.h);

        float mmScrollY = 0.0f;
        if (totalMmH > abs.h && totalEditorH > abs.h) {
            float editorMaxScroll = totalEditorH - abs.h;
            float mmMaxScroll     = totalMmH - abs.h;
            float scrollRatio     = (editorMaxScroll > 0) ? scrollY_ / editorMaxScroll : 0.0f;
            scrollRatio = std::max(0.0f, std::min(1.0f, scrollRatio));
            mmScrollY = scrollRatio * mmMaxScroll;
        }

        float mmContentY = (e.y - abs.y) + mmScrollY;
        int clickedVisLine = static_cast<int>(mmContentY / 3.0f);
        clickedVisLine = std::max(0, std::min(visLines - 1, clickedVisLine));

        float targetScrollY = clickedVisLine * lineHeight_ - abs.h * 0.5f;
        scrollY_ = std::max(0.0f, std::min(maxScroll, targetScrollY));

        markDirty();
        e.consumed = true;
        return;
    }
    TextEdit::onMouseMove(e);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Movement API
// ═════════════════════════════════════════════════════════════════════════════

void CodeEditor::moveLeft(bool select, bool wordMode)
{
    auto move = [&](TextPos& pos, TextPos& anchor) {
        bool hadSel = (pos != anchor);
        if (!select && hadSel) {
            // Collapse to selection start (VSCode behavior)
            TextPos s = pos < anchor ? pos : anchor;
            pos = s;
            anchor = pos;
            return;
        }
        if (wordMode) {
            TextPos ws = findWordStart(pos);
            if (ws == pos && pos.col > 0) {
                pos.col--;
                ws = findWordStart(pos);
            }
            pos = ws;
        } else {
            if (pos.col > 0) pos.col--;
            else if (pos.line > 0) { pos.line--; pos.col = lineColCount(pos.line); }
        }
        if (!select) anchor = pos;
    };
    move(cursor_, selAnchor_);
    for (auto& ec : extraCursors_) move(ec.pos, ec.anchor);
    mergeCursorsIfNeeded();
    ensureCursorVisible(); markDirty(); cursorMoved.emit(cursor_);
}

void CodeEditor::moveRight(bool select, bool wordMode)
{
    int lc = lineCount();
    auto move = [&](TextPos& pos, TextPos& anchor) {
        bool hadSel = (pos != anchor);
        if (!select && hadSel) {
            // Collapse to selection end (VSCode behavior)
            TextPos e = pos > anchor ? pos : anchor;
            pos = e;
            anchor = pos;
            return;
        }
        if (wordMode) {
            TextPos we = findWordEnd(pos);
            if (we == pos && pos.col < lineColCount(pos.line)) {
                pos.col++;
                we = findWordEnd(pos);
            }
            pos = we;
        } else {
            if (pos.col < lineColCount(pos.line)) pos.col++;
            else if (pos.line < lc - 1) { pos.line++; pos.col = 0; }
        }
        if (!select) anchor = pos;
    };
    move(cursor_, selAnchor_);
    for (auto& ec : extraCursors_) move(ec.pos, ec.anchor);
    mergeCursorsIfNeeded();
    ensureCursorVisible(); markDirty(); cursorMoved.emit(cursor_);
}

void CodeEditor::moveUp(int amount, bool select)
{
    auto move = [&](TextPos& pos, TextPos& anchor) {
        bool hadSel = (pos != anchor);
        if (!select && hadSel) {
            TextPos s = pos < anchor ? pos : anchor;
            pos = s;
            anchor = pos;
            return;
        }
        pos.line = std::max(0, pos.line - amount);
        pos.col  = std::min(pos.col, lineColCount(pos.line));
        if (!select) anchor = pos;
    };
    move(cursor_, selAnchor_);
    for (auto& ec : extraCursors_) move(ec.pos, ec.anchor);
    mergeCursorsIfNeeded();
    ensureCursorVisible(); markDirty(); cursorMoved.emit(cursor_);
}

void CodeEditor::moveDown(int amount, bool select)
{
    int lc = lineCount();
    auto move = [&](TextPos& pos, TextPos& anchor) {
        bool hadSel = (pos != anchor);
        if (!select && hadSel) {
            TextPos e = pos > anchor ? pos : anchor;
            pos = e;
            anchor = pos;
            return;
        }
        pos.line = std::min(lc - 1, pos.line + amount);
        pos.col  = std::min(pos.col, lineColCount(pos.line));
        if (!select) anchor = pos;
    };
    move(cursor_, selAnchor_);
    for (auto& ec : extraCursors_) move(ec.pos, ec.anchor);
    mergeCursorsIfNeeded();
    ensureCursorVisible(); markDirty(); cursorMoved.emit(cursor_);
}

void CodeEditor::moveHome(bool select)
{
    auto move = [&](TextPos& pos, TextPos& anchor) {
        pos.col = 0;
        if (!select) anchor = pos;
    };
    move(cursor_, selAnchor_);
    for (auto& ec : extraCursors_) move(ec.pos, ec.anchor);
    mergeCursorsIfNeeded();
    markDirty(); cursorMoved.emit(cursor_);
}

void CodeEditor::moveEnd(bool select)
{
    auto move = [&](TextPos& pos, TextPos& anchor) {
        pos.col = lineColCount(pos.line);
        if (!select) anchor = pos;
    };
    move(cursor_, selAnchor_);
    for (auto& ec : extraCursors_) move(ec.pos, ec.anchor);
    mergeCursorsIfNeeded();
    markDirty(); cursorMoved.emit(cursor_);
}

void CodeEditor::moveTop(bool select)
{
    cursor_ = {0, 0};
    if (!select) selAnchor_ = cursor_;
    extraCursors_.clear();
    ensureCursorVisible(); markDirty(); cursorMoved.emit(cursor_);
}

void CodeEditor::moveBottom(bool select)
{
    int lc = lineCount();
    cursor_ = {lc - 1, lineColCount(lc - 1)};
    if (!select) selAnchor_ = cursor_;
    extraCursors_.clear();
    ensureCursorVisible(); markDirty(); cursorMoved.emit(cursor_);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Line operations
// ═════════════════════════════════════════════════════════════════════════════

void CodeEditor::moveLineUp()
{
    if (readOnly_ || cursor_.line <= 0) return;
    int ln = cursor_.line;
    std::swap(lines_[ln], lines_[ln - 1]);
    cursor_.line--;
    selAnchor_ = cursor_;
    hlDirty_ = true;
    invalidateWrap();
    ensureCursorVisible(); markDirty(); textChanged.emit();
}

void CodeEditor::moveLineDown()
{
    if (readOnly_ || cursor_.line >= lineCount() - 1) return;
    int ln = cursor_.line;
    std::swap(lines_[ln], lines_[ln + 1]);
    cursor_.line++;
    selAnchor_ = cursor_;
    hlDirty_ = true;
    invalidateWrap();
    ensureCursorVisible(); markDirty(); textChanged.emit();
}

void CodeEditor::removeLine()
{
    if (readOnly_) return;
    int ln = cursor_.line;
    int lc = lineCount();
    if (lc <= 1) { lines_[0].clear(); cursor_ = {0, 0}; }
    else {
        lines_.erase(lines_.begin() + ln);
        if (cursor_.line >= lineCount()) cursor_.line = lineCount() - 1;
        cursor_.col = std::min(cursor_.col, lineColCount(cursor_.line));
    }
    selAnchor_ = cursor_;
    hlDirty_ = true;
    invalidateWrap();
    markDirty(); textChanged.emit();
}

void CodeEditor::duplicateLine()
{
    if (readOnly_) return;
    int ln = cursor_.line;
    lines_.insert(lines_.begin() + ln + 1, lines_[ln]);
    cursor_.line++;
    selAnchor_ = cursor_;
    hlDirty_ = true;
    invalidateWrap();
    markDirty(); textChanged.emit();
}

void CodeEditor::indent()
{
    if (readOnly_) return;
    int ln = cursor_.line;
    lines_[ln].insert(0, "\t");
    cursor_.col++;
    selAnchor_ = cursor_;
    hlDirty_ = true;
    invalidateWrap();
    markDirty(); textChanged.emit();
}

void CodeEditor::unindent()
{
    if (readOnly_) return;
    int ln = cursor_.line;
    if (lines_[ln].empty()) return;
    if (lines_[ln][0] == '\t') {
        lines_[ln].erase(0, 1);
        if (cursor_.col > 0) cursor_.col--;
    } else {
        int spaces = 0;
        while (spaces < tabSize_ && spaces < static_cast<int>(lines_[ln].size()) && lines_[ln][spaces] == ' ')
            spaces++;
        if (spaces > 0) {
            lines_[ln].erase(0, spaces);
            cursor_.col = std::max(0, cursor_.col - spaces);
        }
    }
    selAnchor_ = cursor_;
    hlDirty_ = true;
    invalidateWrap();
    markDirty(); textChanged.emit();
}

String CodeEditor::commentPrefix() const
{
    if (highlighter_) {
        String name = highlighter_->languageName();
        if (name == "Python" || name == "Lua") return "# ";
    }
    return "// ";
}

void CodeEditor::toggleComment()
{
    if (readOnly_) return;
    int ln = cursor_.line;
    String prefix = commentPrefix();
    // Check if line starts with comment prefix (ignoring leading whitespace)
    String& line = lines_[ln];
    size_t ws = line.find_first_not_of(" \t");
    if (ws == String::npos) return;
    if (line.compare(ws, prefix.size(), prefix) == 0) {
        // Remove comment
        line.erase(ws, prefix.size());
        if (cursor_.col > static_cast<int>(ws))
            cursor_.col = std::max(static_cast<int>(ws), cursor_.col - static_cast<int>(prefix.size()));
    } else {
        // Add comment
        line.insert(ws, prefix);
        if (cursor_.col >= static_cast<int>(ws))
            cursor_.col += static_cast<int>(prefix.size());
    }
    selAnchor_ = cursor_;
    hlDirty_ = true;
    invalidateWrap();
    markDirty(); textChanged.emit();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Word utilities
// ═════════════════════════════════════════════════════════════════════════════

bool CodeEditor::isWordChar(char c) const
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

TextEdit::TextPos CodeEditor::findWordStart(TextPos pos) const
{
    clampPos(pos);
    if (pos.col <= 0) return pos;
    const String& line = lineAt(pos.line);
    size_t byte = utf8ByteOffset(line, pos.col);
    if (byte == 0) return pos;
    // Move back over word chars
    while (byte > 0 && isWordChar(line[byte - 1])) byte--;
    // Convert byte offset to codepoint offset
    int col = 0;
    size_t b = 0;
    while (b < byte) { b += utf8CharLen(line.c_str() + b); col++; }
    return {pos.line, col};
}

TextEdit::TextPos CodeEditor::findWordEnd(TextPos pos) const
{
    clampPos(pos);
    const String& line = lineAt(pos.line);
    int maxCol = lineColCount(pos.line);
    if (pos.col >= maxCol) return pos;
    size_t byte = utf8ByteOffset(line, pos.col);
    size_t len = line.size();
    // Move forward over word chars
    while (byte < len && isWordChar(line[byte])) byte++;
    int col = 0;
    size_t b = 0;
    while (b < byte) { b += utf8CharLen(line.c_str() + b); col++; }
    return {pos.line, col};
}

// ═════════════════════════════════════════════════════════════════════════════
//  Multi-cursor
// ═════════════════════════════════════════════════════════════════════════════

void CodeEditor::addCursor(int line, int col)
{
    TextPos p{line, col};
    clampPos(p);
    extraCursors_.push_back({p, p});
    mergeCursorsIfNeeded();
    markDirty();
}

void CodeEditor::addCursorForNextOccurrence()
{
    String sel = selectedText();
    if (sel.empty()) {
        // Select the word under cursor
        TextPos ws = findWordStart(cursor_);
        TextPos we = findWordEnd(cursor_);
        if (ws == we) return;
        selAnchor_ = ws;
        cursor_ = we;
        markDirty();
        return;
    }

    // Search for next occurrence after the last cursor's selection
    TextPos searchFrom = cursor_;
    if (!extraCursors_.empty()) {
        // Use the last extra cursor's end as search start
        auto& last = extraCursors_.back();
        TextPos lastEnd = last.pos > last.anchor ? last.pos : last.anchor;
        if (lastEnd > searchFrom) searchFrom = lastEnd;
    }

    // Search through text
    int lc = lineCount();
    for (int pass = 0; pass < 2; pass++) {
        int startLine = (pass == 0) ? searchFrom.line : 0;
        int endLine   = (pass == 0) ? lc : searchFrom.line + 1;
        for (int i = startLine; i < endLine; i++) {
            const String& line = lineAt(i);
            int startCol = (i == searchFrom.line && pass == 0) ? searchFrom.col : 0;
            size_t startByte = utf8ByteOffset(line, startCol);
            size_t selLen = sel.size();
            size_t found = line.find(sel, startByte);
            if (found != String::npos) {
                // Convert byte pos to col
                int foundCol = 0;
                size_t b = 0;
                while (b < found) { b += utf8CharLen(line.c_str() + b); foundCol++; }
                int foundEndCol = foundCol + utf8Length(sel);
                // Check it's not already a cursor
                TextPos ns{i, foundCol}, ne{i, foundEndCol};
                bool duplicate = (selectionStart() == ns && selectionEnd() == ne);
                for (auto& ec : extraCursors_) {
                    TextPos es = ec.pos < ec.anchor ? ec.pos : ec.anchor;
                    TextPos ee = ec.pos > ec.anchor ? ec.pos : ec.anchor;
                    if (es == ns && ee == ne) { duplicate = true; break; }
                }
                if (!duplicate) {
                    extraCursors_.push_back({ne, ns});
                    markDirty();
                    return;
                }
            }
        }
    }
}

void CodeEditor::selectAllOccurrences()
{
    String sel = selectedText();
    if (sel.empty()) return;

    extraCursors_.clear();
    int lc = lineCount();
    bool first = true;
    for (int i = 0; i < lc; i++) {
        const String& line = lineAt(i);
        size_t pos = 0;
        while ((pos = line.find(sel, pos)) != String::npos) {
            int col = 0;
            size_t b = 0;
            while (b < pos) { b += utf8CharLen(line.c_str() + b); col++; }
            int endCol = col + utf8Length(sel);
            TextPos s{i, col}, e{i, endCol};
            if (first) {
                selAnchor_ = s;
                cursor_ = e;
                first = false;
            } else {
                extraCursors_.push_back({e, s});
            }
            pos += sel.size();
        }
    }
    markDirty();
}

void CodeEditor::clearExtraCursors()
{
    extraCursors_.clear();
    markDirty();
}

TextEdit::TextPos CodeEditor::cursorPosAt(int idx) const
{
    if (idx == 0) return cursor_;
    int ei = idx - 1;
    if (ei >= 0 && ei < static_cast<int>(extraCursors_.size()))
        return extraCursors_[ei].pos;
    return cursor_;
}

bool CodeEditor::anyCursorHasSelection() const
{
    if (hasSelection()) return true;
    for (auto& ec : extraCursors_)
        if (ec.pos != ec.anchor) return true;
    return false;
}

bool CodeEditor::allCursorsHaveSelection() const
{
    if (!hasSelection()) return false;
    for (auto& ec : extraCursors_)
        if (ec.pos == ec.anchor) return false;
    return true;
}

void CodeEditor::mergeCursorsIfNeeded()
{
    // Sort extra cursors by position
    std::sort(extraCursors_.begin(), extraCursors_.end(),
        [](const CursorState& a, const CursorState& b) { return a.pos < b.pos; });

    // Remove duplicates that overlap with primary cursor
    auto it = std::remove_if(extraCursors_.begin(), extraCursors_.end(),
        [this](const CursorState& ec) { return ec.pos == cursor_; });
    extraCursors_.erase(it, extraCursors_.end());

    // Remove mutual duplicates
    for (size_t i = 1; i < extraCursors_.size(); ) {
        if (extraCursors_[i].pos == extraCursors_[i - 1].pos) {
            extraCursors_.erase(extraCursors_.begin() + static_cast<ptrdiff_t>(i));
        } else {
            i++;
        }
    }
}

void CodeEditor::applyToCursors(ct::Function<void(TextPos& pos, TextPos& anchor)> fn)
{
    fn(cursor_, selAnchor_);
    for (auto& ec : extraCursors_)
        fn(ec.pos, ec.anchor);
    mergeCursorsIfNeeded();
}

// ── Multi-cursor paint ───────────────────────────────────────────────────────

void CodeEditor::paintExtraCursors(PaintContext& ctx, const Rect& abs)
{
    if (extraCursors_.empty()) return;
    if (!isFocused() || readOnly_) return;
    if (blinkTimer_ >= 0.5f) return;

    const auto& t = Theme::instance();
    float pad = t.textEditPadding;
    float textX = abs.x + gutterW_;

    for (auto& ec : extraCursors_) {
        int vRow = logicalToVisualRow(ec.pos.line);
        float cy = abs.y + vRow * lineHeight_ - scrollY_;
        if (cy + lineHeight_ < abs.y || cy > abs.y + abs.h) continue;

        float cx = textX + pad + colToPixelX(ec.pos.line, ec.pos.col) - scrollX_;
        if (cx >= textX && cx <= abs.x + abs.w) {
            ctx.line.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
            ctx.line.Line2D(static_cast<int>(cx), static_cast<int>(cy + 1),
                            static_cast<int>(cx), static_cast<int>(cy + lineHeight_ - 1));
        }
    }
}

void CodeEditor::paintExtraSelections(PaintContext& ctx, const Rect& abs)
{
    if (extraCursors_.empty()) return;
    if (!isFocused()) return;

    const auto& t = Theme::instance();
    float pad = t.textEditPadding;
    float textX = abs.x + gutterW_;

    for (auto& ec : extraCursors_) {
        if (ec.pos == ec.anchor) continue;
        TextPos s = ec.pos < ec.anchor ? ec.pos : ec.anchor;
        TextPos e = ec.pos > ec.anchor ? ec.pos : ec.anchor;

        for (int i = s.line; i <= e.line; i++) {
            int vRow = logicalToVisualRow(i);
            float ly = abs.y + vRow * lineHeight_ - scrollY_;
            if (ly + lineHeight_ < abs.y || ly > abs.y + abs.h) continue;

            int selS = (i == s.line) ? s.col : 0;
            int selE = (i == e.line) ? e.col : lineColCount(i);
            if (selS >= selE) continue;

            float sx1 = textX + pad + colToPixelX(i, selS) - scrollX_;
            float sx2 = textX + pad + colToPixelX(i, selE) - scrollX_;
            if (i < e.line) sx2 += 6; // trailing newline visual
            float cx1 = std::max(sx1, textX);
            float cx2 = std::min(sx2, abs.x + abs.w);
            if (cx1 < cx2) {
                ctx.fill.SetColor(t.selectionColor.r, t.selectionColor.g,
                                  t.selectionColor.b, t.selectionColor.a);
                ctx.fill.Rectangle(static_cast<int>(cx1), static_cast<int>(ly),
                                   static_cast<int>(cx2 - cx1), static_cast<int>(lineHeight_), true);
            }
        }
    }
}
