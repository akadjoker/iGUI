#pragma once

#include "Widget.hpp"
#include "Signal.hpp"
#include "Theme.hpp"
#include "MenuWidgets.hpp"
#include <string>
#include <vector>
#include <functional>

// ═════════════════════════════════════════════════════════════════════════════
//  Breadcrumbs — navigation path display
//
//  Usage:
//      auto* bc = parent->createChild<Breadcrumbs>();
//      bc->setPath({"Home", "Documents", "Project"});
//      bc->itemClicked.connect([](int index) { /* navigate */ });
// ═════════════════════════════════════════════════════════════════════════════

namespace BuGUI
{
class Breadcrumbs : public Widget
{
public:
    Breadcrumbs();

    /// @brief Set the path segments.
    void setPath(const std::vector<std::string>& segments);
    /// @brief Get the current path segments.
    const std::vector<std::string>& path() const { return segments_; }
    /// @brief Set the separator string (default ">").
    void setSeparator(const std::string& sep) { separator_ = sep; markDirty(); }

    /// @brief Emitted when a segment is clicked (index).
    Signal<int> itemClicked;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;
    void  onMouseLeave() override;

private:
    std::vector<std::string> segments_;
    std::string separator_ = ">";
    int hoveredIndex_ = -1;

    // Cached segment positions (rebuilt in paint)
    struct SegmentRect { float x, w; };
    mutable std::vector<SegmentRect> segRects_;

    int hitTestSegment(float localX, float localY) const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  SearchBar — text input with search icon and clear button
//
//  Usage:
//      auto* sb = parent->createChild<SearchBar>();
//      sb->setPlaceholder("Search...");
//      sb->onSearch.connect([](const std::string& q) { /* filter */ });
// ═════════════════════════════════════════════════════════════════════════════

class SearchBar : public Widget
{
public:
    SearchBar();

    /// @brief Set the placeholder text.
    void setPlaceholder(const std::string& p) { placeholder_ = p; markDirty(); }
    /// @brief Get the placeholder text.
    const std::string& placeholder() const { return placeholder_; }
    /// @brief Set the current query text.
    void setText(const std::string& t);
    /// @brief Get the current query text.
    const std::string& text() const { return text_; }
    /// @brief Clear the search text.
    void clear();

    /// @brief Emitted when the text changes.
    Signal<const std::string&> onTextChanged;
    /// @brief Emitted when Enter is pressed.
    Signal<const std::string&> onSearch;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onKeyPress(KeyEvent& e) override;
    void  onTextInput(KeyEvent& e) override;

private:
    std::string text_;
    std::string placeholder_ = "Search...";
    int  cursorPos_ = 0;
    float blinkTimer_ = 0;
    float scrollX_ = 0;

    float iconW() const;     // search icon area width
    float clearBtnW() const; // clear button area width
    float textAreaX() const; // start of text area
    float textAreaW() const; // width of text area
};

// ═════════════════════════════════════════════════════════════════════════════
//  SplitButton — button with a dropdown arrow for secondary actions
//
//  Usage:
//      auto* sb = parent->createChild<SplitButton>("Save");
//      sb->addAction("Save As...", [](){ ... });
//      sb->addAction("Save All",  [](){ ... });
//      sb->clicked.connect([](){ /* primary save */ });
// ═════════════════════════════════════════════════════════════════════════════

class SplitButton : public Widget
{
public:
    explicit SplitButton(const std::string& text = "Action");
    ~SplitButton() override;

    /// @brief Set the primary button text.
    void setText(const std::string& t) { text_ = t; markDirty(); }
    /// @brief Get the primary button text.
    const std::string& text() const { return text_; }

    /// @brief Add a dropdown action with text and callback.
    void addAction(const std::string& text, std::function<void()> cb);
    /// @brief Add a separator to the dropdown.
    void addSeparator();

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;
    void  onMouseLeave() override;

private:
    std::string text_;
    Menu* dropMenu_ = nullptr;
    bool  hoverMain_ = false;
    bool  hoverDrop_ = false;

