#include "TreePropertyColorWidgets.hpp"
#include "Theme.hpp"
#include "Utf8.hpp"
#include "WidgetApp.hpp"
#include "pch.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

using ig::retained::clamp;
// ─────────────────────────────────────────────────────────────────────────────
//  ColorPickerPopup_ — popup wrapper for PropertyGrid color editing
// ─────────────────────────────────────────────────────────────────────────────
class ColorPickerPopup_ : public Widget
{
public:
    ColorPickerPopup_(PropertyGrid* grid, int row)
            : grid_(grid),
              row_(row)
    {
        acceptsFocus_ = true;
        picker_ = new ColorPicker();
        picker_->setShowAlpha(true);
        picker_->setColor(grid->propData<PropertyGrid::PropColor>(row).value);

        picker_->colorChanged.connect(
                [this](const Color& c)
                {
                    if (row_ >= 0 && row_ < static_cast<int>(grid_->rows_.size()))
                    {
                        auto& d = grid_->propData<PropertyGrid::PropColor>(row_);
                        d.value = c;
                        if (d.onChange) d.onChange(c);
                        grid_->propertyChanged.emit(row_);
                        grid_->markDirty();
                    }
                });
    }

    ~ColorPickerPopup_() override
    {
        delete picker_;
    }

    void syncPickerRect()
    {
        Rect r = rect_;
        float pad = 6.0f;
        Rect pr{ r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2 };
        if (pr.x != picker_->rect().x || pr.y != picker_->rect().y || pr.w != picker_->rect().w ||
                pr.h != picker_->rect().h)
            picker_->setRect(pr);
    }

    void paint(PaintContext& ctx) override
    {
        Rect abs = absoluteRect();
        const auto& t = Theme::instance();

        ctx.fill.SetColor(0, 0, 0, 60);
        ctx.fillRect(abs.x + 3, abs.y + 3, abs.w, abs.h);

        ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, 250);
        ctx.fillRoundedRect(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6);
        ctx.line.SetColor(t.inputBorderHover.r, t.inputBorderHover.g, t.inputBorderHover.b, 200);
        ctx.lineRoundedRect(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6);

        syncPickerRect();
        picker_->paint(ctx);
    }

    void onMousePress(MouseEvent& e) override
    {
        syncPickerRect();
        Rect abs = absoluteRect();
        if (abs.contains(e.x, e.y))
        {
            picker_->onMousePress(e);
            e.consumed = true;
        }
    }

    void onMouseRelease(MouseEvent& e) override
    {
        syncPickerRect();
        picker_->onMouseRelease(e);
    }

    void onMouseMove(MouseEvent& e) override
    {
        syncPickerRect();
        picker_->onMouseMove(e);
    }

    bool popupContains(float x, float y) const override
    {
        return absoluteRect().contains(x, y);
    }

private:
    PropertyGrid* grid_;
    int row_;
    ColorPicker* picker_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  PropComboPopup_ — dropdown list for PropertyGrid combo properties
// ─────────────────────────────────────────────────────────────────────────────
class PropComboPopup_ : public Widget
{
public:
    PropComboPopup_(PropertyGrid* grid, int row)
            : grid_(grid),
              row_(row)
    {
        acceptsFocus_ = true;
        auto& d = grid->propData<PropertyGrid::PropCombo>(row);
        items_ = d.options;
        selected_ = d.index;
    }

    void paint(PaintContext& ctx) override
    {
        Rect abs = absoluteRect();
        const auto& t = Theme::instance();

        ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, 250);
        ctx.fillRoundedRect(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6);
        ctx.line.SetColor(t.inputBorderHover.r, t.inputBorderHover.g, t.inputBorderHover.b, 200);
        ctx.lineRoundedRect(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6);

        ctx.pushClip(abs);
        float rowH = 22.0f;
        int n = static_cast<int>(items_.size());
        ctx.font.SetFontSize(t.fontSize);
        ctx.font.SetBatch(&ctx.text);

        for (int i = 0; i < n; ++i)
        {
            float ry = abs.y + i * rowH - scroll_;
            if (ry + rowH < abs.y || ry > abs.y + abs.h) continue;
            bool isHov = (i == hovered_);
            bool isSel = (i == selected_);

            if (isHov || isSel)
            {
                Color c = isSel ? t.selectionColor
                                : Color(t.buttonHover.r, t.buttonHover.g, t.buttonHover.b, 120);
                ctx.fill.SetColor(c.r, c.g, c.b, c.a);
                ctx.fillRect(abs.x + 1, ry, abs.w - 2, rowH);
            }

            ctx.font.SetColor(isHov ? Color(255, 255, 255, 255) : t.textColor);
            ctx.font.Print(items_[i].c_str(), abs.x + 6,
                    ry + (rowH - t.fontSize) * 0.5f + ctx.font.GetAscender());
        }
        ctx.popClip();
    }

    void onMousePress(MouseEvent& e) override
    {
        Rect abs = absoluteRect();
        if (!abs.contains(e.x, e.y)) return;
        e.consumed = true;

        float rowH = 22.0f;
        int idx = static_cast<int>((e.y - abs.y + scroll_) / rowH);
        if (idx >= 0 && idx < static_cast<int>(items_.size()))
        {
            if (row_ >= 0 && row_ < static_cast<int>(grid_->rows_.size()))
            {
                auto& d = grid_->propData<PropertyGrid::PropCombo>(row_);
                d.index = idx;
                if (d.onChange) d.onChange(idx);
                grid_->propertyChanged.emit(row_);
                grid_->markDirty();
            }
        }
        WidgetApp::instance().closePopup();
    }

    void onMouseMove(MouseEvent& e) override
    {
        Rect abs = absoluteRect();
        float rowH = 22.0f;
        int idx = static_cast<int>((e.y - abs.y + scroll_) / rowH);
        if (idx < 0 || idx >= static_cast<int>(items_.size())) idx = -1;
        if (idx != hovered_)
        {
            hovered_ = idx;
            markDirty();
        }
    }

    void onMouseLeave() override
    {
        if (hovered_ != -1)
        {
            hovered_ = -1;
            markDirty();
        }
    }

    void onMouseScroll(MouseEvent& e) override
    {
        setScroll(scroll_ - e.scrollY * 22.0f * 3.0f);
        e.consumed = true;
    }

    // Scrolls so the selected item sits inside the list (call after setRect).
    void revealSelected()
    {
        const float rowH = 22.0f;
        setScroll(selected_ * rowH - (rect_.h - rowH) * 0.5f);
    }

    bool popupContains(float x, float y) const override
    {
        return absoluteRect().contains(x, y);
    }

private:
    PropertyGrid* grid_;
    int row_;
    ct::Vector<String> items_;
    int selected_ = -1;
    int hovered_ = -1;
    float scroll_ = 0.0f;

    void setScroll(float value)
    {
        const float maxScroll = static_cast<float>(items_.size()) * 22.0f + 2.0f - rect_.h;
        if (value > maxScroll) value = maxScroll;
        if (value < 0.0f) value = 0.0f;
        if (value != scroll_)
        {
            scroll_ = value;
            markDirty();
        }
    }
};

// ═════════════════════════════════════════════════════════════════════════════
//  TreeNode
// ═════════════════════════════════════════════════════════════════════════════

unsigned int TreeNode::sRevision_ = 0;

TreeNode::TreeNode(const String& text)
        : text_(text)
{
}

TreeNode::~TreeNode()
{
    for (auto* c: children_) delete c;
    if (view_) view_->forgetNode(this);
}

TreeNode* TreeNode::addChild(const String& text)
{
    auto* n = new TreeNode(text);
    n->parent_ = this;
    n->view_ = view_;
    children_.push_back(n);
    bumpRevision();
    return n;
}

void TreeNode::removeChild(int index)
{
    if (index < 0 || index >= static_cast<int>(children_.size())) return;
    delete children_[index];
    children_.erase(children_.begin() + index);
    bumpRevision();
    if (view_) view_->reportLostSelection();
}

void TreeNode::clear()
{
    for (auto* c: children_) delete c;
    children_.clear();
    bumpRevision();
    if (view_) view_->reportLostSelection();
}

int TreeNode::addToggle(IconId iconOn, IconId iconOff, bool on, const String& tipOn,
        const String& tipOff)
{
    Toggle toggle;
    toggle.iconOn = iconOn;
    toggle.iconOff = iconOff;
    toggle.on = on;
    toggle.tipOn = tipOn;
    toggle.tipOff = tipOff;
    toggles_.push_back(toggle);
    if (view_) view_->markDirty();
    return static_cast<int>(toggles_.size()) - 1;
}

void TreeNode::setToggleState(int index, bool on)
{
    if (index < 0 || index >= toggleCount() || toggles_[index].on == on) return;
    toggles_[index].on = on;
    if (view_) view_->markDirty();
}

void TreeNode::setIcon(const String& name)
{
    iconStr_ = name;
    if (name == "folder") iconId_ = IconId::Folder;
    else if (name == "folder_open")
        iconId_ = IconId::FolderOpen;
    else if (name == "file")
        iconId_ = IconId::File;
    else if (name == "file_code")
        iconId_ = IconId::FileCode;
    else if (name == "book")
        iconId_ = IconId::Book;
    else if (name == "gear")
        iconId_ = IconId::Gear;
    else if (name == "star")
        iconId_ = IconId::Star;
    else if (name == "heart")
        iconId_ = IconId::Heart;
    else if (name == "search")
        iconId_ = IconId::Search;
    else if (name == "plus")
        iconId_ = IconId::Plus;
    else if (name == "minus")
        iconId_ = IconId::Minus;
    else if (name == "check")
        iconId_ = IconId::Check;
    else if (name == "cross")
        iconId_ = IconId::Cross;
    else if (name == "arrow_right")
        iconId_ = IconId::ArrowRight;
    else if (name == "arrow_down")
        iconId_ = IconId::ArrowDown;
    else if (name == "arrow_up")
        iconId_ = IconId::ArrowUp;
    else if (name == "arrow_left")
        iconId_ = IconId::ArrowLeft;
    else if (name == "eye")
        iconId_ = IconId::Eye;
    else if (name == "eye_off")
        iconId_ = IconId::EyeOff;
    else if (name == "lock")
        iconId_ = IconId::Lock;
    else if (name == "unlock")
        iconId_ = IconId::Unlock;
    else if (name == "refresh")
        iconId_ = IconId::Refresh;
    else if (name == "trash")
        iconId_ = IconId::Trash;
    else if (name == "edit")
        iconId_ = IconId::Edit;
    else if (name == "home")
        iconId_ = IconId::Home;
    else if (name == "user")
        iconId_ = IconId::User;
    else if (name == "warning")
        iconId_ = IconId::Warning;
    else if (name == "info")
        iconId_ = IconId::Info;
    else if (name == "error")
        iconId_ = IconId::Error;
    else
        iconId_ = IconId::None;
}

// ═════════════════════════════════════════════════════════════════════════════
//  TreeView
// ═════════════════════════════════════════════════════════════════════════════

TreeView::TreeView()
{
    acceptsFocus_ = true;
}
TreeView::~TreeView()
{
    detachModel();
    clear();
}

void TreeView::detachModel()
{
    if (model_) model_->disconnect(modelSlots_);
    model_ = nullptr;
}

void TreeView::setModel(TreeModel* model)
{
    detachModel();
    model_ = model;
    if (!model_) return;

    // Build TreeNode wrappers from the model for rendering
    // (TreeView currently renders via TreeNode, so we sync from model)
    modelSlots_.reset = model_->modelReset.connect(
            [this]
            {
                // Rebuild tree nodes from model
                clear();
                if (!model_) return;
                ct::Function<void(TreeModel::Node*, TreeNode*)> syncNode;
                syncNode = [&](TreeModel::Node* mn, TreeNode* parent)
                {
                    for (auto* mc: mn->children)
                    {
                        auto* tn = parent ? parent->addChild(mc->cells.empty() ? "" : mc->cells[0])
                                          : addRoot(mc->cells.empty() ? "" : mc->cells[0]);
                        if (!mc->iconName.empty()) tn->setIcon(mc->iconName);
                        tn->setExpanded(mc->expanded);
                        syncNode(mc, tn);
                    }
                };
                for (auto* root: model_->roots())
                {
                    auto* tn = addRoot(root->cells.empty() ? "" : root->cells[0]);
                    if (!root->iconName.empty()) tn->setIcon(root->iconName);
                    tn->setExpanded(root->expanded);
                    syncNode(root, tn);
                }
                flatDirty_ = true;
                markDirty();
            });
    modelSlots_.inserted = model_->rowsInserted.connect(
            [this]
            {
                flatDirty_ = true;
                markDirty();
            });
    modelSlots_.changed = model_->dataChanged.connect([this](ModelIndex) { markDirty(); });
    modelSlots_.destroyed = model_->destroyed.connect(
            [this]
            {
                model_ = nullptr;
                modelSlots_ = AbstractItemModel::Connections();
                markDirty();
            });

    // Initial sync
    model_->endResetModel();
}

