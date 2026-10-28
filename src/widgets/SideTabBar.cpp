#include "pch.hpp"
#include "SideTabBar.hpp"
#include "Theme.hpp"
#include "WidgetApp.hpp"
#include "Utf8.hpp"

namespace ig { namespace retained
{
namespace
{
const String kEmptySideTabString;

constexpr float kRailPad      = 4.0f;   // above the first button
constexpr float kButtonMargin = 2.0f;   // between a button and the rail edge
constexpr float kButtonGap    = 2.0f;   // between buttons
constexpr float kLabelGap     = 4.0f;   // between the icon and its label
constexpr float kMinRailWidth = 56.0f;
constexpr float kBadgeRadius  = 3.5f;
constexpr float kBadgeInset   = 9.0f;
}

SideTabBar::SideTabBar(Side side)
    : side_(side)
{
}

// ═════════════════════════════════════════════════════════════════════════════
//  Pages
// ═════════════════════════════════════════════════════════════════════════════

int SideTabBar::addPageWidget(IconId icon, const String& label, Widget* content)
{
    Page page;
    page.icon    = icon;
    page.label   = label;
    page.content = content;
    pages_.push_back(page);

    // The serializer loads a page's content under the bar before adopting it
    // as a page, so a widget that is already ours is not added twice.
    if (content && content->parent() != this)
        addChild(content);

    const int index = pageCount() - 1;
    if (currentIndex_ < 0 && lastIndex_ < 0)
    {
        // The first page opens itself, without a signal: nothing changed yet
        // from the caller's point of view.
        currentIndex_ = index;
        lastIndex_    = index;
    }
    else if (content)
    {
        content->setVisible(false);
    }

    markDirty();
    WidgetApp::instance().requestLayout();
    return index;
}

void SideTabBar::removePage(int index)
{
    if (!validIndex(index)) return;

    const int before = currentIndex_;
    if (pages_[index].content)
        removeChild(pages_[index].content);
    pages_.erase(pages_.begin() + index);
    hoverIndex_ = -1;
    pressIndex_ = -1;

    if (index < currentIndex_)
        --currentIndex_;
    else if (currentIndex_ >= pageCount())
        currentIndex_ = pageCount() - 1;   // -1 once the last page is gone

    if (index < lastIndex_)
        --lastIndex_;
    else if (index == lastIndex_)
        lastIndex_ = currentIndex_;

    markDirty();
    WidgetApp::instance().requestLayout();
    if (currentIndex_ != before || index == before)
        currentChanged.emit(currentIndex_);
}

Widget* SideTabBar::pageWidget(int index) const
{
    return validIndex(index) ? pages_[index].content : nullptr;
}

int SideTabBar::indexOf(const Widget* content) const
{
    if (!content) return -1;
    for (int i = 0; i < pageCount(); ++i)
        if (pages_[i].content == content) return i;
    return -1;
}

int SideTabBar::firstVisible() const
{
    for (int i = 0; i < pageCount(); ++i)
        if (pages_[i].visible) return i;
    return -1;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Current page
// ═════════════════════════════════════════════════════════════════════════════

void SideTabBar::showPage(int index)
{
    if (index == currentIndex_) return;
    currentIndex_ = index;
    if (index >= 0) lastIndex_ = index;
    markDirty();
    WidgetApp::instance().requestLayout();
    currentChanged.emit(index);
}

void SideTabBar::setCurrentIndex(int index)
{
    if (index < 0)
    {
        showPage(-1);
        return;
    }
    if (!validIndex(index) || !pages_[index].visible) return;
    showPage(index);
}

void SideTabBar::collapse()
{
    showPage(-1);
}

void SideTabBar::expand()
{
    if (currentIndex_ >= 0) return;
    int index = (validIndex(lastIndex_) && pages_[lastIndex_].visible) ? lastIndex_ : firstVisible();
    if (index >= 0) showPage(index);
}

void SideTabBar::pressButton(int index)
{
    buttonClicked.emit(index);
    if (index == currentIndex_)
    {
        if (collapsible_) showPage(-1);
        return;
    }
    showPage(index);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Per-page state
// ═════════════════════════════════════════════════════════════════════════════

void SideTabBar::setPageLabel(int index, const String& label)
{
    if (!validIndex(index)) return;
    pages_[index].label = label;
    markDirty();
    WidgetApp::instance().requestLayout();   // the fitted rail width follows the labels
}

const String& SideTabBar::pageLabel(int index) const
{
    return validIndex(index) ? pages_[index].label : kEmptySideTabString;
}

void SideTabBar::setPageIcon(int index, IconId icon)
{
    if (!validIndex(index)) return;
    pages_[index].icon = icon;
    markDirty();
}

IconId SideTabBar::pageIcon(int index) const
{
    return validIndex(index) ? pages_[index].icon : IconId::None;
}

void SideTabBar::setPageEnabled(int index, bool enabled)
{
    if (!validIndex(index) || pages_[index].enabled == enabled) return;
    pages_[index].enabled = enabled;
    if (!enabled)
    {
        if (hoverIndex_ == index) hoverIndex_ = -1;
        if (pressIndex_ == index) pressIndex_ = -1;
    }
    markDirty();
}

bool SideTabBar::isPageEnabled(int index) const
{
    return validIndex(index) && pages_[index].enabled;
}

void SideTabBar::setPageVisible(int index, bool visible)
{
    if (!validIndex(index) || pages_[index].visible == visible) return;
    pages_[index].visible = visible;
    hoverIndex_ = -1;
    pressIndex_ = -1;
    if (!visible && index == currentIndex_)
        showPage(-1);
    markDirty();
    WidgetApp::instance().requestLayout();
}

bool SideTabBar::isPageVisible(int index) const
{
    return validIndex(index) && pages_[index].visible;
}

void SideTabBar::setPageBadge(int index, bool on)
{
    if (!validIndex(index) || pages_[index].badge == on) return;
    pages_[index].badge = on;
    markDirty();
}

bool SideTabBar::pageBadge(int index) const
{
    return validIndex(index) && pages_[index].badge;
}

void SideTabBar::setPageTooltip(int index, const String& tip)
{
    if (!validIndex(index)) return;
    pages_[index].tooltip = tip;
}

const String& SideTabBar::pageTooltip(int index) const
{
    return validIndex(index) ? pages_[index].tooltip : kEmptySideTabString;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Appearance
// ═════════════════════════════════════════════════════════════════════════════

void SideTabBar::setSide(Side side)
{
    if (side_ == side) return;
    side_ = side;
    markDirty();
    WidgetApp::instance().requestLayout();
}

void SideTabBar::setCollapsible(bool c)
{
    collapsible_ = c;
}

void SideTabBar::setShowLabels(bool show)
{
    if (showLabels_ == show) return;
    showLabels_ = show;
    markDirty();
    WidgetApp::instance().requestLayout();
}

void SideTabBar::setRailWidth(float w)
{
    if (w < 0.0f) w = 0.0f;
    if (railWidth_ == w) return;
    railWidth_ = w;
    markDirty();
    WidgetApp::instance().requestLayout();
}

void SideTabBar::setButtonHeight(float h)
{
    if (h < 1.0f) h = 1.0f;
    if (buttonHeight_ == h) return;
    buttonHeight_ = h;
    markDirty();
}

void SideTabBar::setIconSize(float s)
{
    if (s < 1.0f) s = 1.0f;
    if (iconSize_ == s) return;
    iconSize_ = s;
    markDirty();
    WidgetApp::instance().requestLayout();
}

void SideTabBar::setPageWidth(float w)
{
    if (w < 0.0f) w = 0.0f;
    if (pageWidth_ == w) return;
    pageWidth_ = w;
    markDirty();
    WidgetApp::instance().requestLayout();
}

float SideTabBar::fittedRailWidth() const
{
    const Theme& t = Theme::instance();
    float width = iconSize_ + t.padding * 4.0f;
    if (width < kMinRailWidth) width = kMinRailWidth;
    if (!showLabels_) return width;

    // Measured with the application's font when there is one; before the
    // first frame (or in a headless test) an average glyph width stands in.
    const WidgetApp* app = WidgetApp::existing();
    for (const Page& page : pages_)
    {
        if (!page.visible || page.label.empty()) continue;
        float textW = app ? app->textWidth(page.label.c_str()) : 0.0f;
        if (textW <= 0.0f)
            textW = static_cast<float>(utf8Length(page.label)) * t.fontSize * 0.55f;
        textW += t.padding * 2.0f + kButtonMargin * 2.0f;
        if (textW > width) width = textW;
    }
    return width;
}

float SideTabBar::effectiveRailWidth() const
{
    return railWidth_ > 0.0f ? railWidth_ : fittedRailWidth();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Geometry
// ═════════════════════════════════════════════════════════════════════════════

Rect SideTabBar::railRect() const
{
    const float w = effectiveRailWidth();
    return {side_ == Side::Left ? 0.0f : rect_.w - w, 0.0f, w, rect_.h};
}

Rect SideTabBar::pageRect() const
{
    if (isCollapsed()) return {};
    const float railW = effectiveRailWidth();
    float w = rect_.w - railW;
    if (w < 0.0f) w = 0.0f;
    return {side_ == Side::Left ? railW : 0.0f, 0.0f, w, rect_.h};
}

Rect SideTabBar::localButtonRect(int index) const
{
    return localButtonRect(index, railRect());
}

Rect SideTabBar::localButtonRect(int index, const Rect& rail) const
{
    if (!validIndex(index) || !pages_[index].visible) return {};
    float y = rail.y + kRailPad;
    for (int i = 0; i < index; ++i)
    {
        if (!pages_[i].visible) continue;
        y += buttonHeight_ + kButtonGap;
    }
    return {rail.x + kButtonMargin, y, rail.w - kButtonMargin * 2.0f, buttonHeight_};
}

bool SideTabBar::buttonRect(int index, Rect& out) const
{
    const Rect local = localButtonRect(index);
    if (local.w <= 0.0f) return false;
    const Rect abs = absoluteRect();
    out = {abs.x + local.x, abs.y + local.y, local.w, local.h};
    return true;
}

int SideTabBar::buttonAt(float x, float y) const
{
    const Rect abs = absoluteRect();
    const Rect rail = railRect();
    for (int i = 0; i < pageCount(); ++i)
    {
        const Rect local = localButtonRect(i, rail);
        if (local.w <= 0.0f) continue;
        const Rect r{abs.x + local.x, abs.y + local.y, local.w, local.h};
        if (r.contains(x, y)) return i;
    }
    return -1;
}

Rect SideTabBar::pageAreaRect() const
{
    const Rect local = pageRect();
    const Rect abs = absoluteRect();
    return {abs.x + local.x, abs.y + local.y, local.w, local.h};
}

// ═════════════════════════════════════════════════════════════════════════════
//  Layout
// ═════════════════════════════════════════════════════════════════════════════

void SideTabBar::layout()
{
    const Rect page = pageRect();
    for (int i = 0; i < pageCount(); ++i)
    {
        Widget* content = pages_[i].content;
        if (!content) continue;
        if (i == currentIndex_ && page.w > 0.0f)
        {
            content->setVisible(true);
            content->setRect(page);
            content->layout();
        }
        else
        {
            content->setVisible(false);
        }
    }
}

Widget::Vec2f SideTabBar::sizeHint() const
{
    return {effectiveRailWidth() + (isCollapsed() ? 0.0f : pageWidth_), rect_.h};
}

// ═════════════════════════════════════════════════════════════════════════════
//  Paint
// ═════════════════════════════════════════════════════════════════════════════

void SideTabBar::paint(PaintContext& ctx)
{
    if (!visible_) return;

    const Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const Theme& t = Theme::instance();
    const Rect rail = railRect();
    const Rect page = pageRect();
    const Rect absRail{abs.x + rail.x, abs.y + rail.y, rail.w, rail.h};
    const Rect absPage{abs.x + page.x, abs.y + page.y, page.w, page.h};

    Rect clipped;
    if (ctx.clipRectIntersect(absRail, clipped))
    {
        ctx.fill.SetColor(t.sideTabRailBg.r, t.sideTabRailBg.g, t.sideTabRailBg.b, t.sideTabRailBg.a);
        ctx.fill.Rectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                           static_cast<int>(clipped.w), static_cast<int>(clipped.h), true);
    }

    if (page.w > 0.0f)
    {
        if (ctx.clipRectIntersect(absPage, clipped))
        {
            ctx.fill.SetColor(t.sideTabPageBg.r, t.sideTabPageBg.g, t.sideTabPageBg.b, t.sideTabPageBg.a);
            ctx.fill.Rectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                               static_cast<int>(clipped.w), static_cast<int>(clipped.h), true);
        }
        // The seam between the rail and the page
        const float seamX = side_ == Side::Left ? absRail.x + absRail.w : absRail.x;
        ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
        ctx.line.Line2D(static_cast<int>(seamX), static_cast<int>(absRail.y),
                        static_cast<int>(seamX), static_cast<int>(absRail.y + absRail.h));
    }

    const float ascender = ctx.font.GetAscender();
    const float textH    = ctx.font.GetFontSize();

    for (int i = 0; i < pageCount(); ++i)
    {
        const Rect local = localButtonRect(i, rail);
        if (local.w <= 0.0f) continue;
        const Rect br{abs.x + local.x, abs.y + local.y, local.w, local.h};
        Rect visibleBr;
        if (!ctx.clipRectIntersect(br, visibleBr)) continue;

        const Page& p = pages_[i];
        const bool active  = i == currentIndex_;
        const bool hovered = i == hoverIndex_ && p.enabled;
        const bool pressed = hovered && i == pressIndex_;

        if (active)
        {
            ctx.fill.SetColor(t.sideTabActiveBg.r, t.sideTabActiveBg.g, t.sideTabActiveBg.b, t.sideTabActiveBg.a);
            ctx.fill.RoundedRectangle(br.x, br.y, br.w, br.h, t.borderRadius, 6, true);
            ctx.line.SetColor(t.sideTabAccent.r, t.sideTabAccent.g, t.sideTabAccent.b, t.sideTabAccent.a);
            ctx.line.RoundedRectangle(br.x, br.y, br.w, br.h, t.borderRadius, 6, false);
        }
        else if (pressed)
        {
            ctx.fill.SetColor(t.buttonPressed.r, t.buttonPressed.g, t.buttonPressed.b, t.buttonPressed.a);
            ctx.fill.RoundedRectangle(br.x, br.y, br.w, br.h, t.borderRadius, 6, true);
        }
        else if (hovered)
        {
            ctx.fill.SetColor(t.sideTabHoverBg.r, t.sideTabHoverBg.g, t.sideTabHoverBg.b, t.sideTabHoverBg.a);
            ctx.fill.RoundedRectangle(br.x, br.y, br.w, br.h, t.borderRadius, 6, true);
        }

        const Color fg = !p.enabled ? t.textDisabled
                       : active     ? t.sideTabAccent
                       : hovered    ? t.sideTabTextActive
                                    : t.sideTabText;

        // The icon above the label, the pair centred in the button
        const bool  withLabel = showLabels_ && !p.label.empty();
        const float blockH    = iconSize_ + (withLabel ? kLabelGap + textH : 0.0f);
        const float top       = br.y + (br.h - blockH) * 0.5f;

        ctx.pushClip(visibleBr);
        if (p.icon != IconId::None)
        {
            ctx.drawIcon(p.icon, br.x + (br.w - iconSize_) * 0.5f, top, iconSize_, fg);
        }
        else if (!p.label.empty())
        {
            // No icon: the label's first character stands in for it
            char initial[8] = {};
            const int n = utf8CharLen(p.label.c_str());
            for (int b = 0; b < n && b < 7; ++b) initial[b] = p.label[static_cast<size_t>(b)];
            ctx.font.SetColor(fg);
            const float w = ctx.font.GetTextWidth(initial);
            ctx.font.Print(initial, br.x + (br.w - w) * 0.5f, top + (iconSize_ + ascender) * 0.5f);
        }

        if (withLabel)
        {
            ctx.font.SetColor(fg);
            const float w = ctx.font.GetTextWidth(p.label.c_str());
            float x = br.x + (br.w - w) * 0.5f;
            if (x < br.x + kButtonMargin) x = br.x + kButtonMargin;
            ctx.font.Print(p.label.c_str(), x, top + iconSize_ + kLabelGap + ascender);
        }
        ctx.popClip();

        if (p.badge)
        {
            ctx.fill.SetColor(t.sideTabBadge.r, t.sideTabBadge.g, t.sideTabBadge.b, t.sideTabBadge.a);
            ctx.fill.Circle(br.x + br.w - kBadgeInset, br.y + kBadgeInset, kBadgeRadius, true);
        }
    }

    if (page.w > 0.0f)
    {
        Widget* current = currentWidget();
        if (current && current->isVisible())
        {
            ctx.pushClip(absPage);
            current->paint(ctx);
            ctx.popClip();
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Mouse
// ═════════════════════════════════════════════════════════════════════════════

void SideTabBar::updateHover(float x, float y)
{
    int index = buttonAt(x, y);
    if (index >= 0 && !pages_[index].enabled) index = -1;
    if (index == hoverIndex_) return;
    hoverIndex_ = index;
    markDirty();

    // The tooltip is the hovered page's; with the labels hidden the label
    // itself is the tooltip, so an icon-only rail still says what it opens.
    String tip;
    if (index >= 0)
    {
        tip = pages_[index].tooltip;
        if (tip.empty() && !showLabels_) tip = pages_[index].label;
    }
    if (tip != tooltip()) setTooltip(tip);
    setCursor(index >= 0 ? CursorType::Hand : CursorType::Arrow);
}

void SideTabBar::onMousePress(MouseEvent& e)
{
    if (!isEnabledInTree() || e.button != 0) return;

    const int index = buttonAt(e.x, e.y);
    if (index < 0)
    {
        // The rail background is ours; the page area reaches us only when no
        // child took the press, and then there is nothing to do with it.
        const Rect abs = absoluteRect();
        const Rect rail = railRect();
        const Rect absRail{abs.x + rail.x, abs.y + rail.y, rail.w, rail.h};
        if (absRail.contains(e.x, e.y)) e.consumed = true;
        return;
    }

    e.consumed = true;
    if (!pages_[index].enabled) return;
    pressIndex_ = index;
    updateHover(e.x, e.y);   // a press without a move first still hovers the button
    markDirty();
}

void SideTabBar::onMouseRelease(MouseEvent& e)
{
    if (e.button != 0 || pressIndex_ < 0) return;

    const int pressed = pressIndex_;
    pressIndex_ = -1;
    markDirty();

    // A button acts on release, over the same button it was pressed on
    if (buttonAt(e.x, e.y) != pressed || !validIndex(pressed) || !pages_[pressed].enabled) return;
    e.consumed = true;
    pressButton(pressed);
}

void SideTabBar::onMouseMove(MouseEvent& e)
{
    Widget::onMouseMove(e);
    if (!isEnabledInTree()) return;
    updateHover(e.x, e.y);
}

void SideTabBar::onMouseLeave()
{
    Widget::onMouseLeave();
    if (hoverIndex_ < 0) return;
    hoverIndex_ = -1;
    setCursor(CursorType::Arrow);
    markDirty();
}

} // namespace retained
} // namespace ig
