#include "ThumbnailGrid.hpp"
#include "ScrollWidgets.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <cmath>

namespace {
    inline float setupFont(PaintContext& ctx, const Color& color, float size)
    {
        ctx.font.SetFontSize(size);
        ctx.font.SetBatch(&ctx.text);
        ctx.font.SetColor(color);
        return ctx.font.GetAscender();
    }

    // First index in [lo, hi) where pred holds, for a pred that stays true once true.
    template<typename Pred>
    int firstWhere(int lo, int hi, Pred pred)
    {
        while (lo < hi)
        {
            const int mid = lo + (hi - lo) / 2;
            if (pred(mid))
            {
                hi = mid;
            }
            else
            {
                lo = mid + 1;
            }
        }
        return lo;
    }
}

ThumbnailGrid::ThumbnailGrid()
{
    vbar_ = createChild<ScrollBar>(ScrollBarOrientation::Vertical);
    vbar_->setScrollable(false);
    vbar_->scrolled.connect(
            [this](float v)
            {
                scrollY_ = v;
                markDirty();
            });
    syncScroll();
}

ThumbnailGrid::Metrics ThumbnailGrid::metrics() const
{
    Metrics m;
    m.labelH = showLabels_ ? 16.f : 0.f;
    m.cellW = thumbW_ + spacing_;
    m.cellH = thumbH_ + m.labelH + spacing_;
    const int count = static_cast<int>(items_.size());
    auto layOut = [&](float viewW)
    {
        m.viewW = viewW;
        m.cols = std::max(1, static_cast<int>((viewW - spacing_) / m.cellW));
        const int rows = (count + m.cols - 1) / m.cols;
        m.contentH = rows == 0 ? 0.f : spacing_ + rows * m.cellH;
    };
    layOut(rect_.w);
    m.bar = ScrollBar::neededFor(m.contentH, rect_.h);
    if (m.bar) layOut(rect_.w - (ScrollBar::kBarThickness + 2.f));
    m.scroll = std::max(0.f, std::min(scrollY_, m.contentH - rect_.h));
    return m;
}

void ThumbnailGrid::syncScroll()
{
    const Metrics m = metrics();
    scrollY_ = m.scroll;
    vbar_->setVisible(m.bar);
    vbar_->setRect({ m.viewW + 2.f, 0.f, ScrollBar::kBarThickness, rect_.h });
    vbar_->setContentSize(m.contentH);
    vbar_->setViewSize(rect_.h);
    vbar_->setValue(scrollY_);
    markDirty();
}

void ThumbnailGrid::setScrollY(float y)
{
    scrollY_ = y;
    syncScroll();
}

void ThumbnailGrid::layout()
{
    syncScroll();
    Widget::layout();
}

int ThumbnailGrid::addItem(const String& label, const Color& color)
{
    items_.push_back({label, color, {}, false});
    syncScroll();
    return static_cast<int>(items_.size()) - 1;
}

void ThumbnailGrid::setItemTexture(int idx, ig::retained::TextureHandle tex)
{
    if (idx < 0 || idx >= static_cast<int>(items_.size())) return;
    items_[idx].tex = tex;
    markDirty();
}

void ThumbnailGrid::removeItem(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(items_.size())) return;
    items_.erase(items_.begin() + idx);
    if (selected_ == idx) selected_ = -1;
    else if (selected_ > idx) --selected_;
    syncScroll();
}

void ThumbnailGrid::clearItems()
{
    items_.clear();
    selected_ = -1;
    syncScroll();
}

void ThumbnailGrid::setSelected(int idx)
{
    if (selected_ >= 0 && selected_ < static_cast<int>(items_.size()))
        items_[selected_].selected = false;
    selected_ = idx;
    if (idx >= 0 && idx < static_cast<int>(items_.size()))
        items_[idx].selected = true;
    markDirty();
}

int ThumbnailGrid::hitTest(float mx, float my) const
{
    const Rect b = absoluteRect();
    const Metrics m = metrics();
    const float labelH = m.labelH;
    const float cellW = m.cellW;
    const float cellH = m.cellH;
    const int cols = m.cols;
    const float padX   = spacing_;
    const float padY   = spacing_;

    const int count = static_cast<int>(items_.size());
    auto colX = [&](int col) { return b.x + padX + col * cellW; };
    auto rowY = [&](int row) { return b.y + padY + row * cellH - m.scroll; };

    if (cellW < 0.f || cellH < 0.f)
    {
        for (int i = 0; i < count; ++i)
        {
            float x = colX(i % cols);
            float y = rowY(i / cols);
            if (mx >= x && mx <= x + thumbW_ && my >= y && my <= y + thumbH_ + labelH) return i;
        }
        return -1;
    }

    // Cell edges grow with the index, so the cells under the mouse are one run
    // of columns and one of rows; the first item of both runs is the hit.
    const int rows = (count + cols - 1) / cols;
    const int col = firstWhere(0, cols, [&](int c) { return mx <= colX(c) + thumbW_; });
    const int row = firstWhere(0, rows, [&](int r) { return my <= rowY(r) + thumbH_ + labelH; });
    if (col == cols || row == rows || !(mx >= colX(col)) || !(my >= rowY(row))) return -1;
    const int i = row * cols + col;
    return i < count ? i : -1;
}

