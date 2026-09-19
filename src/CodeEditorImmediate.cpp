#include "igui/CodeEditor.hpp"
#include "igui/Gui.hpp"

#include <ct/hashset.hpp>
#include <ct/regex.hpp>
#include <ct/sort.hpp>
#include <stdio.h>

namespace ig
{

namespace
{
inline bool isIdentChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}
} // anonymous namespace

// ═════════════════════════════════════════════════════════════════════════════
//  CodeEditorState — buffer, undo/redo, editing primitives
// ═════════════════════════════════════════════════════════════════════════════

CodeEditorState::CodeEditorState()
{
    lines_.push_back(String());
}

CodeEditorState::~CodeEditorState() = default;

void CodeEditorState::setText(const String& text)
{
    lines_.clear();
    String current;
    for (String::size_type i = 0; i < text.size(); ++i)
    {
        char c = text[i];
        if (c == '\n')
        {
            lines_.push_back(current);
            current.clear();
        }
        else if (c != '\r')
        {
            current.push_back(c);
        }
    }
    lines_.push_back(current);

    cursorLine = 0;
    cursorColumn = 0;
    clearSelection();
    clearExtraCursors();
    scrollLine = 0;
    completion = CodeEditorCompletion();
    clearUndoHistory();
    highlight_.rehighlightAll(lineCount(), [this](int i) -> const String& { return lineAt(i); });
    rebuildFoldRanges();
}

String CodeEditorState::text() const
{
    String out;
    for (size_t i = 0; i < lines_.size(); ++i)
    {
        if (i != 0) out.push_back('\n');
        out.append(lines_[i]);
    }
    return out;
}

void CodeEditorState::setHighlighter(syntax::SyntaxHighlighter* hl)
{
    highlight_.setHighlighter(hl);
    rehighlightFrom(0);
}

void CodeEditorState::setLanguage(const syntax::SyntaxLanguage& language)
{
    setHighlighter(new syntax::DefinedHighlighter(language));
}

syntax::SyntaxHighlighter* CodeEditorState::setHighlighterForFile(const String& filename)
{
    syntax::SyntaxHighlighter* hl = highlight_.setHighlighterForFile(filename);
    rehighlightFrom(0);
    return hl;
}

void CodeEditorState::rehighlightFrom(int line)
{
    highlight_.rehighlightFrom(line, lineCount(), [this](int i) -> const String& { return lineAt(i); });
    rebuildFoldRanges();
}

void CodeEditorState::rebuildFoldRanges()
{
    visibleLinesDirty_ = true;
    // Remember which regions were collapsed so a rebuild after an edit
    // doesn't silently re-expand everything the user folded.
    ct::Vector<CodeEditorFoldRange> previous = foldRanges_;
    foldRanges_.clear();

    syntax::SyntaxHighlighter* hl = highlighter();
    if (!hl) return;

    ct::Vector<int> openStack; // line indices that opened a still-unclosed region
    const int lc = lineCount();
    for (int i = 0; i < lc; ++i)
    {
        int delta = hl->foldDelta(i, lineAt(i));
        for (int d = 0; d < delta; ++d) openStack.push_back(i);
        for (int d = 0; d > delta; --d)
        {
            if (openStack.empty()) break;
            int startLine = openStack.back();
            openStack.pop_back();
            foldRanges_.push_back({startLine, i, false});
        }
    }
    // Any region still open at EOF (unbalanced source) closes at the last line.
    while (!openStack.empty())
    {
        int startLine = openStack.back();
        openStack.pop_back();
        foldRanges_.push_back({startLine, lc - 1, false});
    }

    for (auto& range : foldRanges_)
        for (const auto& old : previous)
            if (old.startLine == range.startLine && old.endLine == range.endLine)
            {
                range.collapsed = old.collapsed;
                break;
            }
}

const ct::Vector<int>& CodeEditorState::visibleLines() const
{
    if (!visibleLinesDirty_) return visibleLines_;
    const int count = lineCount();
    // Difference array merges nested/overlapping collapsed ranges in O(lines+folds).
    ct::Vector<int> changes;
    changes.resize(static_cast<size_t>(count)+1);
    for (size_t i=0; i<changes.size(); ++i) changes[i]=0;
    for (const auto& range : foldRanges_) {
        if (!range.collapsed || range.endLine <= range.startLine) continue;
        ++changes[static_cast<size_t>(range.startLine+1)];
        --changes[static_cast<size_t>(range.endLine+1)];
    }
    visibleLines_.clear();
    visibleLines_.reserve(static_cast<size_t>(count));
    int depth=0;
    for (int line=0; line<count; ++line) {
        depth += changes[static_cast<size_t>(line)];
        if (depth == 0) visibleLines_.push_back(line);
    }
    visibleLinesDirty_ = false;
    return visibleLines_;
}

bool CodeEditorState::isFoldHeader(int line) const
{
    for (const auto& range : foldRanges_)
        if (range.startLine == line) return true;
    return false;
}

bool CodeEditorState::isLineHidden(int line) const
{
    for (const auto& range : foldRanges_)
        if (range.collapsed && line > range.startLine && line <= range.endLine)
            return true;
    return false;
}

void CodeEditorState::toggleFoldAt(int line)
{
    for (auto& range : foldRanges_)
        if (range.startLine == line) { range.collapsed = !range.collapsed; visibleLinesDirty_ = true; return; }
}

void CodeEditorState::foldAll()
{
    visibleLinesDirty_ = true;
    for (auto& range : foldRanges_) range.collapsed = true;
}

void CodeEditorState::unfoldAll()
{
    visibleLinesDirty_ = true;
    for (auto& range : foldRanges_) range.collapsed = false;
}

void CodeEditorState::clampCursor()
{
    if (cursorLine < 0) cursorLine = 0;
    if (cursorLine >= lineCount()) cursorLine = lineCount() - 1;
    const int len = static_cast<int>(lineAt(cursorLine).size());
    if (cursorColumn < 0) cursorColumn = 0;
    if (cursorColumn > len) cursorColumn = len;
}

void CodeEditorState::orderedSelection(int& startLine, int& startColumn, int& endLine, int& endColumn) const
{
    int aLine = selectionAnchorLine, aCol = selectionAnchorColumn;
    int bLine = cursorLine, bCol = cursorColumn;
    if (aLine > bLine || (aLine == bLine && aCol > bCol))
    {
        startLine = bLine; startColumn = bCol;
        endLine = aLine; endColumn = aCol;
    }
    else
    {
        startLine = aLine; startColumn = aCol;
        endLine = bLine; endColumn = bCol;
    }
}

namespace
{
String textBetween(const CodeEditorState& state, int sl, int sc, int el, int ec)
{
    if (sl == el) return state.lineAt(sl).substr(static_cast<size_t>(sc), static_cast<size_t>(ec - sc));
    String out = state.lineAt(sl).substr(static_cast<size_t>(sc));
    for (int i = sl + 1; i < el; ++i)
    {
        out.push_back('\n');
        out.append(state.lineAt(i));
    }
    out.push_back('\n');
    out.append(state.lineAt(el).substr(0, static_cast<size_t>(ec)));
    return out;
}
} // anonymous namespace

String CodeEditorState::selectedText() const
{
    if (!hasSelection()) return String();
    int sl, sc, el, ec;
    orderedSelection(sl, sc, el, ec);
    return textBetween(*this, sl, sc, el, ec);
}

// ── Find / replace ─────────────────────────────────────────────────────────

namespace
{
struct FindMatch { int line, column, length; };

// Collects every match across the whole document, line by line (a match
// never spans a line break - see the CodeEditor.hpp contract note).
// Returns an empty vector when pattern is empty, or (for useRegex) invalid.
ct::Vector<FindMatch> collectMatches(const CodeEditorState& state, const String& pattern,
                                     bool caseSensitive, bool useRegex)
{
    ct::Vector<FindMatch> matches;
    if (pattern.empty()) return matches;

    if (useRegex)
    {
        unsigned flags = caseSensitive ? 0u : ct::Regex::IgnoreCase;
        ct::Regex::Error err;
        ct::Regex re = ct::Regex::compile(pattern, flags, &err);
        if (!re.valid()) return matches;

        for (int line = 0; line < state.lineCount(); ++line)
        {
            const String& text = state.lineAt(line);
            ct::StringView view(text.data(), text.size());
            size_t pos = 0;
            ct::Match m;
            while (pos <= view.size() && re.search(view, &m, pos))
            {
                size_t start = m.start(), end = m.end();
                matches.push_back({line, static_cast<int>(start), static_cast<int>(end - start)});
                pos = end;
                if (start == end) ++pos; // never loop on a zero-width match
            }
        }
        return matches;
    }

    for (int line = 0; line < state.lineCount(); ++line)
    {
        const String& text = state.lineAt(line);
        size_t pos = 0;
        while (true)
        {
            size_t found = caseSensitive
                ? text.find(pattern, pos)
                : String::npos; // filled in below for the case-insensitive path
            if (!caseSensitive)
            {
                // Byte-wise ASCII-fold search; matches Syntax.cpp's own
                // no-locale-<cctype> stance (see cmake/CheckNoStd.cmake).
                auto foldChar = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
                found = String::npos;
                if (pattern.size() <= text.size())
                {
                    for (size_t i = pos; i + pattern.size() <= text.size(); ++i)
                    {
                        bool match = true;
                        for (size_t j = 0; j < pattern.size(); ++j)
                            if (foldChar(text[i + j]) != foldChar(pattern[j])) { match = false; break; }
                        if (match) { found = i; break; }
                    }
                }
            }
            if (found == String::npos) break;
            matches.push_back({line, static_cast<int>(found), static_cast<int>(pattern.size())});
            pos = found + 1;
        }
    }
    return matches;
}
} // anonymous namespace

