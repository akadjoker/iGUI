#pragma once

#include "Widget.hpp"
#include "Theme.hpp"
#include "BasicWidgets.hpp"
#include <string>
#include <functional>


namespace BuGUI
{

// ═════════════════════════════════════════════════════════════════════════════
//  GridLayout - uniform grid with N columns, rows auto-calculated
//    Children flow left-to-right, top-to-bottom. Each child = 1 cell.
//    Cell sizes are equal: cellW = (width - padding - gaps) / cols
//    Children can use setStretch() for row-span (like BoxLayout vertical).
// ═════════════════════════════════════════════════════════════════════════════

class GridLayout : public Widget
{
public:
    explicit GridLayout(int cols = 2);

    /// @brief Set the number of columns.
    void setCols(int c) { cols_ = (c > 0) ? c : 1; markDirty(); }
    /// @brief Get the number of columns.
    int  cols() const   { return cols_; }

    /// @brief Set uniform horizontal and vertical spacing.
    void setSpacing(float s)  { spacingH_ = spacingV_ = s; }
    /// @brief Set horizontal and vertical spacing separately.
    void setSpacing(float h, float v) { spacingH_ = h; spacingV_ = v; }
    /// @brief Get horizontal spacing.
    float spacingH() const    { return spacingH_; }
    /// @brief Get vertical spacing.
    float spacingV() const    { return spacingV_; }

    void setPadding(const Edges& e)                      { padding_ = e; }
    void setPadding(float all)                            { padding_ = Edges(all); }
    void setPadding(float tb, float lr)                   { padding_ = Edges(tb, lr); }
    void setPadding(float t, float r, float b, float l)   { padding_ = Edges(t, r, b, l); }
    const Edges& padding() const { return padding_; }

    /// @brief Get the computed number of rows.
    int rows() const;  // computed from child count

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    int   cols_     = 2;
    float spacingH_ = 4.0f;
    float spacingV_ = 4.0f;
    Edges padding_  = Edges(4.0f);
};

// ═════════════════════════════════════════════════════════════════════════════
//  BorderLayout - 5-region layout: Top, Bottom, Left, Right, Center
//    Top/Bottom span full width. Left/Right fill remaining height.
//    Center gets whatever space is left.
//    Each region holds at most one widget. Unset regions take 0 space.
//
//  Region sizes:
//    - Positive value (e.g. 200)  → fixed pixels
//    - Negative value (e.g. -0.2) → percentage of total (abs value, 0..1)
//    - Zero (default)             → use widget's sizeHint
//    Center region ignores size - always fills remaining space.
// ═════════════════════════════════════════════════════════════════════════════

enum class BorderRegion { Top, Bottom, Left, Right, Center };

class BorderLayout : public Widget
{
public:
    BorderLayout() = default;

    // Place a widget in a region. Creates the widget and returns it.
    template <typename T, typename... Args>
    T* set(BorderRegion region, Args&&... args)
    {
        auto* w = new T(std::forward<Args>(args)...);
        setWidget(region, w);
        return w;
    }

    /// @brief Place an existing widget in a region (takes ownership).
    void setWidget(BorderRegion region, Widget* w);

    /// @brief Get the widget in a region (nullptr if empty).
    Widget* get(BorderRegion region) const;

    /// @brief Set region size: +px, -fraction (e.g. -0.2=20%), 0=sizeHint.
    void setRegionSize(BorderRegion region, float size);
    /// @brief Get region size configuration.
    float regionSize(BorderRegion region) const;

    /// @brief Set spacing between regions.
    void setSpacing(float s) { spacing_ = s; markDirty(); }
    /// @brief Get spacing between regions.
    float spacing() const    { return spacing_; }

