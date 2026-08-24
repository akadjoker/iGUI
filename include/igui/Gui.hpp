#pragma once

#include <stdint.h>

#include <ct/deque.hpp>
#include <ct/hashmap.hpp>
#include <ct/slotmap.hpp>
#include <ct/vector.hpp>

#include "Backend.hpp"
#include "Events.hpp"
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
    uint64_t zOrder;
    DrawList drawList;
    DrawList overlayDrawList;

    WindowState()
        : id(InvalidWidgetId), title(), bounds(), open(true), minimized(false), focused(false),
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

class Context
{
public:
    explicit Context(Backend &backend, TextProvider *textProvider = nullptr);

    void pushEvent(const Event &event);
    void beginFrame(const FrameInfo &frame);
    const DrawData &endFrame();

    bool beginWindow(StringView title, const Rect &initialBounds, bool *open = nullptr);
    void endWindow();

    void pushId(uint64_t id);
    void pushId(StringView id);
    void popId();

    void setTheme(const Theme &theme);
    const Theme &theme() const;
    bool wantsPointer() const;
    bool wantsKeyboard() const;
    bool wantsTextInput() const;

    bool button(StringView label, const Rect &bounds);
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
    bool sliderFloat(StringView label, float &value, float minimum, float maximum,
                     const Rect &bounds);
    bool sliderInt(StringView label, int &value, int minimum, int maximum,
                   const Rect &bounds);
    // Horizontal relative drag. The value changes by speed for each moved pixel.
    bool dragFloat(StringView label, float &value, float minimum, float maximum,
                   float speed, const Rect &bounds);
    bool dragInt(StringView label, int &value, int minimum, int maximum,
                 int speed, const Rect &bounds);
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
    void progressBar(float value, float maximum, const Rect &bounds);
    // Draws an application-modal OK or OK/Cancel message box above all windows.
    MessageBoxResult messageBox(StringView title, StringView message, bool &open,
                                bool showCancel = false);
    MessageBoxResult messageBox(StringView title, StringView message, bool &open,
                                const MessageBoxOptions &options);
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

    bool button(StringView label);
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
    ct::Vector<WindowHandle> windowOrder_;
    ct::Vector<WidgetId> idStack_;
    ct::Vector<ToastState> toasts_;
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
    WidgetId activeModal_;
    WidgetId dragWidget_;
    String::size_type textCursor_;
    uint64_t frameNumber_;
    uint64_t nextZOrder_;
    Vec2 windowDragOffset_;
    float dragStartValue_;
    float dragStartX_;
    bool wantsKeyboard_;
    bool wantsTextInput_;
    bool backspacePressed_;
    bool enterPressed_;
    bool homePressed_;
    bool endPressed_;
    bool upPressed_;
    bool downPressed_;
    bool copyRequested_;
    bool pasteRequested_;

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
    bool itemClicked(const Rect &rect, const Rect &clip, WidgetId id);
    bool sliderValue(const Rect &rect, const Rect &clip, WidgetId id,
                     float &value, float minimum, float maximum);
    WindowHandle topWindowAt(const Vec2 &position) const;
    void focusWindow(WindowHandle handle);
    bool currentWindowReceivesPointer() const;
    void drawWindow(WindowState &window);
    static uint32_t buttonIndex(PointerButton button);
};

} // namespace ig
