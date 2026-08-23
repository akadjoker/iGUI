#include "pch.hpp"
#include "MenuWidgets.hpp"   // Menu — complete type needed for delete contextMenu_

// ═════════════════════════════════════════════════════════════════════════════
//  ColorProxy — bridges ctx.fill.* / ctx.line.* to BuGUI::DrawList
//  The bool doFill parameter mirrors the old RenderBatch API.
// ═════════════════════════════════════════════════════════════════════════════

void ColorProxy::Rectangle(float x, float y, float w, float h, bool doFill)
{
    if (!ctx) return;
    if (doFill) ctx->drawList.addRect({x, y, w, h}, current);
    else        ctx->drawList.addRectOutline({x, y, w, h}, current);
}

void ColorProxy::RoundedRectangle(float x, float y, float w, float h,
                                  float r, int seg, bool doFill)
{
    if (!ctx) return;
    if (doFill) ctx->drawList.addRoundRectFilled({x, y, w, h}, r, current, seg);
    else        ctx->drawList.addRoundRect({x, y, w, h}, r, current, 1.0f, seg);
}

void ColorProxy::Circle(float cx, float cy, float radius, bool doFill)
{
    if (!ctx) return;
    if (doFill) ctx->drawList.addCircleFilled({cx, cy}, radius, current);
    else        ctx->drawList.addCircle({cx, cy}, radius, current);
}

void ColorProxy::Triangle(float x0, float y0, float x1, float y1,
                          float x2, float y2, bool doFill)
{
    if (!ctx) return;
    if (doFill) ctx->drawList.addTriangleFilled({x0, y0}, {x1, y1}, {x2, y2}, current);
    else        ctx->drawList.addTriangle({x0, y0}, {x1, y1}, {x2, y2}, current);
}

void ColorProxy::Line2D(float x1, float y1, float x2, float y2)
{
    if (ctx) ctx->drawList.addLine({x1, y1}, {x2, y2}, current);
}

// ═════════════════════════════════════════════════════════════════════════════
//  FontProxy — bridges ctx.font.* to BuGUI::DrawList
// ═════════════════════════════════════════════════════════════════════════════

void FontProxy::SetColor(const Color& c)
{
    if (ctx) ctx->textColor_ = c;
}

void FontProxy::SetColor(u8 r, u8 g, u8 b, u8 a)
{
    if (ctx) ctx->textColor_ = {r, g, b, a};
}

float FontProxy::GetAscender() const
{
    if (!ctx || !ctx->buFont) return 12.0f * scale;
    return ctx->buFont->ascender() * scale;
}

float FontProxy::GetTextWidth(const char* text) const
{
    if (!ctx || !ctx->buFont || !text) return 0.0f;
    return ctx->drawList.calcTextSize(*ctx->buFont, text, scale).x;
}

void FontProxy::Print(const char* text, float x, float y, const void* /*clipHint*/)
{
    if (!ctx || !ctx->buFont || !text) return;
    ctx->drawList.addText(*ctx->buFont, {x, y}, ctx->textColor_, text, scale);
}

// ═════════════════════════════════════════════════════════════════════════════
//  PaintContext — wraps BuGUI::DrawList, zero backend dependencies
// ═════════════════════════════════════════════════════════════════════════════

PaintContext::PaintContext(BuGUI::DrawList& dl,
                           const BuGUI::Font* f,
                           IconAtlas* ico)
    : drawList(dl), buFont(f), icons(ico)
{
    fill.ctx = this;
    line.ctx = this;
    text.ctx = this;
    font.ctx = this;
}

// ── Font stack ────────────────────────────────────────────────────────────

void PaintContext::pushFont(const BuGUI::Font* f)
{
    fontStack_.push_back(buFont);
    buFont = f;
}

void PaintContext::popFont()
{
    if (!fontStack_.empty()) {
        buFont = fontStack_.back();
        fontStack_.pop_back();
    }
}

// ── Clip stack ────────────────────────────────────────────────────────────

