#include "pch.hpp"
#include "InputWidgets.hpp"

// ═════════════════════════════════════════════════════════════════════════════
//  Slider
// ═════════════════════════════════════════════════════════════════════════════

Slider::Slider(float minVal, float maxVal, float value)
    : min_(minVal), max_(maxVal), value_(value)
{
    acceptsFocus_ = true;
    cursor_ = CursorType::Hand;
}

void Slider::setRange(float minVal, float maxVal) { min_ = minVal; max_ = maxVal; }

void Slider::setValue(float v)
{
    v = std::clamp(v, min_, max_);
    if (v != value_)
    {
        value_ = v;
        onValueChanged.emit(value_);
        WidgetApp::instance().fireEvent("change", this);
        markDirty();
    }
}

Widget::Vec2f Slider::sizeHint() const
{
    const auto& t = Theme::instance();
    if (orientation_ == LayoutDir::Vertical)
        return {t.sliderHeight, 120.0f};
    return {120.0f, t.sliderHeight};
}

void Slider::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();
    float ratio = (max_ > min_) ? (value_ - min_) / (max_ - min_) : 0.0f;
    bool vert = (orientation_ == LayoutDir::Vertical);

    if (vert)
    {
        // ── Vertical slider ──────────────────────────────────────────────
        float trackW = 4.0f;
        float trackX = abs.x + (abs.w - trackW) * 0.5f;

        Rect trackRect = {trackX, abs.y, trackW, abs.h};
        Rect clippedTrack;
        if (ctx.clipRectIntersect(trackRect, clippedTrack))
        {
            Color trackC = focused_ ? t.sliderTrackFocused : (hovered_ ? t.sliderTrackHover : t.sliderTrack);
            ctx.fill.SetColor(trackC);
            ctx.fill.RoundedRectangle(clippedTrack.x, clippedTrack.y,
                                      clippedTrack.w, clippedTrack.h, 2.0f, 4, true);
        }

        float fillH = abs.h * ratio;
        float fillY = abs.y + abs.h - fillH;
        Rect fillRect = {trackX, fillY, trackW, fillH};
        Rect clippedFill;
        if (ctx.clipRectIntersect(fillRect, clippedFill))
        {
            Color fillC = hovered_ ? t.sliderFillHover : t.sliderFill;
            ctx.fill.SetColor(fillC);
            ctx.fill.RoundedRectangle(clippedFill.x, clippedFill.y,
                                      clippedFill.w, clippedFill.h, 2.0f, 4, true);
        }

        float thumbR = std::min(abs.w, 24.0f) * 0.4f;
        float thumbX = abs.x + abs.w * 0.5f;
        float thumbY = fillY;
        Rect thumbBounds = {thumbX - thumbR, thumbY - thumbR, thumbR * 2, thumbR * 2};
        if (!ctx.isClipped(thumbBounds))
        {
            Color thumbC = pressed_ ? t.sliderThumbPressed : (hovered_ ? t.sliderThumbHover : t.sliderThumb);
            ctx.fill.SetColor(thumbC);
            ctx.fillCircle(thumbX, thumbY, thumbR);
        }
    }
    else
    {
        // ── Horizontal slider ────────────────────────────────────────────
        float trackH = 4.0f;
        float trackY = abs.y + (abs.h - trackH) * 0.5f;

        Rect trackRect = {abs.x, trackY, abs.w, trackH};
        Rect clippedTrack;
        if (ctx.clipRectIntersect(trackRect, clippedTrack))
        {
            Color trackC = focused_ ? t.sliderTrackFocused : (hovered_ ? t.sliderTrackHover : t.sliderTrack);
            ctx.fill.SetColor(trackC);
            ctx.fill.RoundedRectangle(clippedTrack.x, clippedTrack.y,
                                      clippedTrack.w, clippedTrack.h, 2.0f, 4, true);
        }

        float fillW = abs.w * ratio;
        Rect fillRect = {abs.x, trackY, fillW, trackH};
        Rect clippedFill;
        if (ctx.clipRectIntersect(fillRect, clippedFill))
        {
            Color fillC = hovered_ ? t.sliderFillHover : t.sliderFill;
            ctx.fill.SetColor(fillC);
            ctx.fill.RoundedRectangle(clippedFill.x, clippedFill.y,
                                      clippedFill.w, clippedFill.h, 2.0f, 4, true);
        }

        float thumbR = abs.h * 0.4f;
        float thumbX = abs.x + fillW;
        float thumbY = abs.y + abs.h * 0.5f;
        Rect thumbBounds = {thumbX - thumbR, thumbY - thumbR, thumbR * 2, thumbR * 2};
        if (!ctx.isClipped(thumbBounds))
        {
            Color thumbC = pressed_ ? t.sliderThumbPressed : (hovered_ ? t.sliderThumbHover : t.sliderThumb);
            ctx.fill.SetColor(thumbC);
            ctx.fillCircle(thumbX, thumbY, thumbR);
        }
    }

    // Focus ring
    if (focused_)
    {
        Rect ringRect = {abs.x - 1, abs.y - 1, abs.w + 2, abs.h + 2};
        Rect clippedRing;
        if (ctx.clipRectIntersect(ringRect, clippedRing))
        {
            ctx.line.SetColor(t.focusColor.r, t.focusColor.g, t.focusColor.b, 80);
            ctx.line.RoundedRectangle(clippedRing.x, clippedRing.y,
                                      clippedRing.w, clippedRing.h, 3.0f, 6, false);
        }
    }

    Widget::paint(ctx);
}

