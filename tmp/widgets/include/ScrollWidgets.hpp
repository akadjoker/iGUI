#pragma once

#include "Widget.hpp"
#include "Signal.hpp"
#include "Theme.hpp"
#include <string>
#include <vector>
#include <functional>

// ═════════════════════════════════════════════════════════════════════════════
//  ScrollBar
//
//  Thin bar (horizontal or vertical) for navigating a virtual range.
//  The thumb size is proportional to viewSize / contentSize.
//  Emits scrolled(value) when the user drags or clicks in the track.
// ═════════════════════════════════════════════════════════════════════════════

namespace BuGUI
{
enum class ScrollBarOrientation { Horizontal, Vertical };

class ScrollBar : public Widget
{
public:
    explicit ScrollBar(ScrollBarOrientation orientation = ScrollBarOrientation::Vertical);

    /// @brief Set the total content size.
    void setContentSize(float cs)  { contentSize_ = cs; clampScroll(); markDirty(); }
    /// @brief Set the visible viewport size.
    void setViewSize(float vs)     { viewSize_    = vs; clampScroll(); markDirty(); }
    /// @brief Set the scroll position.
    void setValue(float v)         { scrollValue_ = v; clampScroll(); markDirty(); }
    /// @brief Get the current scroll position.
    float value() const            { return scrollValue_; }
    /// @brief Get the maximum scroll value.
    float maxValue() const         { return max2(0.0f, contentSize_ - viewSize_); }
    /// @brief Check if scrolling is needed (content > view).
    bool  needed() const           { return contentSize_ > viewSize_ + 0.5f; }

    Widget::Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;

    /// @brief Emitted when scroll value changes.
    Signal<float> scrolled;

    static constexpr float kBarThickness = 8.0f;
    static constexpr float kMinThumbLen  = 20.0f;

private:
    void clampScroll();
    Rect thumbRect() const;

    ScrollBarOrientation orientation_;
    float contentSize_ = 100.0f;
    float viewSize_    = 50.0f;
    float scrollValue_ = 0.0f;

    bool  dragging_    = false;
    float dragStart_   = 0.0f;
    float dragOrigin_  = 0.0f;
};

// ═════════════════════════════════════════════════════════════════════════════
//  ScrollView
//
//  A clipping container with optional vertical (and/or horizontal) scroll.
//  Usage:
//    auto* sv = parent->createChild<ScrollView>();
//    auto* col = sv->setContent<BoxLayout>(LayoutDir::Vertical);
//    col->createChild<Label>("item");
// ═════════════════════════════════════════════════════════════════════════════

class ScrollView : public Widget
{
public:
    ScrollView();

    // Set the single content widget (takes ownership). Returns it for chaining.
    template <typename T, typename... Args>
    T* setContent(Args&&... args)
    {
        auto* w = createChild<T>(std::forward<Args>(args)...);
        contentWidget_ = w;
        return w;
    }

    /// @brief Set the horizontal scroll offset.
    void setScrollX(float v);
    /// @brief Set the vertical scroll offset.
    void setScrollY(float v);
    /// @brief Get the horizontal scroll offset.
    float scrollX() const { return scrollX_; }
    /// @brief Get the vertical scroll offset.
    float scrollY() const { return scrollY_; }

    // Widget overrides
    Vec2f scrollOffset() const override { return {scrollX_, scrollY_}; }
    void  layout() override;
    void  paint(PaintContext& ctx) override;
    void  onMouseScroll(MouseEvent& e) override;

private:
    void updateBars();

    Widget*    contentWidget_ = nullptr;
    ScrollBar* vbar_          = nullptr;
    ScrollBar* hbar_          = nullptr;
    float      scrollX_       = 0.0f;
    float      scrollY_       = 0.0f;
    float      contentW_      = 0.0f;
    float      contentH_      = 0.0f;

    static constexpr float kBarGap = 2.0f;
};

// ═════════════════════════════════════════════════════════════════════════════
//  ListBox
//
//  Single-column list with optional string items and selection.
//  Has built-in ScrollBar when items exceed the visible area.
// ═════════════════════════════════════════════════════════════════════════════

class ListBox : public Widget
{
public:
    ListBox() = default;

    /// @brief Add a string item to the list.
    void addItem(const std::string& text);
    /// @brief Remove an item by index.
    void removeItem(int index);
    /// @brief Remove all items.
    void clearItems();
    /// @brief Get the number of items.
    int  itemCount() const { return static_cast<int>(items_.size()); }
    /// @brief Get the text of an item.
    const std::string& itemText(int index) const;

    /// @brief Get the selected item index (-1 if none).
    int  selectedIndex() const { return selectedIdx_; }
    /// @brief Set the selected item by index.
    void setSelectedIndex(int idx);
    /// @brief Deselect the current item.
    void clearSelection();

    void layout() override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;

    /// @brief Emitted when the selected item changes.
    Signal<int> selectionChanged;  // -1 = deselected

private:
    void clampScroll();
    float itemHeight() const;
    int   itemAtY(float absY) const;

    std::vector<std::string> items_;
    int   selectedIdx_  = -1;
    float scrollOffset_ = 0.0f;
    ScrollBar* vbar_    = nullptr;
};

// ═════════════════════════════════════════════════════════════════════════════
//  ListWidget
//
//  Like ListBox but each row can be an arbitrary widget subtree.
//  The caller creates row widgets via addRow<T>(...) which returns the root
//  widget of that row. Clicking a row selects it (highlights the row).
// ═════════════════════════════════════════════════════════════════════════════

class ListWidget : public Widget
{
public:
    ListWidget() = default;

    // Add a new row — creates a child widget of type T inside the list.
    // The widget is the "row root"; it will be sized to fill the row width.
    template <typename T, typename... Args>
    T* addRow(Args&&... args)
    {
        int idx = static_cast<int>(rows_.size());
        auto* w = createChild<T>(std::forward<Args>(args)...);
        rows_.push_back(w);
        w->setScrollable(true);
        // Forward clicks from any descendant to select the row
        connectRowPress(w, idx);
        markDirty();
        return static_cast<T*>(w);
    }

    /// @brief Remove a row by index.
    void removeRow(int index);
    /// @brief Remove all rows.
    void clearRows();
    /// @brief Get the number of rows.
    int  rowCount() const { return static_cast<int>(rows_.size()); }

    /// @brief Get the selected row index (-1 if none).
    int  selectedIndex() const { return selectedIdx_; }
    /// @brief Set the selected row by index.
    void setSelectedIndex(int idx);
    /// @brief Deselect the current row.
    void clearSelection();

    void layout() override;
    void paint(PaintContext& ctx) override;
    void onMouseScroll(MouseEvent& e) override;
    void onMousePress(MouseEvent& e) override;

    Signal<int> selectionChanged;

private:
    void clampScroll();
    void connectRowPress(Widget* w, int idx);
    int  rowAtY(float absY) const;
    float rowH() const;

    std::vector<Widget*> rows_;
    int         selectedIdx_  = -1;
    float       scrollOffset_ = 0.0f;
    ScrollBar*  vbar_         = nullptr;

    static constexpr float kRowPad = 4.0f;
};

} // namespace BuGUI