void PaintContext::pushClip(const Rect& r)
{
    drawList.pushClip({r.x, r.y, r.w, r.h});
    clipStack_.push_back(r);
}

void PaintContext::popClip()
{
    drawList.popClip();
    if (!clipStack_.empty())
        clipStack_.pop_back();
}

const Rect& PaintContext::clipRect() const
{
    if (!clipStack_.empty())
    {
        cachedClip_ = clipStack_.back();
        return cachedClip_;
    }
    cachedClip_ = {0.f, 0.f, 99999.f, 99999.f};
    return cachedClip_;
}

bool PaintContext::hasClip() const
{
    return !clipStack_.empty();
}

bool PaintContext::isClipped(const Rect& r) const
{
    if (clipStack_.empty()) return false;
    const Rect& c = clipStack_.back();
    return (r.x >= c.x + c.w || r.x + r.w <= c.x ||
            r.y >= c.y + c.h || r.y + r.h <= c.y);
}

bool PaintContext::clipRectIntersect(const Rect& in, Rect& out) const
{
    const Rect& c = clipRect();
    float x0 = std::max(in.x, c.x);
    float y0 = std::max(in.y, c.y);
    float x1 = std::min(in.x + in.w, c.x + c.w);
    float y1 = std::min(in.y + in.h, c.y + c.h);
    if (x1 <= x0 || y1 <= y0) return false;
    out = {x0, y0, x1 - x0, y1 - y0};
    return true;
}

void PaintContext::fontClip(float out[4]) const
{
    const Rect& c = clipRect();
    out[0] = c.x; out[1] = c.y; out[2] = c.w; out[3] = c.h;
}

// ── Fill helpers ──────────────────────────────────────────────────────────

void PaintContext::fillRect(float x, float y, float w, float h)
{
    drawList.addRect({x, y, w, h}, fill.current);
}

void PaintContext::fillRoundedRect(float x, float y, float w, float h, float r, int seg)
{
    drawList.addRoundRectFilled({x, y, w, h}, r, fill.current, seg);
}

void PaintContext::fillCircle(float cx, float cy, float radius)
{
    drawList.addCircleFilled({cx, cy}, radius, fill.current);
}

void PaintContext::fillTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
    drawList.addTriangleFilled({x1, y1}, {x2, y2}, {x3, y3}, fill.current);
}

// ── Line helpers ──────────────────────────────────────────────────────────

void PaintContext::lineRect(float x, float y, float w, float h)
{
    drawList.addRectOutline({x, y, w, h}, line.current);
}

void PaintContext::lineRoundedRect(float x, float y, float w, float h, float r, int seg)
{
    drawList.addRoundRect({x, y, w, h}, r, line.current, 1.0f, seg);
}

void PaintContext::lineCircle(float cx, float cy, float radius)
{
    drawList.addCircle({cx, cy}, radius, line.current);
}

void PaintContext::drawLine(float x1, float y1, float x2, float y2)
{
    drawList.addLine({x1, y1}, {x2, y2}, line.current);
}

// ── Text helpers ──────────────────────────────────────────────────────────

void PaintContext::drawText(float x, float y, const char* text)
{
    if (!buFont || !text) return;
    drawList.addText(*buFont, {x, y}, textColor_, text);
}

void PaintContext::drawTextAligned(const Rect& bounds, const char* text,
                                   BuGUI::AlignX ax, BuGUI::AlignY ay)
{
    if (!buFont || !text) return;
    drawList.addTextAligned(*buFont,
                            {bounds.x, bounds.y, bounds.w, bounds.h},
                            textColor_, text, ax, ay);
}

float PaintContext::textWidth(const char* text) const
{
    if (!buFont || !text) return 0.0f;
    return drawList.calcTextSize(*buFont, text).x;
}

float PaintContext::textHeight() const
{
    return buFont ? buFont->lineHeight() : 16.0f;
}

// ── Image ─────────────────────────────────────────────────────────────────