bool CodeEditorState::findNext(const String& pattern, bool caseSensitive, bool useRegex)
{
    ct::Vector<FindMatch> matches = collectMatches(*this, pattern, caseSensitive, useRegex);
    if (matches.empty()) return false;

    for (const auto& m : matches)
    {
        if (m.line > cursorLine || (m.line == cursorLine && m.column >= cursorColumn))
        {
            selectionAnchorLine = m.line; selectionAnchorColumn = m.column;
            cursorLine = m.line; cursorColumn = m.column + m.length;
            breakUndoCoalescing();
            return true;
        }
    }
    // Wrap around to the first match in the document.
    selectionAnchorLine = matches[0].line; selectionAnchorColumn = matches[0].column;
    cursorLine = matches[0].line; cursorColumn = matches[0].column + matches[0].length;
    breakUndoCoalescing();
    return true;
}

bool CodeEditorState::findPrev(const String& pattern, bool caseSensitive, bool useRegex)
{
    ct::Vector<FindMatch> matches = collectMatches(*this, pattern, caseSensitive, useRegex);
    if (matches.empty()) return false;

    for (size_t i = matches.size(); i-- > 0; )
    {
        const auto& m = matches[i];
        if (m.line < cursorLine || (m.line == cursorLine && m.column < cursorColumn))
        {
            selectionAnchorLine = m.line; selectionAnchorColumn = m.column;
            cursorLine = m.line; cursorColumn = m.column + m.length;
            breakUndoCoalescing();
            return true;
        }
    }
    // Wrap around to the last match in the document.
    const auto& last = matches.back();
    selectionAnchorLine = last.line; selectionAnchorColumn = last.column;
    cursorLine = last.line; cursorColumn = last.column + last.length;
    breakUndoCoalescing();
    return true;
}

int CodeEditorState::replaceAll(const String& pattern, const String& replacement,
                                bool caseSensitive, bool useRegex)
{
    ct::Vector<FindMatch> matches = collectMatches(*this, pattern, caseSensitive, useRegex);
    if (matches.empty()) return 0;

    ct::Regex re;
    if (useRegex)
    {
        ct::Regex::Error err;
        re = ct::Regex::compile(pattern, caseSensitive ? 0u : ct::Regex::IgnoreCase, &err);
        if (!re.valid()) return 0;
    }

    // Replace from the last match to the first so earlier positions never
    // shift under matches still to be processed.
    ++editGroup_;
    lastEditWasTyping_ = false;
    for (size_t i = matches.size(); i-- > 0; )
    {
        const auto& m = matches[i];
        String matched = lineAt(m.line).substr(static_cast<size_t>(m.column), static_cast<size_t>(m.length));
        String replaced = useRegex
            ? re.sub(ct::StringView(matched.data(), matched.size()),
                    ct::StringView(replacement.data(), replacement.size()), 1)
            : replacement;
        eraseRange(m.line, m.column, m.line, m.column + m.length);
        insertText(m.line, m.column, replaced);
    }
    return static_cast<int>(matches.size());
}

// ── Multi-cursor (Ctrl+D) ─────────────────────────────────────────────────

namespace
{
// True if [line, col..col+length) exactly coincides with an existing
// cursor's selection (primary or extra) - used so repeated Ctrl+D never
// re-adds an occurrence already covered.
bool matchAlreadyCovered(const CodeEditorState& state, int line, int col, int length)
{
    int sl, sc, el, ec;
    state.orderedSelection(sl, sc, el, ec);
    if (state.hasSelection() && sl == line && sc == col && el == line && ec == col + length) return true;
    for (int i = 0; i < state.extraCursorCount(); ++i)
    {
        const CodeEditorCursor& c = state.extraCursorAt(i);
        int csl = c.line, csc = c.column, cel = c.anchorLine, cec = c.anchorColumn;
        if (csl > cel || (csl == cel && csc > cec)) { int tl = csl, tc = csc; csl = cel; csc = cec; cel = tl; cec = tc; }
        if (csl == line && csc == col && cel == line && cec == col + length) return true;
    }
    return false;
}
} // anonymous namespace

void CodeEditorState::addNextOccurrenceCursor()
{
    if (!hasSelection())
    {
        const String& line = lineAt(cursorLine);
        int start = cursorColumn, end = cursorColumn;
        while (start > 0 && isIdentChar(line[static_cast<size_t>(start - 1)])) --start;
        while (end < static_cast<int>(line.size()) && isIdentChar(line[static_cast<size_t>(end)])) ++end;
        if (start == end) { breakUndoCoalescing(); return; }
        selectionAnchorLine = cursorLine;
        selectionAnchorColumn = start;
        cursorColumn = end;
        breakUndoCoalescing();
        return;
    }

    int sl, sc, el, ec;
    orderedSelection(sl, sc, el, ec);
    const String needle = textBetween(*this, sl, sc, el, ec);
    if (needle.empty() || needle.find('\n') != String::npos) { breakUndoCoalescing(); return; }

    ct::Vector<FindMatch> matches = collectMatches(*this, needle, true, false);
    if (matches.empty()) { breakUndoCoalescing(); return; }

    // Search strictly after the end of the primary's selection (which always
    // holds the most-recently-added cursor); wrap to the first uncovered
    // match if nothing qualifies after it.
    const FindMatch* found = nullptr;
    const FindMatch* firstUncovered = nullptr;
    for (const auto& m : matches)
    {
        if (matchAlreadyCovered(*this, m.line, m.column, m.length)) continue;
        if (!firstUncovered) firstUncovered = &m;
        if (m.line > el || (m.line == el && m.column >= ec)) { found = &m; break; }
    }
    if (!found) found = firstUncovered;
    if (!found) { breakUndoCoalescing(); return; }

    CodeEditorCursor preserved;
    preserved.line = cursorLine; preserved.column = cursorColumn;
    preserved.anchorLine = selectionAnchorLine; preserved.anchorColumn = selectionAnchorColumn;
    extraCursors_.push_back(preserved);

    selectionAnchorLine = found->line;
    selectionAnchorColumn = found->column;
    cursorLine = found->line;
    cursorColumn = found->column + found->length;
    breakUndoCoalescing();
}

// ── Raw buffer mutation (no undo bookkeeping) ─────────────────────────────
// Both insertText/eraseRange funnel through these two, which is where the
// undo stack is actually written to; see the public wrappers below.

namespace
{
// Splits text on '\n' and splices it into lines[] at (line, column),
// leaving the cursor position (endLine, endColumn) just past the insertion.
void spliceInsert(ct::Vector<String>& lines, int line, int column, const String& text,
                   int& endLine, int& endColumn)
{
    const String& original = lines[static_cast<size_t>(line)];
    String before = original.substr(0, static_cast<size_t>(column));
    String after = original.substr(static_cast<size_t>(column));

    ct::Vector<String> inserted;
    String current = before;
    for (String::size_type i = 0; i < text.size(); ++i)
    {
        char c = text[i];
        if (c == '\n')
        {
            inserted.push_back(current);
            current.clear();
        }
        else if (c != '\r')
        {
            current.push_back(c);
        }
    }
    current.append(after);
    inserted.push_back(current);

    endLine = line + static_cast<int>(inserted.size()) - 1;
    endColumn = static_cast<int>(inserted.back().size() - after.size());

    lines[static_cast<size_t>(line)] = inserted[0];
    for (size_t i = 1; i < inserted.size(); ++i)
        lines.insert(lines.begin() + (line + static_cast<int>(i)), inserted[i]);
}

// Removes [startLine,startColumn) .. (endLine,endColumn) from lines[],
// merging the two edges into one line, and returns the removed text.
// Given text starting at (line, column), returns the (line, column) just
// past its end - i.e. where spliceInsert's own endLine/endColumn output
// would land, without actually touching the buffer. Used to turn an edit's
// stored (line, column, text) into the range spliceErase needs to undo it.
void textEndPosition(int line, int column, const String& text, int& endLine, int& endColumn)
{
    endLine = line;
    endColumn = column;
    for (String::size_type i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\n') { ++endLine; endColumn = 0; }
        else ++endColumn;
    }
}

String spliceErase(ct::Vector<String>& lines, int startLine, int startColumn, int endLine, int endColumn)
{
    if (startLine == endLine)
    {
        String& ln = lines[static_cast<size_t>(startLine)];
        String removed = ln.substr(static_cast<size_t>(startColumn), static_cast<size_t>(endColumn - startColumn));
        ln.erase(static_cast<size_t>(startColumn), static_cast<size_t>(endColumn - startColumn));
        return removed;
    }

    String removed = lines[static_cast<size_t>(startLine)].substr(static_cast<size_t>(startColumn));
    for (int i = startLine + 1; i < endLine; ++i)
    {
        removed.push_back('\n');
        removed.append(lines[static_cast<size_t>(i)]);
    }
    removed.push_back('\n');
    removed.append(lines[static_cast<size_t>(endLine)].substr(0, static_cast<size_t>(endColumn)));

    String merged = lines[static_cast<size_t>(startLine)].substr(0, static_cast<size_t>(startColumn));
    merged.append(lines[static_cast<size_t>(endLine)].substr(static_cast<size_t>(endColumn)));
    lines[static_cast<size_t>(startLine)] = merged;
    lines.erase(lines.begin() + (startLine + 1), lines.begin() + (endLine + 1));
    return removed;
}
} // anonymous namespace

