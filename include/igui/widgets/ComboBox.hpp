#pragma once

#include "Widget.hpp"
#include "Theme.hpp"
#include <string>
#include <vector>

// ═════════════════════════════════════════════════════════════════════════════
//  ComboBox - dropdown selector
//    auto* cb = parent->createChild<ComboBox>();
//    cb->addItem("Alpha"); cb->addItem("Beta");
//    cb->selectionChanged.connect([](int idx) { ... });
// ═════════════════════════════════════════════════════════════════════════════

namespace BuGUI
{
class ComboBox : public Widget
{
public:
    ComboBox();

    /// @brief Add an item to the dropdown list.
    void addItem(const std::string& text);
    /// @brief Replace all items with the given list.
    void setItems(const std::vector<std::string>& texts) { clear(); for (auto& t : texts) addItem(t); }
    /// @brief Insert an item at the given index.
    void insertItem(int index, const std::string& text);
    /// @brief Remove the item at the given index.
    void removeItem(int index);
    /// @brief Remove all items from the combo box.
    void clear();

    /// @brief Get the number of items.
    int  itemCount() const { return static_cast<int>(items_.size()); }
    /// @brief Get the text of the item at the given index.
    const std::string& itemText(int index) const;
    /// @brief Set the text of the item at the given index.
    void setItemText(int index, const std::string& text);

    /// @brief Get the currently selected index.
    int  selectedIndex() const { return selectedIndex_; }
    /// @brief Set the selected item by index.
    void setSelectedIndex(int idx);

    /// @brief Get the text of the currently selected item.
    const std::string& currentText() const;

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
    std::vector<std::string> items_;
    int   selectedIndex_ = -1;
    int   maxVisible_    = 8;
    bool  open_          = false;

    friend class ComboPopup_;
};

} // namespace BuGUI
