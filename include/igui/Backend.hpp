#pragma once

#include "DrawData.hpp"

namespace ig
{

struct TextMetrics
{
    float width;
    float height;
    float ascent;
    float descent;

    TextMetrics()
        : width(0.0f), height(0.0f), ascent(0.0f), descent(0.0f)
    {
    }
};

class Backend
{
public:
    virtual ~Backend() {}

    virtual TextMetrics measureText(FontId font, StringView text,
                                    float logicalSize, float dpiScale) = 0;
    // Each command clip is in logical display coordinates and must be honoured.
    virtual bool render(const DrawData &data) = 0;

    // Platform clipboard hooks. Backends that cannot access a native clipboard
    // can keep the defaults; text input will simply ignore those requests.
    virtual String clipboardText() { return String(); }
    virtual bool setClipboardText(StringView) { return false; }
};

// Fonte independente do backend: mede e escreve glyphs no DrawList.
class TextProvider
{
public:
    virtual ~TextProvider() {}

    virtual TextMetrics measureText(FontId font, StringView text,
                                    float logicalSize) const = 0;
    virtual bool appendText(DrawList &drawList, FontId font, StringView text,
                            const Vec2 &position, float logicalSize,
                            const Color &color, const Rect &clip) const = 0;
};

} // namespace ig
