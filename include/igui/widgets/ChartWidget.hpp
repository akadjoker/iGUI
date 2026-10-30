#pragma once
// Chart - pie, donut, bars, lines and scatter.
//
// The chart holds categories (the labels) and series (one row of values per
// category, and for a line or scatter chart the x of each value). A pie or
// donut draws the first visible series; bars draws every visible series side
// by side per category; lines and scatter draw each series over an x axis
// (numeric when the series has xs, the categories otherwise). Everything is painted with the
// retained DrawList primitives, with the theme's text colours, a default
// palette that stays readable for colour-blind viewers, and a legend that
// toggles an entry on press. The geometry of the last paint is kept so a host
// (a test, a designer, an assistant) can ask what is under a point.

#include "Widget.hpp"
#include "Retained.hpp"
#include "Signal.hpp"
#include "String.hpp"
#include <ct/vector.hpp>

namespace ig::retained
{

enum class ChartKind
{
    Pie,
    Donut,
    Bars,
    Line,     // series as polylines over x, markers when the points are few
    Scatter,  // series as markers only
    Histogram // bars touching, one per bin of the series' samples, the bin edges under the axis
};

enum class LegendPosition
{
    Auto,   // right when the widget is wide, below otherwise
    Right,
    Bottom,
    Hidden
};

struct ChartSeries
{
    String            name;
    ct::Vector<float> values;   // one per category; missing entries read as 0
    ct::Vector<float> xs;       // line/scatter: the x of each value; empty means the index
    ct::Vector<float> samples;  // histogram: the raw values its bins count
    Color             color;
    bool              visible = true;
    bool              line    = true;   // line/scatter: join the points
    bool              markers = true;   // line/scatter: a dot at each point (a line hides them past 60 points)
};

class Chart : public Widget
{
public:
    // What is under a point: a slice (series 0, category), a bar, or a point of a line.
    struct Item
    {
        int series = -1;
        int index  = -1;
        bool valid() const { return series >= 0 && index >= 0; }
        bool operator==(const Item& o) const { return series == o.series && index == o.index; }
        bool operator!=(const Item& o) const { return !(*this == o); }
    };

    Chart();

    // ── Data ──────────────────────────────────────────────────────────────
    /// @brief Set the chart kind.
    void      setKind(ChartKind kind);
    ChartKind kind() const { return kind_; }

    /// @brief Replace the category labels. Series keep their values.
    void setCategories(const ct::Vector<String>& labels);
    const ct::Vector<String>& categories() const { return categories_; }
    int  categoryCount() const { return static_cast<int>(categories_.size()); }

    /// @brief Add a series coloured from the palette; returns its index.
    int  addSeries(const String& name, const ct::Vector<float>& values);
    /// @brief Add a series with its own colour; returns its index.
    int  addSeries(const String& name, const ct::Vector<float>& values, const Color& color);
    /// @brief Add a series with its own xs (a line or scatter); returns its index.
    int  addSeries(const String& name, const ct::Vector<float>& xs, const ct::Vector<float>& ys);
    void setSeriesValues(int series, const ct::Vector<float>& values);
    /// @brief The x of each value of a series; empty means the index.
    void setSeriesX(int series, const ct::Vector<float>& xs);
    /// @brief Whether a line or scatter series joins its points and dots them.
    void setSeriesStyle(int series, bool line, bool markers);
    void setSeriesName(int series, const String& name);
    void setSeriesColor(int series, const Color& color);
    void setSeriesVisible(int series, bool visible);
    void removeSeries(int series);
    int  seriesCount() const { return static_cast<int>(series_.size()); }
    const ChartSeries& series(int idx) const { return series_[static_cast<size_t>(idx)]; }

    /// @brief Show or hide one category (a slice, a bar group).
    void setCategoryVisible(int index, bool visible);
    bool categoryVisible(int index) const;

    /// @brief Remove categories and series.
    void clear();

    /// @brief One call for a pie: labels and values, one series.
    void pie(const ct::Vector<String>& labels, const ct::Vector<float>& values);
    /// @brief One call for a donut: labels and values, one series.
    void donut(const ct::Vector<String>& labels, const ct::Vector<float>& values);
    /// @brief One call for bars: labels and one series of values.
    void bars(const ct::Vector<String>& labels, const ct::Vector<float>& values, const String& name = String());
    /// @brief A line of ys over xs, added to a line chart (the chart becomes one when it is not); returns the series.
    int  plot(const ct::Vector<float>& xs, const ct::Vector<float>& ys, const String& name = String());
    /// @brief Points of ys over xs, added to a scatter chart (the chart becomes one when it is neither line nor scatter); returns the series.
    int  scatter(const ct::Vector<float>& xs, const ct::Vector<float>& ys, const String& name = String());
    /// @brief A histogram of the samples in `bins` equal bins, added as a series (the chart becomes a histogram when it is not one; every series shares the bins); returns the series.
    int  hist(const ct::Vector<float>& samples, int bins = 10, const String& name = String());
    /// @brief Number of bins of a histogram; the bins are rebuilt.
    void setBins(int bins);
    int  bins() const { return bins_; }
    /// @brief The bin edges of the last histogram build (bins + 1 numbers).
    const ct::Vector<float>& binEdges() const { return edges_; }

