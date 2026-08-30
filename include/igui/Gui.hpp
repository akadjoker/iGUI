#pragma once

#include <stdint.h>

#include <ct/deque.hpp>
#include <ct/function.hpp>
#include <ct/hashmap.hpp>
#include <ct/slotmap.hpp>
#include <ct/vector.hpp>

#include "Backend.hpp"
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
    Cancelled
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
    bool inputTextMultiline(StringView label, String &value, const Rect &bounds);
    bool inputInt(StringView label, int &value, const Rect &bounds);
    bool inputFloat(StringView label, float &value, const Rect &bounds, int precision = 6);
    // Inline RGBA editor with a preview swatch and four draggable channels.
    bool colorEdit(StringView label, Color &value, const Rect &bounds);
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
    void showToast(StringView id, StringView text, ToastPosition position = ToastPosition::TopRight,
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
                            float height = 120.0f);
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
    // Weighted table columns. All weights must be positive and remain valid
    // until endTable(); for example, {3.0f, 1.0f} creates a 75/25 split.
    bool beginTable(StringView id, Span<const float> columnWeights, float width = 0.0f);
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

        VirtualTableState() : childId(InvalidWidgetId), columns(0) {}
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
        int columns;
        int column;
        float rowY;
        float rowHeight;
        ct::Vector<float> columnWeights;
        float totalColumnWeight;

        TableState()
            : parentLayout(), bounds(), columns(0), column(-1), rowY(0.0f), rowHeight(0.0f),
              columnWeights(), totalColumnWeight(0.0f) {}
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

        ToastState() : id(InvalidWidgetId), text(), position(ToastPosition::TopRight), remaining(0.0f) {}
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
    Rect activeMenuBounds_;
    Rect subMenuPopupBounds_;
    Rect subMenuParentBounds_;
    float menuBarCursorX_;
    bool menuBarActive_;
    bool forceWindowBounds_;
    TableState table_;
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
};

} // namespace ig