TreeNode* TreeView::addRoot(const String& text)
{
    auto* n = new TreeNode(text);
    n->view_ = this;
    roots_.push_back(n);
    flatDirty_ = true;
    markDirty();
    return n;
}

void TreeView::clear()
{
    endDrag();
    for (auto* r: roots_) delete r;
    roots_.clear();
    selected_ = nullptr;
    extra_.clear();
    selectionLost_ = false;
    scrollOffset_ = 0;
    flatDirty_ = true;
    markDirty();
}

void TreeView::setSelectedNode(TreeNode* n)
{
    if (!extra_.empty())
    {
        extra_.clear();
        markDirty();
    }
    if (selected_ != n)
    {
        selected_ = n;
        selectionChanged.emit(n);
        markDirty();
    }
}

void TreeView::forgetNode(TreeNode* n)
{
    if (dragNode_ == n || dragCandidate_ == n || dropSpot_.target == n) endDrag();
    for (int i = static_cast<int>(extra_.size()) - 1; i >= 0; --i)
        if (extra_[i] == n) extra_.erase(extra_.begin() + i);
    if (selected_ != n) return;
    selected_ = nullptr;
    selectionLost_ = true;
    markDirty();
}

bool TreeView::isNodeSelected(const TreeNode* n) const
{
    if (n && n == selected_) return true;
    for (const TreeNode* e: extra_)
        if (e == n) return true;
    return false;
}

void TreeView::selectedNodes(ct::Vector<TreeNode*>& out) const
{
    out.clear();
    if (selected_) out.push_back(selected_);
    for (TreeNode* e: extra_) out.push_back(e);
}

void TreeView::setExtraSelection(const ct::Vector<TreeNode*>& nodes)
{
    extra_.clear();
    for (TreeNode* n: nodes)
        if (n && n != selected_ && !isNodeSelected(n)) extra_.push_back(n);
    markDirty();
}

void TreeView::reportLostSelection()
{
    // Told once the node that held the selection is out of the tree, not from
    // inside its destructor, so a handler sees a whole tree.
    if (!selectionLost_) return;
    selectionLost_ = false;
    selectionChanged.emit(nullptr);
}

Widget::Vec2f TreeView::sizeHint() const
{
    return { 200.f, 200.f };
}

void TreeView::rebuildFlat()
{
    // A node added or removed behind the view's back leaves the cached rows
    // pointing at nodes that are gone (or missing ones that exist): the shared
    // revision counter says when the tree changed at all.
    if (!flatDirty_ && revisionSeen_ == TreeNode::revision()) return;
    flatRows_.clear();
    for (auto* r: roots_) flattenNode(r, 0);
    revisionSeen_ = TreeNode::revision();
    flatDirty_ = false;
}

void TreeView::flattenNode(TreeNode* n, int depth)
{
    flatRows_.push_back({ n, depth });
    if (n->isExpanded())
        for (auto* c: n->children()) flattenNode(c, depth + 1);
}

float TreeView::maxScroll() const
{
    float total = static_cast<float>(flatRows_.size()) * rowHeight_;
    float visible = rect_.h;
    return (total > visible) ? total - visible : 0.f;
}

bool TreeView::scrollbar(Rect& track, Rect& thumb) const
{
    const float range = maxScroll();
    if (range <= 0.f) return false;
    const Rect abs = absoluteRect();
    const float barW = 8.f;
    const float total = static_cast<float>(flatRows_.size()) * rowHeight_;
    const float thumbH = std::max(20.f, abs.h * (abs.h / total));
    const float thumbY = abs.y + (scrollOffset_ / range) * (abs.h - thumbH);
    track = Rect(abs.x + abs.w - barW, abs.y, barW, abs.h);
    thumb = Rect(track.x + 1.f, thumbY, barW - 2.f, thumbH);
    return true;
}

void TreeView::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, t.panelColor.a);
    ctx.fillRect(abs.x, abs.y, abs.w, abs.h);

    ctx.pushClip(abs);
    rebuildFlat();

    int first = static_cast<int>(scrollOffset_ / rowHeight_);
    int last = first + static_cast<int>(abs.h / rowHeight_) + 2;
    last = std::min(last, static_cast<int>(flatRows_.size()));

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();

    for (int i = first; i < last; ++i)
    {
        const auto& row = flatRows_[i];
        float rowY = abs.y + i * rowHeight_ - scrollOffset_;
        float indX = abs.x + 4.f + row.depth * indent_;

        if (isNodeSelected(row.node))
        {
            ctx.fill.SetColor(t.selectionColor.r, t.selectionColor.g, t.selectionColor.b,
                    static_cast<u8>(row.node == selected_ ? t.selectionColor.a
                                                          : t.selectionColor.a * 2 / 3));
            ctx.fillRect(abs.x, rowY, abs.w, rowHeight_);
        }

        if (row.node->hasChildren())
        {
            float ts = 7.f;
            float tx = indX;
            float tcy = rowY + rowHeight_ * 0.5f;
            ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
            if (row.node->isExpanded())
                ctx.fillTriangle(tx, tcy - ts * 0.35f, tx + ts, tcy - ts * 0.35f, tx + ts * 0.5f,
                        tcy + ts * 0.5f);
            else
                ctx.fillTriangle(tx + 1, tcy - ts * 0.5f, tx + 1, tcy + ts * 0.5f, tx + ts, tcy);
            indX += ts + 4.f;
        }
        else
        {
            indX += 11.f;
        }

        if (row.node->iconId() != IconId::None)
        {
            float isz = t.fontSize;
            ctx.drawIcon(row.node->iconId(), indX, rowY + (rowHeight_ - isz) * 0.5f, isz);
            indX += isz + 3.f;
        }

        ctx.font.SetColor(row.node == selected_ ? Color(255, 255, 255, 255) : t.textColor);
        float ty = rowY + (rowHeight_ - t.fontSize) * 0.5f + asc;
        ctx.font.Print(row.node->text().c_str(), indX, ty);

        const int toggles = row.node->toggleCount();
        if (toggles > 0)
        {
            const float slot = toggleSlot();
            const float edge = toggleEdge(abs);
            const float left = edge - slot * toggles;
            if (row.node == selected_)
                ctx.fill.SetColor(t.selectionColor.r, t.selectionColor.g, t.selectionColor.b,
                        t.selectionColor.a);
            else
                ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, t.panelColor.a);
            ctx.fillRect(left - 2.f, rowY, edge - left + 4.f, rowHeight_);
            const float isz = t.fontSize;
            for (int k = 0; k < toggles; ++k)
            {
                const TreeNode::Toggle& toggle = row.node->toggles_[k];
                const bool off = !toggle.on;
                const IconId icon =
                        off && toggle.iconOff != IconId::None ? toggle.iconOff : toggle.iconOn;
                const bool dim = off && toggle.iconOff == IconId::None;
                ctx.drawIcon(icon, left + slot * k + (slot - isz) * 0.5f,
                        rowY + (rowHeight_ - isz) * 0.5f, isz,
                        dim ? Color(255, 255, 255, 80) : Color(255, 255, 255, 255));
            }
        }
    }

    if (dragNode_ && dropSpot_.valid)
    {
        // Where the node would land: a box round the new parent, or a line between rows.
        const Color accent(100, 170, 255, 255);
        const float scrolled = abs.y - scrollOffset_;
        if (dropSpot_.row >= 0 && dropSpot_.zone == DropZone::Inside)
        {
            const float ry = scrolled + dropSpot_.row * rowHeight_;
            ctx.fill.SetColor(Color(100, 170, 255, 40));
            ctx.fillRect(abs.x, ry, abs.w, rowHeight_);
            ctx.line.SetColor(accent);
            ctx.lineRect(abs.x + 1.f, ry + 1.f, abs.w - 2.f, rowHeight_ - 2.f);
        }
        else
        {
            const int rows = static_cast<int>(flatRows_.size());
            const int below = dropSpot_.row < 0
                                      ? rows
                                      : dropSpot_.row + (dropSpot_.zone == DropZone::After ? 1 : 0);
            const int depth = dropSpot_.row < 0 ? 0 : flatRows_[dropSpot_.row].depth;
            const float x = abs.x + 4.f + depth * indent_;
            ctx.fill.SetColor(accent);
            ctx.fillRect(x, scrolled + below * rowHeight_ - 1.f, abs.x + abs.w - x, 2.f);
        }
    }

    Rect track, thumb;
    if (scrollbar(track, thumb))
    {
        ctx.fill.SetColor(25, 27, 32, 200);
        ctx.fillRect(track.x, track.y, track.w, track.h);
        ctx.fill.SetColor(draggingBar_ ? 100 : 70, draggingBar_ ? 106 : 75, draggingBar_ ? 118 : 85,
                200);
        ctx.fillRoundedRect(thumb.x, thumb.y, thumb.w, thumb.h, 3.f);
    }

    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.lineRect(abs.x, abs.y, abs.w, abs.h);
    ctx.popClip();
}

float TreeView::toggleSlot() const
{
    return Theme::instance().fontSize + 8.f;
}

float TreeView::toggleEdge(const Rect& abs) const
{
    Rect track, thumb;
    const bool bar = scrollbar(track, thumb);
    return abs.x + abs.w - (bar ? 12.f : 4.f);
}

int TreeView::toggleAt(const Rect& abs, const TreeNode* node, float x) const
{
    const int count = node->toggleCount();
    if (count == 0) return -1;
    const float slot = toggleSlot();
    const float left = toggleEdge(abs) - slot * count;
    if (x < left || x >= left + slot * count) return -1;
    return static_cast<int>((x - left) / slot);
}

void TreeView::showToggleTip(MouseEvent& e)
{
    rebuildFlat();
    const Rect abs = absoluteRect();
    const int idx = static_cast<int>((e.y - abs.y + scrollOffset_) / rowHeight_);
    String tip;
    if (e.y >= abs.y && idx >= 0 && idx < static_cast<int>(flatRows_.size()))
    {
        const TreeNode* node = flatRows_[idx].node;
        const int hit = toggleAt(abs, node, e.x);
        if (hit >= 0)
            tip = node->toggles_[hit].on ? node->toggles_[hit].tipOn : node->toggles_[hit].tipOff;
    }
    if (tip != tooltip()) setTooltip(tip);
}

void TreeView::onMouseLeave()
{
    hoverRow_ = -1;
    if (dragNode_)
    {
        dropSpot_ = DropSpot();
        markDirty();
    }
    Widget::onMouseLeave();
    if (!tooltip().empty()) setTooltip("");
}

