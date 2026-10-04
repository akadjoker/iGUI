#include "AudioWidgets.hpp"
#include "Theme.hpp"
#include <cmath>
#include <algorithm>
#include <cstdio>

static constexpr float kPi = 3.14159265f;

// ─────────────────────────────────────────────────────────────────────────────
//  Font helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    inline float setupFont(PaintContext& ctx, const Color& color, float size)
    {
        ctx.font.SetFontSize(size);
        ctx.font.SetBatch(&ctx.text);
        ctx.font.SetColor(color);
        return ctx.font.GetAscender();
    }
    inline float setupFont(PaintContext& ctx, const Color& color)
    {
        auto& t = Theme::instance();
        ctx.font.SetFontSize(t.fontSize);
        ctx.font.SetBatch(&ctx.text);
        ctx.font.SetColor(color);
        return ctx.font.GetAscender();
    }

    inline Color lerpColor(const Color& a, const Color& b, float t)
    {
        return Color(
            static_cast<uint8_t>(a.r + (b.r - a.r) * t),
            static_cast<uint8_t>(a.g + (b.g - a.g) * t),
            static_cast<uint8_t>(a.b + (b.b - a.b) * t), 255);
    }

    inline Color levelColor(float t)
    {
        if (t > 0.90f) return Color(230, 50,  50, 255);
        if (t > 0.70f) return Color(240,190,  60, 255);
        return              Color( 70, 200, 100, 255);
    }

    inline float todB(float v) { return (v < 1e-7f) ? -96.0f : 20.0f * std::log10(v); }
    inline float fromDB(float db) { return std::pow(10.0f, db / 20.0f); }

    // One continuous band of a knob's value arc. Stroking it as a fan of short
    // lines leaves the soft edge of every joint overlapping its neighbour, and
    // the alpha adds up there, which reads as notches around the ring.
    inline void strokeArcBand(ig::retained::DrawList& list, float cx, float cy, float radius,
                              float startAngle, float sweepAngle, float fromTurn,
                              float toTurn, const Color& color, float thickness)
    {
        if (toTurn <= fromTurn)
            return;
        const float span = toTurn - fromTurn;
        const int steps = std::max(2, static_cast<int>(std::ceil(span * 48.0f)));
        for (int i = 0; i <= steps; ++i)
        {
            const float t = fromTurn + span * static_cast<float>(i) / static_cast<float>(steps);
            const float a = startAngle + sweepAngle * t;
            list.pathLineTo({cx + std::cos(a) * radius, cy + std::sin(a) * radius});
        }
        list.pathStroke(color, thickness, false);
    }

    static const char* kNoteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
}

// ═════════════════════════════════════════════════════════════════════════════
//  Knob
// ═════════════════════════════════════════════════════════════════════════════

Knob::Knob() {}

void Knob::setValue(float v)
{
    value_ = std::clamp(v, min_, max_);
    onChanged.emit(value_);
    markDirty();
}

void Knob::paint(PaintContext& ctx)
{
    if (!visible_) return;

    const Rect b = absoluteRect();
    ctx.pushClip(b);

    const float labelH = label_.empty() ? 0.f : 16.f;
    const float valH   = showValue_     ? 14.f :  0.f;
    const float knobH  = b.h - labelH - valH;
    const float outerRadius = std::min(b.w, knobH) * 0.5f - 1.f;
    const float cx     = b.x + b.w * 0.5f;
    const float cy     = b.y + knobH * 0.5f;
    const float norm   = normalised();

    const float arcThickness = 4.f;
    const float arcRadius    = outerRadius - arcThickness * 0.5f;
    const float bodyRadius   = outerRadius - arcThickness - 3.f;

    // The value arc's colour is the application's choice: with none set the knob
    // takes the theme accent rather than fixing one of its own.
    const Color accent = hasArcColor_ ? arcColor_ : Theme::instance().focusColor;

    // Value arc outside the body, one band per part so the ring stays clean.
    strokeArcBand(ctx.drawList, cx, cy, arcRadius, kStartAngle, kSweep, 0.f, 1.f,
                  trackColor_, arcThickness);
    strokeArcBand(ctx.drawList, cx, cy, arcRadius, kStartAngle, kSweep, 0.f, norm,
                  accent, arcThickness);

    // Body: a stack of shrinking circles from the bottom colour up to the lit
    // top one, gathered upward, which reads as a domed cap without a gradient
    // primitive for circles.
    const int shadeSteps = 7;
    for (int i = 0; i < shadeSteps; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(shadeSteps - 1);
        const Color shade = lerpColor(bgColor_, bodyTopColor_, t);
        ctx.fill.SetColor(shade.r, shade.g, shade.b, 255);
        ctx.fillCircle(cx, cy - t * bodyRadius * 0.30f, bodyRadius * (1.f - t * 0.55f));
    }

    // Thin ring around the cap.
    const int rimSegments = 40;
    ctx.line.SetColor(rimColor_.r, rimColor_.g, rimColor_.b, 255);
    for (int i = 0; i < rimSegments; ++i)
    {
        const float a0 = 2.f * kPi * static_cast<float>(i) / rimSegments;
        const float a1 = 2.f * kPi * static_cast<float>(i + 1) / rimSegments;
        ctx.drawLine(cx + bodyRadius * std::cos(a0), cy + bodyRadius * std::sin(a0),
                     cx + bodyRadius * std::cos(a1), cy + bodyRadius * std::sin(a1));
    }

    // The needle turns with the value: a line cut from near the centre out to
    // the edge of the cap, with a moulded dot near its tip. That is what makes
    // the knob read as something you grab and turn rather than as a gauge.
    const float angle = kStartAngle + kSweep * norm;
    const float ca = std::cos(angle);
    const float sa = std::sin(angle);
    ctx.drawList.addLine({cx + ca * bodyRadius * 0.20f, cy + sa * bodyRadius * 0.20f},
                         {cx + ca * bodyRadius * 0.92f, cy + sa * bodyRadius * 0.92f},
                         indicatorColor_, 2.5f);
    ctx.fill.SetColor(indicatorColor_.r, indicatorColor_.g, indicatorColor_.b, 255);
    ctx.fillCircle(cx + ca * bodyRadius * 0.78f, cy + sa * bodyRadius * 0.78f, 1.6f);

    // Value text (centred, under the dial)
    if (showValue_) {
        char buf[32];
        if (!valueText_.empty())
            snprintf(buf, sizeof(buf), "%s", valueText_.c_str());
        else
            snprintf(buf, sizeof(buf), "%.2f", value_);
        float asc = setupFont(ctx, Color(228, 234, 244, 255), 10.f);
        float tw  = ctx.font.GetTextWidth(buf);
        ctx.font.Print(buf, cx - tw * 0.5f, b.y + knobH + (valH - 10.f) * 0.5f + asc);
    }

    // Label (centred)
    if (!label_.empty()) {
        float asc = setupFont(ctx, Color(138, 148, 166, 255), 10.f);
        float tw  = ctx.font.GetTextWidth(label_.c_str());
        ctx.font.Print(label_.c_str(), cx - tw * 0.5f, b.y + knobH + valH + (labelH - 10.f) * 0.5f + asc);
    }

    ctx.popClip();
}

void Knob::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;
    dragging_   = true;
    dragStartY_ = e.y;
    dragStartV_ = value_;
    e.consumed  = true;
}

