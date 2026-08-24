#include <raylib.h>

#include <igui/Gui.hpp>
#include <igui_raylib/RaylibBackend.hpp>

namespace
{

const ig::WidgetId ImageAssetPayload = 0x494d414745415353ull;
const ig::WidgetId SceneRootNode = 0x5343454e45524f4full;
const ig::WidgetId CameraNode = 0x43414d4552413344ull;
const ig::WidgetId LightNode = 0x4c494748543344ull;
const ig::WidgetId EnvironmentNode = 0x454e5649524f4eull;
const ig::WidgetId WorldEnvironmentNode = 0x574f524c44454e56ull;

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
    bool sceneNodeExpanded[5];
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
    int sceneParent[5];
    int sceneOrder[4];
    uint64_t droppedImageAsset;
    float gamma;
    float dragExposure;
    ig::String name;
    ig::String notes;
    ig::String nodeName;
    ig::Color accent;

    DemoState()
        : enabled(true), notifications(false), liveUpdates(true), advanced(false), experimental(false), showInspector(true),
          profileOpen(true), numericOpen(true), selectionOpen(true), imagesOpen(true),
          workspaceOpen(true), windowManagerOpen(true), sceneExpanded(true), selectedPreset(false), deleteNodeDialog(false), renameNodeDialog(false),
          volume(0.62f), progress(0.38f), quality(1), samples(8), retries(2),
          sampleOffset(4), dragIterations(12), workspaceTab(0), selectedAsset(1), selectedSceneNode(0), droppedImageAsset(0u),
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
        sceneNodeExpanded[0] = false;
        sceneNodeExpanded[1] = false;
        sceneNodeExpanded[2] = false;
        sceneNodeExpanded[3] = true;
        sceneNodeExpanded[4] = false;
        sceneParent[0] = -1;
        sceneParent[1] = 0;
        sceneParent[2] = 0;
        sceneParent[3] = 0;
        sceneParent[4] = 3;
        sceneOrder[0] = 1;
        sceneOrder[1] = 2;
        sceneOrder[2] = 3;
        sceneOrder[3] = 4;
    }
};

ig::WidgetId sceneNodeId(int node)
{
    switch (node)
    {
    case 1: return CameraNode;
    case 2: return LightNode;
    case 3: return EnvironmentNode;
    case 4: return WorldEnvironmentNode;
    default: return ig::InvalidWidgetId;
    }
}

int sceneNodeIndex(ig::WidgetId id)
{
    for (int node = 1; node <= 4; ++node)
    {
        if (sceneNodeId(node) == id)
            return node;
    }
    return -1;
}

bool sceneNodeHasChildren(const DemoState &state, int parent)
{
    for (int node = 1; node <= 4; ++node)
    {
        if (state.sceneParent[node] == parent)
            return true;
    }
    return false;
}

bool sceneNodeIsDescendant(const DemoState &state, int node, int ancestor)
{
    for (int parent = state.sceneParent[node]; parent > 0; parent = state.sceneParent[parent])
    {
        if (parent == ancestor)
            return true;
    }
    return false;
}

void moveSceneOrder(DemoState &state, int source, int insertion)
{
    int sourceIndex = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (state.sceneOrder[i] == source)
        {
            sourceIndex = i;
            break;
        }
    }
    if (sourceIndex < 0)
        return;
    for (int i = sourceIndex; i < 3; ++i)
        state.sceneOrder[i] = state.sceneOrder[i + 1];
    if (sourceIndex < insertion)
        --insertion;
    for (int i = 3; i > insertion; --i)
        state.sceneOrder[i] = state.sceneOrder[i - 1];
    state.sceneOrder[insertion] = source;
}

bool applySceneTreeDrop(DemoState &state, const ig::TreeDrop &drop)
{
    const int source = sceneNodeIndex(drop.source);
    const int target = sceneNodeIndex(drop.target);
    if (source < 0 || target < 0 || source == target || sceneNodeIsDescendant(state, target, source))
        return false;

    int targetIndex = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (state.sceneOrder[i] == target)
        {
            targetIndex = i;
            break;
        }
    }
    if (targetIndex < 0)
        return false;

    if (drop.position == ig::TreeDropPosition::Inside)
    {
        state.sceneParent[source] = target;
        int insertion = targetIndex + 1;
        while (insertion < 4 && sceneNodeIsDescendant(state, state.sceneOrder[insertion], target))
            ++insertion;
        moveSceneOrder(state, source, insertion);
        state.sceneNodeExpanded[target] = true;
    }
    else
    {
        state.sceneParent[source] = state.sceneParent[target];
        moveSceneOrder(state, source, targetIndex +
                       (drop.position == ig::TreeDropPosition::After ? 1 : 0));
    }
    return true;
}

