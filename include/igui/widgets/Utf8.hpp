#pragma once
#include <string>
#include <cstddef>

namespace BuGUI
{
// ── UTF-8 helpers (inline) ──────────────────────────────────────────────
inline int utf8CharLen(const char* p)
{
    unsigned char c = static_cast<unsigned char>(*p);
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

inline int utf8Length(const std::string& s)
{
    int count = 0;
    const char* p = s.c_str();
    const char* end = p + s.size();
    while (p < end) { p += utf8CharLen(p); ++count; }
    return count;
}

inline size_t utf8ByteOffset(const std::string& s, int cpIndex)
{
    const char* p = s.c_str();
    const char* end = p + s.size();
    int i = 0;
    while (i < cpIndex && p < end) { p += utf8CharLen(p); ++i; }
    return static_cast<size_t>(p - s.c_str());
}

inline std::string utf8Substr(const std::string& s, int cpStart, int cpEnd)
{
    size_t byteStart = utf8ByteOffset(s, cpStart);
    size_t byteEnd   = utf8ByteOffset(s, cpEnd);
    return s.substr(byteStart, byteEnd - byteStart);
}

} // namespace BuGUI
