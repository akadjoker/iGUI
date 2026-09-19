#pragma once

#include <stdint.h>

#include <ct/deque.hpp>
#include <ct/function.hpp>
#include <ct/hashmap.hpp>
#include <ct/slotmap.hpp>
#include <ct/vector.hpp>

#include "Backend.hpp"
#include "CodeEditor.hpp"
#include "Events.hpp"
#include "FileDialog.hpp"
#include "Math.hpp"
#include "Theme.hpp"

namespace ig
{

struct WindowState
{
    WidgetId id;
    String title;
    Rect bounds;
    Rect restoreBounds;
    bool maximized = false;
    bool hasRestoreBounds = false;
    bool open;
    bool minimized;
    bool focused;
    bool showWindowControls;
    bool showTitleBar;
    bool useClientArea;
    bool allowMove;
    bool allowResize;
    uint64_t zOrder;
    DrawList drawList;
    DrawList overlayDrawList;

    WindowState()
        : id(InvalidWidgetId), title(), bounds(), open(true), minimized(false), focused(false),
          showWindowControls(true), showTitleBar(true), useClientArea(false), allowMove(true), allowResize(true),
          zOrder(0), drawList(), overlayDrawList()
    {
    }
};

using WindowHandle = ct::Handle<WindowState>;

struct TreeItemStyle
{
    Color typeColor;
    bool selected;
    bool leaf;
    bool disabled;
    // A leaf can still accept a child; it gains an expansion arrow once the
    // application's tree model reports that child on the next frame.
    bool acceptsChildren;

    TreeItemStyle()
        : typeColor(120u, 170u, 230u, 255u), selected(false), leaf(false), disabled(false),
          acceptsChildren(false) {}
};

// The relation selected when a tree item is dropped over another item.
enum class TreeDropPosition : uint8_t
{
    Before,
    Inside,
    After
};

// Returned by the draggable treeItem overload. Apply this operation to the
// application's tree model after finishing its current draw pass.
struct TreeDrop
{
    WidgetId source;
    WidgetId target;
    TreeDropPosition position;

    TreeDrop()
        : source(InvalidWidgetId), target(InvalidWidgetId), position(TreeDropPosition::Inside) {}
};

enum class MessageBoxResult : uint8_t
{
    None,
    Accepted,
    Cancelled,
    Discarded
};

enum class MessageBoxKind : uint8_t
{
    Information,
    Warning,
    Error,
    Input
};

struct MessageBoxOptions
{
    StringView acceptLabel = "OK";
    StringView cancelLabel = "Cancel";
    bool showDiscard = false;
    MessageBoxKind kind;
    bool showCancel;
    String *inputValue;

    MessageBoxOptions()
        : kind(MessageBoxKind::Information), showCancel(false), inputValue(nullptr) {}
};

enum class ToastPosition : uint8_t
{
    TopLeft, TopCenter, TopRight,
    MiddleLeft, Center, MiddleRight,
    BottomLeft, BottomCenter, BottomRight
};

struct DragDropPayload
{
    WidgetId type;
    WidgetId source;
    uint64_t data;

    DragDropPayload() : type(InvalidWidgetId), source(InvalidWidgetId), data(0u) {}
};

enum class SplitterAxis : uint8_t
{
    Vertical,
    Horizontal
};

// A dockspace divides one editor surface into persistent tab regions.
enum class DockSlot : uint8_t
{
    Left,
    Center,
    Right,
    Bottom
};

enum class Gizmo2DMode : uint8_t
{
    Translate,
    Rotate,
    Scale
};

enum class Gizmo3DMode : uint8_t
{
    Translate,
    Rotate,
    Scale
};

// Transform values are expressed in pixels relative to the gizmo canvas.
// rotation is in degrees and scale starts at (1, 1).
struct Transform2D
{
    Vec2 position;
    float rotation;
    Vec2 scale;

    Transform2D() : position(), rotation(0.0f), scale(1.0f, 1.0f) {}
};

struct Gizmo2DOptions
{
    float axisLength;
    float translateSnap;
    float rotateSnap;
    float scaleSnap;

    Gizmo2DOptions()
        : axisLength(60.0f), translateSnap(0.0f), rotateSnap(0.0f), scaleSnap(0.0f) {}
};

struct Transform3D
{
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;

    Transform3D() : position(), rotation(), scale(1.0f, 1.0f, 1.0f) {}
};

struct Gizmo3DOptions
{
    float axisLength;
    float translateSnap;
    float rotateSnap;
    float scaleSnap;

    Gizmo3DOptions()
        : axisLength(1.0f), translateSnap(0.0f), rotateSnap(0.0f), scaleSnap(0.0f) {}
};

// Application-owned stop for gradientEditor(). Positions are normalized to
// [0, 1]; the editor keeps the vector sorted after every interaction.
struct GradientStop
{
    float position;
    Color color;