    void setPadding(const Edges& e)                      { padding_ = e; }
    void setPadding(float all)                            { padding_ = Edges(all); }
    void setPadding(float tb, float lr)                   { padding_ = Edges(tb, lr); }
    void setPadding(float t, float r, float b, float l)   { padding_ = Edges(t, r, b, l); }
    const Edges& padding() const { return padding_; }

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    Widget* regions_[5]     = {nullptr, nullptr, nullptr, nullptr, nullptr};
    float   regionSizes_[5] = {0, 0, 0, 0, 0};  // per-region size config
    float   spacing_        = 0.0f;
    Edges   padding_        = Edges(0.0f);

    // Resolve a region size: positive→px, negative→% of totalDim, zero→hint
    float resolveSize(BorderRegion region, float totalDim, float hint) const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Collapsible - clickable header that shows/hides content
//  Draws a triangle indicator (▶ collapsed / ▼ expanded) via fillBatch.
// ═════════════════════════════════════════════════════════════════════════════

class Collapsible : public Widget
{
public:
    explicit Collapsible(const std::string& title = "", bool expanded = true);

    /// @brief Set the section title text.
    void setTitle(const std::string& t) { title_ = t; markDirty(); }
    /// @brief Get the section title.
    const std::string& title() const    { return title_; }

    /// @brief Expand or collapse the content area.
    void setExpanded(bool e);
    /// @brief Check if the content area is expanded.
    bool isExpanded() const { return expanded_; }

    /// @brief Set the header bar height.
    void setHeaderHeight(float h) { headerH_ = h; markDirty(); }
    /// @brief Get the header bar height.
    float headerHeight() const    { return headerH_; }

    /// @brief Set the header background color.
    void setHeaderColor(const Color& c) { headerColor_ = c; }
    /// @brief Get the header background color.
    const Color& headerColor() const    { return headerColor_; }

    /// @brief Set padding around the content area.
    void setContentPadding(float p) { contentPad_ = p; markDirty(); }
    /// @brief Get the content padding.
    float contentPadding() const    { return contentPad_; }

    /// @brief Set vertical spacing between content children.
    void setContentSpacing(float s) { contentSpacing_ = s; markDirty(); }
    /// @brief Get the content spacing.
    float contentSpacing() const    { return contentSpacing_; }

    /// @brief Emitted when expanded/collapsed state changes.
    Signal<bool> expandedChanged;

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;

private:
    std::string title_;
    bool  expanded_       = true;
    float headerH_        = 26.0f;
    float contentPad_     = 4.0f;
    float contentSpacing_ = 2.0f;
    Color headerColor_    = Color(0, 0, 0, 0);  // transparent = use theme
};

// ═════════════════════════════════════════════════════════════════════════════
//  StatusBar - horizontal bar (typically at bottom) with auto resize grip
//  Shows resize grip triangle when window is resizable.
//  Children are laid out horizontally (left-aligned).
// ═════════════════════════════════════════════════════════════════════════════

class StatusBar : public Widget
{
public:
    StatusBar() = default;

    /// @brief Set the status bar background color.
    void setBgColor(const Color& c)  { bgColor_ = c; }
    /// @brief Get the background color.
    const Color& bgColor() const     { return bgColor_; }

    /// @brief Set spacing between children.
    void setSpacing(float s) { spacing_ = s; markDirty(); }
    /// @brief Get the spacing.
    float spacing() const    { return spacing_; }

    /// @brief Set a text label shown on the left.
    void setText(const std::string& t) { text_ = t; markDirty(); }
    /// @brief Get the status text.
    const std::string& text() const    { return text_; }

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    Color bgColor_ = Color(35, 36, 40, 255);
    float spacing_  = 6.0f;
    std::string text_;
};

// ═════════════════════════════════════════════════════════════════════════════
//  StackLayout - shows one child at a time (like QStackedLayout)
//  All children occupy the full area; only the active one is visible/painted.
// ═════════════════════════════════════════════════════════════════════════════

class StackLayout : public Widget
{
public:
    StackLayout() = default;

