#include <assert.h>
#include <stdio.h>

#include <igui/Gui.hpp>

class WidgetBackend : public ig::Backend
{
public:
    ig::String clipboard;

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

    ig::String clipboardText() override { return clipboard; }
    bool setClipboardText(ig::StringView text) override
    {
        clipboard = ig::String(text.data(), text.size());
        return true;
    }
};

struct GalleryState
{
    bool checked;
    bool switched;
    bool firstChoice;
    bool selected;
    bool headerExpanded;
    float volume;
    int steps;
    int retries;
    int inputCount;
    float inputScale;
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
    bool integerInputChanged;
    bool floatInputChanged;
    bool comboChanged;

    GalleryState()
        : checked(false), switched(false), firstChoice(false), selected(false), headerExpanded(false), volume(0.0f), steps(1), retries(2), inputCount(7), inputScale(1.5f), name(), quality(0),
          buttonClicked(false), checkboxClicked(false), switchClicked(false), radioClicked(false),
          selectableClicked(false), sliderChanged(false), integerSliderChanged(false), stepperChanged(false),
          inputChanged(false), integerInputChanged(false), floatInputChanged(false), comboChanged(false)
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
    state.headerExpanded = context.collapsingHeader("advanced", state.headerExpanded, 200.0f);
    state.integerInputChanged = context.inputInt("input count", state.inputCount, 200.0f);
    state.floatInputChanged = context.inputFloat("input scale", state.inputScale, 200.0f, 3);
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
    assert(!state.headerExpanded);
    assert(state.quality == 0);
    assert(state.steps == 1);
    assert(state.retries == 2);
    assert(state.inputCount == 7);
    assert(state.inputScale == 1.5f);

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

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 115.0f, 224.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 115.0f, 224.0f));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.sliderChanged);
    assert(state.volume > 0.45f && state.volume < 0.55f);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 115.0f, 258.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 115.0f, 258.0f));
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

    context.pushEvent(ig::Event::keyDown(ig::KeyCode::C, true));
    beginAndDraw(context, state);
    context.endFrame();
    assert(backend.clipboard == state.name);

    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Home));
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::V, true));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.inputChanged);
    assert(state.name == ig::String("XOl\303\241!XOl\303\241!"));

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

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 25.0f, 450.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 25.0f, 450.0f));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.headerExpanded);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 25.0f, 480.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 25.0f, 480.0f));
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Home));
    context.pushEvent(ig::Event::textInput("3"));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.integerInputChanged);
    assert(state.inputCount == 37);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 25.0f, 512.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 25.0f, 512.0f));
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Home));
    context.pushEvent(ig::Event::textInput("2"));
    beginAndDraw(context, state);
    context.endFrame();
    assert(state.floatInputChanged);
    assert(state.inputScale > 21.4f && state.inputScale < 21.6f);
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

static void drawMenus(ig::Context &context, int &menuActions, int &contextActions, bool &inspectorVisible)
{
    assert(context.beginWindow("menus", ig::Rect(10.0f, 10.0f, 240.0f, 190.0f)));
    assert(context.beginMenuBar(ig::Rect(0.0f, 0.0f, 220.0f, 26.0f)));
    if (context.beginMenu("File"))
    {
        if (context.beginSubMenu("Panels"))
        {
            context.menuCheckbox("Inspector", inspectorVisible);
            context.endSubMenu();
        }
        if (context.menuItem("New scene"))
            ++menuActions;
        context.menuSeparator();
        context.menuItem("Save", false);
        context.endMenu();
    }
    context.endMenuBar();

    if (context.beginContextMenu("scene context", ig::Rect(0.0f, 40.0f, 220.0f, 100.0f)))
    {
        if (context.menuItem("Duplicate"))
            ++contextActions;
        context.menuItem("Delete", false);
        context.endContextMenu();
    }
    context.endWindow();
}

static void test_menu_and_context_menu()
{
    WidgetBackend backend;
    ig::Context context(backend);
    int menuActions = 0;
    int contextActions = 0;
    bool inspectorVisible = true;

    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    drawMenus(context, menuActions, contextActions, inspectorVisible);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 55.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 55.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    drawMenus(context, menuActions, contextActions, inspectorVisible);
    const ig::DrawData &menuData = context.endFrame();
    assert(!menuData.commands.empty());

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 35.0f, 105.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 35.0f, 105.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    drawMenus(context, menuActions, contextActions, inspectorVisible);
    context.endFrame();
    assert(menuActions == 1);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 55.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 55.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    drawMenus(context, menuActions, contextActions, inspectorVisible);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 35.0f, 80.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 35.0f, 80.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    drawMenus(context, menuActions, contextActions, inspectorVisible);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 205.0f, 80.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 205.0f, 80.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    drawMenus(context, menuActions, contextActions, inspectorVisible);
    context.endFrame();
    assert(!inspectorVisible);

    // A checkable submenu item closes the menu. It must be possible to open
    // the same submenu again on the following interaction.
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 55.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 55.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    drawMenus(context, menuActions, contextActions, inspectorVisible);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 35.0f, 80.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 35.0f, 80.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    drawMenus(context, menuActions, contextActions, inspectorVisible);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 205.0f, 80.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 205.0f, 80.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    drawMenus(context, menuActions, contextActions, inspectorVisible);
    context.endFrame();
    assert(inspectorVisible);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Right, 100.0f, 110.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Right, 100.0f, 110.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    drawMenus(context, menuActions, contextActions, inspectorVisible);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 120.0f, 120.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 120.0f, 120.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    drawMenus(context, menuActions, contextActions, inspectorVisible);
    context.endFrame();
    assert(contextActions == 1);
}

static void test_image_widgets()
{
    WidgetBackend backend;
    ig::Context context(backend);
    const ig::TextureId texture(77u);

    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    assert(context.beginWindow("image widgets", ig::Rect(10.0f, 10.0f, 220.0f, 190.0f)));
    context.image(texture, ig::Rect(8.0f, 8.0f, 48.0f, 48.0f));
    assert(!context.imageButton("image action", texture, ig::Rect(8.0f, 64.0f, 48.0f, 48.0f)));
    context.endWindow();
    const ig::DrawData &initialData = context.endFrame();

    uint32_t texturedCommands = 0u;
    for (ig::Span<const ig::DrawCommand>::size_type i = 0u; i < initialData.commands.size(); ++i)
    {
        const ig::DrawCommand &command = initialData.commands[i];
        if (command.type == ig::DrawCommandType::Geometry &&
            command.payload.geometry.texture == texture)
            ++texturedCommands;
    }
    assert(texturedCommands == 2u);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 40.0f, 120.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 40.0f, 120.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    assert(context.beginWindow("image widgets", ig::Rect(10.0f, 10.0f, 220.0f, 190.0f)));
    context.image(texture, ig::Rect(8.0f, 8.0f, 48.0f, 48.0f));
    assert(context.imageButton("image action", texture, ig::Rect(8.0f, 64.0f, 48.0f, 48.0f)));
    context.endWindow();
    context.endFrame();
}

static void test_small_buttons()
{
    WidgetBackend backend;
    ig::Context context(backend);
    const ig::TextureId texture(99u);

    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    assert(context.beginWindow("small buttons", ig::Rect(10.0f, 10.0f, 220.0f, 120.0f)));
    assert(!context.smallButton("Reload", ig::Rect(8.0f, 8.0f, 60.0f, 20.0f)));
    assert(!context.smallImageButton("icon reload", texture, ig::Rect(76.0f, 8.0f, 20.0f, 20.0f)));
    context.endWindow();
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 42.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 42.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    assert(context.beginWindow("small buttons", ig::Rect(10.0f, 10.0f, 220.0f, 120.0f)));
    assert(context.smallButton("Reload", ig::Rect(8.0f, 8.0f, 60.0f, 20.0f)));
    assert(!context.smallImageButton("icon reload", texture, ig::Rect(76.0f, 8.0f, 20.0f, 20.0f)));
    context.endWindow();
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 104.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 104.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    assert(context.beginWindow("small buttons", ig::Rect(10.0f, 10.0f, 220.0f, 120.0f)));
    assert(!context.smallButton("Reload", ig::Rect(8.0f, 8.0f, 60.0f, 20.0f)));
    assert(context.smallImageButton("icon reload", texture, ig::Rect(76.0f, 8.0f, 20.0f, 20.0f)));
    context.endWindow();
    context.endFrame();
}

struct NavigationState
{
    int tab;
    int selectedItem;
    bool sceneExpanded;

    NavigationState() : tab(0), selectedItem(0), sceneExpanded(false) {}
};

static void drawNavigationWidgets(ig::Context &context, NavigationState &state)
{
    const ig::StringView tabs[] = {
        ig::StringView("scene"), ig::StringView("assets"), ig::StringView("settings")
    };
    const ig::StringView items[] = {
        ig::StringView("camera"), ig::StringView("light"), ig::StringView("mesh"),
        ig::StringView("material"), ig::StringView("environment"), ig::StringView("post process")
    };

    assert(context.beginWindow("navigation", ig::Rect(10.0f, 10.0f, 240.0f, 240.0f)));
    context.tabBar("navigator tabs", state.tab, ig::Span<const ig::StringView>(tabs),
                   ig::Rect(8.0f, 8.0f, 180.0f, 28.0f));
    context.treeNode("Scene", state.sceneExpanded, ig::Rect(8.0f, 44.0f, 180.0f, 28.0f));
    context.tooltip("Scene hierarchy");
    context.listBox("scene items", state.selectedItem, ig::Span<const ig::StringView>(items),
                    ig::Rect(8.0f, 80.0f, 180.0f, 84.0f));
    context.endWindow();
}