void Knob::onMouseMove(MouseEvent& e)
{
    if (!dragging_) return;
    float delta = (dragStartY_ - e.y) / sensitivity_;
    setValue(std::clamp(dragStartV_ + delta * (max_ - min_), min_, max_));
    e.consumed = true;
}

void Knob::onMouseRelease(MouseEvent& e)
{
    if (e.button != 0) return;
    dragging_ = false;
}

void Knob::onMouseScroll(MouseEvent& e)
{
    float step = (max_ - min_) * 0.02f;
    setValue(std::clamp(value_ + e.scrollY * step, min_, max_));
    e.consumed = true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Fader
// ═════════════════════════════════════════════════════════════════════════════

Fader::Fader()
{
    acceptsFocus_ = true;
    cursor_ = CursorType::SizeV;
}

void Fader::setValue(float v)
{
    const float clamped = std::clamp(v, min_, max_);
    if (clamped == value_)
        return;
    value_ = clamped;
    onChanged.emit(value_);
    markDirty();
}

Widget::Vec2f Fader::sizeHint() const
{
    if (orientation_ == LayoutDir::Vertical)
        return {40, 132};
    return {132, 40};
}

Rect Fader::bodyRect(const Rect& origin) const
{
    const float labelH = label_.empty() ? 0.f : 16.f;
    const float valH   = showValue_     ? 14.f :  0.f;
    const float height = origin.h - labelH - valH;
    return {origin.x, origin.y, origin.w, height > 0.f ? height : origin.h};
}

// The body in the widget's own coordinates, which is the space a MouseEvent
// arrives in: WidgetApp subtracts the widget's absolute origin before calling
// onMousePress/onMouseMove. Painting uses the absolute one instead, and mixing
// the two used to send a drag straight to the end of the range.
Rect Fader::localBody() const
{
    return bodyRect({0.f, 0.f, rect_.w, rect_.h});
}

float Fader::trackThickness(const Rect& body) const
{
    const bool vertical = orientation_ == LayoutDir::Vertical;
    return clamp((vertical ? body.w : body.h) * 0.24f, 6.f, 12.f);
}

float Fader::travel(const Rect& body) const
{
    const bool vertical = orientation_ == LayoutDir::Vertical;
    // The cap is 14 thick along the travel and never hangs off either end.
    const float span = (vertical ? body.h : body.w) - 14.f;
    return span > 1.f ? span : 1.f;
}

float Fader::capCentre(const Rect& body, float norm) const
{
    const bool vertical = orientation_ == LayoutDir::Vertical;
    return vertical ? body.y + body.h - 7.f - travel(body) * norm
                    : body.x + 7.f + travel(body) * norm;
}

void Fader::updateFromPointer(float localX, float localY)
{
    const Rect body = localBody();
    const bool vertical = orientation_ == LayoutDir::Vertical;
    const float along = (vertical ? localY : localX) + grabOffset_;
    const float from = vertical
        ? (body.y + body.h - 7.f - along) / travel(body)
        : (along - body.x - 7.f) / travel(body);
    setValue(min_ + clamp(from, 0.f, 1.f) * (max_ - min_));
}

void Fader::paint(PaintContext& ctx)
{
    if (!visible_) return;

    const Rect b = bodyRect(absoluteRect());
    ctx.pushClip(b);

    const auto& t = Theme::instance();
    const bool vertical = orientation_ == LayoutDir::Vertical;
    const float norm  = normalised();
    const float thick = trackThickness(b);
    const float capCentreNow = capCentre(b, norm);
    const float capAcross = clamp(vertical ? b.w : b.h, 20.f, 30.f);

    // The accent is the application's choice: with none set the fader takes the
    // theme accent rather than fixing one of its own, like the knob beside it.
    const Color accent = hasFillColor_ ? fillColor_ : t.focusColor;
    const Color groove = hovered_ ? t.sliderTrackHover : trackColor_;
    const Color cap    = hovered_ ? t.buttonHover : capColor_;

    const Rect track = vertical
        ? Rect{b.x + (b.w - thick) * 0.5f, b.y, thick, b.h}
        : Rect{b.x, b.y + (b.h - thick) * 0.5f, b.w, thick};
    const float radius = thick * 0.5f;

    // Marks down the side, wherever the control is wide enough to hold them.
    const float margin = ((vertical ? b.w : b.h) - thick) * 0.5f;
    if (showTicks_ && margin >= 8.f && travel(b) >= 48.f)
    {
        const float length = std::min(margin - 4.f, 8.f);
        ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 255);
        for (int i = 0; i <= 10; ++i)
        {
            const float mark = capCentre(b, static_cast<float>(i) / 10.f);
            const float len  = (i % 5 == 0) ? length : length * 0.6f;
            if (vertical)
            {
                const float x = track.x + track.w + 3.f;
                ctx.drawLine(x, mark, x + len, mark);
            }
            else
            {
                const float y = track.y + track.h + 3.f;
                ctx.drawLine(mark, y, mark, y + len);
            }
        }
    }

    // The recessed groove, then the value as the filled part of it.
    ctx.fill.SetColor(groove.r, groove.g, groove.b, 255);
    ctx.fill.RoundedRectangle(track.x, track.y, track.w, track.h, radius, 6, true);

    const Rect filled = vertical
        ? Rect{track.x, capCentreNow, track.w, b.y + b.h - capCentreNow}
        : Rect{track.x, track.y, capCentreNow - track.x, track.h};
    ctx.fill.SetColor(accent.r, accent.g, accent.b, 255);
    ctx.fill.RoundedRectangle(filled.x, filled.y, filled.w, filled.h, radius, 6, true);

    // The cap: the piece a finger pushes, lit on top and shaded underneath so it
    // reads as something with a thickness.
    const Rect capRect = vertical
        ? Rect{b.x + (b.w - capAcross) * 0.5f, capCentreNow - 7.f, capAcross, 14.f}
        : Rect{capCentreNow - 7.f, b.y + (b.h - capAcross) * 0.5f, 14.f, capAcross};
    ctx.fill.SetColor(cap.r, cap.g, cap.b, 255);
    ctx.fill.RoundedRectangle(capRect.x, capRect.y, capRect.w, capRect.h, 3.f, 6, true);
    const Color lit   = lerpColor(cap, Color(255, 255, 255, 255), 0.35f);
    const Color shade = lerpColor(cap, Color(0, 0, 0, 255), 0.45f);
    ctx.fill.SetColor(lit.r, lit.g, lit.b, 255);
    if (vertical)
        ctx.fill.Rectangle(capRect.x + 2.f, capRect.y, capRect.w - 4.f, 1.f, true);
    else
        ctx.fill.Rectangle(capRect.x, capRect.y + 2.f, 1.f, capRect.h - 4.f, true);
    ctx.fill.SetColor(shade.r, shade.g, shade.b, 255);
    if (vertical)
        ctx.fill.Rectangle(capRect.x + 2.f, capRect.y + capRect.h - 1.f, capRect.w - 4.f, 1.f, true);
    else
        ctx.fill.Rectangle(capRect.x + capRect.w - 1.f, capRect.y + 2.f, 1.f, capRect.h - 4.f, true);

    // The line across the cap, in the accent, is what the eye lines up with the
    // marks - and what a finger aims at.
    ctx.fill.SetColor(accent.r, accent.g, accent.b, 255);
    if (vertical)
        ctx.fill.Rectangle(capRect.x + 3.f, capCentreNow - 1.f, capRect.w - 6.f, 2.f, true);
    else
        ctx.fill.Rectangle(capCentreNow - 1.f, capRect.y + 3.f, 2.f, capRect.h - 6.f, true);

    ctx.popClip();

    // Value then label, centred under the control, the way the knob reads.
    const Rect full = absoluteRect();
    const float labelH = label_.empty() ? 0.f : 16.f;
    const float valH   = showValue_     ? 14.f :  0.f;
    const float cx     = full.x + full.w * 0.5f;
    if (showValue_)
    {
        char buf[32];
        if (!valueText_.empty())
            snprintf(buf, sizeof(buf), "%s", valueText_.c_str());
        else
            snprintf(buf, sizeof(buf), "%.2f", value_);
        const float asc = setupFont(ctx, Color(228, 234, 244, 255), 10.f);
        const float tw  = ctx.font.GetTextWidth(buf);
        ctx.font.Print(buf, cx - tw * 0.5f, b.y + b.h + (valH - 10.f) * 0.5f + asc);
    }
    if (!label_.empty())
    {
        const float asc = setupFont(ctx, Color(138, 148, 166, 255), 10.f);
        const float tw  = ctx.font.GetTextWidth(label_.c_str());
        ctx.font.Print(label_.c_str(), cx - tw * 0.5f, b.y + b.h + valH + (labelH - 10.f) * 0.5f + asc);
    }
}

