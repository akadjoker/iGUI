#include "pch.hpp"

#include "PlotChart.hpp"
#include "PlotChartCore.hpp"

#ifndef NDEBUG
#define IGUI_PLOT_REQUIRE(cond, message)                                                                           \
    do                                                                                                             \
    {                                                                                                              \
        if (!(cond))                                                                                               \
        {                                                                                                          \
            fprintf(stderr, "PlotChart: %s\n", message);                                                           \
            __builtin_trap();                                                                                      \
        }                                                                                                          \
    } while (0)
#else
#define IGUI_PLOT_REQUIRE(cond, message) \
    do                                   \
    {                                    \
    } while (0)
#endif

namespace ig
{
namespace retained
{

// ═════════════════════════════════════════════════════════════════════════════
//  Ticks and labels
// ═════════════════════════════════════════════════════════════════════════════

namespace plot
{

double niceStep(double range, int maxTicks)
{
    if (!(range > 0.0) || maxTicks < 1) return 1.0;
    const double raw = range / static_cast<double>(maxTicks);
    const double mag = pow(10.0, floor(log10(raw)));
    const double r = raw / mag;
    const double step = r <= 1.0 ? 1.0 : r <= 2.0 ? 2.0 : r <= 5.0 ? 5.0 : 10.0;
    return step * mag;
}

double niceTimeStep(double range, int maxTicks)
{
    if (!(range > 0.0) || maxTicks < 1) return 1.0;
    const double raw = range / static_cast<double>(maxTicks);
    if (raw < 1.0) return niceStep(range, maxTicks);
    static const double steps[] = { 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600, 7200, 10800, 21600, 43200, 86400, 172800, 432000, 864000 };
    for (size_t i = 0; i < sizeof steps / sizeof steps[0]; ++i)
        if (steps[i] >= raw) return steps[i];
    // beyond ten days: whole days in 1, 2, 5 times a power of ten
    return niceStep(raw / 86400.0 * maxTicks, maxTicks) * 86400.0;
}

void ticksIn(double lo, double hi, double step, ct::Vector<double>& out)
{
    out.clear();
    if (!(step > 0.0) || !(hi >= lo) || lo - lo != 0.0 || hi - hi != 0.0) return;
    const double first = ceil(lo / step - 1e-9);
    const double last = floor(hi / step + 1e-9);
    if (last - first > 10000.0) return;
    for (double k = first; k <= last; k += 1.0) out.push_back(k * step);
}

int formatTimeInto(char* out, int capacity, double seconds, double step)
{
    if (capacity < 2) return 0;
    const size_t cap = static_cast<size_t>(capacity);
    if (seconds != seconds) return snprintf(out, cap, "nan");
    if (step >= 86400.0)
    {
        const double days = floor(seconds / 86400.0 + 1e-9);
        const double rest = seconds - days * 86400.0;
        const int hh = static_cast<int>(rest / 3600.0 + 1e-9), mm = static_cast<int>(fmod(rest, 3600.0) / 60.0 + 1e-9);
        if (hh == 0 && mm == 0) return snprintf(out, cap, "%.0fd", days);
        return snprintf(out, cap, "%.0fd %02d:%02d", days, hh, mm);
    }
    if (step >= 60.0)
    {
        const double total = floor(fabs(seconds) + 1e-9);
        const long h = static_cast<long>(total / 3600.0), m = static_cast<long>(fmod(total, 3600.0) / 60.0), sec = static_cast<long>(fmod(total, 60.0));
        const char* sign = seconds < 0.0 ? "-" : "";
        if (step >= 3600.0) return snprintf(out, cap, "%s%ld:%02ld", sign, h, m);
        if (h > 0) return snprintf(out, cap, "%s%ld:%02ld:%02ld", sign, h, m, sec);
        return snprintf(out, cap, "%s%ld:%02ld", sign, m, sec);
    }
    int decimals = 0;
    if (step < 1.0) decimals = static_cast<int>(ceil(-log10(step) - 1e-9));
    if (decimals < 0) decimals = 0;
    if (decimals > 9) decimals = 9;
    if (seconds == 0.0) seconds = 0.0; // (no "-0.000")
    return snprintf(out, cap, "%.*f", decimals, seconds);
}

String formatTime(double seconds, double step)
{
    char buffer[64];
    formatTimeInto(buffer, sizeof buffer, seconds, step);
    return String(buffer);
}

int formatValueInto(char* out, int capacity, double value, double step)
{
    if (capacity < 2) return 0;
    const size_t cap = static_cast<size_t>(capacity);
    if (value != value) return snprintf(out, cap, "nan");
    const double a = fabs(value);
    if ((a >= 1.0e7) || (a != 0.0 && a < 1.0e-4)) return snprintf(out, cap, "%.3g", value);
    int decimals = 0;
    if (step > 0.0 && step < 1.0) decimals = static_cast<int>(ceil(-log10(step) - 1e-9));
    if (decimals < 0) decimals = 0;
    if (decimals > 8) decimals = 8;
    if (value == 0.0) value = 0.0;
    return snprintf(out, cap, "%.*f", decimals, value);
}

String formatValue(double value, double step)
{
    char buffer[64];
    formatValueInto(buffer, sizeof buffer, value, step);
    return String(buffer);
}

} // namespace plot

// ═════════════════════════════════════════════════════════════════════════════
//  The chart's state
// ═════════════════════════════════════════════════════════════════════════════

namespace
{

constexpr size_t kPyramidBudget = 1u << 20; // samples reduced a frame, over all the series
constexpr float kMiterLimit = 2.5f;
constexpr int kMaxMarkersDots = 48;

uint64_t fnv(uint64_t h, const void* p, size_t n)
{
    const unsigned char* b = static_cast<const unsigned char*>(p);
    for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
    return h;
}

inline double clampd(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

Color withAlpha(const Color& c, int a)
{
    return Color(c.r, c.g, c.b, static_cast<uint8_t>(a < 0 ? 0 : (a > 255 ? 255 : a)));
}

float luma(const Color& c)
{
    return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
}

// black or white, whichever has more contrast over `bg` (a flag drawn in a marker's colour)
Color onColour(const Color& bg)
{
    auto channel = [](unsigned char v)
    {
        const float f = static_cast<float>(v) / 255.0f;
        return f <= 0.03928f ? f / 12.92f : powf((f + 0.055f) / 1.055f, 2.4f);
    };
    const float l = 0.2126f * channel(bg.r) + 0.7152f * channel(bg.g) + 0.0722f * channel(bg.b);
    const float white = 1.05f / (l + 0.05f), black = (l + 0.05f) / 0.05f;
    return white >= black ? Color(255, 255, 255, 255) : Color(0, 0, 0, 255);
}

struct Marker
{
    int id;
    double x;
    Color color;
    String text;
};

struct Band
{
    int id;
    double x0, x1;
    Color color;
};

struct Series
{
    PlotSeriesInfo info;
    int slot = 0;
    const double* x = nullptr;
    const double* y = nullptr;
    size_t n = 0;
    uint64_t revision = 0;
    bool owns = false;
    ct::Vector<double> ownX, ownY;
    plot::Pyramid pyr;
    bool pyrStarted = false;
    // what the debug check keeps of the samples it saw
    double fingerprint = 0.0;
    // the width of the name, measured for a font
    uint32_t nameFont = 0;
    float nameW = 0.0f;
    // what the last layer drew
    bool uniform = false;                 // the x are, to a small error, x[0] + i * step: the column ends of the chart are then found by a formula
    double step = 0.0;
    size_t meshFirst = 0;                 // envelope: the first vertex of the series in the layer's mesh: a (top, bottom) pair for each column drawn
    ct::Vector<Vec2> poly;                // polyline: the points
    bool envelope = false;
    int drawnPoints = 0;
};

// Whether the ascending x are evenly spaced (a sampled signal): checked at 65 places. A wrong guess costs nothing but speed: the column ends are
// found by lowerBoundFrom, which is exact from any start.
bool isUniform(const double* x, size_t n, double& step)
{
    step = 0.0;
    if (!x || n < 16) return false;
    const double dt = (x[n - 1] - x[0]) / static_cast<double>(n - 1);
    if (!(dt > 0.0)) return false;
    for (size_t k = 0; k <= 64; ++k)
    {
        const size_t i = k * (n - 1) / 64;
        const double expect = x[0] + static_cast<double>(i) * dt;
        if (!(fabs(x[i] - expect) <= dt * 0.25)) return false;
    }
    step = dt;
    return true;
}

double fingerprintOf(const double* y, size_t n)
{
    if (!y || n == 0) return 0.0;
    double sum = 0.0;
    const size_t step = n / 16 + 1;
    for (size_t i = 0; i < n; i += step)
        if (y[i] == y[i]) sum += y[i] * static_cast<double>((i % 7) + 1);
    if (y[n - 1] == y[n - 1]) sum += y[n - 1] * 3.0;
    return sum;
}

} // namespace

struct PlotChart::Impl
{
    PlotChart* self = nullptr;

    // ── data ──
    ct::Vector<Series*> series;
    int nextSlot = 0;
    uint32_t dataVersion = 1;
    uint32_t formatVersion = 1;
    uint32_t statsVersion = 1;

    // ── the view ──
    double x0 = 0.0, x1 = 10.0;
    bool viewTouched = false; // the view was set (or fitted) by someone: until then the first paint fits it
    bool extentSet = false;
    double extentLo = 0.0, extentHi = 0.0;
    PlotYMode yMode[2] = { PlotYMode::Auto, PlotYMode::Auto };
    double manualLo[2] = { 0.0, 1.0 };
    double manualHi[2] = { 1.0, 1.0 };
    double yLo[2] = { 0.0, 0.0 }, yHi[2] = { 1.0, 1.0 }; // as of the last layer
    bool timeAxis = true;
    ct::Function<String(double, double)> xFormat;
    ct::Function<String(double, double)> yFormat[2];
    bool followOn = false;
    double followX = 0.0;

    // ── overlay ──
    bool clockValid = false;
    double clockT = 0.0;
    ct::Vector<Marker> markers;
    ct::Vector<Band> bands;
    bool rangeOn = false;
    double rangeA = 0.0, rangeB = 0.0;
    bool rangeTool = false;
    bool cursorEnabled = true;
    bool legendVisible = true;
    bool legendRemovable = false;
    bool legendAutoToggle = true;
    String readoutHeader;
    ct::Vector<String> readoutValues;
    bool readoutSet = false;
    String note;
    ct::Vector<String> statsRows;
    ct::Vector<PlotStatsColumn> statsColumns;
    bool interactive = true;
    PlotBindings bindings;

    // ── hover ──
    bool hover = false;
    bool hoverInside = false;
    double hoverXData = 0.0, hoverYData = 0.0;
    float hoverPx = 0.0f, hoverPy = 0.0f;
    int hoverSeries = -1, hoverPoint = -1;
    uint32_t hoverVersion = 0;
    struct Dot
    {
        float px, py;
        Color color;
        int series;
        char text[28];
    };
    ct::Vector<Dot> dots;

