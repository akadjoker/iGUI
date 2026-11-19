cmake_minimum_required(VERSION 3.14)
project(igui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(IGUI_NO_RTTI "Compile every iGUI target with -fno-rtti (ignored on MSVC)" ON)

if(IGUI_NO_RTTI AND NOT MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
endif()

option(IGUI_BUILD_TESTS "Build iGUI core tests" ON)
option(IGUI_FAIL_ON_SANITIZER_REPORT "A test whose output has a sanitizer report fails even when it exits with 0 (ctest hides the output of a test that passed, so a warning that does not stop the program is never seen; tools/check.sh turns it on for its sanitizer stage)" OFF)
option(IGUI_BUILD_FONT "Build the optional embedded font atlas module" OFF)
set(IGUI_FONT_INPUT "" CACHE FILEPATH
    "Optional TTF/OTF file embedded as iGUI's default font (defaults to Nuklear's Roboto)")
option(IGUI_BUILD_SDL2_DEMO "Build the optional SDL2 integration example" OFF)
option(IGUI_BUILD_WEB "Build the WebAssembly galleries (Emscripten only)" OFF)
option(IGUI_BUILD_RAYLIB_BACKEND "Build the Raylib backend when Raylib is installed" ON)
option(IGUI_BUILD_RAYLIB_DEMO "Build the Raylib integration example" OFF)
option(IGUI_BUILD_CODE_EDITOR_DEMO "Build the immediate-mode code editor demo (requires Raylib)" OFF)
option(IGUI_BUILD_MIXER_DEMO "Build the immediate-mode mixer/knob demo (requires Raylib)" OFF)
option(IGUI_BUILD_SYNTH_DEMO "Build the immediate-mode synth/audio widgets demo (requires Raylib)" OFF)
option(IGUI_BUILD_ZENBEATS_DEMO "Build the Zen Beats port-test demo (requires Raylib)" OFF)
option(IGUI_BUILD_DIALOGS_DEMO "Build the file/folder/settings dialogs demo (requires Raylib)" OFF)
option(IGUI_BUILD_IMMEDIATE "Build the immediate-mode Context API (igui)" ON)
option(IGUI_BUILD_RETAINED "Build the retained widget toolkit (igui_widgets)" ON)
option(IGUI_BUILD_LEGACY "Build the old retained GUI compatibility layer (igui_legacy)" ON)
option(IGUI_BUILD_BENCH "Build the headless iGUI-vs-ImGui comparison benchmark" OFF)
set(IGUI_BENCH_IMGUI_DIR "" CACHE PATH
    "ImGui source tree (imgui.h/imgui.cpp) the benchmark compares iGUI against")
option(IGUI_BUILD_OPENGL_BACKEND "Build the OpenGL 3.3 core backend (igui_opengl)" OFF)
option(IGUI_BUILD_COMPARE_GALLERY "Build the SDL2+OpenGL side-by-side widget gallery (iGUI vs ImGui)" OFF)

set(IGUI_RAYLIB_AVAILABLE OFF)
if(IGUI_BUILD_RAYLIB_BACKEND)
    # Raylib is deliberately consumed from the system installation: the header
    # and static archive are not vendored into this repository.
    find_path(IGUI_RAYLIB_INCLUDE_DIR NAMES raylib.h HINTS /usr/local/include)
    find_library(IGUI_RAYLIB_LIBRARY NAMES raylib libraylib HINTS /usr/local/lib)
    if(IGUI_RAYLIB_INCLUDE_DIR AND IGUI_RAYLIB_LIBRARY)
        set(IGUI_RAYLIB_AVAILABLE ON)
    else()
        message(STATUS "Raylib header or library not found; igui::raylib is disabled")
    endif()
endif()

if(IGUI_BUILD_SDL2_DEMO OR IGUI_BUILD_WEB OR IGUI_RAYLIB_AVAILABLE)
    set(IGUI_BUILD_FONT ON)
endif()

# containers (ct) is vendored as a git submodule under third_party/containers
# rather than fetched by tag, so it tracks upstream via `git submodule update`
# and includes newer headers (e.g. ct/regex.hpp) as soon as the submodule
# pointer is bumped.
# A parent project that vendors containers itself points IGUI_CONTAINERS_DIR at
# its copy, so only one set of ct headers is in play.
set(IGUI_CONTAINERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/containers" CACHE PATH
    "Path to the containers (ct) checkout")
if(NOT EXISTS "${IGUI_CONTAINERS_DIR}/include/ct/vector.hpp")
    message(FATAL_ERROR
        "${IGUI_CONTAINERS_DIR} is empty. Run: git submodule update --init --recursive")
endif()

if(NOT TARGET ct)
    add_library(ct INTERFACE)
    target_include_directories(ct INTERFACE "${IGUI_CONTAINERS_DIR}/include")
endif()

# Shared drawing engine: DrawList/DrawData/Backend, plus the lexical syntax
# highlighting layer (ig::syntax). Both are things the immediate-mode Context
# and the retained widget toolkit actually have in common - everything else
# below depends on this, never on each other.
add_library(igui_core src/DrawList.cpp src/Syntax.cpp src/Symbols.cpp)
add_library(igui::core ALIAS igui_core)
target_include_directories(igui_core PUBLIC include)
target_link_libraries(igui_core PUBLIC ct)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(igui_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# O core imediato não usa a STL (usa os contentores `ct`, ver o README): este
# target não constrói nada, corre o CheckNoStd.cmake, que lê os ficheiros do core e
# falha a build se lá aparecer `std::` ou um contentor/algoritmo da STL. O toolkit
# retido e a camada antiga iGUI.hpp estão fora do contrato (ver o script).
add_custom_target(igui_check_no_std ALL
    COMMAND ${CMAKE_COMMAND}
            -DIGUI_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckNoStd.cmake
    VERBATIM
)
add_dependencies(igui_core igui_check_no_std)

if(IGUI_BUILD_IMMEDIATE)
    add_library(igui src/FileDialog.cpp src/Gui.cpp src/CodeEditorImmediate.cpp)
    add_library(igui::igui ALIAS igui)
    target_include_directories(igui PUBLIC include)
    target_link_libraries(igui PUBLIC igui_core)
    # PUBLIC so consumers get the switch just by linking: <igui/All.hpp> then
    # resolves to the immediate API with no #define on their side. A target that
    # links both igui and igui_widgets receives both switches, which is exactly
    # the IGUI_BOTH_MODES case (see include/igui/Modes.hpp).
    target_compile_definitions(igui PUBLIC IGUI_IMMEDIATEMODE=1)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(igui PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# The pre-existing retained GUI API is kept as a separate compatibility layer.
# It uses STL containers, unlike igui_core/igui above.
if(IGUI_BUILD_LEGACY)
    add_library(igui_legacy src/iGUI.cpp)
    add_library(igui::legacy ALIAS igui_legacy)
    target_include_directories(igui_legacy PUBLIC include)
    target_link_libraries(igui_legacy PUBLIC igui_core)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(igui_legacy PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(IGUI_BUILD_TESTS)
    enable_testing()
    # test_core.cpp exercises both the legacy compat API and ig::Context
    # directly in the same binary - it needs both modes, not just legacy
    # (it only linked before because igui_legacy used to publicly depend on
    # igui; that link is now igui_legacy -> igui_core only, see above).
    # The symbol scan of ig::syntax (outline, go-to-symbol): one sample per
    # language family.
    add_executable(igui_symbols_tests tests/test_symbols.cpp)
    target_link_libraries(igui_symbols_tests PRIVATE igui_core)
    add_test(NAME igui_symbols COMMAND igui_symbols_tests)

    if(IGUI_BUILD_LEGACY AND IGUI_BUILD_IMMEDIATE)
        add_executable(igui_tests tests/test_core.cpp)
        target_link_libraries(igui_tests PRIVATE igui_legacy igui)
        add_test(NAME igui_core COMMAND igui_tests)
    endif()

    # Os testes são feitos de assert(): num build com NDEBUG o assert desaparece,
    # o corpo de cada teste deixa de correr e um ficheiro inteiro passa a não
    # testar nada (ou rebenta onde faz .back() numa lista que ficou vazia). Tira-se
    # o NDEBUG a estes alvos, seja qual for o tipo de build - um teste que não
    # corre é pior do que um que falha.
    set(IGUI_TEST_TARGETS
        igui_tests
        igui_widget_gallery_tests
        igui_interaction_harness_tests
        igui_modal_input_tests
        igui_culling_tests
        igui_fuzz_tests
        igui_numeric_input_tests
        igui_text_editing_tests
        igui_windows_tests
        igui_code_editor_tests
        igui_symbols_tests
        igui_code_outline_tests
        igui_code_editor_keys_tests
        igui_chart_tests
        igui_themes_tests
        igui_text_sizes_tests
        igui_both_modes_tests
        igui_original_widgets_tests
        igui_font_tests
        igui_item_status_tests
        igui_zen_sidetabs_tests
        igui_sdl2_tests)
    function(igui_test_keep_asserts target)
        if(NOT TARGET ${target})
            return()
        endif()
        if(MSVC)
            target_compile_options(${target} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${target} PRIVATE -UNDEBUG)
        endif()
    endfunction()

    if(IGUI_BUILD_IMMEDIATE)
        add_executable(igui_widget_gallery_tests tests/test_widgets.cpp)
        # The gallery test drives the same synth knob the web gallery shows.
        target_include_directories(igui_widget_gallery_tests PRIVATE examples/synth_demo)
        target_link_libraries(igui_widget_gallery_tests PRIVATE igui)
        add_test(NAME igui_widget_gallery COMMAND igui_widget_gallery_tests)

        add_executable(igui_item_status_tests tests/test_item_status.cpp)
        target_link_libraries(igui_item_status_tests PRIVATE igui)
        add_test(NAME igui_item_status COMMAND igui_item_status_tests)

        add_executable(igui_interaction_harness_tests tests/test_interaction_harness.cpp)
        target_link_libraries(igui_interaction_harness_tests PRIVATE igui)
        add_test(NAME igui_interaction_harness COMMAND igui_interaction_harness_tests)

        add_executable(igui_modal_input_tests tests/test_modal_input.cpp)
        target_link_libraries(igui_modal_input_tests PRIVATE igui)
        add_test(NAME igui_modal_input COMMAND igui_modal_input_tests)

        add_executable(igui_culling_tests tests/test_culling.cpp)
        target_link_libraries(igui_culling_tests PRIVATE igui)
        add_test(NAME igui_culling COMMAND igui_culling_tests)

        add_executable(igui_fuzz_tests tests/test_fuzz.cpp)
        target_link_libraries(igui_fuzz_tests PRIVATE igui)
        add_test(NAME igui_fuzz COMMAND igui_fuzz_tests)

        add_executable(igui_numeric_input_tests tests/test_numeric_input.cpp)
        target_link_libraries(igui_numeric_input_tests PRIVATE igui)
        add_test(NAME igui_numeric_input COMMAND igui_numeric_input_tests)

        add_executable(igui_text_editing_tests tests/test_text_editing.cpp)
        target_link_libraries(igui_text_editing_tests PRIVATE igui)
        add_test(NAME igui_text_editing COMMAND igui_text_editing_tests)

        add_executable(igui_windows_tests tests/test_windows.cpp)
        target_link_libraries(igui_windows_tests PRIVATE igui)
        add_test(NAME igui_windows COMMAND igui_windows_tests)

        add_executable(igui_code_editor_tests tests/test_code_editor.cpp)
        target_link_libraries(igui_code_editor_tests PRIVATE igui)
        add_test(NAME igui_code_editor COMMAND igui_code_editor_tests)
    endif()
endif()

# Retained widget toolkit: independent of both the immediate-mode Context
# and of building any SDL2 demo - only needs the shared drawing engine.
if(IGUI_BUILD_RETAINED AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb/stb_impl.cpp")
    add_library(igui_widgets_stb STATIC third_party/stb/stb_impl.cpp)
    target_include_directories(igui_widgets_stb PUBLIC third_party/stb)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(igui_widgets_stb PRIVATE
            -Wno-unused-function -Wno-unused-but-set-variable
            -Wno-array-bounds)
    endif()

    add_library(igui_widgets_r128 STATIC third_party/r128/r128_impl.cpp)
    target_include_directories(igui_widgets_r128 PUBLIC third_party/r128)

    set(IGUI_WIDGET_SOURCES
        src/widgets/Animation.cpp
        src/widgets/AppWidgets.cpp
        src/widgets/AssetBrowser.cpp
        src/widgets/AudioWidgets.cpp
        src/widgets/AutomotiveWidgets.cpp
        src/widgets/BasicWidgets.cpp
        src/widgets/Retained.cpp
        src/widgets/BuImage.cpp
        src/widgets/ChartWidget.cpp
        src/widgets/ChartWidgets.cpp
        src/widgets/CodeEditor.cpp
        src/widgets/CodeEditorTools.cpp
        src/widgets/ComboBox.cpp
        src/widgets/ConsoleWidget.cpp
        src/widgets/DataWidgets.cpp
        src/widgets/DialogWidgets.cpp
        src/widgets/DirCache.cpp
        src/widgets/DockPanel.cpp
        src/widgets/FileDialog.cpp
        src/widgets/FileSystem.cpp
        src/widgets/GizmoWidgets.cpp
        src/widgets/IconAtlas.cpp
        src/widgets/InputWidgets.cpp
        src/widgets/ItemModel.cpp
        src/widgets/LayoutWidgets.cpp
        src/widgets/MenuWidgets.cpp
        src/widgets/NodeEditor.cpp
        src/widgets/Range.cpp
        src/widgets/ScrollWidgets.cpp
        src/widgets/SideTabBar.cpp
        src/widgets/SpinSlider.cpp
        src/widgets/SymbolOutline.cpp
        src/widgets/TextInputWidgets.cpp
        src/widgets/ThumbnailGrid.cpp
        src/widgets/Timeline.cpp
        src/widgets/ToolBar.cpp
        src/widgets/TreePropertyColorWidgets.cpp
        src/widgets/ViewWidgets.cpp
        src/widgets/Widget.cpp
        src/widgets/WidgetApp.cpp
        src/widgets/WidgetSerializer.cpp)
    add_library(igui_widgets STATIC ${IGUI_WIDGET_SOURCES})
    add_library(igui::widgets ALIAS igui_widgets)
    target_compile_features(igui_widgets PUBLIC cxx_std_17)
    target_include_directories(igui_widgets
        PUBLIC include include/igui/widgets third_party
        PRIVATE src/widgets third_party/stb)
    # The retained widget headers expose ct containers, so propagate the
    # core target (and therefore ct's include directory) to consumers.
    target_link_libraries(igui_widgets PUBLIC igui_core PRIVATE igui_widgets_stb igui_widgets_r128)
    # Same reasoning as igui above: linking selects the retained API.
    target_compile_definitions(igui_widgets PUBLIC IGUI_RETAINEDMODE=1)
    target_precompile_headers(igui_widgets PRIVATE src/widgets/pch.hpp)

    # Rounded clips of the retained DrawList, including the software rasteriser's
    # coverage (SoftRaster.cpp needs nothing but the widgets).
    if(IGUI_BUILD_TESTS)
        add_executable(igui_round_clip_tests tests/test_round_clip.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_round_clip_tests PRIVATE cxx_std_17)
        target_include_directories(igui_round_clip_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_round_clip_tests PRIVATE igui_widgets)
        add_test(NAME igui_round_clip COMMAND igui_round_clip_tests)
    endif()

    # Both widget APIs linked into one binary. Only possible when both were
    # built, and the only target that compiles with IGUI_BOTH_MODES=1 - see
    # tests/test_both_modes.cpp and include/igui/Modes.hpp.
    if(IGUI_BUILD_TESTS AND IGUI_BUILD_IMMEDIATE)
        add_executable(igui_both_modes_tests tests/test_both_modes.cpp)
        target_compile_features(igui_both_modes_tests PRIVATE cxx_std_17)
        target_link_libraries(igui_both_modes_tests PRIVATE igui igui_widgets)
        add_test(NAME igui_both_modes COMMAND igui_both_modes_tests)
    endif()

    if(IGUI_BUILD_TESTS)
        # The retained CodeEditor's symbols, reveal and completion popup, and
        # the SymbolOutline beside it, driven through WidgetApp.
        add_executable(igui_code_outline_tests tests/test_code_outline.cpp)
        target_compile_features(igui_code_outline_tests PRIVATE cxx_std_17)
        target_link_libraries(igui_code_outline_tests PRIVATE igui_widgets)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(igui_code_outline_tests PRIVATE -fsanitize=address)
            target_link_options(igui_code_outline_tests PRIVATE -fsanitize=address)
        endif()
        add_test(NAME igui_code_outline COMMAND igui_code_outline_tests)

        # The retained Chart: pie, donut and bars, their legend, labels and
        # hit testing, driven through WidgetApp.
        add_executable(igui_chart_tests tests/test_chart.cpp)
        target_compile_features(igui_chart_tests PRIVATE cxx_std_17)
        target_link_libraries(igui_chart_tests PRIVATE igui_widgets)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(igui_chart_tests PRIVATE -fsanitize=address)
            target_link_options(igui_chart_tests PRIVATE -fsanitize=address)
        endif()
        add_test(NAME igui_chart COMMAND igui_chart_tests)

        # The theme presets: readable colour pairs in each, found by name.
        add_executable(igui_themes_tests tests/test_themes.cpp)
        target_compile_features(igui_themes_tests PRIVATE cxx_std_17)
        target_link_libraries(igui_themes_tests PRIVATE igui_widgets)
        add_test(NAME igui_themes COMMAND igui_themes_tests)

        # The text engine: glyphs at the exact size, kerning, the dynamic page.
        add_executable(igui_text_sizes_tests tests/test_text_sizes.cpp)
        target_compile_features(igui_text_sizes_tests PRIVATE cxx_std_17)
        target_link_libraries(igui_text_sizes_tests PRIVATE igui_widgets)
        add_test(NAME igui_text_sizes COMMAND igui_text_sizes_tests)

        # The DrawList's vertex transform (pushTransform / popTransform).
        add_executable(igui_transform_tests tests/test_transform.cpp)
        target_compile_features(igui_transform_tests PRIVATE cxx_std_17)
        target_link_libraries(igui_transform_tests PRIVATE igui_widgets)
        add_test(NAME igui_transform COMMAND igui_transform_tests)

        # The retained CodeEditor's keyboard: undo groups, the inline bars,
        # the commands, cursors, bookmarks, the jump list.
        add_executable(igui_code_editor_keys_tests tests/test_code_editor_keys.cpp)
        target_compile_features(igui_code_editor_keys_tests PRIVATE cxx_std_17)
        target_link_libraries(igui_code_editor_keys_tests PRIVATE igui_widgets)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(igui_code_editor_keys_tests PRIVATE -fsanitize=address)
            target_link_options(igui_code_editor_keys_tests PRIVATE -fsanitize=address)
        endif()
        add_test(NAME igui_code_editor_keys COMMAND igui_code_editor_keys_tests)

        add_executable(igui_original_widgets_tests tests/test_original_widgets.cpp)
        target_compile_features(igui_original_widgets_tests PRIVATE cxx_std_17)
        target_link_libraries(igui_original_widgets_tests PRIVATE
            igui_widgets)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(igui_original_widgets_tests PRIVATE -fsanitize=address)
            target_link_options(igui_original_widgets_tests PRIVATE -fsanitize=address)
        endif()
        add_test(NAME igui_original_widgets COMMAND igui_original_widgets_tests)
        set_tests_properties(igui_original_widgets PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

        # Retained widgets driven through the real frame loop of the zen adapter, headless
        # (tests/RetainedHarness.hpp): lifetime under event dispatch, text, layout, the timeline,
        # and a random event / tree-mutation fuzz. Most of what they catch is memory errors: run
        # them from a sanitizer build too (-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined).
        foreach(_igui_rt_test lifetime timeline text layout fuzz input hostile)
            add_executable(igui_retained_${_igui_rt_test}_tests tests/test_retained_${_igui_rt_test}.cpp)
            target_compile_features(igui_retained_${_igui_rt_test}_tests PRIVATE cxx_std_17)
            target_link_libraries(igui_retained_${_igui_rt_test}_tests PRIVATE igui_widgets)
            add_test(NAME igui_retained_${_igui_rt_test} COMMAND igui_retained_${_igui_rt_test}_tests)
        endforeach()
        # The hostile test counts the heap requests of every frame through a wrapped malloc and stops at a ceiling,
        # limits its address space where the sanitizers allow it, and has a timeout: a loop that does not stop
        # fails here quickly instead of taking the memory of the machine (tests/test_retained_hostile.cpp).
        target_link_options(igui_retained_hostile_tests PRIVATE -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc)
        set_tests_properties(igui_retained_hostile PROPERTIES TIMEOUT 180)
        # an idle frame must not allocate: counted through a wrapped malloc and operator new
        add_executable(igui_retained_idle_tests tests/test_retained_idle.cpp)
        target_compile_features(igui_retained_idle_tests PRIVATE cxx_std_17)
        target_link_libraries(igui_retained_idle_tests PRIVATE igui_widgets)
        target_link_options(igui_retained_idle_tests PRIVATE -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc)
        add_test(NAME igui_retained_idle COMMAND igui_retained_idle_tests)
        # DataGrid and TreeGrid: the text of headers and cells stays in its column (pixels of the
        # software rasteriser), text that does not fit ends in "...", columns that stretch with the
        # widget; counts allocations like the idle test.
        add_executable(igui_retained_grid_tests tests/test_retained_grid.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_retained_grid_tests PRIVATE cxx_std_17)
        target_include_directories(igui_retained_grid_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_retained_grid_tests PRIVATE igui_widgets)
        target_link_options(igui_retained_grid_tests PRIVATE -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc)
        add_test(NAME igui_retained_grid COMMAND igui_retained_grid_tests)
        # Where widgets put their text: a long text stays in its own rectangle and does not run over the
        # part next to it (pixels of the software rasteriser).
        add_executable(igui_retained_textbounds_tests tests/test_retained_textbounds.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_retained_textbounds_tests PRIVATE cxx_std_17)
        target_include_directories(igui_retained_textbounds_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_retained_textbounds_tests PRIVATE igui_widgets)
        add_test(NAME igui_retained_textbounds COMMAND igui_retained_textbounds_tests)
        # The basic widgets (CheckBox, Button, Label, ComboBox, ImageButton...): what they draw (pixels of the
        # software rasteriser) and the size they ask for.
        add_executable(igui_retained_basic_tests tests/test_retained_basic.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_retained_basic_tests PRIVATE cxx_std_17)
        target_include_directories(igui_retained_basic_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_retained_basic_tests PRIVATE igui_widgets)
        add_test(NAME igui_retained_basic COMMAND igui_retained_basic_tests)
        # Menus: what an item draws, who owns the actions and the submenus, the signals a menu emits.
        add_executable(igui_retained_menus_tests tests/test_retained_menus.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_retained_menus_tests PRIVATE cxx_std_17)
        target_include_directories(igui_retained_menus_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_retained_menus_tests PRIVATE igui_widgets)
        add_test(NAME igui_retained_menus COMMAND igui_retained_menus_tests)
        # Menus, second half of round 4 (docs/ROUND4B.md): icon of an action, toggled state, aboutToShow, popups with widgets.
        add_executable(igui_retained_menus4b_tests tests/test_retained_menus4b.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_retained_menus4b_tests PRIVATE cxx_std_17)
        target_include_directories(igui_retained_menus4b_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_retained_menus4b_tests PRIVATE igui_widgets)
        add_test(NAME igui_retained_menus4b COMMAND igui_retained_menus4b_tests)
        # The rest of the second half of round 4 (docs/ROUND4B.md): synthetic input, tool and tab bars with the
        # application's own icons, the timeline's bars and beats, piano roll velocities, chart double data and axis
        # titles, the slider readout.
        add_executable(igui_retained_round4b_tests tests/test_retained_round4b.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_retained_round4b_tests PRIVATE cxx_std_17)
        target_include_directories(igui_retained_round4b_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_retained_round4b_tests PRIVATE igui_widgets)
        add_test(NAME igui_retained_round4b COMMAND igui_retained_round4b_tests)
        # Round 5 (docs/ROUND5.md): the piano roll's note length and velocity lane with the mouse, hostile inputs for them, the
        # tool bar in the serializer, the scroll bar thickness from the theme. The heap requests of every frame of the hostile
        # cases are counted through a wrapped malloc, as in igui_retained_hostile.
        add_executable(igui_retained_round5_tests tests/test_retained_round5.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_retained_round5_tests PRIVATE cxx_std_17)
        target_include_directories(igui_retained_round5_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_retained_round5_tests PRIVATE igui_widgets)
        add_test(NAME igui_retained_round5 COMMAND igui_retained_round5_tests)
        target_link_options(igui_retained_round5_tests PRIVATE -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc)
        set_tests_properties(igui_retained_round5 PROPERTIES TIMEOUT 180)
        # Round 6 (docs/ROUND6.md): the scroll bar room that painting reads from the bar, the piano roll's notes bounded by
        # the pixels on screen, widgets that delete themselves in their own signals, the tool bar ids of the serializer. The
        # heap requests of every frame of the hostile cases are counted through a wrapped malloc, as in igui_retained_round5.
        add_executable(igui_retained_round6_tests tests/test_retained_round6.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_retained_round6_tests PRIVATE cxx_std_17)
        target_include_directories(igui_retained_round6_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_retained_round6_tests PRIVATE igui_widgets)
        add_test(NAME igui_retained_round6 COMMAND igui_retained_round6_tests)
        target_link_options(igui_retained_round6_tests PRIVATE -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc)
        set_tests_properties(igui_retained_round6 PROPERTIES TIMEOUT 180)
        # Round 7 (docs/ROUND7.md): a handler that takes its own widget out of the tree inside the signal of every widget that
        # emits one (a control, and the removal in a frame and outside one, for each signal). Meant for AddressSanitizer too.
        add_executable(igui_retained_round7_tests tests/test_retained_round7.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_retained_round7_tests PRIVATE cxx_std_17)
        target_include_directories(igui_retained_round7_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_retained_round7_tests PRIVATE igui_widgets)
        add_test(NAME igui_retained_round7 COMMAND igui_retained_round7_tests)
        set_tests_properties(igui_retained_round7 PROPERTIES TIMEOUT 600)
        # Round 8 (docs/COMPARACAO.md, part 2): the signals that rounds 5 to 7 left alone, rectangles of no size and of enormous size,
        # no items and a million, hostile strings, widgets shown and hidden in a hurry, the pixel scale changed at run time.
        add_executable(igui_retained_round8_tests tests/test_retained_round8.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_retained_round8_tests PRIVATE cxx_std_17)
        target_include_directories(igui_retained_round8_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_retained_round8_tests PRIVATE igui_widgets)
        add_test(NAME igui_retained_round8 COMMAND igui_retained_round8_tests)
        set_tests_properties(igui_retained_round8 PROPERTIES TIMEOUT 900)
        # Round 9 (docs/ROUND9.md): the theme switched at run time on a window with most of the widget types: the dark preset draws
        # as before, no dark fill under the light presets, themeChanged once per change, legible text in every preset.
        add_executable(igui_retained_round9_tests tests/test_retained_round9.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_retained_round9_tests PRIVATE cxx_std_17)
        target_include_directories(igui_retained_round9_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_retained_round9_tests PRIVATE igui_widgets)
        add_test(NAME igui_retained_round9 COMMAND igui_retained_round9_tests)
        set_tests_properties(igui_retained_round9 PROPERTIES TIMEOUT 300)
        # Round 9, part 2 (docs/ROUND9.md): the corners of a rounded rectangle by the error, its fast paths against the generic path, commands
        # merged across clips and across the two pages of the font atlas (same pixels as unmerged), the white texel of the dynamic page.
        add_executable(igui_retained_drawlist_tests tests/test_retained_drawlist.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_retained_drawlist_tests PRIVATE cxx_std_17)
        target_include_directories(igui_retained_drawlist_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_retained_drawlist_tests PRIVATE igui_widgets)
        add_test(NAME igui_retained_drawlist COMMAND igui_retained_drawlist_tests)
        set_tests_properties(igui_retained_drawlist PROPERTIES TIMEOUT 300)
        # Round 9, part 2: frames that are not painted: nothing dirty paints nothing, each change paints one frame, what runs on time
        # keeps on time, and skipping leaves no stale picture.
        add_executable(igui_retained_frames_tests tests/test_retained_frames.cpp)
        target_compile_features(igui_retained_frames_tests PRIVATE cxx_std_17)
        target_link_libraries(igui_retained_frames_tests PRIVATE igui_widgets)
        add_test(NAME igui_retained_frames COMMAND igui_retained_frames_tests)
        set_tests_properties(igui_retained_frames PROPERTIES TIMEOUT 300)
        # Round 10, part 2 (docs/ROUND10.md): TreeView labels cut with an ellipsis, a second text and an icon tint on the rows, TreeGrid
        # activation, FileDialog in Save mode (the question before replacing, the extension, the folder by id), list selection text.
        add_executable(igui_retained_round10_tests tests/test_retained_round10.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_retained_round10_tests PRIVATE cxx_std_17)
        target_include_directories(igui_retained_round10_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_retained_round10_tests PRIVATE igui_widgets)
        target_link_options(igui_retained_round10_tests PRIVATE -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc)
        add_test(NAME igui_retained_round10 COMMAND igui_retained_round10_tests)
        set_tests_properties(igui_retained_round10 PROPERTIES TIMEOUT 300)
        # Round 10, part 1 (docs/DRAW-CALLBACK.md): native draw callbacks without a GL context: availability, the fallback of a
        # NativeView, the place in the draw order, nothing merged across one, the software rasteriser skipping it, idle.
        add_executable(igui_native_callback_tests tests/test_native_callback.cpp adapters/zen/SoftRaster.cpp)
        target_compile_features(igui_native_callback_tests PRIVATE cxx_std_17)
        target_include_directories(igui_native_callback_tests PRIVATE adapters/zen/include)
        target_link_libraries(igui_native_callback_tests PRIVATE igui_widgets)
        add_test(NAME igui_native_callback COMMAND igui_native_callback_tests)
        set_tests_properties(igui_native_callback PROPERTIES TIMEOUT 300)
        # the layout test also feeds the software rasteriser odd geometry
        target_sources(igui_retained_layout_tests PRIVATE adapters/zen/SoftRaster.cpp)
        target_include_directories(igui_retained_layout_tests PRIVATE adapters/zen/include)
    endif()
endif()

if(IGUI_BUILD_FONT)
    if(IGUI_FONT_INPUT)
        set(_igui_font_input "${IGUI_FONT_INPUT}")
        if(NOT EXISTS "${_igui_font_input}")
            message(FATAL_ERROR "IGUI_FONT_INPUT does not exist: ${_igui_font_input}")
        endif()
    else()
        include(FetchContent)
        FetchContent_Declare(
            igui_nuklear_assets
            GIT_REPOSITORY https://github.com/Immediate-Mode-UI/Nuklear.git
            GIT_TAG 0dbc52f86404f9e1f26ce0df3015ed23ff54a726
        )
        FetchContent_GetProperties(igui_nuklear_assets)
        if(NOT igui_nuklear_assets_POPULATED)
            FetchContent_Populate(igui_nuklear_assets)
        endif()
        set(_igui_font_input "${igui_nuklear_assets_SOURCE_DIR}/extra_font/Roboto-Regular.ttf")
    endif()

    set(IGUI_DEFAULT_FONT_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated/DefaultFont.cpp")
    add_custom_command(
        OUTPUT "${IGUI_DEFAULT_FONT_CPP}"
        COMMAND ${CMAKE_COMMAND}
                -DIGUI_FONT_INPUT=${_igui_font_input}
                -DIGUI_FONT_OUTPUT=${IGUI_DEFAULT_FONT_CPP}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFont.cmake
        DEPENDS "${_igui_font_input}"
                ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFont.cmake
        VERBATIM
    )
    add_library(igui_font src/FontAtlas.cpp "${IGUI_DEFAULT_FONT_CPP}")
    add_library(igui::font ALIAS igui_font)
    target_include_directories(igui_font
        PUBLIC include
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
    )
    # FontAtlas only uses Backend.hpp/ct::Vector (igui_core), not the
    # immediate-mode Context - it must not require IGUI_BUILD_IMMEDIATE.
    target_link_libraries(igui_font PUBLIC igui_core)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(igui_font PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(IGUI_BUILD_TESTS)
        add_executable(igui_font_tests tests/test_font.cpp)
        target_link_libraries(igui_font_tests PRIVATE igui_font)
        add_test(NAME igui_font COMMAND igui_font_tests)
    endif()
endif()

# Headless benchmark of the retained widgets' hot paths on an editor-sized window: idle frame,
# layout, text measuring, timeline hover and drag, with the allocations of each (bench/retained).
option(IGUI_BUILD_RETAINED_BENCH "Build the headless retained-widgets benchmark" OFF)
if(IGUI_BUILD_RETAINED_BENCH AND TARGET igui_widgets)
    add_executable(igui_retained_bench bench/retained/RetainedBench.cpp)
    target_compile_features(igui_retained_bench PRIVATE cxx_std_17)
    target_link_libraries(igui_retained_bench PRIVATE igui_widgets)
    target_sources(igui_retained_bench PRIVATE adapters/zen/SoftRaster.cpp)
    target_include_directories(igui_retained_bench PRIVATE adapters/zen/include)
    # The allocation counter wraps malloc (ct allocates with it); operator new is replaced in the file.
    target_link_options(igui_retained_bench PRIVATE -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc)
    if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "igui_retained_bench: configure with -DCMAKE_BUILD_TYPE=Release for meaningful numbers.")
    endif()
endif()

# Headless benchmark: same widget scenes submitted through ig::Context and
# through ImGui, timed side by side. See bench/README.md.
if(IGUI_BUILD_BENCH)
    if(NOT IGUI_BUILD_IMMEDIATE)
        message(FATAL_ERROR "IGUI_BUILD_BENCH=ON needs IGUI_BUILD_IMMEDIATE=ON")
    endif()
    add_subdirectory(bench)
endif()

# OpenGL 3.3 core backend: renders the immediate-mode DrawData with no GL loader
# dependency (the application supplies the proc-address callback). Needs the font
# module, which is what provides the atlas texture.
if(IGUI_BUILD_OPENGL_BACKEND)
    if(NOT IGUI_BUILD_FONT)
        message(FATAL_ERROR "IGUI_BUILD_OPENGL_BACKEND=ON needs IGUI_BUILD_FONT=ON")
    endif()
    add_library(igui_opengl adapters/opengl/OpenglBackend.cpp)
    add_library(igui::opengl ALIAS igui_opengl)
    target_include_directories(igui_opengl PUBLIC adapters/opengl/include)
    target_link_libraries(igui_opengl PUBLIC igui_font)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(igui_opengl PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(IGUI_RAYLIB_AVAILABLE)
    add_library(igui_raylib adapters/raylib/RaylibBackend.cpp)
    add_library(igui::raylib ALIAS igui_raylib)
    target_include_directories(igui_raylib PUBLIC adapters/raylib/include)

    add_library(igui_raylib_native STATIC IMPORTED GLOBAL)
    set_target_properties(igui_raylib_native PROPERTIES
        IMPORTED_LOCATION "${IGUI_RAYLIB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${IGUI_RAYLIB_INCLUDE_DIR}")
    add_library(igui::raylib_native ALIAS igui_raylib_native)

    # Consumers receive Raylib's archive and headers through this target. On
    # Linux, Raylib itself may additionally require the platform libraries
    # selected when its static archive was built (for example GLFW and X11).
    target_link_libraries(igui_raylib PUBLIC igui igui_font igui_raylib_native)
    if(UNIX AND NOT APPLE)
        find_package(Threads REQUIRED)
        find_package(OpenGL QUIET)
        find_library(IGUI_RAYLIB_GLFW_LIBRARY NAMES glfw glfw3)
        target_link_libraries(igui_raylib PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
        if(OpenGL_FOUND)
            target_link_libraries(igui_raylib PUBLIC OpenGL::GL)
        endif()
        if(IGUI_RAYLIB_GLFW_LIBRARY)
            target_link_libraries(igui_raylib PUBLIC "${IGUI_RAYLIB_GLFW_LIBRARY}")
        else()
            message(STATUS "GLFW was not found; applications using the static Raylib archive must provide it")
        endif()
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(igui_raylib PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(IGUI_BUILD_RAYLIB_DEMO)
        add_executable(igui_raylib_demo examples/raylib_demo/main.cpp)
        target_link_libraries(igui_raylib_demo PRIVATE igui_raylib)

        add_executable(igui_raylib_dock_demo examples/raylib_dock_demo/main.cpp)
        target_include_directories(igui_raylib_dock_demo PRIVATE third_party/stb)
        target_link_libraries(igui_raylib_dock_demo PRIVATE igui_raylib)
    endif()

    if(IGUI_BUILD_CODE_EDITOR_DEMO)
        add_executable(igui_code_editor_demo examples/code_editor_demo/main.cpp)
        target_link_libraries(igui_code_editor_demo PRIVATE igui_raylib)
    endif()

    if(IGUI_BUILD_MIXER_DEMO)
        add_executable(igui_mixer_demo examples/mixer_demo/main.cpp)
        target_include_directories(igui_mixer_demo PRIVATE examples/mixer_demo)
        target_link_libraries(igui_mixer_demo PRIVATE igui_raylib)
    endif()

    if(IGUI_BUILD_SYNTH_DEMO)
        add_executable(igui_synth_demo examples/synth_demo/main.cpp)
        target_include_directories(igui_synth_demo PRIVATE examples/synth_demo)
        target_link_libraries(igui_synth_demo PRIVATE igui_raylib)
    endif()

    if(IGUI_BUILD_ZENBEATS_DEMO)
        add_executable(igui_zenbeats_demo examples/zenbeats_demo/main.cpp)
        target_include_directories(igui_zenbeats_demo PRIVATE examples/zenbeats_demo)
        target_link_libraries(igui_zenbeats_demo PRIVATE igui_raylib)
    endif()

    if(IGUI_BUILD_DIALOGS_DEMO)
        add_executable(igui_dialogs_demo examples/dialogs_demo/main.cpp)
        target_link_libraries(igui_dialogs_demo PRIVATE igui_raylib)
    endif()
endif()

# zen_plataform adapter: a software rasteriser with anti-aliasing and the window
# and input bridge for the retained widgets (igui_zen), plus the widget gallery.
option(IGUI_BUILD_ZEN_ADAPTER "Build the zen_plataform adapter (igui_zen)" OFF)
option(IGUI_BUILD_ZEN_DEMO "Build the retained widgets demo on zen_plataform" OFF)
set(IGUI_ZEN_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../zen_plataform" CACHE PATH
    "Path to the zen_plataform checkout (used when the parent has no platform target)")
if((IGUI_BUILD_ZEN_ADAPTER OR IGUI_BUILD_ZEN_DEMO) AND TARGET igui_widgets)
    if(NOT TARGET platform)
        enable_language(C)
        set(PLATFORM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(${IGUI_ZEN_PLATFORM_DIR} zen_plataform EXCLUDE_FROM_ALL)
    endif()
    add_library(igui_zen STATIC adapters/zen/SoftRaster.cpp adapters/zen/GlRaster.cpp adapters/zen/ZenApp.cpp)
    add_library(igui::zen ALIAS igui_zen)
    target_compile_features(igui_zen PUBLIC cxx_std_17)
    target_include_directories(igui_zen PUBLIC adapters/zen/include)
    target_link_libraries(igui_zen PUBLIC igui_widgets platform)

    # The tests that open windows or inject keyboard and mouse events must never do it on the display of whoever
    # runs ctest (a bare `ctest` in a desktop session would click and type on the desktop). On Linux they are
    # run through tools/xvfb-test.sh, which starts an Xvfb of its own and tells the test which display that is
    # (the test is skipped, 77, where xvfb-run is not installed), and each test refuses on its own, before it
    # touches a display, unless it can prove it is on that Xvfb (tests/ThrowawayDisplay.hpp). The second ctest
    # entry runs the same program on an Xvfb without telling it, and passes only when it refuses: a guard that
    # is taken out fails there, on a display that does not matter. With the fake backend of zen_plataform
    # there is no window at all and the tests run as they are. Anywhere else there is no throwaway display: the
    # test is built and not registered.
    function(igui_add_window_test name target)
        if(PLATFORM_USE_FAKE)
            target_compile_definitions(${target} PRIVATE IGUI_ZEN_FAKE_PLATFORM=1)
            add_test(NAME ${name} COMMAND ${target})
            set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 180)
        elseif(UNIX AND NOT APPLE)
            set(_igui_xvfb sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/xvfb-test.sh")
            add_test(NAME ${name} COMMAND ${_igui_xvfb} $<TARGET_FILE:${target}>)
            set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 180 RESOURCE_LOCK igui_xvfb)
            add_test(NAME ${name}_refuses_unproven
                COMMAND ${_igui_xvfb} env -u IGUI_TEST_THROWAWAY_DISPLAY $<TARGET_FILE:${target}>)
            # passes on the refusal (the program exits 77 then, which is what is wanted here, not a skip); where there
            # is no xvfb-run the script says so and nothing was started
            set_tests_properties(${name}_refuses_unproven PROPERTIES TIMEOUT 60 RESOURCE_LOCK igui_xvfb
                PASS_REGULAR_EXPRESSION "refused: not started by tools/xvfb-test.sh|xvfb-run or Xvfb is not installed")
        else()
            message(STATUS "${name}: no throwaway display on this system, the test is built and not registered")
        endif()
    endfunction()

    # The SideTabBar driven through a zen_plataform window, no SDL. With
    # PLATFORM_USE_FAKE=ON it runs headless; on X11/Wayland it needs a display
    # and is skipped (exit 77) without one.
    if(IGUI_BUILD_TESTS)
        add_executable(igui_zen_sidetabs_tests tests/test_zen_sidetabs.cpp)
        target_link_libraries(igui_zen_sidetabs_tests PRIVATE igui_zen)
        igui_add_window_test(igui_zen_sidetabs igui_zen_sidetabs_tests)
    endif()

    if(IGUI_BUILD_TESTS)
        # The window lifecycle: the application and dialog windows made and destroyed over and over,
        # parents closed with dialogs open, the scale changed. Needs an X server (77: skipped).
        add_executable(igui_zen_lifecycle_tests tests/test_zen_lifecycle.cpp)
        target_link_libraries(igui_zen_lifecycle_tests PRIVATE igui_zen)
        igui_add_window_test(igui_zen_lifecycle igui_zen_lifecycle_tests)
    endif()

    if(IGUI_BUILD_TESTS)
        # ZEN_SCREENSHOT_BACKEND=gl (the screenshot drawn by OpenGL into an offscreen framebuffer of a window that is never
        # shown), Config::defaultDynamicPage and text drawn with the atlas pixels released. Needs an X server with OpenGL (on the
        # Xvfb of tools/xvfb-test.sh: software GL, which is enough); 77 (skipped) without one.
        add_executable(igui_zen_screenshot_gl_tests tests/test_zen_screenshot_gl.cpp)
        target_link_libraries(igui_zen_screenshot_gl_tests PRIVATE igui_zen)
        igui_add_window_test(igui_zen_screenshot_gl igui_zen_screenshot_gl_tests)
    endif()

    # Round 10: the cost per frame of long lines (PaintContext::drawPolyline) and of the interactive chart in a real window, on OpenGL
    # and on the software rasteriser (bench/retained/PlotBench.cpp). Run it on a private Xvfb: sh tools/xvfb-test.sh igui_plot_bench
    if(IGUI_BUILD_RETAINED_BENCH)
        add_executable(igui_plot_bench bench/retained/PlotBench.cpp)
        target_compile_features(igui_plot_bench PRIVATE cxx_std_17)
        target_link_libraries(igui_plot_bench PRIVATE igui_zen)
        target_link_options(igui_plot_bench PRIVATE -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc)
    endif()

    if(IGUI_BUILD_TESTS)
        # Round 10, part 1 (docs/DRAW-CALLBACK.md): native draw callbacks on the OpenGL renderer: what they are given, a triangle read
        # back inside the widget's rectangle and nothing outside, a scroll area's clip, the order between overlapping widgets, the GL
        # state after a callback that trashes it, idle frames. Needs an X server with OpenGL (77, skipped, without one).
        add_executable(igui_zen_native_callback_gl_tests tests/test_zen_native_callback_gl.cpp)
        target_link_libraries(igui_zen_native_callback_gl_tests PRIVATE igui_zen)
        igui_add_window_test(igui_zen_native_callback_gl igui_zen_native_callback_gl_tests)
    endif()

    # Mouse and keyboard through the adapter, with real events made by the XTEST extension of an X server
    # (Linux): clicks, drags, typing, auto-repeat, the wheel; the delays and what is lost are printed.
    if(IGUI_BUILD_TESTS AND UNIX AND NOT APPLE)
        find_library(IGUI_X11_LIBRARY X11)
        find_library(IGUI_XTEST_LIBRARY Xtst)
        find_path(IGUI_XTEST_INCLUDE_DIR X11/extensions/XTest.h)
        if(IGUI_X11_LIBRARY AND IGUI_XTEST_LIBRARY AND IGUI_XTEST_INCLUDE_DIR)
            add_executable(igui_zen_input_tests tests/test_zen_input.cpp)
            target_include_directories(igui_zen_input_tests PRIVATE "${IGUI_XTEST_INCLUDE_DIR}")
            target_link_libraries(igui_zen_input_tests PRIVATE igui_zen "${IGUI_XTEST_LIBRARY}" "${IGUI_X11_LIBRARY}")
            igui_add_window_test(igui_zen_input igui_zen_input_tests)
        endif()
    endif()

    # The guard those tests share (tests/ThrowawayDisplay.hpp): yes on the Xvfb of tools/xvfb-test.sh, no in every
    # other case it is given. It opens no window.
    if(IGUI_BUILD_TESTS AND UNIX AND NOT APPLE AND NOT PLATFORM_USE_FAKE)
        add_executable(igui_throwaway_display_tests tests/test_throwaway_display.cpp)
        add_test(NAME igui_throwaway_display
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/xvfb-test.sh" $<TARGET_FILE:igui_throwaway_display_tests>)
        set_tests_properties(igui_throwaway_display PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60 RESOURCE_LOCK igui_xvfb)
    endif()

    if(IGUI_BUILD_ZEN_DEMO)
        add_executable(igui_zen_windows examples/zen_windows/main.cpp)
        target_link_libraries(igui_zen_windows PRIVATE igui_zen)
        add_executable(igui_zen_widgets
            examples/zen_widgets/main.cpp
            examples/sdl2_original_widgets/stage_advanced.cpp
            examples/sdl2_original_widgets/stages/stage_demo.cpp
            examples/sdl2_original_widgets/stages/stage_menu.cpp
            examples/sdl2_original_widgets/stages/stage_basic.cpp
            examples/sdl2_original_widgets/stages/stage_controls.cpp
            examples/sdl2_original_widgets/stages/stage_scroll.cpp
            examples/sdl2_original_widgets/stages/stage_inputs.cpp
            examples/sdl2_original_widgets/stages/stage_menus.cpp
            examples/sdl2_original_widgets/stages/stage_dialogs.cpp
            examples/sdl2_original_widgets/stages/stage_dock.cpp
            examples/sdl2_original_widgets/stages/stage_properties.cpp
            examples/sdl2_original_widgets/stages/stage_video.cpp
            examples/sdl2_original_widgets/stages/stage_sidetabs.cpp)
        target_include_directories(igui_zen_widgets PRIVATE examples/sdl2_original_widgets)
        target_link_libraries(igui_zen_widgets PRIVATE igui_zen)
    endif()
endif()

# The comparison programs (iGUI, Dear ImGui, FLTK, GTK 3 side by side: docs/COMPARACAO.md). Off by default, and outside the
# no-std rule on purpose: cmake/CheckNoStd.cmake and tools/check.sh look at src/, include/ and adapters/ only, and
# bench/compare holds the code for the other libraries (which need the C++ library) next to a copy of the scenes for iGUI.
option(IGUI_BUILD_COMPARE "Build the comparison programs of bench/compare (needs IGUI_BUILD_ZEN_ADAPTER=ON); outside the no-std rule" OFF)
if(IGUI_BUILD_COMPARE)
    if(NOT TARGET igui_zen)
        message(FATAL_ERROR "IGUI_BUILD_COMPARE=ON needs IGUI_BUILD_ZEN_ADAPTER=ON")
    endif()
    add_subdirectory(bench/compare)
endif()

if(IGUI_BUILD_SDL2_DEMO OR IGUI_BUILD_WEB)
    # SDL2 arrives one of two ways: a parent project already built or found
    # it (target SDL2::SDL2 - e.g. a vendored submodule), or it is installed
    # system-wide and pkg-config can see it. Prefer the target: pkg-config
    # finds nothing on a plain Windows/MSYS2 or MSVC box, and REQUIRED there
    # killed the whole configure step.
    if(TARGET SDL2::SDL2)
        # On Windows SDL.h renames main to SDL_main, so every executable
        # below needs SDL2main's entry-point shim. Harmless elsewhere (the
        # target only exists for in-tree SDL builds). SDL2main MUST be
        # listed before SDL2: SDL2main.a's own undefined refs (SDL_strlen
        # and friends) are resolved by whatever comes later on the link
        # line, and GNU ld doesn't revisit a library it already passed -
        # SDL2 first left igui_sdl2_demo with "undefined reference to
        # SDL_strlen" out of libSDL2main.a.
        set(IGUI_SDL2_LIBS "")
        if(WIN32 AND TARGET SDL2::SDL2main)
            list(APPEND IGUI_SDL2_LIBS SDL2::SDL2main)
        endif()
        list(APPEND IGUI_SDL2_LIBS SDL2::SDL2)
    elseif(EMSCRIPTEN)
        # Emscripten ships SDL2 as a port selected by a flag at compile and
        # link time, so there is no package to find.
        add_library(igui_emscripten_sdl2 INTERFACE)
        target_compile_options(igui_emscripten_sdl2 INTERFACE -sUSE_SDL=2)
        target_link_options(igui_emscripten_sdl2 INTERFACE -sUSE_SDL=2)
        set(IGUI_SDL2_LIBS igui_emscripten_sdl2)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
        set(IGUI_SDL2_LIBS PkgConfig::SDL2)
    endif()

    add_library(igui_sdl2 adapters/sdl2/SdlBackend.cpp)
    add_library(igui::sdl2 ALIAS igui_sdl2)
    target_include_directories(igui_sdl2 PUBLIC adapters/sdl2/include)
    target_link_libraries(igui_sdl2 PUBLIC igui_font ${IGUI_SDL2_LIBS})
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(igui_sdl2 PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(IGUI_BUILD_IMMEDIATE)
        # main.cpp calls ig::Context directly - used to link transitively
        # through the old igui_font -> igui edge; igui_font is igui_core-only
        # now (it never needed Context), so link igui explicitly here.
        add_executable(igui_sdl2_demo examples/sdl2_demo/main.cpp)
        target_link_libraries(igui_sdl2_demo PRIVATE igui_sdl2 igui)
    endif()

    if(IGUI_BUILD_LEGACY)
        add_executable(igui_sdl2_legacy_gui_demo examples/sdl2_legacy_gui_demo/main.cpp)
        target_link_libraries(igui_sdl2_legacy_gui_demo PRIVATE igui_sdl2 igui_legacy)
    endif()

    # igui_widgets itself (and its own test) is built above, gated only on
    # IGUI_BUILD_RETAINED - this only adds the SDL2-backed demo for it, so
    # it additionally needs an SDL2 window to run in.
    if(TARGET igui_widgets)
        add_executable(igui_sdl2_original_widgets_demo
            examples/sdl2_original_widgets/main.cpp
            examples/sdl2_original_widgets/SdlWidgetBridge.cpp
            examples/sdl2_original_widgets/stage_advanced.cpp
            examples/sdl2_original_widgets/stages/stage_demo.cpp
            examples/sdl2_original_widgets/stages/stage_menu.cpp
            examples/sdl2_original_widgets/stages/stage_basic.cpp
            examples/sdl2_original_widgets/stages/stage_controls.cpp
            examples/sdl2_original_widgets/stages/stage_scroll.cpp
            examples/sdl2_original_widgets/stages/stage_inputs.cpp
            examples/sdl2_original_widgets/stages/stage_menus.cpp
            examples/sdl2_original_widgets/stages/stage_dialogs.cpp
            examples/sdl2_original_widgets/stages/stage_dock.cpp
            examples/sdl2_original_widgets/stages/stage_properties.cpp
            examples/sdl2_original_widgets/stages/stage_video.cpp
            examples/sdl2_original_widgets/stages/stage_sidetabs.cpp)
        target_compile_features(igui_sdl2_original_widgets_demo PRIVATE cxx_std_17)
        target_include_directories(igui_sdl2_original_widgets_demo PRIVATE
            examples/sdl2_original_widgets)
        target_link_libraries(igui_sdl2_original_widgets_demo PRIVATE
            igui_widgets ${IGUI_SDL2_LIBS})
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(igui_sdl2_original_widgets_demo PRIVATE -fsanitize=address)
            target_link_options(igui_sdl2_original_widgets_demo PRIVATE -fsanitize=address)
        endif()
    endif()

    if(IGUI_BUILD_WEB AND EMSCRIPTEN)
        set(IGUI_WEB_LINK_OPTIONS
            -sALLOW_MEMORY_GROWTH=1
            -sNO_EXIT_RUNTIME=1
            --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)

        if(IGUI_BUILD_IMMEDIATE)
            add_executable(igui_web_immediate examples/web/immediate_gallery.cpp)
            # The synth gallery section uses the same knob the synth demo draws,
            # so the two stay the same widget instead of drifting apart.
            target_include_directories(igui_web_immediate PRIVATE examples/synth_demo)
            target_link_libraries(igui_web_immediate PRIVATE igui_sdl2 igui)
            target_link_options(igui_web_immediate PRIVATE ${IGUI_WEB_LINK_OPTIONS})
            set_target_properties(igui_web_immediate PROPERTIES
                OUTPUT_NAME immediate SUFFIX ".html")
        endif()

        if(TARGET igui_widgets)
            add_executable(igui_web_retained
                examples/web/retained_gallery.cpp
                examples/sdl2_original_widgets/SdlWidgetBridge.cpp
                examples/sdl2_original_widgets/stage_advanced.cpp
                examples/sdl2_original_widgets/stages/stage_demo.cpp
                examples/sdl2_original_widgets/stages/stage_menu.cpp
                examples/sdl2_original_widgets/stages/stage_basic.cpp
                examples/sdl2_original_widgets/stages/stage_controls.cpp
                examples/sdl2_original_widgets/stages/stage_scroll.cpp
                examples/sdl2_original_widgets/stages/stage_inputs.cpp
                examples/sdl2_original_widgets/stages/stage_menus.cpp
                examples/sdl2_original_widgets/stages/stage_dialogs.cpp
                examples/sdl2_original_widgets/stages/stage_dock.cpp
                examples/sdl2_original_widgets/stages/stage_properties.cpp
                examples/sdl2_original_widgets/stages/stage_video.cpp
                examples/sdl2_original_widgets/stages/stage_sidetabs.cpp)
            target_compile_features(igui_web_retained PRIVATE cxx_std_17)
            target_include_directories(igui_web_retained PRIVATE examples/sdl2_original_widgets)
            target_link_libraries(igui_web_retained PRIVATE igui_widgets ${IGUI_SDL2_LIBS})
            target_link_options(igui_web_retained PRIVATE ${IGUI_WEB_LINK_OPTIONS})
            set_target_properties(igui_web_retained PROPERTIES
                OUTPUT_NAME retained SUFFIX ".html")
        endif()
    endif()

    # Also calls ig::Context directly - same reasoning as igui_sdl2_demo.
    if(IGUI_BUILD_TESTS AND IGUI_BUILD_IMMEDIATE)
        add_executable(igui_sdl2_tests tests/test_sdl2.cpp)
        target_link_libraries(igui_sdl2_tests PRIVATE igui_sdl2 igui)
        add_test(NAME igui_sdl2 COMMAND igui_sdl2_tests)
    endif()
endif()

# A galeria comparada lado a lado precisa do backend OpenGL e de SDL2.
if(IGUI_BUILD_COMPARE_GALLERY)
    if(NOT IGUI_BUILD_SDL2_DEMO)
        message(FATAL_ERROR
            "IGUI_BUILD_COMPARE_GALLERY=ON needs IGUI_BUILD_SDL2_DEMO=ON (it links igui::sdl2)")
    endif()
    add_subdirectory(examples/compare_gallery)
endif()

# Os alvos de teste são feitos de assert(); o ajuste que os mantém vivos em builds
# com NDEBUG (ver igui_test_keep_asserts) tem de correr depois de todos existirem.
foreach(igui_test_target IN LISTS IGUI_TEST_TARGETS)
    igui_test_keep_asserts(${igui_test_target})
endforeach()

# Um relatorio de um sanitizador na saida de um teste que sai com 0 e um teste que falha: o ctest nao mostra a saida de
# um teste que passou, e um aviso que nao para o programa (UBSan sem -fno-sanitize-recover) nunca se via. Tem de correr
# depois de todos os testes existirem (so apanha os desta pasta).
if(IGUI_FAIL_ON_SANITIZER_REPORT AND IGUI_BUILD_TESTS)
    get_directory_property(igui_all_tests TESTS)
    if(igui_all_tests)
        set_tests_properties(${igui_all_tests} PROPERTIES
            FAIL_REGULAR_EXPRESSION "runtime error:;ERROR: AddressSanitizer;ERROR: LeakSanitizer;SUMMARY: [A-Za-z]*Sanitizer")
    endif()
endif()