static void test_navigation_widgets()
{
    WidgetBackend backend;
    ig::Context context(backend);
    NavigationState state;

    context.beginFrame(ig::FrameInfo(320.0f, 280.0f));
    drawNavigationWidgets(context, state);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 110.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 110.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 280.0f));
    drawNavigationWidgets(context, state);
    context.endFrame();
    assert(state.tab == 1);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 40.0f, 100.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 40.0f, 100.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 280.0f));
    drawNavigationWidgets(context, state);
    context.endFrame();
    assert(state.sceneExpanded);

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    context.pushEvent(ig::Event::pointerMove(40.0f, 140.0f));
    context.pushEvent(wheel);
    context.beginFrame(ig::FrameInfo(320.0f, 280.0f));
    drawNavigationWidgets(context, state);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 40.0f, 190.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 40.0f, 190.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 280.0f));
    drawNavigationWidgets(context, state);
    context.endFrame();
    assert(state.selectedItem == 3);

    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Down));
    context.beginFrame(ig::FrameInfo(320.0f, 280.0f));
    drawNavigationWidgets(context, state);
    context.endFrame();
    assert(state.selectedItem == 4);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 195.0f, 165.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 280.0f));
    drawNavigationWidgets(context, state);
    context.endFrame();

    context.pushEvent(ig::Event::pointerMove(195.0f, 185.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 280.0f));
    drawNavigationWidgets(context, state);
    context.endFrame();

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 195.0f, 185.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 280.0f));
    drawNavigationWidgets(context, state);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 40.0f, 130.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 40.0f, 130.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 280.0f));
    drawNavigationWidgets(context, state);
    context.endFrame();
    // The thumb drag moves the visible range; clicking its first row must no
    // longer select the item that was focused before the drag.
    assert(state.selectedItem < 4);

    context.pushEvent(ig::Event::pointerMove(40.0f, 100.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 280.0f));
    drawNavigationWidgets(context, state);
    context.endFrame();

    context.beginFrame(ig::FrameInfo(320.0f, 280.0f, 1.0f, 0.5f));
    drawNavigationWidgets(context, state);
    const ig::DrawData &data = context.endFrame();
    assert(!data.commands.empty());
    const ig::DrawCommand &tooltip = data.commands.back();
    assert(tooltip.type == ig::DrawCommandType::Text);
    assert(tooltip.payload.text.textSize == 15u);
}

static void test_history_and_keyboard_navigation()
{
    WidgetBackend backend;
    ig::Context context(backend);
    int value = 8;
    context.pushUndo("set value", [&value]() { value = 3; }, [&value]() { value = 8; });
    assert(context.canUndo());
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Z, true));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    context.endFrame();
    assert(value == 3);
    assert(context.canRedo());
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Y, true));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    context.endFrame();
    assert(value == 8);

    int tab = 0;
    int combo = 0;
    const ig::StringView tabs[] = {ig::StringView("one"), ig::StringView("two"), ig::StringView("three")};
    const ig::StringView choices[] = {ig::StringView("red"), ig::StringView("green"), ig::StringView("blue")};
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 35.0f, 62.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 35.0f, 62.0f));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    assert(context.beginWindow("keyboard selectors", ig::Rect(10.0f, 10.0f, 250.0f, 180.0f)));
    context.tabBar("tabs", tab, ig::Span<const ig::StringView>(tabs), ig::Rect(8.0f, 8.0f, 180.0f, 28.0f));
    context.comboBox("combo", combo, ig::Span<const ig::StringView>(choices), ig::Rect(8.0f, 46.0f, 180.0f, 28.0f));
    context.endWindow();
    context.endFrame();
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Right));
    context.beginFrame(ig::FrameInfo(320.0f, 240.0f));
    assert(context.beginWindow("keyboard selectors", ig::Rect(10.0f, 10.0f, 250.0f, 180.0f)));
    assert(context.tabBar("tabs", tab, ig::Span<const ig::StringView>(tabs), ig::Rect(8.0f, 8.0f, 180.0f, 28.0f)));
    context.comboBox("combo", combo, ig::Span<const ig::StringView>(choices), ig::Rect(8.0f, 46.0f, 180.0f, 28.0f));
    context.endWindow();
    context.endFrame();
    assert(tab == 1);
}

static void drawDockSpace(ig::Context &context, bool &hierarchyVisible, bool &assetsVisible,
                          bool &inspectorVisible, bool &consoleVisible)
{
    hierarchyVisible = false;
    assetsVisible = false;
    inspectorVisible = false;
    consoleVisible = false;
    assert(context.beginWindow("dock host", ig::Rect(10.0f, 10.0f, 520.0f, 400.0f)));
    assert(context.beginDockSpace("editor", ig::Rect(8.0f, 8.0f, 480.0f, 330.0f)));
    if (context.beginDockPanel("Hierarchy", ig::DockSlot::Left))
    {
        hierarchyVisible = true;
        context.label("scene");
        context.endDockPanel();
    }
    if (context.beginDockPanel("Assets", ig::DockSlot::Left))
    {
        assetsVisible = true;
        context.label("assets");
        context.endDockPanel();
    }
    if (context.beginDockPanel("Inspector", ig::DockSlot::Right))
    {
        inspectorVisible = true;
        context.label("properties");
        context.endDockPanel();
    }
    if (context.beginDockPanel("Console", ig::DockSlot::Bottom))
    {
        consoleVisible = true;
        context.label("output");
        context.endDockPanel();
    }
    context.endDockSpace();
    context.endWindow();
}

static void test_dock_space()
{
    WidgetBackend backend;
    ig::Context context(backend);
    bool hierarchy = false;
    bool assets = false;
    bool inspector = false;
    bool console = false;
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    drawDockSpace(context, hierarchy, assets, inspector, console);
    context.endFrame();
    assert(hierarchy && !assets && inspector && console);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 192.0f, 64.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 192.0f, 64.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    drawDockSpace(context, hierarchy, assets, inspector, console);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 70.0f, 120.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 70.0f, 120.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    drawDockSpace(context, hierarchy, assets, inspector, console);
    context.endFrame();
    assert(!hierarchy && assets && inspector && console);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 140.0f, 70.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    drawDockSpace(context, hierarchy, assets, inspector, console);
    context.endFrame();
    context.pushEvent(ig::Event::pointerMove(290.0f, 100.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    drawDockSpace(context, hierarchy, assets, inspector, console);
    context.endFrame();
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 290.0f, 100.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    drawDockSpace(context, hierarchy, assets, inspector, console);
    context.endFrame();
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    drawDockSpace(context, hierarchy, assets, inspector, console);
    context.endFrame();
    assert(hierarchy && assets && inspector && console);
}

static void test_full_client_dock_space_and_theme_presets()
{
    const ig::Theme dark = ig::makeTheme(ig::ThemePreset::Dark);
    const ig::Theme light = ig::makeTheme(ig::ThemePreset::Light);
    const ig::Theme blender = ig::makeTheme(ig::ThemePreset::Blender);
    const ig::Theme vscode = ig::makeTheme(ig::ThemePreset::VSCode);
    assert(dark.windowBackground != light.windowBackground);
    assert(blender.focusColor != vscode.focusColor);

    WidgetBackend backend;
    ig::Context context(backend);
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginMainWindow("full client"));
    assert(context.beginDockSpace("root"));
    if (context.beginDockPanel("Viewport", ig::DockSlot::Center))
    {
        context.label("viewport");
        context.endDockPanel();
    }
    context.endDockSpace();
    context.endWindow();
    context.endFrame();

    context.beginFrame(ig::FrameInfo(800.0f, 600.0f));
    assert(context.beginMainWindow("full client"));
    assert(context.beginDockSpace("root"));
    if (context.beginDockPanel("Viewport", ig::DockSlot::Center))
    {
        context.label("viewport");
        context.endDockPanel();
    }
    context.endDockSpace();
    context.endWindow();
    const ig::DrawData &data = context.endFrame();
    bool reachedResizedClientEdge = false;
    for (ig::Span<const ig::DrawVertex>::size_type i = 0u; i < data.vertices.size(); ++i)
    {
        if (data.vertices[i].position.x >= 799.0f)
        {
            reachedResizedClientEdge = true;
            break;
        }
    }
    assert(reachedResizedClientEdge);
}

static void test_dock_panel_close_button()
{
    WidgetBackend backend;
    ig::Context context(backend);
    bool open = true;
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("closable dock", ig::Rect(10.0f, 10.0f, 520.0f, 400.0f)));
    assert(context.beginDockSpace("dock", ig::Rect(8.0f, 8.0f, 480.0f, 330.0f)));
    if (context.beginDockPanel("Closable", ig::DockSlot::Center, &open))
        context.endDockPanel();
    context.endDockSpace();
    context.endWindow();
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 285.0f, 64.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("closable dock", ig::Rect(10.0f, 10.0f, 520.0f, 400.0f)));
    assert(context.beginDockSpace("dock", ig::Rect(8.0f, 8.0f, 480.0f, 330.0f)));
    if (context.beginDockPanel("Closable", ig::DockSlot::Center, &open))
        context.endDockPanel();
    context.endDockSpace();
    context.endWindow();
    context.endFrame();
    assert(open);

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 285.0f, 64.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("closable dock", ig::Rect(10.0f, 10.0f, 520.0f, 400.0f)));
    assert(context.beginDockSpace("dock", ig::Rect(8.0f, 8.0f, 480.0f, 330.0f)));
    assert(!context.beginDockPanel("Closable", ig::DockSlot::Center, &open));
    context.endDockSpace();
    context.endWindow();
    context.endFrame();
    assert(!open);
}

