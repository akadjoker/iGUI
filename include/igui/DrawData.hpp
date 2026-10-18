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

    DrawVertex() {}
    // Lets a draw list build the vertex in place (emplace_back); a temporary
    // copied in costs several times as much per vertex.
    DrawVertex(const Vec2 &p, const Vec2 &texCoord, const Color &c)
        : position(p), uv(texCoord), color(c) {}
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
    DrawList();
    DrawList(const DrawList &other);
    DrawList &operator=(const DrawList &other);

    void clear();
    // Forgets the commands and keeps the geometry, for a list whose arrays
    // other lists share and still point into.
    void clearCommands();
    void append(const DrawList &other);

    // Geometry shared with another list: this list keeps commands of its own
    // but writes vertices, indices and text bytes into `store`'s arrays, so
    // gathering many lists into one frame copies commands, not geometry. The
    // store outlives this list, and only the store's clear() empties the arrays.
    void shareGeometry(DrawList &store);

    // Texture and texel that solid-colour primitives (rects, lines, circles,
    // rounded corners, arcs) put on their vertices. Left at the default the
    // geometry is untextured. Pointing it at a white texel of the font atlas
    // lets shapes and glyphs share one texture, which is what collapses a widget
    // tree into a handful of draw commands instead of one per text run.
    void setSolidStyle(TextureId texture, const Vec2 &whiteUv);
    TextureId solidTexture() const { return solidTexture_; }
    Vec2 solidUv() const { return solidUv_; }

    // Every primitive, text included, draws faded while this is on: colours are
    // blended toward `background` and their alpha scaled by `keep`. The Context
    // turns it on for the lists a beginDisabled()/endDisabled() block draws into,
    // so widgets do not have to know they are inside one. Off by default, and
    // clear() turns it off (the fade belongs to a block, not to a list).
    void setDisabledFade(bool enabled, const Color &background, float keep = 0.45f);
    bool disabledFade() const { return disabledKeep_ < 1.0f; }

    // A run of quads that share one clip rect and one texture, appended as a
    // single geometry command instead of one command per quad. This is what text
    // runs need - one quad per glyph, thousands per frame - so the vertices go
    // straight into the list: reserve the worst case once with reserve(), then
    // add() each quad without a per-quad capacity check. flush() writes the
    // command for what was added and can be called again if something else has
    // to be drawn in the middle of the run.
    class QuadBatch
    {
    public:
        QuadBatch(DrawList &drawList, TextureId texture, const Rect &clip)
            : drawList_(drawList), texture_(texture), clip_(clip), firstVertex_(0u),
              quadCount_(0u), open_(false)
        {
        }

        void reserve(uint32_t quadCount);
        // A quad is trimmed to the clip on the CPU (texture coordinates with
        // it); one outside the clip, with no area or with a transparent colour
        // is not emitted.
        void add(const Rect &rect, const Vec2 &uvMin, const Vec2 &uvMax, const Color &color);
        void flush();

    private:
        QuadBatch(const QuadBatch &);
        QuadBatch &operator=(const QuadBatch &);

        DrawList &drawList_;
        TextureId texture_;
        Rect clip_;
        uint32_t firstVertex_;
        uint32_t quadCount_;
        bool open_;
    };

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
    // An image on any four corners, in order around the quad (a turned or
    // sheared image); the clip is the scissor, the quad is not trimmed.
    void addImageQuad(TextureId texture, const Vec2 (&corners)[4], const Vec2 (&uvs)[4],
                      const Color &color, const Rect &clip);
    void addCircleFilled(const Vec2 &center, float radius,
                         const Color &color, const Rect &clip, uint32_t segments = 0u);
    // Tessellates a simple polygon. Points may be clockwise or counter-clockwise.
    void addPolygonFilled(Span<const Vec2> points, const Color &color, const Rect &clip);
    // Filled rectangle with rounded corners and an anti-aliased outline. The
    // radius is clamped to half the shorter side, so passing a large value on a
    // square gives a circle. A radius of zero falls through to addRectFilled.
    void addRectFilledRounded(const Rect &rect, float radius, const Color &color,
                              const Rect &clip, uint32_t cornerSegments = 0u);
    // Outline-only counterpart, for borders and focus rings.
    void addRectRounded(const Rect &rect, float radius, const Color &color,
                        const Rect &clip, float thickness = 1.0f,
                        uint32_t cornerSegments = 0u);
    // Circular arc as one continuous strip. Stroking an arc with addLine per
    // segment leaves every joint overlapping its neighbour's soft edge, and the
    // alpha adds up there - a knob ring drawn that way looks notched. Here the
    // fringe runs along the outside and inside of the whole band instead, so
    // there are no joints to show. Angles are radians, measured the usual way
    // (x to the right, y downward), and `to` may be less than `from`.
    void addArc(const Vec2 &center, float radius, float from, float to,
                const Color &color, const Rect &clip, float thickness = 1.0f,
                uint32_t segments = 0u);
    // A whole series as one band through `points`: what a plot line, a curve or
    // a graph needs. Add the same thing segment by segment with addLine and the
    // band is rebuilt per segment - the joints overlap (so a translucent line
    // darkens at every vertex), each segment pays the bounds and clip test
    // again, and a 90-sample series costs 89 calls instead of one. Here the
    // fringe runs along the outside and inside of the whole band, so there are
    // no joints, and the geometry is two vertices per point plus the two edges.
    // Points closer together than a pixel keep the direction of the segment
    // before them; the mitre of a sharp turn is not limited, which for a series
    // read from real data is what the caller wants (a spike should show).
    void addPolyline(Span<const Vec2> points, const Color &color, const Rect &clip,
                     float thickness = 1.0f);
    // A run of solid rectangles in one call: one reserve, one index write and
    // one clip test for the whole run, instead of that cost per rectangle. What
    // a histogram, a bar chart or a timeline draws. Rectangles with no area, and
    // the ones wholly outside the clip, are skipped; the rest share the clip.
    void addRectFilledBatch(Span<const Rect> rects, const Color &color, const Rect &clip);
    // Convenience wrapper for the common 3-point case (arrows, chevrons, ...).
    void addTriangleFilled(const Vec2 &a, const Vec2 &b, const Vec2 &c,
                           const Color &color, const Rect &clip)
    {
        const Vec2 points[3] = {a, b, c};
        addPolygonFilled(Span<const Vec2>(points, 3), color, clip);
    }
    void addText(StringView text, const Vec2 &position, FontId font,
                 float logicalSize, const Color &color, const Rect &clip);

    DrawData data(const Vec2 &displaySize, float dpiScale) const;

