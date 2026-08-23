#include "DockPanel.hpp"

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

inline float setupFont(PaintContext& ctx, const Color& color, float size = 12.f)
{
    ctx.font.SetFontSize(size);
    ctx.font.SetBatch(&ctx.text);
    ctx.font.SetColor(color);
    return ctx.font.GetAscender();
}

} // anon

// ─────────────────────────────────────────────────────────────────────────────
//  DockNode helpers
// ─────────────────────────────────────────────────────────────────────────────

DockNode* DockNode::findTab(const std::string& name, int* outIdx)
{
    if (isSplit) {
        DockNode* r = first  ? first->findTab(name, outIdx)  : nullptr;
        if (r) return r;
        return           second ? second->findTab(name, outIdx) : nullptr;
    }
    for (int i = 0; i < (int)tabs.size(); ++i) {
        if (tabs[i].name == name) {
            if (outIdx) *outIdx = i;
            return this;
        }
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  DockPanel - lifecycle
// ─────────────────────────────────────────────────────────────────────────────

DockPanel::~DockPanel()
{
    freeNode(root_);
}

void DockPanel::freeNode(DockNode* n)
{
    if (!n) return;
    freeNode(n->first);
    freeNode(n->second);
    delete n;
}

void DockPanel::ensureRoot()
{
    if (!root_) {
        root_ = new DockNode();
        root_->isSplit = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Panel management
// ─────────────────────────────────────────────────────────────────────────────

void DockPanel::addPanel(const std::string& name, Widget* content)
{
    ensureRoot();
    addChild(content);
    content->setVisible(false);

    DockNode* leaf = root_;
    while (leaf->isSplit) leaf = leaf->first;

    DockNode::Tab t;
    t.name    = name;
    t.content = content;
    leaf->tabs.push_back(t);

    if (leaf->tabs.size() == 1) {
        leaf->currentTab = 0;
        content->setVisible(true);
    }
    markDirty();
}

void DockPanel::splitOff(const std::string& tabName, DockSide side, float ratio)
{
    ensureRoot();
    int tabIdx = -1;
    DockNode* leaf = root_->findTab(tabName, &tabIdx);
    if (!leaf || tabIdx < 0) return;
    if (leaf->tabs.size() < 2) return;

    DockNode::Tab movedTab = leaf->tabs[tabIdx];
    leaf->tabs.erase(leaf->tabs.begin() + tabIdx);
    if (leaf->currentTab >= (int)leaf->tabs.size())
        leaf->currentTab = (int)leaf->tabs.size() - 1;
    if (leaf->currentTab >= 0)
        leaf->tabs[leaf->currentTab].content->setVisible(true);

    DockNode* newLeaf = new DockNode();
    newLeaf->isSplit = false;
    newLeaf->tabs.push_back(movedTab);
    newLeaf->currentTab = 0;
    movedTab.content->setVisible(true);

    DockNode* split = new DockNode();
    split->isSplit = true;
    split->ratio   = 1.0f - ratio;

    bool firstIsNew = (side == DockSide::Left || side == DockSide::Top);
    bool horizontal = (side == DockSide::Left || side == DockSide::Right);
    split->splitDir = horizontal ? LayoutDir::Horizontal : LayoutDir::Vertical;

    if (firstIsNew) {
        split->ratio  = ratio;
        split->first  = newLeaf;
        split->second = leaf;
    } else {
        split->ratio  = 1.0f - ratio;
        split->first  = leaf;
        split->second = newLeaf;
    }

    if (root_ == leaf) {
        root_ = split;
    } else {
        DockNode** slot = findRef(root_, leaf);
        if (slot) *slot = split;
    }
    markDirty();
}

void DockPanel::closePanel(const std::string& name)
{
    ensureRoot();
    int tabIdx = -1;
    DockNode* leaf = root_->findTab(name, &tabIdx);
    if (!leaf || tabIdx < 0) return;

    Widget* w = leaf->tabs[tabIdx].content;
    leaf->tabs.erase(leaf->tabs.begin() + tabIdx);
    if (leaf->currentTab >= (int)leaf->tabs.size())
        leaf->currentTab = (int)leaf->tabs.size() - 1;
    for (auto& t : leaf->tabs) t.content->setVisible(false);
    if (leaf->currentTab >= 0 && !leaf->tabs.empty())
        leaf->tabs[leaf->currentTab].content->setVisible(true);

    removeChild(w);
    delete w;

    pruneNode(root_);
    panelClosed.emit(name);
    markDirty();
}

void DockPanel::moveTabToLeaf(const std::string& sourceName, const std::string& targetName)
{
    ensureRoot();
    int srcIdx = -1;
    DockNode* srcLeaf = root_->findTab(sourceName, &srcIdx);
    if (!srcLeaf || srcIdx < 0) return;
    int dstIdx = -1;
    DockNode* dstLeaf = root_->findTab(targetName, &dstIdx);
    if (!dstLeaf || dstIdx < 0 || srcLeaf == dstLeaf) return;

    DockNode::Tab movedTab = srcLeaf->tabs[srcIdx];
    movedTab.content->setVisible(false);

    srcLeaf->tabs.erase(srcLeaf->tabs.begin() + srcIdx);
    if (srcLeaf->currentTab >= (int)srcLeaf->tabs.size())
        srcLeaf->currentTab = (int)srcLeaf->tabs.size() - 1;
    if (srcLeaf->currentTab >= 0 && !srcLeaf->tabs.empty())
        srcLeaf->tabs[srcLeaf->currentTab].content->setVisible(true);

    dstLeaf->tabs.push_back(movedTab);

    pruneNode(root_);
    markDirty();
}

void DockPanel::showPanel(const std::string& name)
{
    ensureRoot();
    int idx = -1;
    DockNode* leaf = root_->findTab(name, &idx);
    if (!leaf || idx < 0) return;

    for (auto& t : leaf->tabs) t.content->setVisible(false);
    leaf->currentTab = idx;
    leaf->tabs[idx].content->setVisible(true);
    panelActivated.emit(name);
    markDirty();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tree helpers
// ─────────────────────────────────────────────────────────────────────────────

DockNode** DockPanel::findRef(DockNode* n, DockNode* target)
{
    if (!n || !n->isSplit) return nullptr;
    if (n->first  == target) return &n->first;
    if (n->second == target) return &n->second;
    DockNode** r = findRef(n->first,  target);
    if (r) return r;
    return          findRef(n->second, target);
}

void DockPanel::pruneNode(DockNode*& slot)
{
    if (!slot) return;
    if (!slot->isSplit) return;

    pruneNode(slot->first);
    pruneNode(slot->second);

    bool emptyFirst  = slot->first  && slot->first->isEmpty()  && !slot->first->isSplit;
    bool emptySecond = slot->second && slot->second->isEmpty() && !slot->second->isSplit;

    if (emptyFirst && emptySecond) {
        freeNode(slot->first);
        freeNode(slot->second);
        slot->isSplit = false;
        slot->first = slot->second = nullptr;
    } else if (emptyFirst) {
        DockNode* keep = slot->second;
        slot->second   = nullptr;
        freeNode(slot->first);
        delete slot;
        slot = keep;
    } else if (emptySecond) {
        DockNode* keep = slot->first;
        slot->first    = nullptr;
        freeNode(slot->second);
        delete slot;
        slot = keep;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Layout
// ─────────────────────────────────────────────────────────────────────────────

void DockPanel::layout()
{
    if (!root_) return;
    const Rect b = rect();
    layoutNode(root_, {0, 0, b.w, b.h});
    Widget::layout();
}

void DockPanel::layoutNode(DockNode* node, const Rect& r)
{
    if (!node) return;
    node->rect = r;

    if (node->isSplit) {
        Rect rFirst = r, rSecond = r;
        if (node->splitDir == LayoutDir::Horizontal) {
            float w1 = (r.w - handleW_) * node->ratio;
            rFirst.w  = w1;
            rSecond.x = r.x + w1 + handleW_;
            rSecond.w = r.w - w1 - handleW_;
        } else {
            float h1 = (r.h - handleW_) * node->ratio;
            rFirst.h  = h1;
            rSecond.y = r.y + h1 + handleW_;
            rSecond.h = r.h - h1 - handleW_;
        }
        layoutNode(node->first,  rFirst);
        layoutNode(node->second, rSecond);
        return;
    }

    // Leaf - position the active content widget
    for (int i = 0; i < (int)node->tabs.size(); ++i) {
        Widget* w = node->tabs[i].content;
        bool active = (i == node->currentTab);
        w->setVisible(active);
        if (active) {
            Rect contentR = r;
            contentR.y += tabBarH_;
            contentR.h -= tabBarH_;
            w->setRect(contentR);
            w->layout();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Coordinate helpers
// ─────────────────────────────────────────────────────────────────────────────

Rect DockPanel::toScreen(const Rect& local) const
{
    Rect abs = absoluteRect();
    return { abs.x + local.x, abs.y + local.y, local.w, local.h };
}

// ─────────────────────────────────────────────────────────────────────────────
//  Paint
// ─────────────────────────────────────────────────────────────────────────────

void DockPanel::paint(PaintContext& ctx)
{
    if (!root_) return;
    const Rect b = absoluteRect();
    ctx.pushClip(b);
    paintNode(root_, ctx);

    // Visual drag feedback
    if (drag_.active) paintDraggedTab(ctx);
    if (dropValid_)   paintDropOverlay(ctx);

    ctx.popClip();
}

void DockPanel::paintNode(DockNode* node, PaintContext& ctx)
{
    if (!node) return;
    Rect sr = toScreen(node->rect);

    if (node->isSplit) {
        // Splitter handle
        bool hovered = (hoveredSplit_ == node ||
                       (splitDrag_.active && splitDrag_.node == node));
        Color hc = hovered ? Color(90, 90, 95, 255) : Color(50, 50, 53, 255);
        ctx.fill.SetColor(hc);
        if (node->splitDir == LayoutDir::Horizontal) {
            Rect f = toScreen(node->first->rect);
            ctx.fillRect(f.x + f.w, sr.y, handleW_, sr.h);
        } else {
            Rect f = toScreen(node->first->rect);
            ctx.fillRect(sr.x, f.y + f.h, sr.w, handleW_);
        }
        paintNode(node->first,  ctx);
        paintNode(node->second, ctx);
        return;
    }

    // Leaf background
    ctx.fill.SetColor(38, 38, 42, 255);
    ctx.fillRect(sr.x, sr.y, sr.w, sr.h);

    // Tab bar
    Rect barR = { sr.x, sr.y, sr.w, tabBarH_ };
    paintTabBar(node, barR, ctx);

    // Active content — paint children
    if (node->currentTab >= 0 && node->currentTab < (int)node->tabs.size()) {
        Widget* w = node->tabs[node->currentTab].content;
        if (w && w->isVisible()) {
            Rect contentR = { sr.x, sr.y + tabBarH_, sr.w, sr.h - tabBarH_ };
            ctx.pushClip(contentR);
            w->paint(ctx);
            ctx.popClip();
        }
    }

    // Border
    ctx.line.SetColor(60, 60, 65, 255);
    ctx.lineRect(sr.x, sr.y, sr.w, sr.h);
}

void DockPanel::paintTabBar(DockNode* leaf, const Rect& barR, PaintContext& ctx)
{
    // Bar background
    ctx.fill.SetColor(30, 30, 33, 255);
    ctx.fillRect(barR.x, barR.y, barR.w, barR.h);

    const float padX = 10.f;
    float tx = barR.x + 4.f;

    for (int i = 0; i < (int)leaf->tabs.size(); ++i) {
        auto& tab = leaf->tabs[i];

        setupFont(ctx, {200, 200, 200, 255}, 12.f);
        float tw = ctx.font.GetTextWidth(tab.name.c_str()) + padX * 2;
        tab.cachedWidth = tw;

        bool active   = (i == leaf->currentTab);
        bool dragging = drag_.active && drag_.srcNode == leaf && drag_.srcIdx == i;

        if (!dragging) {
            // Tab bg
            if (active) {
                ctx.fill.SetColor(45, 45, 50, 255);
            } else {
                ctx.fill.SetColor(35, 35, 38, 255);
            }
            ctx.fillRect(tx, barR.y, tw, barR.h);

            // Active indicator line
            if (active) {
                ctx.line.SetColor(120, 160, 220, 255);
                ctx.drawLine(tx, barR.y + barR.h - 1, tx + tw, barR.y + barR.h - 1);
            }

            // Label
            Color tc = active ? Color(220, 220, 220, 255) : Color(150, 150, 155, 255);
            float asc = setupFont(ctx, tc, 12.f);
            ctx.pushClip(barR);
            ctx.font.Print(tab.name.c_str(),
                           tx + padX,
                           barR.y + (barR.h - 12.f) * 0.5f + asc);
            ctx.popClip();
        }
        tx += tw + 2.f;
    }

    // Bottom border
    ctx.line.SetColor(55, 55, 60, 255);
    ctx.drawLine(barR.x, barR.y + barR.h, barR.x + barR.w, barR.y + barR.h);
}

void DockPanel::paintDraggedTab(PaintContext& ctx)
{
    if (!drag_.srcNode || drag_.srcIdx < 0 ||
        drag_.srcIdx >= (int)drag_.srcNode->tabs.size())
        return;

    // Only show floating tab if moved beyond threshold
    float dx = drag_.curX - drag_.startX;
    float dy = drag_.curY - drag_.startY;
    if (dx * dx + dy * dy < 36.f) return;

    auto& tab = drag_.srcNode->tabs[drag_.srcIdx];
    setupFont(ctx, {220, 220, 220, 255}, 12.f);
    float tw = ctx.font.GetTextWidth(tab.name.c_str()) + 20.f;
    float th = tabBarH_;

    float fx = drag_.curX - tw * 0.5f;
    float fy = drag_.curY - th * 0.5f;

    // Shadow
    ctx.fill.SetColor(0, 0, 0, 80);
    ctx.fillRoundedRect(fx + 3, fy + 3, tw, th, 4.f);

    // Tab body
    ctx.fill.SetColor(50, 55, 65, 230);
    ctx.fillRoundedRect(fx, fy, tw, th, 4.f);

    // Border
    ctx.line.SetColor(100, 140, 220, 180);
    ctx.lineRoundedRect(fx, fy, tw, th, 4.f);

    // Label
    float asc = setupFont(ctx, {220, 230, 240, 255}, 12.f);
    ctx.font.Print(tab.name.c_str(),
                   fx + 10.f,
                   fy + (th - 12.f) * 0.5f + asc);
}

void DockPanel::paintDropOverlay(PaintContext& ctx)
{
    if (!dropNode_) return;
    Rect sr = toScreen(dropNode_->rect);

    Rect zone = sr;
    const float fifth = 0.2f;
    switch (dropZone_) {
    case DockSide::Left:   zone.w = sr.w * fifth; break;
    case DockSide::Right:  zone.x = sr.x + sr.w * (1.f - fifth); zone.w = sr.w * fifth; break;
    case DockSide::Top:    zone.h = sr.h * fifth; break;
    case DockSide::Bottom: zone.y = sr.y + sr.h * (1.f - fifth); zone.h = sr.h * fifth; break;
    case DockSide::Center: break;
    }

    ctx.fill.SetColor(80, 120, 200, 60);
    ctx.fillRect(zone.x, zone.y, zone.w, zone.h);
    ctx.line.SetColor(80, 140, 220, 180);
    ctx.lineRect(zone.x, zone.y, zone.w, zone.h);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Hit tests
// ─────────────────────────────────────────────────────────────────────────────

DockNode* DockPanel::hitLeaf(float sx, float sy) const
{
    if (!root_) return nullptr;
    const Rect abs = absoluteRect();
    return hitLeafImpl(root_, abs.x, abs.y, sx, sy);
}

DockNode* DockPanel::hitLeafImpl(DockNode* n, float ox, float oy,
                                  float sx, float sy) const
{
    if (!n) return nullptr;
    Rect sr = { ox + n->rect.x, oy + n->rect.y, n->rect.w, n->rect.h };
    if (!sr.contains(sx, sy)) return nullptr;
    if (!n->isSplit) return n;
    DockNode* r = hitLeafImpl(n->first,  ox, oy, sx, sy);
    return r ? r : hitLeafImpl(n->second, ox, oy, sx, sy);
}

int DockPanel::hitTab(DockNode* leaf, float ox, float oy, float sx, float sy) const
{
    if (!leaf) return -1;
    Rect barR = { ox + leaf->rect.x, oy + leaf->rect.y, leaf->rect.w, tabBarH_ };
    if (!barR.contains(sx, sy)) return -1;

    float tx = barR.x + 4.f;
    for (int i = 0; i < (int)leaf->tabs.size(); ++i) {
        float tw = leaf->tabs[i].cachedWidth;
        if (tw <= 0) tw = 60.f;
        if (sx >= tx && sx < tx + tw) return i;
        tx += tw + 2.f;
    }
    return -1;
}

DockNode* DockPanel::hitSplitter(float sx, float sy) const
{
    if (!root_) return nullptr;
    const Rect abs = absoluteRect();
    return hitSplitterImpl(root_, abs.x, abs.y, sx, sy);
}

DockNode* DockPanel::hitSplitterImpl(DockNode* n, float ox, float oy,
                                      float sx, float sy) const
{
    if (!n || !n->isSplit) return nullptr;
    if (n->splitDir == LayoutDir::Horizontal) {
        Rect f = { ox + n->first->rect.x, oy + n->first->rect.y,
                   n->first->rect.w, n->first->rect.h };
        Rect handle = { f.x + f.w, oy + n->rect.y, handleW_, n->rect.h };
        if (handle.contains(sx, sy)) return n;
    } else {
        Rect f = { ox + n->first->rect.x, oy + n->first->rect.y,
                   n->first->rect.w, n->first->rect.h };
        Rect handle = { ox + n->rect.x, f.y + f.h, n->rect.w, handleW_ };
        if (handle.contains(sx, sy)) return n;
    }
    DockNode* r = hitSplitterImpl(n->first,  ox, oy, sx, sy);
    return r ? r : hitSplitterImpl(n->second, ox, oy, sx, sy);
}

DockSide DockPanel::computeDropZone(DockNode* leaf, float ox, float oy,
                                     float sx, float sy) const
{
    Rect sr = { ox + leaf->rect.x, oy + leaf->rect.y, leaf->rect.w, leaf->rect.h };
    float rx = (sx - sr.x) / sr.w;
    float ry = (sy - sr.y) / sr.h;
    const float edge = 0.2f;
    if (rx < edge) return DockSide::Left;
    if (rx > 1.f - edge) return DockSide::Right;
    if (ry < edge) return DockSide::Top;
    if (ry > 1.f - edge) return DockSide::Bottom;
    return DockSide::Center;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Mouse events
// ─────────────────────────────────────────────────────────────────────────────

void DockPanel::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;
    const Rect abs = absoluteRect();
    float ox = abs.x, oy = abs.y;

    // Splitter drag
    DockNode* spl = hitSplitter(e.x, e.y);
    if (spl) {
        splitDrag_.active     = true;
        splitDrag_.node       = spl;
        splitDrag_.startRatio = spl->ratio;
        splitDrag_.startMouse = (spl->splitDir == LayoutDir::Horizontal) ? e.x : e.y;
        e.consumed = true;
        return;
    }

    // Tab bar click
    DockNode* leaf = hitLeaf(e.x, e.y);
    if (!leaf) return;
    int tabIdx = hitTab(leaf, ox, oy, e.x, e.y);
    if (tabIdx >= 0) {
        // Switch tab (skip if already active)
        if (tabIdx != leaf->currentTab && leaf->tabs.size() > 1) {
            for (auto& t : leaf->tabs) t.content->setVisible(false);
            leaf->currentTab = tabIdx;
            leaf->tabs[tabIdx].content->setVisible(true);
            panelActivated.emit(leaf->tabs[tabIdx].name);
            markDirty();
            layout();
        }

        // Begin drag (allow even on active tab for rearranging)
        drag_.active  = true;
        drag_.srcNode = leaf;
        drag_.srcIdx  = tabIdx;
        drag_.startX  = e.x;
        drag_.startY  = e.y;
        drag_.curX    = e.x;
        drag_.curY    = e.y;
        e.consumed    = true;
    }
}

void DockPanel::onMouseRelease(MouseEvent& e)
{
    if (splitDrag_.active) {
        splitDrag_.active = false;
        markDirty();
    }

    if (drag_.active) {
        drag_.active = false;
        if (dropValid_) performDrop();
        dropValid_ = false;
        dropNode_  = nullptr;
        markDirty();
        layout();
    }
}

void DockPanel::onMouseMove(MouseEvent& e)
{
    const Rect abs = absoluteRect();

    if (splitDrag_.active) {
        DockNode* n = splitDrag_.node;
        float total = (n->splitDir == LayoutDir::Horizontal)
                    ? n->rect.w - handleW_
                    : n->rect.h - handleW_;
        if (total <= 0) return;
        float delta = ((n->splitDir == LayoutDir::Horizontal) ? e.x : e.y)
                    - splitDrag_.startMouse;
        float newRatio = splitDrag_.startRatio + delta / total;
        n->ratio = std::max(0.05f, std::min(0.95f, newRatio));
        markDirty();
        layout();
        return;
    }

    hoveredSplit_ = hitSplitter(e.x, e.y);

    if (drag_.active) {
        drag_.curX = e.x;
        drag_.curY = e.y;

        float dx = e.x - drag_.startX, dy = e.y - drag_.startY;
        if (dx * dx + dy * dy > 36.f) {
            DockNode* target = hitLeaf(e.x, e.y);
            if (target && target != drag_.srcNode) {
                dropNode_  = target;
                dropZone_  = computeDropZone(target, abs.x, abs.y, e.x, e.y);
                dropValid_ = true;
            } else if (target == drag_.srcNode) {
                dropValid_ = false;
                dropNode_  = nullptr;
            } else {
                dropValid_ = false;
                dropNode_  = nullptr;
            }
        }
        markDirty();
    }
}

void DockPanel::onMouseLeave()
{
    hoveredSplit_ = nullptr;
    dropValid_    = false;
    dropNode_     = nullptr;
    markDirty();
}

// ─────────────────────────────────────────────────────────────────────────────
//  performDrop
// ─────────────────────────────────────────────────────────────────────────────

void DockPanel::performDrop()
{
    if (!dropNode_ || !drag_.srcNode) return;

    DockNode::Tab movedTab = drag_.srcNode->tabs[drag_.srcIdx];

    // Remove from source
    drag_.srcNode->tabs.erase(drag_.srcNode->tabs.begin() + drag_.srcIdx);
    if (drag_.srcNode->currentTab >= (int)drag_.srcNode->tabs.size())
        drag_.srcNode->currentTab = (int)drag_.srcNode->tabs.size() - 1;
    for (auto& t : drag_.srcNode->tabs) t.content->setVisible(false);
    if (!drag_.srcNode->tabs.empty())
        drag_.srcNode->tabs[drag_.srcNode->currentTab].content->setVisible(true);

    if (dropZone_ == DockSide::Center) {
        // Merge into target leaf
        dropNode_->tabs.push_back(movedTab);
        dropNode_->currentTab = (int)dropNode_->tabs.size() - 1;
        for (auto& t : dropNode_->tabs) t.content->setVisible(false);
        movedTab.content->setVisible(true);
    } else {
        // Split
        bool firstIsNew = (dropZone_ == DockSide::Left || dropZone_ == DockSide::Top);
        bool horiz      = (dropZone_ == DockSide::Left || dropZone_ == DockSide::Right);

        DockNode* newLeaf = new DockNode();
        newLeaf->isSplit = false;
        newLeaf->tabs.push_back(movedTab);
        newLeaf->currentTab = 0;
        movedTab.content->setVisible(true);

        DockNode* split = new DockNode();
        split->isSplit  = true;
        split->splitDir = horiz ? LayoutDir::Horizontal : LayoutDir::Vertical;

        if (firstIsNew) {
            split->ratio  = 0.5f;
            split->first  = newLeaf;
            split->second = dropNode_;
        } else {
            split->ratio  = 0.5f;
            split->first  = dropNode_;
            split->second = newLeaf;
        }

        if (root_ == dropNode_) {
            root_ = split;
        } else {
            DockNode** slot = findRef(root_, dropNode_);
            if (slot) *slot = split;
        }
    }

    pruneNode(root_);
}
