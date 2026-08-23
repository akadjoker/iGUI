#include "pch.hpp"
#include "TreePropertyColorWidgets.hpp"
#include "WidgetApp.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

using BuGUI::clamp;
// ─────────────────────────────────────────────────────────────────────────────
//  ColorPickerPopup_ — popup wrapper for PropertyGrid color editing
// ─────────────────────────────────────────────────────────────────────────────
class ColorPickerPopup_ : public Widget
{
public:
    ColorPickerPopup_(PropertyGrid* grid, int row)
        : grid_(grid), row_(row)
    {
        acceptsFocus_ = true;
        picker_ = new ColorPicker();
        picker_->setShowAlpha(true);
        picker_->setColor(grid->propData<PropertyGrid::PropColor>(row).value);

        picker_->colorChanged.connect([this](const Color& c) {
            if (row_ >= 0 && row_ < static_cast<int>(grid_->rows_.size())) {
                auto& d = grid_->propData<PropertyGrid::PropColor>(row_);
                d.value = c;
                if (d.onChange) d.onChange(c);
                grid_->propertyChanged.emit(row_);
                grid_->markDirty();
            }
        });
    }

    ~ColorPickerPopup_() override { delete picker_; }

    void syncPickerRect()
    {
        Rect r = rect_;
        float pad = 6.0f;
        Rect pr{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
        if (pr.x != picker_->rect().x || pr.y != picker_->rect().y ||
            pr.w != picker_->rect().w || pr.h != picker_->rect().h)
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
        if (abs.contains(e.x, e.y)) {
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
    int           row_;
    ColorPicker*  picker_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  PropComboPopup_ — dropdown list for PropertyGrid combo properties
// ─────────────────────────────────────────────────────────────────────────────
class PropComboPopup_ : public Widget
{
public:
    PropComboPopup_(PropertyGrid* grid, int row)
        : grid_(grid), row_(row)
    {
        acceptsFocus_ = true;
        auto& d = grid->propData<PropertyGrid::PropCombo>(row);
        items_    = d.options;
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

        for (int i = 0; i < n; ++i) {
            float ry = abs.y + i * rowH;
            bool isHov = (i == hovered_);
            bool isSel = (i == selected_);

            if (isHov || isSel) {
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
        int idx = static_cast<int>((e.y - abs.y) / rowH);
        if (idx >= 0 && idx < static_cast<int>(items_.size())) {
            if (row_ >= 0 && row_ < static_cast<int>(grid_->rows_.size())) {
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
        int idx = static_cast<int>((e.y - abs.y) / rowH);
        if (idx < 0 || idx >= static_cast<int>(items_.size())) idx = -1;
        if (idx != hovered_) { hovered_ = idx; markDirty(); }
    }

    void onMouseLeave() override
    {
        if (hovered_ != -1) { hovered_ = -1; markDirty(); }
    }

    bool popupContains(float x, float y) const override
    {
        return absoluteRect().contains(x, y);
    }

private:
    PropertyGrid*            grid_;
    int                      row_;
    std::vector<std::string> items_;
    int                      selected_ = -1;
    int                      hovered_  = -1;
};

// ═════════════════════════════════════════════════════════════════════════════
//  TreeNode
// ═════════════════════════════════════════════════════════════════════════════

TreeNode::TreeNode(const std::string& text) : text_(text) {}

TreeNode::~TreeNode()
{
    for (auto* c : children_) delete c;
}

TreeNode* TreeNode::addChild(const std::string& text)
{
    auto* n = new TreeNode(text);
    n->parent_ = this;
    children_.push_back(n);
    return n;
}

void TreeNode::removeChild(int index)
{
    if (index < 0 || index >= static_cast<int>(children_.size())) return;
    delete children_[index];
    children_.erase(children_.begin() + index);
}

void TreeNode::clear()
{
    for (auto* c : children_) delete c;
    children_.clear();
}

void TreeNode::setIcon(const std::string& name)
{
    iconStr_ = name;
    if      (name == "folder")      iconId_ = IconId::Folder;
    else if (name == "folder_open") iconId_ = IconId::FolderOpen;
    else if (name == "file")        iconId_ = IconId::File;
    else if (name == "file_code")   iconId_ = IconId::FileCode;
    else if (name == "book")        iconId_ = IconId::Book;
    else if (name == "gear")        iconId_ = IconId::Gear;
    else if (name == "star")        iconId_ = IconId::Star;
    else if (name == "heart")       iconId_ = IconId::Heart;
    else if (name == "search")      iconId_ = IconId::Search;
    else if (name == "plus")        iconId_ = IconId::Plus;
    else if (name == "minus")       iconId_ = IconId::Minus;
    else if (name == "check")       iconId_ = IconId::Check;
    else if (name == "cross")       iconId_ = IconId::Cross;
    else if (name == "arrow_right") iconId_ = IconId::ArrowRight;
    else if (name == "arrow_down")  iconId_ = IconId::ArrowDown;
    else if (name == "arrow_up")    iconId_ = IconId::ArrowUp;
    else if (name == "arrow_left")  iconId_ = IconId::ArrowLeft;
    else if (name == "eye")         iconId_ = IconId::Eye;
    else if (name == "eye_off")     iconId_ = IconId::EyeOff;
    else if (name == "lock")        iconId_ = IconId::Lock;
    else if (name == "unlock")      iconId_ = IconId::Unlock;
    else if (name == "refresh")     iconId_ = IconId::Refresh;
    else if (name == "trash")       iconId_ = IconId::Trash;
    else if (name == "edit")        iconId_ = IconId::Edit;
    else if (name == "home")        iconId_ = IconId::Home;
    else if (name == "user")        iconId_ = IconId::User;
    else if (name == "warning")     iconId_ = IconId::Warning;
    else if (name == "info")        iconId_ = IconId::Info;
    else if (name == "error")       iconId_ = IconId::Error;
    else                            iconId_ = IconId::None;
}

// ═════════════════════════════════════════════════════════════════════════════
//  TreeView
// ═════════════════════════════════════════════════════════════════════════════

TreeView::TreeView()  { acceptsFocus_ = true; }
TreeView::~TreeView() { clear(); }

void TreeView::setModel(TreeModel* model)
{
    model_ = model;
    if (!model_) return;

    // Build TreeNode wrappers from the model for rendering
    // (TreeView currently renders via TreeNode, so we sync from model)
    model_->modelReset.connect([this] {
        // Rebuild tree nodes from model
        clear();
        if (!model_) return;
        std::function<void(TreeModel::Node*, TreeNode*)> syncNode;
        syncNode = [&](TreeModel::Node* mn, TreeNode* parent) {
            for (auto* mc : mn->children) {
                auto* tn = parent ? parent->addChild(mc->cells.empty() ? "" : mc->cells[0])
                                  : addRoot(mc->cells.empty() ? "" : mc->cells[0]);
                if (!mc->iconName.empty()) tn->setIcon(mc->iconName);
                tn->setExpanded(mc->expanded);
                syncNode(mc, tn);
            }
        };
        for (auto* root : model_->roots()) {
            auto* tn = addRoot(root->cells.empty() ? "" : root->cells[0]);
            if (!root->iconName.empty()) tn->setIcon(root->iconName);
            tn->setExpanded(root->expanded);
            syncNode(root, tn);
        }
        flatDirty_ = true;
        markDirty();
    });
    model_->rowsInserted.connect([this] { flatDirty_ = true; markDirty(); });
    model_->dataChanged.connect([this](ModelIndex) { markDirty(); });

    // Initial sync
    model_->endResetModel();
}

TreeNode* TreeView::addRoot(const std::string& text)
{
    auto* n = new TreeNode(text);
    roots_.push_back(n);
    flatDirty_ = true;
    markDirty();
    return n;
}

void TreeView::clear()
{
    for (auto* r : roots_) delete r;
    roots_.clear();
    selected_     = nullptr;
    scrollOffset_ = 0;
    flatDirty_ = true;
    markDirty();
}

void TreeView::setSelectedNode(TreeNode* n)
{
    if (selected_ != n) {
        selected_ = n;
        selectionChanged.emit(n);
        markDirty();
    }
}

Widget::Vec2f TreeView::sizeHint() const { return {200.f, 200.f}; }

void TreeView::rebuildFlat()
{
    if (!flatDirty_) return;
    flatRows_.clear();
    for (auto* r : roots_) flattenNode(r, 0);
    flatDirty_ = false;
}

void TreeView::flattenNode(TreeNode* n, int depth)
{
    flatRows_.push_back({n, depth});
    if (n->isExpanded())
        for (auto* c : n->children()) flattenNode(c, depth + 1);
}

float TreeView::maxScroll() const
{
    float total = static_cast<float>(flatRows_.size()) * rowHeight_;
    float visible = rect_.h;
    return (total > visible) ? total - visible : 0.f;
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
    int last  = first + static_cast<int>(abs.h / rowHeight_) + 2;
    last = std::min(last, static_cast<int>(flatRows_.size()));

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();

    for (int i = first; i < last; ++i) {
        const auto& row = flatRows_[i];
        float rowY  = abs.y + i * rowHeight_ - scrollOffset_;
        float indX  = abs.x + 4.f + row.depth * indent_;

        if (row.node == selected_) {
            ctx.fill.SetColor(t.selectionColor.r, t.selectionColor.g,
                              t.selectionColor.b, t.selectionColor.a);
            ctx.fillRect(abs.x, rowY, abs.w, rowHeight_);
        }

        if (row.node->hasChildren()) {
            float ts = 7.f;
            float tx = indX;
            float tcy = rowY + rowHeight_ * 0.5f;
            ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
            if (row.node->isExpanded())
                ctx.fillTriangle(tx, tcy - ts*0.35f, tx+ts, tcy - ts*0.35f, tx+ts*0.5f, tcy+ts*0.5f);
            else
                ctx.fillTriangle(tx+1, tcy-ts*0.5f, tx+1, tcy+ts*0.5f, tx+ts, tcy);
            indX += ts + 4.f;
        } else {
            indX += 11.f;
        }

        if (row.node->iconId() != IconId::None) {
            float isz = t.fontSize;
            ctx.drawIcon(row.node->iconId(), indX, rowY + (rowHeight_ - isz) * 0.5f, isz);
            indX += isz + 3.f;
        }

        ctx.font.SetColor(row.node == selected_ ? Color(255,255,255,255) : t.textColor);
        float ty = rowY + (rowHeight_ - t.fontSize) * 0.5f + asc;
        ctx.font.Print(row.node->text().c_str(), indX, ty);
    }

    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.lineRect(abs.x, abs.y, abs.w, abs.h);
    ctx.popClip();
}

void TreeView::onMousePress(MouseEvent& e)
{
    if (!visible_ || !enabled_) return;
    Rect abs = absoluteRect();
    if (!abs.contains(e.x, e.y)) return;
    e.consumed = true;

    rebuildFlat();
    float localY = e.y - abs.y + scrollOffset_;
    int idx = static_cast<int>(localY / rowHeight_);
    if (idx < 0 || idx >= static_cast<int>(flatRows_.size())) return;

    auto& row = flatRows_[idx];
    float indX = 4.f + row.depth * indent_;

    if (row.node->hasChildren()) {
        float triEnd = indX + 11.f;
        if ((e.x - abs.x) < triEnd) {
            row.node->setExpanded(!row.node->isExpanded());
            if (row.node->isExpanded()) nodeExpanded.emit(row.node);
            else nodeCollapsed.emit(row.node);
            flatDirty_ = true;
            markDirty();
            return;
        }
    }
    setSelectedNode(row.node);
}

void TreeView::onMouseScroll(MouseEvent& e)
{
    if (!visible_ || !enabled_) return;
    if (!absoluteRect().contains(e.x, e.y)) return;
    e.consumed = true;
    scrollOffset_ -= e.scrollY * rowHeight_ * 2.f;
    scrollOffset_ = clamp(scrollOffset_, 0.f, maxScroll());
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  PropertyGrid
// ═════════════════════════════════════════════════════════════════════════════

PropertyGrid::PropertyGrid() { acceptsFocus_ = true; }

int PropertyGrid::addSection(const std::string& title)
{
    PropRow r{PropType::Section, title, "", PropSection{}};
    rows_.push_back(std::move(r)); visDirty_=true; markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addString(const std::string& name, const std::string& value,
                             std::function<void(const std::string&)> onChange, const std::string& desc)
{
    PropRow r{PropType::String, name, desc, PropString{value, std::move(onChange)}};
    rows_.push_back(std::move(r)); visDirty_=true; markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addFloat(const std::string& name, float value, float mn, float mx,
                            std::function<void(float)> onChange, const std::string& desc)
{
    PropRow r{PropType::Float, name, desc, PropFloat{value, mn, mx, std::move(onChange)}};
    rows_.push_back(std::move(r)); visDirty_=true; markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addInt(const std::string& name, int value, int mn, int mx,
                          std::function<void(int)> onChange, const std::string& desc)
{
    PropRow r{PropType::Int, name, desc, PropInt{value, mn, mx, std::move(onChange)}};
    rows_.push_back(std::move(r)); visDirty_=true; markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addBool(const std::string& name, bool value,
                           std::function<void(bool)> onChange, const std::string& desc)
{
    PropRow r{PropType::Bool, name, desc, PropBool{value, std::move(onChange)}};
    rows_.push_back(std::move(r)); visDirty_=true; markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addColor(const std::string& name, const Color& value,
                            std::function<void(const Color&)> onChange, const std::string& desc)
{
    PropRow r{PropType::Color, name, desc, PropColor{value, std::move(onChange)}};
    rows_.push_back(std::move(r)); visDirty_=true; markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addCombo(const std::string& name, const std::vector<std::string>& opts,
                            int sel, std::function<void(int)> onChange, const std::string& desc)
{
    PropRow r{PropType::Combo, name, desc, PropCombo{opts, sel, std::move(onChange)}};
    rows_.push_back(std::move(r)); visDirty_=true; markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addVec2(const std::string& name, float x, float y, float mn, float mx,
                           std::function<void(float,float)> onChange, const std::string& desc)
{
    PropVec v; v.value[0]=x; v.value[1]=y; v.components=2; v.min=mn; v.max=mx;
    v.onChange2=std::move(onChange);
    PropRow r{PropType::Vec2, name, desc, std::move(v)};
    rows_.push_back(std::move(r)); visDirty_=true; markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addVec3(const std::string& name, float x, float y, float z, float mn, float mx,
                           std::function<void(float,float,float)> onChange, const std::string& desc)
{
    PropVec v; v.value[0]=x; v.value[1]=y; v.value[2]=z; v.components=3; v.min=mn; v.max=mx;
    v.onChange3=std::move(onChange);
    PropRow r{PropType::Vec3, name, desc, std::move(v)};
    rows_.push_back(std::move(r)); visDirty_=true; markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addVec4(const std::string& name, float x, float y, float z, float w,
                           float mn, float mx,
                           std::function<void(float,float,float,float)> onChange, const std::string& desc)
{
    PropVec v; v.value[0]=x; v.value[1]=y; v.value[2]=z; v.value[3]=w;
    v.components=4; v.min=mn; v.max=mx;
    v.onChange4=std::move(onChange);
    PropRow r{PropType::Vec4, name, desc, std::move(v)};
    rows_.push_back(std::move(r)); visDirty_=true; markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addButton(const std::string& name, std::function<void()> onClick, const std::string& desc)
{
    PropRow r{PropType::Button, name, desc, PropButton{std::move(onClick)}};
    rows_.push_back(std::move(r)); visDirty_=true; markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addSeparator()
{
    PropRow r{PropType::Separator, "", "", PropSeparator{}};
    rows_.push_back(std::move(r)); visDirty_=true; markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

int PropertyGrid::addRange(const std::string& name, float lo, float hi, float mn, float mx,
                            std::function<void(float,float)> onChange, const std::string& desc)
{
    PropRow r{PropType::Range, name, desc, PropRange{lo, hi, mn, mx, std::move(onChange)}};
    rows_.push_back(std::move(r)); visDirty_=true; markDirty();
    return static_cast<int>(rows_.size()) - 1;
}

void PropertyGrid::setString(int row, const std::string& v)
{ if (row>=0&&row<(int)rows_.size()&&rows_[row].type==PropType::String) std::get<PropString>(rows_[row].data).value=v; markDirty(); }
void PropertyGrid::setFloat(int row, float v)
{ if (row>=0&&row<(int)rows_.size()&&rows_[row].type==PropType::Float) { auto& d=std::get<PropFloat>(rows_[row].data); d.value=clamp(v,d.min,d.max); } markDirty(); }
void PropertyGrid::setInt(int row, int v)
{ if (row>=0&&row<(int)rows_.size()&&rows_[row].type==PropType::Int) { auto& d=std::get<PropInt>(rows_[row].data); d.value=clamp(v,d.min,d.max); } markDirty(); }
void PropertyGrid::setBool(int row, bool v)
{ if (row>=0&&row<(int)rows_.size()&&rows_[row].type==PropType::Bool) std::get<PropBool>(rows_[row].data).value=v; markDirty(); }
void PropertyGrid::setColor(int row, const Color& c)
{ if (row>=0&&row<(int)rows_.size()&&rows_[row].type==PropType::Color) std::get<PropColor>(rows_[row].data).value=c; markDirty(); }
void PropertyGrid::setCombo(int row, int idx)
{ if (row>=0&&row<(int)rows_.size()&&rows_[row].type==PropType::Combo) { auto& d=std::get<PropCombo>(rows_[row].data); if (idx>=0&&idx<(int)d.options.size()) d.index=idx; markDirty(); } }
void PropertyGrid::setVec2(int row, float x, float y)
{ if (row>=0&&row<(int)rows_.size()&&rows_[row].type==PropType::Vec2) { auto& d=std::get<PropVec>(rows_[row].data); d.value[0]=clamp(x,d.min,d.max); d.value[1]=clamp(y,d.min,d.max); markDirty(); } }
void PropertyGrid::setVec3(int row, float x, float y, float z)
{ if (row>=0&&row<(int)rows_.size()&&rows_[row].type==PropType::Vec3) { auto& d=std::get<PropVec>(rows_[row].data); d.value[0]=clamp(x,d.min,d.max); d.value[1]=clamp(y,d.min,d.max); d.value[2]=clamp(z,d.min,d.max); markDirty(); } }
void PropertyGrid::setVec4(int row, float x, float y, float z, float w)
{ if (row>=0&&row<(int)rows_.size()&&rows_[row].type==PropType::Vec4) { auto& d=std::get<PropVec>(rows_[row].data); d.value[0]=clamp(x,d.min,d.max); d.value[1]=clamp(y,d.min,d.max); d.value[2]=clamp(z,d.min,d.max); d.value[3]=clamp(w,d.min,d.max); markDirty(); } }
void PropertyGrid::setRange(int row, float lo, float hi)
{ if (row>=0&&row<(int)rows_.size()&&rows_[row].type==PropType::Range) { auto& d=std::get<PropRange>(rows_[row].data); d.lo=clamp(lo,d.min,d.max); d.hi=clamp(hi,d.min,d.max); if (d.lo>d.hi) std::swap(d.lo,d.hi); markDirty(); } }

std::vector<int> PropertyGrid::visibleRows() const
{
    if (!visDirty_) return cachedVisibleRows_;
    cachedVisibleRows_.clear();
    bool collapsed = false;
    for (int i = 0; i < (int)rows_.size(); ++i) {
        if (rows_[i].type == PropType::Section) { collapsed = !std::get<PropSection>(rows_[i].data).expanded; cachedVisibleRows_.push_back(i); }
        else if (!collapsed) cachedVisibleRows_.push_back(i);
    }
    visDirty_ = false;
    return cachedVisibleRows_;
}

float PropertyGrid::visibleTotalHeight() const { return static_cast<float>(visibleRows().size()) * rowHeight_; }
float PropertyGrid::totalHeight()        const { return visibleTotalHeight(); }
float PropertyGrid::gridHeight()         const { return rect_.h - (descHeight_ > 0 ? descHeight_ : 0); }
float PropertyGrid::maxScroll()          const { float t=totalHeight(),v=gridHeight(); return (t>v)?t-v:0.f; }

int PropertyGrid::hitRow(float localY) const
{
    auto vis = visibleRows();
    int vi = static_cast<int>((localY + scrollOffset_) / rowHeight_);
    if (vi < 0 || vi >= (int)vis.size()) return -1;
    return vis[vi];
}

Widget::Vec2f PropertyGrid::sizeHint() const
{
    return {280.f, std::max(100.f, visibleTotalHeight() + descHeight_)};
}

void PropertyGrid::spinStep(int row, int dir)
{
    if (row<0||row>=(int)rows_.size()) return;
    auto& r = rows_[row];
    if (r.type == PropType::Int) {
        auto& d = std::get<PropInt>(r.data);
        int step = std::max(1, (d.max - d.min) / 100);
        d.value = clamp(d.value + dir * step, d.min, d.max);
        if (d.onChange) d.onChange(d.value);
    } else if (r.type == PropType::Float) {
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
    if (row<0||row>=(int)rows_.size()) return;
    auto& r = rows_[row];
    editing_ = true; activeRow_ = row;
    switch (r.type) {
    case PropType::String: editBuf_ = std::get<PropString>(r.data).value; break;
    case PropType::Float:  { char b[32]; snprintf(b,32,"%.2f",std::get<PropFloat>(r.data).value); editBuf_=b; break; }
    case PropType::Int:    { char b[32]; snprintf(b,32,"%d",std::get<PropInt>(r.data).value);     editBuf_=b; break; }
    default: editing_ = false; return;
    }
    editCursor_ = static_cast<int>(editBuf_.size());
    markDirty();
}

void PropertyGrid::commitEdit()
{
    if (!editing_ || activeRow_ < 0) return;
    auto& r = rows_[activeRow_];
    switch (r.type) {
    case PropType::String: { auto& d=std::get<PropString>(r.data); d.value=editBuf_; if (d.onChange) d.onChange(d.value); break; }
    case PropType::Float:  { auto& d=std::get<PropFloat>(r.data); float v=0; try{v=std::stof(editBuf_);}catch(...){} d.value=clamp(v,d.min,d.max); if (d.onChange) d.onChange(d.value); break; }
    case PropType::Int:    { auto& d=std::get<PropInt>(r.data); int v=0; try{v=std::stoi(editBuf_);}catch(...){} d.value=clamp(v,d.min,d.max); if (d.onChange) d.onChange(d.value); break; }
    default: break;
    }
    propertyChanged.emit(activeRow_);
    editing_ = false; activeRow_ = -1;
    markDirty();
}

void PropertyGrid::cancelEdit() { editing_=false; activeRow_=-1; markDirty(); }

// ── Paint ────────────────────────────────────────────────────────────────

void PropertyGrid::paintRow(PaintContext& ctx, const Rect& abs, int idx, float y)
{
    const auto& t = Theme::instance();
    const auto& row = rows_[idx];
    float labelW  = abs.w * labelRatio_;
    float valueX  = abs.x + labelW;
    float valueW  = abs.w - labelW;

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc     = ctx.font.GetAscender();
    float textY   = y + (rowHeight_ - t.fontSize) * 0.5f + asc;

    // ── Section header ───────────────────────────────────────────────────
    if (row.type == PropType::Section) {
        auto& ds = std::get<PropSection>(row.data);
        Color bg = t.collapsibleHeaderBg;
        if (hoveredRow_ == idx) { bg.r=std::min(255,bg.r+12); bg.g=std::min(255,bg.g+12); bg.b=std::min(255,bg.b+12); }
        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fillRect(abs.x, y, abs.w, rowHeight_);
        float ts=7.f, tx=abs.x+6.f, tcy=y+rowHeight_*0.5f;
        ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
        if (ds.expanded) ctx.fillTriangle(tx,tcy-ts*0.35f,tx+ts,tcy-ts*0.35f,tx+ts*0.5f,tcy+ts*0.5f);
        else             ctx.fillTriangle(tx+1,tcy-ts*0.5f,tx+1,tcy+ts*0.5f,tx+ts,tcy);
        ctx.font.SetColor(t.textColor);
        ctx.font.Print(row.name.c_str(), tx+ts+4.f, textY);
        return;
    }

    // Row highlight
    if (idx == selectedRow_)      { ctx.fill.SetColor(t.selectionColor.r,t.selectionColor.g,t.selectionColor.b,t.selectionColor.a); ctx.fillRect(abs.x,y,abs.w,rowHeight_); }
    else if (idx == hoveredRow_)  { ctx.fill.SetColor(t.buttonHover.r,t.buttonHover.g,t.buttonHover.b,30); ctx.fillRect(abs.x,y,abs.w,rowHeight_); }
    if (idx%2==0 && idx!=selectedRow_) { ctx.fill.SetColor(0,0,0,10); ctx.fillRect(abs.x,y,abs.w,rowHeight_); }

    // Label
    ctx.font.SetColor(idx==selectedRow_ ? Color(255,255,255,255) : t.textColor);
    ctx.font.Print(row.name.c_str(), abs.x+8.f, textY);

    // Column separator
    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 50);
    ctx.drawLine(valueX, y, valueX, y + rowHeight_);

    float pad=4.f, valTextX=valueX+pad, valW=valueW-pad*2;

    // ── Editing ──────────────────────────────────────────────────────────
    if (editing_ && activeRow_ == idx) {
        ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
        ctx.fillRect(valTextX, y+1, valW, rowHeight_-2);
        ctx.line.SetColor(t.focusColor.r, t.focusColor.g, t.focusColor.b, t.focusColor.a);
        ctx.lineRect(valTextX, y+1, valW, rowHeight_-2);
        ctx.font.SetColor(Color(255,255,255,255));
        ctx.font.Print(editBuf_.c_str(), valTextX+3, textY);
        float curX = valTextX + 3 + ctx.font.GetTextWidth(editBuf_.substr(0, editCursor_).c_str());
        ctx.line.SetColor(255,255,255,255);
        ctx.drawLine(curX, y+3, curX, y+rowHeight_-3);
        return;
    }

    // ── Slider helper ────────────────────────────────────────────────────
    auto drawSliderBar = [&](float sx, float sy, float sw, float sh,
                             float val, float mn, float mxa, const char* label, Color barCol) {
        ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
        ctx.fillRect(sx, sy, sw, sh);
        float ratio = (mxa-mn>0.0001f) ? clamp((val-mn)/(mxa-mn),0.f,1.f) : 0.f;
        ctx.fill.SetColor(barCol.r, barCol.g, barCol.b, barCol.a);
        ctx.fillRect(sx, sy, sw*ratio, sh);
        ctx.line.SetColor(t.inputBorder.r, t.inputBorder.g, t.inputBorder.b, t.inputBorder.a);
        ctx.lineRect(sx, sy, sw, sh);
        ctx.font.SetColor(Color(230,230,230,255));
        ctx.font.Print(label, sx+3, sy+(sh-t.fontSize)*0.5f+asc);
    };

    switch (row.type) {
    case PropType::String: {
        auto& d = std::get<PropString>(row.data);
        ctx.fill.SetColor(t.inputBg.r,t.inputBg.g,t.inputBg.b,t.inputBg.a); ctx.fillRect(valTextX,y+2,valW,rowHeight_-4);
        ctx.line.SetColor(t.inputBorder.r,t.inputBorder.g,t.inputBorder.b,t.inputBorder.a); ctx.lineRect(valTextX,y+2,valW,rowHeight_-4);
        ctx.font.SetColor(Color(200,220,255,255)); ctx.font.Print(d.value.c_str(), valTextX+3, textY);
        break;
    }
    case PropType::Float: {
        auto& d = std::get<PropFloat>(row.data);
        char buf[32]; snprintf(buf,32,"%.2f",d.value);
        Color bc(60,100,160,200);
        if (draggingSlider_&&activeRow_==idx) bc=Color(80,130,200,220);
        else if (hoveredRow_==idx) bc=Color(70,110,175,210);
        drawSliderBar(valTextX,y+2,valW,rowHeight_-4,d.value,d.min,d.max,buf,bc);
        break;
    }
    case PropType::Int: {
        auto& d = std::get<PropInt>(row.data);
        char buf[32]; snprintf(buf,32,"%d",d.value);
        Color bc(60,140,90,200);
        if (draggingSlider_&&activeRow_==idx) bc=Color(80,170,110,220);
        else if (hoveredRow_==idx) bc=Color(70,155,100,210);
        drawSliderBar(valTextX,y+2,valW,rowHeight_-4,(float)d.value,(float)d.min,(float)d.max,buf,bc);
        break;
    }
    case PropType::Bool: {
        auto& d = std::get<PropBool>(row.data);
        float bs=14.f, bx=valTextX, by=y+(rowHeight_-bs)*0.5f;
        ctx.fill.SetColor(t.inputBg.r,t.inputBg.g,t.inputBg.b,t.inputBg.a); ctx.fillRect(bx,by,bs,bs);
        ctx.line.SetColor(t.inputBorder.r,t.inputBorder.g,t.inputBorder.b,t.inputBorder.a); ctx.lineRect(bx,by,bs,bs);
        if (d.value) { ctx.line.SetColor(t.checkMark.r,t.checkMark.g,t.checkMark.b,t.checkMark.a); ctx.drawLine(bx+3,by+bs*0.5f,bx+bs*0.4f,by+bs-3); ctx.drawLine(bx+bs*0.4f,by+bs-3,bx+bs-3,by+3); }
        break;
    }
    case PropType::Color: {
        auto& d = std::get<PropColor>(row.data);
        float sh2=rowHeight_-6.f, sw2=std::min(valW,sh2*3.f), sy2=y+3.f;
        ctx.fill.SetColor(d.value.r,d.value.g,d.value.b,d.value.a); ctx.fillRect(valTextX,sy2,sw2,sh2);
        ctx.line.SetColor(t.borderColor.r,t.borderColor.g,t.borderColor.b,t.borderColor.a); ctx.lineRect(valTextX,sy2,sw2,sh2);
        char buf[20]; snprintf(buf,20,"#%02X%02X%02X",d.value.r,d.value.g,d.value.b);
        ctx.font.SetColor(Color(180,180,180,255)); ctx.font.Print(buf, valTextX+sw2+4.f, textY);
        break;
    }
    case PropType::Combo: {
        auto& d = std::get<PropCombo>(row.data);
        ctx.fill.SetColor(t.inputBg.r,t.inputBg.g,t.inputBg.b,t.inputBg.a); ctx.fillRect(valTextX,y+2,valW,rowHeight_-4);
        ctx.line.SetColor(t.inputBorder.r,t.inputBorder.g,t.inputBorder.b,t.inputBorder.a); ctx.lineRect(valTextX,y+2,valW,rowHeight_-4);
        const std::string& txt = (d.index>=0&&d.index<(int)d.options.size()) ? d.options[d.index] : "";
        ctx.font.SetColor(Color(180,220,180,255)); ctx.font.Print(txt.c_str(), valTextX+3, textY);
        float ax=valueX+valueW-14.f, ay=y+rowHeight_*0.5f;
        ctx.fill.SetColor(t.textColor.r,t.textColor.g,t.textColor.b,t.textColor.a); ctx.fillTriangle(ax,ay-3,ax+8,ay-3,ax+4,ay+3);
        break;
    }
    case PropType::Vec2: case PropType::Vec3: case PropType::Vec4: {
        auto& d = std::get<PropVec>(row.data);
        int nc=d.components;
        static const Color ccs[]={{180,60,60,200},{60,140,60,200},{60,80,180,200},{140,120,60,200}};
        static const char* cls[]={"X","Y","Z","W"};
        float gap=2.f, compW=(valW-gap*(nc-1))/nc;
        for (int c=0; c<nc; ++c) {
            float cx2=valTextX+c*(compW+gap); char buf[32]; snprintf(buf,32,"%s %.1f",cls[c],d.value[c]);
            Color bc=ccs[c]; if (draggingSlider_&&activeRow_==idx&&dragComponent_==c) bc=Color(std::min(255,bc.r+30),std::min(255,bc.g+30),std::min(255,bc.b+30),220);
            drawSliderBar(cx2,y+2,compW,rowHeight_-4,d.value[c],d.min,d.max,buf,bc);
        }
        break;
    }
    case PropType::Button: {
        Color bc=(hoveredRow_==idx)?t.buttonHover:t.buttonNormal;
        ctx.fill.SetColor(bc.r,bc.g,bc.b,bc.a); ctx.fillRoundedRect(valTextX,y+2,valW,rowHeight_-4,3.f);
        ctx.line.SetColor(t.borderColor.r,t.borderColor.g,t.borderColor.b,t.borderColor.a); ctx.lineRoundedRect(valTextX,y+2,valW,rowHeight_-4,3.f);
        float tw=ctx.font.GetTextWidth(row.name.c_str()); ctx.font.SetColor(t.textColor);
        ctx.font.Print(row.name.c_str(), valTextX+(valW-tw)*0.5f, textY);
        break;
    }
    case PropType::Separator: {
        float ly=y+rowHeight_*0.5f; ctx.line.SetColor(t.borderColor.r,t.borderColor.g,t.borderColor.b,t.borderColor.a);
        ctx.drawLine(abs.x+4.f,ly,abs.x+abs.w-4.f,ly); break;
    }
    case PropType::Range: {
        auto& d = std::get<PropRange>(row.data);
        float range=d.max-d.min;
        float rlo=(range>0.0001f)?(d.lo-d.min)/range:0.f;
        float rhi=(range>0.0001f)?(d.hi-d.min)/range:1.f;
        rlo=clamp(rlo,0.f,1.f); rhi=clamp(rhi,0.f,1.f);
        float barY2=y+2, barH2=rowHeight_-4;
        ctx.fill.SetColor(t.inputBg.r,t.inputBg.g,t.inputBg.b,t.inputBg.a); ctx.fillRect(valTextX,barY2,valW,barH2);
        float loX=valTextX+rlo*valW, hiX=valTextX+rhi*valW;
        Color bc(80,140,180,200);
        if (draggingSlider_&&activeRow_==idx) bc=Color(100,170,210,230);
        else if (hoveredRow_==idx) bc=Color(90,155,195,215);
        ctx.fill.SetColor(bc.r,bc.g,bc.b,bc.a); ctx.fillRect(loX,barY2,hiX-loX,barH2);
        ctx.fill.SetColor(220,220,220,240); ctx.fillRect(loX-1.5f,barY2,3.f,barH2); ctx.fillRect(hiX-1.5f,barY2,3.f,barH2);
        ctx.line.SetColor(t.inputBorder.r,t.inputBorder.g,t.inputBorder.b,t.inputBorder.a); ctx.lineRect(valTextX,barY2,valW,barH2);
        char buf[48]; snprintf(buf,48,"%.1f - %.1f",d.lo,d.hi);
        ctx.font.SetColor(Color(230,230,230,255)); ctx.font.Print(buf,valTextX+3,textY);
        break;
    }
    default: break;
    }
}

void PropertyGrid::paintDescPanel(PaintContext& ctx, const Rect& abs)
{
    if (descHeight_<=0) return;
    const auto& t=Theme::instance();
    float gh=gridHeight(), descY=abs.y+gh;
    ctx.line.SetColor(t.borderColor.r,t.borderColor.g,t.borderColor.b,t.borderColor.a);
    ctx.drawLine(abs.x,descY,abs.x+abs.w,descY);
    ctx.fill.SetColor(t.panelColor.r,t.panelColor.g,t.panelColor.b,t.panelColor.a);
    ctx.fillRect(abs.x,descY+1,abs.w,descHeight_-1);
    if (selectedRow_<0||selectedRow_>=(int)rows_.size()) return;
    const auto& row=rows_[selectedRow_];
    ctx.font.SetFontSize(t.fontSize); ctx.font.SetBatch(&ctx.text); float asc=ctx.font.GetAscender();
    ctx.font.SetColor(Color(220,220,225,255)); ctx.font.Print(row.name.c_str(),abs.x+6,descY+4+asc);
    if (!row.description.empty()) { ctx.font.SetColor(Color(150,150,155,255)); ctx.font.Print(row.description.c_str(),abs.x+6,descY+4+t.fontSize+2+asc); }
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

    ctx.pushClip({abs.x, abs.y, abs.w, gh});

    auto vis = visibleRows();
    int count = (int)vis.size();
    int first = static_cast<int>(scrollOffset_ / rowHeight_);
    int last  = std::min(first + static_cast<int>(gh / rowHeight_) + 2, count);

    for (int vi = first; vi < last; ++vi) {
        float rowY = abs.y + vi * rowHeight_ - scrollOffset_;
        paintRow(ctx, abs, vis[vi], rowY);
    }

    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 40);
    for (int vi = first; vi < last; ++vi) {
        float ry = abs.y + (vi+1)*rowHeight_ - scrollOffset_;
        ctx.drawLine(abs.x, ry, abs.x+abs.w, ry);
    }

    ctx.popClip();
    paintDescPanel(ctx, abs);

    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.lineRect(abs.x, abs.y, abs.w, abs.h);
}

void PropertyGrid::onMousePress(MouseEvent& e)
{
    if (!visible_||!enabled_) return;
    Rect abs = absoluteRect();
    if (!abs.contains(e.x,e.y)) return;
    e.consumed = true;

    float gh = gridHeight();
    if (e.y > abs.y+gh) return;

    int idx = hitRow(e.y - abs.y);
    if (idx < 0) return;
    auto& row = rows_[idx];

    if (row.type == PropType::Section) { std::get<PropSection>(row.data).expanded=!std::get<PropSection>(row.data).expanded; visDirty_=true; markDirty(); return; }
    if (row.type == PropType::Separator) return;

    if (selectedRow_ != idx) { if (editing_) commitEdit(); selectedRow_=idx; markDirty(); }

    float labelW=abs.w*labelRatio_, valueX=abs.x+labelW;
    if (e.x < valueX) return;
    float valueW=abs.w-labelW, pad=4.f, valTextX=valueX+pad, valW=valueW-pad*2;

    switch (row.type) {
    case PropType::Bool: {
        auto& d=std::get<PropBool>(row.data);
        d.value=!d.value; if (d.onChange) d.onChange(d.value);
        propertyChanged.emit(idx); markDirty(); break;
    }
    case PropType::Float: {
        auto& d=std::get<PropFloat>(row.data);
        if (editing_&&activeRow_==idx) break;
        draggingSlider_=true; activeRow_=idx; dragStartX_=e.x; dragComponent_=0;
        float ratio=clamp((e.x-valTextX)/valW,0.f,1.f);
        d.value=d.min+ratio*(d.max-d.min); dragStartVal_=d.value;
        if (d.onChange) d.onChange(d.value); markDirty(); break;
    }
    case PropType::Int: {
        auto& d=std::get<PropInt>(row.data);
        if (editing_&&activeRow_==idx) break;
        draggingSlider_=true; activeRow_=idx; dragStartX_=e.x; dragComponent_=0;
        float ratio=clamp((e.x-valTextX)/valW,0.f,1.f);
        d.value=static_cast<int>(d.min+ratio*(d.max-d.min)+0.5f); dragStartVal_=(float)d.value;
        if (d.onChange) d.onChange(d.value); markDirty(); break;
    }
    case PropType::Vec2: case PropType::Vec3: case PropType::Vec4: {
        auto& d=std::get<PropVec>(row.data);
        int nc=d.components; float gap=2.f, compW=(valW-gap*(nc-1))/nc;
        int comp=clamp((int)((e.x-valTextX)/(compW+gap)),0,nc-1);
        draggingSlider_=true; activeRow_=idx; dragStartX_=e.x; dragComponent_=comp;
        float cx2=valTextX+comp*(compW+gap), ratio=clamp((e.x-cx2)/compW,0.f,1.f);
        d.value[comp]=d.min+ratio*(d.max-d.min); dragStartVal_=d.value[comp];
        if (row.type==PropType::Vec2&&d.onChange2) d.onChange2(d.value[0],d.value[1]);
        else if (row.type==PropType::Vec3&&d.onChange3) d.onChange3(d.value[0],d.value[1],d.value[2]);
        else if (row.type==PropType::Vec4&&d.onChange4) d.onChange4(d.value[0],d.value[1],d.value[2],d.value[3]);
        markDirty(); break;
    }
    case PropType::String: if (!editing_||activeRow_!=idx) startEdit(idx); break;
    case PropType::Combo: {
        auto& d=std::get<PropCombo>(row.data);
        auto vis=visibleRows(); float rowY=abs.y;
        for (int vi=0;vi<(int)vis.size();++vi) { if (vis[vi]==idx) { rowY=abs.y+vi*rowHeight_-scrollOffset_; break; } }
        const auto& io2=BuGUI::GetIO();
        int n=(int)d.options.size(); float itemH=22.f, popH=n*itemH+2, popW=abs.w-abs.w*labelRatio_;
        float popX=abs.x+abs.w*labelRatio_;
        float popY=rowY+rowHeight_;
        if (popX+popW > io2.displayWidth)  popX = io2.displayWidth  - popW;
        if (popX < 0.f) popX = 0.f;
        if (popY+popH > io2.displayHeight) popY = (rowY-popH >= 0.f) ? rowY-popH : io2.displayHeight-popH;
        if (popY < 0.f) popY = 0.f;
        auto* popup=new PropComboPopup_(this,idx); popup->setRect({popX,popY,popW,popH});
        WidgetApp::instance().showPopup(popup,nullptr); e.consumed=true; markDirty(); break;
    }
    case PropType::Button: { auto& d=std::get<PropButton>(row.data); if (d.onClick) d.onClick(); markDirty(); break; }
    case PropType::Range: {
        auto& d=std::get<PropRange>(row.data);
        float range=d.max-d.min;
        float rlo=(range>0.0001f)?(d.lo-d.min)/range:0.f;
        float rhi=(range>0.0001f)?(d.hi-d.min)/range:1.f;
        float loX=valTextX+rlo*valW, hiX=valTextX+rhi*valW;
        int handle=(std::abs(e.x-loX)<=std::abs(e.x-hiX))?0:1;
        draggingSlider_=true; activeRow_=idx; dragStartX_=e.x; dragComponent_=handle;
        float ratio=clamp((e.x-valTextX)/valW,0.f,1.f), newVal=d.min+ratio*range;
        if (handle==0) d.lo=clamp(newVal,d.min,d.hi);
        else           d.hi=clamp(newVal,d.lo,d.max);
        dragStartVal_=(handle==0)?d.lo:d.hi;
        if (d.onChange) d.onChange(d.lo,d.hi); markDirty(); break;
    }
    case PropType::Color: {
        auto vis=visibleRows(); float rowY=abs.y;
        for (int vi=0;vi<(int)vis.size();++vi) { if (vis[vi]==idx) { rowY=abs.y+vi*rowHeight_-scrollOffset_; break; } }
        const auto& io=BuGUI::GetIO();
        float popW=230.f, popH=270.f;
        float popX=abs.x+abs.w*labelRatio_;
        float popY=rowY+rowHeight_;
        if (popX+popW > io.displayWidth)  popX = io.displayWidth  - popW;
        if (popX < 0.f) popX = 0.f;
        if (popY+popH > io.displayHeight) popY = (rowY-popH >= 0.f) ? rowY-popH : io.displayHeight-popH;
        if (popY < 0.f) popY = 0.f;
        auto* popup=new ColorPickerPopup_(this,idx); popup->setRect({popX,popY,popW,popH});
        WidgetApp::instance().showPopup(popup,nullptr); e.consumed=true; break;
    }
    default: break;
    }
}

void PropertyGrid::onMouseRelease(MouseEvent& e)
{
    if (draggingSlider_) {
        draggingSlider_ = false;
        if (activeRow_ >= 0) {
            float dx = std::abs(e.x - dragStartX_);
            if (dx < 3.f) {
                auto& row = rows_[activeRow_];
                if (row.type==PropType::Float||row.type==PropType::Int) startEdit(activeRow_);
            } else propertyChanged.emit(activeRow_);
            if (!editing_) activeRow_ = -1;
        }
    }
    (void)e;
}

void PropertyGrid::onMouseMove(MouseEvent& e)
{
    if (!visible_||!enabled_) return;
    Rect abs = absoluteRect();

    if (draggingSlider_ && activeRow_>=0 && activeRow_<(int)rows_.size()) {
        e.consumed = true;
        auto& row = rows_[activeRow_];
        float labelW=abs.w*labelRatio_, valueX=abs.x+labelW, valueW=abs.w-labelW;
        float pad=4.f, valTextX=valueX+pad, valW=valueW-pad*2;
        float mn=0,mx2=1,sliderX=valTextX,sliderW=valW;
        if (row.type==PropType::Float) { auto& d=std::get<PropFloat>(row.data); mn=d.min; mx2=d.max; }
        else if (row.type==PropType::Int) { auto& d=std::get<PropInt>(row.data); mn=(float)d.min; mx2=(float)d.max; }
        else if (row.type==PropType::Vec2||row.type==PropType::Vec3||row.type==PropType::Vec4) {
            auto& d=std::get<PropVec>(row.data); mn=d.min; mx2=d.max;
            int nc=d.components; float gap=2.f, compW=(valW-gap*(nc-1))/nc;
            sliderX=valTextX+dragComponent_*(compW+gap); sliderW=compW;
        } else if (row.type==PropType::Range) { auto& d=std::get<PropRange>(row.data); mn=d.min; mx2=d.max; }

        float range=mx2-mn;
        if (range>0.0001f&&sliderW>1.f) {
            float ratio=clamp((e.x-sliderX)/sliderW,0.f,1.f), newVal=mn+ratio*range;
            if (row.type==PropType::Float) { auto& d=std::get<PropFloat>(row.data); d.value=newVal; if (d.onChange) d.onChange(d.value); }
            else if (row.type==PropType::Int) { auto& d=std::get<PropInt>(row.data); d.value=(int)(newVal+0.5f); if (d.onChange) d.onChange(d.value); }
            else if (row.type==PropType::Range) {
                auto& d=std::get<PropRange>(row.data);
                if (dragComponent_==0) d.lo=clamp(newVal,d.min,d.hi);
                else                   d.hi=clamp(newVal,d.lo,d.max);
                if (d.onChange) d.onChange(d.lo,d.hi);
            } else {
                auto& d=std::get<PropVec>(row.data);
                d.value[dragComponent_]=newVal;
                if (row.type==PropType::Vec2&&d.onChange2) d.onChange2(d.value[0],d.value[1]);
                else if (row.type==PropType::Vec3&&d.onChange3) d.onChange3(d.value[0],d.value[1],d.value[2]);
                else if (row.type==PropType::Vec4&&d.onChange4) d.onChange4(d.value[0],d.value[1],d.value[2],d.value[3]);
            }
            markDirty();
        }
        return;
    }

    if (abs.contains(e.x,e.y)&&e.y<abs.y+gridHeight()) {
        int idx=hitRow(e.y-abs.y);
        if (idx!=hoveredRow_) { hoveredRow_=idx; markDirty(); }
    } else { if (hoveredRow_!=-1) { hoveredRow_=-1; markDirty(); } }
}

void PropertyGrid::onMouseScroll(MouseEvent& e)
{
    if (!visible_||!enabled_) return;
    if (!absoluteRect().contains(e.x,e.y)) return;
    e.consumed=true;
    scrollOffset_ -= e.scrollY*rowHeight_*2.f;
    scrollOffset_  = clamp(scrollOffset_,0.f,maxScroll());
    markDirty();
}

void PropertyGrid::onKeyPress(KeyEvent& e)
{
    if (!focused_&&!editing_) return;
    if (editing_) {
        e.consumed=true;
        switch (e.key) {
        case BuGUI::Key::Return: case BuGUI::Key::KPEnter: commitEdit(); return;
        case BuGUI::Key::Escape: cancelEdit(); return;
        case BuGUI::Key::Backspace: if (editCursor_>0&&!editBuf_.empty()) { editBuf_.erase(editCursor_-1,1); editCursor_--; markDirty(); } return;
        case BuGUI::Key::Delete: if (editCursor_<(int)editBuf_.size()) { editBuf_.erase(editCursor_,1); markDirty(); } return;
        case BuGUI::Key::Right: if (editCursor_<(int)editBuf_.size()) { editCursor_++; markDirty(); } return;
        case BuGUI::Key::Left:  if (editCursor_>0) { editCursor_--; markDirty(); } return;
        case BuGUI::Key::Home: editCursor_=0; markDirty(); return;
        case BuGUI::Key::End:  editCursor_=(int)editBuf_.size(); markDirty(); return;
        }
    }
    if (!editing_) {
        auto vis=visibleRows(); int visIdx=-1;
        for (int i=0;i<(int)vis.size();++i) if (vis[i]==selectedRow_) { visIdx=i; break; }
        if (e.key==BuGUI::Key::Up) { e.consumed=true; if (visIdx>0) { selectedRow_=vis[visIdx-1]; markDirty(); } }
        else if (e.key==BuGUI::Key::Down) { e.consumed=true; if (visIdx<(int)vis.size()-1) { selectedRow_=vis[visIdx+1]; markDirty(); } }
        else if (e.key==BuGUI::Key::Return||e.key==BuGUI::Key::KPEnter) {
            e.consumed=true;
            if (selectedRow_>=0&&selectedRow_<(int)rows_.size()) {
                auto& row=rows_[selectedRow_];
                if (row.type==PropType::Section) { auto& ds=std::get<PropSection>(row.data); ds.expanded=!ds.expanded; visDirty_=true; }
                else if (row.type==PropType::String||row.type==PropType::Float||row.type==PropType::Int) startEdit(selectedRow_);
                else if (row.type==PropType::Bool) { auto& db=std::get<PropBool>(row.data); db.value=!db.value; if (db.onChange) db.onChange(db.value); propertyChanged.emit(selectedRow_); }
                markDirty();
            }
        }
    }
}

void PropertyGrid::onTextInput(KeyEvent& e)
{
    if (!editing_) return;
    e.consumed=true;
    if (activeRow_>=0&&activeRow_<(int)rows_.size()) {
        auto& row=rows_[activeRow_];
        if (row.type==PropType::Int) {
            for (int i=0;e.text[i];++i) { char c=e.text[i]; if ((c>='0'&&c<='9')||c=='-') { editBuf_.insert(editCursor_,1,c); editCursor_++; } }
            markDirty(); return;
        }
        if (row.type==PropType::Float) {
            for (int i=0;e.text[i];++i) { char c=e.text[i]; if ((c>='0'&&c<='9')||c=='-'||c=='.') { editBuf_.insert(editCursor_,1,c); editCursor_++; } }
            markDirty(); return;
        }
    }
    for (int i=0;e.text[i];++i) { editBuf_.insert(editCursor_,1,e.text[i]); editCursor_++; }
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  ColorPicker
// ═════════════════════════════════════════════════════════════════════════════

ColorPicker::ColorPicker() { acceptsFocus_=true; }

void ColorPicker::setColor(const Color& c) { rgbToHsv(c,hue_,sat_,val_); alpha_=c.a/255.f; markDirty(); }
Color ColorPicker::color() const { return hsvToRgb(hue_,sat_,val_,alpha_); }
Widget::Vec2f ColorPicker::sizeHint() const { return {220.f,260.f}; }

float ColorPicker::wheelRadius (const Rect& abs) const { float bs=30.f+(showAlpha_?30.f:0.f), av=std::min(abs.w-bs,abs.h-44.f); return std::max(20.f,av*0.5f); }
float ColorPicker::wheelCenterX(const Rect& abs) const { return abs.x+wheelRadius(abs)+4.f; }
float ColorPicker::wheelCenterY(const Rect& abs) const { return abs.y+wheelRadius(abs)+4.f; }
Rect  ColorPicker::valueBarRect (const Rect& abs) const { float r=wheelRadius(abs),cx=wheelCenterX(abs); return {cx+r+10.f,abs.y+4.f,18.f,r*2.f}; }
Rect  ColorPicker::alphaBarRect (const Rect& abs) const { Rect v=valueBarRect(abs); return {v.x+v.w+8.f,v.y,v.w,v.h}; }
Rect  ColorPicker::previewRect  (const Rect& abs) const { float r=wheelRadius(abs); return {abs.x+4.f,abs.y+r*2.f+12.f,40.f,28.f}; }
Rect  ColorPicker::hexRect      (const Rect& abs) const { Rect p=previewRect(abs); return {p.x+p.w+8.f,p.y,100.f,p.h}; }

void ColorPicker::updateFromWheel(float mx, float my, const Rect& abs)
{
    float cx=wheelCenterX(abs), cy=wheelCenterY(abs), r=wheelRadius(abs), ir=r*0.75f;
    float dx=mx-cx, dy=my-cy, dist=sqrtf(dx*dx+dy*dy);
    hue_=atan2f(dy,dx)/(2.f*3.14159265f); if (hue_<0) hue_+=1.f;
    sat_=std::min(dist/ir,1.f);
}

void ColorPicker::updateFromValueBar(float my, const Rect& abs)
{ Rect b=valueBarRect(abs); val_=1.f-clamp((my-b.y)/b.h,0.f,1.f); }

void ColorPicker::updateFromAlphaBar(float my, const Rect& abs)
{ Rect b=alphaBarRect(abs); alpha_=1.f-clamp((my-b.y)/b.h,0.f,1.f); }

Color ColorPicker::hsvToRgb(float h, float s, float v, float a)
{
    float r=v,g=v,b=v;
    if (s>0.f) {
        h=fmodf(h,1.f)*6.f; int i=(int)h; float f=h-i,p=v*(1-s),q=v*(1-s*f),t2=v*(1-s*(1-f));
        switch(i){case 0:r=v;g=t2;b=p;break;case 1:r=q;g=v;b=p;break;case 2:r=p;g=v;b=t2;break;case 3:r=p;g=q;b=v;break;case 4:r=t2;g=p;b=v;break;default:r=v;g=p;b=q;break;}
    }
    return Color((u8)(r*255),(u8)(g*255),(u8)(b*255),(u8)(a*255));
}

void ColorPicker::rgbToHsv(const Color& c, float& h, float& s, float& v)
{
    float r=c.r/255.f,g=c.g/255.f,b=c.b/255.f;
    float mx=std::max({r,g,b}),mn2=std::min({r,g,b}),d=mx-mn2;
    v=mx; s=(mx>0)?d/mx:0;
    if (d<0.00001f) h=0;
    else if (mx==r) h=fmodf((g-b)/d,6.f)/6.f;
    else if (mx==g) h=((b-r)/d+2.f)/6.f;
    else            h=((r-g)/d+4.f)/6.f;
    if (h<0) h+=1.f;
}

void ColorPicker::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs=absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t=Theme::instance();
    ctx.fill.SetColor(t.panelColor.r,t.panelColor.g,t.panelColor.b,t.panelColor.a);
    ctx.fillRect(abs.x,abs.y,abs.w,abs.h);
    ctx.pushClip(abs);

    float cx=wheelCenterX(abs), cy=wheelCenterY(abs), r=wheelRadius(abs);

    // Hue ring
    const int segs=90; const float step=2.f*3.14159265f/segs;
    float outerR=r, innerR=r*0.75f;
    for (int i=0;i<segs;++i) {
        float a0=step*i, a1=step*(i+1)+0.005f;
        Color c=hsvToRgb((float)i/segs,1.f,1.f,1.f);
        ctx.fill.SetColor(c.r,c.g,c.b,c.a);
        ctx.fillTriangle(cx+cosf(a0)*outerR,cy+sinf(a0)*outerR,cx+cosf(a1)*outerR,cy+sinf(a1)*outerR,cx+cosf(a0)*innerR,cy+sinf(a0)*innerR);
        ctx.fillTriangle(cx+cosf(a1)*outerR,cy+sinf(a1)*outerR,cx+cosf(a1)*innerR,cy+sinf(a1)*innerR,cx+cosf(a0)*innerR,cy+sinf(a0)*innerR);
    }

    // Saturation disc
    for (int ring=0;ring<16;++ring) {
        float r0=innerR*ring/16, r1=innerR*(ring+1)/16+0.5f;
        float s0=(float)ring/16, s1=(float)(ring+1)/16;
        for (int seg=0;seg<48;++seg) {
            float a0=2.f*3.14159265f*seg/48, a1=2.f*3.14159265f*(seg+1)/48+0.005f;
            float h=(float)seg/48;
            Color c0=hsvToRgb(h,s0,val_,1.f), c1=hsvToRgb(h,s1,val_,1.f);
            Color avg; avg.r=(c0.r+c1.r)/2; avg.g=(c0.g+c1.g)/2; avg.b=(c0.b+c1.b)/2; avg.a=255;
            ctx.fill.SetColor(avg.r,avg.g,avg.b,avg.a);
            ctx.fillTriangle(cx+cosf(a0)*r0,cy+sinf(a0)*r0,cx+cosf(a0)*r1,cy+sinf(a0)*r1,cx+cosf(a1)*r1,cy+sinf(a1)*r1);
            ctx.fillTriangle(cx+cosf(a0)*r0,cy+sinf(a0)*r0,cx+cosf(a1)*r1,cy+sinf(a1)*r1,cx+cosf(a1)*r0,cy+sinf(a1)*r0);
        }
    }

    // Hue/sat cursor
    float ca=hue_*2.f*3.14159265f, cd=sat_*innerR;
    float curX=cx+cosf(ca)*cd, curY=cy+sinf(ca)*cd;
    ctx.line.SetColor(255,255,255,255); ctx.lineCircle(curX,curY,5.f);
    ctx.line.SetColor(0,0,0,255);       ctx.lineCircle(curX,curY,4.f);

    // Value bar
    Rect vb=valueBarRect(abs); float stepH=vb.h/20;
    for (int i=0;i<20;++i) {
        float v2=1.f-(float)i/20; Color c2=hsvToRgb(hue_,sat_,v2,1.f);
        ctx.fill.SetColor(c2.r,c2.g,c2.b,255); ctx.fillRect(vb.x,vb.y+i*stepH,vb.w,stepH+1);
    }
    ctx.line.SetColor(t.borderColor.r,t.borderColor.g,t.borderColor.b,t.borderColor.a); ctx.lineRect(vb.x,vb.y,vb.w,vb.h);
    float valY=vb.y+(1.f-val_)*vb.h;
    ctx.fill.SetColor(255,255,255,255);
    ctx.fillTriangle(vb.x-4,valY-4,vb.x-4,valY+4,vb.x,valY);
    ctx.fillTriangle(vb.x+vb.w+4,valY-4,vb.x+vb.w+4,valY+4,vb.x+vb.w,valY);

    // Alpha bar
    if (showAlpha_) {
        Rect ab2=alphaBarRect(abs); Color full=hsvToRgb(hue_,sat_,val_,1.f);
        for (int i=0;i<20;++i) {
            float a=(1.f-(float)i/20); ctx.fill.SetColor(full.r,full.g,full.b,(u8)(a*255));
            ctx.fillRect(ab2.x,ab2.y+i*stepH,ab2.w,stepH+1);
        }
        ctx.line.SetColor(t.borderColor.r,t.borderColor.g,t.borderColor.b,t.borderColor.a); ctx.lineRect(ab2.x,ab2.y,ab2.w,ab2.h);
        float alphaY=ab2.y+(1.f-alpha_)*ab2.h;
        ctx.fill.SetColor(255,255,255,255);
        ctx.fillTriangle(ab2.x-4,alphaY-4,ab2.x-4,alphaY+4,ab2.x,alphaY);
        ctx.fillTriangle(ab2.x+ab2.w+4,alphaY-4,ab2.x+ab2.w+4,alphaY+4,ab2.x+ab2.w,alphaY);
    }

    // Preview swatch + hex
    Rect pr=previewRect(abs); Color cur=color();
    ctx.fill.SetColor(cur.r,cur.g,cur.b,cur.a); ctx.fillRect(pr.x,pr.y,pr.w,pr.h);
    ctx.line.SetColor(t.borderColor.r,t.borderColor.g,t.borderColor.b,t.borderColor.a); ctx.lineRect(pr.x,pr.y,pr.w,pr.h);
    Rect hr=hexRect(abs); char hexBuf[20];
    if (showAlpha_) snprintf(hexBuf,20,"#%02X%02X%02X%02X",cur.r,cur.g,cur.b,cur.a);
    else            snprintf(hexBuf,20,"#%02X%02X%02X",cur.r,cur.g,cur.b);
    ctx.font.SetFontSize(t.fontSize); ctx.font.SetColor(t.textColor); ctx.font.SetBatch(&ctx.text);
    ctx.font.Print(hexBuf,hr.x,hr.y+(hr.h-t.fontSize)*0.5f+ctx.font.GetAscender());

    ctx.line.SetColor(t.borderColor.r,t.borderColor.g,t.borderColor.b,t.borderColor.a); ctx.lineRect(abs.x,abs.y,abs.w,abs.h);
    ctx.popClip();
}

void ColorPicker::onMousePress(MouseEvent& e)
{
    if (!visible_||!enabled_) return;
    Rect abs=absoluteRect();
    if (!abs.contains(e.x,e.y)) return;
    e.consumed=true;

    float cx=wheelCenterX(abs), cy=wheelCenterY(abs), r=wheelRadius(abs);
    float dx=e.x-cx, dy=e.y-cy, dist=sqrtf(dx*dx+dy*dy);

    Rect vb=valueBarRect(abs);
    if (vb.contains(e.x,e.y)) { dragTarget_=DragValueBar; updateFromValueBar(e.y,abs); colorChanged.emit(color()); markDirty(); return; }
    if (showAlpha_) { Rect ab2=alphaBarRect(abs); if (ab2.contains(e.x,e.y)) { dragTarget_=DragAlphaBar; updateFromAlphaBar(e.y,abs); colorChanged.emit(color()); markDirty(); return; } }
    if (dist<=r) { dragTarget_=DragWheel; updateFromWheel(e.x,e.y,abs); colorChanged.emit(color()); markDirty(); }
}

void ColorPicker::onMouseRelease(MouseEvent& e) { dragTarget_=DragNone; (void)e; }

void ColorPicker::onMouseMove(MouseEvent& e)
{
    if (dragTarget_==DragNone) return;
    Rect abs=absoluteRect(); e.consumed=true;
    switch (dragTarget_) {
    case DragWheel:    updateFromWheel(e.x,e.y,abs); break;
    case DragValueBar: updateFromValueBar(e.y,abs);  break;
    case DragAlphaBar: updateFromAlphaBar(e.y,abs);  break;
    default: break;
    }
    colorChanged.emit(color());
    markDirty();
}
