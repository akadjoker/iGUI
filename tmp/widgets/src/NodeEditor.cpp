#include "NodeEditor.hpp"
#include "Theme.hpp"
#include <cmath>
#include <algorithm>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
//  Font helper — sets up size / batch / colour, returns ascender for baseline
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    inline float setupFont(PaintContext& ctx, const Color& color)
    {
        auto& t = Theme::instance();
        ctx.font.SetFontSize(t.fontSize);
        ctx.font.SetBatch(&ctx.text);
        ctx.font.SetColor(color);
        return ctx.font.GetAscender();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  NodeEditor
// ═════════════════════════════════════════════════════════════════════════════

NodeEditor::NodeEditor() {}

// ─────────────────────────────────────────────────────────────────────────────
//  Node / Pin / Link management
// ─────────────────────────────────────────────────────────────────────────────

int NodeEditor::addNode(const std::string& title, float x, float y)
{
    Node n;
    n.title = title;
    n.x = x; n.y = y;
    nodes_.push_back(std::move(n));
    markDirty();
    return static_cast<int>(nodes_.size()) - 1;
}

void NodeEditor::removeNode(int nodeId)
{
    if (nodeId < 0 || nodeId >= static_cast<int>(nodes_.size())) return;
    // Remove links referencing this node
    links_.erase(std::remove_if(links_.begin(), links_.end(),
        [nodeId](const Link& l) { return l.srcNode == nodeId || l.dstNode == nodeId; }),
        links_.end());
    nodes_.erase(nodes_.begin() + nodeId);
    // Fix link indices after removal
    for (auto& l : links_) {
        if (l.srcNode > nodeId) --l.srcNode;
        if (l.dstNode > nodeId) --l.dstNode;
    }
    markDirty();
}

void NodeEditor::clearAll()
{
    nodes_.clear();
    links_.clear();
    markDirty();
}

void NodeEditor::setNodeHeader(int nodeId, const Color& c)
{
    if (nodeId < 0 || nodeId >= static_cast<int>(nodes_.size())) return;
    nodes_[nodeId].headerColor = c;
    markDirty();
}

int NodeEditor::addPin(int nodeId, const std::string& name, PinDir dir, PinType type)
{
    if (nodeId < 0 || nodeId >= static_cast<int>(nodes_.size())) return -1;
    auto& n = nodes_[nodeId];
    n.pins.push_back({name, dir, type});
    n.h = nodeHeight(n);
    markDirty();
    return static_cast<int>(n.pins.size()) - 1;
}

int NodeEditor::addLink(int srcNode, int srcPin, int dstNode, int dstPin)
{
    if (!canLink(srcNode, srcPin, dstNode, dstPin)) return -1;
    links_.push_back({srcNode, srcPin, dstNode, dstPin});
    markDirty();
    int id = static_cast<int>(links_.size()) - 1;
    onLinkCreated.emit(id, (srcNode << 16) | dstNode);
    return id;
}

void NodeEditor::removeLink(int linkId)
{
    if (linkId < 0 || linkId >= static_cast<int>(links_.size())) return;
    links_.erase(links_.begin() + linkId);
    markDirty();
    onLinkRemoved.emit(linkId);
}

bool NodeEditor::canLink(int srcNode, int srcPin, int dstNode, int dstPin) const
{
    if (srcNode < 0 || srcNode >= static_cast<int>(nodes_.size())) return false;
    if (dstNode < 0 || dstNode >= static_cast<int>(nodes_.size())) return false;
    if (srcNode == dstNode) return false;
    const auto& sn = nodes_[srcNode];
    const auto& dn = nodes_[dstNode];
    if (srcPin < 0 || srcPin >= static_cast<int>(sn.pins.size())) return false;
    if (dstPin < 0 || dstPin >= static_cast<int>(dn.pins.size())) return false;
    if (sn.pins[srcPin].dir != PinDir::Output) return false;
    if (dn.pins[dstPin].dir != PinDir::Input)  return false;
    // Type compatibility: Any matches anything, otherwise must match
    auto st = sn.pins[srcPin].type, dt = dn.pins[dstPin].type;
    if (st != PinType::Any && dt != PinType::Any && st != dt) return false;
    // No duplicate link on same input pin
    for (const auto& l : links_)
        if (l.dstNode == dstNode && l.dstPin == dstPin) return false;
    return true;
}

void NodeEditor::fitView()
{
    if (nodes_.empty()) { panX_ = 0; panY_ = 0; zoom_ = 1; return; }
    float cx = 0, cy = 0;
    for (const auto& n : nodes_) {
        cx += n.x + n.w * 0.5f;
        cy += n.y + nodeHeight(n) * 0.5f;
    }
    cx /= static_cast<float>(nodes_.size());
    cy /= static_cast<float>(nodes_.size());
    Rect b = absoluteRect();
    panX_ = cx - (b.w * 0.5f) / zoom_;
    panY_ = cy - (b.h * 0.5f) / zoom_;
    markDirty();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Coordinate transforms
// ─────────────────────────────────────────────────────────────────────────────

float NodeEditor::gx(float x) const { Rect b = absoluteRect(); return b.x + (x - panX_) * zoom_; }
float NodeEditor::gy(float y) const { Rect b = absoluteRect(); return b.y + (y - panY_) * zoom_; }
float NodeEditor::sx(float x) const { Rect b = absoluteRect(); return (x - b.x) / zoom_ + panX_; }
float NodeEditor::sy(float y) const { Rect b = absoluteRect(); return (y - b.y) / zoom_ + panY_; }

float NodeEditor::nodeHeight(const Node& n) const
{
    int inputs = 0, outputs = 0;
    for (const auto& p : n.pins) {
        if (p.dir == PinDir::Input) ++inputs;
        else                        ++outputs;
    }
    return kNodeHeaderH + std::max(inputs, outputs) * kPinH + 8;
}

void NodeEditor::getPinPos(int nodeId, int pinIdx, float& px, float& py) const
{
    const auto& n   = nodes_[nodeId];
    const auto& pin = n.pins[pinIdx];

    int idx = 0;
    for (int i = 0; i < pinIdx; ++i)
        if (n.pins[i].dir == pin.dir) ++idx;

    if (pin.dir == PinDir::Input) {
        px = gx(n.x);
        py = gy(n.y + kNodeHeaderH + idx * kPinH + kPinH * 0.5f);
    } else {
        px = gx(n.x + n.w);
        py = gy(n.y + kNodeHeaderH + idx * kPinH + kPinH * 0.5f);
    }
}

Color NodeEditor::pinColor(PinType t) const
{
    switch (t) {
        case PinType::Float:  return Color(160, 200, 100, 255);
        case PinType::Vec2:   return Color(100, 200, 160, 255);
        case PinType::Vec3:   return Color(100, 160, 220, 255);
        case PinType::Vec4:   return Color(180, 100, 220, 255);
        case PinType::Color:  return Color(255, 200,  60, 255);
        case PinType::Bool:   return Color(220, 100, 100, 255);
        case PinType::Int:    return Color(100, 220, 200, 255);
        case PinType::String: return Color(200, 180, 140, 255);
        case PinType::Any:    return Color(180, 180, 180, 255);
    }
    return Color(180, 180, 180, 255);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Hit testing
// ─────────────────────────────────────────────────────────────────────────────

NodeEditor::NodeHit NodeEditor::hitTest(float mx, float my) const
{
    const float pinR = std::max(8.0f, kPinRadius * zoom_ + 4.0f);
    const float pinR2 = pinR * pinR;

    // Iterate in reverse so topmost (last-drawn) nodes are tested first
    for (int i = static_cast<int>(nodes_.size()) - 1; i >= 0; --i) {
        const auto& n = nodes_[i];
        const float nh = nodeHeight(n);

        // Pins
        for (int pi = 0; pi < static_cast<int>(n.pins.size()); ++pi) {
            float px, py;
            getPinPos(i, pi, px, py);
            float dx = mx - px, dy = my - py;
            if (dx * dx + dy * dy < pinR2)
                return {i, pi, false};
        }

        // Node body
        float nx = gx(n.x), ny = gy(n.y);
        float nw = n.w * zoom_, nhz = nh * zoom_;
        if (mx >= nx && mx <= nx + nw && my >= ny && my <= ny + nhz) {
            bool onHdr = (my < ny + kNodeHeaderH * zoom_);
            return {i, -1, onHdr};
        }
    }
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
//  Mouse interaction
// ─────────────────────────────────────────────────────────────────────────────

void NodeEditor::onMousePress(MouseEvent& e)
{
    // ── Middle button → pan ──────────────────────────────────────────────
    if (e.button == 2) {
        dragMode_   = DragMode::Pan;
        dragStartX_ = e.x;  dragStartY_ = e.y;
        panStartX_  = panX_; panStartY_ = panY_;
        e.consumed  = true;
        return;
    }

    // ── Right button → delete nearest link ───────────────────────────────
    if (e.button == 1) {
        float bestD2 = 12.0f * 12.0f;
        int   del    = -1;
        for (int li = 0; li < static_cast<int>(links_.size()); ++li) {
            float x0, y0, x1, y1;
            getPinPos(links_[li].srcNode, links_[li].srcPin, x0, y0);
            getPinPos(links_[li].dstNode, links_[li].dstPin, x1, y1);
            float dist = std::max(30.0f, std::fabs(x1 - x0) * 0.5f);
            float cx0 = x0 + dist, cx1 = x1 - dist;
            // Sample Bezier at 16 points
            for (int si = 1; si <= 16; ++si) {
                float t  = float(si) / 16.0f, it = 1.0f - t;
                float bx = it*it*it*x0 + 3*it*it*t*cx0 + 3*it*t*t*cx1 + t*t*t*x1;
                float by = it*it*it*y0 + 3*it*it*t*y0  + 3*it*t*t*y1  + t*t*t*y1;
                float d2 = (e.x - bx) * (e.x - bx) + (e.y - by) * (e.y - by);
                if (d2 < bestD2) { bestD2 = d2; del = li; }
            }
        }
        if (del >= 0) removeLink(del);
        e.consumed = true;
        return;
    }

    if (e.button != 0) return;

    // ── Left button ──────────────────────────────────────────────────────
    auto hit = hitTest(e.x, e.y);

    // Pin hit → start connection (or detach existing input link)
    if (hit.pin >= 0) {
        auto& hitPin = nodes_[hit.node].pins[hit.pin];

        // If dragging from an input that already has a link → detach & re-drag
        if (hitPin.dir == PinDir::Input) {
            for (int li = 0; li < static_cast<int>(links_.size()); ++li) {
                if (links_[li].dstNode == hit.node && links_[li].dstPin == hit.pin) {
                    int sn = links_[li].srcNode;
                    int sp = links_[li].srcPin;
                    removeLink(li);
                    dragMode_    = DragMode::ConnectPin;
                    connSrcNode_ = sn;
                    connSrcPin_  = sp;
                    connEndX_    = e.x; connEndY_ = e.y;
                    e.consumed   = true;
                    return;
                }
            }
        }

        dragMode_    = DragMode::ConnectPin;
        connSrcNode_ = hit.node;
        connSrcPin_  = hit.pin;
        connEndX_    = e.x; connEndY_ = e.y;
        e.consumed   = true;
        return;
    }

    // Node body hit → select + start move
    if (hit.node >= 0) {
        for (auto& n : nodes_) n.selected = false;
        nodes_[hit.node].selected = true;
        onNodeSelected.emit(hit.node);

        dragMode_   = DragMode::MoveNode;
        dragNode_   = hit.node;
        dragStartX_ = e.x;  dragStartY_ = e.y;
        nodeStartX_ = nodes_[hit.node].x;
        nodeStartY_ = nodes_[hit.node].y;
        e.consumed  = true;
        return;
    }

    // Background → deselect + pan
    for (auto& n : nodes_) n.selected = false;
    dragMode_   = DragMode::Pan;
    dragStartX_ = e.x;  dragStartY_ = e.y;
    panStartX_  = panX_; panStartY_ = panY_;
    e.consumed  = true;
}

void NodeEditor::onMouseRelease(MouseEvent& e)
{
    if (dragMode_ == DragMode::ConnectPin) {
        auto hit = hitTest(e.x, e.y);
        if (hit.pin >= 0 && hit.node != connSrcNode_) {
            auto& srcP = nodes_[connSrcNode_].pins[connSrcPin_];
            if (srcP.dir == PinDir::Output)
                addLink(connSrcNode_, connSrcPin_, hit.node, hit.pin);
            else
                addLink(hit.node, hit.pin, connSrcNode_, connSrcPin_);
        }
    }

    dragMode_    = DragMode::None;
    dragNode_    = -1;
    connSrcNode_ = -1;
    connSrcPin_  = -1;
    markDirty();
    e.consumed = true;
}

void NodeEditor::onMouseMove(MouseEvent& e)
{
    if (dragMode_ == DragMode::Pan) {
        float dx = (e.x - dragStartX_) / zoom_;
        float dy = (e.y - dragStartY_) / zoom_;
        panX_ = panStartX_ - dx;
        panY_ = panStartY_ - dy;
        markDirty();
        e.consumed = true;
    }
    else if (dragMode_ == DragMode::MoveNode && dragNode_ >= 0) {
        float dx = (e.x - dragStartX_) / zoom_;
        float dy = (e.y - dragStartY_) / zoom_;
        float nx = nodeStartX_ + dx;
        float ny = nodeStartY_ + dy;
        if (gridSnap_ > 0) {
            nx = std::round(nx / gridSnap_) * gridSnap_;
            ny = std::round(ny / gridSnap_) * gridSnap_;
        }
        nodes_[dragNode_].x = nx;
        nodes_[dragNode_].y = ny;
        markDirty();
        e.consumed = true;
    }
    else if (dragMode_ == DragMode::ConnectPin) {
        connEndX_ = e.x;
        connEndY_ = e.y;
        markDirty();
        e.consumed = true;
    }
}

void NodeEditor::onMouseScroll(MouseEvent& e)
{
    float oldZoom = zoom_;
    float factor  = (e.scrollY > 0) ? 1.1f : 0.9f;
    zoom_ = std::clamp(zoom_ * factor, 0.2f, 4.0f);

    // Zoom towards mouse position
    Rect b  = absoluteRect();
    float mx = (e.x - b.x) / oldZoom + panX_;
    float my = (e.y - b.y) / oldZoom + panY_;
    panX_ = mx - (e.x - b.x) / zoom_;
    panY_ = my - (e.y - b.y) / zoom_;

    markDirty();
    e.consumed = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Painting — main entry
// ─────────────────────────────────────────────────────────────────────────────

void NodeEditor::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect b = absoluteRect();

    ctx.pushClip(b);

    // Background
    ctx.fill.SetColor(bgColor_.r, bgColor_.g, bgColor_.b, bgColor_.a);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    paintGrid(ctx, b);
    paintLinks(ctx);
    paintNodes(ctx);
    if (dragMode_ == DragMode::ConnectPin)
        paintDragLink(ctx);

    // Thin border
    ctx.fill.SetColor(50, 52, 58, 255);
    ctx.fillRect(b.x,           b.y,           b.w, 1);
    ctx.fillRect(b.x,           b.y + b.h - 1, b.w, 1);
    ctx.fillRect(b.x,           b.y,           1,   b.h);
    ctx.fillRect(b.x + b.w - 1, b.y,           1,   b.h);

    ctx.popClip();

    Widget::paint(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Grid — minor + major lines
// ─────────────────────────────────────────────────────────────────────────────

void NodeEditor::paintGrid(PaintContext& ctx, const Rect& b)
{
    float step = gridSnap_ * zoom_;
    if (step < 8) step *= std::ceil(8.0f / step);

    float offX = std::fmod(-(panX_ * zoom_), step);
    float offY = std::fmod(-(panY_ * zoom_), step);
    if (offX < 0) offX += step;
    if (offY < 0) offY += step;

    // Minor grid
    ctx.fill.SetColor(40, 42, 48, 255);
    for (float x = b.x + offX; x < b.x + b.w; x += step)
        ctx.fillRect(x, b.y, 1, b.h);
    for (float y = b.y + offY; y < b.y + b.h; y += step)
        ctx.fillRect(b.x, y, b.w, 1);

    // Major grid (every 4 cells)
    float major = step * 4;
    float mOffX = std::fmod(-(panX_ * zoom_), major);
    float mOffY = std::fmod(-(panY_ * zoom_), major);
    if (mOffX < 0) mOffX += major;
    if (mOffY < 0) mOffY += major;

    ctx.fill.SetColor(50, 52, 58, 255);
    for (float x = b.x + mOffX; x < b.x + b.w; x += major)
        ctx.fillRect(x, b.y, 1, b.h);
    for (float y = b.y + mOffY; y < b.y + b.h; y += major)
        ctx.fillRect(b.x, y, b.w, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Links — cubic Bezier rendered as triangle strips for line thickness
// ─────────────────────────────────────────────────────────────────────────────

void NodeEditor::paintLinks(PaintContext& ctx)
{
    constexpr int   kSegs  = 24;
    constexpr float kThick = 2.0f;

    for (const auto& link : links_) {
        float x0, y0, x1, y1;
        getPinPos(link.srcNode, link.srcPin, x0, y0);
        getPinPos(link.dstNode, link.dstPin, x1, y1);

        Color col = pinColor(nodes_[link.srcNode].pins[link.srcPin].type);
        ctx.fill.SetColor(col.r, col.g, col.b, 200);

        float dist = std::max(30.0f, std::fabs(x1 - x0) * 0.5f);
        float cx0 = x0 + dist, cy0 = y0;
        float cx1 = x1 - dist, cy1 = y1;

        float prevX = x0, prevY = y0;
        for (int i = 1; i <= kSegs; ++i) {
            float t  = float(i) / kSegs, it = 1.0f - t;
            float bx = it*it*it*x0 + 3*it*it*t*cx0 + 3*it*t*t*cx1 + t*t*t*x1;
            float by = it*it*it*y0 + 3*it*it*t*cy0 + 3*it*t*t*cy1 + t*t*t*y1;

            float dx = bx - prevX, dy = by - prevY;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.5f) {
                float nx = -dy / len * kThick * 0.5f;
                float ny =  dx / len * kThick * 0.5f;
                ctx.fillTriangle(prevX + nx, prevY + ny, prevX - nx, prevY - ny, bx - nx, by - ny);
                ctx.fillTriangle(prevX + nx, prevY + ny, bx - nx, by - ny, bx + nx, by + ny);
            }
            prevX = bx; prevY = by;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Nodes — shadow, body, header, border, title, pins, pin labels
// ─────────────────────────────────────────────────────────────────────────────

void NodeEditor::paintNodes(PaintContext& ctx)
{
    const float asc = setupFont(ctx, Color(230, 232, 240, 255));

    for (int ni = 0; ni < static_cast<int>(nodes_.size()); ++ni) {
        const auto& n = nodes_[ni];
        float nh   = nodeHeight(n);
        float nx   = gx(n.x), ny = gy(n.y);
        float nw   = n.w * zoom_;
        float nhz  = nh  * zoom_;
        float hdrH = kNodeHeaderH * zoom_;

        // ── Shadow ───────────────────────────────────────────────────────
        ctx.fill.SetColor(0, 0, 0, 50);
        ctx.fillRect(nx + 3, ny + 3, nw, nhz);

        // ── Body ─────────────────────────────────────────────────────────
        Color body = n.selected ? Color(50, 55, 65, 240) : Color(40, 42, 48, 240);
        ctx.fill.SetColor(body.r, body.g, body.b, body.a);
        ctx.fillRect(nx, ny, nw, nhz);

        // ── Header ───────────────────────────────────────────────────────
        Color hdr = n.headerColor;
        if (n.selected) {
            hdr.r = static_cast<uint8_t>(std::min(255, hdr.r + 30));
            hdr.g = static_cast<uint8_t>(std::min(255, hdr.g + 30));
            hdr.b = static_cast<uint8_t>(std::min(255, hdr.b + 30));
        }
        ctx.fill.SetColor(hdr.r, hdr.g, hdr.b, hdr.a);
        ctx.fillRect(nx, ny, nw, hdrH);

        // ── Title (vertically centred in header, baseline-correct) ───────
        ctx.font.SetColor(Color(230, 232, 240, 255));
        {
            float textH  = ctx.font.GetFontSize();
            float titleY = ny + (hdrH - textH) * 0.5f + asc;
            ctx.font.Print(n.title.c_str(), nx + 8 * zoom_, titleY);
        }

        // ── Border ───────────────────────────────────────────────────────
        Color border = n.selected ? Color(255, 200, 60, 255) : Color(60, 62, 70, 255);
        ctx.fill.SetColor(border.r, border.g, border.b, border.a);
        ctx.fillRect(nx,          ny,           nw, 1);
        ctx.fillRect(nx,          ny + nhz - 1, nw, 1);
        ctx.fillRect(nx,          ny,           1,  nhz);
        ctx.fillRect(nx + nw - 1, ny,           1,  nhz);

        // ── Pins ─────────────────────────────────────────────────────────
        for (int pi = 0; pi < static_cast<int>(n.pins.size()); ++pi) {
            float px, py;
            getPinPos(ni, pi, px, py);
            Color pc = pinColor(n.pins[pi].type);

            // Pin circle (outline ring + solid fill)
            ctx.fill.SetColor(220, 222, 230, 200);
            ctx.fillCircle(px, py, kPinRadius * zoom_ + 1.0f);
            ctx.fill.SetColor(pc.r, pc.g, pc.b, 255);
            ctx.fillCircle(px, py, kPinRadius * zoom_);

            // Pin label — vertically centred on pin, baseline-aware
            ctx.font.SetColor(Color(190, 192, 200, 220));
            float labelY = py - ctx.font.GetFontSize() * 0.5f + asc;

            if (n.pins[pi].dir == PinDir::Input) {
                ctx.font.Print(n.pins[pi].name.c_str(),
                    px + (kPinRadius + 4) * zoom_, labelY);
            } else {
                // Right-align output labels using measured text width
                float tw = ctx.font.GetTextWidth(n.pins[pi].name.c_str());
                ctx.font.Print(n.pins[pi].name.c_str(),
                    px - (kPinRadius + 4) * zoom_ - tw, labelY);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Drag-link — semi-transparent Bezier while connecting
// ─────────────────────────────────────────────────────────────────────────────

void NodeEditor::paintDragLink(PaintContext& ctx)
{
    if (connSrcNode_ < 0 || connSrcPin_ < 0) return;

    float x0, y0;
    getPinPos(connSrcNode_, connSrcPin_, x0, y0);
    float x1 = connEndX_, y1 = connEndY_;

    Color col = pinColor(nodes_[connSrcNode_].pins[connSrcPin_].type);
    ctx.fill.SetColor(col.r, col.g, col.b, 180);

    float dist = std::max(30.0f, std::fabs(x1 - x0) * 0.5f);
    bool isOutput = (nodes_[connSrcNode_].pins[connSrcPin_].dir == PinDir::Output);
    float cx0 = isOutput ? x0 + dist : x0 - dist;
    float cx1 = isOutput ? x1 - dist : x1 + dist;

    constexpr int   kSegs  = 20;
    constexpr float kThick = 1.5f;

    float prevX = x0, prevY = y0;
    for (int i = 1; i <= kSegs; ++i) {
        float t  = float(i) / kSegs, it = 1.0f - t;
        float bx = it*it*it*x0 + 3*it*it*t*cx0 + 3*it*t*t*cx1 + t*t*t*x1;
        float by = it*it*it*y0 + 3*it*it*t*y0  + 3*it*t*t*y1  + t*t*t*y1;

        float dx = bx - prevX, dy = by - prevY;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len > 0.5f) {
            float nx = -dy / len * kThick * 0.5f;
            float ny =  dx / len * kThick * 0.5f;
            ctx.fillTriangle(prevX + nx, prevY + ny, prevX - nx, prevY - ny, bx - nx, by - ny);
            ctx.fillTriangle(prevX + nx, prevY + ny, bx - nx, by - ny, bx + nx, by + ny);
        }
        prevX = bx; prevY = by;
    }
}
