#pragma once

#include <raylib.h>

#include <ct/vector.hpp>

#include <igui/FontAtlas.hpp>
#include <igui/Gui.hpp>

namespace ig
{
namespace raylib
{

// Translate Raylib's frame-based input state into the platform-neutral events
// consumed by ig::Context. Call once before Context::beginFrame().
void processInput(Context &context);

// Uses the active Raylib window dimensions. Raylib coordinates are already
// logical display coordinates, so dpiScale defaults to 1.
FrameInfo frameInfo(float deltaSeconds, float dpiScale = 1.0f);

// TextureId is the Raylib/OpenGL texture id stored as an opaque integer.
TextureId textureId(const ::Texture2D &texture);

// DrawData renderer for an active Raylib BeginDrawing()/EndDrawing() block.
// The caller owns clearing and presenting the Raylib drawing frame.
class Backend : public ig::Backend
{
public:
    Backend();
    Backend(Span<const FontRange> ranges, uint32_t atlasWidth,
            uint32_t atlasHeight, float bakedSize = 14.0f);
    ~Backend() override;

    FontAtlas &fontAtlas();
    bool prepareFontAtlas();

    TextMetrics measureText(FontId font, StringView text,
                            float logicalSize, float dpiScale) override;
    bool render(const DrawData &data) override;

private:
    FontAtlas fontAtlas_;
    ::Texture2D fontTexture_;
    ct::Vector<uint8_t> fontPixels_;

    bool ensureFontTexture();
    bool renderGeometry(const DrawData &data, const GeometryCommand &command);
    bool renderText(const DrawData &data, const TextCommand &command);
};

} // namespace raylib
} // namespace ig
