#include "pch.hpp"
#include "AssetBrowser.hpp"
#include "WidgetApp.hpp"

namespace {
    inline float setupFont(PaintContext& ctx, const Color& color, float size)
    {
        ctx.font.SetFontSize(size);
        ctx.font.SetBatch(&ctx.text);
        ctx.font.SetColor(color);
        return ctx.font.GetAscender();
    }
}

AssetBrowser::AssetBrowser() {}

// ── Items ─────────────────────────────────────────────────────────────────

void AssetBrowser::addItem(const AssetItem& item)
{
    items_.push_back(item);
    markDirty();
}

void AssetBrowser::clearItems()
{
    items_.clear();
    selected_ = -1;
    scrollY_  = 0.f;
    markDirty();
}

// ── Type helpers ──────────────────────────────────────────────────────────

IconId AssetBrowser::typeToIcon(AssetType t) const
{
    switch (t) {
        case AssetType::Folder:   return IconId::Folder;
        case AssetType::Image:    return IconId::FileImage;
        case AssetType::Audio:    return IconId::File;
        case AssetType::Mesh:     return IconId::File;
        case AssetType::Script:   return IconId::FileCode;
        case AssetType::Material: return IconId::Star;
        case AssetType::Scene:    return IconId::Home;
        default:                  return IconId::File;
    }
}

Color AssetBrowser::typeToColor(AssetType t) const
{
    switch (t) {
        case AssetType::Folder:   return Color(200, 160,  60, 255);
        case AssetType::Image:    return Color( 60, 160, 210, 255);
        case AssetType::Audio:    return Color(100, 200, 140, 255);
        case AssetType::Mesh:     return Color(180, 120, 220, 255);
        case AssetType::Script:   return Color(220, 180,  60, 255);
        case AssetType::Material: return Color( 60, 200, 200, 255);
        case AssetType::Scene:    return Color(240, 100,  80, 255);
        default:                  return Color( 90,  95, 105, 255);
    }
}

// ── Layout ────────────────────────────────────────────────────────────────

AssetBrowser::GridLayout AssetBrowser::computeGrid(const Rect& b) const
{
    GridLayout g;
    const float pad    = 8.f;
    const float labelH = 16.f;
    const float gap    = 6.f;
    g.cellW  = thumbSize_ + 8.f;
    g.cellH  = thumbSize_ + labelH + 6.f;
    g.cols   = std::max(1, static_cast<int>((b.w - pad * 2 + gap) / (g.cellW + gap)));
    g.startX = b.x + pad;
    g.startY = b.y + 28.f;
    int rows = (static_cast<int>(items_.size()) + g.cols - 1) / g.cols;
    g.totalH = rows * (g.cellH + gap) + pad;
    return g;
}

int AssetBrowser::hitItem(float mx, float my) const
{
    const Rect b = absoluteRect();
    if (my < b.y + 27.f || my > b.y + b.h) return -1;

    if (viewMode_ == ViewMode::List) {
        const float row = 22.f;
        float y0 = b.y + 28.f;
        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            float y = y0 + i * row - scrollY_;
            if (my >= y && my < y + row && mx >= b.x && mx < b.x + b.w)
                return i;
        }
        return -1;
    }

    const float gap = 6.f;
    auto g = computeGrid(b);
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        int col = i % g.cols;
        int row = i / g.cols;
        float cx = g.startX + col * (g.cellW + gap);
        float cy = g.startY + row * (g.cellH + gap) - scrollY_;
        if (mx >= cx && mx < cx + g.cellW && my >= cy && my < cy + g.cellH)
            return i;
    }
    return -1;
}

// ── Paint ─────────────────────────────────────────────────────────────────

