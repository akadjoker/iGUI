#include "pch.hpp"
#include "ViewWidgets.hpp"
#include "WidgetApp.hpp"

// ═════════════════════════════════════════════════════════════════════════════
//  FloatWindow
// ═════════════════════════════════════════════════════════════════════════════

FloatWindow::FloatWindow(const std::string& title)
    : title_(title)
{
}

FloatWindow::~FloatWindow()
{
    // content_ is owned by children_ — Widget::~Widget() handles deletion
}

void FloatWindow::setContentWidget(Widget* w)
{
    if (content_)
        removeChild(content_);
    content_ = w;
    if (content_) addChild(content_);
    markDirty();
}

void FloatWindow::setMinimized(bool m)
{
    if (minimized_ != m)
    {
        minimized_ = m;
        minimizedChanged.emit(m);
        markDirty();
        WidgetApp::instance().requestLayout();
    }
}

// ── Geometry helpers ─────────────────────────────────────────────────────────

Rect FloatWindow::titleBarRect() const
{
    return {floatX_, floatY_, floatW_, titleBarH_};
}

Rect FloatWindow::contentRect() const
{
    return {floatX_, floatY_ + titleBarH_, floatW_, floatH_ - titleBarH_};
}

Rect FloatWindow::closeButtonRect() const
{
    float bw = titleBarH_;
    return {floatX_ + floatW_ - bw, floatY_, bw, titleBarH_};
}

Rect FloatWindow::minimizeButtonRect() const
{
    float bw = titleBarH_;
    float off = closable_ ? bw : 0.0f;
    return {floatX_ + floatW_ - off - bw, floatY_, bw, titleBarH_};
}

Rect FloatWindow::resizeGripRect() const
{
    return {floatX_ + floatW_ - kResizeGrip,
            floatY_ + floatH_ - kResizeGrip,
            kResizeGrip, kResizeGrip};
}

Widget::Vec2f FloatWindow::sizeHint() const
{
    return {floatW_, floatH_};
}

// ── Layout ───────────────────────────────────────────────────────────────────

void FloatWindow::layout()
{
    float fh = minimized_ ? titleBarH_ : floatH_;
    setRect({floatX_, floatY_, floatW_, fh});

    if (content_ && !minimized_)
    {
        const float pad = 2.0f;
        content_->setRect({pad, titleBarH_ + pad,
                           floatW_ - pad * 2.0f,
                           floatH_ - titleBarH_ - pad * 2.0f});
        content_->layout();
    }
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void FloatWindow::paint(PaintContext& ctx)
{
    if (!visible_) return;

    const auto& t = Theme::instance();
    float fh = minimized_ ? titleBarH_ : floatH_;

    // ── Window background + rounded border ──────────────────────────────
    ctx.fill.SetColor(t.floatBg.r, t.floatBg.g, t.floatBg.b, t.floatBg.a);
    ctx.fillRoundedRect(floatX_, floatY_, floatW_, fh, t.borderRadius, 4);

    ctx.line.SetColor(t.floatBorder.r, t.floatBorder.g, t.floatBorder.b, t.floatBorder.a);
    ctx.lineRoundedRect(floatX_, floatY_, floatW_, fh, t.borderRadius, 4);

    // ── Title bar background ─────────────────────────────────────────────
    ctx.fill.SetColor(t.floatTitleBg.r, t.floatTitleBg.g, t.floatTitleBg.b, t.floatTitleBg.a);
    ctx.fillRoundedRect(floatX_, floatY_, floatW_, titleBarH_, t.borderRadius, 4);

    // ── Title text ───────────────────────────────────────────────────────
    float fsz = t.fontSize * 0.9f;
    ctx.font.SetFontSize(fsz);
    ctx.font.SetColor(t.floatTitleText);
    ctx.font.SetBatch(&ctx.text);
    float textY = floatY_ + (titleBarH_ - fsz) * 0.5f + ctx.font.GetAscender();
    ctx.font.Print(title_.c_str(), floatX_ + 8.0f, textY);

    // ── Close button (×) ────────────────────────────────────────────────
    if (closable_)
    {
        Rect cb = closeButtonRect();
        if (hoveredBtn_ == 1)
        {
            ctx.fill.SetColor(t.floatCloseHover.r, t.floatCloseHover.g,
                              t.floatCloseHover.b, t.floatCloseHover.a);
            ctx.fillRect(cb.x, cb.y, cb.w, cb.h);
        }
        float cx = cb.x + cb.w * 0.5f, cy = cb.y + cb.h * 0.5f;
        float s = 5.0f;
        ctx.line.SetColor(t.floatTitleText.r, t.floatTitleText.g,
                          t.floatTitleText.b, t.floatTitleText.a);
        ctx.drawLine(cx - s, cy - s, cx + s, cy + s);
        ctx.drawLine(cx + s, cy - s, cx - s, cy + s);
    }

    // ── Minimize button (– / □) ──────────────────────────────────────────
    if (minimizable_)
    {
        Rect mb = minimizeButtonRect();
        if (hoveredBtn_ == 2)
        {
            ctx.fill.SetColor(t.floatBtnHover.r, t.floatBtnHover.g,
                              t.floatBtnHover.b, t.floatBtnHover.a);
            ctx.fillRect(mb.x, mb.y, mb.w, mb.h);
        }
        float mx = mb.x + mb.w * 0.5f, my = mb.y + mb.h * 0.5f;
        float s = 5.0f;
        ctx.line.SetColor(t.floatTitleText.r, t.floatTitleText.g,
                          t.floatTitleText.b, t.floatTitleText.a);
        if (minimized_)
        {
            // □ restore icon
            ctx.lineRect(mx - s, my - s, s * 2.0f, s * 2.0f);
        }
        else
        {
            // – minimize icon
            ctx.drawLine(mx - s, my, mx + s, my);
        }
    }

    // ── Content ──────────────────────────────────────────────────────────
    if (content_ && !minimized_)
    {
        Rect cr = contentRect();
        ctx.pushClip(cr);
        content_->paint(ctx);
        ctx.popClip();
    }

    // ── Resize grip (bottom-right, 3 diagonal lines) ─────────────────────
    if (resizable_ && !minimized_)
    {
        float gx = floatX_ + floatW_ - 2.0f;
        float gy = floatY_ + floatH_ - 2.0f;
        ctx.line.SetColor(t.floatBorder.r, t.floatBorder.g,
                          t.floatBorder.b, static_cast<uint8_t>(t.floatBorder.a * 2 / 3));
        ctx.drawLine(gx - 10.0f, gy,       gx,       gy - 10.0f);
        ctx.drawLine(gx -  6.0f, gy,       gx,       gy -  6.0f);
        ctx.drawLine(gx -  2.0f, gy,       gx,       gy -  2.0f);
    }
}

// ── Mouse handling ────────────────────────────────────────────────────────────

void FloatWindow::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;

    // Close button
    if (closable_ && closeButtonRect().contains(e.x, e.y))
    {
        hoveredBtn_ = 0;
        setVisible(false);
        closed.emit();
        e.consumed = true;
        return;
    }

    // Minimize button
    if (minimizable_ && minimizeButtonRect().contains(e.x, e.y))
    {
        setMinimized(!minimized_);
        e.consumed = true;
        return;
    }

    // Resize grip (check before title bar)
    if (resizable_ && !minimized_ && resizeGripRect().contains(e.x, e.y))
    {
        resizing_     = true;
        resizeOffX_   = e.x;
        resizeOffY_   = e.y;
        resizeStartW_ = floatW_;
        resizeStartH_ = floatH_;
        e.consumed    = true;
        return;
    }

    // Title bar drag
    if (titleBarRect().contains(e.x, e.y))
    {
        dragging_ = true;
        dragOffX_ = e.x - floatX_;
        dragOffY_ = e.y - floatY_;
        e.consumed = true;
        return;
    }

    // Forward to content
    if (content_ && !minimized_)
        content_->onMousePress(e);
}

