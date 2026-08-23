#include "pch.hpp"
#include "AppWidgets.hpp"
#include "TreePropertyColorWidgets.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>

// ═════════════════════════════════════════════════════════════════════════════
//  Breadcrumbs
// ═════════════════════════════════════════════════════════════════════════════

Breadcrumbs::Breadcrumbs()
{
    acceptsFocus_ = false;
}

void Breadcrumbs::setPath(const std::vector<std::string>& segments)
{
    segments_ = segments;
    hoveredIndex_ = -1;
    markDirty();
}

Widget::Vec2f Breadcrumbs::sizeHint() const
{
    const auto& t = Theme::instance();
    float w = t.padding;
    for (size_t i = 0; i < segments_.size(); ++i) {
        w += segments_[i].size() * t.fontSize * 0.6f;
        if (i + 1 < segments_.size())
            w += t.fontSize * 1.2f; // separator space
    }
    w += t.padding;
    return {w, t.fontSize + t.padding * 2};
}

void Breadcrumbs::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    // Background
    ctx.fill.SetColor(t.panelColor);
    ctx.fill.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, true);

    float asc = ctx.font.GetAscender();
    float ty = abs.y + (abs.h + asc) * 0.5f;
    float cx = abs.x + t.padding;

    segRects_.clear();
    segRects_.resize(segments_.size());

    for (size_t i = 0; i < segments_.size(); ++i) {
        float tw = ctx.font.GetTextWidth(segments_[i].c_str());
        segRects_[i] = {cx, tw};

        bool isLast = (i + 1 == segments_.size());
        bool isHov  = (static_cast<int>(i) == hoveredIndex_);

        // Clickable segments (not last) are styled as links
        if (!isLast && isHov) {
            ctx.font.SetColor(Color(120, 170, 220, 255));
            // Underline
            ctx.line.SetColor(Color(120, 170, 220, 255));
            ctx.line.Line2D(cx, ty + 2, cx + tw, ty + 2);
        } else if (!isLast) {
            ctx.font.SetColor(Color(150, 150, 155, 255));
        } else {
            ctx.font.SetColor(t.textColor);
        }
        ctx.font.Print(segments_[i].c_str(), cx, ty);
        cx += tw;

        // Separator
        if (!isLast) {
            float sepW = ctx.font.GetTextWidth(separator_.c_str());
            float sepPad = t.fontSize * 0.3f;
            cx += sepPad;
            ctx.font.SetColor(Color(100, 100, 105, 255));
            ctx.font.Print(separator_.c_str(), cx, ty);
            cx += sepW + sepPad;
        }
    }

    Widget::paint(ctx);
}

int Breadcrumbs::hitTestSegment(float localX, float /*localY*/) const
{
    for (size_t i = 0; i + 1 < segRects_.size(); ++i) { // not last
        if (localX >= segRects_[i].x - absoluteRect().x &&
            localX <  segRects_[i].x - absoluteRect().x + segRects_[i].w)
            return static_cast<int>(i);
    }
    return -1;
}

void Breadcrumbs::onMousePress(MouseEvent& e)
{
    int idx = hitTestSegment(e.localX, e.localY);
    if (idx >= 0) {
        itemClicked.emit(idx);
        e.consumed = true;
    }
}

void Breadcrumbs::onMouseMove(MouseEvent& e)
{
    int idx = hitTestSegment(e.localX, e.localY);
    if (idx != hoveredIndex_) {
        hoveredIndex_ = idx;
        markDirty();
    }
}

void Breadcrumbs::onMouseLeave()
{
    if (hoveredIndex_ >= 0) {
        hoveredIndex_ = -1;
        markDirty();
    }
    Widget::onMouseLeave();
}

// ═════════════════════════════════════════════════════════════════════════════
//  SearchBar
// ═════════════════════════════════════════════════════════════════════════════

SearchBar::SearchBar()
{
    acceptsFocus_ = true;
    cursor_ = CursorType::IBeam;
}

float SearchBar::iconW() const    { return 24.0f; }
float SearchBar::clearBtnW() const { return text_.empty() ? 0.0f : 20.0f; }
float SearchBar::textAreaX() const { return iconW(); }
float SearchBar::textAreaW() const { return rect_.w - iconW() - clearBtnW() - Theme::instance().padding; }

void SearchBar::setText(const std::string& t)
{
    text_ = t;
    cursorPos_ = static_cast<int>(t.size());
    markDirty();
}

void SearchBar::clear()
{
    if (!text_.empty()) {
        text_.clear();
        cursorPos_ = 0;
        scrollX_ = 0;
        markDirty();
        onTextChanged.emit(text_);
    }
}

Widget::Vec2f SearchBar::sizeHint() const
{
    const auto& t = Theme::instance();
    return {200, t.inputHeight};
}