void drawSceneChildren(ig::Context &ui, DemoState &state, int parent, ig::TreeDrop &drop)
{
    for (int i = 0; i < 4; ++i)
    {
        const int node = state.sceneOrder[i];
        if (state.sceneParent[node] != parent)
            continue;

        ig::TreeItemStyle style;
        ig::StringView label;
        if (node == 1)
        {
            style.typeColor = ig::Color(125u, 205u, 250u, 255u);
            label = ig::StringView("Camera3D");
        }
        else if (node == 2)
        {
            style.typeColor = ig::Color(255u, 220u, 105u, 255u);
            label = ig::StringView("DirectionalLight3D");
        }
        else if (node == 3)
        {
            style.typeColor = ig::Color(115u, 225u, 155u, 255u);
            label = ig::StringView("Environment");
        }
        else
        {
            style.typeColor = ig::Color(165u, 125u, 240u, 255u);
            label = ig::StringView("WorldEnvironment");
        }
        style.leaf = !sceneNodeHasChildren(state, node);
        style.acceptsChildren = true;
        style.selected = state.selectedSceneNode == node;
        if (ui.treeItem(sceneNodeId(node), label, state.sceneNodeExpanded[node], style, &drop))
            state.selectedSceneNode = node;
        if (!style.leaf && state.sceneNodeExpanded[node])
        {
            ui.indent();
            drawSceneChildren(ui, state, node, drop);
            ui.unindent();
        }
    }
}

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
    const ig::Vec2 previewPosition = ui.cursor();
    ui.image(previewTexture, 128.0f, 128.0f);
    ig::DragDropPayload payload;
    if (ui.acceptDragDropTarget(ImageAssetPayload,
                                ig::Rect(previewPosition.x, previewPosition.y, 128.0f, 128.0f), payload))
    {
        state.droppedImageAsset = payload.data;
        ui.showToast("asset assigned", "Image asset assigned to preview", ig::ToastPosition::BottomRight);
    }
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

    if (ui.beginMenuBar(ig::Rect(0.0f, 0.0f, ui.availableWidth(), ui.theme().menuBarHeight)))
    {
        if (ui.beginMenu("File"))
        {
            if (ui.menuItem("New scene"))
                ui.showToast("new scene", "New scene created", ig::ToastPosition::BottomRight);
            ui.menuSeparator();
            if (ui.menuItem("Close workspace"))
                state.workspaceOpen = false;
            ui.endMenu();
        }
        if (ui.beginMenu("Edit"))
        {
            if (ui.menuItem("Rename selected"))
                state.renameNodeDialog = true;
            ui.menuItem("Paste", false);
            ui.endMenu();
        }
        if (ui.beginMenu("Scene"))
        {
            if (ui.menuItem("Refresh hierarchy"))
                ui.showToast("scene refresh", "Scene hierarchy refreshed", ig::ToastPosition::BottomRight);
            ui.endMenu();
        }
        if (ui.beginMenu("View"))
        {
            if (ui.beginSubMenu("Panels"))
            {
                ui.menuCheckbox("Inspector", state.showInspector);
                ui.menuCheckbox("Images", state.imagesOpen);
                ui.menuCheckbox("Workspace", state.workspaceOpen);
                ui.endSubMenu();
            }
            ui.endMenu();
        }
        ui.endMenuBar();
    }
    ui.spacing(ui.theme().menuBarHeight + ui.theme().itemSpacing);
    ui.tabBar("workspace tabs", state.workspaceTab, ig::Span<const ig::StringView>(tabs));
    ui.spacing(6.0f);
    if (state.workspaceTab == 0)
    {
        const ig::Vec2 sceneArea = ui.cursor();
        ig::TreeDrop sceneDrop;
        ig::TreeItemStyle sceneStyle;
        sceneStyle.typeColor = ig::Color(105u, 170u, 245u, 255u);
        sceneStyle.selected = state.selectedSceneNode == 0;
        if (ui.treeItem(SceneRootNode, "Demo scene", state.sceneExpanded, sceneStyle, &sceneDrop))
            state.selectedSceneNode = 0;
        if (state.sceneExpanded)
        {
            ui.indent();
            drawSceneChildren(ui, state, 0, sceneDrop);
            ig::TreeItemStyle hiddenStyle;
            hiddenStyle.typeColor = ig::Color(180u, 180u, 190u, 255u);
            hiddenStyle.leaf = true;
            hiddenStyle.disabled = true;
            ui.treeItem("Floor mesh (locked)", state.sceneNodeExpanded[4], hiddenStyle);
            ui.unindent();
        }
        if (sceneDrop.source != ig::InvalidWidgetId)
        {
            if (applySceneTreeDrop(state, sceneDrop))
                ui.showToast("scene tree move",
                             sceneDrop.position == ig::TreeDropPosition::Inside
                                 ? "Scene node reparented" : "Scene node reordered",
                             ig::ToastPosition::BottomRight);
            else
                ui.showToast("scene tree invalid move", "Cannot move a node inside one of its children", ig::ToastPosition::BottomRight);
        }
        if (ui.button("Delete selected node"))
            state.deleteNodeDialog = true;
        ui.sameLine(6.0f);
        if (ui.button("Rename node"))
            state.renameNodeDialog = true;
        if (ui.beginContextMenu("scene context", ig::Rect(sceneArea.x, sceneArea.y,
                                ui.availableWidth(), 176.0f)))
        {
            if (ui.menuItem("Duplicate selected"))
                ui.showToast("duplicate node", "Selected node duplicated", ig::ToastPosition::BottomRight);
            if (ui.menuItem("Rename"))
                state.renameNodeDialog = true;
            ui.menuSeparator();
            if (ui.menuItem("Delete selected"))
                state.deleteNodeDialog = true;
            ui.endContextMenu();
        }
    }
    else if (state.workspaceTab == 1)
    {
        ui.label("Drag an image into the Images preview");
        for (uint64_t i = 0u; i < 3u; ++i)
        {
            const ig::Vec2 position = ui.cursor();
            const ig::Rect row(position.x, position.y, ui.availableWidth(), ui.theme().widgetHeight);
            ui.beginDragSource(i + 1u, ImageAssetPayload, i + 1u, assets[i], row);
            ui.selectable(assets[i], state.selectedAsset == static_cast<int>(i));
        }
        ui.label("Materials");
        ui.label("wood.mat");
        ui.label("character.mesh");
        ui.label("postfx.shader");
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