    /// @brief Set the active child by index (0-based, -1 = none).
    void setCurrentIndex(int idx);
    /// @brief Get the active child index.
    int  currentIndex() const { return currentIndex_; }
    /// @brief Enable animated transitions between children.
    void setAnimated(bool on) { animated_ = on; }
    /// @brief Set transition duration in seconds.
    void setAnimDuration(float seconds) { animDuration_ = seconds; }
    /// @brief Set the easing function used by transitions.
    void setEaseType(EaseType ease) { easeType_ = ease; }

    /// @brief Get the currently visible widget (nullptr if none).
    Widget* currentWidget() const;

    /// @brief Emitted when the active index changes.
    Signal<int> currentChanged;

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    int currentIndex_ = 0;
    int previousIndex_ = -1;
    int animDir_ = 1;
    bool animated_ = false;
    bool animating_ = false;
    float animProgress_ = 1.0f;
    float animDuration_ = 0.22f;
    EaseType easeType_ = EaseType::OutCubic;
};

// ═════════════════════════════════════════════════════════════════════════════
//  FormLayout - two-column label:widget layout (like QFormLayout)
//  Add rows with addRow(label, widget). Labels auto-right-aligned.
// ═════════════════════════════════════════════════════════════════════════════

class FormLayout : public Widget
{
public:
    FormLayout() = default;

    // Add a row: creates a Label + places widget side by side
    template <typename T, typename... Args>
    T* addRow(const std::string& label, Args&&... args)
    {
        createChild<Label>(label);
        return createChild<T>(std::forward<Args>(args)...);
    }

    /// @brief Label column width: positive = fixed px, 0 = auto.
    void setLabelWidth(float w) { labelWidth_ = w; markDirty(); }
    /// @brief Get the label column width.
    float labelWidth() const    { return labelWidth_; }

    void setSpacing(float h, float v) { spacingH_ = h; spacingV_ = v; markDirty(); }
    void setSpacing(float s)          { spacingH_ = s; spacingV_ = s; markDirty(); }
    float spacingH() const { return spacingH_; }
    float spacingV() const { return spacingV_; }

    void setPadding(const Edges& e)                      { padding_ = e; }
    void setPadding(float all)                            { padding_ = Edges(all); }
    void setPadding(float tb, float lr)                   { padding_ = Edges(tb, lr); }
    void setPadding(float t, float r, float b, float l)   { padding_ = Edges(t, r, b, l); }
    const Edges& padding() const { return padding_; }

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    float labelWidth_ = 0;     // 0 = auto
    float spacingH_   = 8.0f;
    float spacingV_   = 4.0f;
    Edges padding_;
};

// ═════════════════════════════════════════════════════════════════════════════
//  FlowLayout - wraps children like text (CSS flex-wrap)
// ═════════════════════════════════════════════════════════════════════════════

class FlowLayout : public Widget
{
public:
    FlowLayout() = default;

    void setSpacing(float h, float v) { spacingH_ = h; spacingV_ = v; markDirty(); }
    void setSpacing(float s)          { spacingH_ = s; spacingV_ = s; markDirty(); }
    float spacingH() const { return spacingH_; }
    float spacingV() const { return spacingV_; }

    void setPadding(const Edges& e)                      { padding_ = e; }
    void setPadding(float all)                            { padding_ = Edges(all); }
    void setPadding(float tb, float lr)                   { padding_ = Edges(tb, lr); }
    void setPadding(float t, float r, float b, float l)   { padding_ = Edges(t, r, b, l); }
    const Edges& padding() const { return padding_; }

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    float spacingH_ = 4.0f;
    float spacingV_ = 4.0f;
    Edges padding_;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Overlay - children stacked in Z-order, all occupy full area
//  First child = bottom, last = top. For popups, floating panels.
// ═════════════════════════════════════════════════════════════════════════════

class Overlay : public Widget
{
public:
    Overlay() = default;

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Splitter - two children with draggable divider (like QSplitter/GtkPaned)
//  Horizontal: left | right.  Vertical: top | bottom.
// ═════════════════════════════════════════════════════════════════════════════

class Splitter : public Widget
{
public:
    explicit Splitter(LayoutDir dir = LayoutDir::Horizontal);

