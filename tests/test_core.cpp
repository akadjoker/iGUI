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

static void test_automatic_layout()
{
    TestBackend backend;
    ig::Context context(backend);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 20.0f, 50.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 20.0f, 50.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(context.button("one"));
    context.sameLine();
    assert(!context.button("two"));
    context.separator();
    context.label("three");
    context.endWindow();
    const ig::DrawData &data = context.endFrame();

    assert(data.vertices[8].position.x == 18.0f);
    assert(data.vertices[8].position.y == 42.0f);
    assert(data.vertices[12].position.x == 88.0f);
    assert(data.vertices[12].position.y == 42.0f);
    assert(data.vertices[16].position.y == 76.0f);
}

static void test_slider_and_radio()
{
    TestBackend backend;
    ig::Context context(backend);
    float value = 0.0f;
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 160.0f, 64.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 160.0f, 64.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(context.sliderFloat("volume", value, 0.0f, 1.0f, ig::Rect(8.0f, 8.0f, 160.0f, 28.0f)));
    context.endWindow();
    context.endFrame();
    assert(value > 0.7f && value < 0.8f);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 55.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 55.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(context.radioButton("choice", false, ig::Rect(8.0f, 8.0f, 18.0f, 18.0f)));
    context.endWindow();
    context.endFrame();
}

static void test_selectable()
{
    TestBackend backend;
    ig::Context context(backend);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(context.selectable("first", false, ig::Rect(8.0f, 8.0f, 120.0f, 28.0f)));
    context.endWindow();
    context.endFrame();

    context.pushEvent(ig::Event::pointerMove(30.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(!context.selectable("first", true, ig::Rect(8.0f, 8.0f, 120.0f, 28.0f)));
    context.endWindow();
    const ig::DrawData &data = context.endFrame();

    assert(data.vertices[8].color.r == 70u);
    assert(data.vertices[8].color.g == 125u);
    assert(data.vertices[8].color.b == 185u);
}

static void test_progress_bar()
{
    TestBackend backend;
    ig::Context context(backend);
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    context.progressBar(50.0f, 100.0f, ig::Rect(8.0f, 8.0f, 120.0f, 20.0f));
    context.progressBar(200.0f, 100.0f, ig::Rect(8.0f, 36.0f, 120.0f, 20.0f));
    context.progressBar(10.0f, 0.0f, ig::Rect(8.0f, 64.0f, 120.0f, 20.0f));
    context.endWindow();
    const ig::DrawData &data = context.endFrame();

    assert(data.vertices[12].position.x == 26.0f);
    assert(data.vertices[13].position.x == 86.0f);
    assert(data.vertices[12].color.r == 90u);
    assert(data.vertices[12].color.g == 160u);
    assert(data.vertices[12].color.b == 230u);
    assert(data.vertices[20].position.x == 26.0f);
    assert(data.vertices[21].position.x == 146.0f);
    assert(data.vertices.size() == 28u);
}

static void test_automatic_selectable_and_progress_bar()
{
    TestBackend backend;
    ig::Context context(backend);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(context.selectable("entry", false, 120.0f));
    context.progressBar(1.0f, 2.0f, 100.0f);
    context.endWindow();
    const ig::DrawData &data = context.endFrame();

    assert(data.vertices[8].position.x == 18.0f);
    assert(data.vertices[8].position.y == 42.0f);
    assert(data.vertices[12].position.x == 18.0f);
    assert(data.vertices[12].position.y == 76.0f);
    assert(data.vertices[17].position.x == 68.0f);
}

static void test_slider_captures_pointer()
{
    TestBackend backend;
    ig::Context context(backend);
    bool checked = false;
    float value = 0.0f;

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 160.0f, 106.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    context.checkbox("check", checked, ig::Rect(8.0f, 8.0f, 18.0f, 18.0f));
    assert(context.sliderFloat("volume", value, 0.0f, 1.0f, ig::Rect(8.0f, 50.0f, 160.0f, 28.0f)));
    context.endWindow();
    context.endFrame();

    context.pushEvent(ig::Event::pointerMove(30.0f, 55.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(!context.checkbox("check", checked, ig::Rect(8.0f, 8.0f, 18.0f, 18.0f)));
    context.sliderFloat("volume", value, 0.0f, 1.0f, ig::Rect(8.0f, 50.0f, 160.0f, 28.0f));
    context.endWindow();
    const ig::DrawData &data = context.endFrame();

    assert(!checked);
    assert(data.vertices[8].color.r == 55u);
    assert(data.vertices[8].color.g == 55u);
    assert(data.vertices[8].color.b == 65u);

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 55.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    context.checkbox("check", checked, ig::Rect(8.0f, 8.0f, 18.0f, 18.0f));
    context.sliderFloat("volume", value, 0.0f, 1.0f, ig::Rect(8.0f, 50.0f, 160.0f, 28.0f));
    context.endWindow();
    context.endFrame();
}

static void test_input_text_focus_and_utf8_backspace()
{
    TestBackend backend;
    ig::Context context(backend);
    ig::String value;
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 60.0f));
    context.pushEvent(ig::Event::textInput(u8"Olá"));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(context.inputText("name", value, ig::Rect(8.0f, 8.0f, 160.0f, 28.0f)));
    assert(context.wantsKeyboard());
    assert(context.wantsTextInput());
    context.endWindow();
    const ig::DrawData &firstData = context.endFrame();
    assert(value.size() == 4u);
    bool foundInputText = false;
    for (ig::Span<const ig::DrawCommand>::size_type i = 0; i < firstData.commands.size(); ++i)
    {
        const ig::DrawCommand &command = firstData.commands[i];
        if (command.type == ig::DrawCommandType::Text && command.payload.text.textSize == value.size() &&
            command.payload.text.position.x == 30.0f)
        {
            assert(command.payload.text.clip.x == 26.0f);
            assert(command.payload.text.clip.y == 50.0f);
            assert(command.payload.text.clip.width == 160.0f);
            assert(command.payload.text.clip.height == 28.0f);
            foundInputText = true;
        }
    }
    assert(foundInputText);

    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Backspace));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(context.inputText("name", value, ig::Rect(8.0f, 8.0f, 160.0f, 28.0f)));
    context.endWindow();
    context.endFrame();
    assert(value.size() == 2u);
    assert(value[0] == 'O');
    assert(value[1] == 'l');
}

