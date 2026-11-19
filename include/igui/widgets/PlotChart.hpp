#pragma once
// PlotChart - an interactive time-series chart (docs/ROUND10.md, docs/specs/chart.md).
//
// Series are pushed by the application with an x array of their own (seconds, double), gaps (NaN), a stable id and colour, a revision
// number (an unchanged revision is unchanged data and costs nothing), an optional "these are min/max pairs" flag (drawn as an envelope),
// a y axis (left or right) and, for an XY plot, the series whose values are the x. The chart zooms and pans (wheel, drag), selects a
// range, reports the point under the pointer and the clicks, draws a vertical cursor, markers, bands, a clock line, a legend and a table
// of statistics, and keeps what is expensive (the background, the grid and the lines) in a layer that is rebuilt only when the data, the
// view, the size, the pixel scale or the theme changes: what moves every frame (the pointer, the clock, a range being dragged) is an
// overlay that never rebuilds it. A series with more points than pixels is drawn from a min/max reduction (never loses an extreme), built
// from a pyramid of min/max blocks made once for each revision (incrementally, with a budget of samples a frame, for a big series).
//
// Everything it draws takes its colours from Theme (Theme::plot and the text colours of every preset).
#include "Retained.hpp"
#include "Signal.hpp"
#include "String.hpp"
#include "Widget.hpp"

namespace ig
{
namespace retained
{

// ── ticks and their labels (also for the application's own axes) ─────────────────────────────────────────────────────────
namespace plot
{
/// A step of the form 1, 2 or 5 times a power of ten for which a range has at most `maxTicks` ticks.
double niceStep(double range, int maxTicks);
/// The same for seconds: 1, 2, 5 times a power of ten below a minute (down to a nanosecond), then 1, 2, 5, 10, 15, 30 s, 1, 2, 5, 10, 15, 30 min,
/// 1, 2, 3, 6, 12 h, and whole days (1, 2, 5, 10, ...).
double niceTimeStep(double range, int maxTicks);
/// The multiples of `step` in [lo, hi], appended to `out` (cleared first).
void ticksIn(double lo, double hi, double step, ct::Vector<double>& out);
/// The default label of a tick of a time axis: seconds with as many decimals as the step needs (down to nanoseconds); "m:ss" and "h:mm:ss"
/// from a step of a minute; "h:mm" from an hour; "Nd hh:mm" from a day.
String formatTime(double seconds, double step);
/// The default label of a tick of a value axis: as many decimals as the step needs, "%g" for the very big and the very small.
String formatValue(double value, double step);
/// The same, written into a buffer (no allocation): the length written.
int formatTimeInto(char* out, int capacity, double seconds, double step);
int formatValueInto(char* out, int capacity, double value, double step);
} // namespace plot

enum class PlotYMode
{
    Auto,   // fitted to the data in the visible x range
    Full,   // fitted to all the data of the series on the axis
    Manual  // what the application or the person set
};

/// Modifier keys of a gesture, or-ed.
enum PlotMod
{
    PlotModNone = 0,
    PlotModShift = 1,
    PlotModCtrl = 2,
    PlotModAlt = 4
};

/// A gesture binding: for a drag the mouse button (0 left, 1 right, 2 middle) and the modifiers held; for the wheel the modifiers. The
/// modifiers must be exactly these (no modifier is "none held").
struct PlotBinding
{
    int button = 0;
    int mods = PlotModNone;
    bool enabled = true;
};

struct PlotBindings
{
    PlotBinding zoomX = { 0, PlotModNone, true };        // wheel: zoom x about the pointer
    PlotBinding zoomY = { 0, PlotModCtrl, true };        // wheel: zoom y about the pointer (y becomes Manual)
    PlotBinding panWheel = { 0, PlotModShift, true };    // wheel: pan x a tenth of the view a notch
    PlotBinding pan = { 0, PlotModNone, true };          // drag: pan x
    PlotBinding panAlt = { 2, PlotModNone, true };       // drag: pan x (middle button)
    PlotBinding range = { 0, PlotModShift, true };       // drag: select a range of x
    bool doubleClickFit = true;                          // a double click fits the whole data (y Auto)
    bool escapeClearsRange = true;                       // Escape, with the focus, clears the range
    bool clickClearsRange = true;                        // a click (press and release without moving) clears it
    float zoomFactor = 1.2f;                             // a wheel notch zooms by this
    float wheelPanFraction = 0.1f;                       // ... and pans by this fraction of the view
    float clickSlop = 4.0f;                              // pixels a press may move and still be a click
};

/// What the application says about a series. Created by the first call with its id.
struct PlotSeriesInfo
{
    int id = 0;
    String name;
    String unit;
    Color color = Color(0, 0, 0, 0); // alpha 0: a colour of the theme's palette, kept for the life of the series (the others coming and going do not move it)
    bool visible = true;
    int yAxis = 0;                   // 0 the left axis, 1 the right one
    bool minMax = false;             // the points are (min, max) pairs of bins: drawn as an envelope
    int xSource = -1;                // XY plot: the id of the series whose values are the x of this one (-1: the x array of this series)
    float lineWidth = 1.5f;
    double breakDx = 0.0;            // two samples farther apart than this are not joined (0: always joined; NaN in y always breaks)
};

struct PlotStatsColumn
{
    int seriesId = -1;          // the colour dot of the column (-1: none)
    String header;
    ct::Vector<String> cells;   // one per row label
};

/// An axis link: the x axes of the charts added to it follow each other (a pan or a zoom by the person on one is applied to the others
/// without being reported as made by the person, so nothing loops). A chart that is destroyed is taken out by itself.
class PlotAxisLink
{
public:
    PlotAxisLink();
    ~PlotAxisLink();
    PlotAxisLink(const PlotAxisLink&) = delete;
    PlotAxisLink& operator=(const PlotAxisLink&) = delete;
    void add(class PlotChart* chart);
    void remove(class PlotChart* chart);
    int count() const
    {
        return static_cast<int>(entries_.size());
    }

private:
    // alive false: the chart is in its destructor (the derived part, and with it its viewChanged signal, is gone already): only what is the widget's is touched
    void drop(class PlotChart* chart, bool alive);
    struct Entry
    {
        PlotChart* chart;
        uint32_t viewSlot;
        uint32_t goneSlot;
    };
    ct::Vector<Entry> entries_;
    bool applying_ = false;
};

class PlotChart : public Widget
{
public:
    IG_WIDGET_CLASS(PlotChart, Widget);

