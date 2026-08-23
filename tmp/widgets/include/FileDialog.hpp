#pragma once

#include "ViewWidgets.hpp"
#include "DirCache.hpp"
#include "Signal.hpp"
#include "IconAtlas.hpp"

namespace BuGUI
{
class BoxLayout;
class Label;
class TextInput;
class Button;
class ImageButton;

// ═════════════════════════════════════════════════════════════════════════════
//  FileDialog — cross-platform file/folder picker as a FloatWindow popup
//
//    auto* fd = app.addFloat<FileDialog>("Open File");
//    fd->setMode(FileDialog::Mode::Open);
//    fd->setFilter("*.cpp;*.hpp");
//    fd->accepted.connect([](const std::string& path) { ... });
//
//  Features:
//    - Open / Save / SelectFolder / OpenImage modes
//    - Breadcrumb navigation
//    - Sort by name / size / date
//    - Hidden files toggle
//    - Extension filter
//    - Bookmarks sidebar
//    - Detail / List / Icon views
//    - Image preview (via WidgetApp texture service)
//    - DirCache backend for fast browsing
// ═════════════════════════════════════════════════════════════════════════════

class FileDialog : public FloatWindow
{
public:
    enum class Mode { Open, Save, SelectFolder, OpenImage };
    enum class ViewMode { Detail, List, Icon };

    explicit FileDialog(const std::string& title = "Open File");
    ~FileDialog() override;

    /// @brief Set the dialog mode (Open, Save, SelectFolder, OpenImage).
    void setMode(Mode m);
    /// @brief Get the dialog mode.
    Mode mode() const { return mode_; }

    /// @brief Set the file listing view mode.
    void setViewMode(ViewMode vm);
    /// @brief Get the view mode.
    ViewMode viewMode() const { return viewMode_; }

    /// @brief Navigate to a directory path.
    void setPath(const std::string& path);
    /// @brief Get the current directory path.
    const std::string& path() const { return currentPath_; }

    /// @brief Set extension filter (e.g. "*.cpp;*.hpp").
    void setFilter(const std::string& filter);
    /// @brief Get the extension filter.
    const std::string& filter() const { return filter_; }

    /// @brief Show or hide hidden files.
    void setShowHidden(bool show);
    /// @brief Check if hidden files are shown.
    bool showHidden() const { return showHidden_; }

    /// @brief Enable multi-file selection.
    void setMultiSelect(bool ms) { multiSelect_ = ms; }
    /// @brief Check if multi-select is enabled.
    bool multiSelect() const     { return multiSelect_; }

    /// @brief Add a sidebar bookmark entry.
    void addBookmark(const std::string& name, const std::string& path);

    /// @brief Get the selected file/folder path.
    const std::string& selectedPath() const { return selectedPath_; }

    /// @brief Emitted when a file is accepted.
    Signal<std::string> accepted;
    /// @brief Emitted when the dialog is cancelled.
    Signal<>            cancelled;

    // ── Widget overrides ─────────────────────────────────────────────────
    void layout() override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onKeyPress(KeyEvent& e) override;

private:
    Mode     mode_     = Mode::Open;
    ViewMode viewMode_ = ViewMode::Detail;

    std::string currentPath_;
    std::string selectedPath_;
    std::string filter_;
    bool showHidden_  = false;
    bool multiSelect_ = false;

    // Bookmarks
    struct Bookmark { std::string name; std::string path; };
    std::vector<Bookmark> bookmarks_;

    // Sort
    enum class SortField { Name, Size, Date };
    SortField sortField_    = SortField::Name;
    bool      sortAscending_ = true;

    // Filter
    std::vector<std::string> filterExts_;
    void parseFilter();
    bool matchesFilter(const std::string& name) const;
    static bool isImageExt(const std::string& ext);

    // Directory listing (flat, sorted, filtered)
    struct FileEntry {
        std::string name;
        std::string path;
        uint64_t    size  = 0;
        time_t      mtime = 0;
        IconId      icon  = IconId::None;
        bool        isDir = false;
    };
    std::vector<FileEntry> entries_;
    DirCache dirCache_;

    void refreshDir();
    void sortEntries();
    void navigateTo(const std::string& path);

    // Selection
    int  selectedIndex_ = -1;
    int  hoveredIndex_  = -1;
    std::vector<bool> multiSel_;   // parallel to entries_
    int  lastClickIdx_  = -1;

    // Preview (image mode)
    BuGUI::TextureHandle previewTex_ = {0};
    int                  previewW_   = 0;
    int                  previewH_   = 0;
    std::string          previewPath_;
    void loadPreview(const std::string& path);
    void clearPreview();

    // ── UI child widgets (not owned — children_ owns them) ──────────────
    BoxLayout* mainLayout_      = nullptr;
    BoxLayout* toolBar_         = nullptr;
    BoxLayout* breadcrumbBar_   = nullptr;
    BoxLayout* bodyLayout_      = nullptr;
    BoxLayout* sidebarLayout_   = nullptr;
    BoxLayout* fileListLayout_  = nullptr;
    Widget*    previewPanel_    = nullptr;
    BoxLayout* bottomBar_       = nullptr;
    TextInput* fileNameInput_   = nullptr;
    Label*     statusLabel_     = nullptr;
    Button*    okButton_        = nullptr;
    Button*    cancelButton_    = nullptr;

    // Toolbar icon buttons (lazily bound to atlas in paint)
    ImageButton* btnViewDetail_ = nullptr;
    ImageButton* btnViewList_   = nullptr;
    ImageButton* btnViewGrid_   = nullptr;
    ImageButton* btnHidden_     = nullptr;
    bool         iconsBound_    = false;
    void         bindToolbarIcons(IconAtlas* atlas);
    void         updateViewModeButtons();

    // File list state
    float scrollY_      = 0.f;
    float rowHeight_    = 22.f;
    float iconCellSize_ = 80.f;

    // Deferred nav
    std::string pendingNav_;

    // Column widths (detail view)
    struct Column { const char* name; float width; SortField field; };
    static constexpr int kColCount = 3;
    Column columns_[kColCount] = {
        {"Name", 300, SortField::Name},
        {"Size",  90, SortField::Size},
        {"Date", 150, SortField::Date},
    };

    // Build
    void buildUI();
    void buildBreadcrumbs();
    void buildSidebar();

    // Paint helpers
    void paintFileArea(PaintContext& ctx, const Rect& area);
    void paintHeader(PaintContext& ctx, const Rect& area);
    void paintDetail(PaintContext& ctx, const Rect& area);
    void paintCompact(PaintContext& ctx, const Rect& area);
    void paintIcons(PaintContext& ctx, const Rect& area);
    void paintPreview(PaintContext& ctx, const Rect& area);

    // Hit test
    int hitIndex(float mx, float my, const Rect& area) const;

    // Actions
    void onOk();
    void onCancel();

    // Icon mapping
    static IconId iconForExt(const std::string& ext);
};

} // namespace BuGUI
