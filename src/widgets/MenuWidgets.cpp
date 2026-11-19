#include "MenuWidgets.hpp"
#include "WidgetApp.hpp"
#include "pch.hpp"
#include <algorithm>
#include <cmath>

// ═════════════════════════════════════════════════════════════════════════════
//  Menu
// ═════════════════════════════════════════════════════════════════════════════

Menu::Menu()
{
    acceptsFocus_ = true;
    scrollable_ = false;
}

MenuAction::~MenuAction()
{
    if (gone_) *gone_ = true;
    if (submenuOwned_) delete submenu_;
}

void MenuAction::setSubmenu(Menu* m, bool owned)
{
    if (m == submenu_)
    {
        submenuOwned_ = owned && m;
        return;
    }
    if (submenuOwned_) delete submenu_; // the one it had, which it owned
    submenu_ = m;
    submenuOwned_ = owned && m;
}

Menu::~Menu()
{
    if (deletedFlag_) *deletedFlag_ = true;
    // activeSubmenu_ is not touched: a caller's submenu may be gone already
    for (auto* a: actions_) delete a;
}

MenuAction* Menu::addAction(const String& text)
{
    auto* a = new MenuAction();
    a->text_ = text;
    actions_.push_back(a);
    return a;
}

MenuAction* Menu::addAction(const String& text, ct::Function<void()> cb)
{
    auto* a = addAction(text);
    a->triggered.connect(std::move(cb));
    return a;
}

MenuAction* Menu::addCheckable(const String& text, bool checked)
{
    auto* a = addAction(text);
    a->checkable_ = true;
    a->checked_ = checked;
    return a;
}

Menu* Menu::addSubmenu(const String& text)
{
    MenuAction* a = addAction(text);
    Menu* sub = new Menu();
    a->setSubmenu(sub, true);
    return sub;
}

MenuAction* Menu::addSeparator()
{
    auto* a = new MenuAction();
    a->separator_ = true;
    actions_.push_back(a);
    return a;
}

void Menu::clearActions()
{
    // an open submenu is about to go (or to lose its action): close it first, and forget the highlight
    hideSubmenu_();
    hoveredIdx_ = -1;
    for (auto* a: actions_) delete a;
    actions_.clear();
    markDirty();
}

MenuAction* Menu::action(int idx) const
{
    if (idx < 0 || idx >= static_cast<int>(actions_.size())) return nullptr;
    return actions_[idx];
}

// ── Internal helpers ──────────────────────────────────────────────────────────

float Menu::rowY_(int idx) const
{
    const auto& t = Theme::instance();
    float y = rect_.y + 4.0f;
    for (int i = 0; i < idx; ++i) y += actions_[i]->isSeparator() ? kSepH : t.menuItemHeight;
    return y;
}

Rect Menu::actionRect(int idx) const
{
    if (idx < 0 || idx >= static_cast<int>(actions_.size())) return Rect();
    const auto& t = Theme::instance();
    const float h = actions_[idx]->isSeparator() ? kSepH : t.menuItemHeight;
    return Rect(rect_.x, rowY_(idx), rect_.w, h);
}

int Menu::hitItem_(float screenY) const
{
    const auto& t = Theme::instance();
    float y = rect_.y + 4.0f;
    for (int i = 0; i < static_cast<int>(actions_.size()); ++i)
    {
        float h = actions_[i]->isSeparator() ? kSepH : t.menuItemHeight;
        if (screenY >= y && screenY < y + h) return i;
        y += h;
    }
    return -1;
}

float Menu::computeWidth_() const
{
    const auto& t = Theme::instance();
    float maxW = t.menuMinWidth;
    for (auto* a: actions_)
    {
        if (a->isSeparator()) continue;
        float tw = WidgetApp::instance().textWidth(a->text().c_str());
        float sw = a->shortcut().empty()
                           ? 0.0f
                           : WidgetApp::instance().textWidth(a->shortcut().c_str()) + kPadX * 2;
        float w = kIconW + kPadX + tw + sw + kPadX;
        if (a->submenu()) w += kArrowW;
        maxW = std::max(maxW, w);
    }
    return maxW;
}

// ── Public API ────────────────────────────────────────────────────────────────

Widget::Vec2f Menu::sizeHint() const
{
    const auto& t = Theme::instance();
    float h = 8.0f;
    for (auto* a: actions_) h += a->isSeparator() ? kSepH : t.menuItemHeight;
    return { computeWidth_(), h };
}

