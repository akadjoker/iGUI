#include "pch.hpp"
#include "MenuWidgets.hpp"
#include "WidgetApp.hpp"
#include <algorithm>
#include <cmath>

// ═════════════════════════════════════════════════════════════════════════════
//  Menu
// ═════════════════════════════════════════════════════════════════════════════

Menu::Menu()
{
    acceptsFocus_ = true;
    scrollable_   = false;
}

Menu::~Menu()
{
    for (auto* a : actions_) delete a;
}

MenuAction* Menu::addAction(const std::string& text)
{
    auto* a = new MenuAction();
    a->text_ = text;
    actions_.push_back(a);
    return a;
}

MenuAction* Menu::addAction(const std::string& text, std::function<void()> cb)
{
    auto* a = addAction(text);
    a->triggered.connect(std::move(cb));
    return a;
}

MenuAction* Menu::addCheckable(const std::string& text, bool checked)
{
    auto* a = addAction(text);
    a->checkable_ = true;
    a->checked_   = checked;
    return a;
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
    for (auto* a : actions_) delete a;
    actions_.clear();
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
    for (int i = 0; i < idx; ++i)
        y += actions_[i]->isSeparator() ? kSepH : t.menuItemHeight;
    return y;
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
    for (auto* a : actions_)
    {
        if (a->isSeparator()) continue;
        float tw = WidgetApp::instance().textWidth(a->text().c_str());
        float sw = a->shortcut().empty() ? 0.0f
                 : WidgetApp::instance().textWidth(a->shortcut().c_str()) + kPadX * 2;
        float w = kIconW + kPadX + tw + sw + kPadX;
        if (a->submenu()) w += 16.0f;
        maxW = std::max(maxW, w);
    }
    return maxW;
}

// ── Public API ────────────────────────────────────────────────────────────────

Widget::Vec2f Menu::sizeHint() const
{
    const auto& t = Theme::instance();
    float h = 8.0f;
    for (auto* a : actions_)
        h += a->isSeparator() ? kSepH : t.menuItemHeight;
    return {computeWidth_(), h};
}

void Menu::exec(float x, float y)
{
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
    WidgetApp::instance().showPopup(this, nullptr, false);
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

    Widget::Vec2f sz = sub->sizeHint();
    Rect abs = absoluteRect();
    float sw = static_cast<float>(WidgetApp::instance().width());
    float sh = static_cast<float>(WidgetApp::instance().height());

    float sx = abs.x + abs.w;
    float sy = rowY_(idx);
    if (sx + sz.x > sw) sx = abs.x - sz.x;  // flip left
    if (sy + sz.y > sh) sy = sh - sz.y;      // flip up
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;

    sub->setPosition(sx, sy);
    sub->setSize(sz.x, sz.y);
    sub->resetPopupState();
    activeSubmenu_    = sub;
    activeSubmenuIdx_ = idx;
    markDirty();
}

void Menu::hideSubmenu_()
{
    if (!activeSubmenu_) return;
    activeSubmenu_->hideSubmenu_();
    activeSubmenu_->resetPopupState();
    activeSubmenu_    = nullptr;
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
        MenuAction* a   = actions_[i];
        float       itemH = a->isSeparator() ? kSepH : t.menuItemHeight;

        if (a->isSeparator())
        {
            float sy = y + kSepH * 0.5f;
            ctx.line.SetColor(t.menuSeparator.r, t.menuSeparator.g,
                              t.menuSeparator.b, t.menuSeparator.a);
            ctx.line.Line2D(abs.x + 4, sy, abs.x + abs.w - 4, sy);
        }
        else
        {
            bool hov = (i == hoveredIdx_) && a->isEnabled();

            // Hover highlight
            if (hov)
            {
                ctx.fill.SetColor(t.menuItemHover.r, t.menuItemHover.g,
                                  t.menuItemHover.b, 255);
                ctx.fill.RoundedRectangle(abs.x + 2, y, abs.w - 4, itemH,
                                          t.borderRadius, 6, true);
            }

            // Checkmark
            if (a->isCheckable() && a->isChecked())
            {
                float cx = abs.x + kIconW * 0.5f;
                float cy = y + itemH * 0.5f;
                ctx.line.SetColor(t.menuCheckMark.r, t.menuCheckMark.g,
                                  t.menuCheckMark.b, 255);
                ctx.line.Line2D(cx - 5, cy,     cx - 1, cy + 4);
                ctx.line.Line2D(cx - 1, cy + 4, cx + 5, cy - 4);
            }

            // Item text
            Color tc = !a->isEnabled()   ? t.menuItemDisabled
                     : hov               ? t.menuItemTextHover
                                         : t.menuItemText;
            ctx.font.SetColor(tc);
            float ty = y + (itemH + asc) * 0.5f;
            ctx.font.Print(a->text().c_str(), abs.x + kIconW + kPadX, ty);

            // Shortcut (right-aligned)
            if (!a->shortcut().empty())
            {
                Color sc = hov ? t.menuShortcutHover : t.menuShortcutText;
                ctx.font.SetColor(sc);
                float shw = ctx.font.GetTextWidth(a->shortcut().c_str());
                ctx.font.Print(a->shortcut().c_str(),
                               abs.x + abs.w - shw - kPadX, ty);
            }

            // Submenu arrow
            if (a->submenu())
            {
                float ax = abs.x + abs.w - 10;
                float ay = y + itemH * 0.5f;
                ctx.line.SetColor(t.menuSubmenuArrow.r, t.menuSubmenuArrow.g,
                                  t.menuSubmenuArrow.b, 255);
                ctx.line.Line2D(ax - 4, ay - 4, ax, ay);
                ctx.line.Line2D(ax,     ay,      ax - 4, ay + 4);
            }
        }

        y += itemH;
    }

    ctx.popClip();

    // Border — outside clip so it draws cleanly over the content
    ctx.line.SetColor(t.menuBorder.r, t.menuBorder.g,
                      t.menuBorder.b, t.menuBorder.a);
    ctx.line.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, false);

    // Recursively paint any open submenu on top
    if (activeSubmenu_)
        activeSubmenu_->paint(ctx);
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
    if (a->isSeparator() || !a->isEnabled()) { e.consumed = true; return; }

    // Submenu item: open on click (don't trigger or close)
    if (a->submenu())
    {
        showSubmenu_(idx);
        e.consumed = true;
        return;
    }

    if (a->isCheckable()) a->checked_ = !a->checked_;
    WidgetApp::instance().closePopup();
    a->triggered.emit();
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
    if (idx != hoveredIdx_) { hoveredIdx_ = idx; markDirty(); }

    if (idx >= 0 && idx < static_cast<int>(actions_.size()))
    {
        MenuAction* a = actions_[idx];
        if (a->submenu())
            showSubmenu_(idx);       // open/switch submenu on hover
        else if (idx != activeSubmenuIdx_)
            hideSubmenu_();          // close submenu when on a non-submenu item
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
    for (auto& e : entries_) delete e.menu;
}

