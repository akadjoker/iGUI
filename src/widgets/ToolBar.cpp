#include "pch.hpp"
#include "ToolBar.hpp"
#include "WidgetApp.hpp"
#include <algorithm>
#include <cmath>

// ═════════════════════════════════════════════════════════════════════════════
//  ToolBar
// ═════════════════════════════════════════════════════════════════════════════

ToolBar::ToolBar()
{
    cursor_ = CursorType::Arrow;
}

ToolBar::~ToolBar()
{
    if (gridTex_)
        WidgetApp::instance().destroyTexture(gridTex_);
    delete gridImg_;
}

// ── Sprite sheet ─────────────────────────────────────────────────────────────

void ToolBar::setImageGrid(BuGUI::BuImage* img, int cellW, int cellH)
{
    delete gridImg_;
    gridImg_  = img;
    cellW_    = cellW;
    cellH_    = cellH;
    gridCols_ = img ? (img->width / cellW) : 0;
    texDirty_ = true;
    markDirty();
}

bool ToolBar::loadImageGrid(const std::string& path, int cellW, int cellH)
{
    auto* img = new BuGUI::BuImage();
    if (!img->Load(path.c_str())) {
        delete img;
        return false;
    }
    setImageGrid(img, cellW, cellH);
    return true;
}

void ToolBar::setTexture(BuGUI::TextureHandle tex, int texW, int texH, int cellW, int cellH)
{
    gridTex_  = tex;
    gridTexW_ = texW;
    gridTexH_ = texH;
    cellW_    = cellW;
    cellH_    = cellH;
    gridCols_ = (texW > 0 && cellW > 0) ? (texW / cellW) : 0;
    texDirty_ = false;
    markDirty();
}

int ToolBar::cellCount() const
{
    if (!gridImg_ || cellW_ <= 0 || cellH_ <= 0) return 0;
    int cols = gridImg_->width  / cellW_;
    int rows = gridImg_->height / cellH_;
    return cols * rows;
}

// ── Item management ──────────────────────────────────────────────────────────

int ToolBar::addButton(int iconIndex, const std::string& tooltip)
{
    Item it;
    it.id        = nextId_++;
    it.type      = ItemType::Button;
    it.iconIndex = iconIndex;
    it.tooltip   = tooltip;
    items_.push_back(std::move(it));
    markDirty();
    return items_.back().id;
}

int ToolBar::addButton(IconId icon, const std::string& tooltip)
{
    Item it;
    it.id      = nextId_++;
    it.type    = ItemType::Button;
    it.iconId  = icon;
    it.tooltip = tooltip;
    items_.push_back(std::move(it));
    markDirty();
    return items_.back().id;
}

int ToolBar::addToggle(int iconIndex, const std::string& tooltip, bool checked)
{
    Item it;
    it.id        = nextId_++;
    it.type      = ItemType::Toggle;
    it.iconIndex = iconIndex;
    it.tooltip   = tooltip;
    it.checked   = checked;
    items_.push_back(std::move(it));
    markDirty();
    return items_.back().id;
}

int ToolBar::addToggle(IconId icon, const std::string& tooltip, bool checked)
{
    Item it;
    it.id      = nextId_++;
    it.type    = ItemType::Toggle;
    it.iconId  = icon;
    it.tooltip = tooltip;
    it.checked = checked;
    items_.push_back(std::move(it));
    markDirty();
    return items_.back().id;
}

int ToolBar::addSeparator()
{
    Item it;
    it.id   = nextId_++;
    it.type = ItemType::Separator;
    items_.push_back(std::move(it));
    markDirty();
    return items_.back().id;
}

int ToolBar::addSpacer()
{
    Item it;
    it.id   = nextId_++;
    it.type = ItemType::Spacer;
    items_.push_back(std::move(it));
    markDirty();
    return items_.back().id;
}

void ToolBar::removeItem(int id)
{
    items_.erase(
        std::remove_if(items_.begin(), items_.end(),
                        [id](const Item& it) { return it.id == id; }),
        items_.end());
    markDirty();
}

// ── Item state ───────────────────────────────────────────────────────────────

ToolBar::Item* ToolBar::findItem(int id)
{
    for (auto& it : items_)
        if (it.id == id) return &it;
    return nullptr;
}

const ToolBar::Item* ToolBar::findItem(int id) const
{
    for (auto& it : items_)
        if (it.id == id) return &it;
    return nullptr;
}

void ToolBar::setItemEnabled(int id, bool enabled)
{
    if (auto* it = findItem(id)) { it->enabled = enabled; markDirty(); }
}
bool ToolBar::isItemEnabled(int id) const
{
    auto* it = findItem(id);
    return it ? it->enabled : false;
}

