#pragma once

#include "Widget.hpp"


namespace BuGUI
{
// ─────────────────────────────────────────────────────────────────────────────
//  DockSide - where to dock relative to a panel
// ─────────────────────────────────────────────────────────────────────────────
/// @brief Side to dock a panel relative to another.
enum class DockSide { Left, Right, Top, Bottom, Center };

// ─────────────────────────────────────────────────────────────────────────────
//  DockNode - internal tree node (not a Widget)
//  Either a split with two children, or a leaf with N tab entries.
//  Rects stored in DockPanel-local coordinates.
// ─────────────────────────────────────────────────────────────────────────────
struct DockNode
{
    bool isSplit = false;

    // ── Split ─────────────────────────────────────────────────────────────
    LayoutDir  splitDir = LayoutDir::Horizontal;
    float      ratio    = 0.5f;   // fraction for first child
    DockNode*  first    = nullptr;
    DockNode*  second   = nullptr;

    // ── Leaf ──────────────────────────────────────────────────────────────
    struct Tab {
        std::string name;
        Widget*     content      = nullptr;
        float       cachedWidth  = 0.f;   // measured during paint
    };
    std::vector<Tab> tabs;
    int currentTab = 0;

    // DockPanel-local geometry (assigned in layoutNode)
    Rect rect = {};

    /// @brief Check if the node has no content.
    bool isEmpty() const { return !isSplit && tabs.empty(); }

    /// @brief Find a tab by name, optionally returning its index.
    DockNode* findTab(const std::string& name, int* outIdx = nullptr);
};

// ─────────────────────────────────────────────────────────────────────────────
//  DockPanel - docking layout system
//
//  Usage:
//    auto* dock = stage->createChild<DockPanel>();
//    dock->setStretch(1);
//    dock->addPanel("Scene",      sceneWidget);
//    dock->addPanel("Properties", propWidget);
//    dock->addPanel("Console",    consoleWidget);
//    dock->splitOff("Properties", DockSide::Right,  0.28f);
//    dock->splitOff("Console",    DockSide::Bottom,  0.30f);
//
//  At runtime the user can:
//     Click a tab to bring that panel to the front
//     Drag a tab and drop it on another panel to merge or split
//     Drag the splitter handles to resize panels
// ─────────────────────────────────────────────────────────────────────────────
class DockPanel : public Widget
{
public:
    DockPanel() = default;
    ~DockPanel() override;

    /// @brief Add a widget as a named panel tab.
    void addPanel(const std::string& name, Widget* content);

    /// @brief Create and add a typed panel.
    template <typename T, typename... Args>
    T* addPanel(const std::string& name, Args&&... args)
    {
        auto* w = new T(std::forward<Args>(args)...);
        addPanel(name, w);
        return w;
    }

    /// @brief Split a panel off to a given side with ratio.
    void splitOff(const std::string& tabName, DockSide side, float ratio = 0.5f);
    /// @brief Close and remove a panel by name.
    void closePanel(const std::string& name);
    /// @brief Bring a panel to front in its tab group.
    void showPanel(const std::string& name);
    /// @brief Move a panel tab into the same leaf as another named panel.
    void moveTabToLeaf(const std::string& sourceName, const std::string& targetName);

    /// @brief Set the tab bar height.
    void  setTabBarHeight(float h) { tabBarH_ = h; markDirty(); }
    /// @brief Get the tab bar height.
    float tabBarHeight()     const { return tabBarH_; }

    /// @brief Set the splitter handle thickness.
    void  setHandleSize(float s)   { handleW_ = s;   markDirty(); }
    /// @brief Get the splitter handle thickness.
    float handleSize()       const { return handleW_; }

    /// @brief Emitted when a panel tab is activated.
    Signal<std::string> panelActivated;
    /// @brief Emitted when a panel is closed.
    Signal<std::string> panelClosed;

    // ── Widget overrides ──────────────────────────────────────────────────
    void  layout() override;
    Vec2f sizeHint() const override { return {200, 200}; }
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;
    void  onMouseLeave() override;

private:
    DockNode* root_    = nullptr;
    float     tabBarH_ = 26.0f;
    float     handleW_ =  5.0f;

    // ── Tab drag state ─────────────────────────────────────────────────────
    struct DragState {
        bool      active  = false;
        DockNode* srcNode = nullptr;
        int       srcIdx  = 0;
        float     startX  = 0, startY = 0;
        float     curX    = 0, curY   = 0;
    } drag_;

    // ── Drop target ────────────────────────────────────────────────────────
    DockNode* dropNode_  = nullptr;
    DockSide  dropZone_  = DockSide::Center;
    bool      dropValid_ = false;

    // ── Splitter drag ──────────────────────────────────────────────────────
    struct SplitDrag {
        bool      active     = false;
        DockNode* node       = nullptr;
        float     startRatio = 0;
        float     startMouse = 0;
    } splitDrag_;

    DockNode* hoveredSplit_ = nullptr;

    // ── Internal helpers ───────────────────────────────────────────────────
    void ensureRoot();
    void freeNode(DockNode* n);

    void layoutNode(DockNode* node, const Rect& r);

    void paintNode(DockNode* node, PaintContext& ctx);
    void paintTabBar(DockNode* leaf, const Rect& barR, PaintContext& ctx);
    void paintDraggedTab(PaintContext& ctx);
    void paintDropOverlay(PaintContext& ctx);

    Rect toScreen(const Rect& local) const;

    DockNode* hitLeaf      (float sx, float sy) const;
    DockNode* hitLeafImpl  (DockNode* n, float ox, float oy, float sx, float sy) const;
    int       hitTab       (DockNode* leaf, float ox, float oy, float sx, float sy) const;
    DockNode* hitSplitter  (float sx, float sy) const;
    DockNode* hitSplitterImpl(DockNode* n, float ox, float oy, float sx, float sy) const;

    DockSide computeDropZone(DockNode* leaf, float ox, float oy,
                             float sx, float sy) const;

    DockNode** findRef  (DockNode* searchFrom, DockNode* target);
    void       performDrop();
    void       pruneNode (DockNode*& slot);
};

} // namespace BuGUI
