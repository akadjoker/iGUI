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
    bool profileOpen;
    bool numericOpen;
    bool selectionOpen;
    bool imagesOpen;
    bool workspaceOpen;
    bool windowManagerOpen;
    bool sceneExpanded;
    bool cameraExpanded;
    bool renderExpanded;
    bool floorExpanded;
    bool selectedPreset;
    bool deleteNodeDialog;
    bool renameNodeDialog;
    float volume;
    float progress;
    int quality;
    int samples;
    int retries;
    int sampleOffset;
    int dragIterations;
    int workspaceTab;
    int selectedAsset;
    int selectedSceneNode;
    float gamma;
    float dragExposure;
    ig::String name;
    ig::String notes;
    ig::String nodeName;
    ig::Color accent;

    DemoState()
        : enabled(true), notifications(false), liveUpdates(true), advanced(false), experimental(false), showInspector(true),
          profileOpen(true), numericOpen(true), selectionOpen(true), imagesOpen(true),
          workspaceOpen(true), windowManagerOpen(true), sceneExpanded(true), cameraExpanded(false),
          renderExpanded(false), floorExpanded(false), selectedPreset(false), deleteNodeDialog(false), renameNodeDialog(false),
          volume(0.62f), progress(0.38f), quality(1), samples(8), retries(2),
          sampleOffset(4), dragIterations(12), workspaceTab(0), selectedAsset(1), selectedSceneNode(0),
          gamma(2.2f), dragExposure(0.25f),
          name("Raylib user"),
          nodeName("DirectionalLight3D"),
          notes("A multiline text editor now has a real scrollbar.\n"
                "Use the wheel over this area.\n"
                "You can also drag the scrollbar thumb.\n"
                "New lines keep the caret visible.\n"
                "Clipboard, Home and End are supported too.\n"
                "This final line demonstrates vertical scrolling."),
          accent(90u, 160u, 230u, 255u)
    {
    }
};

void drawProfileWindow(ig::Context &ui, DemoState &state)
{
    if (!ui.beginWindow("Profile and toggles", ig::Rect(42.0f, 72.0f, 390.0f, 330.0f), &state.profileOpen))
        return;

    ui.label("Text input and binary controls");
    ui.separator();
    ui.inputText("Display name", state.name);
    ui.checkbox("Enable renderer", state.enabled);
    ui.checkbox("Desktop notifications", state.notifications);
    ui.toggleSwitch("Live updates", state.liveUpdates);
    ui.spacing(10.0f);
    if (ui.button(state.showInspector ? "Hide inspector" : "Show inspector"))
        state.showInspector = !state.showInspector;
    ui.endWindow();
}

void drawImageWindow(ig::Context &ui, DemoState &state, ig::TextureId previewTexture)
{
    if (!ui.beginWindow("Images", ig::Rect(926.0f, 566.0f, 410.0f, 240.0f), &state.imagesOpen))
        return;

    ui.label("Texture previews and image buttons");
    ui.separator();
    ui.image(previewTexture, 128.0f, 128.0f);
    ui.sameLine(14.0f);
    ui.imageButton("texture preview action", previewTexture, 92.0f, 92.0f);
    ui.tooltip("Use this texture preview as an action button");
    ui.endWindow();
}

