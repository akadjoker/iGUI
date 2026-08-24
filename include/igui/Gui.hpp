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
    bool focused;
    uint64_t zOrder;
    DrawList drawList;

    WindowState()
        : id(InvalidWidgetId), title(), bounds(), open(true), focused(false),
          zOrder(0), drawList()
    {
    }
};

using WindowHandle = ct::Handle<WindowState>;

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
    bool comboBox(StringView label, int &currentItem, Span<const StringView> items,
                  const Rect &bounds);
    bool sliderFloat(StringView label, float &value, float minimum, float maximum,
                     const Rect &bounds);
    bool inputText(StringView label, String &value, const Rect &bounds);
    void progressBar(float value, float maximum, const Rect &bounds);
    void label(StringView text, const Vec2 &position);

    bool button(StringView label);
    bool checkbox(StringView label, bool &value);
    bool toggleSwitch(StringView label, bool &value);
    bool radioButton(StringView label, bool selected);
    bool selectable(StringView label, bool selected, float width = 0.0f);
    bool comboBox(StringView label, int &currentItem, Span<const StringView> items,
                  float width = 180.0f);
    bool sliderFloat(StringView label, float &value, float minimum, float maximum,
                     float width = 180.0f);
    bool inputText(StringView label, String &value, float width = 180.0f);
    void progressBar(float value, float maximum, float width = 180.0f);
    void label(StringView text);
    void sameLine(float spacing = -1.0f);
    void spacing(float pixels);
    void separator(float thickness = 1.0f);
    void setCursor(const Vec2 &localPosition);
    Vec2 cursor() const;

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
        Rect lastItem;
        bool hasLastItem;

        LayoutState() : origin(), cursor(), lastItem(), hasLastItem(false) {}
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
    ct::Vector<WindowHandle> windowOrder_;
    ct::Vector<WidgetId> idStack_;
    DrawList frameDrawList_;
    DrawData drawData_;
    WindowHandle currentWindow_;
    WindowHandle focusedWindow_;
    WidgetId activeWidget_;
    WidgetId hotWidget_;
    WidgetId focusedWidget_;
    WidgetId textInputWidget_;
    WidgetId openCombo_;
    uint64_t frameNumber_;
    uint64_t nextZOrder_;
    bool wantsKeyboard_;
    bool wantsTextInput_;
    bool backspacePressed_;

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
    static void eraseLastUtf8Codepoint(String &text);
    WindowHandle topWindowAt(const Vec2 &position) const;
    void focusWindow(WindowHandle handle);
    bool currentWindowReceivesPointer() const;
    void drawWindow(WindowState &window);
    static uint32_t buttonIndex(PointerButton button);
};

} // namespace ig