    GradientStop() : position(0.0f), color() {}
    GradientStop(float value, const Color &stopColor) : position(value), color(stopColor) {}
};

// Application-owned point for curveEditor(). Coordinates use the supplied
// value range and are kept ordered by x after an edit.
struct CurvePoint
{
    float x;
    float y;

    CurvePoint() : x(0.0f), y(0.0f) {}
    CurvePoint(float pointX, float pointY) : x(pointX), y(pointY) {}
};
struct TimelineTrack
{
    String label;
    ct::Vector<int> keys;
};
struct SequencerTrack { String label; int startFrame; int endFrame; Color color;
    SequencerTrack() : label(), startFrame(0), endFrame(0), color(90,160,230) {}
    SequencerTrack(StringView n, int s, int e, const Color& c) : label(n.data(),n.size()), startFrame(s), endFrame(e), color(c) {} };

class Context
{
public:
    explicit Context(Backend &backend, TextProvider *textProvider = nullptr);

    void pushEvent(const Event &event);
    void beginFrame(const FrameInfo &frame);
    const DrawData &endFrame();

    bool beginWindow(StringView title, const Rect &initialBounds, bool *open = nullptr);
    // A root window that follows the current viewport every frame.
    bool beginMainWindow(StringView title, bool *open = nullptr);
    void endWindow();
    // Operate on existing floating windows identified by their title.
    void maximizeWindow(StringView title);
    void restoreWindow(StringView title);
    void maximizeAllWindows();
    void minimizeAllWindows();
    void restoreAllWindows();
    void tileAllWindows();
    void cascadeWindows();

    void pushId(uint64_t id);
    void pushId(StringView id);
    void popId();

    void setTheme(const Theme &theme);
    const Theme &theme() const;
    bool wantsPointer() const;
    bool wantsKeyboard() const;
    bool wantsTextInput() const;

    // Application-owned history entries. The callbacks must remain valid for
    // as long as the entry stays in this Context's history.
    void pushUndo(StringView label, ct::Function<void()> undo, ct::Function<void()> redo);
    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const;
    void clearUndoHistory();