void Fader::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;
    const Rect body = localBody();
    const bool vertical = orientation_ == LayoutDir::Vertical;
    const float capAcross = clamp(vertical ? body.w : body.h, 20.f, 30.f);
    const float centre = capCentre(body, normalised());
    const Rect cap = vertical
        ? Rect{body.x + (body.w - capAcross) * 0.5f, centre - 7.f, capAcross, 14.f}
        : Rect{centre - 7.f, body.y + (body.h - capAcross) * 0.5f, 14.f, capAcross};
    // A press on the cap keeps the gap it landed with, so the cap stays where
    // the hand found it; anywhere else jumps the cap under the pointer.
    grabOffset_ = cap.contains(e.localX, e.localY)
        ? centre - (vertical ? e.localY : e.localX)
        : 0.f;
    dragging_ = true;
    updateFromPointer(e.localX, e.localY);
    e.consumed = true;
}

void Fader::onMouseMove(MouseEvent& e)
{
    if (!dragging_) return;
    updateFromPointer(e.localX, e.localY);
    e.consumed = true;
}

void Fader::onMouseRelease(MouseEvent& e)
{
    if (e.button != 0) return;
    dragging_ = false;
}

void Fader::onMouseScroll(MouseEvent& e)
{
    const float step = (max_ - min_) * 0.02f;
    setValue(std::clamp(value_ + e.scrollY * step, min_, max_));
    e.consumed = true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  LinearKnob
// ═════════════════════════════════════════════════════════════════════════════

LinearKnob::LinearKnob() {}

void LinearKnob::setValue(float v)
{
    value_ = std::clamp(v, min_, max_);
    onChanged.emit(value_);
    markDirty();
}

Widget::Vec2f LinearKnob::sizeHint() const
{
    return vertical_ ? Vec2f{28, 100} : Vec2f{120, 28};
}

Rect LinearKnob::trackRect() const
{
    const Rect b = absoluteRect();
    const float labelH = label_.empty() ? 0.f : 14.f;
    const float valH   = showValue_     ? 12.f :  0.f;
    const float footH  = labelH + valH;
    if (vertical_) {
        float tw = 8.f;
        return {b.x + (b.w - tw) * 0.5f, b.y + 8.f, tw, b.h - footH - 16.f};
    }
    float th = 8.f;
    return {b.x + 8.f, b.y + (b.h - footH - th) * 0.5f, b.w - 16.f, th};
}

void LinearKnob::paint(PaintContext& ctx)
{
    const Rect b    = absoluteRect();
    const float labelH = label_.empty() ? 0.f : 14.f;
    const float valH   = showValue_     ? 12.f :  0.f;
    const float norm   = normalised();
    const Rect  tr     = trackRect();

    ctx.pushClip(b);

    // Track background with rounded ends
    ctx.fill.SetColor(trackColor_.r, trackColor_.g, trackColor_.b, 255);
    ctx.fillRect(tr.x, tr.y, tr.w, tr.h);

    // Filled portion
    ctx.fill.SetColor(fillColor_.r, fillColor_.g, fillColor_.b, 200);
    if (vertical_) {
        float fillH = tr.h * norm;
        ctx.fillRect(tr.x, tr.y + tr.h - fillH, tr.w, fillH);
    } else {
        ctx.fillRect(tr.x, tr.y, tr.w * norm, tr.h);
    }

    // Thumb
    float thumbW = vertical_ ? tr.w + 8.f : 8.f;
    float thumbH = vertical_ ? 8.f : tr.h + 8.f;
    float tx, ty;
    if (vertical_) {
        tx = tr.x - 4.f;
        ty = tr.y + tr.h * (1.f - norm) - thumbH * 0.5f;
    } else {
        tx = tr.x + tr.w * norm - thumbW * 0.5f;
        ty = tr.y - 4.f;
    }
    ctx.fill.SetColor(thumbColor_.r, thumbColor_.g, thumbColor_.b, 255);
    ctx.fillRect(tx, ty, thumbW, thumbH);

    // Thumb inner line
    ctx.fill.SetColor(30, 34, 40, 180);
    if (vertical_)
        ctx.fillRect(tx + 3.f, ty + 1.f, thumbW - 6.f, 1);
    else
        ctx.fillRect(tx + 1.f, ty + 3.f, 1, thumbH - 6.f);

    // Track border
    ctx.line.SetColor(30, 34, 40, 180);
    ctx.lineRect(tr.x, tr.y, tr.w, tr.h);

    // Value text
    if (showValue_) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", value_);
        float asc = setupFont(ctx, Color(160, 165, 175, 255), 10.f);
        ctx.font.Print(buf, b.x + 2.f, b.y + b.h - labelH - valH + (valH - 10.f) * 0.5f + asc);
    }

    // Label
    if (!label_.empty()) {
        float asc = setupFont(ctx, Color(200, 205, 215, 255), 10.f);
        ctx.font.Print(label_.c_str(), b.x + 2.f, b.y + b.h - labelH + (labelH - 10.f) * 0.5f + asc);
    }

    ctx.popClip();
}

void LinearKnob::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;
    dragging_ = true;
    Rect tr = trackRect();
    float norm = vertical_
        ? 1.f - std::clamp((e.y - tr.y) / tr.h, 0.f, 1.f)
        : std::clamp((e.x - tr.x) / tr.w, 0.f, 1.f);
    setValue(std::clamp(min_ + norm * (max_ - min_), min_, max_));
    dragStart_  = vertical_ ? e.y : e.x;
    dragStartV_ = value_;
    e.consumed  = true;
}

void LinearKnob::onMouseMove(MouseEvent& e)
{
    if (!dragging_) return;
    Rect tr = trackRect();
    float range = vertical_ ? tr.h : tr.w;
    float delta = (vertical_ ? (dragStart_ - e.y) : (e.x - dragStart_)) / range;
    setValue(std::clamp(dragStartV_ + delta * (max_ - min_), min_, max_));
    e.consumed = true;
}