static void drawEditorWidgets(ig::Context &context, ig::String &notes, ig::Color &color)
{
    assert(context.beginWindow("editors", ig::Rect(10.0f, 10.0f, 300.0f, 320.0f)));
    context.inputTextMultiline("notes", notes, ig::Rect(8.0f, 8.0f, 230.0f, 90.0f));
    context.colorEdit("accent", color, ig::Rect(8.0f, 110.0f, 230.0f, 140.0f));
    context.endWindow();
}

static void test_editor_widgets()
{
    WidgetBackend backend;
    ig::Context context(backend);
    ig::String notes("hello");
    ig::Color color(64u, 96u, 128u, 255u);
    const ig::Color originalColor = color;

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 40.0f, 65.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 40.0f, 65.0f));
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Enter));
    context.pushEvent(ig::Event::textInput("world"));
    context.beginFrame(ig::FrameInfo(360.0f, 360.0f));
    drawEditorWidgets(context, notes, color);
    context.endFrame();
    assert(notes == ig::String("hello\nworld"));
    assert(context.wantsTextInput());

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 120.0f, 184.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 120.0f, 184.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 360.0f));
    drawEditorWidgets(context, notes, color);
    context.endFrame();
    assert(color != originalColor);

    // A fully desaturated RGB value has no encoded hue.  The picker must keep
    // the hue selected afterwards so returning to the SV plane is predictable.
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 26.0f, 184.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 26.0f, 184.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 360.0f));
    drawEditorWidgets(context, notes, color);
    context.endFrame();
    assert(color.r == color.g && color.g == color.b);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 140.0f, 194.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 140.0f, 194.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 360.0f));
    drawEditorWidgets(context, notes, color);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 122.0f, 184.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 122.0f, 184.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 360.0f));
    drawEditorWidgets(context, notes, color);
    context.endFrame();
    assert(color.g > color.r && color.r > color.b);
}

static void drawScrollableEditor(ig::Context &context, ig::String &text)
{
    assert(context.beginWindow("scroll editor", ig::Rect(10.0f, 10.0f, 300.0f, 160.0f)));
    context.inputTextMultiline("document", text, ig::Rect(8.0f, 8.0f, 220.0f, 58.0f));
    context.endWindow();
}

static float multilineTextY(const ig::DrawData &data, uint32_t textSize)
{
    for (ig::Span<const ig::DrawCommand>::size_type i = 0u; i < data.commands.size(); ++i)
    {
        const ig::DrawCommand &command = data.commands[i];
        if (command.type == ig::DrawCommandType::Text && command.payload.text.textSize == textSize)
            return command.payload.text.position.y;
    }
    return -1.0f;
}

static void test_multiline_scroll_and_drag_widgets()
{
    WidgetBackend backend;
    ig::Context context(backend);
    ig::String text("one\ntwo\nthree\nfour\nfive\nsix");

    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    drawScrollableEditor(context, text);
    const ig::DrawData &initial = context.endFrame();
    const float initialY = multilineTextY(initial, static_cast<uint32_t>(text.size()));
    assert(initialY >= 0.0f);

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    context.pushEvent(ig::Event::pointerMove(40.0f, 60.0f));
    context.pushEvent(wheel);
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    drawScrollableEditor(context, text);
    const ig::DrawData &scrolled = context.endFrame();
    assert(multilineTextY(scrolled, static_cast<uint32_t>(text.size())) < initialY);

    float exposure = 0.0f;
    int iterations = 4;
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(context.beginWindow("drag controls", ig::Rect(10.0f, 10.0f, 300.0f, 150.0f)));
    context.dragFloat("exposure", exposure, -1.0f, 1.0f, 0.1f, ig::Rect(8.0f, 8.0f, 220.0f, 28.0f));
    context.dragInt("iterations", iterations, 0, 20, 1, ig::Rect(8.0f, 42.0f, 220.0f, 28.0f));
    context.separatorText("Advanced", 220.0f);
    context.endWindow();
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 60.0f, 50.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 65.0f, 50.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(context.beginWindow("drag controls", ig::Rect(10.0f, 10.0f, 300.0f, 150.0f)));
    const bool floatChanged = context.dragFloat("exposure", exposure, -1.0f, 1.0f, 0.1f,
                                                ig::Rect(8.0f, 8.0f, 220.0f, 28.0f));
    context.dragInt("iterations", iterations, 0, 20, 1, ig::Rect(8.0f, 42.0f, 220.0f, 28.0f));
    context.separatorText("Advanced", 220.0f);
    context.endWindow();
    context.endFrame();
    assert(floatChanged && exposure > 0.4f);

    exposure = 0.0f;
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 190.0f, 50.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 190.0f, 50.0f));
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Home));
    context.pushEvent(ig::Event::textInput("0.75"));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(context.beginWindow("drag controls", ig::Rect(10.0f, 10.0f, 300.0f, 150.0f)));
    assert(context.dragFloat("exposure", exposure, -1.0f, 1.0f, 0.1f,
                             ig::Rect(8.0f, 8.0f, 220.0f, 28.0f)));
    context.dragInt("iterations", iterations, 0, 20, 1, ig::Rect(8.0f, 42.0f, 220.0f, 28.0f));
    context.separatorText("Advanced", 220.0f);
    context.endWindow();
    context.endFrame();
    assert(exposure > 0.74f && exposure < 0.76f);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 60.0f, 84.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 64.0f, 84.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(context.beginWindow("drag controls", ig::Rect(10.0f, 10.0f, 300.0f, 150.0f)));
    context.dragFloat("exposure", exposure, -1.0f, 1.0f, 0.1f, ig::Rect(8.0f, 8.0f, 220.0f, 28.0f));
    const bool intChanged = context.dragInt("iterations", iterations, 0, 20, 1,
                                            ig::Rect(8.0f, 42.0f, 220.0f, 28.0f));
    context.separatorText("Advanced", 220.0f);
    context.endWindow();
    context.endFrame();
    assert(intChanged && iterations == 8);
}

static void drawInspectorContainers(ig::Context &context, ig::String &name, float &exposure)
{
    assert(context.beginWindow("inspector containers", ig::Rect(10.0f, 10.0f, 300.0f, 220.0f)));
    if (context.beginChild("components", 64.0f))
    {
        context.label("child-one");
        context.label("child-two");
        context.label("child-three");
        context.label("child-four");
        context.label("child-five");
        context.label("child-last");
        context.endChild();
    }
    if (context.beginTable("summary", 2))
    {
        context.tableNextColumn();
        context.label("Name");
        context.tableNextColumn();
        context.inputText("table name", name);
        context.tableNextColumn();
        context.label("Exposure");
        context.tableNextColumn();
        context.inputFloat("table exposure", exposure);
        context.endTable();
    }
    if (context.beginPropertyRow("Strength"))
    {
        context.sliderFloat("property strength", exposure, 0.0f, 1.0f);
        context.endPropertyRow();
    }
    context.endWindow();
}

static void test_child_table_and_property_row()
{
    WidgetBackend backend;
    ig::Context context(backend);
    ig::String name("Scene root");
    float exposure = 0.5f;

    context.beginFrame(ig::FrameInfo(360.0f, 260.0f));
    drawInspectorContainers(context, name, exposure);
    const ig::DrawData &initial = context.endFrame();
    const float initialY = multilineTextY(initial, 10u);
    assert(initialY >= 0.0f);

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    context.pushEvent(ig::Event::pointerMove(40.0f, 60.0f));
    context.pushEvent(wheel);
    context.beginFrame(ig::FrameInfo(360.0f, 260.0f));
    drawInspectorContainers(context, name, exposure);
    const ig::DrawData &scrolled = context.endFrame();
    assert(multilineTextY(scrolled, 10u) < initialY);
}

static void test_weighted_table_layout()
{
    WidgetBackend backend;
    ig::Context context(backend);
    const float weights[] = {3.0f, 1.0f};
    float firstWidth = 0.0f;
    float secondWidth = 0.0f;

    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(context.beginWindow("weighted table", ig::Rect(10.0f, 10.0f, 300.0f, 160.0f)));
    assert(context.beginTable("properties", ig::Span<const float>(weights)));
    assert(context.tableNextColumn());
    firstWidth = context.availableWidth();
    context.label("Name");
    assert(context.tableNextColumn());
    secondWidth = context.availableWidth();
    context.label("Value");
    context.endTable();
    context.endWindow();
    context.endFrame();

    assert(firstWidth > secondWidth * 2.9f);
    assert(firstWidth < secondWidth * 3.1f);
}

