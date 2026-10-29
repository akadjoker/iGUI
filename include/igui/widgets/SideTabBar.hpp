#pragma once

#include "Widget.hpp"
#include "Signal.hpp"
#include "Retained.hpp"   // IconId
#include <igui/widgets/String.hpp>
#include <ct/vector.hpp>

// ═════════════════════════════════════════════════════════════════════════════
//  SideTabBar — a vertical rail of icon buttons on the left or right edge and
//  the page the pressed button opens beside it: the sidebar of an editor
//  (Character / Camera / Stage / Compose ... in a 3D tool, the activity bar
//  of a code editor).
//
//  One button per page, the icon on top and the label under it. Pressing a
//  button shows its page; pressing the active one again folds the page away
//  so only the rail stays (setCollapsible(false) keeps a page open at all
//  times). A page can carry a badge: a dot on its button that says something
//  changed there (keys on the pose, an import that finished).
//
//      auto* side = parent->createChild<SideTabBar>(SideTabBar::Side::Left);
//      side->addPage<PropertyGrid>(IconId::Gear, "Adjust");
//      auto* pose = side->addPage<BoxLayout>(IconId::User, "Pose", LayoutDir::Vertical);
//      side->setPageBadge(1, true);
//      side->currentChanged.connect([](int index) { ... });   // -1: folded
//
//  sizeHint() is the rail plus the page while one is open, so a parent
//  BoxLayout gives the room back to the viewport when the sidebar folds.
// ═════════════════════════════════════════════════════════════════════════════

namespace ig { namespace retained
{
class SideTabBar : public Widget
{
public:
    IG_WIDGET_CLASS(SideTabBar, Widget);

    enum class Side { Left, Right };

    explicit SideTabBar(Side side = Side::Left);

    // ── Pages ─────────────────────────────────────────────────────────────
    /// @brief Add a page whose content is a new widget of type T; returns the content.
    template <typename T, typename... Args>
    T* addPage(IconId icon, const String& label, Args&&... args)
    {
        auto* w = new T(std::forward<Args>(args)...);
        addPageWidget(icon, label, w);
        return w;
    }

    /// @brief Add an existing widget as a page (takes ownership); returns its index.
    int addPageWidget(IconId icon, const String& label, Widget* content);

    /// @brief Remove a page by index (deletes its content widget).
    void removePage(int index);

    /// @brief The number of pages.
    int pageCount() const { return static_cast<int>(pages_.size()); }

    /// @brief The content widget of a page; nullptr for a bad index.
    Widget* pageWidget(int index) const;

    /// @brief The index of the page holding this content; -1 if none.
    int indexOf(const Widget* content) const;

    // ── Current page ──────────────────────────────────────────────────────
    /// @brief Show a page; -1 folds the page area so only the rail stays.
    void setCurrentIndex(int index);
    /// @brief The open page's index; -1 while folded.
    int  currentIndex() const { return currentIndex_; }
    /// @brief The open page's content; nullptr while folded.
    Widget* currentWidget() const { return pageWidget(currentIndex_); }

    /// @brief True while no page is open.
    bool isCollapsed() const { return currentIndex_ < 0; }
    /// @brief Fold the page area (collapsible() only governs the click on the active button).
    void collapse();
    /// @brief Reopen the page that was open before the fold (or the first one).
    void expand();

    // ── Per-page state ────────────────────────────────────────────────────
    void          setPageLabel(int index, const String& label);
    const String& pageLabel(int index) const;

    void   setPageIcon(int index, IconId icon);
    IconId pageIcon(int index) const;

    /// @brief A disabled page's button is dimmed and does not react.
    void setPageEnabled(int index, bool enabled);
    bool isPageEnabled(int index) const;

    /// @brief A hidden page has no button on the rail; hiding the open page folds it.
    void setPageVisible(int index, bool visible);
    bool isPageVisible(int index) const;

    /// @brief The dot on a page's button: something changed on that page.
    void setPageBadge(int index, bool on);
    bool pageBadge(int index) const;

    /// @brief The tooltip of a page's button; with labels hidden the label stands in when none is set.
    void          setPageTooltip(int index, const String& tip);
    const String& pageTooltip(int index) const;

    // ── Appearance ────────────────────────────────────────────────────────
    /// @brief Which edge the rail sits on; the page opens on the other side of it.
    void setSide(Side side);
    Side side() const { return side_; }

    /// @brief Whether pressing the active button folds the page (default true).
    void setCollapsible(bool c);
    bool collapsible() const { return collapsible_; }

    /// @brief Whether labels are drawn under the icons (default true).
    void setShowLabels(bool show);
    bool showLabels() const { return showLabels_; }

    /// @brief Fixed rail width in pixels; 0 (default) fits the widest label.
    void  setRailWidth(float w);
    float railWidth() const { return railWidth_; }

    /// @brief Height of each rail button (default 56).
    void  setButtonHeight(float h);
    float buttonHeight() const { return buttonHeight_; }

    /// @brief Icon size inside a button (default 20).
    void  setIconSize(float s);
    float iconSize() const { return iconSize_; }

    /// @brief Width of the page area while a page is open (default 280).
    void  setPageWidth(float w);
    float pageWidth() const { return pageWidth_; }

    /// @brief The rail width in use: railWidth() or the fitted one.
    float effectiveRailWidth() const;

    // ── Geometry (window space, for tests and tutorial videos) ────────────
    /// @brief Rectangle of a page's rail button; false when hidden or unknown.
    bool buttonRect(int index, Rect& out) const;
    /// @brief The page whose button is under a window-space point; -1 if none.
    int  buttonAt(float x, float y) const;
    /// @brief Rectangle of the page area; empty while folded.
    Rect pageAreaRect() const;

    // ── Signals ───────────────────────────────────────────────────────────
    /// @brief The open page changed (-1 when the sidebar folded).
    Signal<int> currentChanged;
    /// @brief A rail button was clicked (index), before the page switches.
    Signal<int> buttonClicked;

    // ── Widget overrides ──────────────────────────────────────────────────
    void  layout() override;
    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;
    void  onMouseLeave() override;
    // A drag over a rail button opens its page, so what is dragged can be dropped there.
    void  onDragHover(const DragPayload& payload, float x, float y) override;

private:
    struct Page
    {
        IconId  icon    = IconId::None;
        String  label;
        String  tooltip;
        Widget* content = nullptr;
        bool    enabled = true;
        bool    visible = true;
        bool    badge   = false;
    };

    ct::Vector<Page> pages_;
    int   currentIndex_ = -1;
    int   lastIndex_    = -1;   // reopened by expand()
    Side  side_         = Side::Left;
    bool  collapsible_  = true;
    bool  showLabels_   = true;
    float railWidth_    = 0.0f;
    float buttonHeight_ = 56.0f;
    float iconSize_     = 20.0f;
    float pageWidth_    = 280.0f;

    int   hoverIndex_   = -1;
    int   pressIndex_   = -1;

    bool  validIndex(int index) const { return index >= 0 && index < static_cast<int>(pages_.size()); }
    int   firstVisible() const;
    void  showPage(int index);                 // switch + signal + relayout
    void  pressButton(int index);              // the click on a rail button

    // Local (widget-space) geometry
    Rect  railRect() const;
    Rect  pageRect() const;
    Rect  localButtonRect(int index) const;    // empty when the page is hidden
    Rect  localButtonRect(int index, const Rect& rail) const;   // the rail already measured
    float fittedRailWidth() const;
    void  updateHover(float x, float y);
};

} // namespace retained
} // namespace ig
