#include "pch.hpp"
#include "LayoutWidgets.hpp"

// ═════════════════════════════════════════════════════════════════════════════
//  GridLayout
// ═════════════════════════════════════════════════════════════════════════════

GridLayout::GridLayout(int cols) : cols_(cols > 0 ? cols : 1) {}

int GridLayout::rows() const
{
    int visible = 0;
    for (auto* c : children_)
        if (c->isVisible()) ++visible;
    return (visible + cols_ - 1) / cols_;
}

Widget::Vec2f GridLayout::sizeHint() const
{
    int r = rows();
    if (r == 0) return {0, 0};

    // Find max child sizeHint for uniform cell sizing
    float maxW = 0, maxH = 0;
    for (auto* c : children_)
    {
        if (!c->isVisible()) continue;
        auto h = c->sizeHint();
        if (h.x > maxW) maxW = h.x;
        if (h.y > maxH) maxH = h.y;
    }

    float w = padding_.left + padding_.right + cols_ * maxW + (cols_ - 1) * spacingH_;
    float h = padding_.top + padding_.bottom + r * maxH + (r - 1) * spacingV_;
    return {w, h};
}

void GridLayout::layout()
{
    // Collect visible children
    std::vector<Widget*> vis;
    for (auto* c : children_)
        if (c->isVisible()) vis.push_back(c);

    if (vis.empty()) return;

    int r = static_cast<int>((vis.size() + cols_ - 1) / cols_);

    // Available space
    float availW = rect_.w - padding_.left - padding_.right;
    float availH = rect_.h - padding_.top  - padding_.bottom;

    float cellW = (availW - (cols_ - 1) * spacingH_) / cols_;
    float cellH = (availH - (r - 1) * spacingV_) / r;

    if (cellW < 0) cellW = 0;
    if (cellH < 0) cellH = 0;

    for (size_t i = 0; i < vis.size(); ++i)
    {
        int col = static_cast<int>(i) % cols_;
        int row = static_cast<int>(i) / cols_;

        float x = padding_.left + col * (cellW + spacingH_);
        float y = padding_.top  + row * (cellH + spacingV_);

        vis[i]->setRect({x, y, cellW, cellH});
        vis[i]->layout();
    }
}

void GridLayout::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    for (auto* c : children_)
        c->paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  BorderLayout
// ═════════════════════════════════════════════════════════════════════════════

void BorderLayout::setWidget(BorderRegion region, Widget* w)
{
    int idx = static_cast<int>(region);
    if (regions_[idx])
        removeChild(regions_[idx]);
    regions_[idx] = w;
    if (w) addChild(w);
    markDirty();
}

Widget* BorderLayout::get(BorderRegion region) const
{
    return regions_[static_cast<int>(region)];
}

void BorderLayout::setRegionSize(BorderRegion region, float size)
{
    regionSizes_[static_cast<int>(region)] = size;
    markDirty();
}

float BorderLayout::regionSize(BorderRegion region) const
{
    return regionSizes_[static_cast<int>(region)];
}

float BorderLayout::resolveSize(BorderRegion region, float totalDim, float hint) const
{
    float s = regionSizes_[static_cast<int>(region)];
    if (s > 0)  return s;                       // fixed pixels
    if (s < 0)  return totalDim * (-s);          // percentage (e.g. -0.2 = 20%)
    return hint;                                  // 0 = use sizeHint
}

Widget::Vec2f BorderLayout::sizeHint() const
{
    // If explicit size was set, honour it
    if (rect_.w > 0 && rect_.h > 0)
        return {rect_.w, rect_.h};

    // Use regionSizes_ (px) when available, otherwise child sizeHint
    auto hintPx = [&](BorderRegion r, float childHint) -> float {
        float s = regionSizes_[static_cast<int>(r)];
        if (s > 0)  return s;           // fixed px
        return childHint;                // % or auto → child hint
    };

    auto* top    = regions_[static_cast<int>(BorderRegion::Top)];
    auto* bottom = regions_[static_cast<int>(BorderRegion::Bottom)];
    auto* left   = regions_[static_cast<int>(BorderRegion::Left)];
    auto* right  = regions_[static_cast<int>(BorderRegion::Right)];
    auto* center = regions_[static_cast<int>(BorderRegion::Center)];

    float topH = 0, bottomH = 0, leftW = 0, rightW = 0;
    float centerW = 0, centerH = 0;

    if (top    && top->isVisible())    topH    = hintPx(BorderRegion::Top,    top->sizeHint().y);
    if (bottom && bottom->isVisible()) bottomH = hintPx(BorderRegion::Bottom, bottom->sizeHint().y);
    if (left   && left->isVisible())   leftW   = hintPx(BorderRegion::Left,   left->sizeHint().x);
    if (right  && right->isVisible())  rightW  = hintPx(BorderRegion::Right,  right->sizeHint().x);
    if (center && center->isVisible()) { centerW = center->sizeHint().x; centerH = center->sizeHint().y; }

    int gapsH = (leftW > 0 ? 1 : 0) + (rightW > 0 ? 1 : 0);
    int gapsV = (topH > 0 ? 1 : 0) + (bottomH > 0 ? 1 : 0);

    float midH = std::max({centerH, 0.f});  // middle row height
    int midGap = (midH > 0 || leftW > 0 || rightW > 0) ? 1 : 0;

    float w = padding_.left + padding_.right + leftW + centerW + rightW + gapsH * spacing_;
    float h = padding_.top + padding_.bottom + topH + bottomH + midH
              + gapsV * spacing_ + midGap * spacing_;

    // Respect explicit dimensions as minimum
    if (rect_.w > 0) w = std::max(w, rect_.w);
    if (rect_.h > 0) h = std::max(h, rect_.h);

    return {w, h};
}

void BorderLayout::layout()
{
    auto* top    = regions_[static_cast<int>(BorderRegion::Top)];
    auto* bottom = regions_[static_cast<int>(BorderRegion::Bottom)];
    auto* left   = regions_[static_cast<int>(BorderRegion::Left)];
    auto* right  = regions_[static_cast<int>(BorderRegion::Right)];
    auto* center = regions_[static_cast<int>(BorderRegion::Center)];

    float availW = rect_.w - padding_.left - padding_.right;
    float availH = rect_.h - padding_.top  - padding_.bottom;
    float x0 = padding_.left;
    float y0 = padding_.top;

    // ── Top: full width ──────────────────────────────────────────────────
    float topH = 0;
    if (top && top->isVisible())
    {
        float hint = top->sizeHint().y;
        topH = resolveSize(BorderRegion::Top, availH, hint);
        top->setRect({x0, y0, availW, topH});
        top->layout();
        y0 += topH + spacing_;
        availH -= topH + spacing_;
    }

    // ── Bottom: full width ───────────────────────────────────────────────
    float bottomH = 0;
    if (bottom && bottom->isVisible())
    {
        float hint = bottom->sizeHint().y;
        bottomH = resolveSize(BorderRegion::Bottom, rect_.h - padding_.top - padding_.bottom, hint);
        availH -= bottomH + spacing_;
        float by = y0 + availH + spacing_;
        bottom->setRect({x0, by, availW, bottomH});
        bottom->layout();
    }

    // ── Left: remaining height ───────────────────────────────────────────
    float leftW = 0;
    float midX = x0;
    float midW = availW;
    if (left && left->isVisible())
    {
        float hint = left->sizeHint().x;
        leftW = resolveSize(BorderRegion::Left, rect_.w - padding_.left - padding_.right, hint);
        left->setRect({midX, y0, leftW, availH});
        left->layout();
        midX += leftW + spacing_;
        midW -= leftW + spacing_;
    }

    // ── Right: remaining height ──────────────────────────────────────────
    float rightW = 0;
    if (right && right->isVisible())
    {
        float hint = right->sizeHint().x;
        rightW = resolveSize(BorderRegion::Right, rect_.w - padding_.left - padding_.right, hint);
        midW -= rightW + spacing_;
        float rx = midX + midW + spacing_;
        right->setRect({rx, y0, rightW, availH});
        right->layout();
    }

    // ── Center: whatever is left ─────────────────────────────────────────
    if (center && center->isVisible())
    {
        if (midW < 0) midW = 0;
        if (availH < 0) availH = 0;
        center->setRect({midX, y0, midW, availH});
        center->layout();
    }
}

