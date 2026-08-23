#include "pch.hpp"
#include "DataWidgets.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numeric>

using BuGUI::clamp;

// ═════════════════════════════════════════════════════════════════════════════
//  DataGrid - constructor
// ═════════════════════════════════════════════════════════════════════════════

DataGrid::DataGrid()
{
    acceptsFocus_ = true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Columns
// ═════════════════════════════════════════════════════════════════════════════

int DataGrid::addColumn(const std::string& name, float width, bool sortable)
{
    Column c;
    c.name = name;
    c.width = width;
    c.sortable = sortable;
    columns_.push_back(std::move(c));
    rebuildSortOrder();
    markDirty();
    return static_cast<int>(columns_.size()) - 1;
}

void DataGrid::setColumnWidth(int col, float w)
{
    if (col >= 0 && col < static_cast<int>(columns_.size())) {
        columns_[col].width = std::max(w, columns_[col].minWidth);
        markDirty();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Rows
// ═════════════════════════════════════════════════════════════════════════════

int DataGrid::addRow(const std::vector<std::string>& cells)
{
    Row r;
    r.cells = cells;
    while (r.cells.size() < columns_.size())
        r.cells.push_back("");
    rows_.push_back(std::move(r));
    rebuildSortOrder();
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

void DataGrid::setCell(int row, int col, const std::string& value)
{
    if (row >= 0 && row < static_cast<int>(rows_.size()) &&
        col >= 0 && col < static_cast<int>(columns_.size())) {
        rows_[row].cells[col] = value;
        markDirty();
    }
}

std::string DataGrid::cell(int row, int col) const
{
    if (row >= 0 && row < static_cast<int>(rows_.size()) &&
        col >= 0 && col < static_cast<int>(columns_.size()))
        return rows_[row].cells[col];
    return "";
}

void DataGrid::removeRow(int row)
{
    if (row >= 0 && row < static_cast<int>(rows_.size())) {
        rows_.erase(rows_.begin() + row);
        rebuildSortOrder();
        if (selectedRow_ == row) selectedRow_ = -1;
        else if (selectedRow_ > row) selectedRow_--;
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
    if (row >= 0 && row < static_cast<int>(rows_.size()))
        return rows_[row].checked;
    return false;
}

void DataGrid::setRowChecked(int row, bool checked)
{
    if (row >= 0 && row < static_cast<int>(rows_.size())) {
        rows_[row].checked = checked;
        markDirty();
    }
}

std::vector<int> DataGrid::checkedRows() const
{
    std::vector<int> result;
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
    if (row >= 0) { selectedRows_.push_back(row); selectedSet_.insert(row); }
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
    return selectedSet_.count(row) > 0;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Sort
// ═════════════════════════════════════════════════════════════════════════════

void DataGrid::rebuildSortOrder()
{
    int nRows = modelRowCount();
    sortedIndices_.resize(nRows);
    std::iota(sortedIndices_.begin(), sortedIndices_.end(), 0);
    if (sortCol_ >= 0 && sortOrder_ != SortOrder::None)
        sortByColumn(sortCol_);
}

void DataGrid::sortByColumn(int col)
{
    if (col < 0 || col >= static_cast<int>(columns_.size())) return;
    sortCol_ = col;
    bool asc = (sortOrder_ == SortOrder::Ascending);

    if (model_) {
        // Sort via model data
        std::stable_sort(sortedIndices_.begin(), sortedIndices_.end(),
            [&](int a, int b) {
                auto sa = model_->data(model_->index(a, col));
                auto sb = model_->data(model_->index(b, col));
                char* endA; char* endB;
                double da = strtod(sa.c_str(), &endA);
                double db = strtod(sb.c_str(), &endB);
                if (endA != sa.c_str() && endB != sb.c_str())
                    return asc ? da < db : da > db;
                return asc ? sa < sb : sa > sb;
            });
    } else {
        std::stable_sort(sortedIndices_.begin(), sortedIndices_.end(),
            [&](int a, int b) {
                const std::string& sa = rows_[a].cells[col];
                const std::string& sb = rows_[b].cells[col];
                char* endA; char* endB;
                double da = strtod(sa.c_str(), &endA);
                double db = strtod(sb.c_str(), &endB);
                if (endA != sa.c_str() && endB != sb.c_str())
                    return asc ? da < db : da > db;
                return asc ? sa < sb : sa > sb;
            });
    }
}

int DataGrid::displayRow(int idx) const
{
    if (idx >= 0 && idx < static_cast<int>(sortedIndices_.size()))
        return sortedIndices_[idx];
    return idx;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Geometry helpers
// ═════════════════════════════════════════════════════════════════════════════

float DataGrid::totalColumnsWidth() const
{
    float w = contentX();
    for (auto& c : columns_) w += c.width;
    return w;
}

float DataGrid::columnX(int col) const
{
    float x = contentX();
    for (int i = 0; i < col && i < static_cast<int>(columns_.size()); ++i)
        x += columns_[i].width;
    return x;
}

int DataGrid::hitColumn(float localX) const
{
    float x = contentX() - scrollX_;
    for (int i = 0; i < static_cast<int>(columns_.size()); ++i) {
        if (localX >= x && localX < x + columns_[i].width)
            return i;
        x += columns_[i].width;
    }
    return -1;
}

int DataGrid::hitRow(float localY) const
{
    if (localY < headerHeight_) return -1;
    float y = localY - headerHeight_ + scrollY_;
    int idx = static_cast<int>(y / rowHeight_);
    if (idx < 0 || idx >= static_cast<int>(sortedIndices_.size()))
        return -1;
    return idx;
}

int DataGrid::hitResizeEdge(float localX) const
{
    float x = contentX() - scrollX_;
    for (int i = 0; i < static_cast<int>(columns_.size()); ++i) {
        x += columns_[i].width;
        if (std::abs(localX - x) <= 4.0f)
            return i;
    }
    return -1;
}

Widget::Vec2f DataGrid::sizeHint() const
{
    float w = totalColumnsWidth();
    float h = headerHeight_ + modelRowCount() * rowHeight_;
    return {w, h};
}

// ═════════════════════════════════════════════════════════════════════════════
//  Inline editing
// ═════════════════════════════════════════════════════════════════════════════

void DataGrid::setColumnReadOnly(int col, bool ro)
{
    if (col >= 0 && col < static_cast<int>(columns_.size()))
        columns_[col].readOnly = ro;
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
    markDirty();
}

void DataGrid::commitEdit()
{
    if (!editing_) return;
    editing_ = false;
    if (editRow_ >= 0 && editRow_ < modelRowCount() &&
        editCol_ >= 0 && editCol_ < static_cast<int>(columns_.size())) {
        if (model_) {
            model_->setData(model_->index(editRow_, editCol_), editBuf_);
        } else {
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
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();
    const int nRows = modelRowCount();
    const int nCols = static_cast<int>(columns_.size());

    ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, t.panelColor.a);
    ctx.fillRect(abs.x, abs.y, abs.w, abs.h);

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();

    // ── Column headers ───────────────────────────────────────────────────
    Rect headerClip = {abs.x, abs.y, abs.w, headerHeight_};
    ctx.pushClip(headerClip);
    {
        Color hdrBg(
            static_cast<uint8_t>(std::min(255, t.panelColor.r + 15)),
            static_cast<uint8_t>(std::min(255, t.panelColor.g + 15)),
            static_cast<uint8_t>(std::min(255, t.panelColor.b + 15)), 255);
        ctx.fill.SetColor(hdrBg.r, hdrBg.g, hdrBg.b, hdrBg.a);
        ctx.fillRect(abs.x, abs.y, abs.w, headerHeight_);

        float hx = abs.x + contentX() - scrollX_;
        float hdrTextY = abs.y + (headerHeight_ - t.fontSize) * 0.5f + asc;

        for (int i = 0; i < nCols; ++i) {
            auto& col = columns_[i];
            float colR = hx + col.width;

            ctx.font.SetColor(t.textColor);
            ctx.font.Print(col.name.c_str(), hx + 4, hdrTextY);

            if (sortCol_ == i && sortOrder_ != SortOrder::None) {
                float arrowX = colR - 14.0f;
                float arrowY = abs.y + headerHeight_ * 0.5f;
                ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
                if (sortOrder_ == SortOrder::Ascending)
                    ctx.fillTriangle(arrowX, arrowY + 3, arrowX + 6, arrowY + 3, arrowX + 3, arrowY - 3);
                else
                    ctx.fillTriangle(arrowX, arrowY - 3, arrowX + 6, arrowY - 3, arrowX + 3, arrowY + 3);
            }

            ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 80);
            ctx.drawLine(colR, abs.y + 2, colR, abs.y + headerHeight_ - 2);
            hx = colR;
        }

        if (showCheckboxes_) {
            ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 80);
            ctx.drawLine(abs.x + checkboxColW_, abs.y + 2, abs.x + checkboxColW_, abs.y + headerHeight_ - 2);
        }

        ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
        ctx.drawLine(abs.x, abs.y + headerHeight_, abs.x + abs.w, abs.y + headerHeight_);
    }
    ctx.popClip();

    // ── Rows ─────────────────────────────────────────────────────────────
    float bodyY = abs.y + headerHeight_;
    float bodyH = abs.h - headerHeight_;
    Rect bodyClip = {abs.x, bodyY, abs.w, bodyH};
    ctx.pushClip(bodyClip);
    {
        int totalVis = static_cast<int>(sortedIndices_.size());
        int firstVis = static_cast<int>(scrollY_ / rowHeight_);
        int lastVis  = firstVis + static_cast<int>(bodyH / rowHeight_) + 2;
        if (firstVis < 0) firstVis = 0;
        if (lastVis > totalVis) lastVis = totalVis;

        for (int vi = firstVis; vi < lastVis; ++vi) {
            int dataIdx = displayRow(vi);
            if (dataIdx < 0 || dataIdx >= nRows) continue;

            float ry = bodyY + vi * rowHeight_ - scrollY_;
            Rect rowR = {abs.x, ry, abs.w, rowHeight_};
            if (ctx.isClipped(rowR)) continue;

            // Zebra stripes
            if (zebraStripes_ && (vi % 2 == 1)) {
                ctx.fill.SetColor(
                    static_cast<uint8_t>(std::min(255, t.panelColor.r + 5)),
                    static_cast<uint8_t>(std::min(255, t.panelColor.g + 5)),
                    static_cast<uint8_t>(std::min(255, t.panelColor.b + 5)), 255);
                ctx.fillRect(abs.x, ry, abs.w, rowHeight_);
            }

            // Selection highlight
            if (isSelected(dataIdx)) {
                ctx.fill.SetColor(t.selectionColor.r, t.selectionColor.g, t.selectionColor.b, t.selectionColor.a);
                ctx.fillRect(abs.x, ry, abs.w, rowHeight_);
            }

            // Hover highlight
            if (hoveredRow_ == vi && !isSelected(dataIdx)) {
                ctx.fill.SetColor(t.buttonHover.r, t.buttonHover.g, t.buttonHover.b, 60);
                ctx.fillRect(abs.x, ry, abs.w, rowHeight_);
            }

            float textY = ry + (rowHeight_ - t.fontSize) * 0.5f + asc;

            // Checkbox
            if (showCheckboxes_) {
                float cbX = abs.x + 6.0f;
                float cbY = ry + (rowHeight_ - 14.0f) * 0.5f;
                float cbS = 14.0f;
                ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
                ctx.fillRect(cbX, cbY, cbS, cbS);
                ctx.line.SetColor(t.inputBorder.r, t.inputBorder.g, t.inputBorder.b, t.inputBorder.a);
                ctx.lineRect(cbX, cbY, cbS, cbS);
                if (modelIsChecked(dataIdx)) {
                    ctx.line.SetColor(t.checkMark.r, t.checkMark.g, t.checkMark.b, t.checkMark.a);
                    ctx.drawLine(cbX + 3, cbY + cbS * 0.5f, cbX + cbS * 0.4f, cbY + cbS - 3);
                    ctx.drawLine(cbX + cbS * 0.4f, cbY + cbS - 3, cbX + cbS - 3, cbY + 3);
                }
            }

            // Cells
            float cx = abs.x + contentX() - scrollX_;
            for (int ci = 0; ci < nCols; ++ci) {
                float colW = columns_[ci].width;

                if (editing_ && editRow_ == dataIdx && editCol_ == ci) {
                    ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, 255);
                    ctx.fillRect(cx + 1, ry + 1, colW - 2, rowHeight_ - 2);
                    ctx.line.SetColor(t.inputBorderHover.r, t.inputBorderHover.g, t.inputBorderHover.b, 255);
                    ctx.lineRect(cx + 1, ry + 1, colW - 2, rowHeight_ - 2);
                    ctx.font.SetColor(Color(255, 255, 255, 255));
                    ctx.font.Print(editBuf_.c_str(), cx + 4, textY);
                    std::string beforeCursor = editBuf_.substr(0, editCursor_);
                    float cursorX = cx + 4 + ctx.font.GetTextWidth(beforeCursor.c_str());
                    ctx.line.SetColor(255, 255, 255, 200);
                    ctx.drawLine(cursorX, ry + 3, cursorX, ry + rowHeight_ - 3);
                } else {
                    std::string cellText = modelCell(dataIdx, ci);
                    if (!cellText.empty()) {
                        ctx.font.SetColor(isSelected(dataIdx) ? Color(255, 255, 255, 255) : t.textColor);
                        ctx.font.Print(cellText.c_str(), cx + 4, textY);
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

    if (contentH > bodyH) {
        float sbW = 8.0f;
        float sbX = abs.x + abs.w - sbW;
        float trackH = bodyH;
        float thumbH = std::max(20.0f, (bodyH / contentH) * trackH);
        float maxSY = contentH - bodyH;
        float thumbY = bodyY + (scrollY_ / maxSY) * (trackH - thumbH);
        ctx.fill.SetColor(
            static_cast<uint8_t>(std::min(255, t.panelColor.r + 20)),
            static_cast<uint8_t>(std::min(255, t.panelColor.g + 20)),
            static_cast<uint8_t>(std::min(255, t.panelColor.b + 20)), 150);
        ctx.fillRect(sbX, bodyY, sbW, trackH);
        ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, 80);
        ctx.fillRect(sbX + 1, thumbY, sbW - 2, thumbH);
    }

    if (contentW > abs.w) {
        float sbH = 8.0f;
        float sbY = abs.y + abs.h - sbH;
        float trackW = abs.w;
        float thumbW = std::max(20.0f, (abs.w / contentW) * trackW);
        float maxSX = contentW - abs.w;
        float thumbX = abs.x + (scrollX_ / maxSX) * (trackW - thumbW);
        ctx.fill.SetColor(
            static_cast<uint8_t>(std::min(255, t.panelColor.r + 20)),
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
    if (!visible_ || !enabled_) return;
    Rect abs = absoluteRect();
    if (!abs.contains(e.x, e.y)) return;

    e.consumed = true;
    float localX = e.x - abs.x;
    float localY = e.y - abs.y;

    // ── Horizontal scrollbar drag ─────────────────────────────────────────
    float contentW = totalColumnsWidth();
    if (contentW > abs.w)
    {
        float sbH    = 8.0f;
        float sbY    = abs.y + abs.h - sbH;
        float trackW = abs.w;
        float thumbW = std::max(20.0f, (abs.w / contentW) * trackW);
        float maxSX  = contentW - abs.w;
        float thumbX = (maxSX > 0.f ? (scrollX_ / maxSX) * (trackW - thumbW) : 0.f);
        if (e.y >= sbY && e.y <= sbY + sbH)
        {
            // Click on thumb → start drag; click on track → jump
            if (e.x >= abs.x + thumbX && e.x <= abs.x + thumbX + thumbW)
            {
                hScrollDrag_      = true;
                hScrollDragStartX_  = e.x;
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

    if (localY < headerHeight_) {
        int resEdge = hitResizeEdge(localX);
        if (resEdge >= 0) {
            resizeCol_    = resEdge;
            resizeStartX_ = e.x;
            resizeStartW_ = columns_[resEdge].width;
            return;
        }

        int col = hitColumn(localX);
        if (col >= 0 && columns_[col].sortable) {
            if (sortCol_ == col) {
                if (sortOrder_ == SortOrder::None)              sortOrder_ = SortOrder::Ascending;
                else if (sortOrder_ == SortOrder::Ascending)   sortOrder_ = SortOrder::Descending;
                else                                            sortOrder_ = SortOrder::None;
            } else {
                sortCol_   = col;
                sortOrder_ = SortOrder::Ascending;
            }
            rebuildSortOrder();
            columnSorted.emit(col);
            markDirty();
        }
        return;
    }

    int visIdx  = hitRow(localY);
    if (visIdx < 0) return;
    int dataIdx = displayRow(visIdx);
    if (dataIdx < 0 || dataIdx >= modelRowCount()) return;

    if (showCheckboxes_ && localX < checkboxColW_) {
        if (model_) {
            bool cur = model_->data(model_->index(dataIdx, 0), ItemRole::Check) == "1";
            model_->setData(model_->index(dataIdx, 0), cur ? "0" : "1", ItemRole::Check);
        } else {
            rows_[dataIdx].checked = !rows_[dataIdx].checked;
        }
        rowCheckChanged.emit(dataIdx, modelIsChecked(dataIdx));
        markDirty();
        return;
    }

    static_cast<void>(0); // double-click per-instance
    uint32_t now = WidgetApp::instance().elapsedMs();
    int col = hitColumn(localX);

    if (dataIdx == lastClickRow_ && (now - lastClickTime_) < 400 && col >= 0) {
        startEdit(dataIdx, col);
        lastClickTime_ = 0;
        lastClickRow_  = -1;
        return;
    }
    lastClickTime_ = now;
    lastClickRow_  = dataIdx;

    if (editing_) commitEdit();

    if (multiSelect_) {
        bool ctrl  = e.ctrl;
        bool shift = e.shift;
        if (ctrl) {
            auto it = std::find(selectedRows_.begin(), selectedRows_.end(), dataIdx);
            if (it != selectedRows_.end()) {
                selectedRows_.erase(it);
                selectedSet_.erase(dataIdx);
            } else {
                selectedRows_.push_back(dataIdx);
                selectedSet_.insert(dataIdx);
            }
            selectedRow_ = dataIdx;
        } else if (shift && selectedRow_ >= 0) {
            selectedRows_.clear();
            selectedSet_.clear();
            int lo = std::min(selectedRow_, dataIdx);
            int hi = std::max(selectedRow_, dataIdx);
            for (int i = lo; i <= hi; ++i) {
                selectedRows_.push_back(i);
                selectedSet_.insert(i);
            }
        } else {
            selectedRow_  = dataIdx;
            selectedRows_ = {dataIdx};
            selectedSet_ = {dataIdx};
        }
    } else {
        selectedRow_  = dataIdx;
        selectedRows_ = {dataIdx};
        selectedSet_ = {dataIdx};
    }
    selectionChanged.emit(dataIdx);
    markDirty();
}

void DataGrid::onMouseRelease(MouseEvent& e)
{
    if (hScrollDrag_) { hScrollDrag_ = false; }
    if (resizeCol_ >= 0) resizeCol_ = -1;
    (void)e;
}

void DataGrid::onMouseMove(MouseEvent& e)
{
    if (!visible_ || !enabled_) return;
    Rect abs = absoluteRect();

    if (hScrollDrag_)
    {
        e.consumed = true;
        float contentW = totalColumnsWidth();
        float trackW   = abs.w;
        float thumbW   = std::max(20.0f, (abs.w / contentW) * trackW);
        float maxSX    = contentW - abs.w;
        float dx       = e.x - hScrollDragStartX_;
        float ratio    = maxSX > 0.f ? maxSX / (trackW - thumbW) : 0.f;
        scrollX_ = std::max(0.f, std::min(maxSX, hScrollDragStartSX_ + dx * ratio));
        markDirty();
        return;
    }

    if (resizeCol_ >= 0) {
        float dx = e.x - resizeStartX_;
        float newW = std::max(columns_[resizeCol_].minWidth, resizeStartW_ + dx);
        columns_[resizeCol_].width = newW;
        markDirty();
        return;
    }

    float localX = e.x - abs.x;
    float localY = e.y - abs.y;
    int visIdx = hitRow(localY);
    int col    = hitColumn(localX);
    if (visIdx != hoveredRow_ || col != hoveredCol_) {
        hoveredRow_ = visIdx;
        hoveredCol_ = col;
        markDirty();
    }
}

void DataGrid::onMouseScroll(MouseEvent& e)
{
    if (!visible_ || !enabled_) return;
    Rect abs = absoluteRect();
    if (!abs.contains(e.x, e.y)) return;

    e.consumed = true;
    bool shift    = e.shift;
    float bodyH   = abs.h - headerHeight_;
    float contentH = static_cast<float>(modelRowCount()) * rowHeight_;
    float contentW = totalColumnsWidth();

    if (shift) {
        scrollX_ -= e.scrollY * 40.0f;
        scrollX_ = clamp(scrollX_, 0.0f, std::max(0.0f, contentW - abs.w));
    } else {
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
    if (!focused_ && !editing_) return;

    if (editing_) {
        e.consumed = true;
        switch (e.key) {
        case BuGUI::Key::Return:
        case BuGUI::Key::KPEnter:
            commitEdit(); return;
        case BuGUI::Key::Escape:
            cancelEdit(); return;
        case BuGUI::Key::Backspace:
            if (editCursor_ > 0 && !editBuf_.empty()) {
                editBuf_.erase(editCursor_ - 1, 1);
                editCursor_--;
                markDirty();
            } return;
        case BuGUI::Key::Delete:
            if (editCursor_ < static_cast<int>(editBuf_.size())) {
                editBuf_.erase(editCursor_, 1);
                markDirty();
            } return;
        case BuGUI::Key::Left:
            if (editCursor_ > 0) { editCursor_--; markDirty(); } return;
        case BuGUI::Key::Right:
            if (editCursor_ < static_cast<int>(editBuf_.size())) { editCursor_++; markDirty(); } return;
        case BuGUI::Key::Home:
            editCursor_ = 0; markDirty(); return;
        case BuGUI::Key::End:
            editCursor_ = static_cast<int>(editBuf_.size()); markDirty(); return;
        case BuGUI::Key::Tab: {
            commitEdit();
            int nextCol = editCol_ + 1;
            int nextRow = editRow_;
            if (nextCol >= static_cast<int>(columns_.size())) { nextCol = 0; nextRow++; }
            if (nextRow < modelRowCount()) startEdit(nextRow, nextCol);
            return;
        }
        case BuGUI::Key::C:
            if (e.ctrl) { WidgetApp::instance().setClipboardText(editBuf_.c_str()); return; }
            break;
        case BuGUI::Key::V:
            if (e.ctrl) {
                std::string clip = WidgetApp::instance().getClipboardText();
                if (!clip.empty()) {
                    editBuf_.insert(editCursor_, clip);
                    editCursor_ += static_cast<int>(clip.size());
                    markDirty();
                }
                return;
            } break;
        default: break;
        }
    }

    if (!editing_ && e.ctrl) {
        if (e.key == BuGUI::Key::C) {
            e.consumed = true;
            std::string text;
            int nc = static_cast<int>(columns_.size());
            for (int c = 0; c < nc; ++c) {
                if (c > 0) text += '\t';
                text += columns_[c].name;
            }
            text += '\n';
            for (int ri : selectedRows_) {
                if (ri < 0 || ri >= modelRowCount()) continue;
                for (int c = 0; c < nc; ++c) {
                    if (c > 0) text += '\t';
                    text += modelCell(ri, c);
                }
                text += '\n';
            }
            if (!text.empty()) WidgetApp::instance().setClipboardText(text.c_str());
            return;
        }
        if (e.key == BuGUI::Key::A) {
            e.consumed = true;
            selectedRows_.clear();
            selectedSet_.clear();
            int nr = modelRowCount();
            for (int i = 0; i < nr; ++i) {
                selectedRows_.push_back(i);
                selectedSet_.insert(i);
            }
            if (nr > 0) selectedRow_ = 0;
            markDirty();
            return;
        }
    }

    if (!editing_) {
        if (e.key == BuGUI::Key::Up) {
            e.consumed = true;
            if (selectedRow_ > 0) { setSelectedRow(selectedRow_ - 1); selectionChanged.emit(selectedRow_); }
        } else if (e.key == BuGUI::Key::Down) {
            e.consumed = true;
            if (selectedRow_ < modelRowCount() - 1) {
                setSelectedRow(selectedRow_ + 1); selectionChanged.emit(selectedRow_);
            }
        } else if (e.key == BuGUI::Key::Return || e.key == BuGUI::Key::KPEnter) {
            e.consumed = true;
            if (selectedRow_ >= 0) startEdit(selectedRow_, 0);
        } else if (e.key == BuGUI::Key::Space && showCheckboxes_) {
            e.consumed = true;
            if (selectedRow_ >= 0 && selectedRow_ < modelRowCount()) {
                if (model_) {
                    bool cur = model_->data(model_->index(selectedRow_, 0), ItemRole::Check) == "1";
                    model_->setData(model_->index(selectedRow_, 0), cur ? "0" : "1", ItemRole::Check);
                } else {
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

void DataGrid::setModel(AbstractItemModel* model)
{
    model_ = model;
    if (!model_) return;

    // Sync columns from model headers if we have no columns yet
    if (columns_.empty()) {
        int nc = model_->columnCount();
        for (int i = 0; i < nc; ++i) {
            Column c;
            c.name = model_->headerData(i);
            c.width = 120.0f;
            c.sortable = true;
            columns_.push_back(std::move(c));
        }
    }

    // Connect model signals to refresh the view
    model_->modelReset.connect([this] {
        selectedRow_ = -1; selectedRows_.clear(); selectedSet_.clear();
        scrollY_ = 0; rebuildSortOrder(); markDirty();
    });
    model_->rowsInserted.connect([this] { rebuildSortOrder(); markDirty(); });
    model_->rowsRemoved.connect([this]  { rebuildSortOrder(); markDirty(); });
    model_->dataChanged.connect([this](ModelIndex) { markDirty(); });
    model_->layoutChanged.connect([this] { markDirty(); });

    rebuildSortOrder();
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

std::string DataGrid::modelCell(int row, int col) const
{
    if (model_) return model_->data(model_->index(row, col));
    if (row >= 0 && row < static_cast<int>(rows_.size()) &&
        col >= 0 && col < static_cast<int>(rows_[row].cells.size()))
        return rows_[row].cells[col];
    return {};
}

std::string DataGrid::modelHeader(int col) const
{
    if (model_) return model_->headerData(col);
    if (col >= 0 && col < static_cast<int>(columns_.size()))
        return columns_[col].name;
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

TreeGrid::Node* TreeGrid::Node::addChild(const std::vector<std::string>& cellData)
{
    auto* child = new Node();
    child->cells  = cellData;
    child->parent = this;
    child->owner_ = owner_;
    child->depth  = depth + 1;
    children.push_back(child);
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
    for (auto* r : roots_) deleteNodes(r);
}

void TreeGrid::deleteNodes(Node* n)
{
    for (auto* c : n->children) deleteNodes(c);
    delete n;
}

void TreeGrid::clearAll()
{
    for (auto* r : roots_) deleteNodes(r);
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
    std::vector<const Node*> stack;
    for (auto* r : roots_) stack.push_back(r);
    while (!stack.empty()) {
        auto* n = stack.back(); stack.pop_back();
        count++;
        for (auto* c : n->children) stack.push_back(c);
    }
    return count;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Columns
// ═════════════════════════════════════════════════════════════════════════════

int TreeGrid::addColumn(const std::string& name, float width, bool sortable)
{
    Column c;
    c.name     = name;
    c.width    = width;
    c.sortable = sortable;
    columns_.push_back(std::move(c));
    markDirty();
    return static_cast<int>(columns_.size()) - 1;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Roots
// ═════════════════════════════════════════════════════════════════════════════

TreeGrid::Node* TreeGrid::addRoot(const std::vector<std::string>& cells)
{
    auto* n = new Node();
    n->cells  = cells;
    n->depth  = 0;
    n->owner_ = this;
    roots_.push_back(n);
    rebuildFlatList();
    markDirty();
    return n;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Flat list
// ═════════════════════════════════════════════════════════════════════════════

void TreeGrid::rebuildFlatList()
{
    flatList_.clear();
    for (auto* r : roots_)
        flattenNode(r, 0);
}

void TreeGrid::flattenNode(Node* n, int depth)
{
    flatList_.push_back({n, depth});
    if (n->expanded) {
        for (auto* c : n->children)
            flattenNode(c, depth + 1);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Sort
// ═════════════════════════════════════════════════════════════════════════════

void TreeGrid::sortChildren(Node* parent)
{
    if (sortCol_ < 0 || sortOrder_ == DataGrid::SortOrder::None) return;
    int col  = sortCol_;
    bool asc = (sortOrder_ == DataGrid::SortOrder::Ascending);

    auto cmp = [col, asc](Node* a, Node* b) {
        const std::string& sa = (col < static_cast<int>(a->cells.size())) ? a->cells[col] : "";
        const std::string& sb = (col < static_cast<int>(b->cells.size())) ? b->cells[col] : "";
        char* endA; char* endB;
        double da = strtod(sa.c_str(), &endA);
        double db = strtod(sb.c_str(), &endB);
        if (endA != sa.c_str() && endB != sb.c_str())
            return asc ? da < db : da > db;
        return asc ? sa < sb : sa > sb;
    };

    if (parent) {
        std::stable_sort(parent->children.begin(), parent->children.end(), cmp);
        for (auto* c : parent->children) sortChildren(c);
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
    for (auto* s : selectedNodes_)
        if (s == n) return true;
    return false;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Geometry helpers
// ═════════════════════════════════════════════════════════════════════════════

float TreeGrid::totalColumnsWidth() const
{
    float w = 0;
    for (auto& c : columns_) w += c.width;
    return w;
}

float TreeGrid::columnX(int col) const
{
    float x = 0;
    for (int i = 0; i < col && i < static_cast<int>(columns_.size()); ++i)
        x += columns_[i].width;
    return x;
}

int TreeGrid::hitColumn(float localX) const
{
    float x = -scrollX_;
    for (int i = 0; i < static_cast<int>(columns_.size()); ++i) {
        if (localX >= x && localX < x + columns_[i].width)
            return i;
        x += columns_[i].width;
    }
    return -1;
}

int TreeGrid::hitRow(float localY) const
{
    if (localY < headerHeight_) return -1;
    float y  = localY - headerHeight_ + scrollY_;
    int   idx = static_cast<int>(y / rowHeight_);
    if (idx < 0 || idx >= static_cast<int>(flatList_.size()))
        return -1;
    return idx;
}

int TreeGrid::hitResizeEdge(float localX) const
{
    float x = -scrollX_;
    for (int i = 0; i < static_cast<int>(columns_.size()); ++i) {
        x += columns_[i].width;
        if (std::abs(localX - x) <= 4.0f) return i;
    }
    return -1;
}

Widget::Vec2f TreeGrid::sizeHint() const
{
    return {totalColumnsWidth(), headerHeight_ + flatList_.size() * rowHeight_};
}

// ═════════════════════════════════════════════════════════════════════════════
//  Read-only / inline edit
// ═════════════════════════════════════════════════════════════════════════════

void TreeGrid::setColumnReadOnly(int col, bool ro)
{
    if (col >= 0 && col < static_cast<int>(columns_.size()))
        columns_[col].readOnly = ro;
}

void TreeGrid::startEdit(Node* node, int col)
{
    if (!node || col < 0 || col >= static_cast<int>(columns_.size())) return;
    if (readOnly_ || columns_[col].readOnly || node->readOnly) return;
    editing_    = true;
    editNode_   = node;
    editCol_    = col;
    editBuf_    = (col < static_cast<int>(node->cells.size())) ? node->cells[col] : "";
    editCursor_ = static_cast<int>(editBuf_.size());
    markDirty();
}

void TreeGrid::commitEdit()
{
    if (!editing_ || !editNode_) return;
    editing_ = false;
    if (editCol_ >= 0 && editCol_ < static_cast<int>(editNode_->cells.size())) {
        editNode_->cells[editCol_] = editBuf_;
        cellEdited.emit(editNode_, editCol_);
    }
    editNode_ = nullptr;
    editCol_  = -1;
    markDirty();
}

void TreeGrid::cancelEdit()
{
    editing_  = false;
    editNode_ = nullptr;
    editCol_  = -1;
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Paint
// ═════════════════════════════════════════════════════════════════════════════

void TreeGrid::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, t.panelColor.a);
    ctx.fillRect(abs.x, abs.y, abs.w, abs.h);

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();

    // ── Column headers ───────────────────────────────────────────────────
    Rect headerClip = {abs.x, abs.y, abs.w, headerHeight_};
    ctx.pushClip(headerClip);
    {
        Color hdrBg(
            static_cast<uint8_t>(std::min(255, t.panelColor.r + 15)),
            static_cast<uint8_t>(std::min(255, t.panelColor.g + 15)),
            static_cast<uint8_t>(std::min(255, t.panelColor.b + 15)), 255);
        ctx.fill.SetColor(hdrBg.r, hdrBg.g, hdrBg.b, hdrBg.a);
        ctx.fillRect(abs.x, abs.y, abs.w, headerHeight_);

        float hx       = abs.x - scrollX_;
        float hdrTextY = abs.y + (headerHeight_ - t.fontSize) * 0.5f + asc;

        for (int i = 0; i < static_cast<int>(columns_.size()); ++i) {
            auto& col = columns_[i];
            float colR = hx + col.width;

            ctx.font.SetColor(t.textColor);
            ctx.font.Print(col.name.c_str(), hx + 4, hdrTextY);

            if (sortCol_ == i && sortOrder_ != DataGrid::SortOrder::None) {
                float arrowX = colR - 14.0f;
                float arrowY = abs.y + headerHeight_ * 0.5f;
                ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
                if (sortOrder_ == DataGrid::SortOrder::Ascending)
                    ctx.fillTriangle(arrowX, arrowY + 3, arrowX + 6, arrowY + 3, arrowX + 3, arrowY - 3);
                else
                    ctx.fillTriangle(arrowX, arrowY - 3, arrowX + 6, arrowY - 3, arrowX + 3, arrowY + 3);
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
    Rect bodyClip = {abs.x, bodyY, abs.w, bodyH};
    ctx.pushClip(bodyClip);
    {
        int totalVis = static_cast<int>(flatList_.size());
        int firstVis = static_cast<int>(scrollY_ / rowHeight_);
        int lastVis  = firstVis + static_cast<int>(bodyH / rowHeight_) + 2;
        if (firstVis < 0) firstVis = 0;
        if (lastVis > totalVis) lastVis = totalVis;

        for (int vi = firstVis; vi < lastVis; ++vi) {
            auto& entry = flatList_[vi];
            Node* node  = entry.node;
            int   depth = entry.depth;

            float ry  = bodyY + vi * rowHeight_ - scrollY_;
            Rect rowR = {abs.x, ry, abs.w, rowHeight_};
            if (ctx.isClipped(rowR)) continue;

            if (zebraStripes_ && (vi % 2 == 1)) {
                ctx.fill.SetColor(
                    static_cast<uint8_t>(std::min(255, t.panelColor.r + 5)),
                    static_cast<uint8_t>(std::min(255, t.panelColor.g + 5)),
                    static_cast<uint8_t>(std::min(255, t.panelColor.b + 5)), 255);
                ctx.fillRect(abs.x, ry, abs.w, rowHeight_);
            }

            if (isSelected(node)) {
                ctx.fill.SetColor(t.selectionColor.r, t.selectionColor.g, t.selectionColor.b, t.selectionColor.a);
                ctx.fillRect(abs.x, ry, abs.w, rowHeight_);
            }

            if (hoveredRow_ == vi && !isSelected(node)) {
                ctx.fill.SetColor(t.buttonHover.r, t.buttonHover.g, t.buttonHover.b, 60);
                ctx.fillRect(abs.x, ry, abs.w, rowHeight_);
            }

            float textY = ry + (rowHeight_ - t.fontSize) * 0.5f + asc;

            float cx = abs.x - scrollX_;
            for (int ci = 0; ci < static_cast<int>(columns_.size()); ++ci) {
                float colW = columns_[ci].width;

                if (ci == 0) {
                    float indent = depth * indentWidth_;
                    float arrowX = cx + indent + 2;
                    float arrowCY = ry + rowHeight_ * 0.5f;

                    if (!node->children.empty()) {
                        ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, 180);
                        if (node->expanded)
                            ctx.fillTriangle(arrowX, arrowCY - 4, arrowX + 8, arrowCY - 4, arrowX + 4, arrowCY + 4);
                        else
                            ctx.fillTriangle(arrowX, arrowCY - 4, arrowX + 8, arrowCY, arrowX, arrowCY + 4);
                    }

                    float textX = cx + indent + 14;

                    if (node->iconId != IconId::None && ctx.icons) {
                        float iconS = rowHeight_ - 6;
                        ctx.drawIcon(node->iconId, textX, ry + 3, iconS);
                        textX += iconS + 2;
                    }

                    if (editing_ && editNode_ == node && editCol_ == 0) {
                        ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, 255);
                        ctx.fillRect(textX - 1, ry + 1, colW - indent - 16, rowHeight_ - 2);
                        ctx.line.SetColor(t.inputBorderHover.r, t.inputBorderHover.g, t.inputBorderHover.b, 255);
                        ctx.lineRect(textX - 1, ry + 1, colW - indent - 16, rowHeight_ - 2);
                        ctx.font.SetColor(Color(255, 255, 255, 255));
                        ctx.font.Print(editBuf_.c_str(), textX + 2, textY);
                        std::string bc = editBuf_.substr(0, editCursor_);
                        float curX = textX + 2 + ctx.font.GetTextWidth(bc.c_str());
                        ctx.line.SetColor(255, 255, 255, 200);
                        ctx.drawLine(curX, ry + 3, curX, ry + rowHeight_ - 3);
                    } else {
                        if (!node->cells.empty()) {
                            ctx.font.SetColor(isSelected(node) ? Color(255, 255, 255, 255) : t.textColor);
                            ctx.font.Print(node->cells[0].c_str(), textX + 2, textY);
                        }
                    }
                } else {
                    if (editing_ && editNode_ == node && editCol_ == ci) {
                        ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, 255);
                        ctx.fillRect(cx + 1, ry + 1, colW - 2, rowHeight_ - 2);
                        ctx.line.SetColor(t.inputBorderHover.r, t.inputBorderHover.g, t.inputBorderHover.b, 255);
                        ctx.lineRect(cx + 1, ry + 1, colW - 2, rowHeight_ - 2);
                        ctx.font.SetColor(Color(255, 255, 255, 255));
                        ctx.font.Print(editBuf_.c_str(), cx + 4, textY);
                        std::string bc = editBuf_.substr(0, editCursor_);
                        float curX = cx + 4 + ctx.font.GetTextWidth(bc.c_str());
                        ctx.line.SetColor(255, 255, 255, 200);
                        ctx.drawLine(curX, ry + 3, curX, ry + rowHeight_ - 3);
                    } else {
                        if (ci < static_cast<int>(node->cells.size())) {
                            ctx.font.SetColor(isSelected(node) ? Color(255, 255, 255, 255) : Color(170, 170, 175, 255));
                            ctx.font.Print(node->cells[ci].c_str(), cx + 4, textY);
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

    if (contentH > bodyH) {
        float sbW  = 8.0f;
        float sbX  = abs.x + abs.w - sbW;
        float trackH = bodyH;
        float thumbH = std::max(20.0f, (bodyH / contentH) * trackH);
        float maxSY  = contentH - bodyH;
        float thumbY = bodyY + (maxSY > 0 ? (scrollY_ / maxSY) * (trackH - thumbH) : 0);
        ctx.fill.SetColor(
            static_cast<uint8_t>(std::min(255, t.panelColor.r + 20)),
            static_cast<uint8_t>(std::min(255, t.panelColor.g + 20)),
            static_cast<uint8_t>(std::min(255, t.panelColor.b + 20)), 150);
        ctx.fillRect(sbX, bodyY, sbW, trackH);
        ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, 80);
        ctx.fillRect(sbX + 1, thumbY, sbW - 2, thumbH);
    }

    if (contentW > abs.w) {
        float sbH  = 8.0f;
        float sbY  = abs.y + abs.h - sbH;
        float trackW = abs.w;
        float thumbW = std::max(20.0f, (abs.w / contentW) * trackW);
        float maxSX  = contentW - abs.w;
        float thumbX = abs.x + (maxSX > 0 ? (scrollX_ / maxSX) * (trackW - thumbW) : 0);
        ctx.fill.SetColor(
            static_cast<uint8_t>(std::min(255, t.panelColor.r + 20)),
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
    if (!visible_ || !enabled_) return;
    Rect abs = absoluteRect();
    if (!abs.contains(e.x, e.y)) return;

    e.consumed = true;
    float localX = e.x - abs.x;
    float localY = e.y - abs.y;

    if (localY < headerHeight_) {
        int resEdge = hitResizeEdge(localX);
        if (resEdge >= 0) {
            resizeCol_    = resEdge;
            resizeStartX_ = e.x;
            resizeStartW_ = columns_[resEdge].width;
            return;
        }

        int col = hitColumn(localX);
        if (col >= 0 && columns_[col].sortable) {
            if (sortCol_ == col) {
                if (sortOrder_ == DataGrid::SortOrder::None)            sortOrder_ = DataGrid::SortOrder::Ascending;
                else if (sortOrder_ == DataGrid::SortOrder::Ascending) sortOrder_ = DataGrid::SortOrder::Descending;
                else                                                    sortOrder_ = DataGrid::SortOrder::None;
            } else {
                sortCol_   = col;
                sortOrder_ = DataGrid::SortOrder::Ascending;
            }
            for (auto* r : roots_) sortChildren(r);
            rebuildFlatList();
            columnSorted.emit(col);
            markDirty();
        }
        return;
    }

    int visIdx = hitRow(localY);
    if (visIdx < 0 || visIdx >= static_cast<int>(flatList_.size())) return;
    Node* node = flatList_[visIdx].node;
    int   dep  = flatList_[visIdx].depth;

    float arrowAreaEnd = dep * indentWidth_ + 14;
    int col = hitColumn(localX);
    if (col == 0 && (localX + scrollX_) < arrowAreaEnd && !node->children.empty()) {
        node->expanded = !node->expanded;
        if (node->expanded) nodeExpanded.emit(node);
        else                nodeCollapsed.emit(node);
        rebuildFlatList();
        markDirty();
        return;
    }

    static uint32_t lastClickTime = 0;
    static Node*  lastClickNode = nullptr;
    uint32_t now = WidgetApp::instance().elapsedMs();

    if (node == lastClickNode && (now - lastClickTime) < 400 && col >= 0) {
        startEdit(node, col);
        lastClickTime = 0;
        lastClickNode = nullptr;
        return;
    }
    lastClickTime = now;
    lastClickNode = node;

    if (editing_) commitEdit();

    if (multiSelect_) {
        bool ctrl = e.ctrl;
        if (ctrl) {
            auto it = std::find(selectedNodes_.begin(), selectedNodes_.end(), node);
            if (it != selectedNodes_.end())
                selectedNodes_.erase(it);
            else
                selectedNodes_.push_back(node);
            selectedNode_ = node;
        } else {
            selectedNode_  = node;
            selectedNodes_ = {node};
        }
    } else {
        selectedNode_  = node;
        selectedNodes_ = {node};
    }
    selectionChanged.emit(node);
    markDirty();
}

void TreeGrid::onMouseRelease(MouseEvent& e)
{
    if (resizeCol_ >= 0) resizeCol_ = -1;
    (void)e;
}

void TreeGrid::onMouseMove(MouseEvent& e)
{
    if (!visible_ || !enabled_) return;
    Rect abs = absoluteRect();

    if (resizeCol_ >= 0) {
        float dx = e.x - resizeStartX_;
        columns_[resizeCol_].width = std::max(columns_[resizeCol_].minWidth, resizeStartW_ + dx);
        markDirty();
        return;
    }

    float localY = e.y - abs.y;
    int visIdx = hitRow(localY);
    if (visIdx != hoveredRow_) {
        hoveredRow_ = visIdx;
        markDirty();
    }
}

void TreeGrid::onMouseScroll(MouseEvent& e)
{
    if (!visible_ || !enabled_) return;
    Rect abs = absoluteRect();
    if (!abs.contains(e.x, e.y)) return;

    e.consumed = true;
    bool shift    = e.shift;
    float bodyH   = abs.h - headerHeight_;
    float contentH = static_cast<float>(flatList_.size()) * rowHeight_;
    float contentW = totalColumnsWidth();

    if (shift) {
        scrollX_ -= e.scrollY * 40.0f;
        scrollX_ = clamp(scrollX_, 0.0f, std::max(0.0f, contentW - abs.w));
    } else {
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
    if (!focused_ && !editing_) return;

    if (editing_) {
        e.consumed = true;
        switch (e.key) {
        case BuGUI::Key::Return: case BuGUI::Key::KPEnter:
            commitEdit(); return;
        case BuGUI::Key::Escape:
            cancelEdit(); return;
        case BuGUI::Key::Backspace:
            if (editCursor_ > 0 && !editBuf_.empty()) {
                editBuf_.erase(editCursor_ - 1, 1);
                editCursor_--; markDirty();
            } return;
        case BuGUI::Key::Delete:
            if (editCursor_ < static_cast<int>(editBuf_.size())) {
                editBuf_.erase(editCursor_, 1); markDirty();
            } return;
        case BuGUI::Key::Left:
            if (editCursor_ > 0) { editCursor_--; markDirty(); } return;
        case BuGUI::Key::Right:
            if (editCursor_ < static_cast<int>(editBuf_.size())) { editCursor_++; markDirty(); } return;
        case BuGUI::Key::Home: editCursor_ = 0; markDirty(); return;
        case BuGUI::Key::End:  editCursor_ = static_cast<int>(editBuf_.size()); markDirty(); return;
        case BuGUI::Key::C:
            if (e.ctrl) { WidgetApp::instance().setClipboardText(editBuf_.c_str()); return; }
            break;
        case BuGUI::Key::V:
            if (e.ctrl) {
                std::string clip = WidgetApp::instance().getClipboardText();
                if (!clip.empty()) { editBuf_.insert(editCursor_, clip); editCursor_ += static_cast<int>(clip.size()); markDirty(); }
                return;
            } break;
        default: break;
        }
    }

    if (!editing_) {
        int curIdx = -1;
        for (int i = 0; i < static_cast<int>(flatList_.size()); ++i)
            if (flatList_[i].node == selectedNode_) { curIdx = i; break; }

        if (e.key == BuGUI::Key::Up && curIdx > 0) {
            e.consumed = true;
            selectedNode_  = flatList_[curIdx - 1].node;
            selectedNodes_ = {selectedNode_};
            selectionChanged.emit(selectedNode_);
            markDirty();
        } else if (e.key == BuGUI::Key::Down && curIdx < static_cast<int>(flatList_.size()) - 1) {
            e.consumed = true;
            selectedNode_  = flatList_[curIdx + 1].node;
            selectedNodes_ = {selectedNode_};
            selectionChanged.emit(selectedNode_);
            markDirty();
        } else if (e.key == BuGUI::Key::Left && selectedNode_) {
            e.consumed = true;
            if (selectedNode_->expanded && !selectedNode_->children.empty()) {
                selectedNode_->expanded = false;
                nodeCollapsed.emit(selectedNode_);
                rebuildFlatList();
            } else if (selectedNode_->parent) {
                selectedNode_  = selectedNode_->parent;
                selectedNodes_ = {selectedNode_};
                selectionChanged.emit(selectedNode_);
            }
            markDirty();
        } else if (e.key == BuGUI::Key::Right && selectedNode_) {
            e.consumed = true;
            if (!selectedNode_->expanded && !selectedNode_->children.empty()) {
                selectedNode_->expanded = true;
                nodeExpanded.emit(selectedNode_);
                rebuildFlatList();
            } else if (!selectedNode_->children.empty()) {
                selectedNode_  = selectedNode_->children[0];
                selectedNodes_ = {selectedNode_};
                selectionChanged.emit(selectedNode_);
            }
            markDirty();
        } else if (e.key == BuGUI::Key::Return && selectedNode_) {
            e.consumed = true;
            startEdit(selectedNode_, 0);
        } else if (e.key == BuGUI::Key::C && e.ctrl && selectedNode_) {
            e.consumed = true;
            std::string text;
            for (int c = 0; c < static_cast<int>(selectedNode_->cells.size()); ++c) {
                if (c > 0) text += '\t';
                text += selectedNode_->cells[c];
            }
            WidgetApp::instance().setClipboardText(text.c_str());
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