static bool beginTableWithTemporaryWeights(ig::Context &context)
{
    ct::Vector<float> weights;
    weights.push_back(3.0f);
    weights.push_back(1.0f);
    return context.beginTable("temporary weights", ig::Span<const float>(weights));
}

static void test_weighted_table_owns_weights()
{
    WidgetBackend backend;
    ig::Context context(backend);

    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(context.beginWindow("temporary weighted table", ig::Rect(10.0f, 10.0f, 300.0f, 160.0f)));
    assert(beginTableWithTemporaryWeights(context));
    assert(context.tableNextColumn());
    const float firstWidth = context.availableWidth();
    context.label("Name");
    assert(context.tableNextColumn());
    const float secondWidth = context.availableWidth();
    context.label("Value");
    context.endTable();
    context.endWindow();
    context.endFrame();

    assert(firstWidth > secondWidth * 2.9f);
    assert(firstWidth < secondWidth * 3.1f);
}

static bool drawSortableTable(ig::Context &context, int &sortColumn, bool &sortAscending)
{
    bool changed = false;
    assert(context.beginWindow("sortable table", ig::Rect(10.0f, 10.0f, 300.0f, 160.0f)));
    assert(context.beginTable("assets", 2));
    assert(context.tableNextColumn());
    changed = context.tableHeader("Name", sortColumn, sortAscending) || changed;
    assert(context.tableNextColumn());
    changed = context.tableHeader("Size", sortColumn, sortAscending) || changed;
    assert(context.tableNextColumn());
    context.label("albedo.png");
    assert(context.tableNextColumn());
    context.label("2 MB");
    context.endTable();
    context.endWindow();
    return changed;
}

static void test_sortable_table_headers()
{
    WidgetBackend backend;
    ig::Context context(backend);
    int sortColumn = -1;
    bool sortAscending = false;

    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(!drawSortableTable(context, sortColumn, sortAscending));
    context.endFrame();
    assert(sortColumn == -1);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 55.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 55.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(drawSortableTable(context, sortColumn, sortAscending));
    context.endFrame();
    assert(sortColumn == 0 && sortAscending);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 55.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 55.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(drawSortableTable(context, sortColumn, sortAscending));
    context.endFrame();
    assert(sortColumn == 0 && !sortAscending);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 190.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 190.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(drawSortableTable(context, sortColumn, sortAscending));
    context.endFrame();
    assert(sortColumn == 1 && sortAscending);
}

static void drawResizableTable(ig::Context &context, float *weights)
{
    int sortColumn = -1;
    bool sortAscending = true;
    assert(context.beginWindow("resizable table", ig::Rect(10.0f, 10.0f, 300.0f, 160.0f)));
    assert(context.beginTable("columns", ig::Span<float>(weights, 2u)));
    assert(context.tableNextColumn());
    context.tableHeader("Name", sortColumn, sortAscending);
    assert(context.tableNextColumn());
    context.tableHeader("Size", sortColumn, sortAscending);
    assert(context.tableNextColumn());
    context.label("albedo.png");
    assert(context.tableNextColumn());
    context.label("2 MB");
    context.endTable();
    context.endWindow();
}