void BorderLayout::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    for (auto* c : children_)
        c->paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Collapsible
// ═════════════════════════════════════════════════════════════════════════════

Collapsible::Collapsible(const std::string& title, bool expanded)
    : title_(title), expanded_(expanded)
{
    acceptsFocus_ = true;
}

void Collapsible::setExpanded(bool e)
{
    if (expanded_ != e)
    {
        expanded_ = e;
        expandedChanged.emit(expanded_);
        markDirty();
        WidgetApp::instance().requestLayout();
    }
}

void Collapsible::onMousePress(MouseEvent& e)
{
    if (!enabled_) return;

    // Only toggle if click is in the header area
    Rect abs = absoluteRect();
    Rect header = {abs.x, abs.y, abs.w, headerH_};
    if (header.contains(e.x, e.y))
    {
        setExpanded(!expanded_);
        e.consumed = true;
        markDirty();
    }
}

Widget::Vec2f Collapsible::sizeHint() const
{
    float w = 200.0f;
    float h = headerH_;

    if (expanded_)
    {
        for (auto* c : children_)
        {
            if (!c->isVisible()) continue;
            auto hint = c->sizeHint();
            if (hint.x > w) w = hint.x;
            h += hint.y + contentSpacing_;
        }
        h += contentPad_;  // bottom padding after content
    }

    if (rect_.w > 0) w = std::max(w, rect_.w);
    return {w, h};
}

void Collapsible::layout()
{
    float y = headerH_;

    if (expanded_)
    {
        float contentW = rect_.w - contentPad_ * 2;
        if (contentW < 0) contentW = 0;

        for (auto* c : children_)
        {
            if (!c->isVisible()) continue;
            auto hint = c->sizeHint();
            float h = hint.y;
            c->setRect({contentPad_, y, contentW, h});
            c->layout();
            y += h + contentSpacing_;
        }
    }
    else
    {
        // Collapsed: hide all children by giving them zero size
        for (auto* c : children_)
            c->setRect({0, headerH_, 0, 0});
    }
}

void Collapsible::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    // ── Header background ────────────────────────────────────────────────
    Rect headerRect = {abs.x, abs.y, abs.w, headerH_};
    Rect clippedHdr;
    if (ctx.clipRectIntersect(headerRect, clippedHdr))
    {
        Color bg = headerColor_.a > 0 ? headerColor_ : t.collapsibleHeaderBg;
        if (hovered_)
        {
            bg.r = std::min(255, bg.r + 15);
            bg.g = std::min(255, bg.g + 15);
            bg.b = std::min(255, bg.b + 15);
        }
        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fill.Rectangle(
            static_cast<int>(clippedHdr.x), static_cast<int>(clippedHdr.y),
            static_cast<int>(clippedHdr.w), static_cast<int>(clippedHdr.h), true);
    }

    // ── Triangle indicator ───────────────────────────────────────────────
    float triSize = 8.0f;
    float triX = abs.x + 8.0f;
    float triCY = abs.y + headerH_ * 0.5f;

    ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);

    if (expanded_)
    {
        // ▼ Down-pointing triangle
        float x1 = triX;
        float y1 = triCY - triSize * 0.35f;
        float x2 = triX + triSize;
        float y2 = y1;
        float x3 = triX + triSize * 0.5f;
        float y3 = triCY + triSize * 0.5f;
        ctx.fill.Triangle(x1, y1, x2, y2, x3, y3, true);
    }
    else
    {
        // ▶ Right-pointing triangle
        float x1 = triX + 1.0f;
        float y1 = triCY - triSize * 0.5f;
        float x2 = triX + 1.0f;
        float y2 = triCY + triSize * 0.5f;
        float x3 = triX + triSize;
        float y3 = triCY;
        ctx.fill.Triangle(x1, y1, x2, y2, x3, y3, true);
    }

    // ── Title text ───────────────────────────────────────────────────────
    float textX = triX + triSize + 6.0f;
    float asc   = ctx.font.GetAscender();
    float textY = abs.y + (headerH_ + asc) * 0.5f;

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetColor(enabled_ ? t.textColor : t.textDisabled);
    ctx.font.SetBatch(&ctx.text);

    Rect textRect = {textX, textY - asc, abs.w, asc};
    if (!ctx.isClipped(textRect))
    {
        ctx.font.Print(title_.c_str(), textX, textY);
    }

    // ── Content (children) - only if expanded ────────────────────────────
    if (expanded_)
    {
        ctx.pushClip(abs);
        for (auto* c : children_)
            c->paint(ctx);
        ctx.popClip();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  StatusBar
// ═════════════════════════════════════════════════════════════════════════════

Widget::Vec2f StatusBar::sizeHint() const
{
    const auto& t = Theme::instance();
    return {200.0f, t.fontSize + 6.0f};
}

void StatusBar::layout()
{
    const auto& t = Theme::instance();
    float h = rect_.h;
    float x = t.padding;

    // Reserve space for status text
    if (!text_.empty())
    {
        float tw = text_.size() * t.fontSize * 0.55f;
        x += tw + spacing_;
    }

    // Layout children horizontally
    for (auto* c : children_)
    {
        if (!c->isVisible()) continue;
        auto hint = c->sizeHint();
        c->setRect({x, 0, hint.x, h});
        c->layout();
        x += hint.x + spacing_;
    }
}

void StatusBar::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    // Background
    Rect clipped;
    if (ctx.clipRectIntersect(abs, clipped))
    {
        ctx.fill.SetColor(bgColor_.r, bgColor_.g, bgColor_.b, bgColor_.a);
        ctx.fill.Rectangle(
            static_cast<int>(clipped.x), static_cast<int>(clipped.y),
            static_cast<int>(clipped.w), static_cast<int>(clipped.h), true);

        // Top border line
        ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
        ctx.line.Line2D(
            static_cast<int>(abs.x), static_cast<int>(abs.y),
            static_cast<int>(abs.x + abs.w), static_cast<int>(abs.y));
    }

    // Status text
    if (!text_.empty())
    {
        ctx.font.SetFontSize(t.fontSize * 0.85f);
        ctx.font.SetColor(t.textDisabled);
        ctx.font.SetBatch(&ctx.text);
        float ty = abs.y + (abs.h - t.fontSize * 0.85f) * 0.5f;
        ctx.font.Print(text_.c_str(), abs.x + t.padding, ty);
    }

    // Resize grip — three diagonal lines at bottom-right corner (Windows style)
    {
        float gx = abs.x + abs.w - 2.0f;
        float gy = abs.y + abs.h - 2.0f;
        ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b,
                          static_cast<uint8_t>(t.borderColor.a * 2 / 3));
        ctx.drawLine(gx - 10.0f, gy, gx, gy - 10.0f);
        ctx.drawLine(gx -  6.0f, gy, gx, gy -  6.0f);
        ctx.drawLine(gx -  2.0f, gy, gx, gy -  2.0f);
    }

    // Children
    for (auto* c : children_)
        c->paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  StackLayout - shows one child at a time
