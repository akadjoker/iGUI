// Open File / Save File / Choose Folder dialogs, and a modal Settings panel,
// all already in iGUI - this demo just wires them up.
//
// Build: cmake -DIGUI_BUILD_RAYLIB_DEMO=ON -DIGUI_BUILD_DIALOGS_DEMO=ON
// Run:   ./igui_dialogs_demo

#include <raylib.h>

#include <stdio.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#include <igui/Gui.hpp>
#include <igui_raylib/RaylibBackend.hpp>

namespace
{

// Same provider as raylib_dock_demo - real filesystem access via Raylib's
// directory functions.
class RaylibFileDialogProvider : public ig::FileDialogProvider
{
public:
    bool listDirectory(ig::StringView path, ct::Vector<ig::FileDialogEntry> &entries,
                       bool = true) override
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
            entry.size = entry.directory ? 0u
                                         : static_cast<uint64_t>(GetFileLength(filePath));
#if defined(_WIN32)
            ::WIN32_FILE_ATTRIBUTE_DATA metadata;
            if (GetFileAttributesExA(filePath, GetFileExInfoStandard, &metadata) != 0)
            {
                const uint64_t ticks =
                    (static_cast<uint64_t>(metadata.ftLastWriteTime.dwHighDateTime) << 32u) |
                    static_cast<uint64_t>(metadata.ftLastWriteTime.dwLowDateTime);
                entry.modifiedTime = ticks > 116444736000000000ull
                                        ? (ticks - 116444736000000000ull) / 10000000ull
                                        : 0u;
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
};

struct AppSettings
{
    float volume;
    bool autosave;
    int theme; // 0 = dark, 1 = light
    ig::String projectName;
};

} // namespace

int main()
{
    InitWindow(900, 620, "iGUI dialogs demo");
    SetTargetFPS(60);

    ig::raylib::Backend backend;
    if (!backend.prepareFontAtlas())
    {
        CloseWindow();
        return 1;
    }

    ig::Context ui(backend, &backend.fontAtlas());
    RaylibFileDialogProvider provider;

    bool openFileVisible = false;
    bool saveFileVisible = false;
    bool chooseFolderVisible = false;
    bool settingsVisible = false;

    ig::FileDialogState openFileState;
    ig::FileDialogState saveFileState;
    ig::FileDialogState chooseFolderState;

    ig::String lastResultLabel;
    ig::String lastResultPath;

    AppSettings settings;
    settings.volume = 0.8f;
    settings.autosave = true;
    settings.theme = 0;
    settings.projectName = "Untitled project";

    while (!WindowShouldClose())
    {
        ig::raylib::processInput(ui);
        ui.beginFrame(ig::raylib::frameInfo(GetFrameTime()));

        if (ui.beginMainWindow("Dialogs demo"))
        {
            ui.label("Every dialog below is already in iGUI (igui/FileDialog.hpp, Context::messageBox, setModalWindow).");
            ui.separator();

            if (ui.button("Open File..."))
            {
                openFileState = ig::FileDialogState();
                openFileVisible = true;
            }
            ui.sameLine();
            if (ui.button("Save File..."))
            {
                saveFileState = ig::FileDialogState();
                saveFileVisible = true;
            }
            ui.sameLine();
            if (ui.button("Choose Folder..."))
            {
                chooseFolderState = ig::FileDialogState();
                chooseFolderVisible = true;
            }
            ui.sameLine();
            if (ui.button("Settings..."))
                settingsVisible = true;

            ui.separator();
            if (!lastResultLabel.empty())
            {
                char line[512];
                snprintf(line, sizeof(line), "%s: %s", lastResultLabel.c_str(),
                        lastResultPath.empty() ? "(cancelled)" : lastResultPath.c_str());
                ui.label(line);
            }
            else
            {
                ui.label("No dialog result yet.");
            }

            char settingsLine[160];
            snprintf(settingsLine, sizeof(settingsLine),
                    "Project: %s  |  Volume: %d%%  |  Autosave: %s  |  Theme: %s",
                    settings.projectName.c_str(),
                    static_cast<int>(settings.volume * 100.0f + 0.5f),
                    settings.autosave ? "on" : "off",
                    settings.theme == 0 ? "dark" : "light");
            ui.label(settingsLine);

            ui.endWindow();
        }

        if (openFileVisible)
        {
            ig::FileDialogOptions options;
            options.title = "Open File";
            options.mode = ig::FileDialogMode::OpenFile;
            const ig::FileDialogResult result =
                ui.fileDialog("open", openFileVisible, openFileState, options, provider);
            if (result.kind != ig::FileDialogResultKind::None)
            {
                openFileVisible = false;
                if (result.kind == ig::FileDialogResultKind::Accepted)
                {
                    lastResultLabel = "Open File";
                    lastResultPath = result.path;
                }
            }
        }

        if (saveFileVisible)
        {
            ig::FileDialogOptions options;
            options.title = "Save File";
            options.mode = ig::FileDialogMode::SaveFile;
            options.initialFileName = "untitled.txt";
            const ig::FileDialogResult result =
                ui.fileDialog("save", saveFileVisible, saveFileState, options, provider);
            if (result.kind != ig::FileDialogResultKind::None)
            {
                saveFileVisible = false;
                if (result.kind == ig::FileDialogResultKind::Accepted)
                {
                    lastResultLabel = "Save File";
                    lastResultPath = result.path;
                }
            }
        }

        if (chooseFolderVisible)
        {
            ig::FileDialogOptions options;
            options.title = "Choose Folder";
            options.mode = ig::FileDialogMode::ChooseFolder;
            const ig::FileDialogResult result = ui.fileDialog(
                "folder", chooseFolderVisible, chooseFolderState, options, provider);
            if (result.kind != ig::FileDialogResultKind::None)
            {
                chooseFolderVisible = false;
                if (result.kind == ig::FileDialogResultKind::Accepted)
                {
                    lastResultLabel = "Choose Folder";
                    lastResultPath = result.path;
                }
            }
        }

        if (settingsVisible)
        {
            // setModalWindow() is what turns an ordinary window into a modal:
            // every other window's widgets stop reacting to hover/click/
            // keyboard while it's set, and it must be cleared once the
            // window closes.
            ui.setModalWindow("Settings", true);
            if (ui.beginWindow("Settings", ig::Rect(260.0f, 160.0f, 380.0f, 280.0f),
                              &settingsVisible))
            {
                ui.label("Project name");
                ui.inputText("settings.name", settings.projectName, 300.0f);
                ui.spacing(8.0f);

                ui.label("Volume");
                ui.sliderFloat("settings.volume", settings.volume, 0.0f, 1.0f,
                              ig::Rect(0.0f, 0.0f, 300.0f, 24.0f));
                ui.spacing(8.0f);

                ui.checkbox("Autosave", settings.autosave,
                           ig::Rect(0.0f, 0.0f, 20.0f, 20.0f));
                ui.spacing(8.0f);

                const ig::StringView themeItems[] = {ig::StringView("Dark"),
                                                     ig::StringView("Light")};
                ui.label("Theme");
                ui.comboBox("settings.theme", settings.theme,
                          ig::Span<const ig::StringView>(themeItems, 2));
                ui.spacing(12.0f);

                if (ui.button("Close"))
                    settingsVisible = false;

                ui.endWindow();
            }
            if (!settingsVisible)
                ui.setModalWindow("Settings", false);
        }

        const ig::DrawData &drawData = ui.endFrame();

        BeginDrawing();
        ClearBackground(::Color{13u, 17u, 28u, 255u});
        backend.render(drawData);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