void TreeView::onMousePress(MouseEvent& e)
{
    if (!visible_ || !isEnabledInTree()) return;
    Rect abs = absoluteRect();
    if (!abs.contains(e.x, e.y)) return;
    e.consumed = true;
    dragCandidate_ = nullptr;

    rebuildFlat();
    Rect track, thumb;
    if (scrollbar(track, thumb) && e.x >= track.x)
    {
        // A press on the track away from the thumb centres the thumb under the cursor.
        if (e.y < thumb.y || e.y > thumb.y + thumb.h)
        {
            const float range = maxScroll();
            const float travel = abs.h - thumb.h;
            scrollOffset_ = travel > 0.f ? clamp((e.y - thumb.h * 0.5f - abs.y) / travel * range,
                                                   0.f, range)
                                         : 0.f;
            scrollbar(track, thumb);
        }
        draggingBar_ = true;
        barGrab_ = e.y - thumb.y;
        markDirty();
        return;
    }

    rebuildFlat();
    float localY = e.y - abs.y + scrollOffset_;
    int idx = static_cast<int>(localY / rowHeight_);
    if (idx < 0 || idx >= static_cast<int>(flatRows_.size())) return;

    auto& row = flatRows_[idx];
    float indX = 4.f + row.depth * indent_;

    const int hit = toggleAt(abs, row.node, e.x);
    if (hit >= 0)
    {
        TreeNode* node = row.node;
        const bool state = !node->toggles_[hit].on;
        node->toggles_[hit].on = state;
        markDirty();
        toggleChanged.emit(node, hit, state);
        return;
    }

    if (row.node->hasChildren())
    {
        float triEnd = indX + 11.f;
        if ((e.x - abs.x) < triEnd)
        {
            row.node->setExpanded(!row.node->isExpanded());
            if (row.node->isExpanded()) nodeExpanded.emit(row.node);
            else
                nodeCollapsed.emit(row.node);
            flatDirty_ = true;
            markDirty();
            return;
        }
    }
    TreeNode* pressed = row.node;
    if (multiSelect_ && selected_ && (e.ctrl || e.shift))
    {
        if (e.shift)
        {
            // The rows between the selected one and this one, both ends in.
            const int from = rowOf(selected_);
            extra_.clear();
            for (int i = from < idx ? from : idx; i <= (from < idx ? idx : from); ++i)
                if (flatRows_[i].node != selected_) extra_.push_back(flatRows_[i].node);
        }
        else if (pressed == selected_)
        {
            TreeNode* next = extra_.empty() ? nullptr : extra_[0];
            if (!extra_.empty()) extra_.erase(extra_.begin());
            selected_ = next;
            selectionChanged.emit(next);
        }
        else
        {
            bool removed = false;
            for (int i = static_cast<int>(extra_.size()) - 1; i >= 0; --i)
                if (extra_[i] == pressed)
                {
                    extra_.erase(extra_.begin() + i);
                    removed = true;
                }
            if (!removed) extra_.push_back(pressed);
        }
        markDirty();
        selectionSetChanged.emit();
        return;
    }
    const bool hadExtra = !extra_.empty();
    setSelectedNode(pressed);
    if (hadExtra) selectionSetChanged.emit();
    // The selection handler may have rebuilt the tree.
    if (selected_ == pressed) dragCandidate_ = pressed;
    if (selected_ == pressed && e.button == 0 && e.clickCount >= 2) nodeActivated.emit(pressed);
}

void TreeView::onMouseMove(MouseEvent& e)
{
    const Rect abs = absoluteRect();
    hoverX_ = e.x - abs.x;
    const float localY = e.y - abs.y + scrollOffset_;
    hoverRow_ = e.y >= abs.y ? static_cast<int>(localY / rowHeight_) : -1;
    if (dragNode_)
    {
        const float edge = 14.f;
        if (e.y - abs.y < edge) scrollOffset_ = std::max(0.f, scrollOffset_ - 6.f);
        else if (e.y - abs.y > abs.h - edge)
            scrollOffset_ = std::min(scrollOffset_ + 6.f, maxScroll());
        dropSpot_ = dropSpotAt(e.y - abs.y);
        e.consumed = true;
        markDirty();
        return;
    }
    if (!draggingBar_)
    {
        showToggleTip(e);
        return;
    }
    Rect track, thumb;
    if (!scrollbar(track, thumb)) return;
    const float travel = abs.h - thumb.h;
    const float range = maxScroll();
    scrollOffset_ =
            travel > 0.f ? clamp((e.y - barGrab_ - abs.y) / travel * range, 0.f, range) : 0.f;
    e.consumed = true;
    markDirty();
}

void TreeView::onMouseRelease(MouseEvent& e)
{
    if (!draggingBar_) return;
    draggingBar_ = false;
    e.consumed = true;
    markDirty();
}

void TreeView::onMouseScroll(MouseEvent& e)
{
    if (!visible_ || !isEnabledInTree()) return;
    if (!absoluteRect().contains(e.x, e.y)) return;
    e.consumed = true;
    scrollOffset_ -= e.scrollY * rowHeight_ * 2.f;
    scrollOffset_ = clamp(scrollOffset_, 0.f, maxScroll());
    markDirty();
}

bool TreeView::isDragSource() const
{
    if (!dragEnabled_ || draggingBar_ || hoverRow_ < 0 ||
            hoverRow_ >= static_cast<int>(flatRows_.size()))
        return false;
    const Rect abs = absoluteRect();
    const FlatRow& fr = flatRows_[hoverRow_];
    // Not from the toggles, the expand arrow or the scrollbar.
    if (toggleAt(abs, fr.node, abs.x + hoverX_) >= 0) return false;
    Rect track, thumb;
    if (scrollbar(track, thumb) && abs.x + hoverX_ >= track.x) return false;
    const float indX = 4.f + fr.depth * indent_;
    if (fr.node->hasChildren() && hoverX_ < indX + 11.f) return false;
    return !dragFilter_ || dragFilter_(fr.node);
}

DragPayload TreeView::onDragBegin()
{
    DragPayload payload;
    if (!dragCandidate_) return payload;
    dragNode_ = dragCandidate_;
    payload.type = "tree-node";
    payload.data = dragNode_;
    return payload;
}

void TreeView::endDrag()
{
    dragCandidate_ = nullptr;
    dragNode_ = nullptr;
    dropSpot_ = DropSpot();
    markDirty();
}

void TreeView::onDragEnd(bool accepted)
{
    (void) accepted;
    endDrag();
}

bool TreeView::acceptsDrop(const DragPayload& p)
{
    if (p.source == this) return dragEnabled_ && dragNode_;
    return externalAccept_ && externalAccept_(p);
}

void TreeView::onDropReceive(const DragPayload& p)
{
    if (p.source != this)
    {
        rebuildFlat();
        const Rect abs = absoluteRect();
        const int row = static_cast<int>((p.dropY - abs.y + scrollOffset_) / rowHeight_);
        TreeNode* target = p.dropY >= abs.y && row >= 0 && row < static_cast<int>(flatRows_.size())
                                   ? flatRows_[row].node
                                   : nullptr;
        externalDropped.emit(p, target);
        return;
    }
    if (!dragNode_) return;
    const DropSpot spot = dropSpotAt(p.dropY - absoluteRect().y);
    if (!spot.valid) return;
    TreeNode* node = dragNode_;
    // Last: the handler may rebuild the tree and delete these nodes.
    endDrag();
    nodeDropped.emit(node, spot.target, spot.zone);
}

TreeView::DropSpot TreeView::dropSpotAt(float localY)
{
    DropSpot spot;
    if (!dragNode_) return spot;
    rebuildFlat();
    const float content = std::max(0.f, localY + scrollOffset_);
    const int rows = static_cast<int>(flatRows_.size());
    const int row = static_cast<int>(content / rowHeight_);
    if (row >= rows)
    {
        spot.zone = DropZone::After;
    }
    else
    {
        const float fraction = content / rowHeight_ - static_cast<float>(row);
        spot.row = row;
        spot.target = flatRows_[row].node;
        spot.zone = fraction < 0.25f   ? DropZone::Before
                    : fraction > 0.75f ? DropZone::After
                                       : DropZone::Inside;
    }
    // Never onto the node itself or into its own branch.
    for (TreeNode* n = spot.target; n; n = n->parent())
        if (n == dragNode_) return spot;
    spot.valid = !dropFilter_ || dropFilter_(dragNode_, spot.target, spot.zone);
    return spot;
}

int TreeView::rowOf(const TreeNode* n) const
{
    for (int i = 0; i < static_cast<int>(flatRows_.size()); ++i)
        if (flatRows_[i].node == n) return i;
    return -1;
}

void TreeView::scrollToRow(int row)
{
    const float top = static_cast<float>(row) * rowHeight_;
    if (top < scrollOffset_) scrollOffset_ = top;
    else if (top + rowHeight_ > scrollOffset_ + rect_.h)
        scrollOffset_ = top + rowHeight_ - rect_.h;
    scrollOffset_ = clamp(scrollOffset_, 0.f, maxScroll());
}

TreeNode* TreeView::nodeAt(float x, float y)
{
    const Rect abs = absoluteRect();
    if (!abs.contains(x, y)) return nullptr;
    rebuildFlat();
    Rect track, thumb;
    if (scrollbar(track, thumb) && x >= track.x) return nullptr;
    const float localY = y - abs.y + scrollOffset_;
    const int row = static_cast<int>(localY / rowHeight_);
    if (localY < 0.f || row < 0 || row >= static_cast<int>(flatRows_.size())) return nullptr;
    return flatRows_[row].node;
}

void TreeView::scrollTo(const TreeNode* node)
{
    rebuildFlat();
    const int row = rowOf(node);
    if (row < 0) return;
    scrollToRow(row);
    markDirty();
}

bool TreeView::rowRect(const TreeNode* node, Rect& out)
{
    rebuildFlat();
    const int row = rowOf(node);
    if (row < 0) return false;
    scrollToRow(row);
    const Rect abs = absoluteRect();
    out = { abs.x, abs.y + row * rowHeight_ - scrollOffset_, abs.w, rowHeight_ };
    return true;
}

bool TreeView::labelPoint(const TreeNode* node, float& x, float& y)
{
    Rect row;
    if (!rowRect(node, row)) return false;
    const auto& t = Theme::instance();
    float left = row.x + 4.f + flatRows_[rowOf(node)].depth * indent_ + 11.f;
    if (node->iconId() != IconId::None) left += t.fontSize + 3.f;
    x = left + 24.f;
    y = row.y + row.h * 0.5f;
    return true;
}

bool TreeView::toggleRect(const TreeNode* node, int index, Rect& out)
{
    Rect row;
    if (!rowRect(node, row) || index < 0 || index >= node->toggleCount()) return false;
    const float slot = toggleSlot();
    const float left = toggleEdge(absoluteRect()) - slot * node->toggleCount();
    out = { left + slot * index, row.y, slot, row.h };
    return true;
}

