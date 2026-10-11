#include "igui/DrawData.hpp"

#include <math.h>
#include <string.h>

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

// The unit circle sampled once. Circles, rounded corners and the arcs they are
// made of read their points from here instead of calling sinf/cosf per vertex
// per frame, which was most of what a rounded button cost.
struct UnitCircle
{
    static const uint32_t Samples = 64u;
    static const uint32_t Quarter = Samples / 4u;
    Vec2 points[Samples + 1u]; // points[Samples] repeats points[0]

    UnitCircle()
    {
        for (uint32_t i = 0u; i <= Samples; ++i)
        {
            const float angle = 6.28318530717958647692f * static_cast<float>(i % Samples) /
                                static_cast<float>(Samples);
            points[i] = Vec2(cosf(angle), sinf(angle));
        }
    }
};

const UnitCircle &unitCircle()
{
    static const UnitCircle table;
    return table;
}

bool rectContains(const Rect &outer, const Rect &inner)
{
    return inner.x >= outer.x && inner.y >= outer.y &&
           inner.x + inner.width <= outer.x + outer.width &&
           inner.y + inner.height <= outer.y + outer.height;
}

} // namespace

void DrawList::emitGeometry(uint32_t firstIndex, uint32_t indexCount, const Rect &clip,
                            TextureId texture, const Rect *bounds)
{
    if (!commands_.empty() && commands_.back().type == DrawCommandType::Geometry)
    {
        GeometryCommand &previous = commands_.back().payload.geometry;
        if (previous.texture == texture && previous.vertexOffset == 0 &&
            previous.firstIndex + previous.indexCount == firstIndex)
        {
            // Same scissor: the run goes on. A tighter scissor that the new
            // geometry never reaches is no scissor at all, so the run goes on
            // under the previous one too - this is what keeps a text field's
            // clipped label, or a whole window that fits its content area, from
            // starting a draw call of its own.
            if (sameRect(previous.clip, clip) ||
                (rectContains(previous.clip, clip) &&
                 (bounds ? rectContains(clip, *bounds)
                         : indexedVerticesInside(firstIndex, indexCount, clip))))
            {
                previous.indexCount += indexCount;
                return;
            }
        }
    }
    GeometryCommand geometry;
    geometry.clip = clip;
    geometry.texture = texture;
    geometry.firstIndex = firstIndex;
    geometry.indexCount = indexCount;
    geometry.vertexOffset = 0;
    commands_.push_back(DrawCommand(geometry));
}

bool DrawList::indexedVerticesInside(uint32_t firstIndex, uint32_t indexCount,
                                     const Rect &clip) const
{
    const float right = clip.x + clip.width;
    const float bottom = clip.y + clip.height;
    const DrawIndex *index = geometry_->indices.data() + firstIndex;
    const DrawIndex *const end = index + indexCount;
    for (; index != end; ++index)
    {
        const Vec2 &p = geometry_->vertices[*index].position;
        if (p.x < clip.x || p.y < clip.y || p.x > right || p.y > bottom)
            return false;
    }
    return true;
}

DrawIndex *DrawList::allocIndices(uint32_t count)
{
    const ct::Vector<DrawIndex>::size_type first = geometry_->indices.size();
    geometry_->indices.resize(first + count);
    return geometry_->indices.data() + first;
}

void DrawList::writeQuadIndices(uint32_t firstVertex)
{
    DrawIndex *out = allocIndices(6u);
    out[0] = firstVertex;
    out[1] = firstVertex + 1u;
    out[2] = firstVertex + 2u;
    out[3] = firstVertex;
    out[4] = firstVertex + 2u;
    out[5] = firstVertex + 3u;
}

DrawList::DrawList()
    : own_(), geometry_(&own_), commands_(), solidTexture_(), solidUv_(0.0f, 0.0f),
      fadeBackground_(), disabledKeep_(1.0f)
{
}

DrawList::DrawList(const DrawList &other)
    : own_(other.own_), geometry_(other.geometry_ == &other.own_ ? &own_ : other.geometry_),
      commands_(other.commands_), solidTexture_(other.solidTexture_), solidUv_(other.solidUv_),
      fadeBackground_(other.fadeBackground_), disabledKeep_(other.disabledKeep_)
{
}

DrawList &DrawList::operator=(const DrawList &other)
{
    if (this == &other)
        return *this;
    own_ = other.own_;
    geometry_ = other.geometry_ == &other.own_ ? &own_ : other.geometry_;
    commands_ = other.commands_;
    solidTexture_ = other.solidTexture_;
    solidUv_ = other.solidUv_;
    fadeBackground_ = other.fadeBackground_;
    disabledKeep_ = other.disabledKeep_;
    return *this;
}

void DrawList::shareGeometry(DrawList &store)
{
    geometry_ = store.geometry_;
}

void DrawList::clearCommands()
{
    commands_.clear();
}

void DrawList::clear()
{
    if (geometry_ == &own_)
    {
        own_.vertices.clear();
        own_.indices.clear();
        own_.textBytes.clear();
    }
    commands_.clear();
    // The solid style belongs to whatever set it for this frame's content, so a
    // reused list must not keep it (see setSolidStyle). The disabled fade is the
    // same: a block turns it on, not a list.
    solidTexture_ = TextureId();
    solidUv_ = Vec2(0.0f, 0.0f);
    disabledKeep_ = 1.0f;
}

void DrawList::setSolidStyle(TextureId texture, const Vec2 &whiteUv)
{
    solidTexture_ = texture;
    solidUv_ = whiteUv;
}

