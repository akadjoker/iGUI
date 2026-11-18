#pragma once

#include "Theme.hpp"
#include "Widget.hpp"
#include <ct/function.hpp>
#include <ct/vector.hpp>
#include <igui/widgets/String.hpp>


namespace ig
{
namespace retained
{
// ═════════════════════════════════════════════════════════════════════════════
//  MenuAction — a single item in a Menu
//    auto* act = menu->addAction("Open");
//    act->setShortcut("Ctrl+O");
//    act->setIcon(IconId::Folder);
//    act->triggered.connect([]() { /* ... */ });
// ═════════════════════════════════════════════════════════════════════════════

class Menu;

class MenuAction
{
public:
    /// @brief Get the action display text.
    const String& text() const
    {
        return text_;
    }
    /// @brief Get the keyboard shortcut string.
    const String& shortcut() const
    {
        return shortcut_;
    }
    /// @brief Check if the action is enabled.
    bool isEnabled() const
    {
        return enabled_;
    }
    /// @brief Check if the action is checkable.
    bool isCheckable() const
    {
        return checkable_;
    }
    /// @brief Check if the action is currently checked.
    bool isChecked() const
    {
        return checked_;
    }
    /// @brief The icon drawn in the left column, where a check mark goes (IconId::None: no icon).
    IconId icon() const
    {
        return icon_;
    }
    /// @brief Check if this is a separator item.
    bool isSeparator() const
    {
        return separator_;
    }
    /// @brief Get the attached submenu (nullptr if none).
    Menu* submenu() const
    {
        return submenu_;
    }

    /// @brief Set the display text.
    void setText(const String& t)
    {
        text_ = t;
    }
    /// @brief Set the keyboard shortcut label.
    void setShortcut(const String& s)
    {
        shortcut_ = s;
    }
    /// @brief Enable or disable the action.
    void setEnabled(bool e)
    {
        enabled_ = e;
    }
    /// @brief Set whether the action is checkable.
    void setCheckable(bool c)
    {
        checkable_ = c;
    }
    /// @brief Set the checked state. Silent: toggled is for the clicks of the person (as CheckBox's).
    void setChecked(bool c)
    {
        checked_ = c;
    }
    /// @brief Whether a click on a checkable action flips it (default true, as it always did). With
    ///        false the click does not touch the checked state: the action only emits triggered()
    ///        (and not toggled()), and the application decides, with setChecked(): a command that can
    ///        be refused, or one that comes back to the menu from a model that owns the state.
    void setAutoToggle(bool a)
    {
        autoToggle_ = a;
    }
    /// @brief True when a click flips a checkable action (the default).
    bool autoToggle() const
    {
        return autoToggle_;
    }
    /// @brief Put an icon in the left column of the item (the column of the check mark, which is
    ///        22 pixels wide: the icon is drawn 16 pixels square and takes the colour of the item's
    ///        text). Any IconId does: the built-in ones, or an application's own picture made with
    ///        registerImageIcon(). A checked checkable action with an icon shows a frame around the
    ///        icon where the others show the tick. IconId::None takes the icon away. The width of
    ///        the menu does not change (the column was always there).
    void setIcon(IconId id)
    {
        icon_ = id;
    }
    /// @brief Attach a submenu the caller owns: nothing here deletes it (the actions of a Menu are
    ///        deleted by it, the submenu they point to is not unless it was given as owned). The
    ///        caller keeps it alive while the action exists and deletes it, after the menu that
    ///        shows it is gone or its actions are cleared. This is what the applications that keep
    ///        their submenus (a recent-files menu rebuilt from time to time) rely on.
    /// @param owned true: the action takes the submenu and deletes it with itself (when the Menu
    ///        deletes the action: clearActions(), its destructor). Menu::addSubmenu() does this.
    void setSubmenu(Menu* m, bool owned = false);
    /// @brief True when the action deletes its submenu with itself (see setSubmenu).
    bool ownsSubmenu() const
    {
        return submenuOwned_;
    }

    /// @brief Emitted when the action is triggered.
    Signal<> triggered;
    /// @brief Emitted when a click flipped a checkable action, with the new state, just before
    ///        triggered() (which then sees the new state too, as it always did). Not emitted by
    ///        setChecked(), nor when setAutoToggle(false) left the state to the application.
    Signal<bool> toggled;

private:
    friend class Menu;
    MenuAction() = default;

    ~MenuAction(); // deletes the submenu when it owns it

