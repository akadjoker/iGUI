# fonts-gl: the commit 1d57c00 (zenhtml-fonts-gl) brought onto main 1a77b1f

Branch `fonts-gl`, based on `origin/main` 1a77b1f, fast-forwardable (origin/main is an ancestor). Commits:

1. `d95866b` the cherry-pick of 1d57c00 (original author kept), conflict resolved, see below.
2. `cf3db2a` tests that draw text with the baked atlas pixels released (headless).
3. `fb89898` GlRaster/ZenApp: the offscreen target of `ZEN_SCREENSHOT_BACKEND=gl` is made and checked in `App::create`.
4. `d3b325e` test_zen_screenshot_gl: two applications (baked text, and dynamic page with a size and a codepoint first used after the release).
5. this note.

The four features of 1d57c00 (FontAtlas page crop, `releasePixels()`, `ZEN_SCREENSHOT_BACKEND=gl`, `Config::defaultDynamicPage`) are all in. The branch is NOT pushed (the task said not to push; a push was asked for later and was refused by the permission system): push it yourself with `git push origin fonts-gl`, it lives in the local repo /media/projectos/projects/cpp/iGUI.

## The conflict (only one)

`CMakeLists.txt`. Main had moved the window tests to `igui_add_window_test` (private Xvfb through `tools/xvfb-test.sh`, plus the `_refuses_unproven` companion); the commit registered its new test with a plain `add_test` in the old sidetabs block. Resolution: main's side entirely, and `igui_zen_screenshot_gl` registered with `igui_add_window_test` in its own block after the lifecycle one. `tests/test_zen_screenshot_gl.cpp` calls `igui_test::onThrowawayDisplay` first thing in `main()`. `ZenApp.cpp`, `GlRaster.cpp/.hpp`, `Retained.hpp/.cpp` and `test_text_sizes.cpp` applied without conflict over the text rework, the hostile-value hardening and `WidgetApp::update(IO&)`.

## Is anything reading the atlas pixels after createTexture? (verified by grep and reading, on this tree)

No, in the zen adapter. The only reader of the baked page of the default atlas is `adapters/zen/ZenApp.cpp:246` (`createTexture(..., atlas_.pixels())`), and `releasePixels()` is at :247, right after. `SoftRaster::createTexture` (adapters/zen/SoftRaster.cpp:114-126) copies the page into its own packed storage; `GlRaster::createTexture` uploads it. The dynamic pages re-upload `dynamicPixels()` (ZenApp.cpp:252, :598; WidgetApp.cpp:1804, :2094), which is another buffer, untouched by the release. `FontAtlas::bakeGlyph` (src/widgets/Retained.cpp:302) rasterises from the face data (`fc.info`), never from `pixels_`. `setPixelScale` re-bakes only the dynamic page. Textures are destroyed only when the main window goes (`App::destroyWindow`, ZenApp.cpp around line 360); there is no context-loss or texture re-creation code in the adapter, so nothing re-uploads. `atlas_` is private in `ZenApp` (no accessor), so a host cannot read the pixels of the default atlas. Screenshots read the framebuffer, not the atlas. The other readers of retained `pixels()` (examples/web/retained_gallery.cpp:179, examples/sdl2_original_widgets/main.cpp:86, bench/retained/RetainedBench.cpp:334, tests/RasterScreen.hpp) own their atlases, upload once and never call `releasePixels()`. The SDL/raylib/OpenGL backends read the other class, `ig::FontAtlas` (src/FontAtlas.cpp), not touched here. A face added with `addFont` + `build()` after the release rebuilds `pixels_` from scratch (the host then uploads again, as before). Not tested: a `.ttc`/CJK fallback face on the ZenApp default atlas (ZenApp does not add any; test_text_sizes has the collection test, headless, unchanged and passing).

Tests for this: `testTextIsDrawnWithTheAtlasPixelsReleased` (tests/test_retained_textbounds.cpp, software raster, pixel-for-pixel equal with and without the release), the new block in `tests/test_text_sizes.cpp` (size not baked, a codepoint outside the baked ranges, the dynamic page filling and restarting, a pixel-scale change, all after the release) and both applications of `tests/test_zen_screenshot_gl.cpp`.

## The new window test