void ToolBar::setItemChecked(int id, bool checked)
{
    if (auto* it = findItem(id)) { it->checked = checked; markDirty(); }
}
bool ToolBar::isItemChecked(int id) const
{
    auto* it = findItem(id);
    return it ? it->checked : false;
}

void ToolBar::setItemVisible(int id, bool visible)
{
    if (auto* it = findItem(id)) { it->visible = visible; markDirty(); }
}
bool ToolBar::isItemVisible(int id) const
{
    auto* it = findItem(id);
    return it ? it->visible : true;
}

void ToolBar::setItemTooltip(int id, const std::string& tip)
{
    if (auto* it = findItem(id)) it->tooltip = tip;
}

void ToolBar::setItemDraggable(int id, bool draggable)
{
    if (auto* it = findItem(id)) it->draggable = draggable;
}

bool ToolBar::isItemDraggable(int id) const
{
    if (auto* it = findItem(id)) return it->draggable;
    return false;
}

// ── Layout ───────────────────────────────────────────────────────────────────

Widget::Vec2f ToolBar::sizeHint() const
{
    float w = 0;
    for (const auto& it : items_) {
        if (!it.visible) continue;
        switch (it.type) {
        case ItemType::Button:
        case ItemType::Toggle:
            w += btnSize_ + spacing_;
            break;
        case ItemType::Separator:
            w += 8.f + spacing_;
            break;
        case ItemType::Spacer:
            w += 16.f;  // minimum spacer width
            break;
        }
    }
    return {w, btnSize_ + 4.f};
}

void ToolBar::layout()
{
    // Compute rects for all items (local coordinates)
    float x = 2.f;
    float y = 2.f;
    float h = rect_.h - 4.f;
    float itemH = std::min(h, btnSize_);

    // Count spacers for flex distribution
    int spacerCount = 0;
    float fixedW = 0;
    for (const auto& it : items_) {
        if (!it.visible) continue;
        switch (it.type) {
        case ItemType::Button:
        case ItemType::Toggle:
            fixedW += btnSize_ + spacing_;
            break;
        case ItemType::Separator:
            fixedW += 8.f + spacing_;
            break;
        case ItemType::Spacer:
            spacerCount++;
            break;
        }
    }

    float totalSpace = rect_.w - 4.f;
    float spacerW = (spacerCount > 0) ? std::max(4.f, (totalSpace - fixedW) / spacerCount) : 0.f;

    for (auto& it : items_) {
        if (!it.visible) {
            it.rect = {0, 0, 0, 0};
            continue;
        }
        float iy = y + (h - itemH) * 0.5f;
        switch (it.type) {
        case ItemType::Button:
        case ItemType::Toggle:
            it.rect = {x, iy, btnSize_, itemH};
            x += btnSize_ + spacing_;
            break;
        case ItemType::Separator:
            it.rect = {x, iy, 8.f, itemH};
            x += 8.f + spacing_;
            break;
        case ItemType::Spacer:
            it.rect = {x, iy, spacerW, itemH};
            x += spacerW;
            break;
        }
    }

    Widget::layout();
}

// ── Texture upload ───────────────────────────────────────────────────────────

void ToolBar::ensureTexture(PaintContext& /*ctx*/)
{
    if (!texDirty_ || !gridImg_) return;
    texDirty_ = false;

    // Ensure the image is RGBA (4 components) for GPU upload
    BuGUI::BuImage* rgba = gridImg_;
    bool ownsRgba = false;
    if (gridImg_->components != 4) {
        rgba = gridImg_->ConvertToRGBA();
        if (!rgba) return;
        ownsRgba = true;
    }

    // Destroy old texture if any
    if (gridTex_) {
        WidgetApp::instance().destroyTexture(gridTex_);
        gridTex_ = {};
    }

    // Upload via WidgetApp's backend-registered texture service
    gridTex_  = WidgetApp::instance().uploadTexture(rgba->pixels, rgba->width, rgba->height);
    gridTexW_ = rgba->width;
    gridTexH_ = rgba->height;

    if (ownsRgba) delete rgba;
}

void ToolBar::drawGridIcon(PaintContext& ctx, int cellIdx, float x, float y, float sz)
{
    if (!gridTex_ || gridCols_ <= 0) return;

    int col = cellIdx % gridCols_;
    int row = cellIdx / gridCols_;
    float srcX = static_cast<float>(col * cellW_);
    float srcY = static_cast<float>(row * cellH_);

    float tw = static_cast<float>(gridTexW_);
    float th = static_cast<float>(gridTexH_);
    if (tw <= 0 || th <= 0) return;

    BuGUI::Rect uv{
        srcX / tw,
        srcY / th,
        static_cast<float>(cellW_) / tw,
        static_cast<float>(cellH_) / th
    };
    ctx.drawImage(gridTex_, {x, y, sz, sz}, uv);
}

// ── Hit test ─────────────────────────────────────────────────────────────────

