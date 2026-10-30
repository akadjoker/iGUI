#pragma once
// Chart - a categorical chart widget: pie, donut and bars.
//
// The chart holds categories (the labels) and series (one row of values per
// category). A pie or donut draws the first visible series; bars draws every
// visible series side by side per category. Everything is painted with the
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
    Bars
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
    Color             color;
    bool              visible = true;
};

class Chart : public Widget
{
public:
    // What is under a point: a slice (series 0, category) or a bar.
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
    void setSeriesValues(int series, const ct::Vector<float>& values);
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

    /// @brief Value of a series at a category; 0 when the series is shorter.
    float value(int series, int index) const;
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
    /// @brief Horizontal grid lines behind the bars.
    void setShowGrid(bool show);
    bool showGrid() const { return showGrid_; }
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
    /// @brief Colour of a category in a pie, of a series in bars.
    Color colorOf(int series, int index) const;
    void  setBackgroundColor(const Color& color);
    void  setDrawBackground(bool draw);

    // ── Geometry of the last paint ────────────────────────────────────────
    /// @brief The slice or bar under a point in widget coordinates.
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
    /// @brief Value range of the last bars axis.
    float axisMin() const { return axisMin_; }
    float axisMax() const { return axisMax_; }
    float axisStep() const { return axisStep_; }

    /// @brief Format a value the way the chart prints it.
    void formatValue(float v, char* out, int outSize) const;

    // ── Signals ───────────────────────────────────────────────────────────
    Signal<int, int> itemPressed;    // (series, index) left press on a slice or bar
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

    // Geometry of the last paint, in widget coordinates.
    Rect   plot_ = {};
    Rect   legendArea_ = {};
    ct::Vector<LegendEntry> legend_;
    ct::Vector<Slice>       slices_;
    ct::Vector<Bar>         bars_;
    float  pieCx_ = 0.0f, pieCy_ = 0.0f, pieR_ = 0.0f;
    float  axisMin_ = 0.0f, axisMax_ = 1.0f, axisStep_ = 0.25f;

    Item   hovered_;
    float  mouseX_ = -1.0f, mouseY_ = -1.0f;   // widget coordinates
    ct::Vector<Vec2> polygon_;   // scratch for the slice outlines, kept to avoid reallocating
    ct::Vector<Vec2> quad_;

    int  firstVisibleSeries() const;
    bool legendOnRight(const Rect& b) const;
    void buildLegend(PaintContext& ctx, const Rect& b, const Rect& titleArea, Rect& plot);
    void buildSlices();
    void sliceLabel(const Slice& slice, float sum, char* out, int outSize) const;
    void buildAxis(float& lo, float& hi, float& step) const;
    void paintBackground(PaintContext& ctx, const Rect& b);
    float paintTitle(PaintContext& ctx, const Rect& b);
    void paintLegend(PaintContext& ctx, const Rect& b);
    void paintPie(PaintContext& ctx, const Rect& b, const Rect& plot);
    void paintBars(PaintContext& ctx, const Rect& b, const Rect& plot);
    void paintTooltip(PaintContext& ctx, const Rect& b);
    void setHovered(Item item);
};

} // namespace ig::retained
