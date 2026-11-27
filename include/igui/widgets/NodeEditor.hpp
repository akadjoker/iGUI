#pragma once

#include "Signal.hpp"
#include "Widget.hpp"
#include <ct/vector.hpp>
#include <igui/widgets/String.hpp>

namespace ig
{
namespace retained
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

enum class PinDir
{
    Input,
    Output
};
enum class PinType
{
    Float,
    Vec2,
    Vec3,
    Vec4,
    Color,
    Bool,
    Int,
    String,
    Any
};

struct Pin
{
    String name;
    PinDir dir;
    PinType type;
};

struct Node
{
    String title;
    float x = 0, y = 0;   // position in graph space
    float w = 140, h = 0; // width computed from the texts, never below minW; height computed
    float minW = 140;
    ct::Vector<Pin> pins;
    Color headerColor = Color(70, 90, 130, 255);
    String detail;
    String error;
    TextureHandle image;
    Rect imageUv = Rect(0.0f, 0.0f, 1.0f, 1.0f);
    float imageW = 0, imageH = 0;
    bool selected = false;
    bool collapsed = false;
};

struct Link
{
    int srcNode = -1, srcPin = -1;
    int dstNode = -1, dstPin = -1;
};

/// @brief The cubic Bezier a link follows between two pins, in screen space, as a polyline.
/// Starts exactly at (x0, y0) and ends exactly at (x1, y1) with a horizontal tangent at both;
/// the number of points follows the length on screen and the reach of the handles follows @p zoom.
void buildLinkCurve(float x0, float y0, float x1, float y1, float zoom, ct::Vector<Vec2>& out);

/// @brief Thickness in pixels of a link at @p zoom: scaled with the zoom, never below 1.5.
float linkThickness(float zoom);

class NodeEditor : public Widget
{
public:
    IG_WIDGET_CLASS(NodeEditor, Widget);

    NodeEditor();

    /// @brief Add a node at position (x, y).
    int addNode(const String& title, float x, float y);
    /// @brief Remove a node by ID.
    void removeNode(int nodeId);
    /// @brief Remove all nodes and links.
    void clearAll();

    /// @brief Get the number of nodes.
    int nodeCount() const
    {
        return static_cast<int>(nodes_.size());
    }
    /// @brief Get a mutable node reference.
    Node& node(int id)
    {
        return nodes_[id];
    }
    /// @brief Get a const node reference.
    const Node& node(int id) const
    {
        return nodes_[id];
    }

    /// @brief Set a node's header color.
    void setNodeHeader(int nodeId, const Color& c);

    /// @brief Mark a node as failed: red header and border, @p message in red under the node.
    ///        An empty message clears the mark.
    void setNodeError(int nodeId, const String& message);
    /// @brief Clear the error mark of every node.
    void clearNodeErrors();
    /// @brief Show an image inside the node body, under the pins, @p width x @p height in graph
    /// units
    ///        (scaled with the zoom); the node grows to hold it. A texture with value 0 removes it.
    void setNodeImage(int nodeId, TextureHandle texture, float width, float height,
            Rect uv = Rect(0.0f, 0.0f, 1.0f, 1.0f));

    /// @brief Add a typed pin to a node.
    int addPin(int nodeId, const String& name, PinDir dir, PinType type);

    /// @brief Add a link between two pins.
    int addLink(int srcNode, int srcPin, int dstNode, int dstPin);
    /// @brief Remove a link by ID.
    void removeLink(int linkId);
    /// @brief Check if two pins can be linked.
    bool canLink(int srcNode, int srcPin, int dstNode, int dstPin) const;
    /// @brief Get the number of links.
    int linkCount() const
    {
        return static_cast<int>(links_.size());
    }

    /// @brief Edits become requests: a link made or taken off, a node taken away, a menu asked for
    /// and a link
    ///        let go over empty space are signals (below) and the editor changes nothing itself, so
    ///        that an application that owns the graph edits it and gives the editor the graph
    ///        again.
    void setEditRequests(bool on)
    {
        editRequests_ = on;
        acceptsFocus_ = on;
    }
    bool editRequests() const
    {
        return editRequests_;
    }
    /// @brief Select one node (and no other); no signal.
    void selectNode(int nodeId);
    void clearSelection();
    /// @brief The selected node, -1 when there is none.
    int selectedNode() const;
    /// @brief Show every node: at the next paint, and again when the editor changes size until it
    /// is panned
    ///        or zoomed by hand.
    void fitAll();
    /// @brief Like fitAll, but the zoom does not go below @p minZoom: when the nodes do not fit at
    ///        that zoom the right end of the graph is shown, centred vertically.
    void fitAll(float minZoom);