void AssetBrowser::paint(PaintContext& ctx)
{
    if (!visible_) return;
    const Rect b = absoluteRect();
    if (ctx.isClipped(b)) return;

    timeAcc_ += WidgetApp::instance().deltaTime();

    ctx.pushClip(b);

    const auto& th = Theme::instance();

    // Background
    ctx.fill.SetColor(22, 24, 28, 255);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    // ── Top bar: path + view toggle ──────────────────────────────────────
    ctx.fill.SetColor(30, 33, 38, 255);
    ctx.fillRect(b.x, b.y, b.w, 26.f);
    ctx.line.SetColor(45, 48, 55, 255);
    ctx.drawLine(b.x, b.y + 26.f, b.x + b.w, b.y + 26.f);

    float asc = setupFont(ctx, Color(160, 165, 175, 255), th.fontSize * 0.85f);
    ctx.font.Print(path_.c_str(), b.x + 8.f, b.y + 7.f + asc);

    // View mode toggle buttons (top-right)
    float bx = b.x + b.w - 52.f;
    float by = b.y + 3.f;
    bool isGrid = (viewMode_ == ViewMode::Grid);

    ctx.fill.SetColor(isGrid ? 60 : 40, isGrid ? 120 : 44, isGrid ? 200 : 52, 255);
    ctx.fillRect(bx, by, 22.f, 20.f);
    if (ctx.icons) ctx.drawIcon(IconId::ViewGrid, bx + 3.f, by + 3.f, 14.f);

    ctx.fill.SetColor(!isGrid ? 60 : 40, !isGrid ? 120 : 44, !isGrid ? 200 : 52, 255);
    ctx.fillRect(bx + 26.f, by, 22.f, 20.f);
    if (ctx.icons) ctx.drawIcon(IconId::ViewList, bx + 29.f, by + 3.f, 14.f);

    // ── Clip content area ────────────────────────────────────────────────
    Rect ca = {b.x, b.y + 27.f, b.w, b.h - 27.f};
    ctx.pushClip(ca);

    if (viewMode_ == ViewMode::Grid) {
        auto g = computeGrid(b);
        const float gap = 6.f;

        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            const auto& it = items_[i];
            int col = i % g.cols;
            int row = i / g.cols;
            float cx = g.startX + col * (g.cellW + gap);
            float cy = g.startY + row * (g.cellH + gap) - scrollY_;

            if (cy + g.cellH < ca.y || cy > ca.y + ca.h) continue;

            bool sel = (selected_ == i);

            Color tc = (it.thumbColor.r != 60) ? it.thumbColor : typeToColor(it.type);
            ctx.fill.SetColor(sel ? tc.r + 20 : tc.r * 40 / 100,
                              sel ? tc.g + 20 : tc.g * 40 / 100,
                              sel ? tc.b + 20 : tc.b * 40 / 100, 255);
            ctx.fillRect(cx, cy, g.cellW, thumbSize_);

            ctx.fill.SetColor(tc.r, tc.g, tc.b, sel ? 255 : 180);
            float ts = thumbSize_ * 0.5f;
            ctx.fillRect(cx + (g.cellW - ts) * 0.5f, cy + (thumbSize_ - ts) * 0.5f, ts, ts);

            if (ctx.icons) {
                float is = std::min(32.f, thumbSize_ * 0.5f);
                ctx.drawIcon(typeToIcon(it.type),
                             cx + (g.cellW - is) * 0.5f,
                             cy + (thumbSize_ - is) * 0.5f, is,
                             Color(255, 255, 255, 200));
            }

            if (sel) {
                ctx.line.SetColor(100, 160, 240, 255);
                ctx.drawLine(cx,          cy,             cx + g.cellW, cy);
                ctx.drawLine(cx,          cy + thumbSize_, cx + g.cellW, cy + thumbSize_);
                ctx.drawLine(cx,          cy,             cx,           cy + thumbSize_);
                ctx.drawLine(cx + g.cellW, cy,            cx + g.cellW, cy + thumbSize_);
            }

            // Label
            float ly = cy + thumbSize_ + 2.f;
            ctx.pushClip({cx, ly, g.cellW, 16.f});
            asc = setupFont(ctx, Color(sel ? 220 : 170, sel ? 225 : 175, sel ? 235 : 185, 255),
                            th.fontSize * 0.8f);
            ctx.font.Print(it.name.c_str(), cx + 2.f, ly + asc);
            ctx.popClip();
        }

    } else {
        // ── List view ────────────────────────────────────────────────────
        const float rowH = 22.f;
        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            const auto& it = items_[i];
            float ry = ca.y + i * rowH - scrollY_;
            if (ry + rowH < ca.y || ry > ca.y + ca.h) continue;

            bool sel  = (selected_ == i);
            bool even = (i % 2 == 0);

            ctx.fill.SetColor(sel ? 50  : (even ? 26 : 30),
                              sel ? 80  : (even ? 28 : 33),
                              sel ? 130 : (even ? 34 : 38), 255);
            ctx.fillRect(b.x, ry, b.w, rowH);

            Color ic = typeToColor(it.type);
            if (ctx.icons) {
                ctx.drawIcon(typeToIcon(it.type), b.x + 6.f, ry + 3.f, 16.f,
                             Color(ic.r, ic.g, ic.b, 220));
            } else {
                ctx.fill.SetColor(ic.r, ic.g, ic.b, 200);
                ctx.fillRect(b.x + 6.f, ry + 5.f, 12.f, 12.f);
            }

            asc = setupFont(ctx, Color(sel ? 230 : 180, sel ? 235 : 185, sel ? 245 : 195, 255),
                            th.fontSize * 0.85f);
            ctx.font.Print(it.name.c_str(), b.x + 26.f, ry + 5.f + asc);

            if (sel) {
                ctx.line.SetColor(80, 140, 220, 180);
                ctx.drawLine(b.x, ry, b.x + b.w - 1, ry);
                ctx.drawLine(b.x, ry + rowH - 1, b.x + b.w - 1, ry + rowH - 1);
            }
        }
    }

    ctx.popClip();  // content area

    // ── Scrollbar ────────────────────────────────────────────────────────
    float totalH = 0.f;
    if (viewMode_ == ViewMode::Grid) {
        auto g = computeGrid(b);
        totalH = g.totalH;
    } else {
        totalH = items_.size() * 22.f;
    }
    float viewH = ca.h;
    if (totalH > viewH) {
        float ratio = viewH / totalH;
        float sbH   = std::max(20.f, viewH * ratio);
        float sbY   = ca.y + (scrollY_ / (totalH - viewH)) * (viewH - sbH);
        ctx.fill.SetColor(60, 65, 75, 200);
        ctx.fillRect(b.x + b.w - 6.f, sbY, 4.f, sbH);
    }

    ctx.popClip();
}

