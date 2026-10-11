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

    // Which texture, and which texel of it, solid-colour geometry should use.
    // A provider whose atlas holds a white texel returns the atlas and that
    // texel: shapes and glyphs then come from one texture, so a backend that
    // merges consecutive geometry with the same texture gets one draw command
    // per run instead of one per switch between them. The default leaves
    // geometry untextured, which is what a provider without such a texel wants.
    struct SolidStyle
    {
        TextureId texture;
        Vec2 uv;

        SolidStyle() : texture(), uv(0.0f, 0.0f) {}
    };

    virtual SolidStyle solidStyle() const { return SolidStyle(); }
};

} // namespace ig
