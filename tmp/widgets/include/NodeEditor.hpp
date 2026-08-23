#pragma once

#include "Widget.hpp"
#include "Signal.hpp"
#include <vector>
#include <string>

namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  NodeEditor — Visual node graph (shaders, AI, scripting)
//
//  Features:
//  - Nodes with typed input/output pins
//  - Drag to connect pins (type-checked by colour)
//  - Pan (middle-drag or left-drag on background) + zoom (scroll wheel)
//  - Selection with highlight
//  - Node drag with optional snap-to-grid
//  - Smooth cubic-Bezier connection lines
//
//  Usage:
//    auto* ed = parent->createChild<NodeEditor>();
//    int n1 = ed->addNode("Add", 100, 100);
//    ed->addPin(n1, "A",      PinDir::Input,  PinType::Float);
//    ed->addPin(n1, "B",      PinDir::Input,  PinType::Float);
//    ed->addPin(n1, "Result", PinDir::Output, PinType::Float);
//    ed->addLink(n1, 2, n2, 0);
// ═════════════════════════════════════════════════════════════════════════════

enum class PinDir  { Input, Output };
enum class PinType { Float, Vec2, Vec3, Vec4, Color, Bool, Int, String, Any };

struct Pin {
    std::string name;
    PinDir      dir;
    PinType     type;
};

struct Node {
    std::string      title;
    float            x = 0, y = 0;     // position in graph space
    float            w = 140, h = 0;   // width fixed, height computed
    std::vector<Pin> pins;
    Color            headerColor = Color(70, 90, 130, 255);
    bool             selected    = false;
    bool             collapsed   = false;
};

struct Link {
    int srcNode = -1, srcPin = -1;
    int dstNode = -1, dstPin = -1;
};

class NodeEditor : public Widget
{
public:
    NodeEditor();

    /// @brief Add a node at position (x, y).
    int  addNode(const std::string& title, float x, float y);
    /// @brief Remove a node by ID.
    void removeNode(int nodeId);
    /// @brief Remove all nodes and links.
    void clearAll();

    /// @brief Get the number of nodes.
    int         nodeCount() const { return static_cast<int>(nodes_.size()); }
    /// @brief Get a mutable node reference.
    Node&       node(int id)       { return nodes_[id]; }
    /// @brief Get a const node reference.
    const Node& node(int id) const { return nodes_[id]; }

    /// @brief Set a node's header color.
    void setNodeHeader(int nodeId, const Color& c);

    /// @brief Add a typed pin to a node.
    int addPin(int nodeId, const std::string& name, PinDir dir, PinType type);

    /// @brief Add a link between two pins.
    int  addLink(int srcNode, int srcPin, int dstNode, int dstPin);
    /// @brief Remove a link by ID.
    void removeLink(int linkId);
    /// @brief Check if two pins can be linked.
    bool canLink(int srcNode, int srcPin, int dstNode, int dstPin) const;
    /// @brief Get the number of links.
    int  linkCount() const { return static_cast<int>(links_.size()); }

    /// @brief Set the snap-to-grid spacing.
    void  setGridSnap(float s) { gridSnap_ = s; }
    /// @brief Fit all nodes into the view.
    void  fitView();
    /// @brief Get the horizontal pan offset.
    float panX() const { return panX_; }
    /// @brief Get the vertical pan offset.
    float panY() const { return panY_; }
    /// @brief Get the current zoom level.
    float zoom() const { return zoom_; }

    /// @brief Set the background color.
    void setBgColor(const Color& c) { bgColor_ = c; markDirty(); }

    /// @brief Emitted when a node is selected.
    Signal<int>      onNodeSelected;
    /// @brief Emitted when a link is created.
    Signal<int, int> onLinkCreated;
    /// @brief Emitted when a link is removed.
    Signal<int>      onLinkRemoved;

    // ── Overrides ────────────────────────────────────────────────────────
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;

private:
    std::vector<Node> nodes_;
    std::vector<Link> links_;
    Color bgColor_ = Color(32, 34, 38, 255);

    // View transform
    float panX_ = 0, panY_ = 0;
    float zoom_ = 1.0f;
    float gridSnap_ = 16.0f;

    // Interaction state
    enum class DragMode { None, Pan, MoveNode, ConnectPin };
    DragMode dragMode_   = DragMode::None;
    float    dragStartX_ = 0, dragStartY_ = 0;
    float    panStartX_  = 0, panStartY_ = 0;
    int      dragNode_   = -1;
    float    nodeStartX_ = 0, nodeStartY_ = 0;
    int      connSrcNode_ = -1, connSrcPin_ = -1;
    float    connEndX_ = 0, connEndY_ = 0;

    // Layout constants
    static constexpr float kNodeHeaderH = 24;
    static constexpr float kPinH        = 20;
    static constexpr float kPinRadius   = 5;

    // ── Helpers ──────────────────────────────────────────────────────────
    float gx(float x) const;   // graph → screen
    float gy(float y) const;
    float sx(float x) const;   // screen → graph
    float sy(float y) const;

    float nodeHeight(const Node& n) const;
    void  getPinPos(int nodeId, int pinIdx, float& px, float& py) const;
    Color pinColor(PinType t) const;

    struct NodeHit { int node = -1; int pin = -1; bool onHeader = false; };
    NodeHit hitTest(float mx, float my) const;

    void paintGrid(PaintContext& ctx, const Rect& b);
    void paintLinks(PaintContext& ctx);
    void paintNodes(PaintContext& ctx);
    void paintDragLink(PaintContext& ctx);
};

} // namespace BuGUI
