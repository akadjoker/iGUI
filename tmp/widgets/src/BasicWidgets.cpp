#include "pch.hpp"
#include "BasicWidgets.hpp"

// ═════════════════════════════════════════════════════════════════════════════
//  Label
// ═════════════════════════════════════════════════════════════════════════════

Label::Label(const std::string& text) : text_(text) {}

void Label::setText(const std::string& t) { text_ = t; markDirty(); }
void Label::setColor(const Color& c)      { color_ = c; }
void Label::setAlign(TextAlign a)         { align_ = a; markDirty(); }

Widget::Vec2f Label::sizeHint() const
{
    const auto& t = Theme::instance();
    float w = text_.size() * t.fontSize * 0.6f;
    return {w + t.padding * 2, t.fontSize + t.padding * 2};
}

void Label::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();
    const Color& c = enabled_ ? color_ : t.textDisabled;

    if (overrideFont_) ctx.pushFont(overrideFont_);
    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetColor(c);
    ctx.font.SetBatch(&ctx.text);

    float textW = ctx.font.GetTextWidth(text_.c_str());
    float tx;
    switch (align_)
    {
    case TextAlign::CENTER: tx = abs.x + (abs.w - textW) * 0.5f; break;
    case TextAlign::RIGHT:  tx = abs.x + abs.w - textW - t.padding; break;
    default:                tx = abs.x + t.padding; break;
    }
    float asc = ctx.font.GetAscender();
    float ty = abs.y + (abs.h + asc) * 0.5f;

    Rect textRect = {tx, ty - asc, textW, asc};
    if (!ctx.isClipped(textRect))
        ctx.font.Print(text_.c_str(), tx, ty);
    if (overrideFont_) ctx.popFont();    Widget::paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Spacer
// ═════════════════════════════════════════════════════════════════════════════

Spacer::Spacer(float size) : size_(size) {}

Widget::Vec2f Spacer::sizeHint() const
{
    return {size_, size_};
}

// ═════════════════════════════════════════════════════════════════════════════
//  Line (separator)
// ═════════════════════════════════════════════════════════════════════════════

Line::Line(float thickness) : thickness_(thickness) {}

Widget::Vec2f Line::sizeHint() const
{
    auto* p = dynamic_cast<BoxLayout*>(parent());
    if (p && p->dir() == LayoutDir::Horizontal)
        return {thickness_, 0.0f};
    return {0.0f, thickness_};
}