void SearchBar::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    // Background
    Color bg = isHovered() ? t.inputBgHover : t.inputBg;
    Color border = isHovered() ? t.inputBorderHover : t.inputBorder;
    if (isFocused()) border = t.focusColor;

    ctx.fill.SetColor(bg);
    ctx.fill.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, true);
    ctx.line.SetColor(border);
    ctx.line.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, false);

    float asc = ctx.font.GetAscender();
    float ty = abs.y + (abs.h + asc) * 0.5f;

    // Search icon (magnifying glass)
    float icX = abs.x + 8;
    float icY = abs.y + abs.h * 0.5f;
    ctx.line.SetColor(Color(140, 140, 145, 255));
    ctx.line.Circle(icX + 5, icY - 1, 5, false);
    ctx.line.Line2D(icX + 9, icY + 3, icX + 13, icY + 7);

    // Text area clip
    float taX = abs.x + textAreaX();
    float taW = textAreaW();
    Rect textClip = {taX, abs.y, taW, abs.h};
    ctx.pushClip(textClip);

    if (text_.empty() && !isFocused()) {
        // Placeholder
        ctx.font.SetColor(t.textDisabled);
        ctx.font.Print(placeholder_.c_str(), taX - scrollX_, ty);
    } else {
        // Text
        ctx.font.SetColor(t.textColor);
        ctx.font.Print(text_.c_str(), taX - scrollX_, ty);

        // Cursor
        if (isFocused()) {
            blinkTimer_ += WidgetApp::instance().deltaTime();
            if (std::fmod(blinkTimer_, 1.0f) < 0.5f) {
                std::string beforeCursor = text_.substr(0, cursorPos_);
                float curX = taX - scrollX_ + ctx.font.GetTextWidth(beforeCursor.c_str());
                ctx.fill.SetColor(t.textColor);
                ctx.fill.Rectangle(curX, abs.y + 3, 1.0f, abs.h - 6, true);
            }
        }
    }

    ctx.popClip();

    // Clear button (X)
    if (!text_.empty()) {
        float cbX = abs.x + abs.w - clearBtnW() - 4;
        float cbY = abs.y + abs.h * 0.5f;
        Color xCol = Color(140, 140, 145, 255);
        ctx.line.SetColor(xCol);
        ctx.line.Line2D(cbX, cbY - 4, cbX + 8, cbY + 4);
        ctx.line.Line2D(cbX + 8, cbY - 4, cbX, cbY + 4);
    }

    Widget::paint(ctx);
}

void SearchBar::onMousePress(MouseEvent& e)
{
    // Check clear button
    if (!text_.empty()) {
        float clearX = rect_.w - clearBtnW() - 4;
        if (e.localX >= clearX) {
            clear();
            e.consumed = true;
            return;
        }
    }

    // Position cursor
    // Simple approximation: use text width
    float taX = textAreaX();
    float clickX = e.localX - taX + scrollX_;
    int best = 0;
    float bestDist = 99999;
    for (int i = 0; i <= static_cast<int>(text_.size()); ++i) {
        float tw = WidgetApp::instance().textWidth(text_.substr(0, i).c_str());
        float d = std::abs(tw - clickX);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    cursorPos_ = best;
    blinkTimer_ = 0;
    markDirty();
    e.consumed = true;
}

void SearchBar::onKeyPress(KeyEvent& e)
{
    if (e.key == BuGUI::Key::Return || e.key == BuGUI::Key::KPEnter) {
        onSearch.emit(text_);
        e.consumed = true;
        return;
    }
    if (e.key == BuGUI::Key::Backspace) {
        if (cursorPos_ > 0) {
            text_.erase(cursorPos_ - 1, 1);
            cursorPos_--;
            blinkTimer_ = 0;
            markDirty();
            onTextChanged.emit(text_);
        }
        e.consumed = true;
        return;
    }
    if (e.key == BuGUI::Key::Delete) {
        if (cursorPos_ < static_cast<int>(text_.size())) {
            text_.erase(cursorPos_, 1);
            blinkTimer_ = 0;
            markDirty();
            onTextChanged.emit(text_);
        }
        e.consumed = true;
        return;
    }
    if (e.key == BuGUI::Key::Left) {
        if (cursorPos_ > 0) cursorPos_--;
        blinkTimer_ = 0;
        markDirty();
        e.consumed = true;
        return;
    }
    if (e.key == BuGUI::Key::Right) {
        if (cursorPos_ < static_cast<int>(text_.size())) cursorPos_++;
        blinkTimer_ = 0;
        markDirty();
        e.consumed = true;
        return;
    }
    if (e.key == BuGUI::Key::Home) {
        cursorPos_ = 0;
        blinkTimer_ = 0;
        markDirty();
        e.consumed = true;
        return;
    }
    if (e.key == BuGUI::Key::End) {
        cursorPos_ = static_cast<int>(text_.size());
        blinkTimer_ = 0;
        markDirty();
        e.consumed = true;
        return;
    }
    if (e.key == BuGUI::Key::Escape) {
        clear();
        e.consumed = true;
        return;
    }
    // Ctrl+A select all (just move cursor to end for now)
    if (e.ctrl && e.key == BuGUI::Key::A) {
        cursorPos_ = static_cast<int>(text_.size());
        blinkTimer_ = 0;
        markDirty();
        e.consumed = true;
        return;
    }
}

void SearchBar::onTextInput(KeyEvent& e)
{
    if (e.text[0] == '\0') return;
    std::string inp(e.text);
    text_.insert(cursorPos_, inp);
    cursorPos_ += static_cast<int>(inp.size());
    blinkTimer_ = 0;
    markDirty();
    onTextChanged.emit(text_);
    e.consumed = true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  SplitButton
// ═════════════════════════════════════════════════════════════════════════════

SplitButton::SplitButton(const std::string& text) : text_(text)
{
    dropMenu_ = new Menu();
}

SplitButton::~SplitButton()
{
    delete dropMenu_;
}

void SplitButton::addAction(const std::string& text, std::function<void()> cb)
{
    auto* act = dropMenu_->addAction(text);
    if (cb) act->triggered.connect(std::move(cb));
}

void SplitButton::addSeparator()
{
    dropMenu_->addSeparator();
}

float SplitButton::dropArrowW() const { return 22.0f; }

Widget::Vec2f SplitButton::sizeHint() const
{
    const auto& t = Theme::instance();
    float tw = text_.size() * t.fontSize * 0.6f;
    return {tw + t.padding * 3 + dropArrowW(), t.buttonHeight};
}

void SplitButton::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    float arrowW = dropArrowW();
    float mainW = abs.w - arrowW;

    // Main button
    Color mainBg = hoverMain_ ? t.buttonHover : t.buttonNormal;
    ctx.fill.SetColor(mainBg);
    ctx.fill.RoundedRectangle(abs.x, abs.y, mainW, abs.h, t.borderRadius, 6, true);

    // Arrow button
    Color dropBg = hoverDrop_ ? t.buttonHover : t.buttonNormal;
    ctx.fill.SetColor(dropBg);
    ctx.fill.RoundedRectangle(abs.x + mainW + 1, abs.y, arrowW - 1, abs.h, t.borderRadius, 6, true);

    // Divider line
    ctx.line.SetColor(t.borderColor);
    ctx.line.Line2D(abs.x + mainW, abs.y + 4, abs.x + mainW, abs.y + abs.h - 4);

    // Border
    ctx.line.SetColor(t.buttonBorder);
    ctx.line.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, false);

    // Text
    float asc = ctx.font.GetAscender();
    float ty = abs.y + (abs.h + asc) * 0.5f;
    float tw = ctx.font.GetTextWidth(text_.c_str());
    float tx = abs.x + (mainW - tw) * 0.5f;
    ctx.font.SetColor(t.textColor);
    ctx.font.Print(text_.c_str(), tx, ty);

    // Down arrow
    float ax = abs.x + mainW + arrowW * 0.5f;
    float ay = abs.y + abs.h * 0.5f;
    ctx.fill.SetColor(t.textColor);
    ctx.fill.Triangle(ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3, true);

    Widget::paint(ctx);
}

