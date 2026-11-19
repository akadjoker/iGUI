# iGUI Chart: zoom, cursor, range (requirements from zenMCAP, 2026-11-19)

Interactions
- Wheel zooms x about the pointer; wheel with a modifier zooms y. Drag pans x. Double click or a call fits the whole data. A "follow" mode keeps a given x (the playback clock) in view.
- A plain click stays free for the application: the widget reports click(x in data units) and does not consume it for selection.
- Range selection on x by drag with a modifier or in a "range mode" the application switches on; Escape or a click clears it. Box zoom not needed.
- Per-axis lock: y automatic to the visible x range (default) or fixed to the full data range; the application can also set either range.

Callbacks (all in data units)
- hover(x, y, seriesIndex of nearest, pointIndex of nearest, inside) and hoverEnded; the widget draws the vertical cursor line itself and lets the application supply the readout text or draw it (time plus one value per series in the legend, not a floating tooltip).
- rangeSelected(x0, x1), rangeCleared.
- viewChanged(x0, x1, y0, y1, byUser): fires during the gesture; setting data from inside the handler must be safe.
- clicked(x, y, button, modifiers).
- legendEntryClicked(seriesIndex) and a close/remove affordance per legend entry; series colours stay stable when one is removed.

Data and sizes
- 1 to about 16 series per chart. Each series at most 4000 points, already reduced by the application to min/max bins (2000 bins, two points per bin) for the visible window; the widget must not need the full data: the application pushes new points on viewChanged. A per-series flag "these are min/max pairs" so the line is drawn as an envelope.
- Series can have gaps (NaN or a break flag) and different x samples per series (no shared x array).
- Data replaced several times per second without flicker or layout change; the visible range survives a data replacement.

Axes
- x is time in seconds as double; tick labels with enough digits down to milliseconds/microseconds (a formatter callback is enough).
- Vertical marker lines at given x with a colour ("now" line; In/Out marks) and a shaded x band (selected range, In..Out).
- Linked x across several charts: setting the x range programmatically without re-emitting byUser is enough.
- Second y axis (per-series axis assignment) and XY plots (x from another series) are their next two features: the API should allow for them from the start.

Theme: every colour from Theme (grid, axis text, cursor, selection band, legend), readable in dark, light, macOS, macOSDark; themeChanged repaint.
Idle: no repaint when nothing changes.

Answers (2026-11-19)
- Gestures: Ctrl+wheel for y zoom, Shift+drag for the range, both configurable.
- Overlay: the cursor line, the "now" marker, the In/Out marks and the range band move without redrawing the series (the clock moves every frame during playback; the series change only on viewChanged or new data). If the backend cannot cache the series layer, at least keep these out of whatever marks the chart's data dirty.

## Reference: zenMCAP's own PlotView contract (2026-11-19; where it differs from the text above, this wins). zenMCAP keeps its own widget; the iGUI Chart is a general one for other tools.
Gestures: wheel = zoom x about the pointer -> onView(lo, hi); shift+wheel = pan x a tenth of the view per notch; ctrl+wheel = zoom y about the pointer, y becomes Manual -> onYMode(Manual); left drag = pan x when zoomed, and at the whole extent it moves the clock (scrub); middle drag = pan x; shift+left drag (or a plain drag while a range tool is on) = mark a range -> onRange(a, b, done), done on release; press and release that moved under 4 px = click: seek and clear the range -> onSeek(t); double click = whole extent, y Auto -> onFit(); Escape with focus -> onRangeClear(); click on a legend entry -> onSeriesAction(id, Toggle); click on the x of a legend entry -> onSeriesAction(id, Remove); pointer over the plot = vertical line, the time in the header, per visible series the value of the NEAREST sample in the legend and a dot on the line.
Push model: setSeries(Vector<Series>), Series = { int id (stable); String name, unit; Color colour (owner's, kept while others come and go); bool visible; bool pending; const double* t, *y; size_t count; uint64 revision (unchanged revision = unchanged data); double fullMin, fullMax }. Arrays belong to the owner and stay valid until the next setSeries (pointers kept, no copy). State: setExtent(lo, hi); setView(lo, hi); setClock(t, valid); setMarks(in, out) (negative = not set); setRange(a, b) / clearRange(); setStats(Vector<StatsColumn{id, pending, count, min, max, mean, first, last, delta, duration}>) for a table under the plot; setYMode(Auto | Full | Manual); setRangeTool(bool); setInteractive(false) refuses zoom, pan and range; setNote(text) right-aligned in the header. What the person changes is applied at once and the callback tells the owner.
Markers and bands: clock line, In and Out flagged lines, range as a translucent band between two lines, hover line with a dot per series; painted over the series every frame, never causing the series layer to redraw.
Their measurements (16 series x 4000 points, 1400x900, CPU time of paint): idle 0.10 ms with a cached texture layer against 5.6 ms through PaintContext polylines; playback 0.10-0.12 ms against 4.7-6.3 ms; panning 0.18-0.28 ms + 0.6-1.0 ms per layer redraw against 2.3-2.8 ms. Lesson for the iGUI Chart: the polyline cost in the draw list is the thing to attack (retained vertex buffer per series, redrawn only on data/view/size/theme change) with an overlay that does not rebuild it.
Other iGUI notes from them: (20) no way to issue GL inside paint (the draw callback); (21) a FileDialog save path needed its own wrapper in the host (ask for details when the round starts).

## Other iGUI items from zenMCAP for the same round (2026-11-19)
- FileDialog Save mode: no defect reported (earlier "point 21" withdrawn). Questions to verify and add if missing: confirm before overwriting an existing file; append the filter's extension when the typed name has none; optionally remember the last folder (the application does it by hand today).
- PaintContext::drawPolyline cost for long lines was measured only headless, not on a GL window: measure on GL.
- TreeView: a long node text is cut at the panel edge without an ellipsis; no second, dim, right-aligned text per row (their Topics panel measures and cuts labels itself).
- TreeGrid: has columns but no activation (double click) signal.