    // ── gestures ──
    enum class Drag
    {
        None,
        Armed,
        Pan,
        Range
    };
    Drag drag = Drag::None;
    int dragButton = 0;
    int dragMods = 0;
    float pressX = 0.0f, pressY = 0.0f;
    float lastX = 0.0f;
    double dragView0 = 0.0, dragView1 = 0.0;
    double dragAnchorX = 0.0;
    bool dragIsRange = false;
    bool pressedInPlot = false;

    // ── geometry and the layer ──
    Rect abs = { 0, 0, 0, 0 };
    Rect plot = { 0, 0, 0, 0 };
    float headerH = 0.0f, axisH = 0.0f, statsH = 0.0f, leftW = 0.0f, rightW = 0.0f;
    bool usesRight = false;
    Mesh mesh;
    bool layerBuilt = false;
    uint32_t rebuilds = 0;
    uint64_t key = 0;
    uint64_t keyThemeless = 0;
    TextureHandle meshTexture;
    struct Label
    {
        double value;
        float pos; // plot pixels
        String text;
        float w;
    };
    ct::Vector<Label> xLabels;
    ct::Vector<Label> yLabels[2];
    ct::Vector<double> tickScratch;
    float valueW = 0.0f;
    float rowH = 0.0f;
    struct LegendEntry
    {
        Rect box;
        Rect cross;
        float valueX;
        int series;
    };
    ct::Vector<LegendEntry> legend;
    float headerTextY = 0.0f;
    // for a paint that has to build it
    uint32_t fontId = 0;
    float fontHeight = 0.0f;
    float scale = 1.0f;
    bool pyramidsPending = false;

    // ── helpers ──
    int indexOf(int id) const
    {
        for (size_t i = 0; i < series.size(); ++i)
            if (series[i]->info.id == id) return static_cast<int>(i);
        return -1;
    }
    Series* find(int id)
    {
        const int i = indexOf(id);
        return i < 0 ? nullptr : series[static_cast<size_t>(i)];
    }
    const Series* find(int id) const
    {
        const int i = indexOf(id);
        return i < 0 ? nullptr : series[static_cast<size_t>(i)];
    }
    bool isXY(const Series& s) const
    {
        return s.info.xSource >= 0 && find(s.info.xSource) != nullptr;
    }
    Color colourOf(const Series& s) const
    {
        if (s.info.color.a > 0) return s.info.color;
        return Theme::instance().plot.series[s.slot % 8];
    }
    void touchData()
    {
        ++dataVersion;
        self->markDirty();
    }
    void resetPyramid(Series& s)
    {
        s.pyr.begin(s.n);
        s.pyrStarted = true;
    }

    // ── what the data says ──
    // the x of a sample, and its y; for an XY series the x is the value of the source series
    double sampleX(const Series& s, size_t i) const
    {
        if (s.info.xSource >= 0)
            if (const Series* src = find(s.info.xSource)) return i < src->n ? src->y[i] : static_cast<double>(NAN);
        return s.x ? s.x[i] : static_cast<double>(i);
    }
    size_t usable(const Series& s) const
    {
        if (s.info.xSource >= 0)
            if (const Series* src = find(s.info.xSource)) return s.n < src->n ? s.n : src->n;
        return s.n;
    }
    void fullExtentX(double& lo, double& hi, bool& any) const
    {
        any = false;
        lo = hi = 0.0;
        for (size_t k = 0; k < series.size(); ++k)
        {
            const Series& s = *series[k];
            if (!s.info.visible || s.n == 0) continue;
            double a, b;
            if (isXY(s))
            {
                const Series* src = find(s.info.xSource);
                const plot::MinMax m = src->pyr.query(src->y, 0, src->n);
                if (!m.any) continue;
                a = m.lo;
                b = m.hi;
            }
            else
            {
                if (!s.x) continue;
                a = s.x[0];
                b = s.x[s.n - 1];
            }
            if (!(a == a) || !(b == b)) continue;
            if (!any)
            {
                lo = a;
                hi = b;
                any = true;
            }
            else
            {
                if (a < lo) lo = a;
                if (b > hi) hi = b;
            }
        }
    }
    void extentOrData(double& lo, double& hi) const
    {
        if (extentSet)
        {
            lo = extentLo;
            hi = extentHi;
            return;
        }
        bool any;
        fullExtentX(lo, hi, any);
        if (!any)
        {
            lo = 0.0;
            hi = 10.0;
        }
        if (!(hi > lo))
        {
            lo -= 0.5;
            hi += 0.5;
        }
    }
    // y range of a series over [a, b) samples: the pyramid where it is built
    plot::MinMax rangeOf(const Series& s, size_t a, size_t b) const
    {
        return s.pyr.query(s.y, a, b);
    }
    plot::MinMax visibleY(const Series& s, double vx0, double vx1) const
    {
        plot::MinMax out;
        if (isXY(s))
        {
            const size_t n = usable(s);
            plot::MinMax m = s.pyr.query(s.y, 0, n);
            return m;
        }
        if (!s.x || s.n == 0) return out;
        size_t a = plot::lowerBound(s.x, s.n, vx0);
        size_t b = plot::lowerBound(s.x, s.n, vx1);
        if (b < s.n) ++b;
        if (a > 0) --a;
        return rangeOf(s, a, b);
    }
    void computeYRanges()
    {
        for (int axis = 0; axis < 2; ++axis)
        {
            if (yMode[axis] == PlotYMode::Manual)
            {
                yLo[axis] = manualLo[axis];
                yHi[axis] = manualHi[axis];
                continue;
            }
            plot::MinMax all;
            for (size_t k = 0; k < series.size(); ++k)
            {
                const Series& s = *series[k];
                if (!s.info.visible || s.info.yAxis != axis || s.n == 0) continue;
                const plot::MinMax m = yMode[axis] == PlotYMode::Full ? (isXY(s) ? visibleY(s, 0, 0) : rangeOf(s, 0, s.n)) : visibleY(s, x0, x1);
                all.merge(m);
            }
            if (!all.any)
            {
                yLo[axis] = 0.0;
                yHi[axis] = 1.0;
                continue;
            }
            double lo = all.lo, hi = all.hi;
            double pad = (hi - lo) * 0.05;
            if (!(pad > 0.0)) pad = fabs(hi) > 0.0 ? fabs(hi) * 0.05 : 0.5;
            yLo[axis] = lo - pad;
            yHi[axis] = hi + pad;
        }
    }

    // ── mapping ──
    double xToPx(double x) const
    {
        return (x - x0) / (x1 - x0) * static_cast<double>(plot.w);
    }
    double pxToX(double px) const
    {
        return x0 + px / static_cast<double>(plot.w) * (x1 - x0);
    }
    double yToPx(int axis, double y) const
    {
        return (yHi[axis] - y) / (yHi[axis] - yLo[axis]) * static_cast<double>(plot.h);
    }
    double pxToY(int axis, double py) const
    {
        return yHi[axis] - py / static_cast<double>(plot.h) * (yHi[axis] - yLo[axis]);
    }

    // ── the pyramids ──
    // a few frames' worth of reduction for each series that needs it; true when some are not done
    bool stepPyramids()
    {
        size_t budget = kPyramidBudget;
        bool pending = false;
        for (size_t k = 0; k < series.size(); ++k)
        {
            Series& s = *series[k];
            if (!s.pyrStarted && s.n > 0 && s.y) resetPyramid(s);
            if (s.pyr.ready() || s.n == 0) continue;
            if (budget == 0)
            {
                pending = true;
                continue;
            }
            const size_t chunk = budget < s.n ? budget : s.n;
            const bool done = s.pyr.build(s.y, chunk);
            budget = budget > chunk ? budget - chunk : 0;
            if (done) ++dataVersion; // (the reduction is exact now: the layer is made again)
            else
                pending = true;
        }
        return pending;
    }

    // ── debug ──
    void checkContract(const Series& s) const
    {
#ifndef NDEBUG
        if (s.owns || !s.y) return;
        IGUI_PLOT_REQUIRE(fingerprintOf(s.y, s.n) == s.fingerprint,
                "series data changed in place with the same revision (a revision is the contract that the data is unchanged), or the arrays "
                "were freed: give setSeriesData a new revision when the data changes, and keep the arrays alive until the next setSeriesData");
#else
        (void) s;
#endif
    }

    // ── geometry building ──
    void layout(PaintContext& ctx, const Rect& a);
    uint64_t makeKey(const Rect& a, PaintContext& ctx) const;
    void rebuild(PaintContext& ctx, const Rect& a);
    void buildLegend(PaintContext& ctx);
    void buildTicks(PaintContext& ctx);
    void buildMesh(PaintContext& ctx);
    void addQuad(float x0, float y0, float x1, float y1, const Color& c, const Vec2& uv);
    void addSeries(Series& s, const Color& c, const Vec2& uv);
    void addEnvelope(Series& s, const Color& c, const Vec2& uv);
    void addPolyline(Series& s, const Color& c, const Vec2& uv);

