#include "igui_sdl2/SdlBackend.hpp"

#include <math.h>

namespace ig
{
namespace sdl2
{

namespace
{

bool pointerButton(uint8_t button, PointerButton &result)
{
    if (button == SDL_BUTTON_LEFT)
    {
        result = PointerButton::Left;
        return true;
    }
    if (button == SDL_BUTTON_MIDDLE)
    {
        result = PointerButton::Middle;
        return true;
    }
    if (button == SDL_BUTTON_RIGHT)
    {
        result = PointerButton::Right;
        return true;
    }
    return false;
}

} // namespace

bool translateEvent(const SDL_Event &nativeEvent, Event &event, float dpiScale)
{
    switch (nativeEvent.type)
    {
    case SDL_MOUSEMOTION:
        event = Event::pointerMove(static_cast<float>(nativeEvent.motion.x),
                                   static_cast<float>(nativeEvent.motion.y));
        return true;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    {
        PointerButton button;
        if (!pointerButton(nativeEvent.button.button, button))
            return false;
        SDL_CaptureMouse(nativeEvent.type == SDL_MOUSEBUTTONDOWN ? SDL_TRUE : SDL_FALSE);
        event = nativeEvent.type == SDL_MOUSEBUTTONDOWN
                    ? Event::pointerDown(button, static_cast<float>(nativeEvent.button.x),
                                         static_cast<float>(nativeEvent.button.y))
                    : Event::pointerUp(button, static_cast<float>(nativeEvent.button.x),
                                       static_cast<float>(nativeEvent.button.y));
        return true;
    }
    case SDL_MOUSEWHEEL:
        event = Event();
        event.type = EventType::PointerWheel;
        event.wheelX = static_cast<float>(nativeEvent.wheel.x);
        event.wheelY = static_cast<float>(nativeEvent.wheel.y);
        if (nativeEvent.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
        {
            event.wheelX = -event.wheelX;
            event.wheelY = -event.wheelY;
        }
        return true;
    case SDL_KEYDOWN:
    {
        KeyCode key = KeyCode::None;
        switch (nativeEvent.key.keysym.sym)
        {
        case SDLK_BACKSPACE: key = KeyCode::Backspace; break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: key = KeyCode::Enter; break;
        case SDLK_DELETE: key = KeyCode::Delete; break;
        case SDLK_TAB: key = KeyCode::Tab; break;
        case SDLK_LEFT: key = KeyCode::Left; break;
        case SDLK_RIGHT: key = KeyCode::Right; break;
        case SDLK_UP: key = KeyCode::Up; break;
        case SDLK_DOWN: key = KeyCode::Down; break;
        case SDLK_HOME: key = KeyCode::Home; break;
        case SDLK_END: key = KeyCode::End; break;
        case SDLK_PAGEUP: key = KeyCode::PageUp; break;
        case SDLK_PAGEDOWN: key = KeyCode::PageDown; break;
        case SDLK_ESCAPE: key = KeyCode::Escape; break;
        case SDLK_n: key = KeyCode::N; break;
        case SDLK_o: key = KeyCode::O; break;
        case SDLK_F4: key = KeyCode::F4; break;
        case SDLK_F5: key = KeyCode::F5; break;
        case SDLK_a: key = KeyCode::A; break;
        case SDLK_c: key = KeyCode::C; break;
        case SDLK_d: key = KeyCode::D; break;
        case SDLK_f: key = KeyCode::F; break;
        case SDLK_h: key = KeyCode::H; break;
        case SDLK_v: key = KeyCode::V; break;
        case SDLK_x: key = KeyCode::X; break;
        case SDLK_y: key = KeyCode::Y; break;
        case SDLK_z: key = KeyCode::Z; break;
        default: return false;
        }
        const SDL_Keymod modifiers = static_cast<SDL_Keymod>(nativeEvent.key.keysym.mod);
        event = Event::keyDown(key, (modifiers & KMOD_CTRL) != 0,
                              (modifiers & KMOD_SHIFT) != 0);
        return true;
    }
    case SDL_TEXTINPUT:
        event = Event::textInput(StringView(nativeEvent.text.text));
        return event.textLength != 0u;
    case SDL_WINDOWEVENT:
        if (nativeEvent.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
        {
            event = Event::focusLost();
            return true;
        }
        if (nativeEvent.window.event == SDL_WINDOWEVENT_RESIZED ||
            nativeEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            event = Event::viewportChanged(static_cast<float>(nativeEvent.window.data1),
                                           static_cast<float>(nativeEvent.window.data2), dpiScale);
            return true;
        }
        return false;
    default:
        return false;
    }
}

FrameInfo frameInfo(SDL_Window *window, float deltaSeconds, float dpiScale)
{
    int width = 0;
    int height = 0;
    if (window)
        SDL_GetWindowSize(window, &width, &height);
    return FrameInfo(static_cast<float>(width), static_cast<float>(height), dpiScale, deltaSeconds);
}

TextureId textureId(SDL_Texture *texture)
{
    return TextureId(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(texture)));
}

Backend::Backend(SDL_Renderer *renderer)
    : renderer_(renderer), fontAtlas_(), fontTexture_(nullptr), vertices_(), indices_(), fontPixels_()
{
}

Backend::Backend(SDL_Renderer *renderer, Span<const FontRange> ranges,
                 uint32_t atlasWidth, uint32_t atlasHeight, float bakedSize)
    : renderer_(renderer), fontAtlas_(ranges, atlasWidth, atlasHeight, bakedSize),
      fontTexture_(nullptr), vertices_(), indices_(), fontPixels_()
{
}

Backend::~Backend()
{
    if (fontTexture_)
        SDL_DestroyTexture(fontTexture_);
    fontAtlas_.setTexture(TextureId());
}

void Backend::setRenderer(SDL_Renderer *renderer)
{
    if (fontTexture_)
    {
        SDL_DestroyTexture(fontTexture_);
        fontTexture_ = nullptr;
    }
    fontAtlas_.setTexture(TextureId());
    renderer_ = renderer;
}

SDL_Renderer *Backend::renderer() const
{
    return renderer_;
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
    if (!::SDL_HasClipboardText())
        return String();
    char *text = ::SDL_GetClipboardText();
    if (!text)
        return String();
    const String result(text);
    ::SDL_free(text);
    return result;
}

bool Backend::setClipboardText(StringView text)
{
    const String copy(text.data(), text.size());
    return ::SDL_SetClipboardText(copy.c_str()) == 0;
}

bool Backend::ensureFontTexture()
{
    if (fontTexture_)
        return true;
    if (!renderer_ || !fontAtlas_.valid())
        return false;
    const Span<const uint8_t> alpha = fontAtlas_.pixels();
    fontPixels_.resize(alpha.size() * 4u);
    for (Span<const uint8_t>::size_type i = 0; i < alpha.size(); ++i)
    {
        const Span<const uint8_t>::size_type pixel = i * 4u;
        fontPixels_[pixel + 0u] = 255u;
        fontPixels_[pixel + 1u] = 255u;
        fontPixels_[pixel + 2u] = 255u;
        fontPixels_[pixel + 3u] = alpha[i];
    }
    fontTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                     static_cast<int>(fontAtlas_.width()), static_cast<int>(fontAtlas_.height()));
    if (!fontTexture_)
        return false;
    if (SDL_UpdateTexture(fontTexture_, nullptr, fontPixels_.data(),
                          static_cast<int>(fontAtlas_.width() * 4u)) != 0 ||
        SDL_SetTextureBlendMode(fontTexture_, SDL_BLENDMODE_BLEND) != 0 ||
        SDL_SetTextureScaleMode(fontTexture_, SDL_ScaleModeLinear) != 0)
    {
        SDL_DestroyTexture(fontTexture_);
        fontTexture_ = nullptr;
        return false;
    }
    fontAtlas_.setTexture(textureId(fontTexture_));
    return true;
}

bool Backend::setClip(const Rect &clip)
{
    if (!renderer_)
        return false;
    SDL_Rect nativeClip;
    nativeClip.x = static_cast<int>(floorf(clip.x));
    nativeClip.y = static_cast<int>(floorf(clip.y));
    nativeClip.w = static_cast<int>(ceilf(clip.x + clip.width)) - nativeClip.x;
    nativeClip.h = static_cast<int>(ceilf(clip.y + clip.height)) - nativeClip.y;
    if (nativeClip.w < 0)
        nativeClip.w = 0;
    if (nativeClip.h < 0)
        nativeClip.h = 0;
    return SDL_RenderSetClipRect(renderer_, &nativeClip) == 0;
}

bool Backend::renderGeometry(const DrawData &data, const GeometryCommand &command)
{
    if (command.firstIndex > data.indices.size() ||
        command.indexCount > data.indices.size() - command.firstIndex)
        return false;

    indices_.clear();
    indices_.reserve(command.indexCount);
    for (uint32_t i = 0; i < command.indexCount; ++i)
    {
        const DrawIndex index = data.indices[command.firstIndex + i];
        const int resolvedIndex = static_cast<int>(index) + command.vertexOffset;
        if (resolvedIndex < 0 || static_cast<uint32_t>(resolvedIndex) >= data.vertices.size())
            return false;
        indices_.push_back(resolvedIndex);
    }

    SDL_Texture *texture = command.texture.value == 0u
                               ? nullptr
                               : reinterpret_cast<SDL_Texture *>(static_cast<uintptr_t>(command.texture.value));
    // Untextured geometry (including modal scrims) also needs alpha blending.
    if (!texture && SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND) != 0)
        return false;
    return SDL_RenderGeometry(renderer_, texture, vertices_.data(),
                              static_cast<int>(vertices_.size()), indices_.data(),
                              static_cast<int>(indices_.size())) == 0;
}

bool Backend::renderText(const DrawData &data, const TextCommand &command)
{
    if (command.textOffset > data.textBytes.size() ||
        command.textSize > data.textBytes.size() - command.textOffset)
        return false;
    if (!ensureFontTexture() ||
        SDL_SetTextureColorMod(fontTexture_, command.color.r, command.color.g, command.color.b) != 0 ||
        SDL_SetTextureAlphaMod(fontTexture_, command.color.a) != 0)
        return false;

    const float scale = command.logicalSize / fontAtlas_.bakedSize();
    float penX = command.position.x;
    float penY = command.position.y + fontAtlas_.ascent() * scale;
    StringView text(data.textBytes.data() + command.textOffset, command.textSize);
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
        const FontGlyph *glyphInfo = fontAtlas_.glyph(command.font, codepoint);
        if (!glyphInfo)
            continue;
        if (glyphInfo->width != 0u && glyphInfo->height != 0u)
        {
            SDL_Rect source = {static_cast<int>(glyphInfo->x), static_cast<int>(glyphInfo->y),
                               static_cast<int>(glyphInfo->width), static_cast<int>(glyphInfo->height)};
            SDL_FRect destination;
            destination.x = penX + glyphInfo->offsetX * scale;
            destination.y = penY + glyphInfo->offsetY * scale;
            destination.w = static_cast<float>(glyphInfo->width) * scale;
            destination.h = static_cast<float>(glyphInfo->height) * scale;
            if (SDL_RenderCopyF(renderer_, fontTexture_, &source, &destination) != 0)
                return false;
        }
        penX += glyphInfo->advance * scale;
    }
    return true;
}

