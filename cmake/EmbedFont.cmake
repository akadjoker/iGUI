if(NOT DEFINED IGUI_FONT_INPUT OR NOT DEFINED IGUI_FONT_OUTPUT)
    message(FATAL_ERROR "IGUI_FONT_INPUT and IGUI_FONT_OUTPUT are required")
endif()

get_filename_component(IGUI_FONT_OUTPUT_DIR "${IGUI_FONT_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${IGUI_FONT_OUTPUT_DIR}")
file(READ "${IGUI_FONT_INPUT}" IGUI_FONT_HEX HEX)
string(REGEX REPLACE "([0-9A-Fa-f][0-9A-Fa-f])" "0x\\1," IGUI_FONT_BYTES "${IGUI_FONT_HEX}")
file(WRITE "${IGUI_FONT_OUTPUT}" "#include <stdint.h>\n\nnamespace ig { namespace detail {\nextern const uint8_t DefaultFontData[] = {${IGUI_FONT_BYTES}};\nextern const uint32_t DefaultFontDataSize = sizeof(DefaultFontData);\n} } // namespace ig::detail\n")