    bool button(StringView label, const Rect &bounds);
    // Compact text action for inspectors and toolbars.
    bool smallButton(StringView label, const Rect &bounds);
    bool checkbox(StringView label, bool &value, const Rect &bounds);
    bool toggleSwitch(StringView label, bool &value, const Rect &bounds);
    bool radioButton(StringView label, bool selected, const Rect &bounds);
    bool selectable(StringView label, bool selected, const Rect &bounds);
    // Returns whether the section is expanded. The bool stores its persistent state.
    bool collapsingHeader(StringView label, bool &expanded, const Rect &bounds);
    // A compact expandable row for hierarchies. Use indent()/unindent() for its children.
    bool treeNode(StringView label, bool &expanded, const Rect &bounds);
    // Scene-oriented tree row. Returns true when the row is selected; expansion
    // is changed only by pressing its disclosure arrow.
    bool treeItem(StringView label, bool &expanded, const TreeItemStyle &style,
                  const Rect &bounds);
    // Draggable scene-tree item. nodeId must be stable in the application's tree
    // model. Drop over the upper, middle or lower area for Before, Inside or After.
    // The source item cannot be its own target.
    bool treeItem(WidgetId nodeId, StringView label, bool &expanded, const TreeItemStyle &style,
                  const Rect &bounds, TreeDrop *drop = nullptr);
    // Renders a tab strip and returns true when the selected tab changes.
    bool tabBar(StringView label, int &currentItem, Span<const StringView> items,
                const Rect &bounds);
    // A scrollable list. The label is an identifier; render a separate label when needed.
    bool listBox(StringView label, int &currentItem, Span<const StringView> items,
                 const Rect &bounds);
    bool comboBox(StringView label, int &currentItem, Span<const StringView> items,
                  const Rect &bounds);
    // A horizontal menu strip. Call beginMenu()/endMenu() between these calls.
    bool beginMenuBar(const Rect &bounds);
    void endMenuBar();
    // Opens a popup menu from the current menu bar.
    bool beginMenu(StringView label);
    void endMenu();
    bool menuItem(StringView label, bool enabled = true);
    // Checkable menu entry. Returns true when its value changed.
    bool menuCheckbox(StringView label, bool &checked, bool enabled = true);
    // One-level submenu, opened by hover or click. End it before ending its parent menu.
    bool beginSubMenu(StringView label, bool enabled = true);
    void endSubMenu();
    void menuSeparator();
    // Opens a popup at the pointer on a right click over bounds. Use menuItem()
    // and menuSeparator(), then close it with endContextMenu().
    bool beginContextMenu(StringView id, const Rect &bounds);
    void endContextMenu();
    bool sliderFloat(StringView label, float &value, float minimum, float maximum,
                     const Rect &bounds);
    bool sliderInt(StringView label, int &value, int minimum, int maximum,
                   const Rect &bounds);
    // Horizontal relative drag. The value changes by speed for each moved pixel;
    // click its value field to enter a precise number.
    bool dragFloat(StringView label, float &value, float minimum, float maximum,
                   float speed, const Rect &bounds);
    bool dragInt(StringView label, int &value, int minimum, int maximum,
                 int speed, const Rect &bounds);
    // Draggable divider. value is the local x/y coordinate inside bounds.
    bool splitter(StringView id, float &value, float minimum, float maximum,
                  SplitterAxis axis, const Rect &bounds, float thickness = 5.0f);
    // Transparent editor overlay for a 2D transform inside bounds.
    bool gizmo2D(StringView id, Transform2D &transform, Gizmo2DMode mode, const Rect &bounds,
                 const Gizmo2DOptions &options = Gizmo2DOptions());
    // Projected 3D transform handles. view and projection are column-major 4x4 matrices.
    bool gizmo3D(StringView id, Transform3D &transform, Gizmo3DMode mode, const Rect &bounds,
                 const float *view, const float *projection,
                 const Gizmo3DOptions &options = Gizmo3DOptions());
    bool stepperInt(StringView label, int &value, int minimum, int maximum,
                    const Rect &bounds);
    bool inputText(StringView label, String &value, const Rect &bounds);
    // followTail keeps the view pinned to the last line as value grows (a
    // console/log), unless the user scrolled up to read back.
    bool inputTextMultiline(StringView label, String &value, const Rect &bounds,
                            bool followTail = false);
    // Syntax-highlighted source editor: line buffer, (line, column) cursor
    // with selection, full undo/redo, and keyword/identifier autocomplete.
    // state is application-owned and persists across frames (see
    // CodeEditorState); use state.setText/setHighlighterForFile to load a
    // document and pick a language. Returns true the frame the buffer changed.
    bool codeEditor(StringView id, CodeEditorState &state, const Rect &bounds,
                    const CodeEditorOptions &options = CodeEditorOptions());
    // Programmatic equivalents of the Ctrl+C/X/V shortcuts codeEditor()
    // already handles internally - for a context menu's Copy/Cut/Paste
    // items, or any other caller that wants to trigger them without
    // synthesizing key events. Safe to call with no selection (Copy/Cut
    // become no-ops) or an empty clipboard (Paste becomes a no-op).
    // Each returns true if it changed the buffer (Cut/Paste; Copy never does).
    bool codeEditorCopy(CodeEditorState &state);
    bool codeEditorCut(CodeEditorState &state);
    bool codeEditorPaste(CodeEditorState &state);
    bool inputInt(StringView label, int &value, const Rect &bounds);
    bool inputFloat(StringView label, float &value, const Rect &bounds, int precision = 6);
    // Inline RGBA editor with a preview swatch and four draggable channels.
    bool colorEdit(StringView label, Color &value, const Rect &bounds);
    // Editable multi-stop gradient. Left-click an empty part of the bar to
    // add an interpolated stop, drag a handle to move it, and right-click a
    // non-endpoint handle to remove it. selectedStop is -1 when none is set.
    bool gradientEditor(StringView id, ct::Vector<GradientStop> &stops, int &selectedStop,
                        const Rect &bounds);
    // Linear curve editor. Click empty canvas space to add a point, drag a
    // point to edit it, and right-click a non-endpoint point to remove it.
    bool curveEditor(StringView id, ct::Vector<CurvePoint> &points, int &selectedPoint,
                     const Rect &bounds, const Vec2 &minimum = Vec2(),
                     const Vec2 &maximum = Vec2(1.0f, 1.0f));
    bool sequencer(StringView id, ct::Vector<SequencerTrack>& tracks, int firstFrame, int lastFrame,
                   int& currentFrame, int& selectedTrack, const Rect& bounds);
    // Click empty track space to add a key, drag diamonds, right-click to delete.
    bool timeline(StringView id, ct::Vector<TimelineTrack>& tracks, int firstFrame, int lastFrame,
                  int& currentFrame, int& selectedTrack, int& selectedKey, const Rect& bounds);
    // Draw an already-owned backend texture. TextureId remains backend-neutral.
    void image(TextureId texture, const Rect &bounds,
               const Vec2 &uvMin = Vec2(0.0f, 0.0f), const Vec2 &uvMax = Vec2(1.0f, 1.0f),
               const Color &tint = Color());
    // The label is an identifier only; the texture is the button's visual.
    bool imageButton(StringView label, TextureId texture, const Rect &bounds,
                     const Vec2 &uvMin = Vec2(0.0f, 0.0f), const Vec2 &uvMax = Vec2(1.0f, 1.0f),
                     const Color &tint = Color());
    // Compact icon-only button. label is an identifier, not rendered text.
    bool smallImageButton(StringView label, TextureId texture, const Rect &bounds,
                          const Vec2 &uvMin = Vec2(0.0f, 0.0f), const Vec2 &uvMax = Vec2(1.0f, 1.0f),
                          const Color &tint = Color());
    // Minimal building blocks for application-defined immediate widgets.
    bool invisibleButton(StringView id, const Rect &bounds);
    bool isHovered(StringView id, const Rect &bounds);
    // Equivalent to invisibleButton, named for custom widget interaction code.
    bool isClicked(StringView id, const Rect &bounds);
    void drawRectFilled(const Rect &bounds, const Color &color);
    void drawRect(const Rect &bounds, const Color &color, float thickness = 1.0f);
    void drawLine(const Vec2 &from, const Vec2 &to, const Color &color, float thickness = 1.0f);
    void drawCircleFilled(const Vec2 &center, float radius, const Color &color);
    bool isKeyPressed(KeyCode key) const;
    bool shortcut(KeyCode key, bool control = true, bool shift = false) const;
    void progressBar(float value, float maximum, const Rect &bounds);
    // Draws an application-modal OK or OK/Cancel message box above all windows.
    MessageBoxResult messageBox(StringView title, StringView message, bool &open,
                                bool showCancel = false);
    MessageBoxResult messageBox(StringView title, StringView message, bool &open,
                                const MessageBoxOptions &options);
    // Application-modal filesystem browser. Its provider performs OS or
    // sandbox access; this Context owns only interaction and rendering.
    FileDialogResult fileDialog(StringView id, bool &open, FileDialogState &state,
                                const FileDialogOptions &options, FileDialogProvider &provider);
    // Starts or refreshes a short notification. The id differentiates concurrent toasts.
    void showToast(StringView id, StringView text, ToastPosition position = ToastPosition::BottomRight,
                   float duration = 3.0f);
    // Call after drawing a local item. The item becomes draggable after a short mouse move.
    bool beginDragSource(WidgetId source, WidgetId type, uint64_t data, StringView preview,
                         const Rect &bounds);
    // Call over a local drop zone. On release, returns the compatible payload.
    bool acceptDragDropTarget(WidgetId acceptedType, const Rect &bounds, DragDropPayload &payload);
    void label(StringView text, const Vec2 &position);
    // Call immediately after the widget that owns this help text.
    void tooltip(StringView text);
    // Tooltips appear after this much stationary hover time. The default is 0.45 seconds.
    void setTooltipDelay(float seconds);
    float tooltipDelay() const;