void DrawList::setDisabledFade(bool enabled, const Color &background, float keep)
{
    fadeBackground_ = background;
    if (!enabled || keep >= 1.0f || keep < 0.0f)
    {
        disabledKeep_ = 1.0f;
        return;
    }
    disabledKeep_ = keep;
}

void DrawList::QuadBatch::reserve(uint32_t quadCount)
{
    drawList_.geometry_->vertices.reserve(drawList_.geometry_->vertices.size() + quadCount * 4u);
    drawList_.geometry_->indices.reserve(drawList_.geometry_->indices.size() + quadCount * 6u);
}

void DrawList::QuadBatch::add(const Rect &rect, const Vec2 &uvMin, const Vec2 &uvMax,
                              const Color &color)
{
    const Color drawn = drawList_.faded(color);
    if (rect.width <= 0.0f || rect.height <= 0.0f || drawn.a == 0u)
        return;
    float x0 = rect.x;
    float y0 = rect.y;
    float x1 = rect.x + rect.width;
    float y1 = rect.y + rect.height;
    float u0 = uvMin.x;
    float v0 = uvMin.y;
    float u1 = uvMax.x;
    float v1 = uvMax.y;
    const float clipRight = clip_.x + clip_.width;
    const float clipBottom = clip_.y + clip_.height;
    if (x0 < clip_.x || y0 < clip_.y || x1 > clipRight || y1 > clipBottom)
    {
        // Trimmed to the clip here, texture coordinates with it, instead of
        // leaving the overhang to the scissor: the run then never needs a
        // scissor of its own and merges into the surrounding draw call.
        if (x1 <= clip_.x || y1 <= clip_.y || x0 >= clipRight || y0 >= clipBottom)
            return;
        const float uPerPixel = (u1 - u0) / (x1 - x0);
        const float vPerPixel = (v1 - v0) / (y1 - y0);
        if (x0 < clip_.x) { u0 += (clip_.x - x0) * uPerPixel; x0 = clip_.x; }
        if (y0 < clip_.y) { v0 += (clip_.y - y0) * vPerPixel; y0 = clip_.y; }
        if (x1 > clipRight) { u1 -= (x1 - clipRight) * uPerPixel; x1 = clipRight; }
        if (y1 > clipBottom) { v1 -= (y1 - clipBottom) * vPerPixel; y1 = clipBottom; }
        if (x1 <= x0 || y1 <= y0)
            return;
    }

    if (!open_)
    {
        firstVertex_ = static_cast<uint32_t>(drawList_.geometry_->vertices.size());
        open_ = true;
    }

    // Vertices only: the indices of a run are all alike, and flush() writes
    // them in one go, which is far cheaper than six pushes per quad.
    drawList_.geometry_->vertices.emplace_back(Vec2(x0, y0), Vec2(u0, v0), drawn);
    drawList_.geometry_->vertices.emplace_back(Vec2(x1, y0), Vec2(u1, v0), drawn);
    drawList_.geometry_->vertices.emplace_back(Vec2(x1, y1), Vec2(u1, v1), drawn);
    drawList_.geometry_->vertices.emplace_back(Vec2(x0, y1), Vec2(u0, v1), drawn);
    ++quadCount_;
}

void DrawList::QuadBatch::flush()
{
    if (!open_ || quadCount_ == 0u)
    {
        open_ = false;
        quadCount_ = 0u;
        return;
    }
    ct::Vector<DrawIndex> &indices = drawList_.geometry_->indices;
    const uint32_t firstIndex = static_cast<uint32_t>(indices.size());
    indices.resize(firstIndex + quadCount_ * 6u);
    DrawIndex *out = indices.data() + firstIndex;
    for (uint32_t quad = 0u; quad < quadCount_; ++quad, out += 6)
    {
        const DrawIndex base = firstVertex_ + quad * 4u;
        out[0] = base;
        out[1] = base + 1u;
        out[2] = base + 2u;
        out[3] = base;
        out[4] = base + 2u;
        out[5] = base + 3u;
    }
    // One command for the whole run: the quads were written contiguously,
    // share the clip rect and the texture, and add() trimmed each to the clip.
    drawList_.emitGeometry(firstIndex, quadCount_ * 6u, clip_, texture_, &clip_);
    open_ = false;
    quadCount_ = 0u;
}

