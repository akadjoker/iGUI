# Proposal: the OpenGL path skips frames that did not change

Branch `proposal/gl-skip-unchanged` (not merged). Commits:

- `be8133d` zen adapter: the OpenGL path neither draws nor swaps a frame whose draw data is the same as the last one drawn in that window. The signature is shared with SoftRaster (`FrameSignature.hpp`, the same FNV hashing, moved). Every GL texture has a version, bumped on create and on every update (video frames, the dynamic font page). A texture GlRaster did not make is never skipped. `readWindowPixels` after a skipped frame draws the last draw data again to read it, without a swap. `Config::skipUnchangedGl` (default true), `glFramesDrawn()`, `glFramesSkipped()`, `invalidateGl()`. Size, scale, minimise / maximise / restore, focus, move and surface events force a draw (the platform has no expose event).
- `69e9cd2` a screenshot run (`ZEN_SCREENSHOT`, also the hidden window of a driven run) draws only the frame it saves; a skipped frame still counts towards that frame number. Without it nothing was skipped in such runs.

## Why

zenWebView draws with OpenGL by default. On a machine without a GPU (Xvfb, Mesa llvmpipe) an idle page redrew the whole window 60 times a second. The software path already skipped rasterising an unchanged frame.

## Numbers on a real machine (X11 :1, Intel Arc (Meteor Lake) iGPU, Mesa, 2026-11-26)

Same zenWebView source built twice, against this branch with `skipUnchangedGl` true and false; CPU time of the process in a window.

| Case | Skip off | Skip on, byte-at-a-time signature (be8133d) | Skip on, 32-byte-step signature (this branch) |
|---|---|---|---|
| frozen github.com page, 20 s | 5.6 to 6.0 s | 10.4 to 10.7 s (twice the CPU) | 5.6 to 5.9 s |
| idle page with a requestAnimationFrame loop, 10 s | 0.52 s | 0.48 s | not re-measured |

On a GPU, drawing an unchanged frame is cheap: the skip saves little CPU there, and the byte-at-a-time FNV over every vertex of a large page cost more than the draw. The signature now hashes 32 bytes a step in four 64-bit lanes (`FrameSignature.hpp`, shared with SoftRaster). The large gain is on machines without a GPU (below).

## Numbers without a GPU (Xvfb :99, Mesa llvmpipe, 2026-11-26)

| Case | Before | After |
|---|---|---|
| Minimal iGUI window at 60 fps, 1280x720, 10 s | 28.94 s CPU (2.89 cores), 600 frames drawn | 0.11 s CPU (0.011 cores), 1 drawn, 599 skipped |
| Same window, software path (reference) | 0.67 s CPU (0.067 cores) | unchanged |
| zenWebView `--drive`, idle page with a requestAnimationFrame loop, 6 s | 33.5 s CPU, 339 drawn, 0 skipped | 0.74 s CPU, 3 drawn, 341 skipped |
| zenWebView `zwv_pace` (share of a core of an idle page) | 2.67 (FAIL) | PASS |

## Tests

iGUI (Release, on :99): `igui_zen_gl_skip_tests` ok (unchanged UI skips and the after-render callback still runs; a changed label draws; a texture updated in place draws with the new pixels; `readWindowPixels` after skipped frames returns the last drawn picture; a pending capture draws; `invalidateGl()` and a resize draw; a second window keeps its own signature; `skipUnchangedGl = false` skips none), `igui_zen_screenshot_gl_tests` ok, `igui_zen_lifecycle_tests` ok, `igui_zen_sidetabs` ok; ctest 48 / 48 (window tests skipped there without xvfb-run).

zenWebView built against this branch (`-DZWV_IGUI_DIR=` the branch), OpenGL, on :99:
- `zwv_pace`, `zwv_hold`, `zwv_window`, `zwv_input`, `zwv_delay`, `zwv_drive`, `zwv_frame_click`, `zwv_frame_remove`, `zwv_frame_style`: 9 / 9 passed.
- Suites that compare screenshots or drive the window: `zwv_geo`, `zwv_examples`, `zwv_media`, `zwv_apps` (18 app runs vs Chrome) passed. `zwv_pages`: 129 passed, 2 failed (`w5_domparser`, `w5_history`): their expected output holds the file path of the main checkout and differs in any worktree; that test is headless (no iGUI window), not related to this patch.
- Not run against this branch: `zwv_api` (fails on zenWebView main already: RSS over 900 reloads), `zwv_net`, `zwv_mcp`, `zwv_pool`.

## Limits

- An application that draws into a texture it made through `App` without calling `updateTexture` must call `invalidateGl()` (or set `skipUnchangedGl = false`), otherwise its picture can be stale.
- The signature still reads every vertex of every frame (now 32 bytes a step). A dirty flag from the retained tree would avoid even that; not done.