void LinearKnob::onMouseRelease(MouseEvent& e)
{
    if (e.button != 0) return;
    dragging_ = false;
}

void LinearKnob::onMouseScroll(MouseEvent& e)
{
    float step = (max_ - min_) * 0.02f;
    setValue(std::clamp(value_ + e.scrollY * step, min_, max_));
    e.consumed = true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  ADSRWidget
// ═════════════════════════════════════════════════════════════════════════════

ADSRWidget::ADSRWidget() {}

void ADSRWidget::setADSR(float a, float d, float s, float r)
{
    a_ = std::max(0.001f, a);
    d_ = std::max(0.001f, d);
    s_ = std::clamp(s, 0.0f, 1.0f);
    r_ = std::max(0.001f, r);
    markDirty();
}

ADSRWidget::Points ADSRWidget::computePoints(const Rect& area) const
{
    const float pad = 8.f;
    float ax = area.x + pad, aw = area.w - pad * 2;
    float ay = area.y + pad, ah = area.h - pad * 2;
    float bottom = ay + ah, top = ay;

    float sustainW = aw * 0.30f;
    float adsr_sum = a_ + d_ + r_;
    float timeW = aw * 0.70f;
    float scale = (adsr_sum > 0) ? timeW / adsr_sum : 1.f;

    float x0 = ax;
    float x1 = ax + a_ * scale;
    float x2 = x1 + d_ * scale;
    float x3 = x2 + sustainW;
    float x4 = x3 + r_ * scale;

    return {x0, bottom, x1, top, x2, top + (1.f - s_) * ah,
            x3, top + (1.f - s_) * ah, x4, bottom};
}

int ADSRWidget::hitHandle(float mx, float my) const
{
    auto p = computePoints(absoluteRect());
    struct H { float x, y; };
    H handles[] = {{p.x1, p.y1}, {p.x2, p.y2}, {p.x3, p.y3}};
    float r2 = (kHandleR + 2) * (kHandleR + 2);
    for (int i = 0; i < 3; ++i) {
        float dx = mx - handles[i].x, dy = my - handles[i].y;
        if (dx * dx + dy * dy <= r2) return i;
    }
    return -1;
}

void ADSRWidget::paint(PaintContext& ctx)
{
    const Rect b = absoluteRect();
    ctx.pushClip(b);

    ctx.fill.SetColor(bgColor_.r, bgColor_.g, bgColor_.b, 255);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    auto p = computePoints(b);
    float bot = b.y + b.h - 8.f;

    // Fill under envelope (trapezoids)
    ctx.fill.SetColor(fillColor_.r, fillColor_.g, fillColor_.b, fillColor_.a);
    auto fillSeg = [&](float ax, float ay, float bx, float by) {
        ctx.fillTriangle(ax, ay, bx, by, ax, bot);
        ctx.fillTriangle(bx, by, bx, bot, ax, bot);
    };
    fillSeg(p.x0, p.y0, p.x1, p.y1);
    fillSeg(p.x1, p.y1, p.x2, p.y2);
    fillSeg(p.x2, p.y2, p.x3, p.y3);
    fillSeg(p.x3, p.y3, p.x4, p.y4);

    // Envelope lines
    ctx.line.SetColor(lineColor_.r, lineColor_.g, lineColor_.b, lineColor_.a);
    ctx.drawLine(p.x0, p.y0, p.x1, p.y1);
    ctx.drawLine(p.x1, p.y1, p.x2, p.y2);
    ctx.drawLine(p.x2, p.y2, p.x3, p.y3);
    ctx.drawLine(p.x3, p.y3, p.x4, p.y4);

    // Sustain divider (dashed vertical)
    ctx.line.SetColor(lineColor_.r, lineColor_.g, lineColor_.b, 60);
    for (float yy = p.y2; yy < bot; yy += 8.f) {
        float y2 = std::min(yy + 4.f, bot);
        ctx.drawLine(p.x2, yy, p.x2, y2);
    }

    // Handles with ring style
    struct HPos { float x, y; };
    HPos handles[] = {{p.x1, p.y1}, {p.x2, p.y2}, {p.x3, p.y3}};
    for (int i = 0; i < 3; ++i) {
        bool active = (dragging_ == i);
        Color outer = active ? Color(255, 220, 80, 255) : Color(200, 200, 200, 255);
        ctx.fill.SetColor(outer.r, outer.g, outer.b, outer.a);
        ctx.fillCircle(handles[i].x, handles[i].y, kHandleR);
        ctx.fill.SetColor(bgColor_.r, bgColor_.g, bgColor_.b, 255);
        ctx.fillCircle(handles[i].x, handles[i].y, kHandleR - 2.f);
    }

    // Labels
    if (showLabels_) {
        float asc = setupFont(ctx, Color(160, 165, 170, 255), 9.f);
        char buf[32];
        snprintf(buf, sizeof(buf), "A:%.0fms", a_ * 1000.f);
        ctx.font.Print(buf, p.x0 + 2, b.y + 4 + asc);

        snprintf(buf, sizeof(buf), "D:%.0fms", d_ * 1000.f);
        ctx.font.Print(buf, p.x2 + 4, b.y + 4 + asc);

        snprintf(buf, sizeof(buf), "S:%.0f%%", s_ * 100.f);
        ctx.font.Print(buf, p.x2 + 4, p.y2 - 14 + asc);

        snprintf(buf, sizeof(buf), "R:%.0fms", r_ * 1000.f);
        ctx.font.Print(buf, p.x3 + 4, b.y + 4 + asc);
    }

    // Border
    ctx.line.SetColor(55, 60, 66, 255);
    ctx.lineRect(b.x, b.y, b.w, b.h);

    ctx.popClip();
}

void ADSRWidget::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;
    int h = hitHandle(e.x, e.y);
    if (h >= 0) { dragging_ = h; markDirty(); e.consumed = true; }
}

void ADSRWidget::onMouseMove(MouseEvent& e)
{
    if (dragging_ < 0) return;
    const Rect b = absoluteRect();
    const float pad = 8.f;
    float aw = b.w - pad * 2, ah = b.h - pad * 2, ay = b.y + pad;
    float sustainW = aw * 0.30f, timeW = aw * 0.70f;
    float adsr_sum = a_ + d_ + r_;
    float scale = (adsr_sum > 0) ? timeW / adsr_sum : 1.f;
    float ax0 = b.x + pad;
    float x1 = ax0 + a_ * scale;
    float x2 = x1 + d_ * scale;
    float x3 = x2 + sustainW;

    if (dragging_ == 0) {
        float nx = std::clamp(e.x, ax0 + 2, x2 - 2);
        a_ = std::max(0.001f, (nx - ax0) / scale);
    } else if (dragging_ == 1) {
        float nx = std::clamp(e.x, x1 + 2, x3 - 2);
        d_ = std::max(0.001f, (nx - x1) / scale);
        float ny = std::clamp(e.y, ay, ay + ah);
        s_ = std::clamp(1.f - (ny - ay) / ah, 0.f, 1.f);
    } else if (dragging_ == 2) {
        float nx = std::clamp(e.x, x3 + 2, ax0 + aw - 2);
        r_ = std::max(0.001f, (nx - x3) / scale);
    }
    onChanged.emit(a_, d_, s_, r_);
    markDirty();
    e.consumed = true;
}

void ADSRWidget::onMouseRelease(MouseEvent&) { dragging_ = -1; }