static void test_resizable_table_columns()
{
    WidgetBackend backend;
    ig::Context context(backend);
    float weights[] = {1.0f, 1.0f};

    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    drawResizableTable(context, weights);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 160.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    drawResizableTable(context, weights);
    context.endFrame();

    context.pushEvent(ig::Event::pointerMove(190.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    drawResizableTable(context, weights);
    context.endFrame();
    assert(weights[0] > weights[1] * 1.4f);

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 190.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    drawResizableTable(context, weights);
    context.endFrame();
}

static void test_weighted_virtual_table_layout()
{
    WidgetBackend backend;
    ig::Context context(backend);
    const float weights[] = {3.0f, 1.0f};
    int firstVisible = 0;
    int lastVisible = 0;

    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(context.beginWindow("weighted virtual table", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    assert(context.beginVirtualTable("assets", 100, ig::Span<const float>(weights), 28.0f, 84.0f,
                                     firstVisible, lastVisible));
    const ig::Rect first = context.virtualTableCellRect(firstVisible, 0);
    const ig::Rect second = context.virtualTableCellRect(firstVisible, 1);
    context.endVirtualTable();
    context.endWindow();
    context.endFrame();

    assert(first.width > second.width * 2.9f);
    assert(first.width < second.width * 3.1f);
    assert(second.x == first.right());
}

static void drawVirtualList(ig::Context &context, int &selectedItem,
                            int &firstVisible, int &lastVisible)
{
    assert(context.beginWindow("virtual list", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    if (context.beginVirtualList("assets", 1000, 28.0f, 84.0f, firstVisible, lastVisible))
    {
        for (int item = firstVisible; item < lastVisible; ++item)
        {
            context.pushId(static_cast<uint64_t>(item));
            if (context.selectable("asset", selectedItem == item, context.virtualListItemRect(item)))
                selectedItem = item;
            context.popId();
        }
        context.endVirtualList();
    }
    context.endWindow();
}

static void test_virtual_list()
{
    WidgetBackend backend;
    ig::Context context(backend);
    int selectedItem = 0;
    int firstVisible = 0;
    int lastVisible = 0;

    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    drawVirtualList(context, selectedItem, firstVisible, lastVisible);
    context.endFrame();
    assert(firstVisible == 0);
    assert(lastVisible > firstVisible && lastVisible < 10);

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    context.pushEvent(ig::Event::pointerMove(40.0f, 60.0f));
    context.pushEvent(wheel);
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    drawVirtualList(context, selectedItem, firstVisible, lastVisible);
    context.endFrame();
    assert(firstVisible > 0);
    assert(lastVisible - firstVisible < 10);
}

static void drawVirtualTable(ig::Context &context, int &firstVisible, int &lastVisible,
                             ig::Rect &firstCell, ig::Rect &lastCell)
{
    assert(context.beginWindow("virtual table", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    if (context.beginVirtualTable("asset table", 1000, 3, 28.0f, 84.0f, firstVisible, lastVisible))
    {
        for (int row = firstVisible; row < lastVisible; ++row)
        {
            context.pushId(static_cast<uint64_t>(row));
            context.selectable("name", false, context.virtualTableCellRect(row, 0));
            context.selectable("type", false, context.virtualTableCellRect(row, 1));
            context.selectable("size", false, context.virtualTableCellRect(row, 2));
            context.popId();
        }
        firstCell = context.virtualTableCellRect(firstVisible, 0);
        lastCell = context.virtualTableCellRect(firstVisible, 2);
        context.endVirtualTable();
    }
    context.endWindow();
}

static void test_virtual_table()
{
    WidgetBackend backend;
    ig::Context context(backend);
    int firstVisible = 0;
    int lastVisible = 0;
    ig::Rect firstCell;
    ig::Rect lastCell;

    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    drawVirtualTable(context, firstVisible, lastVisible, firstCell, lastCell);
    context.endFrame();
    assert(firstVisible == 0);
    assert(lastVisible > firstVisible && lastVisible < 10);
    const float widthDifference = firstCell.width > lastCell.width
        ? firstCell.width - lastCell.width : lastCell.width - firstCell.width;
    assert(firstCell.width > 0.0f && lastCell.width > 0.0f && widthDifference < 0.01f);
    assert(lastCell.x > firstCell.x);

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    context.pushEvent(ig::Event::pointerMove(40.0f, 60.0f));
    context.pushEvent(wheel);
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    drawVirtualTable(context, firstVisible, lastVisible, firstCell, lastCell);
    context.endFrame();
    assert(firstVisible > 0);
}

static void drawVirtualTree(ig::Context &context, int &firstVisible, int &lastVisible,
                            ig::Rect &rootRow, ig::Rect &childRow)
{
    assert(context.beginWindow("virtual tree", ig::Rect(10.0f, 10.0f, 300.0f, 180.0f)));
    if (context.beginVirtualTree("hierarchy", 1000, 28.0f, 84.0f, firstVisible, lastVisible))
    {
        for (int row = firstVisible; row < lastVisible; ++row)
        {
            const int depth = row % 3;
            bool expanded = true;
            ig::TreeItemStyle style;
            style.leaf = depth == 2;
            context.treeItem(static_cast<ig::WidgetId>(row + 1), "node", expanded, style,
                             context.virtualTreeItemRect(row, depth));
        }
        rootRow = context.virtualTreeItemRect(firstVisible, 0);
        childRow = context.virtualTreeItemRect(firstVisible, 2);
        context.endVirtualTree();
    }
    context.endWindow();
}

static void test_virtual_tree()
{
    WidgetBackend backend;
    ig::Context context(backend);
    int firstVisible = 0;
    int lastVisible = 0;
    ig::Rect rootRow;
    ig::Rect childRow;

    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    drawVirtualTree(context, firstVisible, lastVisible, rootRow, childRow);
    context.endFrame();
    assert(firstVisible == 0);
    assert(lastVisible > firstVisible && lastVisible < 10);
    assert(childRow.x > rootRow.x && childRow.width < rootRow.width);

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    context.pushEvent(ig::Event::pointerMove(40.0f, 60.0f));
    context.pushEvent(wheel);
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    drawVirtualTree(context, firstVisible, lastVisible, rootRow, childRow);
    context.endFrame();
    assert(firstVisible > 0);
}

static void test_responsive_window_layout()
{
    WidgetBackend backend;
    ig::Context context(backend);
    ig::String value("resize me");

    context.beginFrame(ig::FrameInfo(480.0f, 260.0f));
    assert(context.beginWindow("responsive", ig::Rect(10.0f, 10.0f, 220.0f, 130.0f)));
    const float initialWidth = context.availableWidth();
    context.inputText("auto width", value);
    context.endWindow();
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 224.0f, 134.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 344.0f, 164.0f));
    context.beginFrame(ig::FrameInfo(480.0f, 260.0f));
    assert(context.beginWindow("responsive", ig::Rect(10.0f, 10.0f, 220.0f, 130.0f)));
    const float resizedWidth = context.availableWidth();
    context.inputText("auto width", value);
    context.endWindow();
    context.endFrame();
    assert(resizedWidth > initialWidth + 100.0f);
}

static void test_scene_tree_and_message_box()
{
    WidgetBackend backend;
    ig::Context context(backend);
    bool expanded = false;
    ig::TreeItemStyle style;
    style.typeColor = ig::Color(255u, 210u, 90u, 255u);

    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(context.beginWindow("scene tree", ig::Rect(10.0f, 10.0f, 260.0f, 140.0f)));
    context.treeItem("DirectionalLight3D", expanded, style, ig::Rect(8.0f, 8.0f, 220.0f, 28.0f));
    context.endWindow();
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 32.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 32.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(context.beginWindow("scene tree", ig::Rect(10.0f, 10.0f, 260.0f, 140.0f)));
    assert(!context.treeItem("DirectionalLight3D", expanded, style, ig::Rect(8.0f, 8.0f, 220.0f, 28.0f)));
    context.endWindow();
    context.endFrame();
    assert(expanded);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 100.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 100.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(context.beginWindow("scene tree", ig::Rect(10.0f, 10.0f, 260.0f, 140.0f)));
    assert(context.treeItem("DirectionalLight3D", expanded, style, ig::Rect(8.0f, 8.0f, 220.0f, 28.0f)));
    context.endWindow();
    context.endFrame();

    bool open = true;
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(context.messageBox("Delete node", "This action cannot be undone.", open, true) ==
           ig::MessageBoxResult::None);
    const ig::DrawData &dialog = context.endFrame();
    assert(!dialog.commands.empty());

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 280.0f, 150.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 280.0f, 150.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(context.messageBox("Delete node", "This action cannot be undone.", open, true) ==
           ig::MessageBoxResult::Accepted);
    context.endFrame();
    assert(!open);

    ig::String nodeName("Node");
    ig::MessageBoxOptions inputOptions;
    inputOptions.kind = ig::MessageBoxKind::Input;
    inputOptions.showCancel = true;
    inputOptions.inputValue = &nodeName;
    open = true;
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    context.messageBox("Rename", "Enter a name.", open, inputOptions);
    context.endFrame();
    context.pushEvent(ig::Event::textInput("3D"));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    context.messageBox("Rename", "Enter a name.", open, inputOptions);
    context.endFrame();
    assert(nodeName == ig::String("Node3D"));

    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    context.showToast("left", "left", ig::ToastPosition::TopLeft, 2.0f);
    context.showToast("center", "center", ig::ToastPosition::Center, 2.0f);
    context.showToast("right", "right", ig::ToastPosition::BottomRight, 2.0f);
    const ig::DrawData &toasts = context.endFrame();
    float leftX = -1.0f;
    float centerX = -1.0f;
    float rightX = -1.0f;
    for (ig::Span<const ig::DrawCommand>::size_type i = 0u; i < toasts.commands.size(); ++i)
    {
        const ig::DrawCommand &command = toasts.commands[i];
        if (command.type != ig::DrawCommandType::Text)
            continue;
        if (command.payload.text.textSize == 4u)
            leftX = command.payload.text.position.x;
        else if (command.payload.text.textSize == 6u)
            centerX = command.payload.text.position.x;
        else if (command.payload.text.textSize == 5u)
            rightX = command.payload.text.position.x;
    }
    assert(leftX >= 0.0f && centerX > leftX && rightX > centerX);
}

static bool drawDragDropWindows(ig::Context &context, ig::DragDropPayload &payload)
{
    const ig::WidgetId imageAsset = 0x494d414745415353ull;
    assert(context.beginWindow("asset source", ig::Rect(10.0f, 10.0f, 160.0f, 120.0f)));
    context.beginDragSource(42u, imageAsset, 9001u, "albedo.png", ig::Rect(8.0f, 8.0f, 120.0f, 28.0f));
    context.selectable("albedo.png", false, ig::Rect(8.0f, 8.0f, 120.0f, 28.0f));
    context.endWindow();

    assert(context.beginWindow("image target", ig::Rect(200.0f, 10.0f, 160.0f, 120.0f)));
    const bool dropped = context.acceptDragDropTarget(imageAsset, ig::Rect(8.0f, 8.0f, 120.0f, 80.0f), payload);
    context.endWindow();
    return dropped;
}

static void test_cross_window_drag_drop()
{
    WidgetBackend backend;
    ig::Context context(backend);
    ig::DragDropPayload payload;

    context.beginFrame(ig::FrameInfo(400.0f, 180.0f));
    assert(!drawDragDropWindows(context, payload));
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 40.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(400.0f, 180.0f));
    assert(!drawDragDropWindows(context, payload));
    context.endFrame();

    context.pushEvent(ig::Event::pointerMove(240.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(400.0f, 180.0f));
    assert(!drawDragDropWindows(context, payload));
    context.endFrame();

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 240.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(400.0f, 180.0f));
    assert(drawDragDropWindows(context, payload));
    context.endFrame();
    assert(payload.source == 42u && payload.data == 9001u);
}

static bool drawTreeDrag(ig::Context &context, ig::TreeDrop &drop, bool targetAcceptsChildren = false)
{
    bool sourceExpanded = false;
    bool targetExpanded = false;
    ig::TreeItemStyle sourceStyle;
    sourceStyle.typeColor = ig::Color(105u, 170u, 245u, 255u);
    ig::TreeItemStyle targetStyle;
    targetStyle.typeColor = ig::Color(255u, 220u, 105u, 255u);
    targetStyle.leaf = true;
    targetStyle.acceptsChildren = targetAcceptsChildren;

    assert(context.beginWindow("tree drag", ig::Rect(10.0f, 10.0f, 260.0f, 150.0f)));
    context.treeItem(101u, "Camera3D", sourceExpanded, sourceStyle,
                     ig::Rect(8.0f, 8.0f, 220.0f, 28.0f), &drop);
    context.treeItem(202u, "DirectionalLight3D", targetExpanded, targetStyle,
                     ig::Rect(8.0f, 40.0f, 220.0f, 28.0f), &drop);
    context.endWindow();
    return drop.source != ig::InvalidWidgetId;
}

static void test_tree_drag_drop_positions()
{
    WidgetBackend backend;
    ig::Context context(backend);
    ig::TreeDrop drop;

    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(!drawTreeDrag(context, drop));
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 100.0f, 70.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(!drawTreeDrag(context, drop));
    context.endFrame();

    context.pushEvent(ig::Event::pointerMove(100.0f, 108.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(!drawTreeDrag(context, drop));
    context.endFrame();

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 100.0f, 108.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(drawTreeDrag(context, drop));
    context.endFrame();
    assert(drop.source == 101u && drop.target == 202u);
    assert(drop.position == ig::TreeDropPosition::After);

    drop = ig::TreeDrop();
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(!drawTreeDrag(context, drop, true));
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 100.0f, 70.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(!drawTreeDrag(context, drop, true));
    context.endFrame();

    context.pushEvent(ig::Event::pointerMove(100.0f, 102.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(!drawTreeDrag(context, drop, true));
    context.endFrame();

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 100.0f, 102.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(drawTreeDrag(context, drop, true));
    context.endFrame();
    assert(drop.source == 101u && drop.target == 202u);
    assert(drop.position == ig::TreeDropPosition::Inside);
}

static bool drawPlainTreeAndDropTarget(ig::Context &context, ig::TreeDrop &drop)
{
    bool sourceExpanded = false;
    bool targetExpanded = false;
    ig::TreeItemStyle style;
    style.leaf = true;
    assert(context.beginWindow("plain tree", ig::Rect(10.0f, 10.0f, 260.0f, 150.0f)));
    context.treeItem("plain item", sourceExpanded, style,
                     ig::Rect(8.0f, 8.0f, 220.0f, 28.0f));
    context.treeItem(202u, "drop target", targetExpanded, style,
                     ig::Rect(8.0f, 40.0f, 220.0f, 28.0f), &drop);
    context.endWindow();
    return drop.source != ig::InvalidWidgetId;
}

static void test_plain_tree_item_is_not_a_drag_source()
{
    WidgetBackend backend;
    ig::Context context(backend);
    ig::TreeDrop drop;

    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(!drawPlainTreeAndDropTarget(context, drop));
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 100.0f, 70.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(!drawPlainTreeAndDropTarget(context, drop));
    context.endFrame();

    context.pushEvent(ig::Event::pointerMove(100.0f, 108.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(!drawPlainTreeAndDropTarget(context, drop));
    context.endFrame();

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 100.0f, 108.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 240.0f));
    assert(!drawPlainTreeAndDropTarget(context, drop));
    context.endFrame();
}

static bool drawFocusLostDragDrop(ig::Context &context, ig::DragDropPayload &payload)
{
    const ig::WidgetId payloadType = 0x464f43555344524full;
    assert(context.beginWindow("focus source", ig::Rect(10.0f, 10.0f, 160.0f, 120.0f)));
    context.beginDragSource(42u, payloadType, 9001u, "albedo.png", ig::Rect(8.0f, 8.0f, 120.0f, 28.0f));
    context.endWindow();
    assert(context.beginWindow("focus target", ig::Rect(200.0f, 10.0f, 160.0f, 120.0f)));
    const bool dropped = context.acceptDragDropTarget(payloadType,
                                                       ig::Rect(8.0f, 8.0f, 120.0f, 80.0f), payload);
    context.endWindow();
    return dropped;
}

static void test_focus_lost_cancels_drag_drop()
{
    WidgetBackend backend;
    ig::Context context(backend);
    ig::DragDropPayload payload;

    context.beginFrame(ig::FrameInfo(400.0f, 180.0f));
    assert(!drawFocusLostDragDrop(context, payload));
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 40.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(400.0f, 180.0f));
    assert(!drawFocusLostDragDrop(context, payload));
    context.endFrame();

    context.pushEvent(ig::Event::pointerMove(240.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(400.0f, 180.0f));
    assert(!drawFocusLostDragDrop(context, payload));
    context.endFrame();

    context.pushEvent(ig::Event::focusLost());
    context.beginFrame(ig::FrameInfo(400.0f, 180.0f));
    assert(!drawFocusLostDragDrop(context, payload));
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 240.0f, 60.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 240.0f, 60.0f));
    context.beginFrame(ig::FrameInfo(400.0f, 180.0f));
    assert(!drawFocusLostDragDrop(context, payload));
    context.endFrame();
}

static bool drawEditorInfrastructure(ig::Context &context, float &split, float &x, float &y, float &z,
                                     float &w)
{
    assert(context.beginWindow("editor infrastructure", ig::Rect(10.0f, 10.0f, 280.0f, 190.0f)));
    context.drawRectFilled(ig::Rect(8.0f, 8.0f, 210.0f, 72.0f), ig::Color(35u, 35u, 38u, 255u));
    context.drawLine(ig::Vec2(12.0f, 18.0f), ig::Vec2(120.0f, 60.0f), ig::Color(105u, 170u, 245u, 255u), 2.0f);
    context.drawCircleFilled(ig::Vec2(150.0f, 42.0f), 8.0f, ig::Color(255u, 220u, 105u, 255u));
    context.splitter("canvas split", split, 32.0f, 190.0f, ig::SplitterAxis::Vertical,
                     ig::Rect(8.0f, 8.0f, 210.0f, 72.0f));
    const bool clicked = context.isClicked("canvas click", ig::Rect(8.0f, 88.0f, 210.0f, 24.0f));
    context.inputFloat3("transform", x, y, z);
    context.dragFloat4("uv", x, y, z, w, -10.0f, 10.0f, 0.1f);
    context.endWindow();
    return clicked;
}

static void test_editor_infrastructure()
{
    WidgetBackend backend;
    ig::Context context(backend);
    float split = 80.0f;
    float x = 1.0f;
    float y = 2.0f;
    float z = 3.0f;
    float w = 4.0f;

    context.pushEvent(ig::Event::keyDown(ig::KeyCode::S, true));
    context.beginFrame(ig::FrameInfo(340.0f, 240.0f));
    assert(context.shortcut(ig::KeyCode::S));
    assert(context.isKeyPressed(ig::KeyCode::S));
    assert(!drawEditorInfrastructure(context, split, x, y, z, w));
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 106.0f, 70.0f));
    context.beginFrame(ig::FrameInfo(340.0f, 240.0f));
    assert(!drawEditorInfrastructure(context, split, x, y, z, w));
    context.endFrame();

    context.pushEvent(ig::Event::pointerMove(150.0f, 70.0f));
    context.beginFrame(ig::FrameInfo(340.0f, 240.0f));
    assert(!drawEditorInfrastructure(context, split, x, y, z, w));
    context.endFrame();
    assert(split > 115.0f && split < 130.0f);

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 150.0f, 70.0f));
    context.beginFrame(ig::FrameInfo(340.0f, 240.0f));
    assert(!drawEditorInfrastructure(context, split, x, y, z, w));
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 45.0f, 140.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 45.0f, 140.0f));
    context.beginFrame(ig::FrameInfo(340.0f, 240.0f));
    assert(drawEditorInfrastructure(context, split, x, y, z, w));
    context.endFrame();
}

static bool drawGizmo2D(ig::Context &context, ig::Transform2D &transform, ig::Gizmo2DMode mode)
{
    assert(context.beginWindow("gizmo canvas", ig::Rect(10.0f, 10.0f, 300.0f, 220.0f)));
    context.drawRectFilled(ig::Rect(8.0f, 8.0f, 240.0f, 160.0f), ig::Color(30u, 34u, 42u, 255u));
    const bool changed = context.gizmo2D("sprite", transform, mode, ig::Rect(8.0f, 8.0f, 240.0f, 160.0f));
    context.endWindow();
    return changed;
}

static void test_gizmo2d_transform_handles()
{
    WidgetBackend backend;
    ig::Context context(backend);
    ig::Transform2D transform;
    transform.position = ig::Vec2(100.0f, 70.0f);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 126.0f, 120.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 260.0f));
    assert(!drawGizmo2D(context, transform, ig::Gizmo2DMode::Translate));
    context.endFrame();
    context.pushEvent(ig::Event::pointerMove(146.0f, 132.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 260.0f));
    assert(drawGizmo2D(context, transform, ig::Gizmo2DMode::Translate));
    context.endFrame();
    assert(transform.position.x == 120.0f && transform.position.y == 82.0f);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 146.0f, 132.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 260.0f));
    drawGizmo2D(context, transform, ig::Gizmo2DMode::Translate);
    context.endFrame();

    transform.position = ig::Vec2(100.0f, 70.0f);
    transform.rotation = 0.0f;
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 175.0f, 120.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 260.0f));
    assert(!drawGizmo2D(context, transform, ig::Gizmo2DMode::Rotate));
    context.endFrame();
    context.pushEvent(ig::Event::pointerMove(126.0f, 169.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 260.0f));
    assert(drawGizmo2D(context, transform, ig::Gizmo2DMode::Rotate));
    context.endFrame();
    assert(transform.rotation > 85.0f && transform.rotation < 95.0f);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 126.0f, 169.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 260.0f));
    drawGizmo2D(context, transform, ig::Gizmo2DMode::Rotate);
    context.endFrame();

    transform.rotation = 0.0f;
    transform.scale = ig::Vec2(1.0f, 1.0f);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 186.0f, 120.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 260.0f));
    assert(!drawGizmo2D(context, transform, ig::Gizmo2DMode::Scale));
    context.endFrame();
    context.pushEvent(ig::Event::pointerMove(216.0f, 120.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 260.0f));
    assert(drawGizmo2D(context, transform, ig::Gizmo2DMode::Scale));
    context.endFrame();
    assert(transform.scale.x > 1.49f && transform.scale.x < 1.51f);
    assert(transform.scale.y == 1.0f);
}

