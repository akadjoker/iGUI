#include "pch.hpp"
#include "AutomotiveWidgets.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float degToRad(float deg) { return deg * static_cast<float>(M_PI) / 180.0f; }

// ═════════════════════════════════════════════════════════════════════════════
//  RadialGauge
// ═════════════════════════════════════════════════════════════════════════════

RadialGauge::RadialGauge(float minVal, float maxVal, float value)
    : min_(minVal), max_(maxVal), value_(value) {}

void RadialGauge::setValue(float v)
{
    v = std::max(min_, std::min(max_, v));
    if (v == value_) return;
    value_ = v;
    markDirty();
    onValueChanged.emit(value_);
}

void RadialGauge::setRange(float minVal, float maxVal)
{
    min_ = minVal; max_ = maxVal;
    setValue(value_);
}

Widget::Vec2f RadialGauge::sizeHint() const { return {240.0f, 240.0f}; }

void RadialGauge::drawArc(PaintContext& ctx, float cx, float cy, float r,
                          float startRad, float endRad, int segs)
{
    for (int i = 0; i < segs; ++i) {
        float t0 = static_cast<float>(i)     / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float a0 = startRad + t0 * (endRad - startRad);
        float a1 = startRad + t1 * (endRad - startRad);
        ctx.drawLine(cx + r * std::cos(a0), cy + r * std::sin(a0),
                     cx + r * std::cos(a1), cy + r * std::sin(a1));
    }
}

void RadialGauge::drawThickArc(PaintContext& ctx, float cx, float cy, float r,
                               float width, float startRad, float endRad, int segs)
{
    if (filledArc_) {
        // Filled arc using triangle fan — much better looking
        float rInner = r - width * 0.5f;
        float rOuter = r + width * 0.5f;
        for (int i = 0; i < segs; ++i) {
            float t0 = static_cast<float>(i)     / segs;
            float t1 = static_cast<float>(i + 1) / segs;
            float a0 = startRad + t0 * (endRad - startRad);
            float a1 = startRad + t1 * (endRad - startRad);
            float cos0 = std::cos(a0), sin0 = std::sin(a0);
            float cos1 = std::cos(a1), sin1 = std::sin(a1);
            // Two triangles per segment (quad)
            ctx.fillTriangle(cx + rInner * cos0, cy + rInner * sin0,
                             cx + rOuter * cos0, cy + rOuter * sin0,
                             cx + rOuter * cos1, cy + rOuter * sin1);
            ctx.fillTriangle(cx + rInner * cos0, cy + rInner * sin0,
                             cx + rOuter * cos1, cy + rOuter * sin1,
                             cx + rInner * cos1, cy + rInner * sin1);
        }
    } else {
        // Line-based fallback
        int layers = std::max(1, static_cast<int>(width));
        float half = width * 0.5f;
        for (int l = 0; l < layers; ++l) {
            float offset = -half + (width * l) / std::max(1, layers - 1);
            drawArc(ctx, cx, cy, r + offset, startRad, endRad, segs);
        }
    }
}