    /// @brief Value of a series at a category; 0 when the series is shorter.
    float value(int series, int index) const;
    /// @brief x of a series at an index: its xs, or the index itself.
    float x(int series, int index) const;
    /// @brief Number of points of a series (its values).
    int   pointCount(int series) const;
    /// @brief Line and scatter charts draw over an x axis.
    bool  isXY() const { return kind_ == ChartKind::Line || kind_ == ChartKind::Scatter; }
    /// @brief Bars and histograms draw bars per category.
    bool  isBars() const { return kind_ == ChartKind::Bars || kind_ == ChartKind::Histogram; }
    /// @brief Sum of the visible categories of a series.
    float total(int series) const;

    // ── Looks ─────────────────────────────────────────────────────────────
    void setTitle(const String& title);
    const String& title() const { return title_; }
    void setShowLegend(bool show);
    bool showLegend() const { return legendPosition_ != LegendPosition::Hidden; }
    void setLegendPosition(LegendPosition position);
    LegendPosition legendPosition() const { return legendPosition_; }
    /// @brief Print the value on each slice or bar (a pie always prints the percent).
    void setShowValues(bool show);
    bool showValues() const { return showValues_; }
    /// @brief Print the percent on slices.
    void setShowPercent(bool show);
    bool showPercent() const { return showPercent_; }
    /// @brief Grid lines behind the bars, lines and points.
    void setShowGrid(bool show);
    bool showGrid() const { return showGrid_; }
    /// @brief Radius of the dots of a line or scatter chart.
    void  setMarkerSize(float radius);
    float markerSize() const { return markerSize_; }
    /// @brief Thickness of the lines of a line chart.
    void  setLineWidth(float width);
    float lineWidth() const { return lineWidth_; }
    /// @brief Text after each printed value, e.g. "%" or " kg".
    void setValueSuffix(const String& suffix);
    const String& valueSuffix() const { return valueSuffix_; }
    /// @brief Decimals of printed values; -1 picks them from the numbers.
    void setValueDecimals(int decimals);
    int  valueDecimals() const { return valueDecimals_; }
    /// @brief Inner radius of a donut as a fraction of the outer radius.
    void setDonutRatio(float ratio);
    float donutRatio() const { return donutRatio_; }
    /// @brief Colours taken by series (bars) or categories (pie) without one.
    void setPalette(const ct::Vector<Color>& palette);
    const ct::Vector<Color>& palette() const { return palette_; }
    static const ct::Vector<Color>& defaultPalette();
    /// @brief Colour of a category in a pie, of a series otherwise.
    Color colorOf(int series, int index) const;
    void  setBackgroundColor(const Color& color);
    void  setDrawBackground(bool draw);

    // ── Geometry of the last paint ────────────────────────────────────────
    /// @brief The slice or bar under a point, or the nearest point of a line within a few pixels (widget coordinates).
    Item hitTest(float localX, float localY) const;
    /// @brief The legend entry under a point, or -1.
    int  legendHitTest(float localX, float localY) const;
    Item hoveredItem() const { return hovered_; }
    /// @brief Area the pie or the bars were drawn in (widget coordinates).
    Rect plotRect() const { return plot_; }
    /// @brief Rectangle of one legend entry (widget coordinates).
    Rect legendEntryRect(int entry) const;
    int  legendEntryCount() const { return static_cast<int>(legend_.size()); }
    /// @brief Rectangle of one bar (widget coordinates); empty when hidden.
    Rect barRect(int series, int index) const;
    /// @brief Centre and outer radius of the last pie.
    Vec2f pieCenter() const { return { pieCx_, pieCy_ }; }
    float pieRadius() const { return pieR_; }
    /// @brief Where a point of a line or scatter series was drawn (widget coordinates); negative when it was not.
    Vec2f pointAt(int series, int index) const;
    /// @brief Value (y) range of the last bars, line or scatter axis.
    float axisMin() const { return axisMin_; }
    float axisMax() const { return axisMax_; }
    float axisStep() const { return axisStep_; }
    /// @brief x range of the last line or scatter axis (the indices when the series have no xs).
    float xAxisMin() const { return xAxisMin_; }
    float xAxisMax() const { return xAxisMax_; }
    float xAxisStep() const { return xAxisStep_; }