void Menu::exec(float x, float y, Widget* owner)
{
    place_(x, y, owner, false);
}

void Menu::execOwned(float x, float y, Widget* owner)
{
    place_(x, y, owner, true);
}

void Menu::place_(float x, float y, Widget* owner, bool owned)
{
    // The items are up to date before the size is worked out. A menu is no widget in a parent: its owner deletes it, from a
    // handler of this signal too (a menu rebuilt, or dropped, when it is about to show): then nothing of it is touched.
    bool deleted = false;
    deletedFlag_ = &deleted;
    aboutToShow.emit();
    if (deleted) return;
    deletedFlag_ = nullptr;
    Vec2f sz = sizeHint();
    float sw = static_cast<float>(WidgetApp::instance().width());
    float sh = static_cast<float>(WidgetApp::instance().height());
    if (x + sz.x > sw) x = sw - sz.x;
    if (y + sz.y > sh) y = sh - sz.y;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    setPosition(x, y);
    setSize(sz.x, sz.y);
    hoveredIdx_ = -1;
    WidgetApp::instance().showPopup(this, owner, owned);
}

void Menu::resetPopupState()
{
    hideSubmenu_();
    hoveredIdx_ = -1;
    markDirty();
}

bool Menu::popupContains(float x, float y) const
{
    if (absoluteRect().contains(x, y)) return true;
    if (activeSubmenu_ && activeSubmenu_->popupContains(x, y)) return true;
    return false;
}

void Menu::showSubmenu_(int idx)
{
    if (activeSubmenuIdx_ == idx) return;
    hideSubmenu_();
    MenuAction* a = actions_[idx];
    Menu* sub = a->submenu();
    if (!sub) return;
    sub->aboutToShow.emit();
    // (a handler that rebuilt this menu took the action away: nothing to open)
    if (idx >= static_cast<int>(actions_.size()) || actions_[idx]->submenu() != sub) return;

    Widget::Vec2f sz = sub->sizeHint();
    Rect abs = absoluteRect();
    float sw = static_cast<float>(WidgetApp::instance().width());
    float sh = static_cast<float>(WidgetApp::instance().height());

    float sx = abs.x + abs.w;
    float sy = rowY_(idx);
    if (sx + sz.x > sw) sx = abs.x - sz.x; // flip left
    if (sy + sz.y > sh) sy = sh - sz.y;    // flip up
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;

    sub->setPosition(sx, sy);
    sub->setSize(sz.x, sz.y);
    sub->resetPopupState();
    activeSubmenu_ = sub;
    activeSubmenuIdx_ = idx;
    markDirty();
}