void Line::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    auto* p = dynamic_cast<BoxLayout*>(parent());
    bool vertical = p && p->dir() == LayoutDir::Horizontal;

    ctx.line.SetColor(color_.r, color_.g, color_.b, color_.a);
    int t = std::max(1, (int)std::round(thickness_));

    if (vertical)
    {
        float cx = abs.x + abs.w * 0.5f;
        float x0 = cx - (t - 1) * 0.5f;
        for (int i = 0; i < t; ++i)
            ctx.drawLine(x0 + i, abs.y, x0 + i, abs.y + abs.h);
    }
    else
    {
        float cy = abs.y + abs.h * 0.5f;
        float y0 = cy - (t - 1) * 0.5f;
        for (int i = 0; i < t; ++i)
            ctx.drawLine(abs.x, y0 + i, abs.x + abs.w, y0 + i);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Button
// ═════════════════════════════════════════════════════════════════════════════

Button::Button(const std::string& text) : text_(text)
{
    acceptsFocus_ = true;
    cursor_ = CursorType::Hand;
}

void Button::setText(const std::string& t) { text_ = t; markDirty(); }
void Button::setAlign(TextAlign a)         { align_ = a; markDirty(); }

Widget::Vec2f Button::sizeHint() const
{
    const auto& t = Theme::instance();
    float textW = text_.size() * t.fontSize * 0.6f;
    float iconW = (iconId_ != IconId::None) ? (t.fontSize + 2.f) : 0.f;
    return {textW + iconW + t.padding * 4, t.buttonHeight};
}

void Button::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    Rect clipped;
    if (ctx.clipRectIntersect(abs, clipped))
    {
        Color bg;
        if (bgColor_.a > 0)
        {
            bg = bgColor_;
            if (pressed_)      { bg.r = bg.r * 3/4; bg.g = bg.g * 3/4; bg.b = bg.b * 3/4; }
            else if (hovered_) { bg.r = std::min(255, bg.r + 20); bg.g = std::min(255, bg.g + 20); bg.b = std::min(255, bg.b + 20); }
        }
        else
        {
            bg = pressed_ ? t.buttonPressed : (hovered_ ? t.buttonHover : t.buttonNormal);
        }
        if (!enabled_) bg = t.panelColor;

        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fill.RoundedRectangle(
            clipped.x, clipped.y, clipped.w, clipped.h,
            t.borderRadius, 6, true);

        Color bc = focused_ ? t.focusColor : t.buttonBorder;
        ctx.line.SetColor(bc.r, bc.g, bc.b, bc.a);
        ctx.line.RoundedRectangle(
            clipped.x, clipped.y, clipped.w, clipped.h,
            t.borderRadius, 6, false);
    }

    if (overrideFont_) ctx.pushFont(overrideFont_);
    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetColor(enabled_ ? t.textColor : t.textDisabled);
    ctx.font.SetBatch(&ctx.text);
    float textW = ctx.font.GetTextWidth(text_.c_str());

    float iconSz = t.fontSize;
    float iconW  = (iconId_ != IconId::None) ? (iconSz + 2.f) : 0.f;
    float totalW = iconW + textW;

    float tx;
    switch (align_)
    {
    case TextAlign::LEFT:   tx = abs.x + t.padding * 2; break;
    case TextAlign::RIGHT:  tx = abs.x + abs.w - totalW - t.padding * 2; break;
    default:                tx = abs.x + (abs.w - totalW) * 0.5f; break;
    }

    // Draw icon
    if (iconId_ != IconId::None) {
        float iy = abs.y + (abs.h - iconSz) * 0.5f;
        ctx.drawIcon(iconId_, tx, iy, iconSz);
        tx += iconW;
    }

    float asc = ctx.font.GetAscender();
    float ty = abs.y + (abs.h + asc) * 0.5f;

    Rect textRect = {tx, ty - asc, textW, asc};
    if (!ctx.isClipped(textRect))
        ctx.font.Print(text_.c_str(), tx, ty);

    if (overrideFont_) ctx.popFont();
    // paint children (no clip needed — button children are rare)
    for (auto& c : children_)
        c->paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  ImageButton
// ═════════════════════════════════════════════════════════════════════════════

Widget::Vec2f ImageButton::sizeHint() const
{
    float w = rect_.w > 0 ? rect_.w : (srcRect_.width + iconPad_ * 2);
    float h = rect_.h > 0 ? rect_.h : (srcRect_.height + iconPad_ * 2);
    return {w, h};
}

void ImageButton::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    Rect clipped;
    if (ctx.clipRectIntersect(abs, clipped))
    {
        Color bg;
        if (bgColor_.a > 0)
        {
            bg = bgColor_;
            if (pressed_)      { bg.r = bg.r * 3/4; bg.g = bg.g * 3/4; bg.b = bg.b * 3/4; }
            else if (hovered_) { bg.r = std::min(255, bg.r + 20); bg.g = std::min(255, bg.g + 20); bg.b = std::min(255, bg.b + 20); }
        }
        else
        {
            bg = pressed_ ? t.buttonPressed : (hovered_ ? t.buttonHover : t.buttonNormal);
        }
        if (!enabled_) bg = t.panelColor;

        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fill.RoundedRectangle(
            clipped.x, clipped.y, clipped.w, clipped.h,
            t.borderRadius, 6, true);

        Color bc = focused_ ? t.focusColor : t.buttonBorder;
        ctx.line.SetColor(bc.r, bc.g, bc.b, bc.a);
        ctx.line.RoundedRectangle(
            clipped.x, clipped.y, clipped.w, clipped.h,
            t.borderRadius, 6, false);
    }

    if (tex_ && texW_ > 0 && texH_ > 0 && srcRect_.width > 0 && srcRect_.height > 0)
    {
        float dstW = abs.w - iconPad_ * 2;
        float dstH = abs.h - iconPad_ * 2;
        float scale = std::min(dstW / srcRect_.width, dstH / srcRect_.height);
        float iw = srcRect_.width  * scale;
        float ih = srcRect_.height * scale;
        float ix = abs.x + (abs.w - iw) * 0.5f;
        float iy = abs.y + (abs.h - ih) * 0.5f;

        float tw = static_cast<float>(texW_);
        float th = static_cast<float>(texH_);
        BuGUI::Rect uv = { srcRect_.x / tw, srcRect_.y / th,
                           srcRect_.width / tw, srcRect_.height / th };
        Rect dst = { ix, iy, iw, ih };
        ctx.drawImage(tex_, dst, uv, tint_);
    }

    Widget::paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  IconButton
// ═════════════════════════════════════════════════════════════════════════════

IconButton::IconButton(Icon icon)
    : icon_(icon), targetIcon_(icon)
{
    acceptsFocus_ = true;
    cursor_ = CursorType::Hand;
}

void IconButton::setIcon(Icon icon)
{
    if (icon == targetIcon_) return;
    if (animated_)
    {
        icon_ = targetIcon_;
        targetIcon_ = icon;
        animProgress_ = 0.0f;
    }
    else
    {
        icon_ = icon;
        targetIcon_ = icon;
        animProgress_ = 1.0f;
    }
    markDirty();
}

Widget::Vec2f IconButton::sizeHint() const
{
    return {32.0f, Theme::instance().buttonHeight};
}

void IconButton::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    if (animated_ && animProgress_ < 1.0f)
    {
        float dt = WidgetApp::instance().deltaTime();
        animProgress_ += dt * 4.0f;
        if (animProgress_ >= 1.0f)
        {
            animProgress_ = 1.0f;
            icon_ = targetIcon_;
        }
        markDirty();
    }

    Rect clipped;
    if (ctx.clipRectIntersect(abs, clipped))
    {
        Color bg = pressed_ ? t.buttonPressed : (hovered_ ? t.buttonHover : t.buttonNormal);
        if (!enabled_) bg = t.panelColor;
        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fill.RoundedRectangle(
            clipped.x, clipped.y, clipped.w, clipped.h,
            t.borderRadius, 6, true);

        Color bc = focused_ ? t.focusColor : t.buttonBorder;
        ctx.line.SetColor(bc.r, bc.g, bc.b, bc.a);
        ctx.line.RoundedRectangle(
            clipped.x, clipped.y, clipped.w, clipped.h,
            t.borderRadius, 6, false);
    }

    float progress = animProgress_;
    if (animated_ && progress < 1.0f)
    {
        Color oldColor = iconColor_;
        oldColor.a = static_cast<unsigned char>(iconColor_.a * (1.0f - progress));
        Color newColor = iconColor_;
        newColor.a = static_cast<unsigned char>(iconColor_.a * progress);

        ctx.line.SetColor(oldColor.r, oldColor.g, oldColor.b, oldColor.a);
        drawIcon(ctx, abs, icon_, 1.0f - progress);
        ctx.line.SetColor(newColor.r, newColor.g, newColor.b, newColor.a);
        drawIcon(ctx, abs, targetIcon_, progress);
    }
    else
    {
        ctx.line.SetColor(iconColor_.r, iconColor_.g, iconColor_.b, iconColor_.a);
        drawIcon(ctx, abs, targetIcon_, 1.0f);
    }

    Widget::paint(ctx);
}