static bool drawGizmo3D(ig::Context &context, ig::Transform3D &transform)
{
    const float identity[16] = {
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
    };
    ig::Gizmo3DOptions options;
    options.axisLength = 0.4f;
    assert(context.beginWindow("gizmo 3d canvas", ig::Rect(10.0f, 10.0f, 300.0f, 220.0f)));
    const bool changed = context.gizmo3D("cube", transform, ig::Gizmo3DMode::Translate,
                                         ig::Rect(8.0f, 8.0f, 240.0f, 160.0f),
                                         identity, identity, options);
    context.endWindow();
    return changed;
}

static void test_gizmo3d_transform_handles()
{
    WidgetBackend backend;
    ig::Context context(backend);
    ig::Transform3D transform;

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 175.0f, 130.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 260.0f));
    assert(!drawGizmo3D(context, transform));
    context.endFrame();

    context.pushEvent(ig::Event::pointerMove(185.0f, 130.0f));
    context.beginFrame(ig::FrameInfo(360.0f, 260.0f));
    assert(drawGizmo3D(context, transform));
    context.endFrame();
    assert(transform.position.x > 0.07f && transform.position.x < 0.10f);
}

class DialogProvider : public ig::FileDialogProvider
{
public:
    int listCalls = 0;
    bool createdFolder = false;