void CodeEditorState::recordEdit(CodeEditorEdit::Kind kind, int line, int column,
                                 const String& removed, const String& inserted,
                                 int beforeLine, int beforeColumn, bool coalesce)
{
    redoStack_.clear();

    if (coalesce && lastEditWasTyping_ && !undoStack_.empty())
    {
        CodeEditorEdit& top = undoStack_.back();
        if (top.kind == kind && top.group == editGroup_ &&
            top.line == beforeLine && top.column + static_cast<int>(top.insertedText.size()) == column &&
            kind == CodeEditorEdit::Insert && inserted.find('\n') == String::npos && inserted.size() == 1)
        {
            top.insertedText.append(inserted);
            top.cursorLineAfter = cursorLine;
            top.cursorColumnAfter = cursorColumn;
            return;
        }
    }

    CodeEditorEdit edit;
    edit.kind = kind;
    edit.line = line;
    edit.column = column;
    edit.removedText = removed;
    edit.insertedText = inserted;
    edit.cursorLineBefore = beforeLine;
    edit.cursorColumnBefore = beforeColumn;
    edit.cursorLineAfter = cursorLine;
    edit.cursorColumnAfter = cursorColumn;
    edit.group = editGroup_;
    undoStack_.push_back(edit);
}

// bumpGroup is false only when called from the multi-cursor entry points
// below, which bump editGroup_ once for the whole action instead of once
// per cursor - see insertTextAtAllCursors/eraseRangeAtAllCursors.
void CodeEditorState::insertTextImpl(int line, int column, const String& text, bool bumpGroup)
{
    if (text.empty()) return;
    const int beforeLine = cursorLine, beforeColumn = cursorColumn;
    int endLine, endColumn;
    spliceInsert(lines_, line, column, text, endLine, endColumn);
    cursorLine = endLine;
    cursorColumn = endColumn;
    clearSelection();

    const bool isPlainTyping = text.size() == 1 && text[0] != '\n';
    if (bumpGroup && !isPlainTyping) ++editGroup_;
    recordEdit(CodeEditorEdit::Insert, line, column, String(), text, beforeLine, beforeColumn, isPlainTyping);
    lastEditWasTyping_ = isPlainTyping;

    rehighlightFrom(line);
}

void CodeEditorState::insertText(int line, int column, const String& text)
{
    insertTextImpl(line, column, text, true);
}

String CodeEditorState::eraseRangeImpl(int startLine, int startColumn, int endLine, int endColumn, bool bumpGroup)
{
    if (startLine > endLine || (startLine == endLine && startColumn > endColumn))
    {
        int tl = startLine, tc = startColumn;
        startLine = endLine; startColumn = endColumn;
        endLine = tl; endColumn = tc;
    }
    if (startLine == endLine && startColumn == endColumn) return String();

    const int beforeLine = cursorLine, beforeColumn = cursorColumn;
    String removed = spliceErase(lines_, startLine, startColumn, endLine, endColumn);
    cursorLine = startLine;
    cursorColumn = startColumn;
    clearSelection();

    if (bumpGroup) ++editGroup_;
    recordEdit(CodeEditorEdit::Delete, startLine, startColumn, removed, String(), beforeLine, beforeColumn, false);
    lastEditWasTyping_ = false;

    rehighlightFrom(startLine);
    return removed;
}

String CodeEditorState::eraseRange(int startLine, int startColumn, int endLine, int endColumn)
{
    return eraseRangeImpl(startLine, startColumn, endLine, endColumn, true);
}

bool CodeEditorState::eraseSelection()
{
    if (!hasSelection()) return false;
    int sl, sc, el, ec;
    orderedSelection(sl, sc, el, ec);
    eraseRange(sl, sc, el, ec);
    return true;
}

// ── Multi-cursor editing entry points ─────────────────────────────────────
// Approach: temporarily point the primary cursor/selection fields at the
// cursor being processed, run it through the existing single-cursor
// primitive (which updates cursorLine/cursorColumn to the post-edit
// position), stash that back into the cursor's slot, move on. Cursors are
// processed bottom-to-top (descending document order) so an edit never
// shifts the (line, column) of a cursor still waiting its turn - except
// when two cursors share a line, where an edit still shifts every
// already-processed result to its right on that same line; see
// shiftResultsAfterEdit below, which corrects for exactly that.

namespace
{
// extraIndex is -1 for the primary, else the entry's slot in extraCursors_ -
// kept so results can be written back to the right place after sorting.
struct AllCursorsEntry { int line, column, anchorLine, anchorColumn; int extraIndex; };
// Where each entry's cursor ended up after its own edit; adjusted in place
// as later (further left/up) entries' edits shift it.
struct AllCursorsResult { int line, column; int extraIndex; };

ct::Vector<AllCursorsEntry> collectCursorsDescending(const CodeEditorState& state)
{
    ct::Vector<AllCursorsEntry> entries;
    entries.push_back({state.cursorLine, state.cursorColumn, state.selectionAnchorLine, state.selectionAnchorColumn, -1});
    for (int i = 0; i < state.extraCursorCount(); ++i)
    {
        const CodeEditorCursor& c = state.extraCursorAt(i);
        entries.push_back({c.line, c.column, c.anchorLine, c.anchorColumn, i});
    }
    ct::sort(entries.begin(), entries.end(), [](const AllCursorsEntry& a, const AllCursorsEntry& b) {
        if (a.line != b.line) return a.line > b.line;
        return a.column > b.column;
    });
    return entries;
}

// An edit removed [*, oldEndLine/oldEndColumn) and left the cursor at
// (newEndLine, newEndColumn) - shift every already-processed result that
// was at or after the removed span's end by the same amount, so a
// same-line cursor to the left processed afterward doesn't invalidate an
// already-recorded result to its right.
void shiftResultsAfterEdit(ct::Vector<AllCursorsResult>& results,
                           int oldEndLine, int oldEndColumn, int newEndLine, int newEndColumn)
{
    const int lineDelta = newEndLine - oldEndLine;
    for (auto& r : results)
    {
        if (r.line < oldEndLine || (r.line == oldEndLine && r.column < oldEndColumn)) continue;
        if (r.line == oldEndLine) r.column = newEndColumn + (r.column - oldEndColumn);
        r.line += lineDelta;
    }
}
} // anonymous namespace

template <typename ResultVector>
void CodeEditorState::applyMultiCursorResults(const ResultVector& results)
{
    for (const auto& r : results)
    {
        if (r.extraIndex >= 0)
        {
            CodeEditorCursor& c = extraCursors_[static_cast<size_t>(r.extraIndex)];
            c.line = r.line; c.column = r.column;
            c.anchorLine = r.line; c.anchorColumn = r.column;
        }
        else
        {
            cursorLine = r.line; cursorColumn = r.column;
        }
    }
    clearSelection();
}

void CodeEditorState::insertTextAtAllCursors(const String& text)
{
    if (extraCursors_.empty()) { insertText(cursorLine, cursorColumn, text); return; }

    // Mirrors insertText()'s own rule (see insertTextImpl) so typing over a
    // multi-cursor selection - eraseSelectionsAtAllCursors() then this -
    // stays one undo group exactly like the single-cursor path does: the
    // erase bumps the group, a plain-typing insert right after does not.
    const bool isPlainTyping = text.size() == 1 && text[0] != '\n';
    if (!isPlainTyping) ++editGroup_;
    ct::Vector<AllCursorsEntry> entries = collectCursorsDescending(*this);
    ct::Vector<AllCursorsResult> results;
    for (const auto& entry : entries) results.push_back({entry.line, entry.column, entry.extraIndex});

    for (size_t i = 0; i < entries.size(); ++i)
    {
        AllCursorsResult& mine = results[i];
        cursorLine = mine.line; cursorColumn = mine.column;
        selectionAnchorLine = mine.line; selectionAnchorColumn = mine.column;
        insertTextImpl(mine.line, mine.column, text, false);
        // A plain insert removes nothing - its "old end" is its own start.
        shiftResultsAfterEdit(results, mine.line, mine.column, cursorLine, cursorColumn);
        mine.line = cursorLine; mine.column = cursorColumn;
    }
    applyMultiCursorResults(results);
}

bool CodeEditorState::eraseSelectionsAtAllCursors()
{
    if (extraCursors_.empty()) return eraseSelection();

    bool anySelected = hasSelection();
    for (int i = 0; i < extraCursorCount() && !anySelected; ++i)
    {
        const CodeEditorCursor& c = extraCursorAt(i);
        anySelected = c.line != c.anchorLine || c.column != c.anchorColumn;
    }
    if (!anySelected) return false; // mirrors eraseSelection(): a no-op never touches editGroup_

    bool erasedAny = false;
    ++editGroup_;
    ct::Vector<AllCursorsEntry> entries = collectCursorsDescending(*this);
    ct::Vector<AllCursorsResult> results;
    for (const auto& entry : entries) results.push_back({entry.line, entry.column, entry.extraIndex});

    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        AllCursorsResult& mine = results[i];
        const bool hadSelection = entry.line != entry.anchorLine || entry.column != entry.anchorColumn;
        if (!hadSelection) continue;
        int sl = entry.anchorLine, sc = entry.anchorColumn, el = entry.line, ec = entry.column;
        if (sl > el || (sl == el && sc > ec)) { int tl = sl, tc = sc; sl = el; sc = ec; el = tl; ec = tc; }
        eraseRangeImpl(sl, sc, el, ec, false);
        erasedAny = true;
        shiftResultsAfterEdit(results, el, ec, cursorLine, cursorColumn);
        mine.line = cursorLine; mine.column = cursorColumn;
    }
    applyMultiCursorResults(results);
    return erasedAny;
}

