#pragma once

#include "Widget.hpp"
#include "Signal.hpp"
#include <ct/function.hpp>
#include <ct/vector.hpp>
#include <igui/widgets/String.hpp>


namespace ig { namespace retained
{
class ScrollBar;

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
    String          label;
    Color                color = Color(80, 90, 110, 255);
    ig::retained::TextureHandle tex   = {};
    bool                 selected = false;
    float                texW  = 0.f;   // with a size, the image keeps its aspect in the cell
    float                texH  = 0.f;
    IconId               icon  = IconId::None;   // drawn over the colour when there is no image
};

class ThumbnailGrid : public Widget
{
public:
    IG_WIDGET_CLASS(ThumbnailGrid, Widget);

    ThumbnailGrid();

    /// @brief Add a thumbnail item with label and color.
    int  addItem(const String& label, const Color& color = Color(80, 90, 110, 255));
    /// @brief Remove a thumbnail by index.
    void removeItem(int idx);
    /// @brief Remove all thumbnails.
    void clearItems();

    /// @brief Set the texture for an existing item.
    void setItemTexture(int idx, ig::retained::TextureHandle tex);
    /// @brief Set the texture with its size, so the image keeps its aspect in the cell.
    void setItemTexture(int idx, ig::retained::TextureHandle tex, float width, float height);
    /// @brief Set the icon drawn in the middle of an item without an image.
    void setItemIcon(int idx, IconId icon);
    /// @brief Let items be dragged: the provider gives the payload for an item. The
    ///        drag goes through WidgetApp, so any widget whose acceptsDrop takes the
    ///        payload's type can receive it. Unset (the default), nothing is dragged.
    void setDragPayload(ct::Function<DragPayload(int)> provider) { dragPayload_ = std::move(provider); }

    /// @brief Get the number of items.
    int  itemCount() const { return static_cast<int>(items_.size()); }
    /// @brief Get a const item reference.
    const ThumbnailItem& item(int idx) const { return items_[idx]; }

    /// @brief Select an item by index.
    void setSelected(int idx);
    /// @brief Get the selected item index.
    int  selected() const { return selected_; }

    /// @brief Set the thumbnail cell size.
    void setThumbSize(float w, float h)
    {
        thumbW_ = w;
        thumbH_ = h;
        syncScroll();
    }
    /// @brief Set the spacing between thumbnails.
    void setSpacing(float s)
    {
        spacing_ = s;
        syncScroll();
    }
    /// @brief Show or hide item labels.
    void setShowLabels(bool s)
    {
        showLabels_ = s;
        syncScroll();
    }
    /// @brief Set the background color.
    void setBgColor(const Color& c)     { bgColor_ = c; markDirty(); }

    /// @brief Scroll the grid to @p y pixels from the top, kept within the content.
    void setScrollY(float y);
    /// @brief Get the vertical scroll offset in pixels.
    float scrollY() const
    {
        return scrollY_;
    }

    /// @brief Emitted when the selected item changes (-1 = none).
    Signal<int> selectionChanged;
    /// @brief Emitted when an item is clicked.
    Signal<int> onItemClicked;
    /// @brief Emitted when an item is double-clicked.
    Signal<int> onItemDoubleClicked;

    Vec2f sizeHint() const override { return {300, 200}; }
    void layout() override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onKeyPress(KeyEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    bool        isDragSource() const override;
    DragPayload onDragBegin() override;

private:
    ct::Function<DragPayload(int)> dragPayload_;
    int hover_ = -1;
    int pressed_ = -1;
    ct::Vector<ThumbnailItem> items_;
    int   selected_ = -1;
    float thumbW_   = 80.f;
    float thumbH_   = 80.f;
    float spacing_  = 8.f;
    bool  showLabels_ = true;
    Color bgColor_  = Color(24, 26, 30, 255);
    ScrollBar* vbar_ = nullptr;
    float scrollY_ = 0.f;

    // The grid as the current size lays it out: the columns leave room for the
    // scroll bar when the rows do not fit, and the scroll is kept in range.
    struct Metrics
    {
        float labelH, cellW, cellH, viewW, contentH, scroll;
        int cols;
        bool bar;
    };
    Metrics metrics() const;
    void syncScroll();
    void scrollToItem(int idx);
    int hitTest(float mx, float my) const;
};

} // namespace retained
} // namespace ig