void drawWorkspaceWindow(ig::Context &ui, DemoState &state)
{
    const ig::StringView tabs[] = {
        ig::StringView("Scene"), ig::StringView("Assets"), ig::StringView("Settings")
    };
    const ig::StringView assets[] = {
        ig::StringView("skybox.hdr"), ig::StringView("albedo.png"), ig::StringView("normal.png"),
        ig::StringView("wood.mat"), ig::StringView("character.mesh"), ig::StringView("postfx.shader")
    };

    if (!ui.beginWindow("Workspace", ig::Rect(462.0f, 492.0f, 420.0f, 310.0f), &state.workspaceOpen))
        return;

    ui.tabBar("workspace tabs", state.workspaceTab, ig::Span<const ig::StringView>(tabs));
    ui.spacing(6.0f);
    if (state.workspaceTab == 0)
    {
        ig::TreeItemStyle sceneStyle;
        sceneStyle.typeColor = ig::Color(105u, 170u, 245u, 255u);
        sceneStyle.selected = state.selectedSceneNode == 0;
        if (ui.treeItem("Demo scene", state.sceneExpanded, sceneStyle))
            state.selectedSceneNode = 0;
        if (state.sceneExpanded)
        {
            ui.indent();
            ig::TreeItemStyle cameraStyle;
            cameraStyle.typeColor = ig::Color(125u, 205u, 250u, 255u);
            cameraStyle.leaf = true;
            cameraStyle.selected = state.selectedSceneNode == 1;
            if (ui.treeItem("Camera3D", state.cameraExpanded, cameraStyle))
                state.selectedSceneNode = 1;
            ig::TreeItemStyle lightStyle;
            lightStyle.typeColor = ig::Color(255u, 220u, 105u, 255u);
            lightStyle.leaf = true;
            lightStyle.selected = state.selectedSceneNode == 2;
            if (ui.treeItem("DirectionalLight3D", state.renderExpanded, lightStyle))
                state.selectedSceneNode = 2;
            ig::TreeItemStyle meshStyle;
            meshStyle.typeColor = ig::Color(115u, 225u, 155u, 255u);
            meshStyle.selected = state.selectedSceneNode == 3;
            if (ui.treeItem("Environment", state.renderExpanded, meshStyle))
                state.selectedSceneNode = 3;
            if (state.renderExpanded)
            {
                ui.indent();
                ig::TreeItemStyle childStyle;
                childStyle.typeColor = ig::Color(165u, 125u, 240u, 255u);
                childStyle.leaf = true;
                childStyle.selected = state.selectedSceneNode == 4;
                if (ui.treeItem("WorldEnvironment", state.floorExpanded, childStyle))
                    state.selectedSceneNode = 4;
                ui.unindent();
            }
            ig::TreeItemStyle hiddenStyle;
            hiddenStyle.typeColor = ig::Color(180u, 180u, 190u, 255u);
            hiddenStyle.leaf = true;
            hiddenStyle.disabled = true;
            ui.treeItem("Floor mesh (locked)", state.floorExpanded, hiddenStyle);
            ui.unindent();
        }
        if (ui.button("Delete selected node"))
            state.deleteNodeDialog = true;
        ui.sameLine(6.0f);
        if (ui.button("Rename node"))
            state.renameNodeDialog = true;
    }
    else if (state.workspaceTab == 1)
    {
        ui.label("Project assets");
        ui.listBox("project assets", state.selectedAsset, ig::Span<const ig::StringView>(assets), 0.0f, 4);
    }
    else
    {
        ui.inputTextMultiline("Notes", state.notes, 0.0f, 78.0f);
        ui.colorEdit("Accent color", state.accent, 0.0f, 142.0f);
    }
    ui.endWindow();
}

void drawWindowManager(ig::Context &ui, DemoState &state)
{
    if (!ui.beginWindow("Windows", ig::Rect(926.0f, 72.0f, 410.0f, 166.0f), &state.windowManagerOpen))
        return;

    ui.checkbox("Profile", state.profileOpen);
    ui.sameLine();
    ui.checkbox("Numeric", state.numericOpen);
    ui.sameLine();
    ui.checkbox("Selection", state.selectionOpen);
    ui.checkbox("Inspector", state.showInspector);
    ui.sameLine();
    ui.checkbox("Workspace", state.workspaceOpen);
    ui.sameLine();
    ui.checkbox("Images", state.imagesOpen);
    if (ui.button("Toast left"))
        ui.showToast("left toast", "Scene hierarchy refreshed", ig::ToastPosition::MiddleLeft);
    ui.sameLine();
    if (ui.button("Toast center"))
        ui.showToast("center toast", "Build complete", ig::ToastPosition::Center);
    ui.sameLine();
    if (ui.button("Toast right"))
        ui.showToast("right toast", "Saved successfully", ig::ToastPosition::BottomRight);
    ui.endWindow();
}

void drawNumericWindow(ig::Context &ui, DemoState &state)
{
    if (!ui.beginWindow("Numeric controls", ig::Rect(462.0f, 72.0f, 420.0f, 410.0f), &state.numericOpen))
        return;

    ui.label("Continuous and discrete values");
    ui.separator();
    ui.sliderFloat("Volume", state.volume, 0.0f, 1.0f);
    ui.sliderInt("Samples", state.samples, 1, 16);
    ui.separatorText("Relative drag controls");
    ui.dragFloat("Exposure", state.dragExposure, -2.0f, 2.0f);
    ui.dragInt("Iterations", state.dragIterations, 1, 64);
    ui.stepperInt("Retries", state.retries, 0, 5);
    ui.label("Sample offset");
    ui.inputInt("sample offset", state.sampleOffset);
    ui.label("Gamma");
    ui.inputFloat("gamma", state.gamma, 0.0f, 3);
    ui.progressBar(state.volume, 1.0f);
    ui.endWindow();
}

