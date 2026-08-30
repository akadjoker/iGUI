#include <raylib.h>
#include <raymath.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>

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
    bool dataTableOpen;
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
    int virtualTreeSelected;
    int assetSortColumn;
    bool assetSortAscending;
    int sceneParent[5];
    int sceneOrder[4];
    bool virtualTreeExpanded[100];
    uint64_t droppedImageAsset;
    float gamma;
    float dragExposure;
    float positionX;
    float positionY;
    float positionZ;
    float scaleX;
    float scaleY;
    float scaleZ;
    float uvX;
    float uvY;
    float uvWidth;
    float uvHeight;
    ig::Transform2D gizmoTransform;
    ig::Gizmo2DMode gizmoMode;
    ig::Transform3D gizmo3DTransform;
    ig::Gizmo3DMode gizmo3DMode;
    ig::String name;
    ig::String notes;
    ig::String nodeName;
    ig::Color accent;
    float assetTableWeights[3];

    DemoState()
        : enabled(true), notifications(false), liveUpdates(true), advanced(false), experimental(false), showInspector(true),
          profileOpen(true), numericOpen(true), selectionOpen(true), imagesOpen(true), dataTableOpen(true),
          workspaceOpen(true), windowManagerOpen(true), sceneExpanded(true), selectedPreset(false), deleteNodeDialog(false), renameNodeDialog(false),
          volume(0.62f), progress(0.38f), quality(1), samples(8), retries(2),
          sampleOffset(4), dragIterations(12), workspaceTab(0), selectedAsset(1), selectedSceneNode(0), virtualTreeSelected(-1),
          assetSortColumn(0), assetSortAscending(true), droppedImageAsset(0u),
          gamma(2.2f), dragExposure(0.25f),
          positionX(0.0f), positionY(1.25f), positionZ(-3.5f),
          scaleX(1.0f), scaleY(1.0f), scaleZ(1.0f),
          uvX(0.0f), uvY(0.0f), uvWidth(1.0f), uvHeight(1.0f),
          gizmoTransform(), gizmoMode(ig::Gizmo2DMode::Translate),
          gizmo3DTransform(), gizmo3DMode(ig::Gizmo3DMode::Translate),
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
        assetTableWeights[0] = 2.2f;
        assetTableWeights[1] = 1.1f;
        assetTableWeights[2] = 0.8f;
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
        gizmoTransform.position = ig::Vec2(190.0f, 76.0f);
        sceneOrder[0] = 1;
        sceneOrder[1] = 2;
        sceneOrder[2] = 3;
        sceneOrder[3] = 4;
        gizmo3DTransform.position = ig::Vec3(0.0f, 0.0f, 0.0f);
        for (int group = 0; group < 100; ++group)
            virtualTreeExpanded[group] = true;
    }
};

struct DemoAssetRow
{
    const char *name;
    const char *kind;
    int sizeKiB;
};

int compareText(const char *left, const char *right)
{
    for (int i = 0; left[i] != '\0' || right[i] != '\0'; ++i)
    {
        if (left[i] < right[i])
            return -1;
        if (left[i] > right[i])
            return 1;
    }
    return 0;
}

int compareAssets(const DemoAssetRow &left, const DemoAssetRow &right, int column)
{
    if (column == 0)
        return compareText(left.name, right.name);
    if (column == 1)
        return compareText(left.kind, right.kind);
    return left.sizeKiB < right.sizeKiB ? -1 : (left.sizeKiB > right.sizeKiB ? 1 : 0);
}

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
    ui.sameLine(8.0f);
    if (ui.smallImageButton("small texture action", previewTexture))
        ui.showToast("small texture action", "Compact image action", ig::ToastPosition::BottomRight);
    ui.tooltip("Compact icon button");
    ui.endWindow();
}

int virtualTreeRowCount(const DemoState &state)
{
    int rowCount = 0;
    for (int group = 0; group < 100; ++group)
    {
        ++rowCount;
        if (state.virtualTreeExpanded[group])
            rowCount += 9;
    }
    return rowCount;
}