// ═════════════════════════════════════════════════════════════════════════════

void StackLayout::setCurrentIndex(int idx)
{
    if (currentIndex_ != idx)
    {
        previousIndex_ = currentIndex_;
        animDir_ = (idx > currentIndex_) ? 1 : -1;
        currentIndex_ = idx;
        if (animated_ && previousIndex_ >= 0)
        {
            animating_ = true;
            animProgress_ = 0.0f;
        }
        else
        {
            previousIndex_ = -1;
            animating_ = false;
            animProgress_ = 1.0f;
        }
        currentChanged.emit(idx);
        markDirty();
        WidgetApp::instance().requestLayout();
    }
}

Widget* StackLayout::currentWidget() const
{
    int i = 0;
    for (auto* c : children_)
    {
        if (i == currentIndex_) return c;
        ++i;
    }
    return nullptr;
}

Widget::Vec2f StackLayout::sizeHint() const
{
    // Max of all children's hints
    float w = 0, h = 0;
    for (auto* c : children_)
    {
        auto hint = c->sizeHint();
        if (hint.x > w) w = hint.x;
        if (hint.y > h) h = hint.y;
    }
    return {w, h};
}

void StackLayout::layout()
{
    int i = 0;
    for (auto* c : children_)
    {
        if (i == currentIndex_ || (animating_ && i == previousIndex_))
        {
            c->setVisible(true);
            c->setRect({0, 0, rect_.w, rect_.h});
            c->layout();
        }
        else
        {
            c->setVisible(false);
        }
        ++i;
    }
}

void StackLayout::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    if (animating_)
    {
        const float dt = WidgetApp::instance().deltaTime();
        const float step = (animDuration_ > 0.0f) ? (dt / animDuration_) : 1.0f;
        animProgress_ += step;
        if (animProgress_ >= 1.0f)
        {
            animProgress_ = 1.0f;
            animating_ = false;
            previousIndex_ = -1;
            WidgetApp::instance().requestLayout();
        }
        markDirty();
    }

    if (animating_ && previousIndex_ >= 0)
    {
        Widget* prev = nullptr;
        Widget* cur = nullptr;
        int i = 0;
        for (auto* c : children_)
        {
            if (i == previousIndex_) prev = c;
            if (i == currentIndex_) cur = c;
            ++i;
        }

        const float t = applyEasing(easeType_, animProgress_);
        const float oldX = -animDir_ * t * rect_.w;
        const float newX = animDir_ * (1.0f - t) * rect_.w;
        ctx.pushClip(abs);

        auto paintShifted = [&](Widget* w, float xOff)
        {
            if (!w || !w->isVisible()) return;
            Rect saved = w->rect();
            w->setRect({xOff, 0, rect_.w, rect_.h});
            w->paint(ctx);
            w->setRect(saved);
        };

        paintShifted(prev, oldX);
        paintShifted(cur, newX);
        ctx.popClip();
        return;
    }

    // Only paint the active child
    Widget* cur = currentWidget();
    if (cur && cur->isVisible())
        cur->paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  FormLayout - two-column label:widget
//  Children are in pairs: [label0, widget0, label1, widget1, ...]
// ═════════════════════════════════════════════════════════════════════════════

Widget::Vec2f FormLayout::sizeHint() const
{
    float maxLabelW = labelWidth_;
    float maxWidgetW = 0;
    float totalH = padding_.top + padding_.bottom;

    int count = 0;
    for (size_t i = 0; i + 1 < children_.size(); i += 2)
    {
        auto* label  = children_[i];
        auto* widget = children_[i + 1];
        if (!label->isVisible() && !widget->isVisible()) continue;

        if (labelWidth_ <= 0)
        {
            float lw = label->sizeHint().x;
            if (lw > maxLabelW) maxLabelW = lw;
        }

        float ww = widget->sizeHint().x;
        if (ww > maxWidgetW) maxWidgetW = ww;

        float rh = std::max(label->sizeHint().y, widget->sizeHint().y);
        totalH += rh + (count > 0 ? spacingV_ : 0);
        ++count;
    }

    float w = padding_.left + padding_.right + maxLabelW + spacingH_ + maxWidgetW;
    return {w, totalH};
}

void FormLayout::layout()
{
    float availW = rect_.w - padding_.left - padding_.right;

    // Determine label column width
    float labelW = labelWidth_;
    if (labelW <= 0)
    {
        for (size_t i = 0; i + 1 < children_.size(); i += 2)
        {
            float lw = children_[i]->sizeHint().x;
            if (lw > labelW) labelW = lw;
        }
    }

    float widgetW = availW - labelW - spacingH_;
    if (widgetW < 0) widgetW = 0;

    float y = padding_.top;
    for (size_t i = 0; i + 1 < children_.size(); i += 2)
    {
        auto* label  = children_[i];
        auto* widget = children_[i + 1];
        if (!label->isVisible() && !widget->isVisible()) continue;

        float rh = std::max(label->sizeHint().y, widget->sizeHint().y);

        // Label: right-aligned in its column, vertically centered
        float ly = y + (rh - label->sizeHint().y) * 0.5f;
        label->setRect({padding_.left, ly, labelW, label->sizeHint().y});
        label->layout();

        // Widget: fills remaining width
        float wy = y + (rh - widget->sizeHint().y) * 0.5f;
        widget->setRect({padding_.left + labelW + spacingH_, wy, widgetW, rh});
        widget->layout();

        y += rh + spacingV_;
    }
}

void FormLayout::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    for (auto* c : children_)
        c->paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  FlowLayout - wraps children like text
// ═════════════════════════════════════════════════════════════════════════════

Widget::Vec2f FlowLayout::sizeHint() const
{
    float availW = rect_.w > 0 ? rect_.w - padding_.left - padding_.right : 400.0f;
    float x = 0, y = 0, rowH = 0, maxW = 0;

    for (auto* c : children_)
    {
        if (!c->isVisible()) continue;
        auto hint = c->sizeHint();

        if (x > 0 && x + hint.x > availW)
        {
            // Wrap
            if (x > maxW) maxW = x - spacingH_;
            y += rowH + spacingV_;
            x = 0;
            rowH = 0;
        }

        x += hint.x + spacingH_;
        if (hint.y > rowH) rowH = hint.y;
    }

    if (x > maxW) maxW = x - spacingH_;
    y += rowH;

    return {padding_.left + padding_.right + maxW,
            padding_.top + padding_.bottom + y};
}

void FlowLayout::layout()
{
    float availW = rect_.w - padding_.left - padding_.right;
    float x = 0, y = 0, rowH = 0;

    // Two passes: first measure row heights, then position
    std::vector<Widget*> vis;
    for (auto* c : children_)
        if (c->isVisible()) vis.push_back(c);

    for (auto* c : vis)
    {
        auto hint = c->sizeHint();

        if (x > 0 && x + hint.x > availW)
        {
            // Wrap to next row
            y += rowH + spacingV_;
            x = 0;
            rowH = 0;
        }

        c->setRect({padding_.left + x, padding_.top + y, hint.x, hint.y});
        c->layout();

        x += hint.x + spacingH_;
        if (hint.y > rowH) rowH = hint.y;
    }
}

void FlowLayout::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    for (auto* c : children_)
        c->paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Overlay - children stacked in Z-order, all full area
// ═════════════════════════════════════════════════════════════════════════════

