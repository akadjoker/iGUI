# A galeria comparada: o demo do ImGui (ShowDemoWindow) de um lado e o mesmo demo
# reescrito com o iGUI do outro, lado a lado, na mesma janela SDL2 com contexto
# OpenGL 3.3.
#
# Precisa de SDL2, de um contexto OpenGL (o backend igui::opengl não liga a
# libGL; o SDL dá-lhe as funções) e de uma árvore de código do ImGui, a mesma que
# o benchmark usa (IGUI_BENCH_IMGUI_DIR).
#
# O levantamento do que falta ao iGUI face ao demo, e o TODO da paridade, está em
# examples/compare_gallery/GAPS.md.

if(NOT IGUI_BUILD_OPENGL_BACKEND)
    message(FATAL_ERROR "IGUI_BUILD_COMPARE_GALLERY=ON needs IGUI_BUILD_OPENGL_BACKEND=ON")
endif()

find_package(SDL2 QUIET)
if(NOT SDL2_FOUND AND NOT TARGET SDL2::SDL2)
    pkg_check_modules(IGUI_COMPARE_SDL2 REQUIRED IMPORTED_TARGET sdl2)
endif()

if(NOT IGUI_BENCH_IMGUI_DIR OR NOT EXISTS "${IGUI_BENCH_IMGUI_DIR}/imgui.h")
    message(FATAL_ERROR
        "IGUI_BUILD_COMPARE_GALLERY=ON needs IGUI_BENCH_IMGUI_DIR pointing at an ImGui source tree")
endif()

add_library(igui_compare_imgui STATIC
    "${IGUI_BENCH_IMGUI_DIR}/imgui.cpp"
    "${IGUI_BENCH_IMGUI_DIR}/imgui_draw.cpp"
    "${IGUI_BENCH_IMGUI_DIR}/imgui_tables.cpp"
    "${IGUI_BENCH_IMGUI_DIR}/imgui_widgets.cpp"
    # O demo window vive aqui, e é ele o lado do ImGui nesta comparação.
    "${IGUI_BENCH_IMGUI_DIR}/imgui_demo.cpp"
    "${IGUI_BENCH_IMGUI_DIR}/backends/imgui_impl_sdl2.cpp"
    "${IGUI_BENCH_IMGUI_DIR}/backends/imgui_impl_opengl3.cpp")
target_include_directories(igui_compare_imgui PUBLIC
    "${IGUI_BENCH_IMGUI_DIR}"
    "${IGUI_BENCH_IMGUI_DIR}/backends")
target_compile_features(igui_compare_imgui PUBLIC cxx_std_14)
# O imgui_impl_sdl2 inclui <SDL.h>; a SDL2 traz esse diretório em dois sítios
# conforme a instalação, e o pkg-config sabe qual.
if(TARGET SDL2::SDL2)
    target_link_libraries(igui_compare_imgui PUBLIC SDL2::SDL2)
else()
    target_link_libraries(igui_compare_imgui PUBLIC PkgConfig::IGUI_COMPARE_SDL2)
endif()

# A fonte dos dois painéis. O demo do ImGui desenha com a fonte de omissão dele
# (ProggyClean, que vive em misc/fonts na árvore do ImGui); usar a mesma do lado
# do iGUI é o que faz dos dois painéis o mesmo ecrã - uma fonte vetorial pedida no
# mesmo tamanho sai mais pequena e mais clara que uma de pixel, e a comparação
# passaria a ser de glifos. IGUI_GALLERY_FONT aponta para outra TTF/OTF, ou vazio
# para ficar com a fonte de omissão do iGUI.
set(IGUI_GALLERY_FONT "${IGUI_BENCH_IMGUI_DIR}/misc/fonts/ProggyClean.ttf" CACHE FILEPATH
    "Font both panels of the comparison gallery are drawn with (empty: iGUI's default)")

# As faces extra que a secção "Fonts" do demo mostra (ver gallery::demoFontFiles):
# são as TTF/OTF que o ImGui traz em misc/fonts, carregadas pelos dois lados - o
# iGUI todas no mesmo atlas, o ImGui uma por AddFontFromFileTTF. Sem o diretório
# (ou sem os ficheiros) a secção fica só com a face do painel.
set(IGUI_GALLERY_FONT_DIR "${IGUI_BENCH_IMGUI_DIR}/misc/fonts" CACHE PATH
    "Directory with the extra font faces the gallery's Fonts section shows (empty: none)")

add_executable(igui_compare_gallery
    main.cpp
    Gallery.cpp
    GalleryIgui.cpp
    GalleryImgui.cpp
    Screenshot.cpp)
target_compile_features(igui_compare_gallery PRIVATE cxx_std_14)
target_compile_definitions(igui_compare_gallery PRIVATE
    IGUI_GALLERY_FONT="${IGUI_GALLERY_FONT}"
    IGUI_GALLERY_FONT_DIR="${IGUI_GALLERY_FONT_DIR}")
target_include_directories(igui_compare_gallery PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/third_party/stb")
target_link_libraries(igui_compare_gallery PRIVATE
    igui::igui
    igui::font
    igui::opengl
    igui::sdl2
    igui_compare_imgui)

if(TARGET SDL2::SDL2)
    target_link_libraries(igui_compare_gallery PRIVATE SDL2::SDL2)
else()
    target_link_libraries(igui_compare_gallery PRIVATE PkgConfig::IGUI_COMPARE_SDL2)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(igui_compare_gallery PRIVATE -Wall -Wextra)
endif()

# Números de microssegundos vindos de um build sem otimizações medem o build, não
# as bibliotecas - e otimizar só um lado seria pior do que inútil.
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING
        "igui_compare_gallery: CMAKE_BUILD_TYPE is '${CMAKE_BUILD_TYPE}'. As bibliotecas "
        "estao sem otimizacoes, por isso os tempos nao comparam bibliotecas. Configure com "
        "-DCMAKE_BUILD_TYPE=Release.")
endif()