void Menu::hideSubmenu_()
{
    if (!activeSubmenu_) return;
    activeSubmenu_->hideSubmenu_();
    activeSubmenu_->resetPopupState();
    activeSubmenu_ = nullptr;
    activeSubmenuIdx_ = -1;
    markDirty();
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void Menu::paint(PaintContext& ctx)
{
    if (!visible_) return;
    const auto& t = Theme::instance();
    Rect abs = absoluteRect();

    // Background
    ctx.fill.SetColor(t.menuBg.r, t.menuBg.g, t.menuBg.b, 255);
    ctx.fill.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, true);

    ctx.pushClip(abs);

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();

    float y = abs.y + 4.0f;
    for (int i = 0; i < static_cast<int>(actions_.size()); ++i)
    {
        MenuAction* a = actions_[i];
        float itemH = a->isSeparator() ? kSepH : t.menuItemHeight;

        if (a->isSeparator())
        {
            float sy = y + kSepH * 0.5f;
            ctx.line.SetColor(t.menuSeparator.r, t.menuSeparator.g, t.menuSeparator.b,
                    t.menuSeparator.a);
            ctx.line.Line2D(abs.x + 4, sy, abs.x + abs.w - 4, sy);
        }
        else
        {
            bool hov = (i == hoveredIdx_) && a->isEnabled();

            // Hover highlight
            if (hov)
            {
                ctx.fill.SetColor(t.menuItemHover.r, t.menuItemHover.g, t.menuItemHover.b, 255);
                ctx.fill.RoundedRectangle(abs.x + 2, y, abs.w - 4, itemH, t.borderRadius, 6, true);
            }

            Color tc = !a->isEnabled() ? t.menuItemDisabled
                       : hov           ? t.menuItemTextHover
                                       : t.menuItemText;

            // Icon, or checkmark, in the left column
            if (a->icon() != IconId::None)
            {
                const float cx = abs.x + kIconW * 0.5f;
                const float cy = y + itemH * 0.5f;
                ctx.drawIcon(a->icon(), cx - kIconSize * 0.5f, cy - kIconSize * 0.5f, kIconSize, tc);
                // checked: a frame around the icon, where an item with no icon has the tick
                if (a->isCheckable() && a->isChecked())
                {
                    ctx.line.SetColor(t.menuCheckMark.r, t.menuCheckMark.g, t.menuCheckMark.b, 255);
                    ctx.line.RoundedRectangle(cx - kIconSize * 0.5f - 2.0f, cy - kIconSize * 0.5f - 2.0f,
                            kIconSize + 4.0f, kIconSize + 4.0f, t.borderRadius, 6, false);
                }
            }
            else if (a->isCheckable() && a->isChecked())
            {
                float cx = abs.x + kIconW * 0.5f;
                float cy = y + itemH * 0.5f;
                ctx.line.SetColor(t.menuCheckMark.r, t.menuCheckMark.g, t.menuCheckMark.b, 255);
                ctx.line.Line2D(cx - 5, cy, cx - 1, cy + 4);
                ctx.line.Line2D(cx - 1, cy + 4, cx + 5, cy - 4);
            }

            // Item text
            ctx.font.SetColor(tc);
            float ty = y + (itemH + asc) * 0.5f;
            ctx.font.Print(a->text().c_str(), abs.x + kIconW + kPadX, ty);

            // Shortcut (right-aligned)
            if (!a->shortcut().empty())
            {
                Color sc = hov ? t.menuShortcutHover : t.menuShortcutText;
                ctx.font.SetColor(sc);
                float shw = ctx.font.GetTextWidth(a->shortcut().c_str());
                // an item with a submenu has its arrow in the last 16 pixels (computeWidth_ made the room): the
                // shortcut ends before it
                const float right = abs.x + abs.w - kPadX - (a->submenu() ? kArrowW : 0.0f);
                ctx.font.Print(a->shortcut().c_str(), right - shw, ty);
            }

            // Submenu arrow
            if (a->submenu())
            {
                float ax = abs.x + abs.w - 10;
                float ay = y + itemH * 0.5f;
                ctx.line.SetColor(t.menuSubmenuArrow.r, t.menuSubmenuArrow.g, t.menuSubmenuArrow.b,
                        255);
                ctx.line.Line2D(ax - 4, ay - 4, ax, ay);
                ctx.line.Line2D(ax, ay, ax - 4, ay + 4);
            }
        }

        y += itemH;
    }

    ctx.popClip();

    // Border — outside clip so it draws cleanly over the content
    ctx.line.SetColor(t.menuBorder.r, t.menuBorder.g, t.menuBorder.b, t.menuBorder.a);
    ctx.line.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, false);

    // Recursively paint any open submenu on top
    if (activeSubmenu_) activeSubmenu_->paint(ctx);
}

// ── Events ────────────────────────────────────────────────────────────────────

void Menu::onMousePress(MouseEvent& e)
{
    // Delegate to open submenu first
    if (activeSubmenu_ && activeSubmenu_->popupContains(e.x, e.y))
    {
        activeSubmenu_->onMousePress(e);
        return;
    }

    int idx = hitItem_(e.y);
    if (idx < 0 || idx >= static_cast<int>(actions_.size()))
    {
        e.consumed = true;
        return;
    }
    MenuAction* a = actions_[idx];
    if (a->isSeparator() || !a->isEnabled())
    {
        e.consumed = true;
        return;
    }

    // Submenu item: open on click (don't trigger or close)
    if (a->submenu())
    {
        showSubmenu_(idx);
        e.consumed = true;
        return;
    }

    const bool flipped = a->isCheckable() && a->autoToggle_;
    if (flipped) a->checked_ = !a->checked_;
    WidgetApp::instance().closePopup();
    if (!flipped)
    {
        a->triggered.emit();
        e.consumed = true;
        return;
    }
    // two signals in a row: a handler of the first may rebuild the menu or delete it, and the action with it
    bool gone = false;
    a->gone_ = &gone;
    a->toggled.emit(a->checked_);
    if (!gone)
    {
        a->gone_ = nullptr;
        a->triggered.emit();
    }
    e.consumed = true;
}