Widget::Vec2f Overlay::sizeHint() const
{
    float w = 0, h = 0;
    for (auto* c : children_)
    {
        if (!c->isVisible()) continue;
        auto hint = c->sizeHint();
        if (hint.x > w) w = hint.x;
        if (hint.y > h) h = hint.y;
    }
    return {w, h};
}

void Overlay::layout()
{
    for (auto* c : children_)
    {
        // All layers fill the full overlay area
        c->setRect({0, 0, rect_.w, rect_.h});
        c->layout();
    }
}

void Overlay::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    // Paint in order: first child = bottom, last = top
    for (auto* c : children_)
        c->paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Splitter - two children with draggable divider
// ═════════════════════════════════════════════════════════════════════════════

Splitter::Splitter(LayoutDir dir) : dir_(dir)
{
    // cursor_ stays Arrow; set to SizeH/SizeV only when hovering the handle
}

void Splitter::setRatio(float r)
{
    r = std::max(minRatio_, std::min(maxRatio_, r));
    if (ratio_ != r)
    {
        ratio_ = r;
        ratioChanged.emit(ratio_);
        markDirty();
        WidgetApp::instance().requestLayout();
    }
}

Widget::Vec2f Splitter::sizeHint() const
{
    float w = 0, h = 0;
    for (auto* c : children_)
    {
        if (!c->isVisible()) continue;
        auto hint = c->sizeHint();
        if (dir_ == LayoutDir::Horizontal)
        {
            w += hint.x;
            if (hint.y > h) h = hint.y;
        }
        else
        {
            h += hint.y;
            if (hint.x > w) w = hint.x;
        }
    }
    if (dir_ == LayoutDir::Horizontal) w += handleSize_;
    else                                h += handleSize_;
    return {w, h};
}

void Splitter::layout()
{
    // We only handle the first two children
    Widget* first  = children_.size() > 0 ? children_[0] : nullptr;
    Widget* second = children_.size() > 1 ? children_[1] : nullptr;
    if (!first && !second) return;

    if (dir_ == LayoutDir::Horizontal)
    {
        float total = rect_.w - handleSize_;
        float firstW  = total * ratio_;
        float secondW = total - firstW;

        // Clamp to min sizes
        if (firstW < minSize_)  { firstW = minSize_; secondW = total - firstW; }
        if (secondW < minSize_) { secondW = minSize_; firstW = total - secondW; }

        if (first)  { first->setRect({0, 0, firstW, rect_.h}); first->layout(); }
        if (second) { second->setRect({firstW + handleSize_, 0, secondW, rect_.h}); second->layout(); }
    }
    else
    {
        float total = rect_.h - handleSize_;
        float firstH  = total * ratio_;
        float secondH = total - firstH;

        if (firstH < minSize_)  { firstH = minSize_; secondH = total - firstH; }
        if (secondH < minSize_) { secondH = minSize_; firstH = total - secondH; }

        if (first)  { first->setRect({0, 0, rect_.w, firstH}); first->layout(); }
        if (second) { second->setRect({0, firstH + handleSize_, rect_.w, secondH}); second->layout(); }
    }
}

void Splitter::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    // Paint children
    for (auto* c : children_)
        c->paint(ctx);

    // Paint handle (divider bar)
    const auto& t = Theme::instance();
    float hx, hy, hw, hh;

    if (dir_ == LayoutDir::Horizontal)
    {
        float total = rect_.w - handleSize_;
        float firstW = total * ratio_;
        hx = abs.x + firstW;
        hy = abs.y;
        hw = handleSize_;
        hh = abs.h;
    }
    else
    {
        float total = rect_.h - handleSize_;
        float firstH = total * ratio_;
        hx = abs.x;
        hy = abs.y + firstH;
        hw = abs.w;
        hh = handleSize_;
    }

    // Handle background
    Color hc = dragging_      ? Color(100, 100, 110, 255)
             : handleHovered_ ? Color(80, 80, 90, 255)
                              : Color(65, 65, 70, 255);
    ctx.fill.SetColor(hc.r, hc.g, hc.b, hc.a);
    ctx.fill.Rectangle(static_cast<int>(hx), static_cast<int>(hy),
                        static_cast<int>(hw), static_cast<int>(hh), true);

    // Center dots/line indicator on the handle
    Color dc = (dragging_ || handleHovered_) ? Color(180, 180, 190, 230)
                                             : Color(130, 130, 138, 200);
    ctx.line.SetColor(dc.r, dc.g, dc.b, dc.a);
    if (dir_ == LayoutDir::Horizontal)
    {
        float cx = hx + hw * 0.5f;
        float cy = hy + hh * 0.5f;
        for (int i = -2; i <= 2; ++i)
        {
            int dy = static_cast<int>(cy + i * 6.0f);
            ctx.fill.SetColor(dc.r, dc.g, dc.b, dc.a);
            ctx.fill.Rectangle(static_cast<int>(cx - 1), dy, 2, 2, true);
        }
    }
    else
    {
        float cx = hx + hw * 0.5f;
        float cy = hy + hh * 0.5f;
        for (int i = -2; i <= 2; ++i)
        {
            int dx = static_cast<int>(cx + i * 6.0f);
            ctx.fill.SetColor(dc.r, dc.g, dc.b, dc.a);
            ctx.fill.Rectangle(dx, static_cast<int>(cy - 1), 2, 2, true);
        }
    }
}

void Splitter::onMousePress(MouseEvent& e)
{
    if (!enabled_) return;

    Rect abs = absoluteRect();
    float localX = e.x - abs.x;
    float localY = e.y - abs.y;

    // Check if click is on the handle
    if (dir_ == LayoutDir::Horizontal)
    {
        float total = rect_.w - handleSize_;
        float handleX = total * ratio_;
        if (localX >= handleX && localX <= handleX + handleSize_)
        {
            dragging_ = true;
            cursor_   = CursorType::SizeH;
            e.consumed = true;
        }
    }
    else
    {
        float total = rect_.h - handleSize_;
        float handleY = total * ratio_;
        if (localY >= handleY && localY <= handleY + handleSize_)
        {
            dragging_ = true;
            cursor_   = CursorType::SizeV;
            e.consumed = true;
        }
    }
}

void Splitter::onMouseRelease(MouseEvent& e)
{
    if (dragging_)
    {
        dragging_ = false;
        cursor_   = CursorType::Arrow;
        e.consumed = true;
        markDirty();
        return;  // don't fire clicked after drag
    }
    Widget::onMouseRelease(e);
}

void Splitter::onMouseMove(MouseEvent& e)
{
    Rect abs = absoluteRect();

    // Track whether mouse is specifically over the handle
    bool onHandle;
    if (dir_ == LayoutDir::Horizontal)
    {
        float total = rect_.w - handleSize_;
        float hx = abs.x + total * ratio_;
        onHandle = (e.x >= hx && e.x <= hx + handleSize_);
    }
    else
    {
        float total = rect_.h - handleSize_;
        float hy = abs.y + total * ratio_;
        onHandle = (e.y >= hy && e.y <= hy + handleSize_);
    }
    if (onHandle != handleHovered_)
    {
        handleHovered_ = onHandle;
        cursor_ = onHandle
            ? (dir_ == LayoutDir::Horizontal ? CursorType::SizeH : CursorType::SizeV)
            : CursorType::Arrow;
        markDirty();
    }

    if (!dragging_) return;

    if (dir_ == LayoutDir::Horizontal)
    {
        float total = rect_.w - handleSize_;
        if (total > 0)
            setRatio((e.x - abs.x) / total);
    }
    else
    {
        float total = rect_.h - handleSize_;
        if (total > 0)
            setRatio((e.y - abs.y) / total);
    }
    e.consumed = true;
}