    bool listDirectory(ig::StringView path, ct::Vector<ig::FileDialogEntry> &entries) override
    {
        ++listCalls;
        ig::FileDialogEntry folder;
        folder.name = "assets";
        folder.path = ig::String(path.data(), path.size()) + "/assets";
        folder.directory = true;
        entries.push_back(folder);
        ig::FileDialogEntry image;
        image.name = "preview.png";
        image.path = ig::String(path.data(), path.size()) + "/preview.png";
        image.size = 1024u;
        entries.push_back(image);
        if (createdFolder)
        {
            ig::FileDialogEntry created;
            created.name = "My Folder";
            created.path = ig::String(path.data(), path.size());
            if (created.path != "/")
                created.path += "/";
            created.path += "My Folder";
            created.directory = true;
            entries.push_back(created);
        }
        return true;
    }

    ig::String parentDirectory(ig::StringView) override { return "/"; }
    ig::String homeDirectory() override { return "/home/test"; }
    ig::String createDirectory(ig::StringView path, ig::StringView name) override
    {
        createdFolder = true;
        ig::String result(path.data(), path.size());
        if (result != "/")
            result += "/";
        result += ig::String(name.data(), name.size());
        return result;
    }

    ig::FileDialogPreview imagePreview(ig::StringView) override
    {
        ig::FileDialogPreview preview;
        preview.texture = ig::TextureId(1u);
        preview.width = 128.0f;
        preview.height = 64.0f;
        return preview;
    }
};

class ManyEntriesDialogProvider : public ig::FileDialogProvider
{
public:
    bool listDirectory(ig::StringView path, ct::Vector<ig::FileDialogEntry> &entries) override
    {
        for (int index = 0; index < 12; ++index)
        {
            char name[24];
            snprintf(name, sizeof(name), "asset_%02d.png", index);
            ig::FileDialogEntry entry;
            entry.name = name;
            entry.path = ig::String(path.data(), path.size()) + "/" + name;
            entry.size = static_cast<uint64_t>(index + 1) * 1024u;
            entries.push_back(entry);
        }
        return true;
    }

    ig::String parentDirectory(ig::StringView) override { return "/"; }
    ig::String homeDirectory() override { return "/home/test"; }
};

static void test_file_dialog_utf8_folder_backspace()
{
    WidgetBackend backend;
    DialogProvider provider;
    ig::Context context(backend);
    ig::FileDialogState state;
    ig::FileDialogOptions options;
    bool open = true;
    state.initialized = true;
    state.path = "/project";
    state.creatingFolder = true;
    state.newFolderName = "caf\xc3\xa9";
    state.newFolderCursor = state.newFolderName.size();

    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Backspace));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("utf8 folder", open, state, options, provider);
    context.endFrame();

    assert(state.newFolderName == "caf");
    assert(state.newFolderCursor == 3u);
}

static void test_file_dialog_scroll_is_clamped()
{
    WidgetBackend backend;
    ManyEntriesDialogProvider provider;
    ig::Context context(backend);
    ig::FileDialogState state;
    ig::FileDialogOptions options;
    options.initialPath = "/project";
    bool open = true;

    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("scroll clamp", open, state, options, provider);
    context.endFrame();
    state.view = ig::FileDialogView::List;

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -100.0f;
    context.pushEvent(ig::Event::pointerMove(50.0f, 130.0f));
    context.pushEvent(wheel);
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("scroll clamp", open, state, options, provider);
    context.endFrame();

    assert(state.scrollOffset == 0.0f);
}

static void test_file_dialog_icon_margin_does_not_select()
{
    WidgetBackend backend;
    ManyEntriesDialogProvider provider;
    ig::Context context(backend);
    ig::FileDialogState state;
    ig::FileDialogOptions options;
    options.initialPath = "/project";
    bool open = true;

    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("icon margin", open, state, options, provider);
    context.endFrame();
    state.view = ig::FileDialogView::Icons;

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 590.0f, 110.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("icon margin", open, state, options, provider);
    context.endFrame();

    assert(state.selectedIndex == -3);
}

static void test_file_dialog_choose_folder()
{
    WidgetBackend backend;
    DialogProvider provider;
    ig::Context context(backend);
    ig::FileDialogState state;
    ig::FileDialogOptions options;
    options.title = "Choose Folder";
    options.initialPath = "/project";
    options.mode = ig::FileDialogMode::ChooseFolder;
    bool open = true;

    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    assert(context.fileDialog("folder dialog", open, state, options, provider).kind == ig::FileDialogResultKind::None);
    context.endFrame();
    assert(state.entries.size() == 2u);
    assert(provider.listCalls == 1);

    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("folder dialog", open, state, options, provider);
    context.endFrame();
    assert(provider.listCalls == 1);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 570.0f, 578.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 570.0f, 578.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    const ig::FileDialogResult result = context.fileDialog("folder dialog", open, state, options, provider);
    context.endFrame();
    assert(!open);
    assert(result.kind == ig::FileDialogResultKind::Accepted);
    assert(result.path == "/project");
}

static void test_file_dialog_resize()
{
    WidgetBackend backend;
    DialogProvider provider;
    ig::Context context(backend);
    ig::FileDialogState state;
    ig::FileDialogOptions options;
    options.initialPath = "/project";
    bool open = true;

    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("resize dialog", open, state, options, provider);
    context.endFrame();
    const ig::Vec2 initialSize = state.size;

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 620.0f, 592.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("resize dialog", open, state, options, provider);
    context.endFrame();
    context.pushEvent(ig::Event::pointerMove(540.0f, 520.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("resize dialog", open, state, options, provider);
    context.endFrame();
    assert(state.size.x < initialSize.x);
    assert(state.size.y < initialSize.y);

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 540.0f, 520.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("resize dialog", open, state, options, provider);
    context.endFrame();
    assert(!state.resizing);
}

static void test_file_dialog_navigation_and_create_folder()
{
    WidgetBackend backend;
    DialogProvider provider;
    ig::Context context(backend);
    ig::FileDialogState state;
    ig::FileDialogOptions options;
    options.initialPath = "/project";
    bool open = true;

    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("navigate dialog", open, state, options, provider);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 190.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 190.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("navigate dialog", open, state, options, provider);
    context.endFrame();
    assert(state.selectedIndex == -1);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 190.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 190.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("navigate dialog", open, state, options, provider);
    context.endFrame();
    assert(state.path == "/");
    assert(provider.listCalls == 2);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 136.0f, 98.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 136.0f, 98.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("navigate dialog", open, state, options, provider);
    context.endFrame();
    assert(state.creatingFolder);

    context.pushEvent(ig::Event::textInput("My Folder"));
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Enter));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("navigate dialog", open, state, options, provider);
    context.endFrame();
    assert(provider.createdFolder);
    assert(!state.creatingFolder);
    assert(state.selectedPath == "/My Folder");
}

static void test_file_dialog_image_preview_zoom()
{
    WidgetBackend backend;
    DialogProvider provider;
    ig::Context context(backend);
    ig::FileDialogState state;
    ig::FileDialogOptions options;
    options.initialPath = "/project";
    options.mode = ig::FileDialogMode::OpenImage;
    options.filter = ".png";
    bool open = true;

    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("image dialog", open, state, options, provider);
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 246.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 246.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("image dialog", open, state, options, provider);
    context.endFrame();
    assert(state.selectedPath == "/project/preview.png");

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = 1.0f;
    context.pushEvent(ig::Event::pointerMove(520.0f, 300.0f));
    context.pushEvent(wheel);
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("image dialog", open, state, options, provider);
    context.endFrame();
    assert(state.previewZoom > 1.0f);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 520.0f, 300.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("image dialog", open, state, options, provider);
    context.endFrame();
    context.pushEvent(ig::Event::pointerMove(550.0f, 324.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("image dialog", open, state, options, provider);
    context.endFrame();
    assert(state.previewPan.x > 29.0f && state.previewPan.y > 23.0f);
}

static void test_file_dialog_double_click_accepts_file()
{
    WidgetBackend backend;
    DialogProvider provider;
    ig::Context context(backend);
    ig::FileDialogState state;
    ig::FileDialogOptions options;
    options.initialPath = "/project";
    options.mode = ig::FileDialogMode::OpenImage;
    options.filter = ".png";
    bool open = true;

    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("double click dialog", open, state, options, provider);
    context.endFrame();
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 246.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 246.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("double click dialog", open, state, options, provider);
    context.endFrame();
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30.0f, 246.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30.0f, 246.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    const ig::FileDialogResult result = context.fileDialog("double click dialog", open, state, options, provider);
    context.endFrame();
    assert(!open);
    assert(result.kind == ig::FileDialogResultKind::Accepted);
    assert(result.path == "/project/preview.png");
}

class SortDialogProvider : public ig::FileDialogProvider
{
public:
    bool listDirectory(ig::StringView, ct::Vector<ig::FileDialogEntry> &entries) override
    {
        ig::FileDialogEntry alpha;
        alpha.name = "alpha.bin";
        alpha.path = "/project/alpha.bin";
        alpha.size = 200u;
        entries.push_back(alpha);
        ig::FileDialogEntry zeta;
        zeta.name = "zeta.bin";
        zeta.path = "/project/zeta.bin";
        zeta.size = 10u;
        entries.push_back(zeta);
        return true;
    }

    ig::String parentDirectory(ig::StringView) override { return "/"; }
    ig::String homeDirectory() override { return "/project"; }
};

static void test_file_dialog_detail_sorting()
{
    WidgetBackend backend;
    SortDialogProvider provider;
    ig::Context context(backend);
    ig::FileDialogState state;
    ig::FileDialogOptions options;
    options.initialPath = "/project";
    bool open = true;

    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("sort dialog", open, state, options, provider);
    context.endFrame();
    assert(state.entries[0].name == "alpha.bin");

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 450.0f, 132.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 450.0f, 132.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("sort dialog", open, state, options, provider);
    context.endFrame();
    assert(state.sortField == ig::FileDialogSortField::Size);
    assert(state.sortAscending);
    assert(state.entries[0].name == "zeta.bin");

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 450.0f, 132.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 450.0f, 132.0f));
    context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
    context.fileDialog("sort dialog", open, state, options, provider);
    context.endFrame();
    assert(!state.sortAscending);
    assert(state.entries[0].name == "alpha.bin");
}

