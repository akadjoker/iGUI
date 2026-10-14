#pragma once
#include <igui/widgets/String.hpp>
#include <cstddef>
#include <cstdint>

namespace ig { namespace retained
{
// ── UTF-8 helpers (inline) ──────────────────────────────────────────────
// A byte that does not start a complete sequence counts as one character, as
// the font decoder draws it; the string's terminator ends any sequence.
inline int utf8CharLen(const char* p)
{
    const unsigned char c = static_cast<unsigned char>(*p);
    int len = 1;
    if ((c & 0xE0) == 0xC0) len = 2;
    else if ((c & 0xF0) == 0xE0) len = 3;
    else if ((c & 0xF8) == 0xF0) len = 4;
    for (int i = 1; i < len; ++i)
        if ((static_cast<unsigned char>(p[i]) & 0xC0) != 0x80)
            return 1;
    return len;
}

inline uint32_t utf8Decode(const char* p, int& len)
{
    const unsigned char* s = reinterpret_cast<const unsigned char*>(p);
    len = utf8CharLen(p);
    switch (len)
    {
    case 2: return static_cast<uint32_t>(((s[0] & 0x1F) << 6) | (s[1] & 0x3F));
    case 3: return static_cast<uint32_t>(((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F));
    case 4:
        return static_cast<uint32_t>(((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
                                     ((s[2] & 0x3F) << 6) | (s[3] & 0x3F));
    default: return s[0] < 0x80 ? s[0] : 0xFFFDu;
    }
}

inline int utf8Length(const String& s)
{
    int count = 0;
    const char* p = s.c_str();
    const char* end = p + s.size();
    while (p < end) { p += utf8CharLen(p); ++count; }
    return count;
}

inline size_t utf8ByteOffset(const String& s, int cpIndex)
{
    const char* p = s.c_str();
    const char* end = p + s.size();
    int i = 0;
    while (i < cpIndex && p < end) { p += utf8CharLen(p); ++i; }
    return static_cast<size_t>(p - s.c_str());
}

inline String utf8Substr(const String& s, int cpStart, int cpEnd)
{
    size_t byteStart = utf8ByteOffset(s, cpStart);
    size_t byteEnd   = utf8ByteOffset(s, cpEnd);
    return s.substr(byteStart, byteEnd - byteStart);
}

} // namespace retained
} // namespace ig