    bool button(StringView label);
    bool smallButton(StringView label);
    bool checkbox(StringView label, bool &value);
    bool toggleSwitch(StringView label, bool &value);
    bool radioButton(StringView label, bool selected);
    bool selectable(StringView label, bool selected, float width = 0.0f);
    bool collapsingHeader(StringView label, bool &expanded, float width = 0.0f);
    bool treeNode(StringView label, bool &expanded, float width = 0.0f);
    bool treeItem(StringView label, bool &expanded, const TreeItemStyle &style,
                  float width = 0.0f);
    bool treeItem(WidgetId nodeId, StringView label, bool &expanded, const TreeItemStyle &style,
                  TreeDrop *drop = nullptr, float width = 0.0f);
    bool tabBar(StringView label, int &currentItem, Span<const StringView> items,
                float width = 0.0f);
    bool listBox(StringView label, int &currentItem, Span<const StringView> items,
                 float width = 0.0f, int visibleItems = 4);
    bool comboBox(StringView label, int &currentItem, Span<const StringView> items,
                  float width = 0.0f);
    bool sliderFloat(StringView label, float &value, float minimum, float maximum,
                     float width = 0.0f);
    bool sliderInt(StringView label, int &value, int minimum, int maximum,
                   float width = 0.0f);
    bool dragFloat(StringView label, float &value, float minimum, float maximum,
                   float speed = 0.01f, float width = 0.0f);
    bool dragInt(StringView label, int &value, int minimum, int maximum,
                 int speed = 1, float width = 0.0f);
    bool inputFloat2(StringView label, float &x, float &y, float width = 0.0f, int precision = 3);
    bool inputFloat3(StringView label, float &x, float &y, float &z, float width = 0.0f, int precision = 3);
    bool inputFloat4(StringView label, float &x, float &y, float &z, float &w,
                     float width = 0.0f, int precision = 3);
    bool dragFloat2(StringView label, float &x, float &y, float minimum, float maximum,
                    float speed = 0.01f, float width = 0.0f);
    bool dragFloat3(StringView label, float &x, float &y, float &z, float minimum, float maximum,
                    float speed = 0.01f, float width = 0.0f);
    bool dragFloat4(StringView label, float &x, float &y, float &z, float &w,
                    float minimum, float maximum, float speed = 0.01f, float width = 0.0f);
    bool stepperInt(StringView label, int &value, int minimum, int maximum,
                    float width = 0.0f);
    bool inputText(StringView label, String &value, float width = 0.0f);
    bool inputTextMultiline(StringView label, String &value, float width = 0.0f,
                            float height = 120.0f, bool followTail = false);
    bool inputInt(StringView label, int &value, float width = 0.0f);
    bool inputFloat(StringView label, float &value, float width = 0.0f, int precision = 6);
    bool colorEdit(StringView label, Color &value, float width = 0.0f, float height = 128.0f);
    void image(TextureId texture, float width, float height,
               const Vec2 &uvMin = Vec2(0.0f, 0.0f), const Vec2 &uvMax = Vec2(1.0f, 1.0f),
               const Color &tint = Color());
    bool imageButton(StringView label, TextureId texture, float width, float height,
                     const Vec2 &uvMin = Vec2(0.0f, 0.0f), const Vec2 &uvMax = Vec2(1.0f, 1.0f),
                     const Color &tint = Color());
    bool smallImageButton(StringView label, TextureId texture, float size = 0.0f,
                          const Vec2 &uvMin = Vec2(0.0f, 0.0f), const Vec2 &uvMax = Vec2(1.0f, 1.0f),
                          const Color &tint = Color());
    // Clipped, vertically scrollable content region. height is required; width
    // defaults to the remaining window width.
    bool beginChild(StringView id, float height, bool border = true, float width = 0.0f);
    void endChild();
    // Editor-style docking. Register panels each frame; only the active tab in
    // each region returns true. Tabs can be dragged between regions; tabs and
    // splitter sizes persist in the Context.
    bool beginDockSpace(StringView id);
    // Reserves topInset logical pixels for a menu bar or toolbar in the current window.
    bool beginDockSpace(StringView id, float topInset);
    bool beginDockSpace(StringView id, const Rect &bounds);
    void endDockSpace();
    // Pass open to render a close button in the tab; closing it sets *open to false.
    bool beginDockPanel(StringView title, DockSlot slot = DockSlot::Center, bool *open = nullptr);
    void endDockPanel();
    // Virtualized child content. Only render rows in [firstVisible, lastVisible)
    // and use virtualListItemRect() with the bounds-based widget overloads.
    bool beginVirtualList(StringView id, int itemCount, float itemHeight, float height,
                          int &firstVisible, int &lastVisible,
                          bool border = true, float width = 0.0f);
    Rect virtualListItemRect(int itemIndex) const;
    void endVirtualList();
    // Virtualized table rows. Render only [firstVisibleRow, lastVisibleRow) and
    // use virtualTableCellRect() for bounds-based widgets in each cell.
    bool beginVirtualTable(StringView id, int rowCount, int columns, float rowHeight, float height,
                           int &firstVisibleRow, int &lastVisibleRow,
                           bool border = true, float width = 0.0f);
    // Virtualized rows with weighted columns. Use the same weights as a
    // preceding beginTable()/tableHeader() row to create a fixed, resizable
    // header aligned with the scrolling cells.
    bool beginVirtualTable(StringView id, int rowCount, Span<const float> columnWeights,
                           float rowHeight, float height,
                           int &firstVisibleRow, int &lastVisibleRow,
                           bool border = true, float width = 0.0f);
    Rect virtualTableCellRect(int row, int column) const;
    void endVirtualTable();
    // Virtualized rows for an application-owned, flattened tree. depth controls
    // indentation; use the bounds with treeItem() to keep its existing behavior.
    bool beginVirtualTree(StringView id, int rowCount, float rowHeight, float height,
                          int &firstVisibleRow, int &lastVisibleRow,
                          bool border = true, float width = 0.0f);
    Rect virtualTreeItemRect(int row, int depth, float indentWidth = 16.0f) const;
    void endVirtualTree();
    // Lightweight immediate table. Call tableNextColumn() before each cell.
    bool beginTable(StringView id, int columns, float width = 0.0f);
    // Weighted table columns. All weights must be positive; they are copied
    // for the frame. For example, {3.0f, 1.0f} creates a 75/25 split.
    bool beginTable(StringView id, Span<const float> columnWeights, float width = 0.0f);
    // Mutable weights opt into resizing through tableHeader() grips. The
    // adjusted weights are written back to the caller and should be kept for
    // the next frame to preserve the chosen column widths.
    bool beginTable(StringView id, Span<float> columnWeights, float width = 0.0f);
    bool tableNextColumn();
    // Draw a clickable header in the current table cell. sortColumn starts at
    // -1; clicking a new column selects ascending order and clicking the
    // selected column toggles sortAscending. Returns true when the sort state
    // changes. Call tableNextColumn() before every header and cell.
    bool tableHeader(StringView label, int &sortColumn, bool &sortAscending);
    void endTable();
    // Aligned label/value row for inspectors. Render the value widgets between
    // beginPropertyRow() and endPropertyRow().
    bool beginPropertyRow(StringView label, float labelWidth = 0.0f);
    void endPropertyRow();
    void progressBar(float value, float maximum, float width = 0.0f);
    void label(StringView text);
    void sameLine(float spacing = -1.0f);
    void spacing(float pixels);
    void indent(float pixels = 16.0f);
    void unindent(float pixels = 16.0f);
    void separator(float thickness = 1.0f);
    void separatorText(StringView text, float width = 0.0f);
    void setCursor(const Vec2 &localPosition);
    Vec2 cursor() const;
    // Remaining horizontal content space at the current layout cursor.
    float availableWidth() const;
    // Remaining vertical space in the current window or panel.
    float availableHeight() const;

private:
    struct PointerState
    {
        Vec2 position;
        Vec2 pressedPosition[3];
        Vec2 releasedPosition[3];
        bool down[3];
        bool pressed[3];
        bool released[3];
        float wheelX;
        float wheelY;