void SplitButton::onMousePress(MouseEvent& e)
{
    Rect abs = absoluteRect();
    float arrowW = dropArrowW();
    float mainW = abs.w - arrowW;

    if (e.localX >= mainW) {
        // Dropdown clicked
        dropMenu_->exec(abs.x, abs.y + abs.h);
    } else {
        // Main button clicked
        clicked.emit();
    }
    e.consumed = true;
}

void SplitButton::onMouseMove(MouseEvent& e)
{
    float arrowW = dropArrowW();
    float mainW = rect_.w - arrowW;
    bool hm = (e.localX < mainW);
    bool hd = (e.localX >= mainW);
    if (hm != hoverMain_ || hd != hoverDrop_) {
        hoverMain_ = hm;
        hoverDrop_ = hd;
        markDirty();
    }
}

void SplitButton::onMouseLeave()
{
    if (hoverMain_ || hoverDrop_) {
        hoverMain_ = false;
        hoverDrop_ = false;
        markDirty();
    }
    Widget::onMouseLeave();
}

// ═════════════════════════════════════════════════════════════════════════════
//  ContextMenuBuilder
// ═════════════════════════════════════════════════════════════════════════════

Menu* ContextMenuBuilder::build(Widget* target, const std::vector<ContextMenuItem>& items)
{
    // Clean up previous context menu if one exists
    if (target)
        target->setContextMenu(nullptr);  // setter deletes old menu

    auto* menu = new Menu();
    for (const auto& item : items) {
        if (item.text.empty() || item.text == "-") {
            menu->addSeparator();
            continue;
        }
        auto* act = menu->addAction(item.text);
        if (!item.shortcut.empty())
            act->setShortcut(item.shortcut);
        act->setEnabled(item.enabled);
        if (item.checkable) {
            act->setCheckable(true);
            act->setChecked(item.checked);
        }
        if (item.callback) {
            auto cb = item.callback;
            act->triggered.connect([cb]() { cb(); });
        }
    }
    if (target)
        target->setContextMenu(menu);
    return menu;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Outliner
// ═════════════════════════════════════════════════════════════════════════════

Outliner::Outliner()
{
    acceptsFocus_ = true;
}

Outliner::~Outliner()
{
    for (auto* r : roots_) delete r;
}

TreeNode* Outliner::addRoot(const std::string& text)
{
    auto* n = new TreeNode(text);
    roots_.push_back(n);
    flatDirty_ = true;
    markDirty();
    return n;
}

void Outliner::clear()
{
    for (auto* r : roots_) delete r;
    roots_.clear();
    flatRows_.clear();
    selected_ = nullptr;
    flatDirty_ = true;
    markDirty();
}

void Outliner::setNodeVisible(TreeNode* node, bool vis) { visMap_[node] = vis; markDirty(); }
bool Outliner::isNodeVisible(TreeNode* node) const {
    auto it = visMap_.find(node);
    return it == visMap_.end() ? true : it->second;
}

void Outliner::setNodeLocked(TreeNode* node, bool locked) { lockMap_[node] = locked; markDirty(); }
bool Outliner::isNodeLocked(TreeNode* node) const {
    auto it = lockMap_.find(node);
    return it == lockMap_.end() ? false : it->second;
}

TreeNode* Outliner::selectedNode() const { return selected_; }

void Outliner::rebuildFlat()
{
    flatRows_.clear();
    for (auto* r : roots_) flattenNode(r, 0);
    flatDirty_ = false;
}

void Outliner::flattenNode(TreeNode* n, int depth)
{
    flatRows_.push_back({n, depth});
    if (n->isExpanded()) {
        for (auto* c : n->children())
            flattenNode(c, depth + 1);
    }
}

float Outliner::maxScroll() const
{
    float totalH = flatRows_.size() * rowHeight_;
    return std::max(0.0f, totalH - rect_.h);
}

int Outliner::hitTestRow(float localY) const
{
    int row = static_cast<int>((localY + scrollOffset_) / rowHeight_);
    if (row < 0 || row >= static_cast<int>(flatRows_.size())) return -1;
    return row;
}

float Outliner::eyeColX() const    { return rect_.w - 40; }
float Outliner::lockColX() const   { return rect_.w - 20; }
float Outliner::textColX(int depth) const { return 4 + indent_ * depth + 16; }

void Outliner::layout()
{
    Widget::layout();
}

void Outliner::paint(PaintContext& ctx)
{
    if (!visible_) return;
    if (flatDirty_) rebuildFlat();

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    // Background
    ctx.fill.SetColor(t.panelColor);
    ctx.fill.Rectangle(abs.x, abs.y, abs.w, abs.h, true);

    ctx.pushClip(abs);

    float asc = ctx.font.GetAscender();
    int firstRow = static_cast<int>(scrollOffset_ / rowHeight_);
    int visRows  = static_cast<int>(abs.h / rowHeight_) + 2;

    for (int i = firstRow; i < firstRow + visRows && i < static_cast<int>(flatRows_.size()); ++i) {
        auto& fr = flatRows_[i];
        float ry = abs.y + i * rowHeight_ - scrollOffset_;

        // Selection highlight
        if (fr.node == selected_) {
            ctx.fill.SetColor(t.selectionColor);
            ctx.fill.Rectangle(abs.x, ry, abs.w, rowHeight_, true);
        }
        // Hover highlight
        if (i == hoveredRow_ && fr.node != selected_) {
            ctx.fill.SetColor(Color(255, 255, 255, 15));
            ctx.fill.Rectangle(abs.x, ry, abs.w, rowHeight_, true);
        }

        float tx = abs.x + textColX(fr.depth);
        float tty = ry + (rowHeight_ + asc) * 0.5f;

        // Expand triangle
        if (fr.node->hasChildren()) {
            float triX = abs.x + 4 + indent_ * fr.depth;
            float triY = ry + rowHeight_ * 0.5f;
            ctx.fill.SetColor(Color(180, 180, 185, 255));
            if (fr.node->isExpanded()) {
                ctx.fill.Triangle(triX, triY - 3, triX + 7, triY - 3, triX + 3.5f, triY + 3, true);
            } else {
                ctx.fill.Triangle(triX, triY - 4, triX + 6, triY, triX, triY + 4, true);
            }
        }

        // Icon (if any)
        if (fr.node->iconId() != IconId::None && ctx.icons) {
            ctx.drawIcon(fr.node->iconId(), tx - 16, ry + 3, 16);
        }

        // Node text
        bool vis = isNodeVisible(fr.node);
        ctx.font.SetColor(vis ? t.textColor : t.textDisabled);
        ctx.font.Print(fr.node->text().c_str(), tx, tty);

        // Eye icon (visibility toggle)
        float eyeX = abs.x + eyeColX();
        Color eyeCol = vis ? Color(180, 180, 185, 255) : Color(80, 80, 85, 255);
        ctx.fill.SetColor(eyeCol);
        // Simple eye: circle + dot
        ctx.line.SetColor(eyeCol);
        ctx.line.Circle(eyeX + 8, ry + rowHeight_ * 0.5f, 5, false);
        if (vis)
            ctx.fill.Circle(eyeX + 8, ry + rowHeight_ * 0.5f, 2, true);

        // Lock icon
        bool locked = isNodeLocked(fr.node);
        float lockX = abs.x + lockColX();
        Color lockCol = locked ? Color(200, 150, 50, 255) : Color(100, 100, 105, 255);
        ctx.line.SetColor(lockCol);
        // Simple lock: rect + arc
        ctx.line.Rectangle(lockX + 3, ry + rowHeight_ * 0.5f, 10, 8, false);
        if (locked)
            ctx.line.Circle(lockX + 8, ry + rowHeight_ * 0.5f - 2, 4, false);
    }

    ctx.popClip();

    Widget::paint(ctx);
}

void Outliner::onMousePress(MouseEvent& e)
{
    if (flatDirty_) rebuildFlat();

    int row = hitTestRow(e.localY);
    if (row < 0 || row >= static_cast<int>(flatRows_.size())) {
        e.consumed = true;
        return;
    }

    auto& fr = flatRows_[row];
    Rect abs = absoluteRect();

    // Eye toggle
    float eyeX = eyeColX();
    if (e.localX >= eyeX && e.localX < eyeX + 16) {
        bool vis = !isNodeVisible(fr.node);
        setNodeVisible(fr.node, vis);
        visibilityChanged.emit(fr.node, vis);
        e.consumed = true;
        return;
    }

    // Lock toggle
    float lckX = lockColX();
    if (e.localX >= lckX && e.localX < lckX + 16) {
        bool lock = !isNodeLocked(fr.node);
        setNodeLocked(fr.node, lock);
        lockChanged.emit(fr.node, lock);
        e.consumed = true;
        return;
    }

    // Triangle expand/collapse
    float triX = 4 + indent_ * fr.depth;
    if (e.localX >= triX && e.localX < triX + 16 && fr.node->hasChildren()) {
        fr.node->setExpanded(!fr.node->isExpanded());
        flatDirty_ = true;
        markDirty();
        e.consumed = true;
        return;
    }

    // Select
    if (selected_ != fr.node) {
        selected_ = fr.node;
        selectionChanged.emit(selected_);
        markDirty();
    }
    e.consumed = true;
}

void Outliner::onMouseScroll(MouseEvent& e)
{
    scrollOffset_ -= e.scrollY * rowHeight_ * 3;
    scrollOffset_ = std::max(0.0f, std::min(scrollOffset_, maxScroll()));
    markDirty();
    e.consumed = true;
}

void Outliner::onMouseMove(MouseEvent& e)
{
    int row = hitTestRow(e.localY);
    if (row != hoveredRow_) {
        hoveredRow_ = row;
        markDirty();
    }
}

void Outliner::onMouseLeave()
{
    if (hoveredRow_ >= 0) {
        hoveredRow_ = -1;
        markDirty();
    }
    Widget::onMouseLeave();
}

// ═════════════════════════════════════════════════════════════════════════════
//  RichText
// ═════════════════════════════════════════════════════════════════════════════

RichText::RichText()
{
    acceptsFocus_ = false;
}

void RichText::setMarkdown(const std::string& md)
{
    rawMd_ = md;
    scrollY_ = 0;
    parse();
    markDirty();
}

// Simple markdown parser
void RichText::parse()
{
    blocks_.clear();
    if (rawMd_.empty()) return;

    // Split into lines
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos < rawMd_.size()) {
        size_t nl = rawMd_.find('\n', pos);
        if (nl == std::string::npos) {
            lines.push_back(rawMd_.substr(pos));
            break;
        }
        lines.push_back(rawMd_.substr(pos, nl - pos));
        pos = nl + 1;
    }

    bool inCodeBlock = false;
    std::string codeAccum;

    for (const auto& line : lines) {
        // Code blocks (```)
        if (line.size() >= 3 && line.substr(0, 3) == "```") {
            if (inCodeBlock) {
                Block b;
                b.type = BlockType::CodeBlock;
                b.spans.push_back({codeAccum, SpanStyle::Code, ""});
                blocks_.push_back(std::move(b));
                codeAccum.clear();
                inCodeBlock = false;
            } else {
                inCodeBlock = true;
            }
            continue;
        }
        if (inCodeBlock) {
            if (!codeAccum.empty()) codeAccum += '\n';
            codeAccum += line;
            continue;
        }

        // Empty line → skip (paragraph separator)
        if (line.empty()) continue;

        Block b;

        // Headings
        if (line.size() >= 4 && line.substr(0, 4) == "### ") {
            b.type = BlockType::Heading3;
            b.spans.push_back({line.substr(4), SpanStyle::Bold, ""});
            blocks_.push_back(std::move(b));
            continue;
        }
        if (line.size() >= 3 && line.substr(0, 3) == "## ") {
            b.type = BlockType::Heading2;
            b.spans.push_back({line.substr(3), SpanStyle::Bold, ""});
            blocks_.push_back(std::move(b));
            continue;
        }
        if (line.size() >= 2 && line.substr(0, 2) == "# ") {
            b.type = BlockType::Heading1;
            b.spans.push_back({line.substr(2), SpanStyle::Bold, ""});
            blocks_.push_back(std::move(b));
            continue;
        }

        // List items
        if (line.size() >= 2 && (line.substr(0, 2) == "- " || line.substr(0, 2) == "* ")) {
            b.type = BlockType::ListItem;
            // Parse inline spans from the rest
            std::string content = line.substr(2);
            // Simple inline parsing
            size_t i = 0;
            std::string accum;
            while (i < content.size()) {
                // Bold **...**
                if (i + 1 < content.size() && content[i] == '*' && content[i+1] == '*') {
                    if (!accum.empty()) { b.spans.push_back({accum, SpanStyle::Normal, ""}); accum.clear(); }
                    i += 2;
                    size_t end = content.find("**", i);
                    if (end != std::string::npos) {
                        b.spans.push_back({content.substr(i, end - i), SpanStyle::Bold, ""});
                        i = end + 2;
                    }
                    continue;
                }
                // Italic *...*
                if (content[i] == '*') {
                    if (!accum.empty()) { b.spans.push_back({accum, SpanStyle::Normal, ""}); accum.clear(); }
                    i++;
                    size_t end = content.find('*', i);
                    if (end != std::string::npos) {
                        b.spans.push_back({content.substr(i, end - i), SpanStyle::Italic, ""});
                        i = end + 1;
                    }
                    continue;
                }
                // Code `...`
                if (content[i] == '`') {
                    if (!accum.empty()) { b.spans.push_back({accum, SpanStyle::Normal, ""}); accum.clear(); }
                    i++;
                    size_t end = content.find('`', i);
                    if (end != std::string::npos) {
                        b.spans.push_back({content.substr(i, end - i), SpanStyle::Code, ""});
                        i = end + 1;
                    }
                    continue;
                }
                // Link [text](url)
                if (content[i] == '[') {
                    size_t rb = content.find(']', i);
                    if (rb != std::string::npos && rb + 1 < content.size() && content[rb+1] == '(') {
                        size_t rp = content.find(')', rb + 2);
                        if (rp != std::string::npos) {
                            if (!accum.empty()) { b.spans.push_back({accum, SpanStyle::Normal, ""}); accum.clear(); }
                            std::string text = content.substr(i + 1, rb - i - 1);
                            std::string url  = content.substr(rb + 2, rp - rb - 2);
                            b.spans.push_back({text, SpanStyle::Link, url});
                            i = rp + 1;
                            continue;
                        }
                    }
                }
                accum += content[i++];
            }
            if (!accum.empty()) b.spans.push_back({accum, SpanStyle::Normal, ""});
            blocks_.push_back(std::move(b));
            continue;
        }

        // Paragraph with inline parsing
        b.type = BlockType::Paragraph;
        size_t i = 0;
        std::string accum;
        while (i < line.size()) {
            if (i + 1 < line.size() && line[i] == '*' && line[i+1] == '*') {
                if (!accum.empty()) { b.spans.push_back({accum, SpanStyle::Normal, ""}); accum.clear(); }
                i += 2;
                size_t end = line.find("**", i);
                if (end != std::string::npos) {
                    b.spans.push_back({line.substr(i, end - i), SpanStyle::Bold, ""});
                    i = end + 2;
                }
                continue;
            }
            if (line[i] == '*') {
                if (!accum.empty()) { b.spans.push_back({accum, SpanStyle::Normal, ""}); accum.clear(); }
                i++;
                size_t end = line.find('*', i);
                if (end != std::string::npos) {
                    b.spans.push_back({line.substr(i, end - i), SpanStyle::Italic, ""});
                    i = end + 1;
                }
                continue;
            }
            if (line[i] == '`') {
                if (!accum.empty()) { b.spans.push_back({accum, SpanStyle::Normal, ""}); accum.clear(); }
                i++;
                size_t end = line.find('`', i);
                if (end != std::string::npos) {
                    b.spans.push_back({line.substr(i, end - i), SpanStyle::Code, ""});
                    i = end + 1;
                }
                continue;
            }
            if (line[i] == '[') {
                size_t rb = line.find(']', i);
                if (rb != std::string::npos && rb + 1 < line.size() && line[rb+1] == '(') {
                    size_t rp = line.find(')', rb + 2);
                    if (rp != std::string::npos) {
                        if (!accum.empty()) { b.spans.push_back({accum, SpanStyle::Normal, ""}); accum.clear(); }
                        b.spans.push_back({line.substr(i+1, rb-i-1), SpanStyle::Link, line.substr(rb+2, rp-rb-2)});
                        i = rp + 1;
                        continue;
                    }
                }
            }
            accum += line[i++];
        }
        if (!accum.empty()) b.spans.push_back({accum, SpanStyle::Normal, ""});
        blocks_.push_back(std::move(b));
    }
}

