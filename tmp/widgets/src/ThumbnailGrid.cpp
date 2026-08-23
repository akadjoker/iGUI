#include "ThumbnailGrid.hpp"
#include "Theme.hpp"
#include <cmath>
#include <algorithm>

namespace {
    inline float setupFont(PaintContext& ctx, const Color& color, float size)
    {
        ctx.font.SetFontSize(size);
        ctx.font.SetBatch(&ctx.text);
        ctx.font.SetColor(color);
        return ctx.font.GetAscender();
    }
}

ThumbnailGrid::ThumbnailGrid() {}

int ThumbnailGrid::addItem(const std::string& label, const Color& color)
{
    items_.push_back({label, color, {}, false});
    markDirty();
    return static_cast<int>(items_.size()) - 1;
}

void ThumbnailGrid::setItemTexture(int idx, BuGUI::TextureHandle tex)
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
    markDirty();
}

void ThumbnailGrid::clearItems()
{
    items_.clear();
    selected_ = -1;
    markDirty();
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
    const float labelH = showLabels_ ? 16.f : 0.f;
    const float cellW  = thumbW_ + spacing_;
    const float cellH  = thumbH_ + labelH + spacing_;
    const int   cols   = std::max(1, static_cast<int>((b.w - spacing_) / cellW));
    const float padX   = spacing_;
    const float padY   = spacing_;

    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        int col = i % cols;
        int row = i / cols;
        float x = b.x + padX + col * cellW;
        float y = b.y + padY + row * cellH;
        if (mx >= x && mx <= x + thumbW_ && my >= y && my <= y + thumbH_ + labelH)
            return i;
    }
    return -1;
}

void ThumbnailGrid::paint(PaintContext& ctx)
{
    const Rect b = absoluteRect();
    ctx.pushClip(b);

    // Background
    ctx.fill.SetColor(bgColor_.r, bgColor_.g, bgColor_.b, bgColor_.a);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    const float labelH = showLabels_ ? 16.f : 0.f;
    const float cellW  = thumbW_ + spacing_;
    const float cellH  = thumbH_ + labelH + spacing_;
    const int   cols   = std::max(1, static_cast<int>((b.w - spacing_) / cellW));
    const float padX   = spacing_;
    const float padY   = spacing_;

    float asc = 0;
    if (showLabels_)
        asc = setupFont(ctx, Color(200, 200, 205, 255), 9.f);

    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        const auto& item = items_[i];
        int col = i % cols;
        int row = i / cols;
        float x = b.x + padX + col * cellW;
        float y = b.y + padY + row * cellH;

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
