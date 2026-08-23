#include "pch.hpp"
#include "ScrollWidgets.hpp"
#include "Theme.hpp"

#include <algorithm>
#include <cmath>

// ═════════════════════════════════════════════════════════════════════════════
//  ScrollBar
// ═════════════════════════════════════════════════════════════════════════════

ScrollBar::ScrollBar(ScrollBarOrientation orientation)
    : orientation_(orientation)
{}

Widget::Vec2f ScrollBar::sizeHint() const
{
    if (orientation_ == ScrollBarOrientation::Vertical)
        return {kBarThickness, 60.0f};
    return {60.0f, kBarThickness};
}

void ScrollBar::clampScroll()
{
    scrollValue_ = std::max(0.0f, std::min(scrollValue_, maxValue()));
}

Rect ScrollBar::thumbRect() const
{
    Rect abs = absoluteRect();
    float maxV = maxValue();
    if (maxV <= 0.0f) return abs;

    if (orientation_ == ScrollBarOrientation::Vertical)
    {
        float trackLen = abs.h;
        float thumbLen = std::max(kMinThumbLen, trackLen * viewSize_ / contentSize_);
        thumbLen = std::min(thumbLen, trackLen);
        float travel  = trackLen - thumbLen;
        float ty      = (maxV > 0.0f) ? (scrollValue_ / maxV) * travel : 0.0f;
        return {abs.x, abs.y + ty, abs.w, thumbLen};
    }
    else
    {
        float trackLen = abs.w;
        float thumbLen = std::max(kMinThumbLen, trackLen * viewSize_ / contentSize_);
        thumbLen = std::min(thumbLen, trackLen);
        float travel  = trackLen - thumbLen;
        float tx      = (maxV > 0.0f) ? (scrollValue_ / maxV) * travel : 0.0f;
        return {abs.x + tx, abs.y, thumbLen, abs.h};
    }
}

void ScrollBar::paint(PaintContext& ctx)
{
    if (!visible_ || !needed()) return;

    Rect abs   = absoluteRect();
    Rect thumb = thumbRect();
    const auto& t = Theme::instance();

    // Track (very subtle)
    ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, 80);
    ctx.fill.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, 4.0f, 4, true);

    // Thumb
    bool hovered = isHovered();
    Color tc = t.scrollbarThumb;
    if (hovered || dragging_)
        tc = Color(tc.r + 40, tc.g + 40, tc.b + 40, std::min(255, (int)tc.a + 80));

    ctx.fill.SetColor(tc.r, tc.g, tc.b, tc.a);
    ctx.fill.RoundedRectangle(thumb.x, thumb.y, thumb.w, thumb.h, 4.0f, 4, true);
}

void ScrollBar::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;
    Rect thumb = thumbRect();
    if (thumb.contains(e.x, e.y))
    {
        dragging_   = true;
        dragStart_  = (orientation_ == ScrollBarOrientation::Vertical) ? e.y : e.x;
        dragOrigin_ = scrollValue_;
    }
    else
    {
        // Click on track → jump
        Rect abs = absoluteRect();
        float maxV = maxValue();
        if (orientation_ == ScrollBarOrientation::Vertical)
        {
            float ratio = (abs.h > 0.0f) ? (e.y - abs.y) / abs.h : 0.0f;
            scrollValue_ = ratio * maxV;
        }
        else
        {
            float ratio = (abs.w > 0.0f) ? (e.x - abs.x) / abs.w : 0.0f;
            scrollValue_ = ratio * maxV;
        }
        clampScroll();
        scrolled.emit(scrollValue_);
        markDirty();
    }
}

void ScrollBar::onMouseRelease(MouseEvent& e)
{
    (void)e;
    dragging_ = false;
}