void Slider::onMousePress(MouseEvent& e)
{
    if (!enabled_) return;
    dragging_ = true;
    updateFromMouse(e.localX, e.localY);
    e.consumed = true;
    Widget::onMousePress(e);
}

void Slider::onMouseRelease(MouseEvent& e)
{
    dragging_ = false;
    e.consumed = true;
    Widget::onMouseRelease(e);
}

void Slider::onMouseMove(MouseEvent& e)
{
    if (dragging_)
    {
        updateFromMouse(e.localX, e.localY);
        e.consumed = true;
    }
}

void Slider::updateFromMouse(float localX, float localY)
{
    float ratio;
    if (orientation_ == LayoutDir::Vertical)
    {
        if (rect_.h <= 0.0f) return;
        ratio = 1.0f - std::clamp(localY / rect_.h, 0.0f, 1.0f);
    }
    else
    {
        if (rect_.w <= 0.0f) return;
        ratio = std::clamp(localX / rect_.w, 0.0f, 1.0f);
    }
    setValue(min_ + ratio * (max_ - min_));
}

// ═════════════════════════════════════════════════════════════════════════════
//  ProgressBar
// ═════════════════════════════════════════════════════════════════════════════

ProgressBar::ProgressBar(float minVal, float maxVal, float value)
    : min_(minVal), max_(maxVal), value_(value) {}

void ProgressBar::setValue(float v)
{
    v = std::clamp(v, min_, max_);
    if (v != value_)
    {
        value_ = v;
        onValueChanged.emit(value_);
        WidgetApp::instance().fireEvent("change", this);
        markDirty();
    }
}

void ProgressBar::setRange(float minVal, float maxVal)
{
    min_ = minVal;
    max_ = maxVal;
    markDirty();
}

Widget::Vec2f ProgressBar::sizeHint() const
{
    if (orientation_ == LayoutDir::Vertical)
        return {24.0f, 120.0f};
    return {120.0f, 20.0f};
}

void ProgressBar::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;

    const auto& t = Theme::instance();
    float ratio = (max_ > min_) ? (value_ - min_) / (max_ - min_) : 0.0f;
    bool vert = (orientation_ == LayoutDir::Vertical);

    // Track background
    Rect clipped;
    if (ctx.clipRectIntersect(abs, clipped))
    {
        ctx.fill.SetColor(t.sliderTrack);
        ctx.fill.RoundedRectangle(clipped.x, clipped.y, clipped.w, clipped.h, 3.0f, 6, true);
    }

    // Fill bar
    if (vert)
    {
        float fillH = abs.h * ratio;
        float fillY = abs.y + abs.h - fillH;
        Rect fillRect = {abs.x, fillY, abs.w, fillH};
        Rect clippedFill;
        if (ctx.clipRectIntersect(fillRect, clippedFill))
        {
            ctx.fill.SetColor(barColor_);
            ctx.fill.RoundedRectangle(clippedFill.x, clippedFill.y,
                                      clippedFill.w, clippedFill.h, 3.0f, 6, true);
        }
    }
    else
    {
        float fillW = abs.w * ratio;
        Rect fillRect = {abs.x, abs.y, fillW, abs.h};
        Rect clippedFill;
        if (ctx.clipRectIntersect(fillRect, clippedFill))
        {
            ctx.fill.SetColor(barColor_);
            ctx.fill.RoundedRectangle(clippedFill.x, clippedFill.y,
                                      clippedFill.w, clippedFill.h, 3.0f, 6, true);
        }
    }

    // Border
    if (ctx.clipRectIntersect(abs, clipped))
    {
        ctx.line.SetColor(t.borderColor);
        ctx.line.RoundedRectangle(clipped.x, clipped.y, clipped.w, clipped.h, 3.0f, 6, false);
    }

    // Status text (horizontal only)
    if (!text_.empty() && !vert)
    {
        if (overrideFont_) ctx.pushFont(overrideFont_);
        float textH = ctx.font.GetFontSize();
        float textW = ctx.font.GetTextWidth(text_.c_str());
        float tx, ty = abs.y + (abs.h + ctx.font.GetAscender()) * 0.5f;

        switch (textAlign_)
        {
        case TextAlign::LEFT:
            tx = abs.x + 4.0f;
            break;
        case TextAlign::RIGHT:
            tx = abs.x + abs.w - textW - 4.0f;
            break;
        case TextAlign::CENTER:
        default:
            tx = abs.x + (abs.w - textW) * 0.5f;
            break;
        }

        ctx.font.SetColor(textColor_);
        Rect textRect = {tx, ty - textH, textW, textH};
        if (!ctx.isClipped(textRect))
            ctx.font.Print(text_.c_str(), tx, ty);
        if (overrideFont_) ctx.popFont();
    }

    Widget::paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  SpinBox
