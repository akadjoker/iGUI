#include "DataWidgets.hpp"
#include "EditText.hpp"
#include "Theme.hpp"
#include "Utf8.hpp"
#include "pch.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numeric>

using ig::retained::clamp;

namespace
{
// Space around the text of a header or a cell, left and right.
const float kCellPad = 4.0f;
// What a sorted column keeps for its arrow at the right, from the right edge: the arrow (6 wide,
// from 14 to 8 pixels in) and a gap before it.
const float kSortArrowRoom = 18.0f;
// Whether column i of `cols` takes part in the stretching: it was asked to, or it is the last and the
// grid stretches its last column.
template<class ColumnT>
bool isStretching(const ct::Vector<ColumnT>& cols, bool stretchLast, size_t i)
{
    return cols[i].stretch || (stretchLast && i + 1 == cols.size());
}

// Prints the text of a cell elided to its room, with the memo of the cell: a text the memo already holds
// and that fits is printed here as the grid always printed it, one call, and only the cells that
// are new, changed, or elided go through PaintContext::printElided.
inline void printCell(PaintContext& ctx, const Rect& bounds, const String& text, float y, float ascender, ElideMemo& memo,
        uint32_t fontId)
{
    if (!text.empty() && memo.holds(text, bounds.w, fontId) && !memo.elided)
        ctx.font.Print(text.c_str(), bounds.x, y);
    else
        ctx.printElided(bounds, text, y, ascender, memo, fontId);
}

// The width of a grid for the column layout: finite, not negative, and not absurd.
float layoutWidth(float width)
{
    if (!(width > 0.0f)) return 0.0f;
    return width < 1.0e6f ? width : 1.0e6f;
}

// The width of every column of a grid `gridW` wide whose columns start `leading` pixels in (the checkbox
// column), and the total width of it all. A column that does not stretch has its own width. The ones that
// stretch share equally what the others leave over - but a column is never narrower than its own width:
// one whose own width is more than the equal share keeps it, and the others share what that leaves.
// When even the own widths do not fit, every column has its own width, and the total is more than gridW
// (the grid scrolls sideways); when they fill it, the total is gridW exactly.
template<class ColumnT>
void layoutColumns(const ct::Vector<ColumnT>& cols, bool stretchLast, float gridW, float leading,
        ct::Vector<float>& widths, float& total)
{
    const size_t n = cols.size();
    widths.resize(n);
    float fixedSum = 0.0f;
    size_t open = 0; // stretching columns that have no width yet
    for (size_t i = 0; i < n; ++i)
    {
        widths[i] = cols[i].width;
        if (isStretching(cols, stretchLast, i)) ++open;
        else
            fixedSum += cols[i].width;
    }
    float pool = gridW - leading - fixedSum;
    ct::Vector<unsigned char> pinned; // stretching columns held at their own width
    pinned.resize(n);
    for (size_t i = 0; i < n; ++i) pinned[i] = 0;
    bool filled = false;
    while (open > 0)
    {
        const float share = pool / static_cast<float>(open);
        bool pinnedOne = false;
        for (size_t i = 0; i < n; ++i)
            if (!pinned[i] && isStretching(cols, stretchLast, i) && cols[i].width > share)
            {
                pinned[i] = 1;
                pool -= cols[i].width;
                --open;
                pinnedOne = true;
            }
        if (pinnedOne) continue;
        for (size_t i = 0; i < n; ++i)
            if (!pinned[i] && isStretching(cols, stretchLast, i)) widths[i] = share;
        filled = true;
        break;
    }
    if (filled)
    {
        total = gridW;
        return;
    }
    total = leading;
    for (size_t i = 0; i < n; ++i) total += widths[i];
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
//  DataGrid - constructor
// ═════════════════════════════════════════════════════════════════════════════

DataGrid::DataGrid()
{
    acceptsFocus_ = true;
}

DataGrid::~DataGrid()
{
    detachModel();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Columns
// ═════════════════════════════════════════════════════════════════════════════

int DataGrid::addColumn(const String& name, float width, bool sortable)
{
    Column c;
    c.name = name;
    c.width = width;
    c.sortable = sortable;
    columns_.push_back(std::move(c));
    invalidateColumns();
    // Rows added before this column have no cell for it, and the accessors
    // index cells up to columns_.size(): pad them the way addRow does.
    for (auto& row: rows_)
        while (row.cells.size() < columns_.size()) row.cells.push_back("");
    sortDirty_ = true;
    markDirty();
    return static_cast<int>(columns_.size()) - 1;
}

void DataGrid::setColumnWidth(int col, float w)
{
    if (col >= 0 && col < static_cast<int>(columns_.size()) && w == w)
    {
        columns_[col].width = std::max(w, columns_[col].minWidth);
        invalidateColumns();
        markDirty();
    }
}

float DataGrid::columnWidth(int col) const
{
    ensureColumns();
    return col >= 0 && col < static_cast<int>(colW_.size()) ? colW_[col] : 0.f;
}

void DataGrid::ensureColumns() const
{
    const float gridW = layoutWidth(rect_.w);
    if (!colDirty_ && colW_.size() == columns_.size() && colGridW_ == gridW) return;
    colDirty_ = false;
    colGridW_ = gridW;
    layoutColumns(columns_, stretchLast_, gridW, contentX(), colW_, colTotal_);
    // A grid scrolled sideways does not stay scrolled past the end of its columns when they get
    // narrower or the grid wider (a window made larger)
    const float most = colTotal_ > gridW ? colTotal_ - gridW : 0.0f;
    if (scrollX_ > most) scrollX_ = most;
}

void DataGrid::reserveMemos(const Rect& abs)
{
    // the rows and columns that can be in sight, with some to spare for the scroll
    const float bodyH = abs.h - headerHeight_;
    const int rows = (bodyH > 0.0f && rowHeight_ > 0.0f ? static_cast<int>(bodyH / rowHeight_) : 0) + 4;
    int cols = 0;
    float x = abs.x + contentX() - scrollX_;
    for (size_t i = 0; i < colW_.size(); ++i)
    {
        if (x + colW_[i] > abs.x && x < abs.x + abs.w) ++cols;
        x += colW_[i];
    }
    memos_.reserve(rows, cols + 2);
}

void DataGrid::setColumnStretch(int col, bool stretch)
{
    if (col >= 0 && col < static_cast<int>(columns_.size()))
    {
        columns_[col].stretch = stretch;
        invalidateColumns();
        markDirty();
    }
}

bool DataGrid::columnStretch(int col) const
{
    return col >= 0 && col < static_cast<int>(columns_.size()) && columns_[col].stretch;
}

void DataGrid::setStretchLastColumn(bool stretch)
{
    stretchLast_ = stretch;
    invalidateColumns();
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Rows
// ═════════════════════════════════════════════════════════════════════════════

int DataGrid::addRow(const ct::Vector<String>& cells)
{
    Row r;
    r.cells = cells;
    while (r.cells.size() < columns_.size()) r.cells.push_back("");
    rows_.push_back(std::move(r));
    sortDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

void DataGrid::setCell(int row, int col, const String& value)
{
    if (row >= 0 && row < static_cast<int>(rows_.size()) && col >= 0 &&
            col < static_cast<int>(columns_.size()))
    {
        rows_[row].cells[col] = value;
        markDirty();
    }
}

String DataGrid::cell(int row, int col) const
{
    if (row >= 0 && row < static_cast<int>(rows_.size()) && col >= 0 &&
            col < static_cast<int>(columns_.size()))
        return rows_[row].cells[col];
    return "";
}

void DataGrid::removeRow(int row)
{
    if (row >= 0 && row < static_cast<int>(rows_.size()))
    {
        rows_.erase(rows_.begin() + row);
        sortDirty_ = true;
        if (selectedRow_ == row) selectedRow_ = -1;
        else if (selectedRow_ > row)
            selectedRow_--;
        markDirty();
    }
}

void DataGrid::clearRows()
{
    rows_.clear();
    sortedIndices_.clear();
    selectedRow_ = -1;
    selectedRows_.clear();
    selectedSet_.clear();
    scrollY_ = 0;
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Checkboxes
// ═════════════════════════════════════════════════════════════════════════════

bool DataGrid::isRowChecked(int row) const
{
    if (row >= 0 && row < static_cast<int>(rows_.size())) return rows_[row].checked;
    return false;
}

void DataGrid::setRowChecked(int row, bool checked)
{
    if (row >= 0 && row < static_cast<int>(rows_.size()))
    {
        rows_[row].checked = checked;
        markDirty();
    }
}

ct::Vector<int> DataGrid::checkedRows() const
{
    ct::Vector<int> result;
    for (int i = 0; i < static_cast<int>(rows_.size()); ++i)
        if (rows_[i].checked) result.push_back(i);
    return result;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Selection
// ═════════════════════════════════════════════════════════════════════════════

void DataGrid::setSelectedRow(int row)
{
    selectedRow_ = row;
    selectedRows_.clear();
    selectedSet_.clear();
    if (row >= 0)
    {
        selectedRows_.push_back(row);
        selectedSet_.insert(row);
    }
    markDirty();
}

void DataGrid::clearSelection()
{
    selectedRow_ = -1;
    selectedRows_.clear();
    selectedSet_.clear();
    markDirty();
}

bool DataGrid::isSelected(int row) const
{
    if (!multiSelect_) return row == selectedRow_;
    return selectedSet_.contains(row);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Sort
// ═════════════════════════════════════════════════════════════════════════════

void DataGrid::rebuildSortOrder() const
{
    sortDirty_ = false;
    int nRows = modelRowCount();
    sortedIndices_.resize(nRows);
    std::iota(sortedIndices_.begin(), sortedIndices_.end(), 0);
    if (sortCol_ >= 0 && sortOrder_ != SortOrder::None) sortByColumn(sortCol_);
}

void DataGrid::sortByColumn(int col) const
{
    if (col < 0 || col >= static_cast<int>(columns_.size())) return;
    bool asc = (sortOrder_ == SortOrder::Ascending);

    if (model_)
    {
        // Sort via model data
        std::stable_sort(sortedIndices_.begin(), sortedIndices_.end(),
                [&](int a, int b)
                {
                    auto sa = model_->data(model_->index(a, col));
                    auto sb = model_->data(model_->index(b, col));
                    char* endA;
                    char* endB;
                    double da = strtod(sa.c_str(), &endA);
                    double db = strtod(sb.c_str(), &endB);
                    if (endA != sa.c_str() && endB != sb.c_str()) return asc ? da < db : da > db;
                    return asc ? sa < sb : sa > sb;
                });
    }
    else
    {
        std::stable_sort(sortedIndices_.begin(), sortedIndices_.end(),
                [&](int a, int b)
                {
                    const String& sa = rows_[a].cells[col];
                    const String& sb = rows_[b].cells[col];
                    char* endA;
                    char* endB;
                    double da = strtod(sa.c_str(), &endA);
                    double db = strtod(sb.c_str(), &endB);
                    if (endA != sa.c_str() && endB != sb.c_str()) return asc ? da < db : da > db;
                    return asc ? sa < sb : sa > sb;
                });
    }
}

int DataGrid::displayRow(int idx) const
{
    ensureSortOrder();
    if (idx >= 0 && idx < static_cast<int>(sortedIndices_.size())) return sortedIndices_[idx];
    return idx;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Geometry helpers
// ═════════════════════════════════════════════════════════════════════════════

float DataGrid::totalColumnsWidth() const
{
    ensureColumns();
    return colTotal_;
}

float DataGrid::columnX(int col) const
{
    ensureColumns();
    float x = contentX();
    for (int i = 0; i < col && i < static_cast<int>(colW_.size()); ++i) x += colW_[i];
    return x;
}

int DataGrid::hitColumn(float localX) const
{
    ensureColumns();
    float x = contentX() - scrollX_;
    for (int i = 0; i < static_cast<int>(colW_.size()); ++i)
    {
        if (localX >= x && localX < x + colW_[i]) return i;
        x += colW_[i];
    }
    return -1;
}

int DataGrid::hitRow(float localY) const
{
    ensureSortOrder();
    if (localY < headerHeight_) return -1;
    float y = localY - headerHeight_ + scrollY_;
    int idx = static_cast<int>(y / rowHeight_);
    if (idx < 0 || idx >= static_cast<int>(sortedIndices_.size())) return -1;
    return idx;
}

int DataGrid::hitResizeEdge(float localX) const
{
    ensureColumns();
    float x = contentX() - scrollX_;
    for (int i = 0; i < static_cast<int>(colW_.size()); ++i)
    {
        x += colW_[i];
        // a stretching column is as wide as the grid makes it: its edge is not one to drag
        const float away = localX - x;
        if (away <= 4.0f && away >= -4.0f && !isStretching(columns_, stretchLast_, static_cast<size_t>(i))) return i;
    }
    return -1;
}

float DataGrid::minColumnsWidth() const
{
    float w = contentX();
    for (auto& c: columns_) w += c.width;
    return w;
}

Widget::Vec2f DataGrid::sizeHint() const
{
    // the least the columns need: the stretching ones are as wide as the grid makes them
    float w = minColumnsWidth();
    float h = headerHeight_ + modelRowCount() * rowHeight_;
    return { w, h };
}

// ═════════════════════════════════════════════════════════════════════════════
//  Inline editing
// ═════════════════════════════════════════════════════════════════════════════

void DataGrid::setColumnReadOnly(int col, bool ro)
{
    if (col >= 0 && col < static_cast<int>(columns_.size())) columns_[col].readOnly = ro;
}

void DataGrid::startEdit(int row, int col)
{
    if (row < 0 || row >= modelRowCount()) return;
    if (col < 0 || col >= static_cast<int>(columns_.size())) return;
    if (readOnly_ || columns_[col].readOnly) return;
    if (model_ && !modelIsEditable(row, col)) return;
    editing_ = true;
    editRow_ = row;
    editCol_ = col;
    editBuf_ = modelCell(row, col);
    editCursor_ = static_cast<int>(editBuf_.size());
    editScroll_ = 0.f;
    markDirty();
}

void DataGrid::commitEdit()
{
    if (!editing_) return;
    editing_ = false;
    if (editRow_ >= 0 && editRow_ < modelRowCount() && editCol_ >= 0 &&
            editCol_ < static_cast<int>(columns_.size()))
    {
        if (model_)
        {
            model_->setData(model_->index(editRow_, editCol_), editBuf_);
        }
        else
        {
            rows_[editRow_].cells[editCol_] = editBuf_;
        }
        cellEdited.emit(editRow_, editCol_);
    }
    editRow_ = -1;
    editCol_ = -1;
    markDirty();
}

void DataGrid::cancelEdit()
{
    editing_ = false;
    editRow_ = -1;
    editCol_ = -1;
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Paint
// ═════════════════════════════════════════════════════════════════════════════

void DataGrid::paint(PaintContext& ctx)
{
    ensureSortOrder();
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();
    const int nRows = modelRowCount();
    const int nCols = static_cast<int>(columns_.size());
    ensureColumns();
    reserveMemos(abs);

    ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, t.panelColor.a);
    ctx.fillRect(abs.x, abs.y, abs.w, abs.h);

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();
    const uint32_t fontId = ctx.fontStamp().id; // the font stays this one all through the paint

    // ── Column headers ───────────────────────────────────────────────────
    Rect headerClip = { abs.x, abs.y, abs.w, headerHeight_ };
    ctx.pushClip(headerClip);
    {
        Color hdrBg(static_cast<uint8_t>(std::min(255, t.panelColor.r + 15)),
                static_cast<uint8_t>(std::min(255, t.panelColor.g + 15)),
                static_cast<uint8_t>(std::min(255, t.panelColor.b + 15)), 255);
        ctx.fill.SetColor(hdrBg.r, hdrBg.g, hdrBg.b, hdrBg.a);
        ctx.fillRect(abs.x, abs.y, abs.w, headerHeight_);

        float hx = abs.x + contentX() - scrollX_;
        float hdrTextY = abs.y + (headerHeight_ - t.fontSize) * 0.5f + asc;

        for (int i = 0; i < nCols; ++i)
        {
            auto& col = columns_[i];
            const float colW = colW_[i];
            float colR = hx + colW;
            if (colR <= abs.x || hx >= abs.x + abs.w)
            {
                hx = colR; // out of sight: nothing to measure or draw
                continue;
            }

            // The name is cut with "..." before the arrow of a sorted column (or the right edge), so
            // it never reaches the next column.
            const bool arrow = sortCol_ == i && sortOrder_ != SortOrder::None;
            const float textRight = colR - (arrow ? kSortArrowRoom : kCellPad);
            ctx.font.SetColor(t.textColor);
            ctx.printElided({ hx + kCellPad, abs.y, textRight - (hx + kCellPad), headerHeight_ }, col.name,
                    hdrTextY, asc, memos_.header(i), fontId);

            if (arrow)
            {
                float arrowX = colR - 14.0f;
                float arrowY = abs.y + headerHeight_ * 0.5f;
                ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
                // cut at the column: in a column too narrow for it the arrow would be drawn over
                // the one before
                ctx.pushClip({ hx, abs.y, colW, headerHeight_ });
                if (sortOrder_ == SortOrder::Ascending)
                    ctx.fillTriangle(arrowX, arrowY + 3, arrowX + 6, arrowY + 3, arrowX + 3,
                            arrowY - 3);
                else
                    ctx.fillTriangle(arrowX, arrowY - 3, arrowX + 6, arrowY - 3, arrowX + 3,
                            arrowY + 3);
                ctx.popClip();
            }

            ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 80);
            ctx.drawLine(colR, abs.y + 2, colR, abs.y + headerHeight_ - 2);
            hx = colR;
        }

        if (showCheckboxes_)
        {
            ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 80);
            ctx.drawLine(abs.x + checkboxColW_, abs.y + 2, abs.x + checkboxColW_,
                    abs.y + headerHeight_ - 2);
        }

        ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
        ctx.drawLine(abs.x, abs.y + headerHeight_, abs.x + abs.w, abs.y + headerHeight_);
    }
    ctx.popClip();

    // ── Rows ─────────────────────────────────────────────────────────────
    float bodyY = abs.y + headerHeight_;
    float bodyH = abs.h - headerHeight_;
    Rect bodyClip = { abs.x, bodyY, abs.w, bodyH };
    ctx.pushClip(bodyClip);
    {
        int totalVis = static_cast<int>(sortedIndices_.size());
        int firstVis = static_cast<int>(scrollY_ / rowHeight_);
        int lastVis = firstVis + static_cast<int>(bodyH / rowHeight_) + 2;
        if (firstVis < 0) firstVis = 0;
        if (lastVis > totalVis) lastVis = totalVis;

        String cellScratch;
        const int memoCols = memos_.cols();
        for (int vi = firstVis; vi < lastVis; ++vi)
        {
            int dataIdx = displayRow(vi);
            if (dataIdx < 0 || dataIdx >= nRows) continue;
            ElideMemo* const memoRow = memos_.row(vi);

            float ry = bodyY + vi * rowHeight_ - scrollY_;
            Rect rowR = { abs.x, ry, abs.w, rowHeight_ };
            if (ctx.isClipped(rowR)) continue;

            // Zebra stripes
            if (zebraStripes_ && (vi % 2 == 1))
            {
                ctx.fill.SetColor(static_cast<uint8_t>(std::min(255, t.panelColor.r + 5)),
                        static_cast<uint8_t>(std::min(255, t.panelColor.g + 5)),
                        static_cast<uint8_t>(std::min(255, t.panelColor.b + 5)), 255);
                ctx.fillRect(abs.x, ry, abs.w, rowHeight_);
            }

            // Selection highlight
            if (isSelected(dataIdx))
            {
                ctx.fill.SetColor(t.selectionColor.r, t.selectionColor.g, t.selectionColor.b,
                        t.selectionColor.a);
                ctx.fillRect(abs.x, ry, abs.w, rowHeight_);
            }

            // Hover highlight
            if (hoveredRow_ == vi && !isSelected(dataIdx))
            {
                ctx.fill.SetColor(t.buttonHover.r, t.buttonHover.g, t.buttonHover.b, 60);
                ctx.fillRect(abs.x, ry, abs.w, rowHeight_);
            }

            float textY = ry + (rowHeight_ - t.fontSize) * 0.5f + asc;

            // Checkbox
            if (showCheckboxes_)
            {
                float cbX = abs.x + 6.0f;
                float cbY = ry + (rowHeight_ - 14.0f) * 0.5f;
                float cbS = 14.0f;
                ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
                ctx.fillRect(cbX, cbY, cbS, cbS);
                ctx.line.SetColor(t.inputBorder.r, t.inputBorder.g, t.inputBorder.b,
                        t.inputBorder.a);
                ctx.lineRect(cbX, cbY, cbS, cbS);
                if (modelIsChecked(dataIdx))
                {
                    ctx.line.SetColor(t.checkMark.r, t.checkMark.g, t.checkMark.b, t.checkMark.a);
                    ctx.drawLine(cbX + 3, cbY + cbS * 0.5f, cbX + cbS * 0.4f, cbY + cbS - 3);
                    ctx.drawLine(cbX + cbS * 0.4f, cbY + cbS - 3, cbX + cbS - 3, cbY + 3);
                }
            }

            // Cells
            float cx = abs.x + contentX() - scrollX_;
            int memoCol = 0; // ci % memoCols
            for (int ci = 0; ci < nCols; ++ci, memoCol = memoCol + 1 == memoCols ? 0 : memoCol + 1)
            {
                float colW = colW_[ci];
                if (cx + colW <= abs.x || cx >= abs.x + abs.w)
                {
                    cx += colW; // out of sight: nothing to measure or draw
                    continue;
                }

                if (editing_ && editRow_ == dataIdx && editCol_ == ci)
                {
                    ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, 255);
                    ctx.fillRect(cx + 1, ry + 1, colW - 2, rowHeight_ - 2);
                    ctx.line.SetColor(t.inputBorderHover.r, t.inputBorderHover.g,
                            t.inputBorderHover.b, 255);
                    ctx.lineRect(cx + 1, ry + 1, colW - 2, rowHeight_ - 2);
                    ctx.font.SetColor(Color(255, 255, 255, 255));
                    paintEditText(ctx, editBuf_, editCursor_, editScroll_, cx + 1, colW - 2, ry, rowHeight_, textY, asc);
                }
                else
                {
                    // a cell of the grid's own rows is read where it is; a model's comes by value
                    const String& cellText = cellRef(dataIdx, ci, cellScratch);
                    if (!cellText.empty())
                    {
                        ctx.font.SetColor(
                                isSelected(dataIdx) ? Color(255, 255, 255, 255) : t.textColor);
                        printCell(ctx, { cx + kCellPad, ry, colW - 2.0f * kCellPad, rowHeight_ }, cellText, textY, asc,
                                memoRow[memoCol], fontId);
                    }
                }

                ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 30);
                ctx.drawLine(cx + colW, ry, cx + colW, ry + rowHeight_);
                cx += colW;
            }

            ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 30);
            ctx.drawLine(abs.x, ry + rowHeight_, abs.x + abs.w, ry + rowHeight_);
        }
    }
    ctx.popClip();

    // ── Scrollbars ───────────────────────────────────────────────────────
    float contentH = static_cast<float>(nRows) * rowHeight_;
    float contentW = totalColumnsWidth();

    if (bodyH > 0.0f && contentH > bodyH)
    {
        float sbW = 8.0f;
        float sbX = abs.x + abs.w - sbW;
        float trackH = bodyH;
        float thumbH = std::max(20.0f, (bodyH / contentH) * trackH);
        float maxSY = contentH - bodyH;
        float thumbY = bodyY + (scrollY_ / maxSY) * (trackH - thumbH);
        ctx.fill.SetColor(static_cast<uint8_t>(std::min(255, t.panelColor.r + 20)),
                static_cast<uint8_t>(std::min(255, t.panelColor.g + 20)),
                static_cast<uint8_t>(std::min(255, t.panelColor.b + 20)), 150);
        ctx.fillRect(sbX, bodyY, sbW, trackH);
        ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, 80);
        ctx.fillRect(sbX + 1, thumbY, sbW - 2, thumbH);
    }

    if (abs.w > 0.0f && contentW > abs.w)
    {
        float sbH = 8.0f;
        float sbY = abs.y + abs.h - sbH;
        float trackW = abs.w;
        float thumbW = std::max(20.0f, (abs.w / contentW) * trackW);
        float maxSX = contentW - abs.w;
        float thumbX = abs.x + (scrollX_ / maxSX) * (trackW - thumbW);
        ctx.fill.SetColor(static_cast<uint8_t>(std::min(255, t.panelColor.r + 20)),
                static_cast<uint8_t>(std::min(255, t.panelColor.g + 20)),
                static_cast<uint8_t>(std::min(255, t.panelColor.b + 20)), 150);
        ctx.fillRect(abs.x, sbY, trackW, sbH);
        ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, 80);
        ctx.fillRect(thumbX, sbY + 1, thumbW, sbH - 2);
    }

    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.lineRect(abs.x, abs.y, abs.w, abs.h);
    if (overrideFont_) ctx.popFont();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Mouse events
// ═════════════════════════════════════════════════════════════════════════════

void DataGrid::onMousePress(MouseEvent& e)
{
    if (!visible_ || !isEnabledInTree()) return;
    Rect abs = absoluteRect();
    if (!abs.contains(e.x, e.y)) return;

    e.consumed = true;
    float localX = e.x - abs.x;
    float localY = e.y - abs.y;

    // ── Horizontal scrollbar drag ─────────────────────────────────────────
    float contentW = totalColumnsWidth();
    if (contentW > abs.w)
    {
        float sbH = 8.0f;
        float sbY = abs.y + abs.h - sbH;
        float trackW = abs.w;
        float thumbW = std::max(20.0f, (abs.w / contentW) * trackW);
        float maxSX = contentW - abs.w;
        float thumbX = (maxSX > 0.f ? (scrollX_ / maxSX) * (trackW - thumbW) : 0.f);
        if (e.y >= sbY && e.y <= sbY + sbH)
        {
            // Click on thumb → start drag; click on track → jump
            if (e.x >= abs.x + thumbX && e.x <= abs.x + thumbX + thumbW)
            {
                hScrollDrag_ = true;
                hScrollDragStartX_ = e.x;
                hScrollDragStartSX_ = scrollX_;
            }
            else
            {
                // Jump to clicked position
                float clickRatio = (localX - thumbW * 0.5f) / (trackW - thumbW);
                scrollX_ = std::max(0.f, std::min(maxSX, clickRatio * maxSX));
                markDirty();
            }
            return;
        }
    }

    if (localY < headerHeight_)
    {
        int resEdge = hitResizeEdge(localX);
        if (resEdge >= 0)
        {
            resizeCol_ = resEdge;
            resizeStartX_ = e.x;
            resizeStartW_ = colW_[resEdge];
            return;
        }

        int col = hitColumn(localX);
        if (col >= 0 && columns_[col].sortable)
        {
            if (sortCol_ == col)
            {
                if (sortOrder_ == SortOrder::None) sortOrder_ = SortOrder::Ascending;
                else if (sortOrder_ == SortOrder::Ascending)
                    sortOrder_ = SortOrder::Descending;
                else
                    sortOrder_ = SortOrder::None;
            }
            else
            {
                sortCol_ = col;
                sortOrder_ = SortOrder::Ascending;
            }
            sortDirty_ = true;
            columnSorted.emit(col);
            markDirty();
        }
        return;
    }

    int visIdx = hitRow(localY);
    if (visIdx < 0) return;
    int dataIdx = displayRow(visIdx);
    if (dataIdx < 0 || dataIdx >= modelRowCount()) return;

    if (showCheckboxes_ && localX < checkboxColW_)
    {
        if (model_)
        {
            bool cur = model_->data(model_->index(dataIdx, 0), ItemRole::Check) == "1";
            model_->setData(model_->index(dataIdx, 0), cur ? "0" : "1", ItemRole::Check);
        }
        else
        {
            rows_[dataIdx].checked = !rows_[dataIdx].checked;
        }
        rowCheckChanged.emit(dataIdx, modelIsChecked(dataIdx));
        markDirty();
        return;
    }

    static_cast<void>(0); // double-click per-instance
    uint32_t now = WidgetApp::instance().elapsedMs();
    int col = hitColumn(localX);

    if (dataIdx == lastClickRow_ && (now - lastClickTime_) < 400 && col >= 0)
    {
        startEdit(dataIdx, col);
        lastClickTime_ = 0;
        lastClickRow_ = -1;
        return;
    }
    lastClickTime_ = now;
    lastClickRow_ = dataIdx;

    if (editing_) commitEdit();

    if (multiSelect_)
    {
        bool ctrl = e.ctrl;
        bool shift = e.shift;
        if (ctrl)
        {
            auto it = std::find(selectedRows_.begin(), selectedRows_.end(), dataIdx);
            if (it != selectedRows_.end())
            {
                selectedRows_.erase(it);
                selectedSet_.erase(dataIdx);
            }
            else
            {
                selectedRows_.push_back(dataIdx);
                selectedSet_.insert(dataIdx);
            }
            selectedRow_ = dataIdx;
        }
        else if (shift && selectedRow_ >= 0)
        {
            selectedRows_.clear();
            selectedSet_.clear();
            int lo = std::min(selectedRow_, dataIdx);
            int hi = std::max(selectedRow_, dataIdx);
            for (int i = lo; i <= hi; ++i)
            {
                selectedRows_.push_back(i);
                selectedSet_.insert(i);
            }
        }
        else
        {
            selectedRow_ = dataIdx;
            selectedRows_ = { dataIdx };
            selectedSet_.clear();
            selectedSet_.insert(dataIdx);
        }
    }
    else
    {
        selectedRow_ = dataIdx;
        selectedRows_ = { dataIdx };
        selectedSet_.clear();
        selectedSet_.insert(dataIdx);
    }
    selectionChanged.emit(dataIdx);
    markDirty();
}

void DataGrid::onMouseRelease(MouseEvent& e)
{
    if (hScrollDrag_)
    {
        hScrollDrag_ = false;
    }
    if (resizeCol_ >= 0) resizeCol_ = -1;
    (void) e;
}

void DataGrid::onMouseMove(MouseEvent& e)
{
    if (!visible_ || !isEnabledInTree()) return;
    Rect abs = absoluteRect();

    if (hScrollDrag_)
    {
        e.consumed = true;
        float contentW = totalColumnsWidth();
        float trackW = abs.w;
        float thumbW = std::max(20.0f, (abs.w / contentW) * trackW);
        float maxSX = contentW - abs.w;
        float dx = e.x - hScrollDragStartX_;
        float ratio = maxSX > 0.f ? maxSX / (trackW - thumbW) : 0.f;
        scrollX_ = std::max(0.f, std::min(maxSX, hScrollDragStartSX_ + dx * ratio));
        markDirty();
        return;
    }

    if (resizeCol_ >= 0)
    {
        float dx = e.x - resizeStartX_;
        float newW = std::max(columns_[resizeCol_].minWidth, resizeStartW_ + dx);
        columns_[resizeCol_].width = newW;
        invalidateColumns();
        markDirty();
        return;
    }

    float localX = e.x - abs.x;
    float localY = e.y - abs.y;
    int visIdx = hitRow(localY);
    int col = hitColumn(localX);
    if (visIdx != hoveredRow_ || col != hoveredCol_)
    {
        hoveredRow_ = visIdx;
        hoveredCol_ = col;
        markDirty();
    }
}

void DataGrid::onMouseScroll(MouseEvent& e)
{
    if (!visible_ || !isEnabledInTree()) return;
    Rect abs = absoluteRect();
    if (!abs.contains(e.x, e.y)) return;

    e.consumed = true;
    bool shift = e.shift;
    float bodyH = abs.h - headerHeight_;
    float contentH = static_cast<float>(modelRowCount()) * rowHeight_;
    float contentW = totalColumnsWidth();

    if (shift)
    {
        scrollX_ -= e.scrollY * 40.0f;
        scrollX_ = clamp(scrollX_, 0.0f, std::max(0.0f, contentW - abs.w));
    }
    else
    {
        scrollY_ -= e.scrollY * rowHeight_ * 2.0f;
        scrollY_ = clamp(scrollY_, 0.0f, std::max(0.0f, contentH - bodyH));
    }
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Keyboard events
// ═════════════════════════════════════════════════════════════════════════════

void DataGrid::onKeyPress(KeyEvent& e)
{
    ensureSortOrder();
    if (!focused_ && !editing_) return;

    if (editing_)
    {
        e.consumed = true;
        switch (e.key)
        {
            case ig::retained::Key::Return:
            case ig::retained::Key::KPEnter:
                commitEdit();
                return;
            case ig::retained::Key::Escape:
                cancelEdit();
                return;
            case ig::retained::Key::Backspace:
                if (editCursor_ > 0)
                {
                    const int prev = static_cast<int>(utf8PrevOffset(editBuf_, editCursor_));
                    editBuf_.erase(prev, editCursor_ - prev);
                    editCursor_ = prev;
                    markDirty();
                }
                return;
            case ig::retained::Key::Delete:
                if (editCursor_ < static_cast<int>(editBuf_.size()))
                {
                    const int next = static_cast<int>(utf8NextOffset(editBuf_, editCursor_));
                    editBuf_.erase(editCursor_, next - editCursor_);
                    markDirty();
                }
                return;
            case ig::retained::Key::Left:
                if (editCursor_ > 0)
                {
                    editCursor_ = static_cast<int>(utf8PrevOffset(editBuf_, editCursor_));
                    markDirty();
                }
                return;
            case ig::retained::Key::Right:
                if (editCursor_ < static_cast<int>(editBuf_.size()))
                {
                    editCursor_ = static_cast<int>(utf8NextOffset(editBuf_, editCursor_));
                    markDirty();
                }
                return;
            case ig::retained::Key::Home:
                editCursor_ = 0;
                markDirty();
                return;
            case ig::retained::Key::End:
                editCursor_ = static_cast<int>(editBuf_.size());
                markDirty();
                return;
            case ig::retained::Key::Tab:
            {
                const int cols = static_cast<int>(columns_.size());
                const int step = e.shift ? -1 : 1;
                int visual = editRow_;
                for (int v = 0; v < static_cast<int>(sortedIndices_.size()); ++v)
                    if (sortedIndices_[v] == editRow_)
                    {
                        visual = v;
                        break;
                    }
                int cell = visual * cols + editCol_ + step;
                commitEdit();
                for (; cell >= 0 && cell < modelRowCount() * cols && !editing_; cell += step)
                    startEdit(displayRow(cell / cols), cell % cols);
                return;
            }
            case ig::retained::Key::C:
                if (e.ctrl)
                {
                    WidgetApp::instance().setClipboardText(editBuf_.c_str());
                    return;
                }
                break;
            case ig::retained::Key::V:
                if (e.ctrl)
                {
                    String clip = WidgetApp::instance().getClipboardText();
                    if (!clip.empty())
                    {
                        editBuf_.insert(editCursor_, clip);
                        editCursor_ += static_cast<int>(clip.size());
                        markDirty();
                    }
                    return;
                }
                break;
            default:
                break;
        }
    }

    if (!editing_ && e.ctrl)
    {
        if (e.key == ig::retained::Key::C)
        {
            e.consumed = true;
            String text;
            int nc = static_cast<int>(columns_.size());
            for (int c = 0; c < nc; ++c)
            {
                if (c > 0) text += '\t';
                text += columns_[c].name;
            }
            text += '\n';
            for (int ri: selectedRows_)
            {
                if (ri < 0 || ri >= modelRowCount()) continue;
                for (int c = 0; c < nc; ++c)
                {
                    if (c > 0) text += '\t';
                    text += modelCell(ri, c);
                }
                text += '\n';
            }
            if (!text.empty()) WidgetApp::instance().setClipboardText(text.c_str());
            return;
        }
        if (e.key == ig::retained::Key::A)
        {
            e.consumed = true;
            selectedRows_.clear();
            selectedSet_.clear();
            int nr = modelRowCount();
            for (int i = 0; i < nr; ++i)
            {
                selectedRows_.push_back(i);
                selectedSet_.insert(i);
            }
            if (nr > 0) selectedRow_ = 0;
            markDirty();
            return;
        }
    }

    if (!editing_)
    {
        if (e.key == ig::retained::Key::Up || e.key == ig::retained::Key::Down ||
                e.key == ig::retained::Key::Home || e.key == ig::retained::Key::End ||
                e.key == ig::retained::Key::PageUp || e.key == ig::retained::Key::PageDown)
        {
            // Move by position on screen, which a sorted column reorders
            const int count = modelRowCount();
            int pos = -1;
            for (int i = 0; i < count && pos < 0; ++i)
                if (displayRow(i) == selectedRow_) pos = i;
            const float bodyH = rect_.h - headerHeight_;
            const int next = listKeyTarget(e.key, pos, count, static_cast<int>(bodyH / rowHeight_));
            if (next != pos)
            {
                e.consumed = true;
                setSelectedRow(displayRow(next));
                const float top = static_cast<float>(next) * rowHeight_;
                if (top < scrollY_) scrollY_ = top;
                else if (top + rowHeight_ > scrollY_ + bodyH)
                    scrollY_ = top + rowHeight_ - bodyH;
                selectionChanged.emit(selectedRow_);
            }
        }
        else if (e.key == ig::retained::Key::Return || e.key == ig::retained::Key::KPEnter)
        {
            e.consumed = true;
            if (selectedRow_ >= 0) startEdit(selectedRow_, 0);
        }
        else if (e.key == ig::retained::Key::Space && showCheckboxes_)
        {
            e.consumed = true;
            if (selectedRow_ >= 0 && selectedRow_ < modelRowCount())
            {
                if (model_)
                {
                    bool cur = model_->data(model_->index(selectedRow_, 0), ItemRole::Check) == "1";
                    model_->setData(model_->index(selectedRow_, 0), cur ? "0" : "1",
                            ItemRole::Check);
                }
                else
                {
                    rows_[selectedRow_].checked = !rows_[selectedRow_].checked;
                }
                rowCheckChanged.emit(selectedRow_, modelIsChecked(selectedRow_));
                markDirty();
            }
        }
    }
}

void DataGrid::onTextInput(KeyEvent& e)
{
    if (!editing_) return;
    e.consumed = true;
    editBuf_.insert(editCursor_, e.text);
    editCursor_ += static_cast<int>(strlen(e.text));
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  DataGrid — Model/View integration
// ═════════════════════════════════════════════════════════════════════════════

void DataGrid::detachModel()
{
    if (model_) model_->disconnect(modelSlots_);
    model_ = nullptr;
}

void DataGrid::setModel(AbstractItemModel* model)
{
    detachModel();
    model_ = model;
    if (!model_) return;

    // Sync columns from model headers if we have no columns yet
    if (columns_.empty())
    {
        int nc = model_->columnCount();
        for (int i = 0; i < nc; ++i)
        {
            Column c;
            c.name = model_->headerData(i);
            c.width = 120.0f;
            c.sortable = true;
            columns_.push_back(std::move(c));
        }
        invalidateColumns();
    }

    // Connect model signals to refresh the view
    modelSlots_.reset = model_->modelReset.connect(
            [this]
            {
                selectedRow_ = -1;
                selectedRows_.clear();
                selectedSet_.clear();
                scrollY_ = 0;
                sortDirty_ = true;
                markDirty();
            });
    modelSlots_.inserted = model_->rowsInserted.connect(
            [this]
            {
                sortDirty_ = true;
                markDirty();
            });
    modelSlots_.removed = model_->rowsRemoved.connect(
            [this]
            {
                sortDirty_ = true;
                markDirty();
            });
    modelSlots_.changed = model_->dataChanged.connect([this](ModelIndex) { markDirty(); });
    modelSlots_.layout = model_->layoutChanged.connect([this] { markDirty(); });
    modelSlots_.destroyed = model_->destroyed.connect(
            [this]
            {
                model_ = nullptr;
                modelSlots_ = AbstractItemModel::Connections();
                selectedRow_ = -1;
                selectedRows_.clear();
                selectedSet_.clear();
                scrollY_ = 0;
                sortDirty_ = true;
                markDirty();
            });

    sortDirty_ = true;
    markDirty();
}

int DataGrid::modelRowCount() const
{
    if (model_) return model_->rowCount();
    return static_cast<int>(rows_.size());
}

int DataGrid::modelColCount() const
{
    if (model_) return model_->columnCount();
    return static_cast<int>(columns_.size());
}

String DataGrid::modelCell(int row, int col) const
{
    if (model_) return model_->data(model_->index(row, col));
    if (row >= 0 && row < static_cast<int>(rows_.size()) && col >= 0 &&
            col < static_cast<int>(rows_[row].cells.size()))
        return rows_[row].cells[col];
    return {};
}

const String& DataGrid::cellRef(int row, int col, String& scratch) const
{
    if (model_)
    {
        scratch = model_->data(model_->index(row, col));
        return scratch;
    }
    if (row >= 0 && row < static_cast<int>(rows_.size()) && col >= 0 &&
            col < static_cast<int>(rows_[row].cells.size()))
        return rows_[row].cells[col];
    scratch.clear();
    return scratch;
}

String DataGrid::modelHeader(int col) const
{
    if (model_) return model_->headerData(col);
    if (col >= 0 && col < static_cast<int>(columns_.size())) return columns_[col].name;
    return {};
}

bool DataGrid::modelIsChecked(int row) const
{
    if (model_) return model_->data(model_->index(row, 0), ItemRole::Check) == "1";
    if (row >= 0 && row < static_cast<int>(rows_.size())) return rows_[row].checked;
    return false;
}

bool DataGrid::modelIsEditable(int row, int col) const
{
    if (model_) return (model_->flags(model_->index(row, col)) & AbstractItemModel::Editable) != 0;
    if (col >= 0 && col < static_cast<int>(columns_.size())) return !columns_[col].readOnly;
    return false;
}


// ═════════════════════════════════════════════════════════════════════════════
//  TreeGrid::Node
// ═════════════════════════════════════════════════════════════════════════════

TreeGrid::Node* TreeGrid::Node::addChild(const ct::Vector<String>& cellData)
{
    auto* child = new Node();
    child->cells = cellData;
    child->parent = this;
    child->owner_ = owner_;
    child->depth = depth + 1;
    children.push_back(child);
    if (owner_)
    {
        owner_->flatDirty_ = true;
        owner_->markDirty();
    }
    return child;
}

void TreeGrid::Node::setExpanded(bool e)
{
    expanded = e;
    if (owner_) owner_->rebuild();
}

// ═════════════════════════════════════════════════════════════════════════════
//  TreeGrid - constructor / destructor
// ═════════════════════════════════════════════════════════════════════════════

TreeGrid::TreeGrid()
{
    acceptsFocus_ = true;
}

TreeGrid::~TreeGrid()
{
    for (auto* r: roots_) deleteNodes(r);
}

void TreeGrid::deleteNodes(Node* n)
{
    for (auto* c: n->children) deleteNodes(c);
    delete n;
}

void TreeGrid::clearAll()
{
    for (auto* r: roots_) deleteNodes(r);
    roots_.clear();
    flatList_.clear();
    selectedNode_ = nullptr;
    selectedNodes_.clear();
    scrollY_ = 0;
    markDirty();
}

int TreeGrid::nodeCount() const
{
    int count = 0;
    ct::Vector<const Node*> stack;
    for (auto* r: roots_) stack.push_back(r);
    while (!stack.empty())
    {
        auto* n = stack.back();
        stack.pop_back();
        count++;
        for (auto* c: n->children) stack.push_back(c);
    }
    return count;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Columns
// ═════════════════════════════════════════════════════════════════════════════

int TreeGrid::addColumn(const String& name, float width, bool sortable)
{
    Column c;
    c.name = name;
    c.width = width;
    c.sortable = sortable;
    columns_.push_back(std::move(c));
    invalidateColumns();
    markDirty();
    return static_cast<int>(columns_.size()) - 1;
}

void TreeGrid::setColumnWidth(int col, float w)
{
    if (col < 0 || col >= static_cast<int>(columns_.size()) || !(w == w)) return;
    columns_[col].width = w > columns_[col].minWidth ? w : columns_[col].minWidth;
    invalidateColumns();
    markDirty();
}

float TreeGrid::columnWidth(int col) const
{
    ensureColumns();
    return col >= 0 && col < static_cast<int>(colW_.size()) ? colW_[col] : 0.f;
}

void TreeGrid::ensureColumns() const
{
    const float gridW = layoutWidth(rect_.w);
    if (!colDirty_ && colW_.size() == columns_.size() && colGridW_ == gridW) return;
    colDirty_ = false;
    colGridW_ = gridW;
    layoutColumns(columns_, stretchLast_, gridW, 0.0f, colW_, colTotal_);
    // A grid scrolled sideways does not stay scrolled past the end of its columns when they get
    // narrower or the grid wider (a window made larger)
    const float most = colTotal_ > gridW ? colTotal_ - gridW : 0.0f;
    if (scrollX_ > most) scrollX_ = most;
}

void TreeGrid::reserveMemos(const Rect& abs)
{
    // the rows and columns that can be in sight, with some to spare for the scroll
    const float bodyH = abs.h - headerHeight_;
    const int rows = (bodyH > 0.0f && rowHeight_ > 0.0f ? static_cast<int>(bodyH / rowHeight_) : 0) + 4;
    int cols = 0;
    float x = abs.x - scrollX_;
    for (size_t i = 0; i < colW_.size(); ++i)
    {
        if (x + colW_[i] > abs.x && x < abs.x + abs.w) ++cols;
        x += colW_[i];
    }
    memos_.reserve(rows, cols + 2);
}

void TreeGrid::setColumnStretch(int col, bool stretch)
{
    if (col >= 0 && col < static_cast<int>(columns_.size()))
    {
        columns_[col].stretch = stretch;
        invalidateColumns();
        markDirty();
    }
}

bool TreeGrid::columnStretch(int col) const
{
    return col >= 0 && col < static_cast<int>(columns_.size()) && columns_[col].stretch;
}

void TreeGrid::setStretchLastColumn(bool stretch)
{
    stretchLast_ = stretch;
    invalidateColumns();
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Roots
// ═════════════════════════════════════════════════════════════════════════════

TreeGrid::Node* TreeGrid::addRoot(const ct::Vector<String>& cells)
{
    auto* n = new Node();
    n->cells = cells;
    n->depth = 0;
    n->owner_ = this;
    roots_.push_back(n);
    flatDirty_ = true;
    markDirty();
    return n;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Flat list
// ═════════════════════════════════════════════════════════════════════════════

void TreeGrid::rebuildFlatList() const
{
    flatDirty_ = false;
    flatList_.clear();
    for (auto* r: roots_) flattenNode(r, 0);
}

void TreeGrid::flattenNode(Node* n, int depth) const
{
    flatList_.push_back({ n, depth });
    if (n->expanded)
    {
        for (auto* c: n->children) flattenNode(c, depth + 1);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Sort
// ═════════════════════════════════════════════════════════════════════════════

void TreeGrid::sortChildren(Node* parent)
{
    if (sortCol_ < 0 || sortOrder_ == DataGrid::SortOrder::None) return;
    int col = sortCol_;
    bool asc = (sortOrder_ == DataGrid::SortOrder::Ascending);

    auto cmp = [col, asc](Node* a, Node* b)
    {
        const String& sa = (col < static_cast<int>(a->cells.size())) ? a->cells[col] : "";
        const String& sb = (col < static_cast<int>(b->cells.size())) ? b->cells[col] : "";
        char* endA;
        char* endB;
        double da = strtod(sa.c_str(), &endA);
        double db = strtod(sb.c_str(), &endB);
        if (endA != sa.c_str() && endB != sb.c_str()) return asc ? da < db : da > db;
        return asc ? sa < sb : sa > sb;
    };

    if (parent)
    {
        std::stable_sort(parent->children.begin(), parent->children.end(), cmp);
        for (auto* c: parent->children) sortChildren(c);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Selection
// ═════════════════════════════════════════════════════════════════════════════

void TreeGrid::clearSelection()
{
    selectedNode_ = nullptr;
    selectedNodes_.clear();
    markDirty();
}

bool TreeGrid::isSelected(Node* n) const
{
    if (!multiSelect_) return n == selectedNode_;
    for (auto* s: selectedNodes_)
        if (s == n) return true;
    return false;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Geometry helpers
// ═════════════════════════════════════════════════════════════════════════════

float TreeGrid::totalColumnsWidth() const
{
    ensureColumns();
    return colTotal_;
}

float TreeGrid::columnX(int col) const
{
    ensureColumns();
    float x = 0;
    for (int i = 0; i < col && i < static_cast<int>(colW_.size()); ++i) x += colW_[i];
    return x;
}

int TreeGrid::hitColumn(float localX) const
{
    ensureColumns();
    float x = -scrollX_;
    for (int i = 0; i < static_cast<int>(colW_.size()); ++i)
    {
        if (localX >= x && localX < x + colW_[i]) return i;
        x += colW_[i];
    }
    return -1;
}

int TreeGrid::hitRow(float localY) const
{
    ensureFlatList();
    if (localY < headerHeight_) return -1;
    float y = localY - headerHeight_ + scrollY_;
    int idx = static_cast<int>(y / rowHeight_);
    if (idx < 0 || idx >= static_cast<int>(flatList_.size())) return -1;
    return idx;
}

int TreeGrid::hitResizeEdge(float localX) const
{
    ensureColumns();
    float x = -scrollX_;
    for (int i = 0; i < static_cast<int>(colW_.size()); ++i)
    {
        x += colW_[i];
        // a stretching column is as wide as the grid makes it: its edge is not one to drag
        const float away = localX - x;
        if (away <= 4.0f && away >= -4.0f && !isStretching(columns_, stretchLast_, static_cast<size_t>(i))) return i;
    }
    return -1;
}

float TreeGrid::minColumnsWidth() const
{
    float w = 0;
    for (auto& c: columns_) w += c.width;
    return w;
}

Widget::Vec2f TreeGrid::sizeHint() const
{
    ensureFlatList();
    // the least the columns need: the stretching ones are as wide as the grid makes them
    return { minColumnsWidth(), headerHeight_ + flatList_.size() * rowHeight_ };
}

// ═════════════════════════════════════════════════════════════════════════════
//  Read-only / inline edit
// ═════════════════════════════════════════════════════════════════════════════

void TreeGrid::setColumnReadOnly(int col, bool ro)
{
    if (col >= 0 && col < static_cast<int>(columns_.size())) columns_[col].readOnly = ro;
}

void TreeGrid::startEdit(Node* node, int col)
{
    if (!node || col < 0 || col >= static_cast<int>(columns_.size())) return;
    if (readOnly_ || columns_[col].readOnly || node->readOnly) return;
    editing_ = true;
    editNode_ = node;
    editCol_ = col;
    editBuf_ = (col < static_cast<int>(node->cells.size())) ? node->cells[col] : "";
    editCursor_ = static_cast<int>(editBuf_.size());
    editScroll_ = 0.f;
    markDirty();
}

void TreeGrid::commitEdit()
{
    if (!editing_ || !editNode_) return;
    editing_ = false;
    if (editCol_ >= 0 && editCol_ < static_cast<int>(editNode_->cells.size()))
    {
        editNode_->cells[editCol_] = editBuf_;
        cellEdited.emit(editNode_, editCol_);
    }
    editNode_ = nullptr;
    editCol_ = -1;
    markDirty();
}

void TreeGrid::cancelEdit()
{
    editing_ = false;
    editNode_ = nullptr;
    editCol_ = -1;
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Paint
// ═════════════════════════════════════════════════════════════════════════════

void TreeGrid::paint(PaintContext& ctx)
{
    ensureFlatList();
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();
    ensureColumns();
    reserveMemos(abs);

    ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, t.panelColor.a);
    ctx.fillRect(abs.x, abs.y, abs.w, abs.h);

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();
    const uint32_t fontId = ctx.fontStamp().id; // the font stays this one all through the paint

    // ── Column headers ───────────────────────────────────────────────────
    Rect headerClip = { abs.x, abs.y, abs.w, headerHeight_ };
    ctx.pushClip(headerClip);
    {
        Color hdrBg(static_cast<uint8_t>(std::min(255, t.panelColor.r + 15)),
                static_cast<uint8_t>(std::min(255, t.panelColor.g + 15)),
                static_cast<uint8_t>(std::min(255, t.panelColor.b + 15)), 255);
        ctx.fill.SetColor(hdrBg.r, hdrBg.g, hdrBg.b, hdrBg.a);
        ctx.fillRect(abs.x, abs.y, abs.w, headerHeight_);

        float hx = abs.x - scrollX_;
        float hdrTextY = abs.y + (headerHeight_ - t.fontSize) * 0.5f + asc;

        for (int i = 0; i < static_cast<int>(columns_.size()); ++i)
        {
            auto& col = columns_[i];
            const float colW = colW_[i];
            float colR = hx + colW;
            if (colR <= abs.x || hx >= abs.x + abs.w)
            {
                hx = colR; // out of sight: nothing to measure or draw
                continue;
            }

            // The name is cut with "..." before the arrow of a sorted column (or the right edge), so
            // it never reaches the next column.
            const bool arrow = sortCol_ == i && sortOrder_ != DataGrid::SortOrder::None;
            const float textRight = colR - (arrow ? kSortArrowRoom : kCellPad);
            ctx.font.SetColor(t.textColor);
            ctx.printElided({ hx + kCellPad, abs.y, textRight - (hx + kCellPad), headerHeight_ }, col.name,
                    hdrTextY, asc, memos_.header(i), fontId);

            if (arrow)
            {
                float arrowX = colR - 14.0f;
                float arrowY = abs.y + headerHeight_ * 0.5f;
                ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
                // cut at the column: in a column too narrow for it the arrow would be drawn over
                // the one before
                ctx.pushClip({ hx, abs.y, colW, headerHeight_ });
                if (sortOrder_ == DataGrid::SortOrder::Ascending)
                    ctx.fillTriangle(arrowX, arrowY + 3, arrowX + 6, arrowY + 3, arrowX + 3,
                            arrowY - 3);
                else
                    ctx.fillTriangle(arrowX, arrowY - 3, arrowX + 6, arrowY - 3, arrowX + 3,
                            arrowY + 3);
                ctx.popClip();
            }

            ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 80);
            ctx.drawLine(colR, abs.y + 2, colR, abs.y + headerHeight_ - 2);
            hx = colR;
        }

        ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
        ctx.drawLine(abs.x, abs.y + headerHeight_, abs.x + abs.w, abs.y + headerHeight_);
    }
    ctx.popClip();

    // ── Rows ─────────────────────────────────────────────────────────────
    float bodyY = abs.y + headerHeight_;
    float bodyH = abs.h - headerHeight_;
    Rect bodyClip = { abs.x, bodyY, abs.w, bodyH };
    ctx.pushClip(bodyClip);
    {
        int totalVis = static_cast<int>(flatList_.size());
        int firstVis = static_cast<int>(scrollY_ / rowHeight_);
        int lastVis = firstVis + static_cast<int>(bodyH / rowHeight_) + 2;
        if (firstVis < 0) firstVis = 0;
        if (lastVis > totalVis) lastVis = totalVis;

        const int memoCols = memos_.cols();
        for (int vi = firstVis; vi < lastVis; ++vi)
        {
            auto& entry = flatList_[vi];
            Node* node = entry.node;
            int depth = entry.depth;
            ElideMemo* const memoRow = memos_.row(vi);

            float ry = bodyY + vi * rowHeight_ - scrollY_;
            Rect rowR = { abs.x, ry, abs.w, rowHeight_ };
            if (ctx.isClipped(rowR)) continue;

            if (zebraStripes_ && (vi % 2 == 1))
            {
                ctx.fill.SetColor(static_cast<uint8_t>(std::min(255, t.panelColor.r + 5)),
                        static_cast<uint8_t>(std::min(255, t.panelColor.g + 5)),
                        static_cast<uint8_t>(std::min(255, t.panelColor.b + 5)), 255);
                ctx.fillRect(abs.x, ry, abs.w, rowHeight_);
            }

            if (isSelected(node))
            {
                ctx.fill.SetColor(t.selectionColor.r, t.selectionColor.g, t.selectionColor.b,
                        t.selectionColor.a);
                ctx.fillRect(abs.x, ry, abs.w, rowHeight_);
            }

            if (hoveredRow_ == vi && !isSelected(node))
            {
                ctx.fill.SetColor(t.buttonHover.r, t.buttonHover.g, t.buttonHover.b, 60);
                ctx.fillRect(abs.x, ry, abs.w, rowHeight_);
            }

            float textY = ry + (rowHeight_ - t.fontSize) * 0.5f + asc;

            float cx = abs.x - scrollX_;
            int memoCol = 0; // ci % memoCols
            for (int ci = 0; ci < static_cast<int>(columns_.size()); ++ci, memoCol = memoCol + 1 == memoCols ? 0 : memoCol + 1)
            {
                float colW = colW_[ci];
                if (cx + colW <= abs.x || cx >= abs.x + abs.w)
                {
                    cx += colW; // out of sight: nothing to measure or draw
                    continue;
                }

                if (ci == 0)
                {
                    float indent = depth * indentWidth_;
                    float arrowX = cx + indent + 2;
                    float arrowCY = ry + rowHeight_ * 0.5f;
                    float textX = cx + indent + 14;
                    const float iconS = rowHeight_ - 6;
                    const bool hasIcon = node->iconId != IconId::None && ctx.icons;
                    const bool hasArrow = !node->children.empty();

                    // The expander and the icon come before the text, and a deep node in a narrow
                    // column has them past the right edge of the column (a pixel more for the
                    // anti-aliasing): cut there, so they are not drawn over the next column.
                    const bool spills = (hasArrow && arrowX + 8 + 1 > cx + colW) || (hasIcon && textX + iconS + 1 > cx + colW);
                    if (spills) ctx.pushClip({ cx, ry, colW, rowHeight_ });

                    if (hasArrow)
                    {
                        ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, 180);
                        if (node->expanded)
                            ctx.fillTriangle(arrowX, arrowCY - 4, arrowX + 8, arrowCY - 4,
                                    arrowX + 4, arrowCY + 4);
                        else
                            ctx.fillTriangle(arrowX, arrowCY - 4, arrowX + 8, arrowCY, arrowX,
                                    arrowCY + 4);
                    }

                    if (hasIcon)
                    {
                        ctx.drawIcon(node->iconId, textX, ry + 3, iconS);
                        textX += iconS + 2;
                    }
                    if (spills) ctx.popClip();

                    if (editing_ && editNode_ == node && editCol_ == 0)
                    {
                        // the field starts at the text and ends a little before the column does
                        const float fieldX = textX - 1;
                        const float fieldW = cx + colW - 3 - fieldX;
                        if (fieldW > 0.0f)
                        {
                            ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, 255);
                            ctx.fillRect(fieldX, ry + 1, fieldW, rowHeight_ - 2);
                            ctx.line.SetColor(t.inputBorderHover.r, t.inputBorderHover.g,
                                    t.inputBorderHover.b, 255);
                            ctx.lineRect(fieldX, ry + 1, fieldW, rowHeight_ - 2);
                            ctx.font.SetColor(Color(255, 255, 255, 255));
                            paintEditText(ctx, editBuf_, editCursor_, editScroll_, fieldX, fieldW, ry, rowHeight_, textY, asc);
                        }
                    }
                    else
                    {
                        if (!node->cells.empty())
                        {
                            ctx.font.SetColor(
                                    isSelected(node) ? Color(255, 255, 255, 255) : t.textColor);
                            const float textLeft = textX + 2;
                            printCell(ctx, { textLeft, ry, cx + colW - kCellPad - textLeft, rowHeight_ }, node->cells[0], textY,
                                    asc, memoRow[memoCol], fontId);
                        }
                    }
                }
                else
                {
                    if (editing_ && editNode_ == node && editCol_ == ci)
                    {
                        ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, 255);
                        ctx.fillRect(cx + 1, ry + 1, colW - 2, rowHeight_ - 2);
                        ctx.line.SetColor(t.inputBorderHover.r, t.inputBorderHover.g,
                                t.inputBorderHover.b, 255);
                        ctx.lineRect(cx + 1, ry + 1, colW - 2, rowHeight_ - 2);
                        ctx.font.SetColor(Color(255, 255, 255, 255));
                        paintEditText(ctx, editBuf_, editCursor_, editScroll_, cx + 1, colW - 2, ry, rowHeight_, textY, asc);
                    }
                    else
                    {
                        if (ci < static_cast<int>(node->cells.size()))
                        {
                            ctx.font.SetColor(isSelected(node) ? Color(255, 255, 255, 255)
                                                               : Color(170, 170, 175, 255));
                            printCell(ctx, { cx + kCellPad, ry, colW - 2.0f * kCellPad, rowHeight_ }, node->cells[ci], textY, asc,
                                    memoRow[memoCol], fontId);
                        }
                    }
                }

                ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 30);
                ctx.drawLine(cx + colW, ry, cx + colW, ry + rowHeight_);
                cx += colW;
            }

            ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 30);
            ctx.drawLine(abs.x, ry + rowHeight_, abs.x + abs.w, ry + rowHeight_);
        }
    }
    ctx.popClip();

    // ── Scrollbars ───────────────────────────────────────────────────────
    float contentH = static_cast<float>(flatList_.size()) * rowHeight_;
    float contentW = totalColumnsWidth();

    if (bodyH > 0.0f && contentH > bodyH)
    {
        float sbW = 8.0f;
        float sbX = abs.x + abs.w - sbW;
        float trackH = bodyH;
        float thumbH = std::max(20.0f, (bodyH / contentH) * trackH);
        float maxSY = contentH - bodyH;
        float thumbY = bodyY + (maxSY > 0 ? (scrollY_ / maxSY) * (trackH - thumbH) : 0);
        ctx.fill.SetColor(static_cast<uint8_t>(std::min(255, t.panelColor.r + 20)),
                static_cast<uint8_t>(std::min(255, t.panelColor.g + 20)),
                static_cast<uint8_t>(std::min(255, t.panelColor.b + 20)), 150);
        ctx.fillRect(sbX, bodyY, sbW, trackH);
        ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, 80);
        ctx.fillRect(sbX + 1, thumbY, sbW - 2, thumbH);
    }

    if (abs.w > 0.0f && contentW > abs.w)
    {
        float sbH = 8.0f;
        float sbY = abs.y + abs.h - sbH;
        float trackW = abs.w;
        float thumbW = std::max(20.0f, (abs.w / contentW) * trackW);
        float maxSX = contentW - abs.w;
        float thumbX = abs.x + (maxSX > 0 ? (scrollX_ / maxSX) * (trackW - thumbW) : 0);
        ctx.fill.SetColor(static_cast<uint8_t>(std::min(255, t.panelColor.r + 20)),
                static_cast<uint8_t>(std::min(255, t.panelColor.g + 20)),
                static_cast<uint8_t>(std::min(255, t.panelColor.b + 20)), 150);
        ctx.fillRect(abs.x, sbY, trackW, sbH);
        ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, 80);
        ctx.fillRect(thumbX, sbY + 1, thumbW, sbH - 2);
    }

    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.lineRect(abs.x, abs.y, abs.w, abs.h);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Mouse events
// ═════════════════════════════════════════════════════════════════════════════

void TreeGrid::onMousePress(MouseEvent& e)
{
    ensureFlatList();
    if (!visible_ || !isEnabledInTree()) return;
    Rect abs = absoluteRect();
    if (!abs.contains(e.x, e.y)) return;

    e.consumed = true;
    float localX = e.x - abs.x;
    float localY = e.y - abs.y;

    if (localY < headerHeight_)
    {
        int resEdge = hitResizeEdge(localX);
        if (resEdge >= 0)
        {
            resizeCol_ = resEdge;
            resizeStartX_ = e.x;
            resizeStartW_ = colW_[resEdge];
            return;
        }

        int col = hitColumn(localX);
        if (col >= 0 && columns_[col].sortable)
        {
            if (sortCol_ == col)
            {
                if (sortOrder_ == DataGrid::SortOrder::None)
                    sortOrder_ = DataGrid::SortOrder::Ascending;
                else if (sortOrder_ == DataGrid::SortOrder::Ascending)
                    sortOrder_ = DataGrid::SortOrder::Descending;
                else
                    sortOrder_ = DataGrid::SortOrder::None;
            }
            else
            {
                sortCol_ = col;
                sortOrder_ = DataGrid::SortOrder::Ascending;
            }
            for (auto* r: roots_) sortChildren(r);
            rebuildFlatList();
            columnSorted.emit(col);
            markDirty();
        }
        return;
    }

    int visIdx = hitRow(localY);
    if (visIdx < 0 || visIdx >= static_cast<int>(flatList_.size())) return;
    Node* node = flatList_[visIdx].node;
    int dep = flatList_[visIdx].depth;

    float arrowAreaEnd = dep * indentWidth_ + 14;
    int col = hitColumn(localX);
    if (col == 0 && (localX + scrollX_) < arrowAreaEnd && !node->children.empty())
    {
        node->expanded = !node->expanded;
        if (node->expanded) nodeExpanded.emit(node);
        else
            nodeCollapsed.emit(node);
        rebuildFlatList();
        markDirty();
        return;
    }

    static uint32_t lastClickTime = 0;
    static Node* lastClickNode = nullptr;
    uint32_t now = WidgetApp::instance().elapsedMs();

    if (node == lastClickNode && (now - lastClickTime) < 400 && col >= 0)
    {
        startEdit(node, col);
        lastClickTime = 0;
        lastClickNode = nullptr;
        return;
    }
    lastClickTime = now;
    lastClickNode = node;

    if (editing_) commitEdit();

    if (multiSelect_)
    {
        bool ctrl = e.ctrl;
        if (ctrl)
        {
            auto it = std::find(selectedNodes_.begin(), selectedNodes_.end(), node);
            if (it != selectedNodes_.end()) selectedNodes_.erase(it);
            else
                selectedNodes_.push_back(node);
            selectedNode_ = node;
        }
        else
        {
            selectedNode_ = node;
            selectedNodes_ = { node };
        }
    }
    else
    {
        selectedNode_ = node;
        selectedNodes_ = { node };
    }
    selectionChanged.emit(node);
    markDirty();
}

void TreeGrid::onMouseRelease(MouseEvent& e)
{
    if (resizeCol_ >= 0) resizeCol_ = -1;
    (void) e;
}

void TreeGrid::onMouseMove(MouseEvent& e)
{
    if (!visible_ || !isEnabledInTree()) return;
    Rect abs = absoluteRect();

    if (resizeCol_ >= 0)
    {
        float dx = e.x - resizeStartX_;
        columns_[resizeCol_].width = std::max(columns_[resizeCol_].minWidth, resizeStartW_ + dx);
        invalidateColumns();
        markDirty();
        return;
    }

    float localY = e.y - abs.y;
    int visIdx = hitRow(localY);
    if (visIdx != hoveredRow_)
    {
        hoveredRow_ = visIdx;
        markDirty();
    }
}

void TreeGrid::onMouseScroll(MouseEvent& e)
{
    ensureFlatList();
    if (!visible_ || !isEnabledInTree()) return;
    Rect abs = absoluteRect();
    if (!abs.contains(e.x, e.y)) return;

    e.consumed = true;
    bool shift = e.shift;
    float bodyH = abs.h - headerHeight_;
    float contentH = static_cast<float>(flatList_.size()) * rowHeight_;
    float contentW = totalColumnsWidth();

    if (shift)
    {
        scrollX_ -= e.scrollY * 40.0f;
        scrollX_ = clamp(scrollX_, 0.0f, std::max(0.0f, contentW - abs.w));
    }
    else
    {
        scrollY_ -= e.scrollY * rowHeight_ * 2.0f;
        scrollY_ = clamp(scrollY_, 0.0f, std::max(0.0f, contentH - bodyH));
    }
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Keyboard
// ═════════════════════════════════════════════════════════════════════════════

void TreeGrid::onKeyPress(KeyEvent& e)
{
    ensureFlatList();
    if (!focused_ && !editing_) return;

    if (editing_)
    {
        e.consumed = true;
        switch (e.key)
        {
            case ig::retained::Key::Return:
            case ig::retained::Key::KPEnter:
                commitEdit();
                return;
            case ig::retained::Key::Escape:
                cancelEdit();
                return;
            case ig::retained::Key::Backspace:
                if (editCursor_ > 0)
                {
                    const int prev = static_cast<int>(utf8PrevOffset(editBuf_, editCursor_));
                    editBuf_.erase(prev, editCursor_ - prev);
                    editCursor_ = prev;
                    markDirty();
                }
                return;
            case ig::retained::Key::Delete:
                if (editCursor_ < static_cast<int>(editBuf_.size()))
                {
                    const int next = static_cast<int>(utf8NextOffset(editBuf_, editCursor_));
                    editBuf_.erase(editCursor_, next - editCursor_);
                    markDirty();
                }
                return;
            case ig::retained::Key::Left:
                if (editCursor_ > 0)
                {
                    editCursor_ = static_cast<int>(utf8PrevOffset(editBuf_, editCursor_));
                    markDirty();
                }
                return;
            case ig::retained::Key::Right:
                if (editCursor_ < static_cast<int>(editBuf_.size()))
                {
                    editCursor_ = static_cast<int>(utf8NextOffset(editBuf_, editCursor_));
                    markDirty();
                }
                return;
            case ig::retained::Key::Home:
                editCursor_ = 0;
                markDirty();
                return;
            case ig::retained::Key::End:
                editCursor_ = static_cast<int>(editBuf_.size());
                markDirty();
                return;
            case ig::retained::Key::C:
                if (e.ctrl)
                {
                    WidgetApp::instance().setClipboardText(editBuf_.c_str());
                    return;
                }
                break;
            case ig::retained::Key::V:
                if (e.ctrl)
                {
                    String clip = WidgetApp::instance().getClipboardText();
                    if (!clip.empty())
                    {
                        editBuf_.insert(editCursor_, clip);
                        editCursor_ += static_cast<int>(clip.size());
                        markDirty();
                    }
                    return;
                }
                break;
            default:
                break;
        }
    }

    if (!editing_)
    {
        Node* before = selectedNode_;
        int curIdx = -1;
        for (int i = 0; i < static_cast<int>(flatList_.size()); ++i)
            if (flatList_[i].node == selectedNode_)
            {
                curIdx = i;
                break;
            }

        const int page = static_cast<int>((rect_.h - headerHeight_) / rowHeight_);
        const int target = listKeyTarget(e.key, curIdx, static_cast<int>(flatList_.size()), page);
        if (target != curIdx)
        {
            e.consumed = true;
            selectedNode_ = flatList_[target].node;
            selectedNodes_ = { selectedNode_ };
            selectionChanged.emit(selectedNode_);
            markDirty();
        }
        else if (e.key == ig::retained::Key::Left && selectedNode_)
        {
            e.consumed = true;
            if (selectedNode_->expanded && !selectedNode_->children.empty())
            {
                selectedNode_->expanded = false;
                nodeCollapsed.emit(selectedNode_);
                rebuildFlatList();
            }
            else if (selectedNode_->parent)
            {
                selectedNode_ = selectedNode_->parent;
                selectedNodes_ = { selectedNode_ };
                selectionChanged.emit(selectedNode_);
            }
            markDirty();
        }
        else if (e.key == ig::retained::Key::Right && selectedNode_)
        {
            e.consumed = true;
            if (!selectedNode_->expanded && !selectedNode_->children.empty())
            {
                selectedNode_->expanded = true;
                nodeExpanded.emit(selectedNode_);
                rebuildFlatList();
            }
            else if (!selectedNode_->children.empty())
            {
                selectedNode_ = selectedNode_->children[0];
                selectedNodes_ = { selectedNode_ };
                selectionChanged.emit(selectedNode_);
            }
            markDirty();
        }
        else if (e.key == ig::retained::Key::Return && selectedNode_)
        {
            e.consumed = true;
            startEdit(selectedNode_, 0);
        }
        else if (e.key == ig::retained::Key::C && e.ctrl && selectedNode_)
        {
            e.consumed = true;
            String text;
            for (int c = 0; c < static_cast<int>(selectedNode_->cells.size()); ++c)
            {
                if (c > 0) text += '\t';
                text += selectedNode_->cells[c];
            }
            WidgetApp::instance().setClipboardText(text.c_str());
        }

        // Keep the node the arrows moved to in view
        if (selectedNode_ != before && selectedNode_)
        {
            ensureFlatList();
            for (int i = 0; i < static_cast<int>(flatList_.size()); ++i)
            {
                if (flatList_[i].node != selectedNode_) continue;
                const float bodyH = rect_.h - headerHeight_;
                const float top = static_cast<float>(i) * rowHeight_;
                if (top < scrollY_) scrollY_ = top;
                else if (top + rowHeight_ > scrollY_ + bodyH)
                    scrollY_ = top + rowHeight_ - bodyH;
                break;
            }
        }
    }
}

void TreeGrid::onTextInput(KeyEvent& e)
{
    if (!editing_) return;
    e.consumed = true;
    editBuf_.insert(editCursor_, e.text);
    editCursor_ += static_cast<int>(strlen(e.text));
    markDirty();
}
