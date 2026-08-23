#pragma once

#include "Widget.hpp"
#include "Signal.hpp"
#include "IconAtlas.hpp"   // IconId for fallback icons
#include "BuGUI.hpp"       // TextureHandle
#include <string>
#include <vector>

// ═════════════════════════════════════════════════════════════════════════════
//  ToolBar — Delphi-style icon toolbar with sprite-sheet grid
//
//  Load a sprite sheet (grid of icons), then add buttons by cell index.
//  Supports: push buttons, toggle buttons, separators, spacers.
//  Items can be reordered by dragging.
//
//  Usage:
//      auto* tb = parent->createChild<ToolBar>();
//      tb->setImageGrid(&myImage, 24, 24);   // 24×24 px cells
//      int b0 = tb->addButton(0, "New");      // cell 0
//      int b1 = tb->addButton(1, "Open");     // cell 1
//      tb->addSeparator();
//      int b2 = tb->addToggle(5, "Bold");     // toggle, cell 5
//      tb->buttonClicked.connect([](int id) { ... });
//
//  The sprite sheet is uploaded as a GL texture on first paint
//  (requires a valid GL context at that point).
// ═════════════════════════════════════════════════════════════════════════════

namespace BuGUI
{
class ToolBar : public Widget
{
public:
    ToolBar();
    ~ToolBar() override;

    // ── Sprite sheet ──────────────────────────────────────────────────────
    /// @brief Set the icon sprite sheet image (takes ownership).
    void setImageGrid(BuGUI::BuImage* img, int cellW, int cellH);

    /// @brief Load a sprite sheet from a file path.
    bool loadImageGrid(const std::string& path, int cellW, int cellH);

    /// @brief Set texture manually (if uploaded externally).
    void setTexture(BuGUI::TextureHandle tex, int texW, int texH, int cellW, int cellH);

    // Alternative: use existing IconAtlas icons (no sprite sheet needed).
    // Buttons added with addButton(IconId, ...) will draw from the atlas.

    /// @brief Get cell width in pixels.
    int cellWidth()  const { return cellW_; }
    /// @brief Get cell height in pixels.
    int cellHeight() const { return cellH_; }
    /// @brief Get total number of cells in the grid.
    int cellCount()  const;

    // ── Item management ───────────────────────────────────────────────────
    enum class ItemType { Button, Toggle, Separator, Spacer };

    /// @brief Add a push button by grid cell index.
    int addButton(int iconIndex, const std::string& tooltip = "");

    /// @brief Add a push button using an IconAtlas icon.
    int addButton(IconId icon, const std::string& tooltip = "");

    /// @brief Add a toggle button by grid cell index.
    int addToggle(int iconIndex, const std::string& tooltip = "", bool checked = false);
    /// @brief Add a toggle button using an IconAtlas icon.
    int addToggle(IconId icon, const std::string& tooltip = "", bool checked = false);

    /// @brief Add a visual separator line.
    int addSeparator();

    /// @brief Add a flexible spacer.
    int addSpacer();

    /// @brief Remove an item by id.
    void removeItem(int id);

    /// @brief Enable or disable an item.
    void setItemEnabled(int id, bool enabled);
    /// @brief Check if an item is enabled.
    bool isItemEnabled(int id) const;

    /// @brief Set the checked state of a toggle item.
    void setItemChecked(int id, bool checked);
    /// @brief Check if a toggle item is checked.
    bool isItemChecked(int id) const;

    /// @brief Show or hide an item.
    void setItemVisible(int id, bool visible);
    /// @brief Check if an item is visible.
    bool isItemVisible(int id) const;

    /// @brief Set an item's tooltip text.
    void setItemTooltip(int id, const std::string& tip);

    /// @brief Set whether an item can be reordered by drag.
    void setItemDraggable(int id, bool draggable);
    /// @brief Check if an item is draggable.
    bool isItemDraggable(int id) const;

    /// @brief Set the button size in pixels.
    void  setButtonSize(float sz) { btnSize_ = sz; markDirty(); }
    /// @brief Get the button size.
    float buttonSize() const      { return btnSize_; }

    /// @brief Set padding inside each button.
    void  setIconPadding(float p) { iconPad_ = p; markDirty(); }
    /// @brief Get the icon padding.
    float iconPadding() const     { return iconPad_; }

    /// @brief Set spacing between items.
    void  setSpacing(float s)     { spacing_ = s; markDirty(); }
    /// @brief Get spacing between items.
    float spacing() const         { return spacing_; }

    /// @brief Enable drag reordering of items.
    void  setDraggable(bool d)    { draggable_ = d; }
    /// @brief Check if drag reordering is enabled.
    bool  isDraggable() const     { return draggable_; }

    /// @brief Emitted on push button click (item id).
    Signal<int>       buttonClicked;
    /// @brief Emitted on toggle change (item id, checked).
    Signal<int, bool> toggleChanged;

    // ── Widget overrides ──────────────────────────────────────────────────
    Vec2f sizeHint() const override;
    void  layout() override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;

private:
    struct Item {
        int         id        = 0;
        ItemType    type      = ItemType::Button;
        int         iconIndex = -1;   // grid cell index (-1 = use iconId)
        IconId      iconId    = IconId::None;
        std::string tooltip;
        bool        enabled   = true;
        bool        visible   = true;
        bool        checked   = false; // for Toggle
        bool        draggable = true;
        bool        hovered   = false;
        bool        pressed   = false;
        Rect        rect;             // computed layout rect (local coords)
    };

    Item* findItem(int id);
    const Item* findItem(int id) const;

    // Sprite sheet
    BuGUI::BuImage*      gridImg_    = nullptr;   // owned
    BuGUI::TextureHandle  gridTex_   {};          // uploaded on first paint
    int                   gridTexW_  = 0;
    int                   gridTexH_  = 0;
    int                   cellW_     = 24;
    int                   cellH_     = 24;
    int                   gridCols_  = 0;
    bool                  texDirty_  = true;

    void ensureTexture(PaintContext& ctx);
    void drawGridIcon(PaintContext& ctx, int cellIdx, float x, float y, float sz);

    // Items
    std::vector<Item> items_;
    int nextId_ = 1;

    // Visual
    float btnSize_  = 28.f;
    float iconPad_  = 4.f;
    float spacing_  = 2.f;
    bool  draggable_ = true;

    // Drag reorder
    struct DragState {
        bool  active    = false;
        int   itemId    = -1;
        int   origIdx   = -1;
        float startX    = 0;
        float curX      = 0;
    } drag_;

    int hitItem(float absX, float absY) const;  // returns item id or -1
};

} // namespace BuGUI
