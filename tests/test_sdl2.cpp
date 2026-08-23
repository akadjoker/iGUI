#include <assert.h>

#include <SDL.h>

#include <igui/Gui.hpp>
#include <igui_sdl2/SdlBackend.hpp>

static void test_pointer_translation()
{
    SDL_Event nativeEvent;
    SDL_zero(nativeEvent);
    nativeEvent.type = SDL_MOUSEBUTTONDOWN;
    nativeEvent.button.button = SDL_BUTTON_LEFT;
    nativeEvent.button.x = 24;
    nativeEvent.button.y = 48;

    ig::Event event;
    assert(ig::sdl2::translateEvent(nativeEvent, event));
    assert(event.type == ig::EventType::PointerDown);
    assert(event.button == ig::PointerButton::Left);
    assert(event.position.x == 24.0f);
    assert(event.position.y == 48.0f);
}

static void test_window_and_wheel_translation()
{
    SDL_Event nativeEvent;
    SDL_zero(nativeEvent);
    nativeEvent.type = SDL_WINDOWEVENT;
    nativeEvent.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    nativeEvent.window.data1 = 800;
    nativeEvent.window.data2 = 600;

    ig::Event event;
    assert(ig::sdl2::translateEvent(nativeEvent, event, 2.0f));
    assert(event.type == ig::EventType::ViewportChanged);
    assert(event.viewportSize.x == 800.0f);
    assert(event.viewportSize.y == 600.0f);
    assert(event.dpiScale == 2.0f);

    SDL_zero(nativeEvent);
    nativeEvent.type = SDL_MOUSEWHEEL;
    nativeEvent.wheel.x = 2;
    nativeEvent.wheel.y = 3;
    nativeEvent.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    assert(ig::sdl2::translateEvent(nativeEvent, event));
    assert(event.type == ig::EventType::PointerWheel);
    assert(event.wheelX == -2.0f);
    assert(event.wheelY == -3.0f);

    SDL_zero(nativeEvent);
    nativeEvent.type = SDL_KEYDOWN;
    nativeEvent.key.keysym.sym = SDLK_BACKSPACE;
    assert(ig::sdl2::translateEvent(nativeEvent, event));
    assert(event.type == ig::EventType::KeyDown);
    assert(event.key == ig::KeyCode::Backspace);

    SDL_zero(nativeEvent);
    nativeEvent.type = SDL_KEYDOWN;
    nativeEvent.key.keysym.sym = SDLK_f;
    nativeEvent.key.keysym.mod = KMOD_CTRL;
    assert(ig::sdl2::translateEvent(nativeEvent, event));
    assert(event.key == ig::KeyCode::F);
    assert(event.control);

    SDL_zero(nativeEvent);
    nativeEvent.type = SDL_KEYDOWN;
    nativeEvent.key.keysym.sym = SDLK_PAGEUP;
    nativeEvent.key.keysym.mod = KMOD_SHIFT;
    assert(ig::sdl2::translateEvent(nativeEvent, event));
    assert(event.key == ig::KeyCode::PageUp);
    assert(event.shift);

    SDL_zero(nativeEvent);
    nativeEvent.type = SDL_TEXTINPUT;
    nativeEvent.text.text[0] = 'a';
    nativeEvent.text.text[1] = '\0';
    assert(ig::sdl2::translateEvent(nativeEvent, event));
    assert(event.type == ig::EventType::TextInput);
    assert(event.textLength == 1u);
}

static void test_software_rendering()
{
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 160, 100, 32, SDL_PIXELFORMAT_RGBA32);
    assert(surface);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    assert(renderer);

    ig::sdl2::Backend backend(renderer);
    ig::DrawList drawList;
    const ig::Rect clip(0.0f, 0.0f, 160.0f, 100.0f);
    drawList.addRectFilled(ig::Rect(8.0f, 8.0f, 80.0f, 36.0f),
                           ig::Color(20u, 40u, 60u, 255u), clip);
    drawList.addText(u8"Olá SDL2", ig::Vec2(12.0f, 12.0f), ig::FontId(), 14.0f,
                     ig::Color(255u, 255u, 255u, 255u), clip);
    const ig::DrawData data = drawList.data(ig::Vec2(160.0f, 100.0f), 1.0f);
    assert(backend.render(data));

    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
}

static void test_context_emits_textured_font_geometry()
{
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 320, 160, 32, SDL_PIXELFORMAT_RGBA32);
    assert(surface);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    assert(renderer);

    ig::sdl2::Backend backend(renderer);
    assert(backend.prepareFontAtlas());
    ig::Context context(backend, &backend.fontAtlas());
    context.beginFrame(ig::FrameInfo(320.0f, 160.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 200.0f, 100.0f)));
    context.label(u8"Olá", ig::Vec2(8.0f, 8.0f));
    context.endWindow();
    const ig::DrawData &data = context.endFrame();

    bool hasFontGeometry = false;
    for (ig::Span<const ig::DrawCommand>::size_type i = 0; i < data.commands.size(); ++i)
    {
        assert(data.commands[i].type == ig::DrawCommandType::Geometry);
        if (data.commands[i].payload.geometry.texture.value != 0u)
            hasFontGeometry = true;
    }
    assert(hasFontGeometry);
    assert(backend.render(data));

    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
}

int main()
{
    test_pointer_translation();
    test_window_and_wheel_translation();
    test_software_rendering();
    test_context_emits_textured_font_geometry();
    return 0;
}