bool virtualTreeNodeAt(const DemoState &state, int row, int &group, int &child)
{
    for (group = 0; group < 100; ++group)
    {
        if (row == 0)
        {
            child = -1;
            return true;
        }
        --row;
        if (state.virtualTreeExpanded[group])
        {
            if (row < 9)
            {
                child = row;
                return true;
            }
            row -= 9;
        }
    }
    child = -1;
    return false;
}

bool projectDemo3D(const ::Matrix &view, const ::Matrix &projection, const ig::Rect &viewport,
                   const ig::Vec3 &world, ig::Vec2 &screen)
{
    const float viewX = view.m0 * world.x + view.m4 * world.y + view.m8 * world.z + view.m12;
    const float viewY = view.m1 * world.x + view.m5 * world.y + view.m9 * world.z + view.m13;
    const float viewZ = view.m2 * world.x + view.m6 * world.y + view.m10 * world.z + view.m14;
    const float viewW = view.m3 * world.x + view.m7 * world.y + view.m11 * world.z + view.m15;
    const float clipX = projection.m0 * viewX + projection.m4 * viewY + projection.m8 * viewZ + projection.m12 * viewW;
    const float clipY = projection.m1 * viewX + projection.m5 * viewY + projection.m9 * viewZ + projection.m13 * viewW;
    const float clipW = projection.m3 * viewX + projection.m7 * viewY + projection.m11 * viewZ + projection.m15 * viewW;
    if (clipW < 0.00001f && clipW > -0.00001f)
        return false;
    screen.x = viewport.x + (clipX / clipW * 0.5f + 0.5f) * viewport.width;
    screen.y = viewport.y + (0.5f - clipY / clipW * 0.5f) * viewport.height;
    return true;
}

ig::Vec3 rotateDemo3D(const ig::Vec3 &value, const ig::Vec3 &rotation)
{
    const float radians = DEG2RAD;
    const float cosX = cosf(rotation.x * radians);
    const float sinX = sinf(rotation.x * radians);
    const float cosY = cosf(rotation.y * radians);
    const float sinY = sinf(rotation.y * radians);
    const float cosZ = cosf(rotation.z * radians);
    const float sinZ = sinf(rotation.z * radians);
    const float y = value.y * cosX - value.z * sinX;
    const float z = value.y * sinX + value.z * cosX;
    const float x = value.x * cosY + z * sinY;
    const float rotatedZ = -value.x * sinY + z * cosY;
    return ig::Vec3(x * cosZ - y * sinZ, x * sinZ + y * cosZ, rotatedZ);
}