// ═════════════════════════════════════════════════════════════════════════════

SpinBox::SpinBox(float minVal, float maxVal, float value, float step)
    : min_(minVal), max_(maxVal), value_(value), step_(step)
{
    acceptsFocus_ = true;
    setSize(120, 28);
}

void SpinBox::setValue(float v)
{
    v = std::max(min_, std::min(max_, v));
    if (v == value_) return;
    value_ = v;
    markDirty();
    onValueChanged.emit(value_);
}

void SpinBox::setRange(float minVal, float maxVal)
{
    min_ = minVal;
    max_ = maxVal;
    setValue(value_);
}

float SpinBox::buttonWidth() const { return rect_.h; }

std::string SpinBox::formatValue() const
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", decimals_, static_cast<double>(value_));
    std::string result;
    if (!prefix_.empty()) result += prefix_;
    result += buf;
    if (!suffix_.empty()) result += suffix_;
    return result;
}

SpinBox::HitZone SpinBox::hitZone(float localX) const
{
    float bw = buttonWidth();
    if (localX < bw)               return Minus;
    if (localX > rect_.w - bw)     return Plus;
    return Value;
}

Widget::Vec2f SpinBox::sizeHint() const { return {120.0f, 28.0f}; }

void SpinBox::paint(PaintContext& ctx)
{
    if (!visible_) return;
    const auto& t = Theme::instance();
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;
    Rect clipped;
    float bw = buttonWidth();

    // Background
    if (ctx.clipRectIntersect(abs, clipped)) {
        ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
        ctx.fill.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                   static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                   t.borderRadius, 6, true);
    }

    // Minus button
    Rect minusR = {abs.x, abs.y, bw, abs.h};
    if (ctx.clipRectIntersect(minusR, clipped)) {
        ctx.fill.SetColor(t.buttonNormal.r, t.buttonNormal.g, t.buttonNormal.b, t.buttonNormal.a);
        ctx.fill.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                   static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                   t.borderRadius, 6, true);
    }
    // Minus glyph
    float mcy = abs.y + abs.h * 0.5f;
    float mcx = abs.x + bw * 0.5f;
    ctx.line.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
    ctx.line.Line2D(mcx - 4, mcy, mcx + 4, mcy);

    // Plus button
    Rect plusR = {abs.x + abs.w - bw, abs.y, bw, abs.h};
    if (ctx.clipRectIntersect(plusR, clipped)) {
        ctx.fill.SetColor(t.buttonNormal.r, t.buttonNormal.g, t.buttonNormal.b, t.buttonNormal.a);
        ctx.fill.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                   static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                   t.borderRadius, 6, true);
    }
    // Plus glyph
    float pcx = abs.x + abs.w - bw * 0.5f;
    float pcy = abs.y + abs.h * 0.5f;
    ctx.line.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
    ctx.line.Line2D(pcx - 4, pcy, pcx + 4, pcy);
    ctx.line.Line2D(pcx, pcy - 4, pcx, pcy + 4);

    // Value text
    if (overrideFont_) ctx.pushFont(overrideFont_);
    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    std::string txt = formatValue();
    float tw = ctx.font.GetTextWidth(txt.c_str());
    float asc = ctx.font.GetAscender();
    float tx = abs.x + bw + (abs.w - 2 * bw - tw) * 0.5f;
    float ty = abs.y + (abs.h + asc) * 0.5f;
    ctx.font.SetColor(enabled_ ? t.textColor : t.textDisabled);
    ctx.font.Print(txt.c_str(), tx, ty);
    if (overrideFont_) ctx.popFont();

    // Border
    Color bc = isFocused() ? t.focusColor : (isHovered() ? t.inputBorderHover : t.inputBorder);
    if (ctx.clipRectIntersect(abs, clipped)) {
        ctx.line.SetColor(bc.r, bc.g, bc.b, bc.a);
        ctx.line.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                   static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                   t.borderRadius, 6, false);
    }
}