void drawSelectionWindow(ig::Context &ui, DemoState &state)
{
    const ig::StringView qualityItems[] = {
        ig::StringView("Low"), ig::StringView("Balanced"), ig::StringView("Ultra")
    };

    if (!ui.beginWindow("Selection widgets", ig::Rect(42.0f, 440.0f, 390.0f, 350.0f), &state.selectionOpen))
        return;

    ui.label("Lists, radio buttons and selection");
    ui.separator();
    ui.comboBox("Quality", state.quality,
                ig::Span<const ig::StringView>(qualityItems));
    ui.spacing(6.0f);
    ui.label("Preset");
    if (ui.radioButton("Safe defaults", !state.selectedPreset))
        state.selectedPreset = false;
    if (ui.radioButton("Performance", state.selectedPreset))
        state.selectedPreset = true;
    if (ui.collapsingHeader("Advanced options", state.advanced))
    {
        ui.checkbox("Use experimental pipeline", state.experimental);
        ui.sliderFloat("Exposure", state.volume, 0.0f, 1.0f);
    }
    ui.endWindow();
}

void drawInspector(ig::Context &ui, DemoState &state, float deltaSeconds)
{
    if (!state.showInspector)
        return;

    if (!ui.beginWindow("Live inspector", ig::Rect(926.0f, 252.0f, 410.0f, 300.0f), &state.showInspector))
        return;

    state.progress += deltaSeconds * (state.enabled ? 0.18f : 0.04f);
    if (state.progress > 1.0f)
        state.progress = 0.0f;

    ui.label("Backend: Raylib");
    ui.label("Renderer: iGUI immediate");
    ui.separator();
    ui.label(state.enabled ? "Status: rendering" : "Status: paused");
    ui.progressBar(state.progress, 1.0f);
    ui.spacing(4.0f);
    ui.checkbox("Animate preview", state.enabled);
    ui.selectable("Click to focus this window", true);
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
    const ::Image previewImage = ::GenImageChecked(96, 96, 12, 12,
                                                    ::Color{59u, 94u, 138u, 255u},
                                                    ::Color{87u, 162u, 226u, 255u});
    const ::Texture2D previewTexture = ::LoadTextureFromImage(previewImage);
    ::UnloadImage(previewImage);
    if (previewTexture.id == 0u)
    {
        CloseWindow();
        return 1;
    }

    while (!WindowShouldClose())
    {
        ig::raylib::processInput(ui);
        ui.beginFrame(ig::raylib::frameInfo(GetFrameTime()));
        drawProfileWindow(ui, state);
        drawNumericWindow(ui, state);
        drawSelectionWindow(ui, state);
        drawWindowManager(ui, state);
        drawWorkspaceWindow(ui, state);
        drawInspector(ui, state, GetFrameTime());
        drawImageWindow(ui, state, ig::raylib::textureId(previewTexture));
        ig::MessageBoxOptions deleteOptions;
        deleteOptions.kind = ig::MessageBoxKind::Error;
        deleteOptions.showCancel = true;
        ui.messageBox("Delete scene node", "This action cannot be undone.", state.deleteNodeDialog, deleteOptions);
        ig::MessageBoxOptions renameOptions;
        renameOptions.kind = ig::MessageBoxKind::Input;
        renameOptions.showCancel = true;
        renameOptions.inputValue = &state.nodeName;
        ui.messageBox("Rename scene node", "Enter the new node name.", state.renameNodeDialog, renameOptions);
        const ig::DrawData &drawData = ui.endFrame();

        BeginDrawing();
        ClearBackground(::Color{24u, 27u, 35u, 255u});
        DrawCircleGradient(1290, 74, 320.0f, ::Color{42u, 86u, 130u, 90u},
                           ::Color{24u, 27u, 35u, 0u});
        backend.render(drawData);
        EndDrawing();
    }

    UnloadTexture(previewTexture);
    CloseWindow();
    return 0;
}
