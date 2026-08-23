#include <assert.h>

#include <igui/Gui.hpp>

class TestBackend : public ig::Backend
{
public:
    ig::DrawData lastData;
    bool rendered;

    TestBackend() : lastData(), rendered(false) {}

    ig::TextMetrics measureText(ig::FontId, ig::StringView text,
                                float logicalSize, float) override
    {
        ig::TextMetrics metrics;
        metrics.width = static_cast<float>(text.size()) * logicalSize * 0.5f;
        metrics.height = logicalSize;
        metrics.ascent = logicalSize * 0.75f;
        metrics.descent = logicalSize * 0.25f;
        return metrics;
    }

    bool render(const ig::DrawData &data) override
    {
        lastData = data;
        rendered = true;
        return true;
    }
};

static void test_draw_data_and_label()
{
    TestBackend backend;
    ig::Context context(backend);
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    context.label("hello", ig::Vec2(10.0f, 10.0f));
    context.endWindow();
    const ig::DrawData &data = context.endFrame();
    assert(data.displaySize.x == 640.0f);
    assert(data.commands.size() >= 3u);
    assert(data.textBytes.size() >= 9u);
    assert(backend.render(data));
    assert(backend.rendered);
}

static void test_button_click()
{
    TestBackend backend;
    ig::Context context(backend);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(context.button("ok", ig::Rect(8.0f, 8.0f, 100.0f, 28.0f)));
    context.endWindow();
    context.endFrame();
}

static void test_button_release_outside_does_not_click()
{
    TestBackend backend;
    ig::Context context(backend);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 500.0f, 400.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(!context.button("ok", ig::Rect(8.0f, 8.0f, 100.0f, 28.0f)));
    context.endWindow();
    context.endFrame();
}

static void test_checkbox()
{
    TestBackend backend;
    ig::Context context(backend);
    bool checked = false;
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 55.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 55.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(context.checkbox("check", checked, ig::Rect(8.0f, 8.0f, 18.0f, 18.0f)));
    assert(checked);
    context.endWindow();
    context.endFrame();
}

static void test_focus_lost_cancels_capture()
{
    TestBackend backend;
    ig::Context context(backend);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 60.0f));
    context.pushEvent(ig::Event::focusLost());
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(!context.button("ok", ig::Rect(8.0f, 8.0f, 100.0f, 28.0f)));
    context.endWindow();
    context.endFrame();
}

static void test_draw_primitives()
{
    ig::DrawList drawList;
    const ig::Rect clip(0.0f, 0.0f, 100.0f, 100.0f);
    const ig::Color color(255u, 255u, 255u, 255u);
    const ig::Vec2 polygon[] = {
        ig::Vec2(0.0f, 0.0f), ig::Vec2(20.0f, 0.0f), ig::Vec2(20.0f, 20.0f),
        ig::Vec2(10.0f, 10.0f), ig::Vec2(0.0f, 20.0f)
    };

    drawList.addLine(ig::Vec2(0.0f, 0.0f), ig::Vec2(10.0f, 0.0f), color, clip, 2.0f);
    drawList.addRect(ig::Rect(10.0f, 10.0f, 20.0f, 10.0f), color, clip);
    drawList.addCircleFilled(ig::Vec2(40.0f, 40.0f), 8.0f, color, clip, 8u);
    drawList.addPolygonFilled(ig::Span<const ig::Vec2>(polygon), color, clip);
    const ig::DrawData data = drawList.data(ig::Vec2(100.0f, 100.0f), 1.0f);

    assert(data.commands.size() == 7u);
    assert(data.vertices.size() == 34u);
    assert(data.indices.size() == 63u);
    assert(data.commands[5].payload.geometry.indexCount == 24u);
    assert(data.commands[6].payload.geometry.indexCount == 9u);
}

static void test_window_can_reopen()
{
    TestBackend backend;
    ig::Context context(backend);
    bool open = false;

    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(!context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f), &open));
    context.endFrame();

    open = true;
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f), &open));
    context.endWindow();
    context.endFrame();
}

static void test_content_clip_applies_to_checkbox_label()
{
    TestBackend backend;
    ig::Context context(backend);
    bool checked = false;
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    context.checkbox("check", checked, ig::Rect(8.0f, 8.0f, 18.0f, 18.0f));
    context.endWindow();
    const ig::DrawData &data = context.endFrame();

    const ig::DrawCommand &command = data.commands[data.commands.size() - 1u];
    assert(command.type == ig::DrawCommandType::Text);
    assert(command.payload.text.clip.x == 18.0f);
    assert(command.payload.text.clip.y == 42.0f);
    assert(command.payload.text.clip.width == 284.0f);
    assert(command.payload.text.clip.height == 140.0f);
}

static void test_clipped_button_cannot_be_clicked()
{
    TestBackend backend;
    ig::Context context(backend);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 65.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 65.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 100.0f, 50.0f)));
    assert(!context.button("hidden", ig::Rect(8.0f, 20.0f, 60.0f, 20.0f)));
    context.endWindow();
    context.endFrame();
}

static void test_clicked_window_is_composed_on_top()
{
    TestBackend backend;
    ig::Context context(backend);

    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("back", ig::Rect(0.0f, 0.0f, 100.0f, 100.0f)));
    context.endWindow();
    assert(context.beginWindow("front", ig::Rect(20.0f, 20.0f, 100.0f, 100.0f)));
    context.endWindow();
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 10.0f, 10.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("front", ig::Rect(20.0f, 20.0f, 100.0f, 100.0f)));
    context.endWindow();
    assert(context.beginWindow("back", ig::Rect(0.0f, 0.0f, 100.0f, 100.0f)));
    context.endWindow();
    const ig::DrawData &data = context.endFrame();

    assert(data.vertices[0].position.x == 20.0f);
    assert(data.vertices[0].position.y == 20.0f);
}

int main()
{
    test_draw_data_and_label();
    test_button_click();
    test_button_release_outside_does_not_click();
    test_checkbox();
    test_focus_lost_cancels_capture();
    test_draw_primitives();
    test_window_can_reopen();
    test_content_clip_applies_to_checkbox_label();
    test_clipped_button_cannot_be_clicked();
    test_clicked_window_is_composed_on_top();
    return 0;
}