void Splitter::onMouseLeave()
{
    if (handleHovered_) { handleHovered_ = false; markDirty(); }
    cursor_ = CursorType::Arrow;
    Widget::onMouseLeave();
}

// ═════════════════════════════════════════════════════════════════════════════
//  TabLayout - tabbed container
// ═════════════════════════════════════════════════════════════════════════════

static const std::string kEmptyTabLabel;

void TabLayout::addTabWidget(const std::string& label, Widget* content)
{
    tabs_.push_back({label, content});
    addChild(content);

    // First tab added → show it
    if (tabs_.size() == 1)
        currentIndex_ = 0;

    markDirty();
    WidgetApp::instance().requestLayout();
}

void TabLayout::setTabPosition(TabPosition pos)
{
    if (tabPosition_ != pos)
    {
        tabPosition_ = pos;
        markDirty();
        WidgetApp::instance().requestLayout();
    }
}

void TabLayout::removeTab(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return;

    auto& tab = tabs_[index];
    removeChild(tab.content);
    tabs_.erase(tabs_.begin() + index);

    // Adjust current index
    if (currentIndex_ >= static_cast<int>(tabs_.size()))
        currentIndex_ = static_cast<int>(tabs_.size()) - 1;

    markDirty();
    WidgetApp::instance().requestLayout();
}

void TabLayout::setCurrentIndex(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(tabs_.size())) return;
    if (currentIndex_ != idx)
    {
        currentIndex_ = idx;
        currentChanged.emit(idx);
        markDirty();
        WidgetApp::instance().requestLayout();
    }
}

Widget* TabLayout::currentWidget() const
{
    if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(tabs_.size()))
        return tabs_[currentIndex_].content;
    return nullptr;
}

void TabLayout::setTabLabel(int index, const std::string& label)
{
    if (index >= 0 && index < static_cast<int>(tabs_.size()))
    {
        tabs_[index].label = label;
        markDirty();
    }
}

const std::string& TabLayout::tabLabel(int index) const
{
    if (index >= 0 && index < static_cast<int>(tabs_.size()))
        return tabs_[index].label;
    return kEmptyTabLabel;
}

void TabLayout::computeRects(Rect& barRect, Rect& contentRect) const
{
    float s = tabBarSize_;

    // For Left/Right, compute bar width from widest tab label
    auto barWidth = [&]() -> float {
        const auto& t = Theme::instance();
        float maxW = 60.0f;
        for (auto& tab : tabs_)
        {
            float tw = tab.label.size() * t.fontSize * 0.55f + t.padding * 2;
            if (closable_) tw += 18.0f;
            if (tw > maxW) maxW = tw;
        }
        return maxW + 4.0f;  // 2px margin each side
    };

    switch (tabPosition_)
    {
    case TabPosition::Top:
        barRect     = {0, 0, rect_.w, s};
        contentRect = {0, s, rect_.w, rect_.h - s};
        break;
    case TabPosition::Bottom:
        contentRect = {0, 0, rect_.w, rect_.h - s};
        barRect     = {0, rect_.h - s, rect_.w, s};
        break;
    case TabPosition::Left:
    {
        float bw = barWidth();
        barRect     = {0, 0, bw, rect_.h};
        contentRect = {bw, 0, rect_.w - bw, rect_.h};
        break;
    }
    case TabPosition::Right:
    {
        float bw = barWidth();
        contentRect = {0, 0, rect_.w - bw, rect_.h};
        barRect     = {rect_.w - bw, 0, bw, rect_.h};
        break;
    }
    }
}

Rect TabLayout::tabRect(int index, const Rect& barRect) const
{
    const auto& t = Theme::instance();
    bool horiz = (tabPosition_ == TabPosition::Top || tabPosition_ == TabPosition::Bottom);

    if (horiz)
    {
        // Measure tab widths from font
        float x = barRect.x + 2.0f;
        for (int i = 0; i <= index && i < static_cast<int>(tabs_.size()); ++i)
        {
            float tw = tabs_[i].label.size() * t.fontSize * 0.55f + t.padding * 2;
            if (closable_) tw += 18.0f;  // space for X button
            if (tw < 60.0f) tw = 60.0f;

            if (i == index)
                return {x, barRect.y, tw, barRect.h};
            x += tw + 1.0f;
        }
    }
    else
    {
        // Vertical tabs
        float y = barRect.y + 2.0f;
        float tabH = tabBarSize_;
        for (int i = 0; i <= index && i < static_cast<int>(tabs_.size()); ++i)
        {
            if (i == index)
                return {barRect.x, y, barRect.w, tabH};
            y += tabH + 1.0f;
        }
    }
    return {};
}

Rect TabLayout::closeRect(const Rect& tabR) const
{
    float sz = 14.0f;
    bool horiz = (tabPosition_ == TabPosition::Top || tabPosition_ == TabPosition::Bottom);
    if (horiz)
        return {tabR.x + tabR.w - sz - 4.0f, tabR.y + (tabR.h - sz) * 0.5f, sz, sz};
    else
        return {tabR.x + (tabR.w - sz) * 0.5f, tabR.y + tabR.h - sz - 4.0f, sz, sz};
}

Widget::Vec2f TabLayout::sizeHint() const
{
    float w = 200, h = 200;
    for (auto& tab : tabs_)
    {
        if (!tab.content) continue;
        auto hint = tab.content->sizeHint();
        if (hint.x > w) w = hint.x;
        if (hint.y > h) h = hint.y;
    }

    bool horiz = (tabPosition_ == TabPosition::Top || tabPosition_ == TabPosition::Bottom);
    if (horiz)
        h += tabBarSize_;
    else
    {
        // Compute bar width from labels
        float maxW = 60.0f;
        const auto& t = Theme::instance();
        for (auto& tab : tabs_)
        {
            float tw = tab.label.size() * t.fontSize * 0.55f + t.padding * 2;
            if (closable_) tw += 18.0f;
            if (tw > maxW) maxW = tw;
        }
        w += maxW + 4.0f;
    }

    return {w, h};
}

void TabLayout::layout()
{
    Rect barR, contentR;
    computeRects(barR, contentR);

    for (int i = 0; i < static_cast<int>(tabs_.size()); ++i)
    {
        auto* c = tabs_[i].content;
        if (!c) continue;

        if (i == currentIndex_)
        {
            c->setVisible(true);
            c->setRect(contentR);
            c->layout();
        }
        else
        {
            c->setVisible(false);
        }
    }
}

