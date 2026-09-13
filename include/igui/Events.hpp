#pragma once

#include <stdint.h>

#include "Math.hpp"
#include "Types.hpp"

namespace ig
{

enum class EventType : uint8_t
{
    None,
    PointerMove,
    PointerDown,
    PointerUp,
    PointerWheel,
    KeyDown,
    TextInput,
    FocusLost,
    ViewportChanged
};

enum class PointerButton : uint8_t
{
    Left = 0,
    Middle = 1,
    Right = 2
};

enum class KeyCode : uint8_t
{
    None,
    Backspace,
    Enter,
    Delete,
    Tab,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    PageUp,
    PageDown,
    Escape,
    A,
    C,
    D,
    F,
    H,
    S,
    V,
    X,
    Y,
    Z,
    N,
    O,
    F4,
    F5
};

struct FrameInfo
{
    float deltaSeconds;
    Vec2 displaySize;
    float dpiScale;

    FrameInfo() : deltaSeconds(0.0f), displaySize(), dpiScale(1.0f) {}
    FrameInfo(float width, float height, float scale = 1.0f, float delta = 0.0f)
        : deltaSeconds(delta), displaySize(width, height), dpiScale(scale)
    {
    }
};

// Evento de tamanho fixo. O texto fica limitado a 63 bytes nesta primeira entrega.
// A API de edição completa será adicionada quando TextInput for migrado.
struct Event
{
    EventType type;
    PointerButton button;
    KeyCode key;
    Vec2 position;
    float wheelX;
    float wheelY;
    Vec2 viewportSize;
    float dpiScale;
    char text[64];
    uint32_t textLength;
    bool control;
    bool shift;

    Event()
        : type(EventType::None), button(PointerButton::Left), key(KeyCode::None), position(),
          wheelX(0.0f), wheelY(0.0f), viewportSize(), dpiScale(1.0f),
          textLength(0), control(false), shift(false)
    {
        text[0] = '\0';
    }

    static Event pointerMove(float x, float y)
    {
        Event event;
        event.type = EventType::PointerMove;
        event.position = Vec2(x, y);
        return event;
    }

    static Event pointerDown(PointerButton pointerButton, float x, float y)
    {
        Event event;
        event.type = EventType::PointerDown;
        event.button = pointerButton;
        event.position = Vec2(x, y);
        return event;
    }

    static Event pointerUp(PointerButton pointerButton, float x, float y)
    {
        Event event;
        event.type = EventType::PointerUp;
        event.button = pointerButton;
        event.position = Vec2(x, y);
        return event;
    }

    static Event focusLost()
    {
        Event event;
        event.type = EventType::FocusLost;
        return event;
    }

    static Event keyDown(KeyCode keyCode, bool controlDown = false, bool shiftDown = false)
    {
        Event event;
        event.type = EventType::KeyDown;
        event.key = keyCode;
        event.control = controlDown;
        event.shift = shiftDown;
        return event;
    }

    static Event textInput(StringView utf8)
    {
        Event event;
        event.type = EventType::TextInput;
        const StringView::size_type count = utf8.size() < 63u ? utf8.size() : 63u;
        for (StringView::size_type i = 0; i < count; ++i)
            event.text[i] = utf8[i];
        event.text[count] = '\0';
        event.textLength = static_cast<uint32_t>(count);
        return event;
    }

    static Event viewportChanged(float width, float height, float scale = 1.0f)
    {
        Event event;
        event.type = EventType::ViewportChanged;
        event.viewportSize = Vec2(width, height);
        event.dpiScale = scale;
        return event;
    }
};

} // namespace ig