Widget::Vec2f RichText::sizeHint() const
{
    const auto& t = Theme::instance();
    float h = t.padding * 2;
    for (const auto& b : blocks_) {
        switch (b.type) {
            case BlockType::Heading1:  h += t.fontSize * 2.0f + 8; break;
            case BlockType::Heading2:  h += t.fontSize * 1.6f + 6; break;
            case BlockType::Heading3:  h += t.fontSize * 1.3f + 4; break;
            case BlockType::CodeBlock: h += t.fontSize * 1.2f * 3 + 12; break;
            default:                   h += t.fontSize + t.lineSpacing + 4; break;
        }
    }
    return {300, h};
}

void RichText::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    // Background
    ctx.fill.SetColor(t.panelColor);
    ctx.fill.Rectangle(abs.x, abs.y, abs.w, abs.h, true);

    ctx.pushClip(abs);

    linkRects_.clear();

    float cx = abs.x + t.padding;
    float cy = abs.y + t.padding - scrollY_;
    float maxW = abs.w - t.padding * 2;

    for (const auto& block : blocks_) {
        float lineH = t.fontSize + t.lineSpacing;
        float scale = 1.0f;
        float blockGap = 4.0f;

        switch (block.type) {
            case BlockType::Heading1: scale = 1.8f; lineH = t.fontSize * scale + 8; blockGap = 8; break;
            case BlockType::Heading2: scale = 1.4f; lineH = t.fontSize * scale + 6; blockGap = 6; break;
            case BlockType::Heading3: scale = 1.15f; lineH = t.fontSize * scale + 4; blockGap = 4; break;
            case BlockType::ListItem: lineH = t.fontSize + 4; break;
            case BlockType::CodeBlock: lineH = t.fontSize * 1.1f; break;
            default: break;
        }

        float asc = ctx.font.GetAscender() * scale;
        float ty = cy + asc;

        // Code block background
        if (block.type == BlockType::CodeBlock) {
            // Count lines in code
            int codeLines = 1;
            for (auto& sp : block.spans)
                for (char c : sp.text) if (c == '\n') codeLines++;
            float codeH = codeLines * lineH + 8;
            ctx.fill.SetColor(Color(25, 25, 28, 255));
            ctx.fill.RoundedRectangle(cx - 4, cy, maxW + 8, codeH, 3, 4, true);

            // Render code line by line
            ctx.font.SetColor(Color(180, 210, 180, 255));
            float codeTy = cy + ctx.font.GetAscender() + 4;
            size_t pos = 0;
            for (auto& sp : block.spans) {
                while (pos < sp.text.size()) {
                    size_t nl = sp.text.find('\n', pos);
                    std::string ln = (nl == std::string::npos) ? sp.text.substr(pos) : sp.text.substr(pos, nl - pos);
                    ctx.font.Print(ln.c_str(), cx + 4, codeTy);
                    codeTy += lineH;
                    pos = (nl == std::string::npos) ? sp.text.size() : nl + 1;
                }
            }
            cy += codeLines * lineH + 12;
            continue;
        }

        // List bullet
        float spanX = cx;
        if (block.type == BlockType::ListItem) {
            ctx.fill.SetColor(t.textColor);
            ctx.fill.Circle(cx + 3, cy + lineH * 0.5f, 2, true);
            spanX += 14;
        }

        // Heading underline
        if (block.type == BlockType::Heading1 || block.type == BlockType::Heading2) {
            ctx.line.SetColor(t.borderColor);
            ctx.line.Line2D(cx, cy + lineH - 2, cx + maxW, cy + lineH - 2);
        }

        // Render spans
        for (const auto& span : block.spans) {
            Color col = t.textColor;
            switch (span.style) {
                case SpanStyle::Bold:
                    col = Color(255, 255, 255, 255);
                    break;
                case SpanStyle::Italic:
                    col = Color(200, 200, 205, 255);
                    break;
                case SpanStyle::Code:
                    col = Color(180, 210, 180, 255);
                    // Code bg
                    {
                        float cw = ctx.font.GetTextWidth(span.text.c_str()) * scale;
                        ctx.fill.SetColor(Color(25, 25, 28, 255));
                        ctx.fill.RoundedRectangle(spanX - 2, cy + 1, cw + 4, lineH - 2, 2, 4, true);
                    }
                    break;
                case SpanStyle::Link:
                    col = Color(100, 170, 230, 255);
                    {
                        float lw = ctx.font.GetTextWidth(span.text.c_str()) * scale;
                        linkRects_.push_back({spanX, cy, lw, lineH, span.url});
                        // Underline
                        ctx.line.SetColor(col);
                        ctx.line.Line2D(spanX, ty + 2, spanX + lw, ty + 2);
                    }
                    break;
                default:
                    break;
            }
            ctx.font.SetColor(col);
            ctx.font.scale = scale;
            ctx.font.Print(span.text.c_str(), spanX, ty);
            spanX += ctx.font.GetTextWidth(span.text.c_str());
            ctx.font.scale = 1.0f;
        }

        cy += lineH + blockGap;
    }

    ctx.popClip();

    Widget::paint(ctx);
}