    PlotChart();
    ~PlotChart() override;

    // ── Series ────────────────────────────────────────────────────────────
    /// Creates the series of `info.id`, or changes its name, colour, visibility, axis, flags and width.
    void setSeries(const PlotSeriesInfo& info);
    /// The data of a series, NOT copied: the arrays belong to the caller and must stay valid and unchanged until the next setSeriesData
    /// of this id, setSeriesDataCopy, removeSeries, clearSeries or the destruction of the chart. `x` is ascending (seconds); a NaN in `y`
    /// is a gap. An unchanged `revision` (with the same pointers and count) is unchanged data: nothing is done. Data changed in place
    /// needs a new revision; a debug build checks a few samples against what the chart saw and stops at the contract broken.
    void setSeriesData(int id, const double* x, const double* y, size_t count, uint64_t revision);
    /// The data of a series, copied (the chart owns it): the arrays are the caller's again at once.
    void setSeriesDataCopy(int id, const double* x, const double* y, size_t count);
    void removeSeries(int id);
    void clearSeries();
    int seriesCount() const;
    int seriesIdAt(int index) const;
    int seriesIndexOf(int id) const; // -1 when there is none
    const PlotSeriesInfo* seriesInfo(int id) const;
    void setSeriesVisible(int id, bool visible);
    /// The colour the series is drawn in (its own, or its place in the palette of the theme).
    Color seriesColor(int id) const;
    size_t seriesPointCount(int id) const;
    /// How much of the min/max pyramid of the series is built (0..1); the exact reduction needs 1 (the chart asks for frames until it is).
    float pyramidProgress(int id) const;

    // ── View and axes ─────────────────────────────────────────────────────
    /// The visible x range. A programmatic change: viewChanged is emitted with byUser false.
    void setViewX(double x0, double x1);
    double viewX0() const;
    double viewX1() const;
    /// The y range of an axis; the axis becomes Manual.
    void setViewY(int axis, double y0, double y1);
    double viewY0(int axis = 0) const;
    double viewY1(int axis = 0) const;
    void setYMode(int axis, PlotYMode mode);
    PlotYMode yMode(int axis = 0) const;
    /// The x range that "the whole data" is (fit()): the application's when it gave one (the series it pushes are windows), else the extent of the series.
    void setExtent(double lo, double hi);
    void clearExtent();
    /// The whole extent in x, y Auto on both axes.
    void fit();
    void fitX();
    /// Keeps `x` (the playback clock) in view while `follow` is on: when it leaves the view the view moves so that it is at three quarters of it.
    void setFollowX(double x, bool follow);
    /// Zoom and pan by code (byUser false).
    void zoomX(double factor, double aboutX);
    void panX(double dx);
    /// The first and the last x of the data of the visible series (false when there is none).
    bool dataExtent(double& lo, double& hi) const;

    /// Text of the ticks: (value, step) -> label. The defaults are plot::formatTime for x and plot::formatValue for y.
    void setXTickFormatter(ct::Function<String(double, double)> fn);
    void setYTickFormatter(int axis, ct::Function<String(double, double)> fn);
    /// An x axis that is time in seconds (nice steps of seconds, minutes, hours; the default) or plain numbers.
    void setTimeAxis(bool time);