    /// @brief Set the snap-to-grid spacing.
    void setGridSnap(float s)
    {
        gridSnap_ = s;
    }
    /// @brief Fit all nodes into the view.
    void fitView();
    /// @brief Get the horizontal pan offset.
    float panX() const
    {
        return panX_;
    }
    /// @brief Get the vertical pan offset.
    float panY() const
    {
        return panY_;
    }
    /// @brief Get the current zoom level.
    float zoom() const
    {
        return zoom_;
    }

    /// @brief Set the background color.
    void setBgColor(const Color& c)
    {
        bgColor_ = c;
        markDirty();
    }

    /// @brief Emitted when a node is selected.
    Signal<int> onNodeSelected;
    /// @brief Emitted when a link is created.
    Signal<int, int> onLinkCreated;
    /// @brief Emitted when a link is removed.
    Signal<int> onLinkRemoved;
    /// @brief Emitted when the background is pressed with nothing selected afterwards.
    Signal<> onNothingSelected;
    /// @brief Emitted when a dragged node is let go, with its position.
    Signal<int, float, float> onNodeMoved;
    /// @brief Emitted by a double click on a node.
    Signal<int> onNodeActivated;
    /// @brief Edit requests (setEditRequests): Delete on the selected node.
    Signal<int> onNodeDeleteRequested;
    /// @brief Edit requests: an output pin dropped on an input pin (source node, pin, destination
    /// node, pin).
    Signal<int, int, int, int> onLinkRequested;
    /// @brief Edit requests: a link taken off (source node and pin, destination node and pin).
    Signal<int, int, int, int> onLinkRemoveRequested;
    /// @brief Edit requests: a link let go over empty space (node, pin, screen x and y, graph x and
    /// y).
    Signal<int, int, float, float, float, float> onLinkDropped;
    /// @brief Edit requests: right click on the background (screen x and y, graph x and y).
    Signal<float, float, float, float> onContextRequested;
    /// @brief Edit requests: right click on a node (node, screen x and y).
    Signal<int, float, float> onNodeContextRequested;

    // ── Overrides ────────────────────────────────────────────────────────
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onKeyPress(KeyEvent& e) override;

private:
    ct::Vector<Node> nodes_;
    ct::Vector<Link> links_;
    Color bgColor_ = Color(0, 0, 0, 0); // alpha 0: follow the theme's editor.nodeBg

    // View transform
    float panX_ = 0, panY_ = 0;
    float zoom_ = 1.0f;
    float gridSnap_ = 16.0f;
    bool editRequests_ = false;
    bool fitPending_ = false;
    float fitMinZoom_ = 0.0f;
    bool autoFit_ = false;
    float fitW_ = 0, fitH_ = 0;

    // Interaction state
    enum class DragMode
    {
        None,
        Pan,
        MoveNode,
        ConnectPin
    };
    DragMode dragMode_ = DragMode::None;
    float dragStartX_ = 0, dragStartY_ = 0;
    float panStartX_ = 0, panStartY_ = 0;
    int dragNode_ = -1;
    bool dragMoved_ = false;
    float nodeStartX_ = 0, nodeStartY_ = 0;
    int connSrcNode_ = -1, connSrcPin_ = -1;
    float connEndX_ = 0, connEndY_ = 0;

    // Layout constants
    static constexpr float kNodeHeaderH = 24;
    static constexpr float kPinH = 20;
    static constexpr float kPinRadius = 5;

    // ── Helpers ──────────────────────────────────────────────────────────
    float gx(float x) const; // graph → screen
    float gy(float y) const;
    float sx(float x) const; // screen → graph
    float sy(float y) const;

    float nodeHeight(const Node& n) const;
    float requiredWidth(PaintContext& ctx, const Node& n) const;
    bool updateNodeWidths(PaintContext& ctx);
    float pinsTop(const Node& n) const;
    float imageTop(const Node& n) const;
    void startFit();
    void getPinPos(int nodeId, int pinIdx, float& px, float& py) const;
    Color pinColor(PinType t) const;

    struct NodeHit
    {
        int node = -1;
        int pin = -1;
        bool onHeader = false;
    };
    NodeHit hitTest(float mx, float my) const;

    void paintGrid(PaintContext& ctx, const Rect& b);
    void paintLinks(PaintContext& ctx);
    void paintNodes(PaintContext& ctx);
    void paintDragLink(PaintContext& ctx);
};

} // namespace retained
} // namespace ig
