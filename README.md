# iGUI

A C++14 GUI toolkit built around a single backend-agnostic drawing engine, with two independent widget APIs on top of it:

- **Immediate mode** (`ig::Context`): widgets are function calls made every frame, and no widget objects outlive the frame.
- **Retained mode** (`ig::retained`): widgets are long-lived objects in a tree, driven by events and signals.

The library never talks to a window system or a GPU directly. It produces a list of draw commands per frame (`ig::DrawData`), and a small `ig::Backend` implementation renders them. Adapters for **SDL2** and **Raylib** are included.

## Features

- **Backend-agnostic rendering.** Widgets emit clipped, textured triangle lists; a backend only has to measure text and render `DrawData`.
- **Two widget APIs, one engine.** Pick immediate mode, retained mode, or link both in the same program.
- **Large retained widget set:** layouts, scroll areas, text inputs, combo boxes, menus, dialogs, tree/property/color widgets, data views, charts, node editor, timeline, audio widgets (e.g. piano roll), thumbnail grid, file dialog, console, dock panels, asset browser, gizmos, automotive-style gauges, and a widget serializer.
- **Built-in code editor** with lexical syntax highlighting (`ig::syntax`), available in both modes.
- **Immediate-mode core without the STL.** `igui_core` and the immediate-mode library use the bundled `ct` containers; a build-time check (`cmake/CheckNoStd.cmake`) fails the build if `std::` or STL container headers appear in that code.
- **UTF-8 text**, DPI scaling, and clipboard hooks.
- **Embedded default font** (Roboto, or any TTF/OTF you provide).

## Requirements

