#pragma once

#include "Widget.hpp"
#include "Signal.hpp"
#include <vector>
#include <string>
#include <functional>

namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  GradientEditor - horizontal color gradient with draggable stops
// ═════════════════════════════════════════════════════════════════════════════

struct GradientStop {
    float pos;   // 0..1
    Color color;
};

class GradientEditor : public Widget
{
public:
    GradientEditor();

    /// @brief Add a color stop at the given position.
    void addStop(float pos, const Color& color);
    /// @brief Remove a stop by index.
    void removeStop(int idx);
    /// @brief Set the color of a stop.
    void setStopColor(int idx, const Color& color);
    /// @brief Set the position of a stop (0..1).
    void setStopPos(int idx, float pos);
    /// @brief Remove all stops.
    void clearStops();

    /// @brief Get the list of gradient stops.
    const std::vector<GradientStop>& stops() const { return stops_; }
    /// @brief Get the number of stops.
    int  stopCount() const { return static_cast<int>(stops_.size()); }
    /// @brief Get the currently selected stop index.
    int  selectedStop() const { return selected_; }

    /// @brief Sample the gradient color at position t (0..1).
    Color sample(float t) const;

    /// @brief Set the gradient bar height in pixels.
    void  setBarHeight(float h) { barHeight_ = h; markDirty(); }
    /// @brief Get the gradient bar height.
    float barHeight() const     { return barHeight_; }
    /// @brief Set whether to show the alpha checkerboard.
    void  setShowAlpha(bool s)  { showAlpha_ = s; markDirty(); }
    /// @brief Get whether alpha display is enabled.
    bool  showAlpha() const     { return showAlpha_; }

    /// @brief Emitted when any stop changes.
    Signal<const std::vector<GradientStop>&> onChanged;
    /// @brief Emitted when a stop is double-clicked for editing.
    Signal<int, Color>                       onEditStop;

    Vec2f sizeHint() const override { return {200, 58}; }
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;

private:
    std::vector<GradientStop> stops_;
    int   selected_   = -1;
    int   dragging_   = -1;
    float dragOffX_   = 0;
    float barHeight_  = 24.0f;
    bool  showAlpha_  = true;

    Rect barRect() const;
    Rect handleRect(int idx) const;
    int  hitStop(float mx, float my) const;
    void sortStops();
    void emit();
};

// ═════════════════════════════════════════════════════════════════════════════
//  HistogramWidget - Value distribution display
// ═════════════════════════════════════════════════════════════════════════════

class HistogramWidget : public Widget
{
public:
    HistogramWidget();

    /// @brief Set the raw data values and recompute bins.
    void setData(const std::vector<float>& values);
    /// @brief Set pre-computed bin values and range.
    void setBins(const std::vector<float>& bins, float rangeMin, float rangeMax);
    /// @brief Clear all data and bins.
    void clearData();
    /// @brief Set the number of bins.
    void setBinCount(int n);
    /// @brief Get the number of bins.
    int  binCount() const { return binCount_; }

    /// @brief Set the value range.
    void  setRange(float lo, float hi) { rangeMin_ = lo; rangeMax_ = hi; recomputeBins(); markDirty(); }
    /// @brief Get the range minimum.
    float rangeMin() const { return rangeMin_; }
    /// @brief Get the range maximum.
    float rangeMax() const { return rangeMax_; }
    /// @brief Enable or disable automatic range detection.
    void  setAutoRange(bool a) { autoRange_ = a; }

    /// @brief Set the bar color.
    void  setBarColor(const Color& c) { barColor_ = c; markDirty(); }
    /// @brief Get the current bar color.
    Color barColor() const            { return barColor_; }

    /// @brief Set the chart title.
    void setTitle(const std::string& t) { title_ = t; markDirty(); }
    /// @brief Set whether to show value labels on bars.
    void setShowValues(bool s) { showValues_ = s; markDirty(); }
    /// @brief Set whether to show the mean line.
    void setShowMean(bool s)   { showMean_ = s; markDirty(); }
    /// @brief Enable logarithmic y-axis scale.
    void setLogScale(bool s)   { logScale_ = s; markDirty(); }

    /// @brief Get the computed mean of the data.
    float mean()   const { return mean_; }
    /// @brief Get the computed standard deviation.
    float stddev() const { return stddev_; }

    Vec2f sizeHint() const override { return {200, 120}; }
    void  paint(PaintContext& ctx) override;

private:
    std::vector<float> rawData_;
    std::vector<float> bins_;