void IconButton::drawIcon(PaintContext& ctx, const Rect& abs, Icon icon, float t)
{
    float iconSize = 14.0f;
    float cx = abs.x + abs.w * 0.5f;
    float cy = abs.y + abs.h * 0.5f;
    float hs = iconSize * 0.5f;
    float lw = lineWidth_;

    switch (icon)
    {
    case Hamburger:
    {
        float left = cx - hs;
        float y0 = cy - hs + 1;
        float y1 = cy;
        float y2 = cy + hs - 1;
        ctx.fill.SetColor(iconColor_.r, iconColor_.g, iconColor_.b,
                          static_cast<unsigned char>(iconColor_.a * t));
        ctx.fill.Rectangle(static_cast<int>(left), static_cast<int>(y0 - lw * 0.5f),
                           static_cast<int>(iconSize), static_cast<int>(lw), true);
        ctx.fill.Rectangle(static_cast<int>(left), static_cast<int>(y1 - lw * 0.5f),
                           static_cast<int>(iconSize), static_cast<int>(lw), true);
        ctx.fill.Rectangle(static_cast<int>(left), static_cast<int>(y2 - lw * 0.5f),
                           static_cast<int>(iconSize), static_cast<int>(lw), true);
        break;
    }
    case Close:
    {
        float left = cx - hs, right = cx + hs;
        float top  = cy - hs, bot   = cy + hs;
        ctx.line.Line2D(left, top, right, bot);
        ctx.line.Line2D(right, top, left, bot);
        break;
    }
    case ArrowLeft:
    {
        float left = cx - hs * 0.6f, right = cx + hs * 0.6f;
        ctx.line.Line2D(right, cy - hs, left, cy);
        ctx.line.Line2D(left, cy, right, cy + hs);
        break;
    }
    case ArrowRight:
    {
        float left = cx - hs * 0.6f, right = cx + hs * 0.6f;
        ctx.line.Line2D(left, cy - hs, right, cy);
        ctx.line.Line2D(right, cy, left, cy + hs);
        break;
    }
    case ArrowUp:
    {
        float top = cy - hs * 0.6f, bot = cy + hs * 0.6f;
        ctx.line.Line2D(cx - hs, bot, cx, top);
        ctx.line.Line2D(cx, top, cx + hs, bot);
        break;
    }
    case ArrowDown:
    {
        float top = cy - hs * 0.6f, bot = cy + hs * 0.6f;
        ctx.line.Line2D(cx - hs, top, cx, bot);
        ctx.line.Line2D(cx, bot, cx + hs, top);
        break;
    }
    case Plus:
    {
        ctx.line.Line2D(cx - hs, cy, cx + hs, cy);
        ctx.line.Line2D(cx, cy - hs, cx, cy + hs);
        break;
    }
    case Minus:
    {
        ctx.line.Line2D(cx - hs, cy, cx + hs, cy);
        break;
    }
    case Check:
    {
        float x0 = cx - hs,       x1 = cx - hs * 0.3f, x2 = cx + hs;
        float y0 = cy,            y1 = cy + hs * 0.6f,  y2 = cy - hs * 0.5f;
        ctx.line.Line2D(x0, y0, x1, y1);
        ctx.line.Line2D(x1, y1, x2, y2);
        break;
    }
    case Chevron:
    {
        float left = cx - hs * 0.4f, right = cx + hs * 0.4f;
        ctx.line.Line2D(left, cy - hs, right, cy);
        ctx.line.Line2D(right, cy, left, cy + hs);
        break;
    }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Panel
// ═════════════════════════════════════════════════════════════════════════════

void Panel::setBgColor(const Color& c) { bgColor_ = c; }

void Panel::layout()
{
    for (auto* c : children_)
    {
        c->setRect({0, 0, rect_.w, rect_.h});
        c->layout();
    }
}

void Panel::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();

    Rect clipped;
    if (ctx.clipRectIntersect(abs, clipped))
    {
        ctx.fill.SetColor(bgColor_.r, bgColor_.g, bgColor_.b, bgColor_.a);
        ctx.fill.RoundedRectangle(
            clipped.x, clipped.y, clipped.w, clipped.h,
            t.borderRadius, 6, true);

        ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
        ctx.line.RoundedRectangle(
            clipped.x, clipped.y, clipped.w, clipped.h,
            t.borderRadius, 6, false);
    }

    ctx.pushClip(abs);
    for (auto& c : children_)
        c->paint(ctx);
    ctx.popClip();
}

// ═════════════════════════════════════════════════════════════════════════════
//  CheckBox
// ═════════════════════════════════════════════════════════════════════════════

CheckBox::CheckBox(const std::string& text) : text_(text)
{
    acceptsFocus_ = true;
    cursor_ = CursorType::Hand;
    clicked.connect([this]() {
        checked_ = !checked_;
        toggled.emit(checked_);
        WidgetApp::instance().fireEvent("toggle", this);
        markDirty();
    });
}

void CheckBox::setText(const std::string& t) { text_ = t; markDirty(); }
void CheckBox::setChecked(bool c)            { checked_ = c; markDirty(); }

Widget::Vec2f CheckBox::sizeHint() const
{
    const auto& t = Theme::instance();
    float boxSize = t.fontSize;
    float textW = text_.size() * t.fontSize * 0.6f;
    return {boxSize + t.padding * 2 + textW, t.fontSize + t.padding * 2};
}

void CheckBox::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();
    float boxSize = t.fontSize;
    float bx = abs.x + t.padding;
    float by = abs.y + (abs.h - boxSize) * 0.5f;

    Rect boxRect = {bx, by, boxSize, boxSize};
    Rect clippedBox;
    if (ctx.clipRectIntersect(boxRect, clippedBox))
    {
        Color bg = hovered_ ? t.inputBgHover : t.inputBg;
        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fill.Rectangle(clippedBox.x, clippedBox.y,
                            clippedBox.w, clippedBox.h, true);

        Color border = focused_ ? t.focusColor : (hovered_ ? t.inputBorderHover : t.inputBorder);
        ctx.line.SetColor(border.r, border.g, border.b, border.a);
        ctx.line.Rectangle(clippedBox.x, clippedBox.y,
                            clippedBox.w, clippedBox.h, false);

        if (checked_)
        {
            float m = 3.0f;
            Rect markRect = {bx + m, by + m, boxSize - m * 2, boxSize - m * 2};
            Rect clippedMark;
            if (ctx.clipRectIntersect(markRect, clippedMark))
            {
                ctx.fill.SetColor(t.checkMark.r, t.checkMark.g, t.checkMark.b, t.checkMark.a);
                ctx.fill.Rectangle(clippedMark.x, clippedMark.y,
                                    clippedMark.w, clippedMark.h, true);
            }
        }
    }

    if (overrideFont_) ctx.pushFont(overrideFont_);
    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetColor(enabled_ ? t.textColor : t.textDisabled);
    ctx.font.SetBatch(&ctx.text);
    float tx = bx + boxSize + t.padding;
    float asc = ctx.font.GetAscender();
    float ty = abs.y + (abs.h + asc) * 0.5f;

    Rect textRect = {tx, ty - asc, abs.w, asc};
    if (!ctx.isClipped(textRect))
        ctx.font.Print(text_.c_str(), tx, ty);
    if (overrideFont_) ctx.popFont();    Widget::paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  RadioGroup
// ═════════════════════════════════════════════════════════════════════════════

RadioGroup::~RadioGroup()
{
    for (auto* rb : buttons_)
        rb->group_ = nullptr;
}

void RadioGroup::add(RadioButton* rb)
{
    buttons_.push_back(rb);
}

void RadioGroup::remove(RadioButton* rb)
{
    buttons_.erase(std::remove(buttons_.begin(), buttons_.end(), rb), buttons_.end());
    if (selected_ == rb) selected_ = nullptr;
}

int RadioGroup::selectedIndex() const
{
    for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
        if (buttons_[i] == selected_) return i;
    return -1;
}

void RadioGroup::select(RadioButton* rb)
{
    if (selected_ == rb) return;

    if (selected_)
    {
        selected_->selected_ = false;
        selected_->markDirty();
    }

    selected_ = rb;
    if (rb)
    {
        rb->selected_ = true;
        rb->markDirty();
        rb->toggled.emit(true);
    }

    selectionChanged.emit(selectedIndex());
}

// ═════════════════════════════════════════════════════════════════════════════
//  RadioButton
// ═════════════════════════════════════════════════════════════════════════════

RadioButton::RadioButton(const std::string& text, RadioGroup* group)
    : text_(text), group_(group)
{
    acceptsFocus_ = true;
    cursor_ = CursorType::Hand;

    if (group_) group_->add(this);

    clicked.connect([this]() {
        if (group_)
            group_->select(this);
        else
        {
            selected_ = true;
            toggled.emit(true);
            markDirty();
        }
    });
}

RadioButton::~RadioButton()
{
    if (group_) group_->remove(this);
}

void RadioButton::setSelected(bool s)
{
    if (s && group_)
        group_->select(this);
    else if (!s && selected_)
    {
        selected_ = false;
        toggled.emit(false);
        markDirty();
    }
}

void RadioButton::setGroup(RadioGroup* g)
{
    if (group_) group_->remove(this);
    group_ = g;
    if (group_) group_->add(this);
}

Widget::Vec2f RadioButton::sizeHint() const
{
    const auto& t = Theme::instance();
    float circSize = t.fontSize;
    float textW = text_.size() * t.fontSize * 0.6f;
    return {circSize + t.padding * 2 + textW, t.fontSize + t.padding * 2};
}

void RadioButton::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();
    float r = t.fontSize * 0.5f;
    float cx = abs.x + t.padding + r;
    float cy = abs.y + abs.h * 0.5f;

    Color border = focused_ ? t.focusColor : (hovered_ ? t.inputBorderHover : t.inputBorder);
    ctx.line.SetColor(border.r, border.g, border.b, border.a);
    ctx.lineCircle(cx, cy, r);

    Color bg = hovered_ ? t.inputBgHover : t.inputBg;
    ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
    ctx.fillCircle(cx, cy, r - 1.0f);

    if (selected_)
    {
        ctx.fill.SetColor(t.checkMark.r, t.checkMark.g, t.checkMark.b, t.checkMark.a);
        ctx.fillCircle(cx, cy, r * 0.45f);
    }

    if (overrideFont_) ctx.pushFont(overrideFont_);
    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetColor(enabled_ ? t.textColor : t.textDisabled);
    ctx.font.SetBatch(&ctx.text);
    float rtx = abs.x + t.padding + t.fontSize + t.padding;
    float asc2 = ctx.font.GetAscender();
    float rty = abs.y + (abs.h + asc2) * 0.5f;

    Rect trRect = {rtx, rty - asc2, abs.w, asc2};
    if (!ctx.isClipped(trRect))
        ctx.font.Print(text_.c_str(), rtx, rty);
    if (overrideFont_) ctx.popFont();    Widget::paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Switch
// ═════════════════════════════════════════════════════════════════════════════

Switch::Switch(const std::string& text) : text_(text)
{
    acceptsFocus_ = true;
    cursor_ = CursorType::Hand;

    clicked.connect([this]() {
        on_ = !on_;
        toggled.emit(on_);
        markDirty();
    });
}

void Switch::setOn(bool v)
{
    if (on_ != v) { on_ = v; toggled.emit(on_); markDirty(); }
}

Widget::Vec2f Switch::sizeHint() const
{
    const auto& t = Theme::instance();
    float trackW = 36.0f;
    float trackH = 18.0f;
    float textW  = text_.size() * t.fontSize * 0.6f;
    float w = trackW + (text_.empty() ? 0 : t.padding + textW);
    return {w + t.padding * 2, std::max(trackH, t.fontSize) + t.padding * 2};
}

void Switch::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();
    float trackW = 36.0f;
    float trackH = 18.0f;
    float swx = abs.x + t.padding;
    float swy = abs.y + (abs.h - trackH) * 0.5f;
    float thumbR = trackH * 0.5f - 2.0f;

    Color trackCol = on_ ? onColor_ : offColor_;
    if (hovered_)
    {
        trackCol.r = static_cast<u8>(std::min(255, trackCol.r + 15));
        trackCol.g = static_cast<u8>(std::min(255, trackCol.g + 15));
        trackCol.b = static_cast<u8>(std::min(255, trackCol.b + 15));
    }
    ctx.fill.SetColor(trackCol.r, trackCol.g, trackCol.b, trackCol.a);
    ctx.fillRoundedRect(swx, swy, trackW, trackH, trackH * 0.5f, 8);

    Color brd = focused_ ? t.focusColor : (hovered_ ? t.inputBorderHover : t.inputBorder);
    ctx.line.SetColor(brd.r, brd.g, brd.b, brd.a);
    ctx.lineRoundedRect(swx, swy, trackW, trackH, trackH * 0.5f, 8);

    float thumbX = on_ ? (swx + trackW - trackH * 0.5f) : (swx + trackH * 0.5f);
    float thumbY = swy + trackH * 0.5f;
    ctx.fill.SetColor(t.switchThumb.r, t.switchThumb.g, t.switchThumb.b, t.switchThumb.a);
    ctx.fillCircle(thumbX, thumbY, thumbR);

    if (overrideFont_) ctx.pushFont(overrideFont_);
    if (!text_.empty())
    {
        ctx.font.SetFontSize(t.fontSize);
        ctx.font.SetColor(enabled_ ? t.textColor : t.textDisabled);
        ctx.font.SetBatch(&ctx.text);
        float lx = swx + trackW + t.padding;
        float asc3 = ctx.font.GetAscender();
        float ly = abs.y + (abs.h + asc3) * 0.5f;

        Rect textRect = {lx, ly - asc3, abs.w, asc3};
        if (!ctx.isClipped(textRect))
            ctx.font.Print(text_.c_str(), lx, ly);
    }
    if (overrideFont_) ctx.popFont();    Widget::paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Toolbar
// ═════════════════════════════════════════════════════════════════════════════

Toolbar::Toolbar(float height)
    : height_(height)
{
}

IconButton* Toolbar::addButton(const std::string& tooltip, IconButton::Icon icon,
                               std::function<void()> onClick)
{
    auto* btn = createChild<IconButton>(icon);
    btn->setSize(height_ - 4, height_ - 4);
    if (!tooltip.empty())
        btn->setTooltip(tooltip);
    if (onClick)
        btn->clicked.connect([cb = std::move(onClick)](){ cb(); });
    markDirty();
    return btn;
}

Widget* Toolbar::addWidget(Widget* w)
{
    addChild(w);
    markDirty();
    return w;
}

void Toolbar::addSeparator()
{
    auto* sep = createChild<Line>(1.0f);
    sep->setSize(1, height_ - 8);
    markDirty();
}

void Toolbar::layout()
{
    float x = spacing_;
    for (auto* c : children_) {
        if (!c->isVisible()) continue;
        auto hint = c->sizeHint();
        float cw = hint.x > 0 ? hint.x : c->rect().w;
        float ch = hint.y > 0 ? hint.y : c->rect().h;
        ch = std::min(ch, height_ - 4.0f);
        float yoff = (height_ - ch) * 0.5f;
        c->setPosition(x, yoff);
        c->setSize(cw, ch);
        x += cw + spacing_;
    }
}

Widget::Vec2f Toolbar::sizeHint() const
{
    float w = spacing_;
    for (auto* c : children_) {
        if (!c->isVisible()) continue;
        auto hint = c->sizeHint();
        w += (hint.x > 0 ? hint.x : c->rect().w) + spacing_;
    }
    return {w, height_};
}

void Toolbar::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    Color bg = bgColor_.a > 0 ? bgColor_ : Theme::instance().panelColor;
    Rect clipped;
    if (ctx.clipRectIntersect(abs, clipped)) {
        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fill.Rectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                           static_cast<int>(clipped.w), static_cast<int>(clipped.h), true);
    }

    // Bottom border
    const auto& t = Theme::instance();
    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.line.Line2D(abs.x, abs.y + abs.h - 1, abs.x + abs.w, abs.y + abs.h - 1);

    Widget::paint(ctx);
}
