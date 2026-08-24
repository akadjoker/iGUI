#pragma once

#include "Widget.hpp"
#include "Theme.hpp"
#include <igui/widgets/String.hpp>
#include <ct/vector.hpp>

// ═════════════════════════════════════════════════════════════════════════════
//  ComboBox - dropdown selector
//    auto* cb = parent->createChild<ComboBox>();
//    cb->addItem("Alpha"); cb->addItem("Beta");
//    cb->selectionChanged.connect([](int idx) { ... });
// ═════════════════════════════════════════════════════════════════════════════

namespace ig { namespace retained
{
class ComboBox : public Widget
{
public:
    ComboBox();

    /// @brief Add an item to the dropdown list.
    void addItem(const String& text);
    /// @brief Replace all items with the given list.
    void setItems(const ct::Vector<String>& texts) { clear(); for (auto& t : texts) addItem(t); }
    /// @brief Insert an item at the given index.
    void insertItem(int index, const String& text);
    /// @brief Remove the item at the given index.
    void removeItem(int index);
    /// @brief Remove all items from the combo box.
    void clear();

    /// @brief Get the number of items.
    int  itemCount() const { return static_cast<int>(items_.size()); }
    /// @brief Get the text of the item at the given index.
    const String& itemText(int index) const;
    /// @brief Set the text of the item at the given index.
    void setItemText(int index, const String& text);

    /// @brief Get the currently selected index.
    int  selectedIndex() const { return selectedIndex_; }
    /// @brief Set the selected item by index.
    void setSelectedIndex(int idx);

    /// @brief Get the text of the currently selected item.
    const String& currentText() const;

    /// @brief Set the maximum number of visible dropdown items.
    void setMaxVisible(int n) { maxVisible_ = n; }
    /// @brief Get the maximum number of visible dropdown items.
    int  maxVisible() const   { return maxVisible_; }

    /// @brief Emitted when the selected item changes.
    Signal<int> selectionChanged;

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;

    /// @brief Open the dropdown popup.
    void openDropdown();
    /// @brief Close the dropdown popup.
    void closeDropdown();

private:
    ct::Vector<String> items_;
    int   selectedIndex_ = -1;
    int   maxVisible_    = 8;
    bool  open_          = false;

    friend class ComboPopup_;
};

} // namespace retained
} // namespace ig