void RichText::onMousePress(MouseEvent& e)
{
    Rect abs = absoluteRect();
    float mx = e.x, my = e.y;
    for (const auto& lr : linkRects_) {
        if (mx >= lr.x && mx < lr.x + lr.w && my >= lr.y && my < lr.y + lr.h) {
            linkClicked.emit(lr.url);
            e.consumed = true;
            return;
        }
    }
}

void RichText::onMouseMove(MouseEvent& e)
{
    Rect abs = absoluteRect();
    float mx = e.x, my = e.y;
    int prev = hoveredLink_;
    hoveredLink_ = -1;
    for (int i = 0; i < static_cast<int>(linkRects_.size()); ++i) {
        auto& lr = linkRects_[i];
        if (mx >= lr.x && mx < lr.x + lr.w && my >= lr.y && my < lr.y + lr.h) {
            hoveredLink_ = i;
            break;
        }
    }
    if (hoveredLink_ != prev) markDirty();
}

void RichText::onMouseLeave()
{
    if (hoveredLink_ >= 0) { hoveredLink_ = -1; markDirty(); }
    Widget::onMouseLeave();
}

void RichText::onMouseScroll(MouseEvent& e)
{
    float totalH = sizeHint().y;
    float maxS = std::max(0.0f, totalH - rect_.h);
    scrollY_ -= e.scrollY * 30;
    scrollY_ = std::max(0.0f, std::min(scrollY_, maxS));
    markDirty();
    e.consumed = true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Viewport3D
// ═════════════════════════════════════════════════════════════════════════════

Viewport3D::Viewport3D()
{
    acceptsFocus_ = true;
}

void Viewport3D::setTexture(BuGUI::TextureHandle tex, int w, int h)
{
    tex_ = tex;
    texW_ = w;
    texH_ = h;
    markDirty();
}

void Viewport3D::setOrbit(float y, float p)
{
    yaw_ = y;
    pitch_ = p;
    markDirty();
}

Widget::Vec2f Viewport3D::sizeHint() const
{
    return {400, 300};
}

void Viewport3D::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    // Detect resize
    int w = static_cast<int>(abs.w);
    int h = static_cast<int>(abs.h);
    if (w != lastW_ || h != lastH_) {
        lastW_ = w;
        lastH_ = h;
        onResize.emit(w, h);
    }

    if (tex_) {
        // Draw the FBO texture filling the widget area
        ctx.drawImage(tex_, abs);
    } else {
        // No texture → dark background with grid hint
        ctx.fill.SetColor(Color(20, 20, 22, 255));
        ctx.fill.Rectangle(abs.x, abs.y, abs.w, abs.h, true);

        // Grid pattern
        ctx.line.SetColor(Color(40, 40, 44, 255));
        float step = 32.0f;
        for (float gx = abs.x; gx < abs.x + abs.w; gx += step)
            ctx.line.Line2D(gx, abs.y, gx, abs.y + abs.h);
        for (float gy = abs.y; gy < abs.y + abs.h; gy += step)
            ctx.line.Line2D(abs.x, gy, abs.x + abs.w, gy);

        // Center label
        const char* msg = "No render target";
        float tw = ctx.font.GetTextWidth(msg);
        float asc = ctx.font.GetAscender();
        ctx.font.SetColor(Color(100, 100, 105, 255));
        ctx.font.Print(msg, abs.x + (abs.w - tw) * 0.5f, abs.y + (abs.h + asc) * 0.5f);
    }

    // Border
    ctx.line.SetColor(Color(60, 60, 65, 255));
    ctx.line.Rectangle(abs.x, abs.y, abs.w, abs.h, false);

    Widget::paint(ctx);
}