    int   binCount_   = 32;
    float rangeMin_   = 0, rangeMax_ = 1;
    bool  autoRange_  = true;
    bool  logScale_   = false;
    bool  showValues_ = false;
    bool  showMean_   = true;
    float mean_       = 0;
    float stddev_     = 0;

    Color barColor_ = Color(100, 160, 230, 200);
    std::string title_;

    static constexpr float kPadL = 8.0f;
    static constexpr float kPadR = 8.0f;
    static constexpr float kPadT = 20.0f;
    static constexpr float kPadB = 20.0f;

    void recomputeBins();
    void computeStats();
};

// ═════════════════════════════════════════════════════════════════════════════
//  PlotWidget - Line / Bar / Scatter charts with pan + zoom
// ═════════════════════════════════════════════════════════════════════════════

enum class PlotType { Line, Bar, Scatter };

struct PlotSeries {
    std::string name;
    Color       color;
    PlotType    type    = PlotType::Line;
    bool        visible = true;
    float       lineWidth = 2.0f;
    std::vector<float> values;
};

class PlotWidget : public Widget
{
public:
    PlotWidget();

    /// @brief Add a named data series and return its index.
    int  addSeries(const std::string& name, const Color& color, PlotType type = PlotType::Line);
    /// @brief Remove a series by index.
    void removeSeries(int idx);
    /// @brief Remove all series.
    void clearSeries();

    /// @brief Append a data point to a series.
    void addPoint(int seriesIdx, float y);
    /// @brief Replace all points in a series.
    void setPoints(int seriesIdx, const std::vector<float>& ys);
    /// @brief Clear all points from a series.
    void clearPoints(int seriesIdx);

    /// @brief Get the number of series.
    int  seriesCount() const { return static_cast<int>(series_.size()); }
    /// @brief Get a series by index.
    const PlotSeries& series(int idx) const { return series_[idx]; }
    /// @brief Set the visibility of a series.
    void setSeriesVisible(int idx, bool v) { series_[idx].visible = v; markDirty(); }

    /// @brief Set the chart title.
    void setTitle(const std::string& t)  { title_ = t; markDirty(); }
    /// @brief Set the x-axis label.
    void setXLabel(const std::string& l) { xLabel_ = l; markDirty(); }
    /// @brief Set the y-axis label.
    void setYLabel(const std::string& l) { yLabel_ = l; markDirty(); }

    /// @brief Set the y-axis range and disable auto range.
    void setYRange(float yMin, float yMax) { yMin_ = yMin; yMax_ = yMax; autoY_ = false; markDirty(); }
    /// @brief Enable or disable automatic y-axis range.
    void setAutoYRange(bool a) { autoY_ = a; markDirty(); }
    /// @brief Set the maximum number of points per series.
    void setMaxPoints(int n) { maxPoints_ = n; }

    /// @brief Set whether to show grid lines.
    void setShowGrid(bool s) { showGrid_ = s; markDirty(); }
    /// @brief Set whether to show the legend.
    void setShowLegend(bool s) { showLegend_ = s; markDirty(); }

    /// @brief Reset the view to default pan/zoom.
    void resetView();

    Vec2f sizeHint() const override { return {300, 180}; }
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;

private:
    std::vector<PlotSeries> series_;

    std::string title_;
    std::string xLabel_;
    std::string yLabel_;

    float yMin_ = 0, yMax_ = 1;
    bool  autoY_ = true;
    int   maxPoints_ = 0;

    bool showGrid_   = true;
    bool showLegend_ = true;

    float viewXMin_ = 0, viewXMax_ = 100;
    float viewYMin_ = 0, viewYMax_ = 1;
    bool  panning_ = false;
    float panStartX_ = 0, panStartY_ = 0;
    float panViewX0_ = 0, panViewY0_ = 0;

    Rect plotArea_ = {};

    void  computeAutoRange();
    Rect  plotArea(const Rect& b) const;
    float dataToScreenX(float x, const Rect& pa) const;
    float dataToScreenY(float y, const Rect& pa) const;
    float screenToDataX(float sx, const Rect& pa) const;
    float screenToDataY(float sy, const Rect& pa) const;
    void  paintGrid(PaintContext& ctx, const Rect& pa);
    void  paintSeries(PaintContext& ctx, const Rect& pa);
    void  paintLegend(PaintContext& ctx, const Rect& b);
    void  paintAxes(PaintContext& ctx, const Rect& b, const Rect& pa);
};

// ═════════════════════════════════════════════════════════════════════════════
//  CurveEditor - Bezier/Hermite animation curve editing
// ═════════════════════════════════════════════════════════════════════════════

enum class TangentMode { Free, Aligned, Auto };

