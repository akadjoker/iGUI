# fonts-gl: the commit 1d57c00 (zenhtml-fonts-gl) brought onto main 1a77b1f

Branch `fonts-gl`, based on `origin/main` 1a77b1f, fast-forwardable (origin/main is an ancestor). Commits:

1. `d95866b` the cherry-pick of 1d57c00 (original author kept), conflict resolved, see below.
2. `cf3db2a` tests that draw text with the baked atlas pixels released (headless).
3. `fb89898` GlRaster/ZenApp: the offscreen target of `ZEN_SCREENSHOT_BACKEND=gl` is made and checked in `App::create`.
4. `d3b325e` test_zen_screenshot_gl: two applications (baked text, and dynamic page with a size and a codepoint first used after the release).
5. `07e2a69` the first version of this note.
6. `eb5cfb8` RetainedBench: two comment-only notes for whoever compares two builds (nothing else in the file changed).
7. this note (the bench answer).

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

Again at `eb5cfb8` (the bench comments, `--jobs 3`, cores 6,7,10,11, `IGUI_CONTAINERS_DIR` containers 34169bc, `IGUI_ZEN_PLATFORM_DIR` zen_plataform db76a13): nostd PASS, release PASS 40/40, asan PASS 40/40 (no skipped test; the window tests ran in the private Xvfb, `igui_zen_screenshot_gl` and `igui_zen_lifecycle` among them), hash PASS; the first run at `07e2a69` gave the same four. This note is documentation only.