    float dropArrowW() const; // width of the arrow area
};

// ═════════════════════════════════════════════════════════════════════════════
//  ContextMenuBuilder — declarative right-click menu builder
//
//  Usage:
//      ContextMenuBuilder::build(widget, {
//          {"Cut",   [](){ ... }, "Ctrl+X"},
//          {"Copy",  [](){ ... }, "Ctrl+C"},
//          {"-"},  // separator
//          {"Paste", [](){ ... }, "Ctrl+V"},
//      });
// ═════════════════════════════════════════════════════════════════════════════

struct ContextMenuItem
{
    std::string text;                      // empty or "-" = separator
    std::function<void()> callback;
    std::string shortcut;
    bool enabled = true;
    bool checkable = false;
    bool checked = false;
};

class ContextMenuBuilder
{
public:
    /// @brief Build and attach a context menu to a widget.
    static Menu* build(Widget* target, const std::vector<ContextMenuItem>& items);
};

// ═════════════════════════════════════════════════════════════════════════════
//  Outliner — scene tree widget (extends TreeView with visibility/lock toggles)
//
//  Usage:
//      auto* ol = parent->createChild<Outliner>();
//      auto* root = ol->addRoot("Scene");
//      auto* cam  = root->addChild("Camera");
//      auto* mesh = root->addChild("Cube");
//      ol->setNodeVisible(mesh, false);
//      ol->itemRenamed.connect([](TreeNode* n, const std::string& name) { ... });
// ═════════════════════════════════════════════════════════════════════════════

class TreeNode;
class TreeView;

class Outliner : public Widget
{
public:
    Outliner();
    ~Outliner() override;

    /// @brief Add a root node.
    TreeNode* addRoot(const std::string& text);
    /// @brief Clear all nodes.
    void clear();

    /// @brief Set node visibility toggle state.
    void setNodeVisible(TreeNode* node, bool vis);
    /// @brief Check if a node is visible.
    bool isNodeVisible(TreeNode* node) const;
    /// @brief Set node lock state.
    void setNodeLocked(TreeNode* node, bool locked);
    /// @brief Check if a node is locked.
    bool isNodeLocked(TreeNode* node) const;

    /// @brief Get the selected node.
    TreeNode* selectedNode() const;

    /// @brief Emitted when selection changes.
    Signal<TreeNode*> selectionChanged;
    /// @brief Emitted when visibility is toggled.
    Signal<TreeNode*, bool> visibilityChanged;
    /// @brief Emitted when lock is toggled.
    Signal<TreeNode*, bool> lockChanged;

    void layout() override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseLeave() override;

private:
    struct FlatRow { TreeNode* node; int depth; };

    void rebuildFlat();
    void flattenNode(TreeNode* n, int depth);
    float maxScroll() const;
    int  hitTestRow(float localY) const;

    std::vector<TreeNode*> roots_;
    std::vector<FlatRow>   flatRows_;
    TreeNode* selected_ = nullptr;
    float scrollOffset_ = 0;
    float rowHeight_    = 22.0f;
    float indent_       = 16.0f;
    bool  flatDirty_    = true;
    int   hoveredRow_   = -1;

    // Per-node states (keyed by pointer)
    std::unordered_map<TreeNode*, bool> visMap_;
    std::unordered_map<TreeNode*, bool> lockMap_;

    // Icon column positions
    float eyeColX() const;
    float lockColX() const;
    float textColX(int depth) const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  RichText — simple markdown-subset renderer (read-only)
//
//  Supports: **bold**, *italic*, `code`, # headings, - lists, [links](url)
//
//  Usage:
//      auto* rt = parent->createChild<RichText>();
//      rt->setMarkdown("# Title\n\nSome **bold** text.");
// ═════════════════════════════════════════════════════════════════════════════

class RichText : public Widget
{
public:
    RichText();