// ═════════════════════════════════════════════════════════════════════════════
//  VUMeter
// ═════════════════════════════════════════════════════════════════════════════

VUMeter::VUMeter() { setChannelCount(2); }

void VUMeter::setChannelCount(int n)
{
    levels_.resize(std::max(1, n));
    markDirty();
}

void VUMeter::setLevel(int ch, float level)
{
    if (ch < 0 || ch >= static_cast<int>(levels_.size())) return;
    auto& c = levels_[ch];
    c.level = std::clamp(level, 0.f, 1.f);
    if (c.level >= c.peak) { c.peak = c.level; c.peakTimer = 1.5f; }
    markDirty();
}

float VUMeter::level(int ch) const
{
    if (ch < 0 || ch >= static_cast<int>(levels_.size())) return 0.f;
    return levels_[ch].level;
}

void VUMeter::update(float dt)
{
    bool dirty = false;
    for (auto& c : levels_) {
        if (c.peakTimer > 0.f) { c.peakTimer -= dt; dirty = true; }
        if (c.peakTimer <= 0.f && c.peak > 0.f) {
            c.peak -= peakDecay_ * dt;
            if (c.peak < 0.f) c.peak = 0.f;
            dirty = true;
        }
    }
    if (dirty) markDirty();
}

Widget::Vec2f VUMeter::sizeHint() const
{
    int n = std::max(1, static_cast<int>(levels_.size()));
    int scaleW = showScale_ ? 28 : 0;
    if (orient_ == Orientation::Vertical) {
        int barW = 18;
        return {float(scaleW + n * barW + (n - 1) * barSpacing_ + 8), 120.f};
    }
    int barH = 18;
    return {180.f, float(n * barH + (n - 1) * barSpacing_ + (showScale_ ? 16 : 0) + 8)};
}

void VUMeter::paintChannel(PaintContext& ctx, const Rect& bar, const Channel& ch) const
{
    const bool vert = (orient_ == Orientation::Vertical);

    ctx.fill.SetColor(30, 32, 36, 255);
    ctx.fillRect(bar.x, bar.y, bar.w, bar.h);

    const int segs = vert ? static_cast<int>(bar.h) / 3 : static_cast<int>(bar.w) / 3;
    if (segs < 1) return;

    for (int i = 0; i < segs; ++i) {
        float t = vert
            ? float(segs - 1 - i) / (segs - 1)
            : float(i)            / (segs - 1);

        Color c = levelColor(t);
        if (t > ch.level)
            ctx.fill.SetColor(c.r / 4, c.g / 4, c.b / 4, 255);
        else
            ctx.fill.SetColor(c.r, c.g, c.b, 255);

        if (vert) {
            float segH = bar.h / segs;
            ctx.fillRect(bar.x, bar.y + i * segH + 0.5f, bar.w, segH - 1.f);
        } else {
            float segW = bar.w / segs;
            ctx.fillRect(bar.x + i * segW + 0.5f, bar.y, segW - 1.f, bar.h);
        }
    }

    // Peak hold
    if (peakHold_ && ch.peak > 0.f) {
        Color c = levelColor(ch.peak);
        ctx.fill.SetColor(c.r, c.g, c.b, 255);
        if (vert)
            ctx.fillRect(bar.x, bar.y + bar.h * (1.f - ch.peak), bar.w, 2);
        else
            ctx.fillRect(bar.x + bar.w * ch.peak, bar.y, 2, bar.h);
    }

    // Border
    ctx.line.SetColor(60, 60, 65, 255);
    ctx.lineRect(bar.x, bar.y, bar.w, bar.h);
}

void VUMeter::paint(PaintContext& ctx)
{
    const Rect b = absoluteRect();
    ctx.pushClip(b);

    ctx.fill.SetColor(24, 26, 30, 255);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    const bool vert = (orient_ == Orientation::Vertical);
    const int  n    = static_cast<int>(levels_.size());
    if (n == 0) { ctx.popClip(); return; }

    const int scaleW = (showScale_ && vert)  ? 28 : 0;
    const int scaleH = (showScale_ && !vert) ? 16 : 0;
    const float areaX = b.x + scaleW + 4;
    const float areaY = b.y + 4;
    const float areaW = b.w - scaleW - 8;
    const float areaH = b.h - scaleH - 8;

    // dB scale labels
    if (showScale_) {
        float asc = setupFont(ctx, Color(140, 140, 140, 255), 9.f);
        float marks[] = {0.f, -6.f, -12.f, -18.f, -24.f, -36.f, -48.f};
        float lastPos = -9999.f;
        char tmp[8];
        for (float db : marks) {
            float lin = fromDB(db);
            if (vert) {
                float y = areaY + (1.f - lin) * areaH;
                if (y - lastPos < 12.f) continue;
                lastPos = y;
                snprintf(tmp, sizeof(tmp), "%d", static_cast<int>(db));
                ctx.font.Print(tmp, b.x + 2, y - 6 + asc);
            } else {
                float x = areaX + lin * areaW;
                if (x - lastPos < 24.f) continue;
                lastPos = x;
                snprintf(tmp, sizeof(tmp), "%d", static_cast<int>(db));
                ctx.font.Print(tmp, x - 4, b.y + areaH + 4 + asc);
            }
        }
    }

    // Bar layout
    float totalSpace = vert ? areaW : areaH;
    float barSize = (totalSpace - (n - 1) * barSpacing_) / n;
    barSize = std::max(4.f, barSize);

    for (int i = 0; i < n; ++i) {
        Rect bar;
        if (vert) {
            bar = {areaX + i * (barSize + barSpacing_), areaY, barSize, areaH};
        } else {
            bar = {areaX, areaY + i * (barSize + barSpacing_), areaW, barSize};
        }
        paintChannel(ctx, bar, levels_[i]);
    }

    ctx.popClip();
}

// ═════════════════════════════════════════════════════════════════════════════
//  SpectrumAnalyzer
// ═════════════════════════════════════════════════════════════════════════════

SpectrumAnalyzer::SpectrumAnalyzer() { setBinCount(32); }

void SpectrumAnalyzer::setBinCount(int n)
{
    bins_.resize(std::max(1, n));
    markDirty();
}

void SpectrumAnalyzer::setMagnitudes(const float* magnitudes, int count)
{
    int n = std::min(count, static_cast<int>(bins_.size()));
    for (int i = 0; i < n; ++i) {
        float v = std::clamp(magnitudes[i], 0.f, 1.f);
        auto& bin = bins_[i];
        bin.value = bin.value * smoothing_ + v * (1.f - smoothing_);
        if (bin.value >= bin.peak) { bin.peak = bin.value; bin.peakTimer = 1.2f; }
    }
    markDirty();
}

void SpectrumAnalyzer::setFreqLabels(const float* freqs, int count)
{
    freqLabels_.assign(freqs, freqs + count);
    markDirty();
}

void SpectrumAnalyzer::update(float dt)
{
    bool dirty = false;
    for (auto& bin : bins_) {
        if (bin.peakTimer > 0.f) { bin.peakTimer -= dt; dirty = true; }
        if (bin.peakTimer <= 0.f && bin.peak > 0.f) {
            bin.peak -= peakDecay_ * dt;
            if (bin.peak < 0.f) bin.peak = 0.f;
            dirty = true;
        }
    }
    if (dirty) markDirty();
}

