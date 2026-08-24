#include "igui/DrawData.hpp"

#include <math.h>

namespace ig
{

namespace
{

float cross(const Vec2 &a, const Vec2 &b, const Vec2 &c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool pointInTriangle(const Vec2 &point, const Vec2 &a, const Vec2 &b, const Vec2 &c)
{
    const float ab = cross(a, b, point);
    const float bc = cross(b, c, point);
    const float ca = cross(c, a, point);
    const bool hasNegative = ab < 0.0f || bc < 0.0f || ca < 0.0f;
    const bool hasPositive = ab > 0.0f || bc > 0.0f || ca > 0.0f;
    return !hasNegative || !hasPositive;
}

bool sameRect(const Rect &a, const Rect &b)
{
    return a.x == b.x && a.y == b.y &&
           a.width == b.width && a.height == b.height;
}

void addGeometryCommand(ct::Vector<DrawCommand> &commands,
                        uint32_t firstIndex, uint32_t indexCount,
                        const Rect &clip, TextureId texture = TextureId())
{
    GeometryCommand geometry;
    geometry.clip = clip;
    geometry.texture = texture;
    geometry.firstIndex = firstIndex;
    geometry.indexCount = indexCount;
    geometry.vertexOffset = 0;

    if (!commands.empty() && commands.back().type == DrawCommandType::Geometry)
    {
        GeometryCommand &previous = commands.back().payload.geometry;
        if (previous.texture == geometry.texture &&
            previous.vertexOffset == geometry.vertexOffset &&
            previous.firstIndex + previous.indexCount == geometry.firstIndex &&
            sameRect(previous.clip, geometry.clip))
        {
            previous.indexCount += geometry.indexCount;
            return;
        }
    }
    commands.push_back(DrawCommand(geometry));
}

} // namespace

void DrawList::clear()
{
    vertices_.clear();
    indices_.clear();
    textBytes_.clear();
    commands_.clear();
}

void DrawList::append(const DrawList &other)
{
    const uint32_t vertexOffset = static_cast<uint32_t>(vertices_.size());
    const uint32_t indexOffset = static_cast<uint32_t>(indices_.size());
    const uint32_t textOffset = static_cast<uint32_t>(textBytes_.size());

    for (ct::Vector<DrawVertex>::size_type i = 0; i < other.vertices_.size(); ++i)
        vertices_.push_back(other.vertices_[i]);
    for (ct::Vector<DrawIndex>::size_type i = 0; i < other.indices_.size(); ++i)
        indices_.push_back(other.indices_[i] + vertexOffset);
    for (ct::Vector<char>::size_type i = 0; i < other.textBytes_.size(); ++i)
        textBytes_.push_back(other.textBytes_[i]);

    for (ct::Vector<DrawCommand>::size_type i = 0; i < other.commands_.size(); ++i)
    {
        DrawCommand command(other.commands_[i]);
        if (command.type == DrawCommandType::Geometry)
            command.payload.geometry.firstIndex += indexOffset;
        else
            command.payload.text.textOffset += textOffset;
        commands_.push_back(command);
    }
}

void DrawList::addLine(const Vec2 &from, const Vec2 &to,
                       const Color &color, const Rect &clip, float thickness)
{
    if (thickness <= 0.0f || color.a == 0u)
        return;

    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float lengthSquared = dx * dx + dy * dy;
    const float halfThickness = thickness * 0.5f;
    if (lengthSquared <= 0.0f)
    {
        addRectFilled(Rect(from.x - halfThickness, from.y - halfThickness,
                           thickness, thickness), color, clip);
        return;
    }

    const float inverseLength = 1.0f / sqrtf(lengthSquared);
    const Vec2 perpendicular(-dy * inverseLength * halfThickness,
                             dx * inverseLength * halfThickness);
    const uint32_t firstVertex = static_cast<uint32_t>(vertices_.size());
    const uint32_t firstIndex = static_cast<uint32_t>(indices_.size());
    const Vec2 uv(0.0f, 0.0f);

    vertices_.push_back(DrawVertex{Vec2(from.x + perpendicular.x, from.y + perpendicular.y), uv, color});
    vertices_.push_back(DrawVertex{Vec2(to.x + perpendicular.x, to.y + perpendicular.y), uv, color});
    vertices_.push_back(DrawVertex{Vec2(to.x - perpendicular.x, to.y - perpendicular.y), uv, color});
    vertices_.push_back(DrawVertex{Vec2(from.x - perpendicular.x, from.y - perpendicular.y), uv, color});
    indices_.push_back(firstVertex + 0u);
    indices_.push_back(firstVertex + 1u);
    indices_.push_back(firstVertex + 2u);
    indices_.push_back(firstVertex + 0u);
    indices_.push_back(firstVertex + 2u);
    indices_.push_back(firstVertex + 3u);
    addGeometryCommand(commands_, firstIndex, 6u, clip);
}

void DrawList::addRect(const Rect &rect,
                       const Color &color, const Rect &clip, float thickness)
{
    if (rect.width <= 0.0f || rect.height <= 0.0f)
        return;

    const Vec2 topLeft(rect.x, rect.y);
    const Vec2 topRight(rect.x + rect.width, rect.y);
    const Vec2 bottomRight(rect.x + rect.width, rect.y + rect.height);
    const Vec2 bottomLeft(rect.x, rect.y + rect.height);
    addLine(topLeft, topRight, color, clip, thickness);
    addLine(topRight, bottomRight, color, clip, thickness);
    addLine(bottomRight, bottomLeft, color, clip, thickness);
    addLine(bottomLeft, topLeft, color, clip, thickness);
}

void DrawList::addRectFilled(const Rect &rect, const Color &color, const Rect &clip)
{
    if (rect.width <= 0.0f || rect.height <= 0.0f || color.a == 0u)
        return;

    const uint32_t firstVertex = static_cast<uint32_t>(vertices_.size());
    const uint32_t firstIndex = static_cast<uint32_t>(indices_.size());

    const Vec2 uv(0.0f, 0.0f);
    vertices_.push_back(DrawVertex{Vec2(rect.x, rect.y), uv, color});
    vertices_.push_back(DrawVertex{Vec2(rect.x + rect.width, rect.y), uv, color});
    vertices_.push_back(DrawVertex{Vec2(rect.x + rect.width, rect.y + rect.height), uv, color});
    vertices_.push_back(DrawVertex{Vec2(rect.x, rect.y + rect.height), uv, color});

    indices_.push_back(firstVertex + 0u);
    indices_.push_back(firstVertex + 1u);
    indices_.push_back(firstVertex + 2u);
    indices_.push_back(firstVertex + 0u);
    indices_.push_back(firstVertex + 2u);
    indices_.push_back(firstVertex + 3u);

    addGeometryCommand(commands_, firstIndex, 6u, clip);
}

void DrawList::addRectGradient(const Rect &rect, const Color &topLeft, const Color &topRight,
                               const Color &bottomRight, const Color &bottomLeft, const Rect &clip)
{
    if (rect.width <= 0.0f || rect.height <= 0.0f)
        return;

    const uint32_t firstVertex = static_cast<uint32_t>(vertices_.size());
    const uint32_t firstIndex = static_cast<uint32_t>(indices_.size());
    const Vec2 uv(0.0f, 0.0f);
    vertices_.push_back(DrawVertex{Vec2(rect.x, rect.y), uv, topLeft});
    vertices_.push_back(DrawVertex{Vec2(rect.x + rect.width, rect.y), uv, topRight});
    vertices_.push_back(DrawVertex{Vec2(rect.x + rect.width, rect.y + rect.height), uv, bottomRight});
    vertices_.push_back(DrawVertex{Vec2(rect.x, rect.y + rect.height), uv, bottomLeft});
    indices_.push_back(firstVertex + 0u);
    indices_.push_back(firstVertex + 1u);
    indices_.push_back(firstVertex + 2u);
    indices_.push_back(firstVertex + 0u);
    indices_.push_back(firstVertex + 2u);
    indices_.push_back(firstVertex + 3u);
    addGeometryCommand(commands_, firstIndex, 6u, clip);
}

void DrawList::addImage(TextureId texture, const Rect &rect, const Vec2 &uvMin,
                        const Vec2 &uvMax, const Color &color, const Rect &clip)
{
    if (rect.width <= 0.0f || rect.height <= 0.0f || color.a == 0u)
        return;

    const uint32_t firstVertex = static_cast<uint32_t>(vertices_.size());
    const uint32_t firstIndex = static_cast<uint32_t>(indices_.size());
    vertices_.push_back(DrawVertex{Vec2(rect.x, rect.y), uvMin, color});
    vertices_.push_back(DrawVertex{Vec2(rect.x + rect.width, rect.y),
                                   Vec2(uvMax.x, uvMin.y), color});
    vertices_.push_back(DrawVertex{Vec2(rect.x + rect.width, rect.y + rect.height), uvMax, color});
    vertices_.push_back(DrawVertex{Vec2(rect.x, rect.y + rect.height),
                                   Vec2(uvMin.x, uvMax.y), color});
    indices_.push_back(firstVertex + 0u);
    indices_.push_back(firstVertex + 1u);
    indices_.push_back(firstVertex + 2u);
    indices_.push_back(firstVertex + 0u);
    indices_.push_back(firstVertex + 2u);
    indices_.push_back(firstVertex + 3u);

    addGeometryCommand(commands_, firstIndex, 6u, clip, texture);
}

void DrawList::addCircleFilled(const Vec2 &center, float radius,
                               const Color &color, const Rect &clip, uint32_t segments)
{
    if (radius <= 0.0f || color.a == 0u)
        return;

    if (segments < 3u)
    {
        // UI controls are often small, where a low polygon count is especially
        // noticeable.  Keep their silhouette round before adding the soft edge
        // below.
        segments = static_cast<uint32_t>(radius * 0.8f) + 12u;
        segments = segments < 12u ? 12u : (segments > 64u ? 64u : segments);
    }
    segments = segments > 256u ? 256u : segments;

    const uint32_t firstVertex = static_cast<uint32_t>(vertices_.size());
    const uint32_t firstIndex = static_cast<uint32_t>(indices_.size());
    const Vec2 uv(0.0f, 0.0f);
    const float step = 6.28318530717958647692f / static_cast<float>(segments);
    const Color transparentEdge(color.r, color.g, color.b, 0u);
    const float fringe = radius < 2.0f ? 0.5f : 1.0f;

    vertices_.push_back(DrawVertex{center, uv, color});
    for (uint32_t i = 0; i < segments; ++i)
    {
        const float angle = step * static_cast<float>(i);
        vertices_.push_back(DrawVertex{Vec2(center.x + cosf(angle) * radius,
                                            center.y + sinf(angle) * radius), uv, color});
    }
    for (uint32_t i = 0; i < segments; ++i)
    {
        const float angle = step * static_cast<float>(i);
        vertices_.push_back(DrawVertex{Vec2(center.x + cosf(angle) * (radius + fringe),
                                            center.y + sinf(angle) * (radius + fringe)),
                                       uv, transparentEdge});
    }
    for (uint32_t i = 0; i < segments; ++i)
    {
        const uint32_t next = (i + 1u) % segments;
        const uint32_t inner = firstVertex + 1u + i;
        const uint32_t innerNext = firstVertex + 1u + next;
        const uint32_t outer = firstVertex + 1u + segments + i;
        const uint32_t outerNext = firstVertex + 1u + segments + next;

        indices_.push_back(firstVertex);
        indices_.push_back(inner);
        indices_.push_back(innerNext);

        // One transparent pixel of geometry gives the circle a stable
        // anti-aliased edge on every backend, without needing a font glyph.
        indices_.push_back(inner);
        indices_.push_back(outer);
        indices_.push_back(outerNext);
        indices_.push_back(inner);
        indices_.push_back(outerNext);
        indices_.push_back(innerNext);
    }
    addGeometryCommand(commands_, firstIndex, segments * 9u, clip);
}

void DrawList::addPolygonFilled(Span<const Vec2> points, const Color &color, const Rect &clip)
{
    if (points.size() < 3u || color.a == 0u)
        return;

    float signedArea = 0.0f;
    for (Span<const Vec2>::size_type i = 0; i < points.size(); ++i)
    {
        const Vec2 &current = points[i];
        const Vec2 &next = points[(i + 1u) % points.size()];
        signedArea += current.x * next.y - current.y * next.x;
    }
    if (signedArea == 0.0f)
        return;

    ct::Vector<uint32_t> remaining;
    ct::Vector<DrawIndex> triangleIndices;
    remaining.reserve(points.size());
    triangleIndices.reserve((points.size() - 2u) * 3u);
    for (Span<const Vec2>::size_type i = 0; i < points.size(); ++i)
        remaining.push_back(static_cast<uint32_t>(i));

    const bool clockwise = signedArea < 0.0f;
    while (remaining.size() > 3u)
    {
        bool removedEar = false;
        for (ct::Vector<uint32_t>::size_type i = 0; i < remaining.size(); ++i)
        {
            const uint32_t previous = remaining[(i + remaining.size() - 1u) % remaining.size()];
            const uint32_t current = remaining[i];
            const uint32_t next = remaining[(i + 1u) % remaining.size()];
            const float turn = cross(points[previous], points[current], points[next]);
            if ((clockwise && turn >= 0.0f) || (!clockwise && turn <= 0.0f))
                continue;

            bool containsPoint = false;
            for (ct::Vector<uint32_t>::size_type other = 0; other < remaining.size(); ++other)
            {
                const uint32_t candidate = remaining[other];
                if (candidate != previous && candidate != current && candidate != next &&
                    pointInTriangle(points[candidate], points[previous], points[current], points[next]))
                {
                    containsPoint = true;
                    break;
                }
            }
            if (containsPoint)
                continue;

            triangleIndices.push_back(previous);
            triangleIndices.push_back(current);
            triangleIndices.push_back(next);
            remaining.erase(remaining.begin() + i);
            removedEar = true;
            break;
        }
        if (!removedEar)
            return;
    }

    triangleIndices.push_back(remaining[0]);
    triangleIndices.push_back(remaining[1]);
    triangleIndices.push_back(remaining[2]);

    const uint32_t firstVertex = static_cast<uint32_t>(vertices_.size());
    const uint32_t firstIndex = static_cast<uint32_t>(indices_.size());
    const Vec2 uv(0.0f, 0.0f);
    for (Span<const Vec2>::size_type i = 0; i < points.size(); ++i)
        vertices_.push_back(DrawVertex{points[i], uv, color});
    for (ct::Vector<DrawIndex>::size_type i = 0; i < triangleIndices.size(); ++i)
        indices_.push_back(firstVertex + triangleIndices[i]);
    addGeometryCommand(commands_, firstIndex,
                       static_cast<uint32_t>(triangleIndices.size()), clip);
}

void DrawList::addText(StringView text, const Vec2 &position, FontId font,
                       float logicalSize, const Color &color, const Rect &clip)
{
    const uint32_t offset = static_cast<uint32_t>(textBytes_.size());
    for (StringView::size_type i = 0; i < text.size(); ++i)
        textBytes_.push_back(text[i]);

    TextCommand command;
    command.clip = clip;
    command.position = position;
    command.font = font;
    command.logicalSize = logicalSize;
    command.color = color;
    command.textOffset = offset;
    command.textSize = static_cast<uint32_t>(text.size());
    commands_.push_back(DrawCommand(command));
}

DrawData DrawList::data(const Vec2 &displaySize, float dpiScale) const
{
    DrawData result;
    result.displaySize = displaySize;
    result.dpiScale = dpiScale;
    result.vertices = Span<const DrawVertex>(vertices_);
    result.indices = Span<const DrawIndex>(indices_);
    result.textBytes = Span<const char>(textBytes_);
    result.commands = Span<const DrawCommand>(commands_);
    return result;
}

} // namespace ig