struct CurveKey {
    float time  = 0;
    float value = 0;
    float tanInX  = -0.1f, tanInY  = 0;
    float tanOutX =  0.1f, tanOutY = 0;
    TangentMode tangentMode = TangentMode::Auto;
    bool selected = false;
};

struct CurveData {
    std::string name;
    Color       color;
    std::vector<CurveKey> keys;
    bool visible = true;
};

class CurveEditor : public Widget
{
public:
    CurveEditor();

    /// @brief Add a named curve and return its id.
    int  addCurve(const std::string& name, const Color& color);
    /// @brief Remove a curve by id.
    void removeCurve(int curveId);
    /// @brief Remove all curves.
    void clearCurves();

    /// @brief Get the number of curves.
    int  curveCount() const { return static_cast<int>(curves_.size()); }
    /// @brief Get a mutable reference to a curve.
    CurveData&       curve(int id)       { return curves_[id]; }
    /// @brief Get a const reference to a curve.
    const CurveData& curve(int id) const { return curves_[id]; }

    /// @brief Set the visibility of a curve.
    void setCurveVisible(int curveId, bool v) { curves_[curveId].visible = v; markDirty(); }

    /// @brief Add a keyframe to a curve and return its index.
    int  addKey(int curveId, float time, float value);
    /// @brief Remove a keyframe from a curve.
    void removeKey(int curveId, int keyIdx);
    /// @brief Set the time and value of a keyframe.
    void setKey(int curveId, int keyIdx, float time, float value);

    /// @brief Set the visible time and value range.
    void setViewRange(float minT, float maxT, float minV, float maxV);
    /// @brief Auto-fit the view to encompass all keys.
    void fitView();

    /// @brief Get the minimum visible time.
    float viewMinT() const { return viewMinT_; }
    /// @brief Get the maximum visible time.
    float viewMaxT() const { return viewMaxT_; }
    /// @brief Get the minimum visible value.
    float viewMinV() const { return viewMinV_; }
    /// @brief Get the maximum visible value.
    float viewMaxV() const { return viewMaxV_; }

    /// @brief Set the time snap grid interval (0 = off).
    void  setSnapTime(float s)  { snapTime_ = s; }
    /// @brief Set the value snap grid interval (0 = off).
    void  setSnapValue(float s) { snapValue_ = s; }

    /// @brief Set the playhead position in time.
    void  setPlayhead(float t) { playhead_ = t; markDirty(); }
    /// @brief Get the current playhead position.
    float playhead() const     { return playhead_; }
    /// @brief Set whether to show the playhead line.
    void  setShowPlayhead(bool b) { showPlayhead_ = b; }

    /// @brief Evaluate a curve at the given time.
    float evaluate(int curveId, float time) const;

    /// @brief Set the background color.
    void setBgColor(const Color& c) { bgColor_ = c; }

    /// @brief Emitted when a key is moved (curve, key, time, value).
    Signal<int, int, float, float> onKeyChanged;
    /// @brief Emitted when a key is added (curve, key index).
    Signal<int, int>               onKeyAdded;
    /// @brief Emitted when a key is removed (curve, key index).
    Signal<int, int>               onKeyRemoved;

    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;

private:
    std::vector<CurveData> curves_;
    Color bgColor_ = Color(28, 30, 34, 255);

    float viewMinT_ = 0, viewMaxT_ = 2;
    float viewMinV_ = -1, viewMaxV_ = 2;

    bool dragging_     = false;
    bool panning_      = false;
    int  dragCurve_    = -1;
    int  dragKey_      = -1;
    bool dragTangent_  = false;
    bool dragTanIn_    = false;
    float dragStartX_  = 0, dragStartY_ = 0;
    float panStartMinT_ = 0, panStartMaxT_ = 0;
    float panStartMinV_ = 0, panStartMaxV_ = 0;

    float snapTime_  = 0;
    float snapValue_ = 0;

    float playhead_     = 0;
    bool  showPlayhead_ = true;

    float timeToX(float t) const;
    float valueToY(float v) const;
    float xToTime(float x) const;
    float yToValue(float y) const;

    float snap(float val, float grid) const;
    void  autoTangents(CurveData& c, int keyIdx);
    float evalSegment(const CurveKey& k0, const CurveKey& k1, float t) const;

    struct HitResult { int curve = -1; int key = -1; bool tangent = false; bool tanIn = false; };
    HitResult hitTest(float mx, float my) const;

    void paintGrid(PaintContext& ctx, const Rect& b);
    void paintCurves(PaintContext& ctx, const Rect& b);
    void paintKeys(PaintContext& ctx, const Rect& b);
    void paintPlayhead(PaintContext& ctx, const Rect& b);
};

} // namespace BuGUI
