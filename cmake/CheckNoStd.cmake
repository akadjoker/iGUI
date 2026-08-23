file(GLOB_RECURSE IG_CORE_FILES
     "${IGUI_SOURCE_DIR}/include/igui/*.hpp"
     "${IGUI_SOURCE_DIR}/src/*.cpp"
     "${IGUI_SOURCE_DIR}/tests/*.cpp")

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

message(STATUS "igui_no_std: core contains no std::")