int ToolBar::hitItem(float absX, float absY) const
{
    Rect abs = absoluteRect();
    float lx = absX - abs.x;
    float ly = absY - abs.y;
    for (const auto& it : items_) {
        if (!it.visible) continue;
        if (it.type == ItemType::Spacer) continue;
        if (lx >= it.rect.x && lx <= it.rect.x + it.rect.w &&
            ly >= it.rect.y && ly <= it.rect.y + it.rect.h)
            return it.id;
    }
    return -1;
}

// ── Paint ────────────────────────────────────────────────────────────────────

void ToolBar::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    ensureTexture(ctx);

    const auto& t = Theme::instance();

    // Toolbar background
    Rect clipped;
    if (ctx.clipRectIntersect(abs, clipped)) {
        ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, t.panelColor.a);
        ctx.fill.Rectangle(
            static_cast<int>(clipped.x), static_cast<int>(clipped.y),
            static_cast<int>(clipped.w), static_cast<int>(clipped.h), true);
    }

    // Draw items
    for (int idx = 0; idx < static_cast<int>(items_.size()); ++idx) {
        const auto& it = items_[idx];
        if (!it.visible) continue;

        Rect ir = {abs.x + it.rect.x, abs.y + it.rect.y, it.rect.w, it.rect.h};

        // Skip if being dragged (we'll draw it at the drag position)
        if (drag_.active && it.id == drag_.itemId) continue;

        switch (it.type) {
        case ItemType::Button:
        case ItemType::Toggle:
        {
            Rect cr;
            if (!ctx.clipRectIntersect(ir, cr)) break;

            // Background
            Color bg;
            if (it.type == ItemType::Toggle && it.checked)
                bg = Color(60, 100, 160, 255);
            else if (it.pressed)
                bg = t.buttonPressed;
            else if (it.hovered)
                bg = t.buttonHover;
            else
                bg = Color(0, 0, 0, 0);  // transparent when idle

            if (!it.enabled) bg = Color(0, 0, 0, 0);

            if (bg.a > 0) {
                ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
                ctx.fill.RoundedRectangle(ir.x, ir.y, ir.w, ir.h,
                                          t.borderRadius, 6, true);
            }

            // Hover border
            if (it.hovered && it.enabled) {
                ctx.line.SetColor(t.buttonBorder.r, t.buttonBorder.g, t.buttonBorder.b, 120);
                ctx.line.RoundedRectangle(ir.x, ir.y, ir.w, ir.h,
                                          t.borderRadius, 6, false);
            }

            // Icon
            float iconSz = btnSize_ - iconPad_ * 2;
            float ix = ir.x + (ir.w - iconSz) * 0.5f;
            float iy = ir.y + (ir.h - iconSz) * 0.5f;

            Color iconTint = it.enabled ? Color(220, 220, 225, 255) : Color(100, 100, 110, 255);

            if (it.iconIndex >= 0 && gridTex_) {
                drawGridIcon(ctx, it.iconIndex, ix, iy, iconSz);
            } else if (it.iconId != IconId::None) {
                ctx.drawIcon(it.iconId, ix, iy, iconSz, iconTint);
            }

            // Toggle check indicator (small dot in bottom-right)
            if (it.type == ItemType::Toggle && it.checked) {
                ctx.fill.SetColor(100, 200, 255, 255);
                float dotR = 2.5f;
                ctx.fill.Circle(ir.x + ir.w - 5.f, ir.y + ir.h - 5.f, dotR, true);
            }
            break;
        }
        case ItemType::Separator:
        {
            float sx = ir.x + ir.w * 0.5f;
            float sy1 = ir.y + 3.f;
            float sy2 = ir.y + ir.h - 3.f;
            ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 150);
            ctx.line.Line2D(static_cast<int>(sx), static_cast<int>(sy1),
                            static_cast<int>(sx), static_cast<int>(sy2));
            break;
        }
        case ItemType::Spacer:
            break;
        }
    }

    // Draw drag ghost
    if (drag_.active) {
        auto* it = findItem(drag_.itemId);
        if (it) {
            float dx = drag_.curX - drag_.startX;
            Rect ir = {abs.x + it->rect.x + dx, abs.y + it->rect.y,
                       it->rect.w, it->rect.h};

            // Semi-transparent background
            ctx.fill.SetColor(80, 120, 180, 150);
            ctx.fill.RoundedRectangle(ir.x, ir.y, ir.w, ir.h,
                                      t.borderRadius, 6, true);

            // Icon
            float iconSz = btnSize_ - iconPad_ * 2;
            float ix = ir.x + (ir.w - iconSz) * 0.5f;
            float iy = ir.y + (ir.h - iconSz) * 0.5f;
            if (it->iconIndex >= 0 && gridTex_)
                drawGridIcon(ctx, it->iconIndex, ix, iy, iconSz);
            else if (it->iconId != IconId::None)
                ctx.drawIcon(it->iconId, ix, iy, iconSz);
        }
    }

    // Bottom border line
    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.line.Line2D(static_cast<int>(abs.x), static_cast<int>(abs.y + abs.h - 1),
                    static_cast<int>(abs.x + abs.w), static_cast<int>(abs.y + abs.h - 1));
}

