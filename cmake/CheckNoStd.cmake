# Contrato: o core imediato (o Context e o motor de desenho) não usa a STL - usa os
# contentores `ct`. Isto não é um target de build, é a verificação que o garante (o
# igui_core depende dele, por isso o core não compila antes de isto passar).
file(GLOB_RECURSE IG_CORE_FILES
     "${IGUI_SOURCE_DIR}/include/igui/*.hpp"
     "${IGUI_SOURCE_DIR}/src/*.cpp"
     "${IGUI_SOURCE_DIR}/tests/*.cpp")

# The original retained widget library intentionally uses STL containers.
# Keep the no-std contract scoped to the immediate-mode core only.
list(FILTER IG_CORE_FILES EXCLUDE REGEX "/include/igui/widgets/")
list(FILTER IG_CORE_FILES EXCLUDE REGEX "/src/widgets/")
list(FILTER IG_CORE_FILES EXCLUDE REGEX "/tests/test_original_widgets\\.cpp$")
# The compatibility GUI was moved under include/src and intentionally uses STL.
list(FILTER IG_CORE_FILES EXCLUDE REGEX "/include/igui/iGUI\\.hpp$")
list(FILTER IG_CORE_FILES EXCLUDE REGEX "/src/iGUI\\.cpp$")

foreach(IG_FILE IN LISTS IG_CORE_FILES)
    file(READ "${IG_FILE}" IG_CONTENT)
    string(REGEX MATCH "std[ \t\r\n]*::" IG_STD_MATCH "${IG_CONTENT}")
    if(IG_STD_MATCH)
        message(FATAL_ERROR "std namespace use found in: ${IG_FILE}")
    endif()
    string(REGEX MATCH "using[ \t\r\n]+namespace[ \t\r\n]+std" IG_USING_STD_MATCH "${IG_CONTENT}")
    if(IG_USING_STD_MATCH)
        message(FATAL_ERROR "using namespace std found in: ${IG_FILE}")
    endif()
    string(REGEX MATCH "#[ \t]*include[ \t]*[<\"](vector|string|unordered_map|map|functional|algorithm|memory|array|deque|queue|stack)[>\"]" IG_STL_INCLUDE_MATCH "${IG_CONTENT}")
    if(IG_STL_INCLUDE_MATCH)
        message(FATAL_ERROR "STL container or algorithm header found in: ${IG_FILE}")
    endif()
endforeach()

message(STATUS "igui_check_no_std: core contains no std::")