FileDialog ASan question: `test_file_dialog_breadcrumbs_follow_the_path_root` is called from main() of `igui_original_widgets`; that program, built with ASan+UBSan, run from the repo root, exits 0 with no report on origin/main 1a77b1f (and igui_original_widgets passed in check.sh's ASan stage on this branch). Not reproduced: it does not crash on current main or on the tip.

## Atlas memory (measured, Release, Xvfb, software raster hidden window, 320x200)

Baked page: main 2048x1024 RGBA = 8388608 bytes held in RAM and again in the backend; branch 2048x512 = 4194304 bytes in the backend only (RAM copy released). Dynamic page 1024x1024 RGBA = 4 MB RAM + 4 MB texture, `defaultDynamicPage=false` drops both. Resident memory of one ZenApp after create (VmRSS delta from before create, scratch program, not in the repo): main 81.2 MB; branch with dynamic page 68.9 MB; branch with `defaultDynamicPage=false` 64.7 MB. With a real GL window (llvmpipe, which adds its own memory): main 130.1 MB, branch 117.8 MB, branch without dynamic page 114.0 MB.

## Benches: main against the branch (answered)

Short answer: the `editor*/text_width_x100` and `bigtext/field100000_*` rows that the first note had at +21% to +42% did NOT reproduce: in three full tables (two plain, one aligned) they differ by at most 1.5% either way, and the code they run is byte-for-byte the same on both sides with the same instruction count. The only rows more than 3% slower on the branch are `softraster/cached_frame` (+5% to +8%: where glibc puts two 5.76 MB buffers, decided by its `mmap` threshold, which the smaller atlas page changes) and `loops/pianoroll_32_beats` (+2% to +4%: code placement, unchanged main swings 9% on it with a NOP pad). Nothing here is a code slowdown, so no code was changed; only two comment notes were added to the bench.

### How it was measured

- Same bench source on both sides (`git diff 1a77b1f eb5cfb8 -- bench` is the two comment notes only), Release (`-O3 -DNDEBUG`), `-ffile-prefix-map` so path strings are equal, dependencies `containers` 34169bc and `zen_plataform` db76a13. The rebuilt binaries are byte-identical to the benched ones (builds are deterministic), and the bench binary of the `tools/check.sh` Release tree has the same text size and the same function addresses as the one benched.
- The bench only links `src/`, `include/` and `SoftRaster.cpp`; after `d95866b` nothing in `src/`, `include/` or `bench/` changes (`git diff d95866b eb5cfb8 -- src include bench` is the comment notes), so the five commits reduce to one for the bench: `d95866b` (the crop in `FontAtlas::build` and the new, never called, `releasePixels`).
- `taskset -c 8` in a `systemd-run` scope with `MemoryMax=4G`, six runs per side alternating A B / B A, no display; the value of a row is the minimum over the runs (every run is already the best of 7 repetitions). Core 9 (the SMT sibling) and the other cores are checked idle for 2 s before each run (the first plain run was made without that check). The machine is shared and the runs have noise episodes of +60% to +100% on a whole run, on either side, so only the minima are used (the spread columns of the tables show it).
- "Aligned" is the same pair built with `-falign-functions=64 -falign-loops=64 -falign-jumps=64 -falign-labels=64` (the flags are in `CMAKE_CXX_FLAGS` and `CMAKE_C_FLAGS`).

### The rows over 3% (tip against main, minima, microseconds per iteration)

| row | main | tip | delta | which table |
|---|---|---|---|---|
| softraster/cached_frame | 1160.8 | 1221.3 | +5.2% | plain, run 1 |
| softraster/cached_frame | 1127.7 | 1186.7 | +5.2% | plain, run 2 |
| softraster/cached_frame | 1116.1 | 1205.3 | +8.0% | aligned |
| loops/pianoroll_32_beats | 17.04 | 17.43 | +2.3% | plain, run 2 (run 1: 16.98 -> 17.36, +2.2%) |
| loops/pianoroll_32_beats | 17.23 | 17.99 | +4.4% | aligned (every run: main 17.23-17.34, tip 17.99-18.50) |
| editor300/layout_only | 10.52 | 10.88 | +3.4% | plain, run 2 only (run 1: 10.95 against 10.94; the same main binary was 10.95 in run 1 and 10.5-10.6 in run 2) |
| widget/listbox_per_widget | 0.29 | 0.30 | +3.4% | aligned only: one digit of the 0.01 us the bench prints (plain: 0.28 and 0.28) |
| bigtext/doc20000_type | 114.6 | 104.7 | -8.6% | plain, run 1; -8.7% in run 2 (tip faster, deterministic; not investigated) |
| editor3000/clip_drag_frame, timeline300/idle_frame_all_visible | | | -3.1%, -3.2% | plain, run 1 (tip faster) |
| timeline300/idle_frame, timeline300/idle_frame_all_visible, timeline3000/idle_frame, timeline3000/idle_frame_all_visible, widget/statusbar_per_widget, datagrid_short/paint_only, datagrid_short/scroll_frame | | | -3.0% to -6.7% | aligned (tip faster; the main side of these has runs 60-100% over its minimum) |

The rows the first note listed (rows `text`, `text_width`, `idle_frame`, `widget/label`, `bigtext/`), tip against main:

| row | plain, run 1 | plain, run 2 | aligned |
|---|---|---|---|
| editor300/text_width_x100 | 16.09 -> 16.14 (+0.3%) | 16.11 -> 16.12 (+0.1%) | 16.82 -> 16.82 (0.0%) |
| editor3000/text_width_x100 | 16.08 -> 16.03 (-0.3%) | 16.10 -> 16.02 (-0.5%) | 16.93 -> 16.96 (+0.2%) |
| editor300/label_sizehints | +0.4% | -0.4% | +0.7% |
| bigtext/field100000_frame | 3301.8 -> 3308.8 (+0.2%) | 3301.1 -> 3307.4 (+0.2%) | 3346.3 -> 3355.3 (+0.3%) |
| bigtext/field100000_type | +0.5% | +0.5% | 0.0% |
| bigtext/field100000_key_left | 0.0% | +0.4% | +0.2% |
| bigtext/line200000_key_left | +0.9% | +1.1% | +0.3% |
| bigtext/doc20000_frame, _key_down | -0.5%, -0.4% | not over 1% | not over 1% |
| editor300/idle_frame, editor3000/idle_frame | -1.6%, -1.5% | within 1.5% | within 1.5% |
| text/elided_8000_chars_frame | 0.0% | within 1% | within 1% |
| widget/label_per_widget | 0.0% | 0.0% | within 1% |

Only 3 to 10 of the 117 rows of each table are over 3% in either direction. The tables are in the scratch folder of this session, not in the repository.

### What decided each row

1. The code of the text paths is byte-for-byte the same. `objdump` of every function of both binaries (3507 lines of listing, names kept, addresses of calls, jumps and `%rip` displacements normalised): the functions whose code differs are `FontAtlas::build` (the crop) and `ct::Vector<unsigned int>::grow` (GCC inlined `HeapAlloc::reallocate` into it on main and outlined it on the branch: 61 against 52 instructions on the growth path, which no measured loop runs more than once; `stbi_zlib_compress.cold` differs only in padding); `FontAtlas::releasePixels` is new. `Font::findGlyph`, `DrawList::calcTextSize`, the `ct::HashMap` find/index functions, `WidgetApp::textWidth`, `TextInput::cursorXOffset/hitTestChar` and every `SoftRaster` function are equal. `findGlyph` and `calcTextSize` (before `build` in `Retained.o`) moved by exactly +0x40 (a whole cache line: their offset in a 64-byte line is the same); what is linked after `build` moved by +0x2d0 (720 bytes, 16 mod 64); `SoftRaster` did not move.
2. Same number of instructions. `perf` is not usable here (`perf_event_paranoid` is 4, and the counters need root), so there are no cache-miss or branch-miss counts. Instead a ptrace single-stepper counted the instructions of `h.app.textWidth("Parâmetro número 7 da cena")` x100 in the editor scene of the bench (300 clips): 282201 instructions on main and 282201 on the branch (second pass 282202 and 282202). With equal code and equal instruction counts a time difference could only come from cache or placement, and there was none.
3. Main alone moves as much as any difference seen. 400 bytes of NOP (and 0, 16, 32, 48, 64, 80, 128, 256, 720) appended to `BasicWidgets.o`, an object that has nothing to do with the rows, on a copy of main (the bench of that copy has a few diagnostic lines in the softraster scene, which by themselves move the code by 0xe0):

| NOP bytes | text_width_x100 | field100000_frame | pianoroll_32_beats |
|---|---|---|---|
| 0 | 16.34 | 3266 | 17.61 |
| 16 | 16.16 | 3367 | 17.22 |
| 32 | 16.22 | 3311 | 17.27 |
| 48 | 16.06 | 3310 | 17.45 |
| 64 | 15.97 | 3257 | 17.58 |
| 80 | 16.17 | 3348 | 17.26 |
| 128 | 16.11 | 3265 | 16.91 |
| 256 | 16.12 | 3262 | 16.22 |
| 400 | 16.19 | 3446 | 16.10 |
| 720 | 16.22 | 3340 | 16.15 |
| main, plain build | 16.04 | 3310 | 17.30 |
| branch, plain build | 16.18 | 3310 | 17.49 |

   Minimum of 2 runs (3 for pianoroll). `text_width_x100` spans 2.3% over the pads (the first note's +24% is outside anything seen), `field100000_frame` 5.8%, and `loops/pianoroll_32_beats` 9.4% (16.10 to 17.61): bigger than the +2% to +4% the branch shows, so that row is placement. The `editor*/text_width_x100` swing of +10% to +17% that `igui-round4` found with a pad was not reached by these pads on this tree either: that row's sensitivity depends on the build, which is why it is not used as a verdict here.
4. `softraster/cached_frame` is the glibc `mmap` threshold. The row is a hash of the draw data and a copy of 5.76 MB (1600x900) from the rasteriser's cache to the bench's `pixels`; the code is the same bytes at the same addresses on both sides. The pages the first scene's atlas allocates are 2 MB (a temporary), 8 MB (the baked page, 2048x1024) and 4 MB (the dynamic page) on main and 2, 4 and 4 MB on the branch (the crop). Freeing an `mmap`ed block makes glibc raise its threshold to the block's size (up to 32 MB), so once the earlier scenes have freed their atlases the threshold is at least 8 MB on main and 4 MB on the branch. The addresses of the two 5.76 MB buffers show the consequence (traced with a diagnostic in the allocation wrapper, not in the repository): on main they are on the `brk` heap next to the binary, on the branch they are `mmap` chunks. Forcing the same policy on both with `GLIBC_TUNABLES` removes the difference (minimum of 6 alternating runs, microseconds):

| policy | main | branch | delta |
|---|---|---|---|
| default | 1163.8 | 1224.6 | +5.2% |
| `glibc.malloc.mmap_threshold=1048576` (both `mmap`) | 1190.3 | 1195.2 | +0.4% |
| `glibc.malloc.mmap_threshold=33554432` (both heap) | 1123.7 | 1138.8 | +1.3% |

   (A standalone `memcpy` of the same shape, 900 rows of 6400 bytes between two `mmap` buffers, moves 14% with the offset between the two buffers: 873 us at an offset of 16, 763-780 us for the others; that shows the sensitivity, not the exact mechanism, which was not isolated. Transparent huge pages are `madvise` only here, so they are not it.) In a real `ZenApp` nothing frees an 8 MB block on main (only the 2 MB temporary), and on the branch `releasePixels` frees the 4 MB baked copy, so there the threshold is the same or higher on the branch, not lower (reasoned from the code, not measured).
5. The dependencies are not the explanation of the first note either. The same pair built against the live checkouts (`containers` 35f69b5, `zen_plataform` beb1db0, a different code layout): `text_width_x100` 16.04 -> 15.93, `field100000_frame` 3303 -> 3249, `_type` 3790 -> 3687, `_key_left` 3779 -> 3672 (minimum of 4 runs): the branch is equal or up to 3% faster.

### Verdict

- `editor*/text_width_x100`, `bigtext/field100000_*`, `bigtext/line200000_key_left`, `label_sizehints`, idle frames: no difference (|delta| at most 1.5%). Sure about this on this machine, in plain, aligned and live-dependency builds, with equal function bytes and equal instruction counts. The +21% to +42% of the first note I could not reproduce and could not explain: my belief is a measurement artefact of that session (the note says the machine went down, and it benched on core 3, the SMT sibling of a core another agent benches on), not a property of the branch; I did not reproduce the conditions.
- `softraster/cached_frame`: not a slowdown of code, a different place for two 5.76 MB buffers (sure about the placement and that forcing the same one removes the difference; the micro mechanism of why `mmap` chunks copy 4% to 5% slower here is not isolated).
- `loops/pianoroll_32_beats`: placement (sure: unchanged main moves 9% with a pad, the functions it runs are equal).
- `editor300/layout_only`, `widget/listbox_per_widget`: noise of the session / quantisation of the printed digit.
- The memory saving is untouched (atlas 81.2 -> 68.9 MB above).

### For the next A/B on this bench

Six or more alternating runs per side on one core with the machine quiet, minima, plus the aligned pair, plus `objdump` equality of the functions the row runs; a row that moves with equal functions is placement until a pad sweep on the old side says otherwise. `softraster/cached_frame` needs `GLIBC_TUNABLES=glibc.malloc.mmap_threshold=33554432` on both sides. These notes are in the header of `bench/retained/RetainedBench.cpp` (`eb5cfb8`). Not done: PMU counters (not available), a build with only the crop and one with only `releasePixels` (not needed: the code of the measured paths is equal in both).

## Next steps

1. Push `fonts-gl`, review, and fast-forward `origin/main` to it.
2. Optional: a test where a driver without the offscreen framebuffer skips (needs a driver or a mock that fails `glCheckFramebufferStatus`).
3. API notes for callers: `FontAtlas::height()` of a baked page is now a power of two between 64 and 1024 (use `height()`, never 1024; UVs are relative to it); `releasePixels()` is opt-in (`#ifdef IGUI_FONTATLAS_RELEASE_PIXELS`); `ZenApp::Config::defaultDynamicPage` defaults to true (no change); `GlRaster::setOffscreen/prepareOffscreen` are new; `ZEN_SCREENSHOT_BACKEND=gl` makes `create()` fail without OpenGL or without a complete offscreen framebuffer.