| Component | Requirement |
| --- | --- |
| Compiler | C++14 (GCC, Clang, or MSVC) |
| Build system | CMake 3.14 or newer (Ninja recommended) |
| Submodule | [`containers`](https://github.com/akadjoker/containers) in `third_party/containers` |
| SDL2 backend (optional) | SDL2 development files, found through the `SDL2::SDL2` target or `pkg-config` |
| Raylib backend (optional) | A system-installed Raylib (header and static library) |

The embedded font is fetched at configure time when `IGUI_BUILD_FONT` is on and no `IGUI_FONT_INPUT` is given, so that configuration needs network access.

## Getting the source

The `containers` dependency is a git submodule, so clone recursively:

```sh
git clone --recurse-submodules https://github.com/akadjoker/iGUI.git
```

If you already cloned without it:

```sh
git submodule update --init --recursive
```

The submodule tracks the `main` branch of `containers`. To move it to the latest `main`:

```sh
git submodule update --init --remote --recursive
```

CI always builds against the latest `containers` `main`, and Dependabot opens a pull request daily when the recorded submodule commit falls behind.

CMake stops with a clear error if `third_party/containers` is empty.

## Building

Immediate-mode library, retained widget toolkit, legacy layer, and tests (the defaults):

```sh
cmake -S . -B build -G Ninja -DIGUI_BUILD_RAYLIB_BACKEND=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

With the SDL2 demos:

```sh
cmake -S . -B build -G Ninja -DIGUI_BUILD_SDL2_DEMO=ON -DIGUI_BUILD_RAYLIB_BACKEND=OFF
cmake --build build --parallel
./build/igui_sdl2_demo
```

### CMake options

| Option | Default | Description |
| --- | --- | --- |
| `IGUI_BUILD_IMMEDIATE` | `ON` | Build the immediate-mode library (`igui::igui`) |
| `IGUI_BUILD_RETAINED` | `ON` | Build the retained widget toolkit (`igui::widgets`) |
| `IGUI_BUILD_LEGACY` | `ON` | Build the older retained compatibility layer (`igui::legacy`) |
| `IGUI_BUILD_TESTS` | `ON` | Build and register the test executables |
| `IGUI_BUILD_FONT` | `OFF` | Build the embedded font atlas module (`igui::font`); forced on when a backend is built |
| `IGUI_FONT_INPUT` | empty | TTF/OTF file to embed as the default font (defaults to Roboto Regular) |
| `IGUI_BUILD_SDL2_DEMO` | `OFF` | Build the SDL2 backend (`igui::sdl2`), its demos, and its tests |
| `IGUI_BUILD_RAYLIB_BACKEND` | `ON` | Build the Raylib backend (`igui::raylib`) if Raylib is found |
| `IGUI_BUILD_RAYLIB_DEMO` | `OFF` | Build the Raylib demos |
| `IGUI_BUILD_CODE_EDITOR_DEMO` | `OFF` | Build the immediate-mode code editor demo (requires Raylib) |

## Using iGUI in your project

Add the repository as a subdirectory and link the targets you need:

```cmake
add_subdirectory(iGUI)

target_link_libraries(app PRIVATE igui::igui)                 # immediate mode
target_link_libraries(app PRIVATE igui::widgets)              # retained mode
target_link_libraries(app PRIVATE igui::igui igui::widgets)   # both
```

| Target | Purpose |
| --- | --- |
| `igui::core` | Shared drawing engine: `DrawList`, `DrawData`, `Backend`, colors, math, syntax highlighting |
| `igui::igui` | Immediate-mode `ig::Context` |
| `igui::widgets` | Retained-mode widget toolkit (`ig::retained`) |
| `igui::legacy` | Older retained GUI API, kept for compatibility |
| `igui::font` | Embedded default font and font atlas |
| `igui::sdl2` | SDL2 backend and event translation |
| `igui::raylib` | Raylib backend |

Linking `igui::igui` or `igui::widgets` defines the matching mode switch for you, so you can simply include the umbrella header:

```cpp
#include <igui/All.hpp>
```

### Choosing a mode

The switches `IGUI_IMMEDIATEMODE` and `IGUI_RETAINEDMODE` (each `0` or `1`) control which API `<igui/All.hpp>` exposes. Defining neither exposes everything that was built.

When both are enabled, `IGUI_BOTH_MODES` is set. The types `Theme`, `GradientStop`, and `TimelineTrack` exist in both APIs with different meanings, so the retained API stays inside `ig::retained` instead of being hoisted into `ig::`:

```cpp
ig::Context ctx;                 // immediate
ig::retained::Button button;     // retained
```

See [include/igui/Modes.hpp](include/igui/Modes.hpp) for the full rules.

## Quick start (immediate mode with SDL2)

```cpp
#include <SDL.h>
#include <igui/Gui.hpp>
#include <igui_sdl2/SdlBackend.hpp>

int main(int, char **)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("iGUI", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 960, 640,
                                          SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    ig::sdl2::Backend backend(renderer);
    backend.prepareFontAtlas();
    ig::Context ui(backend, &backend.fontAtlas());

    bool running = true, enabled = false;
    float volume = 0.5f;
    uint64_t previous = SDL_GetPerformanceCounter();

    while (running)
    {
        uint64_t now = SDL_GetPerformanceCounter();
        float dt = float(now - previous) / float(SDL_GetPerformanceFrequency());
        previous = now;

        SDL_Event native;
        while (SDL_PollEvent(&native))
        {
            if (native.type == SDL_QUIT)
                running = false;
            ig::Event event;
            if (ig::sdl2::translateEvent(native, event))
                ui.pushEvent(event);
        }

        ui.beginFrame(ig::sdl2::frameInfo(window, dt));
        if (ui.beginWindow("Hello", ig::Rect(80, 60, 320, 200)))
        {
            if (ui.button("Toggle"))
                enabled = !enabled;
            ui.sameLine();
            ui.checkbox("Enabled", enabled);
            ui.sliderFloat("Volume", volume, 0.0f, 1.0f);
            ui.endWindow();
        }
        const ig::DrawData &draw = ui.endFrame();

        SDL_SetRenderDrawColor(renderer, 32, 34, 40, 255);
        SDL_RenderClear(renderer);
        backend.render(draw);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
```

A complete version that exercises most immediate-mode widgets is in [examples/sdl2_demo/main.cpp](examples/sdl2_demo/main.cpp).

## Writing a backend

A backend implements `ig::Backend` ([include/igui/Backend.hpp](include/igui/Backend.hpp)):

```cpp
class Backend
{
public:
    virtual TextMetrics measureText(FontId font, StringView text,
                                    float logicalSize, float dpiScale) = 0;
    virtual bool render(const DrawData &data) = 0;          // must honour each command's clip rect
    virtual String clipboardText();                         // optional
    virtual bool setClipboardText(StringView);              // optional
};
```

The SDL2 ([adapters/sdl2](adapters/sdl2)) and Raylib ([adapters/raylib](adapters/raylib)) adapters are working references.

## Examples

| Example | Target | Description |
| --- | --- | --- |
| [examples/sdl2_demo](examples/sdl2_demo) | `igui_sdl2_demo` | Immediate-mode widget gallery |
| [examples/sdl2_original_widgets](examples/sdl2_original_widgets) | `igui_sdl2_original_widgets_demo` | Retained-mode showcase, organized in stages (basic, controls, scroll, inputs, menus, dialogs, dock, properties) |
| [examples/sdl2_legacy_gui_demo](examples/sdl2_legacy_gui_demo) | `igui_sdl2_legacy_gui_demo` | Legacy compatibility layer (supports `--smoke` for headless checks) |
| [examples/raylib_demo](examples/raylib_demo) | `igui_raylib_demo` | Immediate-mode demo on Raylib |
| [examples/raylib_dock_demo](examples/raylib_dock_demo) | `igui_raylib_dock_demo` | Docking layout on Raylib |
| [examples/code_editor_demo](examples/code_editor_demo) | `igui_code_editor_demo` | Immediate-mode code editor |

The retained SDL2 demo is built with AddressSanitizer enabled on GCC and Clang.

### Web gallery

Both widget APIs also build to WebAssembly with Emscripten. The galleries run in the browser on SDL2's Emscripten port and are published to GitHub Pages by `.github/workflows/pages.yml` on every push to `main`.

| Page | Target | Content |
| --- | --- | --- |
| `immediate.html` | `igui_web_immediate` | Immediate-mode widgets in ten sections: basic, inputs, lists and tables, layout, windows and dialogs, menus and submenus, icon buttons, a synth voice with knobs and a mixer desk of faders, a 3D transform gizmo that draws the object it moves, code editor |
| `retained.html` | `igui_web_retained` | The retained-mode stages - basic widgets, controls, gadgets, galleries, gizmos (2D and the same 3D modes), toolbars, dock panels, node editor, timeline and file dialog |
| `index.html` | | Page with a tab for each gallery (`web/index.html`) |

To build locally, activate an Emscripten SDK and run:

```sh
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DIGUI_BUILD_WEB=ON -DIGUI_BUILD_TESTS=OFF -DIGUI_BUILD_LEGACY=OFF -DIGUI_BUILD_RAYLIB_BACKEND=OFF
cmake --build build-web --target igui_web_immediate igui_web_retained
cp web/index.html build-web/
python3 -m http.server -d build-web 8000
```

The file dialog in the retained gallery browses the in-browser virtual filesystem, not the visitor's disk.

## Project layout

```
include/igui/            Public headers (core, immediate mode, legacy)
include/igui/widgets/    Retained widget toolkit headers
src/                     Core, immediate-mode, and legacy sources
src/widgets/             Retained widget toolkit sources
adapters/sdl2/           SDL2 backend
adapters/raylib/         Raylib backend
examples/                Demo applications
tests/                   Test executables (run through CTest)
third_party/             containers (submodule) and stb headers
cmake/                   Build helper scripts
```

## Testing

```sh
ctest --test-dir build --output-on-failure
```

Registered tests: `igui_core`, `igui_widget_gallery`, `igui_both_modes`, `igui_original_widgets`, plus `igui_font` and `igui_sdl2` when those modules are built. Continuous integration builds and tests both the immediate-mode and retained configurations on every push and pull request.

## Third-party components

- [containers](https://github.com/akadjoker/containers) (`ct`): container library used by the core and immediate-mode code (git submodule).
- [stb](https://github.com/nothings/stb): `stb_image`, `stb_image_write`, `stb_image_resize2`, `stb_rect_pack`, `stb_truetype`, bundled in `third_party/stb`.
- Roboto Regular: default embedded font, or your own via `IGUI_FONT_INPUT`.

## License

Released under the [MIT License](LICENSE).