private:
    // Appends a geometry command, or extends the previous one when a backend
    // could not tell the difference: same texture, contiguous indices and either
    // the same clip or a tighter clip that the geometry stays inside of. bounds
    // is the box of what was drawn (antialiasing fringe included), which every
    // primitive knows already; without it the vertices are checked when needed.
    void emitGeometry(uint32_t firstIndex, uint32_t indexCount, const Rect &clip,
                      TextureId texture, const Rect *bounds);
    bool indexedVerticesInside(uint32_t firstIndex, uint32_t indexCount, const Rect &clip) const;
    // Room for `count` indices at the end of the list, written by the caller:
    // one resize instead of a push per index. writeQuadIndices() is the
    // two-triangle pattern every quad uses.
    DrawIndex *allocIndices(uint32_t count);
    void writeQuadIndices(uint32_t firstVertex);

    struct Geometry
    {
        ct::Vector<DrawVertex> vertices;
        ct::Vector<DrawIndex> indices;
        ct::Vector<char> textBytes;
    };
    Geometry own_;
    Geometry *geometry_; // &own_, or the store this list shares
    ct::Vector<DrawCommand> commands_;
    TextureId solidTexture_;
    Vec2 solidUv_;
    Color fadeBackground_;
    float disabledKeep_; // 1.0 is off

    // A cor como ela sai num bloco desativado (ver setDisabledFade). Sem fade
    // ligado devolve a própria cor, e é esse o caminho quente.
    Color faded(const Color &color) const
    {
        if (disabledKeep_ >= 1.0f)
            return color;
        const float keep = disabledKeep_;
        return Color(
            static_cast<uint8_t>(static_cast<float>(fadeBackground_.r) +
                                 (static_cast<float>(color.r) - static_cast<float>(fadeBackground_.r)) * keep + 0.5f),
            static_cast<uint8_t>(static_cast<float>(fadeBackground_.g) +
                                 (static_cast<float>(color.g) - static_cast<float>(fadeBackground_.g)) * keep + 0.5f),
            static_cast<uint8_t>(static_cast<float>(fadeBackground_.b) +
                                 (static_cast<float>(color.b) - static_cast<float>(fadeBackground_.b)) * keep + 0.5f),
            static_cast<uint8_t>(static_cast<float>(color.a) * keep + 0.5f));
    }
};

} // namespace ig
