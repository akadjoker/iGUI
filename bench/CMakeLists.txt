# Headless comparison between the immediate-mode Context and ImGui.
#
# Enabled from the top level with -DIGUI_BUILD_BENCH=ON, and pointed at an ImGui
# source tree with -DIGUI_BENCH_IMGUI_DIR=/path/to/imgui-1.92.5. Without that
# path the iGUI side still builds and runs on its own (it reports the ImGui row
# as unavailable), so the benchmark can double as a plain iGUI profile.
#
# ImGui is consumed as source, not as a package: only the four files a
# backend-less build needs are compiled (imgui_demo.cpp is not).

set(_igui_bench_sources
    main.cpp
    IguiRunner.cpp)

if(IGUI_BENCH_IMGUI_DIR)
    if(NOT EXISTS "${IGUI_BENCH_IMGUI_DIR}/imgui.h" OR
       NOT EXISTS "${IGUI_BENCH_IMGUI_DIR}/imgui.cpp")
        message(FATAL_ERROR
            "IGUI_BENCH_IMGUI_DIR='${IGUI_BENCH_IMGUI_DIR}' is not an ImGui source tree "
            "(no imgui.h / imgui.cpp)")
    endif()
    list(APPEND _igui_bench_sources ImguiRunner.cpp)
endif()

add_executable(igui_bench ${_igui_bench_sources})
target_compile_features(igui_bench PRIVATE cxx_std_14)
target_link_libraries(igui_bench PRIVATE igui_core igui)

# iGUI's real font code measures text the way a deployed build does. Without it
# the benchmark falls back to a fixed-advance measurer, which is why the row
# reports which one was used.
if(TARGET igui_font)
    target_link_libraries(igui_bench PRIVATE igui_font)
    target_compile_definitions(igui_bench PRIVATE IGUI_BENCH_HAS_FONT=1)
else()
    target_compile_definitions(igui_bench PRIVATE IGUI_BENCH_HAS_FONT=0)
endif()

if(IGUI_BENCH_IMGUI_DIR)
    add_library(igui_bench_imgui STATIC
        "${IGUI_BENCH_IMGUI_DIR}/imgui.cpp"
        "${IGUI_BENCH_IMGUI_DIR}/imgui_draw.cpp"
        "${IGUI_BENCH_IMGUI_DIR}/imgui_tables.cpp"
        "${IGUI_BENCH_IMGUI_DIR}/imgui_widgets.cpp")
    target_include_directories(igui_bench_imgui PUBLIC "${IGUI_BENCH_IMGUI_DIR}")
    target_compile_features(igui_bench_imgui PUBLIC cxx_std_14)
    target_link_libraries(igui_bench PRIVATE igui_bench_imgui)
    target_compile_definitions(igui_bench PRIVATE IGUI_BENCH_HAS_IMGUI=1)
else()
    target_compile_definitions(igui_bench PRIVATE IGUI_BENCH_HAS_IMGUI=0)
endif()

# Microsecond numbers from an unoptimized build measure the build, not the
# library, and optimizing only one side of the comparison would be worse than
# useless: warn and let the numbers say what they are.
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING
        "igui_bench: CMAKE_BUILD_TYPE is '${CMAKE_BUILD_TYPE}'. The libraries are "
        "unoptimized, so the numbers do not compare libraries. Configure with "
        "-DCMAKE_BUILD_TYPE=Release for a meaningful run.")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(igui_bench PRIVATE -Wall -Wextra -Wpedantic)
endif()

# A short run so `ctest` notices a broken benchmark, not a 300-frame one.
if(IGUI_BUILD_TESTS)
    add_test(NAME igui_bench_smoke
             COMMAND igui_bench --frames=5 --warmup=2 --blocks=2 --scene=gallery)
endif()