        PointerState();
    };

    struct LayoutState
    {
        Vec2 origin;
        Vec2 cursor;
        float baseOriginX;
        Rect lastItem;
        bool hasLastItem;

        LayoutState() : origin(), cursor(), baseOriginX(0.0f), lastItem(), hasLastItem(false) {}
    };

    struct ColorPickerState
    {
        float hue;
        float saturation;
        float brightness;
        Color lastColor;
        bool initialized;

        ColorPickerState()
            : hue(0.0f), saturation(0.0f), brightness(0.0f), lastColor(), initialized(false) {}
    };

    struct ChildScrollState
    {
        float offset;
        float contentHeight;

        ChildScrollState() : offset(0.0f), contentHeight(0.0f) {}
    };

    struct Gizmo2DState
    {
        uint8_t axis;
        Vec2 pointerStart;
        Transform2D transformStart;

        Gizmo2DState() : axis(0u), pointerStart(), transformStart() {}
    };

    struct Gizmo3DState
    {
        uint8_t axis;
        Vec2 pointerStart;
        Transform3D transformStart;

        Gizmo3DState() : axis(0u), pointerStart(), transformStart() {}
    };

    struct ChildState
    {
        LayoutState parentLayout;
        Rect outer;
        Rect content;
        Rect parentClip;
        WidgetId id;
        bool border;