static void test_input_text_scrolls_to_caret()
{
    TestBackend backend;
    ig::Context context(backend);
    ig::String value("this text is longer than the editor width");
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("main", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    context.inputText("name", value, ig::Rect(8.0f, 8.0f, 80.0f, 28.0f));
    context.endWindow();
    const ig::DrawData &data = context.endFrame();

    bool foundInputText = false;
    for (ig::Span<const ig::DrawCommand>::size_type i = 0; i < data.commands.size(); ++i)
    {
        const ig::DrawCommand &command = data.commands[i];
        if (command.type == ig::DrawCommandType::Text && command.payload.text.textSize == value.size())
        {
            assert(command.payload.text.position.x < 30.0f);
            assert(command.payload.text.clip.x == 26.0f);
            assert(command.payload.text.clip.width == 80.0f);
            foundInputText = true;
        }
    }
    assert(foundInputText);
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
    test_automatic_layout();
    test_slider_and_radio();
    test_selectable();
    test_progress_bar();
    test_automatic_selectable_and_progress_bar();
    test_slider_captures_pointer();
    test_input_text_focus_and_utf8_backspace();
    test_input_text_scrolls_to_caret();
    test_window_can_reopen();
    test_content_clip_applies_to_checkbox_label();
    test_clipped_button_cannot_be_clicked();
    test_clicked_window_is_composed_on_top();
    return 0;
}