void FloatWindow::onMouseRelease(MouseEvent& e)
{
    if (dragging_) { dragging_ = false; e.consumed = true; return; }
    if (resizing_) { resizing_ = false; e.consumed = true; return; }
    if (content_ && !minimized_)
        content_->onMouseRelease(e);
}

void FloatWindow::onMouseMove(MouseEvent& e)
{
    if (dragging_)
    {
        floatX_ = e.x - dragOffX_;
        floatY_ = e.y - dragOffY_;
        layout();
        markDirty();
        e.consumed = true;
        return;
    }

    if (resizing_)
    {
        floatW_ = std::max(120.0f, resizeStartW_ + (e.x - resizeOffX_));
        floatH_ = std::max(titleBarH_ + 20.0f, resizeStartH_ + (e.y - resizeOffY_));
        layout();
        markDirty();
        e.consumed = true;
        return;
    }

    // Button hover tracking
    int old = hoveredBtn_;
    hoveredBtn_ = 0;
    if (closable_    && closeButtonRect().contains(e.x, e.y))   hoveredBtn_ = 1;
    else if (minimizable_ && minimizeButtonRect().contains(e.x, e.y)) hoveredBtn_ = 2;
    if (hoveredBtn_ != old) markDirty();

    if (content_ && !minimized_)
        content_->onMouseMove(e);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Canvas
// ═════════════════════════════════════════════════════════════════════════════

void Canvas::layout()
{
    for (auto* c : children_)
        c->layout();
}

void Canvas::paint(PaintContext& ctx)
{
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    // Background
    ctx.fill.SetColor(bgColor_);
    ctx.fillRect(abs.x, abs.y, abs.w, abs.h);

    // Border
    auto& t = Theme::instance();
    ctx.line.SetColor(t.borderColor);
    ctx.lineRect(abs.x, abs.y, abs.w, abs.h);

    ctx.pushClip(abs);

    if (onPaint_)
        onPaint_(ctx, abs);

    for (auto* c : children_)
        c->paint(ctx);

    ctx.popClip();
}

// ═════════════════════════════════════════════════════════════════════════════
//  ImageView
// ═════════════════════════════════════════════════════════════════════════════

void ImageView::setTexture(BuGUI::TextureHandle tex, int w, int h)
{
    texture_ = tex;
    texW_ = w;
    texH_ = h;
    if (tex && rect_.w == 0 && rect_.h == 0)
        setSize(static_cast<float>(w), static_cast<float>(h));
    markDirty();
}

void ImageView::paint(PaintContext& ctx)
{
    if (!texture_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    ctx.pushClip(abs);

    Rect dst = { abs.x + offsetX_, abs.y + offsetY_,
                 static_cast<float>(texW_), static_cast<float>(texH_) };
    ctx.drawImage(texture_, dst, {0.f, 0.f, 1.f, 1.f}, tint_);

    Widget::paint(ctx);
    ctx.popClip();
}

// ═════════════════════════════════════════════════════════════════════════════
//  PageView
// ═════════════════════════════════════════════════════════════════════════════

void PageView::addPageImpl(Widget* w)
{
    addChild(w);
    w->setVisible(false);
    pages_.push_back(w);

    if (pages_.size() == 1) {
        currentIdx_ = 0;
        w->setVisible(true);
    }
}

Widget* PageView::page(int idx) const
{
    if (idx < 0 || idx >= static_cast<int>(pages_.size())) return nullptr;
    return pages_[idx];
}

void PageView::setPage(int idx, Transition tr)
{
    if (idx < 0 || idx >= static_cast<int>(pages_.size())) return;
    if (idx == currentIdx_ && transProgress_ >= 1.0f) return;

    prevIdx_ = currentIdx_;
    currentIdx_ = idx;

    if (tr == SlideAuto) {
        if (prevIdx_ < 0) tr = None;
        else tr = (idx > prevIdx_) ? SlideLeft : SlideRight;
    }
    transType_ = tr;
    transProgress_ = (tr == None) ? 1.0f : 0.0f;

    if (currentIdx_ >= 0 && currentIdx_ < static_cast<int>(pages_.size()))
        pages_[currentIdx_]->setVisible(true);

    pageChanged.emit(currentIdx_);
    markDirty();
    WidgetApp::instance().requestLayout();
}

void PageView::setPage(Widget* pg, Transition tr)
{
    for (int i = 0; i < static_cast<int>(pages_.size()); i++) {
        if (pages_[i] == pg) { setPage(i, tr); return; }
    }
}

void PageView::layout()
{
    for (auto* p : pages_) {
        p->setRect({0, 0, rect_.w, rect_.h});
        if (p->isVisible()) p->layout();
    }
}

void PageView::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    float dt = WidgetApp::instance().deltaTime();

    if (transProgress_ < 1.0f) {
        float speed = (transDuration_ > 0.0f) ? (1.0f / transDuration_) : 10.0f;
        transProgress_ += speed * dt;
        if (transProgress_ >= 1.0f) {
            transProgress_ = 1.0f;
            if (prevIdx_ >= 0 && prevIdx_ < static_cast<int>(pages_.size()) && prevIdx_ != currentIdx_)
                pages_[prevIdx_]->setVisible(false);
            prevIdx_ = -1;
        }
        markDirty();
    }

    ctx.pushClip(abs);

    bool animating = (transProgress_ < 1.0f && prevIdx_ >= 0);

    if (!animating) {
        if (currentIdx_ >= 0 && currentIdx_ < static_cast<int>(pages_.size()))
            pages_[currentIdx_]->paint(ctx);
    } else {
        float t = transProgress_;
        float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);

        Widget* oldPage = pages_[prevIdx_];
        Widget* newPage = pages_[currentIdx_];

        switch (transType_) {
        case SlideLeft: {
            float oldX = -abs.w * eased;
            float newX = abs.w * (1.0f - eased);
            Rect oldR = oldPage->rect(), newR = newPage->rect();
            oldPage->setRect({oldR.x + oldX, oldR.y, oldR.w, oldR.h});
            newPage->setRect({newR.x + newX, newR.y, newR.w, newR.h});
            oldPage->paint(ctx);
            newPage->paint(ctx);
            oldPage->setRect(oldR);
            newPage->setRect(newR);
            break;
        }
        case SlideRight: {
            float oldX = abs.w * eased;
            float newX = -abs.w * (1.0f - eased);
            Rect oldR = oldPage->rect(), newR = newPage->rect();
            oldPage->setRect({oldR.x + oldX, oldR.y, oldR.w, oldR.h});
            newPage->setRect({newR.x + newX, newR.y, newR.w, newR.h});
            oldPage->paint(ctx);
            newPage->paint(ctx);
            oldPage->setRect(oldR);
            newPage->setRect(newR);
            break;
        }
        case Fade: {
            oldPage->paint(ctx);
            unsigned char alpha = static_cast<unsigned char>(eased * 180);
            ctx.fill.SetColor(0, 0, 0, alpha);
            ctx.fillRect(abs.x, abs.y, abs.w, abs.h);
            newPage->paint(ctx);
            break;
        }
        default:
            newPage->paint(ctx);
            break;
        }
    }

    ctx.popClip();
}