void ScrollBar::onMouseMove(MouseEvent& e)
{
    if (!dragging_) return;
    Rect abs  = absoluteRect();
    float maxV = maxValue();

    if (orientation_ == ScrollBarOrientation::Vertical)
    {
        float trackLen = abs.h;
        float thumbLen = std::max(kMinThumbLen, trackLen * viewSize_ / contentSize_);
        float travel   = trackLen - thumbLen;
        if (travel <= 0.0f) return;
        float delta    = e.y - dragStart_;
        scrollValue_   = dragOrigin_ + delta * maxV / travel;
    }
    else
    {
        float trackLen = abs.w;
        float thumbLen = std::max(kMinThumbLen, trackLen * viewSize_ / contentSize_);
        float travel   = trackLen - thumbLen;
        if (travel <= 0.0f) return;
        float delta    = e.x - dragStart_;
        scrollValue_   = dragOrigin_ + delta * maxV / travel;
    }
    clampScroll();
    scrolled.emit(scrollValue_);
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  ScrollView
// ═════════════════════════════════════════════════════════════════════════════

ScrollView::ScrollView()
{
    vbar_ = createChild<ScrollBar>(ScrollBarOrientation::Vertical);
    vbar_->setScrollable(false);
    vbar_->scrolled.connect([this](float v){ setScrollY(v); });

    hbar_ = createChild<ScrollBar>(ScrollBarOrientation::Horizontal);
    hbar_->setScrollable(false);
    hbar_->scrolled.connect([this](float v){ setScrollX(v); });
}

void ScrollView::setScrollX(float v)
{
    scrollX_ = v;
    hbar_->setValue(v);
    markDirty();
}

void ScrollView::setScrollY(float v)
{
    scrollY_ = v;
    vbar_->setValue(v);
    markDirty();
}

void ScrollView::updateBars()
{
    float vw = rect_.w;
    float vh = rect_.h;

    bool needV = vbar_ && contentH_ > vh + 0.5f;
    bool needH = hbar_ && contentW_ > vw + 0.5f;

    float barThick = ScrollBar::kBarThickness + kBarGap;
    float viewW = needV ? vw - barThick : vw;
    float viewH = needH ? vh - barThick : vh;

    if (vbar_)
    {
        vbar_->setVisible(needV);
        vbar_->setRect({viewW + kBarGap, 0, ScrollBar::kBarThickness, viewH});
        vbar_->setContentSize(contentH_);
        vbar_->setViewSize(viewH);
    }
    if (hbar_)
    {
        hbar_->setVisible(needH);
        hbar_->setRect({0, viewH + kBarGap, viewW, ScrollBar::kBarThickness});
        hbar_->setContentSize(contentW_);
        hbar_->setViewSize(viewW);
    }

    // Clamp scroll positions
    float maxX = std::max(0.0f, contentW_ - viewW);
    float maxY = std::max(0.0f, contentH_ - viewH);
    scrollX_ = std::min(scrollX_, maxX);
    scrollY_ = std::min(scrollY_, maxY);
}

void ScrollView::layout()
{
    if (!contentWidget_) { Widget::layout(); return; }

    // First pass: measure content at full width
    contentWidget_->setScrollable(true);
    contentWidget_->setRect({0, 0,
                              std::max(rect_.w, contentW_),
                              std::max(rect_.h, contentH_)});
    contentWidget_->layout();

    // Read the content's resulting size hint
    auto sh   = contentWidget_->sizeHint();
    contentW_ = std::max(sh.x, rect_.w);
    contentH_ = sh.y;

    updateBars();

    // Second pass: content rect = viewport dimensions (excluding scrollbar areas).
    // The scroll offset handles showing the overflowing content.
    float barThick = ScrollBar::kBarThickness + kBarGap;
    bool needV = vbar_ && vbar_->isVisible();
    bool needH = hbar_ && hbar_->isVisible();
    float viewW = needV ? rect_.w - barThick : rect_.w;
    float viewH = needH ? rect_.h - barThick : rect_.h;
    contentWidget_->setRect({0, 0, viewW, contentH_});
    contentWidget_->layout();
}

void ScrollView::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    // Clip to view area (exclude scrollbar region)
    float barThick = ScrollBar::kBarThickness + kBarGap;
    bool needV = vbar_ && vbar_->isVisible();
    bool needH = hbar_ && hbar_->isVisible();
    float viewW = needV ? abs.w - barThick : abs.w;
    float viewH = needH ? abs.h - barThick : abs.h;

    Rect viewRect = {abs.x, abs.y, viewW, viewH};

    // Paint scrollbars first (same scissor as background, merges with it)
    if (needV) vbar_->paint(ctx);
    if (needH) hbar_->paint(ctx);

    ctx.pushClip(viewRect);
    if (contentWidget_)
        contentWidget_->paint(ctx);
    ctx.popClip();
}