void SpinBox::onMousePress(MouseEvent& e)
{
    auto zone = hitZone(e.localX);
    if (zone == Minus) {
        setValue(value_ - step_);
    } else if (zone == Plus) {
        setValue(value_ + step_);
    } else {
        // Start drag on value area
        dragging_   = true;
        dragStartY_ = e.y;
        dragStartV_ = value_;
    }
    e.consumed = true;
}

void SpinBox::onMouseRelease(MouseEvent& e)
{
    dragging_ = false;
    e.consumed = true;
}

void SpinBox::onMouseMove(MouseEvent& e)
{
    if (dragging_) {
        float dy = dragStartY_ - e.y;  // up = increase
        float delta = dy * step_ * dragSens_;
        setValue(dragStartV_ + delta);
        e.consumed = true;
    }
}

void SpinBox::onMouseScroll(MouseEvent& e)
{
    if (isHovered() || isFocused()) {
        setValue(value_ + e.scrollY * step_);
        e.consumed = true;
    }
}

void SpinBox::onKeyPress(KeyEvent& e)
{
    if (e.key == BuGUI::Key::Up) {
        setValue(value_ + step_);
        e.consumed = true;
    } else if (e.key == BuGUI::Key::Down) {
        setValue(value_ - step_);
        e.consumed = true;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  CalendarPopup_ — internal popup for DatePicker
// ═════════════════════════════════════════════════════════════════════════════

static int daysInMonth(int year, int month)
{
    static const int d[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
        return 29;
    return d[month];
}

static int dayOfWeek(int year, int month, int day)
{
    // Tomohiko Sakamoto's algorithm (0=Sun, 1=Mon, ..., 6=Sat)
    static const int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    if (month < 3) year--;
    return (year + year/4 - year/100 + year/400 + t[month-1] + day) % 7;
}

class CalendarPopup_ : public Widget
{
public:
    CalendarPopup_(DatePicker* owner, const Date& d)
        : owner_(owner), viewYear_(d.year), viewMonth_(d.month), selected_(d)
    {
        acceptsFocus_ = true;
        yearPageStart_ = (d.year / 12) * 12;  // page of 12 years
    }

    bool popupContains(float x, float y) const override
    {
        Rect abs = absoluteRect();
        return abs.contains(x, y);
    }

    void paint(PaintContext& ctx) override
    {
        const auto& t = Theme::instance();
        Rect abs = absoluteRect();
        Rect clipped;

        // Background + border
        if (ctx.clipRectIntersect(abs, clipped)) {
            ctx.fill.SetColor(t.panelColor.r, t.panelColor.g, t.panelColor.b, 250);
            ctx.fill.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                       static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                       t.borderRadius, 6, true);
            ctx.line.SetColor(t.inputBorderHover.r, t.inputBorderHover.g, t.inputBorderHover.b, 255);
            ctx.line.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                       static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                       t.borderRadius, 6, false);
        }

        ctx.pushClip(abs);

        if (mode_ == YearPick)
            paintYearPicker(ctx, abs);
        else
            paintCalendar(ctx, abs);

        ctx.popClip();
    }

    // ── Calendar (day) view ──────────────────────────────────────────────
    void paintCalendar(PaintContext& ctx, const Rect& abs)
    {
        const auto& t = Theme::instance();
        float pad   = 8.0f;
        float cellW = (abs.w - pad * 2) / 7.0f;
        float cellH = 22.0f;
        float headerH = 28.0f;

        ctx.font.SetFontSize(t.fontSize);
        ctx.font.SetBatch(&ctx.text);
        float asc = ctx.font.GetAscender();

        float hx = abs.x + pad;
        float hy = abs.y + pad;

        // Left arrow  — prev month
        arrowLeftRect_ = {hx, hy, cellW, headerH};
        ctx.font.SetColor(t.focusColor);
        float tw = ctx.font.GetTextWidth("<");
        ctx.font.Print("<", hx + (cellW - tw) * 0.5f, hy + (headerH + asc) * 0.5f);

        // Right arrow — next month
        float rx = abs.x + abs.w - pad - cellW;
        arrowRightRect_ = {rx, hy, cellW, headerH};
        tw = ctx.font.GetTextWidth(">");
        ctx.font.Print(">", rx + (cellW - tw) * 0.5f, hy + (headerH + asc) * 0.5f);

        // Month + year label (clickable — switches to year picker)
        static const char* months[] = {"", "Jan","Feb","Mar","Apr","May","Jun",
                                           "Jul","Aug","Sep","Oct","Nov","Dec"};
        char label[32];
        std::snprintf(label, sizeof(label), "%s %d", months[viewMonth_], viewYear_);
        ctx.font.SetColor(t.focusColor);  // hint it's clickable
        tw = ctx.font.GetTextWidth(label);
        float lx = abs.x + (abs.w - tw) * 0.5f;
        float ly = hy + (headerH + asc) * 0.5f;
        ctx.font.Print(label, lx, ly);
        headerLabelRect_ = {lx - 4, hy, tw + 8, headerH};

        // Day-of-week headers
        float dy = hy + headerH + 4;
        static const char* dow[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};
        ctx.font.SetColor(Color(140, 140, 160, 255));
        for (int i = 0; i < 7; ++i) {
            float dx = abs.x + pad + i * cellW;
            tw = ctx.font.GetTextWidth(dow[i]);
            ctx.font.Print(dow[i], dx + (cellW - tw) * 0.5f, dy + (cellH + asc) * 0.5f);
        }

        // Day grid
        int numDays = daysInMonth(viewYear_, viewMonth_);
        int startDow = dayOfWeek(viewYear_, viewMonth_, 1);
        float gy = dy + cellH + 2;

        int row = 0, col = startDow;
        for (int d = 1; d <= numDays; ++d) {
            float gx = abs.x + pad + col * cellW;
            float cy2 = gy + row * cellH;
            bool isSel = (d == selected_.day && viewMonth_ == selected_.month && viewYear_ == selected_.year);

            if (isSel) {
                Rect cellR = {gx, cy2, cellW, cellH};
                Rect rc;
                if (ctx.clipRectIntersect(cellR, rc)) {
                    ctx.fill.SetColor(t.focusColor.r, t.focusColor.g, t.focusColor.b, 200);
                    ctx.fill.RoundedRectangle(static_cast<int>(rc.x + 2), static_cast<int>(rc.y + 1),
                                               static_cast<int>(rc.w - 4), static_cast<int>(rc.h - 2),
                                               3, 4, true);
                }
            }

            char buf[4];
            std::snprintf(buf, sizeof(buf), "%d", d);
            tw = ctx.font.GetTextWidth(buf);
            ctx.font.SetColor(isSel ? Color(255,255,255,255) : t.textColor);
            ctx.font.Print(buf, gx + (cellW - tw) * 0.5f, cy2 + (cellH + asc) * 0.5f);

            col++;
            if (col >= 7) { col = 0; row++; }
        }

        gridY_        = gy;
        gridStartX_   = abs.x + pad;
        gridCellW_    = cellW;
        gridCellH_    = cellH;
        gridStartDow_ = startDow;
    }

    // ── Year picker view (4×3 grid of 12 years) ─────────────────────────
    void paintYearPicker(PaintContext& ctx, const Rect& abs)
    {
        const auto& t = Theme::instance();
        float pad     = 8.0f;
        float headerH = 28.0f;
        float cellW   = (abs.w - pad * 2) / 4.0f;
        float cellH   = 28.0f;

        ctx.font.SetFontSize(t.fontSize);
        ctx.font.SetBatch(&ctx.text);
        float asc = ctx.font.GetAscender();

        float hx = abs.x + pad;
        float hy = abs.y + pad;

        // Left arrow — prev 12 years
        arrowLeftRect_ = {hx, hy, cellW, headerH};
        ctx.font.SetColor(t.focusColor);
        float tw = ctx.font.GetTextWidth("<");
        ctx.font.Print("<", hx + (cellW - tw) * 0.5f, hy + (headerH + asc) * 0.5f);

        // Right arrow — next 12 years
        float rx = abs.x + abs.w - pad - cellW;
        arrowRightRect_ = {rx, hy, cellW, headerH};
        tw = ctx.font.GetTextWidth(">");
        ctx.font.Print(">", rx + (cellW - tw) * 0.5f, hy + (headerH + asc) * 0.5f);

        // Range label
        char rangeLabel[32];
        std::snprintf(rangeLabel, sizeof(rangeLabel), "%d - %d", yearPageStart_, yearPageStart_ + 11);
        ctx.font.SetColor(t.textColor);
        tw = ctx.font.GetTextWidth(rangeLabel);
        ctx.font.Print(rangeLabel, abs.x + (abs.w - tw) * 0.5f, hy + (headerH + asc) * 0.5f);

        // 4×3 year grid
        float gy = hy + headerH + 8;
        yrGridY_     = gy;
        yrGridX_     = abs.x + pad;
        yrGridCellW_ = cellW;
        yrGridCellH_ = cellH;

        for (int i = 0; i < 12; ++i) {
            int yr  = yearPageStart_ + i;
            int col = i % 4;
            int row = i / 4;
            float gx  = abs.x + pad + col * cellW;
            float cy2 = gy + row * cellH;

            bool isSel = (yr == viewYear_);
            if (isSel) {
                Rect cellR = {gx, cy2, cellW, cellH};
                Rect rc;
                if (ctx.clipRectIntersect(cellR, rc)) {
                    ctx.fill.SetColor(t.focusColor.r, t.focusColor.g, t.focusColor.b, 200);
                    ctx.fill.RoundedRectangle(static_cast<int>(rc.x + 2), static_cast<int>(rc.y + 1),
                                               static_cast<int>(rc.w - 4), static_cast<int>(rc.h - 2),
                                               3, 4, true);
                }
            }

            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", yr);
            tw = ctx.font.GetTextWidth(buf);
            ctx.font.SetColor(isSel ? Color(255,255,255,255) : t.textColor);
            ctx.font.Print(buf, gx + (cellW - tw) * 0.5f, cy2 + (cellH + asc) * 0.5f);
        }
    }

    void onMousePress(MouseEvent& e) override
    {
        if (mode_ == YearPick)
            onMousePressYear(e);
        else
            onMousePressCalendar(e);
    }

    void onMousePressCalendar(MouseEvent& e)
    {
        // Header label click → switch to year picker
        if (headerLabelRect_.contains(e.x, e.y)) {
            mode_ = YearPick;
            yearPageStart_ = (viewYear_ / 12) * 12;
            markDirty();
            e.consumed = true;
            return;
        }

        // Left arrow
        if (arrowLeftRect_.contains(e.x, e.y)) {
            viewMonth_--;
            if (viewMonth_ < 1) { viewMonth_ = 12; viewYear_--; }
            markDirty();
            e.consumed = true;
            return;
        }
        // Right arrow
        if (arrowRightRect_.contains(e.x, e.y)) {
            viewMonth_++;
            if (viewMonth_ > 12) { viewMonth_ = 1; viewYear_++; }
            markDirty();
            e.consumed = true;
            return;
        }

        // Day grid
        if (e.y >= gridY_) {
            int col = static_cast<int>((e.x - gridStartX_) / gridCellW_);
            int row = static_cast<int>((e.y - gridY_) / gridCellH_);
            if (col >= 0 && col < 7) {
                int day = row * 7 + col - gridStartDow_ + 1;
                int numDays = daysInMonth(viewYear_, viewMonth_);
                if (day >= 1 && day <= numDays) {
                    Date nd{viewYear_, viewMonth_, day};
                    owner_->setDate(nd);
                    owner_->closeCalendar();
                    e.consumed = true;
                    return;
                }
            }
        }
        e.consumed = true;
    }

    void onMousePressYear(MouseEvent& e)
    {
        // Left arrow — prev 12
        if (arrowLeftRect_.contains(e.x, e.y)) {
            yearPageStart_ -= 12;
            markDirty();
            e.consumed = true;
            return;
        }
        // Right arrow — next 12
        if (arrowRightRect_.contains(e.x, e.y)) {
            yearPageStart_ += 12;
            markDirty();
            e.consumed = true;
            return;
        }

        // Year grid hit
        if (e.y >= yrGridY_) {
            int col = static_cast<int>((e.x - yrGridX_) / yrGridCellW_);
            int row = static_cast<int>((e.y - yrGridY_) / yrGridCellH_);
            if (col >= 0 && col < 4 && row >= 0 && row < 3) {
                int idx = row * 4 + col;
                viewYear_ = yearPageStart_ + idx;
                mode_ = Calendar;
                markDirty();
                e.consumed = true;
                return;
            }
        }
        e.consumed = true;
    }

    void onKeyPress(KeyEvent& e) override
    {
        if (e.key == BuGUI::Key::Escape) {
            if (mode_ == YearPick) {
                mode_ = Calendar;
                markDirty();
            } else {
                owner_->closeCalendar();
            }
            e.consumed = true;
        }
    }

private:
    DatePicker* owner_;
    int  viewYear_, viewMonth_;
    Date selected_;

    enum Mode { Calendar, YearPick };
    Mode mode_ = Calendar;

    // Calendar hit-test
    Rect arrowLeftRect_{}, arrowRightRect_{}, headerLabelRect_{};
    float gridY_       = 0;
    float gridStartX_  = 0;
    float gridCellW_   = 0;
    float gridCellH_   = 0;
    int   gridStartDow_ = 0;

    // Year picker hit-test
    int   yearPageStart_ = 2020;
    float yrGridY_     = 0;
    float yrGridX_     = 0;
    float yrGridCellW_ = 0;
    float yrGridCellH_ = 0;
};

// ═════════════════════════════════════════════════════════════════════════════
//  DatePicker
// ═════════════════════════════════════════════════════════════════════════════

DatePicker::DatePicker()
{
    acceptsFocus_ = true;
    setSize(150, 28);
}

void DatePicker::setDate(const Date& d)
{
    if (d == date_) return;
    date_ = d;
    markDirty();
    onDateChanged.emit(d.year, d.month, d.day);
}

void DatePicker::setDate(int year, int month, int day)
{
    setDate(Date{year, month, day});
}

std::string DatePicker::formatDate() const
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", date_.year, date_.month, date_.day);
    return buf;
}