void ThumbnailGrid::paint(PaintContext& ctx)
{
    if (!visible_) return;
    const Rect b = absoluteRect();
    ctx.pushClip(b);

    // Background
    ctx.fill.SetColor(bgColor_.r, bgColor_.g, bgColor_.b, bgColor_.a);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    const Metrics m = metrics();
    const float labelH = m.labelH;
    const float cellW = m.cellW;
    const float cellH = m.cellH;
    const int cols = m.cols;
    const float padX   = spacing_;
    const float padY   = spacing_;
    if (m.bar) ctx.pushClip({ b.x, b.y, m.viewW, b.h });

    float asc = 0;
    if (showLabels_)
        asc = setupFont(ctx, Color(200, 200, 205, 255), 9.f);

    const int count = static_cast<int>(items_.size());
    auto rowY = [&](int row) { return b.y + padY + row * cellH - m.scroll; };
    int firstRow = 0, endRow = (count + cols - 1) / cols;
    if (cellH >= 0.f)
    {
        firstRow = firstWhere(0, endRow, [&](int r) { return !(rowY(r) + cellH < b.y); });
        endRow = firstWhere(firstRow, endRow, [&](int r) { return rowY(r) > b.y + b.h; });
    }

    for (int i = firstRow * cols; i < count && i < endRow * cols; ++i)
    {
        const auto& item = items_[i];
        int col = i % cols;
        int row = i / cols;
        float x = b.x + padX + col * cellW;
        float y = rowY(row);

        // Skip if off-screen
        if (y + cellH < b.y || y > b.y + b.h) continue;

        // Selection highlight
        if (item.selected) {
            ctx.fill.SetColor(255, 200, 60, 40);
            ctx.fillRect(x - 2, y - 2, thumbW_ + 4, thumbH_ + labelH + 4);
        }

        // Thumbnail
        if (item.tex) {
            Rect dst = {x, y, thumbW_, thumbH_};
            Rect uv  = {0.f, 0.f, 1.f, 1.f};
            ctx.drawImage(item.tex, dst, uv);
        } else {
            ctx.fill.SetColor(item.color.r, item.color.g, item.color.b, item.color.a);
            ctx.fillRect(x, y, thumbW_, thumbH_);

            // Subtle gradient at bottom of thumb (darkening)
            float gradH = thumbH_ * 0.3f;
            for (int g = 0; g < static_cast<int>(gradH); ++g) {
                float t2 = float(g) / gradH;
                uint8_t a = static_cast<uint8_t>(t2 * 80.f);
                ctx.fill.SetColor(0, 0, 0, a);
                ctx.fillRect(x, y + thumbH_ - gradH + g, thumbW_, 1);
            }
        }

        // Border
        Color brd = item.selected ? Color(255, 200, 60, 255) : Color(50, 52, 58, 255);
        ctx.fill.SetColor(brd.r, brd.g, brd.b, brd.a);
        ctx.fillRect(x,             y,             thumbW_, 1);
        ctx.fillRect(x,             y + thumbH_-1, thumbW_, 1);
        ctx.fillRect(x,             y,             1,       thumbH_);
        ctx.fillRect(x + thumbW_-1, y,             1,       thumbH_);

        // Label (truncated to thumb width)
        if (showLabels_ && !item.label.empty()) {
            ctx.font.SetColor(item.selected ? Color(255, 220, 80, 255) : Color(180, 182, 190, 255));
            // Simple truncation: measure and clip
            ctx.font.Print(item.label.c_str(), x, y + thumbH_ + (labelH - 9.f) * 0.5f + asc);
        }
    }

    if (m.bar) ctx.popClip();
    ctx.popClip();
    Widget::paint(ctx);
}

void ThumbnailGrid::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;
    int idx = hitTest(e.x, e.y);
    if (idx >= 0) {
        setSelected(idx);
        onItemClicked.emit(idx);
        e.consumed = true;
    }
}

void ThumbnailGrid::onMouseScroll(MouseEvent& e)
{
    const Metrics m = metrics();
    if (!m.bar) return;
    e.consumed = true;
    setScrollY(m.scroll - e.scrollY * m.cellH);
}