void ScrollView::onMouseScroll(MouseEvent& e)
{
    float step = 30.0f;
    if (e.scrollY != 0.0f)
    {
        scrollY_ -= e.scrollY * step;
        scrollY_  = std::max(0.0f, std::min(scrollY_, std::max(0.0f, contentH_ - rect_.h)));
        vbar_->setValue(scrollY_);
        markDirty();
    }
    if (e.scrollX != 0.0f)
    {
        scrollX_ -= e.scrollX * step;
        scrollX_  = std::max(0.0f, std::min(scrollX_, std::max(0.0f, contentW_ - rect_.w)));
        hbar_->setValue(scrollX_);
        markDirty();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  ListBox
// ═════════════════════════════════════════════════════════════════════════════

void ListBox::addItem(const std::string& text)
{
    items_.push_back(text);
    markDirty();
    if (vbar_) {
        vbar_->setContentSize(static_cast<float>(items_.size()) * itemHeight());
        vbar_->setViewSize(rect_.h);
    }
}

void ListBox::removeItem(int index)
{
    if (index < 0 || index >= static_cast<int>(items_.size())) return;
    items_.erase(items_.begin() + index);
    if (selectedIdx_ == index) selectedIdx_ = -1;
    else if (selectedIdx_ > index) --selectedIdx_;
    markDirty();
}

void ListBox::clearItems()
{
    items_.clear();
    selectedIdx_  = -1;
    scrollOffset_ = 0.0f;
    markDirty();
}

static const std::string kEmptyString;
const std::string& ListBox::itemText(int index) const
{
    if (index < 0 || index >= static_cast<int>(items_.size())) return kEmptyString;
    return items_[static_cast<size_t>(index)];
}

void ListBox::setSelectedIndex(int idx)
{
    if (idx < -1 || idx >= static_cast<int>(items_.size())) return;
    selectedIdx_ = idx;
    selectionChanged.emit(selectedIdx_);
    markDirty();
}

void ListBox::clearSelection()
{
    setSelectedIndex(-1);
}

float ListBox::itemHeight() const
{
    const auto& t = Theme::instance();
    return t.fontSize + t.padding * 2.0f;
}

void ListBox::clampScroll()
{
    float total  = static_cast<float>(items_.size()) * itemHeight();
    float maxOff = std::max(0.0f, total - rect_.h);
    scrollOffset_ = std::max(0.0f, std::min(scrollOffset_, maxOff));
}

int ListBox::itemAtY(float absY) const
{
    Rect abs = absoluteRect();
    float relY = absY - abs.y + scrollOffset_;
    int idx = static_cast<int>(relY / itemHeight());
    if (idx < 0 || idx >= static_cast<int>(items_.size())) return -1;
    return idx;
}

void ListBox::layout()
{
    if (!vbar_)
    {
        vbar_ = createChild<ScrollBar>(ScrollBarOrientation::Vertical);
        vbar_->setScrollable(false);
        vbar_->scrolled.connect([this](float v){
            scrollOffset_ = v;
            markDirty();
        });
    }

    float barThick = ScrollBar::kBarThickness + 2.0f;
    float viewW = rect_.w - barThick;
    float total  = static_cast<float>(items_.size()) * itemHeight();

    vbar_->setRect({viewW + 2.0f, 0, ScrollBar::kBarThickness, rect_.h});
    vbar_->setContentSize(total);
    vbar_->setViewSize(rect_.h);
    clampScroll();
}

void ListBox::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    // Background
    ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
    ctx.fill.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, true);
    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.line.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, false);

    float ih  = itemHeight();
    float barW = (vbar_ && vbar_->needed()) ? ScrollBar::kBarThickness + 2.0f : 0.0f;
    float viewW = abs.w - barW;

    // Paint scrollbar first — same scissor as background, merges without clip change
    if (vbar_ && vbar_->needed())
        vbar_->paint(ctx);

    ctx.pushClip({abs.x, abs.y, viewW, abs.h});

    int first = static_cast<int>(scrollOffset_ / ih);
    int last  = static_cast<int>((scrollOffset_ + abs.h) / ih) + 1;
    last = std::min(last, static_cast<int>(items_.size()));

    if (overrideFont_) ctx.pushFont(overrideFont_);
    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);

    for (int i = first; i < last; ++i)
    {
        float iy = abs.y + static_cast<float>(i) * ih - scrollOffset_;

        if (i == selectedIdx_)
        {
            ctx.fill.SetColor(t.selectionColor.r, t.selectionColor.g,
                              t.selectionColor.b, t.selectionColor.a);
            ctx.fill.Rectangle(abs.x, iy, viewW, ih, true);
        }
        else if (isHovered())
        {
            // Per-row hover is handled via mouse move — skip for now
        }

        float asc = ctx.font.GetAscender();
        float ty  = iy + t.padding + asc;
        float tx  = abs.x + t.padding;

        ctx.font.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
        ctx.font.Print(items_[static_cast<size_t>(i)].c_str(), tx, ty);
    }

    if (overrideFont_) ctx.popFont();
    ctx.popClip();
}

void ListBox::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;
    int idx = itemAtY(e.y);
    if (idx >= 0)
    {
        selectedIdx_ = idx;
        selectionChanged.emit(selectedIdx_);
        markDirty();
    }
}