void drawWorkspaceWindow(ig::Context &ui, DemoState &state)
{
    const ig::StringView tabs[] = {
        ig::StringView("Scene"), ig::StringView("Assets"), ig::StringView("Settings"),
        ig::StringView("Hierarchy"), ig::StringView("Gizmo3D"), ig::StringView("Docking")
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
        ui.label("Virtual asset table (1,000 items)");
        int firstVisible = 0;
        int lastVisible = 0;
        if (ui.beginVirtualTable("asset table", 1000, 2, ui.theme().widgetHeight, 190.0f,
                                 firstVisible, lastVisible))
        {
            for (int item = firstVisible; item < lastVisible; ++item)
            {
                char generatedAsset[32];
                const ig::StringView asset = item < 6
                    ? assets[item]
                    : (snprintf(generatedAsset, sizeof(generatedAsset), "texture_%04d.png", item),
                       ig::StringView(generatedAsset));
                const ig::StringView type = item < 3 ? ig::StringView("image")
                                                      : ig::StringView("texture");
                const ig::Rect assetCell = ui.virtualTableCellRect(item, 0);
                const ig::Rect typeCell = ui.virtualTableCellRect(item, 1);
                const ig::Rect row(assetCell.x, assetCell.y,
                                   assetCell.width + typeCell.width, assetCell.height);
                ui.pushId(static_cast<uint64_t>(item));
                ui.beginDragSource(static_cast<ig::WidgetId>(item + 1), ImageAssetPayload,
                                   static_cast<uint64_t>(item + 1), asset, row);
                if (ui.selectable(asset, state.selectedAsset == item, assetCell) ||
                    ui.selectable(type, state.selectedAsset == item, typeCell))
                    state.selectedAsset = item;
                ui.popId();
            }
            ui.endVirtualTable();
        }
    }
    else if (state.workspaceTab == 2)
    {
        ui.label("2D viewport gizmo");
        if (ui.smallButton("Translate"))
            state.gizmoMode = ig::Gizmo2DMode::Translate;
        ui.sameLine();
        if (ui.smallButton("Rotate"))
            state.gizmoMode = ig::Gizmo2DMode::Rotate;
        ui.sameLine();
        if (ui.smallButton("Scale"))
            state.gizmoMode = ig::Gizmo2DMode::Scale;
        const ig::Vec2 canvasPosition = ui.cursor();
        const float canvasWidth = ui.availableWidth();
        const ig::Rect canvas(canvasPosition.x, canvasPosition.y, canvasWidth, 152.0f);
        ui.drawRectFilled(canvas, ig::Color(28u, 32u, 42u, 255u));
        for (float x = 0.0f; x < canvas.width; x += 24.0f)
            ui.drawLine(ig::Vec2(canvas.x + x, canvas.y),
                        ig::Vec2(canvas.x + x, canvas.y + canvas.height),
                        ig::Color(45u, 51u, 66u, 255u));
        for (float y = 0.0f; y < canvas.height; y += 24.0f)
            ui.drawLine(ig::Vec2(canvas.x, canvas.y + y),
                        ig::Vec2(canvas.x + canvas.width, canvas.y + y),
                        ig::Color(45u, 51u, 66u, 255u));
        const float objectHalfWidth = 21.0f * state.gizmoTransform.scale.x;
        const float objectHalfHeight = 15.0f * state.gizmoTransform.scale.y;
        const float rotation = state.gizmoTransform.rotation * 3.14159265358979323846f / 180.0f;
        const float cosine = cosf(rotation);
        const float sine = sinf(rotation);
        const float centerX = canvas.x + state.gizmoTransform.position.x;
        const float centerY = canvas.y + state.gizmoTransform.position.y;
        const ig::Vec2 corner0(centerX - objectHalfWidth * cosine + objectHalfHeight * sine,
                               centerY - objectHalfWidth * sine - objectHalfHeight * cosine);
        const ig::Vec2 corner1(centerX + objectHalfWidth * cosine + objectHalfHeight * sine,
                               centerY + objectHalfWidth * sine - objectHalfHeight * cosine);
        const ig::Vec2 corner2(centerX + objectHalfWidth * cosine - objectHalfHeight * sine,
                               centerY + objectHalfWidth * sine + objectHalfHeight * cosine);
        const ig::Vec2 corner3(centerX - objectHalfWidth * cosine - objectHalfHeight * sine,
                               centerY - objectHalfWidth * sine + objectHalfHeight * cosine);
        ui.drawLine(corner0, corner1, state.accent, 2.0f);
        ui.drawLine(corner1, corner2, state.accent, 2.0f);
        ui.drawLine(corner2, corner3, state.accent, 2.0f);
        ui.drawLine(corner3, corner0, state.accent, 2.0f);
        ig::Gizmo2DOptions gizmoOptions;
        gizmoOptions.translateSnap = 1.0f;
        gizmoOptions.rotateSnap = 15.0f;
        gizmoOptions.scaleSnap = 0.1f;
        ui.gizmo2D("viewport sprite", state.gizmoTransform, state.gizmoMode, canvas, gizmoOptions);
        ui.setCursor(ig::Vec2(canvas.x, canvas.y + canvas.height + ui.theme().itemSpacing));
        ui.inputFloat2("gizmo position", state.gizmoTransform.position.x, state.gizmoTransform.position.y);
        ui.dragFloat("gizmo rotation", state.gizmoTransform.rotation, -180.0f, 180.0f, 0.5f);
    }
    else if (state.workspaceTab == 3)
    {
        ui.label("Virtual hierarchy (100 collections / up to 1,000 nodes)");
        const int rowCount = virtualTreeRowCount(state);
        int firstVisible = 0;
        int lastVisible = 0;
        int toggledGroup = -1;
        bool toggledValue = false;
        if (ui.beginVirtualTree("large hierarchy", rowCount, ui.theme().widgetHeight, 190.0f,
                                firstVisible, lastVisible))
        {
            for (int row = firstVisible; row < lastVisible; ++row)
            {
                int group = 0;
                int child = -1;
                if (!virtualTreeNodeAt(state, row, group, child))
                    continue;

                char label[40];
                ig::TreeItemStyle style;
                const bool isGroup = child < 0;
                const int node = isGroup ? group * 10 : group * 10 + child + 1;
                bool expanded = state.virtualTreeExpanded[group];
                if (isGroup)
                {
                    snprintf(label, sizeof(label), "Collection %03d", group + 1);
                    style.typeColor = ig::Color(105u, 170u, 245u, 255u);
                    style.leaf = false;
                    style.acceptsChildren = true;
                }
                else
                {
                    snprintf(label, sizeof(label), "Mesh %03d-%02d", group + 1, child + 1);
                    style.typeColor = ig::Color(120u, 225u, 165u, 255u);
                    style.leaf = true;
                }
                style.selected = state.virtualTreeSelected == node;
                const ig::WidgetId nodeId = isGroup
                    ? static_cast<ig::WidgetId>(0x5647524f55500000ull + static_cast<uint64_t>(group))
                    : static_cast<ig::WidgetId>(0x56474348494c4400ull +
                                                 static_cast<uint64_t>(group * 9 + child));
                if (ui.treeItem(nodeId, ig::StringView(label), expanded, style,
                                ui.virtualTreeItemRect(row, isGroup ? 0 : 1)))
                    state.virtualTreeSelected = node;
                if (isGroup && expanded != state.virtualTreeExpanded[group])
                {
                    toggledGroup = group;
                    toggledValue = expanded;
                }
            }
            ui.endVirtualTree();
        }
        if (toggledGroup >= 0)
            state.virtualTreeExpanded[toggledGroup] = toggledValue;
    }
    else if (state.workspaceTab == 4)
    {
        ui.label("Perspective 3D gizmo");
        if (ui.smallButton("Translate 3D"))
            state.gizmo3DMode = ig::Gizmo3DMode::Translate;
        ui.sameLine();
        if (ui.smallButton("Rotate 3D"))
            state.gizmo3DMode = ig::Gizmo3DMode::Rotate;
        ui.sameLine();
        if (ui.smallButton("Scale 3D"))
            state.gizmo3DMode = ig::Gizmo3DMode::Scale;
        const ig::Vec2 canvasPosition = ui.cursor();
        const ig::Rect canvas(canvasPosition.x, canvasPosition.y, ui.availableWidth(), 152.0f);
        ui.drawRectFilled(canvas, ig::Color(28u, 32u, 42u, 255u));
        const ::Matrix view = ::MatrixLookAt(::Vector3{1.8f, 1.35f, 2.6f},
                                              ::Vector3{0.0f, 0.0f, 0.0f},
                                              ::Vector3{0.0f, 1.0f, 0.0f});
        const ::Matrix projection = ::MatrixPerspective(55.0f * DEG2RAD,
                                                         canvas.width / canvas.height,
                                                         0.1f, 20.0f);
        const float viewData[16] = {
            view.m0, view.m1, view.m2, view.m3, view.m4, view.m5, view.m6, view.m7,
            view.m8, view.m9, view.m10, view.m11, view.m12, view.m13, view.m14, view.m15
        };
        const float projectionData[16] = {
            projection.m0, projection.m1, projection.m2, projection.m3,
            projection.m4, projection.m5, projection.m6, projection.m7,
            projection.m8, projection.m9, projection.m10, projection.m11,
            projection.m12, projection.m13, projection.m14, projection.m15
        };
        for (int line = -3; line <= 3; ++line)
        {
            ig::Vec2 from;
            ig::Vec2 to;
            if (projectDemo3D(view, projection, canvas, ig::Vec3(-1.5f, -0.45f, line * 0.5f), from) &&
                projectDemo3D(view, projection, canvas, ig::Vec3(1.5f, -0.45f, line * 0.5f), to))
                ui.drawLine(from, to, ig::Color(47u, 59u, 76u, 255u));
            if (projectDemo3D(view, projection, canvas, ig::Vec3(line * 0.5f, -0.45f, -1.5f), from) &&
                projectDemo3D(view, projection, canvas, ig::Vec3(line * 0.5f, -0.45f, 1.5f), to))
                ui.drawLine(from, to, ig::Color(47u, 59u, 76u, 255u));
        }
        const float halfX = 0.2f * state.gizmo3DTransform.scale.x;
        const float halfY = 0.2f * state.gizmo3DTransform.scale.y;
        const float halfZ = 0.2f * state.gizmo3DTransform.scale.z;
        const ig::Vec3 position = state.gizmo3DTransform.position;
        const ig::Vec3 localCorners[] = {
            ig::Vec3(-halfX, -halfY, -halfZ), ig::Vec3(halfX, -halfY, -halfZ),
            ig::Vec3(halfX, halfY, -halfZ), ig::Vec3(-halfX, halfY, -halfZ),
            ig::Vec3(-halfX, -halfY, halfZ), ig::Vec3(halfX, -halfY, halfZ),
            ig::Vec3(halfX, halfY, halfZ), ig::Vec3(-halfX, halfY, halfZ)
        };
        ig::Vec3 corners[8];
        for (int corner = 0; corner < 8; ++corner)
        {
            const ig::Vec3 rotated = rotateDemo3D(localCorners[corner], state.gizmo3DTransform.rotation);
            corners[corner] = ig::Vec3(position.x + rotated.x, position.y + rotated.y, position.z + rotated.z);
        }
        const int edges[][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7}
        };
        for (int edge = 0; edge < 12; ++edge)
        {
            ig::Vec2 from;
            ig::Vec2 to;
            if (projectDemo3D(view, projection, canvas, corners[edges[edge][0]], from) &&
                projectDemo3D(view, projection, canvas, corners[edges[edge][1]], to))
                ui.drawLine(from, to, state.accent, 2.0f);
        }
        ig::Gizmo3DOptions gizmo3DOptions;
        gizmo3DOptions.axisLength = 0.65f;
        gizmo3DOptions.translateSnap = 0.1f;
        gizmo3DOptions.rotateSnap = 15.0f;
        gizmo3DOptions.scaleSnap = 0.1f;
        ui.gizmo3D("viewport object", state.gizmo3DTransform, state.gizmo3DMode, canvas,
                   viewData, projectionData, gizmo3DOptions);
        ui.setCursor(ig::Vec2(canvas.x, canvas.y + canvas.height + ui.theme().itemSpacing));
        ui.inputFloat3("gizmo 3d position", state.gizmo3DTransform.position.x,
                       state.gizmo3DTransform.position.y, state.gizmo3DTransform.position.z);
    }
    else
    {
        const ig::Vec2 dockPosition = ui.cursor();
        if (ui.beginDockSpace("workspace dock", ig::Rect(dockPosition.x, dockPosition.y,
                                                           ui.availableWidth(), 184.0f)))
        {
            if (ui.beginDockPanel("Hierarchy", ig::DockSlot::Left))
            {
                ui.label("Demo scene");
                ui.label("Camera");
                ui.label("Directional light");
                ui.endDockPanel();
            }
            if (ui.beginDockPanel("Assets", ig::DockSlot::Left))
            {
                ui.label("skybox.hdr");
                ui.label("character.mesh");
                ui.label("postfx.shader");
                ui.endDockPanel();
            }
            if (ui.beginDockPanel("Inspector", ig::DockSlot::Right))
            {
                ui.label("Transform");
                ui.dragFloat("X", state.positionX, -10.0f, 10.0f, 0.05f);
                ui.dragFloat("Y", state.positionY, -10.0f, 10.0f, 0.05f);
                ui.endDockPanel();
            }
            if (ui.beginDockPanel("Console", ig::DockSlot::Bottom))
            {
                ui.label("Renderer ready");
                ui.label("Drag the separators or switch the left tabs");
                ui.endDockPanel();
            }
            ui.endDockSpace();
        }
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
    ui.label("Tab changes focus; Enter activates buttons; Esc leaves text fields.");
    ui.separator();
    const float previousVolume = state.volume;
    if (ui.sliderFloat("Volume", state.volume, 0.0f, 1.0f))
    {
        const float nextVolume = state.volume;
        ui.pushUndo("Volume", [&state, previousVolume]() { state.volume = previousVolume; },
                    [&state, nextVolume]() { state.volume = nextVolume; });
    }
    const int previousSamples = state.samples;
    if (ui.sliderInt("Samples", state.samples, 1, 16))
    {
        const int nextSamples = state.samples;
        ui.pushUndo("Samples", [&state, previousSamples]() { state.samples = previousSamples; },
                    [&state, nextSamples]() { state.samples = nextSamples; });
    }
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

    if (ui.beginChild("inspector properties", 174.0f))
    {
        if (ui.beginPropertyRow("Backend"))
        {
            ui.label("Raylib");
            ui.endPropertyRow();
        }
        if (ui.beginPropertyRow("Renderer"))
        {
            ui.label("iGUI immediate");
            ui.endPropertyRow();
        }
        if (ui.beginPropertyRow("Position"))
        {
            ui.inputFloat3("position", state.positionX, state.positionY, state.positionZ);
            ui.endPropertyRow();
        }
        if (ui.beginPropertyRow("Scale"))
        {
            ui.dragFloat3("scale", state.scaleX, state.scaleY, state.scaleZ,
                          0.01f, 100.0f, 0.02f);
            ui.endPropertyRow();
        }
        if (ui.beginPropertyRow("UV rect"))
        {
            ui.inputFloat4("uv", state.uvX, state.uvY, state.uvWidth, state.uvHeight);
            ui.endPropertyRow();
        }
        ui.separator();
        const float runtimeSummaryWeights[] = {1.0f, 2.0f};
        if (ui.beginTable("runtime summary", ig::Span<const float>(runtimeSummaryWeights)))
        {
            ui.tableNextColumn();
            ui.label("Status");
            ui.tableNextColumn();
            ui.label(state.enabled ? "Rendering" : "Paused");
            ui.tableNextColumn();
            ui.label("Progress");
            ui.tableNextColumn();
            ui.progressBar(state.progress, 1.0f);
            ui.endTable();
        }
        ui.spacing(4.0f);
        ui.checkbox("Animate preview", state.enabled);
        ui.endChild();
    }
    ui.spacing(4.0f);
    if (ui.smallButton("Refresh"))
        ui.showToast("inspector refresh", "Inspector refreshed", ig::ToastPosition::BottomRight);
    ui.sameLine(5.0f);
    if (ui.smallButton("Reset"))
        state.progress = 0.0f;
    ui.selectable("Click to focus this window", true);
    ui.endWindow();
}

