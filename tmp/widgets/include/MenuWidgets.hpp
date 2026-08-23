#pragma once

#include "Widget.hpp"
#include "Theme.hpp"
#include <string>
#include <vector>
#include <functional>


namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  MenuAction — a single item in a Menu
//    auto* act = menu->addAction("Open");
//    act->setShortcut("Ctrl+O");
//    act->triggered.connect([]() { /* ... */ });
// ═════════════════════════════════════════════════════════════════════════════

class Menu;

class MenuAction
{
public:
    /// @brief Get the action display text.
    const std::string& text()      const { return text_; }
    /// @brief Get the keyboard shortcut string.
    const std::string& shortcut()  const { return shortcut_; }
    /// @brief Check if the action is enabled.
    bool  isEnabled()   const { return enabled_; }
    /// @brief Check if the action is checkable.
    bool  isCheckable() const { return checkable_; }
    /// @brief Check if the action is currently checked.
    bool  isChecked()   const { return checked_; }
    /// @brief Check if this is a separator item.
    bool  isSeparator() const { return separator_; }
    /// @brief Get the attached submenu (nullptr if none).
    Menu* submenu()     const { return submenu_; }

    /// @brief Set the display text.
    void  setText(const std::string& t)     { text_ = t; }
    /// @brief Set the keyboard shortcut label.
    void  setShortcut(const std::string& s) { shortcut_ = s; }
    /// @brief Enable or disable the action.
    void  setEnabled(bool e)                { enabled_ = e; }
    /// @brief Set whether the action is checkable.
    void  setCheckable(bool c)              { checkable_ = c; }
    /// @brief Set the checked state.
    void  setChecked(bool c)                { checked_ = c; }
    /// @brief Attach a submenu (not owned).
    void  setSubmenu(Menu* m)               { submenu_ = m; }

    /// @brief Emitted when the action is triggered.
    Signal<> triggered;

private:
    friend class Menu;
    MenuAction() = default;

    std::string text_;
    std::string shortcut_;
    bool        enabled_   = true;
    bool        checkable_ = false;
    bool        checked_   = false;
    bool        separator_ = false;
    Menu*       submenu_   = nullptr;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Menu — popup menu with a list of actions
//
//    auto* menu = new Menu();  // owned by caller (e.g. MenuBar)
//    menu->addAction("New", []() { ... })->setShortcut("Ctrl+N");
//    menu->addSeparator();
//    menu->addCheckable("Show Grid", true);
//    menu->exec(x, y);  // show as popup at screen coordinates
// ═════════════════════════════════════════════════════════════════════════════

class Menu : public Widget
{
public:
    Menu();
    ~Menu() override;

    /// @brief Add a text action to the menu.
    MenuAction* addAction(const std::string& text);
    /// @brief Add a text action with a callback.
    MenuAction* addAction(const std::string& text, std::function<void()> cb);
    /// @brief Add a checkable action.
    MenuAction* addCheckable(const std::string& text, bool checked = false);
    /// @brief Add a separator line.
    MenuAction* addSeparator();

    /// @brief Remove all actions.
    void        clearActions();
    /// @brief Get the number of actions.
    int         actionCount()   const { return static_cast<int>(actions_.size()); }
    /// @brief Get an action by index.
    MenuAction* action(int idx) const;

    /// @brief Show the menu as a popup at screen coordinates.
    void exec(float x, float y);

    // ── Widget overrides ──────────────────────────────────────────────────
    void resetPopupState()                     override;
    bool popupContains(float x, float y) const override;

    Vec2f sizeHint() const override;
    void  layout()   override {}  // positioned by exec()
    void  paint(PaintContext& ctx) override;

    void onMousePress(MouseEvent& e)  override;
    void onMouseMove(MouseEvent& e)   override;
    void onMouseLeave()               override;

private:
    static constexpr float kSepH   = 9.0f;
    static constexpr float kIconW  = 22.0f;
    static constexpr float kPadX   = 12.0f;

    std::vector<MenuAction*> actions_;  // owned
    int    hoveredIdx_       = -1;
    Menu*  activeSubmenu_    = nullptr;  // not owned (belongs to MenuAction)
    int    activeSubmenuIdx_ = -1;

    float  rowY_(int idx) const;
    int    hitItem_(float screenY) const;
    float  computeWidth_() const;
    void   showSubmenu_(int idx);
    void   hideSubmenu_();
};

// ═════════════════════════════════════════════════════════════════════════════
//  MenuBar — horizontal bar with top-level menus
//
//    auto* bar = root->createChild<MenuBar>();
//    Menu* file = bar->addMenu("File");
//    file->addAction("New")->setShortcut("Ctrl+N");
// ═════════════════════════════════════════════════════════════════════════════

class MenuBar : public Widget
{
public:
    MenuBar();
    ~MenuBar() override;

    /// @brief Add a top-level menu with a title.
    Menu*              addMenu(const std::string& title);
    /// @brief Get the number of menus.
    int                menuCount()      const { return static_cast<int>(entries_.size()); }
    /// @brief Get a menu by index.
    Menu*              menu(int i)      const;
    /// @brief Get a menu title by index.
    const std::string& menuTitle(int i) const;

    Vec2f sizeHint() const override;
    void  layout()   override;
    void  paint(PaintContext& ctx) override;

    void onMousePress(MouseEvent& e)  override;
    void onMouseMove(MouseEvent& e)   override;
    void onMouseLeave()               override;

private:
    struct Entry {
        std::string title;
        Menu*       menu  = nullptr;  // owned by MenuBar
        float       x     = 0;
        float       w     = 0;
    };

    static constexpr float kPadX = 14.0f;

    std::vector<Entry> entries_;
    int hoveredIdx_ = -1;

    int  hitEntry_(float localX) const;
    int  openMenuIdx_()          const;  // index of currently open menu, or -1
    void openMenu_(int idx);
    void closeMenu_();
    void computeEntryWidths_();
};

} // namespace BuGUI