        ChildState() : parentLayout(), outer(), content(), parentClip(), id(InvalidWidgetId), border(true) {}
    };

    struct UndoState
    {
        String label;
        ct::Function<void()> undo;
        ct::Function<void()> redo;
    };

    struct DockTabState
    {
        WidgetId id;
        String title;
        DockSlot slot;
        uint64_t lastSeenFrame;
        bool *open;

        DockTabState() : id(InvalidWidgetId), title(), slot(DockSlot::Center), lastSeenFrame(0u), open(nullptr) {}
    };

    struct DockSpaceState
    {
        Rect bounds;
        Rect clip;
        float leftWidth;
        float rightWidth;
        float bottomHeight;
        WidgetId selected[4];
        bool tabListOpen[4];
        bool tabBarHidden[4];
        uint64_t tabBarFrame[4];
        ct::Vector<DockTabState> tabs;

        DockSpaceState() : bounds(), clip(), leftWidth(180.0f), rightWidth(240.0f), bottomHeight(180.0f), tabs()
        {
            for (uint32_t i = 0u; i < 4u; ++i)
            {
                selected[i] = InvalidWidgetId;
                tabListOpen[i] = false;
                tabBarHidden[i] = false;
                tabBarFrame[i] = 0u;
            }
        }
    };

    struct DockPanelState
    {
        LayoutState parentLayout;
        Rect content;
        WidgetId id;

        DockPanelState() : parentLayout(), content(), id(InvalidWidgetId) {}
    };

    struct VirtualListState
    {
        WidgetId childId;
        Rect content;
        int itemCount;
        float itemHeight;
        float scrollOffset;

        VirtualListState()
            : childId(InvalidWidgetId), content(), itemCount(0), itemHeight(0.0f), scrollOffset(0.0f) {}
    };

    struct VirtualTableState
    {
        WidgetId childId;
        int columns;
        ct::Vector<float> columnWeights;
        float totalColumnWeight;

        VirtualTableState()
            : childId(InvalidWidgetId), columns(0), columnWeights(), totalColumnWeight(0.0f) {}
    };

    struct VirtualTreeState
    {
        WidgetId childId;

        VirtualTreeState() : childId(InvalidWidgetId) {}
    };

    struct TableState
    {
        LayoutState parentLayout;
        Rect bounds;
        WidgetId id;
        int columns;
        int column;
        float rowY;
        float rowHeight;
        ct::Vector<float> columnWeights;
        float *resizableColumnWeights;
        float totalColumnWeight;

        TableState()
            : parentLayout(), bounds(), id(InvalidWidgetId), columns(0), column(-1), rowY(0.0f),
              rowHeight(0.0f), columnWeights(), resizableColumnWeights(nullptr), totalColumnWeight(0.0f) {}
    };

    struct TableResizeState
    {
        WidgetId tableId;
        int column;
        float lastPointerX;

        TableResizeState() : tableId(InvalidWidgetId), column(-1), lastPointerX(0.0f) {}
    };

    struct PropertyRowState
    {
        LayoutState parentLayout;
        Rect bounds;

        PropertyRowState() : parentLayout(), bounds() {}
    };

    struct ToastState
    {
        WidgetId id;
        String text;
        ToastPosition position;
        float remaining;
        float duration = 0.0f;

        ToastState() : id(InvalidWidgetId), text(), position(ToastPosition::BottomRight), remaining(0.0f) {}
    };