void Viewport3D::onMousePress(MouseEvent& e)
{
    if (e.button == 1 || (e.button == 0 && e.alt)) {
        // Middle button or Alt+Left → orbit
        dragMode_ = DragMode::Orbit;
        dragStartX_ = e.x;
        dragStartY_ = e.y;
        dragYaw_ = yaw_;
        dragPitch_ = pitch_;
        e.consumed = true;
    } else if (e.button == 2 || (e.button == 0 && e.shift)) {
        // Right button or Shift+Left → pan
        dragMode_ = DragMode::Pan;
        dragStartX_ = e.x;
        dragStartY_ = e.y;
        e.consumed = true;
    }
}

void Viewport3D::onMouseRelease(MouseEvent& e)
{
    if (dragMode_ != DragMode::None) {
        dragMode_ = DragMode::None;
        e.consumed = true;
    }
}

void Viewport3D::onMouseMove(MouseEvent& e)
{
    if (dragMode_ == DragMode::Orbit) {
        float dx = e.x - dragStartX_;
        float dy = e.y - dragStartY_;
        yaw_   = dragYaw_   + dx * orbitSpeed_;
        pitch_ = dragPitch_ + dy * orbitSpeed_;
        pitch_ = std::max(-89.0f, std::min(89.0f, pitch_));
        onOrbit.emit(yaw_, pitch_);
        markDirty();
        e.consumed = true;
    } else if (dragMode_ == DragMode::Pan) {
        float dx = e.x - dragStartX_;
        float dy = e.y - dragStartY_;
        dragStartX_ = e.x;
        dragStartY_ = e.y;
        onPan.emit(dx * panSpeed_, dy * panSpeed_);
        markDirty();
        e.consumed = true;
    }
}

void Viewport3D::onMouseScroll(MouseEvent& e)
{
    if (e.scrollY != 0.0f) {
        onZoom.emit(e.scrollY * zoomSpeed_);
        markDirty();
        e.consumed = true;
    }
}