void SpectrumAnalyzer::paint(PaintContext& ctx)
{
    const Rect b = absoluteRect();
    ctx.pushClip(b);

    ctx.fill.SetColor(bgColor_.r, bgColor_.g, bgColor_.b, 255);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    const int   n      = static_cast<int>(bins_.size());
    const float labelH = (showLabels_ && !freqLabels_.empty()) ? 14.f : 0.f;
    const float areaH  = b.h - labelH - 4.f;
    const float areaY  = b.y + 2.f;
    const float areaX  = b.x + 2.f;
    const float areaW  = b.w - 4.f;

    if (n == 0 || areaH <= 0 || areaW <= 0) { ctx.popClip(); return; }

    // Grid lines at 25%, 50%, 75%
    ctx.line.SetColor(40, 44, 50, 255);
    for (float frac : {0.25f, 0.5f, 0.75f}) {
        float gy = areaY + areaH * (1.f - frac);
        ctx.drawLine(areaX, gy, areaX + areaW, gy);
    }

    float totalSpacing = barSpacing_ * (n - 1);
    float barW = std::max(1.f, (areaW - totalSpacing) / n);

    for (int i = 0; i < n; ++i) {
        float xi = areaX + i * (barW + barSpacing_);
        float v  = bins_[i].value;
        float barH = v * areaH;

        // Dim background
        Color dim = lerpColor(colorLow_, colorHigh_, 0.5f);
        ctx.fill.SetColor(dim.r / 5, dim.g / 5, dim.b / 5, 255);
        ctx.fillRect(xi, areaY, barW, areaH - barH);

        // Lit bar
        if (barH > 0.5f) {
            Color c = lerpColor(colorLow_, colorHigh_, v);
            ctx.fill.SetColor(c.r, c.g, c.b, 255);
            ctx.fillRect(xi, areaY + areaH - barH, barW, barH);
        }

        // Peak hold
        if (peakHold_ && bins_[i].peak > 0.f) {
            Color pc = lerpColor(colorLow_, colorHigh_, bins_[i].peak);
            ctx.fill.SetColor(pc.r, pc.g, pc.b, 255);
            ctx.fillRect(xi, areaY + areaH - bins_[i].peak * areaH, barW, 2);
        }
    }

    // Frequency labels
    if (showLabels_ && !freqLabels_.empty()) {
        float asc = setupFont(ctx, Color(130, 130, 130, 255), 9.f);
        int step = std::max(1, n / 8);
        char tmp[12];
        for (int i = 0; i < n; i += step) {
            float xi = areaX + i * (barW + barSpacing_);
            float hz = (i < static_cast<int>(freqLabels_.size()))
                       ? freqLabels_[i] : (sampleRate_ * 0.5f * i / n);
            if (hz >= 1000.f)
                snprintf(tmp, sizeof(tmp), "%.0fk", hz / 1000.f);
            else
                snprintf(tmp, sizeof(tmp), "%.0f", hz);
            ctx.font.Print(tmp, xi, b.y + b.h - labelH + asc);
        }
    }

    // Border
    ctx.line.SetColor(55, 58, 65, 255);
    ctx.lineRect(b.x, b.y, b.w, b.h);

    ctx.popClip();
}

// ═════════════════════════════════════════════════════════════════════════════
//  WaveformView
// ═════════════════════════════════════════════════════════════════════════════

WaveformView::WaveformView() {}

void WaveformView::setSamples(const float* data, int count, int channels, float sampleRate)
{
    if (!data || count <= 0) { clearSamples(); return; }
    channels_    = std::max(1, channels);
    sampleCount_ = count / channels_;
    sampleRate_  = sampleRate;
    samples_.assign(data, data + count);
    markDirty();
}

void WaveformView::clearSamples() { samples_.clear(); sampleCount_ = 0; channels_ = 1; markDirty(); }

void WaveformView::setLoopRegion(float start, float end)
{
    loopStart_ = std::clamp(start, 0.f, 1.f);
    loopEnd_   = std::clamp(end, 0.f, 1.f);
    hasLoop_   = true;
    markDirty();
}

void WaveformView::setViewRange(float start, float end)
{
    viewStart_ = std::clamp(start, 0.f, 1.f);
    viewEnd_   = std::clamp(end, viewStart_ + 0.001f, 1.f);
    markDirty();
}

ct::Vector<WaveformView::EnvPair> WaveformView::buildEnvelope(int ch, float vs, float ve, int pw) const
{
    ct::Vector<EnvPair> env(pw, {0.f, 0.f});
    if (sampleCount_ <= 0 || pw <= 0) return env;
    for (int px = 0; px < pw; ++px) {
        float t0 = vs + (ve - vs) * (float(px)     / pw);
        float t1 = vs + (ve - vs) * (float(px + 1) / pw);
        int s0 = std::clamp(int(t0 * sampleCount_), 0, sampleCount_ - 1);
        int s1 = std::clamp(int(t1 * sampleCount_), s0, sampleCount_ - 1);
        float mn = 0.f, mx = 0.f;
        bool first = true;
        for (int si = s0; si <= s1; ++si) {
            int idx = si * channels_ + ch;
            if (idx >= static_cast<int>(samples_.size())) break;
            float v = samples_[idx];
            if (first) { mn = mx = v; first = false; }
            else { mn = std::min(mn, v); mx = std::max(mx, v); }
        }
        env[px] = {mn, mx};
    }
    return env;
}

void WaveformView::paint(PaintContext& ctx)
{
    const Rect b = absoluteRect();
    ctx.pushClip(b);

    ctx.fill.SetColor(bgColor_.r, bgColor_.g, bgColor_.b, 255);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    const int nch = std::max(1, channels_);
    float chH = b.h / nch;

    for (int ch = 0; ch < nch; ++ch) {
        float chY0 = b.y + ch * chH;
        float mid  = chY0 + chH * 0.5f;

        if (showCentre_) {
            ctx.line.SetColor(50, 55, 60, 255);
            ctx.drawLine(b.x, mid, b.x + b.w, mid);
        }

        if (sampleCount_ > 0) {
            // Loop region highlight
            if (hasLoop_) {
                float lx0 = b.x + (loopStart_ - viewStart_) / (viewEnd_ - viewStart_) * b.w;
                float lx1 = b.x + (loopEnd_   - viewStart_) / (viewEnd_ - viewStart_) * b.w;
                ctx.fill.SetColor(waveColor_.r, waveColor_.g, waveColor_.b, 40);
                ctx.fillRect(lx0, chY0, lx1 - lx0, chH);
            }

            // Waveform envelope via vertical lines
            const int maxCols = 512;
            int pw = std::min(maxCols, static_cast<int>(b.w));
            float colW = b.w / pw;
            auto env = buildEnvelope(ch, viewStart_, viewEnd_, pw);
            ctx.line.SetColor(waveColor_.r, waveColor_.g, waveColor_.b, 220);
            for (int px = 0; px < pw; ++px) {
                float top    = mid - env[px].mx * chH * 0.5f;
                float bottom = mid - env[px].mn * chH * 0.5f;
                if (bottom <= top) bottom = top + 1.f;
                ctx.drawLine(b.x + (px + 0.5f) * colW, top,
                             b.x + (px + 0.5f) * colW, bottom);
            }
        } else {
            ctx.fill.SetColor(50, 55, 60, 255);
            ctx.fillRect(b.x, mid, b.w, 1);
        }

        if (ch > 0) {
            ctx.line.SetColor(40, 44, 50, 255);
            ctx.drawLine(b.x, chY0, b.x + b.w, chY0);
        }
    }

    // Playhead
    float t = (playhead_ - viewStart_) / (viewEnd_ - viewStart_);
    if (t >= 0.f && t <= 1.f) {
        ctx.fill.SetColor(playheadColor_.r, playheadColor_.g, playheadColor_.b, 255);
        ctx.fillRect(b.x + t * b.w, b.y, 2, b.h);
    }

    // Border
    ctx.line.SetColor(60, 65, 70, 255);
    ctx.lineRect(b.x, b.y, b.w, b.h);

    ctx.popClip();
}