    // ── interaction ──
    void emitView(bool byUser);
    bool emitNow(bool byUser);
    bool setX(double a, double b, bool byUser);
    void pick(float px, float py);
    void updateDots();
    bool matches(const PlotBinding& b, int button, bool ctrl, bool shift, bool alt, bool wheel) const
    {
        if (!b.enabled) return false;
        if (!wheel && b.button != button) return false;
        const int mods = (shift ? PlotModShift : 0) | (ctrl ? PlotModCtrl : 0) | (alt ? PlotModAlt : 0);
        return b.mods == mods;
    }
    Rect absolute(const Rect& r) const
    {
        return { r.x + abs.x, r.y + abs.y, r.w, r.h };
    }
    bool inPlot(float x, float y) const
    {
        return x >= plot.x && x < plot.x + plot.w && y >= plot.y && y < plot.y + plot.h;
    }
};

// ═════════════════════════════════════════════════════════════════════════════
//  The layer
// ═════════════════════════════════════════════════════════════════════════════

uint64_t PlotChart::Impl::makeKey(const Rect& a, PaintContext& ctx) const
{
    const Theme& t = Theme::instance();
    uint64_t h = 1469598103934665603ull;
    const float geometry[4] = { a.w, a.h, ctx.fontHeight(), scale };
    h = fnv(h, geometry, sizeof geometry);
    const double view[4] = { x0, x1, extentSet ? extentLo : 0.0, extentSet ? extentHi : 0.0 };
    h = fnv(h, view, sizeof view);
    const double manual[4] = { manualLo[0], manualHi[0], manualLo[1], manualHi[1] };
    h = fnv(h, manual, sizeof manual);
    const int modes[8] = { static_cast<int>(yMode[0]), static_cast<int>(yMode[1]), timeAxis ? 1 : 0, legendVisible ? 1 : 0, legendRemovable ? 1 : 0,
        note.empty() ? 0 : 1, static_cast<int>(statsRows.size()), static_cast<int>(statsColumns.size()) };
    h = fnv(h, modes, sizeof modes);
    const uint32_t versions[3] = { dataVersion, formatVersion, statsVersion };
    h = fnv(h, versions, sizeof versions);
    h = fnv(h, &fontId, sizeof fontId);
    // the colours the layer is made of, and the metrics it depends on
    h = fnv(h, &t.plot, sizeof t.plot);
    h = fnv(h, &t.panelColor, sizeof t.panelColor);
    h = fnv(h, &t.textColor, sizeof t.textColor);
    h = fnv(h, &t.textSecondary, sizeof t.textSecondary);
    h = fnv(h, &t.padding, sizeof t.padding);
    h = fnv(h, &t.fontSize, sizeof t.fontSize);
    const uintptr_t tex[3] = { meshTexture.value, 0, 0 };
    h = fnv(h, tex, sizeof tex);
    return h;
}

void PlotChart::Impl::buildLegend(PaintContext& ctx)
{
    const Theme& t = Theme::instance();
    legend.clear();
    const float pad = 4.0f;
    rowH = ctx.fontHeight() + 6.0f;
    headerH = rowH + 2.0f; // the header line: the time of the cursor, the note
    // the width of a value: the widest a value text goes
    valueW = ctx.font.GetTextWidth("-00000.000");
    if (!legendVisible) return;
    float x = pad, y = headerH; // (relative to the widget: it may move without the layer being made again)
    const float right = abs.w - pad;
    const float swatch = ctx.fontHeight() * 0.7f;
    int rows = 1;
    for (size_t k = 0; k < series.size(); ++k)
    {
        Series& s = *series[k];
        if (s.nameFont != fontId)
        {
            s.nameW = ctx.font.GetTextWidth(s.info.name.c_str());
            s.nameFont = fontId;
        }
        const float cross = legendRemovable ? ctx.fontHeight() : 0.0f;
        const float w = swatch + 4.0f + s.nameW + 4.0f + valueW + (cross > 0.0f ? 4.0f + cross : 0.0f) + 10.0f;
        if (x + w > right && x > pad + 1.0f)
        {
            x = pad;
            y += rowH;
            ++rows;
        }
        LegendEntry e;
        e.box = { x, y, w - 4.0f, rowH };
        e.cross = cross > 0.0f ? Rect{ x + w - 10.0f - cross, y + (rowH - cross) * 0.5f, cross, cross } : Rect{ 0, 0, 0, 0 };
        e.valueX = x + swatch + 4.0f + s.nameW + 4.0f;
        e.series = static_cast<int>(k);
        legend.push_back(e);
        x += w;
    }
    headerH += series.empty() ? 0.0f : rows * rowH;
    (void) t;
}

void PlotChart::Impl::buildTicks(PaintContext& ctx)
{
    (void) ctx;
    const int maxY = static_cast<int>(plot.h / (ctx.fontHeight() * 2.6f));
    for (int axis = 0; axis < 2; ++axis)
    {
        yLabels[axis].clear();
        if (axis == 1 && !usesRight) continue;
        const double range = yHi[axis] - yLo[axis];
        const double step = plot::niceStep(range, maxY < 2 ? 2 : maxY);
        plot::ticksIn(yLo[axis], yHi[axis], step, tickScratch);
        for (size_t i = 0; i < tickScratch.size(); ++i)
        {
            Label l;
            l.value = tickScratch[i];
            l.pos = static_cast<float>(yToPx(axis, l.value));
            l.text = yFormat[axis] ? yFormat[axis](l.value, step) : plot::formatValue(l.value, step);
            l.w = ctx.font.GetTextWidth(l.text.c_str());
            yLabels[axis].push_back(l);
        }
    }
    // x
    xLabels.clear();
    const double range = x1 - x0;
    // a label is as wide as the widest of the ends, plus room: ask for as many as fit
    const float labelW = ctx.font.GetTextWidth("00:00:00.000000") * 0.6f + 36.0f;
    int maxX = static_cast<int>(plot.w / labelW);
    if (maxX < 2) maxX = 2;
    double step = timeAxis ? plot::niceTimeStep(range, maxX) : plot::niceStep(range, maxX);
    plot::ticksIn(x0, x1, step, tickScratch);
    // labels that would touch are thinned: every second one until none does
    for (int attempt = 0; attempt < 6; ++attempt)
    {
        xLabels.clear();
        float lastEnd = -1.0e9f;
        bool clash = false;
        for (size_t i = 0; i < tickScratch.size(); ++i)
        {
            Label l;
            l.value = tickScratch[i];
            l.pos = static_cast<float>(xToPx(l.value));
            l.text = xFormat ? xFormat(l.value, step) : (timeAxis ? plot::formatTime(l.value, step) : plot::formatValue(l.value, step));
            l.w = ctx.font.GetTextWidth(l.text.c_str());
            if (l.pos - l.w * 0.5f < lastEnd + 6.0f) clash = true;
            lastEnd = l.pos + l.w * 0.5f;
            xLabels.push_back(l);
        }
        if (!clash) break;
        step = timeAxis ? plot::niceTimeStep(step * 1.01 * (attempt % 2 == 0 ? 2.0 : 2.5), 1) : step * 2.0;
        plot::ticksIn(x0, x1, step, tickScratch);
    }
}

void PlotChart::Impl::addQuad(float qx0, float qy0, float qx1, float qy1, const Color& c, const Vec2& uv)
{
    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(DrawVertex(qx0, qy0, uv.x, uv.y, c));
    mesh.vertices.push_back(DrawVertex(qx1, qy0, uv.x, uv.y, c));
    mesh.vertices.push_back(DrawVertex(qx1, qy1, uv.x, uv.y, c));
    mesh.vertices.push_back(DrawVertex(qx0, qy1, uv.x, uv.y, c));
    const uint32_t ix[6] = { base, base + 1, base + 2, base, base + 2, base + 3 };
    for (int i = 0; i < 6; ++i) mesh.indices.push_back(ix[i]);
}

// A min/max reduction of the series by pixel column, drawn as a band: the series has more samples than columns, or says it is min/max pairs.
void PlotChart::Impl::addEnvelope(Series& s, const Color& c, const Vec2& uv)
{
    const int cols = static_cast<int>(ceilf(plot.w));
    const double dx = (x1 - x0) / static_cast<double>(plot.w);
    const double yTop = yHi[s.info.yAxis], yScale = static_cast<double>(plot.h) / (yHi[s.info.yAxis] - yLo[s.info.yAxis]);
    const float minThick = s.info.lineWidth;
    // room for the most a column adds (two vertices, six indices), written through pointers: the numbers of a whole layer are in the millions
    const size_t v0 = mesh.vertices.size(), i0 = mesh.indices.size();
    mesh.vertices.resize(v0 + 2 * static_cast<size_t>(cols));
    mesh.indices.resize(i0 + 6 * static_cast<size_t>(cols));
    DrawVertex* vp = mesh.vertices.data() + v0;
    uint32_t* ip = mesh.indices.data() + i0;
    uint32_t nv = 0, ni = 0;
    const double* xs = s.x;
    const size_t n = s.n;
    size_t a = plot::lowerBound(xs, n, x0);
    int previousColumn = -2;
    uint32_t previousBase = 0;
    for (int col = 0; col < cols; ++col)
    {
        const double xb = x0 + (col + 1) * dx;
        // the end of the column: a few steps from the end of the one before (a series of a few samples a column), else by doubling steps
        size_t b;
        if (s.uniform)
        {
            // a column reads the samples at its edges (a block of 32 beside each end), and on a big series they are not in the cache: ask for the ones a few
            // columns on while this one is reduced (the index is the formula's, near enough for a prefetch)
            {
                double ga = (xb + 6.0 * dx - xs[0]) / s.step;
                ga = ga < 0.0 ? 0.0 : (ga > static_cast<double>(n - 1) ? static_cast<double>(n - 1) : ga);
                const size_t ia = (static_cast<size_t>(ga) / plot::Pyramid::kBlock) * plot::Pyramid::kBlock;
                __builtin_prefetch(xs + static_cast<size_t>(ga));
                for (size_t k = 0; k < static_cast<size_t>(plot::Pyramid::kBlock); k += 8) __builtin_prefetch(s.y + ia + k);
            }
            // evenly spaced: the index the formula gives, then corrected (a couple of probes beside it)
            double g = (xb - xs[0]) / s.step;
            g = g < 0.0 ? 0.0 : (g > static_cast<double>(n) ? static_cast<double>(n) : g);
            b = plot::lowerBoundFrom(xs, n, static_cast<size_t>(g), xb);
        }
        else
        {
            // a few steps from the end of the column before (a series of a few samples a column), else by doubling steps
            b = a;
            const size_t lim = a + 24 < n ? a + 24 : n;
            while (b < lim && xs[b] < xb) ++b;
            if (b == lim && lim < n && xs[b] < xb) b = plot::lowerBoundFrom(xs, n, b, xb);
        }
        const size_t first = a;
        a = b;
        if (b <= first) continue;
        plot::MinMax m;
        if (!s.pyr.ready() && b - first > 256)
        {
            // the pyramid is not built yet: a strided look at the column (exact once it is)
            const size_t stride = (b - first) / 128 + 1;
            for (size_t i = first; i < b; i += stride) m.add(s.y[i]);
        }
        else
            m = s.pyr.query(s.y, first, b);
        if (!m.any) continue;
        double topD = (yTop - m.hi) * yScale, bottomD = (yTop - m.lo) * yScale;
        topD = topD < -1.0e5 ? -1.0e5 : (topD > 1.0e5 ? 1.0e5 : topD);
        bottomD = bottomD < -1.0e5 ? -1.0e5 : (bottomD > 1.0e5 ? 1.0e5 : bottomD);
        float top = static_cast<float>(topD), bottom = static_cast<float>(bottomD);
        if (bottom - top < minThick)
        {
            const float mid = (top + bottom) * 0.5f;
            top = mid - minThick * 0.5f;
            bottom = mid + minThick * 0.5f;
        }
        const float px = static_cast<float>(col) + 0.5f;
        const uint32_t base = static_cast<uint32_t>(v0) + nv;
        vp[nv++] = DrawVertex(px, top, uv.x, uv.y, c);
        vp[nv++] = DrawVertex(px, bottom, uv.x, uv.y, c);
        if (previousColumn == col - 1)
        {
            ip[ni++] = previousBase;
            ip[ni++] = previousBase + 1;
            ip[ni++] = base;
            ip[ni++] = previousBase + 1;
            ip[ni++] = base + 1;
            ip[ni++] = base;
        }
        previousColumn = col;
        previousBase = base;
    }
    mesh.vertices.resize(v0 + nv);
    mesh.indices.resize(i0 + ni);
    s.meshFirst = v0;
    s.drawnPoints = static_cast<int>(nv);
}

// The samples as a line of a width, with mitred joins: for a series with fewer samples than columns.
void PlotChart::Impl::addPolyline(Series& s, const Color& c, const Vec2& uv)
{
    s.poly.clear();
    const size_t n = usable(s);
    if (n == 0) return;
    const bool xy = isXY(s);
    size_t a = 0, b = n;
    size_t stride = 1;
    if (!xy)
    {
        a = plot::lowerBound(s.x, n, x0);
        b = plot::lowerBound(s.x, n, x1);
        if (b < n) ++b;
        if (a > 0) --a;
    }
    else
        stride = n / 20000 + 1;
    const float hw = s.info.lineWidth * 0.5f;
    const bool dots = (b - a) / stride <= static_cast<size_t>(kMaxMarkersDots);
    // runs of valid points
    ct::Vector<Vec2>& run = s.poly; // (the run being built is kept as the points drawn: the last run is what a test sees)
    ct::Vector<Vec2> scratch;
    (void) scratch;
    size_t runStart = 0;
    double lastX = 0.0;
    bool lastValid = false;
    int total = 0;
    ct::Vector<Vec2> points;
    points.reserve(b - a < 4096 ? b - a : 4096);
    auto flush = [&]()
    {
        const size_t count = points.size();
        if (count == 1)
        {
            addQuad(points[0].x - hw, points[0].y - hw, points[0].x + hw, points[0].y + hw, c, uv);
        }
        else if (count >= 2)
        {
            // the unit direction of each segment, and the normal at each point (the bisector, mitre limited)
            const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
            float pdx = 0.0f, pdy = 0.0f;
            bool havePrev = false;
            for (size_t i = 0; i < count; ++i)
            {
                float ndx = 0.0f, ndy = 0.0f;
                bool haveNext = false;
                for (size_t j = i + 1; j < count; ++j)
                {
                    const float dx = points[j].x - points[i].x, dy = points[j].y - points[i].y;
                    const float len2 = dx * dx + dy * dy;
                    if (len2 > 1.0e-10f)
                    {
                        const float inv = 1.0f / sqrtf(len2);
                        ndx = dx * inv;
                        ndy = dy * inv;
                        haveNext = true;
                        break;
                    }
                }
                float nx, ny;
                if (havePrev && haveNext)
                {
                    float bx = -(pdy + ndy), by = pdx + ndx; // the normals added
                    const float bl = sqrtf(bx * bx + by * by);
                    if (bl > 1.0e-6f)
                    {
                        bx /= bl;
                        by /= bl;
                        float cosine = bx * (-ndy) + by * ndx; // the bisector against the normal of the next segment
                        if (cosine < 1.0f / kMiterLimit) cosine = 1.0f / kMiterLimit;
                        nx = bx / cosine;
                        ny = by / cosine;
                    }
                    else
                    {
                        nx = -ndy;
                        ny = ndx;
                    }
                }
                else if (haveNext)
                {
                    nx = -ndy;
                    ny = ndx;
                }
                else
                {
                    nx = -pdy;
                    ny = pdx;
                }
                mesh.vertices.push_back(DrawVertex(points[i].x + nx * hw, points[i].y + ny * hw, uv.x, uv.y, c));
                mesh.vertices.push_back(DrawVertex(points[i].x - nx * hw, points[i].y - ny * hw, uv.x, uv.y, c));
                if (haveNext)
                {
                    pdx = ndx;
                    pdy = ndy;
                    havePrev = true;
                }
            }
            for (size_t i = 0; i + 1 < count; ++i)
            {
                const uint32_t i0 = base + 2 * static_cast<uint32_t>(i);
                const uint32_t ix[6] = { i0, i0 + 1, i0 + 2, i0 + 1, i0 + 3, i0 + 2 };
                for (int k = 0; k < 6; ++k) mesh.indices.push_back(ix[k]);
            }
        }
        total += static_cast<int>(count);
        if (dots && count > 1)
            for (size_t i = 0; i < count; ++i)
                addQuad(points[i].x - hw * 1.6f, points[i].y - hw * 1.6f, points[i].x + hw * 1.6f, points[i].y + hw * 1.6f, c, uv);
        points.clear();
    };
    (void) runStart;
    for (size_t i = a; i < b; i += stride)
    {
        const double yv = s.y[i];
        const double xv = sampleX(s, i);
        if (yv != yv || xv != xv)
        {
            flush();
            lastValid = false;
            continue;
        }
        if (lastValid && s.info.breakDx > 0.0 && xv - lastX > s.info.breakDx) flush();
        const float px = static_cast<float>(clampd(xToPx(xv), -1.0e5, 1.0e5));
        const float py = static_cast<float>(clampd(yToPx(s.info.yAxis, yv), -1.0e5, 1.0e5));
        points.push_back(Vec2{ px, py });
        if (run.size() < 2048) run.push_back(Vec2{ px, py });
        lastX = xv;
        lastValid = true;
    }
    flush();
    s.drawnPoints = total;
}

void PlotChart::Impl::addSeries(Series& s, const Color& c, const Vec2& uv)
{
    s.envelope = false;
    s.drawnPoints = 0;
    s.poly.clear();
    if (!s.info.visible || s.n == 0) return;
    if (!isXY(s) && s.x)
    {
        // more samples in view than columns: the reduction; a min/max series is always a band once it has two samples a column
        size_t a = plot::lowerBound(s.x, s.n, x0);
        size_t b = plot::lowerBound(s.x, s.n, x1);
        const size_t inView = b > a ? b - a : 0;
        if (inView > static_cast<size_t>(plot.w))
        {
            s.envelope = true;
            addEnvelope(s, c, uv);
            return;
        }
    }
    addPolyline(s, c, uv);
}

void PlotChart::Impl::buildMesh(PaintContext& ctx)
{
    const Theme& t = Theme::instance();
    mesh.vertices.clear();
    mesh.indices.clear();
    meshTexture = ctx.drawList.solidTexture();
    mesh.texture = meshTexture;
    const Vec2 uv = ctx.drawList.solidUV();
    // the background of the plot
    addQuad(0.0f, 0.0f, plot.w, plot.h, t.plot.bg, uv);
    // the grid: a line of one pixel at each tick, snapped to whole device pixels
    const float px = 1.0f / scale;
    auto snap = [&](float v) { return floorf(v * scale + 0.5f) / scale; };
    for (size_t i = 0; i < xLabels.size(); ++i)
    {
        const float x = snap(xLabels[i].pos);
        if (x < 0.0f || x > plot.w) continue;
        addQuad(x, 0.0f, x + px, plot.h, t.plot.grid, uv);
    }
    for (size_t i = 0; i < yLabels[0].size(); ++i)
    {
        const float y = snap(yLabels[0][i].pos);
        if (y < 0.0f || y > plot.h) continue;
        addQuad(0.0f, y, plot.w, y + px, t.plot.grid, uv);
    }
    // the series, in order
    for (size_t k = 0; k < series.size(); ++k)
    {
        Series& s = *series[k];
        addSeries(s, colourOf(s), uv);
    }
    mesh.changed();
}

void PlotChart::Impl::rebuild(PaintContext& ctx, const Rect& a)
{
    abs = a;
    ++rebuilds;
    // the axes: which is used, and the ranges of the data
    usesRight = false;
    for (size_t k = 0; k < series.size(); ++k)
        if (series[k]->info.visible && series[k]->info.yAxis == 1) usesRight = true;
    buildLegend(ctx);
    const float pad = 4.0f;
    axisH = ctx.fontHeight() + 8.0f;
    statsH = 0.0f;
    if (!statsRows.empty() && !statsColumns.empty()) statsH = (static_cast<float>(statsRows.size()) + 1.0f) * rowH + 6.0f;
    plot.y = a.y + headerH;
    plot.h = a.h - headerH - axisH - statsH - 2.0f;
    if (plot.h < 8.0f) plot.h = 8.0f;
    plot.w = a.w;
    // the y ranges (they need the pyramids' answers) and the ticks that make the margins
    computeYRanges();
    plot.x = a.x;
    plot.w = a.w - pad;
    leftW = rightW = 0.0f;
    // provisional plot size for the y ticks: the height does not depend on the margins
    buildTicks(ctx);
    float widest = 0.0f;
    for (size_t i = 0; i < yLabels[0].size(); ++i)
        if (yLabels[0][i].w > widest) widest = yLabels[0][i].w;
    leftW = widest + 10.0f;
    if (usesRight)
    {
        float w2 = 0.0f;
        for (size_t i = 0; i < yLabels[1].size(); ++i)
            if (yLabels[1][i].w > w2) w2 = yLabels[1][i].w;
        rightW = w2 + 10.0f;
    }
    plot.x = a.x + leftW;
    plot.w = a.w - leftW - rightW - pad;
    if (plot.w < 8.0f) plot.w = 8.0f;
    // the x ticks for the width the margins left
    buildTicks(ctx);
    buildMesh(ctx);
    layerBuilt = true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Construction and the series
// ═════════════════════════════════════════════════════════════════════════════

PlotChart::PlotChart()
        : impl_(new Impl())
{
    impl_->self = this;
    acceptsFocus_ = true;
    cursor_ = CursorType::Crosshair;
}

PlotChart::~PlotChart()
{
    for (size_t i = 0; i < impl_->series.size(); ++i) delete impl_->series[i];
    delete impl_;
}

void PlotChart::setSeries(const PlotSeriesInfo& info)
{
    Impl& d = *impl_;
    Series* s = d.find(info.id);
    bool changed = false;
    if (!s)
    {
        s = new Series();
        s->slot = d.nextSlot++;
        s->info = info;
        d.series.push_back(s);
        changed = true;
    }
    else
    {
        const PlotSeriesInfo& o = s->info;
        changed = !(o.name == info.name) || o.color.r != info.color.r || o.color.g != info.color.g || o.color.b != info.color.b || o.color.a != info.color.a ||
                  o.visible != info.visible || o.yAxis != info.yAxis || o.minMax != info.minMax || o.xSource != info.xSource || o.lineWidth != info.lineWidth ||
                  o.breakDx != info.breakDx;
        const bool unitChanged = !(o.unit == info.unit);
        if (!changed && !unitChanged) return;
        s->info = info;
        s->nameFont = 0;
    }
    if (info.yAxis < 0) s->info.yAxis = 0;
    if (info.yAxis > 1) s->info.yAxis = 1;
    if (changed) d.touchData();
    else
        markDirty();
}

void PlotChart::setSeriesData(int id, const double* x, const double* y, size_t count, uint64_t revision)
{
    Impl& d = *impl_;
    Series* s = d.find(id);
    if (!s)
    {
        PlotSeriesInfo info;
        info.id = id;
        setSeries(info);
        s = d.find(id);
    }
    if (!s->owns && s->x == x && s->y == y && s->n == count && s->revision == revision && s->pyrStarted)
    {
        d.checkContract(*s); // (the same revision: the same samples)
        return;
    }
    s->owns = false;
    s->ownX.clear();
    s->ownY.clear();
    s->x = x;
    s->y = y;
    s->n = x && y ? count : 0;
    s->revision = revision;
    s->fingerprint = fingerprintOf(y, s->n);
    s->uniform = isUniform(s->x, s->n, s->step);
    d.resetPyramid(*s);
    d.touchData();
}

void PlotChart::setSeriesDataCopy(int id, const double* x, const double* y, size_t count)
{
    Impl& d = *impl_;
    Series* s = d.find(id);
    if (!s)
    {
        PlotSeriesInfo info;
        info.id = id;
        setSeries(info);
        s = d.find(id);
    }
    s->ownX.resize(x ? count : 0);
    s->ownY.resize(y ? count : 0);
    for (size_t i = 0; x && i < count; ++i) s->ownX[i] = x[i];
    for (size_t i = 0; y && i < count; ++i) s->ownY[i] = y[i];
    s->owns = true;
    s->x = s->ownX.data();
    s->y = s->ownY.data();
    s->n = x && y ? count : 0;
    ++s->revision;
    s->fingerprint = 0.0;
    s->uniform = isUniform(s->x, s->n, s->step);
    d.resetPyramid(*s);
    d.touchData();
}

void PlotChart::removeSeries(int id)
{
    Impl& d = *impl_;
    const int i = d.indexOf(id);
    if (i < 0) return;
    delete d.series[static_cast<size_t>(i)];
    d.series.erase(d.series.begin() + i);
    d.hoverSeries = -1;
    d.touchData();
}

void PlotChart::clearSeries()
{
    Impl& d = *impl_;
    if (d.series.empty()) return;
    for (size_t i = 0; i < d.series.size(); ++i) delete d.series[i];
    d.series.clear();
    d.hoverSeries = -1;
    d.touchData();
}

int PlotChart::seriesCount() const
{
    return static_cast<int>(impl_->series.size());
}

int PlotChart::seriesIdAt(int index) const
{
    return index >= 0 && index < seriesCount() ? impl_->series[static_cast<size_t>(index)]->info.id : -1;
}

int PlotChart::seriesIndexOf(int id) const
{
    return impl_->indexOf(id);
}

const PlotSeriesInfo* PlotChart::seriesInfo(int id) const
{
    const Series* s = impl_->find(id);
    return s ? &s->info : nullptr;
}

void PlotChart::setSeriesVisible(int id, bool visible)
{
    Series* s = impl_->find(id);
    if (!s || s->info.visible == visible) return;
    s->info.visible = visible;
    impl_->touchData();
}

Color PlotChart::seriesColor(int id) const
{
    const Series* s = impl_->find(id);
    return s ? impl_->colourOf(*s) : Color(0, 0, 0, 0);
}

size_t PlotChart::seriesPointCount(int id) const
{
    const Series* s = impl_->find(id);
    return s ? s->n : 0;
}

float PlotChart::pyramidProgress(int id) const
{
    const Series* s = impl_->find(id);
    return s ? s->pyr.progress() : 1.0f;
}

// ═════════════════════════════════════════════════════════════════════════════
//  The view
// ═════════════════════════════════════════════════════════════════════════════

// The view changed: the y ranges are worked out again (an Auto axis follows the x range), then viewChanged is emitted; true when a handler took the chart away.
bool PlotChart::Impl::emitNow(bool byUser)
{
    if (yMode[0] != PlotYMode::Manual || yMode[1] != PlotYMode::Manual) computeYRanges();
    return WidgetApp::instance().emitGone(self, self->viewChanged, x0, x1, yLo[0], yHi[0], byUser);
}

void PlotChart::Impl::emitView(bool byUser)
{
    emitNow(byUser);
}

bool PlotChart::Impl::setX(double a, double b, bool byUser)
{
    if (!(a == a) || !(b == b)) return false;
    if (b < a)
    {
        const double t = a;
        a = b;
        b = t;
    }
    // the shortest and the longest view: a nanosecond, and twenty times what the data covers
    double lo, hi;
    extentOrData(lo, hi);
    const double minWidth = 1.0e-9;
    const double maxWidth = !byUser ? 1.0e15 : ((hi - lo) * 20.0 > minWidth ? (hi - lo) * 20.0 : 1.0e9);
    if (b - a < minWidth)
    {
        const double mid = (a + b) * 0.5;
        a = mid - minWidth * 0.5;
        b = mid + minWidth * 0.5;
    }
    if (b - a > maxWidth)
    {
        const double mid = (a + b) * 0.5;
        a = mid - maxWidth * 0.5;
        b = mid + maxWidth * 0.5;
    }
    viewTouched = true;
    if (a == x0 && b == x1) return false;
    x0 = a;
    x1 = b;
    self->markDirty();
    return true;
}

void PlotChart::setViewX(double a, double b)
{
    if (impl_->setX(a, b, false)) impl_->emitNow(false);
}

double PlotChart::viewX0() const
{
    return impl_->x0;
}

double PlotChart::viewX1() const
{
    return impl_->x1;
}

void PlotChart::setViewY(int axis, double y0, double y1)
{
    if (axis < 0 || axis > 1 || !(y0 == y0) || !(y1 == y1) || y0 == y1) return;
    Impl& d = *impl_;
    if (y1 < y0)
    {
        const double t = y0;
        y0 = y1;
        y1 = t;
    }
    const bool modeChanged = d.yMode[axis] != PlotYMode::Manual;
    if (!modeChanged && d.manualLo[axis] == y0 && d.manualHi[axis] == y1) return;
    d.yMode[axis] = PlotYMode::Manual;
    d.manualLo[axis] = y0;
    d.manualHi[axis] = y1;
    d.yLo[axis] = y0;
    d.yHi[axis] = y1;
    markDirty();
    if (modeChanged)
    {
        if (axis == 0 && d.self->emitYMode(axis, PlotYMode::Manual)) return;
    }
    d.emitView(false);
}

double PlotChart::viewY0(int axis) const
{
    return impl_->yLo[axis < 0 || axis > 1 ? 0 : axis];
}

double PlotChart::viewY1(int axis) const
{
    return impl_->yHi[axis < 0 || axis > 1 ? 0 : axis];
}

void PlotChart::setYMode(int axis, PlotYMode mode)
{
    if (axis < 0 || axis > 1 || impl_->yMode[axis] == mode) return;
    impl_->yMode[axis] = mode;
    impl_->touchData();
}

PlotYMode PlotChart::yMode(int axis) const
{
    return impl_->yMode[axis < 0 || axis > 1 ? 0 : axis];
}

void PlotChart::setExtent(double lo, double hi)
{
    if (!(lo == lo) || !(hi == hi) || hi <= lo) return;
    Impl& d = *impl_;
    if (d.extentSet && d.extentLo == lo && d.extentHi == hi) return;
    d.extentSet = true;
    d.extentLo = lo;
    d.extentHi = hi;
    markDirty();
}

void PlotChart::clearExtent()
{
    if (!impl_->extentSet) return;
    impl_->extentSet = false;
    markDirty();
}

void PlotChart::fit()
{
    Impl& d = *impl_;
    const bool modes = d.yMode[0] != PlotYMode::Auto || d.yMode[1] != PlotYMode::Auto;
    d.yMode[0] = d.yMode[1] = PlotYMode::Auto;
    ++d.dataVersion;
    markDirty();
    double lo, hi;
    d.extentOrData(lo, hi);
    const bool moved = d.setX(lo, hi, false);
    if (moved || modes) d.emitNow(false);
}

void PlotChart::fitX()
{
    Impl& d = *impl_;
    double lo, hi;
    d.extentOrData(lo, hi);
    markDirty();
    if (d.setX(lo, hi, false)) d.emitView(false);
}

void PlotChart::setFollowX(double x, bool follow)
{
    Impl& d = *impl_;
    d.followOn = follow;
    d.followX = x;
    if (!follow || (x >= d.x0 && x <= d.x1)) return;
    const double w = d.x1 - d.x0;
    if (d.setX(x - w * 0.75, x + w * 0.25, false)) d.emitView(false);
}

void PlotChart::zoomX(double factor, double aboutX)
{
    Impl& d = *impl_;
    if (!(factor > 0.0)) return;
    const double a = aboutX - (aboutX - d.x0) * factor, b = aboutX + (d.x1 - aboutX) * factor;
    if (d.setX(a, b, false)) d.emitView(false);
}

void PlotChart::panX(double dx)
{
    Impl& d = *impl_;
    if (d.setX(d.x0 + dx, d.x1 + dx, false)) d.emitView(false);
}

bool PlotChart::dataExtent(double& lo, double& hi) const
{
    bool any;
    impl_->fullExtentX(lo, hi, any);
    return any;
}

void PlotChart::setXTickFormatter(ct::Function<String(double, double)> fn)
{
    impl_->xFormat = static_cast<ct::Function<String(double, double)>&&>(fn);
    ++impl_->formatVersion;
    markDirty();
}

void PlotChart::setYTickFormatter(int axis, ct::Function<String(double, double)> fn)
{
    if (axis < 0 || axis > 1) return;
    impl_->yFormat[axis] = static_cast<ct::Function<String(double, double)>&&>(fn);
    ++impl_->formatVersion;
    markDirty();
}

void PlotChart::setTimeAxis(bool time)
{
    if (impl_->timeAxis == time) return;
    impl_->timeAxis = time;
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  The overlay's state
// ═════════════════════════════════════════════════════════════════════════════

void PlotChart::setClock(double t, bool valid)
{
    Impl& d = *impl_;
    if (d.clockValid == valid && (!valid || d.clockT == t)) return;
    d.clockValid = valid;
    d.clockT = t;
    markDirty();
}

void PlotChart::setMarker(int id, double x, const Color& color, const String& text)
{
    Impl& d = *impl_;
    for (size_t i = 0; i < d.markers.size(); ++i)
        if (d.markers[i].id == id)
        {
            Marker& m = d.markers[i];
            if (m.x == x && m.color.r == color.r && m.color.g == color.g && m.color.b == color.b && m.color.a == color.a && m.text == text) return;
            m.x = x;
            m.color = color;
            m.text = text;
            markDirty();
            return;
        }
    d.markers.push_back({ id, x, color, text });
    markDirty();
}

void PlotChart::removeMarker(int id)
{
    Impl& d = *impl_;
    for (size_t i = 0; i < d.markers.size(); ++i)
        if (d.markers[i].id == id)
        {
            d.markers.erase(d.markers.begin() + static_cast<ptrdiff_t>(i));
            markDirty();
            return;
        }
}

void PlotChart::clearMarkers()
{
    if (impl_->markers.empty()) return;
    impl_->markers.clear();
    markDirty();
}

void PlotChart::setBand(int id, double x0, double x1, const Color& color)
{
    Impl& d = *impl_;
    for (size_t i = 0; i < d.bands.size(); ++i)
        if (d.bands[i].id == id)
        {
            Band& b = d.bands[i];
            if (b.x0 == x0 && b.x1 == x1 && b.color.r == color.r && b.color.g == color.g && b.color.b == color.b && b.color.a == color.a) return;
            b.x0 = x0;
            b.x1 = x1;
            b.color = color;
            markDirty();
            return;
        }
    d.bands.push_back({ id, x0, x1, color });
    markDirty();
}

void PlotChart::removeBand(int id)
{
    Impl& d = *impl_;
    for (size_t i = 0; i < d.bands.size(); ++i)
        if (d.bands[i].id == id)
        {
            d.bands.erase(d.bands.begin() + static_cast<ptrdiff_t>(i));
            markDirty();
            return;
        }
}

void PlotChart::clearBands()
{
    if (impl_->bands.empty()) return;
    impl_->bands.clear();
    markDirty();
}

void PlotChart::setRange(double a, double b)
{
    Impl& d = *impl_;
    if (!(a == a) || !(b == b)) return;
    if (b < a)
    {
        const double t = a;
        a = b;
        b = t;
    }
    if (d.rangeOn && d.rangeA == a && d.rangeB == b) return;
    d.rangeOn = true;
    d.rangeA = a;
    d.rangeB = b;
    markDirty();
}

void PlotChart::clearRange()
{
    if (!impl_->rangeOn) return;
    impl_->rangeOn = false;
    markDirty();
}

bool PlotChart::hasRange() const
{
    return impl_->rangeOn;
}

double PlotChart::rangeStart() const
{
    return impl_->rangeA;
}

double PlotChart::rangeEnd() const
{
    return impl_->rangeB;
}

void PlotChart::setRangeTool(bool on)
{
    impl_->rangeTool = on;
}

void PlotChart::setCursorEnabled(bool on)
{
    if (impl_->cursorEnabled == on) return;
    impl_->cursorEnabled = on;
    markDirty();
}

bool PlotChart::hasHover() const
{
    return impl_->hover;
}

double PlotChart::hoverX() const
{
    return impl_->hoverXData;
}

int PlotChart::hoverSeries() const
{
    return impl_->hoverSeries;
}

int PlotChart::hoverPoint() const
{
    return impl_->hoverPoint;
}

void PlotChart::setLegendVisible(bool on)
{
    if (impl_->legendVisible == on) return;
    impl_->legendVisible = on;
    impl_->touchData();
}

void PlotChart::setLegendAutoToggle(bool on)
{
    impl_->legendAutoToggle = on;
}

void PlotChart::setLegendRemovable(bool on)
{
    if (impl_->legendRemovable == on) return;
    impl_->legendRemovable = on;
    impl_->touchData();
}

void PlotChart::setReadout(const String& header, const ct::Vector<String>& values)
{
    Impl& d = *impl_;
    d.readoutHeader = header;
    d.readoutValues = values;
    d.readoutSet = true;
    markDirty();
}

void PlotChart::clearReadout()
{
    if (!impl_->readoutSet) return;
    impl_->readoutSet = false;
    impl_->readoutHeader.clear();
    impl_->readoutValues.clear();
    markDirty();
}

void PlotChart::setNote(const String& text)
{
    if (impl_->note == text) return;
    impl_->note = text;
    markDirty();
}

void PlotChart::setStats(const ct::Vector<String>& rowLabels, const ct::Vector<PlotStatsColumn>& columns)
{
    Impl& d = *impl_;
    d.statsRows = rowLabels;
    d.statsColumns = columns;
    ++d.statsVersion;
    markDirty();
}

void PlotChart::clearStats()
{
    Impl& d = *impl_;
    if (d.statsRows.empty() && d.statsColumns.empty()) return;
    d.statsRows.clear();
    d.statsColumns.clear();
    ++d.statsVersion;
    markDirty();
}

void PlotChart::setInteractive(bool on)
{
    impl_->interactive = on;
}

bool PlotChart::interactive() const
{
    return impl_->interactive;
}

void PlotChart::setBindings(const PlotBindings& bindings)
{
    impl_->bindings = bindings;
}

const PlotBindings& PlotChart::bindings() const
{
    return impl_->bindings;
}

// ── measures ──

Rect PlotChart::plotRect() const
{
    return impl_->plot;
}

double PlotChart::xToPixel(double x) const
{
    return impl_->plot.x + impl_->xToPx(x);
}

double PlotChart::pixelToX(double px) const
{
    return impl_->pxToX(px - impl_->plot.x);
}

double PlotChart::yToPixel(int axis, double y) const
{
    return impl_->plot.y + impl_->yToPx(axis < 0 || axis > 1 ? 0 : axis, y);
}

double PlotChart::pixelToY(int axis, double py) const
{
    return impl_->pxToY(axis < 0 || axis > 1 ? 0 : axis, py - impl_->plot.y);
}

uint32_t PlotChart::layerRebuilds() const
{
    return impl_->rebuilds;
}

int PlotChart::layerPointCount(int seriesIndex) const
{
    return seriesIndex >= 0 && seriesIndex < seriesCount() ? impl_->series[static_cast<size_t>(seriesIndex)]->drawnPoints : 0;
}

bool PlotChart::layerColumn(int seriesIndex, int column, float& x, float& lo, float& hi) const
{
    if (seriesIndex < 0 || seriesIndex >= seriesCount()) return false;
    const Series& s = *impl_->series[static_cast<size_t>(seriesIndex)];
    if (!s.envelope || column < 0 || static_cast<size_t>(column) * 2 + 1 >= static_cast<size_t>(s.drawnPoints)) return false;
    const size_t at = s.meshFirst + static_cast<size_t>(column) * 2;
    if (at + 1 >= impl_->mesh.vertices.size()) return false;
    x = impl_->mesh.vertices[at].x;
    hi = impl_->mesh.vertices[at].y;     // the top of the band: the smaller y
    lo = impl_->mesh.vertices[at + 1].y; // the bottom
    return true;
}

bool PlotChart::layerIsEnvelope(int seriesIndex) const
{
    return seriesIndex >= 0 && seriesIndex < seriesCount() && impl_->series[static_cast<size_t>(seriesIndex)]->envelope;
}

int PlotChart::xTickCount() const
{
    return static_cast<int>(impl_->xLabels.size());
}

double PlotChart::xTick(int i) const
{
    return impl_->xLabels[static_cast<size_t>(i)].value;
}

String PlotChart::xTickLabel(int i) const
{
    return impl_->xLabels[static_cast<size_t>(i)].text;
}

int PlotChart::yTickCount(int axis) const
{
    return static_cast<int>(impl_->yLabels[axis < 0 || axis > 1 ? 0 : axis].size());
}

double PlotChart::yTick(int axis, int i) const
{
    return impl_->yLabels[axis < 0 || axis > 1 ? 0 : axis][static_cast<size_t>(i)].value;
}

bool PlotChart::layerByReference() const
{
    return GetIO().retainedMeshes;
}

Widget::Vec2f PlotChart::sizeHint() const
{
    return { 400.0f, 240.0f };
}

// ═════════════════════════════════════════════════════════════════════════════
//  Hover
// ═════════════════════════════════════════════════════════════════════════════

// the nearest sample of each series to the cursor, for the dots and the values of the legend; the nearest of all for the signal
void PlotChart::Impl::pick(float px, float py)
{
    hoverSeries = -1;
    hoverPoint = -1;
    dots.clear();
    const double cx = pxToX(px - plot.x);
    float best = 1.0e30f;
    for (size_t k = 0; k < series.size(); ++k)
    {
        const Series& s = *series[k];
        Dot dot;
        dot.series = static_cast<int>(k);
        dot.text[0] = 0;
        bool found = false;
        size_t bestIndex = 0;
        float bpx = 0.0f, bpy = 0.0f;
        if (!s.info.visible || s.n == 0) continue;
        const size_t n = usable(s);
        if (isXY(s))
        {
            // x is not time: the nearest point of the curve in the plane
            float bestDist = 1.0e30f;
            const size_t stride = n / 4000 + 1;
            for (size_t i = 0; i < n; i += stride)
            {
                const double xv = sampleX(s, i), yv = s.y[i];
                if (xv != xv || yv != yv) continue;
                const float qx = static_cast<float>(plot.x + xToPx(xv)), qy = static_cast<float>(plot.y + yToPx(s.info.yAxis, yv));
                const float dd = (qx - px) * (qx - px) + (qy - py) * (qy - py);
                if (dd < bestDist)
                {
                    bestDist = dd;
                    bestIndex = i;
                    bpx = qx;
                    bpy = qy;
                    found = true;
                }
            }
        }
        else if (s.x)
        {
            // the sample nearest in x that is a number (a gap is skipped), looked for outward from where x falls
            const size_t at = plot::lowerBound(s.x, n, cx);
            double bestDx = 1.0e300;
            for (size_t step = 0; step < 100000; ++step)
            {
                bool inRange = false;
                for (int side = 0; side < 2; ++side)
                {
                    const size_t i = side == 0 ? at + step : (at > step ? at - step - 1 : n);
                    if (i >= n) continue;
                    inRange = true;
                    if (s.y[i] != s.y[i] || s.x[i] != s.x[i]) continue;
                    const double dx = fabs(s.x[i] - cx);
                    if (dx < bestDx)
                    {
                        bestDx = dx;
                        bestIndex = i;
                        found = true;
                    }
                }
                // a number was found on one side and the other side cannot be nearer (the samples go ascending): done after one more step
                if (!inRange || (found && step >= 1)) break;
            }
            if (found)
            {
                bpx = static_cast<float>(plot.x + xToPx(s.x[bestIndex]));
                bpy = static_cast<float>(plot.y + yToPx(s.info.yAxis, s.y[bestIndex]));
            }
        }
        if (!found) continue;
        dot.px = bpx;
        dot.py = bpy;
        dot.color = colourOf(s);
        snprintf(dot.text, sizeof dot.text, "%.6g", s.y[bestIndex]);
        dots.push_back(dot);
        const float dd = (bpx - px) * (bpx - px) + (bpy - py) * (bpy - py);
        if (dd < best)
        {
            best = dd;
            hoverSeries = static_cast<int>(k);
            hoverPoint = static_cast<int>(bestIndex);
        }
    }
    hoverVersion = dataVersion;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Input
// ═════════════════════════════════════════════════════════════════════════════

void PlotChart::onMousePress(MouseEvent& e)
{
    Impl& d = *impl_;
    if (!d.interactive || !visible_) return;
    WidgetApp& app = WidgetApp::instance();
    d.pressedInPlot = d.inPlot(e.x, e.y);
    // the legend: a click on an entry toggles it, on its cross removes it
    for (size_t i = 0; i < d.legend.size(); ++i)
    {
        const Impl::LegendEntry& le = d.legend[i];
        if (d.legendRemovable && le.cross.w > 0.0f && d.absolute(le.cross).contains(e.x, e.y) && e.button == 0)
        {
            e.consumed = true;
            const int index = le.series;
            if (app.emitGone(this, legendEntryRemoveClicked, index)) return;
            return;
        }
        if (d.absolute(le.box).contains(e.x, e.y) && e.button == 0)
        {
            e.consumed = true;
            const int index = le.series;
            if (d.legendAutoToggle && index >= 0 && index < static_cast<int>(d.series.size()))
            {
                Series& toggled = *d.series[static_cast<size_t>(index)];
                toggled.info.visible = !toggled.info.visible;
                d.touchData();
            }
            if (app.emitGone(this, legendEntryClicked, index)) return;
            return;
        }
    }
    if (!d.pressedInPlot) return;
    if (!acceptsFocus_ || !focused_) app.setFocused(this);
    if (e.clickCount >= 2 && e.button == 0 && d.bindings.doubleClickFit)
    {
        e.consumed = true;
        d.drag = Impl::Drag::None;
        const PlotYMode before = d.yMode[0];
        fit();
        if (before != PlotYMode::Auto) emitYMode(0, PlotYMode::Auto);
        return;
    }
    d.pressX = e.x;
    d.pressY = e.y;
    d.lastX = e.x;
    d.dragButton = e.button;
    d.dragMods = (e.shift ? PlotModShift : 0) | (e.ctrl ? PlotModCtrl : 0) | (e.alt ? PlotModAlt : 0);
    d.dragView0 = d.x0;
    d.dragView1 = d.x1;
    d.dragAnchorX = d.pxToX(e.x - d.plot.x);
    const bool rangeWanted = d.matches(d.bindings.range, e.button, e.ctrl, e.shift, e.alt, false) || (d.rangeTool && e.button == 0 && !e.shift && !e.ctrl && !e.alt);
    d.dragIsRange = rangeWanted;
    if (rangeWanted || d.matches(d.bindings.pan, e.button, e.ctrl, e.shift, e.alt, false) || d.matches(d.bindings.panAlt, e.button, e.ctrl, e.shift, e.alt, false))
    {
        d.drag = Impl::Drag::Armed;
        e.consumed = true;
    }
    else
        d.drag = Impl::Drag::None;
}

void PlotChart::onMouseMove(MouseEvent& e)
{
    Impl& d = *impl_;
    WidgetApp& app = WidgetApp::instance();
    const bool inside = d.inPlot(e.x, e.y);
    // a gesture
    if (d.drag != Impl::Drag::None)
    {
        const float moved = fabsf(e.x - d.pressX) + fabsf(e.y - d.pressY);
        if (d.drag == Impl::Drag::Armed && moved >= d.bindings.clickSlop)
        {
            d.drag = d.dragIsRange ? Impl::Drag::Range : Impl::Drag::Pan;
            if (d.drag == Impl::Drag::Range)
            {
                d.rangeOn = true;
                d.rangeA = d.dragAnchorX;
            }
        }
        if (d.drag == Impl::Drag::Pan)
        {
            const double perPixel = (d.dragView1 - d.dragView0) / static_cast<double>(d.plot.w);
            const double shift = -(static_cast<double>(e.x) - d.pressX) * perPixel;
            e.consumed = true;
            if (d.setX(d.dragView0 + shift, d.dragView1 + shift, true))
            {
                d.followOn = false;
                if (d.emitNow(true)) return;
            }
        }
        else if (d.drag == Impl::Drag::Range)
        {
            e.consumed = true;
            const double b = d.pxToX(clampf(e.x, d.plot.x, d.plot.x + d.plot.w) - d.plot.x);
            d.rangeOn = true;
            d.rangeB = b;
            markDirty();
            const double lo = d.rangeA < b ? d.rangeA : b, hi = d.rangeA < b ? b : d.rangeA;
            if (app.emitGone(this, rangeChanged, lo, hi, false)) return;
        }
    }
    // the pointer over the plot
    if (inside)
    {
        const bool was = d.hover;
        const float oldX = d.hoverPx, oldY = d.hoverPy;
        d.hover = true;
        d.hoverPx = e.x;
        d.hoverPy = e.y;
        d.hoverInside = true;
        d.pick(e.x, e.y);
        d.hoverXData = d.pxToX(e.x - d.plot.x);
        d.hoverYData = d.pxToY(0, e.y - d.plot.y);
        if (!was || oldX != e.x || oldY != e.y) markDirty();
        app.emitGone(this, hovered, d.hoverXData, d.hoverYData, d.hoverSeries, d.hoverPoint, true);
    }
    else if (d.hover && d.drag == Impl::Drag::None)
        onMouseLeave();
}

void PlotChart::onMouseRelease(MouseEvent& e)
{
    Impl& d = *impl_;
    WidgetApp& app = WidgetApp::instance();
    const Impl::Drag drag = d.drag;
    d.drag = Impl::Drag::None;
    if (drag == Impl::Drag::None && !d.pressedInPlot)
    {
        Widget::onMouseRelease(e);
        return;
    }
    const float moved = fabsf(e.x - d.pressX) + fabsf(e.y - d.pressY);
    if (drag == Impl::Drag::Range)
    {
        e.consumed = true;
        const double lo = d.rangeA < d.rangeB ? d.rangeA : d.rangeB, hi = d.rangeA < d.rangeB ? d.rangeB : d.rangeA;
        d.rangeA = lo;
        d.rangeB = hi;
        markDirty();
        if (app.emitGone(this, rangeChanged, lo, hi, true)) return;
        rangeSelected.emit(lo, hi);
        return;
    }
    if (drag == Impl::Drag::Pan)
    {
        e.consumed = true;
        return;
    }
    // a press and a release that moved less than the slop: a click, reported and not consumed
    if (d.pressedInPlot && moved < d.bindings.clickSlop)
    {
        const double x = d.pxToX(e.x - d.plot.x), y = d.pxToY(0, e.y - d.plot.y);
        const int mods = (e.shift ? PlotModShift : 0) | (e.ctrl ? PlotModCtrl : 0) | (e.alt ? PlotModAlt : 0);
        if (d.bindings.clickClearsRange && d.rangeOn && e.button == 0)
        {
            d.rangeOn = false;
            markDirty();
            if (app.emitGone(this, rangeCleared)) return;
        }
        d.pressedInPlot = false;
        app.emitGone(this, plotClicked, x, y, e.button, mods);
    }
    d.pressedInPlot = false;
}

void PlotChart::onMouseLeave()
{
    Impl& d = *impl_;
    if (!d.hover) return;
    d.hover = false;
    d.hoverSeries = -1;
    d.hoverPoint = -1;
    d.dots.clear();
    markDirty();
    hoverEnded.emit();
}

void PlotChart::onMouseScroll(MouseEvent& e)
{
    Impl& d = *impl_;
    if (!d.interactive || !d.inPlot(e.x, e.y)) return;
    const float notches = e.scrollY;
    if (notches == 0.0f) return;
    if (d.matches(d.bindings.zoomY, 0, e.ctrl, e.shift, e.alt, true))
    {
        e.consumed = true;
        // y zooms about the pointer, on the axis the pointer's side... the left one (the right follows when it is in use)
        const double factor = pow(static_cast<double>(d.bindings.zoomFactor), -static_cast<double>(notches));
        const bool wasManual = d.yMode[0] == PlotYMode::Manual;
        for (int axis = 0; axis < (d.usesRight ? 2 : 1); ++axis)
        {
            const double about = d.pxToY(axis, e.y - d.plot.y);
            const double lo = about + (d.yLo[axis] - about) * factor, hi = about + (d.yHi[axis] - about) * factor;
            d.yMode[axis] = PlotYMode::Manual;
            d.manualLo[axis] = lo;
            d.manualHi[axis] = hi;
            d.yLo[axis] = lo;
            d.yHi[axis] = hi;
        }
        ++d.dataVersion;
        markDirty();
        WidgetApp& app = WidgetApp::instance();
        if (!wasManual && app.emitGone(this, yModeChanged, 0, PlotYMode::Manual)) return;
        d.emitNow(true);
        return;
    }
    if (d.matches(d.bindings.panWheel, 0, e.ctrl, e.shift, e.alt, true))
    {
        e.consumed = true;
        const double dx = -static_cast<double>(notches) * static_cast<double>(d.bindings.wheelPanFraction) * (d.x1 - d.x0);
        if (d.setX(d.x0 + dx, d.x1 + dx, true))
        {
            d.followOn = false;
            d.emitNow(true);
        }
        return;
    }
    if (d.matches(d.bindings.zoomX, 0, e.ctrl, e.shift, e.alt, true))
    {
        e.consumed = true;
        const double factor = pow(static_cast<double>(d.bindings.zoomFactor), -static_cast<double>(notches));
        const double about = d.pxToX(e.x - d.plot.x);
        if (d.setX(about - (about - d.x0) * factor, about + (d.x1 - about) * factor, true))
        {
            d.followOn = false;
            d.emitNow(true);
        }
    }
}

void PlotChart::onKeyPress(KeyEvent& e)
{
    Impl& d = *impl_;
    if (!focused_ || !d.interactive) return;
    if (e.key == Key::Escape && d.bindings.escapeClearsRange && d.rangeOn)
    {
        e.consumed = true;
        d.rangeOn = false;
        markDirty();
        rangeCleared.emit();
    }
}

bool PlotChart::emitYMode(int axis, PlotYMode mode)
{
    return WidgetApp::instance().emitGone(this, yModeChanged, axis, mode);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Painting
// ═════════════════════════════════════════════════════════════════════════════

void PlotChart::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Impl& d = *impl_;
    const Rect a = absoluteRect();
    if (ctx.isClipped(a)) return;
    const Theme& t = Theme::instance();
    const PlotColors& pc = t.plot;

    ctx.font.SetFontSize(t.fontSize);
    d.scale = GetIO().framebufferScaleX > 0.0f ? GetIO().framebufferScaleX : 1.0f;
    d.fontId = ctx.fontStamp().id;
    d.meshTexture = ctx.drawList.solidTexture();

    // the pyramids of the big series, a few frames at a time; a frame is asked for while some are not done
    if (d.stepPyramids()) WidgetApp::instance().requestFrame();

    if (!d.viewTouched)
    {
        // nobody set the view: the whole data
        double lo, hi;
        d.extentOrData(lo, hi);
        d.x0 = lo;
        d.x1 = hi;
        d.viewTouched = true;
    }
#ifndef NDEBUG
    for (size_t i = 0; i < d.series.size(); ++i) d.checkContract(*d.series[i]);
#endif
    // follow the clock
    if (d.followOn && d.clockValid && (d.clockT < d.x0 || d.clockT > d.x1))
    {
        const double w = d.x1 - d.x0;
        d.x0 = d.clockT - w * 0.75;
        d.x1 = d.clockT + w * 0.25;
    }

    // the layer: one build for any change of what it is made of; nothing for a pointer, a clock, a marker
    const uint64_t key = d.makeKey(a, ctx);
    if (!d.layerBuilt || key != d.key || d.abs.w != a.w || d.abs.h != a.h)
    {
        d.rebuild(ctx, a);
        d.key = key;
    }
    d.abs = a;
    d.plot.x = a.x + d.leftW;
    d.plot.y = a.y + d.headerH;
    if (d.hover && d.hoverVersion != d.dataVersion) d.pick(d.hoverPx, d.hoverPy);

    const float fh = ctx.fontHeight();
    const float asc = ctx.font.GetAscender();
    // the widget's own background
    ctx.drawList.addRect(a, t.panelColor);

    // the plot: the layer (background, grid, lines) as one piece, then what moves over it
    const Rect plot = d.plot;
    ctx.pushClip(plot);
    ctx.drawList.addMesh(d.mesh, plot.x, plot.y);
    DrawList& dl = ctx.drawList;
    for (size_t i = 0; i < d.bands.size(); ++i)
    {
        const Band& b = d.bands[i];
        const float bx0 = clampf(static_cast<float>(plot.x + d.xToPx(b.x0)), plot.x, plot.x + plot.w), bx1 = clampf(static_cast<float>(plot.x + d.xToPx(b.x1)), plot.x, plot.x + plot.w);
        if (bx1 > bx0) dl.addRect({ bx0, plot.y, bx1 - bx0, plot.h }, b.color.a > 0 ? b.color : pc.band);
    }
    if (d.rangeOn)
    {
        const float bx0 = clampf(static_cast<float>(plot.x + d.xToPx(d.rangeA)), plot.x, plot.x + plot.w), bx1 = clampf(static_cast<float>(plot.x + d.xToPx(d.rangeB)), plot.x, plot.x + plot.w);
        if (bx1 > bx0)
        {
            dl.addRect({ bx0, plot.y, bx1 - bx0, plot.h }, pc.band);
            dl.addRect({ bx0, plot.y, 1.0f, plot.h }, pc.bandEdge);
            dl.addRect({ bx1 - 1.0f, plot.y, 1.0f, plot.h }, pc.bandEdge);
        }
    }
    for (size_t i = 0; i < d.markers.size(); ++i)
    {
        const Marker& m = d.markers[i];
        const float mx = static_cast<float>(plot.x + d.xToPx(m.x));
        if (mx < plot.x - 1.0f || mx > plot.x + plot.w + 1.0f) continue;
        dl.addRect({ mx - 0.5f, plot.y, 1.5f, plot.h }, m.color);
        if (!m.text.empty())
        {
            const float tw = ctx.font.GetTextWidth(m.text.c_str());
            const float fx = mx + 1.0f + tw + 8.0f > plot.x + plot.w ? mx - 1.0f - tw - 8.0f : mx + 1.0f;
            dl.addRect({ fx, plot.y, tw + 8.0f, fh + 4.0f }, m.color);
            ctx.font.SetColor(onColour(m.color));
            ctx.font.Print(m.text.c_str(), fx + 4.0f, plot.y + 2.0f + asc);
        }
    }
    if (d.clockValid)
    {
        const float cx = static_cast<float>(plot.x + d.xToPx(d.clockT));
        if (cx >= plot.x - 1.0f && cx <= plot.x + plot.w + 1.0f) dl.addRect({ cx - 0.5f, plot.y, 1.5f, plot.h }, pc.clock);
    }
    if (d.hover && d.cursorEnabled && d.hoverInside)
    {
        dl.addRect({ d.hoverPx - 0.5f, plot.y, 1.0f, plot.h }, pc.cursor);
        for (size_t i = 0; i < d.dots.size(); ++i)
        {
            const Impl::Dot& dot = d.dots[i];
            dl.addRect({ dot.px - 3.5f, dot.py - 3.5f, 7.0f, 7.0f }, pc.bg);
            dl.addRect({ dot.px - 2.5f, dot.py - 2.5f, 5.0f, 5.0f }, dot.color);
        }
    }
    ctx.popClip();
    // the border of the plot
    dl.addRect({ plot.x - 1.0f, plot.y - 1.0f, plot.w + 2.0f, 1.0f }, pc.axis);
    dl.addRect({ plot.x - 1.0f, plot.y + plot.h, plot.w + 2.0f, 1.0f }, pc.axis);
    dl.addRect({ plot.x - 1.0f, plot.y, 1.0f, plot.h }, pc.axis);
    dl.addRect({ plot.x + plot.w, plot.y, 1.0f, plot.h }, pc.axis);

    // axis labels
    ctx.font.SetColor(t.textSecondary);
    for (size_t i = 0; i < d.xLabels.size(); ++i)
    {
        const Impl::Label& l = d.xLabels[i];
        const float lx = plot.x + l.pos - l.w * 0.5f;
        if (lx < plot.x - l.w * 0.5f || lx + l.w > a.x + a.w + 2.0f) continue;
        dl.addRect({ plot.x + l.pos - 0.5f, plot.y + plot.h, 1.0f, 3.0f }, pc.axis);
        ctx.font.Print(l.text.c_str(), lx, plot.y + plot.h + 4.0f + asc);
    }
    for (int axis = 0; axis < 2; ++axis)
    {
        if (axis == 1 && !d.usesRight) continue;
        for (size_t i = 0; i < d.yLabels[axis].size(); ++i)
        {
            const Impl::Label& l = d.yLabels[axis][i];
            const float ly = plot.y + l.pos;
            if (ly < plot.y - 1.0f || ly > plot.y + plot.h + 1.0f) continue;
            const float lx = axis == 0 ? plot.x - 5.0f - l.w : plot.x + plot.w + 5.0f;
            ctx.font.Print(l.text.c_str(), lx, ly - fh * 0.5f + asc);
        }
    }

    // the header: the time of the cursor (or the application's text), the note
    const float headerY = a.y + 2.0f + asc;
    ctx.font.SetColor(t.textColor);
    char buffer[64];
    if (d.readoutSet && !d.readoutHeader.empty()) ctx.font.Print(d.readoutHeader.c_str(), a.x + 6.0f, headerY);
    else if (d.hover && d.hoverInside)
    {
        const double step = (d.x1 - d.x0) / 200.0;
        if (d.xFormat)
        {
            const String text = d.xFormat(d.hoverXData, step); // (a formatter of the application allocates: the defaults do not)
            snprintf(buffer, sizeof buffer, "%s", text.c_str());
        }
        else if (d.timeAxis)
            plot::formatTimeInto(buffer, sizeof buffer, d.hoverXData, step);
        else
            plot::formatValueInto(buffer, sizeof buffer, d.hoverXData, step);
        ctx.font.Print(buffer, a.x + 6.0f, headerY);
    }
    if (!d.note.empty())
    {
        ctx.font.SetColor(t.textSecondary);
        ctx.font.Print(d.note.c_str(), a.x + a.w - ctx.font.GetTextWidth(d.note.c_str()) - 6.0f, headerY);
    }
    // the legend: a swatch, the name, the value of the nearest sample under the cursor, a cross
    if (d.legendVisible)
    {
        const float swatch = fh * 0.7f;
        for (size_t i = 0; i < d.legend.size(); ++i)
        {
            Impl::LegendEntry le = d.legend[i];
            le.box = d.absolute(le.box);
            le.cross = d.absolute(le.cross);
            le.valueX += d.abs.x;
            const Series& s = *d.series[static_cast<size_t>(le.series)];
            const Color colour = d.colourOf(s);
            const bool on = s.info.visible;
            dl.addRect({ le.box.x + 1.0f, le.box.y + (le.box.h - swatch) * 0.5f, swatch, swatch }, on ? colour : withAlpha(colour, 70));
            ctx.font.SetColor(on ? t.textColor : t.textSecondary);
            const float ty = le.box.y + (le.box.h - fh) * 0.5f + asc;
            ctx.font.Print(s.info.name.c_str(), le.box.x + swatch + 5.0f, ty);
            ctx.font.SetColor(t.textSecondary);
            if (static_cast<size_t>(le.series) < d.readoutValues.size() && d.readoutSet && !d.readoutValues[static_cast<size_t>(le.series)].empty())
                ctx.font.Print(d.readoutValues[static_cast<size_t>(le.series)].c_str(), le.valueX, ty);
            else if (d.hover && d.hoverInside && on)
                for (size_t k = 0; k < d.dots.size(); ++k)
                    if (d.dots[k].series == le.series)
                    {
                        ctx.font.Print(d.dots[k].text, le.valueX, ty);
                        break;
                    }
            if (d.legendRemovable && le.cross.w > 0.0f)
            {
                const Rect& c = le.cross;
                dl.addLine({ c.x + 3.0f, c.y + 3.0f }, { c.x + c.w - 3.0f, c.y + c.h - 3.0f }, t.textSecondary, 1.0f);
                dl.addLine({ c.x + 3.0f, c.y + c.h - 3.0f }, { c.x + c.w - 3.0f, c.y + 3.0f }, t.textSecondary, 1.0f);
            }
        }
    }
    // the table of statistics under the axis
    if (d.statsH > 0.0f)
    {
        float y = plot.y + plot.h + d.axisH + 4.0f;
        float labelW = 0.0f;
        for (size_t r = 0; r < d.statsRows.size(); ++r)
        {
            const float w = ctx.font.GetTextWidth(d.statsRows[r].c_str());
            if (w > labelW) labelW = w;
        }
        float x = a.x + 6.0f;
        const float colTop = y;
        float colX = x + labelW + 14.0f;
        // rows
        ctx.font.SetColor(t.textSecondary);
        for (size_t r = 0; r < d.statsRows.size(); ++r) ctx.font.Print(d.statsRows[r].c_str(), x, colTop + (r + 1) * d.rowH + (d.rowH - fh) * 0.5f + asc);
        for (size_t c = 0; c < d.statsColumns.size(); ++c)
        {
            const PlotStatsColumn& col = d.statsColumns[c];
            float w = ctx.font.GetTextWidth(col.header.c_str()) + (col.seriesId >= 0 ? fh : 0.0f);
            for (size_t r = 0; r < col.cells.size(); ++r)
            {
                const float cw = ctx.font.GetTextWidth(col.cells[r].c_str());
                if (cw > w) w = cw;
            }
            float tx = colX;
            if (col.seriesId >= 0)
            {
                const Series* s = d.find(col.seriesId);
                if (s) dl.addRect({ colX, colTop + (d.rowH - fh * 0.7f) * 0.5f, fh * 0.7f, fh * 0.7f }, d.colourOf(*s));
                tx += fh;
            }
            ctx.font.SetColor(t.textColor);
            ctx.font.Print(col.header.c_str(), tx, colTop + (d.rowH - fh) * 0.5f + asc);
            ctx.font.SetColor(t.textSecondary);
            for (size_t r = 0; r < col.cells.size(); ++r) ctx.font.Print(col.cells[r].c_str(), colX, colTop + (r + 1) * d.rowH + (d.rowH - fh) * 0.5f + asc);
            colX += w + 16.0f;
        }
        dl.addRect({ a.x + 4.0f, colTop + d.rowH - 1.0f, a.w - 8.0f, 1.0f }, pc.grid);
        (void) y;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Axis links
// ═════════════════════════════════════════════════════════════════════════════

PlotAxisLink::PlotAxisLink() {}

PlotAxisLink::~PlotAxisLink()
{
    for (size_t i = 0; i < entries_.size(); ++i)
    {
        entries_[i].chart->viewChanged.disconnect(entries_[i].viewSlot);
        entries_[i].chart->destroyed.disconnect(entries_[i].goneSlot);
    }
}

void PlotAxisLink::add(PlotChart* chart)
{
    if (!chart) return;
    for (size_t i = 0; i < entries_.size(); ++i)
        if (entries_[i].chart == chart) return;
    Entry e;
    e.chart = chart;
    e.viewSlot = chart->viewChanged.connect(
            [this, chart](double x0, double x1, double, double, bool byUser)
            {
                if (!byUser || applying_) return;
                applying_ = true;
                // a copy of the list: a handler of the others' viewChanged may take a chart out
                ct::Vector<PlotChart*> others;
                for (size_t k = 0; k < entries_.size(); ++k)
                    if (entries_[k].chart != chart) others.push_back(entries_[k].chart);
                for (size_t k = 0; k < others.size(); ++k)
                {
                    bool still = false;
                    for (size_t m = 0; m < entries_.size(); ++m) still = still || entries_[m].chart == others[k];
                    if (still) others[k]->setViewX(x0, x1);
                }
                applying_ = false;
            });
    e.goneSlot = chart->destroyed.connect([this, chart]() { drop(chart, false); });
    entries_.push_back(e);
}

void PlotAxisLink::remove(PlotChart* chart)
{
    drop(chart, true);
}

void PlotAxisLink::drop(PlotChart* chart, bool alive)
{
    for (size_t i = 0; i < entries_.size(); ++i)
        if (entries_[i].chart == chart)
        {
            // the destroyed signal is emitting when a chart goes: the slot is marked and freed after (Signal)
            if (alive) chart->viewChanged.disconnect(entries_[i].viewSlot);
            chart->destroyed.disconnect(entries_[i].goneSlot);
            entries_.erase(entries_.begin() + static_cast<ptrdiff_t>(i));
            return;
        }
}

} // namespace retained
} // namespace ig