void TabLayout::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    Rect barR, contentR;
    computeRects(barR, contentR);

    // Tab bar background
    Rect absBar = {abs.x + barR.x, abs.y + barR.y, barR.w, barR.h};
    Rect clippedBar;
    if (ctx.clipRectIntersect(absBar, clippedBar))
    {
        ctx.fill.SetColor(38, 40, 44, 255);
        ctx.fill.Rectangle(
            static_cast<int>(clippedBar.x), static_cast<int>(clippedBar.y),
            static_cast<int>(clippedBar.w), static_cast<int>(clippedBar.h), true);
    }

    // Draw each tab button
    bool horiz = (tabPosition_ == TabPosition::Top || tabPosition_ == TabPosition::Bottom);

    for (int i = 0; i < static_cast<int>(tabs_.size()); ++i)
    {
        Rect tr = tabRect(i, barR);
        Rect absTr = {abs.x + tr.x, abs.y + tr.y, tr.w, tr.h};
        Rect clippedTab;
        if (!ctx.clipRectIntersect(absTr, clippedTab)) continue;

        bool active = (i == currentIndex_);

        // Tab background
        Color bg = active ? Color(55, 58, 65, 255) : Color(42, 44, 50, 255);

        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fill.Rectangle(
            static_cast<int>(absTr.x), static_cast<int>(absTr.y),
            static_cast<int>(absTr.w), static_cast<int>(absTr.h), true);

        // Active indicator line
        if (active)
        {
            ctx.fill.SetColor(100, 160, 220, 255);
            if (tabPosition_ == TabPosition::Top)
                ctx.fill.Rectangle(static_cast<int>(absTr.x), static_cast<int>(absTr.y + absTr.h - 2),
                                    static_cast<int>(absTr.w), 2, true);
            else if (tabPosition_ == TabPosition::Bottom)
                ctx.fill.Rectangle(static_cast<int>(absTr.x), static_cast<int>(absTr.y),
                                    static_cast<int>(absTr.w), 2, true);
            else if (tabPosition_ == TabPosition::Left)
                ctx.fill.Rectangle(static_cast<int>(absTr.x + absTr.w - 2), static_cast<int>(absTr.y),
                                    2, static_cast<int>(absTr.h), true);
            else
                ctx.fill.Rectangle(static_cast<int>(absTr.x), static_cast<int>(absTr.y),
                                    2, static_cast<int>(absTr.h), true);
        }

        // Tab label
        ctx.font.SetFontSize(t.fontSize * 0.9f);
        ctx.font.SetColor(active ? t.textColor : t.textDisabled);
        ctx.font.SetBatch(&ctx.text);

        float asc   = ctx.font.GetAscender();
        float textX = horiz ? absTr.x + t.padding : absTr.x + 4.0f;
        float textY = absTr.y + (absTr.h + asc) * 0.5f;  // baseline-correct centering

        ctx.font.Print(tabs_[i].label.c_str(), textX, textY);

        // Close button (X)
        if (closable_)
        {
            Rect cr = closeRect(tr);
            Rect absCr = {abs.x + cr.x, abs.y + cr.y, cr.w, cr.h};

            // X lines
            Color xc = active ? Color(160, 160, 165, 200) : Color(100, 100, 105, 150);
            ctx.line.SetColor(xc.r, xc.g, xc.b, xc.a);
            float m = 3.0f;
            ctx.line.Line2D(
                static_cast<int>(absCr.x + m), static_cast<int>(absCr.y + m),
                static_cast<int>(absCr.x + absCr.w - m), static_cast<int>(absCr.y + absCr.h - m));
            ctx.line.Line2D(
                static_cast<int>(absCr.x + absCr.w - m), static_cast<int>(absCr.y + m),
                static_cast<int>(absCr.x + m), static_cast<int>(absCr.y + absCr.h - m));
        }
    }

    // Separator line between bar and content
    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    Rect absContent = {abs.x + contentR.x, abs.y + contentR.y, contentR.w, contentR.h};
    if (tabPosition_ == TabPosition::Top)
        ctx.line.Line2D(static_cast<int>(absBar.x), static_cast<int>(absBar.y + absBar.h),
                         static_cast<int>(absBar.x + absBar.w), static_cast<int>(absBar.y + absBar.h));
    else if (tabPosition_ == TabPosition::Bottom)
        ctx.line.Line2D(static_cast<int>(absBar.x), static_cast<int>(absBar.y),
                         static_cast<int>(absBar.x + absBar.w), static_cast<int>(absBar.y));
    else if (tabPosition_ == TabPosition::Left)
        ctx.line.Line2D(static_cast<int>(absBar.x + absBar.w), static_cast<int>(absBar.y),
                         static_cast<int>(absBar.x + absBar.w), static_cast<int>(absBar.y + absBar.h));
    else
        ctx.line.Line2D(static_cast<int>(absBar.x), static_cast<int>(absBar.y),
                         static_cast<int>(absBar.x), static_cast<int>(absBar.y + absBar.h));

    // Paint active content (clip to content area)
    ctx.pushClip(absContent);
    Widget* cur = currentWidget();
    if (cur && cur->isVisible())
        cur->paint(ctx);
    ctx.popClip();
}