Widget::Vec2f DatePicker::sizeHint() const { return {150.0f, 28.0f}; }

void DatePicker::paint(PaintContext& ctx)
{
    if (!visible_) return;
    const auto& t = Theme::instance();
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;
    Rect clipped;

    // Background
    if (ctx.clipRectIntersect(abs, clipped)) {
        ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
        ctx.fill.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                   static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                   t.borderRadius, 6, true);
    }

    // Text
    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    std::string txt = formatDate();
    float asc = ctx.font.GetAscender();
    ctx.font.SetColor(t.textColor);
    ctx.font.Print(txt.c_str(), abs.x + t.padding, abs.y + (abs.h + asc) * 0.5f);

    // Calendar icon (small grid hint)
    float ix = abs.x + abs.w - 20;
    float iy = abs.y + abs.h * 0.5f;
    ctx.line.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, 160);
    ctx.line.Rectangle(static_cast<int>(ix), static_cast<int>(iy - 5),
                       12, 10, false);
    ctx.line.Line2D(ix, iy - 2, ix + 12, iy - 2);

    // Border
    Color bc = isFocused() ? t.focusColor : (isHovered() ? t.inputBorderHover : t.inputBorder);
    if (ctx.clipRectIntersect(abs, clipped)) {
        ctx.line.SetColor(bc.r, bc.g, bc.b, bc.a);
        ctx.line.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                   static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                   t.borderRadius, 6, false);
    }
}