void CodeEditorState::backspaceAtAllCursors()
{
    if (extraCursors_.empty())
    {
        if (eraseSelection()) return;
        if (cursorColumn > 0) eraseRange(cursorLine, cursorColumn - 1, cursorLine, cursorColumn);
        else if (cursorLine > 0)
        {
            int previousLen = static_cast<int>(lineAt(cursorLine - 1).size());
            eraseRange(cursorLine - 1, previousLen, cursorLine, 0);
        }
        return;
    }

    ++editGroup_;
    ct::Vector<AllCursorsEntry> entries = collectCursorsDescending(*this);
    ct::Vector<AllCursorsResult> results;
    for (const auto& entry : entries) results.push_back({entry.line, entry.column, entry.extraIndex});

    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        AllCursorsResult& mine = results[i];
        const bool hadSelection = entry.line != entry.anchorLine || entry.column != entry.anchorColumn;
        int sl, sc, el, ec;
        if (hadSelection)
        {
            sl = entry.anchorLine; sc = entry.anchorColumn; el = entry.line; ec = entry.column;
            if (sl > el || (sl == el && sc > ec)) { int tl = sl, tc = sc; sl = el; sc = ec; el = tl; ec = tc; }
        }
        else if (mine.column > 0)
        {
            sl = mine.line; sc = mine.column - 1; el = mine.line; ec = mine.column;
        }
        else if (mine.line > 0)
        {
            sl = mine.line - 1; sc = static_cast<int>(lineAt(mine.line - 1).size()); el = mine.line; ec = 0;
        }
        else continue;

        eraseRangeImpl(sl, sc, el, ec, false);
        shiftResultsAfterEdit(results, el, ec, cursorLine, cursorColumn);
        mine.line = cursorLine; mine.column = cursorColumn;
    }
    applyMultiCursorResults(results);
}

void CodeEditorState::deleteAtAllCursors()
{
    if (extraCursors_.empty())
    {
        if (eraseSelection()) return;
        const int len = static_cast<int>(lineAt(cursorLine).size());
        if (cursorColumn < len) eraseRange(cursorLine, cursorColumn, cursorLine, cursorColumn + 1);
        else if (cursorLine + 1 < lineCount()) eraseRange(cursorLine, cursorColumn, cursorLine + 1, 0);
        return;
    }

    ++editGroup_;
    ct::Vector<AllCursorsEntry> entries = collectCursorsDescending(*this);
    ct::Vector<AllCursorsResult> results;
    for (const auto& entry : entries) results.push_back({entry.line, entry.column, entry.extraIndex});

    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        AllCursorsResult& mine = results[i];
        const bool hadSelection = entry.line != entry.anchorLine || entry.column != entry.anchorColumn;
        int sl, sc, el, ec;
        if (hadSelection)
        {
            sl = entry.anchorLine; sc = entry.anchorColumn; el = entry.line; ec = entry.column;
            if (sl > el || (sl == el && sc > ec)) { int tl = sl, tc = sc; sl = el; sc = ec; el = tl; ec = tc; }
        }
        else
        {
            const int len = static_cast<int>(lineAt(mine.line).size());
            if (mine.column < len) { sl = mine.line; sc = mine.column; el = mine.line; ec = mine.column + 1; }
            else if (mine.line + 1 < lineCount()) { sl = mine.line; sc = mine.column; el = mine.line + 1; ec = 0; }
            else continue;
        }

        eraseRangeImpl(sl, sc, el, ec, false);
        shiftResultsAfterEdit(results, el, ec, cursorLine, cursorColumn);
        mine.line = cursorLine; mine.column = cursorColumn;
    }
    applyMultiCursorResults(results);
}

// ── Undo / redo ────────────────────────────────────────────────────────────

void CodeEditorState::applyEditForward(const CodeEditorEdit& edit)
{
    switch (edit.kind)
    {
    case CodeEditorEdit::Insert:
    {
        int endLine, endColumn;
        spliceInsert(lines_, edit.line, edit.column, edit.insertedText, endLine, endColumn);
        break;
    }
    case CodeEditorEdit::Delete:
    {
        // Forward direction of a Delete edit reproduces the deletion.
        int endLine, endColumn;
        textEndPosition(edit.line, edit.column, edit.removedText, endLine, endColumn);
        spliceErase(lines_, edit.line, edit.column, endLine, endColumn);
        break;
    }
    case CodeEditorEdit::Replace:
    {
        int endLine, endColumn;
        textEndPosition(edit.line, edit.column, edit.removedText, endLine, endColumn);
        spliceErase(lines_, edit.line, edit.column, endLine, endColumn);
        int ignoredLine, ignoredColumn;
        spliceInsert(lines_, edit.line, edit.column, edit.insertedText, ignoredLine, ignoredColumn);
        break;
    }
    }
}

void CodeEditorState::applyEditBackward(const CodeEditorEdit& edit)
{
    switch (edit.kind)
    {
    case CodeEditorEdit::Insert:
    {
        int endLine, endColumn;
        textEndPosition(edit.line, edit.column, edit.insertedText, endLine, endColumn);
        spliceErase(lines_, edit.line, edit.column, endLine, endColumn);
        break;
    }
    case CodeEditorEdit::Delete:
    {
        int endLine, endColumn;
        spliceInsert(lines_, edit.line, edit.column, edit.removedText, endLine, endColumn);
        break;
    }
    case CodeEditorEdit::Replace:
    {
        int endLine, endColumn;
        textEndPosition(edit.line, edit.column, edit.insertedText, endLine, endColumn);
        spliceErase(lines_, edit.line, edit.column, endLine, endColumn);
        int ignoredLine, ignoredColumn;
        spliceInsert(lines_, edit.line, edit.column, edit.removedText, ignoredLine, ignoredColumn);
        break;
    }
    }
}

void CodeEditorState::undo()
{
    if (undoStack_.empty()) return;
    clearExtraCursors();

    // Edits sharing one group were pushed in the order they were applied
    // (bottom-to-top document order for a multi-cursor edit - see
    // insertTextAtAllCursors/eraseSelectionsAtAllCursors above), so
    // undoStack_.back() is always the most-recently-applied one in the
    // group; popping back-to-front and undoing in that same order is what
    // keeps every other edit's stored (line, column) valid as we go. The
    // FIRST edit popped is therefore the topmost (smallest line) one in the
    // group - land the single surviving cursor at its
    // cursorLineBefore/cursorColumnBefore.
    const uint64_t group = undoStack_.back().group;
    int minLine = undoStack_.back().line;
    const int restoreLine = undoStack_.back().cursorLineBefore;
    const int restoreColumn = undoStack_.back().cursorColumnBefore;

    while (!undoStack_.empty() && undoStack_.back().group == group)
    {
        CodeEditorEdit edit = undoStack_.back();
        undoStack_.pop_back();
        applyEditBackward(edit);
        if (edit.line < minLine) minLine = edit.line;
        redoStack_.push_back(edit);
    }
    cursorLine = restoreLine;
    cursorColumn = restoreColumn;
    clearSelection();
    clampCursor();
    lastEditWasTyping_ = false;
    ++editGroup_;
    rehighlightFrom(minLine);
}

void CodeEditorState::redo()
{
    if (redoStack_.empty()) return;
    clearExtraCursors();

    // redoStack_ holds the same group in the opposite order undo popped
    // them (bottom-to-top became top-to-bottom), so popping it back-to-front
    // replays the edits in their original forward (bottom-to-top) order -
    // the last one popped is therefore the last one originally applied.
    const uint64_t group = redoStack_.back().group;
    int minLine = redoStack_.back().line;
    CodeEditorEdit lastApplied;

    while (!redoStack_.empty() && redoStack_.back().group == group)
    {
        CodeEditorEdit edit = redoStack_.back();
        redoStack_.pop_back();
        applyEditForward(edit);
        if (edit.line < minLine) minLine = edit.line;
        lastApplied = edit;
        undoStack_.push_back(edit);
    }
    cursorLine = lastApplied.cursorLineAfter;
    cursorColumn = lastApplied.cursorColumnAfter;
    clearSelection();
    clampCursor();
    lastEditWasTyping_ = false;
    ++editGroup_;
    rehighlightFrom(minLine);
}

void CodeEditorState::clearUndoHistory()
{
    undoStack_.clear();
    redoStack_.clear();
    lastEditWasTyping_ = false;
    ++editGroup_;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Context::codeEditor — immediate-mode widget
// ═════════════════════════════════════════════════════════════════════════════

// Rebuilds the autocomplete popup for the identifier the cursor sits inside
// (or just after). Collects the highlighter's keywords/types/constants plus
// every distinct identifier already in the document, filtered by prefix.
void Context::updateCodeCompletion(CodeEditorState& state, const CodeEditorOptions& options)
{
    CodeEditorCompletion& completion = state.completion;
    completion.visible = false;
    if (!options.autocomplete) return;

    const String& line = state.lineAt(state.cursorLine);
    int end = state.cursorColumn;
    int start = end;
    while (start > 0 && isIdentChar(line[static_cast<size_t>(start - 1)])) --start;
    if (end - start < 2) return; // require a couple of characters before suggesting

    const String prefix = line.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));

    ct::HashSet<String> seen;
    ct::Vector<String> items;
    auto tryAdd = [&](const String& word) {
        if (word.size() <= prefix.size()) return;
        if (word.compare(0, prefix.size(), prefix.c_str()) != 0) return;
        if (seen.contains(word)) return;
        seen.insert(word);
        items.push_back(word);
    };

    if (syntax::SyntaxHighlighter* hl = state.highlighter())
    {
        for (const auto& w : hl->keywordSet()) tryAdd(w);
        for (const auto& w : hl->typeSet()) tryAdd(w);
        for (const auto& w : hl->constantSet()) tryAdd(w);
    }

    for (int i = 0; i < state.lineCount() && items.size() < 200; ++i)
    {
        const String& text = state.lineAt(i);
        int n = static_cast<int>(text.size());
        int j = 0;
        while (j < n)
        {
            if (!isIdentChar(text[static_cast<size_t>(j)]) ||
                (text[static_cast<size_t>(j)] >= '0' && text[static_cast<size_t>(j)] <= '9'))
            {
                ++j;
                continue;
            }
            int wordStart = j;
            while (j < n && isIdentChar(text[static_cast<size_t>(j)])) ++j;
            if (i == state.cursorLine && wordStart == start) continue; // skip the word being typed
            tryAdd(text.substr(static_cast<size_t>(wordStart), static_cast<size_t>(j - wordStart)));
        }
    }

    if (items.empty()) return;

    ct::sort(items.begin(), items.end());
    if (items.size() > 50) items.resize(50);

    completion.items = items;
    completion.selected = 0;
    completion.line = state.cursorLine;
    completion.wordStart = start;
    completion.wordEnd = end;
    completion.visible = true;
    completion.navigated = false;
}