    /// @brief Set content from a markdown string.
    void setMarkdown(const std::string& md);
    /// @brief Get the raw markdown string.
    const std::string& markdown() const { return rawMd_; }

    /// @brief Emitted when a link is clicked.
    Signal<const std::string&> linkClicked;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;
    void  onMouseLeave() override;
    void  onMouseScroll(MouseEvent& e) override;

private:
    // Parsed inline span
    enum class SpanStyle { Normal, Bold, Italic, Code, Link };
    struct Span {
        std::string text;
        SpanStyle   style = SpanStyle::Normal;
        std::string url;   // for Link spans
    };

    // Parsed block
    enum class BlockType { Paragraph, Heading1, Heading2, Heading3, ListItem, CodeBlock };
    struct Block {
        BlockType type = BlockType::Paragraph;
        std::vector<Span> spans;
    };

    void parse();

    std::string rawMd_;
    std::vector<Block> blocks_;
    float scrollY_ = 0;
    int   hoveredLink_ = -1;

    // Link hit rects (rebuilt on paint)
    struct LinkRect { float x, y, w, h; std::string url; };
    mutable std::vector<LinkRect> linkRects_;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Viewport3D — renders a TextureHandle (FBO color attachment) as a widget
//
//  The user renders their 3D scene to an FBO externally, then passes the
//  resulting color-attachment TextureHandle to this widget for display.
//  The widget handles mouse orbit/pan/zoom and emits camera change signals.
//
//  Usage:
//      auto* vp = parent->createChild<Viewport3D>();
//      vp->setTexture(fboColorTex, fboW, fboH);
//      vp->onOrbit.connect([](float yaw, float pitch) { ... });
//      vp->onPan.connect([](float dx, float dy) { ... });
//      vp->onZoom.connect([](float delta) { ... });
// ═════════════════════════════════════════════════════════════════════════════

class Viewport3D : public Widget
{
public:
    Viewport3D();

    /// @brief Set the texture to display (FBO color attachment).
    void setTexture(BuGUI::TextureHandle tex, int w, int h);
    /// @brief Get the current texture handle.
    BuGUI::TextureHandle texture() const { return tex_; }

    /// @brief Set orbit speed (degrees per pixel).
    void setOrbitSpeed(float s) { orbitSpeed_ = s; }
    /// @brief Set pan speed (units per pixel).
    void setPanSpeed(float s)   { panSpeed_ = s; }
    /// @brief Set zoom speed (units per scroll tick).
    void setZoomSpeed(float s)  { zoomSpeed_ = s; }

    /// @brief Get the current yaw angle (degrees).
    float yaw()   const { return yaw_; }
    /// @brief Get the current pitch angle (degrees).
    float pitch() const { return pitch_; }

    /// @brief Set the camera angles programmatically.
    void setOrbit(float yaw, float pitch);

    /// @brief Emitted when orbit changes (yaw, pitch in degrees).
    Signal<float, float> onOrbit;
    /// @brief Emitted when pan changes (dx, dy in screen pixels).
    Signal<float, float> onPan;
    /// @brief Emitted when zoom changes (delta).
    Signal<float> onZoom;
    /// @brief Emitted when widget is resized (for FBO recreation).
    Signal<int, int> onResize;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;
    void  onMouseScroll(MouseEvent& e) override;

private:
    BuGUI::TextureHandle tex_ = {};
    int texW_ = 0, texH_ = 0;

    float orbitSpeed_ = 0.3f;
    float panSpeed_   = 1.0f;
    float zoomSpeed_  = 1.0f;

    float yaw_   = 0, pitch_ = 0;

    // Drag state
    enum class DragMode { None, Orbit, Pan };
    DragMode dragMode_ = DragMode::None;
    float    dragStartX_ = 0, dragStartY_ = 0;
    float    dragYaw_ = 0, dragPitch_ = 0;

    int lastW_ = 0, lastH_ = 0; // for resize detection
};

} // namespace BuGUI
