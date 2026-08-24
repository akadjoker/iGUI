#include <assert.h>

#include <igui/Gui.hpp>

class WidgetBackend : public ig::Backend
{
public:
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

    bool render(const ig::DrawData &) override
    {
        return true;
    }
};

struct GalleryState
{
    bool checked;
    bool switched;
    bool firstChoice;
    bool selected;
    float volume;
    int steps;
    int retries;
    ig::String name;
    int quality;
    bool buttonClicked;
    bool checkboxClicked;
    bool switchClicked;
    bool radioClicked;
    bool selectableClicked;
    bool sliderChanged;
    bool integerSliderChanged;
    bool stepperChanged;
    bool inputChanged;
    bool comboChanged;

    GalleryState()
        : checked(false), switched(false), firstChoice(false), selected(false), volume(0.0f), steps(1), retries(2), name(), quality(0),
          buttonClicked(false), checkboxClicked(false), switchClicked(false), radioClicked(false),
          selectableClicked(false), sliderChanged(false), integerSliderChanged(false), stepperChanged(false),
          inputChanged(false), comboChanged(false)
    {
    }
};

static void drawGallery(ig::Context &context, GalleryState &state)
{
    const ig::StringView qualityItems[] = {
        ig::StringView("low"), ig::StringView("medium"), ig::StringView("high")
    };
    assert(context.beginWindow("widget gallery", ig::Rect(10.0f, 10.0f, 300.0f, 560.0f)));
    state.buttonClicked = context.button("action");
    state.checkboxClicked = context.checkbox("enabled", state.checked);
    state.switchClicked = context.toggleSwitch("live update", state.switched);
    state.radioClicked = context.radioButton("choice", state.firstChoice);
    if (state.radioClicked)
        state.firstChoice = true;
    state.selectableClicked = context.selectable("entry", state.selected, 200.0f);
    if (state.selectableClicked)
        state.selected = !state.selected;
    state.sliderChanged = context.sliderFloat("volume", state.volume, 0.0f, 1.0f, 200.0f);
    state.integerSliderChanged = context.sliderInt("steps", state.steps, 0, 10, 200.0f);
    state.stepperChanged = context.stepperInt("retries", state.retries, 0, 5, 200.0f);
    state.inputChanged = context.inputText("name", state.name, 200.0f);
    context.progressBar(state.volume, 1.0f, 200.0f);
    context.label("all widgets rendered");
    state.comboChanged = context.comboBox("quality", state.quality,
                                          ig::Span<const ig::StringView>(qualityItems), 200.0f);
    context.endWindow();
}

static void beginAndDraw(ig::Context &context, GalleryState &state)
{
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    drawGallery(context, state);
}

static void test_widget_gallery()
{
    WidgetBackend backend;
    ig::Context context(backend);
    GalleryState state;

    beginAndDraw(context, state);
    const ig::DrawData &initialData = context.endFrame();
    assert(initialData.commands.size() >= 17u);
    assert(initialData.vertices.size() >= 44u);
    assert(!state.buttonClicked);
    assert(!state.checked);
    assert(!state.switched);
    assert(!state.firstChoice);
    assert(!state.selected);
    assert(state.quality == 0);
    assert(state.steps == 1);
    assert(state.retries == 2);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 25.0f, 55.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 25.0f, 55.0f));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.buttonClicked);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 25.0f, 83.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 25.0f, 83.0f));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.checkboxClicked);
    assert(state.checked);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 25.0f, 115.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 25.0f, 115.0f));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.switchClicked);
    assert(state.switched);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 25.0f, 147.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 25.0f, 147.0f));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.radioClicked);
    assert(state.firstChoice);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 25.0f, 182.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 25.0f, 182.0f));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.selectableClicked);
    assert(state.selected);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 143.0f, 224.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 143.0f, 224.0f));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.sliderChanged);
    assert(state.volume > 0.45f && state.volume < 0.55f);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 143.0f, 258.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 143.0f, 258.0f));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.integerSliderChanged);
    assert(state.steps > 3 && state.steps < 7);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 205.0f, 294.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 205.0f, 294.0f));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.stepperChanged);
    assert(state.retries == 3);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 25.0f, 323.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 25.0f, 323.0f));
    context.pushEvent(ig::Event::textInput(u8"Olá"));
    beginAndDraw(context, state);
    const ig::DrawData &inputData = context.endFrame();
    assert(state.inputChanged);
    assert(context.wantsKeyboard());
    assert(context.wantsTextInput());
    assert(state.name.size() == 4u);
    // Consecutive compatible geometry is batched, so command count is lower
    // than the number of individual widget primitives.
    assert(inputData.commands.size() >= 17u);
    assert(inputData.vertices.size() >= 44u);

    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Home));
    context.pushEvent(ig::Event::textInput("X"));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.inputChanged);
    assert(state.name == ig::String("XOl\303\241"));

    context.pushEvent(ig::Event::keyDown(ig::KeyCode::End));
    context.pushEvent(ig::Event::textInput("!"));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.inputChanged);
    assert(state.name == ig::String("XOl\303\241!"));

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 25.0f, 412.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 25.0f, 412.0f));
    beginAndDraw(context, state);
    context.endFrame();
    assert(!state.comboChanged);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 25.0f, 500.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 25.0f, 500.0f));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.comboChanged);
    assert(state.quality == 2);
}

static void test_combo_popup_overlay()
{
    WidgetBackend backend;
    ig::Context context(backend);
    const ig::StringView items[] = {
        ig::StringView("low"), ig::StringView("medium"), ig::StringView("high")
    };
    int selected = 0;

    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    assert(context.beginWindow("popup ordering", ig::Rect(10.0f, 10.0f, 220.0f, 200.0f)));
    context.comboBox("quality", selected, ig::Span<const ig::StringView>(items), 180.0f);
    context.button("drawn after combo");
    context.endWindow();
    context.endFrame();

    // Open the combo box. The button is declared after it, so the popup must
    // still become the final layer in the generated draw data.
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 25.0f, 55.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 25.0f, 55.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    assert(context.beginWindow("popup ordering", ig::Rect(10.0f, 10.0f, 220.0f, 200.0f)));
    context.comboBox("quality", selected, ig::Span<const ig::StringView>(items), 180.0f);
    context.button("drawn after combo");
    context.endWindow();
    const ig::DrawData &data = context.endFrame();

    assert(!data.commands.empty());
    const ig::DrawCommand &lastCommand = data.commands.back();
    assert(lastCommand.type == ig::DrawCommandType::Text);
    assert(lastCommand.payload.text.position.y > 120.0f);
}

int main()
{
    test_widget_gallery();
    test_combo_popup_overlay();
    return 0;
}
