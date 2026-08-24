#pragma once

#include <new>
#include <stdint.h>

#include <ct/vector.hpp>

#include "Color.hpp"
#include "Math.hpp"
#include "Types.hpp"

namespace ig
{

struct DrawVertex
{
    Vec2 position;
    Vec2 uv;
    Color color;
};

using DrawIndex = uint32_t;

struct GeometryCommand
{
    Rect clip;
    TextureId texture;
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t vertexOffset;
};

struct TextCommand
{
    Rect clip;
    Vec2 position;
    FontId font;
    float logicalSize;
    Color color;
    uint32_t textOffset;
    uint32_t textSize;
};

enum class DrawCommandType : uint8_t
{
    Geometry,
    Text
};

struct DrawCommand
{
    DrawCommandType type;
    union Payload
    {
        GeometryCommand geometry;
        TextCommand text;
        Payload() {}
        ~Payload() {}
    } payload;

    DrawCommand() : type(DrawCommandType::Geometry), payload()
    {
        new (&payload.geometry) GeometryCommand();
    }

    explicit DrawCommand(const GeometryCommand &command)
        : type(DrawCommandType::Geometry), payload()
    {
        new (&payload.geometry) GeometryCommand(command);
    }

    explicit DrawCommand(const TextCommand &command)
        : type(DrawCommandType::Text), payload()
    {
        new (&payload.text) TextCommand(command);
    }

    DrawCommand(const DrawCommand &other) : type(other.type), payload()
    {
        if (type == DrawCommandType::Geometry)
            new (&payload.geometry) GeometryCommand(other.payload.geometry);
        else
            new (&payload.text) TextCommand(other.payload.text);
    }

    DrawCommand &operator=(const DrawCommand &other)
    {
        if (this == &other)
            return *this;
        if (type == other.type)
        {
            if (type == DrawCommandType::Geometry)
                payload.geometry = other.payload.geometry;
            else
                payload.text = other.payload.text;
            return *this;
        }

        if (type == DrawCommandType::Geometry)
            payload.geometry.~GeometryCommand();
        else
            payload.text.~TextCommand();
        type = other.type;
        if (type == DrawCommandType::Geometry)
            new (&payload.geometry) GeometryCommand(other.payload.geometry);
        else
            new (&payload.text) TextCommand(other.payload.text);
        return *this;
    }

    ~DrawCommand()
    {
        if (type == DrawCommandType::Geometry)
            payload.geometry.~GeometryCommand();
        else
            payload.text.~TextCommand();
    }
};

struct DrawData
{
    Vec2 displaySize;
    float dpiScale;
    Span<const DrawVertex> vertices;
    Span<const DrawIndex> indices;
    Span<const char> textBytes;
    Span<const DrawCommand> commands;
};

class DrawList
{
public:
    void clear();
    void append(const DrawList &other);

    void addLine(const Vec2 &from, const Vec2 &to,
                 const Color &color, const Rect &clip, float thickness = 1.0f);
    void addRect(const Rect &rect,
                 const Color &color, const Rect &clip, float thickness = 1.0f);
    void addRectFilled(const Rect &rect, const Color &color, const Rect &clip);
    // Per-corner colours are linearly interpolated by the backend.
    void addRectGradient(const Rect &rect, const Color &topLeft, const Color &topRight,
                         const Color &bottomRight, const Color &bottomLeft, const Rect &clip);
    void addImage(TextureId texture, const Rect &rect, const Vec2 &uvMin,
                  const Vec2 &uvMax, const Color &color, const Rect &clip);
    void addCircleFilled(const Vec2 &center, float radius,
                         const Color &color, const Rect &clip, uint32_t segments = 0u);
    // Tessellates a simple polygon. Points may be clockwise or counter-clockwise.
    void addPolygonFilled(Span<const Vec2> points, const Color &color, const Rect &clip);
    void addText(StringView text, const Vec2 &position, FontId font,
                 float logicalSize, const Color &color, const Rect &clip);

    DrawData data(const Vec2 &displaySize, float dpiScale) const;

private:
    ct::Vector<DrawVertex> vertices_;
    ct::Vector<DrawIndex> indices_;
    ct::Vector<char> textBytes_;
    ct::Vector<DrawCommand> commands_;
};

} // namespace ig