    /// @brief Get the layout direction.
    LayoutDir dir() const { return dir_; }

    /// @brief Set the split ratio (0..1, fraction for first child).
    void  setRatio(float r);
    /// @brief Get the current split ratio.
    float ratio() const { return ratio_; }

    /// @brief Set minimum ratio (clamps on set).
    void  setMinRatio(float r) { minRatio_ = r; setRatio(ratio_); }
    /// @brief Set maximum ratio (clamps on set).
    void  setMaxRatio(float r) { maxRatio_ = r; setRatio(ratio_); }
    /// @brief Get minimum ratio.
    float minRatio() const     { return minRatio_; }
    /// @brief Get maximum ratio.
    float maxRatio() const     { return maxRatio_; }

    /// @brief Set divider thickness in pixels.
    void  setHandleSize(float s) { handleSize_ = s; markDirty(); }
    /// @brief Get divider thickness.
    float handleSize() const     { return handleSize_; }

    /// @brief Set minimum panel size to prevent collapse.
    void  setMinSize(float s) { minSize_ = s; }
    /// @brief Get minimum panel size.
    float minSize() const     { return minSize_; }

    /// @brief Emitted when the ratio changes.
    Signal<float> ratioChanged;

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseLeave() override;

private:
    LayoutDir dir_;
    float ratio_      = 0.5f;
    float minRatio_   = 0.0f;
    float maxRatio_   = 1.0f;
    float handleSize_ = 5.0f;
    float minSize_    = 30.0f;
    bool  dragging_      = false;
    bool  handleHovered_ = false;
};

// ═════════════════════════════════════════════════════════════════════════════
//  TabLayout - tabbed container (like QTabWidget)
//  Tab bar can be Top, Bottom, Left or Right.
//  Each tab has a label and optional close button (X).
//
//    auto* tabs = parent->createChild<TabLayout>();
//    tabs->addTab("Scene",   scenePanel);
//    tabs->addTab("Assets",  assetsPanel);
//    tabs->setTabsClosable(true);       // shows X on each tab
//    tabs->setTabPosition(TabPosition::Bottom);
// ═════════════════════════════════════════════════════════════════════════════

enum class TabPosition { Top, Bottom, Left, Right };

class TabLayout : public Widget
{
public:
    TabLayout() = default;

    // Add a tab with a label. Creates widget of type T as content. Returns it.
    template <typename T, typename... Args>
    T* addTab(const std::string& label, Args&&... args)
    {
        auto* w = new T(std::forward<Args>(args)...);
        addTabWidget(label, w);
        return w;
    }

    /// @brief Add an existing widget as a tab page.
    void addTabWidget(const std::string& label, Widget* content);

    /// @brief Remove a tab by index (deletes content widget).
    void removeTab(int index);

    /// @brief Get the number of tabs.
    int tabCount() const { return static_cast<int>(tabs_.size()); }

    /// @brief Set the active tab by index.
    void setCurrentIndex(int idx);
    /// @brief Get the active tab index.
    int  currentIndex() const { return currentIndex_; }
    /// @brief Get the content widget of the active tab.
    Widget* currentWidget() const;

    /// @brief Set the label for a tab.
    void setTabLabel(int index, const std::string& label);
    /// @brief Get a tab's label.
    const std::string& tabLabel(int index) const;

    /// @brief Set the tab bar position.
    void setTabPosition(TabPosition pos);
    /// @brief Get the tab bar position.
    TabPosition tabPosition() const      { return tabPosition_; }

    /// @brief Enable close buttons on tabs.
    void setTabsClosable(bool c) { closable_ = c; markDirty(); }
    /// @brief Check if tabs are closable.
    bool tabsClosable() const    { return closable_; }

    /// @brief Set the tab bar thickness.
    void setTabBarSize(float s) { tabBarSize_ = s; markDirty(); }
    /// @brief Get the tab bar thickness.
    float tabBarSize() const    { return tabBarSize_; }