void WaveformView::onMousePress(MouseEvent& e)
{
    const Rect b = absoluteRect();
    if (e.button == 1) {
        float t = std::clamp(viewStart_ + (e.x - b.x) / b.w * (viewEnd_ - viewStart_), 0.f, 1.f);
        playhead_ = t;
        dragging_ = true;
        onScrub.emit(t);
        markDirty();
        e.consumed = true;
    } else if (e.button == 2) {
        panning_ = true;
        panStartX_ = e.x - b.x;
        panVS_ = viewStart_; panVE_ = viewEnd_;
        e.consumed = true;
    }
}

void WaveformView::onMouseMove(MouseEvent& e)
{
    const Rect b = absoluteRect();
    if (dragging_) {
        float t = std::clamp(viewStart_ + (e.x - b.x) / b.w * (viewEnd_ - viewStart_), 0.f, 1.f);
        playhead_ = t;
        onScrub.emit(t);
        markDirty();
    } else if (panning_) {
        float dx   = (e.x - b.x) - panStartX_;
        float dtpp = (panVE_ - panVS_) / b.w;
        float shift = -dx * dtpp;
        float span = panVE_ - panVS_;
        float ns = panVS_ + shift, ne = panVE_ + shift;
        if (ns < 0.f) { ns = 0.f; ne = span; }
        if (ne > 1.f) { ne = 1.f; ns = 1.f - span; }
        viewStart_ = ns; viewEnd_ = ne;
        markDirty();
    }
}

void WaveformView::onMouseRelease(MouseEvent&) { dragging_ = false; panning_ = false; }

void WaveformView::onMouseScroll(MouseEvent& e)
{
    const Rect b = absoluteRect();
    float t    = viewStart_ + (e.x - b.x) / b.w * (viewEnd_ - viewStart_);
    float span = std::clamp((viewEnd_ - viewStart_) * (e.scrollY > 0 ? 0.85f : 1.f / 0.85f), 0.001f, 1.f);
    float ns = t - (e.x - b.x) / b.w * span, ne = ns + span;
    if (ns < 0.f) { ns = 0.f; ne = span; }
    if (ne > 1.f) { ne = 1.f; ns = 1.f - span; }
    viewStart_ = ns; viewEnd_ = ne;
    markDirty();
    e.consumed = true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  PianoRoll
// ═════════════════════════════════════════════════════════════════════════════

PianoRoll::PianoRoll() { setPitchRange(48, 72); }

void PianoRoll::addNote(int pitch, float start, float length)
{
    notes_.push_back({pitch, start, std::max(0.01f, length)});
    markDirty();
}

void PianoRoll::removeNote(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(notes_.size())) return;
    notes_.erase(notes_.begin() + idx);
    if (draggingNote_ == idx) draggingNote_ = -1;
    markDirty();
}

void PianoRoll::clearNotes() { notes_.clear(); draggingNote_ = -1; markDirty(); }

void PianoRoll::setPitchRange(int lo, int hi)
{
    pitchLo_ = std::clamp(lo, 0, 127);
    pitchHi_ = std::clamp(hi, pitchLo_ + 1, 127);
    markDirty();
}

void PianoRoll::setTimeRange(float s, float e)
{
    viewStart_ = s;
    viewEnd_   = std::max(viewStart_ + 0.01f, e);
    markDirty();
}

void PianoRoll::layout() { Widget::layout(); }

Rect PianoRoll::keyArea() const
{
    const Rect b = absoluteRect();
    return {b.x, b.y, keyW_, b.h};
}

Rect PianoRoll::gridArea() const
{
    const Rect b = absoluteRect();
    return {b.x + keyW_, b.y, b.w - keyW_, b.h};
}

float PianoRoll::pitchToY(int pitch) const
{
    const Rect ga = gridArea();
    return ga.y + (pitchHi_ - 1 - pitch) * rowH_;
}

int PianoRoll::yToPitch(float y) const
{
    const Rect ga = gridArea();
    int row = static_cast<int>((y - ga.y) / rowH_);
    return std::clamp(pitchHi_ - 1 - row, pitchLo_, pitchHi_ - 1);
}

float PianoRoll::beatToX(float beat) const
{
    const Rect ga = gridArea();
    return ga.x + (beat - viewStart_) / (viewEnd_ - viewStart_) * ga.w;
}

float PianoRoll::xToBeat(float x) const
{
    const Rect ga = gridArea();
    return viewStart_ + (x - ga.x) / ga.w * (viewEnd_ - viewStart_);
}

Rect PianoRoll::noteRect(const PianoNote& n) const
{
    float x = beatToX(n.start);
    float x2 = beatToX(n.start + n.length);
    float y = pitchToY(n.pitch);
    return {x, y + 1, std::max(4.f, x2 - x - 1), rowH_ - 2};
}