void Menu::onMouseMove(MouseEvent& e)
{
    // Delegate move to submenu if cursor is over it
    if (activeSubmenu_ && activeSubmenu_->popupContains(e.x, e.y))
    {
        activeSubmenu_->onMouseMove(e);
        return;
    }

    int idx = hitItem_(e.y);
    if (idx != hoveredIdx_)
    {
        hoveredIdx_ = idx;
        markDirty();
    }

    if (idx >= 0 && idx < static_cast<int>(actions_.size()))
    {
        MenuAction* a = actions_[idx];
        if (a->submenu()) showSubmenu_(idx); // open/switch submenu on hover
        else if (idx != activeSubmenuIdx_)
            hideSubmenu_(); // close submenu when on a non-submenu item
    }
}

void Menu::onMouseLeave()
{
    hoveredIdx_ = -1;
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  MenuBar
// ═════════════════════════════════════════════════════════════════════════════

MenuBar::MenuBar()
{
    acceptsFocus_ = false;
}

MenuBar::~MenuBar()
{
    for (auto& e: entries_) delete e.menu;
}

Menu* MenuBar::addMenu(const String& title)
{
    Entry e;
    e.title = title;
    e.menu = new Menu();
    entries_.push_back(std::move(e));
    return entries_.back().menu;
}

Menu* MenuBar::menu(int i) const
{
    if (i < 0 || i >= static_cast<int>(entries_.size())) return nullptr;
    return entries_[i].menu;
}

const String& MenuBar::menuTitle(int i) const
{
    static const String empty;
    if (i < 0 || i >= static_cast<int>(entries_.size())) return empty;
    return entries_[i].title;
}

// ── Internal helpers ──────────────────────────────────────────────────────────

void MenuBar::computeEntryWidths_()
{
    float x = 0;
    for (auto& e: entries_)
    {
        float tw = WidgetApp::instance().textWidth(e.title.c_str());
        e.w = tw + kPadX * 2;
        e.x = x;
        x += e.w;
    }
}

int MenuBar::hitEntry_(float localX) const
{
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i)
        if (localX >= entries_[i].x && localX < entries_[i].x + entries_[i].w) return i;
    return -1;
}

int MenuBar::openMenuIdx_() const
{
    Widget* p = WidgetApp::instance().popup();
    if (!p) return -1;
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i)
        if (entries_[i].menu == p) return i;
    return -1;
}

Rect MenuBar::titleRect(int i) const
{
    if (i < 0 || i >= static_cast<int>(entries_.size())) return Rect();
    const Rect abs = absoluteRect();
    return Rect(abs.x + entries_[i].x, abs.y, entries_[i].w, abs.h);
}

void MenuBar::openMenu_(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(entries_.size())) return;
    closeMenu_();
    auto& e = entries_[idx];
    Rect abs = absoluteRect();
    e.menu->exec(abs.x + e.x, abs.y + abs.h, this);
}

void MenuBar::closeMenu_()
{
    WidgetApp::instance().closePopup();
}

// ── Widget overrides ──────────────────────────────────────────────────────────

Widget::Vec2f MenuBar::sizeHint() const
{
    return { rect_.w, Theme::instance().menuBarHeight };
}

void MenuBar::layout()
{
    computeEntryWidths_();
}

void MenuBar::paint(PaintContext& ctx)
{
    if (!visible_) return;
    const auto& t = Theme::instance();
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    // Recompute widths each frame so font changes are reflected
    computeEntryWidths_();

    // Background
    ctx.fill.SetColor(t.menuBarBg.r, t.menuBarBg.g, t.menuBarBg.b, 255);
    ctx.fill.Rectangle(abs.x, abs.y, abs.w, abs.h, true);

    ctx.pushClip(abs);

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();
    int open = openMenuIdx_();

    for (int i = 0; i < static_cast<int>(entries_.size()); ++i)
    {
        auto& e = entries_[i];
        float ix = abs.x + e.x;
        bool hov = (i == hoveredIdx_) || (i == open);

        if (hov)
        {
            ctx.fill.SetColor(t.menuBarItemHover.r, t.menuBarItemHover.g, t.menuBarItemHover.b,
                    255);
            ctx.fill.RoundedRectangle(ix + 2, abs.y + 2, e.w - 4, abs.h - 4, t.borderRadius, 6,
                    true);
        }

        Color tc = hov ? t.menuItemTextHover : t.menuItemText;
        ctx.font.SetColor(tc);
        float tw = ctx.font.GetTextWidth(e.title.c_str());
        float tx = ix + (e.w - tw) * 0.5f;
        float ty = abs.y + (abs.h + asc) * 0.5f;
        ctx.font.Print(e.title.c_str(), tx, ty);
    }

    ctx.popClip();

    // Bottom border line
    ctx.line.SetColor(t.menuBorder.r, t.menuBorder.g, t.menuBorder.b, t.menuBorder.a);
    ctx.line.Line2D(abs.x, abs.y + abs.h - 1, abs.x + abs.w, abs.y + abs.h - 1);
}