    /// @brief Format a value the way the chart prints it.
    void formatValue(float v, char* out, int outSize) const;

    // ── Signals ───────────────────────────────────────────────────────────
    Signal<int, int> itemPressed;    // (series, index) left press on a slice, a bar or a point
    Signal<Item>     hoverChanged;   // the item under the mouse changed
    Signal<int>      legendToggled;  // legend entry index whose visibility flipped

    // ── Widget ────────────────────────────────────────────────────────────
    Vec2f sizeHint() const override { return { 320.0f, 240.0f }; }
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseLeave() override;

private:
    struct LegendEntry
    {
        Rect   rect;     // widget coordinates
        String label;
        Color  color;
        bool   visible = true;
        int    target = 0;   // category (pie) or series (bars) index
    };
    struct Slice
    {
        int   index = -1;
        float start = 0.0f;  // radians, clockwise from the top
        float sweep = 0.0f;
        float value = 0.0f;
    };
    struct Bar
    {
        int  series = -1;
        int  index  = -1;
        Rect rect;           // widget coordinates
        float value = 0.0f;
    };
    struct Point
    {
        int   series = -1;
        int   index  = -1;
        float x = 0.0f, y = 0.0f;   // widget coordinates
    };

    ChartKind          kind_ = ChartKind::Pie;
    String             title_;
    ct::Vector<String> categories_;
    ct::Vector<bool>   categoryHidden_;
    ct::Vector<ChartSeries> series_;
    ct::Vector<Color>  palette_;
    LegendPosition     legendPosition_ = LegendPosition::Auto;
    bool               showValues_  = false;
    bool               showPercent_ = true;
    bool               showGrid_    = true;
    bool               drawBackground_ = true;
    bool               hasBackground_  = false;
    Color              background_;
    String             valueSuffix_;
    int                valueDecimals_ = -1;
    float              donutRatio_ = 0.55f;
    float              markerSize_ = 3.5f;
    float              lineWidth_  = 2.0f;
    int                bins_ = 10;
    ct::Vector<float>  edges_;

    // Geometry of the last paint, in widget coordinates.
    Rect   plot_ = {};
    Rect   legendArea_ = {};
    ct::Vector<LegendEntry> legend_;
    ct::Vector<Slice>       slices_;
    ct::Vector<Bar>         bars_;
    ct::Vector<Point>       points_;
    float  pieCx_ = 0.0f, pieCy_ = 0.0f, pieR_ = 0.0f;
    float  axisMin_ = 0.0f, axisMax_ = 1.0f, axisStep_ = 0.25f;
    float  xAxisMin_ = 0.0f, xAxisMax_ = 1.0f, xAxisStep_ = 0.25f;

    Item   hovered_;
    float  mouseX_ = -1.0f, mouseY_ = -1.0f;   // widget coordinates
    ct::Vector<Vec2> polygon_;   // scratch for the slice outlines, kept to avoid reallocating
    ct::Vector<Vec2> quad_;
    ct::Vector<Vec2> polyline_;

    int  firstVisibleSeries() const;
    bool legendOnRight(const Rect& b) const;
    void buildLegend(PaintContext& ctx, const Rect& b, const Rect& titleArea, Rect& plot);
    void buildSlices();
    void buildHistogram();
    void sliceLabel(const Slice& slice, float sum, char* out, int outSize) const;
    bool isPie() const { return kind_ == ChartKind::Pie || kind_ == ChartKind::Donut; }
    void buildAxis(float& lo, float& hi, float& step) const;
    static void niceAxis(float vmin, float vmax, float& lo, float& hi, float& step);
    float valueAxisGutter(PaintContext& ctx, float lo, float hi, float step, int decimals) const;
    void  paintValueAxis(PaintContext& ctx, float ax, float ay, float aw, float ah, float lo, float hi, float step, int decimals);
    void paintBackground(PaintContext& ctx, const Rect& b);
    float paintTitle(PaintContext& ctx, const Rect& b);
    void paintLegend(PaintContext& ctx, const Rect& b);
    void paintPie(PaintContext& ctx, const Rect& b, const Rect& plot);
    void paintBars(PaintContext& ctx, const Rect& b, const Rect& plot);
    void paintXY(PaintContext& ctx, const Rect& b, const Rect& plot);
    void paintTooltip(PaintContext& ctx, const Rect& b);
    void setHovered(Item item);
};

} // namespace ig::retained
