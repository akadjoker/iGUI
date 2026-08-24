#pragma once

#include <stdint.h>

#include <ct/vector.hpp>

#include "Types.hpp"

namespace ig
{

// The immediate file dialog deliberately does not access the operating system
// itself. Applications provide this small adapter for their platform/sandbox.
enum class FileDialogMode : uint8_t
{
    OpenFile,
    SaveFile,
    ChooseFolder,
    OpenImage
};

enum class FileDialogView : uint8_t
{
    Icons,
    List,
    Details
};

enum class FileDialogSortField : uint8_t
{
    Name,
    Size,
    Modified
};

enum class FileDialogResultKind : uint8_t
{
    None,
    Accepted,
    Cancelled
};

struct FileDialogEntry
{
    String name;
    String path;
    uint64_t size;
    uint64_t modifiedTime;
    bool directory;
    bool hidden;

    FileDialogEntry()
        : name(), path(), size(0u), modifiedTime(0u), directory(false), hidden(false) {}
};

struct FileDialogPreview
{
    TextureId texture;
    float width;
    float height;

    FileDialogPreview() : texture(), width(0.0f), height(0.0f) {}
};

class FileDialogProvider
{
public:
    virtual ~FileDialogProvider() {}

    // Fill entries with the direct children of path. Entry paths must be full
    // paths. Return false when path cannot be read.
    virtual bool listDirectory(StringView path, ct::Vector<FileDialogEntry> &entries) = 0;
    virtual String parentDirectory(StringView path) = 0;
    virtual String homeDirectory() = 0;
    // Create one direct child directory and return its full path. The default
    // keeps read-only or sandboxed providers simple.
    virtual String createDirectory(StringView, StringView) { return String(); }

    // OpenImage asks this only for the selected image. The provider owns the
    // texture and may return an empty preview when it cannot decode the file.
    virtual FileDialogPreview imagePreview(StringView) { return FileDialogPreview(); }
};

struct FileDialogOptions
{
    StringView title;
    StringView initialPath;
    // Semicolon-separated extensions: ".png;.jpg;.jpeg". Empty accepts all.
    StringView filter;
    FileDialogMode mode;
    bool showHidden;

    FileDialogOptions()
        : title("Open File"), initialPath(), filter(), mode(FileDialogMode::OpenFile), showHidden(false) {}
};

struct FileDialogState
{
    String path;
    String selectedPath;
    // Cached directory snapshot. Context refreshes this only after opening,
    // navigating, going up, or creating a folder; never every draw frame.
    ct::Vector<FileDialogEntry> entries;
    ct::Vector<String> backHistory;
    ct::Vector<String> forwardHistory;
    FileDialogView view;
    FileDialogSortField sortField;
    Vec2 position;
    Vec2 size;
    Vec2 resizePointerStart;
    Vec2 resizeSizeStart;
    Vec2 previewPan;
    Vec2 previewPointerStart;
    Vec2 previewPanStart;
    String newFolderName;
    String newFolderError;
    String previewPath;
    float scrollOffset;
    float previewZoom;
    int selectedIndex;
    int lastClickedIndex;
    uint64_t lastClickedFrame;
    String::size_type newFolderCursor;
    bool initialized;
    bool resizing;
    bool creatingFolder;
    bool previewPanning;
    bool sortAscending;

    FileDialogState()
        : path(), selectedPath(), entries(), backHistory(), forwardHistory(), view(FileDialogView::Details),
          sortField(FileDialogSortField::Name), position(), size(),
          resizePointerStart(), resizeSizeStart(), previewPan(), previewPointerStart(), previewPanStart(),
          newFolderName(), newFolderError(), previewPath(), scrollOffset(0.0f), previewZoom(1.0f),
          selectedIndex(-3), lastClickedIndex(-3), lastClickedFrame(0u), newFolderCursor(0u),
          initialized(false), resizing(false), creatingFolder(false), previewPanning(false), sortAscending(true) {}

    // Call reset to discard navigation state manually. Context also resets it
    // after an accepted or cancelled dialog result.
    void reset()
    {
        path.clear();
        selectedPath.clear();
        entries.clear();
        backHistory.clear();
        forwardHistory.clear();
        view = FileDialogView::Details;
        sortField = FileDialogSortField::Name;
        position = Vec2();
        size = Vec2();
        resizePointerStart = Vec2();
        resizeSizeStart = Vec2();
        previewPan = Vec2();
        previewPointerStart = Vec2();
        previewPanStart = Vec2();
        newFolderName.clear();
        newFolderError.clear();
        previewPath.clear();
        scrollOffset = 0.0f;
        previewZoom = 1.0f;
        selectedIndex = -3;
        lastClickedIndex = -3;
        lastClickedFrame = 0u;
        newFolderCursor = 0u;
        initialized = false;
        resizing = false;
        creatingFolder = false;
        previewPanning = false;
        sortAscending = true;
    }
};

struct FileDialogResult
{
    FileDialogResultKind kind;
    String path;

    FileDialogResult() : kind(FileDialogResultKind::None), path() {}
};

} // namespace ig