bool PianoRoll::isBlackKey(int pitch) const
{
    int pc = pitch % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

void PianoRoll::paintKeys(PaintContext& ctx, const Rect& ka) const
{
    float asc = setupFont(ctx, Color(40, 40, 45, 255), 9.f);
    for (int p = pitchLo_; p < pitchHi_; ++p) {
        float y = pitchToY(p);
        bool black = isBlackKey(p);
        ctx.fill.SetColor(black ? 30 : 210, black ? 32 : 210, black ? 36 : 215, 255);
        ctx.fillRect(ka.x, y, ka.w, rowH_);
        ctx.fill.SetColor(90, 90, 95, 255);
        ctx.fillRect(ka.x, y + rowH_ - 1, ka.w, 1);

        if (p % 12 == 0 && showOctave_) {
            char buf[8];
            snprintf(buf, sizeof(buf), "C%d", p / 12 - 1);
            ctx.font.SetColor(black ? Color(180,180,185,255) : Color(40,40,45,255));
            ctx.font.Print(buf, ka.x + 2, y + (rowH_ - 9.f) * 0.5f + asc);
        }
    }
    ctx.fill.SetColor(60, 62, 66, 255);
    ctx.fillRect(ka.x + ka.w - 1, ka.y, 1, ka.h);
}

void PianoRoll::paintGrid(PaintContext& ctx, const Rect& ga) const
{
    for (int p = pitchLo_; p < pitchHi_; ++p) {
        float y = pitchToY(p);
        ctx.fill.SetColor(isBlackKey(p) ? 22 : 30, isBlackKey(p) ? 24 : 32, isBlackKey(p) ? 28 : 36, 255);
        ctx.fillRect(ga.x, y, ga.w, rowH_);
        ctx.fill.SetColor(40, 42, 46, 255);
        ctx.fillRect(ga.x, y + rowH_ - 1, ga.w, 1);
    }

    // Beat/bar grid
    float firstBeat = std::floor(viewStart_);
    for (float beat = firstBeat; beat <= viewEnd_; beat += 1.f) {
        float x = beatToX(beat);
        if (x < ga.x || x > ga.x + ga.w) continue;
        bool isBar = (std::fmod(beat, float(bpb_)) < 0.001f);
        ctx.fill.SetColor(isBar ? 70 : 45, isBar ? 74 : 48, isBar ? 80 : 54, 255);
        ctx.fillRect(x, ga.y, 1, ga.h);
    }

    // Bar numbers
    float asc = setupFont(ctx, Color(100, 104, 110, 255), 9.f);
    for (float beat = firstBeat; beat <= viewEnd_; beat += float(bpb_)) {
        float x = beatToX(beat);
        if (x < ga.x || x > ga.x + ga.w) continue;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", static_cast<int>(beat / bpb_) + 1);
        ctx.font.Print(buf, x + 2, ga.y + 1 + asc);
    }
}

void PianoRoll::paintNotes(PaintContext& ctx, const Rect& ga) const
{
    for (int i = 0; i < static_cast<int>(notes_.size()); ++i) {
        const auto& n = notes_[i];
        if (n.pitch < pitchLo_ || n.pitch >= pitchHi_) continue;
        Rect nr = noteRect(n);
        if (nr.x + nr.w < ga.x || nr.x > ga.x + ga.w) continue;

        bool active = (i == draggingNote_);
        Color c = active ? noteSelColor_ : noteColor_;
        ctx.fill.SetColor(c.r, c.g, c.b, c.a);
        ctx.fillRect(nr.x, nr.y, nr.w, nr.h);

        // Top highlight
        ctx.fill.SetColor(
            static_cast<uint8_t>(std::min(255, c.r + 60)),
            static_cast<uint8_t>(std::min(255, c.g + 60)),
            static_cast<uint8_t>(std::min(255, c.b + 60)), 200);
        ctx.fillRect(nr.x, nr.y, nr.w, 2);

        // Border
        ctx.fill.SetColor(c.r / 2, c.g / 2, c.b / 2, 255);
        ctx.fillRect(nr.x,        nr.y,         nr.w, 1);
        ctx.fillRect(nr.x,        nr.y+nr.h-1,  nr.w, 1);
        ctx.fillRect(nr.x,        nr.y,         1,    nr.h);
        ctx.fillRect(nr.x+nr.w-1, nr.y,         1,    nr.h);
    }
}

void PianoRoll::paintPlayhead(PaintContext& ctx, const Rect& ga) const
{
    float x = beatToX(playhead_);
    if (x < ga.x || x > ga.x + ga.w) return;
    ctx.fill.SetColor(255, 200, 80, 220);
    ctx.fillRect(x, ga.y, 2, ga.h);
}

void PianoRoll::paint(PaintContext& ctx)
{
    const Rect b  = absoluteRect();
    const Rect ka = keyArea();
    const Rect ga = gridArea();

    ctx.pushClip(b);
    ctx.fill.SetColor(24, 26, 30, 255);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    ctx.pushClip(ga);
    paintGrid(ctx, ga);
    paintNotes(ctx, ga);
    paintPlayhead(ctx, ga);
    ctx.popClip();

    ctx.pushClip(ka);
    paintKeys(ctx, ka);
    ctx.popClip();

    // Border
    ctx.fill.SetColor(55, 58, 65, 255);
    ctx.fillRect(b.x,       b.y,       b.w, 1);
    ctx.fillRect(b.x,       b.y+b.h-1, b.w, 1);
    ctx.fillRect(b.x,       b.y,       1,   b.h);
    ctx.fillRect(b.x+b.w-1, b.y,       1,   b.h);

    ctx.popClip();
}

void PianoRoll::onMousePress(MouseEvent& e)
{
    const Rect ga = gridArea();
    if (e.x < ga.x || e.x > ga.x + ga.w || e.y < ga.y || e.y > ga.y + ga.h) return;

    if (e.button == 2) {
        for (int i = static_cast<int>(notes_.size()) - 1; i >= 0; --i) {
            Rect nr = noteRect(notes_[i]);
            if (e.x >= nr.x && e.x <= nr.x+nr.w && e.y >= nr.y && e.y <= nr.y+nr.h) {
                onNoteRemoved.emit(i);
                removeNote(i);
                e.consumed = true;
                return;
            }
        }
        return;
    }

    if (e.button == 1) {
        for (int i = static_cast<int>(notes_.size()) - 1; i >= 0; --i) {
            Rect nr = noteRect(notes_[i]);
            if (e.x >= nr.x && e.x <= nr.x+nr.w && e.y >= nr.y && e.y <= nr.y+nr.h) {
                draggingNote_ = i;
                dragOffBeat_  = xToBeat(e.x) - notes_[i].start;
                dragOffPitch_ = yToPitch(e.y) - notes_[i].pitch;
                onNoteClicked.emit(i);
                e.consumed = true;
                return;
            }
        }
        int pitch = yToPitch(e.y);
        float beat = std::floor(xToBeat(e.x) * 4.f) / 4.f;
        newNotePitch_ = pitch;
        newNoteStart_ = beat;
        addNote(pitch, beat, 0.25f);
        draggingNote_ = static_cast<int>(notes_.size()) - 1;
        dragOffBeat_ = 0.f;
        dragOffPitch_ = 0;
        e.consumed = true;
    }

    if (e.button == 3) {
        panning_ = true;
        panStartX_ = e.x;
        panVS_ = viewStart_;
        panVE_ = viewEnd_;
        e.consumed = true;
    }
}

void PianoRoll::onMouseMove(MouseEvent& e)
{
    if (panning_) {
        const Rect ga = gridArea();
        float dx = e.x - panStartX_;
        float dtpp = (panVE_ - panVS_) / ga.w;
        viewStart_ = panVS_ - dx * dtpp;
        viewEnd_   = panVE_ - dx * dtpp;
        markDirty();
        return;
    }
    if (draggingNote_ >= 0 && draggingNote_ < static_cast<int>(notes_.size())) {
        float beat = std::max(0.f, std::floor((xToBeat(e.x) - dragOffBeat_) * 4.f) / 4.f);
        int pitch  = std::clamp(yToPitch(e.y) - dragOffPitch_, pitchLo_, pitchHi_ - 1);
        notes_[draggingNote_].start = beat;
        notes_[draggingNote_].pitch = pitch;
        markDirty();
        e.consumed = true;
    }
}

void PianoRoll::onMouseRelease(MouseEvent&)
{
    if (draggingNote_ >= 0)
        onNoteAdded.emit(notes_[draggingNote_].pitch,
                         notes_[draggingNote_].start,
                         notes_[draggingNote_].length);
    draggingNote_ = -1;
    newNotePitch_ = -1;
    panning_ = false;
}

void PianoRoll::onMouseScroll(MouseEvent& e)
{
    const Rect ga = gridArea();
    float t = viewStart_ + (e.x - ga.x) / ga.w * (viewEnd_ - viewStart_);
    float span = std::clamp((viewEnd_ - viewStart_) * (e.scrollY > 0 ? 0.8f : 1.f / 0.8f), 0.25f, 64.f);
    float ns = t - (e.x - ga.x) / ga.w * span;
    viewStart_ = ns;
    viewEnd_   = ns + span;
    markDirty();
    e.consumed = true;
}