Menu* MenuBar::addMenu(const std::string& title)
{
    Entry e;
    e.title = title;
    e.menu  = new Menu();
    entries_.push_back(std::move(e));
    return entries_.back().menu;
}

Menu* MenuBar::menu(int i) const
{
    if (i < 0 || i >= static_cast<int>(entries_.size())) return nullptr;
    return entries_[i].menu;
}

const std::string& MenuBar::menuTitle(int i) const
{
    static const std::string empty;
    if (i < 0 || i >= static_cast<int>(entries_.size())) return empty;
    return entries_[i].title;
}

// ── Internal helpers ──────────────────────────────────────────────────────────

void MenuBar::computeEntryWidths_()
{
    float x = 0;
    for (auto& e : entries_)
    {
        float tw = WidgetApp::instance().textWidth(e.title.c_str());
        e.w = tw + kPadX * 2;
        e.x = x;
        x  += e.w;
    }
}

int MenuBar::hitEntry_(float localX) const
{
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i)
        if (localX >= entries_[i].x && localX < entries_[i].x + entries_[i].w)
            return i;
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

void MenuBar::openMenu_(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(entries_.size())) return;
    closeMenu_();
    auto& e = entries_[idx];
    Rect abs = absoluteRect();
    e.menu->exec(abs.x + e.x, abs.y + abs.h);
}

void MenuBar::closeMenu_()
{
    WidgetApp::instance().closePopup();
}

// ── Widget overrides ──────────────────────────────────────────────────────────

Widget::Vec2f MenuBar::sizeHint() const
{
    return {rect_.w, Theme::instance().menuBarHeight};
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
    float asc  = ctx.font.GetAscender();
    int   open = openMenuIdx_();

    for (int i = 0; i < static_cast<int>(entries_.size()); ++i)
    {
        auto&  e  = entries_[i];
        float  ix = abs.x + e.x;
        bool   hov = (i == hoveredIdx_) || (i == open);

        if (hov)
        {
            ctx.fill.SetColor(t.menuBarItemHover.r, t.menuBarItemHover.g,
                              t.menuBarItemHover.b, 255);
            ctx.fill.RoundedRectangle(ix + 2, abs.y + 2, e.w - 4,
                                      abs.h - 4, t.borderRadius, 6, true);
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
    ctx.line.SetColor(t.menuBorder.r, t.menuBorder.g,
                      t.menuBorder.b, t.menuBorder.a);
    ctx.line.Line2D(abs.x, abs.y + abs.h - 1, abs.x + abs.w, abs.y + abs.h - 1);
}

void MenuBar::onMousePress(MouseEvent& e)
{
    Rect abs    = absoluteRect();
    int  idx    = hitEntry_(e.x - abs.x);
    if (idx < 0) return;
    int  open   = openMenuIdx_();
    if (open == idx)
        closeMenu_();
    else
        openMenu_(idx);
    e.consumed = true;
}

void MenuBar::onMouseMove(MouseEvent& e)
{
    Rect abs = absoluteRect();
    int  idx = hitEntry_(e.x - abs.x);

    if (idx != hoveredIdx_) { hoveredIdx_ = idx; markDirty(); }

    // If a menu is already open, hover to switch
    int open = openMenuIdx_();
    if (open >= 0 && idx >= 0 && idx != open)
        openMenu_(idx);
}

void MenuBar::onMouseLeave()
{
    hoveredIdx_ = -1;
    markDirty();
}
