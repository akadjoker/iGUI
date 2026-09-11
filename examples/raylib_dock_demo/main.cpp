#include <raylib.h>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/stat.h>
#endif

#include <igui/Gui.hpp>
#include <igui_raylib/RaylibBackend.hpp>

namespace
{

class RaylibFileDialogProvider : public ig::FileDialogProvider
{
public:
    RaylibFileDialogProvider() : preview_(), previewPath_() {}

    ~RaylibFileDialogProvider() override
    {
        if (preview_.id != 0u)
            UnloadTexture(preview_);
    }

    bool listDirectory(ig::StringView path, ct::Vector<ig::FileDialogEntry> &entries) override
    {
        const ig::String nativePath(path.data(), path.size());
        const ::FilePathList files = LoadDirectoryFiles(nativePath.c_str());
        for (unsigned int i = 0u; i < files.count; ++i)
        {
            const char *filePath = files.paths[i];
            if (!filePath)
                continue;
            ig::FileDialogEntry entry;
            entry.path = filePath;
            entry.name = GetFileName(filePath);
            entry.directory = DirectoryExists(filePath);
            entry.size = entry.directory ? 0u : static_cast<uint64_t>(GetFileLength(filePath));
#if defined(_WIN32)
            ::WIN32_FILE_ATTRIBUTE_DATA metadata;
            if (GetFileAttributesExA(filePath, GetFileExInfoStandard, &metadata) != 0)
            {
                const uint64_t ticks = (static_cast<uint64_t>(metadata.ftLastWriteTime.dwHighDateTime) << 32u) |
                                       static_cast<uint64_t>(metadata.ftLastWriteTime.dwLowDateTime);
                entry.modifiedTime = ticks > 116444736000000000ull
                    ? (ticks - 116444736000000000ull) / 10000000ull : 0u;
            }
#else
            struct stat metadata;
            if (stat(filePath, &metadata) == 0)
                entry.modifiedTime = static_cast<uint64_t>(metadata.st_mtime);
#endif
            entry.hidden = !entry.name.empty() && entry.name[0] == '.';
            entries.push_back(entry);
        }
        UnloadDirectoryFiles(files);
        return true;
    }

    ig::String parentDirectory(ig::StringView path) override
    {
        const ig::String nativePath(path.data(), path.size());
        const char *parent = GetDirectoryPath(nativePath.c_str());
        return parent ? ig::String(parent) : ig::String();
    }

    ig::String homeDirectory() override
    {
        const char *directory = GetWorkingDirectory();
        return directory ? ig::String(directory) : ig::String();
    }

    ig::String createDirectory(ig::StringView parent, ig::StringView name) override
    {
        ig::String path(parent.data(), parent.size());
        if (!path.empty() && path[path.size() - 1u] != '/')
            path += "/";
        path += ig::String(name.data(), name.size());
#if defined(_WIN32)
        return CreateDirectoryA(path.c_str(), nullptr) != 0 ? path : ig::String();
#else
        return mkdir(path.c_str(), 0755) == 0 ? path : ig::String();
#endif
    }

    ig::FileDialogPreview imagePreview(ig::StringView path) override
    {
        const ig::String requested(path.data(), path.size());
        if (requested != previewPath_)
        {
            if (preview_.id != 0u)
                UnloadTexture(preview_);
            // Decode independently of the system Raylib's optional format flags.
            int width = 0, height = 0, channels = 0;
            unsigned char* pixels = stbi_load(requested.c_str(), &width, &height, &channels, 4);
            preview_ = {};
            if (pixels)
            {
                ::Image image = {pixels, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                preview_ = LoadTextureFromImage(image);
                if (preview_.id) SetTextureFilter(preview_, TEXTURE_FILTER_BILINEAR);
                stbi_image_free(pixels);
            }
            previewPath_ = requested;
        }
        ig::FileDialogPreview result;
        if (preview_.id != 0u)
        {
            result.texture = ig::raylib::textureId(preview_);
            result.width = static_cast<float>(preview_.width);
            result.height = static_cast<float>(preview_.height);
        }
        return result;
    }

private:
    ::Texture2D preview_;
    ig::String previewPath_;
};

struct StudioState
{
    int theme;
    int selectedAsset;
    int viewportTool;
    bool grid;
    bool snap;
    bool hierarchyOpen;
    bool assetsOpen;
    bool viewportOpen;
    bool inspectorOpen;
    bool outputOpen;
    bool profilerOpen;
    bool fileDialogOpen;
    int fileDialogKind;
    ig::FileDialogState fileDialog;
    float zoom;
    float exposure;
    ig::Transform2D transform;

