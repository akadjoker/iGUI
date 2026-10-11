# iGUI vs ImGui — headless comparison

Same widget scenes, submitted once through `ig::Context` (iGUI immediate mode)
and once through ImGui 1.92.5, measured without a window or a GPU. The point is
to answer "is iGUI worth it" with numbers instead of impressions: how much CPU
does a frame of the same UI cost in each library, and how much work does the
backend get handed at the end.

```
lib     submit mean    render     total    median       p95       min     vtx      idx    cmds widgets text meas/shape   rss kb
igui          33.39      0.74     34.14     33.28     39.82     26.46    1160     2466       1     130   173/133           -
imgui         24.55      0.77     25.32     25.30     28.72     18.82    1476     2400       2     130             -

igui relative to imgui (1.00 == equal):
  submit       1.36x        (widget calls, draw data ready, mean)
  submit+render 1.35x       (the whole CPU side of a frame)
  submit (min) 1.41x        (the fastest frame: the metric a noisy machine does not spoil)
  submit (med) 1.32x
  vertices     0.79x        (glyphs included; a real backend draws these)
  draw calls   0.50x
```

## Building and running

```sh
cmake -S . -B build -G Ninja \
  -DIGUI_BUILD_BENCH=ON \
  -DIGUI_BENCH_IMGUI_DIR=/path/to/imgui-1.92.5 \
  -DIGUI_BUILD_FONT=ON \
  -DIGUI_FONT_INPUT=/path/to/a.ttf        # optional; otherwise CMake downloads Roboto
cmake --build build --target igui_bench
./build/bench/igui_bench --scene=gallery --blocks=10 --frames=800 --size=1280x3000
```

`IGUI_BENCH_IMGUI_DIR` is optional: without it the benchmark measures iGUI alone
and prints the ImGui row as unavailable. `IGUI_BUILD_FONT` decides whether iGUI
measures text with its real font atlas or with a fixed-advance stand-in, and the
`note:` line under the table always says which one was used.

| Option | Meaning |
| --- | --- |
| `--lib=igui\|imgui\|both` | what to measure (default `both`) |
| `--scene=basic\|gallery\|text\|windows\|plots` | scene to submit (default `gallery`) |
| `--blocks=N` | repetitions of the widget block (`gallery`, `text`) |
| `--windows=N` | windows in the `windows` scene |
| `--frames=N`, `--warmup=N` | measured frames and frames before measuring |
| `--size=WxH` | viewport; the window height follows it |
| `--no-font` | iGUI only: fixed-advance metrics, no glyph geometry (diagnostic) |
| `--csv=FILE` | one row per library, for plotting |

`ctest -R igui_bench_smoke` runs a 5-frame pass so a broken benchmark fails CI.
Build the benchmark Release, or with no `CMAKE_BUILD_TYPE` at all (the bench
targets then add `-O2` themselves): microsecond numbers from a debug build
measure the build.

## Scenes

The widget blocks are written twice — once with iGUI's calls in
[IguiRunner.cpp](./IguiRunner.cpp), once with ImGui's in [ImguiRunner.cpp](./ImguiRunner.cpp)
— and are kept the same sequence on purpose. Think of them as the contract:

| Scene | Per repetition | Counting |
| --- | --- | --- |
| `basic` | one window, one `gallery` block | |
| `gallery` | `label`, `button` + `sameLine` + `checkbox`, `sliderFloat`, `inputText`, `comboBox`, `separator`, `progressBar`, `collapsingHeader` (expanded → 3 labels), `treeNode` (expanded → 1 label) | 13 widgets |
| `text` | 10 labels of ~25 characters | 10 widgets |
| `windows` | `windows` windows, each `label`, `button`, `checkbox`, `sliderFloat` | 4 widgets |
| `plots` | `plotLines` and `plotHistogram` over the same 90-sample ring, both 400 px wide | 2 widgets |

Both runners count widgets, and `main` prints a warning when the two counts
differ, so a widget added on one side and forgotten on the other is reported
instead of quietly skewing the ratio. Two deliberate equalizations:

* iGUI's theme radius and ImGui's `FrameRounding`/`WindowRounding` are set to 0,
  so the vertex counts compare the widgets and not one library's taste in
  decoration. iGUI's shipped theme rounds and anti-aliases every frame, which
  adds vertices ImGui does not draw.