void PaintContext::drawImage(BuGUI::TextureHandle tex, const Rect& dst,
                             BuGUI::Rect uv, Color tint)
{
    drawList.addImage(tex, {dst.x, dst.y, dst.w, dst.h}, uv, tint);
}

// ── Icons ─────────────────────────────────────────────────────────────────

void PaintContext::drawIcon(IconId id, float x, float y, float size)
{
    drawIcon(id, x, y, size, Color(255, 255, 255, 255));
}

void PaintContext::drawIcon(IconId id, float x, float y, float size, const Color& tint)
{
    if (!icons || !icons->ready() || id == IconId::None) return;

    Rect bb{x, y, size, size};
    if (isClipped(bb)) return;

    FloatRect src = icons->srcRect(id);
    int tw = icons->textureWidth();
    int th = icons->textureHeight();
    if (tw <= 0 || th <= 0) return;

    BuGUI::TextureHandle tex = icons->texture();

    BuGUI::Rect uv{
        src.x     / static_cast<float>(tw),
        src.y     / static_cast<float>(th),
        src.width / static_cast<float>(tw),
        src.height/ static_cast<float>(th)
    };
    drawList.addImage(tex, {x, y, size, size}, uv, tint);
}


// ═════════════════════════════════════════════════════════════════════════════
//  Widget — tree / geometry / paint
// ═════════════════════════════════════════════════════════════════════════════

Widget::~Widget()
{
    // Delete owned context menu (not in children_)
    delete contextMenu_;
    contextMenu_ = nullptr;

    for (Widget* c : children_)
        delete c;
    children_.clear();
}

void Widget::setContextMenu(Menu* m)
{
    if (m != contextMenu_) {
        delete contextMenu_;
        contextMenu_ = m;
    }
}

void Widget::removeChild(Widget* child)
{
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end())
    {
        WidgetApp::instance().notifyWidgetRemoved(child);
        delete *it;
        children_.erase(it);
    }
    markDirty();
}

Rect Widget::absoluteRect() const
{
    Rect r = rect_;
    const Widget* child = this;
    const Widget* p = parent_;
    while (p)
    {
        r.x += p->rect_.x;
        r.y += p->rect_.y;
        // Apply scroll offset only if this child is scrollable
        if (child->scrollable_)
        {
            auto so = p->scrollOffset();
            r.x -= so.x;
            r.y -= so.y;
        }
        child = p;
        p = p->parent_;
    }
    return r;
}

Widget* Widget::findById(const std::string& searchId)
{
    if (id_ == searchId) return this;
    for (auto* c : children_) {
        Widget* found = c->findById(searchId);
        if (found) return found;
    }
    return nullptr;
}

void Widget::findByTag(const std::string& tag, std::vector<Widget*>& result)
{
    if (hasTag(tag))
        result.push_back(this);
    for (auto* c : children_)
        c->findByTag(tag, result);
}

void Widget::layout()
{
    for (auto& c : children_)
        c->layout();
}