    StudioState()
        : theme(0), selectedAsset(1), viewportTool(1), grid(true), snap(false), hierarchyOpen(true), assetsOpen(true),
          viewportOpen(true), inspectorOpen(true), outputOpen(true), profilerOpen(true),
          fileDialogOpen(false), fileDialogKind(0),
          zoom(100.0f), exposure(0.0f), transform()
    {
        transform.position = ig::Vec2(210.0f, 128.0f);
    }
};

ig::ThemePreset themePreset(int index)
{
    switch (index)
    {
    case 1: return ig::ThemePreset::Light;
    case 2: return ig::ThemePreset::Blender;
    case 3: return ig::ThemePreset::VSCode;
    default: return ig::ThemePreset::Dark;
    }
}

bool toolbarButton(ig::Context &ui, ig::StringView id, ig::StringView tooltip,
                   const ig::Rect &bounds, int icon, bool selected)
{
    ui.pushId(id);
    const bool clicked = ui.invisibleButton("button", bounds);
    const bool hovered = ui.isHovered("button", bounds);
    ui.popId();

    const ig::Color background = selected ? ui.theme().selectableSelected
                              : (hovered ? ui.theme().buttonHovered : ui.theme().buttonBackground);
    const ig::Color foreground = ui.theme().buttonText;
    ui.drawRectFilled(bounds, background);
    const float left = bounds.x + bounds.width * 0.25f;
    const float right = bounds.x + bounds.width * 0.75f;
    const float top = bounds.y + bounds.height * 0.25f;
    const float bottom = bounds.y + bounds.height * 0.75f;
    const float middleX = (left + right) * 0.5f;
    const float middleY = (top + bottom) * 0.5f;
    if (icon == 0)
    {
        ui.drawLine(ig::Vec2(left, top), ig::Vec2(left + 2.0f, bottom), foreground, 2.0f);
        ui.drawLine(ig::Vec2(left, top), ig::Vec2(right, middleY), foreground, 2.0f);
        ui.drawLine(ig::Vec2(left + 2.0f, bottom), ig::Vec2(middleX, middleY), foreground, 2.0f);
    }
    else if (icon == 1)
    {
        ui.drawLine(ig::Vec2(left, middleY), ig::Vec2(right, middleY), foreground, 2.0f);
        ui.drawLine(ig::Vec2(middleX, top), ig::Vec2(middleX, bottom), foreground, 2.0f);
        ui.drawCircleFilled(ig::Vec2(middleX, middleY), 2.5f, foreground);
    }
    else if (icon == 2)
    {
        ui.drawLine(ig::Vec2(left, bottom), ig::Vec2(left, top), foreground, 2.0f);
        ui.drawLine(ig::Vec2(left, top), ig::Vec2(right, top), foreground, 2.0f);
        ui.drawLine(ig::Vec2(right, top), ig::Vec2(right, middleY), foreground, 2.0f);
        ui.drawLine(ig::Vec2(right, middleY), ig::Vec2(right - 4.0f, middleY - 3.0f), foreground, 2.0f);
    }
    else if (icon == 3)
    {
        ui.drawRect(ig::Rect(left, top, right - left, bottom - top), foreground, 2.0f);
        ui.drawCircleFilled(ig::Vec2(right, bottom), 2.5f, foreground);
    }
    else if (icon == 4)
    {
        const float cell = (right - left) * 0.38f;
        ui.drawRect(ig::Rect(left, top, cell, cell), foreground);
        ui.drawRect(ig::Rect(right - cell, top, cell, cell), foreground);
        ui.drawRect(ig::Rect(left, bottom - cell, cell, cell), foreground);
        ui.drawRect(ig::Rect(right - cell, bottom - cell, cell, cell), foreground);
    }
    else if (icon == 5)
    {
        ui.drawLine(ig::Vec2(left, middleY), ig::Vec2(right, middleY), foreground, 2.0f);
        ui.drawLine(ig::Vec2(middleX, top), ig::Vec2(middleX, bottom), foreground, 2.0f);
        ui.drawCircleFilled(ig::Vec2(middleX, middleY), 3.0f, background);
        ui.drawRect(ig::Rect(middleX - 3.0f, middleY - 3.0f, 6.0f, 6.0f), foreground);
    }
    else if (icon == 6)
    {
        const float corner = (right - left) * 0.34f;
        ui.drawLine(ig::Vec2(left, top + corner), ig::Vec2(left, top), foreground, 2.0f);
        ui.drawLine(ig::Vec2(left, top), ig::Vec2(left + corner, top), foreground, 2.0f);
        ui.drawLine(ig::Vec2(right - corner, bottom), ig::Vec2(right, bottom), foreground, 2.0f);
        ui.drawLine(ig::Vec2(right, bottom), ig::Vec2(right, bottom - corner), foreground, 2.0f);
    }
    else
    {
        ui.drawLine(ig::Vec2(left, top), ig::Vec2(right, top), foreground, 2.0f);
        ui.drawLine(ig::Vec2(right, top), ig::Vec2(right, bottom), foreground, 2.0f);
        ui.drawLine(ig::Vec2(right, bottom), ig::Vec2(left + 3.0f, bottom), foreground, 2.0f);
        ui.drawLine(ig::Vec2(left + 3.0f, bottom), ig::Vec2(left + 7.0f, bottom - 4.0f), foreground, 2.0f);
    }
    ui.tooltip(tooltip);
    return clicked;
}

void drawViewportToolbar(ig::Context &ui, StudioState &state)
{
    const ig::Vec2 position = ui.cursor();
    const float height = ui.theme().widgetHeight + 8.0f;
    const ig::Rect toolbar(position.x, position.y, ui.availableWidth(), height);
    ui.drawRectFilled(toolbar, ui.theme().panelColor);
    ui.drawRect(toolbar, ui.theme().borderColor);
    const float size = ui.theme().widgetHeight - 4.0f;
    float x = toolbar.x + 4.0f;
    const float y = toolbar.y + 4.0f;
    if (toolbarButton(ui, "select", "Select", ig::Rect(x, y, size, size), 0, state.viewportTool == 0))
        state.viewportTool = 0;
    x += size + 3.0f;
    if (toolbarButton(ui, "move", "Move", ig::Rect(x, y, size, size), 1, state.viewportTool == 1))
        state.viewportTool = 1;
    x += size + 3.0f;
    if (toolbarButton(ui, "rotate", "Rotate", ig::Rect(x, y, size, size), 2, state.viewportTool == 2))
        state.viewportTool = 2;
    x += size + 3.0f;
    if (toolbarButton(ui, "scale", "Scale", ig::Rect(x, y, size, size), 3, state.viewportTool == 3))
        state.viewportTool = 3;
    x += size + 6.0f;
    ui.drawLine(ig::Vec2(x, y + 2.0f), ig::Vec2(x, y + size - 2.0f), ui.theme().borderColor);
    x += 6.0f;
    if (toolbarButton(ui, "grid", "Toggle grid", ig::Rect(x, y, size, size), 4, state.grid))
        state.grid = !state.grid;
    x += size + 3.0f;
    if (toolbarButton(ui, "snap", "Toggle snap", ig::Rect(x, y, size, size), 5, state.snap))
        state.snap = !state.snap;
    x += size + 6.0f;
    ui.drawLine(ig::Vec2(x, y + 2.0f), ig::Vec2(x, y + size - 2.0f), ui.theme().borderColor);
    x += 6.0f;
    if (toolbarButton(ui, "frame", "Frame selection", ig::Rect(x, y, size, size), 6, false))
        state.transform.position = ig::Vec2(210.0f, 128.0f);
    x += size + 3.0f;
    if (toolbarButton(ui, "reset", "Reset transform", ig::Rect(x, y, size, size), 7, false))
    {
        state.transform = ig::Transform2D();
        state.transform.position = ig::Vec2(210.0f, 128.0f);
    }
    ui.setCursor(ig::Vec2(position.x, position.y + height + ui.theme().itemSpacing));
}

void openFileDialog(StudioState &state, int kind)
{
    state.fileDialogOpen = true;
    state.fileDialogKind = kind;
}

void drawStudio(ig::Context &ui, StudioState &state, ig::FileDialogProvider &files)
{
    const ig::StringView themeNames[] = {
        ig::StringView("Dark"), ig::StringView("Light"), ig::StringView("Blender"), ig::StringView("VS Code")
    };
    const ig::StringView assets[] = {
        ig::StringView("skybox.hdr"), ig::StringView("robot.mesh"), ig::StringView("ground.mat"),
        ig::StringView("postfx.shader"), ig::StringView("icon.png")
    };

    if (!ui.beginMainWindow("iGUI Docking Studio"))
        return;

    const float menuHeight = ui.theme().menuBarHeight;
    if (ui.beginMenuBar(ig::Rect(0.0f, 0.0f, ui.availableWidth(), menuHeight)))
    {
        if (ui.beginMenu("File"))
        {
            if (ui.menuItem("New scene"))
                ui.showToast("new", "New scene created", ig::ToastPosition::BottomRight);
            if (ui.menuItem("Open file"))
                openFileDialog(state, 0);
            if (ui.menuItem("Open image"))
                openFileDialog(state, 1);
            if (ui.menuItem("Choose folder"))
                openFileDialog(state, 2);
            if (ui.menuItem("Save scene"))
                ui.showToast("save", "Scene saved", ig::ToastPosition::BottomRight);
            ui.endMenu();
        }
        if (ui.beginMenu("View"))
        {
            ui.menuCheckbox("Hierarchy", state.hierarchyOpen);
            ui.menuCheckbox("Assets", state.assetsOpen);
            ui.menuCheckbox("Viewport", state.viewportOpen);
            ui.menuCheckbox("Inspector", state.inspectorOpen);
            ui.menuCheckbox("Output", state.outputOpen);
            ui.menuCheckbox("Profiler", state.profilerOpen);
            ui.endMenu();
        }
        ui.endMenuBar();
    }
    const float dockInset = menuHeight + 4.0f;
    if (ui.beginDockSpace("studio", dockInset))
    {
        if (ui.beginDockPanel("Hierarchy", ig::DockSlot::Left, &state.hierarchyOpen))
        {
            bool sceneOpen = true;
            ui.treeNode("Demo scene", sceneOpen);
            ui.indent();
            ui.selectable("Camera", false);
            ui.selectable("Directional light", false);
            ui.selectable("Robot", true);
            ui.selectable("Ground", false);
            ui.unindent();
            ui.endDockPanel();
        }
        if (ui.beginDockPanel("Assets", ig::DockSlot::Left, &state.assetsOpen))
        {
            ui.label("Project assets");
            for (int item = 0; item < 5; ++item)
            {
                if (ui.selectable(assets[item], state.selectedAsset == item))
                    state.selectedAsset = item;
            }
            ui.endDockPanel();
        }
        if (ui.beginDockPanel("Viewport", ig::DockSlot::Center, &state.viewportOpen))
        {
            drawViewportToolbar(ui, state);
            const ig::Vec2 canvasPosition = ui.cursor();
            const float canvasWidth = ui.availableWidth();
            const ig::Rect canvas(canvasPosition.x, canvasPosition.y, canvasWidth, 260.0f);
            ui.drawRectFilled(canvas, ig::Color(27u, 31u, 40u, 255u));
            if (state.grid)
            {
                for (float x = 0.0f; x < canvas.width; x += 24.0f)
                    ui.drawLine(ig::Vec2(canvas.x + x, canvas.y),
                                ig::Vec2(canvas.x + x, canvas.y + canvas.height),
                                ig::Color(55u, 64u, 82u, 255u));
                for (float y = 0.0f; y < canvas.height; y += 24.0f)
                    ui.drawLine(ig::Vec2(canvas.x, canvas.y + y),
                                ig::Vec2(canvas.x + canvas.width, canvas.y + y),
                                ig::Color(55u, 64u, 82u, 255u));
            }
            const float x = canvas.x + state.transform.position.x;
            const float y = canvas.y + state.transform.position.y;
            ui.drawRect(ig::Rect(x - 30.0f, y - 22.0f, 60.0f, 44.0f), ig::Color(95u, 177u, 242u, 255u), 2.0f);
            ig::Gizmo2DOptions options;
            options.translateSnap = state.snap ? 8.0f : 0.0f;
            if (state.viewportTool != 0)
            {
                const ig::Gizmo2DMode mode = state.viewportTool == 2 ? ig::Gizmo2DMode::Rotate
                                         : (state.viewportTool == 3 ? ig::Gizmo2DMode::Scale
                                                                    : ig::Gizmo2DMode::Translate);
                ui.gizmo2D("robot transform", state.transform, mode, canvas, options);
            }
            ui.setCursor(ig::Vec2(canvas.x, canvas.y + canvas.height + ui.theme().itemSpacing));
            ui.label("Drag the arrows to move the selected object");
            ui.endDockPanel();
        }
        if (ui.beginDockPanel("Inspector", ig::DockSlot::Right, &state.inspectorOpen))
        {
            ui.separatorText("Appearance");
            if (ui.comboBox("Theme", state.theme, ig::Span<const ig::StringView>(themeNames)))
                ui.showToast("theme", "Theme changed", ig::ToastPosition::BottomRight);
            ui.tooltip("Choose the editor color preset");
            ui.checkbox("Grid", state.grid);
            ui.checkbox("Snap to grid", state.snap);
            ui.separatorText("View");
            ui.sliderFloat("Zoom", state.zoom, 25.0f, 200.0f);
            ui.dragFloat("Exposure", state.exposure, -2.0f, 2.0f, 0.02f);
            ui.separatorText("Transform");
            ui.inputFloat2("Position", state.transform.position.x, state.transform.position.y);
            ui.endDockPanel();
        }
        if (ui.beginDockPanel("Output", ig::DockSlot::Bottom, &state.outputOpen))
        {
            ui.label("Renderer initialized");
            ui.label("Drag tabs between regions and resize the separators");
            ui.label("Ctrl+Z / Ctrl+Y are available for registered application actions");
            ui.endDockPanel();
        }
        if (ui.beginDockPanel("Profiler", ig::DockSlot::Bottom, &state.profilerOpen))
        {
            ui.label("Frame: 0.7 ms");
            ui.progressBar(70.0f, 100.0f);
            ui.endDockPanel();
        }
        ui.endDockSpace();
    }
    ui.endWindow();

    if (state.fileDialogOpen)
    {
        ig::FileDialogOptions options;
        options.initialPath = GetWorkingDirectory();
        if (state.fileDialogKind == 1)
        {
            options.title = "Open Image";
            options.mode = ig::FileDialogMode::OpenImage;
            options.filter = ".png;.jpg;.jpeg;.bmp;.tga;.gif;.webp";
        }
        else if (state.fileDialogKind == 2)
        {
            options.title = "Choose Project Folder";
            options.mode = ig::FileDialogMode::ChooseFolder;
        }
        else
            options.title = "Open File";

        if (!state.fileDialog.initialized)
            state.fileDialog.view = state.fileDialogKind == 1 ? ig::FileDialogView::Icons : ig::FileDialogView::Details;
        const ig::FileDialogResult dialog = ui.fileDialog("project browser", state.fileDialogOpen,
                                                           state.fileDialog, options, files);
        if (dialog.kind == ig::FileDialogResultKind::Accepted)
        {
            ui.showToast("file dialog", dialog.path, ig::ToastPosition::BottomRight, 4.0f);
        }
    }
}

} // namespace

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 780, "iGUI Docking Studio");
    SetTargetFPS(60);

    ig::raylib::Backend backend;
    if (!backend.prepareFontAtlas())
    {
        CloseWindow();
        return 1;
    }

    ig::Context ui(backend, &backend.fontAtlas());
    StudioState state;
    RaylibFileDialogProvider files;
    while (!WindowShouldClose())
    {
        ig::raylib::processInput(ui);
        ui.beginFrame(ig::raylib::frameInfo(GetFrameTime()));
        ui.setTheme(ig::makeTheme(themePreset(state.theme)));
        drawStudio(ui, state, files);
        const ig::DrawData &drawData = ui.endFrame();

        BeginDrawing();
        const ig::Color background = ui.theme().bgColor;
        ClearBackground(::Color{background.r, background.g, background.b, background.a});
        backend.render(drawData);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