// ── Mouse events ─────────────────────────────────────────────────────────────

void ToolBar::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;

    int id = hitItem(e.x, e.y);
    if (id < 0) return;

    auto* it = findItem(id);
    if (!it || !it->enabled) return;

    if (it->type == ItemType::Separator) {
        // Separators are not clickable, but they're drag handles
        // for reordering adjacent items
        return;
    }

    it->pressed = true;
    markDirty();

    // Start drag for reorder
    if (draggable_ && it->draggable && (it->type == ItemType::Button || it->type == ItemType::Toggle)) {
        drag_.active  = false;  // will activate on move
        drag_.itemId  = id;
        drag_.startX  = e.x;
        drag_.curX    = e.x;
        // Find original index
        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            if (items_[i].id == id) { drag_.origIdx = i; break; }
        }
    }

    e.consumed = true;
}

void ToolBar::onMouseRelease(MouseEvent& e)
{
    if (e.button != 0) return;

    // Check if any item was pressed and released inside
    for (auto& it : items_) {
        if (!it.pressed) continue;
        it.pressed = false;

        // Only fire click if not dragging
        if (!drag_.active && it.enabled) {
            Rect abs = absoluteRect();
            Rect ir = {abs.x + it.rect.x, abs.y + it.rect.y, it.rect.w, it.rect.h};
            if (ir.contains(e.x, e.y)) {
                if (it.type == ItemType::Button) {
                    buttonClicked.emit(it.id);
                } else if (it.type == ItemType::Toggle) {
                    it.checked = !it.checked;
                    toggleChanged.emit(it.id, it.checked);
                }
            }
        }
    }

    // Finish drag
    if (drag_.active || drag_.itemId >= 0) {
        drag_.active = false;
        drag_.itemId = -1;
        markDirty();
        layout();
    }

    e.consumed = true;
}

void ToolBar::onMouseMove(MouseEvent& e)
{
    Rect abs = absoluteRect();

    // Update hover state
    for (auto& it : items_) {
        if (!it.visible || it.type == ItemType::Spacer) {
            it.hovered = false;
            continue;
        }
        Rect ir = {abs.x + it.rect.x, abs.y + it.rect.y, it.rect.w, it.rect.h};
        bool wasHovered = it.hovered;
        it.hovered = ir.contains(e.x, e.y) && it.enabled;
        if (wasHovered != it.hovered) markDirty();
    }

    // Drag reorder
    if (drag_.itemId >= 0) {
        float dx = e.x - drag_.startX;
        if (!drag_.active && std::abs(dx) > 4.f)
            drag_.active = true;

        if (drag_.active) {
            drag_.curX = e.x;
            markDirty();

            // Check if we should swap with neighbour
            auto* dragIt = findItem(drag_.itemId);
            if (dragIt) {
                // dragCenterX: where the dragged item's center currently appears (local coords)
                float dragCenterX = dragIt->rect.x + dragIt->rect.w * 0.5f + dx;

                // Find current index
                int curIdx = -1;
                for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
                    if (items_[i].id == drag_.itemId) { curIdx = i; break; }
                }

                bool swapped = false;
                float oldRectX = dragIt->rect.x;

                // Try swap left
                if (curIdx > 0) {
                    auto& left = items_[curIdx - 1];
                    if (left.visible && left.draggable &&
                        left.type != ItemType::Separator &&
                        left.type != ItemType::Spacer &&
                        dragCenterX < left.rect.x + left.rect.w * 0.5f)
                    {
                        std::swap(items_[curIdx], items_[curIdx - 1]);
                        swapped = true;
                    }
                }

                // Try swap right (re-find index after possible left swap)
                if (!swapped && curIdx >= 0 && curIdx < static_cast<int>(items_.size()) - 1) {
                    auto& right = items_[curIdx + 1];
                    if (right.visible && right.draggable &&
                        right.type != ItemType::Separator &&
                        right.type != ItemType::Spacer &&
                        dragCenterX > right.rect.x + right.rect.w * 0.5f)
                    {
                        std::swap(items_[curIdx], items_[curIdx + 1]);
                        swapped = true;
                    }
                }

                if (swapped) {
                    layout();
                    // Adjust startX so ghost doesn't jump after layout changes rect.x
                    auto* movedIt = findItem(drag_.itemId);
                    if (movedIt) {
                        drag_.startX += (movedIt->rect.x - oldRectX);
                    }
                }
            }
        }
    }
}
