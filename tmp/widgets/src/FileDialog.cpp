#include "FileDialog.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "TextInputWidgets.hpp"
#include "WidgetApp.hpp"
#include "BuImage.hpp"

// ═════════════════════════════════════════════════════════════════════════════
//  Helpers
// ═════════════════════════════════════════════════════════════════════════════

namespace {
    inline float setupFont(PaintContext& ctx, const Color& color, float size)
    {
        ctx.font.SetFontSize(size);
        ctx.font.SetBatch(&ctx.text);
        ctx.font.SetColor(color);
        return ctx.font.GetAscender();
    }

    inline std::string toLowerStr(const std::string& s)
    {
        std::string r = s;
        for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return r;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Constructor / Destructor
// ═════════════════════════════════════════════════════════════════════════════

FileDialog::FileDialog(const std::string& title)
    : FloatWindow(title)
{
    setFloatSize(700, 480);
    setResizable(true);
    setMinimizable(false);

    bookmarks_.push_back({"Home", FileSystem::homePath()});
#if !defined(_WIN32)
    bookmarks_.push_back({"/", "/"});
#endif

    currentPath_ = FileSystem::homePath();
    dirCache_.setRoot(currentPath_);

    buildUI();
    refreshDir();
}

FileDialog::~FileDialog()
{
    clearPreview();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Configuration
// ═════════════════════════════════════════════════════════════════════════════

void FileDialog::setMode(Mode m)
{
    mode_ = m;
    if (okButton_) {
        switch (m) {
        case Mode::Open:         okButton_->setText("Open");   break;
        case Mode::Save:         okButton_->setText("Save");   break;
        case Mode::SelectFolder: okButton_->setText("Select"); break;
        case Mode::OpenImage:    okButton_->setText("Open");   break;
        }
    }
    bool hideFile = (m == Mode::SelectFolder);
    if (fileNameInput_) fileNameInput_->setVisible(!hideFile);

    bool imageMode = (m == Mode::OpenImage);
    if (previewPanel_) previewPanel_->setVisible(imageMode);
    if (imageMode) {
        setFloatSize(850, 520);
        if (filter_.empty()) setFilter("*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.gif");
        if (viewMode_ == ViewMode::Detail) setViewMode(ViewMode::Icon);
    }
    refreshDir();
}

void FileDialog::setViewMode(ViewMode vm)
{
    viewMode_ = vm;
    scrollY_ = 0;
    updateViewModeButtons();
    markDirty();
}

void FileDialog::setPath(const std::string& path) { navigateTo(path); }

void FileDialog::setFilter(const std::string& f)
{
    filter_ = f;
    parseFilter();
    refreshDir();
}

void FileDialog::setShowHidden(bool show)
{
    showHidden_ = show;
    dirCache_.setShowHidden(show);
    dirCache_.clearCache();
    dirCache_.setRoot(currentPath_);
    // Update hidden toggle icon
    if (iconsBound_ && btnHidden_) {
        auto& atlas = WidgetApp::instance().iconAtlas();
        if (atlas.ready())
            btnHidden_->setSrcRect(atlas.srcRect(show ? IconId::Eye : IconId::EyeOff));
    }
    refreshDir();
}

void FileDialog::addBookmark(const std::string& name, const std::string& path)
{
    bookmarks_.push_back({name, path});
    buildSidebar();
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Filter
// ═════════════════════════════════════════════════════════════════════════════

void FileDialog::parseFilter()
{
    filterExts_.clear();
    if (filter_.empty()) return;
    size_t pos = 0;
    while (pos < filter_.size()) {
        size_t semi = filter_.find(';', pos);
        if (semi == std::string::npos) semi = filter_.size();
        std::string tok = filter_.substr(pos, semi - pos);
        if (tok.size() >= 2 && tok[0] == '*' && tok[1] == '.') tok = tok.substr(1);
        if (!tok.empty()) filterExts_.push_back(tok);
        pos = semi + 1;
    }
}

bool FileDialog::matchesFilter(const std::string& name) const
{
    if (filterExts_.empty()) return true;
    std::string ext = toLowerStr(FileSystem::extension(name));
    for (auto& fe : filterExts_) {
        if (ext == toLowerStr(fe)) return true;
    }
    return false;
}

bool FileDialog::isImageExt(const std::string& ext)
{
    std::string e = toLowerStr(ext);
    return e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".bmp" ||
           e == ".tga" || e == ".gif" || e == ".hdr";
}

// ═════════════════════════════════════════════════════════════════════════════
//  Directory listing — uses DirCache
// ═════════════════════════════════════════════════════════════════════════════

void FileDialog::refreshDir()
{
    entries_.clear();
    selectedIndex_ = -1;
    hoveredIndex_ = -1;
    multiSel_.clear();
    scrollY_ = 0;

    // Navigate DirCache to current path
    DirCache::DirNode* node = dirCache_.navigate(currentPath_);
    if (!node) {
        // Path changed outside root — reset cache
        dirCache_.clearCache();
        dirCache_.setRoot(currentPath_);
        node = dirCache_.root();
    }
    if (!node) { buildBreadcrumbs(); markDirty(); return; }

    if (!node->scanned) dirCache_.expand(node);

    for (int i = 0; i < node->kidCount; ++i) {
        auto* kid = node->kids[i];
        if (!showHidden_ && kid->hidden) continue;
        if (!kid->isDir && mode_ == Mode::SelectFolder) continue;
        if (!kid->isDir && !matchesFilter(kid->name)) continue;

        FileEntry fe;
        fe.name  = kid->name;
        fe.path  = kid->path;
        fe.isDir = kid->isDir;
        fe.size  = kid->size;
        fe.mtime = kid->mtime;
        fe.icon  = kid->isDir ? IconId::Folder : iconForExt(kid->ext);
        entries_.push_back(fe);
    }

    sortEntries();
    multiSel_.resize(entries_.size(), false);
    buildBreadcrumbs();
    clearPreview();
    markDirty();
}

void FileDialog::sortEntries()
{
    auto cmp = [this](const FileEntry& a, const FileEntry& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        switch (sortField_) {
        case SortField::Size:
            return sortAscending_ ? a.size < b.size : a.size > b.size;
        case SortField::Date:
            return sortAscending_ ? a.mtime < b.mtime : a.mtime > b.mtime;
        case SortField::Name:
        default: {
            std::string na = toLowerStr(a.name), nb = toLowerStr(b.name);
            return sortAscending_ ? na < nb : na > nb;
        }
        }
    };
    std::stable_sort(entries_.begin(), entries_.end(), cmp);
}

void FileDialog::navigateTo(const std::string& path)
{
    if (!FileSystem::isDir(path)) return;
    pendingNav_ = FileSystem::normalizePath(path);
    markDirty();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Preview
// ═════════════════════════════════════════════════════════════════════════════

void FileDialog::loadPreview(const std::string& path)
{
    if (path == previewPath_) return;
    clearPreview();
    previewPath_ = path;
    if (!isImageExt(FileSystem::extension(path))) return;

    auto& app = WidgetApp::instance();
    previewTex_ = app.loadImageTexture(path.c_str(), previewW_, previewH_);
    markDirty();
}

void FileDialog::clearPreview()
{
    if (previewTex_) {
        WidgetApp::instance().destroyTexture(previewTex_);
        previewTex_ = {0};
    }
    previewW_ = previewH_ = 0;
    previewPath_.clear();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Icon mapping
// ═════════════════════════════════════════════════════════════════════════════

IconId FileDialog::iconForExt(const std::string& ext)
{
    if (ext == ".cpp" || ext == ".c" || ext == ".hpp" || ext == ".h" ||
        ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".java" ||
        ext == ".rs" || ext == ".go" || ext == ".lua")
        return IconId::FileCode;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
        ext == ".tga" || ext == ".gif" || ext == ".svg" || ext == ".hdr" || ext == ".webp")
        return IconId::FileImage;
    if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".bz2" ||
        ext == ".xz" || ext == ".7z" || ext == ".rar")
        return IconId::FileArchive;
    return IconId::File;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Bind toolbar ImageButtons to icon atlas (called once when atlas available)
// ═════════════════════════════════════════════════════════════════════════════

void FileDialog::bindToolbarIcons(IconAtlas* atlas)
{
    if (!atlas || !atlas->ready()) return;
    iconsBound_ = true;

    auto tex = atlas->texture();
    int tw = atlas->textureWidth();
    int th = atlas->textureHeight();

    auto bind = [&](ImageButton* btn, IconId id) {
        if (!btn) return;
        btn->setTexture(tex, tw, th);
        btn->setSrcRect(atlas->srcRect(id));
    };

    bind(btnViewDetail_, IconId::ViewDetail);
    bind(btnViewList_,   IconId::ViewList);
    bind(btnViewGrid_,   IconId::ViewGrid);
    bind(btnHidden_,     showHidden_ ? IconId::Eye : IconId::EyeOff);

    updateViewModeButtons();
}

void FileDialog::updateViewModeButtons()
{
    Color active(60, 100, 160, 255);
    Color normal(0, 0, 0, 0);  // transparent = use theme default
    if (btnViewDetail_) btnViewDetail_->setBgColor(viewMode_ == ViewMode::Detail ? active : normal);
    if (btnViewList_)   btnViewList_->setBgColor(viewMode_ == ViewMode::List   ? active : normal);
    if (btnViewGrid_)   btnViewGrid_->setBgColor(viewMode_ == ViewMode::Icon   ? active : normal);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Build UI
// ═════════════════════════════════════════════════════════════════════════════

void FileDialog::buildUI()
{
    mainLayout_ = setContent<BoxLayout>(LayoutDir::Vertical);
    mainLayout_->setSpacing(4);
    mainLayout_->setPadding(4);

    // ── Toolbar: breadcrumbs ─────────────────────────────────────────────
    toolBar_ = mainLayout_->createChild<BoxLayout>(LayoutDir::Horizontal);
    toolBar_->setSize(0, 26);
    toolBar_->setSpacing(2);

    breadcrumbBar_ = toolBar_->createChild<BoxLayout>(LayoutDir::Horizontal);
    breadcrumbBar_->setStretch(1);
    breadcrumbBar_->setSpacing(2);

    btnViewDetail_ = toolBar_->createChild<ImageButton>();
    btnViewDetail_->setSize(24, 22); btnViewDetail_->setTooltip("Detail view");
    btnViewDetail_->clicked.connect([this] { setViewMode(ViewMode::Detail); });

    btnViewList_ = toolBar_->createChild<ImageButton>();
    btnViewList_->setSize(24, 22); btnViewList_->setTooltip("List view");
    btnViewList_->clicked.connect([this] { setViewMode(ViewMode::List); });

    btnViewGrid_ = toolBar_->createChild<ImageButton>();
    btnViewGrid_->setSize(24, 22); btnViewGrid_->setTooltip("Grid view");
    btnViewGrid_->clicked.connect([this] { setViewMode(ViewMode::Icon); });

    btnHidden_ = toolBar_->createChild<ImageButton>();
    btnHidden_->setSize(24, 22); btnHidden_->setTooltip("Toggle hidden files");
    btnHidden_->clicked.connect([this] { setShowHidden(!showHidden_); });

    // ── Body: sidebar + file list + preview ──────────────────────────────
    bodyLayout_ = mainLayout_->createChild<BoxLayout>(LayoutDir::Horizontal);
    bodyLayout_->setStretch(1);
    bodyLayout_->setSpacing(4);

    sidebarLayout_ = bodyLayout_->createChild<BoxLayout>(LayoutDir::Vertical);
    sidebarLayout_->setSize(120, 0);
    sidebarLayout_->setSpacing(2);
    sidebarLayout_->setPadding(2);
    buildSidebar();

    fileListLayout_ = bodyLayout_->createChild<BoxLayout>(LayoutDir::Vertical);
    fileListLayout_->setStretch(1);

    previewPanel_ = bodyLayout_->createChild<Widget>();
    previewPanel_->setSize(180, 0);
    previewPanel_->setVisible(false);

    // ── Bottom bar ───────────────────────────────────────────────────────
    bottomBar_ = mainLayout_->createChild<BoxLayout>(LayoutDir::Horizontal);
    bottomBar_->setSize(0, 30);
    bottomBar_->setSpacing(6);

    auto* fileLabel = bottomBar_->createChild<Label>("File:");
    fileLabel->setSize(30, 0);

    fileNameInput_ = bottomBar_->createChild<TextInput>();
    fileNameInput_->setStretch(1);
    fileNameInput_->setPlaceholder("filename");

    cancelButton_ = bottomBar_->createChild<Button>("Cancel");
    cancelButton_->setSize(70, 0);
    cancelButton_->clicked.connect([this] { onCancel(); });

    okButton_ = bottomBar_->createChild<Button>("Open");
    okButton_->setSize(70, 0);
    okButton_->clicked.connect([this] { onOk(); });

    // Status
    statusLabel_ = mainLayout_->createChild<Label>("");
    statusLabel_->setSize(0, 16);
}

void FileDialog::buildBreadcrumbs()
{
    while (!breadcrumbBar_->children().empty())
        breadcrumbBar_->removeChild(breadcrumbBar_->children().back());

    std::vector<std::pair<std::string, std::string>> parts;
#if defined(_WIN32)
    parts.push_back({"C:", "C:\\"});
    std::string accum = "C:";
#else
    parts.push_back({"/", "/"});
    std::string accum;
#endif
    size_t start = 1;
    for (size_t i = start; i <= currentPath_.size(); ++i) {
        if (i == currentPath_.size() || currentPath_[i] == '/') {
            std::string seg = currentPath_.substr(start, i - start);
            if (!seg.empty()) {
                accum += "/" + seg;
                parts.push_back({seg, accum});
            }
            start = i + 1;
        }
    }

    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            auto* sep = breadcrumbBar_->createChild<Label>(">");
            sep->setSize(12, 0);
        }
        std::string target = parts[i].second;
        auto* btn = breadcrumbBar_->createChild<Button>(parts[i].first);
        btn->setSize(0, 22);
        btn->clicked.connect([this, target] { navigateTo(target); });
    }
    markDirty();
}

void FileDialog::buildSidebar()
{
    while (!sidebarLayout_->children().empty())
        sidebarLayout_->removeChild(sidebarLayout_->children().back());

    auto* bookLabel = sidebarLayout_->createChild<Label>("Bookmarks");
    (void)bookLabel;

    for (size_t i = 0; i < bookmarks_.size(); ++i) {
        std::string target = bookmarks_[i].path;
        auto* btn = sidebarLayout_->createChild<Button>(bookmarks_[i].name);
        btn->setSize(0, 22);
        btn->clicked.connect([this, target] { navigateTo(target); });
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Layout
// ═════════════════════════════════════════════════════════════════════════════

void FileDialog::layout()
{
    if (!pendingNav_.empty()) {
        std::string nav = std::move(pendingNav_);
        pendingNav_.clear();
        currentPath_ = nav;
        // Update DirCache root if path outside current root
        if (nav.find(dirCache_.rootPath()) != 0 || dirCache_.rootPath().empty()) {
            dirCache_.clearCache();
            // Set root to parent of target for broader caching
            std::string rootDir = FileSystem::parentPath(nav);
            if (rootDir.empty() || rootDir == nav) rootDir = nav;
            dirCache_.setRoot(rootDir);
        }
        refreshDir();
    }

    FloatWindow::layout();

    if (statusLabel_) {
        int dirs = 0, files = 0;
        for (auto& e : entries_) { if (e.isDir) ++dirs; else ++files; }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d folders, %d files", dirs, files);
        statusLabel_->setText(buf);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Paint
// ═════════════════════════════════════════════════════════════════════════════

void FileDialog::paint(PaintContext& ctx)
{
    // Lazily bind toolbar icons when atlas becomes available
    if (!iconsBound_ && ctx.icons && ctx.icons->ready())
        bindToolbarIcons(ctx.icons);

    FloatWindow::paint(ctx);

    if (!fileListLayout_) return;
    Rect area = fileListLayout_->absoluteRect();
    if (area.w < 10 || area.h < 10) return;

    paintFileArea(ctx, area);

    // Sidebar separator
    if (sidebarLayout_) {
        Rect sr = sidebarLayout_->absoluteRect();
        ctx.line.SetColor(60, 62, 66, 255);
        ctx.drawLine(sr.x + sr.w, sr.y, sr.x + sr.w, sr.y + sr.h);
    }

    // Preview
    if (mode_ == Mode::OpenImage && previewPanel_ && previewPanel_->isVisible()) {
        Rect pr = previewPanel_->absoluteRect();
        auto& t = Theme::instance();
        ctx.fill.SetColor(t.panelColor.r + 15, t.panelColor.g + 15, t.panelColor.b + 15, 255);
        ctx.fillRect(pr.x, pr.y, pr.w, pr.h);
        ctx.line.SetColor(60, 62, 66, 255);
        ctx.drawLine(pr.x, pr.y, pr.x, pr.y + pr.h);
        paintPreview(ctx, pr);
    }
}

void FileDialog::paintFileArea(PaintContext& ctx, const Rect& area)
{
    auto& t = Theme::instance();
    ctx.fill.SetColor(t.panelColor.r - 5, t.panelColor.g - 5, t.panelColor.b - 5, 255);
    ctx.fillRect(area.x, area.y, area.w, area.h);

    switch (viewMode_) {
    case ViewMode::Detail: {
        float hdrH = 22.f;
        paintHeader(ctx, {area.x, area.y, area.w, hdrH});
        Rect body = {area.x, area.y + hdrH, area.w, area.h - hdrH};
        ctx.pushClip(body);
        paintDetail(ctx, body);
        ctx.popClip();
        break;
    }
    case ViewMode::List:
        ctx.pushClip(area);
        paintCompact(ctx, area);
        ctx.popClip();
        break;
    case ViewMode::Icon:
        ctx.pushClip(area);
        paintIcons(ctx, area);
        ctx.popClip();
        break;
    }

    ctx.line.SetColor(55, 58, 65, 255);
    ctx.lineRect(area.x, area.y, area.w, area.h);
}

void FileDialog::paintHeader(PaintContext& ctx, const Rect& area)
{
    auto& t = Theme::instance();
    ctx.fill.SetColor(t.panelColor.r + 10, t.panelColor.g + 10, t.panelColor.b + 10, 255);
    ctx.fillRect(area.x, area.y, area.w, area.h);

    float asc = setupFont(ctx, t.textColor, t.fontSize * 0.9f);
    float x = area.x;
    for (int c = 0; c < kColCount; ++c) {
        ctx.font.Print(columns_[c].name, x + 4, area.y + (area.h - t.fontSize * 0.9f) * 0.5f + asc);
        if (sortField_ == columns_[c].field) {
            float ax = x + columns_[c].width - 14;
            float ay = area.y + area.h * 0.5f;
            ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, t.textColor.a);
            if (sortAscending_)
                ctx.fillTriangle(ax, ay + 3, ax + 6, ay + 3, ax + 3, ay - 3);
            else
                ctx.fillTriangle(ax, ay - 3, ax + 6, ay - 3, ax + 3, ay + 3);
        }
        x += columns_[c].width;
        ctx.line.SetColor(55, 58, 65, 60);
        ctx.drawLine(x, area.y + 2, x, area.y + area.h - 2);
    }
    ctx.line.SetColor(55, 58, 65, 255);
    ctx.drawLine(area.x, area.y + area.h, area.x + area.w, area.y + area.h);
}

void FileDialog::paintDetail(PaintContext& ctx, const Rect& area)
{
    auto& t = Theme::instance();
    int total = static_cast<int>(entries_.size());
    int first = static_cast<int>(scrollY_ / rowHeight_);
    int last  = first + static_cast<int>(area.h / rowHeight_) + 2;
    if (first < 0) first = 0;
    if (last > total) last = total;

    float asc = setupFont(ctx, t.textColor, t.fontSize * 0.9f);

    for (int i = first; i < last; ++i) {
        auto& fe = entries_[i];
        float ry = area.y + i * rowHeight_ - scrollY_;
        if (ry + rowHeight_ < area.y || ry > area.y + area.h) continue;

        // Zebra
        if (i % 2 == 1) {
            ctx.fill.SetColor(t.panelColor.r + 3, t.panelColor.g + 3, t.panelColor.b + 3, 255);
            ctx.fillRect(area.x, ry, area.w, rowHeight_);
        }
        // Selection
        bool sel = (i == selectedIndex_) || (multiSelect_ && i < static_cast<int>(multiSel_.size()) && multiSel_[i]);
        if (sel) {
            ctx.fill.SetColor(40, 70, 120, 200);
            ctx.fillRect(area.x, ry, area.w, rowHeight_);
        } else if (i == hoveredIndex_) {
            ctx.fill.SetColor(50, 55, 65, 100);
            ctx.fillRect(area.x, ry, area.w, rowHeight_);
        }

        float textY = ry + (rowHeight_ - t.fontSize * 0.9f) * 0.5f + asc;
        float cx = area.x;

        // Icon + Name
        float nameW = columns_[0].width;
        if (fe.icon != IconId::None) {
            float isz = rowHeight_ - 6;
            ctx.drawIcon(fe.icon, cx + 2, ry + 3, isz);
            cx += isz + 4;
        } else cx += 4;

        ctx.font.SetColor(fe.isDir ? Color(120, 180, 255, 255) :
                           (sel ? Color(255, 255, 255, 255) : t.textColor));
        ctx.font.Print(fe.name.c_str(), cx, textY);
        cx = area.x + nameW;

        // Size
        if (!fe.isDir) {
            ctx.font.SetColor(Color(160, 160, 165, 255));
            ctx.font.Print(FileSystem::humanSize(fe.size).c_str(), cx + 4, textY);
        }
        cx += columns_[1].width;

        // Date
        ctx.font.SetColor(Color(140, 140, 145, 255));
        ctx.font.Print(FileSystem::humanDate(fe.mtime).c_str(), cx + 4, textY);
    }

    // Scrollbar
    float contentH = total * rowHeight_;
    if (contentH > area.h) {
        float sbW = 6, sbX = area.x + area.w - sbW;
        float thumbH = std::max(20.f, (area.h / contentH) * area.h);
        float maxSY = contentH - area.h;
        float thumbY = area.y + (maxSY > 0 ? (scrollY_ / maxSY) * (area.h - thumbH) : 0);
        ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, 60);
        ctx.fillRect(sbX + 1, thumbY, sbW - 2, thumbH);
    }
}

void FileDialog::paintCompact(PaintContext& ctx, const Rect& area)
{
    auto& t = Theme::instance();
    float rh = rowHeight_;
    float colW = 200.f;
    int rowsPerCol = std::max(1, static_cast<int>(area.h / rh));
    int total = static_cast<int>(entries_.size());

    float asc = setupFont(ctx, t.textColor, t.fontSize * 0.9f);

    for (int i = 0; i < total; ++i) {
        int col = i / rowsPerCol;
        int row = i % rowsPerCol;
        float rx = area.x + col * colW - scrollY_;
        float ry = area.y + row * rh;
        if (rx + colW < area.x || rx > area.x + area.w) continue;

        auto& fe = entries_[i];
        bool sel = (i == selectedIndex_) || (multiSelect_ && i < static_cast<int>(multiSel_.size()) && multiSel_[i]);
        if (sel) {
            ctx.fill.SetColor(40, 70, 120, 200);
            ctx.fillRect(rx, ry, colW, rh);
        } else if (i == hoveredIndex_) {
            ctx.fill.SetColor(50, 55, 65, 100);
            ctx.fillRect(rx, ry, colW, rh);
        }

        float cx = rx;
        if (fe.icon != IconId::None) {
            float isz = rh - 6;
            ctx.drawIcon(fe.icon, cx + 2, ry + 3, isz);
            cx += isz + 4;
        } else cx += 4;

        float textY = ry + (rh - t.fontSize * 0.9f) * 0.5f + asc;
        ctx.font.SetColor(fe.isDir ? Color(120, 180, 255, 255) : t.textColor);
        ctx.font.Print(fe.name.c_str(), cx, textY);
    }
}

void FileDialog::paintIcons(PaintContext& ctx, const Rect& area)
{
    auto& t = Theme::instance();
    float cell = iconCellSize_;
    int cols = std::max(1, static_cast<int>(area.w / cell));
    int total = static_cast<int>(entries_.size());

    float asc = setupFont(ctx, t.textColor, t.fontSize * 0.8f);

    for (int i = 0; i < total; ++i) {
        int c = i % cols;
        int r = i / cols;
        float cx = area.x + c * cell;
        float cy = area.y + r * cell - scrollY_;
        if (cy + cell < area.y || cy > area.y + area.h) continue;

        auto& fe = entries_[i];
        bool sel = (i == selectedIndex_) || (multiSelect_ && i < static_cast<int>(multiSel_.size()) && multiSel_[i]);
        if (sel) {
            ctx.fill.SetColor(40, 70, 120, 200);
            ctx.fillRect(cx + 2, cy + 2, cell - 4, cell - 4);
        } else if (i == hoveredIndex_) {
            ctx.fill.SetColor(50, 55, 65, 80);
            ctx.fillRect(cx + 2, cy + 2, cell - 4, cell - 4);
        }

        float isz = cell * 0.45f;
        float ix = cx + (cell - isz) * 0.5f;
        float iy = cy + 6;
        if (fe.icon != IconId::None) ctx.drawIcon(fe.icon, ix, iy, isz);

        // Truncated label
        std::string disp = fe.name;
        if (disp.size() > 12) disp = disp.substr(0, 10) + "..";
        float tw = ctx.font.GetTextWidth(disp.c_str());
        float tx = cx + (cell - tw) * 0.5f;
        float ty = cy + cell - 20 + asc;
        ctx.font.SetColor(fe.isDir ? Color(120, 180, 255, 255) : t.textColor);
        ctx.font.Print(disp.c_str(), tx, ty);
    }

    // Scrollbar
    int totalRows = (total + cols - 1) / cols;
    float contentH = totalRows * cell;
    if (contentH > area.h) {
        float sbW = 6, sbX = area.x + area.w - sbW;
        float thumbH = std::max(20.f, (area.h / contentH) * area.h);
        float maxSY = contentH - area.h;
        float thumbY = area.y + (maxSY > 0 ? (scrollY_ / maxSY) * (area.h - thumbH) : 0);
        ctx.fill.SetColor(t.textColor.r, t.textColor.g, t.textColor.b, 60);
        ctx.fillRect(sbX + 1, thumbY, sbW - 2, thumbH);
    }
}

void FileDialog::paintPreview(PaintContext& ctx, const Rect& area)
{
    auto& t = Theme::instance();
    if (!previewTex_) {
        float asc = setupFont(ctx, Color(100, 100, 110, 255), t.fontSize * 0.8f);
        ctx.font.Print("No preview", area.x + 10, area.y + area.h * 0.5f + asc);
        return;
    }

    float imgW = static_cast<float>(previewW_);
    float imgH = static_cast<float>(previewH_);
    float maxW = area.w - 8, maxH = area.h - 30;
    float scale = std::min(maxW / imgW, maxH / imgH);
    if (scale > 1.f) scale = 1.f;
    float dw = imgW * scale, dh = imgH * scale;
    float dx = area.x + (area.w - dw) * 0.5f;
    float dy = area.y + 4;

    ctx.drawImage(previewTex_, {dx, dy, dw, dh});

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%dx%d", previewW_, previewH_);
    float asc = setupFont(ctx, Color(130, 130, 140, 255), t.fontSize * 0.75f);
    ctx.font.Print(buf, area.x + 6, dy + dh + 4 + asc);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Hit test
// ═════════════════════════════════════════════════════════════════════════════

int FileDialog::hitIndex(float mx, float my, const Rect& area) const
{
    int total = static_cast<int>(entries_.size());
    switch (viewMode_) {
    case ViewMode::Detail: {
        float hdrH = 22.f;
        float localY = my - (area.y + hdrH) + scrollY_;
        return static_cast<int>(localY / rowHeight_);
    }
    case ViewMode::List: {
        int rowsPerCol = std::max(1, static_cast<int>(area.h / rowHeight_));
        float colW = 200.f;
        int col = static_cast<int>((mx - area.x + scrollY_) / colW);
        int row = static_cast<int>((my - area.y) / rowHeight_);
        int idx = col * rowsPerCol + row;
        return (idx >= 0 && idx < total) ? idx : -1;
    }
    case ViewMode::Icon: {
        int cols = std::max(1, static_cast<int>(area.w / iconCellSize_));
        int col = static_cast<int>((mx - area.x) / iconCellSize_);
        int row = static_cast<int>((my - area.y + scrollY_) / iconCellSize_);
        int idx = row * cols + col;
        return (idx >= 0 && idx < total) ? idx : -1;
    }
    }
    return -1;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Mouse events
// ═════════════════════════════════════════════════════════════════════════════

void FileDialog::onMousePress(MouseEvent& e)
{
    if (!fileListLayout_) { FloatWindow::onMousePress(e); return; }

    Rect area = fileListLayout_->absoluteRect();

    // Header click → sort (detail mode)
    if (viewMode_ == ViewMode::Detail) {
        Rect hdr = {area.x, area.y, area.w, 22.f};
        if (hdr.contains(e.x, e.y)) {
            e.consumed = true;
            float cx = area.x;
            for (int c = 0; c < kColCount; ++c) {
                if (e.x >= cx && e.x < cx + columns_[c].width) {
                    if (sortField_ == columns_[c].field) sortAscending_ = !sortAscending_;
                    else { sortField_ = columns_[c].field; sortAscending_ = true; }
                    sortEntries(); markDirty();
                    return;
                }
                cx += columns_[c].width;
            }
            return;
        }
    }

    // Body click
    bool inBody = false;
    if (viewMode_ == ViewMode::Detail) {
        Rect body = {area.x, area.y + 22.f, area.w, area.h - 22.f};
        inBody = body.contains(e.x, e.y);
    } else {
        inBody = area.contains(e.x, e.y);
    }

    if (inBody && e.button == 0) {
        e.consumed = true;
        int total = static_cast<int>(entries_.size());
        int idx = hitIndex(e.x, e.y, area);
        if (idx < 0 || idx >= total) return;

        // Modifiers from IO
        auto& io = BuGUI::GetIO();
        bool ctrl  = io.keyCtrl;
        bool shift = io.keyShift;

        // Double-click detection (simple: same index within 400ms)
        static float lastClickTime = 0;
        static int   lastIdx = -1;
        float now = lastClickTime + WidgetApp::instance().deltaTime();
        // Simple: track via frame-accumulated time
        bool dblClick = (idx == lastIdx && (now - lastClickTime) < 0.4f);
        lastClickTime = now;
        lastIdx = idx;

        // Selection
        if (multiSelect_ && ctrl) {
            if (idx < static_cast<int>(multiSel_.size()))
                multiSel_[idx] = !multiSel_[idx];
            lastClickIdx_ = idx;
            selectedIndex_ = idx;
        } else if (multiSelect_ && shift && lastClickIdx_ >= 0) {
            std::fill(multiSel_.begin(), multiSel_.end(), false);
            int lo = std::min(lastClickIdx_, idx), hi = std::max(lastClickIdx_, idx);
            for (int i = lo; i <= hi; ++i)
                if (i < static_cast<int>(multiSel_.size())) multiSel_[i] = true;
            selectedIndex_ = idx;
        } else {
            std::fill(multiSel_.begin(), multiSel_.end(), false);
            selectedIndex_ = idx;
            if (idx < static_cast<int>(multiSel_.size())) multiSel_[idx] = true;
            lastClickIdx_ = idx;
        }

        auto& fe = entries_[idx];
        if (fileNameInput_ && !fe.isDir)
            fileNameInput_->setText(fe.name);

        if (mode_ == Mode::OpenImage && !fe.isDir)
            loadPreview(fe.path);

        // Double-click action
        if (dblClick) {
            if (fe.isDir) {
                navigateTo(fe.path);
            } else {
                selectedPath_ = fe.path;
                accepted.emit(selectedPath_);
                WidgetApp::instance().removeFloat(this);
            }
        }
        markDirty();
        return;
    }

    FloatWindow::onMousePress(e);
}

void FileDialog::onMouseMove(MouseEvent& e)
{
    if (fileListLayout_) {
        Rect area = fileListLayout_->absoluteRect();
        bool inBody = false;
        if (viewMode_ == ViewMode::Detail) {
            Rect body = {area.x, area.y + 22.f, area.w, area.h - 22.f};
            inBody = body.contains(e.x, e.y);
        } else {
            inBody = area.contains(e.x, e.y);
        }

        if (inBody) {
            int idx = hitIndex(e.x, e.y, area);
            if (idx >= 0 && idx < static_cast<int>(entries_.size()) && hoveredIndex_ != idx) {
                hoveredIndex_ = idx;
                markDirty();
            }
        } else if (hoveredIndex_ >= 0) {
            hoveredIndex_ = -1;
            markDirty();
        }
    }
    FloatWindow::onMouseMove(e);
}

void FileDialog::onMouseRelease(MouseEvent& e) { FloatWindow::onMouseRelease(e); }

void FileDialog::onMouseScroll(MouseEvent& e)
{
    if (fileListLayout_) {
        Rect area = fileListLayout_->absoluteRect();
        if (area.contains(e.x, e.y)) {
            e.consumed = true;
            float step = (viewMode_ == ViewMode::Icon) ? iconCellSize_ : rowHeight_ * 3.f;
            scrollY_ -= e.scrollY * step;

            // Clamp
            int total = static_cast<int>(entries_.size());
            float contentH = 0;
            if (viewMode_ == ViewMode::Icon) {
                int cols = std::max(1, static_cast<int>(area.w / iconCellSize_));
                contentH = ((total + cols - 1) / cols) * iconCellSize_;
            } else if (viewMode_ == ViewMode::List) {
                int rowsPerCol = std::max(1, static_cast<int>(area.h / rowHeight_));
                contentH = ((total + rowsPerCol - 1) / rowsPerCol) * 200.f; // horizontal scroll
            } else {
                contentH = total * rowHeight_;
                float bodyH = area.h - 22.f;
                scrollY_ = std::clamp(scrollY_, 0.f, std::max(0.f, contentH - bodyH));
                markDirty();
                return;
            }
            scrollY_ = std::clamp(scrollY_, 0.f, std::max(0.f, contentH - area.h));
            markDirty();
            return;
        }
    }
    FloatWindow::onMouseScroll(e);
}

void FileDialog::onKeyPress(KeyEvent& e)
{
    // Escape → cancel
    if (e.key == 27) { e.consumed = true; onCancel(); return; }
    // Enter → ok
    if (e.key == 13) { e.consumed = true; onOk(); return; }
    // Alt+Backspace → parent
    if (e.key == 8 && e.alt) {
        e.consumed = true;
        navigateTo(FileSystem::parentPath(currentPath_));
        return;
    }
    // Ctrl+A → select all
    if (e.key == 'a' && e.ctrl && multiSelect_) {
        e.consumed = true;
        std::fill(multiSel_.begin(), multiSel_.end(), true);
        markDirty();
        return;
    }
    // Arrow keys
    if (e.key == 0x40000052 && selectedIndex_ > 0) { // UP
        e.consumed = true;
        selectedIndex_--;
        std::fill(multiSel_.begin(), multiSel_.end(), false);
        if (selectedIndex_ < static_cast<int>(multiSel_.size())) multiSel_[selectedIndex_] = true;
        if (fileNameInput_ && !entries_[selectedIndex_].isDir)
            fileNameInput_->setText(entries_[selectedIndex_].name);
        markDirty();
    } else if (e.key == 0x40000051 && selectedIndex_ < static_cast<int>(entries_.size()) - 1) { // DOWN
        e.consumed = true;
        selectedIndex_++;
        std::fill(multiSel_.begin(), multiSel_.end(), false);
        if (selectedIndex_ < static_cast<int>(multiSel_.size())) multiSel_[selectedIndex_] = true;
        if (fileNameInput_ && !entries_[selectedIndex_].isDir)
            fileNameInput_->setText(entries_[selectedIndex_].name);
        markDirty();
    }

    FloatWindow::onKeyPress(e);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Actions
// ═════════════════════════════════════════════════════════════════════════════

void FileDialog::onOk()
{
    if (mode_ == Mode::SelectFolder) {
        selectedPath_ = currentPath_;
        accepted.emit(selectedPath_);
        WidgetApp::instance().removeFloat(this);
        return;
    }

    std::string fname;
    if (fileNameInput_) fname = fileNameInput_->text();

    if (fname.empty() && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(entries_.size())) {
        auto& fe = entries_[selectedIndex_];
        if (fe.isDir) { navigateTo(fe.path); return; }
        fname = fe.name;
    }
    if (fname.empty()) return;

    selectedPath_ = FileSystem::joinPath(currentPath_, fname);
    bool needsExist = (mode_ == Mode::Open || mode_ == Mode::OpenImage);
    if (needsExist && !FileSystem::exists(selectedPath_)) return;

    accepted.emit(selectedPath_);
    WidgetApp::instance().removeFloat(this);
}

void FileDialog::onCancel()
{
    cancelled.emit();
    WidgetApp::instance().removeFloat(this);
}