void TabLayout::onMousePress(MouseEvent& e)
{
    if (!enabled_) return;

    Rect abs = absoluteRect();
    Rect barR, contentR;
    computeRects(barR, contentR);

    // Check if click is in the tab bar
    Rect absBar = {abs.x + barR.x, abs.y + barR.y, barR.w, barR.h};
    if (!absBar.contains(e.x, e.y))
    {
        // Click in content area - let children handle it
        return;
    }

    // Find which tab was clicked
    for (int i = 0; i < static_cast<int>(tabs_.size()); ++i)
    {
        Rect tr = tabRect(i, barR);
        Rect absTr = {abs.x + tr.x, abs.y + tr.y, tr.w, tr.h};

        if (!absTr.contains(e.x, e.y)) continue;

        // Check close button first
        if (closable_)
        {
            Rect cr = closeRect(tr);
            Rect absCr = {abs.x + cr.x, abs.y + cr.y, cr.w, cr.h};
            if (absCr.contains(e.x, e.y))
            {
                tabClosed.emit(i);
                removeTab(i);
                e.consumed = true;
                return;
            }
        }

        // Click on tab label → switch (skip if already active or only 1 tab)
        if (i == currentIndex_ || tabs_.size() <= 1)
        {
            e.consumed = true;
            return;
        }
        setCurrentIndex(i);
        e.consumed = true;
        return;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  AnchorLayout - children positioned by anchors relative to parent
// ═════════════════════════════════════════════════════════════════════════════

static const char* kAnchorKey = "__anchor_rule__";

void AnchorLayout::setAnchors(Widget* child, const AnchorRule& rule)
{
    child->setData(kAnchorKey, rule);
}

AnchorRule AnchorLayout::anchors(const Widget* child)
{
    if (child->hasData(kAnchorKey))
        return child->data<AnchorRule>(kAnchorKey);
    return {};
}

bool AnchorLayout::hasAnchors(const Widget* child)
{
    return child->hasData(kAnchorKey);
}

Widget::Vec2f AnchorLayout::sizeHint() const
{
    // AnchorLayout typically fills its parent; sizeHint = current rect or minimum
    float w = rect_.w > 0 ? rect_.w : 200.0f;
    float h = rect_.h > 0 ? rect_.h : 200.0f;
    return {w, h};
}

void AnchorLayout::layout()
{
    float pw = rect_.w - padding_.left - padding_.right;
    float ph = rect_.h - padding_.top  - padding_.bottom;
    if (pw < 0) pw = 0;
    if (ph < 0) ph = 0;

    for (auto* c : children_)
    {
        if (!c->isVisible()) continue;

        AnchorRule a;
        if (hasAnchors(c))
            a = anchors(c);
        else
        {
            // No anchors set: default fill
            c->setRect({padding_.left, padding_.top, pw, ph});
            c->layout();
            continue;
        }

        float x0 = padding_.left + a.left  * pw + a.leftOff;
        float x1 = padding_.left + a.right * pw + a.rightOff;
        float y0 = padding_.top  + a.top   * ph + a.topOff;
        float y1 = padding_.top  + a.bottom * ph + a.bottomOff;

        float cx = x0;
        float cy = y0;
        float cw = x1 - x0;
        float ch = y1 - y0;
        if (cw < 0) cw = 0;
        if (ch < 0) ch = 0;

        c->setRect({cx, cy, cw, ch});
        c->layout();
    }
}

void AnchorLayout::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    ctx.pushClip(abs);
    for (auto* c : children_)
        c->paint(ctx);
    ctx.popClip();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Carousel
// ═════════════════════════════════════════════════════════════════════════════

unsigned int Carousel::arrowTexId_ = 0;

Carousel::Carousel() = default;

void Carousel::addPageWidget(Widget* page)
{
    pages_.push_back(page);
    addChild(page);
    markDirty();
    WidgetApp::instance().requestLayout();
}

void Carousel::removePage(int index)
{
    if (index < 0 || index >= static_cast<int>(pages_.size())) return;
    Widget* p = pages_[index];
    pages_.erase(pages_.begin() + index);
    auto it = std::find(children_.begin(), children_.end(), p);
    if (it != children_.end()) children_.erase(it);
    delete p;
    if (currentPage_ >= static_cast<int>(pages_.size()))
        currentPage_ = static_cast<int>(pages_.size()) - 1;
    if (currentPage_ < 0) currentPage_ = 0;
    markDirty();
}

Rect Carousel::contentRect() const
{
    float dy = (showDots_ && !pages_.empty()) ? dotsHeight() : 0.0f;
    return {0, 0, rect_.w, rect_.h - dy};
}

Rect Carousel::arrowRect(bool rightSide) const
{
    Rect cr = contentRect();
    float aw = 36.0f;
    float ah = 60.0f;
    float ay = (cr.h - ah) * 0.5f;
    if (rightSide)
        return {cr.w - aw - 4.0f, ay, aw, ah};
    else
        return {4.0f, ay, aw, ah};
}

void Carousel::startAnim(int from, int to, int dir)
{
    animFrom_     = from;
    animDir_      = dir;
    animProgress_ = 0.0f;
    animating_    = true;
    currentPage_  = to;
    markDirty();
}

void Carousel::setCurrentPage(int idx)
{
    if (pages_.empty()) return;
    int n = static_cast<int>(pages_.size());
    idx = idx < 0 ? 0 : (idx >= n ? n - 1 : idx);
    if (idx == currentPage_) return;
    int dir = (idx > currentPage_) ? 1 : -1;
    startAnim(currentPage_, idx, dir);
    pageChanged.emit(currentPage_);
}

void Carousel::next()
{
    if (pages_.empty()) return;
    int n = static_cast<int>(pages_.size());
    int next = currentPage_ + 1;
    if (next >= n)
    {
        if (!loop_) return;
        next = 0;
    }
    startAnim(currentPage_, next, 1);
    pageChanged.emit(currentPage_);
}

void Carousel::prev()
{
    if (pages_.empty()) return;
    int n = static_cast<int>(pages_.size());
    int prev = currentPage_ - 1;
    if (prev < 0)
    {
        if (!loop_) return;
        prev = n - 1;
    }
    startAnim(currentPage_, prev, -1);
    pageChanged.emit(currentPage_);
}

Widget::Vec2f Carousel::sizeHint() const
{
    return {rect_.w > 0 ? rect_.w : 200.0f,
            rect_.h > 0 ? rect_.h : 200.0f};
}

void Carousel::layout()
{
    Rect cr = contentRect();
    for (auto* p : pages_)
    {
        p->setRect({0, 0, cr.w, cr.h});
        p->layout();
    }
}

void Carousel::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& theme = Theme::instance();

    // Advance animation
    if (animating_)
    {
        float dt = WidgetApp::instance().deltaTime();
        float step = (animDuration_ > 0.0f) ? (dt / animDuration_) : 1.0f;
        animProgress_ += step;
        if (animProgress_ >= 1.0f)
        {
            animProgress_ = 1.0f;
            animating_    = false;
            animFrom_     = -1;
        }
        markDirty();
    }

    // Auto-play
    if (autoPlay_ && !animating_ && !pages_.empty())
    {
        autoTimer_ += WidgetApp::instance().deltaTime();
        if (autoTimer_ >= autoInterval_)
        {
            autoTimer_ = 0.0f;
            next();
        }
        markDirty();
    }

    Rect cr  = contentRect();
    Rect cra = {abs.x, abs.y, cr.w, cr.h};    ctx.pushClip(cra);

    // Draw current page (and sliding-out page during animation)
    if (!pages_.empty())
    {
        float slideW = cr.w;

        if (animating_ && animFrom_ >= 0 && animFrom_ < static_cast<int>(pages_.size()))
        {
            float t    = applyEasing(easeType_, animProgress_);
            float curOff = -animDir_ * slideW * (1.0f - t);
            float oldOff =  animDir_ * slideW * t;

            Widget* oldPage = pages_[animFrom_];
            Widget* curPage = pages_[currentPage_];

            float savedOldX = oldPage->rect().x;
            float savedCurX = curPage->rect().x;

            oldPage->setRect({oldOff, 0, cr.w, cr.h});
            curPage->setRect({curOff, 0, cr.w, cr.h});

            oldPage->paint(ctx);
            curPage->paint(ctx);

            oldPage->setRect({savedOldX, 0, cr.w, cr.h});
            curPage->setRect({savedCurX, 0, cr.w, cr.h});
        }
        else
        {
            Widget* p = pages_[currentPage_];
            p->setRect({0, 0, cr.w, cr.h});
            p->paint(ctx);
        }
    }

    ctx.popClip();

    // Arrows
    if (showArrows_)
    {
        auto paintArrow = [&](bool right)
        {
            Rect ar = arrowRect(right);
            ar.x += abs.x;
            ar.y += abs.y;
            bool hov = (right ? hoveredArrow_ == 1 : hoveredArrow_ == -1);
            uint8_t a = hov ? 200 : 120;
            ctx.fill.SetColor(20, 20, 30, a);
            ctx.fillRoundedRect(ar.x, ar.y, ar.w, ar.h, 4.0f, 4);
            ctx.line.SetColor(200, 200, 200, a);
            // Draw triangle arrow
            float cx_ = ar.x + ar.w * 0.5f;
            float cy_ = ar.y + ar.h * 0.5f;
            float s   = 8.0f;
            if (right)
                ctx.fillTriangle(cx_ - s * 0.5f, cy_ - s, cx_ + s * 0.5f, cy_, cx_ - s * 0.5f, cy_ + s);
            else
                ctx.fillTriangle(cx_ + s * 0.5f, cy_ - s, cx_ - s * 0.5f, cy_, cx_ + s * 0.5f, cy_ + s);
        };
        paintArrow(false);
        paintArrow(true);
    }

    // Dots
    if (showDots_ && !pages_.empty())
    {
        int n         = static_cast<int>(pages_.size());
        float dotR    = 5.0f;
        float dotGap  = 14.0f;
        float totalW  = n * dotGap - (dotGap - dotR * 2.0f);
        float startX  = abs.x + (rect_.w - totalW) * 0.5f + dotR;
        float dotY    = abs.y + cr.h + dotsHeight() * 0.5f;

        for (int i = 0; i < n; ++i)
        {
            float dx = startX + i * dotGap;
            if (i == currentPage_)
            {
                ctx.fill.SetColor(220, 220, 240, 255);
                ctx.fillCircle(dx, dotY, dotR);
            }
            else
            {
                ctx.fill.SetColor(100, 100, 120, 200);
                ctx.fillCircle(dx, dotY, dotR - 1.5f);
            }
        }
    }
}

void Carousel::onMousePress(MouseEvent& e)
{
    if (pages_.empty()) return;

    Rect abs = absoluteRect();
    float lx = e.x - abs.x;
    float ly = e.y - abs.y;

    // Arrow clicks
    if (showArrows_)
    {
        if (arrowRect(false).contains(lx, ly))
        {
            prev();
            e.consumed = true;
            return;
        }
        if (arrowRect(true).contains(lx, ly))
        {
            next();
            e.consumed = true;
            return;
        }
    }

    // Dot clicks
    if (showDots_)
    {
        Rect cr = contentRect();
        int n        = static_cast<int>(pages_.size());
        float dotR   = 5.0f;
        float dotGap = 14.0f;
        float totalW = n * dotGap - (dotGap - dotR * 2.0f);
        float startX = (rect_.w - totalW) * 0.5f + dotR;
        float dotY   = cr.h + dotsHeight() * 0.5f;

        for (int i = 0; i < n; ++i)
        {
            float dx = startX + i * dotGap;
            float distX = lx - dx;
            float distY = ly - dotY;
            if (distX * distX + distY * distY <= (dotR + 4.0f) * (dotR + 4.0f))
            {
                setCurrentPage(i);
                e.consumed = true;
                return;
            }
        }
    }
}

void Carousel::onMouseMove(MouseEvent& e)
{
    int oldHover = hoveredArrow_;
    hoveredArrow_ = 0;
    bool overInteractive = false;

    Rect abs = absoluteRect();
    float lx = e.x - abs.x;
    float ly = e.y - abs.y;

    if (showArrows_)
    {
        if (arrowRect(false).contains(lx, ly))
        {
            hoveredArrow_ = -1;
            overInteractive = true;
        }
        else if (arrowRect(true).contains(lx, ly))
        {
            hoveredArrow_ = 1;
            overInteractive = true;
        }
    }

    // Check dot hover
    if (!overInteractive && showDots_ && !pages_.empty())
    {
        Rect cr = contentRect();
        int n        = static_cast<int>(pages_.size());
        float dotR   = 5.0f;
        float dotGap = 14.0f;
        float totalW = n * dotGap - (dotGap - dotR * 2.0f);
        float startX = (rect_.w - totalW) * 0.5f + dotR;
        float dotY   = cr.h + dotsHeight() * 0.5f;

        for (int i = 0; i < n; ++i)
        {
            float dx = startX + i * dotGap;
            float distX = lx - dx;
            float distY = ly - dotY;
            if (distX * distX + distY * distY <= (dotR + 4.0f) * (dotR + 4.0f))
            {
                overInteractive = true;
                break;
            }
        }
    }

    setCursor(overInteractive ? CursorType::Hand : CursorType::Arrow);
    if (hoveredArrow_ != oldHover) markDirty();
}

void Carousel::onMouseLeave()
{
    hoveredArrow_ = 0;
    setCursor(CursorType::Arrow);
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  SlidePanel
// ═════════════════════════════════════════════════════════════════════════════

SlidePanel::SlidePanel(Side side, float width)
    : side_(side), panelWidth_(width)
{
    visible_ = false;   // starts closed — hitTest skips it
}

void SlidePanel::open()
{
    if (open_ && !animating_) return;
    open_      = true;
    animating_ = true;
    visible_   = true;   // make hittable
    // animProgress_ keeps its current value so reverse is smooth
    openChanged.emit(true);
    WidgetApp::instance().requestLayout();
    markDirty();
}

void SlidePanel::close()
{
    if (!open_ && !animating_) return;
    open_      = false;
    animating_ = true;
    openChanged.emit(false);
    markDirty();
}

void SlidePanel::toggle()
{
    if (open_) close(); else open();
}

Widget::Vec2f SlidePanel::sizeHint() const
{
    return {rect_.w > 0 ? rect_.w : 300.0f,
            rect_.h > 0 ? rect_.h : 200.0f};
}

void SlidePanel::layout()
{
    // The SlidePanel itself fills its parent rect (it's an overlay).
    // Its first child is the drawer content, sized to panelWidth_ x full height.
    if (children_.empty()) return;

    const float drawerW = std::max(1.0f, std::min(panelWidth_, rect_.w));
    Widget* content = children_[0];
    content->setRect({0, 0, drawerW, rect_.h});
    content->layout();
}

void SlidePanel::paint(PaintContext& ctx)
{
    if (!visible_) return;

    // Advance animation
    if (animating_)
    {
        float dt   = WidgetApp::instance().deltaTime();
        float step = (animDuration_ > 0.0f) ? (dt / animDuration_) : 1.0f;

        if (open_)
        {
            animProgress_ += step;
            if (animProgress_ >= 1.0f) { animProgress_ = 1.0f; animating_ = false; }
        }
        else
        {
            animProgress_ -= step;
            if (animProgress_ <= 0.0f) {
                animProgress_ = 0.0f;
                animating_ = false;
                visible_ = false;   // fully closed — hitTest skips us
            }
        }
        markDirty();
    }

    // Nothing to draw when fully closed
    if (animProgress_ <= 0.0f && !animating_) return;

    Rect abs = absoluteRect();
    const float drawerW = std::max(1.0f, std::min(panelWidth_, abs.w));
    float t = applyEasing(easeType_, animProgress_);

    // Scrim (dim background)
    if (showScrim_)
    {
        uint8_t alpha = static_cast<uint8_t>(120.0f * t);
        ctx.fill.SetColor(0, 0, 0, alpha);
        ctx.fill.Rectangle(abs.x, abs.y, abs.w, abs.h, true);
    }

    // Drawer offset
    float drawerX;
    if (side_ == Left)
        drawerX = abs.x + (t - 1.0f) * drawerW;   // slides from left
    else
        drawerX = abs.x + abs.w - t * drawerW;     // slides from right

    Rect drawerRect = {drawerX, abs.y, drawerW, abs.h};
    ctx.pushClip(drawerRect);

    // Draw panel background
    const auto& theme = Theme::instance();
    ctx.fill.SetColor(theme.drawerBg.r, theme.drawerBg.g, theme.drawerBg.b, theme.drawerBg.a);
    ctx.fill.Rectangle(drawerRect.x, drawerRect.y, drawerRect.w, drawerRect.h, true);

    // Draw content child offset
    if (!children_.empty())
    {
        Widget* content = children_[0];
        // Temporarily shift child for painting
        float savedX = content->rect().x;
        content->setRect({drawerX - abs.x, 0, drawerW, abs.h});
        content->paint(ctx);
        content->setRect({savedX, 0, drawerW, abs.h});
    }

    // Right/left border line
    ctx.line.SetColor(theme.drawerBorder.r, theme.drawerBorder.g, theme.drawerBorder.b, theme.drawerBorder.a);
    if (side_ == Left)
        ctx.line.Line2D(drawerRect.x + drawerRect.w, drawerRect.y,
                        drawerRect.x + drawerRect.w, drawerRect.y + drawerRect.h);
    else
        ctx.line.Line2D(drawerRect.x, drawerRect.y,
                        drawerRect.x, drawerRect.y + drawerRect.h);

    ctx.popClip();
}

void SlidePanel::onMousePress(MouseEvent& e)
{
    if (animProgress_ <= 0.0f) return;  // fully closed, ignore

    Rect abs = absoluteRect();
    const float drawerW = std::max(1.0f, std::min(panelWidth_, abs.w));
    float t = applyEasing(easeType_, animProgress_);

    // Compute drawer rect in screen coords
    float drawerX;
    if (side_ == Left)
        drawerX = abs.x + (t - 1.0f) * drawerW;
    else
        drawerX = abs.x + abs.w - t * drawerW;

    Rect drawerRect = {drawerX, abs.y, drawerW, abs.h};

    // If click is outside drawer, close it
    if (e.x < drawerRect.x || e.x > drawerRect.x + drawerRect.w ||
        e.y < drawerRect.y || e.y > drawerRect.y + drawerRect.h)
    {
        close();
        e.consumed = true;
        return;
    }

    // Otherwise bubble to children normally
}