void Context::acceptCodeCompletion(CodeEditorState& state)
{
    CodeEditorCompletion& completion = state.completion;
    if (!completion.visible || completion.items.empty()) return;
    const String replacement = completion.items[static_cast<size_t>(completion.selected)];
    state.eraseRange(completion.line, completion.wordStart, completion.line, completion.wordEnd);
    state.insertText(completion.line, completion.wordStart, replacement);
    state.breakUndoCoalescing();
    completion.visible = false;
}

// Draws one run of text at x, honoring embedded '\t' bytes by advancing to
// the next tab stop instead of asking the font for a glyph it doesn't have
// (a raw tab byte handed to drawText renders as the font's missing-glyph
// placeholder, e.g. '?'). Returns the x position just past the run.
float Context::drawCodeRun(DrawList& drawList, const StringView& run, const Vec2& position,
                           const Color& color, const Rect& clip, int tabSize, int startVisCol, float fontSize)
{
    if (tabSize <= 0) tabSize = 4;
    float x = position.x;
    int visCol = startVisCol;
    size_t segStart = 0;
    const float spaceWidth = measureText(theme_.font, StringView(" "), fontSize).width;

    for (size_t i = 0; i <= run.size(); ++i)
    {
        const bool atTab = i < run.size() && run[i] == '\t';
        if (atTab || i == run.size())
        {
            if (i > segStart)
            {
                const StringView segment(run.data() + segStart, i - segStart);
                drawText(drawList, theme_.font, segment, Vec2(x, position.y), fontSize, color, clip);
                x += measureText(theme_.font, segment, fontSize).width;
                visCol += static_cast<int>(segment.size());
            }
            if (atTab)
            {
                const int stop = tabSize - (visCol % tabSize);
                x += static_cast<float>(stop) * spaceWidth;
                visCol += stop;
                segStart = i + 1;
            }
        }
    }
    return x;
}

// Draws one line, byte-slicing it into the highlighter's spans and drawing
// each run in its token color; falls back to a single plain-colored run
// when there is no highlighter (or the line produced no spans).
void Context::drawHighlightedLine(DrawList& drawList, const CodeEditorState& state,
                                  int line, const String& text, const Vec2& position,
                                  const Rect& clip, int tabSize, float fontSize)
{
    syntax::SyntaxHighlighter* hl = state.highlighter();
    if (!hl || text.empty())
    {
        drawCodeRun(drawList, StringView(text.data(), text.size()), position, theme_.buttonText, clip, tabSize, 0, fontSize);
        return;
    }

    const auto result = state.highlightSpans(line);
    const int n = static_cast<int>(text.size());
    int cursor = 0;
    float x = position.x;
    int visCol = 0;
    for (const auto& span : result.spans)
    {
        if (span.startCol > cursor)
        {
            const StringView run(text.data() + cursor, static_cast<size_t>(span.startCol - cursor));
            const float endX = drawCodeRun(drawList, run, Vec2(x, position.y), theme_.buttonText, clip, tabSize, visCol, fontSize);
            visCol += static_cast<int>(run.size());
            x = endX;
        }
        const int end = span.endCol < n ? span.endCol : n;
        if (end > span.startCol)
        {
            const StringView run(text.data() + span.startCol, static_cast<size_t>(end - span.startCol));
            const float endX = drawCodeRun(drawList, run, Vec2(x, position.y), hl->colorFor(span.type), clip, tabSize, visCol, fontSize);
            visCol += static_cast<int>(run.size());
            x = endX;
        }
        cursor = end;
    }
    if (cursor < n)
    {
        const StringView run(text.data() + cursor, static_cast<size_t>(n - cursor));
        drawCodeRun(drawList, run, Vec2(x, position.y), theme_.buttonText, clip, tabSize, visCol, fontSize);
    }
}

float Context::codeEditorColumnX(const CodeEditorState& state, int line, int column, int tabSize, float fontSize) const
{
    const String& text = state.lineAt(line);
    if (column <= 0) return 0.0f;
    const int n = static_cast<int>(text.size());
    if (column > n) column = n;
    if (tabSize <= 0) tabSize = 4;

    // Fast path: no tabs before `column`, the common case, is one measurement.
    bool hasTab = false;
    for (int i = 0; i < column; ++i) if (text[static_cast<size_t>(i)] == '\t') { hasTab = true; break; }
    if (!hasTab)
        return measureText(theme_.font, StringView(text.data(), static_cast<size_t>(column)), fontSize).width;

    // Walk run by run, snapping to the next tab stop (tabSize columns, in
    // space-widths) at each '\t' instead of measuring it as a glyph.
    const float spaceWidth = measureText(theme_.font, StringView(" "), fontSize).width;
    float x = 0.0f;
    int visCol = 0;
    int runStart = 0;
    for (int i = 0; i <= column; ++i)
    {
        if (i == column || text[static_cast<size_t>(i)] == '\t')
        {
            if (i > runStart)
            {
                const StringView run(text.data() + runStart, static_cast<size_t>(i - runStart));
                float w = measureText(theme_.font, run, fontSize).width;
                x += w;
                visCol += static_cast<int>(run.size()); // approximation: 1 byte ~= 1 column for non-tab runs
            }
            if (i < column) // this iteration stopped on a '\t', not the target column
            {
                int stop = tabSize - (visCol % tabSize);
                x += static_cast<float>(stop) * spaceWidth;
                visCol += stop;
                runStart = i + 1;
            }
        }
    }
    return x;
}

int Context::codeEditorColumnAt(const CodeEditorState& state, int line, float localX, int tabSize, float fontSize) const
{
    const String& text = state.lineAt(line);
    const int n = static_cast<int>(text.size());
    if (localX <= 0.0f) return 0;

    bool hasTab = false;
    for (int i = 0; i < n; ++i) if (text[static_cast<size_t>(i)] == '\t') { hasTab = true; break; }

    if (!hasTab)
    {
        // Binary search the byte offset whose prefix width first exceeds
        // localX, then decide against the previous offset whichever is
        // closer - measureText is monotonic in prefix length, so this is
        // safe without tabs (with tabs, codeEditorColumnX is not monotonic
        // in a binary-search-friendly way per byte, so fall through below).
        int lo = 0, hi = n;
        while (lo < hi)
        {
            int mid = lo + (hi - lo) / 2;
            float width = measureText(theme_.font, StringView(text.data(), static_cast<size_t>(mid)), fontSize).width;
            if (width < localX) lo = mid + 1;
            else hi = mid;
        }
        if (lo > 0)
        {
            float widthAtLo = measureText(theme_.font, StringView(text.data(), static_cast<size_t>(lo)), fontSize).width;
            float widthAtPrev = measureText(theme_.font, StringView(text.data(), static_cast<size_t>(lo - 1)), fontSize).width;
            if (localX - widthAtPrev < widthAtLo - localX) return lo - 1;
        }
        return lo;
    }

    // With tabs present, walk forward one byte at a time (lines are short
    // enough in practice that this is cheap) and stop at the closest column.
    float prevX = 0.0f;
    for (int column = 1; column <= n; ++column)
    {
        float x = codeEditorColumnX(state, line, column, tabSize, fontSize);
        if (x >= localX) return (localX - prevX < x - localX) ? column - 1 : column;
        prevX = x;
    }
    return n;
}

bool Context::codeEditorCopy(CodeEditorState &state)
{
    const String selected = state.selectedText();
    if (!selected.empty()) backend_.setClipboardText(selected);
    return false; // Copy never changes the buffer
}

bool Context::codeEditorCut(CodeEditorState &state)
{
    const String selected = state.selectedText();
    if (selected.empty()) return false;
    backend_.setClipboardText(selected);
    return state.eraseSelection();
}

bool Context::codeEditorPaste(CodeEditorState &state)
{
    const String clipboard = backend_.clipboardText();
    if (clipboard.empty()) return false;
    state.eraseSelection();
    state.insertText(state.cursorLine, state.cursorColumn, clipboard);
    state.breakUndoCoalescing();
    return true;
}

