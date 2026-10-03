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

// Off-clip culling. Backends scissor every command to its clip, so geometry
// that lies entirely outside it costs vertices, indices, a command and GPU
// work for zero visible pixels - a scrolled child with a few hundred rows, or
// a multi-line text with thousands of lines, was >95% such geometry. Every
// add* call tests its bounding box (including the antialiasing fringe, at
// most 1px) here first and emits nothing when it cannot touch the clip.
//
// The test must never drop a visible pixel: backends round the scissor
// outward to whole pixels (floor/ceil) and raylib snaps glyphs by up to half
// a pixel, so anything within CullMargin of the clip is kept. A clip with no
// area draws nothing on any backend (checked with SDL2's renderer), so it
// culls everything.
const float CullMargin = 2.0f;

bool outsideClip(float minX, float minY, float maxX, float maxY, const Rect &clip)
{
    if (!(clip.width > 0.0f) || !(clip.height > 0.0f))
        return true;
    return maxX < clip.x - CullMargin || maxY < clip.y - CullMargin ||
           minX > clip.x + clip.width + CullMargin || minY > clip.y + clip.height + CullMargin;
}

bool rectOutsideClip(const Rect &rect, float grow, const Rect &clip)
{
    return outsideClip(rect.x - grow, rect.y - grow, rect.x + rect.width + grow,
                       rect.y + rect.height + grow, clip);
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
    // Half the (at least 1px) core plus the 1px fringe on each side.
    const float reach = (thickness > 1.0f ? thickness : 1.0f) * 0.5f + 1.0f;
    if (outsideClip((from.x < to.x ? from.x : to.x) - reach, (from.y < to.y ? from.y : to.y) - reach,
                    (from.x > to.x ? from.x : to.x) + reach, (from.y > to.y ? from.y : to.y) + reach, clip))
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
    const Vec2 unitPerpendicular(-dy * inverseLength, dx * inverseLength);
    const uint32_t firstVertex = static_cast<uint32_t>(vertices_.size());
    const uint32_t firstIndex = static_cast<uint32_t>(indices_.size());
    const Vec2 uv(0.0f, 0.0f);

    // Soft edge, same idea as addCircleFilled: the quad is grown by a one-pixel
    // fringe on each side whose outer vertices are fully transparent, so the
    // rasterizer interpolates alpha across it instead of ending on a hard step.
    // Without it every diagonal and every arc (an arc is a fan of short lines)
    // comes out visibly stair-stepped - the single biggest difference from a
    // canvas, which antialiases everything by default.
    //
    // A hairline is special-cased: below one pixel the core would be thinner
    // than the fringe, so the core is pinned at one pixel and the colour is
    // scaled by the requested coverage instead, which keeps a 0.5px line half
    // as opaque rather than dropping it to a hard 1px line.
    // An axis-aligned line at an integer thickness lands on whole pixels, so a
    // fringe there would only blur a separator or a grid rule that is currently
    // crisp - and these are by far the most common lines in a UI. Emit the
    // plain quad for them and keep the soft edge for what actually needs it:
    // diagonals and the short segments that arcs and curves are built from.
    const bool axisAligned = (from.x == to.x) || (from.y == to.y);
    if (axisAligned && thickness >= 1.0f &&
        thickness == static_cast<float>(static_cast<int>(thickness)))
    {
        const Vec2 flatPerpendicular(-dy * inverseLength * halfThickness,
                                     dx * inverseLength * halfThickness);
        const uint32_t flatFirst = static_cast<uint32_t>(vertices_.size());
        const uint32_t flatIndex = static_cast<uint32_t>(indices_.size());
        const Vec2 flatUv(0.0f, 0.0f);
        vertices_.push_back(DrawVertex{Vec2(from.x + flatPerpendicular.x, from.y + flatPerpendicular.y), flatUv, color});
        vertices_.push_back(DrawVertex{Vec2(to.x + flatPerpendicular.x, to.y + flatPerpendicular.y), flatUv, color});
        vertices_.push_back(DrawVertex{Vec2(to.x - flatPerpendicular.x, to.y - flatPerpendicular.y), flatUv, color});
        vertices_.push_back(DrawVertex{Vec2(from.x - flatPerpendicular.x, from.y - flatPerpendicular.y), flatUv, color});
        indices_.push_back(flatFirst + 0u);
        indices_.push_back(flatFirst + 1u);
        indices_.push_back(flatFirst + 2u);
        indices_.push_back(flatFirst + 0u);
        indices_.push_back(flatFirst + 2u);
        indices_.push_back(flatFirst + 3u);
        addGeometryCommand(commands_, flatIndex, 6u, clip);
        return;
    }

    const float fringe = 1.0f;
    float coreHalf = halfThickness;
    Color coreColor = color;
    if (thickness < 1.0f)
    {
        coreHalf = 0.5f;
        coreColor.a = static_cast<uint8_t>(static_cast<float>(color.a) * thickness + 0.5f);
    }
    const Vec2 core(unitPerpendicular.x * coreHalf, unitPerpendicular.y * coreHalf);
    const Vec2 outer(unitPerpendicular.x * (coreHalf + fringe),
                     unitPerpendicular.y * (coreHalf + fringe));
    const Color transparentEdge(coreColor.r, coreColor.g, coreColor.b, 0u);

    // Six vertices per end: transparent outer, opaque core, opaque core,
    // transparent outer - two fringe strips around one solid core strip.
    vertices_.push_back(DrawVertex{Vec2(from.x + outer.x, from.y + outer.y), uv, transparentEdge});
    vertices_.push_back(DrawVertex{Vec2(to.x + outer.x, to.y + outer.y), uv, transparentEdge});
    vertices_.push_back(DrawVertex{Vec2(from.x + core.x, from.y + core.y), uv, coreColor});
    vertices_.push_back(DrawVertex{Vec2(to.x + core.x, to.y + core.y), uv, coreColor});
    vertices_.push_back(DrawVertex{Vec2(from.x - core.x, from.y - core.y), uv, coreColor});
    vertices_.push_back(DrawVertex{Vec2(to.x - core.x, to.y - core.y), uv, coreColor});
    vertices_.push_back(DrawVertex{Vec2(from.x - outer.x, from.y - outer.y), uv, transparentEdge});
    vertices_.push_back(DrawVertex{Vec2(to.x - outer.x, to.y - outer.y), uv, transparentEdge});

    // Three strips: fringe, core, fringe.
    static const uint32_t stripOffsets[3] = {0u, 2u, 4u};
    for (uint32_t strip = 0; strip < 3u; ++strip)
    {
        const uint32_t base = firstVertex + stripOffsets[strip];
        indices_.push_back(base + 0u);
        indices_.push_back(base + 1u);
        indices_.push_back(base + 3u);
        indices_.push_back(base + 0u);
        indices_.push_back(base + 3u);
        indices_.push_back(base + 2u);
    }
    addGeometryCommand(commands_, firstIndex, 18u, clip);
}