* ImGui's window position and size are forced every frame (`ImGuiCond_Always`),
  because the iGUI side is handed explicit bounds.

What is *not* equalized, because it is a property of the libraries:

* Text: iGUI measures through `TextProvider::measureText` and shapes through
  `appendText`, ImGui does both inside the widget call. Both halves are timed and
  the split is reported.
* ImGui's `TreeNode` indents its children itself; iGUI's layout `treeNode` does
  not.
* The iGUI side links the real font module when available, so both sides shape
  real glyphs.

## Results

Minima of 600 frames, best of three runs, measured in **CPU time** (not wall
time - this machine is shared, and wall time then measures how busy it is), with
the library built `-O3`. Ratios below 5% are noise.

| Scene | Widgets | iGUI | ImGui | Ratio (min) | iGUI geometry | ImGui geometry |
| --- | --- | --- | --- | --- | --- | --- |
| `basic` (1 window) | 13 | 6.2 µs | 6.2 µs | 0.99x | 626 vtx, 1 cmd | 530 vtx, 2 cmds |
| `gallery`, clipped to 800px | 130 | 26.5 µs | 18.7 µs | 1.42x | 1160 vtx, 1 cmd | 1476 vtx, 2 cmds |
| `gallery`, all visible | 130 | 42.6 µs | 33.6 µs | 1.27x | 4560 vtx, 3 cmds | 4796 vtx, 2 cmds |
| `text` (100 labels) | 100 | 32.5 µs | 26.9 µs | 1.21x | 2928 vtx, 1 cmd | 3836 vtx, 2 cmds |
| `windows` (8 windows) | 32 | 26.7 µs | 19.8 µs | 1.35x | 2728 vtx, 1 cmd | 1568 vtx, 16 cmds |
| `plots` (10 lines + 10 histograms) | 20 | 31.6 µs | 25.1 µs | 1.26x | 2932 vtx, 3 cmds | 2928 vtx, 2 cmds |

The ratio column uses the **min** of 600 frames (best of 3 runs), not the mean:
this machine is shared, and a frame that loses the CPU for a moment moves the
mean without saying anything about the library. `igui_bench` prints both ratios
(`submit (min)` and `submit (med)`) for that reason. Measurements drift by up to
10% between sessions, so read a row as "clipped gallery 1.4x, not 1.0x", not as
one decimal place of truth.

Peak RSS, one library per process: iGUI 6.3 MB, ImGui 5.1 MB. In `--lib=both`
the figure belongs to the whole process, so the table prints `-` there; use a
single `--lib` for memory.

The `render` column (the backend walking the draw data) is under 1 µs for both,
so the whole comparison is the CPU side of building a frame.

Reproduce with `--frames=600` (and `--size=1280x3000` for the row where the
whole window is visible at once). `--dump` prints every draw command of the last
frame - clip, texture, index count and the bounding box of what it draws - which
is how the draw-call counts below were taken apart.

The `gallery` rows are the ones where iGUI is behind: 130 widgets, every one of
them a label plus a frame plus an antialiased rounded border, and the clipped row
draws all of them every frame. The `text` row is mostly the font path (below).

The `plots` row was the worst of the set (1.78x) until the series became one
primitive: a plot line is now a single antialiased band through every sample
(`DrawList::addPolyline`, two vertices per point plus the two soft edges) and a
histogram one batched call for all its bars (`DrawList::addRectFilledBatch`),
where before each of the 89 segments and 90 bars was its own call with its own
bounds and clip test. That took the scene from 44.4 µs to 31.9 µs and its
vertices from 3836 to 2932; what is left of the ratio is ImGui drawing a plain
four-vertex quad per segment against iGUI's three index strips per segment (its
band is soft on both sides, and the index count is the price: 8658 against
4428).

### Where iGUI's time goes

The `text` scene is the cleanest decomposition, because both libraries then draw
from one texture and the same text: 2928 vertices against 3836.

| | iGUI | ImGui |
| --- | --- | --- |
| Whole frame | 32.5 µs | 26.9 µs |
| Same frame with `--no-font` (no measuring, no glyph quads) | 16.1 µs | - |
| Font path (100 labels, ~25 glyphs each, measure + shape) | 16.4 µs (50%) | - |

