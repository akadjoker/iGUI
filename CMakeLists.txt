cmake_minimum_required(VERSION 3.14)
project(igui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(IGUI_BUILD_TESTS "Build iGUI core tests" ON)
option(IGUI_BUILD_FONT "Build the optional embedded font atlas module" OFF)
set(IGUI_FONT_INPUT "" CACHE FILEPATH
    "Optional TTF/OTF file embedded as iGUI's default font (defaults to Nuklear's Roboto)")
option(IGUI_BUILD_SDL2_DEMO "Build the optional SDL2 integration example" OFF)
option(IGUI_BUILD_RAYLIB_BACKEND "Build the Raylib backend when Raylib is installed" ON)
option(IGUI_BUILD_RAYLIB_DEMO "Build the Raylib integration example" OFF)

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

if(IGUI_BUILD_SDL2_DEMO OR IGUI_RAYLIB_AVAILABLE)
    set(IGUI_BUILD_FONT ON)
endif()

include(FetchContent)

FetchContent_Declare(
    igui_containers
    GIT_REPOSITORY https://github.com/akadjoker/containers.git
    GIT_TAG f94b31807048ff5e4df8cb3884094a63675544d9
)
FetchContent_GetProperties(igui_containers)
if(NOT igui_containers_POPULATED)
    FetchContent_Populate(igui_containers)
endif()

add_library(ct INTERFACE)
target_include_directories(ct INTERFACE "${igui_containers_SOURCE_DIR}/include")

add_library(igui
    src/DrawList.cpp
    src/FileDialog.cpp
    src/Gui.cpp
)
add_library(igui::igui ALIAS igui)
target_include_directories(igui PUBLIC include)
target_link_libraries(igui PUBLIC ct)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(igui PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_target(igui_no_std ALL
    COMMAND ${CMAKE_COMMAND}
            -DIGUI_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckNoStd.cmake
    VERBATIM
)
add_dependencies(igui igui_no_std)

# The pre-existing retained GUI API is kept as a separate compatibility layer.
# It uses STL containers, unlike the immediate-mode core above.
add_library(igui_legacy src/iGUI.cpp)
add_library(igui::legacy ALIAS igui_legacy)
target_include_directories(igui_legacy PUBLIC include)
target_link_libraries(igui_legacy PUBLIC igui)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(igui_legacy PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(IGUI_BUILD_TESTS)
    enable_testing()
    add_executable(igui_tests tests/test_core.cpp)
    target_link_libraries(igui_tests PRIVATE igui_legacy)
    add_test(NAME igui_core COMMAND igui_tests)

    add_executable(igui_widget_gallery_tests tests/test_widgets.cpp)
    target_link_libraries(igui_widget_gallery_tests PRIVATE igui)
    add_test(NAME igui_widget_gallery COMMAND igui_widget_gallery_tests)

endif()

if(IGUI_BUILD_FONT)
    FetchContent_Declare(
        igui_nuklear_assets
        GIT_REPOSITORY https://github.com/Immediate-Mode-UI/Nuklear.git
        GIT_TAG 0dbc52f86404f9e1f26ce0df3015ed23ff54a726
    )
    FetchContent_GetProperties(igui_nuklear_assets)
    if(NOT igui_nuklear_assets_POPULATED)
        FetchContent_Populate(igui_nuklear_assets)
    endif()

    if(IGUI_FONT_INPUT)
        set(_igui_font_input "${IGUI_FONT_INPUT}")
    else()
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
        PRIVATE "${igui_nuklear_assets_SOURCE_DIR}/src"
    )
    target_link_libraries(igui_font PUBLIC igui)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(igui_font PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(IGUI_BUILD_TESTS)
        add_executable(igui_font_tests tests/test_font.cpp)
        target_link_libraries(igui_font_tests PRIVATE igui_font)
        add_test(NAME igui_font COMMAND igui_font_tests)
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
    target_link_libraries(igui_raylib PUBLIC igui_font igui_raylib_native)
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
        target_link_libraries(igui_raylib_dock_demo PRIVATE igui_raylib)
    endif()
endif()

if(IGUI_BUILD_SDL2_DEMO)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

    add_library(igui_sdl2 adapters/sdl2/SdlBackend.cpp)
    add_library(igui::sdl2 ALIAS igui_sdl2)
    target_include_directories(igui_sdl2 PUBLIC adapters/sdl2/include)
    target_link_libraries(igui_sdl2 PUBLIC igui_font PkgConfig::SDL2)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(igui_sdl2 PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_executable(igui_sdl2_demo examples/sdl2_demo/main.cpp)
    target_link_libraries(igui_sdl2_demo PRIVATE igui_sdl2)

    add_executable(igui_sdl2_legacy_gui_demo examples/sdl2_legacy_gui_demo/main.cpp)
    target_link_libraries(igui_sdl2_legacy_gui_demo PRIVATE igui_sdl2 igui_legacy)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb/stb_impl.cpp")
        add_library(igui_widgets_stb STATIC third_party/stb/stb_impl.cpp)
        target_include_directories(igui_widgets_stb PUBLIC third_party/stb)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(igui_widgets_stb PRIVATE
                -Wno-unused-function -Wno-unused-but-set-variable
                -Wno-array-bounds)
        endif()

        set(IGUI_WIDGET_SOURCES
            src/widgets/Animation.cpp
            src/widgets/AppWidgets.cpp
            src/widgets/AssetBrowser.cpp
            src/widgets/AudioWidgets.cpp
            src/widgets/AutomotiveWidgets.cpp
            src/widgets/BasicWidgets.cpp
            src/widgets/Retained.cpp
            src/widgets/BuImage.cpp
            src/widgets/ChartWidgets.cpp
            src/widgets/CodeEditor.cpp
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
            src/widgets/ScrollWidgets.cpp
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
        target_link_libraries(igui_widgets PUBLIC igui PRIVATE igui_widgets_stb)
        target_precompile_headers(igui_widgets PRIVATE src/widgets/pch.hpp)

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
            examples/sdl2_original_widgets/stages/stage_properties.cpp)
        target_compile_features(igui_sdl2_original_widgets_demo PRIVATE cxx_std_17)
        target_include_directories(igui_sdl2_original_widgets_demo PRIVATE
            examples/sdl2_original_widgets)
        target_link_libraries(igui_sdl2_original_widgets_demo PRIVATE
            igui_widgets PkgConfig::SDL2)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(igui_sdl2_original_widgets_demo PRIVATE -fsanitize=address)
            target_link_options(igui_sdl2_original_widgets_demo PRIVATE -fsanitize=address)
        endif()
        if(IGUI_BUILD_TESTS)
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
        endif()
    endif()

    if(IGUI_BUILD_TESTS)
        add_executable(igui_sdl2_tests tests/test_sdl2.cpp)
        target_link_libraries(igui_sdl2_tests PRIVATE igui_sdl2)
        add_test(NAME igui_sdl2 COMMAND igui_sdl2_tests)
    endif()
endif()
