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
    bool showInspector;
    bool selectedPreset;
    float volume;
    float progress;
    int quality;
    ig::String name;

    DemoState()
        : enabled(true), notifications(false), liveUpdates(true), advanced(false), showInspector(true),
          selectedPreset(false), volume(0.62f), progress(0.38f), quality(1), name("Raylib user")
    {
    }
};

void drawControls(ig::Context &ui, DemoState &state)
{
    const ig::StringView qualityItems[] = {
        ig::StringView("Low"), ig::StringView("Balanced"), ig::StringView("Ultra")
    };

    if (!ui.beginWindow("iGUI / Raylib", ig::Rect(56.0f, 64.0f, 420.0f, 550.0f)))
        return;

    ui.label("Immediate mode visual demo");
    ui.separator();
    ui.label("Profile");
    ui.inputText("Display name", state.name, 330.0f);
    ui.checkbox("Enable renderer", state.enabled);
    ui.checkbox("Desktop notifications", state.notifications);
    ui.toggleSwitch("Live updates", state.liveUpdates);
    ui.spacing(4.0f);

    ui.label("Rendering quality");
    ui.comboBox("Quality", state.quality,
                ig::Span<const ig::StringView>(qualityItems), 330.0f);
    ui.sliderFloat("Volume", state.volume, 0.0f, 1.0f, 330.0f);
    ui.progressBar(state.volume, 1.0f, 330.0f);
    ui.spacing(4.0f);

    ui.label("Preset");
    if (ui.radioButton("Safe defaults", !state.selectedPreset))
        state.selectedPreset = false;
    if (ui.radioButton("Performance", state.selectedPreset))
        state.selectedPreset = true;
    if (ui.selectable("Advanced options", state.advanced, 330.0f))
        state.advanced = !state.advanced;
    if (ui.button(state.showInspector ? "Hide inspector" : "Show inspector"))
        state.showInspector = !state.showInspector;
    ui.endWindow();
}

void drawInspector(ig::Context &ui, DemoState &state, float deltaSeconds)
{
    if (!state.showInspector)
        return;

    if (!ui.beginWindow("Live inspector", ig::Rect(510.0f, 148.0f, 350.0f, 280.0f)))
        return;

    state.progress += deltaSeconds * (state.enabled ? 0.18f : 0.04f);
    if (state.progress > 1.0f)
        state.progress = 0.0f;

    ui.label("Backend: Raylib");
    ui.label("Renderer: iGUI immediate");
    ui.separator();
    ui.label(state.enabled ? "Status: rendering" : "Status: paused");
    ui.progressBar(state.progress, 1.0f, 270.0f);
    ui.spacing(4.0f);
    ui.checkbox("Animate preview", state.enabled);
    ui.selectable("Click to focus this window", true, 270.0f);
    ui.endWindow();
}

} // namespace

int main()
{
    InitWindow(960, 640, "iGUI Raylib visual demo");
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
        drawControls(ui, state);
        drawInspector(ui, state, GetFrameTime());
        const ig::DrawData &drawData = ui.endFrame();

        BeginDrawing();
        ClearBackground(::Color{24u, 27u, 35u, 255u});
        DrawCircleGradient(875, 54, 220.0f, ::Color{42u, 86u, 130u, 90u},
                           ::Color{24u, 27u, 35u, 0u});
        backend.render(drawData);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
