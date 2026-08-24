#include <assert.h>

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
    const ig::DrawData &data = context.endFrame();
    assert(!data.commands.empty());
    const ig::DrawCommand &tooltip = data.commands.back();
    assert(tooltip.type == ig::DrawCommandType::Text);
    assert(tooltip.payload.text.textSize == 15u);
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

static bool drawEditorInfrastructure(ig::Context &context, float &split, float &x, float &y, float &z)
{
    assert(context.beginWindow("editor infrastructure", ig::Rect(10.0f, 10.0f, 280.0f, 190.0f)));
    context.drawRectFilled(ig::Rect(8.0f, 8.0f, 210.0f, 72.0f), ig::Color(35u, 35u, 38u, 255u));
    context.drawLine(ig::Vec2(12.0f, 18.0f), ig::Vec2(120.0f, 60.0f), ig::Color(105u, 170u, 245u, 255u), 2.0f);
    context.drawCircleFilled(ig::Vec2(150.0f, 42.0f), 8.0f, ig::Color(255u, 220u, 105u, 255u));
    context.splitter("canvas split", split, 32.0f, 190.0f, ig::SplitterAxis::Vertical,
                     ig::Rect(8.0f, 8.0f, 210.0f, 72.0f));
    const bool clicked = context.invisibleButton("canvas click", ig::Rect(8.0f, 88.0f, 210.0f, 24.0f));
    context.inputFloat3("transform", x, y, z);
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

    context.pushEvent(ig::Event::keyDown(ig::KeyCode::S, true));
    context.beginFrame(ig::FrameInfo(340.0f, 240.0f));
    assert(context.shortcut(ig::KeyCode::S));
    assert(context.isKeyPressed(ig::KeyCode::S));
    assert(!drawEditorInfrastructure(context, split, x, y, z));
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 106.0f, 70.0f));
    context.beginFrame(ig::FrameInfo(340.0f, 240.0f));
    assert(!drawEditorInfrastructure(context, split, x, y, z));
    context.endFrame();

    context.pushEvent(ig::Event::pointerMove(150.0f, 70.0f));
    context.beginFrame(ig::FrameInfo(340.0f, 240.0f));
    assert(!drawEditorInfrastructure(context, split, x, y, z));
    context.endFrame();
    assert(split > 115.0f && split < 130.0f);

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 150.0f, 70.0f));
    context.beginFrame(ig::FrameInfo(340.0f, 240.0f));
    assert(!drawEditorInfrastructure(context, split, x, y, z));
    context.endFrame();

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 45.0f, 140.0f));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 45.0f, 140.0f));
    context.beginFrame(ig::FrameInfo(340.0f, 240.0f));
    assert(drawEditorInfrastructure(context, split, x, y, z));
    context.endFrame();
}

int main()
{
    test_widget_gallery();
    test_combo_popup_overlay();
    test_menu_and_context_menu();
    test_image_widgets();
    test_small_buttons();
    test_navigation_widgets();
    test_editor_widgets();
    test_multiline_scroll_and_drag_widgets();
    test_child_table_and_property_row();
    test_responsive_window_layout();
    test_scene_tree_and_message_box();
    test_cross_window_drag_drop();
    test_tree_drag_drop_positions();
    test_editor_infrastructure();
    return 0;
}