void DatePicker::onMousePress(MouseEvent& e)
{
    if (!enabled_) return;
    if (open_) closeCalendar(); else openCalendar();
    e.consumed = true;
}

void DatePicker::openCalendar()
{
    if (open_) return;
    open_ = true;

    Rect abs = absoluteRect();
    float popW = 7 * 32.0f + 16;  // 7 cols * ~32px + padding
    float popH = 8 * 22.0f + 40;  // header + dow + 6 rows + padding

    auto* popup = new CalendarPopup_(this, date_);
    const auto& io = BuGUI::GetIO();
    float popX = abs.x;
    float popY = abs.y + abs.h + 2;
    if (popX + popW > io.displayWidth)  popX = io.displayWidth  - popW;
    if (popX < 0.f) popX = 0.f;
    if (popY + popH > io.displayHeight) popY = (abs.y - popH >= 0.f) ? abs.y - popH : io.displayHeight - popH;
    if (popY < 0.f) popY = 0.f;
    popup->setRect({popX, popY, popW, popH});
    WidgetApp::instance().showPopup(popup, this);
    markDirty();
}

void DatePicker::closeCalendar()
{
    if (!open_) return;
    open_ = false;
    WidgetApp::instance().closePopup();
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  TimePicker
// ═════════════════════════════════════════════════════════════════════════════

TimePicker::TimePicker()
{
    acceptsFocus_ = true;
    setSize(130, 28);
}

void TimePicker::setTime(const Time& t)
{
    if (t == time_) return;
    time_ = t;
    markDirty();
    onTimeChanged.emit(t.hour, t.minute, t.second);
}

void TimePicker::setTime(int h, int m, int s)
{
    setTime(Time{
        std::max(0, std::min(23, h)),
        std::max(0, std::min(59, m)),
        std::max(0, std::min(59, s))
    });
}

float TimePicker::fieldWidth() const
{
    return 30.0f;
}

float TimePicker::separatorWidth() const
{
    return 12.0f;
}

TimePicker::Field TimePicker::hitField(float localX) const
{
    float fw = fieldWidth();
    float sw = separatorWidth();
    float pad = 6.0f;
    if (localX < pad + fw) return Hour;
    if (localX < pad + fw + sw + fw) return Minute;
    return Second;
}

void TimePicker::adjustField(Field f, int delta)
{
    Time t = time_;
    switch (f) {
        case Hour:   t.hour   = (t.hour   + delta + 24) % 24; break;
        case Minute: t.minute = (t.minute + delta + 60) % 60; break;
        case Second: t.second = (t.second + delta + 60) % 60; break;
    }
    setTime(t);
}

std::string TimePicker::formatTime() const
{
    char buf[16];
    if (showSeconds_)
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", time_.hour, time_.minute, time_.second);
    else
        std::snprintf(buf, sizeof(buf), "%02d:%02d", time_.hour, time_.minute);
    return buf;
}

Widget::Vec2f TimePicker::sizeHint() const
{
    float fw  = fieldWidth();
    float sw  = separatorWidth();
    float pad = 6.0f;
    int fields = showSeconds_ ? 3 : 2;
    float w = pad * 2 + fields * fw + (fields - 1) * sw;
    return {w, 28.0f};
}

void TimePicker::paint(PaintContext& ctx)
{
    if (!visible_) return;
    const auto& t = Theme::instance();
    Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;
    Rect clipped;

    // Background
    if (ctx.clipRectIntersect(abs, clipped)) {
        ctx.fill.SetColor(t.inputBg.r, t.inputBg.g, t.inputBg.b, t.inputBg.a);
        ctx.fill.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                   static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                   t.borderRadius, 6, true);
    }

    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    float asc = ctx.font.GetAscender();
    float fw  = fieldWidth();
    float sw  = separatorWidth();
    float pad = 6.0f;
    float x   = abs.x + pad;
    float cy  = abs.y + (abs.h + asc) * 0.5f;

    auto drawField = [&](const char* text, Field field, float fx) {
        bool active = isFocused() && activeField_ == field;
        // Highlight active field
        if (active) {
            float tw2 = ctx.font.GetTextWidth(text);
            Rect hlR = {fx - 2, abs.y + 3, tw2 + 4, abs.h - 6};
            Rect hlC;
            if (ctx.clipRectIntersect(hlR, hlC)) {
                ctx.fill.SetColor(t.focusColor.r, t.focusColor.g, t.focusColor.b, 60);
                ctx.fill.RoundedRectangle(static_cast<int>(hlC.x), static_cast<int>(hlC.y),
                                           static_cast<int>(hlC.w), static_cast<int>(hlC.h),
                                           2, 4, true);
            }
        }
        ctx.font.SetColor(active ? t.focusColor : t.textColor);
        ctx.font.Print(text, fx, cy);
    };

    char hh[4], mm[4], ss[4];
    std::snprintf(hh, sizeof(hh), "%02d", time_.hour);
    std::snprintf(mm, sizeof(mm), "%02d", time_.minute);
    std::snprintf(ss, sizeof(ss), "%02d", time_.second);

    drawField(hh, Hour, x);
    x += fw;

    // Separator ":"
    ctx.font.SetColor(Color(120, 120, 130, 255));
    float stw = ctx.font.GetTextWidth(":");
    ctx.font.Print(":", x + (sw - stw) * 0.5f, cy);
    x += sw;

    drawField(mm, Minute, x);
    x += fw;

    if (showSeconds_) {
        ctx.font.SetColor(Color(120, 120, 130, 255));
        ctx.font.Print(":", x + (sw - stw) * 0.5f, cy);
        x += sw;
        drawField(ss, Second, x);
    }

    // Up/down arrows on right side
    float arrowX = abs.x + abs.w - 14;
    float midY   = abs.y + abs.h * 0.5f;
    ctx.line.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, 160);
    // Up triangle
    ctx.line.Line2D(arrowX, midY - 3, arrowX + 4, midY - 7);
    ctx.line.Line2D(arrowX + 4, midY - 7, arrowX + 8, midY - 3);
    // Down triangle
    ctx.line.Line2D(arrowX, midY + 3, arrowX + 4, midY + 7);
    ctx.line.Line2D(arrowX + 4, midY + 7, arrowX + 8, midY + 3);

    // Border
    Color bc = isFocused() ? t.focusColor : (isHovered() ? t.inputBorderHover : t.inputBorder);
    if (ctx.clipRectIntersect(abs, clipped)) {
        ctx.line.SetColor(bc.r, bc.g, bc.b, bc.a);
        ctx.line.RoundedRectangle(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                                   static_cast<int>(clipped.w), static_cast<int>(clipped.h),
                                   t.borderRadius, 6, false);
    }
}