`igui_zen_screenshot_gl` + `igui_zen_screenshot_gl_refuses_unproven`, through `igui_add_window_test` (tools/xvfb-test.sh, guard in tests/ThrowawayDisplay.hpp). On this machine's private Xvfb (llvmpipe, OpenGL 4.5 core, msaa 4 so the multisample resolve is exercised) it RAN and passed, in the Release and ASan builds. `App::create` now fails (so the test skips with 77 and a message) when the driver cannot complete the offscreen framebuffer (GlRaster retries single sample first); that skip path was not exercised (the driver here completes it).

## Verified (tools/check.sh at d3b325e, `IGUI_ZEN_PLATFORM_DIR=/media/projectos/projects/cpp/zen_plataform`, `--jobs 2`)

nostd PASS; release PASS 40/40; asan PASS 40/40 with 1 skipped (igui_zen_lifecycle, "no window here" under heavy machine load; rerun alone and through ctest on the ASan build it passes, and on main's ASan build too); hash PASS (draw checksums unchanged). `std::` count per touched file before/after equal for all 12 files.

FileDialog ASan question: `test_file_dialog_breadcrumbs_follow_the_path_root` is called from main() of `igui_original_widgets`; that program, built with ASan+UBSan, run from the repo root, exits 0 with no report on origin/main 1a77b1f (and igui_original_widgets passed in check.sh's ASan stage on this branch). Not reproduced: it does not crash on current main or on the tip.

## Atlas memory (measured, Release, Xvfb, software raster hidden window, 320x200)

Baked page: main 2048x1024 RGBA = 8388608 bytes held in RAM and again in the backend; branch 2048x512 = 4194304 bytes in the backend only (RAM copy released). Dynamic page 1024x1024 RGBA = 4 MB RAM + 4 MB texture, `defaultDynamicPage=false` drops both. Resident memory of one ZenApp after create (VmRSS delta from before create, scratch program, not in the repo): main 81.2 MB; branch with dynamic page 68.9 MB; branch with `defaultDynamicPage=false` 64.7 MB. With a real GL window (llvmpipe, which adds its own memory): main 130.1 MB, branch 117.8 MB, branch without dynamic page 114.0 MB.

## OWED: benches (not finished)

`igui_retained_bench`, main (build `main-b`) vs branch (`fonts-b`), Release, `taskset -c 3`, 5 alternating runs, min of runs, rows text/, text_width, /idle_frame, widget/label, bigtext/. Result: draw hash unchanged, but several rows moved more than 3% and consistently: `editor300/text_width_x100` 16.76 -> 20.73 us (+24%), `editor3000/text_width_x100` +21%, `bigtext/field100000_frame` +36%, `_type` +30%, `_key_left` +42%, `bigtext/line200000_key_left` +23%; the idle frames moved both ways (-8% to +8%) and `bigtext/doc20000_*` -3% to -11%; `widget/label_per_widget` -4%. The code that those rows run is not touched by the branch (crop and release are in `FontAtlas::build` and after the upload), so a code-layout effect is the likely cause, but it is NOT proven. A rebuild of both trees with `-falign-functions=64 -falign-loops=64` was being built and benchmarked when the machine went down. To finish: rebuild both with those flags (CMAKE_CXX_FLAGS and CMAKE_C_FLAGS), run `igui_retained_bench --only=<pattern>` (substring match) 5 times alternating, pinned, and compare the minima; if the text_width and field100000 rows still move more than 3% look at what runs there (perf, or objdump of Font::findGlyph / DrawList::calcTextSize) before merging.

## Next steps

1. Finish the aligned bench comparison above.
2. Push `fonts-gl`, review, and fast-forward `origin/main` to it.
3. Optional: a test where a driver without the offscreen framebuffer skips (needs a driver or a mock that fails `glCheckFramebufferStatus`).
4. API notes for callers: `FontAtlas::height()` of a baked page is now a power of two between 64 and 1024 (use `height()`, never 1024; UVs are relative to it); `releasePixels()` is opt-in (`#ifdef IGUI_FONTATLAS_RELEASE_PIXELS`); `ZenApp::Config::defaultDynamicPage` defaults to true (no change); `GlRaster::setOffscreen/prepareOffscreen` are new; `ZEN_SCREENSHOT_BACKEND=gl` makes `create()` fail without OpenGL or without a complete offscreen framebuffer.