    String text_;
    String shortcut_;
    bool submenuOwned_ = false;
    bool enabled_ = true;
    bool checkable_ = false;
    bool checked_ = false;
    bool separator_ = false;
    bool autoToggle_ = true;
    IconId icon_ = IconId::None;
    Menu* submenu_ = nullptr;
    bool* gone_ = nullptr; // set by the click that is emitting: becomes true if a handler deletes the action
};

// ═════════════════════════════════════════════════════════════════════════════
//  Menu — popup menu with a list of actions
//
//    auto* menu = new Menu();  // owned by caller (e.g. MenuBar)
//    menu->addAction("New", []() { ... })->setShortcut("Ctrl+N");
//    menu->addSeparator();
//    menu->addCheckable("Show Grid", true);
//    menu->exec(x, y);  // show as popup at screen coordinates
//
//  Who owns what: a Menu deletes its MenuActions (in clearActions() and its destructor); a MenuBar
//  deletes the menus made with addMenu(); a submenu is deleted by its action only when it was made
//  with Menu::addSubmenu() or given to MenuAction::setSubmenu(menu, true): a submenu given to
//  setSubmenu(menu) stays the caller's (a Widget deletes the context menu it was given, see
//  Widget::setContextMenu). A menu shown with exec() is not owned by the popup system.
// ═════════════════════════════════════════════════════════════════════════════

class Menu : public Widget
{
public:
    IG_WIDGET_CLASS(Menu, Widget);

    Menu();
    ~Menu() override;

    /// @brief Add a text action to the menu.
    MenuAction* addAction(const String& text);
    /// @brief Add a text action with a callback.
    MenuAction* addAction(const String& text, ct::Function<void()> cb);
    /// @brief Add a checkable action.
    MenuAction* addCheckable(const String& text, bool checked = false);
    /// @brief Add a separator line.
    MenuAction* addSeparator();
    /// @brief Add an action with a submenu of its own, and return the submenu to fill: it belongs to
    ///        the action, which deletes it when the menu clears its actions or goes away, so there is
    ///        nothing to keep or delete (setSubmenu(), by default, leaves that to the caller).
    Menu* addSubmenu(const String& text);

    /// @brief Remove all actions. The actions are deleted (and the submenus they own, see
    ///        MenuAction::setSubmenu); a submenu the caller owns is left alone. A menu that is open
    ///        closes its open submenu and forgets the highlight, so it can be rebuilt while it is shown.
    void clearActions();
    /// @brief Get the number of actions.
    int actionCount() const
    {
        return static_cast<int>(actions_.size());
    }
    /// @brief Get an action by index.
    MenuAction* action(int idx) const;

    /// @brief Show the menu as a popup at screen coordinates.
    void exec(float x, float y, Widget* owner = nullptr);
    /// @brief Highlight an item as if the mouse were over it (-1 clears).
    void highlight(int idx)
    {
        hoveredIdx_ = idx;
        markDirty();
    }
    /// @brief Screen rectangle of an item while the menu is shown.
    Rect actionRect(int idx) const;

    // ── Widget overrides ──────────────────────────────────────────────────
    void resetPopupState() override;
    bool popupContains(float x, float y) const override;

    Vec2f sizeHint() const override;
    void layout() override
    {
    } // positioned by exec()
    void paint(PaintContext& ctx) override;

    void onMousePress(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseLeave() override;

private:
    static constexpr float kSepH = 9.0f;
    static constexpr float kIconW = 22.0f;
    static constexpr float kIconSize = 16.0f; // an action's icon, centred in the kIconW column
    static constexpr float kPadX = 12.0f;
    static constexpr float kArrowW = 16.0f; // what an item with a submenu adds on the right for its arrow

    ct::Vector<MenuAction*> actions_; // owned
    int hoveredIdx_ = -1;
    Menu* activeSubmenu_ = nullptr; // not owned (belongs to MenuAction)
    int activeSubmenuIdx_ = -1;

    float rowY_(int idx) const;
    int hitItem_(float screenY) const;
    float computeWidth_() const;
    void showSubmenu_(int idx);
    void hideSubmenu_();
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
    IG_WIDGET_CLASS(MenuBar, Widget);

    MenuBar();
    ~MenuBar() override;

    /// @brief Add a top-level menu with a title.
    Menu* addMenu(const String& title);
    /// @brief Get the number of menus.
    int menuCount() const
    {
        return static_cast<int>(entries_.size());
    }
    /// @brief Get a menu by index.
    Menu* menu(int i) const;
    /// @brief Get a menu title by index.
    const String& menuTitle(int i) const;
    /// @brief Open a menu by index, as a click on its title does.
    void openMenu(int i)
    {
        openMenu_(i);
    }
    /// @brief Close the open menu, if any.
    void closeMenu()
    {
        closeMenu_();
    }
    /// @brief Screen rectangle of a menu title.
    Rect titleRect(int i) const;

    Vec2f sizeHint() const override;
    void layout() override;
    void paint(PaintContext& ctx) override;

    void onMousePress(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseLeave() override;

private:
    struct Entry
    {
        String title;
        Menu* menu = nullptr; // owned by MenuBar
        float x = 0;
        float w = 0;
    };

    static constexpr float kPadX = 14.0f;

    ct::Vector<Entry> entries_;
    int hoveredIdx_ = -1;

    int hitEntry_(float localX) const;
    int openMenuIdx_() const; // index of currently open menu, or -1
    void openMenu_(int idx);
    void closeMenu_();
    void computeEntryWidths_();
};

} // namespace retained
} // namespace ig