    /// @brief Emitted when the active tab changes.
    Signal<int> currentChanged;
    /// @brief Emitted when a tab's close button is clicked.
    Signal<int> tabClosed;

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;

private:
    struct TabInfo {
        std::string label;
        Widget*     content = nullptr;
    };

    std::vector<TabInfo> tabs_;
    int currentIndex_     = 0;
    TabPosition tabPosition_ = TabPosition::Top;
    bool closable_        = false;
    float tabBarSize_     = 28.0f;

    // Compute tab bar rect and content rect from current geometry
    void computeRects(Rect& barRect, Rect& contentRect) const;

    // Get rect of a specific tab button within the bar
    Rect tabRect(int index, const Rect& barRect) const;

    // Get rect of close button within a tab rect
    Rect closeRect(const Rect& tabR) const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  AnchorLayout - children positioned by anchors relative to parent
//  Each child's position/size is defined by anchor rules.
//  Anchors are 0..1 fractions of parent size + pixel offsets.
//
//    auto* anchor = parent->createChild<AnchorLayout>();
//    auto* btn = anchor->createChild<Button>("OK");
//    AnchorLayout::setAnchors(btn, {
//        .left = 0.5f, .right = 0.5f,      // centered horizontally
//        .bottom = 1.0f, .top = 1.0f,      // anchored to bottom
//        .leftOff = -50, .rightOff = 50,    // 100px wide centered
//        .topOff = -40, .bottomOff = -8     // 32px tall, 8px from bottom
//    });
// ═════════════════════════════════════════════════════════════════════════════

struct AnchorRule
{
    // Anchor fractions (0.0 = parent left/top, 1.0 = parent right/bottom)
    float left   = 0.0f;
    float right  = 1.0f;
    float top    = 0.0f;
    float bottom = 1.0f;

    // Pixel offsets added after anchor fraction
    float leftOff   = 0.0f;
    float rightOff  = 0.0f;
    float topOff    = 0.0f;
    float bottomOff = 0.0f;
};

class AnchorLayout : public Widget
{
public:
    AnchorLayout() = default;

    /// @brief Set anchor rules for a child widget.
    static void setAnchors(Widget* child, const AnchorRule& rule);
    /// @brief Get anchor rules for a child.
    static AnchorRule anchors(const Widget* child);
    /// @brief Check if a child has anchor rules set.
    static bool hasAnchors(const Widget* child);

    void setPadding(const Edges& e)                      { padding_ = e; }
    void setPadding(float all)                            { padding_ = Edges(all); }
    void setPadding(float tb, float lr)                   { padding_ = Edges(tb, lr); }
    void setPadding(float t, float r, float b, float l)   { padding_ = Edges(t, r, b, l); }
    const Edges& padding() const { return padding_; }

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    Edges padding_;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Carousel - animated page viewer with arrows, dots, auto-play
//
//    auto* car = parent->createChild<Carousel>();
//    car->addPage<Panel>();  // page 0
//    car->addPage<Panel>();  // page 1
//    car->setAutoPlay(true);
//    car->setLoop(true);
// ═════════════════════════════════════════════════════════════════════════════

class Carousel : public Widget
{
public:
    Carousel();

    // Add a page (creates widget of type T as content, returns it)
    template <typename T, typename... Args>
    T* addPage(Args&&... args)
    {
        auto* w = new T(std::forward<Args>(args)...);
        addPageWidget(w);
        return w;
    }

    void addPageWidget(Widget* page);
    void removePage(int index);
    int  pageCount() const { return static_cast<int>(pages_.size()); }

    // Navigation
    void setCurrentPage(int idx);
    int  currentPage() const { return currentPage_; }
    void next();
    void prev();

    // Auto-play (advances automatically)
    void setAutoPlay(bool on)            { autoPlay_ = on; autoTimer_ = 0; }
    bool autoPlay() const                { return autoPlay_; }
    void setAutoPlayInterval(float sec)  { autoInterval_ = sec; }
    float autoPlayInterval() const       { return autoInterval_; }