void TreeView::onKeyPress(KeyEvent& e)
{
    if (!focused_ || !isEnabledInTree()) return;
    rebuildFlat();
    const int count = static_cast<int>(flatRows_.size());
    const int cur = rowOf(selected_);
    // Only a node on a row is touched: the selection may name one that was
    // deleted behind the view's back.
    TreeNode* node = cur >= 0 ? flatRows_[cur].node : nullptr;
    TreeNode* const before = selected_;

    const int target = listKeyTarget(e.key, cur, count, static_cast<int>(rect_.h / rowHeight_));
    if (target != cur)
    {
        e.consumed = true;
        setSelectedNode(flatRows_[target].node);
    }
    else if (e.key == ig::retained::Key::Left && node)
    {
        e.consumed = true;
        if (node->isExpanded() && node->hasChildren())
        {
            node->setExpanded(false);
            nodeCollapsed.emit(node);
            flatDirty_ = true;
            markDirty();
        }
        else if (node->parent())
            setSelectedNode(node->parent());
    }
    else if (e.key == ig::retained::Key::Right && node)
    {
        e.consumed = true;
        if (!node->isExpanded() && node->hasChildren())
        {
            node->setExpanded(true);
            nodeExpanded.emit(node);
            flatDirty_ = true;
            markDirty();
        }
        else if (node->hasChildren())
            setSelectedNode(node->children()[0]);
    }

    if (selected_ != before && selected_)
    {
        rebuildFlat();
        const int row = rowOf(selected_);
        if (row >= 0) scrollToRow(row);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  PropertyGrid
// ═════════════════════════════════════════════════════════════════════════════

PropertyGrid::PropertyGrid()
{
    acceptsFocus_ = true;
}

int PropertyGrid::addSection(const String& title)
{
    PropRow r{ PropType::Section, title, "", PropSection{} };
    rows_.push_back(std::move(r));
    visDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addString(const String& name, const String& value,
        ct::Function<void(const String&)> onChange, const String& desc)
{
    PropRow r{ PropType::String, name, desc, PropString{ value, std::move(onChange) } };
    rows_.push_back(std::move(r));
    visDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addFloat(const String& name, float value, float mn, float mx,
        ct::Function<void(float)> onChange, const String& desc)
{
    PropRow r{ PropType::Float, name, desc, PropFloat{ value, mn, mx, std::move(onChange) } };
    rows_.push_back(std::move(r));
    visDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addInt(const String& name, int value, int mn, int mx,
        ct::Function<void(int)> onChange, const String& desc)
{
    PropRow r{ PropType::Int, name, desc, PropInt{ value, mn, mx, std::move(onChange) } };
    rows_.push_back(std::move(r));
    visDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addBool(const String& name, bool value, ct::Function<void(bool)> onChange,
        const String& desc)
{
    PropRow r{ PropType::Bool, name, desc, PropBool{ value, std::move(onChange) } };
    rows_.push_back(std::move(r));
    visDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addColor(const String& name, const Color& value,
        ct::Function<void(const Color&)> onChange, const String& desc)
{
    PropRow r{ PropType::Color, name, desc, PropColor{ value, std::move(onChange) } };
    rows_.push_back(std::move(r));
    visDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addCombo(const String& name, const ct::Vector<String>& opts, int sel,
        ct::Function<void(int)> onChange, const String& desc)
{
    PropRow r{ PropType::Combo, name, desc, PropCombo{ opts, sel, std::move(onChange) } };
    rows_.push_back(std::move(r));
    visDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addVec2(const String& name, float x, float y, float mn, float mx,
        ct::Function<void(float, float)> onChange, const String& desc)
{
    PropVec v;
    v.value[0] = x;
    v.value[1] = y;
    v.components = 2;
    v.min = mn;
    v.max = mx;
    v.onChange2 = std::move(onChange);
    PropRow r{ PropType::Vec2, name, desc, std::move(v) };
    rows_.push_back(std::move(r));
    visDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addVec3(const String& name, float x, float y, float z, float mn, float mx,
        ct::Function<void(float, float, float)> onChange, const String& desc)
{
    PropVec v;
    v.value[0] = x;
    v.value[1] = y;
    v.value[2] = z;
    v.components = 3;
    v.min = mn;
    v.max = mx;
    v.onChange3 = std::move(onChange);
    PropRow r{ PropType::Vec3, name, desc, std::move(v) };
    rows_.push_back(std::move(r));
    visDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addVec4(const String& name, float x, float y, float z, float w, float mn,
        float mx, ct::Function<void(float, float, float, float)> onChange, const String& desc)
{
    PropVec v;
    v.value[0] = x;
    v.value[1] = y;
    v.value[2] = z;
    v.value[3] = w;
    v.components = 4;
    v.min = mn;
    v.max = mx;
    v.onChange4 = std::move(onChange);
    PropRow r{ PropType::Vec4, name, desc, std::move(v) };
    rows_.push_back(std::move(r));
    visDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addButton(const String& name, ct::Function<void()> onClick, const String& desc)
{
    PropRow r{ PropType::Button, name, desc, PropButton{ std::move(onClick) } };
    rows_.push_back(std::move(r));
    visDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addSeparator()
{
    PropRow r{ PropType::Separator, "", "", PropSeparator{} };
    rows_.push_back(std::move(r));
    visDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addRange(const String& name, float lo, float hi, float mn, float mx,
        ct::Function<void(float, float)> onChange, const String& desc)
{
    PropRow r{ PropType::Range, name, desc, PropRange{ lo, hi, mn, mx, std::move(onChange) } };
    rows_.push_back(std::move(r));
    visDirty_ = true;
    markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

void PropertyGrid::setString(int row, const String& v)
{
    if (row >= 0 && row < (int) rows_.size() && rows_[row].type == PropType::String)
        std::get<PropString>(rows_[row].data).value = v;
    markDirty();
}
void PropertyGrid::setFloat(int row, float v)
{
    if (row >= 0 && row < (int) rows_.size() && rows_[row].type == PropType::Float)
    {
        auto& d = std::get<PropFloat>(rows_[row].data);
        d.value = clamp(v, d.min, d.max);
    }
    markDirty();
}
void PropertyGrid::setInt(int row, int v)
{
    if (row >= 0 && row < (int) rows_.size() && rows_[row].type == PropType::Int)
    {
        auto& d = std::get<PropInt>(rows_[row].data);
        d.value = clamp(v, d.min, d.max);
    }
    markDirty();
}
void PropertyGrid::setBool(int row, bool v)
{
    if (row >= 0 && row < (int) rows_.size() && rows_[row].type == PropType::Bool)
        std::get<PropBool>(rows_[row].data).value = v;
    markDirty();
}
void PropertyGrid::setColor(int row, const Color& c)
{
    if (row >= 0 && row < (int) rows_.size() && rows_[row].type == PropType::Color)
        std::get<PropColor>(rows_[row].data).value = c;
    markDirty();
}
void PropertyGrid::setCombo(int row, int idx)
{
    if (row >= 0 && row < (int) rows_.size() && rows_[row].type == PropType::Combo)
    {
        auto& d = std::get<PropCombo>(rows_[row].data);
        if (idx >= 0 && idx < (int) d.options.size()) d.index = idx;
        markDirty();
    }
}
void PropertyGrid::setComboOptions(int row, const ct::Vector<String>& options, int idx)
{
    if (row < 0 || row >= (int) rows_.size() || rows_[row].type != PropType::Combo) return;
    auto& d = std::get<PropCombo>(rows_[row].data);
    d.options = options;
    d.index = idx >= 0 && idx < (int) options.size() ? idx : 0;
    markDirty();
}
void PropertyGrid::setVec2(int row, float x, float y)
{
    if (row >= 0 && row < (int) rows_.size() && rows_[row].type == PropType::Vec2)
    {
        auto& d = std::get<PropVec>(rows_[row].data);
        d.value[0] = clamp(x, d.min, d.max);
        d.value[1] = clamp(y, d.min, d.max);
        markDirty();
    }
}
void PropertyGrid::setDragStep(int row, float perPixel)
{
    if (row >= 0 && row < (int) rows_.size() && std::isfinite(perPixel))
        rows_[row].dragStep = std::max(0.0f, perPixel);
}

void PropertyGrid::setVec3(int row, float x, float y, float z)
{
    if (row >= 0 && row < (int) rows_.size() && rows_[row].type == PropType::Vec3)
    {
        auto& d = std::get<PropVec>(rows_[row].data);
        d.value[0] = clamp(x, d.min, d.max);
        d.value[1] = clamp(y, d.min, d.max);
        d.value[2] = clamp(z, d.min, d.max);
        markDirty();
    }
}
void PropertyGrid::setVec4(int row, float x, float y, float z, float w)
{
    if (row >= 0 && row < (int) rows_.size() && rows_[row].type == PropType::Vec4)
    {
        auto& d = std::get<PropVec>(rows_[row].data);
        d.value[0] = clamp(x, d.min, d.max);
        d.value[1] = clamp(y, d.min, d.max);
        d.value[2] = clamp(z, d.min, d.max);
        d.value[3] = clamp(w, d.min, d.max);
        markDirty();
    }
}
void PropertyGrid::setRange(int row, float lo, float hi)
{
    if (row >= 0 && row < (int) rows_.size() && rows_[row].type == PropType::Range)
    {
        auto& d = std::get<PropRange>(rows_[row].data);
        d.lo = clamp(lo, d.min, d.max);
        d.hi = clamp(hi, d.min, d.max);
        if (d.lo > d.hi) std::swap(d.lo, d.hi);
        markDirty();
    }
}

ct::Vector<int> PropertyGrid::visibleRows() const
{
    if (!visDirty_) return cachedVisibleRows_;
    cachedVisibleRows_.clear();
    bool collapsed = false;
    for (int i = 0; i < (int) rows_.size(); ++i)
    {
        if (rows_[i].type == PropType::Section)
        {
            collapsed = !std::get<PropSection>(rows_[i].data).expanded;
            cachedVisibleRows_.push_back(i);
        }
        else if (!collapsed)
            cachedVisibleRows_.push_back(i);
    }
    visDirty_ = false;
    return cachedVisibleRows_;
}

float PropertyGrid::visibleTotalHeight() const
{
    return static_cast<float>(visibleRows().size()) * rowHeight_;
}
float PropertyGrid::totalHeight() const
{
    return visibleTotalHeight();
}
float PropertyGrid::gridHeight() const
{
    return rect_.h - (descHeight_ > 0 ? descHeight_ : 0);
}
float PropertyGrid::maxScroll() const
{
    float t = totalHeight(), v = gridHeight();
    return (t > v) ? t - v : 0.f;
}

int PropertyGrid::hitRow(float localY) const
{
    auto vis = visibleRows();
    int vi = static_cast<int>((localY + scrollOffset_) / rowHeight_);
    if (vi < 0 || vi >= (int) vis.size()) return -1;
    return vis[vi];
}

Widget::Vec2f PropertyGrid::sizeHint() const
{
    return { 280.f, std::max(100.f, visibleTotalHeight() + descHeight_) };
}

void PropertyGrid::spinStep(int row, int dir)
{
    if (row < 0 || row >= (int) rows_.size()) return;
    auto& r = rows_[row];
    if (r.type == PropType::Int)
    {
        auto& d = std::get<PropInt>(r.data);
        int step = std::max(1, (d.max - d.min) / 100);
        d.value = clamp(d.value + dir * step, d.min, d.max);
        if (d.onChange) d.onChange(d.value);
    }
    else if (r.type == PropType::Float)
    {
        auto& d = std::get<PropFloat>(r.data);
        float step = (d.max - d.min) / 100.f;
        d.value = clamp(d.value + dir * step, d.min, d.max);
        if (d.onChange) d.onChange(d.value);
    }
    propertyChanged.emit(row);
    markDirty();
}

void PropertyGrid::startEdit(int row)
{
    if (row < 0 || row >= (int) rows_.size()) return;
    auto& r = rows_[row];
    editing_ = true;
    activeRow_ = row;
    switch (r.type)
    {
        case PropType::String:
            editBuf_ = std::get<PropString>(r.data).value;
            break;
        case PropType::Float:
        {
            char b[32];
            snprintf(b, 32, "%.2f", std::get<PropFloat>(r.data).value);
            editBuf_ = b;
            break;
        }
        case PropType::Int:
        {
            char b[32];
            snprintf(b, 32, "%d", std::get<PropInt>(r.data).value);
            editBuf_ = b;
            break;
        }
        case PropType::Vec2:
        case PropType::Vec3:
        case PropType::Vec4:
        {
            char b[48];
            snprintf(b, sizeof(b), "%.9g", std::get<PropVec>(r.data).value[dragComponent_]);
            editBuf_ = b;
            break;
        }
        default:
            editing_ = false;
            return;
    }
    editCursor_ = static_cast<int>(editBuf_.size());
    markDirty();
}

void PropertyGrid::commitEdit()
{
    if (!editing_ || activeRow_ < 0) return;
    auto& r = rows_[activeRow_];
    if (r.type != PropType::String)
    {
        char* end = nullptr;
        const double number = std::strtod(editBuf_.c_str(), &end);
        if (end == editBuf_.c_str() || *end != '\0' || !std::isfinite(number)) return;
    }
    switch (r.type)
    {
        case PropType::String:
        {
            auto& d = std::get<PropString>(r.data);
            d.value = editBuf_;
            if (d.onChange) d.onChange(d.value);
            break;
        }
        case PropType::Float:
        {
            auto& d = std::get<PropFloat>(r.data);
            float v = editBuf_.to_float();
            d.value = clamp(v, d.min, d.max);
            if (d.onChange) d.onChange(d.value);
            break;
        }
        case PropType::Int:
        {
            auto& d = std::get<PropInt>(r.data);
            int v = editBuf_.to_int();
            d.value = clamp(v, d.min, d.max);
            if (d.onChange) d.onChange(d.value);
            break;
        }
        case PropType::Vec2:
        case PropType::Vec3:
        case PropType::Vec4:
        {
            auto& d = std::get<PropVec>(r.data);
            d.value[dragComponent_] = clamp(editBuf_.to_float(), d.min, d.max);
            if (r.type == PropType::Vec2 && d.onChange2) d.onChange2(d.value[0], d.value[1]);
            if (r.type == PropType::Vec3 && d.onChange3)
                d.onChange3(d.value[0], d.value[1], d.value[2]);
            if (r.type == PropType::Vec4 && d.onChange4)
                d.onChange4(d.value[0], d.value[1], d.value[2], d.value[3]);
            break;
        }
        default:
            break;
    }
    propertyChanged.emit(activeRow_);
    editing_ = false;
    activeRow_ = -1;
    markDirty();
}

void PropertyGrid::cancelEdit()
{
    editing_ = false;
    activeRow_ = -1;
    markDirty();
}

// ── Paint ────────────────────────────────────────────────────────────────

void PropertyGrid::paintRow(PaintContext& ctx, const Rect& abs, int idx, float y)
{
    const auto& t = Theme::instance();
    const auto& row = rows_[idx];
    float labelW = abs.w * labelRatio_;
    float valueX = abs.x + labelW;
    float valueW = abs.w - labelW;

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();
    float textY = y + (rowHeight_ - t.fontSize) * 0.5f + asc;

    // ── Section header ───────────────────────────────────────────────────
    if (row.type == PropType::Section)
    {
        auto& ds = std::get<PropSection>(row.data);
        Color bg = t.collapsibleHeaderBg;
        if (hoveredRow_ == idx)
        {
            bg.r = std::min(255, bg.r + 12);
            bg.g = std::min(255, bg.g + 12);
            bg.b = std::min(255, bg.b + 12);
        }
        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fillRect(abs.x, y, abs.w, rowHeight_);
        float ts = 7.f, tx = abs.x + 6.f, tcy = y + rowHeight_ * 0.5f;
        ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
        if (ds.expanded)
            ctx.fillTriangle(tx, tcy - ts * 0.35f, tx + ts, tcy - ts * 0.35f, tx + ts * 0.5f,
                    tcy + ts * 0.5f);
        else
            ctx.fillTriangle(tx + 1, tcy - ts * 0.5f, tx + 1, tcy + ts * 0.5f, tx + ts, tcy);
        ctx.font.SetColor(t.textColor);
        ctx.font.Print(row.name.c_str(), tx + ts + 4.f, textY);
        return;
    }

    // Row highlight
    if (idx == selectedRow_)
    {
        ctx.fill.SetColor(t.selectionColor.r, t.selectionColor.g, t.selectionColor.b,
                t.selectionColor.a);
        ctx.fillRect(abs.x, y, abs.w, rowHeight_);
    }
    else if (idx == hoveredRow_)
    {
        ctx.fill.SetColor(t.buttonHover.r, t.buttonHover.g, t.buttonHover.b, 30);
        ctx.fillRect(abs.x, y, abs.w, rowHeight_);
    }
    if (idx % 2 == 0 && idx != selectedRow_)
    {
        ctx.fill.SetColor(0, 0, 0, 10);
        ctx.fillRect(abs.x, y, abs.w, rowHeight_);
    }

    // Label
    ctx.font.SetColor(idx == selectedRow_ ? Color(255, 255, 255, 255) : t.textColor);
    ctx.font.Print(row.name.c_str(), abs.x + 8.f, textY);

    // Column separator
    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 50);
    ctx.drawLine(valueX, y, valueX, y + rowHeight_);

    float pad = 4.f, valTextX = valueX + pad, valW = valueW - pad * 2;

    // ── Editing ──────────────────────────────────────────────────────────
    if (editing_ && activeRow_ == idx)
    {
        if (row.type == PropType::Vec2 || row.type == PropType::Vec3 || row.type == PropType::Vec4)
        {
            const int count = std::get<PropVec>(row.data).components;
            const float componentWidth = (valW - 2.0f * (count - 1)) / count;
            valTextX += dragComponent_ * (componentWidth + 2.0f);
            valW = componentWidth;
        }
        ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
        ctx.fillRect(valTextX, y + 1, valW, rowHeight_ - 2);
        ctx.line.SetColor(t.focusColor.r, t.focusColor.g, t.focusColor.b, t.focusColor.a);
        ctx.lineRect(valTextX, y + 1, valW, rowHeight_ - 2);
        ctx.font.SetColor(Color(255, 255, 255, 255));
        ctx.font.Print(editBuf_.c_str(), valTextX + 3, textY);
        float curX = valTextX + 3 + ctx.font.GetTextWidth(editBuf_.substr(0, editCursor_).c_str());
        ctx.line.SetColor(255, 255, 255, 255);
        ctx.drawLine(curX, y + 3, curX, y + rowHeight_ - 3);
        return;
    }

    // ── Slider helper ────────────────────────────────────────────────────
    auto drawSliderBar = [&](float sx, float sy, float sw, float sh, float val, float mn, float mxa,
                                 const char* label, Color barCol)
    {
        ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
        ctx.fillRect(sx, sy, sw, sh);
        float ratio = (mxa - mn > 0.0001f) ? clamp((val - mn) / (mxa - mn), 0.f, 1.f) : 0.f;
        ctx.fill.SetColor(barCol.r, barCol.g, barCol.b, barCol.a);
        ctx.fillRect(sx, sy, sw * ratio, sh);
        ctx.line.SetColor(t.inputBorder.r, t.inputBorder.g, t.inputBorder.b, t.inputBorder.a);
        ctx.lineRect(sx, sy, sw, sh);
        ctx.font.SetColor(Color(230, 230, 230, 255));
        ctx.font.Print(label, sx + 3, sy + (sh - t.fontSize) * 0.5f + asc);
    };

    switch (row.type)
    {
        case PropType::String:
        {
            auto& d = std::get<PropString>(row.data);
            ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
            ctx.fillRect(valTextX, y + 2, valW, rowHeight_ - 4);
            ctx.line.SetColor(t.inputBorder.r, t.inputBorder.g, t.inputBorder.b, t.inputBorder.a);
            ctx.lineRect(valTextX, y + 2, valW, rowHeight_ - 4);
            ctx.font.SetColor(Color(200, 220, 255, 255));
            ctx.font.Print(d.value.c_str(), valTextX + 3, textY);
            break;
        }
        case PropType::Float:
        {
            auto& d = std::get<PropFloat>(row.data);
            char buf[32];
            snprintf(buf, 32, "%.2f", d.value);
            Color bc(60, 100, 160, 200);
            if (draggingSlider_ && activeRow_ == idx) bc = Color(80, 130, 200, 220);
            else if (hoveredRow_ == idx)
                bc = Color(70, 110, 175, 210);
            drawSliderBar(valTextX, y + 2, valW, rowHeight_ - 4, d.value, d.min, d.max, buf, bc);
            break;
        }
        case PropType::Int:
        {
            auto& d = std::get<PropInt>(row.data);
            char buf[32];
            snprintf(buf, 32, "%d", d.value);
            Color bc(60, 140, 90, 200);
            if (draggingSlider_ && activeRow_ == idx) bc = Color(80, 170, 110, 220);
            else if (hoveredRow_ == idx)
                bc = Color(70, 155, 100, 210);
            drawSliderBar(valTextX, y + 2, valW, rowHeight_ - 4, (float) d.value, (float) d.min,
                    (float) d.max, buf, bc);
            break;
        }
        case PropType::Bool:
        {
            auto& d = std::get<PropBool>(row.data);
            float bs = 14.f, bx = valTextX, by = y + (rowHeight_ - bs) * 0.5f;
            ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
            ctx.fillRect(bx, by, bs, bs);
            ctx.line.SetColor(t.inputBorder.r, t.inputBorder.g, t.inputBorder.b, t.inputBorder.a);
            ctx.lineRect(bx, by, bs, bs);
            if (d.value)
            {
                ctx.line.SetColor(t.checkMark.r, t.checkMark.g, t.checkMark.b, t.checkMark.a);
                ctx.drawLine(bx + 3, by + bs * 0.5f, bx + bs * 0.4f, by + bs - 3);
                ctx.drawLine(bx + bs * 0.4f, by + bs - 3, bx + bs - 3, by + 3);
            }
            break;
        }
        case PropType::Color:
        {
            auto& d = std::get<PropColor>(row.data);
            float sh2 = rowHeight_ - 6.f, sw2 = std::min(valW, sh2 * 3.f), sy2 = y + 3.f;
            ctx.fill.SetColor(d.value.r, d.value.g, d.value.b, d.value.a);
            ctx.fillRect(valTextX, sy2, sw2, sh2);
            ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
            ctx.lineRect(valTextX, sy2, sw2, sh2);
            char buf[20];
            snprintf(buf, 20, "#%02X%02X%02X", d.value.r, d.value.g, d.value.b);
            ctx.font.SetColor(Color(180, 180, 180, 255));
            ctx.font.Print(buf, valTextX + sw2 + 4.f, textY);
            break;
        }
        case PropType::Combo:
        {
            auto& d = std::get<PropCombo>(row.data);
            ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
            ctx.fillRect(valTextX, y + 2, valW, rowHeight_ - 4);
            ctx.line.SetColor(t.inputBorder.r, t.inputBorder.g, t.inputBorder.b, t.inputBorder.a);
            ctx.lineRect(valTextX, y + 2, valW, rowHeight_ - 4);
            const String& txt =
                    (d.index >= 0 && d.index < (int) d.options.size()) ? d.options[d.index] : "";
            ctx.font.SetColor(Color(180, 220, 180, 255));
            ctx.font.Print(txt.c_str(), valTextX + 3, textY);
            float ax = valueX + valueW - 14.f, ay = y + rowHeight_ * 0.5f;
            ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
            ctx.fillTriangle(ax, ay - 3, ax + 8, ay - 3, ax + 4, ay + 3);
            break;
        }
        case PropType::Vec2:
        case PropType::Vec3:
        case PropType::Vec4:
        {
            auto& d = std::get<PropVec>(row.data);
            int nc = d.components;
            static const Color ccs[] = { { 180, 60, 60, 200 }, { 60, 140, 60, 200 },
                { 60, 80, 180, 200 }, { 140, 120, 60, 200 } };
            static const char* cls[] = { "X", "Y", "Z", "W" };
            float gap = 2.f, compW = (valW - gap * (nc - 1)) / nc;
            for (int c = 0; c < nc; ++c)
            {
                float cx2 = valTextX + c * (compW + gap);
                char buf[32];
                snprintf(buf, 32, "%s %.1f", cls[c], d.value[c]);
                Color bc = ccs[c];
                if (draggingSlider_ && activeRow_ == idx && dragComponent_ == c)
                    bc = Color(std::min(255, bc.r + 30), std::min(255, bc.g + 30),
                            std::min(255, bc.b + 30), 220);
                drawSliderBar(cx2, y + 2, compW, rowHeight_ - 4, d.value[c], d.min, d.max, buf, bc);
            }
            break;
        }
        case PropType::Button:
        {
            Color bc = (hoveredRow_ == idx) ? t.buttonHover : t.buttonNormal;
            ctx.fill.SetColor(bc.r, bc.g, bc.b, bc.a);
            ctx.fillRoundedRect(valTextX, y + 2, valW, rowHeight_ - 4, 3.f);
            ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
            ctx.lineRoundedRect(valTextX, y + 2, valW, rowHeight_ - 4, 3.f);
            float tw = ctx.font.GetTextWidth(row.name.c_str());
            ctx.font.SetColor(t.textColor);
            ctx.font.Print(row.name.c_str(), valTextX + (valW - tw) * 0.5f, textY);
            break;
        }
        case PropType::Separator:
        {
            float ly = y + rowHeight_ * 0.5f;
            ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
            ctx.drawLine(abs.x + 4.f, ly, abs.x + abs.w - 4.f, ly);
            break;
        }
        case PropType::Range:
        {
            auto& d = std::get<PropRange>(row.data);
            float range = d.max - d.min;
            float rlo = (range > 0.0001f) ? (d.lo - d.min) / range : 0.f;
            float rhi = (range > 0.0001f) ? (d.hi - d.min) / range : 1.f;
            rlo = clamp(rlo, 0.f, 1.f);
            rhi = clamp(rhi, 0.f, 1.f);
            float barY2 = y + 2, barH2 = rowHeight_ - 4;
            ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
            ctx.fillRect(valTextX, barY2, valW, barH2);
            float loX = valTextX + rlo * valW, hiX = valTextX + rhi * valW;
            Color bc(80, 140, 180, 200);
            if (draggingSlider_ && activeRow_ == idx) bc = Color(100, 170, 210, 230);
            else if (hoveredRow_ == idx)
                bc = Color(90, 155, 195, 215);
            ctx.fill.SetColor(bc.r, bc.g, bc.b, bc.a);
            ctx.fillRect(loX, barY2, hiX - loX, barH2);
            ctx.fill.SetColor(220, 220, 220, 240);
            ctx.fillRect(loX - 1.5f, barY2, 3.f, barH2);
            ctx.fillRect(hiX - 1.5f, barY2, 3.f, barH2);
            ctx.line.SetColor(t.inputBorder.r, t.inputBorder.g, t.inputBorder.b, t.inputBorder.a);
            ctx.lineRect(valTextX, barY2, valW, barH2);
            char buf[48];
            snprintf(buf, 48, "%.1f - %.1f", d.lo, d.hi);
            ctx.font.SetColor(Color(230, 230, 230, 255));
            ctx.font.Print(buf, valTextX + 3, textY);
            break;
        }
        default:
            break;
    }
}

void PropertyGrid::paintDescPanel(PaintContext& ctx, const Rect& abs)
{
    if (descHeight_ <= 0) return;
    const auto& t = Theme::instance();
    float gh = gridHeight(), descY = abs.y + gh;
    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.drawLine(abs.x, descY, abs.x + abs.w, descY);
    ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, t.panelColor.a);
    ctx.fillRect(abs.x, descY + 1, abs.w, descHeight_ - 1);
    if (selectedRow_ < 0 || selectedRow_ >= (int) rows_.size()) return;
    const auto& row = rows_[selectedRow_];
    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();
    ctx.font.SetColor(Color(220, 220, 225, 255));
    ctx.font.Print(row.name.c_str(), abs.x + 6, descY + 4 + asc);
    if (!row.description.empty())
    {
        ctx.font.SetColor(Color(150, 150, 155, 255));
        ctx.font.Print(row.description.c_str(), abs.x + 6, descY + 4 + t.fontSize + 2 + asc);
    }
}

void PropertyGrid::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();
    float gh = gridHeight();

    ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, t.panelColor.a);
    ctx.fillRect(abs.x, abs.y, abs.w, abs.h);

    ctx.pushClip({ abs.x, abs.y, abs.w, gh });

    auto vis = visibleRows();
    int count = (int) vis.size();
    int first = static_cast<int>(scrollOffset_ / rowHeight_);
    int last = std::min(first + static_cast<int>(gh / rowHeight_) + 2, count);

    for (int vi = first; vi < last; ++vi)
    {
        float rowY = abs.y + vi * rowHeight_ - scrollOffset_;
        paintRow(ctx, abs, vis[vi], rowY);
    }

    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 40);
    for (int vi = first; vi < last; ++vi)
    {
        float ry = abs.y + (vi + 1) * rowHeight_ - scrollOffset_;
        ctx.drawLine(abs.x, ry, abs.x + abs.w, ry);
    }

    ctx.popClip();
    paintDescPanel(ctx, abs);

    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.lineRect(abs.x, abs.y, abs.w, abs.h);
}

void PropertyGrid::onMousePress(MouseEvent& e)
{
    if (!visible_ || !isEnabledInTree()) return;
    Rect abs = absoluteRect();
    if (!abs.contains(e.x, e.y)) return;
    e.consumed = true;

    float gh = gridHeight();
    if (e.y > abs.y + gh) return;

    int idx = hitRow(e.y - abs.y);
    if (idx < 0) return;
    auto& row = rows_[idx];

    if (row.type == PropType::Section)
    {
        std::get<PropSection>(row.data).expanded = !std::get<PropSection>(row.data).expanded;
        visDirty_ = true;
        markDirty();
        return;
    }
    if (row.type == PropType::Separator) return;

    if (selectedRow_ != idx)
    {
        if (editing_) commitEdit();
        selectedRow_ = idx;
        markDirty();
    }

    float labelW = abs.w * labelRatio_, valueX = abs.x + labelW;
    if (e.x < valueX) return;
    float valueW = abs.w - labelW, pad = 4.f, valTextX = valueX + pad, valW = valueW - pad * 2;

    switch (row.type)
    {
        case PropType::Bool:
        {
            auto& d = std::get<PropBool>(row.data);
            d.value = !d.value;
            if (d.onChange) d.onChange(d.value);
            propertyChanged.emit(idx);
            markDirty();
            break;
        }
        case PropType::Float:
        {
            auto& d = std::get<PropFloat>(row.data);
            if (editing_ && activeRow_ == idx) break;
            draggingSlider_ = true;
            activeRow_ = idx;
            dragStartX_ = e.x;
            dragComponent_ = 0;
            dragStartVal_ = d.value;
            markDirty();
            break;
        }
        case PropType::Int:
        {
            auto& d = std::get<PropInt>(row.data);
            if (editing_ && activeRow_ == idx) break;
            draggingSlider_ = true;
            activeRow_ = idx;
            dragStartX_ = e.x;
            dragComponent_ = 0;
            dragStartVal_ = (float) d.value;
            markDirty();
            break;
        }
        case PropType::Vec2:
        case PropType::Vec3:
        case PropType::Vec4:
        {
            auto& d = std::get<PropVec>(row.data);
            if (editing_)
            {
                commitEdit();
                if (editing_) break;
            }
            int nc = d.components;
            float gap = 2.f, compW = (valW - gap * (nc - 1)) / nc;
            int comp = clamp((int) ((e.x - valTextX) / (compW + gap)), 0, nc - 1);
            draggingSlider_ = true;
            activeRow_ = idx;
            dragStartX_ = e.x;
            dragComponent_ = comp;
            activeRow_ = idx;
            dragStartVal_ = d.value[comp];
            markDirty();
            break;
        }
        case PropType::String:
            if (!editing_ || activeRow_ != idx) startEdit(idx);
            break;
        case PropType::Combo:
        {
            auto& d = std::get<PropCombo>(row.data);
            auto vis = visibleRows();
            float rowY = abs.y;
            for (int vi = 0; vi < (int) vis.size(); ++vi)
            {
                if (vis[vi] == idx)
                {
                    rowY = abs.y + vi * rowHeight_ - scrollOffset_;
                    break;
                }
            }
            const auto& io2 = ig::retained::GetIO();
            int n = (int) d.options.size();
            float itemH = 22.f, popH = (n < 16 ? n : 16) * itemH + 2,
                  popW = abs.w - abs.w * labelRatio_;
            float popX = abs.x + abs.w * labelRatio_;
            float popY = rowY + rowHeight_;
            if (popX + popW > io2.displayWidth) popX = io2.displayWidth - popW;
            if (popX < 0.f) popX = 0.f;
            if (popY + popH > io2.displayHeight)
                popY = (rowY - popH >= 0.f) ? rowY - popH : io2.displayHeight - popH;
            if (popY < 0.f) popY = 0.f;
            auto* popup = new PropComboPopup_(this, idx);
            popup->setRect({ popX, popY, popW, popH });
            popup->revealSelected();
            WidgetApp::instance().showPopup(popup, nullptr);
            e.consumed = true;
            markDirty();
            break;
        }
        case PropType::Button:
        {
            auto& d = std::get<PropButton>(row.data);
            if (d.onClick) d.onClick();
            markDirty();
            break;
        }
        case PropType::Range:
        {
            auto& d = std::get<PropRange>(row.data);
            float range = d.max - d.min;
            float rlo = (range > 0.0001f) ? (d.lo - d.min) / range : 0.f;
            float rhi = (range > 0.0001f) ? (d.hi - d.min) / range : 1.f;
            float loX = valTextX + rlo * valW, hiX = valTextX + rhi * valW;
            int handle = (std::abs(e.x - loX) <= std::abs(e.x - hiX)) ? 0 : 1;
            draggingSlider_ = true;
            activeRow_ = idx;
            dragStartX_ = e.x;
            dragComponent_ = handle;
            float ratio = clamp((e.x - valTextX) / valW, 0.f, 1.f), newVal = d.min + ratio * range;
            if (handle == 0) d.lo = clamp(newVal, d.min, d.hi);
            else
                d.hi = clamp(newVal, d.lo, d.max);
            dragStartVal_ = (handle == 0) ? d.lo : d.hi;
            if (d.onChange) d.onChange(d.lo, d.hi);
            markDirty();
            break;
        }
        case PropType::Color:
        {
            auto vis = visibleRows();
            float rowY = abs.y;
            for (int vi = 0; vi < (int) vis.size(); ++vi)
            {
                if (vis[vi] == idx)
                {
                    rowY = abs.y + vi * rowHeight_ - scrollOffset_;
                    break;
                }
            }
            const auto& io = ig::retained::GetIO();
            float popW = 230.f, popH = 270.f;
            float popX = abs.x + abs.w * labelRatio_;
            float popY = rowY + rowHeight_;
            if (popX + popW > io.displayWidth) popX = io.displayWidth - popW;
            if (popX < 0.f) popX = 0.f;
            if (popY + popH > io.displayHeight)
                popY = (rowY - popH >= 0.f) ? rowY - popH : io.displayHeight - popH;
            if (popY < 0.f) popY = 0.f;
            auto* popup = new ColorPickerPopup_(this, idx);
            popup->setRect({ popX, popY, popW, popH });
            WidgetApp::instance().showPopup(popup, nullptr);
            e.consumed = true;
            break;
        }
        default:
            break;
    }
}

void PropertyGrid::onMouseRelease(MouseEvent& e)
{
    if (draggingSlider_)
    {
        draggingSlider_ = false;
        if (activeRow_ >= 0)
        {
            float dx = std::abs(e.x - dragStartX_);
            if (dx < 3.f)
            {
                auto& row = rows_[activeRow_];
                if (row.type == PropType::Float || row.type == PropType::Int ||
                        row.type == PropType::Vec2 || row.type == PropType::Vec3 ||
                        row.type == PropType::Vec4)
                    startEdit(activeRow_);
            }
            else
                propertyChanged.emit(activeRow_);
            if (!editing_) activeRow_ = -1;
        }
    }
    (void) e;
}

void PropertyGrid::onMouseMove(MouseEvent& e)
{
    if (!visible_ || !isEnabledInTree()) return;
    Rect abs = absoluteRect();

    if (draggingSlider_ && activeRow_ >= 0 && activeRow_ < (int) rows_.size())
    {
        e.consumed = true;
        auto& row = rows_[activeRow_];
        float labelW = abs.w * labelRatio_, valueX = abs.x + labelW, valueW = abs.w - labelW;
        float pad = 4.f, valTextX = valueX + pad, valW = valueW - pad * 2;
        float mn = 0, mx2 = 1, sliderX = valTextX, sliderW = valW;
        if (row.type == PropType::Float)
        {
            auto& d = std::get<PropFloat>(row.data);
            mn = d.min;
            mx2 = d.max;
        }
        else if (row.type == PropType::Int)
        {
            auto& d = std::get<PropInt>(row.data);
            mn = (float) d.min;
            mx2 = (float) d.max;
        }
        else if (row.type == PropType::Vec2 || row.type == PropType::Vec3 ||
                 row.type == PropType::Vec4)
        {
            auto& d = std::get<PropVec>(row.data);
            mn = d.min;
            mx2 = d.max;
            int nc = d.components;
            float gap = 2.f, compW = (valW - gap * (nc - 1)) / nc;
            sliderX = valTextX + dragComponent_ * (compW + gap);
            sliderW = compW;
        }
        else if (row.type == PropType::Range)
        {
            auto& d = std::get<PropRange>(row.data);
            mn = d.min;
            mx2 = d.max;
        }

        float range = mx2 - mn;
        if (range > 0.0001f && sliderW > 1.f)
        {
            if (std::abs(e.x - dragStartX_) < 3.0f) return;
            float ratio = clamp((e.x - sliderX) / sliderW, 0.f, 1.f);
            const float sensitivity = e.shift ? 0.1f : (e.ctrl ? 10.0f : 1.0f);
            const float perPixel =
                    row.dragStep > 0.0f ? row.dragStep : range / std::max(200.0f, sliderW);
            float newVal =
                    row.type == PropType::Range
                            ? mn + ratio * range
                            : clamp(dragStartVal_ + (e.x - dragStartX_) * perPixel * sensitivity,
                                      mn, mx2);
            if (row.type == PropType::Float)
            {
                auto& d = std::get<PropFloat>(row.data);
                d.value = newVal;
                if (d.onChange) d.onChange(d.value);
            }
            else if (row.type == PropType::Int)
            {
                auto& d = std::get<PropInt>(row.data);
                d.value = (int) std::round(newVal);
                if (d.onChange) d.onChange(d.value);
            }
            else if (row.type == PropType::Range)
            {
                auto& d = std::get<PropRange>(row.data);
                if (dragComponent_ == 0) d.lo = clamp(newVal, d.min, d.hi);
                else
                    d.hi = clamp(newVal, d.lo, d.max);
                if (d.onChange) d.onChange(d.lo, d.hi);
            }
            else
            {
                auto& d = std::get<PropVec>(row.data);
                d.value[dragComponent_] = newVal;
                if (row.type == PropType::Vec2 && d.onChange2) d.onChange2(d.value[0], d.value[1]);
                else if (row.type == PropType::Vec3 && d.onChange3)
                    d.onChange3(d.value[0], d.value[1], d.value[2]);
                else if (row.type == PropType::Vec4 && d.onChange4)
                    d.onChange4(d.value[0], d.value[1], d.value[2], d.value[3]);
            }
            markDirty();
        }
        return;
    }

    if (abs.contains(e.x, e.y) && e.y < abs.y + gridHeight())
    {
        int idx = hitRow(e.y - abs.y);
        if (idx != hoveredRow_)
        {
            hoveredRow_ = idx;
            markDirty();
        }
    }
    else
    {
        if (hoveredRow_ != -1)
        {
            hoveredRow_ = -1;
            markDirty();
        }
    }
}

void PropertyGrid::onMouseScroll(MouseEvent& e)
{
    if (!visible_ || !isEnabledInTree()) return;
    if (!absoluteRect().contains(e.x, e.y)) return;
    e.consumed = true;
    scrollOffset_ -= e.scrollY * rowHeight_ * 2.f;
    scrollOffset_ = clamp(scrollOffset_, 0.f, maxScroll());
    markDirty();
}

void PropertyGrid::onKeyPress(KeyEvent& e)
{
    if (!focused_ && !editing_) return;
    if (editing_)
    {
        e.consumed = true;
        switch (e.key)
        {
            case ig::retained::Key::Return:
            case ig::retained::Key::KPEnter:
                commitEdit();
                return;
            case ig::retained::Key::Escape:
                cancelEdit();
                return;
            case ig::retained::Key::Backspace:
                if (editCursor_ > 0)
                {
                    const int prev = (int) utf8PrevOffset(editBuf_, editCursor_);
                    editBuf_.erase(prev, editCursor_ - prev);
                    editCursor_ = prev;
                    markDirty();
                }
                return;
            case ig::retained::Key::Delete:
                if (editCursor_ < (int) editBuf_.size())
                {
                    const int next = (int) utf8NextOffset(editBuf_, editCursor_);
                    editBuf_.erase(editCursor_, next - editCursor_);
                    markDirty();
                }
                return;
            case ig::retained::Key::Right:
                if (editCursor_ < (int) editBuf_.size())
                {
                    editCursor_ = (int) utf8NextOffset(editBuf_, editCursor_);
                    markDirty();
                }
                return;
            case ig::retained::Key::Left:
                if (editCursor_ > 0)
                {
                    editCursor_ = (int) utf8PrevOffset(editBuf_, editCursor_);
                    markDirty();
                }
                return;
            case ig::retained::Key::Home:
                editCursor_ = 0;
                markDirty();
                return;
            case ig::retained::Key::End:
                editCursor_ = (int) editBuf_.size();
                markDirty();
                return;
        }
    }
    if (!editing_)
    {
        auto vis = visibleRows();
        int visIdx = -1;
        for (int i = 0; i < (int) vis.size(); ++i)
            if (vis[i] == selectedRow_)
            {
                visIdx = i;
                break;
            }
        if (e.key == ig::retained::Key::Up)
        {
            e.consumed = true;
            if (visIdx > 0)
            {
                selectedRow_ = vis[visIdx - 1];
                markDirty();
            }
        }
        else if (e.key == ig::retained::Key::Down)
        {
            e.consumed = true;
            if (visIdx < (int) vis.size() - 1)
            {
                selectedRow_ = vis[visIdx + 1];
                markDirty();
            }
        }
        else if (e.key == ig::retained::Key::Return || e.key == ig::retained::Key::KPEnter)
        {
            e.consumed = true;
            if (selectedRow_ >= 0 && selectedRow_ < (int) rows_.size())
            {
                auto& row = rows_[selectedRow_];
                if (row.type == PropType::Section)
                {
                    auto& ds = std::get<PropSection>(row.data);
                    ds.expanded = !ds.expanded;
                    visDirty_ = true;
                }
                else if (row.type == PropType::String || row.type == PropType::Float ||
                         row.type == PropType::Int)
                    startEdit(selectedRow_);
                else if (row.type == PropType::Bool)
                {
                    auto& db = std::get<PropBool>(row.data);
                    db.value = !db.value;
                    if (db.onChange) db.onChange(db.value);
                    propertyChanged.emit(selectedRow_);
                }
                markDirty();
            }
        }
    }
}

void PropertyGrid::onTextInput(KeyEvent& e)
{
    if (!editing_) return;
    e.consumed = true;
    if (activeRow_ >= 0 && activeRow_ < (int) rows_.size())
    {
        auto& row = rows_[activeRow_];
        if (row.type == PropType::Int)
        {
            for (int i = 0; e.text[i]; ++i)
            {
                char c = e.text[i];
                if ((c >= '0' && c <= '9') || c == '-')
                {
                    editBuf_.insert(editCursor_, 1, c);
                    editCursor_++;
                }
            }
            markDirty();
            return;
        }
        if (row.type == PropType::Float)
        {
            for (int i = 0; e.text[i]; ++i)
            {
                char c = e.text[i];
                if ((c >= '0' && c <= '9') || c == '-' || c == '.')
                {
                    editBuf_.insert(editCursor_, 1, c);
                    editCursor_++;
                }
            }
            markDirty();
            return;
        }
    }
    for (int i = 0; e.text[i]; ++i)
    {
        editBuf_.insert(editCursor_, 1, e.text[i]);
        editCursor_++;
    }
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  ColorPicker
// ═════════════════════════════════════════════════════════════════════════════

ColorPicker::ColorPicker()
{
    acceptsFocus_ = true;
}
ColorPicker::~ColorPicker()
{
    releaseTextures();
}

void ColorPicker::releaseTextures()
{
    WidgetApp& app = WidgetApp::instance();
    if (wheelTex_) app.destroyTexture(wheelTex_);
    if (valueTex_) app.destroyTexture(valueTex_);
    if (alphaTex_) app.destroyTexture(alphaTex_);
    wheelTex_ = TextureHandle{};
    valueTex_ = TextureHandle{};
    alphaTex_ = TextureHandle{};
}

static unsigned char pickerChannel(float v)
{
    return (unsigned char) (std::min(1.f, std::max(0.f, v)) * 255.f + 0.5f);
}

TextureHandle ColorPicker::buildWheelTexture(float radius)
{
    const int scale = 2;
    const int n = (int) std::ceil(radius * 2.f) * scale;
    const float c = n * 0.5f, outer = radius * scale, inner = outer * 0.75f;
    texPixels_.assign((size_t) n * n * 4, 0);
    for (int y = 0; y < n; ++y)
    {
        for (int x = 0; x < n; ++x)
        {
            float dx = x + 0.5f - c, dy = y + 0.5f - c, d = std::sqrt(dx * dx + dy * dy);
            float cover = std::min(1.f, std::max(0.f, outer - d + 0.5f));
            if (cover <= 0.f) continue;
            float h = std::atan2(dy, dx) / (2.f * 3.14159265f);
            if (h < 0.f) h += 1.f;
            Color col = d >= inner ? hsvToRgb(h, 1.f, 1.f, 1.f)
                                   : hsvToRgb(h, std::min(1.f, d / inner), val_, 1.f);
            // blend the ring over the disc across the inner edge
            float ringMix = std::min(1.f, std::max(0.f, d - inner + 0.5f));
            if (ringMix > 0.f && ringMix < 1.f)
            {
                Color disc = hsvToRgb(h, 1.f, val_, 1.f), ring = hsvToRgb(h, 1.f, 1.f, 1.f);
                col.r = (unsigned char) (disc.r + (ring.r - disc.r) * ringMix);
                col.g = (unsigned char) (disc.g + (ring.g - disc.g) * ringMix);
                col.b = (unsigned char) (disc.b + (ring.b - disc.b) * ringMix);
            }
            unsigned char* p = &texPixels_[((size_t) y * n + x) * 4];
            p[0] = col.r;
            p[1] = col.g;
            p[2] = col.b;
            p[3] = pickerChannel(cover);
        }
    }
    return WidgetApp::instance().uploadTexture(texPixels_.data(), n, n);
}

TextureHandle ColorPicker::buildBarTexture(int height, bool alphaBar)
{
    const int scale = 2, h = std::max(2, height * scale), w = 2;
    texPixels_.assign((size_t) w * h * 4, 255);
    Color full = hsvToRgb(hue_, sat_, val_, 1.f);
    for (int y = 0; y < h; ++y)
    {
        float t = 1.f - (y + 0.5f) / h;
        Color col = alphaBar ? full : hsvToRgb(hue_, sat_, t, 1.f);
        for (int x = 0; x < w; ++x)
        {
            unsigned char* p = &texPixels_[((size_t) y * w + x) * 4];
            p[0] = col.r;
            p[1] = col.g;
            p[2] = col.b;
            p[3] = alphaBar ? pickerChannel(t) : 255;
        }
    }
    return WidgetApp::instance().uploadTexture(texPixels_.data(), w, h);
}

void ColorPicker::setColor(const Color& c)
{
    rgbToHsv(c, hue_, sat_, val_);
    alpha_ = c.a / 255.f;
    markDirty();
}
Color ColorPicker::color() const
{
    return hsvToRgb(hue_, sat_, val_, alpha_);
}
Widget::Vec2f ColorPicker::sizeHint() const
{
    return { 150.f, 170.f };
}

float ColorPicker::wheelRadius(const Rect& abs) const
{
    float bs = 30.f + (showAlpha_ ? 30.f : 0.f), av = std::min(abs.w - bs, abs.h - 44.f);
    return std::max(20.f, av * 0.5f);
}
float ColorPicker::wheelCenterX(const Rect& abs) const
{
    return abs.x + wheelRadius(abs) + 4.f;
}
float ColorPicker::wheelCenterY(const Rect& abs) const
{
    return abs.y + wheelRadius(abs) + 4.f;
}
Rect ColorPicker::valueBarRect(const Rect& abs) const
{
    float r = wheelRadius(abs), cx = wheelCenterX(abs);
    return { cx + r + 10.f, abs.y + 4.f, 18.f, r * 2.f };
}
Rect ColorPicker::alphaBarRect(const Rect& abs) const
{
    Rect v = valueBarRect(abs);
    return { v.x + v.w + 8.f, v.y, v.w, v.h };
}
Rect ColorPicker::previewRect(const Rect& abs) const
{
    float r = wheelRadius(abs);
    return { abs.x + 4.f, abs.y + r * 2.f + 12.f, 40.f, 28.f };
}
Rect ColorPicker::hexRect(const Rect& abs) const
{
    Rect p = previewRect(abs);
    return { p.x + p.w + 8.f, p.y, 100.f, p.h };
}

void ColorPicker::updateFromWheel(float mx, float my, const Rect& abs)
{
    float cx = wheelCenterX(abs), cy = wheelCenterY(abs), r = wheelRadius(abs), ir = r * 0.75f;
    float dx = mx - cx, dy = my - cy, dist = sqrtf(dx * dx + dy * dy);
    hue_ = atan2f(dy, dx) / (2.f * 3.14159265f);
    if (hue_ < 0) hue_ += 1.f;
    sat_ = std::min(dist / ir, 1.f);
}

void ColorPicker::updateFromValueBar(float my, const Rect& abs)
{
    Rect b = valueBarRect(abs);
    val_ = 1.f - clamp((my - b.y) / b.h, 0.f, 1.f);
}

void ColorPicker::updateFromAlphaBar(float my, const Rect& abs)
{
    Rect b = alphaBarRect(abs);
    alpha_ = 1.f - clamp((my - b.y) / b.h, 0.f, 1.f);
}

Color ColorPicker::hsvToRgb(float h, float s, float v, float a)
{
    float r = v, g = v, b = v;
    if (s > 0.f)
    {
        h = fmodf(h, 1.f) * 6.f;
        int i = (int) h;
        float f = h - i, p = v * (1 - s), q = v * (1 - s * f), t2 = v * (1 - s * (1 - f));
        switch (i)
        {
            case 0:
                r = v;
                g = t2;
                b = p;
                break;
            case 1:
                r = q;
                g = v;
                b = p;
                break;
            case 2:
                r = p;
                g = v;
                b = t2;
                break;
            case 3:
                r = p;
                g = q;
                b = v;
                break;
            case 4:
                r = t2;
                g = p;
                b = v;
                break;
            default:
                r = v;
                g = p;
                b = q;
                break;
        }
    }
    return Color((u8) (r * 255), (u8) (g * 255), (u8) (b * 255), (u8) (a * 255));
}

void ColorPicker::rgbToHsv(const Color& c, float& h, float& s, float& v)
{
    float r = c.r / 255.f, g = c.g / 255.f, b = c.b / 255.f;
    float mx = std::max({ r, g, b }), mn2 = std::min({ r, g, b }), d = mx - mn2;
    v = mx;
    s = (mx > 0) ? d / mx : 0;
    if (d < 0.00001f) h = 0;
    else if (mx == r)
        h = fmodf((g - b) / d, 6.f) / 6.f;
    else if (mx == g)
        h = ((b - r) / d + 2.f) / 6.f;
    else
        h = ((r - g) / d + 4.f) / 6.f;
    if (h < 0) h += 1.f;
}

void ColorPicker::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();
    ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, t.panelColor.a);
    ctx.fillRect(abs.x, abs.y, abs.w, abs.h);
    ctx.pushClip(abs);

    float cx = wheelCenterX(abs), cy = wheelCenterY(abs), r = wheelRadius(abs);
    float innerR = r * 0.75f;

    // Wheel: hue ring and saturation disc for the current value
    if (!wheelTex_ || wheelTexRadius_ != r || wheelTexVal_ != val_)
    {
        if (wheelTex_) WidgetApp::instance().destroyTexture(wheelTex_);
        wheelTex_ = buildWheelTexture(r);
        wheelTexRadius_ = r;
        wheelTexVal_ = val_;
    }
    if (wheelTex_) ctx.drawImage(wheelTex_, { cx - r, cy - r, r * 2.f, r * 2.f });

    // Hue/sat cursor
    float ca = hue_ * 2.f * 3.14159265f, cd = sat_ * innerR;
    float curX = cx + cosf(ca) * cd, curY = cy + sinf(ca) * cd;
    ctx.line.SetColor(255, 255, 255, 255);
    ctx.lineCircle(curX, curY, 5.f);
    ctx.line.SetColor(0, 0, 0, 255);
    ctx.lineCircle(curX, curY, 4.f);

    // Value bar
    Rect vb = valueBarRect(abs);
    const int barH = (int) vb.h;
    if (!valueTex_ || barTexHue_ != hue_ || barTexSat_ != sat_ || barTexVal_ != val_ ||
            barTexHeight_ != barH)
    {
        if (valueTex_) WidgetApp::instance().destroyTexture(valueTex_);
        if (alphaTex_) WidgetApp::instance().destroyTexture(alphaTex_);
        valueTex_ = buildBarTexture(barH, false);
        alphaTex_ = showAlpha_ ? buildBarTexture(barH, true) : TextureHandle{};
        barTexHue_ = hue_;
        barTexSat_ = sat_;
        barTexVal_ = val_;
        barTexHeight_ = barH;
    }
    if (valueTex_) ctx.drawImage(valueTex_, vb);
    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.lineRect(vb.x, vb.y, vb.w, vb.h);
    float valY = vb.y + (1.f - val_) * vb.h;
    ctx.fill.SetColor(255, 255, 255, 255);
    ctx.fillTriangle(vb.x - 4, valY - 4, vb.x - 4, valY + 4, vb.x, valY);
    ctx.fillTriangle(vb.x + vb.w + 4, valY - 4, vb.x + vb.w + 4, valY + 4, vb.x + vb.w, valY);

    // Alpha bar
    if (showAlpha_)
    {
        Rect ab2 = alphaBarRect(abs);
        if (alphaTex_) ctx.drawImage(alphaTex_, ab2);
        ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
        ctx.lineRect(ab2.x, ab2.y, ab2.w, ab2.h);
        float alphaY = ab2.y + (1.f - alpha_) * ab2.h;
        ctx.fill.SetColor(255, 255, 255, 255);
        ctx.fillTriangle(ab2.x - 4, alphaY - 4, ab2.x - 4, alphaY + 4, ab2.x, alphaY);
        ctx.fillTriangle(ab2.x + ab2.w + 4, alphaY - 4, ab2.x + ab2.w + 4, alphaY + 4,
                ab2.x + ab2.w, alphaY);
    }

    // Preview swatch + hex
    Rect pr = previewRect(abs);
    Color cur = color();
    ctx.fill.SetColor(cur.r, cur.g, cur.b, cur.a);
    ctx.fillRect(pr.x, pr.y, pr.w, pr.h);
    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.lineRect(pr.x, pr.y, pr.w, pr.h);
    Rect hr = hexRect(abs);
    char hexBuf[20];
    if (showAlpha_) snprintf(hexBuf, 20, "#%02X%02X%02X%02X", cur.r, cur.g, cur.b, cur.a);
    else
        snprintf(hexBuf, 20, "#%02X%02X%02X", cur.r, cur.g, cur.b);
    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetColor(t.textColor);
    ctx.font.SetBatch(&ctx.text);
    ctx.font.Print(hexBuf, hr.x, hr.y + (hr.h - t.fontSize) * 0.5f + ctx.font.GetAscender());

    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.lineRect(abs.x, abs.y, abs.w, abs.h);
    ctx.popClip();
}

void ColorPicker::onMousePress(MouseEvent& e)
{
    if (!visible_ || !isEnabledInTree()) return;
    Rect abs = absoluteRect();
    if (!abs.contains(e.x, e.y)) return;
    e.consumed = true;

    float cx = wheelCenterX(abs), cy = wheelCenterY(abs), r = wheelRadius(abs);
    float dx = e.x - cx, dy = e.y - cy, dist = sqrtf(dx * dx + dy * dy);

    dragStart_ = color();
    Rect vb = valueBarRect(abs);
    if (vb.contains(e.x, e.y))
    {
        dragTarget_ = DragValueBar;
        updateFromValueBar(e.y, abs);
        colorChanged.emit(color());
        markDirty();
        return;
    }
    if (showAlpha_)
    {
        Rect ab2 = alphaBarRect(abs);
        if (ab2.contains(e.x, e.y))
        {
            dragTarget_ = DragAlphaBar;
            updateFromAlphaBar(e.y, abs);
            colorChanged.emit(color());
            markDirty();
            return;
        }
    }
    if (dist <= r)
    {
        dragTarget_ = DragWheel;
        updateFromWheel(e.x, e.y, abs);
        colorChanged.emit(color());
        markDirty();
    }
}

void ColorPicker::onMouseRelease(MouseEvent& e)
{
    (void) e;
    if (dragTarget_ == DragNone) return;
    dragTarget_ = DragNone;
    const Color now = color();
    if (now.r != dragStart_.r || now.g != dragStart_.g || now.b != dragStart_.b ||
            now.a != dragStart_.a)
        colorCommitted.emit(now);
}

// ─────────────────────────────────────────────────────────────────────────────
//  ColorButton
// ─────────────────────────────────────────────────────────────────────────────

namespace
{

// The popup a ColorButton opens: a panel holding the picker.
class ColorButtonPopup_ : public Widget
{
public:
    ColorButtonPopup_()
    {
        acceptsFocus_ = true;
        picker = createChild<ColorPicker>();
    }

    void placePicker()
    {
        const float pad = 6.0f;
        picker->setRect({ pad, pad, rect_.w - pad * 2.0f, rect_.h - pad * 2.0f });
    }

    void paint(PaintContext& ctx) override
    {
        placePicker();
        const Rect abs = absoluteRect();
        const auto& t = Theme::instance();
        ctx.fill.SetColor(0, 0, 0, 60);
        ctx.fillRect(abs.x + 3, abs.y + 3, abs.w, abs.h);
        ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, 250);
        ctx.fillRoundedRect(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6);
        ctx.line.SetColor(t.inputBorderHover.r, t.inputBorderHover.g, t.inputBorderHover.b, 200);
        ctx.lineRoundedRect(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6);
        Widget::paint(ctx);
    }

    ColorPicker* picker = nullptr;
};

} // namespace

ColorButton::ColorButton(const Color& color)
        : color_(color)
{
}

Widget::Vec2f ColorButton::sizeHint() const
{
    const char* widest = showAlpha_ ? "#WWWWWWWW" : "#WWWWWW";
    WidgetApp* app = WidgetApp::existing();
    const float text = app ? app->textWidth(widest)
                           : Theme::instance().fontSize * 0.62f * (showAlpha_ ? 9.0f : 7.0f);
    return { 36.0f + 6.0f + text, 22.0f };
}

void ColorButton::setColor(const Color& color)
{
    color_ = color;
    if (ColorPicker* open = picker()) open->setColor(color);
    markDirty();
}

ColorPicker* ColorButton::picker() const
{
    WidgetApp& app = WidgetApp::instance();
    if (!popup_ || app.popup() != popup_ || app.popupOwner() != this) return nullptr;
    return static_cast<ColorButtonPopup_*>(popup_)->picker;
}

void ColorButton::openPicker()
{
    auto* popup = new ColorButtonPopup_();
    ColorPicker* p = popup->picker;
    p->setShowAlpha(showAlpha_);
    p->setColor(color_);
    // The popup goes before this button does: WidgetApp drops it when its owner
    // is removed, so these never run on a button that is gone.
    p->colorChanged.connect(
            [this](Color c)
            {
                color_ = c;
                markDirty();
                colorChanged.emit(c);
            });
    p->colorCommitted.connect([this](Color c) { colorCommitted.emit(c); });

    const Rect abs = absoluteRect();
    const auto& io = GetIO();
    const float w = showAlpha_ ? 230.0f : 200.0f, h = 200.0f;
    float x = abs.x, y = abs.y + abs.h + 2.0f;
    if (io.displayWidth > 0.0f && x + w > io.displayWidth) x = io.displayWidth - w;
    if (io.displayHeight > 0.0f && y + h > io.displayHeight) y = abs.y - h - 2.0f;
    if (x < 0.0f) x = 0.0f;
    if (y < 0.0f) y = 0.0f;
    popup->setRect({ x, y, w, h });
    popup->placePicker();
    popup_ = popup;
    WidgetApp::instance().showPopup(popup, this);
}

void ColorButton::onMousePress(MouseEvent& e)
{
    if (!visible_ || !isEnabledInTree() || e.button != 0) return;
    e.consumed = true;
    // A second click on the swatch closes the picker it opened.
    if (picker())
    {
        WidgetApp::instance().closePopup();
        return;
    }
    openPicker();
}

void ColorButton::paint(PaintContext& ctx)
{
    if (!visible_) return;
    const Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;
    const auto& t = Theme::instance();
    const float sw = std::min(abs.w, 36.0f);
    const Rect swatch{ abs.x + 2.0f, abs.y + 3.0f, sw - 4.0f, abs.h - 6.0f };
    // A checkerboard under the swatch, so the alpha shows.
    ctx.pushClip(swatch);
    const float cell = 5.0f;
    ctx.fill.SetColor(200, 200, 200, 255);
    ctx.fillRect(swatch.x, swatch.y, swatch.w, swatch.h);
    ctx.fill.SetColor(140, 140, 140, 255);
    for (int cy = 0; cy * cell < swatch.h; ++cy)
        for (int cx = 0; cx * cell < swatch.w; ++cx)
            if (((cx + cy) & 1) == 0)
                ctx.fillRect(swatch.x + cx * cell, swatch.y + cy * cell, cell, cell);
    ctx.fill.SetColor(color_.r, color_.g, color_.b, color_.a);
    ctx.fillRect(swatch.x, swatch.y, swatch.w, swatch.h);
    ctx.popClip();
    ctx.line.SetColor(isHovered() ? t.inputBorderHover : t.inputBorder);
    ctx.lineRect(swatch.x, swatch.y, swatch.w, swatch.h);
    // The hex beside it, when there is room.
    if (abs.w > sw + 40.0f)
    {
        char hex[16];
        if (showAlpha_)
            std::snprintf(hex, sizeof hex, "#%02X%02X%02X%02X", color_.r, color_.g, color_.b,
                    color_.a);
        else
            std::snprintf(hex, sizeof hex, "#%02X%02X%02X", color_.r, color_.g, color_.b);
        ctx.font.SetFontSize(t.fontSize);
        ctx.font.SetBatch(&ctx.text);
        ctx.font.SetColor(t.textColor);
        const float asc = ctx.font.GetAscender();
        ctx.font.Print(ctx.fitText(hex, abs.w - sw - 6.0f).c_str(), abs.x + sw + 4.0f,
                abs.y + (abs.h - t.fontSize) * 0.5f + asc);
    }
}

void ColorPicker::onMouseMove(MouseEvent& e)
{
    if (dragTarget_ == DragNone) return;
    Rect abs = absoluteRect();
    e.consumed = true;
    switch (dragTarget_)
    {
        case DragWheel:
            updateFromWheel(e.x, e.y, abs);
            break;
        case DragValueBar:
            updateFromValueBar(e.y, abs);
            break;
        case DragAlphaBar:
            updateFromAlphaBar(e.y, abs);
            break;
        default:
            break;
    }
    colorChanged.emit(color());
    markDirty();
}
