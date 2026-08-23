#include "ChartWidgets.hpp"
#include "Theme.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cstdio>

namespace {
    // Font setup helper — call before any ctx.font.Print sequence
    // Returns the ascender (offset from top to baseline)
    inline float setupFont(PaintContext& ctx, const Color& color) {
        auto& t = Theme::instance();
        ctx.font.SetFontSize(t.fontSize);
        ctx.font.SetBatch(&ctx.text);
        ctx.font.SetColor(color);
        return ctx.font.GetAscender();
    }
    inline float setupFont(PaintContext& ctx, const Color& color, float size) {
        ctx.font.SetFontSize(size);
        ctx.font.SetBatch(&ctx.text);
        ctx.font.SetColor(color);
        return ctx.font.GetAscender();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  GradientEditor
// ═════════════════════════════════════════════════════════════════════════════

namespace {
    inline uint8_t lerpU8(uint8_t a, uint8_t b, float t)
    {
        float r = a + (float(b) - float(a)) * t + 0.5f;
        return static_cast<uint8_t>(r < 0.f ? 0 : r > 255.f ? 255 : r);
    }

    static constexpr float kHandleH = 10.0f;
    static constexpr float kHandleW =  8.0f;
    static constexpr int   kMinStops = 2;
}

GradientEditor::GradientEditor()
{
    stops_.push_back({0.0f, Color(0,   0,   0,   255)});
    stops_.push_back({1.0f, Color(255, 255, 255, 255)});
}

void GradientEditor::addStop(float pos, const Color& color)
{
    pos = std::clamp(pos, 0.0f, 1.0f);
    stops_.push_back({pos, color});
    sortStops();
    markDirty();
    emit();
}

void GradientEditor::removeStop(int idx)
{
    if ((int)stops_.size() <= kMinStops) return;
    if (idx < 0 || idx >= (int)stops_.size()) return;
    stops_.erase(stops_.begin() + idx);
    if (selected_ >= (int)stops_.size()) selected_ = (int)stops_.size() - 1;
    markDirty();
    emit();
}

void GradientEditor::setStopColor(int idx, const Color& color)
{
    if (idx < 0 || idx >= (int)stops_.size()) return;
    stops_[idx].color = color;
    markDirty();
    emit();
}

void GradientEditor::setStopPos(int idx, float pos)
{
    if (idx < 0 || idx >= (int)stops_.size()) return;
    stops_[idx].pos = std::clamp(pos, 0.0f, 1.0f);
    sortStops();
    markDirty();
    emit();
}

void GradientEditor::clearStops()
{
    stops_.clear();
    selected_ = -1;
    markDirty();
}

Color GradientEditor::sample(float t) const
{
    if (stops_.empty()) return Color(0,0,0,255);
    if (stops_.size() == 1) return stops_[0].color;
    t = std::clamp(t, 0.0f, 1.0f);

    if (t <= stops_.front().pos) return stops_.front().color;
    if (t >= stops_.back().pos)  return stops_.back().color;

    for (int i = 0; i < (int)stops_.size() - 1; ++i) {
        const auto& a = stops_[i];
        const auto& b = stops_[i+1];
        if (t <= b.pos) {
            float span = b.pos - a.pos;
            float f = (span < 1e-6f) ? 0.0f : (t - a.pos) / span;
            f = std::clamp(f, 0.0f, 1.0f);
            return Color(
                lerpU8(a.color.r, b.color.r, f),
                lerpU8(a.color.g, b.color.g, f),
                lerpU8(a.color.b, b.color.b, f),
                lerpU8(a.color.a, b.color.a, f)
            );
        }
    }
    return stops_.back().color;
}

Rect GradientEditor::barRect() const
{
    const Rect b = absoluteRect();
    return { b.x + kHandleW, b.y + 2, b.w - kHandleW * 2, barHeight_ };
}

Rect GradientEditor::handleRect(int idx) const
{
    const Rect bar = barRect();
    float px = bar.x + stops_[idx].pos * bar.w;
    return { px - kHandleW, bar.y + bar.h + 2, kHandleW * 2, kHandleH };
}

int GradientEditor::hitStop(float mx, float my) const
{
    for (int i = 0; i < (int)stops_.size(); ++i) {
        const Rect hr = handleRect(i);
        if (mx >= hr.x - 4 && mx <= hr.x + hr.w + 4 &&
            my >= hr.y - 4 && my <= hr.y + hr.h + 4)
            return i;
    }
    return -1;
}

void GradientEditor::sortStops()
{
    std::sort(stops_.begin(), stops_.end(),
              [](const GradientStop& a, const GradientStop& b){ return a.pos < b.pos; });
}

void GradientEditor::emit()
{
    onChanged.emit(stops_);
}

void GradientEditor::paint(PaintContext& ctx)
{
    const Rect b  = absoluteRect();
    const Rect br = barRect();

    ctx.pushClip(b);

    // Widget background
    ctx.fill.SetColor(32, 32, 36, 255);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    // Checkerboard (alpha preview) behind bar
    if (showAlpha_) {
        int csize = 6;
        int bx = (int)br.x, by = (int)br.y;
        int bw = (int)br.w, bh = (int)br.h;
        int cols = bw / csize + 1;
        int rows = bh / csize + 1;
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                uint8_t v = ((row + col) % 2 == 0) ? 200 : 140;
                ctx.fill.SetColor(v, v, v, 255);
                float cx = bx + col * csize;
                float cy = by + row * csize;
                float cw = std::min((float)csize, (float)bx + bw - cx);
                float ch = std::min((float)csize, (float)by + bh - cy);
                if (cw > 0 && ch > 0)
                    ctx.fillRect(cx, cy, cw, ch);
            }
        }
    }