void ListBox::onMouseScroll(MouseEvent& e)
{
    float step = itemHeight() * 2.0f;
    scrollOffset_ -= e.scrollY * step;
    clampScroll();
    if (vbar_) vbar_->setValue(scrollOffset_);
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  ListWidget
// ═════════════════════════════════════════════════════════════════════════════

float ListWidget::rowH() const
{
    const auto& t = Theme::instance();
    return t.buttonHeight + kRowPad * 2.0f;
}

void ListWidget::clampScroll()
{
    float total  = static_cast<float>(rows_.size()) * rowH();
    float maxOff = std::max(0.0f, total - rect_.h);
    scrollOffset_ = std::max(0.0f, std::min(scrollOffset_, maxOff));
}

int ListWidget::rowAtY(float absY) const
{
    Rect abs = absoluteRect();
    float relY = absY - abs.y + scrollOffset_;
    int idx = static_cast<int>(relY / rowH());
    if (idx < 0 || idx >= static_cast<int>(rows_.size())) return -1;
    return idx;
}

void ListWidget::setSelectedIndex(int idx)
{
    if (idx < -1 || idx >= static_cast<int>(rows_.size())) return;
    selectedIdx_ = idx;
    selectionChanged.emit(selectedIdx_);
    markDirty();
}

void ListWidget::clearSelection() { setSelectedIndex(-1); }

void ListWidget::removeRow(int index)
{
    if (index < 0 || index >= static_cast<int>(rows_.size())) return;
    auto* w = rows_[static_cast<size_t>(index)];
    rows_.erase(rows_.begin() + index);
    removeChild(w);
    if (selectedIdx_ == index) selectedIdx_ = -1;
    else if (selectedIdx_ > index) --selectedIdx_;
    markDirty();
}

void ListWidget::clearRows()
{
    for (auto* w : rows_) removeChild(w);
    rows_.clear();
    selectedIdx_  = -1;
    scrollOffset_ = 0.0f;
    markDirty();
}

void ListWidget::connectRowPress(Widget* w, int idx)
{
    w->pressed.connect([this, idx](int btn){
        if (btn == 0) setSelectedIndex(idx);
    });
    for (auto* c : w->children())
        connectRowPress(c, idx);
}

void ListWidget::layout()
{
    if (!vbar_)
    {
        vbar_ = createChild<ScrollBar>(ScrollBarOrientation::Vertical);
        vbar_->setScrollable(false);
        vbar_->scrolled.connect([this](float v){
            scrollOffset_ = v;
            markDirty();
        });
    }

    float barThick = ScrollBar::kBarThickness + 2.0f;
    float viewW = rect_.w - barThick;
    float rh    = rowH();
    float total = static_cast<float>(rows_.size()) * rh;

    vbar_->setRect({viewW + 2.0f, 0, ScrollBar::kBarThickness, rect_.h});
    vbar_->setContentSize(total);
    vbar_->setViewSize(rect_.h);
    clampScroll();

    // Position each row (relative to ListBox/ListWidget parent — 0,0 based)
    for (int i = 0; i < static_cast<int>(rows_.size()); ++i)
    {
        float ry = static_cast<float>(i) * rh + kRowPad;
        rows_[static_cast<size_t>(i)]->setRect({kRowPad, ry,
                                                 viewW - kRowPad * 2, rh - kRowPad * 2});
        rows_[static_cast<size_t>(i)]->layout();
    }
}

void ListWidget::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    // Background
    ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
    ctx.fill.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, true);
    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.line.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, false);

    float rh    = rowH();
    float barW  = (vbar_ && vbar_->needed()) ? ScrollBar::kBarThickness + 2.0f : 0.0f;
    float viewW = abs.w - barW;

    // Paint scrollbar first — same scissor as background, merges without clip change
    if (vbar_ && vbar_->needed())
        vbar_->paint(ctx);

    ctx.pushClip({abs.x, abs.y, viewW, abs.h});

    int first = static_cast<int>(scrollOffset_ / rh);
    int last  = static_cast<int>((scrollOffset_ + abs.h) / rh) + 1;
    last = std::min(last, static_cast<int>(rows_.size()));

    for (int i = first; i < last; ++i)
    {
        float ry = abs.y + static_cast<float>(i) * rh - scrollOffset_;

        if (i == selectedIdx_)
        {
            ctx.fill.SetColor(t.selectionColor.r, t.selectionColor.g,
                              t.selectionColor.b, t.selectionColor.a);
            ctx.fill.Rectangle(abs.x + 1, ry, viewW - 2, rh, true);
        }

        // Temporarily move row to its scroll-adjusted relative position for paint
        Widget* row = rows_[static_cast<size_t>(i)];
        Rect savedRect = row->rect();
        float newY = static_cast<float>(i) * rh - scrollOffset_ + kRowPad;
        row->setPosition(kRowPad, newY);
        row->paint(ctx);
        row->setPosition(savedRect.x, savedRect.y);
    }

    ctx.popClip();
}

void ListWidget::onMouseScroll(MouseEvent& e)
{
    float step = rowH() * 1.5f;
    scrollOffset_ -= e.scrollY * step;
    clampScroll();
    if (vbar_) vbar_->setValue(scrollOffset_);
    markDirty();
}

void ListWidget::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;
    int idx = rowAtY(e.y);
    if (idx >= 0) setSelectedIndex(idx);
}