About 1.5 µs of that font path is the price of crisp glyphs: a glyph is placed on
whole pixels (`truncf`, the way ImGui's `RenderText` rounds `pos.x`), because a
fractional position makes the atlas' linear filter interpolate between
neighbouring texels - text came out blurry and dimmer than the colour asked for
(peak 207/255 against 226/255 now). Without the rounding the `text` row measures
31.3 µs; the 5% buys glyphs that look like ImGui's.

What is left of the gap is spread thin: the per-widget machinery (`contentClip()`,
`topWindowAt()` for every widget, labels measured twice by the auto-layout button
and checkbox - 173 measure calls against 133 shape calls in the gallery scene),
and the three antialiased circles a slider draws where ImGui draws a rectangle.

Cost per widget, all 130 gallery widgets visible: iGUI ~0.30 µs, ImGui ~0.25 µs.
On the `plots` scene the same figure is ~1.6 µs against ~1.25 µs: both draw 90
bars and 89 line segments per graph, iGUI as two calls.

### What was optimized here, and what it bought

All in the core, found with the numbers above and `--dump`:

* **A tighter clip that cuts nothing is no clip.** `DrawList::emitGeometry`
  merges a command into the previous one when its clip rect lies inside the
  previous clip and the geometry lies inside its own clip (every primitive hands
  over its bounding box, fringe included). A text field's clipped label, a combo's
  clipped caption, a whole window that fits its content area - none starts a
  draw call any more.
* **Glyphs are trimmed to the clip on the CPU** (`DrawList::QuadBatch::add`,
  texture coordinates adjusted), the way ImGui does, so a text run never needs a
  scissor of its own.
* **Windows share the frame's geometry.** Every window's draw list writes its
  vertices and indices straight into the frame's arrays (`DrawList::shareGeometry`)
  and keeps only its commands; `endFrame` gathers commands, not geometry. That
  removed a copy of every vertex of the frame - 19% of the text scene - and lets
  the chrome of adjacent windows merge into one draw call across the seam.
* **Vertices are built in place.** `push_back(DrawVertex{...})` cost 6-7 ns per
  vertex in `ct::Vector` (a temporary, then a copy); `emplace_back` through a
  `DrawVertex` constructor costs 1.1 ns. A run's indices are written in one go
  at `flush()` instead of six pushes per glyph; rects, lines and circles write
  theirs through `allocIndices()`.
* **The font atlas looks glyphs up in a table.** `FontAtlas::lookup_` maps a
  codepoint to its glyph in one load; glyph texture coordinates are precomputed;
  ASCII bytes skip the UTF-8 decoder. `measureText` went from ~30 ns to 2.7 ns
  per glyph.
* **No `sinf`/`cosf` per vertex.** Circles and rounded corners read a 64-sample
  unit circle (`unitCircle()`); a rounded rectangle wholly inside its clip emits
  its triangles in bulk instead of culling each one. With the default theme's
  `borderRadius` of 4 this is every button and input frame; in the bench it is
  the slider track, and the rounded frames are most of what a window costs: the
  8-window scene went from 70.5 µs to 26.7 µs, 1.35x of ImGui's 19.8 µs instead
  of 3.6x.

| Scene | CPU before | CPU now | ImGui | draw calls before | now | ImGui |
| --- | --- | --- | --- | --- | --- | --- |
| `basic` | 15.1 µs | 6.2 µs | 6.2 µs | 7 | 1 | 2 |
| `gallery`, clipped | 42.2 µs | 26.5 µs | 18.7 µs | 11 | 1 | 2 |
| `gallery`, all visible | 107.7 µs | 42.6 µs | 33.6 µs | 122 | 3 | 2 |
| `text` | 75.9 µs | 32.5 µs | 26.9 µs | 3 | 1 | 2 |
| `windows` (8) | 70.5 µs | 26.7 µs | 19.8 µs | 96 | 1 | 16 |

The "before" column is measured, not remembered: the tree this work started from
is in git (`git archive HEAD` into a scratch directory, plus this bench
directory - the bench itself is not in that commit). It is the same benchmark,
the same scenes and the same machine, so the two columns are comparable to each
other even where this table's absolute microseconds have drifted since.

### Draw calls

ImGui keeps its white pixel inside the font atlas, and so does iGUI, so both draw
shapes and glyphs from one texture. ImGui then spends two commands per window
(the chrome under the outer clip, the content under the inner one), 16 for the
`windows` scene. iGUI's merge rule folds a window's content into the chrome's
command when the content stays inside its clip, and the shared geometry lets the
next window continue the same command, so a screen of windows that fit their
content is one draw call. What still splits a window is geometry that really
crosses the content clip's edge - a widget half scrolled out of view, or the 1px
antialiasing fringe of the last widget touching the edge, which is the 3 of the
all-visible gallery.

## Widget and feature parity

Both libraries cover the same core set, with the same names in most cases:
window/child/scroll, label/text variants (including disabled, coloured, wrapped
and bullets), button/small/invisible/image button, checkbox/radio/selectable,
slider (float/int), drag (1–4 components), input text/single and multiline (with
hint), input int/float (1–4), combo, list box, tab bar, tree node/leaf,
collapsing header (with and without a close button), menu bar/menu/submenu/
context menu, separator and separator text, progress bar (with overlay and the
indeterminate form), plots (lines and histogram), colour button and editor,
tooltips, tables, drag & drop, and disabled blocks.

What iGUI's immediate mode has that ImGui's core does not:

* Docking built in (`beginDockSpace`/`beginDockPanel`, with tab dragging between
  regions), where ImGui ships it as a separate branch/library.
* A code editor widget with syntax highlighting, in both modes.
* Domain widgets in the immediate API: `gizmo2D`/`gizmo3D`, `fader`,
  `gradientEditor`, `curveEditor`, `sequencer`, `timeline`.
* Virtualized views (`beginVirtualList`/`Table`/`Tree`) and an aligned
  property-row helper.
* Message boxes, modal dialogs and toasts as first-class calls.
* Window state persistence (position, size, minimized, dock layout) in the
  `.ini`, with the same behaviour as ImGui's `imgui.ini`.
* A second, retained widget API on the same drawing engine — the one that carries
  the node editor, timeline, audio, asset browser, thumbnail grid and file
  dialog widgets — and the two can be linked into one binary.

What ImGui's core has that iGUI's immediate mode does not:

* The demo/metrics windows and style editor, and the `ImDrawList` API as a
  first-class public surface.
* Backend adapter libraries for every common renderer (DX11/12, Metal, Vulkan,
  OpenGL, SDL, GLFW, WebGPU...), where iGUI ships SDL2 and Raylib adapters, and
  the widget-testing automation of `imgui_test_engine`.
* A unit-testable "no backend" mode is not documented there, which is why the
  ImGui side of this benchmark claims `ImGuiBackendFlags_RendererHasTextures`
  instead of building a font atlas.

## Verdict

On frame submission iGUI costs between 0.99x and 1.42x the CPU of ImGui for the
same widgets, and hands the backend fewer draw calls (1-3 against 2-16). The
output geometry is comparable (0.79x the vertices of ImGui on the gallery scene,
0.76x on the text scene), the text hot loops are on par, and the remainder is
per-widget machinery and the slider's circles rather than one hot spot - the
`plots` scene, once the worst at 1.78x, is now inside that range at 1.26x.

At these costs a 130-widget screen is 27-43 µs (0.16-0.26% of a 60 fps budget)
and the 100-label screen 33 µs, so for an editor or tool UI the difference is not
measurable in frame time; what the ratio says is that the widget set ImGui does
not have (docking, code editor, gizmos, timeline, retained mode sharing one
engine) and a core that never touches the STL no longer come with a performance
bill attached.

Against the tree this work started from, the same benchmark measures iGUI at
15.1 µs (`basic`), 42.2 µs (clipped gallery), 107.7 µs (full gallery), 75.9 µs
(`text`) and 70.5 µs (8 windows) - and 122 draw calls on the full gallery against
3 now. That is what "it is worth it" rests on: not that iGUI beats ImGui
everywhere, but that it is now within a factor of 1.4 of it on the scenes where
it is behind at all, from a starting point of 2.4-3.6x.

What the data points at next, in order:

1. `topWindowAt()` runs once per widget (hit-testing every window); it is ~4% of
   the `windows` scene and could be answered once per window per frame.
2. The auto-layout `button` and `checkbox` measure their label for the layout and
   again inside the positioned call (173 measure calls against 133 shape calls in
   the gallery scene).
3. `QuadBatch::add` is an out-of-line call per glyph; inlining it would let the
   compiler keep the run's state in registers.
4. The plot band is soft on both sides, which costs three index strips per
   segment; a single-quad-per-segment variant (the way ImGui does it) would halve
   the indices at the price of one hard edge. Worth it only if a backend ever
   shows the index count rather than the draw calls.