void DrawList::append(const DrawList &other)
{
    if (other.geometry_ == geometry_)
    {
        // Same arrays: the commands already point at the right geometry, so
        // only they move - through the usual merge, so a run that continues
        // across the seam stays one draw call.
        for (ct::Vector<DrawCommand>::size_type i = 0; i < other.commands_.size(); ++i)
        {
            const DrawCommand &command = other.commands_[i];
            if (command.type == DrawCommandType::Geometry)
            {
                const GeometryCommand &geometry = command.payload.geometry;
                emitGeometry(geometry.firstIndex, geometry.indexCount, geometry.clip,
                             geometry.texture, nullptr);
            }
            else
                commands_.push_back(command);
        }
        return;
    }

    const uint32_t vertexOffset = static_cast<uint32_t>(geometry_->vertices.size());
    const uint32_t indexOffset = static_cast<uint32_t>(geometry_->indices.size());
    const uint32_t textOffset = static_cast<uint32_t>(geometry_->textBytes.size());

    // Whole-array copies: a frame's worth of glyph quads goes through here once
    // per window, and one element at a time it was a measurable share of the
    // frame.
    geometry_->vertices.resize(vertexOffset + other.geometry_->vertices.size());
    if (!other.geometry_->vertices.empty())
        memcpy(geometry_->vertices.data() + vertexOffset, other.geometry_->vertices.data(),
               other.geometry_->vertices.size() * sizeof(DrawVertex));
    geometry_->indices.resize(indexOffset + other.geometry_->indices.size());
    {
        DrawIndex *out = geometry_->indices.data() + indexOffset;
        const DrawIndex *in = other.geometry_->indices.data();
        const ct::Vector<DrawIndex>::size_type count = other.geometry_->indices.size();
        for (ct::Vector<DrawIndex>::size_type i = 0; i < count; ++i)
            out[i] = in[i] + vertexOffset;
    }
    geometry_->textBytes.resize(textOffset + other.geometry_->textBytes.size());
    if (!other.geometry_->textBytes.empty())
        memcpy(geometry_->textBytes.data() + textOffset, other.geometry_->textBytes.data(), other.geometry_->textBytes.size());

    for (ct::Vector<DrawCommand>::size_type i = 0; i < other.commands_.size(); ++i)
    {
        const DrawCommand &command = other.commands_[i];
        if (command.type == DrawCommandType::Geometry)
        {
            // Through the same merge as a fresh command, so two windows whose
            // chrome shares the viewport clip become one draw call at the seam.
            const GeometryCommand &geometry = command.payload.geometry;
            emitGeometry(geometry.firstIndex + indexOffset, geometry.indexCount, geometry.clip,
                         geometry.texture, nullptr);
            continue;
        }
        DrawCommand text(command);
        text.payload.text.textOffset += textOffset;
        commands_.push_back(text);
    }
}