void drawDataTableWindow(ig::Context &ui, DemoState &state)
{
    static const DemoAssetRow assets[] = {
        {"albedo.png", "Texture", 2048},
        {"environment.hdr", "Texture", 8192},
        {"player.mesh", "Mesh", 1536},
        {"scene.json", "Data", 96},
        {"theme.igui", "Theme", 12}
    };
    const int baseAssetCount = static_cast<int>(sizeof(assets) / sizeof(assets[0]));
    const int assetCount = 120;

    if (!ui.beginWindow("Data table", ig::Rect(458.0f, 560.0f, 420.0f, 258.0f), &state.dataTableOpen))
        return;

    ui.label("Click a header to sort. Drag its separator to resize columns.");
    ui.separator();
    bool sortChanged = false;
    if (ui.beginTable("asset browser header", ig::Span<float>(state.assetTableWeights, 3u)))
    {
        assert(ui.tableNextColumn());
        sortChanged = ui.tableHeader("Name", state.assetSortColumn, state.assetSortAscending) || sortChanged;
        assert(ui.tableNextColumn());
        sortChanged = ui.tableHeader("Type", state.assetSortColumn, state.assetSortAscending) || sortChanged;
        assert(ui.tableNextColumn());
        sortChanged = ui.tableHeader("Size", state.assetSortColumn, state.assetSortAscending) || sortChanged;
        ui.endTable();
    }
    if (sortChanged)
        ui.showToast("asset table sort", "Asset table sorted", ig::ToastPosition::BottomRight);

    int order[assetCount];
    for (int i = 0; i < assetCount; ++i)
        order[i] = i;
    for (int i = 0; i < assetCount - 1; ++i)
    {
        for (int j = i + 1; j < assetCount; ++j)
        {
            const DemoAssetRow &left = assets[order[i] % baseAssetCount];
            const DemoAssetRow &right = assets[order[j] % baseAssetCount];
            int comparison = compareAssets(left, right, state.assetSortColumn);
            if (state.assetSortColumn == 2)
            {
                const int leftSize = left.sizeKiB + (order[i] / baseAssetCount) * 64;
                const int rightSize = right.sizeKiB + (order[j] / baseAssetCount) * 64;
                comparison = leftSize < rightSize ? -1 : (leftSize > rightSize ? 1 : 0);
            }
            if (comparison == 0)
                comparison = order[i] < order[j] ? -1 : (order[i] > order[j] ? 1 : 0);
            const bool swap = state.assetSortAscending ? comparison > 0 : comparison < 0;
            if (swap)
            {
                const int index = order[i];
                order[i] = order[j];
                order[j] = index;
            }
        }
    }

    int firstVisible = 0;
    int lastVisible = 0;
    if (ui.beginVirtualTable("asset browser rows", assetCount,
                             ig::Span<const float>(state.assetTableWeights, 3u),
                             ui.theme().widgetHeight, 142.0f,
                             firstVisible, lastVisible, false))
    {
        for (int row = firstVisible; row < lastVisible; ++row)
        {
            const int assetIndex = order[row];
            const DemoAssetRow &asset = assets[assetIndex % baseAssetCount];
            char name[64];
            char size[32];
            snprintf(name, sizeof(name), "%s #%03d", asset.name, assetIndex + 1);
            snprintf(size, sizeof(size), "%d KiB", asset.sizeKiB + (assetIndex / baseAssetCount) * 64);
            ui.pushId(static_cast<uint64_t>(assetIndex + 1));
            if (ui.selectable(name, state.selectedAsset == assetIndex,
                              ui.virtualTableCellRect(row, 0)))
                state.selectedAsset = assetIndex;
            ui.selectable(asset.kind, state.selectedAsset == assetIndex,
                          ui.virtualTableCellRect(row, 1));
            ui.selectable(size, state.selectedAsset == assetIndex,
                          ui.virtualTableCellRect(row, 2));
            ui.popId();
        }
        ui.endVirtualTable();
    }
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
        if (ui.shortcut(ig::KeyCode::S))
            ui.showToast("save shortcut", "Ctrl+S: scene saved", ig::ToastPosition::BottomRight);
        if (ui.shortcut(ig::KeyCode::Z) && ui.canRedo())
            ui.showToast("undo shortcut", "Undo applied", ig::ToastPosition::BottomRight);
        if ((ui.shortcut(ig::KeyCode::Y) || ui.shortcut(ig::KeyCode::Z, true, true)) && ui.canUndo())
            ui.showToast("redo shortcut", "Redo applied", ig::ToastPosition::BottomRight);
        drawProfileWindow(ui, state);
        drawNumericWindow(ui, state);
        drawSelectionWindow(ui, state);
        drawWindowManager(ui, state);
        drawWorkspaceWindow(ui, state);
        drawInspector(ui, state, GetFrameTime());
        drawDataTableWindow(ui, state);
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
