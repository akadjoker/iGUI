#pragma once

#include <stdint.h>

#include "Math.hpp"

namespace ig
{

enum class EventType : uint8_t
{
    None,
    PointerMove,
    PointerDown,
    PointerUp,
    PointerWheel,
    FocusLost,
    ViewportChanged
};

enum class PointerButton : uint8_t
{
    Left = 0,
    Middle = 1,
    Right = 2
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
    Vec2 position;
    float wheelX;
    float wheelY;
    Vec2 viewportSize;
    float dpiScale;
    char text[64];
    uint32_t textLength;

    Event()
        : type(EventType::None), button(PointerButton::Left), position(),
          wheelX(0.0f), wheelY(0.0f), viewportSize(), dpiScale(1.0f),
          textLength(0)
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
