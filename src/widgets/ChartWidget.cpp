#include "ChartWidget.hpp"
#include "Theme.hpp"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace ig::retained
{

namespace
{

constexpr float kPi = 3.14159265358979f;
constexpr float kPad = 8.0f;          // inner margin of the widget
constexpr float kSwatch = 10.0f;      // legend colour square
constexpr float kLegendGap = 6.0f;    // between swatch and label, between entries
constexpr float kAxisPad = 6.0f;      // between the axis labels and the plot
constexpr float kHoverPop = 4.0f;     // how far a hovered slice moves out
constexpr float kLeader = 10.0f;      // length of the leader line past the rim

float setupFont(PaintContext& ctx, const Color& color)
{
    ctx.font.SetBatch(&ctx.text);
    ctx.font.SetColor(color);
    return ctx.font.GetAscender();
}

float lineHeight(const PaintContext& ctx)
{
    return ctx.font.GetFontSize() * 1.25f;
}

Color lighten(const Color& c, float amount)
{
    return c.Lerp(Color(255, 255, 255, c.a), amount);
}

Color muted(const Color& c)
{
    return Color(c.r, c.g, c.b, 90);
}

// A label truncated with an ellipsis to fit a width. Returns the text drawn.
void fitText(const PaintContext& ctx, const char* text, float maxWidth, char* out, int outSize)
{
    const int len = static_cast<int>(strlen(text));
    if (len < outSize && ctx.font.GetTextWidth(text) <= maxWidth)
    {
        memcpy(out, text, static_cast<size_t>(len) + 1);
        return;
    }
    const float ellipsisW = ctx.font.GetTextWidth("...");
    int keep = len < outSize - 4 ? len : outSize - 4;
    while (keep > 0)
    {
        // Never cut a UTF-8 sequence in the middle.
        while (keep > 0 && (static_cast<unsigned char>(text[keep]) & 0xC0u) == 0x80u)
            --keep;
        memcpy(out, text, static_cast<size_t>(keep));
        out[keep] = '\0';
        if (ctx.font.GetTextWidth(out) + ellipsisW <= maxWidth)
            break;
        --keep;
    }
    memcpy(out + keep, "...", 4);
    if (keep == 0 && maxWidth < ellipsisW)
        out[0] = '\0';
}

// Decimals needed so a step prints without losing its digits: the step is multiplied by ten until it is a whole number,
// give or take a ten thousandth of itself (so the noise of a float that went in as a number of the data, 0.30000001,
// is not a digit). Up to nine decimals, for a step of nanoseconds. (It was bounded by a ten thousandth of 1 + |s| and by six
// decimals: a step of 0.0001 or less had none.)
int decimalsFor(double step)
{
    int decimals = 0;
    double s = step;
    while (decimals < 9 && fabs(s - round(s)) > 1e-4 * fabs(s))
    {
        s *= 10.0;
        ++decimals;
    }
    return decimals;
}

// Points of a slice edge at an angle measured clockwise from the top.
inline void polar(float cx, float cy, float r, float angle, float& x, float& y)
{
    x = cx + sinf(angle) * r;
    y = cy - cosf(angle) * r;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
//  Data
// ═════════════════════════════════════════════════════════════════════════════

const ct::Vector<Color>& Chart::defaultPalette()
{
    static ct::Vector<Color> palette;
    if (palette.empty())
    {
        // Tableau 10: distinct in hue and lightness, readable for most forms
        // of colour blindness, calm on a dark panel.
        palette.push_back(Color(78, 121, 167));
        palette.push_back(Color(242, 142, 43));
        palette.push_back(Color(225, 87, 89));
        palette.push_back(Color(118, 183, 178));
        palette.push_back(Color(89, 161, 79));
        palette.push_back(Color(237, 201, 72));
        palette.push_back(Color(176, 122, 161));
        palette.push_back(Color(255, 157, 167));
        palette.push_back(Color(156, 117, 95));
        palette.push_back(Color(186, 176, 172));
    }
    return palette;
}

Chart::Chart()
{
    palette_ = defaultPalette();
}

void Chart::setKind(ChartKind kind)
{
    if (kind_ == kind)
        return;
    kind_ = kind;
    hovered_ = Item();
    markDirty();
}

void Chart::setCategories(const ct::Vector<String>& labels)
{
    categories_ = labels;
    categoryHidden_.assign(labels.size(), false);
    hovered_ = Item();
    markDirty();
}

int Chart::addSeries(const String& name, const ct::Vector<float>& values)
{
    return addSeriesT(name, values, nullptr);
}

int Chart::addSeries(const String& name, const ct::Vector<float>& values, const Color& color)
{
    return addSeriesT(name, values, &color);
}

int Chart::addSeries(const String& name, const ct::Vector<float>& xs, const ct::Vector<float>& ys)
{
    return addSeriesXYT(name, xs, ys);
}

void Chart::setSeriesValues(int series, const ct::Vector<float>& values)
{
    setSeriesValuesT(series, values);
}

void Chart::setSeriesX(int series, const ct::Vector<float>& xs)
{
    setSeriesXT(series, xs);
}

namespace
{
// The numbers of a vector of float or double as doubles, in a vector that keeps its room: giving a series new numbers of the
// same count again and again allocates nothing, and a float is widened in the same pass that copies it.
template<typename T>
void assignDoubles(ct::Vector<double>& dst, const ct::Vector<T>& src)
{
    const size_t n = src.size();
    dst.resize(n);
    double* out = dst.data();
    const T* in = src.data();
    for (size_t i = 0; i < n; ++i)
        out[i] = static_cast<double>(in[i]);
}
} // namespace

template<typename T>
int Chart::addSeriesT(const String& name, const ct::Vector<T>& values, const Color* color)
{
    const size_t n = series_.size();
    series_.push_back(ChartSeries());
    ChartSeries& s = series_.back();
    s.name = name;
    assignDoubles(s.values, values);
    s.color = color ? *color : (palette_.empty() ? Color(160, 160, 165) : palette_[n % palette_.size()]);
    if (categories_.size() < values.size())
    {
        // Values beyond the labels get numbered labels so nothing is lost.
        char buf[16];
        for (size_t i = categories_.size(); i < values.size(); ++i)
        {
            snprintf(buf, sizeof(buf), "%d", static_cast<int>(i) + 1);
            categories_.push_back(String(buf));
            categoryHidden_.push_back(false);
        }
    }
    markDirty();
    return static_cast<int>(series_.size()) - 1;
}

template<typename TX, typename TY>
int Chart::addSeriesXYT(const String& name, const ct::Vector<TX>& xs, const ct::Vector<TY>& ys)
{
    const int idx = addSeriesT(name, ys, nullptr);
    assignDoubles(series_[static_cast<size_t>(idx)].xs, xs);
    return idx;
}

template<typename T>
void Chart::setSeriesValuesT(int series, const ct::Vector<T>& values)
{
    if (series < 0 || series >= seriesCount())
        return;
    assignDoubles(series_[static_cast<size_t>(series)].values, values);
    markDirty();
}

template<typename T>
void Chart::setSeriesXT(int series, const ct::Vector<T>& xs)
{
    if (series < 0 || series >= seriesCount())
        return;
    assignDoubles(series_[static_cast<size_t>(series)].xs, xs);
    markDirty();
}

void Chart::setSeriesStyle(int series, bool line, bool markers)
{
    if (series < 0 || series >= seriesCount())
        return;
    series_[static_cast<size_t>(series)].line = line;
    series_[static_cast<size_t>(series)].markers = markers;
    markDirty();
}

void Chart::setSeriesName(int series, const String& name)
{
    if (series < 0 || series >= seriesCount())
        return;
    series_[static_cast<size_t>(series)].name = name;
    markDirty();
}

void Chart::setSeriesColor(int series, const Color& color)
{
    if (series < 0 || series >= seriesCount())
        return;
    series_[static_cast<size_t>(series)].color = color;
    markDirty();
}

void Chart::setSeriesVisible(int series, bool visible)
{
    if (series < 0 || series >= seriesCount())
        return;
    series_[static_cast<size_t>(series)].visible = visible;
    hovered_ = Item();
    markDirty();
}

void Chart::removeSeries(int series)
{
    if (series < 0 || series >= seriesCount())
        return;
    series_.erase(series_.begin() + series);
    hovered_ = Item();
    markDirty();
}

void Chart::setCategoryVisible(int index, bool visible)
{
    if (index < 0 || index >= categoryCount())
        return;
    categoryHidden_[static_cast<size_t>(index)] = !visible;
    hovered_ = Item();
    markDirty();
}

bool Chart::categoryVisible(int index) const
{
    if (index < 0 || index >= categoryCount())
        return false;
    return !categoryHidden_[static_cast<size_t>(index)];
}

void Chart::clear()
{
    categories_.clear();
    categoryHidden_.clear();
    series_.clear();
    legend_.clear();
    slices_.clear();
    bars_.clear();
    points_.clear();
    edges_.clear();
    edgesF_.clear();
    hovered_ = Item();
    markDirty();
}

void Chart::pie(const ct::Vector<String>& labels, const ct::Vector<float>& values)
{
    pieT(labels, values);
}

void Chart::donut(const ct::Vector<String>& labels, const ct::Vector<float>& values)
{
    pieT(labels, values);
    kind_ = ChartKind::Donut;
}

void Chart::bars(const ct::Vector<String>& labels, const ct::Vector<float>& values, const String& name)
{
    barsT(labels, values, name);
}

int Chart::plot(const ct::Vector<float>& xs, const ct::Vector<float>& ys, const String& name)
{
    return plotT(xs, ys, name);
}

int Chart::scatter(const ct::Vector<float>& xs, const ct::Vector<float>& ys, const String& name)
{
    return scatterT(xs, ys, name);
}

int Chart::hist(const ct::Vector<float>& samples, int bins, const String& name)
{
    return histT(samples, bins, name);
}

template<typename T>
void Chart::pieT(const ct::Vector<String>& labels, const ct::Vector<T>& values)
{
    clear();
    kind_ = ChartKind::Pie;
    setCategories(labels);
    addSeriesT(String(), values, nullptr);
}

template<typename T>
void Chart::barsT(const ct::Vector<String>& labels, const ct::Vector<T>& values, const String& name)
{
    clear();
    kind_ = ChartKind::Bars;
    setCategories(labels);
    addSeriesT(name, values, nullptr);
}

template<typename TX, typename TY>
int Chart::plotT(const ct::Vector<TX>& xs, const ct::Vector<TY>& ys, const String& name)
{
    // An empty chart takes the kind; a line or scatter chart gets one more
    // series; anything else is replaced.
    if (!isXY() || series_.empty())
    {
        clear();
        kind_ = ChartKind::Line;
    }
    const int idx = addSeriesXYT(name, xs, ys);
    series_[static_cast<size_t>(idx)].line = true;
    series_[static_cast<size_t>(idx)].markers = true;
    return idx;
}

template<typename TX, typename TY>
int Chart::scatterT(const ct::Vector<TX>& xs, const ct::Vector<TY>& ys, const String& name)
{
    if (!isXY() || series_.empty())
    {
        clear();
        kind_ = ChartKind::Scatter;
    }
    const int idx = addSeriesXYT(name, xs, ys);
    series_[static_cast<size_t>(idx)].line = false;
    series_[static_cast<size_t>(idx)].markers = true;
    return idx;
}

template<typename T>
int Chart::histT(const ct::Vector<T>& samples, int bins, const String& name)
{
    if (kind_ != ChartKind::Histogram || series_.empty())
    {
        clear();
        kind_ = ChartKind::Histogram;
    }
    if (bins > 0)
        bins_ = bins > 1000 ? 1000 : bins;
    const int idx = addSeriesT(name, ct::Vector<double>(), nullptr);
    assignDoubles(series_[static_cast<size_t>(idx)].samples, samples);
    buildHistogram();
    return idx;
}

// The templates above are used with floats (the overloads of float) and with doubles (everything else is widened to
// double first).
template int Chart::addSeriesT<float>(const String&, const ct::Vector<float>&, const Color*);
template int Chart::addSeriesT<double>(const String&, const ct::Vector<double>&, const Color*);
template int Chart::addSeriesXYT<float, float>(const String&, const ct::Vector<float>&, const ct::Vector<float>&);
template int Chart::addSeriesXYT<double, double>(const String&, const ct::Vector<double>&, const ct::Vector<double>&);
template void Chart::setSeriesValuesT<float>(int, const ct::Vector<float>&);
template void Chart::setSeriesValuesT<double>(int, const ct::Vector<double>&);
template void Chart::setSeriesXT<float>(int, const ct::Vector<float>&);
template void Chart::setSeriesXT<double>(int, const ct::Vector<double>&);
template void Chart::pieT<float>(const ct::Vector<String>&, const ct::Vector<float>&);
template void Chart::pieT<double>(const ct::Vector<String>&, const ct::Vector<double>&);
template void Chart::barsT<float>(const ct::Vector<String>&, const ct::Vector<float>&, const String&);
template void Chart::barsT<double>(const ct::Vector<String>&, const ct::Vector<double>&, const String&);
template int Chart::plotT<float, float>(const ct::Vector<float>&, const ct::Vector<float>&, const String&);
template int Chart::plotT<double, double>(const ct::Vector<double>&, const ct::Vector<double>&, const String&);
template int Chart::scatterT<float, float>(const ct::Vector<float>&, const ct::Vector<float>&, const String&);
template int Chart::scatterT<double, double>(const ct::Vector<double>&, const ct::Vector<double>&, const String&);
template int Chart::histT<float>(const ct::Vector<float>&, int, const String&);
template int Chart::histT<double>(const ct::Vector<double>&, int, const String&);

void Chart::setBins(int bins)
{
    if (bins < 1)
        bins = 1;
    if (bins > 1000)
        bins = 1000;
    if (bins_ == bins)
        return;
    bins_ = bins;
    if (kind_ == ChartKind::Histogram)
        buildHistogram();
    markDirty();
}

// Equal bins over the range of every series' samples, counted per series.
// The categories become the bins ("lo-hi"), the values the counts.
void Chart::buildHistogram()
{
    bool any = false;
    double lo = 0.0, hi = 0.0;
    for (const ChartSeries& se : series_)
    {
        for (double v : se.samples)
        {
            if (!any) { lo = hi = v; any = true; }
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    }
    edges_.clear();
    edgesF_.clear();
    categories_.clear();
    categoryHidden_.clear();
    if (!any)
    {
        for (ChartSeries& se : series_)
            se.values.clear();
        markDirty();
        return;
    }
    if (hi - lo < 1e-6)
    {
        lo -= 0.5;
        hi += 0.5;
    }
    const int n = bins_;
    const double width = (hi - lo) / static_cast<double>(n);
    char buf[96];
    const int decimals = decimalsFor(width) > 3 ? 3 : decimalsFor(width);
    for (int i = 0; i <= n; ++i)
    {
        edges_.push_back(i == n ? hi : lo + width * static_cast<double>(i));
        edgesF_.push_back(static_cast<float>(edges_.back()));
    }
    for (int i = 0; i < n; ++i)
    {
        snprintf(buf, sizeof(buf), "%.*f-%.*f", decimals, edges_[static_cast<size_t>(i)], decimals, edges_[static_cast<size_t>(i) + 1]);
        categories_.push_back(String(buf));
        categoryHidden_.push_back(false);
    }
    const double last = static_cast<double>(n - 1);
    for (ChartSeries& se : series_)
    {
        se.values.assign(static_cast<size_t>(n), 0.0);
        for (double v : se.samples)
        {
            // clamped to the bins in the double (the two selects are minsd and maxsd, no branch): a sample under the lowest edge, or
            // that is not a number, is in the first bin, and the top edge belongs to the last; the cast is of a number that fits
            double pos = (v - lo) / width;
            pos = last < pos ? last : pos;
            pos = pos > 0.0 ? pos : 0.0;
            const int k = static_cast<int>(pos);
            se.values[static_cast<size_t>(k)] += 1.0;
        }
    }
    hovered_ = Item();
    markDirty();
}

float Chart::x(int series, int index) const
{
    return static_cast<float>(xD(series, index));
}

double Chart::xD(int series, int index) const
{
    if (series < 0 || series >= seriesCount() || index < 0)
        return 0.0;
    const ChartSeries& s = series_[static_cast<size_t>(series)];
    if (static_cast<size_t>(index) < s.xs.size())
        return s.xs[static_cast<size_t>(index)];
    return static_cast<double>(index);
}

int Chart::pointCount(int series) const
{
    if (series < 0 || series >= seriesCount())
        return 0;
    return static_cast<int>(series_[static_cast<size_t>(series)].values.size());
}

float Chart::value(int series, int index) const
{
    return static_cast<float>(valueD(series, index));
}

double Chart::valueD(int series, int index) const
{
    if (series < 0 || series >= seriesCount() || index < 0)
        return 0.0;
    const ChartSeries& s = series_[static_cast<size_t>(series)];
    if (static_cast<size_t>(index) >= s.values.size())
        return 0.0;
    return s.values[static_cast<size_t>(index)];
}

float Chart::total(int series) const
{
    return static_cast<float>(totalD(series));
}

double Chart::totalD(int series) const
{
    double sum = 0.0;
    for (int i = 0; i < categoryCount(); ++i)
        if (categoryVisible(i))
            sum += valueD(series, i);
    return sum;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Looks
// ═════════════════════════════════════════════════════════════════════════════

void Chart::setTitle(const String& title)
{
    title_ = title;
    markDirty();
}

void Chart::setXAxisTitle(const String& title)
{
    if (xTitle_ == title)
        return;
    xTitle_ = title;
    markDirty();
}

void Chart::setYAxisTitle(const String& title)
{
    if (yTitle_ == title)
        return;
    yTitle_ = title;
    markDirty();
}

void Chart::setShowLegend(bool show)
{
    setLegendPosition(show ? LegendPosition::Auto : LegendPosition::Hidden);
}

void Chart::setLegendPosition(LegendPosition position)
{
    legendPosition_ = position;
    markDirty();
}

void Chart::setShowValues(bool show)
{
    showValues_ = show;
    markDirty();
}

void Chart::setShowPercent(bool show)
{
    showPercent_ = show;
    markDirty();
}

void Chart::setShowGrid(bool show)
{
    showGrid_ = show;
    markDirty();
}

void Chart::setMarkerSize(float radius)
{
    markerSize_ = radius < 1.0f ? 1.0f : (radius > 20.0f ? 20.0f : radius);
    markDirty();
}

void Chart::setLineWidth(float width)
{
    lineWidth_ = width < 1.0f ? 1.0f : (width > 10.0f ? 10.0f : width);
    markDirty();
}

void Chart::setValueSuffix(const String& suffix)
{
    valueSuffix_ = suffix;
    markDirty();
}

void Chart::setValueDecimals(int decimals)
{
    valueDecimals_ = decimals < -1 ? -1 : (decimals > 6 ? 6 : decimals);
    markDirty();
}

void Chart::setDonutRatio(float ratio)
{
    donutRatio_ = ratio < 0.1f ? 0.1f : (ratio > 0.9f ? 0.9f : ratio);
    markDirty();
}

void Chart::setPalette(const ct::Vector<Color>& palette)
{
    palette_ = palette.empty() ? defaultPalette() : palette;
    markDirty();
}

Color Chart::colorOf(int series, int index) const
{
    if (!isPie())
    {
        if (series >= 0 && series < seriesCount())
            return series_[static_cast<size_t>(series)].color;
        return Color(160, 160, 165);
    }
    if (palette_.empty() || index < 0)
        return Color(160, 160, 165);
    return palette_[static_cast<size_t>(index) % palette_.size()];
}

void Chart::setBackgroundColor(const Color& color)
{
    background_ = color;
    hasBackground_ = true;
    markDirty();
}

void Chart::setDrawBackground(bool draw)
{
    drawBackground_ = draw;
    markDirty();
}

void Chart::formatValue(float v, char* out, int outSize) const
{
    formatValueD(static_cast<double>(v), out, outSize);
}

void Chart::formatValueD(double v, char* out, int outSize) const
{
    int decimals = valueDecimals_;
    if (decimals < 0)
    {
        const double a = fabs(v);
        if (a >= 100.0 || fabs(v - round(v)) < 1e-4)
            decimals = 0;
        else if (a >= 10.0)
            decimals = 1;
        else
            decimals = 2;
    }
    // a whole number is printed in full (a float above 1e9 is a whole number): there was a %.3g here that
    // turned 1234567890 into 1.23e+09
    char number[400]; // (%f of 1e308 is 309 digits)
    snprintf(number, sizeof(number), "%.*f", decimals, v);
    char grouped[512];
    const char* text = number;
    if (thousandsSeparator_ != '\0')
    {
        const char* digits = number + (number[0] == '-' ? 1 : 0);
        int whole = 0;
        while (digits[whole] >= '0' && digits[whole] <= '9') ++whole;
        int n = 0;
        if (number[0] == '-') grouped[n++] = '-';
        for (int i = 0; i < whole && n < static_cast<int>(sizeof(grouped)) - 8; ++i)
        {
            grouped[n++] = digits[i];
            const int left = whole - 1 - i;
            if (left > 0 && left % 3 == 0) grouped[n++] = thousandsSeparator_;
        }
        for (const char* rest = digits + whole; *rest && n < static_cast<int>(sizeof(grouped)) - 1; ++rest) grouped[n++] = *rest;
        grouped[n] = '\0';
        text = grouped;
    }
    snprintf(out, static_cast<size_t>(outSize), "%s%s", text, valueSuffix_.c_str());
}

void Chart::setThousandsSeparator(char separator)
{
    thousandsSeparator_ = separator;
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Geometry
// ═════════════════════════════════════════════════════════════════════════════

int Chart::firstVisibleSeries() const
{
    for (int i = 0; i < seriesCount(); ++i)
        if (series_[static_cast<size_t>(i)].visible)
            return i;
    return -1;
}

bool Chart::legendOnRight(const Rect& b) const
{
    if (legendPosition_ == LegendPosition::Right)
        return true;
    if (legendPosition_ == LegendPosition::Bottom)
        return false;
    return b.w >= b.h * 1.25f;
}

void Chart::buildSlices()
{
    slices_.clear();
    const int s = firstVisibleSeries();
    if (s < 0)
        return;
    const double sum = totalD(s);
    if (sum <= 0.0)
        return;
    float angle = 0.0f;
    for (int i = 0; i < categoryCount(); ++i)
    {
        if (!categoryVisible(i))
            continue;
        const double v = valueD(s, i);
        if (v <= 0.0)
            continue;
        Slice slice;
        slice.index = i;
        slice.value = v;
        slice.start = angle;
        slice.sweep = static_cast<float>(v / sum * 2.0 * static_cast<double>(kPi));
        angle += slice.sweep;
        slices_.push_back(slice);
    }
}

// A 1-2-5 axis that contains every visible bar and zero.
void Chart::buildAxis(double& lo, double& hi, double& step) const
{
    // Bars grow from zero, so the axis always holds it.
    double vmin = 0.0, vmax = 0.0;
    for (int s = 0; s < seriesCount(); ++s)
    {
        if (!series_[static_cast<size_t>(s)].visible)
            continue;
        for (int i = 0; i < categoryCount(); ++i)
        {
            if (!categoryVisible(i))
                continue;
            const double v = valueD(s, i);
            if (v < vmin) vmin = v;
            if (v > vmax) vmax = v;
        }
    }
    niceAxis(vmin, vmax, lo, hi, step);
}

// A 1-2-5 axis that holds vmin..vmax with about five steps.
void Chart::niceAxis(double vmin, double vmax, double& lo, double& hi, double& step)
{
    if (vmax - vmin < 1e-6)
    {
        // One value: a span around it, so the point sits in the middle.
        const double pad = fabs(vmin) > 1e-6 ? fabs(vmin) * 0.1 : 0.5;
        vmin -= pad;
        vmax += pad;
    }
    const double span = vmax - vmin;
    const double rough = span / 5.0;
    const double mag = pow(10.0, floor(log10(rough)));
    const double frac = rough / mag;
    // The nearest of 1, 2, 5, 10 in log space, so 30 ticks by 5, not by 10.
    double nice = 10.0;
    if (frac < 1.414) nice = 1.0;
    else if (frac < 3.162) nice = 2.0;
    else if (frac < 7.071) nice = 5.0;
    step = nice * mag;
    lo = floor(vmin / step) * step;
    hi = ceil(vmax / step) * step;
    if (hi - lo < step)
        hi = lo + step;
}

// The text printed on or beside a slice: percent, value, or both.
void Chart::sliceLabel(const Slice& slice, double sum, char* out, int outSize) const
{
    char valueText[48];
    valueText[0] = '\0';
    if (showValues_)
        formatValueD(slice.value, valueText, sizeof(valueText));
    const double pct = sum > 0.0 ? slice.value / sum * 100.0 : 0.0;
    // A slice that is a fraction of a percent is there, and "0%" would say it is not: it reads "<1%" (the tooltip, which
    // has the room, prints the tenth of a percent).
    char pctText[16];
    if (pct > 0.0 && pct < 1.0)
        snprintf(pctText, sizeof(pctText), "<1%%");
    else
        snprintf(pctText, sizeof(pctText), "%.0f%%", pct);
    if (showPercent_ && showValues_)
        snprintf(out, static_cast<size_t>(outSize), "%s (%s)", valueText, pctText);
    else if (showPercent_)
        snprintf(out, static_cast<size_t>(outSize), "%s", pctText);
    else
        snprintf(out, static_cast<size_t>(outSize), "%s", valueText);
}

String Chart::sliceText(int category) const
{
    const int s = firstVisibleSeries();
    if (s < 0)
        return String();
    const double sum = totalD(s);
    for (const Slice& slice : slices_)
    {
        if (slice.index != category)
            continue;
        char text[64];
        sliceLabel(slice, sum, text, sizeof(text));
        return String(text);
    }
    return String();
}

Chart::Item Chart::hitTest(float localX, float localY) const
{
    Item hit;
    if (isXY())
    {
        // The nearest point within a few pixels of the mouse.
        const float reach = markerSize_ + 6.0f;
        float best = reach * reach;
        for (const Point& pt : points_)
        {
            const float dx = pt.x - localX, dy = pt.y - localY;
            const float d = dx * dx + dy * dy;
            if (d <= best)
            {
                best = d;
                hit.series = pt.series;
                hit.index = pt.index;
            }
        }
        return hit;
    }
    if (isBars())
    {
        for (const Bar& bar : bars_)
        {
            if (bar.rect.contains(localX, localY))
            {
                hit.series = bar.series;
                hit.index = bar.index;
                return hit;
            }
        }
        return hit;
    }
    if (pieR_ <= 0.0f || slices_.empty())
        return hit;
    const float dx = localX - pieCx_;
    const float dy = localY - pieCy_;
    const float d = sqrtf(dx * dx + dy * dy);
    if (d > pieR_ + kHoverPop)
        return hit;
    if (kind_ == ChartKind::Donut && d < pieR_ * donutRatio_)
        return hit;
    float angle = atan2f(dx, -dy);   // clockwise from the top
    if (angle < 0.0f)
        angle += 2.0f * kPi;
    for (const Slice& slice : slices_)
    {
        if (angle >= slice.start && angle < slice.start + slice.sweep)
        {
            hit.series = firstVisibleSeries();
            hit.index = slice.index;
            return hit;
        }
    }
    // Rounding at the seam: the last slice owns 2*pi.
    hit.series = firstVisibleSeries();
    hit.index = slices_.back().index;
    return hit;
}

String Chart::legendLabel(int entry) const
{
    if (entry < 0 || entry >= legendEntryCount())
        return String();
    return legend_[static_cast<size_t>(entry)].label;
}

int Chart::legendHitTest(float localX, float localY) const
{
    for (size_t i = 0; i < legend_.size(); ++i)
        if (legend_[i].rect.contains(localX, localY))
            return static_cast<int>(i);
    return -1;
}

Rect Chart::legendEntryRect(int entry) const
{
    if (entry < 0 || entry >= legendEntryCount())
        return Rect();
    return legend_[static_cast<size_t>(entry)].rect;
}

Widget::Vec2f Chart::pointAt(int series, int index) const
{
    for (const Point& pt : points_)
        if (pt.series == series && pt.index == index)
            return { pt.x, pt.y };
    return { -1.0f, -1.0f };
}

Rect Chart::barRect(int series, int index) const
{
    for (const Bar& bar : bars_)
        if (bar.series == series && bar.index == index)
            return bar.rect;
    return Rect();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Paint
// ═════════════════════════════════════════════════════════════════════════════

void Chart::paintBackground(PaintContext& ctx, const Rect& b)
{
    if (!drawBackground_)
        return;
    const Theme& t = Theme::instance();
    const Color bg = hasBackground_ ? background_ : t.panelColor;
    ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
    ctx.fillRoundedRect(b.x, b.y, b.w, b.h, t.borderRadius);
    ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, t.borderColor.a);
    ctx.lineRoundedRect(b.x + 0.5f, b.y + 0.5f, b.w - 1.0f, b.h - 1.0f, t.borderRadius);
}

float Chart::paintTitle(PaintContext& ctx, const Rect& b)
{
    if (title_.empty())
        return 0.0f;
    const Theme& t = Theme::instance();
    const float asc = setupFont(ctx, t.textColor);
    char buf[256];
    fitText(ctx, title_.c_str(), b.w - 2.0f * kPad, buf, sizeof(buf));
    const float tw = ctx.font.GetTextWidth(buf);
    ctx.font.Print(buf, b.x + (b.w - tw) * 0.5f, b.y + kPad + asc);
    return lineHeight(ctx) + 4.0f;
}

// The titles of the axes: a line under the plot for the x one, a line above it for the y one, both taken from the plot.
void Chart::paintAxisTitles(PaintContext& ctx, const Rect& b, Rect& plot)
{
    const Theme& t = Theme::instance();
    const float line = lineHeight(ctx) + 2.0f;
    char buf[256];
    if (!yTitle_.empty() && plot.h > line * 2.0f)
    {
        const float asc = setupFont(ctx, t.textDisabled);
        fitText(ctx, yTitle_.c_str(), plot.w, buf, sizeof(buf));
        ctx.font.Print(buf, b.x + plot.x, b.y + plot.y + asc);
        plot.y += line;
        plot.h -= line;
    }
    if (!xTitle_.empty() && plot.h > line * 2.0f)
    {
        const float asc = setupFont(ctx, t.textDisabled);
        fitText(ctx, xTitle_.c_str(), plot.w, buf, sizeof(buf));
        const float tw = ctx.font.GetTextWidth(buf);
        ctx.font.Print(buf, b.x + plot.x + (plot.w - tw) * 0.5f, b.y + plot.y + plot.h - line + 2.0f + asc);
        plot.h -= line;
    }
}

// Lays the legend entries out and takes their space from the plot.
void Chart::buildLegend(PaintContext& ctx, const Rect& b, const Rect& titleArea, Rect& plot)
{
    legend_.clear();
    legendArea_ = Rect();
    if (legendPosition_ == LegendPosition::Hidden)
        return;

    // Entries: the categories of a pie, the series otherwise.
    ct::Vector<LegendEntry> entries;
    if (!isPie())
    {
        const bool single = seriesCount() == 1 && series_[0].name.empty();
        if (single)
            return;
        for (int s = 0; s < seriesCount(); ++s)
        {
            LegendEntry e;
            // A series with no name next to others (a histogram added without one) still needs a line the eye can tell
            // from its neighbours: "Series 2", counted from one. (A single series with no name has no legend at all.)
            e.label = seriesLabel(s);
            e.color = series_[static_cast<size_t>(s)].color;
            e.visible = series_[static_cast<size_t>(s)].visible;
            e.target = s;
            entries.push_back(e);
        }
    }
    else
    {
        for (int i = 0; i < categoryCount(); ++i)
        {
            LegendEntry e;
            e.label = categories_[static_cast<size_t>(i)];
            e.color = colorOf(0, i);
            e.visible = categoryVisible(i);
            e.target = i;
            entries.push_back(e);
        }
    }
    if (entries.empty())
        return;

    const float lh = lineHeight(ctx);
    const float rowH = lh + 2.0f;
    const Rect local(0.0f, titleArea.h, b.w, b.h - titleArea.h);

    if (legendOnRight(b))
    {
        // One column at the right; width from the widest label, capped so the
        // pie keeps at least half of the widget.
        float widest = 0.0f;
        for (const LegendEntry& e : entries)
        {
            const float w = ctx.font.GetTextWidth(e.label.c_str());
            if (w > widest) widest = w;
        }
        const float maxLabel = b.w * 0.4f - kSwatch - kLegendGap;
        if (widest > maxLabel) widest = maxLabel;
        const float colW = kSwatch + kLegendGap + widest;
        const float x0 = local.right() - kPad - colW;
        // Vertically centred against the plot, scrolled off when too many.
        const float totalH = rowH * static_cast<float>(entries.size());
        float y = local.y + kPad;
        if (totalH < local.h - 2.0f * kPad)
            y = local.y + (local.h - totalH) * 0.5f;
        for (LegendEntry& e : entries)
        {
            e.rect = Rect(x0, y, colW, rowH);
            y += rowH;
            if (e.rect.bottom() <= local.bottom() - kPad)
                legend_.push_back(e);
        }
        legendArea_ = Rect(x0, local.y, colW + kPad, local.h);
        plot.w = x0 - kPad - plot.x;
    }
    else
    {
        // Rows at the bottom, wrapped; at most a third of the height.
        const float maxW = local.w - 2.0f * kPad;
        const float maxRows = floorf((local.h * 0.34f) / rowH);
        ct::Vector<float> widths;
        float rowsNeeded = 1.0f;
        float x = 0.0f;
        for (const LegendEntry& e : entries)
        {
            float w = kSwatch + kLegendGap + ctx.font.GetTextWidth(e.label.c_str()) + kLegendGap * 2.0f;
            if (w > maxW) w = maxW;
            if (x > 0.0f && x + w > maxW)
            {
                rowsNeeded += 1.0f;
                x = 0.0f;
            }
            widths.push_back(w);
            x += w;
        }
        const float rows = rowsNeeded < maxRows ? rowsNeeded : (maxRows < 1.0f ? 1.0f : maxRows);
        const float areaH = rows * rowH + kPad;
        const float y0 = local.bottom() - areaH;
        legendArea_ = Rect(local.x, y0, local.w, areaH);
        plot.h = y0 - plot.y;

        // Place: each row centred.
        size_t first = 0;
        float y = y0 + kPad * 0.5f;
        int row = 0;
        while (first < entries.size() && row < static_cast<int>(rows))
        {
            size_t last = first;
            float rowW = 0.0f;
            while (last < entries.size() && (last == first || rowW + widths[last] <= maxW))
            {
                rowW += widths[last];
                ++last;
            }
            float xx = local.x + (local.w - rowW) * 0.5f;
            for (size_t i = first; i < last; ++i)
            {
                entries[i].rect = Rect(xx, y, widths[i], rowH);
                legend_.push_back(entries[i]);
                xx += widths[i];
            }
            first = last;
            y += rowH;
            ++row;
        }
    }
}

void Chart::paintLegend(PaintContext& ctx, const Rect& b)
{
    if (legend_.empty())
        return;
    const Theme& t = Theme::instance();
    const float lh = lineHeight(ctx);
    const bool onRight = legendArea_.w < b.w;
    for (size_t i = 0; i < legend_.size(); ++i)
    {
        const LegendEntry& e = legend_[i];
        const Rect r(b.x + e.rect.x, b.y + e.rect.y, e.rect.w, e.rect.h);
        const bool hot = e.rect.contains(mouseX_, mouseY_);
        if (hot)
        {
            ctx.fill.SetColor(255, 255, 255, 14);
            ctx.fillRoundedRect(r.x - 3.0f, r.y, r.w + 6.0f, r.h, 3.0f);
        }
        const float sx = r.x + (onRight ? 0.0f : kLegendGap);
        const float sy = r.y + (r.h - kSwatch) * 0.5f;
        if (e.visible)
        {
            ctx.fill.SetColor(e.color.r, e.color.g, e.color.b, e.color.a);
            ctx.fillRoundedRect(sx, sy, kSwatch, kSwatch, 2.0f);
        }
        else
        {
            ctx.line.SetColor(e.color.r, e.color.g, e.color.b, 160);
            ctx.lineRoundedRect(sx + 0.5f, sy + 0.5f, kSwatch - 1.0f, kSwatch - 1.0f, 2.0f);
        }
        const Color textColor = e.visible ? t.textColor : t.textDisabled;
        const float asc = setupFont(ctx, textColor);
        char buf[160];
        // Below the plot an entry is gap, swatch, gap, label, gap wide.
        const float labelW = r.w - kSwatch - kLegendGap - (onRight ? 0.0f : kLegendGap * 2.0f);
        fitText(ctx, e.label.c_str(), labelW, buf, sizeof(buf));
        ctx.font.Print(buf, sx + kSwatch + kLegendGap, r.y + (r.h - lh) * 0.5f + asc);
    }
}

void Chart::paintPie(PaintContext& ctx, const Rect& b, const Rect& plot)
{
    const Theme& t = Theme::instance();
    buildSlices();
    const float lh = lineHeight(ctx);

    // Leave room around the rim for labels drawn outside thin slices: a line
    // of text at the top and bottom, the widest label at the sides.
    const bool outsideLabels = showPercent_ || showValues_;
    const int s = firstVisibleSeries();
    const double sum = s >= 0 ? totalD(s) : 0.0;
    float widest = 0.0f;
    char labelText[64];
    if (outsideLabels)
    {
        for (const Slice& slice : slices_)
        {
            sliceLabel(slice, sum, labelText, sizeof(labelText));
            const float w = ctx.font.GetTextWidth(labelText);
            if (w > widest) widest = w;
        }
    }
    const float marginX = outsideLabels ? (kLeader + 6.0f + widest) : 4.0f;
    const float marginY = outsideLabels ? (kLeader + lh) : 4.0f;
    const float rx = plot.w * 0.5f - marginX;
    const float ry = plot.h * 0.5f - marginY;
    float r = rx < ry ? rx : ry;
    if (r < 24.0f)
        r = (plot.w < plot.h ? plot.w : plot.h) * 0.5f - 4.0f;
    if (r < 4.0f)
        r = 4.0f;
    pieCx_ = plot.x + plot.w * 0.5f;
    pieCy_ = plot.y + plot.h * 0.5f;
    pieR_ = r;
    const float cx = b.x + pieCx_;
    const float cy = b.y + pieCy_;
    const float inner = kind_ == ChartKind::Donut ? r * donutRatio_ : 0.0f;

    if (slices_.empty())
    {
        // Nothing to show: an empty ring and a note.
        ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 255);
        ctx.lineCircle(cx, cy, r);
        const float asc = setupFont(ctx, t.textDisabled);
        const char* note = "no data";
        ctx.font.Print(note, cx - ctx.font.GetTextWidth(note) * 0.5f, cy - lh * 0.5f + asc);
        return;
    }

    // Slices as one convex polygon each (pie) or a strip of quads (donut).
    const float step = r > 0.0f ? (3.0f / r < 0.09f ? (3.0f / r > 0.02f ? 3.0f / r : 0.02f) : 0.09f) : 0.09f;
    struct Label
    {
        int   slice;
        float x, y;      // text position (top-left), absolute
        float w;
        bool  outside;
        bool  right;     // on the right half of the pie
        float ax, ay;    // anchor on the rim, absolute
        char  text[64];
    };
    ct::Vector<Label> labels;

    for (size_t si = 0; si < slices_.size(); ++si)
    {
        const Slice& slice = slices_[si];
        const bool hot = hovered_.valid() && hovered_.index == slice.index;
        const Color c = colorOf(s, slice.index);
        const Color fill = hot ? lighten(c, 0.18f) : c;
        float ox = 0.0f, oy = 0.0f;
        if (hot)
            polar(0.0f, 0.0f, kHoverPop, slice.start + slice.sweep * 0.5f, ox, oy);
        const float scx = cx + ox, scy = cy + oy;

        int segs = Math::toInt(ceilf(slice.sweep / step), 1, 1, 100000);
        if (segs < 1) segs = 1;
        const float da = slice.sweep / static_cast<float>(segs);
        ctx.fill.SetColor(fill.r, fill.g, fill.b, fill.a);
        if (inner > 0.0f)
        {
            // Quads have no acute corner, so their anti-aliasing fringe
            // never grows a hair the way a sliver triangle's would.
            float a0 = slice.start;
            Vec2 o0, i0;
            polar(scx, scy, r, a0, o0.x, o0.y);
            polar(scx, scy, inner, a0, i0.x, i0.y);
            for (int k = 0; k < segs; ++k)
            {
                const float a1 = a0 + da;
                Vec2 o1, i1;
                polar(scx, scy, r, a1, o1.x, o1.y);
                polar(scx, scy, inner, a1, i1.x, i1.y);
                quad_.clear();
                quad_.push_back(i0);
                quad_.push_back(o0);
                quad_.push_back(o1);
                quad_.push_back(i1);
                ctx.fillPolygon(quad_);
                o0 = o1; i0 = i1; a0 = a1;
            }
        }
        else
        {
            // A slice is convex: the centre plus the points along its arc.
            if (slice.sweep >= 2.0f * kPi - 1e-4f)
                ctx.fillCircle(scx, scy, r);
            else
            {
                polygon_.clear();
                polygon_.push_back(Vec2{ scx, scy });
                for (int k = 0; k <= segs; ++k)
                {
                    Vec2 p;
                    polar(scx, scy, r, slice.start + da * static_cast<float>(k), p.x, p.y);
                    polygon_.push_back(p);
                }
                ctx.fillPolygon(polygon_);
            }
        }
        // A seam in the panel colour between slices keeps them apart.
        if (slices_.size() > 1)
        {
            const Color seam = hasBackground_ ? background_ : t.panelColor;
            ctx.line.SetColor(seam.r, seam.g, seam.b, 255);
            float ex, ey, exi, eyi;
            polar(scx, scy, r, slice.start, ex, ey);
            polar(scx, scy, inner, slice.start, exi, eyi);
            ctx.drawLine(exi, eyi, ex, ey);
        }

        // Label text.
        if (!outsideLabels)
            continue;
        Label label;
        label.slice = static_cast<int>(si);
        sliceLabel(slice, sum, label.text, sizeof(label.text));
        label.w = ctx.font.GetTextWidth(label.text);
        const float mid = slice.start + slice.sweep * 0.5f;
        // Inside when the slice is wide enough for the text at 0.62 r.
        const float labelR = inner > 0.0f ? (inner + r) * 0.5f : r * 0.62f;
        const float arc = slice.sweep * labelR;
        const float ring = inner > 0.0f ? (r - inner) : r;
        label.outside = !(arc >= label.w + 8.0f && ring >= lh + 4.0f);
        label.right = sinf(mid) >= 0.0f;
        polar(scx, scy, r, mid, label.ax, label.ay);
        if (!label.outside)
        {
            float lx, ly;
            polar(scx, scy, labelR, mid, lx, ly);
            label.x = lx - label.w * 0.5f;
            label.y = ly - lh * 0.5f;
        }
        else
        {
            float lx, ly;
            polar(scx, scy, r + kLeader, mid, lx, ly);
            label.x = label.right ? lx + 4.0f : lx - 4.0f - label.w;
            label.y = ly - lh * 0.5f;
        }
        labels.push_back(label);
    }

    // Outside labels on one side must not overlap or leave the plot: sorted
    // by height, they are pushed down from the top, then back up from the
    // bottom, so a cluster of thin slices reads as a column.
    const float minY = b.y + plot.y;
    const float maxY = b.y + plot.bottom() - lh;
    for (int side = 0; side < 2; ++side)
    {
        const bool right = side == 0;
        ct::Vector<int> order;
        for (size_t i = 0; i < labels.size(); ++i)
            if (labels[i].outside && labels[i].right == right)
                order.push_back(static_cast<int>(i));
        for (size_t i = 1; i < order.size(); ++i)
        {
            const int k = order[i];
            size_t j = i;
            while (j > 0 && labels[static_cast<size_t>(order[j - 1])].y > labels[static_cast<size_t>(k)].y)
            {
                order[j] = order[j - 1];
                --j;
            }
            order[j] = k;
        }
        for (size_t i = 0; i < order.size(); ++i)
        {
            Label& l = labels[static_cast<size_t>(order[i])];
            const float floor = i == 0 ? minY : labels[static_cast<size_t>(order[i - 1])].y + lh;
            if (l.y < floor)
                l.y = floor;
        }
        for (size_t i = order.size(); i-- > 0;)
        {
            Label& l = labels[static_cast<size_t>(order[i])];
            const float ceiling = i + 1 == order.size() ? maxY : labels[static_cast<size_t>(order[i + 1])].y - lh;
            if (l.y > ceiling)
                l.y = ceiling;
        }
    }

    for (const Label& l : labels)
    {
        if (l.text[0] == '\0')
            continue;
        if (l.outside)
        {
            const float ex = l.right ? l.x - 2.0f : l.x + l.w + 2.0f;
            const float ey = l.y + lh * 0.5f;
            ctx.line.SetColor(t.textDisabled.r, t.textDisabled.g, t.textDisabled.b, 255);
            ctx.drawLine(l.ax, l.ay, ex, ey);
            const float asc = setupFont(ctx, t.textColor);
            ctx.font.Print(l.text, l.x, l.y + asc);
        }
        else
        {
            // Dark text on a light slice, light text on a dark one.
            const Color c = colorOf(s, slices_[static_cast<size_t>(l.slice)].index);
            const float luma = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
            const Color textColor = luma > 150.0f ? Color(24, 24, 28) : Color(245, 245, 248);
            const float asc = setupFont(ctx, textColor);
            ctx.font.Print(l.text, l.x, l.y + asc);
        }
    }

    // A donut shows the total in the hole.
    if (inner > 0.0f)
    {
        char buf[48];
        formatValueD(sum, buf, sizeof(buf));
        const float w = ctx.font.GetTextWidth(buf);
        if (w < inner * 1.8f)
        {
            const float asc = setupFont(ctx, t.textColor);
            ctx.font.Print(buf, cx - w * 0.5f, cy - lh * 0.5f + asc);
        }
    }
}

// Width taken by the value axis labels at the left, the widest tick plus a gap.
float Chart::valueAxisGutter(PaintContext& ctx, double lo, double hi, double step, int decimals) const
{
    char buf[400];
    float gutter = 0.0f;
    const int ticks = Math::toInt(roundf((hi - lo) / step), 0, 0, 10000); // (data that is not a number: no ticks)
    for (int k = 0; k <= ticks; ++k)
    {
        snprintf(buf, sizeof(buf), "%.*f%s", decimals, lo + step * static_cast<double>(k), valueSuffix_.c_str());
        const float w = ctx.font.GetTextWidth(buf);
        if (w > gutter) gutter = w;
    }
    return gutter + kAxisPad;
}

// The value axis of an area (absolute coordinates): grid lines or just the
// zero line, and a label per tick at the left.
void Chart::paintValueAxis(PaintContext& ctx, float ax, float ay, float aw, float ah, double lo, double hi, double step, int decimals)
{
    const Theme& t = Theme::instance();
    const float lh = lineHeight(ctx);
    char buf[400];
    const int ticks = Math::toInt(roundf((hi - lo) / step), 0, 0, 10000); // (data that is not a number: no ticks)
    for (int k = 0; k <= ticks; ++k)
    {
        const double v = lo + step * static_cast<double>(k);
        const float y = floorf(ay + ah - static_cast<float>((v - lo) / (hi - lo) * static_cast<double>(ah))) + 0.5f;
        const bool zero = fabs(v) < step * 0.01;
        if (showGrid_ || zero)
        {
            const Color c = zero ? t.textDisabled : t.borderColor;
            ctx.line.SetColor(c.r, c.g, c.b, zero ? 255 : 160);
            ctx.drawLine(ax, y, ax + aw, y);
        }
        snprintf(buf, sizeof(buf), "%.*f%s", decimals, v, valueSuffix_.c_str());
        const float w = ctx.font.GetTextWidth(buf);
        const float asc = setupFont(ctx, t.textDisabled);
        ctx.font.Print(buf, ax - kAxisPad - w, y - lh * 0.5f + asc);
    }
}

void Chart::paintBars(PaintContext& ctx, const Rect& b, const Rect& plot)
{
    const Theme& t = Theme::instance();
    bars_.clear();
    const float lh = lineHeight(ctx);

    double lo, hi, step;
    buildAxis(lo, hi, step);
    axisMin_ = lo;
    axisMax_ = hi;
    axisStep_ = step;

    const int decimals = valueDecimals_ >= 0 ? valueDecimals_ : decimalsFor(step);
    char buf[400];
    const float gutter = valueAxisGutter(ctx, lo, hi, step, decimals);

    // A histogram's bars touch, and the bin edges are written under the axis
    // in place of the category labels.
    const bool histogram = kind_ == ChartKind::Histogram && edges_.size() == static_cast<size_t>(categoryCount()) + 1;
    const bool labelsBelow = categoryCount() > 0;
    Rect area(plot.x + gutter, plot.y + (showValues_ ? lh : 4.0f), plot.w - gutter - (histogram ? 16.0f : 4.0f),
              plot.h - (showValues_ ? lh : 4.0f) - (labelsBelow ? lh + kAxisPad : 4.0f));
    if (area.w < 10.0f || area.h < 10.0f)
        return;
    const float ax = b.x + area.x;
    const float ay = b.y + area.y;
    // (the pixel is worked out in double from the distance to the bottom of the axis, and turned to float last)
    auto yOf = [&](double v) { return ay + area.h - static_cast<float>((v - lo) / (hi - lo) * static_cast<double>(area.h)); };

    paintValueAxis(ctx, ax, ay, area.w, area.h, lo, hi, step, decimals);

    // Visible categories and series.
    ct::Vector<int> cats;
    for (int i = 0; i < categoryCount(); ++i)
        if (categoryVisible(i) || histogram)
            cats.push_back(i);
    ct::Vector<int> sers;
    for (int s = 0; s < seriesCount(); ++s)
        if (series_[static_cast<size_t>(s)].visible)
            sers.push_back(s);
    if (cats.empty() || sers.empty())
    {
        const float asc = setupFont(ctx, t.textDisabled);
        const char* note = "no data";
        ctx.font.Print(note, ax + (area.w - ctx.font.GetTextWidth(note)) * 0.5f, ay + area.h * 0.5f - lh * 0.5f + asc);
        return;
    }

    const float groupW = area.w / static_cast<float>(cats.size());
    const float groupPad = histogram ? 0.5f : groupW * 0.18f;
    const float innerW = groupW - 2.0f * groupPad;
    const float gap = sers.size() > 1 ? (innerW * 0.06f < 2.0f ? innerW * 0.06f : 2.0f) : 0.0f;
    float barW = (innerW - gap * static_cast<float>(sers.size() - 1)) / static_cast<float>(sers.size());
    if (barW < 1.0f) barW = 1.0f;
    const float zeroY = yOf(lo > 0.0f ? lo : (hi < 0.0f ? hi : 0.0f));

    for (size_t ci = 0; ci < cats.size(); ++ci)
    {
        const int cat = cats[ci];
        const float gx = ax + groupW * static_cast<float>(ci);
        for (size_t sj = 0; sj < sers.size(); ++sj)
        {
            const int s = sers[sj];
            const double v = valueD(s, cat);
            const float x = gx + groupPad + (barW + gap) * static_cast<float>(sj);
            const float yv = yOf(v);
            const float top = yv < zeroY ? yv : zeroY;
            float h = fabsf(zeroY - yv);
            if (h < 1.0f) h = 1.0f;
            Bar bar;
            bar.series = s;
            bar.index = cat;
            bar.value = v;
            bar.rect = Rect(x - b.x, top - b.y, barW, h);
            bars_.push_back(bar);

            const bool hot = hovered_.series == s && hovered_.index == cat;
            const Color c = series_[static_cast<size_t>(s)].color;
            const Color fill = hot ? lighten(c, 0.22f) : c;
            ctx.fill.SetColor(fill.r, fill.g, fill.b, fill.a);
            const float radius = barW > 8.0f && !histogram ? 2.0f : 0.0f;
            if (radius > 0.0f)
                ctx.fillRoundedRect(x, top, barW, h, radius, 4);
            else
                ctx.fillRect(x, top, barW, h);

            if (showValues_)
            {
                formatValueD(v, buf, sizeof(buf));
                const float w = ctx.font.GetTextWidth(buf);
                if (w <= groupW)
                {
                    const float asc = setupFont(ctx, t.textColor);
                    const float ty = v >= 0.0 ? top - lh : top + h;
                    ctx.font.Print(buf, x + (barW - w) * 0.5f, ty + asc);
                }
            }
        }

        // Category label under the group (a histogram writes its edges below instead).
        if (labelsBelow && !histogram)
        {
            const String& label = categories_[static_cast<size_t>(cat)];
            char fitted[128];
            fitText(ctx, label.c_str(), groupW - 4.0f, fitted, sizeof(fitted));
            const float w = ctx.font.GetTextWidth(fitted);
            const float asc = setupFont(ctx, t.textColor);
            ctx.font.Print(fitted, gx + (groupW - w) * 0.5f, ay + area.h + kAxisPad + asc);
        }
    }

    // A histogram's bin edges along the bottom, as many as fit.
    if (histogram)
    {
        const int n = static_cast<int>(cats.size());
        const double width = edges_[1] - edges_[0];
        const int decimals = decimalsFor(width) > 3 ? 3 : decimalsFor(width);
        snprintf(buf, sizeof(buf), "%.*f", decimals, edges_.back());
        const float labelW = ctx.font.GetTextWidth(buf) + 10.0f;
        int every = 1;
        while (n / every > 0 && groupW * static_cast<float>(every) < labelW)
            ++every;
        for (int i = 0; i <= n; i += every)
        {
            const float px = ax + groupW * static_cast<float>(i);
            ctx.line.SetColor(t.textDisabled.r, t.textDisabled.g, t.textDisabled.b, 255);
            ctx.drawLine(floorf(px) + 0.5f, ay + area.h, floorf(px) + 0.5f, ay + area.h + 4.0f);
            snprintf(buf, sizeof(buf), "%.*f", decimals, edges_[static_cast<size_t>(i)]);
            const float w = ctx.font.GetTextWidth(buf);
            const float asc = setupFont(ctx, t.textDisabled);
            ctx.font.Print(buf, px - w * 0.5f, ay + area.h + kAxisPad + asc);
        }
    }

    // The axis line.
    ctx.line.SetColor(t.textDisabled.r, t.textDisabled.g, t.textDisabled.b, 255);
    ctx.drawLine(ax + 0.5f, ay, ax + 0.5f, ay + area.h);
}

// Lines and scatter: every visible series over a shared x axis, numeric when
// any series has xs, the categories (or the indices) otherwise.
void Chart::paintXY(PaintContext& ctx, const Rect& b, const Rect& plot)
{
    const Theme& t = Theme::instance();
    points_.clear();
    const float lh = lineHeight(ctx);

    // Ranges over the visible points.
    bool numericX = false;
    bool any = false;
    double xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0;
    int maxCount = 0;
    for (int s = 0; s < seriesCount(); ++s)
    {
        const ChartSeries& se = series_[static_cast<size_t>(s)];
        if (!se.visible)
            continue;
        if (!se.xs.empty())
            numericX = true;
        const int n = pointCount(s);
        if (n > maxCount) maxCount = n;
        for (int i = 0; i < n; ++i)
        {
            if (se.xs.empty() && i < categoryCount() && !categoryVisible(i))
                continue;
            const double px = xD(s, i), py = valueD(s, i);
            if (!any) { xmin = xmax = px; ymin = ymax = py; any = true; }
            if (px < xmin) xmin = px;
            if (px > xmax) xmax = px;
            if (py < ymin) ymin = py;
            if (py > ymax) ymax = py;
        }
    }

    double lo, hi, step;
    niceAxis(any ? ymin : 0.0, any ? ymax : 1.0, lo, hi, step);
    axisMin_ = lo;
    axisMax_ = hi;
    axisStep_ = step;
    const int decimals = valueDecimals_ >= 0 ? valueDecimals_ : decimalsFor(step);
    const float gutter = valueAxisGutter(ctx, lo, hi, step, decimals);

    // The x axis: nice ticks over the numbers, or one slot per index.
    double xlo = 0.0, xhi = 1.0, xstep = 1.0;
    const int slots = maxCount > categoryCount() ? maxCount : categoryCount();
    if (numericX)
        niceAxis(any ? xmin : 0.0, any ? xmax : 1.0, xlo, xhi, xstep);
    else
    {
        xlo = 0.0;
        xhi = static_cast<double>(slots > 1 ? slots - 1 : 1);
        xstep = 1.0;
    }
    xAxisMin_ = xlo;
    xAxisMax_ = xhi;
    xAxisStep_ = xstep;

    Rect area(plot.x + gutter, plot.y + (showValues_ ? lh : markerSize_ + 2.0f), plot.w - gutter - 8.0f,
              plot.h - (showValues_ ? lh : markerSize_ + 2.0f) - lh - kAxisPad);
    if (area.w < 10.0f || area.h < 10.0f)
        return;
    const float ax = b.x + area.x;
    const float ay = b.y + area.y;
    // Index slots leave half a slot at each side, so the first and last
    // points do not sit on the frame.
    const float xpad = numericX ? 0.0f : (slots > 1 ? area.w / static_cast<float>(slots) * 0.5f : area.w * 0.5f);
    // The pixel of a number: its distance to the start of the axis is worked out in double (a timestamp of 1.7e9 and
    // the one a millisecond after it are a pixel apart, not equal) and only that distance, scaled to the pixels
    // of the axis, is turned to float.
    const bool xRange = xhi > xlo; // (an axis that is not a number has no range: every point at the left edge, as before)
    const double xScale = xRange ? static_cast<double>(area.w - 2.0f * xpad) / (xhi - xlo) : 0.0;
    const double yScale = static_cast<double>(area.h) / (hi - lo);
    auto xOf = [&](double v) { return ax + xpad + (xRange ? static_cast<float>((v - xlo) * xScale) : 0.0f); };
    auto yOf = [&](double v) { return ay + area.h - static_cast<float>((v - lo) * yScale); };

    paintValueAxis(ctx, ax, ay, area.w, area.h, lo, hi, step, decimals);

    // The x ticks: numbers along the bottom, or the category labels.
    char buf[400];
    if (numericX)
    {
        const int xdecimals = decimalsFor(xstep);
        const int xticks = Math::toInt(roundf((xhi - xlo) / xstep), 0, 0, 10000);
        // Skip ticks when their labels would touch.
        snprintf(buf, sizeof(buf), "%.*f", xdecimals, xhi);
        const float labelW = ctx.font.GetTextWidth(buf) + 10.0f;
        int every = 1;
        while (xticks / every > 0 && area.w / static_cast<float>(xticks / every) < labelW)
            ++every;
        for (int k = 0; k <= xticks; k += every)
        {
            const double v = xlo + xstep * static_cast<double>(k);
            const float px = floorf(xOf(v)) + 0.5f;
            if (showGrid_)
            {
                ctx.line.SetColor(t.borderColor.r, t.borderColor.g, t.borderColor.b, 110);
                ctx.drawLine(px, ay, px, ay + area.h);
            }
            snprintf(buf, sizeof(buf), "%.*f", xdecimals, v);
            const float w = ctx.font.GetTextWidth(buf);
            const float asc = setupFont(ctx, t.textDisabled);
            ctx.font.Print(buf, px - w * 0.5f, ay + area.h + kAxisPad + asc);
        }
    }
    else if (slots > 0)
    {
        const float slotW = area.w / static_cast<float>(slots);
        for (int i = 0; i < slots; ++i)
        {
            const float px = xOf(static_cast<double>(i));
            if (i < categoryCount())
            {
                char fitted[128];
                fitText(ctx, categories_[static_cast<size_t>(i)].c_str(), slotW - 4.0f, fitted, sizeof(fitted));
                const float w = ctx.font.GetTextWidth(fitted);
                if (w > 0.0f)
                {
                    const float asc = setupFont(ctx, categoryVisible(i) ? t.textColor : t.textDisabled);
                    ctx.font.Print(fitted, px - w * 0.5f, ay + area.h + kAxisPad + asc);
                }
            }
            else
            {
                snprintf(buf, sizeof(buf), "%d", i);
                const float w = ctx.font.GetTextWidth(buf);
                const float asc = setupFont(ctx, t.textDisabled);
                ctx.font.Print(buf, px - w * 0.5f, ay + area.h + kAxisPad + asc);
            }
        }
    }

    // The axis lines.
    ctx.line.SetColor(t.textDisabled.r, t.textDisabled.g, t.textDisabled.b, 255);
    ctx.drawLine(ax + 0.5f, ay, ax + 0.5f, ay + area.h);
    ctx.drawLine(ax, floorf(ay + area.h) + 0.5f, ax + area.w, floorf(ay + area.h) + 0.5f);

    if (!any)
    {
        const float asc = setupFont(ctx, t.textDisabled);
        const char* note = "no data";
        ctx.font.Print(note, ax + (area.w - ctx.font.GetTextWidth(note)) * 0.5f, ay + area.h * 0.5f - lh * 0.5f + asc);
        return;
    }

    // Each series: its line, then its markers, so the dots sit on the line.
    ctx.pushClip(Rect(ax - markerSize_ - 2.0f, ay - markerSize_ - 2.0f, area.w + 2.0f * markerSize_ + 4.0f, area.h + 2.0f * markerSize_ + 4.0f));
    for (int s = 0; s < seriesCount(); ++s)
    {
        const ChartSeries& se = series_[static_cast<size_t>(s)];
        if (!se.visible)
            continue;
        const int n = pointCount(s);
        polyline_.clear();
        const size_t firstPoint = points_.size();
        for (int i = 0; i < n; ++i)
        {
            if (se.xs.empty() && i < categoryCount() && !categoryVisible(i))
                continue;
            Point pt;
            pt.series = s;
            pt.index = i;
            pt.x = xOf(xD(s, i)) - b.x;
            pt.y = yOf(valueD(s, i)) - b.y;
            points_.push_back(pt);
            polyline_.push_back(Vec2{ pt.x + b.x, pt.y + b.y });
        }
        const Color c = se.color;
        if (se.line && polyline_.size() >= 2)
        {
            ctx.line.SetColor(c.r, c.g, c.b, c.a);
            ctx.drawPolyline(polyline_, lineWidth_, false);
        }
        const bool dots = se.markers && (!se.line || n <= 60);
        for (size_t k = firstPoint; k < points_.size(); ++k)
        {
            const Point& pt = points_[k];
            const bool hot = hovered_.series == pt.series && hovered_.index == pt.index;
            if (!dots && !hot)
                continue;
            const Color fill = hot ? lighten(c, 0.3f) : c;
            ctx.fill.SetColor(fill.r, fill.g, fill.b, fill.a);
            ctx.fillCircle(pt.x + b.x, pt.y + b.y, hot ? markerSize_ + 2.0f : markerSize_);
            if (showValues_ && n <= 24)
            {
                formatValueD(valueD(pt.series, pt.index), buf, sizeof(buf));
                const float w = ctx.font.GetTextWidth(buf);
                const float asc = setupFont(ctx, t.textColor);
                ctx.font.Print(buf, pt.x + b.x - w * 0.5f, pt.y + b.y - markerSize_ - lh + asc);
            }
        }
    }
    ctx.popClip();
}

// The x of a point as the tooltip prints it: the shortest decimal (from 6 to 15 digits) that gives the number back, so
// that 1700000000.001 reads as such and 0.5 as 0.5; a number that has none (a float widened to double, 0.1f) is
// printed with the 6 digits %g always gave it.
void Chart::formatX(double v, char* out, int outSize)
{
    for (int digits = 6; digits <= 15; ++digits)
    {
        snprintf(out, static_cast<size_t>(outSize), "%.*g", digits, v);
        if (strtod(out, nullptr) == v)
            return;
    }
    snprintf(out, static_cast<size_t>(outSize), "%g", v);
}

// The text of the tooltip of an item: what the last paint would show for it.
void Chart::tooltipInto(const Item& item, char* text, int size) const
{
    text[0] = '\0';
    if (!item.valid() || item.series >= seriesCount())
        return;
    char valueText[400];
    formatValueD(valueD(item.series, item.index), valueText, sizeof(valueText));
    const char* category = item.index < categoryCount() ? categories_[static_cast<size_t>(item.index)].c_str() : "";
    if (isXY())
    {
        const ChartSeries& se = series_[static_cast<size_t>(item.series)];
        char xText[400];
        if (se.xs.empty() && item.index < categoryCount())
            snprintf(xText, sizeof(xText), "%s", category);
        else
            formatX(xD(item.series, item.index), xText, sizeof(xText));
        const String name = seriesLabel(item.series);
        if (name.empty())
            snprintf(text, static_cast<size_t>(size), "%s: %s", xText, valueText);
        else
            snprintf(text, static_cast<size_t>(size), "%s - %s: %s", name.c_str(), xText, valueText);
    }
    else if (isBars())
    {
        const String name = seriesLabel(item.series);
        if (name.empty())
            snprintf(text, static_cast<size_t>(size), "%s: %s", category, valueText);
        else
            snprintf(text, static_cast<size_t>(size), "%s - %s: %s", category, name.c_str(), valueText);
    }
    else
    {
        const double sum = totalD(item.series);
        const double pct = sum > 0.0 ? valueD(item.series, item.index) / sum * 100.0 : 0.0;
        snprintf(text, static_cast<size_t>(size), "%s: %s (%.1f%%)", category, valueText, pct);
    }
}

String Chart::tooltipText(const Item& item) const
{
    char text[1024];
    tooltipInto(item, text, sizeof(text));
    return String(text);
}

// A series by the name the legend gives it: its own, or "Series N" (counted from one) for one with no name that has
// others beside it. A single series with no name has none (it has no legend either).
String Chart::seriesLabel(int series) const
{
    if (series < 0 || series >= seriesCount())
        return String();
    const String& name = series_[static_cast<size_t>(series)].name;
    if (!name.empty() || seriesCount() == 1)
        return name;
    return String("Series ") + String::number(series + 1);
}

String Chart::xTickText(double v) const
{
    char buf[400];
    snprintf(buf, sizeof(buf), "%.*f", decimalsFor(xAxisStep_), v);
    return String(buf);
}

String Chart::yTickText(double v) const
{
    char buf[400];
    snprintf(buf, sizeof(buf), "%.*f%s", valueDecimals_ >= 0 ? valueDecimals_ : decimalsFor(axisStep_), v, valueSuffix_.c_str());
    return String(buf);
}

void Chart::paintTooltip(PaintContext& ctx, const Rect& b)
{
    if (!hovered_.valid() || mouseX_ < 0.0f)
        return;
    const Theme& t = Theme::instance();
    const float lh = lineHeight(ctx);
    char text[1024];
    tooltipInto(hovered_, text, sizeof(text));
    const float w = ctx.font.GetTextWidth(text) + 2.0f * t.tooltipPadX;
    const float h = lh + 2.0f * t.tooltipPadY;
    float x = b.x + mouseX_ + 14.0f;
    float y = b.y + mouseY_ + 18.0f;
    if (x + w > b.right() - 2.0f) x = b.x + mouseX_ - w - 6.0f;
    if (y + h > b.bottom() - 2.0f) y = b.y + mouseY_ - h - 6.0f;
    if (x < b.x) x = b.x + 2.0f;
    if (y < b.y) y = b.y + 2.0f;
    ctx.fill.SetColor(t.tooltipBg.r, t.tooltipBg.g, t.tooltipBg.b, t.tooltipBg.a);
    ctx.fillRoundedRect(x, y, w, h, 3.0f);
    ctx.line.SetColor(t.tooltipBorder.r, t.tooltipBorder.g, t.tooltipBorder.b, t.tooltipBorder.a);
    ctx.lineRoundedRect(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, 3.0f);
    const float asc = setupFont(ctx, t.tooltipText);
    ctx.font.Print(text, x + t.tooltipPadX, y + t.tooltipPadY + asc);
}

void Chart::paint(PaintContext& ctx)
{
    const Rect b = absoluteRect();
    if (b.w <= 0.0f || b.h <= 0.0f)
        return;
    paintBackground(ctx, b);
    ctx.pushClip(b);

    const float titleH = paintTitle(ctx, b);
    const Rect titleArea(0.0f, 0.0f, b.w, titleH);
    Rect plot(kPad, titleH + kPad, b.w - 2.0f * kPad, b.h - titleH - 2.0f * kPad);
    buildLegend(ctx, b, titleArea, plot);
    if (plot.w < 0.0f) plot.w = 0.0f;
    if (plot.h < 0.0f) plot.h = 0.0f;
    if ((isBars() || isXY()) && (!xTitle_.empty() || !yTitle_.empty()))
        paintAxisTitles(ctx, b, plot);
    plot_ = plot;

    if (isBars())
        paintBars(ctx, b, plot);
    else if (isXY())
        paintXY(ctx, b, plot);
    else
        paintPie(ctx, b, plot);
    paintLegend(ctx, b);
    paintTooltip(ctx, b);

    ctx.popClip();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Mouse
// ═════════════════════════════════════════════════════════════════════════════

void Chart::setHovered(Item item)
{
    if (item == hovered_)
        return;
    hovered_ = item;
    markDirty();
    hoverChanged.emit(item); // (last: a handler may take the chart out of the tree, see WidgetApp::hasGone)
}

void Chart::onMousePress(MouseEvent& e)
{
    Widget::onMousePress(e);
    if (e.button != 0)
        return;
    const Rect b = absoluteRect();
    const float lx = e.x - b.x;
    const float ly = e.y - b.y;
    const int entry = legendHitTest(lx, ly);
    if (entry >= 0)
    {
        const LegendEntry& le = legend_[static_cast<size_t>(entry)];
        if (!isPie())
            setSeriesVisible(le.target, !le.visible);
        else
            setCategoryVisible(le.target, !le.visible);
        legendToggled.emit(entry);
        e.consumed = true;
        return;
    }
    const Item item = hitTest(lx, ly);
    if (item.valid())
    {
        // The pressed item is the hovered one while the handlers run, even
        // when no move came first (a touch, a synthetic press).
        e.consumed = true;
        WidgetApp& app = WidgetApp::instance();
        const uint32_t seen = app.deletions();
        setHovered(item);
        if (app.hasGone(this, seen)) return;
        itemPressed.emit(item.series, item.index);
    }
}

void Chart::onMouseMove(MouseEvent& e)
{
    Widget::onMouseMove(e);
    const Rect b = absoluteRect();
    const float lx = e.x - b.x;
    const float ly = e.y - b.y;
    const bool overLegend = legendHitTest(lx, ly) >= 0;
    const bool wasOverLegend = legendHitTest(mouseX_, mouseY_) >= 0;
    mouseX_ = lx;
    mouseY_ = ly;
    if (overLegend != wasOverLegend)
        markDirty();
    WidgetApp& app = WidgetApp::instance();
    const uint32_t seen = app.deletions();
    setHovered(hitTest(lx, ly));
    if (app.hasGone(this, seen)) return;
    if (hovered_.valid())
        markDirty();   // the tooltip follows the mouse
}

void Chart::onMouseLeave()
{
    Widget::onMouseLeave();
    mouseX_ = -1.0f;
    mouseY_ = -1.0f;
    setHovered(Item());
}

} // namespace ig::retained
