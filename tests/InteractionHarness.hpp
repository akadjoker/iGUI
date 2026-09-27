#pragma once
// Scripted-interaction harness: drives an ig::Context with synthetic pointer
// events and inspects the resulting DrawData. No window, no backend, no human
// looking at a screen - a click bug (wrong widget reacts, drag steals input
// from something on top) shows up as an assertion, not a visual glitch.

#include <igui/Gui.hpp>

#include <assert.h>

namespace ig
{

// Headless backend: fixed-width glyphs, no real rendering.
class TestBackend : public Backend
{
public:
    ct::Vector<DrawData> renderedFrames;
    bool captureFrames;

    TestBackend() : renderedFrames(), captureFrames(false) {}

    TextMetrics measureText(FontId, StringView text, float logicalSize,
                            float) override
    {
        TextMetrics metrics;
        metrics.width = static_cast<float>(text.size()) * logicalSize * 0.5f;
        metrics.height = logicalSize;
        metrics.ascent = logicalSize * 0.75f;
        metrics.descent = logicalSize * 0.25f;
        return metrics;
    }

    bool render(const DrawData &data) override
    {
        if (captureFrames)
            renderedFrames.push_back(data);
        return true;
    }
};

class Harness
{
public:
    explicit Harness(Context &context)
        : context_(context), width_(1280.0f), height_(800.0f), lastData_(nullptr)
    {
    }

    void setViewport(float width, float height)
    {
        width_ = width;
        height_ = height;
    }

    void queue(const Event &event) { context_.pushEvent(event); }

    // Runs one frame with whatever events were queued.
    template <typename Build>
    const DrawData &frame(Build build)
    {
        context_.beginFrame(FrameInfo(width_, height_));
        build(context_);
        lastData_ = &context_.endFrame();
        return *lastData_;
    }

    // Press then release at (x, y), one frame apart.
    template <typename Build>
    const DrawData &click(float x, float y, Build build,
                          PointerButton button = PointerButton::Left)
    {
        context_.pushEvent(Event::pointerDown(button, x, y));
        frame(build);
        context_.pushEvent(Event::pointerUp(button, x, y));
        return frame(build);
    }

    // Press, `steps` move frames, release; build() runs once per frame.
    template <typename Build>
    void drag(float fromX, float fromY, float toX, float toY, Build build,
             int steps = 3, PointerButton button = PointerButton::Left)
    {
        assert(steps >= 2);
        context_.pushEvent(Event::pointerDown(button, fromX, fromY));
        frame(build);
        for (int i = 1; i < steps - 1; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps - 1);
            const float x = fromX + (toX - fromX) * t;
            const float y = fromY + (toY - fromY) * t;
            context_.pushEvent(Event::pointerMove(x, y));
            frame(build);
        }
        context_.pushEvent(Event::pointerMove(toX, toY));
        frame(build);
        context_.pushEvent(Event::pointerUp(button, toX, toY));
        frame(build);
    }

    // Queries below read the DrawData from the last frame()/click()/drag().

    // Locate by drawn position, not vertex index - see hasVertexAt usage in
    // test_core.cpp for why (geometry earlier in the frame shifts indices).
    bool hasVertexAt(float x, float y) const
    {
        assert(lastData_ && "call frame()/click()/drag() before asserting");
        for (size_t i = 0; i < lastData_->vertices.size(); ++i)
        {
            const Vec2 &p = lastData_->vertices[i].position;
            if (p.x == x && p.y == y)
                return true;
        }
        return false;
    }

    bool hasVertexNear(float x, float y, float radius) const
    {
        assert(lastData_ && "call frame()/click()/drag() before asserting");
        const float radiusSquared = radius * radius;
        for (size_t i = 0; i < lastData_->vertices.size(); ++i)
        {
            const Vec2 &p = lastData_->vertices[i].position;
            const float dx = p.x - x;
            const float dy = p.y - y;
            if (dx * dx + dy * dy <= radiusSquared)
                return true;
        }
        return false;
    }

    // Transparent black if nothing is there.
    Color colorAtVertex(float x, float y) const
    {
        assert(lastData_ && "call frame()/click()/drag() before asserting");
        for (size_t i = 0; i < lastData_->vertices.size(); ++i)
        {
            if (lastData_->vertices[i].position.x == x &&
                lastData_->vertices[i].position.y == y)
                return lastData_->vertices[i].color;
        }
        return Color(0u, 0u, 0u, 0u);
    }

    size_t vertexCount() const
    {
        assert(lastData_);
        return lastData_->vertices.size();
    }

    size_t commandCount() const
    {
        assert(lastData_);
        return lastData_->commands.size();
    }

private:
    Context &context_;
    float width_;
    float height_;
    const DrawData *lastData_;
};

} // namespace ig