    // ── What the application draws over the series (an overlay: it never rebuilds the layer) ─────────────────────
    void setClock(double t, bool valid);
    void setMarker(int id, double x, const Color& color, const String& text = String());
    void removeMarker(int id);
    void clearMarkers();
    void setBand(int id, double x0, double x1, const Color& color = Color(0, 0, 0, 0)); // alpha 0: the theme's band colour
    void removeBand(int id);
    void clearBands();
    /// The range being selected, by code (or by the person: see rangeSelected).
    void setRange(double a, double b);
    void clearRange();
    bool hasRange() const;
    double rangeStart() const;
    double rangeEnd() const;
    /// While on, a plain left drag selects a range (the application's "range tool").
    void setRangeTool(bool on);
    /// The vertical cursor line while the pointer is over the plot.
    void setCursorEnabled(bool on);
    bool hasHover() const;
    double hoverX() const;
    int hoverSeries() const; // index, -1: none
    int hoverPoint() const;

    /// The legend, with a click that toggles a series and a cross that removes it when `removable`.
    void setLegendVisible(bool on);
    void setLegendRemovable(bool on);
    /// A click on a legend entry shows or hides the series itself (on by default) and then reports it (legendEntryClicked); off: it only reports.
    void setLegendAutoToggle(bool on);
    /// The text next to the cursor in the legend: with none set the chart draws the time of the cursor and, for each series, the value of
    /// the nearest sample; `values` has one text per series, by index (empty: that one is drawn by the chart).
    void setReadout(const String& header, const ct::Vector<String>& values);
    void clearReadout();
    /// Text right-aligned in the header.
    void setNote(const String& text);
    /// A table under the plot: the row labels, and one column for each series (or any other column).
    void setStats(const ct::Vector<String>& rowLabels, const ct::Vector<PlotStatsColumn>& columns);
    void clearStats();

    void setInteractive(bool on);
    bool interactive() const;
    void setBindings(const PlotBindings& bindings);
    const PlotBindings& bindings() const;

    // ── Signals (all in data units) ──────────────────────────────────────
    /// The view changed: x0, x1, y0, y1 of the left axis, and whether the person did it (a gesture) or the code did. Emitted during the gesture.
    Signal<double, double, double, double, bool> viewChanged;
    /// The pointer over the plot: x and y of the pointer (left axis), the series (index) and the point (index) nearest to it, and whether the pointer is inside.
    Signal<double, double, int, int, bool> hovered;
    Signal<> hoverEnded;
    /// A range being dragged (done false) and released (done true), and selected / cleared.
    Signal<double, double, bool> rangeChanged;
    Signal<double, double> rangeSelected;
    Signal<> rangeCleared;
    /// A click (press and release that moved less than the slop) on the plot: x, y (left axis), button, modifiers (PlotMod). It does not consume the press for a selection.
    Signal<double, double, int, int> plotClicked;
    Signal<int> legendEntryClicked;       // series index
    Signal<int> legendEntryRemoveClicked; // series index
    Signal<int, PlotYMode> yModeChanged;  // axis, mode: the person zoomed y (it became Manual) or fit

    // ── Geometry and measures (for the application, and the tests) ────────────────────────────────
    /// The plot area, in window coordinates, as of the last paint.
    Rect plotRect() const;
    double xToPixel(double x) const;
    double pixelToX(double px) const;
    double yToPixel(int axis, double y) const;
    double pixelToY(int axis, double py) const;
    /// How many times the layer (background, grid, lines) was built: it must not grow on a hover, a moving clock or a marker.
    uint32_t layerRebuilds() const;
    /// The points drawn for a series as of the last layer: (x, y) in plot pixels, the polyline or the (min, max) of each column.
    int layerPointCount(int seriesIndex) const;
    bool layerColumn(int seriesIndex, int column, float& x, float& lo, float& hi) const;
    bool layerIsEnvelope(int seriesIndex) const;
    /// The tick labels of the last layer (x then the left y axis).
    int xTickCount() const;
    double xTick(int i) const;
    String xTickLabel(int i) const;
    int yTickCount(int axis) const;
    double yTick(int axis, int i) const;
    /// Whether the layer is kept as geometry drawn by reference by the renderer (OpenGL) or copied into the draw list.
    bool layerByReference() const;

    // ── Widget ───────────────────────────────────────────────────────────
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onMouseLeave() override;
    void onKeyPress(KeyEvent& e) override;

private:
    struct Impl;
    Impl* impl_;
    bool emitYMode(int axis, PlotYMode mode);
};

} // namespace retained
} // namespace ig
