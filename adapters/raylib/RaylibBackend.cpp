#include "igui_raylib/RaylibBackend.hpp"

#include <math.h>

#include <rlgl.h>

namespace ig
{
namespace raylib
{

namespace
{

::Color nativeColor(const Color &color)
{
    ::Color result;
    result.r = color.r;
    result.g = color.g;
    result.b = color.b;
    result.a = color.a;
    return result;
}

bool keyCode(int nativeKey, KeyCode &key)
{
    switch (nativeKey)
    {
    case KEY_BACKSPACE: key = KeyCode::Backspace; return true;
    case KEY_ENTER: key = KeyCode::Enter; return true;
    case KEY_DELETE: key = KeyCode::Delete; return true;
    case KEY_TAB: key = KeyCode::Tab; return true;
    case KEY_LEFT: key = KeyCode::Left; return true;
    case KEY_RIGHT: key = KeyCode::Right; return true;
    case KEY_UP: key = KeyCode::Up; return true;
    case KEY_DOWN: key = KeyCode::Down; return true;
    case KEY_HOME: key = KeyCode::Home; return true;
    case KEY_END: key = KeyCode::End; return true;
    case KEY_PAGE_UP: key = KeyCode::PageUp; return true;
    case KEY_PAGE_DOWN: key = KeyCode::PageDown; return true;
    case KEY_ESCAPE: key = KeyCode::Escape; return true;
    case KEY_A: key = KeyCode::A; return true;
    case KEY_C: key = KeyCode::C; return true;
    case KEY_D: key = KeyCode::D; return true;
    case KEY_F: key = KeyCode::F; return true;
    case KEY_H: key = KeyCode::H; return true;
    case KEY_S: key = KeyCode::S; return true;
    case KEY_V: key = KeyCode::V; return true;
    case KEY_X: key = KeyCode::X; return true;
    case KEY_Y: key = KeyCode::Y; return true;
    case KEY_Z: key = KeyCode::Z; return true;
    default: return false;
    }
}

bool appendCodepointEvent(Context &context, int codepoint)
{
    if (codepoint <= 0 || codepoint > 0x10ffff)
        return false;

    char utf8[5] = {};
    uint32_t length = 0u;
    if (codepoint <= 0x7f)
    {
        utf8[0] = static_cast<char>(codepoint);
        length = 1u;
    }
    else if (codepoint <= 0x7ff)
    {
        utf8[0] = static_cast<char>(0xc0 | (codepoint >> 6));
        utf8[1] = static_cast<char>(0x80 | (codepoint & 0x3f));
        length = 2u;
    }
    else if (codepoint <= 0xffff)
    {
        utf8[0] = static_cast<char>(0xe0 | (codepoint >> 12));
        utf8[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
        utf8[2] = static_cast<char>(0x80 | (codepoint & 0x3f));
        length = 3u;
    }
    else
    {
        utf8[0] = static_cast<char>(0xf0 | (codepoint >> 18));
        utf8[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
        utf8[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
        utf8[3] = static_cast<char>(0x80 | (codepoint & 0x3f));
        length = 4u;
    }
    context.pushEvent(Event::textInput(StringView(utf8, length)));
    return true;
}

} // namespace

void processInput(Context &context)
{
    if (!::IsWindowFocused())
        context.pushEvent(Event::focusLost());

    const ::Vector2 pointer = ::GetMousePosition();
    context.pushEvent(Event::pointerMove(pointer.x, pointer.y));

    const int buttons[] = {MOUSE_BUTTON_LEFT, MOUSE_BUTTON_MIDDLE, MOUSE_BUTTON_RIGHT};
    const PointerButton pointerButtons[] = {
        PointerButton::Left, PointerButton::Middle, PointerButton::Right};
    for (uint32_t i = 0u; i < 3u; ++i)
    {
        if (::IsMouseButtonPressed(buttons[i]))
            context.pushEvent(Event::pointerDown(pointerButtons[i], pointer.x, pointer.y));
        if (::IsMouseButtonReleased(buttons[i]))
            context.pushEvent(Event::pointerUp(pointerButtons[i], pointer.x, pointer.y));
    }

    const ::Vector2 wheel = ::GetMouseWheelMoveV();
    if (wheel.x != 0.0f || wheel.y != 0.0f)
    {
        Event event;
        event.type = EventType::PointerWheel;
        event.wheelX = wheel.x;
        event.wheelY = wheel.y;
        context.pushEvent(event);
    }

    const int keys[] = {KEY_BACKSPACE, KEY_ENTER, KEY_DELETE, KEY_TAB,
                        KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_HOME, KEY_END,
                        KEY_PAGE_UP, KEY_PAGE_DOWN, KEY_ESCAPE, KEY_A, KEY_C, KEY_D,
                        KEY_F, KEY_H, KEY_S, KEY_V, KEY_X, KEY_Y, KEY_Z};
    const bool control = ::IsKeyDown(KEY_LEFT_CONTROL) || ::IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shift = ::IsKeyDown(KEY_LEFT_SHIFT) || ::IsKeyDown(KEY_RIGHT_SHIFT);
    for (uint32_t i = 0u; i < sizeof(keys) / sizeof(keys[0]); ++i)
    {
        if (::IsKeyPressed(keys[i]) || ::IsKeyPressedRepeat(keys[i]))
        {
            KeyCode key;
            if (keyCode(keys[i], key))
                context.pushEvent(Event::keyDown(key, control, shift));
        }
    }

    int codepoint = 0;
    while ((codepoint = ::GetCharPressed()) != 0)
        appendCodepointEvent(context, codepoint);
}

FrameInfo frameInfo(float deltaSeconds, float dpiScale)
{
    return FrameInfo(static_cast<float>(::GetScreenWidth()),
                     static_cast<float>(::GetScreenHeight()), dpiScale, deltaSeconds);
}

TextureId textureId(const ::Texture2D &texture)
{
    return TextureId(static_cast<uint64_t>(texture.id));
}

Backend::Backend()
    // Rasterize at twice the usual UI size. Text is normally displayed at
    // 14 px, so downsampling this atlas keeps curved glyph edges sharp.
    : fontAtlas_(defaultFontRanges(), 2048u, 1024u, 28.0f), fontTexture_(), fontPixels_()
{
}

Backend::Backend(Span<const FontRange> ranges, uint32_t atlasWidth,
                 uint32_t atlasHeight, float bakedSize)
    : fontAtlas_(ranges, atlasWidth, atlasHeight, bakedSize), fontTexture_(), fontPixels_()
{
}

Backend::~Backend()
{
    if (fontTexture_.id != 0u && ::IsWindowReady())
        ::UnloadTexture(fontTexture_);
    fontAtlas_.setTexture(TextureId());
}

FontAtlas &Backend::fontAtlas()
{
    return fontAtlas_;
}

bool Backend::prepareFontAtlas()
{
    return ensureFontTexture();
}

TextMetrics Backend::measureText(FontId font, StringView text, float logicalSize, float)
{
    return fontAtlas_.measureText(font, text, logicalSize);
}

String Backend::clipboardText()
{
    if (!::IsWindowReady())
        return String();
    const char *text = ::GetClipboardText();
    return text ? String(text) : String();
}

bool Backend::setClipboardText(StringView text)
{
    if (!::IsWindowReady())
        return false;
    const String copy(text.data(), text.size());
    ::SetClipboardText(copy.c_str());
    return true;
}

bool Backend::ensureFontTexture()
{
    if (fontTexture_.id != 0u)
        return true;
    if (!::IsWindowReady() || !fontAtlas_.valid())
        return false;

    const Span<const uint8_t> alpha = fontAtlas_.pixels();
    fontPixels_.resize(alpha.size() * 4u);
    for (Span<const uint8_t>::size_type i = 0u; i < alpha.size(); ++i)
    {
        const Span<const uint8_t>::size_type pixel = i * 4u;
        fontPixels_[pixel] = 255u;
        fontPixels_[pixel + 1u] = 255u;
        fontPixels_[pixel + 2u] = 255u;
        fontPixels_[pixel + 3u] = alpha[i];
    }

    ::Image image = {};
    image.data = fontPixels_.data();
    image.width = static_cast<int>(fontAtlas_.width());
    image.height = static_cast<int>(fontAtlas_.height());
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    fontTexture_ = ::LoadTextureFromImage(image);
    if (fontTexture_.id == 0u)
        return false;
    // A font atlas contains alpha coverage, not pixel art. Bilinear sampling
    // keeps the high-resolution glyphs smooth when their logical size differs
    // from the baked size.
    ::SetTextureFilter(fontTexture_, TEXTURE_FILTER_BILINEAR);
    fontAtlas_.setTexture(textureId(fontTexture_));
    return true;
}

bool Backend::renderGeometry(const DrawData &data, const GeometryCommand &command)
{
    if (command.firstIndex > data.indices.size() ||
        command.indexCount > data.indices.size() - command.firstIndex)
        return false;
    for (uint32_t i = 0u; i < command.indexCount; ++i)
    {
        const int32_t resolved = static_cast<int32_t>(data.indices[command.firstIndex + i]) +
                                 command.vertexOffset;
        if (resolved < 0 || static_cast<uint32_t>(resolved) >= data.vertices.size())
            return false;
    }

    // rlBegin() resets the active batch texture when the primitive mode
    // changes. Select the image after it so glyph quads sample their atlas
    // instead of Raylib's solid-white default texture.
    ::rlBegin(RL_TRIANGLES);
    ::rlSetTexture(static_cast<unsigned int>(command.texture.value));
    // Emit triangles with inverted winding so they are front-facing under
    // Raylib's default backface culling (GL_CULL_FACE is enabled by rlgl).
    for (uint32_t tri = 0u; tri + 2u < command.indexCount; tri += 3u)
    {
        for (uint32_t k = 0u; k < 3u; ++k)
        {
            const uint32_t i = tri + (2u - k);
            const DrawVertex &vertex = data.vertices[static_cast<uint32_t>(
                static_cast<int32_t>(data.indices[command.firstIndex + i]) + command.vertexOffset)];
            ::rlColor4ub(vertex.color.r, vertex.color.g, vertex.color.b, vertex.color.a);
            ::rlTexCoord2f(vertex.uv.x, vertex.uv.y);
            ::rlVertex2f(vertex.position.x, vertex.position.y);
        }
    }
    ::rlEnd();
    ::rlSetTexture(0u);
    return true;
}

bool Backend::renderText(const DrawData &data, const TextCommand &command)
{
    if (command.textOffset > data.textBytes.size() ||
        command.textSize > data.textBytes.size() - command.textOffset || !ensureFontTexture())
        return false;

    const float scale = command.logicalSize / fontAtlas_.bakedSize();
    float penX = command.position.x;
    float penY = command.position.y + fontAtlas_.ascent() * scale;
    const StringView text(data.textBytes.data() + command.textOffset, command.textSize);
    StringView::size_type offset = 0u;
    uint32_t codepoint = 0u;
    while (decodeUtf8(text, offset, codepoint))
    {
        if (codepoint == static_cast<uint32_t>('\n'))
        {
            penX = command.position.x;
            penY += fontAtlas_.lineHeight() * scale;
            continue;
        }
        const FontGlyph *glyph = fontAtlas_.glyph(command.font, codepoint);
        if (!glyph)
            continue;
        if (glyph->width != 0u && glyph->height != 0u)
        {
            const ::Rectangle source = {static_cast<float>(glyph->x), static_cast<float>(glyph->y),
                                        static_cast<float>(glyph->width), static_cast<float>(glyph->height)};
            const ::Rectangle destination = {floorf(penX + glyph->offsetX * scale + 0.5f),
                                             floorf(penY + glyph->offsetY * scale + 0.5f),
                                             static_cast<float>(glyph->width) * scale,
                                             static_cast<float>(glyph->height) * scale};
            ::DrawTexturePro(fontTexture_, source, destination, ::Vector2{0.0f, 0.0f},
                             0.0f, nativeColor(command.color));
        }
        penX += glyph->advance * scale;
    }
    return true;
}

bool Backend::render(const DrawData &data)
{
    if (!::IsWindowReady())
        return false;

    // Raw rlBegin/rlEnd immediate mode bypasses Raylib's 2D state management.
    // rlEnd() bumps the depth per draw, so with the depth test enabled later
    // commands would be rejected against earlier ones. 2D UI needs no depth.
    ::rlDisableDepthTest();
    ::rlDisableDepthMask();
    ::rlDisableBackfaceCulling();
    ::rlEnableColorBlend();
    ::rlSetBlendMode(RL_BLEND_ALPHA);

    for (Span<const DrawCommand>::size_type i = 0u; i < data.commands.size(); ++i)
    {
        const DrawCommand &command = data.commands[i];
        const Rect &clip = command.type == DrawCommandType::Geometry
                               ? command.payload.geometry.clip : command.payload.text.clip;
        const int left = static_cast<int>(floorf(clip.x));
        const int top = static_cast<int>(floorf(clip.y));
        const int right = static_cast<int>(ceilf(clip.x + clip.width));
        const int bottom = static_cast<int>(ceilf(clip.y + clip.height));
        const int width = right - left;
        const int height = bottom - top;
        if (width <= 0 || height <= 0)
            continue;

        ::BeginScissorMode(left, top, width, height);
        const bool rendered = command.type == DrawCommandType::Geometry
                                  ? renderGeometry(data, command.payload.geometry)
                                  : renderText(data, command.payload.text);
        ::EndScissorMode();
        if (!rendered)
            return false;
    }
    return true;
}

} // namespace raylib
} // namespace ig