bool Context::codeEditor(StringView labelText, CodeEditorState &state, const Rect &bounds,
                         const CodeEditorOptions &options)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    if (!options.autoDetectFileName.empty() && !state.highlighter())
        state.setHighlighterForFile(String(options.autoDetectFileName.data(), options.autoDetectFileName.size()));

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    // fontSize is this editor's effective size: the theme's size scaled by
    // this state's zoom (see CodeEditorState::zoomIn/Out/Reset). Every
    // measurement, draw call, and click/caret calculation below must use
    // this instead of theme_.fontSize directly, or zooming would desync the
    // math from what's actually drawn.
    const float fontSize = theme_.fontSize * state.fontScale();

    // Only line height comes from a fixed-glyph measurement; horizontal
    // positions always go through codeEditorColumnX/codeEditorColumnAt,
    // which measure the real prefix width - the font is not guaranteed to
    // be monospaced, so a single "M" width cannot stand in for every column.
    const TextMetrics lineMetrics = measureText(theme_.font, StringView("M"), fontSize);
    const float lineHeight = lineMetrics.height > 0.0f ? lineMetrics.height : fontSize;
    const float padding = theme_.textEditPadding;

    const int totalLines = state.lineCount();

    // With folding disabled, visual and logical rows are identical. Avoid
    // allocating a map and scanning every fold region for every source line.
    const bool foldingAvailable = options.showFolding && state.highlighter() != nullptr;
    auto rebuildVisibleLines = [&]() { if (foldingAvailable) state.visibleLines(); };
    auto rowCount = [&]() { return foldingAvailable ? static_cast<int>(state.visibleLines().size()) : state.lineCount(); };
    auto logicalLine = [&](int row) { return foldingAvailable ? state.visibleLines()[static_cast<size_t>(row)] : row; };
    int visibleLineCount = rowCount();
    int cursorRow = state.cursorLine;
    if (foldingAvailable)
        for (int row = 0; row < visibleLineCount; ++row)
            if (logicalLine(row) >= state.cursorLine) { cursorRow = row; break; }

    float gutterWidth = 0.0f;
    if (options.showLineNumbers)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", totalLines > 0 ? totalLines : 1);
        const TextMetrics gutterMetrics = measureText(theme_.font, StringView(buf), fontSize);
        gutterWidth = gutterMetrics.width + theme_.gutterPadding * 2.0f;
    }

    const float foldGutterWidth = foldingAvailable ? lineHeight : 0.0f;
    gutterWidth += foldGutterWidth;

    const float innerHeight = rect.height - padding * 2.0f;
    const int visibleRows = innerHeight >= lineHeight ? static_cast<int>(innerHeight / lineHeight) : 1;
    const int maximumScroll = visibleLineCount - visibleRows > 0 ? visibleLineCount - visibleRows : 0;
    const bool hasScrollbar = maximumScroll > 0;
    const float scrollbarWidth = hasScrollbar ? theme_.scrollbarWidth : 0.0f;

    const Rect textArea(rect.x + gutterWidth, rect.y, rect.width - gutterWidth - scrollbarWidth, rect.height);
    const Rect textClip = intersect(Rect(textArea.x + padding, textArea.y + padding,
                                         textArea.width - padding * 2.0f,
                                         textArea.height - padding * 2.0f), clip);
    const Rect scrollbar(rect.x + rect.width - scrollbarWidth, rect.y, scrollbarWidth, rect.height);
    const WidgetId scrollbarId = combineIds(id, 0x544558545343524Cull);

    if (state.scrollLine < 0) state.scrollLine = 0;
    if (state.scrollLine > maximumScroll) state.scrollLine = maximumScroll;

    itemHovered(rect, clip, id); // registers hotWidget_/lastItemId_ for tooltips; no visual hover state here
    // The scrollbar is handled separately below and must not be claimed by
    // the text-area click. itemClicked() on the full rect used to set
    // activeWidget_ = id for a press anywhere inside it - including the
    // scrollbar thumb - so the thumb-drag branch below never saw
    // activeWidget_ == InvalidWidgetId and the cursor instead followed the
    // pointer, selecting text while the user tried to scroll.
    const Rect interactiveArea(rect.x, rect.y, rect.width - scrollbarWidth, rect.height);
    itemClicked(interactiveArea, clip, id);
    if (hasScrollbar && currentWindowReceivesPointer() &&
        contains(intersect(rect, clip), pointer_.position) && pointer_.wheelY != 0.0f)
    {
        state.scrollLine += pointer_.wheelY > 0.0f ? -1 : 1;
        if (state.scrollLine < 0) state.scrollLine = 0;
        if (state.scrollLine > maximumScroll) state.scrollLine = maximumScroll;
    }

    Rect thumb;
    if (hasScrollbar)
    {
        const float requestedThumb = scrollbar.height * static_cast<float>(visibleRows) / static_cast<float>(visibleLineCount);
        const float thumbHeight = requestedThumb > theme_.scrollbarMinThumb ? requestedThumb : theme_.scrollbarMinThumb;
        const float travel = scrollbar.height - thumbHeight;
        const float offset = maximumScroll > 0
            ? travel * static_cast<float>(state.scrollLine) / static_cast<float>(maximumScroll) : 0.0f;
        thumb = Rect(scrollbar.x, scrollbar.y + offset, scrollbar.width, thumbHeight);
        const uint32_t left = buttonIndex(PointerButton::Left);
        const bool pressedThumb = pointer_.pressed[left] && currentWindow_ == focusedWindow_ &&
                                  activeWidget_ == InvalidWidgetId &&
                                  contains(intersect(thumb, clip), pointer_.pressedPosition[left]);
        if (pressedThumb)
        {
            activeWidget_ = scrollbarId;
            focusedWidget_ = id;
        }
        if (activeWidget_ == scrollbarId)
        {
            if (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left])
            {
                const float normalized = travel > 0.0f
                    ? clamp((pointer_.position.y - scrollbar.y - thumb.height * 0.5f) / travel, 0.0f, 1.0f) : 0.0f;
                state.scrollLine = static_cast<int>(normalized * static_cast<float>(maximumScroll) + 0.5f);
            }
            if (pointer_.released[left])
                activeWidget_ = InvalidWidgetId;
        }
        const float updatedOffset = maximumScroll > 0
            ? travel * static_cast<float>(state.scrollLine) / static_cast<float>(maximumScroll) : 0.0f;
        thumb.y = scrollbar.y + updatedOffset;
    }

    const bool focused = focusedWidget_ == id;
    bool changed = false;
    const uint32_t leftButton = buttonIndex(PointerButton::Left);

    // Clicking the fold gutter (immediately left of the text area) toggles
    // the region whose header is on that row, if any.
    if (foldingAvailable && focused && pointer_.pressed[leftButton])
    {
        const Rect foldGutterRect(textArea.x - foldGutterWidth, rect.y, foldGutterWidth, rect.height);
        if (contains(intersect(foldGutterRect, clip), pointer_.pressedPosition[leftButton]))
        {
            const Vec2 local = pointer_.pressedPosition[leftButton];
            int row = state.scrollLine + static_cast<int>((local.y - (textArea.y + padding)) / lineHeight);
            if (row >= 0 && row < visibleLineCount)
            {
                int line = logicalLine(row);
                if (state.isFoldHeader(line))
                {
                    state.toggleFoldAt(line);
                    state.completion.visible = false;
                }
            }
        }
    }

    // Clicking inside the text area places the cursor at that (line, column)
    // and starts a drag-to-select: while the button stays down (dragging ==
    // activeWidget_ == id), every subsequent frame's pointer position - even
    // outside textArea, clamped to it - extends the selection by moving the
    // cursor and keeping the anchor fixed at the press point. Releasing ends
    // the drag; a plain click with no movement leaves no selection, same as
    // before.
    const bool pressedInText = pointer_.pressed[leftButton] &&
        contains(intersect(textArea, clip), pointer_.pressedPosition[leftButton]);
    if (focused && pressedInText)
    {
        activeWidget_ = id;
        const Vec2 local = pointer_.pressedPosition[leftButton];
        int row = state.scrollLine + static_cast<int>((local.y - (textArea.y + padding)) / lineHeight);
        if (row < 0) row = 0;
        if (row >= visibleLineCount) row = visibleLineCount - 1;
        int line = logicalLine(row);
        int column = codeEditorColumnAt(state, line, local.x - (textArea.x + padding), options.tabSize, fontSize);
        if (column < 0) column = 0;
        if (column > static_cast<int>(state.lineAt(line).size())) column = static_cast<int>(state.lineAt(line).size());
        state.cursorLine = line;
        state.cursorColumn = column;
        state.clearSelection(); // anchor starts here; drag (below) or shift-click would extend it
        state.clearExtraCursors();
        state.breakUndoCoalescing();
        state.completion.visible = false;
    }
    else if (focused && activeWidget_ == id && pointer_.down[leftButton])
    {
        // Dragging: move the cursor to follow the pointer, clamped inside
        // textArea, while leaving the anchor set by the press above alone.
        const Vec2 local = pointer_.position;
        const float clampedY = local.y < textArea.y ? textArea.y
            : (local.y > textArea.y + textArea.height ? textArea.y + textArea.height : local.y);
        int row = state.scrollLine + static_cast<int>((clampedY - (textArea.y + padding)) / lineHeight);
        if (row < 0) row = 0;
        if (row >= visibleLineCount) row = visibleLineCount - 1;
        int line = logicalLine(row);
        int column = codeEditorColumnAt(state, line, local.x - (textArea.x + padding), options.tabSize, fontSize);
        if (column < 0) column = 0;
        if (column > static_cast<int>(state.lineAt(line).size())) column = static_cast<int>(state.lineAt(line).size());
        state.cursorLine = line;
        state.cursorColumn = column;
    }
    if (activeWidget_ == id && pointer_.released[leftButton])
        activeWidget_ = InvalidWidgetId;

    if (focused)
    {
        textInputWidget_ = id;
        wantsKeyboard_ = true;
        wantsTextInput_ = true;

        // Cursor position at the start of the keyboard/typing block: the
        // "keep the cursor visible" clamp below must only run when the
        // cursor actually moved this frame. Gating it this way is what lets
        // wheel/scrollbar scrolling leave the cursor off-screen without the
        // view snapping straight back to it on the next frame.
        const int focusedCursorLine = state.cursorLine;
        const int focusedCursorColumn = state.cursorColumn;

        // A popup only stays valid while the cursor is still inside the
        // identifier that opened it - on the same line, at or after
        // wordStart and at or before wordEnd. Any other cursor movement
        // (arrow keys, Home/End, a click handled above) closes it, so Enter
        // and Tab are never silently swallowed by a stale suggestion list.
        if (state.completion.visible &&
            (state.cursorLine != state.completion.line ||
             state.cursorColumn < state.completion.wordStart ||
             state.cursorColumn > state.completion.wordEnd))
        {
            state.completion.visible = false;
        }
        const bool completionOpen = state.completion.visible;

        if (completionOpen && (upPressed_ || downPressed_))
        {
            int count = static_cast<int>(state.completion.items.size());
            if (upPressed_) state.completion.selected = (state.completion.selected - 1 + count) % count;
            if (downPressed_) state.completion.selected = (state.completion.selected + 1) % count;
            state.completion.navigated = true;
        }
        else if (upPressed_)
        {
            // Step by visible row, not raw line number, so a collapsed fold
            // is skipped in one keypress instead of landing on a hidden line.
            if (cursorRow > 0) state.cursorLine = logicalLine(cursorRow - 1);
            if (state.cursorColumn > static_cast<int>(state.lineAt(state.cursorLine).size()))
                state.cursorColumn = static_cast<int>(state.lineAt(state.cursorLine).size());
            if (!keyShift_[static_cast<uint32_t>(KeyCode::Up)]) { state.clearSelection(); state.clearExtraCursors(); }
            state.breakUndoCoalescing();
        }
        else if (downPressed_)
        {
            if (cursorRow + 1 < visibleLineCount) state.cursorLine = logicalLine(cursorRow + 1);
            if (state.cursorColumn > static_cast<int>(state.lineAt(state.cursorLine).size()))
                state.cursorColumn = static_cast<int>(state.lineAt(state.cursorLine).size());
            if (!keyShift_[static_cast<uint32_t>(KeyCode::Down)]) { state.clearSelection(); state.clearExtraCursors(); }
            state.breakUndoCoalescing();
        }

        if (completionOpen && state.completion.navigated && (tabPressed_ || enterPressed_))
        {
            if (tabPressed_) tabConsumedByWidget_ = true; // Tab accepted a suggestion; don't also move focus
            acceptCodeCompletion(state);
            changed = true;
        }
        else
        {
            if (leftPressed_)
            {
                if (state.cursorColumn > 0) --state.cursorColumn;
                else if (cursorRow > 0)
                {
                    state.cursorLine = logicalLine(cursorRow - 1);
                    state.cursorColumn = static_cast<int>(state.lineAt(state.cursorLine).size());
                }
                if (!keyShift_[static_cast<uint32_t>(KeyCode::Left)]) { state.clearSelection(); state.clearExtraCursors(); }
                state.breakUndoCoalescing();
            }
            if (rightPressed_)
            {
                if (state.cursorColumn < static_cast<int>(state.lineAt(state.cursorLine).size())) ++state.cursorColumn;
                else if (cursorRow + 1 < visibleLineCount)
                {
                    state.cursorLine = logicalLine(cursorRow + 1);
                    state.cursorColumn = 0;
                }
                if (!keyShift_[static_cast<uint32_t>(KeyCode::Right)]) { state.clearSelection(); state.clearExtraCursors(); }
                state.breakUndoCoalescing();
            }
            if (homePressed_)
            {
                state.cursorColumn = 0;
                if (!keyShift_[static_cast<uint32_t>(KeyCode::Home)]) { state.clearSelection(); state.clearExtraCursors(); }
                state.breakUndoCoalescing();
            }
            if (endPressed_)
            {
                state.cursorColumn = static_cast<int>(state.lineAt(state.cursorLine).size());
                if (!keyShift_[static_cast<uint32_t>(KeyCode::End)]) { state.clearSelection(); state.clearExtraCursors(); }
                state.breakUndoCoalescing();
            }

            if (shortcut(KeyCode::A, true, false))
            {
                state.selectionAnchorLine = 0;
                state.selectionAnchorColumn = 0;
                state.cursorLine = totalLines - 1;
                state.cursorColumn = static_cast<int>(state.lineAt(state.cursorLine).size());
                state.breakUndoCoalescing();
            }
            else if (shortcut(KeyCode::D, true, false))
            {
                state.addNextOccurrenceCursor();
            }
            else if (shortcut(KeyCode::Z, true, false))
            {
                state.undo();
                changed = true;
            }
            else if (shortcut(KeyCode::Y, true, false))
            {
                state.redo();
                changed = true;
            }
            else if (shortcut(KeyCode::X, true, false))
            {
                if (codeEditorCut(state)) changed = true;
            }
            else if (copyRequested_)
            {
                codeEditorCopy(state);
            }
            else if (pasteRequested_)
            {
                if (codeEditorPaste(state)) changed = true;
            }
            else if (enterPressed_)
            {
                state.completion.visible = false; // an un-navigated popup never blocks Enter
                state.eraseSelectionsAtAllCursors();
                const String &currentLine = state.lineAt(state.cursorLine);
                String currentIndent;
                if (options.autoIndent)
                {
                    String::size_type indentEnd = 0;
                    while (indentEnd < currentLine.size() &&
                          (currentLine[indentEnd] == ' ' || currentLine[indentEnd] == '\t'))
                        ++indentEnd;
                    currentIndent = currentLine.substr(0, indentEnd);
                }

                // Enter right between a bracket pair on the same line (e.g.
                // "foo() {|}" or "[|]") opens the block onto its own,
                // further-indented line and drops the closing bracket to a
                // new line at the original indent - "foo() {\n    |\n}"
                // instead of "foo() {\n    |}". Skipped with multiple
                // cursors active - falls back to a plain newline+indent at
                // every cursor instead (see report).
                bool splitBracketPair = false;
                if (!state.hasMultipleCursors() && options.autoIndent && state.cursorColumn > 0 &&
                    state.cursorColumn < static_cast<int>(currentLine.size()))
                {
                    char before = currentLine[static_cast<size_t>(state.cursorColumn - 1)];
                    char after = currentLine[static_cast<size_t>(state.cursorColumn)];
                    splitBracketPair = (before == '{' && after == '}') ||
                                       (before == '[' && after == ']') ||
                                       (before == '(' && after == ')');
                }

                if (splitBracketPair)
                {
                    String innerIndent = currentIndent;
                    if (options.insertSpacesForTab)
                        for (int i = 0; i < options.tabSize; ++i) innerIndent.push_back(' ');
                    else
                        innerIndent.push_back('\t');
                    String toInsert = "\n";
                    toInsert.append(innerIndent);
                    toInsert.push_back('\n');
                    toInsert.append(currentIndent);
                    const int line = state.cursorLine, column = state.cursorColumn;
                    state.insertText(line, column, toInsert);
                    // Land the cursor on the new, further-indented middle
                    // line rather than after the closing bracket.
                    state.cursorLine = line + 1;
                    state.cursorColumn = static_cast<int>(innerIndent.size());
                    state.clearSelection();
                }
                else
                {
                    String toInsert = "\n";
                    toInsert.append(currentIndent);
                    state.insertTextAtAllCursors(toInsert);
                }
                state.breakUndoCoalescing();
                changed = true;
            }
            else if (tabPressed_ && !tabShiftPressed_)
            {
                tabConsumedByWidget_ = true; // Tab indented; don't also move focus
                state.completion.visible = false; // an un-navigated popup never blocks Tab
                state.eraseSelectionsAtAllCursors();
                String indent;
                if (options.insertSpacesForTab)
                    for (int i = 0; i < options.tabSize; ++i) indent.push_back(' ');
                else
                    indent.push_back('\t');
                state.insertTextAtAllCursors(indent);
                state.breakUndoCoalescing();
                changed = true;
            }
            else if (backspacePressed_)
            {
                state.backspaceAtAllCursors();
                changed = true;
            }
            else if (isKeyPressed(KeyCode::Delete))
            {
                state.deleteAtAllCursors();
                changed = true;
            }
            else if (!textEvents_.empty())
            {
                state.eraseSelectionsAtAllCursors();
                for (ct::Vector<Event>::size_type i = 0; i < textEvents_.size(); ++i)
                {
                    const Event &event = textEvents_[i];
                    if (event.textLength == 0u) continue;
                    state.insertTextAtAllCursors(String(event.text, event.textLength));
                    changed = true;
                }
            }
        }

        if (changed)
        {
            // Editing can add/remove lines and re-fold regions, so the row
            // mapping built at the top of the frame is stale - the scroll
            // clamp below needs an up to date one. codeEditor() is one call
            // per editor per frame, so a second pass here is the cheapest
            // correct fix rather than threading a mutable rebuild through
            // every edit path above.
            rebuildVisibleLines();
            visibleLineCount = rowCount();
            updateCodeCompletion(state, options);
        }
        if (escapePressed_) { state.completion.visible = false; state.clearExtraCursors(); }

        cursorRow = state.cursorLine;
        if (foldingAvailable)
            for (int row = 0; row < rowCount(); ++row)
                if (logicalLine(row) >= state.cursorLine) { cursorRow = row; break; }

        // Keep the cursor's row inside the visible window - but only when
        // the cursor moved this frame (typing, arrow keys, Home/End, ...).
        // If the view was scrolled by wheel or the scrollbar thumb instead,
        // the cursor may legitimately sit off-screen and must not pull the
        // view back to it.
        const bool cursorMoved = state.cursorLine != focusedCursorLine ||
                                 state.cursorColumn != focusedCursorColumn;
        if (cursorMoved)
        {
            if (cursorRow < state.scrollLine) state.scrollLine = cursorRow;
            else if (cursorRow >= state.scrollLine + visibleRows) state.scrollLine = cursorRow - visibleRows + 1;
        }
        if (state.scrollLine < 0) state.scrollLine = 0;
        const int updatedMaxScroll = rowCount() > visibleRows ? rowCount() - visibleRows : 0;
        if (state.scrollLine > updatedMaxScroll) state.scrollLine = updatedMaxScroll;
    }
    else
    {
        state.completion.visible = false;
    }

    // ── Painting ─────────────────────────────────────────────────────────
    // A code editor is a workspace, not a button: unlike inputText, its
    // background never changes for hover/focus - the blinking caret is
    // feedback enough, and a color swap on focus/hover read as unrelated UI
    // noise rather than useful state.
    drawList->addRectFilled(rect, theme_.inputBg, clip);

    if (options.showLineNumbers)
    {
        const Rect gutterRect(rect.x, rect.y, gutterWidth, rect.height);
        drawList->addRectFilled(gutterRect, theme_.gutterBg, clip);
    }

    const int firstRow = state.scrollLine;
    const int lastRow = firstRow + visibleRows - 1 < visibleLineCount - 1
        ? firstRow + visibleRows - 1 : visibleLineCount - 1;

    int selStartLine = 0, selStartColumn = 0, selEndLine = 0, selEndColumn = 0;
    const bool hasSelection = state.hasSelection();
    if (hasSelection) state.orderedSelection(selStartLine, selStartColumn, selEndLine, selEndColumn);

    // Extra cursors' selections, ordered the same way orderedSelection()
    // orders the primary's (that method only sees the primary, so this
    // mirrors its logic per extra cursor).
    struct OrderedExtraSelection { int startLine, startColumn, endLine, endColumn; };
    ct::Vector<OrderedExtraSelection> extraSelections;
    for (int i = 0; i < state.extraCursorCount(); ++i)
    {
        const CodeEditorCursor &c = state.extraCursorAt(i);
        if (c.line == c.anchorLine && c.column == c.anchorColumn) continue;
        if (c.anchorLine > c.line || (c.anchorLine == c.line && c.anchorColumn > c.column))
            extraSelections.push_back({c.line, c.column, c.anchorLine, c.anchorColumn});
        else
            extraSelections.push_back({c.anchorLine, c.anchorColumn, c.line, c.column});
    }

    for (int row = firstRow; row <= lastRow; ++row)
    {
        const int line = logicalLine(row);
        const float y = textArea.y + padding + static_cast<float>(row - firstRow) * lineHeight;
        const String &text = state.lineAt(line);

        if (options.showLineNumbers)
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", line + 1);
            const TextMetrics numMetrics = measureText(theme_.font, StringView(buf), fontSize);
            const Color numberColor = focused && line == state.cursorLine
                ? theme_.lineNumberActive : theme_.lineNumberColor;
            drawText(*drawList, theme_.font, StringView(buf),
                    Vec2(rect.x + gutterWidth - foldGutterWidth - theme_.gutterPadding - numMetrics.width, y),
                    fontSize, numberColor, clip);
        }

        if (foldingAvailable && state.isFoldHeader(line))
        {
            const bool collapsed = [&] {
                for (const auto &range : state.foldRanges())
                    if (range.startLine == line) return range.collapsed;
                return false;
            }();
            const float cx = textArea.x - foldGutterWidth * 0.5f;
            const float cy = y + lineHeight * 0.5f;
            const float sz = lineHeight * 0.18f;
            if (collapsed)
                // Right-pointing chevron (collapsed: fold starts here, hidden to the right/below).
                drawList->addTriangleFilled(Vec2(cx - sz * 0.5f, cy - sz), Vec2(cx + sz * 0.5f, cy),
                                            Vec2(cx - sz * 0.5f, cy + sz), theme_.textColor, clip);
            else
                // Down-pointing chevron (expanded: contents shown below).
                drawList->addTriangleFilled(Vec2(cx - sz, cy - sz * 0.5f), Vec2(cx, cy + sz * 0.5f),
                                            Vec2(cx + sz, cy - sz * 0.5f), theme_.textColor, clip);
        }

        if (hasSelection && line >= selStartLine && line <= selEndLine)
        {
            const int lineLen = static_cast<int>(text.size());
            const int fromCol = line == selStartLine ? selStartColumn : 0;
            const int toCol = line == selEndLine ? selEndColumn : lineLen;
            const float fromX = textArea.x + padding + codeEditorColumnX(state, line, fromCol, options.tabSize, fontSize);
            float toX = textArea.x + padding + codeEditorColumnX(state, line, toCol, options.tabSize, fontSize);
            if (line != selEndLine) toX += fontSize * 0.25f; // visually mark the line break
            drawList->addRectFilled(Rect(fromX, y, toX - fromX, lineHeight), theme_.selectionColor, textClip);
        }
        for (const auto &sel : extraSelections)
        {
            if (line < sel.startLine || line > sel.endLine) continue;
            const int lineLen = static_cast<int>(text.size());
            const int fromCol = line == sel.startLine ? sel.startColumn : 0;
            const int toCol = line == sel.endLine ? sel.endColumn : lineLen;
            const float fromX = textArea.x + padding + codeEditorColumnX(state, line, fromCol, options.tabSize, fontSize);
            float toX = textArea.x + padding + codeEditorColumnX(state, line, toCol, options.tabSize, fontSize);
            if (line != sel.endLine) toX += fontSize * 0.25f;
            drawList->addRectFilled(Rect(fromX, y, toX - fromX, lineHeight), theme_.selectionColor, textClip);
        }

        drawHighlightedLine(*drawList, state, line, text, Vec2(textArea.x + padding, y), textClip, options.tabSize, fontSize);

        if (options.showWhitespace)
        {
            const Color whitespaceColor(100, 110, 130, 140);
            const float midY = y + lineHeight * 0.5f;
            int column = 0;
            while (column < static_cast<int>(text.size()))
            {
                const char c = text[static_cast<size_t>(column)];
                if (c == ' ')
                {
                    const float cx = textArea.x + padding +
                        (codeEditorColumnX(state, line, column, options.tabSize, fontSize) + codeEditorColumnX(state, line, column + 1, options.tabSize, fontSize)) * 0.5f;
                    const float dotRadius = lineHeight * 0.06f > 1.0f ? lineHeight * 0.06f : 1.0f;
                    drawList->addCircleFilled(Vec2(cx, midY), dotRadius, whitespaceColor, textClip);
                    ++column;
                }
                else if (c == '\t')
                {
                    const float startX = textArea.x + padding + codeEditorColumnX(state, line, column, options.tabSize, fontSize);
                    const float endX = textArea.x + padding + codeEditorColumnX(state, line, column + 1, options.tabSize, fontSize);
                    const float inset = (endX - startX) * 0.2f;
                    const float x1 = startX + inset, x2 = endX - inset;
                    if (x2 > x1)
                    {
                        drawList->addLine(Vec2(x1, midY), Vec2(x2, midY), whitespaceColor, textClip);
                        const float arrowSize = lineHeight * 0.12f;
                        drawList->addLine(Vec2(x2, midY), Vec2(x2 - arrowSize, midY - arrowSize), whitespaceColor, textClip);
                        drawList->addLine(Vec2(x2, midY), Vec2(x2 - arrowSize, midY + arrowSize), whitespaceColor, textClip);
                    }
                    ++column;
                }
                else
                {
                    ++column;
                }
            }
        }

        if (foldingAvailable && state.isLineHidden(line + 1) && state.isFoldHeader(line))
        {
            // Header of a collapsed region: mark that lines are hidden here.
            const float endOfLineX = codeEditorColumnX(state, line, static_cast<int>(text.size()), options.tabSize, fontSize);
            drawText(*drawList, theme_.font, StringView(" ... "),
                    Vec2(textArea.x + padding + endOfLineX, y), fontSize,
                    theme_.textColor, textClip);
        }
    }

    if (hasScrollbar)
    {
        drawList->addRectFilled(scrollbar, theme_.sliderBackground, clip);
        drawList->addRectFilled(thumb, theme_.scrollbarThumb, clip);
    }

    if (focused)
    {
        const float caretX = textArea.x + padding + codeEditorColumnX(state, state.cursorLine, state.cursorColumn, options.tabSize, fontSize);
        const float caretY = textArea.y + padding + static_cast<float>(cursorRow - state.scrollLine) * lineHeight;
        if (caretY >= textArea.y + padding && caretY < textArea.y + textArea.height - padding)
            drawList->addRectFilled(Rect(caretX, caretY, 1.0f, lineHeight), theme_.buttonText, textClip);

        for (int i = 0; i < state.extraCursorCount(); ++i)
        {
            const CodeEditorCursor &c = state.extraCursorAt(i);
            int row = c.line;
            if (foldingAvailable)
                for (int r = 0; r < rowCount(); ++r)
                    if (logicalLine(r) >= c.line) { row = r; break; }
            const float extraCaretX = textArea.x + padding + codeEditorColumnX(state, c.line, c.column, options.tabSize, fontSize);
            const float extraCaretY = textArea.y + padding + static_cast<float>(row - state.scrollLine) * lineHeight;
            if (extraCaretY >= textArea.y + padding && extraCaretY < textArea.y + textArea.height - padding)
                drawList->addRectFilled(Rect(extraCaretX, extraCaretY, 1.0f, lineHeight), theme_.buttonText, textClip);
        }

        if (state.completion.visible && !state.completion.items.empty())
        {
            const float popupX = textArea.x + padding +
                codeEditorColumnX(state, state.completion.line, state.completion.wordStart, options.tabSize, fontSize);
            const float popupY = textArea.y + padding + static_cast<float>(cursorRow - state.scrollLine + 1) * lineHeight;
            const int shown = static_cast<int>(state.completion.items.size()) < 8
                ? static_cast<int>(state.completion.items.size()) : 8;
            const float popupHeight = static_cast<float>(shown) * lineHeight + padding * 2.0f;
            float popupWidth = 0.0f;
            for (int i = 0; i < shown; ++i)
            {
                const TextMetrics itemMetrics = measureText(theme_.font, state.completion.items[static_cast<size_t>(i)], fontSize);
                if (itemMetrics.width > popupWidth) popupWidth = itemMetrics.width;
            }
            popupWidth += padding * 2.0f;
            const Rect popupRect(popupX, popupY, popupWidth, popupHeight);
            drawList->addRectFilled(popupRect, theme_.sliderBackground, clip);
            for (int i = 0; i < shown; ++i)
            {
                const float itemY = popupY + padding + static_cast<float>(i) * lineHeight;
                if (i == state.completion.selected)
                    drawList->addRectFilled(Rect(popupX, itemY, popupWidth, lineHeight), theme_.buttonHovered, clip);
                drawText(*drawList, theme_.font, state.completion.items[static_cast<size_t>(i)],
                        Vec2(popupX + padding, itemY), fontSize, theme_.buttonText, clip);
            }
        }
    }

    return changed;
}

} // namespace ig