static void test_save_file_names_and_overwrite()
{
    WidgetBackend backend;
    DialogProvider provider;
    ig::Context context(backend);
    ig::FileDialogOptions options;
    options.mode = ig::FileDialogMode::SaveFile;
    options.initialPath = "/project";
    ig::FileDialogState state;
    bool open = true;
    auto frame = [&]() {
        context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
        auto result = context.fileDialog("save", open, state, options, provider);
        context.endFrame();
        return result;
    };
    frame();
    context.pushEvent(ig::Event::textInput("new.txt"));
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Enter));
    auto result = frame();
    assert(result.kind == ig::FileDialogResultKind::Accepted && result.path == "/project/new.txt");

    open = true;
    options.initialFileName = "preview.png";
    frame();
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Enter));
    result = frame();
    assert(open && result.kind == ig::FileDialogResultKind::None && !state.overwritePath.empty());
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Enter));
    result = frame();
    assert(!open && result.path == "/project/preview.png");

    open = true;
    options.initialFileName = "../outside";
    frame();
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Enter));
    result = frame();
    assert(open && result.kind == ig::FileDialogResultKind::None && !state.fileError.empty());
    state.fileName = "assets";
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Enter));
    result = frame();
    assert(open && result.kind == ig::FileDialogResultKind::None && !state.fileError.empty());
}

static void test_file_dialog_keyboard_navigation()
{
    WidgetBackend backend;
    DialogProvider provider;
    ig::Context context(backend);
    ig::FileDialogOptions options;
    options.initialPath = "/project";
    ig::FileDialogState state;
    bool open = true;
    auto frame = [&]() {
        context.beginFrame(ig::FrameInfo(640.0f, 640.0f));
        auto result = context.fileDialog("keyboard", open, state, options, provider);
        context.endFrame();
        return result;
    };
    frame();
    for (int i = 0; i < 3; ++i) {
        context.pushEvent(ig::Event::keyDown(ig::KeyCode::Down));
        frame();
    }
    assert(state.selectedPath == "/project/assets");
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Enter));
    frame();
    assert(open && state.path == "/project/assets");
    options.mode = ig::FileDialogMode::ChooseFolder;
    for (int i = 0; i < 5; ++i) {
        context.pushEvent(ig::Event::keyDown(ig::KeyCode::Down));
        frame();
    }
    assert(state.entries[state.selectedIndex].directory);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Enter));
    auto result = frame();
    assert(result.kind == ig::FileDialogResultKind::Accepted && result.path == "/project/assets/assets");
}

static void test_small_dock_space()
{
    WidgetBackend backend;
    ig::Context context(backend);
    context.beginFrame(ig::FrameInfo(640.0f, 480.0f));
    assert(context.beginWindow("small dock", ig::Rect(0, 0, 400, 400)));
    assert(context.beginDockSpace("dock", ig::Rect(8, 8, 200, 200)));
    assert(context.beginDockPanel("Left", ig::DockSlot::Left));
    context.endDockPanel();
    assert(context.beginDockPanel("Center", ig::DockSlot::Center));
    context.endDockPanel();
    assert(context.beginDockPanel("Right", ig::DockSlot::Right));
    context.endDockPanel();
    context.endDockSpace();
    context.endWindow();
    context.endFrame();
}

static void test_file_dialog_read_error_and_paste()
{
    class Unreadable : public DialogProvider {
        bool listDirectory(ig::StringView, ct::Vector<ig::FileDialogEntry>& entries) override {
            ig::FileDialogEntry partial; partial.name = "partial"; entries.push_back(partial);
            return false;
        }
    } unreadable;
    WidgetBackend backend;
    ig::Context context(backend);
    ig::FileDialogOptions options;
    options.mode = ig::FileDialogMode::ChooseFolder;
    ig::FileDialogState state;
    bool open = true;
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Enter));
    context.beginFrame(ig::FrameInfo(640, 640));
    auto result = context.fileDialog("read error", open, state, options, unreadable);
    context.endFrame();
    assert(open && result.kind == ig::FileDialogResultKind::None);
    assert(!state.directoryError.empty() && state.entries.empty());

    state.reset();
    options.mode = ig::FileDialogMode::SaveFile;
    DialogProvider provider;
    backend.clipboard = "caf\xc3\xa9.txt";
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::V, true));
    context.beginFrame(ig::FrameInfo(640, 640));
    context.fileDialog("paste", open, state, options, provider);
    context.endFrame();
    assert(state.fileName == backend.clipboard && state.fileNameCursor == state.fileName.size());
}

static void test_menu_hover_switch()
{
    WidgetBackend backend;
    ig::Context context(backend);
    bool file = false, edit = false;
    auto frame = [&]() {
        context.beginFrame(ig::FrameInfo(320, 240));
        assert(context.beginWindow("hover menus", ig::Rect(10, 10, 280, 210)));
        assert(context.beginMenuBar(ig::Rect(0, 0, 260, 26)));
        file = context.beginMenu("File");
        if (file) { context.menuItem("Open"); context.endMenu(); }
        edit = context.beginMenu("Edit");
        if (edit) { context.menuItem("Copy"); context.endMenu(); }
        context.endMenuBar(); context.endWindow(); context.endFrame();
    };
    frame();
    context.pushEvent(ig::Event::pointerMove(90, 55));
    frame(); assert(!file && !edit);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 30, 55));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 30, 55));
    frame(); assert(file && !edit);
    context.pushEvent(ig::Event::pointerMove(90, 55));
    frame(); assert(edit);
    frame(); assert(!file && edit);
    context.pushEvent(ig::Event::pointerMove(30, 55));
    frame(); assert(file && !edit);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Escape));
    frame(); assert(!file && !edit);
}

static void test_file_dialog_resize_left_edge()
{
    WidgetBackend backend;
    DialogProvider provider;
    ig::Context context(backend);
    ig::FileDialogState state;
    ig::FileDialogOptions options;
    bool open = true;
    auto frame = [&]() {
        context.beginFrame(ig::FrameInfo(1000, 800));
        context.fileDialog("edge resize", open, state, options, provider);
        context.endFrame();
    };
    frame();
    const float right = state.position.x + state.size.x;
    const float oldWidth = state.size.x;
    const float x = state.position.x, y = state.position.y + 150.0f;
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
    frame();
    assert(state.resizing);
    context.pushEvent(ig::Event::pointerMove(x + 50, y));
    frame();
    assert(state.size.x == oldWidth - 50 && state.position.x + state.size.x == right);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x + 60, y));
    frame();
    assert(!state.resizing && state.size.x == oldWidth - 60);
}

int main()
{
    test_file_dialog_resize_left_edge();
    test_menu_hover_switch();
    test_file_dialog_read_error_and_paste();
    test_save_file_names_and_overwrite();
    test_file_dialog_keyboard_navigation();
    test_small_dock_space();
    test_widget_gallery();
    test_combo_popup_overlay();
    test_menu_and_context_menu();
    test_image_widgets();
    test_small_buttons();
    test_navigation_widgets();
    test_history_and_keyboard_navigation();
    test_dock_space();
    test_full_client_dock_space_and_theme_presets();
    test_dock_panel_close_button();
    test_editor_widgets();
    test_multiline_scroll_and_drag_widgets();
    test_child_table_and_property_row();
    test_weighted_table_layout();
    test_weighted_table_owns_weights();
    test_sortable_table_headers();
    test_resizable_table_columns();
    test_weighted_virtual_table_layout();
    test_virtual_list();
    test_virtual_table();
    test_virtual_tree();
    test_responsive_window_layout();
    test_scene_tree_and_message_box();
    test_cross_window_drag_drop();
    test_tree_drag_drop_positions();
    test_plain_tree_item_is_not_a_drag_source();
    test_focus_lost_cancels_drag_drop();
    test_editor_infrastructure();
    test_gizmo2d_transform_handles();
    test_gizmo3d_transform_handles();
    test_file_dialog_utf8_folder_backspace();
    test_file_dialog_scroll_is_clamped();
    test_file_dialog_icon_margin_does_not_select();
    test_file_dialog_choose_folder();
    test_file_dialog_resize();
    test_file_dialog_navigation_and_create_folder();
    test_file_dialog_image_preview_zoom();
    test_file_dialog_double_click_accepts_file();
    test_file_dialog_detail_sorting();
    return 0;
}