bool Backend::render(const DrawData &data)
{
    if (!renderer_)
        return false;

    // Convert the shared vertex buffer once. Geometry commands only select
    // their index ranges below; reconverting it per command is quadratic for
    // dense widgets such as the color picker.
    vertices_.clear();
    vertices_.reserve(data.vertices.size());
    for (Span<const DrawVertex>::size_type i = 0; i < data.vertices.size(); ++i)
    {
        SDL_Vertex vertex;
        vertex.position.x = data.vertices[i].position.x;
        vertex.position.y = data.vertices[i].position.y;
        vertex.color.r = data.vertices[i].color.r;
        vertex.color.g = data.vertices[i].color.g;
        vertex.color.b = data.vertices[i].color.b;
        vertex.color.a = data.vertices[i].color.a;
        vertex.tex_coord.x = data.vertices[i].uv.x;
        vertex.tex_coord.y = data.vertices[i].uv.y;
        vertices_.push_back(vertex);
    }

    for (Span<const DrawCommand>::size_type i = 0; i < data.commands.size(); ++i)
    {
        const DrawCommand &command = data.commands[i];
        const Rect &clip = command.type == DrawCommandType::Geometry
                               ? command.payload.geometry.clip : command.payload.text.clip;
        if (!setClip(clip))
            return false;
        const bool rendered = command.type == DrawCommandType::Geometry
                                  ? renderGeometry(data, command.payload.geometry)
                                  : renderText(data, command.payload.text);
        if (!rendered)
            return false;
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
    return true;
}

} // namespace sdl2
} // namespace ig