    struct DragDropState
    {
        WidgetId widget;
        DragDropPayload payload;
        String preview;
        Vec2 pressedPosition;
        bool armed;
        bool active;
        bool accepted;

        DragDropState()
            : widget(InvalidWidgetId), payload(), preview(), pressedPosition(), armed(false), active(false), accepted(false) {}
    };

    Backend &backend_;
    TextProvider *textProvider_;
    Theme theme_;
    FrameInfo frame_;
    PointerState pointer_;
    LayoutState layout_;
    ct::Deque<Event> events_;
    ct::Vector<Event> textEvents_;
    ct::SlotMap<WindowState> windows_;
    ct::HashMap<WidgetId, WindowHandle> windowsById_;
    ct::HashMap<WidgetId, int> listScrolls_;
    ct::HashMap<WidgetId, int> textScrolls_;
    // Line count drawn last frame, kept only for followTail widgets so the
    // next frame can tell growth from a scroll-away (see inputTextMultiline).
    ct::HashMap<WidgetId, int> textTailLines_;
    struct SequenceDrag
    {
        int row = -1;
        int mode = 0;
        int start = 0;
        int end = 0;
        float x = 0;
    };
    ct::HashMap<WidgetId, SequenceDrag> sequenceDrags_;
    struct TimeView
    {
        float zoom = 1;
        float offset = 0;
        float panX = 0;
        float panOffset = 0;
    };
    ct::HashMap<WidgetId, TimeView> timeViews_;
    void updateTimeView(WidgetId id, const Rect& area, const Rect& ruler, TimeView& view);
    void drawTimeView(const Rect& area, const Rect& ruler, const TimeView& view);
    void drawTimeRuler(const Rect& area, const Rect& ruler, const TimeView& view, int first, double count);
    struct NumericEditState
    {
        String text;
        bool editing = false;
        bool moved = false;
    };
    ct::HashMap<WidgetId, NumericEditState> numericEdits_;
    ct::HashMap<WidgetId, ColorPickerState> colorPickers_;
    ct::HashMap<WidgetId, ChildScrollState> childScrolls_;
    ct::HashMap<WidgetId, Gizmo2DState> gizmo2DStates_;
    ct::HashMap<WidgetId, Gizmo3DState> gizmo3DStates_;
    ct::HashMap<WidgetId, DockSpaceState> dockSpaces_;
    ct::Vector<WindowHandle> windowOrder_;
    ct::Vector<WidgetId> idStack_;
    ct::Vector<WidgetId> focusOrder_;
    ct::Vector<ChildState> childStack_;
    ct::Vector<VirtualListState> virtualListStack_;
    ct::Vector<VirtualTableState> virtualTableStack_;
    ct::Vector<VirtualTreeState> virtualTreeStack_;
    ct::Vector<DockPanelState> dockPanelStack_;
    ct::Vector<ToastState> toasts_;
    ct::Vector<UndoState> undoStack_;
    ct::Vector<UndoState> redoStack_;
    DragDropState dragDrop_;
    DrawList frameDrawList_;
    DrawList dragDropDrawList_;
    DrawList toastDrawList_;
    DrawList modalDrawList_;
    DrawData drawData_;
    WindowHandle currentWindow_;
    WindowHandle focusedWindow_;
    WindowHandle draggingWindow_;
    WindowHandle resizingWindow_;
    WidgetId activeWidget_;
    WidgetId hotWidget_;
    WidgetId lastItemId_;
    WidgetId focusedWidget_;
    WidgetId textInputWidget_;
    WidgetId openCombo_;
    WidgetId openMenu_;
    WidgetId openContextMenu_;
    WidgetId openSubMenu_;
    WidgetId activeMenu_;
    WidgetId subMenuParent_;
    WidgetId activeModal_;
    WidgetId dragWidget_;
    WidgetId activeDockSpace_;
    WidgetId dockDragSpace_;
    WidgetId dockDragTab_;
    String::size_type textCursor_;
    uint64_t frameNumber_;
    uint64_t nextZOrder_;
    Vec2 windowDragOffset_;
    Vec2 dockDragStart_;
    Rect menuBarBounds_;
    Rect menuPopupBounds_;
    // Width of the open menu's popup, keyed by menu id. A dropdown cannot
    // know how wide its widest item is before the items are submitted, so
    // menuItemInternal records the widest label it drew and endMenu latches
    // it here; beginMenu/beginContextMenu size the next frame's panel with
    // it (menuMinWidth is the floor, so a menu reaches its full width from
    // its second frame on). Without this the panel stayed at menuMinWidth
    // and a long item ("Build and run native") drew its tail outside the
    // border.
    WidgetId menuWidthId_ = InvalidWidgetId;
    float menuPopupWidth_ = 0.0f;
    float menuMeasuredWidth_ = 0.0f;
    Rect activeMenuBounds_;
    Rect subMenuPopupBounds_;
    Rect subMenuParentBounds_;
    float menuBarCursorX_;
    bool menuBarActive_;
    bool forceWindowBounds_;
    TableState table_;
    TableResizeState tableResize_;
    PropertyRowState propertyRow_;
    bool tableActive_;
    bool propertyRowActive_;
    float dragStartValue_;
    float dragStartX_;
    float tooltipDelay_;
    float tooltipHoverSeconds_;
    Vec2 tooltipPointerPosition_;
    WidgetId tooltipWidget_;
    bool wantsKeyboard_;
    bool wantsTextInput_;
    bool backspacePressed_;
    bool enterPressed_;
    bool homePressed_;
    bool endPressed_;
    bool upPressed_;
    bool downPressed_;
    bool leftPressed_;
    bool rightPressed_;
    bool pageUpPressed_;
    bool pageDownPressed_;
    bool copyRequested_;
    bool pasteRequested_;
    bool escapePressed_;
    bool tabPressed_;
    bool tabShiftPressed_;
    // Set by a widget (codeEditor) that used this frame's Tab itself
    // (indent, accept an autocomplete suggestion) rather than leaving it for
    // advanceFocus() to move focus with. Reset every frame in consumeEvents.
    bool tabConsumedByWidget_;
    bool keyPressed_[32];
    bool keyControl_[32];
    bool keyShift_[32];