// ── Mouse ─────────────────────────────────────────────────────────────────

void AssetBrowser::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;
    const Rect b = absoluteRect();

    // View toggle buttons
    float bx = b.x + b.w - 52.f;
    float by = b.y + 3.f;
    if (e.y >= by && e.y < by + 20.f) {
        if (e.x >= bx && e.x < bx + 22.f)          { setViewMode(ViewMode::Grid); e.consumed = true; return; }
        if (e.x >= bx + 26.f && e.x < bx + 48.f)   { setViewMode(ViewMode::List); e.consumed = true; return; }
    }

    int idx = hitItem(e.x, e.y);
    if (idx < 0) return;

    selected_ = idx;
    onSelect.emit(items_[idx]);

    // Double-click detection using accumulated time
    float now = timeAcc_;
    if (lastClick_ == idx && (now - lastClickT_) < 0.5f) {
        onOpen.emit(items_[idx]);
        lastClick_ = -1;
    } else {
        lastClick_  = idx;
        lastClickT_ = now;
    }

    markDirty();
    e.consumed = true;
}

void AssetBrowser::onMouseScroll(MouseEvent& e)
{
    const Rect b = absoluteRect();
    float totalH = 0.f;
    if (viewMode_ == ViewMode::Grid) {
        auto g = computeGrid(b);
        totalH = g.totalH;
    } else {
        totalH = items_.size() * 22.f;
    }
    float viewH    = b.h - 28.f;
    float maxScroll = std::max(0.f, totalH - viewH);
    scrollY_ = std::clamp(scrollY_ - e.scrollY * 24.f, 0.f, maxScroll);
    markDirty();
    e.consumed = true;
}