void Widget::paint(PaintContext& ctx)
{
    if (!visible_) return;
    if (children_.empty()) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    for (auto& c : children_)
        c->paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Widget — base event handlers (emit signals)
// ═════════════════════════════════════════════════════════════════════════════

void Widget::onMousePress(MouseEvent& e)
{
    if (!enabled_) return;
    pressed_ = true;
    pressed.emit(e.button);
    WidgetApp::instance().fireEvent("press", this);
    if (acceptsFocus_) e.consumed = true;
    markDirty();
}

void Widget::onMouseRelease(MouseEvent& e)
{
    if (!enabled_) return;
    bool wasPressed = pressed_;
    pressed_ = false;
    released.emit(e.button);
    WidgetApp::instance().fireEvent("release", this);
    if (acceptsFocus_) e.consumed = true;
    markDirty();

    // clicked = press + release with left button while still inside
    if (wasPressed && e.button == 0)
    {
        Rect abs = absoluteRect();
        if (abs.contains(e.x, e.y))
        {
            clicked.emit();
            WidgetApp::instance().fireEvent("click", this);
        }
    }
}

void Widget::onMouseMove(MouseEvent& e)
{
    moved.emit(e.localX, e.localY);
    WidgetApp::instance().fireEvent("move", this);
}

void Widget::onMouseEnter()
{
    hoverEnter.emit();
    WidgetApp::instance().fireEvent("hover", this);
    markDirty();
}

void Widget::onMouseLeave()
{
    pressed_ = false;
    hoverLeave.emit();
    WidgetApp::instance().fireEvent("leave", this);
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  BoxLayout
// ═════════════════════════════════════════════════════════════════════════════

void BoxLayout::layout()
{
    const bool vert = (dir_ == LayoutDir::Vertical);

    // Available space inside padding
    float areaX = padding_.left;
    float areaY = padding_.top;
    float areaW = rect_.w - padding_.left - padding_.right;
    float areaH = rect_.h - padding_.top  - padding_.bottom;

    // ── Pass 1: collect visible children, measure fixed + stretch ────────
    struct Item {
        Widget* w;
        Vec2f   hint;
        float   mainHint;   // size on main axis (from sizeHint)
        float   mainMargin; // margin on main axis (before + after)
        float   crossMargin;
    };

    std::vector<Item> items;
    items.reserve(children_.size());

    float totalFixed   = 0;
    float totalStretch = 0;
    int   count        = 0;

    for (auto* child : children_)
    {
        if (!child->isVisible()) continue;

        // Pre-set the cross-axis dimension so children like FlowLayout
        // can compute a width-dependent sizeHint (e.g. row wrapping).
        // We only update the single dimension without triggering markDirty.
        const Edges& m = child->margins();
        if (vert)
        {
            float cw = areaW - m.left - m.right;
            if (cw > 0 && child->rect().w != cw)
            {
                Rect r = child->rect();
                r.w = cw;
                child->setRect(r);
            }
        }
        else
        {
            float ch = areaH - m.top - m.bottom;
            if (ch > 0 && child->rect().h != ch)
            {
                Rect r = child->rect();
                r.h = ch;
                child->setRect(r);
            }
        }

        const Vec2f hint = child->sizeHint();
        float mainMargin  = vert ? m.top + m.bottom : m.left + m.right;
        float crossMargin = vert ? m.left + m.right : m.top + m.bottom;

        float mainH = vert ? hint.y : hint.x;

        items.push_back({child, hint, mainH, mainMargin, crossMargin});

        if (child->stretch() > 0)
        {
            totalStretch += child->stretch();
            totalFixed += mainMargin;   // margins still consume fixed space
        }
        else
            totalFixed += mainH + mainMargin;

        count++;
    }

    if (count == 0) return;

    // Total spacing between items
    float totalSpacing = (count > 1) ? spacing_ * (count - 1) : 0;

    // Available for main axis (after padding, spacing, fixed items)
    float mainAvail = (vert ? areaH : areaW) - totalSpacing - totalFixed;
    if (mainAvail < 0) mainAvail = 0;

    // ── Pass 2: compute main-axis sizes ──────────────────────────────────
    struct Placed { float pos; float size; };
    std::vector<Placed> mainPlaced(count);

    float usedMain = 0;
    for (int i = 0; i < count; ++i)
    {
        auto& it = items[i];
        if (it.w->stretch() > 0 && totalStretch > 0)
            mainPlaced[i].size = (it.w->stretch() / totalStretch) * mainAvail;
        else
            mainPlaced[i].size = it.mainHint;
        usedMain += mainPlaced[i].size + it.mainMargin;
    }
    usedMain += totalSpacing;

    // ── Pass 3: main-axis positioning (mainAlign) ────────────────────────
    float extraMain = (vert ? areaH : areaW) - usedMain;
    if (extraMain < 0) extraMain = 0;

    float gap     = 0;
    float startOff = 0;

    switch (mainAlign_)
    {
    case MainAlign::Start:
        break;
    case MainAlign::Center:
        startOff = extraMain * 0.5f;
        break;
    case MainAlign::End:
        startOff = extraMain;
        break;
    case MainAlign::SpaceBetween:
        if (count > 1) gap = extraMain / (count - 1);
        break;
    case MainAlign::SpaceEvenly:
        gap = extraMain / (count + 1);
        startOff = gap;
        break;
    }

    float cursor = (vert ? areaY : areaX) + startOff;

    for (int i = 0; i < count; ++i)
    {
        auto& it = items[i];
        const Edges& m = it.w->margins();
        float mainBefore = vert ? m.top  : m.left;
        float crossBefore = vert ? m.left : m.top;

        cursor += mainBefore;
        mainPlaced[i].pos = cursor;
        cursor += mainPlaced[i].size;
        cursor += (vert ? m.bottom : m.right);
        cursor += spacing_ + gap;
    }

    // ── Pass 4: cross-axis alignment + set rects ─────────────────────────
    for (int i = 0; i < count; ++i)
    {
        auto& it = items[i];
        const Edges& m = it.w->margins();
        Rect cr;

        float crossTotal = vert ? areaW : areaH;
        float crossHint  = vert ? it.hint.x : it.hint.y;
        float crossMBefore = vert ? m.left : m.top;
        float crossMAfter  = vert ? m.right : m.bottom;
        float crossAvail = crossTotal - crossMBefore - crossMAfter;

        float crossSize, crossPos;
        Align al = it.w->layoutAlign();
        switch (al)
        {
        case Align::Start:
            crossSize = std::min(crossHint, crossAvail);
            crossPos  = (vert ? areaX : areaY) + crossMBefore;
            break;
        case Align::Center:
            crossSize = std::min(crossHint, crossAvail);
            crossPos  = (vert ? areaX : areaY) + crossMBefore + (crossAvail - crossSize) * 0.5f;
            break;
        case Align::End:
            crossSize = std::min(crossHint, crossAvail);
            crossPos  = (vert ? areaX : areaY) + crossMBefore + crossAvail - crossSize;
            break;
        case Align::Fill:
        default:
            crossSize = crossAvail;
            crossPos  = (vert ? areaX : areaY) + crossMBefore;
            break;
        }

        if (vert)
        {
            cr.x = crossPos;
            cr.y = mainPlaced[i].pos;
            cr.w = crossSize;
            cr.h = mainPlaced[i].size;
        }
        else
        {
            cr.x = mainPlaced[i].pos;
            cr.y = crossPos;
            cr.w = mainPlaced[i].size;
            cr.h = crossSize;
        }

        it.w->setRect(cr);
        it.w->layout();
    }
}

Widget::Vec2f BoxLayout::sizeHint() const
{
    float mainAxis  = padding_.top + padding_.bottom;
    float crossAxis = 0;
    int count = 0;

    if (dir_ == LayoutDir::Horizontal)
        mainAxis = padding_.left + padding_.right;

    for (const auto& child : children_)
    {
        if (!child->isVisible()) continue;
        const Vec2f hint = child->sizeHint();
        const Edges& m   = child->margins();

        if (dir_ == LayoutDir::Vertical)
        {
            mainAxis  += hint.y + m.top + m.bottom;
            crossAxis  = std::max(crossAxis, hint.x + m.left + m.right);
        }
        else
        {
            mainAxis  += hint.x + m.left + m.right;
            crossAxis  = std::max(crossAxis, hint.y + m.top + m.bottom);
        }
        count++;
    }

    if (count > 1) mainAxis += spacing_ * (count - 1);

    if (dir_ == LayoutDir::Vertical)
    {
        crossAxis += padding_.left + padding_.right;
        return {crossAxis, mainAxis};
    }
    else
    {
        crossAxis += padding_.top + padding_.bottom;
        return {mainAxis, crossAxis};
    }
}