void DrawList::addRect(const Rect &rect,
                       const Color &color, const Rect &clip, float thickness)
{
    if (rectOutsideClip(rect, (thickness > 1.0f ? thickness : 1.0f) * 0.5f + 1.0f, clip))
        return;
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
    if (rectOutsideClip(rect, 0.0f, clip))
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
    if (rectOutsideClip(rect, 0.0f, clip))
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
    if (rectOutsideClip(rect, 0.0f, clip))
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
    if (outsideClip(center.x - radius - 1.0f, center.y - radius - 1.0f,
                    center.x + radius + 1.0f, center.y + radius + 1.0f, clip))
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

namespace
{

// Writes the outline of a rounded rectangle, walking the four corner arcs in
// order. `inset` grows (or, when negative, shrinks) the outline away from the
// rectangle - that is what produces the transparent fringe ring.
// Returns the number of points written.
uint32_t buildRoundedOutline(const Rect &rect, float radius, uint32_t cornerSegments,
                             float inset, Vec2 *out)
{
    // The corner centres are fixed by the rectangle itself and must NOT move
    // with the inset: only the arc radius grows. Offsetting the edges as well
    // would push the fringe out by twice the inset on the straight sides.
    const float left = rect.x;
    const float top = rect.y;
    const float right = rect.x + rect.width;
    const float bottom = rect.y + rect.height;
    const float cornerRadius = radius + inset;
    // Y grows downward, so an angle sweeping from 0 upward visits the corners
    // bottom-right, bottom-left, top-left, top-right in that order. The centre
    // of each corner must match the quadrant its arc is drawn in; pairing them
    // the wrong way round makes the outline jump straight across the rectangle
    // between corners, which shows up as a spur at each corner.
    const float cx[4] = {right - radius, left + radius, left + radius, right - radius};
    const float cy[4] = {bottom - radius, bottom - radius, top + radius, top + radius};
    static const float kQuarter = 1.57079632679489661923f;
    uint32_t count = 0;
    for (uint32_t corner = 0; corner < 4u; ++corner)
    {
        const float start = static_cast<float>(corner) * kQuarter;
        for (uint32_t i = 0; i <= cornerSegments; ++i)
        {
            const float angle =
                start + kQuarter * static_cast<float>(i) / static_cast<float>(cornerSegments);
            out[count].x = cx[corner] + cosf(angle) * cornerRadius;
            out[count].y = cy[corner] + sinf(angle) * cornerRadius;
            ++count;
        }
    }
    return count;
}

uint32_t roundedCornerSegments(float radius, uint32_t requested)
{
    if (requested > 0u)
        return requested > 32u ? 32u : requested;
    // Enough segments that a corner reads as a curve rather than a bevel, but
    // no more than the radius can show.
    uint32_t segments = static_cast<uint32_t>(radius * 0.5f) + 3u;
    return segments > 16u ? 16u : segments;
}

} // namespace

void DrawList::addRectFilledRounded(const Rect &rect, float radius, const Color &color,
                                    const Rect &clip, uint32_t cornerSegments)
{
    if (rect.width <= 0.0f || rect.height <= 0.0f || color.a == 0u)
        return;
    if (rectOutsideClip(rect, 1.0f, clip))
        return;

    const float maxRadius = (rect.width < rect.height ? rect.width : rect.height) * 0.5f;
    if (radius > maxRadius)
        radius = maxRadius;
    if (radius <= 0.0f)
    {
        addRectFilled(rect, color, clip);
        return;
    }

    const uint32_t segments = roundedCornerSegments(radius, cornerSegments);
    // Four corners, each contributing segments+1 points.
    const uint32_t pointCount = 4u * (segments + 1u);
    Vec2 inner[4u * (32u + 1u)];
    Vec2 outer[4u * (32u + 1u)];
    buildRoundedOutline(rect, radius, segments, 0.0f, inner);
    buildRoundedOutline(rect, radius, segments, 1.0f, outer);

    const uint32_t firstVertex = static_cast<uint32_t>(vertices_.size());
    const uint32_t firstIndex = static_cast<uint32_t>(indices_.size());
    const Vec2 uv(0.0f, 0.0f);
    const Color transparentEdge(color.r, color.g, color.b, 0u);
    const Vec2 centre(rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f);

    vertices_.push_back(DrawVertex{centre, uv, color});
    for (uint32_t i = 0; i < pointCount; ++i)
        vertices_.push_back(DrawVertex{inner[i], uv, color});
    for (uint32_t i = 0; i < pointCount; ++i)
        vertices_.push_back(DrawVertex{outer[i], uv, transparentEdge});

    // Same construction as addCircleFilled: a fan over the solid interior plus
    // a one-pixel transparent ring that gives the silhouette its soft edge.
    // A rectangle cut by its clip keeps only the triangles that can touch it.
    auto emit = [this, &clip](uint32_t a, uint32_t b, uint32_t c) -> uint32_t
    {
        const Vec2 &pa = vertices_[a].position;
        const Vec2 &pb = vertices_[b].position;
        const Vec2 &pc = vertices_[c].position;
        const float minX = pa.x < pb.x ? (pa.x < pc.x ? pa.x : pc.x) : (pb.x < pc.x ? pb.x : pc.x);
        const float maxX = pa.x > pb.x ? (pa.x > pc.x ? pa.x : pc.x) : (pb.x > pc.x ? pb.x : pc.x);
        const float minY = pa.y < pb.y ? (pa.y < pc.y ? pa.y : pc.y) : (pb.y < pc.y ? pb.y : pc.y);
        const float maxY = pa.y > pb.y ? (pa.y > pc.y ? pa.y : pc.y) : (pb.y > pc.y ? pb.y : pc.y);
        if (outsideClip(minX, minY, maxX, maxY, clip) ||
            minX >= clip.x + clip.width + CullMargin || minY >= clip.y + clip.height + CullMargin ||
            maxX <= clip.x - CullMargin || maxY <= clip.y - CullMargin)
            return 0u;
        indices_.push_back(static_cast<DrawIndex>(a));
        indices_.push_back(static_cast<DrawIndex>(b));
        indices_.push_back(static_cast<DrawIndex>(c));
        return 3u;
    };
    uint32_t emitted = 0u;
    for (uint32_t i = 0; i < pointCount; ++i)
    {
        const uint32_t next = (i + 1u) % pointCount;
        const uint32_t innerIndex = firstVertex + 1u + i;
        const uint32_t innerNext = firstVertex + 1u + next;
        const uint32_t outerIndex = firstVertex + 1u + pointCount + i;
        const uint32_t outerNext = firstVertex + 1u + pointCount + next;

        emitted += emit(firstVertex, innerIndex, innerNext);
        emitted += emit(innerIndex, outerIndex, outerNext);
        emitted += emit(innerIndex, outerNext, innerNext);
    }
    if (emitted == 0u)
    {
        vertices_.resize(firstVertex);
        return;
    }
    addGeometryCommand(commands_, firstIndex, emitted, clip);
}

void DrawList::addArc(const Vec2 &center, float radius, float from, float to,
                      const Color &color, const Rect &clip, float thickness,
                      uint32_t segments)
{
    if (radius <= 0.0f || thickness <= 0.0f || color.a == 0u)
        return;
    // The whole circle's box: cheap, and never smaller than the arc's.
    const float reach = radius + thickness * 0.5f + 1.0f;
    if (outsideClip(center.x - reach, center.y - reach, center.x + reach, center.y + reach, clip))
        return;
    if (to < from)
    {
        const float swap = from;
        from = to;
        to = swap;
    }
    const float sweep = to - from;
    if (sweep <= 0.0f)
        return;

    if (segments == 0u)
    {
        // Enough that the chord error stays well under a pixel: the sagitta of
        // one segment is r*(1-cos(step/2)), so tie the count to both the arc
        // length and its angle.
        const float lengthSegments = radius * sweep * 0.5f;
        segments = static_cast<uint32_t>(lengthSegments) + 6u;
    }
    if (segments < 3u)
        segments = 3u;
    if (segments > 256u)
        segments = 256u;

    const float half = thickness * 0.5f;
    const float fringe = 1.0f;
    // A band thinner than a pixel keeps a one-pixel core and loses opacity
    // instead, the same rule addLine uses for hairlines.
    float coreHalf = half;
    Color coreColor = color;
    if (thickness < 1.0f)
    {
        coreHalf = 0.5f;
        coreColor.a = static_cast<uint8_t>(static_cast<float>(color.a) * thickness + 0.5f);
    }
    const Color transparentEdge(coreColor.r, coreColor.g, coreColor.b, 0u);

    const uint32_t firstVertex = static_cast<uint32_t>(vertices_.size());
    const uint32_t firstIndex = static_cast<uint32_t>(indices_.size());
    const Vec2 uv(0.0f, 0.0f);
    const uint32_t ringPoints = segments + 1u;

    // Four concentric rings: outer fringe, outer core, inner core, inner
    // fringe. Every ring shares the same angles, so the strips between them
    // have no joints at all.
    for (uint32_t ring = 0; ring < 4u; ++ring)
    {
        float offset = 0.0f;
        Color ringColor = coreColor;
        switch (ring)
        {
        case 0: offset = coreHalf + fringe; ringColor = transparentEdge; break;
        case 1: offset = coreHalf; break;
        case 2: offset = -coreHalf; break;
        default: offset = -(coreHalf + fringe); ringColor = transparentEdge; break;
        }
        float ringRadius = radius + offset;
        if (ringRadius < 0.0f)
            ringRadius = 0.0f;
        for (uint32_t i = 0; i < ringPoints; ++i)
        {
            const float angle =
                from + sweep * static_cast<float>(i) / static_cast<float>(segments);
            vertices_.push_back(DrawVertex{
                Vec2(center.x + cosf(angle) * ringRadius,
                     center.y + sinf(angle) * ringRadius),
                uv, ringColor});
        }
    }

    for (uint32_t strip = 0; strip < 3u; ++strip)
    {
        const uint32_t outerBase = firstVertex + strip * ringPoints;
        const uint32_t innerBase = outerBase + ringPoints;
        for (uint32_t i = 0; i + 1u < ringPoints; ++i)
        {
            indices_.push_back(outerBase + i);
            indices_.push_back(outerBase + i + 1u);
            indices_.push_back(innerBase + i + 1u);
            indices_.push_back(outerBase + i);
            indices_.push_back(innerBase + i + 1u);
            indices_.push_back(innerBase + i);
        }
    }
    addGeometryCommand(commands_, firstIndex, segments * 18u, clip);
}

void DrawList::addRectRounded(const Rect &rect, float radius, const Color &color,
                              const Rect &clip, float thickness, uint32_t cornerSegments)
{
    if (rect.width <= 0.0f || rect.height <= 0.0f || color.a == 0u || thickness <= 0.0f)
        return;
    if (rectOutsideClip(rect, (thickness > 1.0f ? thickness : 1.0f) * 0.5f + 1.0f, clip))
        return;

    const float maxRadius = (rect.width < rect.height ? rect.width : rect.height) * 0.5f;
    if (radius > maxRadius)
        radius = maxRadius;
    if (radius <= 0.0f)
    {
        addRect(rect, color, clip, thickness);
        return;
    }

    const uint32_t segments = roundedCornerSegments(radius, cornerSegments);
    const uint32_t pointCount = 4u * (segments + 1u);
    Vec2 outline[4u * (32u + 1u)];
    buildRoundedOutline(rect, radius, segments, 0.0f, outline);

    // Stroke the outline segment by segment; addLine already anti-aliases the
    // diagonals these arcs are made of.
    for (uint32_t i = 0; i < pointCount; ++i)
    {
        const uint32_t next = (i + 1u) % pointCount;
        addLine(outline[i], outline[next], color, clip, thickness);
    }
}

void DrawList::addPolygonFilled(Span<const Vec2> points, const Color &color, const Rect &clip)
{
    if (points.size() < 3u || color.a == 0u)
        return;
    float minX = points[0].x, minY = points[0].y, maxX = points[0].x, maxY = points[0].y;
    for (Span<const Vec2>::size_type i = 1; i < points.size(); ++i)
    {
        minX = points[i].x < minX ? points[i].x : minX;
        minY = points[i].y < minY ? points[i].y : minY;
        maxX = points[i].x > maxX ? points[i].x : maxX;
        maxY = points[i].y > maxY ? points[i].y : maxY;
    }
    if (outsideClip(minX - 1.0f, minY - 1.0f, maxX + 1.0f, maxY + 1.0f, clip))
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
    // The backend shapes this text, so its width and line count are unknown
    // here: only cull what certainly starts past the clip's right or bottom
    // edge (text runs right and down from position).
    if (!(clip.width > 0.0f) || !(clip.height > 0.0f) ||
        position.x > clip.x + clip.width + CullMargin ||
        position.y > clip.y + clip.height + CullMargin)
        return;
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