void RadialGauge::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    float cx = abs.x + abs.w * 0.5f;
    float cy = abs.y + abs.h * 0.5f;
    float radius = std::min(abs.w, abs.h) * 0.5f - 8.0f;

    float startRad = degToRad(startAngle_);
    float endRad   = degToRad(startAngle_ + sweepAngle_);
    float norm     = (max_ > min_) ? (value_ - min_) / (max_ - min_) : 0.0f;
    float valueRad = startRad + norm * (endRad - startRad);
    constexpr int kSegs = 64;

    // ── Background arc ──────────────────────────────────────────────────
    if (filledArc_) {
        ctx.fill.SetColor(arcBgColor_.r, arcBgColor_.g, arcBgColor_.b, arcBgColor_.a);
    } else {
        ctx.line.SetColor(arcBgColor_.r, arcBgColor_.g, arcBgColor_.b, arcBgColor_.a);
    }
    drawThickArc(ctx, cx, cy, radius, arcWidth_, startRad, endRad, kSegs);

    // ── Glow (wider, low alpha) ─────────────────────────────────────────
    if (showGlow_ && norm > 0.01f) {
        if (filledArc_) {
            ctx.fill.SetColor(glowColor_.r, glowColor_.g, glowColor_.b, glowColor_.a);
        } else {
            ctx.line.SetColor(glowColor_.r, glowColor_.g, glowColor_.b, glowColor_.a);
        }
        drawThickArc(ctx, cx, cy, radius, arcWidth_ + 6.0f, startRad, valueRad,
                     static_cast<int>(norm * kSegs) + 1);
    }

    // ── Value arc ───────────────────────────────────────────────────────
    if (norm > 0.005f) {
        if (filledArc_) {
            ctx.fill.SetColor(arcColor_.r, arcColor_.g, arcColor_.b, arcColor_.a);
        } else {
            ctx.line.SetColor(arcColor_.r, arcColor_.g, arcColor_.b, arcColor_.a);
        }
        drawThickArc(ctx, cx, cy, radius, arcWidth_, startRad, valueRad,
                     static_cast<int>(norm * kSegs) + 1);
    }

    // ── Needle ──────────────────────────────────────────────────────────
    if (showNeedle_) {
        float needleR   = radius * needleLen_;
        float needleAng = valueRad;
        float tipX  = cx + needleR * std::cos(needleAng);
        float tipY  = cy + needleR * std::sin(needleAng);

        // Needle is a thin triangle from center to tip
        float halfW = needleWidth_;
        float perpAng = needleAng + static_cast<float>(M_PI) * 0.5f;
        float px = halfW * std::cos(perpAng);
        float py = halfW * std::sin(perpAng);

        // Main needle triangle
        ctx.fill.SetColor(needleColor_.r, needleColor_.g, needleColor_.b, needleColor_.a);
        ctx.fillTriangle(cx + px, cy + py,
                         cx - px, cy - py,
                         tipX, tipY);

        // Small tail (opposite direction)
        float tailR = radius * 0.12f;
        float tailX = cx - tailR * std::cos(needleAng);
        float tailY = cy - tailR * std::sin(needleAng);
        float tailPx = halfW * 1.3f * std::cos(perpAng);
        float tailPy = halfW * 1.3f * std::sin(perpAng);
        ctx.fillTriangle(cx + tailPx, cy + tailPy,
                         cx - tailPx, cy - tailPy,
                         tailX, tailY);

        // Center hub circle
        ctx.fill.SetColor(needleColor_.r, needleColor_.g, needleColor_.b, 255);
        ctx.fillCircle(cx, cy, needleWidth_ * 2.2f);
        // Inner hub (dark)
        ctx.fill.SetColor(30, 30, 32, 255);
        ctx.fillCircle(cx, cy, needleWidth_ * 1.2f);
    }

    // ── Tick marks ──────────────────────────────────────────────────────
    if (showTicks_ && tickInterval_ > 0) {
        float range = max_ - min_;
        int numTicks = static_cast<int>(range / tickInterval_) + 1;
        for (int i = 0; i < numTicks; ++i) {
            float tv = min_ + i * tickInterval_;
            float tn = (tv - min_) / range;
            float a  = startRad + tn * (endRad - startRad);
            float r0 = radius + arcWidth_ * 0.5f + 2;
            float r1 = r0 + 6;
            bool isMajor = (static_cast<int>(tv) % static_cast<int>(tickInterval_ * 2 + 0.5f) == 0)
                           || i == 0 || i == numTicks - 1;
            if (isMajor) r1 += 3;

            ctx.line.SetColor(120, 120, 125, isMajor ? 200 : 120);
            ctx.drawLine(cx + r0 * std::cos(a), cy + r0 * std::sin(a),
                         cx + r1 * std::cos(a), cy + r1 * std::sin(a));
        }
    }

    // ── Unit text (above center) ────────────────────────────────────────
    ctx.font.SetFontSize(13.0f);
    ctx.font.SetBatch(&ctx.text);
    if (!unit_.empty()) {
        ctx.font.SetColor(Color(160, 160, 165, 255));
        float tw = ctx.font.GetTextWidth(unit_.c_str());
        float asc = ctx.font.GetAscender();
        ctx.font.Print(unit_.c_str(), cx - tw * 0.5f, cy - radius * 0.22f + asc);
    }

    // ── Big value text (center) ─────────────────────────────────────────
    {
        char buf[32];
        if (decimals_ == 0)
            std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(value_));
        else
            std::snprintf(buf, sizeof(buf), "%.*f", decimals_, static_cast<double>(value_));

        // Draw big — we'll use multiple prints to simulate larger font
        // since we only have one font size. Scale with the widget
        ctx.font.SetColor(valueColor_);

        // Use large pseudo-font by rendering at base size
        ctx.font.SetFontSize(14.0f);
        float tw = ctx.font.GetTextWidth(buf);
        float asc = ctx.font.GetAscender();
        // Center the value
        ctx.font.Print(buf, cx - tw * 0.5f, cy + asc * 0.4f);
    }

    // ── Label text (below center) ───────────────────────────────────────
    if (!label_.empty()) {
        ctx.font.SetFontSize(12.0f);
        ctx.font.SetColor(Color(130, 130, 135, 255));
        float tw = ctx.font.GetTextWidth(label_.c_str());
        float asc = ctx.font.GetAscender();
        ctx.font.Print(label_.c_str(), cx - tw * 0.5f, cy + radius * 0.35f + asc);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  PowerBar
// ═════════════════════════════════════════════════════════════════════════════

PowerBar::PowerBar(float minVal, float maxVal, float value)
    : min_(minVal), max_(maxVal), value_(value) {}

void PowerBar::setValue(float v)
{
    v = std::max(min_, std::min(max_, v));
    if (v == value_) return;
    value_ = v;
    markDirty();
    onValueChanged.emit(value_);
}

void PowerBar::setRange(float minVal, float maxVal)
{
    min_ = minVal; max_ = maxVal;
    setValue(value_);
}

Widget::Vec2f PowerBar::sizeHint() const { return {300.0f, 36.0f}; }

void PowerBar::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    float barH = 4.0f;
    float barY = abs.y + abs.h * 0.5f - barH * 0.5f - 6;
    float barX = abs.x + 20;
    float barW = abs.w - 40;

    // Center position (where 0 is)
    float range = max_ - min_;
    float zeroNorm = (range > 0) ? (0.0f - min_) / range : 0.5f;
    float centerX = barX + zeroNorm * barW;

    // ── Background ticks ────────────────────────────────────────────────
    if (showTicks_ && tickCount_ > 0) {
        for (int i = 0; i <= tickCount_; ++i) {
            float t = static_cast<float>(i) / tickCount_;
            float tx = barX + t * barW;
            bool isCenter = (std::abs(tx - centerX) < barW / tickCount_ * 0.3f);
            float tickH = isCenter ? 10.0f : 5.0f;
            ctx.line.SetColor(80, 80, 85, isCenter ? 255 : 160);
            ctx.drawLine(tx, barY - tickH * 0.5f, tx, barY + barH + tickH * 0.5f);
        }
    }

    // ── Background bar ──────────────────────────────────────────────────
    Rect clipped;
    Rect barRect = {barX, barY, barW, barH};
    if (ctx.clipRectIntersect(barRect, clipped)) {
        ctx.fill.SetColor(barBgColor_.r, barBgColor_.g, barBgColor_.b, barBgColor_.a);
        ctx.fillRect(clipped.x, clipped.y, clipped.w, clipped.h);
    }

    // ── Value fill ──────────────────────────────────────────────────────
    if (std::abs(value_) > 0.01f) {
        float valueNorm = (range > 0) ? (value_ - min_) / range : 0.5f;
        float valueX = barX + valueNorm * barW;

        float fillX, fillW;
        Color fillColor;
        if (value_ >= 0) {
            fillX = centerX;
            fillW = valueX - centerX;
            fillColor = posColor_;
        } else {
            fillX = valueX;
            fillW = centerX - valueX;
            fillColor = negColor_;
        }
        if (fillW > 0) {
            Rect fillRect = {fillX, barY, fillW, barH};
            if (ctx.clipRectIntersect(fillRect, clipped)) {
                ctx.fill.SetColor(fillColor.r, fillColor.g, fillColor.b, fillColor.a);
                ctx.fillRect(clipped.x, clipped.y, clipped.w, clipped.h);
            }
        }
    }

    // ── Value text + unit ───────────────────────────────────────────────
    ctx.font.SetFontSize(13.0f);
    ctx.font.SetBatch(&ctx.text);
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d %s", static_cast<int>(value_), unit_.c_str());
        ctx.font.SetColor(Color(150, 150, 155, 200));
        float tw = ctx.font.GetTextWidth(buf);
        float asc = ctx.font.GetAscender();
        ctx.font.Print(buf, abs.x + (abs.w - tw) * 0.5f, barY + barH + 16 + asc);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  InfoTile
// ═════════════════════════════════════════════════════════════════════════════

InfoTile::InfoTile(const std::string& icon, const std::string& value,
                   const std::string& suffix)
    : icon_(icon), value_(value), suffix_(suffix)
{
    setSize(100, 28);
}

Widget::Vec2f InfoTile::sizeHint() const { return {100.0f, 28.0f}; }

void InfoTile::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    ctx.font.SetFontSize(13.0f);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();
    float cy = abs.y + (abs.h + asc) * 0.5f;
    float x = abs.x + 4;

    // Icon
    if (!icon_.empty()) {
        ctx.font.SetColor(iconColor_);
        ctx.font.Print(icon_.c_str(), x, cy);
        x += ctx.font.GetTextWidth(icon_.c_str()) + 4;
    }

    // Value
    ctx.font.SetColor(valueColor_);
    ctx.font.Print(value_.c_str(), x, cy);
    x += ctx.font.GetTextWidth(value_.c_str());

    // Suffix
    if (!suffix_.empty()) {
        ctx.font.SetColor(Color(140, 140, 145, 200));
        ctx.font.Print(suffix_.c_str(), x + 2, cy);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  DriveMode
// ═════════════════════════════════════════════════════════════════════════════

DriveMode::DriveMode()
{
    acceptsFocus_ = true;
    setSize(120, 28);
}

int DriveMode::addMode(const std::string& name, const Color& color)
{
    modes_.push_back({name, color});
    markDirty();
    return static_cast<int>(modes_.size()) - 1;
}

void DriveMode::setMode(int index)
{
    if (index < 0 || index >= static_cast<int>(modes_.size())) return;
    if (index == current_) return;
    current_ = index;
    markDirty();
    onModeChanged.emit(current_);
}

const std::string& DriveMode::modeName() const
{
    static const std::string empty;
    if (current_ >= 0 && current_ < static_cast<int>(modes_.size()))
        return modes_[current_].name;
    return empty;
}

Widget::Vec2f DriveMode::sizeHint() const { return {120.0f, 28.0f}; }

void DriveMode::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    if (modes_.empty()) return;
    const auto& m = modes_[current_];

    // Up/down arrows
    float arrowX = abs.x + 4;
    float midY = abs.y + abs.h * 0.5f;
    ctx.line.SetColor(100, 100, 105, 200);
    // Up
    ctx.drawLine(arrowX, midY - 1, arrowX + 4, midY - 5);
    ctx.drawLine(arrowX + 4, midY - 5, arrowX + 8, midY - 1);
    // Down
    ctx.drawLine(arrowX, midY + 1, arrowX + 4, midY + 5);
    ctx.drawLine(arrowX + 4, midY + 5, arrowX + 8, midY + 1);

    // Mode name
    ctx.font.SetFontSize(14.0f);
    ctx.font.SetBatch(&ctx.text);
    ctx.font.SetColor(m.color);
    float asc = ctx.font.GetAscender();
    float tw = ctx.font.GetTextWidth(m.name.c_str());
    ctx.font.Print(m.name.c_str(), abs.x + 18, abs.y + (abs.h + asc) * 0.5f);
}

void DriveMode::onMousePress(MouseEvent& e)
{
    if (!enabled_ || modes_.empty()) return;
    Rect abs = absoluteRect();
    float midY = abs.y + abs.h * 0.5f;
    if (e.y < midY)
        setMode((current_ + 1) % static_cast<int>(modes_.size()));
    else
        setMode((current_ - 1 + static_cast<int>(modes_.size())) % static_cast<int>(modes_.size()));
    e.consumed = true;
}

void DriveMode::onMouseScroll(MouseEvent& e)
{
    if (!enabled_ || modes_.empty()) return;
    int delta = (e.scrollY > 0) ? 1 : -1;
    setMode((current_ + delta + static_cast<int>(modes_.size())) % static_cast<int>(modes_.size()));
    e.consumed = true;
}

void DriveMode::onKeyPress(KeyEvent& e)
{
    if (modes_.empty()) return;
    if (e.key == BuGUI::Key::Up) {
        setMode((current_ + 1) % static_cast<int>(modes_.size()));
        e.consumed = true;
    } else if (e.key == BuGUI::Key::Down) {
        setMode((current_ - 1 + static_cast<int>(modes_.size())) % static_cast<int>(modes_.size()));
        e.consumed = true;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  DigitalSpeed
// ═════════════════════════════════════════════════════════════════════════════

DigitalSpeed::DigitalSpeed(float value)
    : value_(value)
{
    setSize(200, 120);
}

void DigitalSpeed::setValue(float v)
{
    if (v == value_) return;
    value_ = v;
    markDirty();
    onValueChanged.emit(value_);
}

Widget::Vec2f DigitalSpeed::sizeHint() const { return {200.0f, 120.0f}; }

void DigitalSpeed::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    float cx = abs.x + abs.w * 0.5f;

    ctx.font.SetFontSize(14.0f);
    ctx.font.SetBatch(&ctx.text);

    // Unit text (above)
    if (!unit_.empty()) {
        ctx.font.SetColor(unitColor_);
        float tw = ctx.font.GetTextWidth(unit_.c_str());
        float asc = ctx.font.GetAscender();
        ctx.font.Print(unit_.c_str(), cx - tw * 0.5f, abs.y + abs.h * 0.28f);
    }

    // Big speed value
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(value_));

        ctx.font.SetColor(valueColor_);
        // We only have one font size, so we render at the font size available
        // but draw it centered prominently
        float tw = ctx.font.GetTextWidth(buf);
        float asc = ctx.font.GetAscender();
        ctx.font.Print(buf, cx - tw * 0.5f, abs.y + abs.h * 0.6f);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  BatteryGauge
// ═════════════════════════════════════════════════════════════════════════════

BatteryGauge::BatteryGauge(float percent)
    : percent_(percent)
{
    setSize(80, 24);
}

void BatteryGauge::setPercent(float p)
{
    p = std::max(0.0f, std::min(100.0f, p));
    if (p == percent_) return;
    percent_ = p;
    markDirty();
    onPercentChanged.emit(percent_);
}

Widget::Vec2f BatteryGauge::sizeHint() const { return {80.0f, 24.0f}; }

void BatteryGauge::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    float battW  = 24.0f;
    float battH  = 14.0f;
    float tipW   = 3.0f;
    float bx = abs.x + 4;
    float by = abs.y + (abs.h - battH) * 0.5f;

    Rect clipped;

    // Battery outline
    ctx.line.SetColor(140, 140, 145, 200);
    Rect outR = {bx, by, battW, battH};
    if (ctx.clipRectIntersect(outR, clipped))
        ctx.line.Rectangle(clipped.x, clipped.y, clipped.w, clipped.h, false);

    // Tip
    ctx.fill.SetColor(140, 140, 145, 200);
    float tipY = by + battH * 0.25f;
    float tipH = battH * 0.5f;
    ctx.fillRect(bx + battW, tipY, tipW, tipH);

    // Fill
    float fillW = (battW - 4) * (percent_ / 100.0f);
    Color fillCol;
    if (percent_ > 50)      fillCol = Color(60, 200, 80, 255);
    else if (percent_ > 20) fillCol = Color(220, 180, 40, 255);
    else                     fillCol = Color(220, 60, 60, 255);

    if (charging_) fillCol = Color(60, 180, 240, 255);

    if (fillW > 0) {
        Rect fillR = {bx + 2, by + 2, fillW, battH - 4};
        if (ctx.clipRectIntersect(fillR, clipped)) {
            ctx.fill.SetColor(fillCol.r, fillCol.g, fillCol.b, fillCol.a);
            ctx.fillRect(clipped.x, clipped.y, clipped.w, clipped.h);
        }
    }

    // Percentage text
    ctx.font.SetFontSize(12.0f);
    ctx.font.SetBatch(&ctx.text);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(percent_));
    float asc = ctx.font.GetAscender();
    ctx.font.SetColor(Color(200, 200, 205, 255));
    ctx.font.Print(buf, bx + battW + tipW + 4, abs.y + (abs.h + asc) * 0.5f);

    // Charging icon (bolt)
    if (charging_) {
        float ix = bx + battW + tipW + 4 + ctx.font.GetTextWidth(buf) + 4;
        float iy = abs.y + abs.h * 0.5f;
        ctx.line.SetColor(60, 180, 240, 255);
        ctx.drawLine(ix + 4, iy - 5, ix, iy);
        ctx.drawLine(ix, iy, ix + 6, iy);
        ctx.drawLine(ix + 6, iy, ix + 2, iy + 5);
    }
}