    // Gradient bar
    if (stops_.size() >= 2) {
        for (int i = 0; i < (int)stops_.size() - 1; ++i) {
            const Color& ca = stops_[i].color;
            const Color& cb = stops_[i+1].color;
            float x0 = br.x + stops_[i].pos   * br.w;
            float x1 = br.x + stops_[i+1].pos * br.w;
            float span = x1 - x0;
            if (span < 0.5f) continue;

            int segs = std::max(1, (int)(span / 2));
            for (int s = 0; s < segs; ++s) {
                float f0 = std::clamp((float)s       / segs, 0.0f, 1.0f);
                float f1 = std::clamp((float)(s + 1) / segs, 0.0f, 1.0f);
                float fc = (f0 + f1) * 0.5f;
                Color c(lerpU8(ca.r, cb.r, fc),
                        lerpU8(ca.g, cb.g, fc),
                        lerpU8(ca.b, cb.b, fc),
                        lerpU8(ca.a, cb.a, fc));
                ctx.fill.SetColor(c.r, c.g, c.b, c.a);
                ctx.fillRect(x0 + f0 * span, br.y, f1 * span - f0 * span + 1.0f, br.h);
            }
        }
    } else if (!stops_.empty()) {
        const Color& c = stops_[0].color;
        ctx.fill.SetColor(c.r, c.g, c.b, c.a);
        ctx.fillRect(br.x, br.y, br.w, br.h);
    }

    // Bar border
    ctx.fill.SetColor(80, 80, 80, 255);
    ctx.fillRect(br.x,          br.y,         br.w, 1);
    ctx.fillRect(br.x,          br.y+br.h-1,  br.w, 1);
    ctx.fillRect(br.x,          br.y,         1,    br.h);
    ctx.fillRect(br.x+br.w-1,   br.y,         1,    br.h);

    // Stop handles (triangles)
    for (int i = 0; i < (int)stops_.size(); ++i) {
        const Rect hr = handleRect(i);
        float cx   = hr.x + hr.w * 0.5f;
        float tipY = hr.y;
        float baseY = hr.y + hr.h;
        bool sel = (i == selected_);

        Color border = sel ? Color(255,255,255,255) : Color(60,60,60,255);
        ctx.fill.SetColor(border.r, border.g, border.b, 255);
        ctx.fillTriangle(cx, tipY - 1, cx - kHandleW - 1, baseY + 1, cx + kHandleW + 1, baseY + 1);

        const Color& sc = stops_[i].color;
        ctx.fill.SetColor(sc.r, sc.g, sc.b, 255);
        ctx.fillTriangle(cx, tipY, cx - kHandleW, baseY, cx + kHandleW, baseY);
    }

    // Selected stop info
    if (selected_ >= 0 && selected_ < (int)stops_.size()) {
        const auto& s = stops_[selected_];
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f  #%02X%02X%02X", s.pos, s.color.r, s.color.g, s.color.b);
        float asc = setupFont(ctx, Color(180, 180, 180, 255));
        ctx.font.Print(buf, b.x + kHandleW, b.y + 2.0f + barHeight_ + 2.0f + kHandleH + 4.0f + asc);
    }

    ctx.popClip();
}

void GradientEditor::onMousePress(MouseEvent& e)
{
    if (e.button != 0) {
        int hit = hitStop(e.x, e.y);
        if (hit >= 0) removeStop(hit);
        return;
    }

    int hit = hitStop(e.x, e.y);
    if (hit >= 0) {
        selected_ = hit;
        dragging_ = hit;
        dragOffX_ = e.x - handleRect(hit).x - kHandleW;
        markDirty();
        return;
    }

    const Rect br = barRect();
    if (e.x >= br.x && e.x <= br.x + br.w && e.y >= br.y && e.y <= br.y + br.h) {
        float t = (e.x - br.x) / br.w;
        Color c = sample(t);
        addStop(t, c);
        for (int i = 0; i < (int)stops_.size(); ++i) {
            if (std::fabs(stops_[i].pos - t) < 0.002f) { selected_ = i; break; }
        }
        markDirty();
    }
}

void GradientEditor::onMouseRelease(MouseEvent& e)
{
    (void)e;
    dragging_ = -1;
}

void GradientEditor::onMouseMove(MouseEvent& e)
{
    if (dragging_ < 0 || dragging_ >= (int)stops_.size()) return;
    const Rect br = barRect();
    float t = (e.x - dragOffX_ - br.x) / br.w;
    t = std::clamp(t, 0.0f, 1.0f);

    stops_[dragging_].pos = t;
    sortStops();
    for (int i = 0; i < (int)stops_.size(); ++i) {
        if (std::fabs(stops_[i].pos - t) < 1e-5f) { dragging_ = i; selected_ = i; break; }
    }
    markDirty();
    emit();
}

// ═════════════════════════════════════════════════════════════════════════════
//  HistogramWidget
// ═════════════════════════════════════════════════════════════════════════════

HistogramWidget::HistogramWidget()
{
    bins_.resize(binCount_, 0.0f);
}

void HistogramWidget::setData(const std::vector<float>& values)
{
    rawData_ = values;
    if (autoRange_ && !values.empty()) {
        rangeMin_ = *std::min_element(values.begin(), values.end());
        rangeMax_ = *std::max_element(values.begin(), values.end());
        if (rangeMax_ <= rangeMin_) rangeMax_ = rangeMin_ + 1.0f;
    }
    computeStats();
    recomputeBins();
    markDirty();
}

void HistogramWidget::setBins(const std::vector<float>& bins, float rangeMin, float rangeMax)
{
    bins_     = bins;
    rangeMin_ = rangeMin;
    rangeMax_ = rangeMax;
    rawData_.clear();
    markDirty();
}

void HistogramWidget::clearData()
{
    rawData_.clear();
    bins_.assign(binCount_, 0.0f);
    markDirty();
}

void HistogramWidget::setBinCount(int n)
{
    binCount_ = std::max(1, n);
    recomputeBins();
    markDirty();
}

void HistogramWidget::computeStats()
{
    if (rawData_.empty()) { mean_ = 0; stddev_ = 0; return; }
    double sum = std::accumulate(rawData_.begin(), rawData_.end(), 0.0);
    mean_ = static_cast<float>(sum / rawData_.size());
    double sq = 0;
    for (float v : rawData_) sq += (v - mean_) * (v - mean_);
    stddev_ = static_cast<float>(std::sqrt(sq / rawData_.size()));
}