void MenuBar::onMousePress(MouseEvent& e)
{
    Rect abs = absoluteRect();
    int idx = hitEntry_(e.x - abs.x);
    int open = openMenuIdx_();
    if (idx < 0)
    {
        if (open >= 0) closeMenu_();
        return;
    }
    if (open == idx) closeMenu_();
    else
        openMenu_(idx);
    e.consumed = true;
}

void MenuBar::onMouseMove(MouseEvent& e)
{
    Rect abs = absoluteRect();
    int idx = hitEntry_(e.x - abs.x);

    if (idx != hoveredIdx_)
    {
        hoveredIdx_ = idx;
        markDirty();
    }

    // If a menu is already open, hover to switch
    int open = openMenuIdx_();
    if (open >= 0 && idx >= 0 && idx != open) openMenu_(idx);
}

void MenuBar::onMouseLeave()
{
    hoveredIdx_ = -1;
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  PopupPanel
// ═════════════════════════════════════════════════════════════════════════════

PopupPanel::PopupPanel(LayoutDir dir)
        : BoxLayout(dir)
{
    acceptsFocus_ = true; // the focus lives here when no child has it: Escape reaches the panel
    setTabStop(false);    // but Tab goes between the children, not through the panel itself
    scrollable_ = false;
}

void PopupPanel::open(float x, float y, Widget* owner)
{
    WidgetApp& app = WidgetApp::instance();
    const Vec2f sz = sizeHint();
    const float sw = static_cast<float>(app.width());
    const float sh = static_cast<float>(app.height());
    if (x + sz.x > sw) x = sw - sz.x;
    if (y + sz.y > sh) y = sh - sz.y;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    setRect(Rect(x, y, sz.x, sz.y));
    layout();
    if (isOpen()) return; // moved: it is shown, with the focus where it was put
    app.showPopup(this, owner, false); // (this lets go of the panel if it was fading out: closed goes out for that opening)
    shown_ = true;
    app.lendFocus(this, focusTarget_ ? focusTarget_ : this);
}

void PopupPanel::openBelow(Widget* anchor)
{
    if (!anchor) return;
    const Rect a = anchor->absoluteRect();
    const Vec2f sz = sizeHint();
    float y = a.y + a.h + 2.0f;
    // no room below and room above: above
    if (y + sz.y > static_cast<float>(WidgetApp::instance().height()) && a.y - 2.0f - sz.y >= 0.0f) y = a.y - 2.0f - sz.y;
    open(a.x, y, anchor);
}

void PopupPanel::close()
{
    if (isOpen()) WidgetApp::instance().closePopup();
}

bool PopupPanel::isOpen() const
{
    return WidgetApp::instance().popup() == this;
}

void PopupPanel::resetPopupState()
{
    // Called when the popup system is done with the panel, in the middle of retiring it (and perhaps of showing another
    // popup): the signal does not go out from in here, where a handler that opens a popup, or the panel itself, would be
    // undone by the rest of the retirement. It goes out in the next update(), as a call the application makes after its
    // events (deferred calls are dropped if the panel is deleted before then).
    if (!shown_) return;
    shown_ = false;
    WidgetApp::instance().callDeferred(this, [this]() { closed.emit(); });
}

void PopupPanel::onKeyPress(KeyEvent& e)
{
    if (e.key == ig::retained::Key::Escape)
    {
        close();
        e.consumed = true;
        return;
    }
    BoxLayout::onKeyPress(e);
}

void PopupPanel::paint(PaintContext& ctx)
{
    if (!visible_) return;
    const auto& t = Theme::instance();
    const Rect abs = absoluteRect();
    ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, 255);
    ctx.fill.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, true);
    BoxLayout::paint(ctx);
    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.line.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, false);
}