    // Loop (wrap around at ends)
    void setLoop(bool l)   { loop_ = l; }
    bool loop() const      { return loop_; }

    // Show/hide controls
    void setShowArrows(bool s) { showArrows_ = s; markDirty(); }
    bool showArrows() const    { return showArrows_; }
    void setShowDots(bool s)   { showDots_ = s; markDirty(); }
    bool showDots() const      { return showDots_; }

    // Animation duration (seconds)
    void  setAnimDuration(float sec) { animDuration_ = sec; }
    float animDuration() const       { return animDuration_; }

    // Easing curve for page transition
    void     setEaseType(EaseType e) { easeType_ = e; }
    EaseType easeType() const        { return easeType_; }

    Signal<int> pageChanged;

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseLeave() override;

private:
    std::vector<Widget*> pages_;
    int   currentPage_   = 0;

    bool  autoPlay_      = false;
    float autoInterval_  = 3.0f;
    float autoTimer_     = 0.0f;

    bool  loop_          = true;
    bool  showArrows_    = true;
    bool  showDots_      = true;

    float animDuration_  = 0.3f;
    float animProgress_  = 0.0f;   // 0..1
    EaseType easeType_   = EaseType::OutCubic;
    int   animFrom_      = -1;     // page sliding out
    int   animDir_       = 0;      // -1=slide right, +1=slide left
    bool  animating_     = false;

    int   hoveredArrow_  = 0;      // 0=none, -1=left, +1=right

    float dotsHeight() const { return 24.0f; }
    Rect contentRect() const;
    Rect arrowRect(bool rightSide) const;
    void startAnim(int from, int to, int dir);

    void ensureArrowTexture();
    static unsigned int arrowTexId_;
    static constexpr int kIconSize = 32;
};

// ═════════════════════════════════════════════════════════════════════════════
//  SlidePanel — drawer that slides in from Left/Right edge with easing
//    Owns one content child (whatever you put inside).
//    open()/close()/toggle() animate it in/out.
//    Optional scrim (dim overlay) that fades in and is click-to-close.
// ═════════════════════════════════════════════════════════════════════════════

class SlidePanel : public Widget
{
public:
    enum Side { Left, Right };

    explicit SlidePanel(Side side = Left, float width = 260.0f);

    /// @brief Open the slide panel.
    void open();
    /// @brief Close the slide panel.
    void close();
    /// @brief Toggle open/closed state.
    void toggle();
    /// @brief Check if the panel is open.
    bool isOpen() const { return open_; }

    /// @brief Set the panel width.
    void setWidth(float w)           { panelWidth_ = w; markDirty(); }
    /// @brief Get the panel width.
    float panelWidth() const         { return panelWidth_; }

    /// @brief Set which side the panel slides from.
    void setSide(Side s)             { side_ = s; markDirty(); }
    /// @brief Get the slide side.
    Side side() const                { return side_; }

    /// @brief Set animation duration in seconds.
    void setAnimDuration(float sec)  { animDuration_ = sec; }
    /// @brief Get animation duration.
    float animDuration() const       { return animDuration_; }

    /// @brief Set the easing function for animation.
    void setEaseType(EaseType e)     { easeType_ = e; }
    /// @brief Get the easing function.
    EaseType easeType() const        { return easeType_; }

    /// @brief Enable/disable the dim scrim overlay.
    void setShowScrim(bool s)        { showScrim_ = s; markDirty(); }
    /// @brief Check if the scrim overlay is shown.
    bool showScrim() const           { return showScrim_; }

    /// @brief Emitted when the panel opens or closes.
    Signal<bool> openChanged;

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;

private:
    Side     side_         = Left;
    float    panelWidth_   = 260.0f;
    bool     open_         = false;
    bool     animating_    = false;
    float    animProgress_ = 0.0f;   // 0=closed, 1=open
    float    animDuration_ = 0.25f;
    EaseType easeType_     = EaseType::OutCubic;
    bool     showScrim_    = true;
};


} // namespace BuGUI