    static WidgetId hashText(StringView text);
    static WidgetId combineIds(WidgetId a, WidgetId b);
    WidgetId makeWidgetId(StringView label) const;

    void consumeEvents();
    WindowState *currentWindow();
    const WindowState *currentWindow() const;
    DrawList *currentDrawList();
    TextMetrics measureText(FontId font, StringView text, float logicalSize) const;
    void drawText(DrawList &drawList, FontId font, StringView text, const Vec2 &position,
                  float logicalSize, const Color &color, const Rect &clip);
    void drawDragDropPreview();
    void drawToasts();
    void beginLayout(const WindowState &window);
    void advanceLayout(const Rect &item);
    float autoButtonWidth(StringView label) const;
    WindowState *getOrCreateWindow(WidgetId id, StringView title, const Rect &bounds);
    Rect contentRect(const Rect &local) const;
    Rect contentClip() const;
    bool itemHovered(const Rect &rect, const Rect &clip, WidgetId id);
    bool itemClicked(const Rect &rect, const Rect &clip, WidgetId id, bool focusable = true);
    void registerFocusable(WidgetId id);
    void advanceFocus();
    bool sliderValue(const Rect &rect, const Rect &clip, WidgetId id,
                     float &value, float minimum, float maximum);
    WindowHandle topWindowAt(const Vec2 &position) const;
    void focusWindow(WindowHandle handle);
    bool currentWindowReceivesPointer() const;
    Rect dockSlotBounds(const DockSpaceState &dockSpace, DockSlot slot) const;
    bool beginDockSpaceInternal(StringView id, const Rect &outer, const Rect &clip);
    static uint32_t dockSlotIndex(DockSlot slot);
    float tableColumnX(int column) const;
    float tableColumnWidth(int column) const;
    bool menuItemInternal(StringView label, bool enabled, bool *checked);
    void drawWindow(WindowState &window);
    static uint32_t buttonIndex(PointerButton button);
    // fontSize is the editor's effective size (theme_.fontSize * the
    // CodeEditorState's zoom), never the theme's own fontSize directly - a
    // zoomed editor must scale every one of these consistently or the
    // caret/selection/click math drifts away from what is drawn.
    void drawHighlightedLine(DrawList &drawList, const CodeEditorState &state,
                             int line, const String &text, const Vec2 &position, const Rect &clip,
                             int tabSize, float fontSize);
    // Draws one run of plain text, honoring embedded '\t' bytes by advancing
    // to the next tab stop (tabSize columns, in space-widths) instead of
    // asking the font for a glyph it doesn't have. startVisCol is the
    // column the run starts at, needed to land on the correct tab stop when
    // a run doesn't start at column 0. Returns the x position past the run.
    float drawCodeRun(DrawList &drawList, const StringView &run, const Vec2 &position,
                      const Color &color, const Rect &clip, int tabSize, int startVisCol, float fontSize);
    // Pixel X offset of a column on a line, measuring the real glyph widths
    // of the prefix up to it - never assume a fixed per-character width,
    // the font is not guaranteed to be monospaced. A '\t' byte advances to
    // the next tab stop (tabSize columns, in space-widths) rather than
    // being measured as a glyph.
    float codeEditorColumnX(const CodeEditorState &state, int line, int column, int tabSize, float fontSize) const;
    // Inverse of codeEditorColumnX: the column whose prefix width is closest
    // to localX (measured from the start of the line, i.e. already relative
    // to the text area's left edge + padding).
    int codeEditorColumnAt(const CodeEditorState &state, int line, float localX, int tabSize, float fontSize) const;
    void updateCodeCompletion(CodeEditorState &state, const CodeEditorOptions &options);
    void acceptCodeCompletion(CodeEditorState &state);
};

} // namespace ig