void TimePicker::onMousePress(MouseEvent& e)
{
    if (!enabled_) return;

    // Check up/down arrow area
    if (e.localX > rect_.w - 18) {
        float midY = rect_.h * 0.5f;
        if (e.localY < midY)
            adjustField(activeField_, 1);   // up
        else
            adjustField(activeField_, -1);  // down
        e.consumed = true;
        return;
    }

    // Determine which field was clicked
    activeField_ = hitField(e.localX);
    markDirty();
    e.consumed = true;
}

void TimePicker::onMouseScroll(MouseEvent& e)
{
    if (isHovered() || isFocused()) {
        int delta = (e.scrollY > 0) ? 1 : -1;
        adjustField(activeField_, delta);
        e.consumed = true;
    }
}

void TimePicker::onKeyPress(KeyEvent& e)
{
    if (e.key == BuGUI::Key::Up) {
        adjustField(activeField_, 1);
        e.consumed = true;
    } else if (e.key == BuGUI::Key::Down) {
        adjustField(activeField_, -1);
        e.consumed = true;
    } else if (e.key == BuGUI::Key::Left) {
        if (activeField_ > Hour) {
            activeField_ = static_cast<Field>(activeField_ - 1);
            markDirty();
        }
        e.consumed = true;
    } else if (e.key == BuGUI::Key::Right) {
        int maxField = showSeconds_ ? Second : Minute;
        if (activeField_ < maxField) {
            activeField_ = static_cast<Field>(activeField_ + 1);
            markDirty();
        }
        e.consumed = true;
    }
}