void HistogramWidget::recomputeBins()
{
    bins_.assign(binCount_, 0.0f);
    if (rawData_.empty() || binCount_ <= 0) return;
    float span = rangeMax_ - rangeMin_;
    if (span < 1e-9f) return;
    for (float v : rawData_) {
        int idx = static_cast<int>((v - rangeMin_) / span * binCount_);
        idx = std::clamp(idx, 0, binCount_ - 1);
        bins_[idx] += 1.0f;
    }
}

void HistogramWidget::paint(PaintContext& ctx)
{
    const Rect b = absoluteRect();

    // Background
    ctx.fill.SetColor(28, 28, 32, 255);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    // Plot area
    Rect pa = { b.x + kPadL, b.y + kPadT, b.w - kPadL - kPadR, b.h - kPadT - kPadB };
    ctx.fill.SetColor(20, 20, 24, 255);
    ctx.fillRect(pa.x, pa.y, pa.w, pa.h);

    // Title
    if (!title_.empty()) {
        float asc = setupFont(ctx, Color(200, 200, 200, 255));
        float tw = ctx.font.GetTextWidth(title_.c_str());
        ctx.font.Print(title_.c_str(), b.x + (b.w - tw) * 0.5f, b.y + 4 + asc);
    }

    if (bins_.empty()) return;

    float maxBin = *std::max_element(bins_.begin(), bins_.end());
    if (maxBin < 1.0f) maxBin = 1.0f;

    ctx.pushClip(pa);

    float barW = pa.w / (float)bins_.size();
    for (int i = 0; i < (int)bins_.size(); ++i) {
        float val = bins_[i];
        float norm;
        if (logScale_) {
            norm = (val > 0) ? std::log1p(val) / std::log1p(maxBin) : 0.0f;
        } else {
            norm = val / maxBin;
        }
        float bh  = norm * pa.h;
        float bx  = pa.x + i * barW;
        float by  = pa.y + pa.h - bh;

        ctx.fill.SetColor(barColor_.r, barColor_.g, barColor_.b, barColor_.a);
        ctx.fillRect(bx + 0.5f, by, barW - 1.0f, bh);

        if (showValues_ && val > 0) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%g", val);
            float asc = setupFont(ctx, Color(220, 220, 220, 200));
            ctx.font.Print(buf, bx + 1, by - 12 + asc);
        }
    }

    // Mean line
    if (showMean_ && !rawData_.empty()) {
        float span = rangeMax_ - rangeMin_;
        if (span > 1e-9f) {
            float mx = pa.x + (mean_ - rangeMin_) / span * pa.w;
            ctx.fill.SetColor(255, 200, 80, 220);
            ctx.fillRect(mx, pa.y, 1, pa.h);
        }
    }

    ctx.popClip();

    // Border
    ctx.fill.SetColor(70, 70, 75, 255);
    ctx.fillRect(pa.x,          pa.y,          pa.w, 1);
    ctx.fillRect(pa.x,          pa.y+pa.h-1,   pa.w, 1);
    ctx.fillRect(pa.x,          pa.y,          1,    pa.h);
    ctx.fillRect(pa.x+pa.w-1,   pa.y,          1,    pa.h);

    // X axis labels
    float ascX = setupFont(ctx, Color(130, 130, 130, 255));
    char bufMin[16], bufMax[16];
    snprintf(bufMin, sizeof(bufMin), "%.4g", rangeMin_);
    snprintf(bufMax, sizeof(bufMax), "%.4g", rangeMax_);
    float maxW = ctx.font.GetTextWidth(bufMax);
    ctx.font.Print(bufMin, pa.x,                   pa.y + pa.h + 4 + ascX);
    ctx.font.Print(bufMax, pa.x + pa.w - maxW,     pa.y + pa.h + 4 + ascX);

    if (!rawData_.empty()) {
        char statBuf[48];
        snprintf(statBuf, sizeof(statBuf), "u=%.3g  s=%.3g", mean_, stddev_);
        float ascS = setupFont(ctx, Color(180, 180, 120, 255));
        ctx.font.Print(statBuf, pa.x + pa.w * 0.3f, pa.y + pa.h + 4 + ascS);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  PlotWidget
// ═════════════════════════════════════════════════════════════════════════════

namespace {
    static constexpr float kPlotPadL = 46.0f;
    static constexpr float kPlotPadR = 10.0f;
    static constexpr float kPlotPadT = 24.0f;
    static constexpr float kPlotPadB = 28.0f;
}

PlotWidget::PlotWidget()
{
    viewXMin_ = 0; viewXMax_ = 10;
    viewYMin_ = 0; viewYMax_ = 1;
}

int PlotWidget::addSeries(const std::string& name, const Color& color, PlotType type)
{
    PlotSeries s;
    s.name  = name;
    s.color = color;
    s.type  = type;
    series_.push_back(std::move(s));
    markDirty();
    return static_cast<int>(series_.size()) - 1;
}

void PlotWidget::removeSeries(int idx)
{
    if (idx >= 0 && idx < (int)series_.size()) {
        series_.erase(series_.begin() + idx);
        markDirty();
    }
}

void PlotWidget::clearSeries()
{
    series_.clear();
    markDirty();
}

void PlotWidget::addPoint(int idx, float y)
{
    if (idx < 0 || idx >= (int)series_.size()) return;
    series_[idx].values.push_back(y);
    if (maxPoints_ > 0 && (int)series_[idx].values.size() > maxPoints_)
        series_[idx].values.erase(series_[idx].values.begin());
    markDirty();
}

void PlotWidget::setPoints(int idx, const std::vector<float>& ys)
{
    if (idx < 0 || idx >= (int)series_.size()) return;
    series_[idx].values = ys;
    markDirty();
}

void PlotWidget::clearPoints(int idx)
{
    if (idx < 0 || idx >= (int)series_.size()) return;
    series_[idx].values.clear();
    markDirty();
}

void PlotWidget::computeAutoRange()
{
    if (!autoY_) return;
    bool first = true;
    float lo = 0, hi = 1;
    for (auto& s : series_) {
        if (!s.visible) continue;
        for (float v : s.values) {
            if (first) { lo = hi = v; first = false; }
            else { lo = std::min(lo, v); hi = std::max(hi, v); }
        }
    }
    if (first) { lo = 0; hi = 1; }
    float span = hi - lo;
    if (span < 1e-6f) span = 1.0f;
    viewYMin_ = lo - span * 0.05f;
    viewYMax_ = hi + span * 0.05f;
}

Rect PlotWidget::plotArea(const Rect& b) const
{
    return { b.x + kPlotPadL, b.y + kPlotPadT, b.w - kPlotPadL - kPlotPadR, b.h - kPlotPadT - kPlotPadB };
}

float PlotWidget::dataToScreenX(float x, const Rect& pa) const
{
    float span = viewXMax_ - viewXMin_;
    if (span < 1e-6f) return pa.x;
    return pa.x + (x - viewXMin_) / span * pa.w;
}

float PlotWidget::dataToScreenY(float y, const Rect& pa) const
{
    float span = viewYMax_ - viewYMin_;
    if (span < 1e-6f) return pa.y + pa.h;
    return pa.y + pa.h - (y - viewYMin_) / span * pa.h;
}

float PlotWidget::screenToDataX(float sx, const Rect& pa) const
{
    return viewXMin_ + (sx - pa.x) / pa.w * (viewXMax_ - viewXMin_);
}

float PlotWidget::screenToDataY(float sy, const Rect& pa) const
{
    return viewYMin_ + (pa.y + pa.h - sy) / pa.h * (viewYMax_ - viewYMin_);
}

void PlotWidget::resetView()
{
    int maxN = 10;
    for (auto& s : series_) maxN = std::max(maxN, (int)s.values.size());
    viewXMin_ = 0; viewXMax_ = (float)maxN;
    computeAutoRange();
    markDirty();
}

void PlotWidget::paintGrid(PaintContext& ctx, const Rect& pa)
{
    // Y grid lines
    float ySpan = viewYMax_ - viewYMin_;
    float yStep = std::pow(10.0f, std::floor(std::log10(ySpan / 5.0f)));
    float yFirst = std::ceil(viewYMin_ / yStep) * yStep;

    for (float y = yFirst; y <= viewYMax_ + yStep * 0.01f; y += yStep) {
        float sy = dataToScreenY(y, pa);
        if (sy < pa.y || sy > pa.y + pa.h) continue;

        ctx.fill.SetColor(55, 55, 60, 255);
        ctx.fillRect(pa.x, sy, pa.w, 1);

        char buf[16];
        if (std::fabs(y) < 1e4f && std::fabs(y) >= 0.01f)
            snprintf(buf, sizeof(buf), "%.4g", y);
        else
            snprintf(buf, sizeof(buf), "%.2e", y);
        float ascY = setupFont(ctx, Color(140, 140, 140, 255));
        ctx.font.Print(buf, pa.x - kPlotPadL + 2, sy - 6 + ascY);
    }

    // X grid lines
    float xSpan = viewXMax_ - viewXMin_;
    float xStep = std::pow(10.0f, std::floor(std::log10(xSpan / 5.0f)));
    float xFirst = std::ceil(viewXMin_ / xStep) * xStep;

    for (float x = xFirst; x <= viewXMax_ + xStep * 0.01f; x += xStep) {
        float sx = dataToScreenX(x, pa);
        if (sx < pa.x || sx > pa.x + pa.w) continue;

        ctx.fill.SetColor(55, 55, 60, 255);
        ctx.fillRect(sx, pa.y, 1, pa.h);

        char buf[16];
        snprintf(buf, sizeof(buf), "%.4g", x);
        float ascXl = setupFont(ctx, Color(140, 140, 140, 255));
        ctx.font.Print(buf, sx - 10, pa.y + pa.h + 4 + ascXl);
    }
}

void PlotWidget::paintSeries(PaintContext& ctx, const Rect& pa)
{
    for (auto& s : series_) {
        if (!s.visible || s.values.empty()) continue;
        int n = (int)s.values.size();

        if (s.type == PlotType::Bar) {
            float barW = std::max(1.0f, pa.w / (float)n - 1.0f);
            for (int i = 0; i < n; ++i) {
                float xi = (float)i;
                float sx = dataToScreenX(xi, pa);
                float sy = dataToScreenY(s.values[i], pa);
                float sy0 = dataToScreenY(0.0f, pa);
                sy0 = std::clamp(sy0, pa.y, pa.y + pa.h);
                float top  = std::min(sy, sy0);
                float h    = std::fabs(sy0 - sy);
                if (sx < pa.x || sx > pa.x + pa.w) continue;
                ctx.fill.SetColor(s.color.r, s.color.g, s.color.b, 180);
                ctx.fillRect(sx - barW * 0.5f, top, barW, h);
            }
        } else if (s.type == PlotType::Scatter) {
            for (int i = 0; i < n; ++i) {
                float sx = dataToScreenX((float)i, pa);
                float sy = dataToScreenY(s.values[i], pa);
                if (sx < pa.x || sx > pa.x + pa.w) continue;
                ctx.fill.SetColor(s.color.r, s.color.g, s.color.b, 220);
                ctx.fillCircle(sx, sy, 3);
            }
        } else {
            // Line
            for (int i = 0; i < n - 1; ++i) {
                float sx0 = dataToScreenX((float)i,   pa);
                float sy0 = dataToScreenY(s.values[i], pa);
                float sx1 = dataToScreenX((float)(i+1), pa);
                float sy1 = dataToScreenY(s.values[i+1], pa);

                if (sx1 < pa.x || sx0 > pa.x + pa.w) continue;

                float dx = sx1 - sx0, dy = sy1 - sy0;
                float len = std::sqrt(dx*dx + dy*dy);
                if (len < 0.5f) continue;
                float nx = -dy / len * 1.5f;
                float ny =  dx / len * 1.5f;

                ctx.fill.SetColor(s.color.r, s.color.g, s.color.b, s.color.a);
                ctx.fillTriangle(sx0+nx, sy0+ny, sx0-nx, sy0-ny, sx1+nx, sy1+ny);
                ctx.fillTriangle(sx0-nx, sy0-ny, sx1-nx, sy1-ny, sx1+nx, sy1+ny);
            }
        }
    }
}

void PlotWidget::paintLegend(PaintContext& ctx, const Rect& b)
{
    float lx = b.x + kPlotPadL + 6;
    float ly = b.y + kPlotPadT + 6;

    for (auto& s : series_) {
        if (s.name.empty()) continue;
        ctx.fill.SetColor(s.color.r, s.color.g, s.color.b, 200);
        ctx.fillRect(lx, ly + 3, 12, 3);
        float ascL = setupFont(ctx, Color(200, 200, 200, 255));
        ctx.font.Print(s.name.c_str(), lx + 16, ly + ascL);
        ly += 14;
    }
}

void PlotWidget::paintAxes(PaintContext& ctx, const Rect& b, const Rect& pa)
{
    // Border around plot area
    ctx.fill.SetColor(70, 70, 75, 255);
    ctx.fillRect(pa.x,          pa.y,          pa.w, 1);
    ctx.fillRect(pa.x,          pa.y+pa.h-1,   pa.w, 1);
    ctx.fillRect(pa.x,          pa.y,          1,    pa.h);
    ctx.fillRect(pa.x+pa.w-1,   pa.y,          1,    pa.h);

    // Title
    if (!title_.empty()) {
        float ascT = setupFont(ctx, Color(210, 210, 210, 255));
        float tw = ctx.font.GetTextWidth(title_.c_str());
        ctx.font.Print(title_.c_str(), b.x + (b.w - tw) * 0.5f, b.y + 4 + ascT);
    }

    // X label
    if (!xLabel_.empty()) {
        float ascXL = setupFont(ctx, Color(160, 160, 160, 255));
        float xlW = ctx.font.GetTextWidth(xLabel_.c_str());
        ctx.font.Print(xLabel_.c_str(), pa.x + (pa.w - xlW) * 0.5f, b.y + b.h - 14 + ascXL);
    }
}

void PlotWidget::paint(PaintContext& ctx)
{
    const Rect b  = absoluteRect();
    const Rect pa = plotArea(b);
    plotArea_ = pa;

    computeAutoRange();

    // Background
    ctx.fill.SetColor(28, 28, 32, 255);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    // Plot area background
    ctx.fill.SetColor(20, 20, 24, 255);
    ctx.fillRect(pa.x, pa.y, pa.w, pa.h);

    ctx.pushClip(pa);
    if (showGrid_)  paintGrid(ctx, pa);
    paintSeries(ctx, pa);
    ctx.popClip();

    paintAxes(ctx, b, pa);
    if (showLegend_) paintLegend(ctx, b);
}

void PlotWidget::onMousePress(MouseEvent& e)
{
    if (e.button == 1) {
        panning_   = true;
        panStartX_ = e.x; panStartY_ = e.y;
        panViewX0_ = viewXMin_; panViewY0_ = viewYMin_;
    }
}

void PlotWidget::onMouseRelease(MouseEvent& e)
{
    (void)e;
    panning_ = false;
}

void PlotWidget::onMouseMove(MouseEvent& e)
{
    if (!panning_) return;
    const Rect pa = plotArea_;
    if (pa.w < 1 || pa.h < 1) return;

    float dxData = -(e.x - panStartX_) / pa.w * (viewXMax_ - viewXMin_);
    float dyData =  (e.y - panStartY_) / pa.h * (viewYMax_ - viewYMin_);

    float spanX = viewXMax_ - viewXMin_;
    float spanY = viewYMax_ - viewYMin_;
    viewXMin_ = panViewX0_ + dxData;
    viewXMax_ = viewXMin_ + spanX;
    viewYMin_ = panViewY0_ + dyData;
    viewYMax_ = viewYMin_ + spanY;
    autoY_    = false;
    markDirty();
}

void PlotWidget::onMouseScroll(MouseEvent& e)
{
    const Rect pa = plotArea_;
    if (pa.w < 1) return;

    float factor = (e.scrollY > 0) ? 0.85f : 1.0f / 0.85f;

    float mx = screenToDataX(e.x, pa);
    float my = screenToDataY(e.y, pa);

    viewXMin_ = mx + (viewXMin_ - mx) * factor;
    viewXMax_ = mx + (viewXMax_ - mx) * factor;
    viewYMin_ = my + (viewYMin_ - my) * factor;
    viewYMax_ = my + (viewYMax_ - my) * factor;
    autoY_    = false;
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  CurveEditor
// ═════════════════════════════════════════════════════════════════════════════

CurveEditor::CurveEditor()
{
}

int CurveEditor::addCurve(const std::string& name, const Color& color)
{
    curves_.push_back({name, color, {}, true});
    markDirty();
    return static_cast<int>(curves_.size()) - 1;
}

void CurveEditor::removeCurve(int curveId)
{
    if (curveId >= 0 && curveId < (int)curves_.size()) {
        curves_.erase(curves_.begin() + curveId);
        markDirty();
    }
}

void CurveEditor::clearCurves()
{
    curves_.clear();
    markDirty();
}

int CurveEditor::addKey(int curveId, float time, float value)
{
    if (curveId < 0 || curveId >= (int)curves_.size()) return -1;

    auto& keys = curves_[curveId].keys;
    CurveKey k;
    k.time = time;
    k.value = value;

    auto it = std::lower_bound(keys.begin(), keys.end(), k,
        [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
    int idx = static_cast<int>(it - keys.begin());
    keys.insert(it, k);

    autoTangents(curves_[curveId], idx);
    if (idx > 0) autoTangents(curves_[curveId], idx - 1);
    if (idx + 1 < (int)keys.size()) autoTangents(curves_[curveId], idx + 1);

    markDirty();
    onKeyAdded.emit(curveId, idx);
    return idx;
}

void CurveEditor::removeKey(int curveId, int keyIdx)
{
    if (curveId < 0 || curveId >= (int)curves_.size()) return;
    auto& keys = curves_[curveId].keys;
    if (keyIdx < 0 || keyIdx >= (int)keys.size()) return;
    keys.erase(keys.begin() + keyIdx);
    markDirty();
    onKeyRemoved.emit(curveId, keyIdx);
}

void CurveEditor::setKey(int curveId, int keyIdx, float time, float value)
{
    if (curveId < 0 || curveId >= (int)curves_.size()) return;
    auto& keys = curves_[curveId].keys;
    if (keyIdx < 0 || keyIdx >= (int)keys.size()) return;
    keys[keyIdx].time = time;
    keys[keyIdx].value = value;
    autoTangents(curves_[curveId], keyIdx);
    markDirty();
    onKeyChanged.emit(curveId, keyIdx, time, value);
}

void CurveEditor::setViewRange(float minT, float maxT, float minV, float maxV)
{
    viewMinT_ = minT; viewMaxT_ = maxT;
    viewMinV_ = minV; viewMaxV_ = maxV;
    markDirty();
}

void CurveEditor::fitView()
{
    if (curves_.empty()) return;
    float tMin = 1e9f, tMax = -1e9f, vMin = 1e9f, vMax = -1e9f;
    for (auto& c : curves_) {
        for (auto& k : c.keys) {
            tMin = std::min(tMin, k.time);
            tMax = std::max(tMax, k.time);
            vMin = std::min(vMin, k.value);
            vMax = std::max(vMax, k.value);
        }
    }
    float tPad = std::max(0.1f, (tMax - tMin) * 0.1f);
    float vPad = std::max(0.1f, (vMax - vMin) * 0.1f);
    setViewRange(tMin - tPad, tMax + tPad, vMin - vPad, vMax + vPad);
}

float CurveEditor::timeToX(float t) const
{
    Rect b = absoluteRect();
    float margin = 40;
    return b.x + margin + (t - viewMinT_) / (viewMaxT_ - viewMinT_) * (b.w - margin * 2);
}

float CurveEditor::valueToY(float v) const
{
    Rect b = absoluteRect();
    float margin = 24;
    return b.y + b.h - margin - (v - viewMinV_) / (viewMaxV_ - viewMinV_) * (b.h - margin * 2);
}

float CurveEditor::xToTime(float x) const
{
    Rect b = absoluteRect();
    float margin = 40;
    return viewMinT_ + (x - b.x - margin) / (b.w - margin * 2) * (viewMaxT_ - viewMinT_);
}

float CurveEditor::yToValue(float y) const
{
    Rect b = absoluteRect();
    float margin = 24;
    return viewMinV_ + (b.y + b.h - margin - y) / (b.h - margin * 2) * (viewMaxV_ - viewMinV_);
}

float CurveEditor::snap(float val, float grid) const
{
    if (grid <= 0) return val;
    return std::round(val / grid) * grid;
}

void CurveEditor::autoTangents(CurveData& c, int idx)
{
    auto& keys = c.keys;
    if (idx < 0 || idx >= (int)keys.size()) return;
    auto& k = keys[idx];
    if (k.tangentMode != TangentMode::Auto) return;

    if (keys.size() <= 1) {
        k.tanInX = -0.1f; k.tanInY = 0;
        k.tanOutX = 0.1f; k.tanOutY = 0;
        return;
    }

    float dtPrev = 0, dvPrev = 0, dtNext = 0, dvNext = 0;
    if (idx > 0) {
        dtPrev = k.time - keys[idx - 1].time;
        dvPrev = k.value - keys[idx - 1].value;
    }
    if (idx + 1 < (int)keys.size()) {
        dtNext = keys[idx + 1].time - k.time;
        dvNext = keys[idx + 1].value - k.value;
    }

    float slope = 0;
    if (idx > 0 && idx + 1 < (int)keys.size()) {
        float dt = keys[idx + 1].time - keys[idx - 1].time;
        float dv = keys[idx + 1].value - keys[idx - 1].value;
        if (std::fabs(dt) > 1e-6f) slope = dv / dt;
    } else if (idx > 0) {
        if (std::fabs(dtPrev) > 1e-6f) slope = dvPrev / dtPrev;
    } else if (idx + 1 < (int)keys.size()) {
        if (std::fabs(dtNext) > 1e-6f) slope = dvNext / dtNext;
    }

    float tanLen = 0.33f;
    k.tanInX  = -(dtPrev > 0 ? dtPrev : 0.1f) * tanLen;
    k.tanInY  = k.tanInX * slope;
    k.tanOutX =  (dtNext > 0 ? dtNext : 0.1f) * tanLen;
    k.tanOutY = k.tanOutX * slope;
}

float CurveEditor::evalSegment(const CurveKey& k0, const CurveKey& k1, float t) const
{
    float dt = k1.time - k0.time;
    if (std::fabs(dt) < 1e-8f) return k0.value;

    float u = (t - k0.time) / dt;
    u = std::max(0.0f, std::min(1.0f, u));

    float p0 = k0.value;
    float p1 = k0.value + k0.tanOutY;
    float p2 = k1.value + k1.tanInY;
    float p3 = k1.value;

    float iu = 1.0f - u;
    return iu*iu*iu*p0 + 3*iu*iu*u*p1 + 3*iu*u*u*p2 + u*u*u*p3;
}

float CurveEditor::evaluate(int curveId, float time) const
{
    if (curveId < 0 || curveId >= (int)curves_.size()) return 0;
    auto& keys = curves_[curveId].keys;
    if (keys.empty()) return 0;
    if (keys.size() == 1) return keys[0].value;

    if (time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time)  return keys.back().value;

    for (int i = 0; i + 1 < (int)keys.size(); ++i) {
        if (time >= keys[i].time && time <= keys[i + 1].time)
            return evalSegment(keys[i], keys[i + 1], time);
    }
    return keys.back().value;
}

CurveEditor::HitResult CurveEditor::hitTest(float mx, float my) const
{
    float hitR = 8.0f;
    float tanHitR = 6.0f;

    for (int ci = 0; ci < (int)curves_.size(); ++ci) {
        auto& c = curves_[ci];
        if (!c.visible) continue;
        for (int ki = 0; ki < (int)c.keys.size(); ++ki) {
            auto& k = c.keys[ki];
            float kx = timeToX(k.time);
            float ky = valueToY(k.value);

            if (k.selected) {
                float tix = timeToX(k.time + k.tanInX);
                float tiy = valueToY(k.value + k.tanInY);
                if ((mx - tix) * (mx - tix) + (my - tiy) * (my - tiy) < tanHitR * tanHitR)
                    return {ci, ki, true, true};

                float tox = timeToX(k.time + k.tanOutX);
                float toy = valueToY(k.value + k.tanOutY);
                if ((mx - tox) * (mx - tox) + (my - toy) * (my - toy) < tanHitR * tanHitR)
                    return {ci, ki, true, false};
            }

            if (std::fabs(mx - kx) + std::fabs(my - ky) < hitR)
                return {ci, ki, false, false};
        }
    }
    return {};
}

void CurveEditor::onMousePress(MouseEvent& e)
{
    if (e.button == 1) {
        panning_ = true;
        dragStartX_ = e.x;
        dragStartY_ = e.y;
        panStartMinT_ = viewMinT_;
        panStartMaxT_ = viewMaxT_;
        panStartMinV_ = viewMinV_;
        panStartMaxV_ = viewMaxV_;
        e.consumed = true;
        return;
    }

    if (e.button != 0) return;

    auto hit = hitTest(e.x, e.y);

    if (hit.curve >= 0) {
        auto& keys = curves_[hit.curve].keys;
        for (auto& c : curves_)
            for (auto& k : c.keys)
                k.selected = false;
        keys[hit.key].selected = true;

        if (hit.tangent) {
            dragTangent_ = true;
            dragTanIn_   = hit.tanIn;
        } else {
            dragTangent_ = false;
        }

        dragging_   = true;
        dragCurve_  = hit.curve;
        dragKey_    = hit.key;
        dragStartX_ = e.x;
        dragStartY_ = e.y;
        e.consumed  = true;
    } else {
        for (auto& c : curves_)
            for (auto& k : c.keys)
                k.selected = false;
        markDirty();
    }
}

void CurveEditor::onMouseRelease(MouseEvent& e)
{
    if (panning_) {
        panning_ = false;
        e.consumed = true;
        return;
    }
    if (dragging_) {
        dragging_ = false;
        dragCurve_ = -1;
        dragKey_ = -1;
        e.consumed = true;
    }
}

void CurveEditor::onMouseMove(MouseEvent& e)
{
    if (panning_) {
        float dx = e.x - dragStartX_;
        float dy = e.y - dragStartY_;

        Rect b = absoluteRect();
        float margin = 40;
        float tPerPx = (panStartMaxT_ - panStartMinT_) / (b.w - margin * 2);
        float vPerPx = (panStartMaxV_ - panStartMinV_) / (b.h - 48);

        viewMinT_ = panStartMinT_ - dx * tPerPx;
        viewMaxT_ = panStartMaxT_ - dx * tPerPx;
        viewMinV_ = panStartMinV_ + dy * vPerPx;
        viewMaxV_ = panStartMaxV_ + dy * vPerPx;
        markDirty();
        e.consumed = true;
        return;
    }

    if (!dragging_) return;
    e.consumed = true;

    float t = xToTime(e.x);
    float v = yToValue(e.y);

    if (snapTime_ > 0)  t = snap(t, snapTime_);
    if (snapValue_ > 0) v = snap(v, snapValue_);

    auto& k = curves_[dragCurve_].keys[dragKey_];

    if (dragTangent_) {
        float dt = t - k.time;
        float dv = v - k.value;
        if (dragTanIn_) {
            k.tanInX = dt;
            k.tanInY = dv;
            if (k.tangentMode == TangentMode::Aligned) {
                float len = std::sqrt(k.tanOutX * k.tanOutX + k.tanOutY * k.tanOutY);
                float inLen = std::sqrt(dt * dt + dv * dv);
                if (inLen > 1e-6f) {
                    k.tanOutX = -dt / inLen * len;
                    k.tanOutY = -dv / inLen * len;
                }
            }
        } else {
            k.tanOutX = dt;
            k.tanOutY = dv;
            if (k.tangentMode == TangentMode::Aligned) {
                float len = std::sqrt(k.tanInX * k.tanInX + k.tanInY * k.tanInY);
                float outLen = std::sqrt(dt * dt + dv * dv);
                if (outLen > 1e-6f) {
                    k.tanInX = -dt / outLen * len;
                    k.tanInY = -dv / outLen * len;
                }
            }
        }
        k.tangentMode = (k.tangentMode == TangentMode::Auto) ? TangentMode::Free : k.tangentMode;
    } else {
        k.time = t;
        k.value = v;
        autoTangents(curves_[dragCurve_], dragKey_);
    }

    markDirty();
    onKeyChanged.emit(dragCurve_, dragKey_, k.time, k.value);
}

void CurveEditor::onMouseScroll(MouseEvent& e)
{
    float mx = e.x, my = e.y;
    float tAtMouse = xToTime(mx);
    float vAtMouse = yToValue(my);

    float zoomFactor = (e.scrollY > 0) ? 0.9f : 1.1f;

    viewMinT_ = tAtMouse + (viewMinT_ - tAtMouse) * zoomFactor;
    viewMaxT_ = tAtMouse + (viewMaxT_ - tAtMouse) * zoomFactor;
    viewMinV_ = vAtMouse + (viewMinV_ - vAtMouse) * zoomFactor;
    viewMaxV_ = vAtMouse + (viewMaxV_ - vAtMouse) * zoomFactor;

    markDirty();
    e.consumed = true;
}

void CurveEditor::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect b = absoluteRect();

    ctx.pushClip(b);

    // Background
    ctx.fill.SetColor(bgColor_.r, bgColor_.g, bgColor_.b, bgColor_.a);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    paintGrid(ctx, b);
    paintCurves(ctx, b);
    paintKeys(ctx, b);
    if (showPlayhead_) paintPlayhead(ctx, b);

    // Border
    ctx.fill.SetColor(50, 52, 58, 255);
    ctx.fillRect(b.x,           b.y,            b.w, 1);
    ctx.fillRect(b.x,           b.y + b.h - 1,  b.w, 1);
    ctx.fillRect(b.x,           b.y,            1,   b.h);
    ctx.fillRect(b.x + b.w - 1, b.y,            1,   b.h);

    ctx.popClip();

    Widget::paint(ctx);
}

void CurveEditor::paintGrid(PaintContext& ctx, const Rect& b)
{
    auto niceStep = [](float range, int maxLines) -> float {
        float rough = range / maxLines;
        float mag = std::pow(10.0f, std::floor(std::log10(rough)));
        float norm = rough / mag;
        if (norm < 1.5f) return mag;
        if (norm < 3.5f) return 2 * mag;
        if (norm < 7.5f) return 5 * mag;
        return 10 * mag;
    };

    float tStep = niceStep(viewMaxT_ - viewMinT_, 10);
    float vStep = niceStep(viewMaxV_ - viewMinV_, 8);

    // Time grid (vertical lines)
    float tStart = std::floor(viewMinT_ / tStep) * tStep;
    for (float t = tStart; t <= viewMaxT_; t += tStep) {
        float x = timeToX(t);
        if (x < b.x + 40 || x > b.x + b.w - 40) continue;

        bool isZero = std::fabs(t) < tStep * 0.01f;
        ctx.fill.SetColor(isZero ? 80 : 45, isZero ? 80 : 47, isZero ? 80 : 52, 255);
        ctx.fillRect(x, b.y, 1, b.h - 24);

        char buf[16];
        snprintf(buf, sizeof(buf), "%.2g", t);
        float ascG = setupFont(ctx, Color(120, 122, 128, 200), 10.0f);
        ctx.font.Print(buf, x - 8, b.y + b.h - 4);
    }

    // Value grid (horizontal lines)
    float vStart = std::floor(viewMinV_ / vStep) * vStep;
    for (float v = vStart; v <= viewMaxV_; v += vStep) {
        float y = valueToY(v);
        if (y < b.y || y > b.y + b.h - 24) continue;

        bool isZero = std::fabs(v) < vStep * 0.01f;
        ctx.fill.SetColor(isZero ? 80 : 45, isZero ? 80 : 47, isZero ? 80 : 52, 255);
        ctx.fillRect(b.x + 40, y, b.w - 80, 1);

        char buf[16];
        snprintf(buf, sizeof(buf), "%.2g", v);
        float ascV = setupFont(ctx, Color(120, 122, 128, 200), 10.0f);
        ctx.font.Print(buf, b.x + 4, y - 5 + ascV);
    }
}

void CurveEditor::paintCurves(PaintContext& ctx, const Rect& b)
{
    int segments = std::max(60, static_cast<int>(b.w / 3));

    for (int ci = 0; ci < (int)curves_.size(); ++ci) {
        auto& c = curves_[ci];
        if (!c.visible || c.keys.size() < 2) continue;

        Color col = c.color;
        float prevX = 0, prevY = 0;
        bool first = true;

        for (int s = 0; s <= segments; ++s) {
            float t = viewMinT_ + (viewMaxT_ - viewMinT_) * (float(s) / segments);
            float v = evaluate(ci, t);
            float x = timeToX(t);
            float y = valueToY(v);

            if (!first) {
                float dx = x - prevX, dy = y - prevY;
                float len = std::sqrt(dx * dx + dy * dy);
                if (len > 0.5f) {
                    float thick = 1.5f;
                    float nx = -dy / len * thick * 0.5f;
                    float ny =  dx / len * thick * 0.5f;
                    ctx.fill.SetColor(col.r, col.g, col.b, col.a);
                    ctx.fillTriangle(prevX+nx, prevY+ny, prevX-nx, prevY-ny, x-nx, y-ny);
                    ctx.fillTriangle(prevX+nx, prevY+ny, x-nx, y-ny, x+nx, y+ny);
                }
            }
            prevX = x; prevY = y; first = false;
        }
    }
}

void CurveEditor::paintKeys(PaintContext& ctx, const Rect& b)
{
    for (int ci = 0; ci < (int)curves_.size(); ++ci) {
        auto& c = curves_[ci];
        if (!c.visible) continue;

        for (int ki = 0; ki < (int)c.keys.size(); ++ki) {
            auto& k = c.keys[ki];
            float kx = timeToX(k.time);
            float ky = valueToY(k.value);

            // Tangent handles (only for selected keys)
            if (k.selected) {
                float tix = timeToX(k.time + k.tanInX);
                float tiy = valueToY(k.value + k.tanInY);
                float tox = timeToX(k.time + k.tanOutX);
                float toy = valueToY(k.value + k.tanOutY);

                ctx.fill.SetColor(c.color.r, c.color.g, c.color.b, 120);
                ctx.fillTriangle(tix - 0.5f, tiy, tix + 0.5f, tiy, kx + 0.5f, ky);
                ctx.fillTriangle(tix - 0.5f, tiy, kx - 0.5f,  ky,  kx + 0.5f, ky);
                ctx.fillTriangle(kx - 0.5f, ky, kx + 0.5f, ky, tox + 0.5f, toy);
                ctx.fillTriangle(kx - 0.5f, ky, tox - 0.5f, toy, tox + 0.5f, toy);

                ctx.fill.SetColor(c.color.r, c.color.g, c.color.b, 200);
                ctx.fillCircle(tix, tiy, 3);
                ctx.fillCircle(tox, toy, 3);
            }

            // Key diamond
            float ds = k.selected ? 6.0f : 5.0f;
            float ods = ds + 1.0f;
            Color oc(255, 255, 255, k.selected ? 255 : 80);
            ctx.fill.SetColor(oc.r, oc.g, oc.b, oc.a);
            ctx.fillTriangle(kx, ky - ods, kx + ods, ky, kx, ky + ods);
            ctx.fillTriangle(kx, ky - ods, kx, ky + ods, kx - ods, ky);

            Color kc = k.selected ? Color(255, 200, 60, 255) : c.color;
            ctx.fill.SetColor(kc.r, kc.g, kc.b, kc.a);
            ctx.fillTriangle(kx, ky - ds, kx + ds, ky, kx, ky + ds);
            ctx.fillTriangle(kx, ky - ds, kx, ky + ds, kx - ds, ky);
        }
    }
}

void CurveEditor::paintPlayhead(PaintContext& ctx, const Rect& b)
{
    float x = timeToX(playhead_);
    if (x < b.x + 40 || x > b.x + b.w - 40) return;

    ctx.fill.SetColor(255, 80, 80, 200);
    ctx.fillRect(x, b.y, 1, b.h - 24);
    ctx.fill.SetColor(255, 80, 80, 220);
    ctx.fillTriangle(x - 5, b.y, x + 5, b.y, x, b.y + 8);
}
