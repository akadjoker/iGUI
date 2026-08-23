#pragma once

#include "Widget.hpp"
#include "Signal.hpp"
#include <vector>
#include <string>


namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  ThumbnailGrid — Grid of thumbnail cards with label + selection
//
//  Each item is a coloured rectangle (placeholder for future image support)
//  arranged in a responsive grid that wraps to fit the available width.
//
//  Usage:
//    auto* grid = parent->createChild<ThumbnailGrid>();
//    grid->addItem("photo_001.jpg", Color(180, 60, 60, 255));
//    grid->addItem("photo_002.jpg", Color(60, 180, 60, 255));
//    grid->onItemClicked.connect([](int idx) { ... });
//    grid->onItemDoubleClicked.connect([](int idx) { ... });
// ═════════════════════════════════════════════════════════════════════════════

struct ThumbnailItem {
    std::string          label;
    Color                color = Color(80, 90, 110, 255);
    BuGUI::TextureHandle tex   = {};
    bool                 selected = false;
};

class ThumbnailGrid : public Widget
{
public:
    ThumbnailGrid();

    /// @brief Add a thumbnail item with label and color.
    int  addItem(const std::string& label, const Color& color = Color(80, 90, 110, 255));
    /// @brief Remove a thumbnail by index.
    void removeItem(int idx);
    /// @brief Remove all thumbnails.
    void clearItems();

    /// @brief Set the texture for an existing item.
    void setItemTexture(int idx, BuGUI::TextureHandle tex);

    /// @brief Get the number of items.
    int  itemCount() const { return static_cast<int>(items_.size()); }
    /// @brief Get a const item reference.
    const ThumbnailItem& item(int idx) const { return items_[idx]; }

    /// @brief Select an item by index.
    void setSelected(int idx);
    /// @brief Get the selected item index.
    int  selected() const { return selected_; }

    /// @brief Set the thumbnail cell size.
    void setThumbSize(float w, float h) { thumbW_ = w; thumbH_ = h; markDirty(); }
    /// @brief Set the spacing between thumbnails.
    void setSpacing(float s)            { spacing_ = s; markDirty(); }
    /// @brief Show or hide item labels.
    void setShowLabels(bool s)          { showLabels_ = s; markDirty(); }
    /// @brief Set the background color.
    void setBgColor(const Color& c)     { bgColor_ = c; markDirty(); }

    /// @brief Emitted when an item is clicked.
    Signal<int> onItemClicked;
    /// @brief Emitted when an item is double-clicked.
    Signal<int> onItemDoubleClicked;

    Vec2f sizeHint() const override { return {300, 200}; }
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;

private:
    std::vector<ThumbnailItem> items_;
    int   selected_ = -1;
    float thumbW_   = 80.f;
    float thumbH_   = 80.f;
    float spacing_  = 8.f;
    bool  showLabels_ = true;
    Color bgColor_  = Color(24, 26, 30, 255);

    int hitTest(float mx, float my) const;
};

} // namespace BuGUI
