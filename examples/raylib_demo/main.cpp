#include <raylib.h>

#include <igui/Gui.hpp>
#include <igui_raylib/RaylibBackend.hpp>

namespace
{

struct DemoState
{
    bool enabled;
    bool notifications;
    bool liveUpdates;
    bool advanced;
    bool experimental;
    bool showInspector;
    bool selectedPreset;
    float volume;
    float progress;
    int quality;
    int samples;
    int retries;
    int sampleOffset;
    float gamma;
    ig::String name;

    DemoState()
        : enabled(true), notifications(false), liveUpdates(true), advanced(false), experimental(false), showInspector(true),
          selectedPreset(false), volume(0.62f), progress(0.38f), quality(1), samples(8), retries(2),
          sampleOffset(4), gamma(2.2f),
          name("Raylib user")
    {
    }
};

void drawProfileWindow(ig::Context &ui, DemoState &state)
{
    if (!ui.beginWindow("Profile and toggles", ig::Rect(42.0f, 72.0f, 390.0f, 330.0f)))
        return;

    ui.label("Text input and binary controls");
    ui.separator();
    ui.inputText("Display name", state.name, 320.0f);
    ui.checkbox("Enable renderer", state.enabled);
    ui.checkbox("Desktop notifications", state.notifications);
    ui.toggleSwitch("Live updates", state.liveUpdates);
    ui.spacing(10.0f);
    if (ui.button(state.showInspector ? "Hide inspector" : "Show inspector"))
        state.showInspector = !state.showInspector;
    ui.endWindow();
}

void drawNumericWindow(ig::Context &ui, DemoState &state)
{
    if (!ui.beginWindow("Numeric controls", ig::Rect(462.0f, 72.0f, 420.0f, 350.0f)))
        return;

    ui.label("Continuous and discrete values");
    ui.separator();
    ui.sliderFloat("Volume", state.volume, 0.0f, 1.0f, 330.0f);
    ui.sliderInt("Samples", state.samples, 1, 16, 330.0f);
    ui.stepperInt("Retries", state.retries, 0, 5, 330.0f);
    ui.label("Sample offset");
    ui.inputInt("sample offset", state.sampleOffset, 330.0f);
    ui.label("Gamma");
    ui.inputFloat("gamma", state.gamma, 330.0f, 3);
    ui.progressBar(state.volume, 1.0f, 330.0f);
    ui.endWindow();
}

void drawSelectionWindow(ig::Context &ui, DemoState &state)
{
    const ig::StringView qualityItems[] = {
        ig::StringView("Low"), ig::StringView("Balanced"), ig::StringView("Ultra")
    };

    if (!ui.beginWindow("Selection widgets", ig::Rect(42.0f, 440.0f, 390.0f, 350.0f)))
        return;

    ui.label("Lists, radio buttons and selection");
    ui.separator();
    ui.comboBox("Quality", state.quality,
                ig::Span<const ig::StringView>(qualityItems), 320.0f);
    ui.spacing(6.0f);
    ui.label("Preset");
    if (ui.radioButton("Safe defaults", !state.selectedPreset))
        state.selectedPreset = false;
    if (ui.radioButton("Performance", state.selectedPreset))
        state.selectedPreset = true;
    if (ui.collapsingHeader("Advanced options", state.advanced, 320.0f))
    {
        ui.checkbox("Use experimental pipeline", state.experimental);
        ui.sliderFloat("Exposure", state.volume, 0.0f, 1.0f, 320.0f);
    }
    ui.endWindow();
}

void drawInspector(ig::Context &ui, DemoState &state, float deltaSeconds)
{
    if (!state.showInspector)
        return;

    if (!ui.beginWindow("Live inspector", ig::Rect(926.0f, 190.0f, 410.0f, 300.0f)))
        return;

    state.progress += deltaSeconds * (state.enabled ? 0.18f : 0.04f);
    if (state.progress > 1.0f)
        state.progress = 0.0f;

    ui.label("Backend: Raylib");
    ui.label("Renderer: iGUI immediate");
    ui.separator();
    ui.label(state.enabled ? "Status: rendering" : "Status: paused");
    ui.progressBar(state.progress, 1.0f, 330.0f);
    ui.spacing(4.0f);
    ui.checkbox("Animate preview", state.enabled);
    ui.selectable("Click to focus this window", true, 330.0f);
    ui.endWindow();
}

} // namespace

int main()
{
    InitWindow(1440, 860, "iGUI Raylib widget gallery");
    SetTargetFPS(60);

    ig::raylib::Backend backend;
    if (!backend.prepareFontAtlas())
    {
        CloseWindow();
        return 1;
    }

    ig::Context ui(backend, &backend.fontAtlas());
    DemoState state;

    while (!WindowShouldClose())
    {
        ig::raylib::processInput(ui);
        ui.beginFrame(ig::raylib::frameInfo(GetFrameTime()));
        drawProfileWindow(ui, state);
        drawNumericWindow(ui, state);
        drawSelectionWindow(ui, state);
        drawInspector(ui, state, GetFrameTime());
        const ig::DrawData &drawData = ui.endFrame();

        BeginDrawing();
        ClearBackground(::Color{24u, 27u, 35u, 255u});
        DrawCircleGradient(1290, 74, 320.0f, ::Color{42u, 86u, 130u, 90u},
                           ::Color{24u, 27u, 35u, 0u});
        backend.render(drawData);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