void DrawList::addLine(const Vec2 &from, const Vec2 &to,
                       const Color &color, const Rect &clip, float thickness)
{
    if (thickness <= 0.0f || color.a == 0u)
        return;
    // Half the (at least 1px) core plus the 1px fringe on each side.
    const float reach = (thickness > 1.0f ? thickness : 1.0f) * 0.5f + 1.0f;
    const Rect bounds((from.x < to.x ? from.x : to.x) - reach, (from.y < to.y ? from.y : to.y) - reach,
                      fabsf(to.x - from.x) + reach * 2.0f, fabsf(to.y - from.y) + reach * 2.0f);
    if (rectOutsideClip(bounds, 0.0f, clip))
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
    const Color shade = faded(color);

    const float inverseLength = 1.0f / sqrtf(lengthSquared);
    const Vec2 unitPerpendicular(-dy * inverseLength, dx * inverseLength);
    const uint32_t firstVertex = static_cast<uint32_t>(geometry_->vertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(geometry_->indices.size());
    const Vec2 uv = solidUv_;

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
        const uint32_t flatFirst = static_cast<uint32_t>(geometry_->vertices.size());
        const uint32_t flatIndex = static_cast<uint32_t>(geometry_->indices.size());
        geometry_->vertices.emplace_back(Vec2(from.x + flatPerpendicular.x, from.y + flatPerpendicular.y), uv, shade);
        geometry_->vertices.emplace_back(Vec2(to.x + flatPerpendicular.x, to.y + flatPerpendicular.y), uv, shade);
        geometry_->vertices.emplace_back(Vec2(to.x - flatPerpendicular.x, to.y - flatPerpendicular.y), uv, shade);
        geometry_->vertices.emplace_back(Vec2(from.x - flatPerpendicular.x, from.y - flatPerpendicular.y), uv, shade);
        writeQuadIndices(flatFirst);
        emitGeometry(flatIndex, 6u, clip, solidTexture_, &bounds);
        return;
    }

    const float fringe = 1.0f;
    float coreHalf = halfThickness;
    Color coreColor = shade;
    if (thickness < 1.0f)
    {
        coreHalf = 0.5f;
        coreColor.a = static_cast<uint8_t>(static_cast<float>(shade.a) * thickness + 0.5f);
    }
    const Vec2 core(unitPerpendicular.x * coreHalf, unitPerpendicular.y * coreHalf);
    const Vec2 outer(unitPerpendicular.x * (coreHalf + fringe),
                     unitPerpendicular.y * (coreHalf + fringe));
    const Color transparentEdge(coreColor.r, coreColor.g, coreColor.b, 0u);

    // Six vertices per end: transparent outer, opaque core, opaque core,
    // transparent outer - two fringe strips around one solid core strip.
    geometry_->vertices.emplace_back(Vec2(from.x + outer.x, from.y + outer.y), uv, transparentEdge);
    geometry_->vertices.emplace_back(Vec2(to.x + outer.x, to.y + outer.y), uv, transparentEdge);
    geometry_->vertices.emplace_back(Vec2(from.x + core.x, from.y + core.y), uv, coreColor);
    geometry_->vertices.emplace_back(Vec2(to.x + core.x, to.y + core.y), uv, coreColor);
    geometry_->vertices.emplace_back(Vec2(from.x - core.x, from.y - core.y), uv, coreColor);
    geometry_->vertices.emplace_back(Vec2(to.x - core.x, to.y - core.y), uv, coreColor);
    geometry_->vertices.emplace_back(Vec2(from.x - outer.x, from.y - outer.y), uv, transparentEdge);
    geometry_->vertices.emplace_back(Vec2(to.x - outer.x, to.y - outer.y), uv, transparentEdge);

    // Three strips: fringe, core, fringe.
    static const uint32_t stripOffsets[3] = {0u, 2u, 4u};
    DrawIndex *out = allocIndices(18u);
    for (uint32_t strip = 0; strip < 3u; ++strip, out += 6)
    {
        const uint32_t base = firstVertex + stripOffsets[strip];
        out[0] = base;
        out[1] = base + 1u;
        out[2] = base + 3u;
        out[3] = base;
        out[4] = base + 3u;
        out[5] = base + 2u;
    }
    emitGeometry(firstIndex, 18u, clip, solidTexture_, &bounds);
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
    const Color shade = faded(color);
    if (rect.width <= 0.0f || rect.height <= 0.0f || shade.a == 0u)
        return;
    if (rectOutsideClip(rect, 0.0f, clip))
        return;

    const uint32_t firstVertex = static_cast<uint32_t>(geometry_->vertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(geometry_->indices.size());

    const Vec2 uv = solidUv_;
    geometry_->vertices.emplace_back(Vec2(rect.x, rect.y), uv, shade);
    geometry_->vertices.emplace_back(Vec2(rect.x + rect.width, rect.y), uv, shade);
    geometry_->vertices.emplace_back(Vec2(rect.x + rect.width, rect.y + rect.height), uv, shade);
    geometry_->vertices.emplace_back(Vec2(rect.x, rect.y + rect.height), uv, shade);

    writeQuadIndices(firstVertex);

    emitGeometry(firstIndex, 6u, clip, solidTexture_, &rect);
}

void DrawList::addRectGradient(const Rect &rect, const Color &topLeft, const Color &topRight,
                               const Color &bottomRight, const Color &bottomLeft, const Rect &clip)
{
    const Color shadeTopLeft = faded(topLeft);
    const Color shadeTopRight = faded(topRight);
    const Color shadeBottomRight = faded(bottomRight);
    const Color shadeBottomLeft = faded(bottomLeft);
    if (rect.width <= 0.0f || rect.height <= 0.0f)
        return;
    if (rectOutsideClip(rect, 0.0f, clip))
        return;

    const uint32_t firstVertex = static_cast<uint32_t>(geometry_->vertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(geometry_->indices.size());
    const Vec2 uv = solidUv_;
    geometry_->vertices.emplace_back(Vec2(rect.x, rect.y), uv, shadeTopLeft);
    geometry_->vertices.emplace_back(Vec2(rect.x + rect.width, rect.y), uv, shadeTopRight);
    geometry_->vertices.emplace_back(Vec2(rect.x + rect.width, rect.y + rect.height), uv, shadeBottomRight);
    geometry_->vertices.emplace_back(Vec2(rect.x, rect.y + rect.height), uv, shadeBottomLeft);
    writeQuadIndices(firstVertex);
    emitGeometry(firstIndex, 6u, clip, solidTexture_, &rect);
}

void DrawList::addImage(TextureId texture, const Rect &rect, const Vec2 &uvMin,
                        const Vec2 &uvMax, const Color &color, const Rect &clip)
{
    const Color shade = faded(color);
    if (rect.width <= 0.0f || rect.height <= 0.0f || shade.a == 0u)
        return;
    if (rectOutsideClip(rect, 0.0f, clip))
        return;

    const uint32_t firstVertex = static_cast<uint32_t>(geometry_->vertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(geometry_->indices.size());
    geometry_->vertices.emplace_back(Vec2(rect.x, rect.y), uvMin, shade);
    geometry_->vertices.emplace_back(Vec2(rect.x + rect.width, rect.y),
                                   Vec2(uvMax.x, uvMin.y), shade);
    geometry_->vertices.emplace_back(Vec2(rect.x + rect.width, rect.y + rect.height), uvMax, shade);
    geometry_->vertices.emplace_back(Vec2(rect.x, rect.y + rect.height),
                                   Vec2(uvMin.x, uvMax.y), shade);
    writeQuadIndices(firstVertex);

    emitGeometry(firstIndex, 6u, clip, texture, &rect);
}

void DrawList::addCircleFilled(const Vec2 &center, float radius,
                               const Color &color, const Rect &clip, uint32_t segments)
{
    const Color shade = faded(color);
    if (radius <= 0.0f || shade.a == 0u)
        return;
    const Rect bounds(center.x - radius - 1.0f, center.y - radius - 1.0f,
                      (radius + 1.0f) * 2.0f, (radius + 1.0f) * 2.0f);
    if (rectOutsideClip(bounds, 0.0f, clip))
        return;

    // A count that divides the unit-circle table is read from it; the
    // automatic count is rounded to one that does. Another explicit count is
    // honoured with sinf/cosf.
    uint32_t stride = 0u;
    if (segments < 3u)
    {
        // O mesmo critério do ImGui para o número de segmentos de um círculo: o
        // que mantém a flecha da corda abaixo de 0.3 px. Um botão de slider tem
        // ~7 px de raio, e isso dá 10 segmentos - a fórmula antiga (`radius * 0.8
        // + 12`) dava 17, que depois de encaixados na tabela eram 21 segmentos e
        // 43 vértices por círculo, num sítio onde o ImGui gasta ~26.
        const float maxError = radius < 0.3f ? radius : 0.3f;
        const float angle = acosf(1.0f - maxError / radius);
        segments = static_cast<uint32_t>(3.14159265358979323846f / angle + 0.5f);
        segments = segments < 8u ? 8u : (segments > UnitCircle::Samples ? UnitCircle::Samples : segments);
        stride = UnitCircle::Samples / segments;
        segments = UnitCircle::Samples / stride;
    }
    else if (segments <= UnitCircle::Samples && UnitCircle::Samples % segments == 0u)
        stride = UnitCircle::Samples / segments;
    segments = segments > 256u ? 256u : segments;

    const uint32_t firstVertex = static_cast<uint32_t>(geometry_->vertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(geometry_->indices.size());
    const Vec2 uv = solidUv_;
    const float step = 6.28318530717958647692f / static_cast<float>(segments);
    const Color transparentEdge(shade.r, shade.g, shade.b, 0u);
    const float fringe = radius < 2.0f ? 0.5f : 1.0f;

    // The unit circle once; both rings reuse it.
    float unitX[256];
    float unitY[256];
    if (stride != 0u)
    {
        const UnitCircle &unit = unitCircle();
        for (uint32_t i = 0; i < segments; ++i)
        {
            unitX[i] = unit.points[i * stride].x;
            unitY[i] = unit.points[i * stride].y;
        }
    }
    else
    {
        for (uint32_t i = 0; i < segments; ++i)
        {
            const float angle = step * static_cast<float>(i);
            unitX[i] = cosf(angle);
            unitY[i] = sinf(angle);
        }
    }
    geometry_->vertices.emplace_back(center, uv, shade);
    for (uint32_t i = 0; i < segments; ++i)
        geometry_->vertices.emplace_back(Vec2(center.x + unitX[i] * radius, center.y + unitY[i] * radius),
                               uv, shade);
    const float outerRadius = radius + fringe;
    for (uint32_t i = 0; i < segments; ++i)
        geometry_->vertices.emplace_back(Vec2(center.x + unitX[i] * outerRadius,
                                    center.y + unitY[i] * outerRadius),
                               uv, transparentEdge);
    DrawIndex *out = allocIndices(segments * 9u);
    for (uint32_t i = 0; i < segments; ++i, out += 9)
    {
        const uint32_t next = (i + 1u) % segments;
        const uint32_t inner = firstVertex + 1u + i;
        const uint32_t innerNext = firstVertex + 1u + next;
        const uint32_t outer = firstVertex + 1u + segments + i;
        const uint32_t outerNext = firstVertex + 1u + segments + next;

        out[0] = firstVertex;
        out[1] = inner;
        out[2] = innerNext;
        // One transparent pixel of geometry gives the circle a stable
        // anti-aliased edge on every backend, without needing a font glyph.
        out[3] = inner;
        out[4] = outer;
        out[5] = outerNext;
        out[6] = inner;
        out[7] = outerNext;
        out[8] = innerNext;
    }
    emitGeometry(firstIndex, segments * 9u, clip, solidTexture_, &bounds);
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
    // A count that divides the table's quarter (what roundedCornerSegments
    // hands out) is read from the table; any other is computed.
    const bool tabulated = cornerSegments > 0u && cornerSegments <= UnitCircle::Quarter &&
                           UnitCircle::Quarter % cornerSegments == 0u;
    const uint32_t stride = tabulated ? UnitCircle::Quarter / cornerSegments : 0u;
    const UnitCircle &unit = unitCircle();
    uint32_t count = 0;
    for (uint32_t corner = 0; corner < 4u; ++corner)
    {
        const float start = static_cast<float>(corner) * kQuarter;
        for (uint32_t i = 0; i <= cornerSegments; ++i)
        {
            if (tabulated)
            {
                const Vec2 &u = unit.points[corner * UnitCircle::Quarter + i * stride];
                out[count].x = cx[corner] + u.x * cornerRadius;
                out[count].y = cy[corner] + u.y * cornerRadius;
            }
            else
            {
                const float angle =
                    start + kQuarter * static_cast<float>(i) / static_cast<float>(cornerSegments);
                out[count].x = cx[corner] + cosf(angle) * cornerRadius;
                out[count].y = cy[corner] + sinf(angle) * cornerRadius;
            }
            ++count;
        }
    }
    return count;
}

// Quantos segmentos um canto de raio `radius` leva: o critério é o mesmo do
// ImGui (o CircleTessellationMaxError, 0.3 px por omissão): o número de
// segmentos do círculo inteiro para o qual a flecha da corda - a distância entre
// o arco e a corda - fica abaixo desse erro, e um canto é um quarto disso. O
// que estava aqui antes (`radius * 0.5 + 3`, arredondado para 4/8/16) dava 8
// segmentos a um canto de 4 px, o que custava 73 vértices por moldura redonda -
// mais do que os ~30 do ImGui para a mesma moldura, e é isso que se nota num
// slider (duas faixas em forma de pílula) ou num botão.
uint32_t roundedCornerSegments(float radius, uint32_t requested)
{
    if (requested > 0u)
        return requested > 16u ? 16u : requested;
    if (radius <= 0.0f)
        return 1u;
    const float maxError = radius < 0.3f ? radius : 0.3f;
    const float angle = acosf(1.0f - maxError / radius);
    uint32_t segments = static_cast<uint32_t>(3.14159265358979323846f / angle * 0.25f + 0.5f);
    if (segments < 2u)
        segments = 2u;
    // Arredondado para um divisor do quarto da tabela do círculo unitário, para
    // o arco ser lido dela em vez de calculado com sinf/cosf (ver
    // buildRoundedOutline); para cima, para o arredondamento nunca piorar o erro.
    return segments <= 2u ? 2u : (segments <= 4u ? 4u : (segments <= 8u ? 8u : 16u));
}

} // namespace

void DrawList::addRectFilledRounded(const Rect &rect, float radius, const Color &color,
                                    const Rect &clip, uint32_t cornerSegments)
{
    if (rect.width <= 0.0f || rect.height <= 0.0f || color.a == 0u)
        return;
    const Rect bounds(rect.x - 1.0f, rect.y - 1.0f, rect.width + 2.0f, rect.height + 2.0f);
    if (rectOutsideClip(bounds, 0.0f, clip))
        return;

    const float maxRadius = (rect.width < rect.height ? rect.width : rect.height) * 0.5f;
    if (radius > maxRadius)
        radius = maxRadius;
    if (radius <= 0.0f)
    {
        addRectFilled(rect, color, clip);
        return;
    }
    const Color shade = faded(color);

    const uint32_t segments = roundedCornerSegments(radius, cornerSegments);
    // Four corners, each contributing segments+1 points.
    const uint32_t pointCount = 4u * (segments + 1u);
    Vec2 inner[4u * (32u + 1u)];
    Vec2 outer[4u * (32u + 1u)];
    buildRoundedOutline(rect, radius, segments, 0.0f, inner);
    buildRoundedOutline(rect, radius, segments, 1.0f, outer);

    const uint32_t firstVertex = static_cast<uint32_t>(geometry_->vertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(geometry_->indices.size());
    const Vec2 uv = solidUv_;
    const Color transparentEdge(shade.r, shade.g, shade.b, 0u);
    const Vec2 centre(rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f);

    geometry_->vertices.emplace_back(centre, uv, shade);
    for (uint32_t i = 0; i < pointCount; ++i)
        geometry_->vertices.emplace_back(inner[i], uv, shade);
    for (uint32_t i = 0; i < pointCount; ++i)
        geometry_->vertices.emplace_back(outer[i], uv, transparentEdge);

    // Same construction as addCircleFilled: a fan over the solid interior plus
    // a one-pixel transparent ring that gives the silhouette its soft edge.
    // A rectangle cut by its clip keeps only the triangles that can touch it.
    auto emit = [this, &clip](uint32_t a, uint32_t b, uint32_t c) -> uint32_t
    {
        const Vec2 &pa = geometry_->vertices[a].position;
        const Vec2 &pb = geometry_->vertices[b].position;
        const Vec2 &pc = geometry_->vertices[c].position;
        const float minX = pa.x < pb.x ? (pa.x < pc.x ? pa.x : pc.x) : (pb.x < pc.x ? pb.x : pc.x);
        const float maxX = pa.x > pb.x ? (pa.x > pc.x ? pa.x : pc.x) : (pb.x > pc.x ? pb.x : pc.x);
        const float minY = pa.y < pb.y ? (pa.y < pc.y ? pa.y : pc.y) : (pb.y < pc.y ? pb.y : pc.y);
        const float maxY = pa.y > pb.y ? (pa.y > pc.y ? pa.y : pc.y) : (pb.y > pc.y ? pb.y : pc.y);
        if (outsideClip(minX, minY, maxX, maxY, clip) ||
            minX >= clip.x + clip.width + CullMargin || minY >= clip.y + clip.height + CullMargin ||
            maxX <= clip.x - CullMargin || maxY <= clip.y - CullMargin)
            return 0u;
        geometry_->indices.push_back(static_cast<DrawIndex>(a));
        geometry_->indices.push_back(static_cast<DrawIndex>(b));
        geometry_->indices.push_back(static_cast<DrawIndex>(c));
        return 3u;
    };
    uint32_t emitted = 0u;
    if (rectContains(clip, bounds))
    {
        // Nothing to cull: every triangle, written in one go.
        DrawIndex *out = allocIndices(pointCount * 9u);
        for (uint32_t i = 0; i < pointCount; ++i, out += 9)
        {
            const uint32_t next = (i + 1u) % pointCount;
            const uint32_t innerIndex = firstVertex + 1u + i;
            const uint32_t innerNext = firstVertex + 1u + next;
            const uint32_t outerIndex = firstVertex + 1u + pointCount + i;
            const uint32_t outerNext = firstVertex + 1u + pointCount + next;
            out[0] = firstVertex;
            out[1] = innerIndex;
            out[2] = innerNext;
            out[3] = innerIndex;
            out[4] = outerIndex;
            out[5] = outerNext;
            out[6] = innerIndex;
            out[7] = outerNext;
            out[8] = innerNext;
        }
        emitted = pointCount * 9u;
    }
    else
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
        geometry_->vertices.resize(firstVertex);
        return;
    }
    emitGeometry(firstIndex, emitted, clip, solidTexture_, &bounds);
}

void DrawList::addArc(const Vec2 &center, float radius, float from, float to,
                      const Color &color, const Rect &clip, float thickness,
                      uint32_t segments)
{
    const Color shade = faded(color);
    if (radius <= 0.0f || thickness <= 0.0f || shade.a == 0u)
        return;
    // The whole circle's box: cheap, and never smaller than the arc's.
    const float reach = radius + thickness * 0.5f + 1.0f;
    const Rect bounds(center.x - reach, center.y - reach, reach * 2.0f, reach * 2.0f);
    if (rectOutsideClip(bounds, 0.0f, clip))
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
    Color coreColor = shade;
    if (thickness < 1.0f)
    {
        coreHalf = 0.5f;
        coreColor.a = static_cast<uint8_t>(static_cast<float>(shade.a) * thickness + 0.5f);
    }
    const Color transparentEdge(coreColor.r, coreColor.g, coreColor.b, 0u);

    const uint32_t firstVertex = static_cast<uint32_t>(geometry_->vertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(geometry_->indices.size());
    const Vec2 uv = solidUv_;
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
            geometry_->vertices.emplace_back(
                Vec2(center.x + cosf(angle) * ringRadius,
                     center.y + sinf(angle) * ringRadius),
                uv, ringColor);
        }
    }

    for (uint32_t strip = 0; strip < 3u; ++strip)
    {
        const uint32_t outerBase = firstVertex + strip * ringPoints;
        const uint32_t innerBase = outerBase + ringPoints;
        for (uint32_t i = 0; i + 1u < ringPoints; ++i)
        {
            geometry_->indices.push_back(outerBase + i);
            geometry_->indices.push_back(outerBase + i + 1u);
            geometry_->indices.push_back(innerBase + i + 1u);
            geometry_->indices.push_back(outerBase + i);
            geometry_->indices.push_back(innerBase + i + 1u);
            geometry_->indices.push_back(innerBase + i);
        }
    }
    emitGeometry(firstIndex, segments * 18u, clip, solidTexture_, &bounds);
}

void DrawList::addPolyline(Span<const Vec2> points, const Color &color, const Rect &clip,
                           float thickness)
{
    const Color shade = faded(color);
    if (points.size() < 2u || thickness <= 0.0f || shade.a == 0u)
        return;

    float minX = points[0].x, minY = points[0].y, maxX = points[0].x, maxY = points[0].y;
    for (Span<const Vec2>::size_type i = 1u; i < points.size(); ++i)
    {
        minX = points[i].x < minX ? points[i].x : minX;
        minY = points[i].y < minY ? points[i].y : minY;
        maxX = points[i].x > maxX ? points[i].x : maxX;
        maxY = points[i].y > maxY ? points[i].y : maxY;
    }
    const float half = thickness * 0.5f;
    const float fringe = 1.0f;
    const float reach = half + fringe;
    const Rect bounds(minX - reach, minY - reach, (maxX - minX) + reach * 2.0f,
                      (maxY - minY) + reach * 2.0f);
    if (rectOutsideClip(bounds, 0.0f, clip))
        return;

    // A band thinner than a pixel keeps a one-pixel core and loses opacity
    // instead, the same rule addLine uses for hairlines.
    float coreHalf = half;
    Color coreColor = shade;
    if (thickness < 1.0f)
    {
        coreHalf = 0.5f;
        coreColor.a = static_cast<uint8_t>(static_cast<float>(shade.a) * thickness + 0.5f);
    }
    const Color transparentEdge(coreColor.r, coreColor.g, coreColor.b, 0u);

    const uint32_t pointCount = static_cast<uint32_t>(points.size());
    const uint32_t firstVertex = static_cast<uint32_t>(geometry_->vertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(geometry_->indices.size());
    const Vec2 uv = solidUv_;

    // Per point: the outer fringe, the two edges of the core and the inner
    // fringe, in that order (point i owns vertices i*4 .. i*4+3). The normal of a
    // point is the sum of the normals of the segments it joins - a mitre - which
    // is what fills the corner without a joint for the alpha to double up in. A
    // zero-length segment keeps the previous direction; a point with no direction
    // at all (a series of identical samples) collapses into four coincident
    // vertices, which draw nothing and cost nothing on the GPU.
    Vec2 previousNormal(0.0f, 0.0f);
    for (uint32_t i = 0u; i < pointCount; ++i)
    {
        const Vec2 current = points[i];
        Vec2 normal = previousNormal;
        if (i + 1u < pointCount)
        {
            const Vec2 next = points[i + 1u];
            const float dx = next.x - current.x;
            const float dy = next.y - current.y;
            const float lengthSquared = dx * dx + dy * dy;
            if (lengthSquared > 0.0f)
            {
                const float inverse = 1.0f / sqrtf(lengthSquared);
                const Vec2 forward(-dy * inverse, dx * inverse);
                if (i == 0u)
                    normal = forward;
                else
                {
                    // A média das duas normais tem de ser normalizada: sem isso a
                    // fita ficava com o dobro da espessura em cada nó (o
                    // comprimento da soma é 2cos(θ/2)), e num canto fechado
                    // esticava até virar um pico. Abaixo de |soma| = 0.5 (mais de
                    // ~151 graus) fica-se com a direcção do segmento atual, que é
                    // o bisel do canto.
                    const Vec2 sum(previousNormal.x + forward.x, previousNormal.y + forward.y);
                    const float sumLengthSquared = sum.x * sum.x + sum.y * sum.y;
                    if (sumLengthSquared > 0.25f)
                    {
                        const float inverse = 1.0f / sqrtf(sumLengthSquared);
                        normal = Vec2(sum.x * inverse, sum.y * inverse);
                    }
                    else
                        normal = forward;
                }
            }
        }
        previousNormal = normal;

        const float outerX = normal.x * reach;
        const float outerY = normal.y * reach;
        const float innerX = normal.x * coreHalf;
        const float innerY = normal.y * coreHalf;
        geometry_->vertices.emplace_back(Vec2(current.x + outerX, current.y + outerY), uv,
                                         transparentEdge);
        geometry_->vertices.emplace_back(Vec2(current.x + innerX, current.y + innerY), uv,
                                         coreColor);
        geometry_->vertices.emplace_back(Vec2(current.x - innerX, current.y - innerY), uv,
                                         coreColor);
        geometry_->vertices.emplace_back(Vec2(current.x - outerX, current.y - outerY), uv,
                                         transparentEdge);
    }

    const uint32_t indexCount = (pointCount - 1u) * 18u;
    DrawIndex *out = allocIndices(indexCount);
    for (uint32_t i = 0u; i + 1u < pointCount; ++i)
    {
        const uint32_t outer = firstVertex + i * 4u;
        const uint32_t inner = outer + 4u;
        for (uint32_t strip = 0u; strip < 3u; ++strip)
        {
            out[0] = static_cast<DrawIndex>(outer + strip);
            out[1] = static_cast<DrawIndex>(inner + strip);
            out[2] = static_cast<DrawIndex>(inner + strip + 1u);
            out[3] = static_cast<DrawIndex>(outer + strip);
            out[4] = static_cast<DrawIndex>(inner + strip + 1u);
            out[5] = static_cast<DrawIndex>(outer + strip + 1u);
            out += 6;
        }
    }
    emitGeometry(firstIndex, indexCount, clip, solidTexture_, &bounds);
}

void DrawList::addRectFilledBatch(Span<const Rect> rects, const Color &color, const Rect &clip)
{
    const Color shade = faded(color);
    if (rects.size() == 0u || shade.a == 0u)
        return;
    if (!(clip.width > 0.0f) || !(clip.height > 0.0f))
        return;

    // Two passes: the first finds what is visible and the box of what will be
    // written, the second writes. One index reservation and one bounding box for
    // the whole run instead of one per rectangle.
    uint32_t visible = 0u;
    Rect bounds;
    for (Span<const Rect>::size_type i = 0u; i < rects.size(); ++i)
    {
        const Rect &rect = rects[i];
        if (rect.width <= 0.0f || rect.height <= 0.0f || rectOutsideClip(rect, 0.0f, clip))
            continue;
        if (visible == 0u)
        {
            bounds = rect;
        }
        else
        {
            const float left = bounds.x < rect.x ? bounds.x : rect.x;
            const float top = bounds.y < rect.y ? bounds.y : rect.y;
            float right = bounds.x + bounds.width;
            float bottom = bounds.y + bounds.height;
            if (rect.x + rect.width > right)
                right = rect.x + rect.width;
            if (rect.y + rect.height > bottom)
                bottom = rect.y + rect.height;
            bounds = Rect(left, top, right - left, bottom - top);
        }
        ++visible;
    }
    if (visible == 0u)
        return;

    const uint32_t firstVertex = static_cast<uint32_t>(geometry_->vertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(geometry_->indices.size());
    const Vec2 uv = solidUv_;
    DrawIndex *out = allocIndices(visible * 6u);

    uint32_t written = 0u;
    for (Span<const Rect>::size_type i = 0u; i < rects.size(); ++i)
    {
        const Rect &rect = rects[i];
        if (rect.width <= 0.0f || rect.height <= 0.0f || rectOutsideClip(rect, 0.0f, clip))
            continue;
        const uint32_t vertex = firstVertex + written * 4u;
        geometry_->vertices.emplace_back(Vec2(rect.x, rect.y), uv, shade);
        geometry_->vertices.emplace_back(Vec2(rect.x + rect.width, rect.y), uv, shade);
        geometry_->vertices.emplace_back(Vec2(rect.x + rect.width, rect.y + rect.height), uv, shade);
        geometry_->vertices.emplace_back(Vec2(rect.x, rect.y + rect.height), uv, shade);
        out[0] = static_cast<DrawIndex>(vertex);
        out[1] = static_cast<DrawIndex>(vertex + 1u);
        out[2] = static_cast<DrawIndex>(vertex + 2u);
        out[3] = static_cast<DrawIndex>(vertex);
        out[4] = static_cast<DrawIndex>(vertex + 2u);
        out[5] = static_cast<DrawIndex>(vertex + 3u);
        out += 6;
        ++written;
    }
    emitGeometry(firstIndex, written * 6u, clip, solidTexture_, &bounds);
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
    const Color shade = faded(color);
    if (points.size() < 3u || shade.a == 0u)
        return;
    float minX = points[0].x, minY = points[0].y, maxX = points[0].x, maxY = points[0].y;
    for (Span<const Vec2>::size_type i = 1; i < points.size(); ++i)
    {
        minX = points[i].x < minX ? points[i].x : minX;
        minY = points[i].y < minY ? points[i].y : minY;
        maxX = points[i].x > maxX ? points[i].x : maxX;
        maxY = points[i].y > maxY ? points[i].y : maxY;
    }
    const Rect bounds(minX - 1.0f, minY - 1.0f, maxX - minX + 2.0f, maxY - minY + 2.0f);
    if (rectOutsideClip(bounds, 0.0f, clip))
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

    const uint32_t firstVertex = static_cast<uint32_t>(geometry_->vertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(geometry_->indices.size());
    const Vec2 uv = solidUv_;
    for (Span<const Vec2>::size_type i = 0; i < points.size(); ++i)
        geometry_->vertices.emplace_back(points[i], uv, shade);
    for (ct::Vector<DrawIndex>::size_type i = 0; i < triangleIndices.size(); ++i)
        geometry_->indices.push_back(firstVertex + triangleIndices[i]);
    emitGeometry(firstIndex,
                       static_cast<uint32_t>(triangleIndices.size()), clip, solidTexture_, &bounds);
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
    const uint32_t offset = static_cast<uint32_t>(geometry_->textBytes.size());
    for (StringView::size_type i = 0; i < text.size(); ++i)
        geometry_->textBytes.push_back(text[i]);

    TextCommand command;
    command.clip = clip;
    command.position = position;
    command.font = font;
    command.logicalSize = logicalSize;
    command.color = faded(color);
    command.textOffset = offset;
    command.textSize = static_cast<uint32_t>(text.size());
    commands_.push_back(DrawCommand(command));
}

DrawData DrawList::data(const Vec2 &displaySize, float dpiScale) const
{
    DrawData result;
    result.displaySize = displaySize;
    result.dpiScale = dpiScale;
    result.vertices = Span<const DrawVertex>(geometry_->vertices);
    result.indices = Span<const DrawIndex>(geometry_->indices);
    result.textBytes = Span<const char>(geometry_->textBytes);
    result.commands = Span<const DrawCommand>(commands_);
    return result;
}

} // namespace ig
