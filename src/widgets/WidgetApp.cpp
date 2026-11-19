#include "Animation.hpp"     // Animator
#include "DialogWidgets.hpp" // Toast
#include "DockPanel.hpp"     // DockPanel layout persistence
#include "MenuWidgets.hpp"   // Menu::exec (for context menu)
#include "ViewWidgets.hpp"   // FloatWindow (real implementation)
#include "pch.hpp"

namespace ig
{
namespace retained
{

struct ContextMenu
{
    static void show(Menu* menu, float x, float y, Widget* /*owner*/)
    {
        if (menu) menu->exec(x, y);
    }
};

#include <cstring>

// ── helpers ───────────────────────────────────────────────────────────────

// The widgets Tab reaches, in DFS order, with `from` in its place even when Tab
// does not stop there.
static void collectFocusable(Widget* w, ct::Vector<Widget*>& out, const Widget* from)
{
    if (!w || !w->isVisible() || !w->isEnabled()) return;
    if ((w->acceptsFocus() && w->isTabStop()) || w == from) out.push_back(w);
    if (!w->childrenShown()) return;
    for (auto* c: w->children()) collectFocusable(c, out, from);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Singleton
// ═════════════════════════════════════════════════════════════════════════════

namespace
{
WidgetApp* gWidgetApp = nullptr;
}

WidgetApp& WidgetApp::instance()
{
    static WidgetApp app;
    if (!gWidgetApp) gWidgetApp = &app;
    return app;
}

WidgetApp* WidgetApp::existing()
{
    return gWidgetApp;
}

void WidgetApp::swapSurface(Surface& other)
{
    Surface tmp(static_cast<Surface&&>(s_));
    s_ = static_cast<Surface&&>(other);
    other = static_cast<Surface&&>(tmp);
}

void WidgetApp::releaseSurface()
{
    saveWindowState();
    flushWidgetDeletes();
    // Everything of the window goes now. The destructors below tell the manager about each widget,
    // and what it would do with that (give a lent focus back, walk the trees, cancel a drag) reads
    // the surface, whose root and floats are freed one by one here: it is skipped until the end.
    releasing_ = true;
    if (s_.popupOwned_) delete s_.popup_;
    if (s_.popupClosingOwned_) delete s_.popupClosing_;
    flushPopupDeletes();
    for (auto& [name, stageRoot]: s_.stages_)
    {
        if (stageRoot != s_.root_) delete stageRoot;
    }
    delete s_.root_;
    // The float lists leave the surface before their windows go: a destructor may look a float up
    // and remove it (FileDialog removes its new-folder prompt), which would edit the list under the loop.
    auto floats = static_cast<decltype(s_.floats_)&&>(s_.floats_);
    auto pendingFloats = static_cast<decltype(s_.pendingFloatDeletes_)&&>(s_.pendingFloatDeletes_);
    s_.floats_.clear();
    s_.pendingFloatDeletes_.clear();
    for (auto* fw: floats) delete fw;
    for (auto* fw: pendingFloats) delete fw;
    s_ = Surface();
    releasing_ = false;
}

WidgetApp::~WidgetApp()
{
    shutdown();
    // Widgets destroyed after this (static destruction order) must not try to
    // tell the manager about it any more.
    gWidgetApp = nullptr;
}

void WidgetApp::shutdown()
{
    inited_ = false;
    flushWidgetDeletes();

    // Write the float window geometry and dock layouts out before the windows
    // and stages they belong to go away. A no-op when persistence is off.
    saveWindowState();

    // Clean up popups BEFORE root
    if (s_.popupOwned_) delete s_.popup_;
    s_.popup_ = nullptr;
    s_.popupOwner_ = nullptr;
    s_.popupOwned_ = true;

    if (s_.popupClosingOwned_) delete s_.popupClosing_;
    s_.popupClosing_ = nullptr;
    s_.popupClosingOwner_ = nullptr;
    s_.popupClosingOwned_ = true;

    flushPopupDeletes();

    for (auto& [name, stageRoot]: s_.stages_)
    {
        if (stageRoot != s_.root_) delete stageRoot;
    }
    s_.stages_.clear();

    delete s_.root_;
    s_.root_ = nullptr;
    for (auto* fw: s_.floats_) delete fw;
    s_.floats_.clear();
    // Windows closed since the last frame were only queued for deletion.
    for (auto* fw: s_.pendingFloatDeletes_) delete fw;
    s_.pendingFloatDeletes_.clear();
    delete iconAtlas_;
    iconAtlas_ = nullptr; // stub
}

// ═════════════════════════════════════════════════════════════════════════════
//  Init
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::setTheme(const Theme& theme, ThemeParts parts)
{
    switch (parts)
    {
        case ThemeParts::Colors:
            // the colours only: no size a layout reads changes, so no layout is asked for
            Theme::instance().applyColors(theme);
            themeNotify(false);
            return;
        case ThemeParts::Metrics:
            Theme::instance().applyMetrics(theme);
            break;
        case ThemeParts::All:
            Theme::instance() = theme;
            break;
    }
    notifyThemeChanged();
}

bool WidgetApp::setThemePreset(const char* name, ThemeParts parts)
{
    if (!name) return false;
    for (int i = 0; i < Theme::kPresetCount; ++i)
    {
        if (strcmp(name, Theme::presetName(i)) != 0) continue;
        setTheme(Theme::byName(name), parts);
        return true;
    }
    return false;
}

void WidgetApp::notifyThemeChanged()
{
    themeNotify(true);
}

void WidgetApp::themeNotify(bool relayout)
{
    // the copy first: a handler that changes the theme again is a change of its own
    memcpy(static_cast<void*>(&themeSeen_), static_cast<const void*>(&Theme::instance()), sizeof(Theme));
    themeSeenSet_ = true;
    s_.forcePaint_ = true;
    // sizes that come from the theme (font size, padding, bars) are worked out again at the next layout
    if (relayout) requestRelayout();
    themeChanged.emit();
}

bool WidgetApp::init()
{
    if (inited_) return true;

    iconAtlas_ = nullptr; // IconAtlas not yet compiled

    s_.root_ = nullptr;
    // WidgetSerializer::registerBuiltinTypes();  // re-enable when migrated

    inited_ = true;
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Per-frame input dispatch
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::update(ig::retained::IO& io)
{
    BusyScope busy(*this);
    // A theme changed by hand since the last frame: tell the widgets (one memcmp of the theme, a kilobyte, a frame).
    if (!themeSeenSet_)
    {
        memcpy(static_cast<void*>(&themeSeen_), static_cast<const void*>(&Theme::instance()), sizeof(Theme));
        themeSeenSet_ = true;
    }
    else if (watchTheme_ && memcmp(static_cast<const void*>(&Theme::instance()), static_cast<const void*>(&themeSeen_), sizeof(Theme)) != 0)
        notifyThemeChanged();
    dt_ = io.deltaTime;
    // The time the loop slept is for the clock (a blink, the time between two clicks) and the tooltip delay, not for animations (IO::idleSeconds).
    idle_ = io.idleSeconds > 0.0f ? io.idleSeconds : 0.0f;
    io.idleSeconds = 0.0f;
    elapsedSeconds_ += (dt_ > 0.0f ? static_cast<double>(dt_) : 0.0) + static_cast<double>(idle_);
    // Where requestFrameFromAnyThread() wakes the loop (read from other threads: see there).
    __atomic_store_n(&wakeLoop_, io.wakeLoop, __ATOMIC_RELEASE);
    elapsedMs_ = static_cast<uint32_t>(fmod(elapsedSeconds_ * 1000.0, 4294967296.0));

    // Cache modifier keys & clipboard callbacks from IO
    ioKeyCtrl_ = io.keyCtrl;
    ioKeyShift_ = io.keyShift;
    ioKeyAlt_ = io.keyAlt;
    clipSet_ = io.setClipboardText;
    clipGet_ = io.getClipboardText;
    primarySet_ = io.setPrimaryText;
    primaryGet_ = io.getPrimaryText;
    mouseCapture_ = io.setMouseCaptured;
    mouseWarp_ = io.warpMouse;

    // Whether this frame has anything for the widgets to do: an event, the pointer moved or a button changed since the last one, the
    // wheel, a request from another thread. A frame with none draws what the last one drew, unless time or a widget says otherwise
    // (needsFrame).
    if (!io.mouseEvents.empty() || !io.keyEvents.empty() || !io.inputChars.empty() || !io.dropEvents.empty() || io.mouseX != s_.prevMouseX_ ||
            io.mouseY != s_.prevMouseY_ || io.mouseWheelX != 0.0f || io.mouseWheelY != 0.0f)
        s_.inputSeen_ = true;
    for (int i = 0; i < 5; ++i)
        if (io.mouseDown[i] != s_.prevMouseDown_[i]) s_.inputSeen_ = true;
    if (__atomic_exchange_n(&wakeFromThread_, 0, __ATOMIC_ACQ_REL) != 0) s_.forcePaint_ = true;

    int newW = static_cast<int>(io.displayWidth);
    int newH = static_cast<int>(io.displayHeight);
    if (newW != s_.width_ || newH != s_.height_)
    {
        s_.forcePaint_ = true;
        s_.width_ = newW;
        s_.height_ = newH;
        if (s_.width_ != 0 && s_.height_ != 0)
        {
            s_.needsLayout_ = true;
            for (FloatWindow* fw: s_.floats_) keepOnScreen(fw);
        }
    }

    float mx = io.mouseX;
    float my = io.mouseY;

    // What the platform's event stream said, in order, before the state the frame ends with: each press,
    // release and move is dispatched where it happened (a click that began and ended between two frames
    // is not lost, a press is where the pointer was). The previous state follows each, so the comparison
    // with the polled state below finds only what the events did not say; and a button the events spoke of
    // is not compared with the polled state at all (a platform may hold a press in its polled state for a
    // frame after the release, which would be a second press at the end of the frame).
    //
    // The events come out of the IO before the first handler runs. A handler may run a nested loop (a modal
    // dialog pumps the frames of its own window from the click that opened it, ZenApp::pump): its frames
    // put the events of that window into this same IO and the first Render clears it. What this frame is
    // dispatching must not move or vanish under it (a vector that grows while it is walked is freed memory
    // read), and the nested frame must find its own events only, not these again: so they are taken, by swapping
    // with vectors kept between frames (the IO and the buffer trade their storage, nothing is allocated once
    // both have room), and what is added to the IO from here on belongs to the next frame to take it.
    FrameEvents* buffer = nullptr;       // where they were taken to; none when the IO has no events
    FrameEvents* nestedEvents = nullptr; // a buffer of its own, for an update() nested in another that is dispatching
    if (!io.mouseEvents.empty() || !io.keyEvents.empty() || !io.inputChars.empty() || !io.dropEvents.empty())
    {
        if (frameEventsBusy_) buffer = nestedEvents = new FrameEvents;
        else
        {
            buffer = &frameEvents_;
            frameEventsBusy_ = true;
        }
        buffer->mouse.swap(io.mouseEvents);
        buffer->keys.swap(io.keyEvents);
        buffer->chars.swap(io.inputChars);
        buffer->drops.swap(io.dropEvents);
    }
    const FrameEvents& events = buffer ? *buffer : noEvents_;

    bool spokenOf[5] = {};
    for (size_t e = 0; e < events.mouse.size(); ++e)
    {
        const ig::retained::IO::MouseInput& me = events.mouse[e];
        if (me.button < 0 || me.button >= 5)
        {
            if (me.x != s_.prevMouseX_ || me.y != s_.prevMouseY_) dispatchMouseMove(me.x, me.y);
            s_.prevMouseX_ = me.x;
            s_.prevMouseY_ = me.y;
            continue;
        }
        if (me.x != s_.prevMouseX_ || me.y != s_.prevMouseY_) dispatchMouseMove(me.x, me.y);
        s_.prevMouseX_ = me.x;
        s_.prevMouseY_ = me.y;
        if (me.down && !s_.prevMouseDown_[me.button]) dispatchMousePress(me.x, me.y, me.button);
        else if (!me.down && s_.prevMouseDown_[me.button])
            dispatchMouseRelease(me.x, me.y, me.button);
        s_.prevMouseDown_[me.button] = me.down;
        spokenOf[me.button] = true;
    }

    // Mouse move
    if (mx != s_.prevMouseX_ || my != s_.prevMouseY_) dispatchMouseMove(mx, my);

    // Mouse buttons (delta detection)
    for (int i = 0; i < 5; ++i)
    {
        if (spokenOf[i]) continue;
        if (io.mouseDown[i] && !s_.prevMouseDown_[i]) dispatchMousePress(mx, my, i);
        else if (!io.mouseDown[i] && s_.prevMouseDown_[i])
            dispatchMouseRelease(mx, my, i);
    }

    // Scroll
    if (io.mouseWheelX != 0.0f || io.mouseWheelY != 0.0f)
        dispatchMouseScroll(mx, my, io.mouseWheelX, io.mouseWheelY);

    // Key press / release events, and the letters typed before each of them put in first, so that
    // "a", Left, "b" in one frame is what it was typed as
    unsigned charsDone = 0;
    ct::Vector<uint32_t> typedBefore;
    for (size_t e = 0; e < events.keys.size(); ++e)
    {
        const ig::retained::IO::KeyEvent& ke = events.keys[e];
        if (ke.charsBefore > charsDone && ke.charsBefore <= events.chars.size())
        {
            typedBefore.clear();
            for (unsigned i = charsDone; i < ke.charsBefore; ++i) typedBefore.push_back(events.chars[i]);
            charsDone = ke.charsBefore;
            dispatchTextInput(typedBefore);
        }
        if (debugLayoutKey_ != 0 && ke.scancode == debugLayoutKey_)
        {
            if (ke.down) debugLayout_ = !debugLayout_;
        }
        else
            dispatchKeyEvent(ke);
    }

    // Text input (what is left of it)
    if (charsDone == 0)
    {
        if (!events.chars.empty()) dispatchTextInput(events.chars);
    }
    else if (charsDone < events.chars.size())
    {
        typedBefore.clear();
        for (size_t i = charsDone; i < events.chars.size(); ++i) typedBefore.push_back(events.chars[i]);
        dispatchTextInput(typedBefore);
    }

    // OS-level file drops
    if (!events.drops.empty()) dispatchDropEvents(events.drops);

    // Dispatched: the buffer is empty again (and keeps its room) for the next frame, or for the one this was nested in.
    if (buffer)
    {
        buffer->mouse.clear();
        buffer->keys.clear();
        buffer->chars.clear();
        buffer->drops.clear();
        if (nestedEvents) delete nestedEvents;
        else
            frameEventsBusy_ = false;
    }

    // Calls deferred by the handlers above, in the order they were made; one
    // deferred while they run joins this same pass.
    for (ct::Vector<Surface::DeferredCall>::size_type i = 0; i < s_.deferredCalls_.size(); ++i)
    {
        Surface::DeferredCall call = std::move(s_.deferredCalls_[i]);
        if (call.owner) call.fn();
    }
    s_.deferredCalls_.clear();

    // Tooltip timer
    if (s_.hovered_ && s_.hovered_->hasTooltip())
    {
        s_.tooltipTimer_ += dt_ + idle_;
        if (!s_.tooltipVisible_ && s_.tooltipTimer_ >= s_.hovered_->tooltipDelay())
        {
            s_.tooltipVisible_ = true;
            s_.tooltipWidget_ = s_.hovered_;
        }
    }
    else
    {
        s_.tooltipTimer_ = 0.0f;
        s_.tooltipVisible_ = false;
        s_.tooltipWidget_ = nullptr;
    }

    // Flush deferred deletes. A destructor running here can queue another window
    // (a dialog closing the prompt it opened), so the queue is drained until it
    // is empty instead of iterating a vector that may grow under it.
    while (!s_.pendingFloatDeletes_.empty())
    {
        FloatWindow* queued = s_.pendingFloatDeletes_.front();
        s_.pendingFloatDeletes_.erase(s_.pendingFloatDeletes_.begin());
        delete queued;
    }
    flushPopupDeletes();

    // A window state file read before the stages existed: hand the dock
    // layouts over now that the panels they name are there.
    if (s_.dockLayoutPending_) restoreDockLayouts();

    // Tick animations
    Animator::instance().tick(dt_);

    // Tick popup animation
    if (s_.popupAnimOpening_)
    {
        s_.popupAnimProgress_ += dt_ / s_.popupAnimDuration_;
        if (s_.popupAnimProgress_ >= 1.0f)
        {
            s_.popupAnimProgress_ = 1.0f;
            s_.popupAnimOpening_ = false;
        }
    }
    if (s_.popupAnimClosing_)
    {
        s_.popupAnimProgress_ -= dt_ / s_.popupAnimDuration_;
        if (s_.popupAnimProgress_ <= 0.0f)
        {
            s_.popupAnimProgress_ = 0.0f;
            s_.popupAnimClosing_ = false;
            // Now actually destroy
            if (s_.popupClosing_)
            {
                retirePopup(s_.popupClosing_, s_.popupClosingOwned_);
                s_.popupClosing_ = nullptr;
                s_.popupClosingOwner_ = nullptr;
                s_.popupClosingOwned_ = true;
            }
        }
    }

    // Store prev state
    s_.prevMouseX_ = mx;
    s_.prevMouseY_ = my;
    for (int i = 0; i < 5; ++i)
        if (!spokenOf[i]) s_.prevMouseDown_[i] = io.mouseDown[i];

    // Write cursor hint into IO so backend can read it
    ig::retained::GetIO().wantedCursor = static_cast<int>(s_.wantedCursor_);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Synthetic input
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::makeSyntheticIO(ig::retained::IO& io, bool shift, bool ctrl, bool alt) const
{
    // what update() compares the IO with, so that it finds nothing to do but the one event that is added
    io.deltaTime = 0.0f;
    io.displayWidth = static_cast<float>(s_.width_);
    io.displayHeight = static_cast<float>(s_.height_);
    io.mouseX = s_.prevMouseX_;
    io.mouseY = s_.prevMouseY_;
    for (int i = 0; i < 5; ++i) io.mouseDown[i] = s_.prevMouseDown_[i];
    io.keyShift = shift;
    io.keyCtrl = ctrl;
    io.keyAlt = alt;
    // update() keeps these from the IO it is given: a feed must not lose the backend's
    io.setClipboardText = clipSet_;
    io.getClipboardText = clipGet_;
    io.setPrimaryText = primarySet_;
    io.getPrimaryText = primaryGet_;
    io.setMouseCaptured = mouseCapture_;
    io.warpMouse = mouseWarp_;
}

// The pointer the events left, written where a backend writes it: the next frame, update(GetIO()), then finds the state
// it already has and does not move the pointer back to the position GetIO() had.
void WidgetApp::syncPointerToGlobalIO()
{
    ig::retained::IO& global = ig::retained::GetIO();
    global.mouseX = s_.prevMouseX_;
    global.mouseY = s_.prevMouseY_;
    for (int i = 0; i < 5; ++i) global.mouseDown[i] = s_.prevMouseDown_[i];
}

void WidgetApp::feedMouseMove(float x, float y)
{
    ig::retained::IO io;
    makeSyntheticIO(io, ioKeyShift_, ioKeyCtrl_, ioKeyAlt_);
    io.mouseX = x;
    io.mouseY = y;
    io.addMouseMoveEvent(x, y);
    update(io);
    syncPointerToGlobalIO();
}

void WidgetApp::feedMouseButton(int button, bool down, bool shift, bool ctrl, bool alt)
{
    if (button < 0 || button >= 5) return;
    ig::retained::IO io;
    makeSyntheticIO(io, shift, ctrl, alt);
    io.mouseDown[button] = down;
    io.addMouseButtonEvent(io.mouseX, io.mouseY, button, down);
    update(io);
    syncPointerToGlobalIO();
}

void WidgetApp::feedMouseWheel(float dx, float dy, bool shift, bool ctrl, bool alt)
{
    ig::retained::IO io;
    makeSyntheticIO(io, shift, ctrl, alt);
    io.mouseWheelX = dx;
    io.mouseWheelY = dy;
    update(io);
    syncPointerToGlobalIO();
}

void WidgetApp::feedKey(int key, bool down, bool shift, bool ctrl, bool alt)
{
    ig::retained::IO io;
    makeSyntheticIO(io, shift, ctrl, alt);
    io.addKeyEvent(key, key, shift, ctrl, alt, down);
    update(io);
    syncPointerToGlobalIO();
}

void WidgetApp::feedText(const char* utf8)
{
    if (!utf8 || !*utf8) return;
    ig::retained::IO io;
    makeSyntheticIO(io, ioKeyShift_, ioKeyCtrl_, ioKeyAlt_);
    while (*utf8)
    {
        int len = 1;
        io.addInputCharacter(utf8Decode(utf8, len));
        utf8 += len > 0 ? len : 1;
    }
    update(io);
    syncPointerToGlobalIO();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Input dispatch helpers (formerly SDL event handlers)
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::dispatchMouseMove(float x, float y)
{
    const float relX = x - s_.mouseX_;
    const float relY = y - s_.mouseY_;
    s_.mouseX_ = x;
    s_.mouseY_ = y;

    if (s_.pressed_)
    {
        // Check drag threshold
        if (s_.dragPending_ && !s_.dragPayload_ && s_.dragSource_)
        {
            float dx = x - s_.dragStartX_;
            float dy = y - s_.dragStartY_;
            if (dx * dx + dy * dy >= dragThreshold_ * dragThreshold_)
            {
                auto* p = new DragPayload(s_.dragSource_->onDragBegin());
                p->source = s_.dragSource_;
                s_.dragPayload_ = p;
                s_.dragPending_ = false;
            }
        }
        applyCursor(s_.dragPayload_ ? CursorType::SizeAll : s_.pressed_->cursor());
        MouseEvent me = makeMouseEvent(x, y);
        me.relX = relX;
        me.relY = relY;
        bubble(s_.pressed_, me, &Widget::onMouseMove);
        if (s_.dragPayload_)
            for (Widget* w = topWidgetAt(x, y); w; w = w->parent())
                w->onDragHover(*s_.dragPayload_, x, y);
        return;
    }

    Widget* hit = topWidgetAt(x, y);
    if (hit != s_.hovered_) updateHover(hit);

    if (s_.hovered_)
    {
        MouseEvent me = makeMouseEvent(x, y);
        me.relX = relX;
        me.relY = relY;
        bubble(s_.hovered_, me, &Widget::onMouseMove);
        // Re-read cursor — walk chain leaf→root, pick first non-Arrow
        CursorType cur = CursorType::Arrow;
        for (auto it = s_.hoverChain_.rbegin(); it != s_.hoverChain_.rend(); ++it)
        {
            if ((*it)->cursor() != CursorType::Arrow)
            {
                cur = (*it)->cursor();
                break;
            }
        }
        applyCursor(cur);
    }
}

void WidgetApp::dispatchMousePress(float x, float y, int btn)
{
    s_.mouseX_ = x;
    s_.mouseY_ = y;

    // ── Double/triple-click detection ────────────────────────────────────
    constexpr uint32_t kDblClickMs = 400; // max ms between clicks
    constexpr float kDblClickDist = 5.0f; // max pixel drift
    float dx = x - s_.lastClickX_, dy = y - s_.lastClickY_;
    bool sameSpot = (dx * dx + dy * dy) < kDblClickDist * kDblClickDist;
    bool sameBtn = (btn == s_.lastClickBtn_);
    bool inTime = (elapsedMs_ - s_.lastClickMs_) < kDblClickMs;
    if (sameBtn && sameSpot && inTime) s_.clickSeq_ = std::min(s_.clickSeq_ + 1, 3);
    else
        s_.clickSeq_ = 1;
    s_.lastClickMs_ = elapsedMs_;
    s_.lastClickX_ = x;
    s_.lastClickY_ = y;
    s_.lastClickBtn_ = btn;

    // While a button is held, the other buttons go to the widget that took it.
    const int bit = 1 << btn;
    if (s_.pressedButtons_ != 0 && (s_.pressedButtons_ & bit) == 0)
    {
        s_.pressedButtons_ |= bit;
        if (s_.pressed_)
        {
            MouseEvent me = makeMouseEvent(x, y, btn);
            bubble(s_.pressed_, me, &Widget::onMousePress);
        }
        return;
    }

    if (s_.popup_)
    {
        Widget* popupHit = hitTest(s_.popup_, x, y);
        if (!popupHit && s_.popup_->popupContains(x, y)) popupHit = s_.popup_;
        if (popupHit)
        {
            // a popup of widgets (PopupPanel): the press gives the focus to the widget under it that takes it
            if (s_.popup_->popupHoldsWidgets() && popupHit->acceptsFocus() && popupHit->isEnabledInTree() &&
                    popupHit != s_.focused_)
                setFocused(popupHit);
            MouseEvent me = makeMouseEvent(x, y, btn);
            bubble(popupHit, me, &Widget::onMousePress);
            if (s_.popup_) s_.pressed_ = popupHit;
            if (s_.pressed_) s_.pressedButtons_ = bit;
            return;
        }
        bool ownerToggle = (btn == 0) && s_.popupOwner_ && hitTest(s_.popupOwner_, x, y) != nullptr;
        if (!ownerToggle) closePopup();
    }

    for (int i = static_cast<int>(s_.floats_.size()) - 1; i >= 0; --i)
    {
        auto* fw = s_.floats_[i];
        if (!fw->isVisible()) continue;
        Widget* fwHit = hitTest(fw, x, y);
        if (fwHit)
        {
            bringToFront(fw);
            if (fwHit->acceptsFocus() && fwHit->isEnabledInTree() && fwHit != s_.focused_)
                setFocused(fwHit);
            MouseEvent me = makeMouseEvent(x, y, btn);
            bubble(fwHit, me, &Widget::onMousePress);
            // fw may have been removed (and queued for deletion) during the event —
            // only keep pressed_ if the window is still alive in the float list.
            if (std::find(s_.floats_.begin(), s_.floats_.end(), fw) != s_.floats_.end()) s_.pressed_ = fwHit;
            if (s_.pressed_) s_.pressedButtons_ = bit;
            return;
        }
    }

    Widget* hit = hitTest(s_.root_, x, y);
    if (hit)
    {
        if (btn == 1)
        {
            Widget* w = hit;
            while (w)
            {
                if (w->contextMenu())
                {
                    ContextMenu::show(w->contextMenu(), x, y, w);
                    return;
                }
                // a widget with a menu of its own (a text field's Cut, Copy, Paste...) gets the press, which opens it: a menu of one of its parents does not
                if (w->ownsContextMenu()) break;
                w = w->parent();
            }
        }
        if (hit->acceptsFocus() && hit->isEnabledInTree() && hit != s_.focused_) setFocused(hit);

        // Check for drag source on left-button press
        if (btn == 0 && !s_.dragPending_)
        {
            Widget* ds = hit;
            while (ds)
            {
                if (ds->isDragSource())
                {
                    s_.dragPending_ = true;
                    s_.dragSource_ = ds;
                    s_.dragStartX_ = x;
                    s_.dragStartY_ = y;
                    break;
                }
                ds = ds->parent();
            }
        }

        MouseEvent me = makeMouseEvent(x, y, btn);
        bubble(hit, me, &Widget::onMousePress);
        // The handler may have removed the widget, or the window that holds it.
        s_.pressed_ = isWidgetInTrees(hit) ? hit : nullptr;
        if (s_.pressed_) s_.pressedButtons_ = bit;
    }
    else
    {
        setFocused(nullptr);
    }
}

void WidgetApp::dispatchMouseRelease(float x, float y, int btn)
{
    s_.mouseX_ = x;
    s_.mouseY_ = y;

    // Complete widget-to-widget drag & drop on left-button release
    if (btn == 0 && s_.dragPayload_)
    {
        Widget* target = nullptr;
        for (int i = static_cast<int>(s_.floats_.size()) - 1; i >= 0; --i)
        {
            auto* fw = s_.floats_[i];
            if (!fw->isVisible()) continue;
            target = hitTest(fw, x, y);
            if (target) break;
        }
        if (!target) target = hitTest(s_.root_, x, y);

        bool accepted = false;
        s_.dragPayload_->dropX = x;
        s_.dragPayload_->dropY = y;
        if (target)
        {
            // Walk up from target to find an acceptor
            Widget* w = target;
            while (w)
            {
                if (w->acceptsDrop(*s_.dragPayload_))
                {
                    w->onDropReceive(*s_.dragPayload_);
                    accepted = true;
                    break;
                }
                w = w->parent();
            }
        }
        if (s_.dragSource_) s_.dragSource_->onDragEnd(accepted);
        delete s_.dragPayload_;
        s_.dragPayload_ = nullptr;
        s_.dragSource_ = nullptr;
        s_.dragPending_ = false;
    }
    if (btn == 0)
    {
        s_.dragPending_ = false;
        s_.dragSource_ = nullptr;
    }

    s_.pressedButtons_ &= ~(1 << btn);
    if (s_.pressed_)
    {
        MouseEvent me = makeMouseEvent(x, y, btn);
        bubble(s_.pressed_, me, &Widget::onMouseRelease);
        if (s_.pressedButtons_ == 0)
        {
            s_.pressed_ = nullptr;
            Widget* hit2 = topWidgetAt(x, y);
            if (hit2 != s_.hovered_) updateHover(hit2);
        }
    }
}

// ── MouseEvent with current IO modifiers ─────────────────────────────────
MouseEvent WidgetApp::makeMouseEvent(float x, float y, int btn) const
{
    MouseEvent me;
    me.x = x;
    me.y = y;
    me.button = btn;
    me.clickCount = s_.clickSeq_;
    me.ctrl = ioKeyCtrl_;
    me.shift = ioKeyShift_;
    me.alt = ioKeyAlt_;
    return me;
}

// ── Clipboard forwarding ─────────────────────────────────────────────────
void WidgetApp::setClipboardText(const char* text)
{
    if (clipSet_) clipSet_(text);
}

String WidgetApp::getClipboardText() const
{
    if (clipGet_) return clipGet_();
    return {};
}

void WidgetApp::setPrimaryText(const char* text)
{
    if (primarySet_) primarySet_(text);
}

String WidgetApp::getPrimaryText() const
{
    if (primaryGet_) return primaryGet_();
    return getClipboardText();
}

// The widget the buttons went to stops being shown: it gets the release of
// every button still held, as if they had gone up.
void WidgetApp::dropMouseFocus()
{
    Widget* target = s_.pressed_;
    const int buttons = s_.pressedButtons_;
    s_.pressed_ = nullptr;
    s_.pressedButtons_ = 0;
    if (!target) return;
    for (int i = 0; i < 3; ++i)
    {
        if ((buttons & (1 << i)) == 0) continue;
        if (!isWidgetInTrees(target)) return;
        MouseEvent me = makeMouseEvent(s_.mouseX_, s_.mouseY_, i);
        bubble(target, me, &Widget::onMouseRelease);
    }
}

void WidgetApp::focusNeighbor(Widget* from, bool backward)
{
    ct::Vector<Widget*> focusable;
    if (s_.popup_ && s_.popup_->popupHoldsWidgets()) collectFocusable(s_.popup_, focusable, from); // Tab stays in the popup
    else
    {
        collectFocusable(s_.root_, focusable, from);
        for (auto* fw: s_.floats_) collectFocusable(fw, focusable, from);
    }
    if (focusable.empty()) return;

    auto it = std::find(focusable.begin(), focusable.end(), from);
    if (backward)
    {
        if (it == focusable.begin() || it == focusable.end()) it = focusable.end();
        --it;
    }
    else
    {
        if (it != focusable.end()) ++it;
        if (it == focusable.end()) it = focusable.begin();
    }
    if (*it == from && !from->isTabStop()) return;
    s_.focusFromTab_ = true;
    setFocused(*it);
    s_.focusFromTab_ = false;
}

void WidgetApp::callDeferred(Widget* owner, ct::Function<void()> fn)
{
    s_.deferredCalls_.push_back({ owner, std::move(fn) });
}

void WidgetApp::setMouseCaptured(bool captured)
{
    if (mouseCapture_) mouseCapture_(captured);
}

void WidgetApp::warpMouse(float x, float y)
{
    if (mouseWarp_) mouseWarp_(x, y);
    s_.mouseX_ = x;
    s_.mouseY_ = y;
}

void WidgetApp::dispatchMouseScroll(float x, float y, float sx, float sy)
{
    if (!s_.hovered_) return;
    MouseEvent me = makeMouseEvent(x, y);
    me.scrollX = sx;
    me.scrollY = sy;
    bubble(s_.hovered_, me, &Widget::onMouseScroll);
}

void WidgetApp::dispatchKeyEvent(const ig::retained::IO::KeyEvent& ke)
{
    // ── Tab / Shift+Tab focus cycling ────────────────────────────────────
    if (ke.down && ke.key == ig::retained::Key::Tab && !ke.ctrl && !ke.alt)
    {
        // First give the focused widget a chance to consume Tab
        if (s_.focused_)
        {
            KeyEvent we;
            we.key = ke.key;
            we.scancode = ke.scancode;
            we.shift = ke.shift;
            we.ctrl = ke.ctrl;
            we.alt = ke.alt;
            we.super = ke.super;
            s_.focused_->onKeyPress(we);
            if (we.consumed) return;
        }
        focusNeighbor(s_.focused_, ke.shift);
        return;
    }

    KeyEvent we;
    we.key = ke.key;
    we.scancode = ke.scancode;
    we.shift = ke.shift;
    we.ctrl = ke.ctrl;
    we.alt = ke.alt;
    we.super = ke.super;
    // A key that made a widget go away was handled, consumed or not.
    const uint32_t deletionsBefore = deletions_;
    if (s_.focused_) bubbleKey(we, ke.down ? &Widget::onKeyPress : &Widget::onKeyRelease);
    if (ke.down && !we.consumed && deletions_ == deletionsBefore) unhandledKey.emit(we);
}

void WidgetApp::bubbleKey(KeyEvent& e, void (Widget::*handler)(KeyEvent&))
{
    // the ancestors: on the stack (a tree is not deeper than a few dozen; deeper ones go to the heap), so that an event allocates nothing
    Widget* stack[32];
    ct::Vector<Widget*> deep;
    size_t count = 0;
    for (Widget* w = s_.focused_; w; w = w->parent())
    {
        if (count < 32) stack[count++] = w;
        else
        {
            if (deep.empty())
                for (size_t k = 0; k < 32; ++k) deep.push_back(stack[k]);
            deep.push_back(w);
        }
    }
    Widget** chain = deep.empty() ? stack : deep.data();
    if (!deep.empty()) count = deep.size();

    uint32_t seen = deletions_;
    for (size_t i = 0; i < count && !e.consumed; ++i)
    {
        Widget* w = chain[i];
        if (seen != deletions_)
        {
            if (!isWidgetInTrees(w)) return;
            seen = deletions_;
        }
        if (w->isEnabledInTree()) (w->*handler)(e);
    }
}

void WidgetApp::dispatchTextInput(const ct::Vector<uint32_t>& chars)
{
    if (!s_.focused_) return;
    // A text event carries 28 bytes. More than that in one frame (a slow frame, an input method
    // committing a phrase) goes in several events, in order: the characters past the first 28
    // bytes used to be dropped.
    KeyEvent ke;
    int off = 0;
    auto flush = [&]()
    {
        if (off == 0) return;
        ke.text[off] = 0;
        bubbleKey(ke, &Widget::onTextInput);
        ke = KeyEvent();
        off = 0;
    };
    for (uint32_t cp: chars)
    {
        char utf8[4];
        int n = 0;
        // Convert UTF-32 codepoints to UTF-8
        if (cp < 0x80) utf8[n++] = static_cast<char>(cp);
        else if (cp < 0x800)
        {
            utf8[n++] = static_cast<char>(0xC0 | (cp >> 6));
            utf8[n++] = static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            utf8[n++] = static_cast<char>(0xE0 | (cp >> 12));
            utf8[n++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            utf8[n++] = static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (cp <= 0x10FFFF)
        {
            utf8[n++] = static_cast<char>(0xF0 | (cp >> 18));
            utf8[n++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            utf8[n++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            utf8[n++] = static_cast<char>(0x80 | (cp & 0x3F));
        }
        if (n == 0) continue; // not a character
        if (off + n > 28) flush();
        if (!s_.focused_) return; // a handler took the focus away or deleted the widget
        for (int i = 0; i < n; ++i) ke.text[off++] = utf8[i];
    }
    flush();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Root management
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::setRoot(Widget* r)
{
    if (s_.root_ != r)
    {
        Widget* old = s_.root_;
        s_.root_ = r;
        if (old)
        {
            // The old root may be a stage's root: that stage goes with it (its table entry would
            // point at freed memory), and a click that replaces the whole interface may be
            // running inside it, so it is deleted when the frame phase is over.
            ct::Vector<String> gone;
            for (auto& [name, stageRoot]: s_.stages_)
                if (stageRoot == old) gone.push_back(name);
            for (const String& name: gone)
            {
                s_.stages_.erase(name);
                if (name == s_.currentStage_) s_.currentStage_.clear();
            }
            notifyWidgetRemoved(old);
            deleteLater(old);
        }
    }
    s_.focused_ = nullptr;
    s_.hovered_ = nullptr;
    s_.pressed_ = nullptr;
    s_.pressedButtons_ = 0;
    s_.hoverChain_.clear();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Stage management
// ═════════════════════════════════════════════════════════════════════════════

Widget* WidgetApp::addStage(const String& name)
{
    Widget* const* existing = s_.stages_.find(name);
    if (existing) return *existing;

    auto* stageRoot = new Widget();
    s_.stages_[name] = stageRoot;
    // Stage built after the state file was read: its dock panels have not taken
    // their layout yet, so ask for another pass on the next frame.
    if (!s_.windowState_.empty()) s_.dockLayoutPending_ = true;
    return stageRoot;
}

void WidgetApp::setTransition(TransitionType type, float durationSec, EaseType ease)
{
    s_.transType_ = type;
    s_.transDuration_ = durationSec;
    s_.transEase_ = ease;
}

bool WidgetApp::setStage(const String& name)
{
    return setStage(name, s_.transType_, s_.transEase_);
}

bool WidgetApp::setStage(const String& name, TransitionType transition)
{
    return setStage(name, transition, s_.transEase_);
}

bool WidgetApp::setStage(const String& name, TransitionType transition, EaseType ease)
{
    Widget* const* stageRoot = s_.stages_.find(name);
    if (!stageRoot) return false;
    if (name == s_.currentStage_) return true;

    // Clear interaction state (old tree pointers become invalid). A popup open on the stage that is
    // left (a drop-down, a menu) closes with it: it would stay over the next stage, with an owner
    // that is not on screen.
    dropMouseFocus();
    closePopup();
    s_.focused_ = nullptr;
    s_.hovered_ = nullptr;
    s_.hoverChain_.clear();
    s_.needsLayout_ = true;

    // Start transition or instant swap
    if (transition != TransitionType::None && s_.root_)
    {
        s_.transActive_ = true;
        s_.transProgress_ = 0.0f;
        s_.transCurrent_ = transition;
        s_.transEaseCur_ = ease;
        s_.transOldRoot_ = s_.root_;
        s_.transOldStage_ = s_.currentStage_;
    }

    // Swap root to the new stage's widget
    s_.root_ = *stageRoot;
    s_.currentStage_ = name;
    return true;
}

Widget* WidgetApp::stage(const String& name) const
{
    Widget* const* stageRoot = s_.stages_.find(name);
    return stageRoot ? *stageRoot : nullptr;
}

bool WidgetApp::removeStage(const String& name)
{
    if (name == s_.currentStage_) return false; // cannot remove active stage
    Widget* const* stageRoot = s_.stages_.find(name);
    if (!stageRoot) return false;

    notifyWidgetRemoved(*stageRoot);
    delete *stageRoot;
    s_.stages_.erase(name);
    return true;
}

IconAtlas& WidgetApp::iconAtlas()
{
    if (!iconAtlas_)
    {
        iconAtlas_ = new IconAtlas(); /* backend calls buildImage()+setTexture() */
    }
    return *iconAtlas_;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Widget lookup
// ═════════════════════════════════════════════════════════════════════════════

ct::Vector<Widget*> WidgetApp::widgetsByTag(const String& tag)
{
    ct::Vector<Widget*> result;
    if (s_.root_) s_.root_->findByTag(tag, result);
    return result;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Global event dispatcher
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::on(const String& event, const String& widgetId, EventCallback cb)
{
    globalHandlers_[event].byId[widgetId].push_back(std::move(cb));
}

void WidgetApp::onAny(const String& event, EventCallback cb)
{
    globalHandlers_[event].any.push_back(std::move(cb));
}

void WidgetApp::fireEvent(const String& event, Widget* w)
{
    if (!w || globalHandlers_.empty()) return;
    EventHandlers* handlers = globalHandlers_.find(event);
    if (!handlers) return;
    ct::Vector<EventCallback>* targeted = w->id().empty() ? nullptr : handlers->byId.find(w->id());
    if ((!targeted || targeted->empty()) && handlers->any.empty()) return;

    // The handlers are walked from a copy: a handler is allowed to register
    // another one (on()/onAny()), and inserting into the map can move its
    // storage out from under a pointer taken before the call.
    ct::Vector<EventCallback> callbacks;
    if (targeted) callbacks = *targeted;
    for (auto& cb: handlers->any) callbacks.push_back(cb);

    for (auto& cb: callbacks) cb(w);
}

// ═════════════════════════════════════════════════════════════════════════════
//  notifyWidgetRemoved - clear dangling pointers before widget deletion
// ═════════════════════════════════════════════════════════════════════════════

static bool isDescendantOf(Widget* candidate, Widget* root)
{
    Widget* w = candidate;
    while (w)
    {
        if (w == root) return true;
        w = w->parent();
    }
    return false;
}

// Walks the tree the frame draws. @p target is only compared, never read: it may
// already have been deleted by an event handler.
static bool treeContains(const Widget* node, const Widget* target)
{
    if (!node) return false;
    if (node == target) return true;
    for (auto* child: node->children())
        if (treeContains(child, target)) return true;
    return false;
}

bool WidgetApp::isWidgetInTrees(const Widget* w) const
{
    if (!w) return false;
    if (treeContains(s_.root_, w)) return true;
    for (auto* fw: s_.floats_)
        if (treeContains(fw, w)) return true;
    if (treeContains(s_.popup_, w) || treeContains(s_.popupClosing_, w)) return true;
    return false;
}

void WidgetApp::notifyWidgetRemoved(Widget* w)
{
    if (!w) return;
    ++deletions_;
    if (Animator* animator = Animator::existing()) animator->forget(w);
    if (releasing_) return; // the whole surface is going (releaseSurface): nothing in it to fix
    for (Surface::DeferredCall& call: s_.deferredCalls_)
        if (call.owner && isDescendantOf(call.owner, w)) call.owner = nullptr;
    if (s_.tooltipWidget_ && isDescendantOf(s_.tooltipWidget_, w))
    {
        s_.tooltipWidget_ = nullptr;
        s_.tooltipVisible_ = false;
        s_.tooltipTimer_ = 0.0f;
    }
    // The widget that opened a popup going away closes it: the popup holds a
    // pointer back to its owner, which would be the memory just freed.
    if (s_.popupOwner_ && isDescendantOf(s_.popupOwner_, w))
    {
        if (s_.popup_) returnFocus(s_.popup_);
        if (s_.popup_ && s_.popupOwned_ && !isDescendantOf(s_.popup_, w)) queuePopupDelete(s_.popup_);
        if (s_.popupClosing_ && s_.popupClosingOwned_ && !isDescendantOf(s_.popupClosing_, w))
            queuePopupDelete(s_.popupClosing_);
        s_.popup_ = nullptr;
        s_.popupOwner_ = nullptr;
        s_.popupClosing_ = nullptr;
        s_.popupClosingOwner_ = nullptr;
    }
    // A popup (or the window a popup was opened from) can go away while it is
    // still the one being painted: painting it after that reads freed memory.
    if (s_.popup_ && isDescendantOf(s_.popup_, w))
    {
        s_.popup_ = nullptr;
        s_.popupOwner_ = nullptr;
    }
    if (s_.popupClosing_ && isDescendantOf(s_.popupClosing_, w))
    {
        s_.popupClosing_ = nullptr;
        s_.popupClosingOwner_ = nullptr;
    }
    if (s_.transOldRoot_ && isDescendantOf(s_.transOldRoot_, w))
    {
        s_.transActive_ = false;
        s_.transProgress_ = 1.0f;
        s_.transOldRoot_ = nullptr;
        s_.transOldStage_.clear();
    }
    if (s_.pressed_ && isDescendantOf(s_.pressed_, w))
    {
        s_.pressed_ = nullptr;
        s_.pressedButtons_ = 0;
    }
    const bool focusWentAway = s_.focused_ && isDescendantOf(s_.focused_, w);
    if (focusWentAway) s_.focused_ = nullptr;
    for (ct::Vector<Surface::FocusLoan>::size_type i = 0; i < s_.focusLoans_.size();)
    {
        Surface::FocusLoan& loan = s_.focusLoans_[i];
        if (loan.previous && isDescendantOf(loan.previous, w)) loan.previous = nullptr;
        if (!isDescendantOf(loan.holder, w))
        {
            ++i;
            continue;
        }
        Widget* previous = loan.previous;
        s_.focusLoans_.erase(s_.focusLoans_.begin() + i);
        // (what had the focus before may be on a stage that is not shown now: not there)
        if (focusWentAway && !s_.focused_ && previous && isWidgetInTrees(previous)) setFocused(previous);
    }
    if (s_.dragSource_ && isDescendantOf(s_.dragSource_, w)) cancelDrag();
    if (s_.hovered_ && isDescendantOf(s_.hovered_, w))
    {
        s_.hovered_ = nullptr;
        s_.hoverChain_.clear();
    }
    else
    {
        // Remove any entries in hover chain that point to the subtree
        s_.hoverChain_.erase(std::remove_if(s_.hoverChain_.begin(), s_.hoverChain_.end(),
                                  [w](Widget* h) { return isDescendantOf(h, w); }),
                s_.hoverChain_.end());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Float windows
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::addFloatImpl(FloatWindow* fw)
{
    if (!fw) return;
    applyWindowState(fw);
    s_.floats_.push_back(fw);
    fw->layout();
    if (Widget* target = fw->focusOnOpen()) lendFocus(fw, target);
}

void WidgetApp::removeFloat(FloatWindow* fw)
{
    if (!fw) return;
    // Already closed and waiting for the flush: a second call - the application
    // removing a dialog that closed itself, say - must not queue the same
    // pointer again, or the flush would delete it twice.
    if (std::find(s_.pendingFloatDeletes_.begin(), s_.pendingFloatDeletes_.end(), fw) !=
            s_.pendingFloatDeletes_.end())
        return;
    // Keep the geometry of a window that goes away, so reopening it next run
    // (or in the same run) lands where it was left.
    captureWindowState(fw);
    notifyWidgetRemoved(fw);
    s_.floats_.erase(std::remove(s_.floats_.begin(), s_.floats_.end(), fw), s_.floats_.end());
    // Defer delete - may be called during signal/bubble handling
    s_.pendingFloatDeletes_.push_back(fw);
}

void WidgetApp::bringToFront(FloatWindow* fw)
{
    auto it = std::find(s_.floats_.begin(), s_.floats_.end(), fw);
    if (it != s_.floats_.end() && it != s_.floats_.end() - 1)
    {
        s_.floats_.erase(it);
        s_.floats_.push_back(fw);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Window state (.ini)
// ═════════════════════════════════════════════════════════════════════════════

namespace
{

// Dock panels are found by walking the stage trees: an application builds its
// stages after setWindowStatePath(), so a layout in the file cannot be applied
// when the file is read - only once the panels it names exist.
void collectDockPanels(Widget* widget, ct::Vector<DockPanel*>& out)
{
    if (!widget) return;
    if (auto* dock = widget_cast<DockPanel>(widget)) out.push_back(dock);
    for (auto* child: widget->children()) collectDockPanels(child, out);
}

} // namespace

void WidgetApp::setWindowStatePath(const String& path)
{
    s_.windowState_.setPath(path);
    if (!s_.windowState_.path().empty()) loadWindowState();
}

bool WidgetApp::loadWindowState()
{
    const bool loaded = s_.windowState_.load();
    // A fresh read applies to every panel again, including one that already took
    // a layout from the file read before.
    ct::Vector<DockPanel*> panels;
    for (auto& entry: s_.stages_) collectDockPanels(entry.value, panels);
    for (auto* dock: panels) dock->setSettingsApplied(false);
    for (auto* fw: s_.floats_) applyWindowState(fw);
    restoreDockLayouts();
    return loaded;
}

bool WidgetApp::saveWindowState()
{
    for (auto* fw: s_.floats_) captureWindowState(fw);
    captureDockLayouts();
    return s_.windowState_.save();
}

void WidgetApp::captureWindowState(FloatWindow* fw)
{
    if (!fw) return;
    WindowGeometry geometry;
    geometry.x = fw->floatX();
    geometry.y = fw->floatY();
    geometry.w = fw->floatW();
    geometry.h = fw->floatH();
    geometry.minimized = fw->isMinimized();
    s_.windowState_.putWindow(fw->title(), geometry);
}

void WidgetApp::applyWindowState(FloatWindow* fw)
{
    if (!fw) return;
    // Anything the file does not mention stays as the caller set it up.
    WindowGeometry geometry;
    geometry.x = fw->floatX();
    geometry.y = fw->floatY();
    geometry.w = fw->floatW();
    geometry.h = fw->floatH();
    geometry.minimized = fw->isMinimized();
    if (!s_.windowState_.getWindow(fw->title(), geometry)) return;
    fw->setFloatPos(geometry.x, geometry.y);
    fw->setFloatSize(geometry.w, geometry.h);
    fw->setMinimized(geometry.minimized);
    keepOnScreen(fw);
}

// The window fits the display and enough of its title bar stays on it to be
// dragged back.
void WidgetApp::keepOnScreen(FloatWindow* fw)
{
    const float sw = static_cast<float>(s_.width_);
    const float sh = static_cast<float>(s_.height_);
    if (!fw || sw <= 0.0f || sh <= 0.0f) return;
    const float w = std::min(fw->floatW(), sw);
    const float h = std::min(fw->floatH(), sh);
    const float grip = std::min(w, 40.0f);
    const float x = std::max(grip - w, std::min(fw->floatX(), sw - grip));
    const float y = std::max(0.0f, std::min(fw->floatY(), sh - fw->titleBarHeight()));
    if (w != fw->floatW() || h != fw->floatH()) fw->setFloatSize(w, h);
    if (x != fw->floatX() || y != fw->floatY()) fw->setFloatPos(x, y);
}

void WidgetApp::captureDockLayouts()
{
    ct::Vector<DockPanel*> panels;
    for (auto& entry: s_.stages_) collectDockPanels(entry.value, panels);
    for (auto* dock: panels)
    {
        const String key = dock->settingsKey();
        if (key.empty()) continue;
        s_.windowState_.put(dockStateSection(key), "layout", dock->saveLayout());
    }
}

void WidgetApp::restoreDockLayouts()
{
    ct::Vector<DockPanel*> panels;
    for (auto& entry: s_.stages_) collectDockPanels(entry.value, panels);

    for (auto* dock: panels)
    {
        // A panel that already took its layout is left alone: a stage built
        // later must not throw away an arrangement the user has changed since.
        if (dock->settingsApplied()) continue;
        const String key = dock->settingsKey();
        if (key.empty()) continue; // the application may still set a key for it
        dock->setSettingsApplied(true);
        const String section = dockStateSection(key);
        if (!s_.windowState_.has(section, "layout")) continue;
        dock->restoreLayout(s_.windowState_.get(section, "layout"));
    }

    // addStage() and loadWindowState() ask for another pass when they bring
    // panels that have not taken their layout yet.
    s_.dockLayoutPending_ = false;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Popup overlay
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::showPopup(Widget* popup, Widget* owner, bool owned)
{
    // If a close animation is running, finish it immediately
    if (s_.popupClosing_)
    {
        retirePopup(s_.popupClosing_, s_.popupClosingOwned_);
        s_.popupClosing_ = nullptr;
        s_.popupAnimClosing_ = false;
    }

    // Close previous popup instantly if any
    if (s_.popup_) retirePopup(s_.popup_, s_.popupOwned_);

    s_.popup_ = popup;
    s_.popupOwner_ = owner;
    s_.popupOwned_ = owned;

    // Start open animation
    s_.popupAnimProgress_ = 0.0f;
    s_.popupAnimOpening_ = true;
    s_.popupAnimClosing_ = false;
    s_.popupAnimEase_ = EaseType::OutCubic;
}

void WidgetApp::forgetPopup(Widget* w)
{
    if (!w) return;
    if (s_.popup_ == w)
    {
        s_.popup_ = nullptr;
        s_.popupOwner_ = nullptr;
        s_.popupOwned_ = true;
    }
    if (s_.popupClosing_ == w)
    {
        s_.popupClosing_ = nullptr;
        s_.popupClosingOwner_ = nullptr;
        s_.popupClosingOwned_ = true;
        s_.popupAnimClosing_ = false;
    }
    if (s_.popupOwner_ == w) s_.popupOwner_ = nullptr;
    if (s_.popupClosingOwner_ == w) s_.popupClosingOwner_ = nullptr;
    for (size_t i = 0; i < s_.pendingPopupDeletes_.size();)
    {
        if (s_.pendingPopupDeletes_[i] == w)
            s_.pendingPopupDeletes_.erase(s_.pendingPopupDeletes_.begin() + i);
        else
            ++i;
    }
}

void WidgetApp::closePopup()
{
    if (s_.popup_)
    {
        // Start close animation — keep popup alive during the tween
        s_.popupClosing_ = s_.popup_;
        s_.popupClosingOwner_ = s_.popupOwner_;
        s_.popupClosingOwned_ = s_.popupOwned_;

        s_.popupAnimProgress_ = 1.0f;
        s_.popupAnimClosing_ = true;
        s_.popupAnimOpening_ = false;
        s_.popupAnimEase_ = EaseType::InCubic;

        // Clear the active popup pointer so input goes through
        returnFocus(s_.popup_);
        s_.popup_ = nullptr;
        s_.popupOwner_ = nullptr;
        s_.popupOwned_ = true;
    }
}

void WidgetApp::lendFocus(Widget* holder, Widget* target)
{
    if (!holder) return;
    s_.focusLoans_.push_back({ holder, s_.focused_ });
    setFocused(target);
}

void WidgetApp::returnFocus(Widget* holder)
{
    Widget* previous = nullptr;
    for (ct::Vector<Surface::FocusLoan>::size_type i = 0; i < s_.focusLoans_.size(); ++i)
    {
        if (s_.focusLoans_[i].holder != holder) continue;
        previous = s_.focusLoans_[i].previous;
        s_.focusLoans_.erase(s_.focusLoans_.begin() + i);
        break;
    }
    // (what had the focus before may be on a stage that is not shown now: not there)
    if (s_.focused_ && isDescendantOf(s_.focused_, holder))
        setFocused(previous && isWidgetInTrees(previous) ? previous : nullptr);
}

void WidgetApp::retirePopup(Widget* popup, bool owned)
{
    // The references to the popup go first (notifyWidgetRemoved), then it is told: what resetPopupState() does (a PopupPanel
    // asks for a call of the application to be made after the frame) is not undone by the references being dropped.
    notifyWidgetRemoved(popup);
    popup->resetPopupState();
    if (owned) queuePopupDelete(popup);
}

void WidgetApp::queuePopupDelete(Widget* popup)
{
    for (Widget* queued: s_.pendingPopupDeletes_)
        if (queued == popup) return;
    s_.pendingPopupDeletes_.push_back(popup);
}

void WidgetApp::flushPopupDeletes()
{
    while (!s_.pendingPopupDeletes_.empty())
    {
        Widget* queued = s_.pendingPopupDeletes_.front();
        s_.pendingPopupDeletes_.erase(s_.pendingPopupDeletes_.begin());
        delete queued;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Hit testing
// ═════════════════════════════════════════════════════════════════════════════

Widget* WidgetApp::hitTest(Widget* w, float x, float y)
{
    if (!w || !w->isVisible()) return nullptr;

    Rect abs = w->absoluteRect();
    if (!abs.contains(x, y)) return nullptr;
    if (!w->childrenShown()) return w;

    // Deepest child first (reverse order = topmost drawn last)
    const auto& ch = w->children();

    // If this widget scrolls, clip child hits to the visible viewport
    auto so = w->scrollOffset();
    bool clips = (so.x != 0 || so.y != 0);

    for (int i = static_cast<int>(ch.size()) - 1; i >= 0; --i)
    {
        // Skip children outside the viewport when parent scrolls,
        // but never clip non-scrollable children (e.g. scrollbars).
        if (clips && ch[i]->isScrollable())
        {
            Rect childAbs = ch[i]->absoluteRect();
            // Child must overlap the parent's visible area
            if (childAbs.right() <= abs.x || childAbs.x >= abs.right() ||
                    childAbs.bottom() <= abs.y || childAbs.y >= abs.bottom())
                continue;
        }
        Widget* hit = hitTest(ch[i], x, y);
        if (hit) return hit;
    }

    return w;
}

Widget* WidgetApp::topWidgetAt(float x, float y)
{
    if (s_.popup_)
    {
        Widget* hit = hitTest(s_.popup_, x, y);
        if (!hit && s_.popup_->popupContains(x, y)) hit = s_.popup_;
        return hit;
    }
    for (int i = static_cast<int>(s_.floats_.size()) - 1; i >= 0; --i)
    {
        FloatWindow* fw = s_.floats_[i];
        if (!fw->isVisible()) continue;
        if (Widget* hit = hitTest(fw, x, y)) return hit;
    }
    return hitTest(s_.root_, x, y);
}

void WidgetApp::fillLocal(Widget* target, MouseEvent& e)
{
    if (!target) return;
    Rect abs = target->absoluteRect();
    e.localX = e.x - abs.x;
    e.localY = e.y - abs.y;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Event bubbling - call handler on target, then walk up parents
// ═════════════════════════════════════════════════════════════════════════════

template<typename Func>
void WidgetApp::bubble(Widget* target, MouseEvent& e, Func handler)
{
    // The parents are read before any handler runs: a handler may delete the
    // widget it was called with, and `w->parent()` would then read freed memory.
    Widget* stack[32]; // (on the stack: a tree is not deeper than a few dozen; deeper ones go to the heap, so that an event allocates nothing)
    ct::Vector<Widget*> deep;
    size_t count = 0;
    for (Widget* w = target; w; w = w->parent())
    {
        if (count < 32) stack[count++] = w;
        else
        {
            if (deep.empty())
                for (size_t k = 0; k < 32; ++k) deep.push_back(stack[k]);
            deep.push_back(w);
        }
    }
    Widget** chain = deep.empty() ? stack : deep.data();
    if (!deep.empty()) count = deep.size();

    uint32_t seen = deletions_;
    for (size_t i = 0; i < count && !e.consumed; ++i)
    {
        Widget* w = chain[i];
        if (seen != deletions_)
        {
            // Something went away during a handler: the rest of the chain may
            // have gone with it, so it is looked up in the tree that is drawn.
            if (!isWidgetInTrees(w)) return;
            seen = deletions_;
        }
        if (!w->isEnabledInTree()) continue;
        fillLocal(w, e);
        (w->*handler)(e);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Hover chain - track enter/leave for the full widget path
// ═════════════════════════════════════════════════════════════════════════════

ct::Vector<Widget*> WidgetApp::buildChain(Widget* w)
{
    ct::Vector<Widget*> chain;
    while (w)
    {
        chain.push_back(w);
        w = w->parent();
    }
    // Reverse so chain goes root → ... → leaf
    std::reverse(chain.begin(), chain.end());
    return chain;
}

void WidgetApp::applyCursor(CursorType type)
{
    s_.wantedCursor_ = type;
}

void WidgetApp::updateHover(Widget* newLeaf)
{
    ct::Vector<Widget*> newChain = newLeaf ? buildChain(newLeaf) : ct::Vector<Widget*>{};
    // The old chain is walked from a copy: a leave handler may delete widgets, and the manager
    // then edits s_.hoverChain_ under the loop.
    const ct::Vector<Widget*> oldChain = s_.hoverChain_;

    // Find common prefix length
    size_t common = 0;
    size_t minLen = oldChain.size() < newChain.size() ? oldChain.size() : newChain.size();
    while (common < minLen && oldChain[common] == newChain[common]) ++common;

    // A handler may delete widgets, the one it was called for, others of the chains, or the whole
    // tree. Once one did (the count of deletions moved), a widget is only touched when it is still
    // in the tree that is drawn.
    const uint32_t deletionsBefore = deletions_;
    auto stillThere = [&](const Widget* w) { return deletions_ == deletionsBefore || isWidgetInTrees(w); };

    // Leave: old widgets that are no longer in the chain (deepest first)
    for (size_t i = oldChain.size(); i > common; --i)
    {
        Widget* w = oldChain[i - 1];
        if (!stillThere(w)) continue;
        w->hovered_ = false;
        w->onMouseLeave();
        if (stillThere(w)) w->markDirty();
    }

    // Enter: new widgets that just appeared in the chain (shallowest first)
    for (size_t i = common; i < newChain.size(); ++i)
    {
        Widget* w = newChain[i];
        if (!stillThere(w)) break; // what is below a deleted widget went with it
        w->hovered_ = true;
        w->onMouseEnter();
        if (stillThere(w)) w->markDirty();
    }

    // What survived the handlers is the hover chain: a widget deleted meanwhile is not in it, and
    // the hovered widget is the deepest one left.
    if (deletions_ != deletionsBefore)
    {
        size_t keep = 0;
        while (keep < newChain.size() && isWidgetInTrees(newChain[keep])) ++keep;
        newChain.resize(keep);
        newLeaf = newChain.empty() ? nullptr : newChain.back();
    }
    s_.hoverChain_ = std::move(newChain);
    s_.hovered_ = newLeaf;

    // Update cursor to match deepest hovered widget
    applyCursor(s_.hovered_ ? s_.hovered_->cursor() : CursorType::Arrow);

    // Reset tooltip when hover target changes
    s_.tooltipTimer_ = 0.0f;
    s_.tooltipVisible_ = false;
    s_.tooltipWidget_ = nullptr;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Focus
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::setFocused(Widget* w)
{
    ++gWidgetChanges; // (the focus ring, the caret: what is drawn changes with it)
    if (s_.focused_)
    {
        s_.focused_->setFocused(false);
        s_.focused_->onFocusLost();
    }
    s_.focused_ = w;
    if (s_.focused_)
    {
        s_.focused_->setFocused(true);
        s_.focused_->onFocusGained();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Debug layout overlay
// ═════════════════════════════════════════════════════════════════════════════

namespace
{

// Cycle through distinct colors per depth level
static Color debugColor(int depth)
{
    static const Color colors[] = {
        { 255, 50, 50, 180 },  // red
        { 50, 255, 50, 180 },  // green
        { 80, 80, 255, 180 },  // blue
        { 255, 255, 50, 180 }, // yellow
        { 255, 50, 255, 180 }, // magenta
        { 50, 255, 255, 180 }, // cyan
        { 255, 160, 50, 180 }, // orange
    };
    return colors[depth % 7];
}

void debugPaintWidget(Widget* w, ig::retained::DrawList& dl, int depth)
{
    if (!w || !w->isVisible()) return;

    ig::retained::Rect abs = { w->absoluteRect().x, w->absoluteRect().y, w->absoluteRect().w,
        w->absoluteRect().h };
    Color c = debugColor(depth);
    dl.addRectOutline(abs, c);

    const Edges& m = w->margins();
    if (m.top > 0 || m.right > 0 || m.bottom > 0 || m.left > 0)
    {
        ig::retained::Rect outer = { abs.x - m.left, abs.y - m.top, abs.w + m.left + m.right,
            abs.h + m.top + m.bottom };
        Color dim = { c.r, c.g, c.b, 80 };
        dl.addRectOutline(outer, dim);
    }

    for (auto* child: w->children()) debugPaintWidget(child, dl, depth + 1);
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
//  Easing functions
// ═════════════════════════════════════════════════════════════════════════════

static constexpr float PI = 3.14159265358979323846f;

static float bounceOut(float t)
{
    if (t < 1.0f / 2.75f) return 7.5625f * t * t;
    if (t < 2.0f / 2.75f)
    {
        t -= 1.5f / 2.75f;
        return 7.5625f * t * t + 0.75f;
    }
    if (t < 2.5f / 2.75f)
    {
        t -= 2.25f / 2.75f;
        return 7.5625f * t * t + 0.9375f;
    }
    t -= 2.625f / 2.75f;
    return 7.5625f * t * t + 0.984375f;
}

float applyEasing(EaseType type, float t)
{
    switch (type)
    {
        case EaseType::Linear:
            return t;
        case EaseType::InQuad:
            return t * t;
        case EaseType::OutQuad:
            return t * (2.0f - t);
        case EaseType::InOutQuad:
            return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
        case EaseType::InCubic:
            return t * t * t;
        case EaseType::OutCubic:
        {
            float u = t - 1.0f;
            return u * u * u + 1.0f;
        }
        case EaseType::InOutCubic:
            return t < 0.5f ? 4.0f * t * t * t
                            : 1.0f - 0.5f * ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) *
                                                    (-2.0f * t + 2.0f));
        case EaseType::InExpo:
            return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
        case EaseType::OutExpo:
            return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
        case EaseType::InOutExpo:
            if (t == 0.0f || t == 1.0f) return t;
            return t < 0.5f ? 0.5f * std::pow(2.0f, 20.0f * t - 10.0f)
                            : 1.0f - 0.5f * std::pow(2.0f, -20.0f * t + 10.0f);
        case EaseType::InBack:
        {
            constexpr float s = 1.70158f;
            return t * t * ((s + 1.0f) * t - s);
        }
        case EaseType::OutBack:
        {
            constexpr float s = 1.70158f;
            float u = t - 1.0f;
            return u * u * ((s + 1.0f) * u + s) + 1.0f;
        }
        case EaseType::InOutBack:
        {
            constexpr float s = 1.70158f * 1.525f;
            float u = t * 2.0f;
            if (u < 1.0f) return 0.5f * (u * u * ((s + 1.0f) * u - s));
            u -= 2.0f;
            return 0.5f * (u * u * ((s + 1.0f) * u + s) + 2.0f);
        }
        case EaseType::InBounce:
            return 1.0f - bounceOut(1.0f - t);
        case EaseType::OutBounce:
            return bounceOut(t);
        case EaseType::InOutBounce:
            return t < 0.5f ? 0.5f * (1.0f - bounceOut(1.0f - 2.0f * t))
                            : 0.5f * bounceOut(2.0f * t - 1.0f) + 0.5f;
        case EaseType::InElastic:
            return t == 0.0f || t == 1.0f
                           ? t
                           : -std::pow(2.0f, 10.0f * t - 10.0f) *
                                     std::sin((t * 10.0f - 10.75f) * (2.0f * PI / 3.0f));
        case EaseType::OutElastic:
            return t == 0.0f || t == 1.0f
                           ? t
                           : std::pow(2.0f, -10.0f * t) *
                                             std::sin((t * 10.0f - 0.75f) * (2.0f * PI / 3.0f)) +
                                     1.0f;
        case EaseType::InOutElastic:
            if (t == 0.0f || t == 1.0f) return t;
            return t < 0.5f ? -0.5f * std::pow(2.0f, 20.0f * t - 10.0f) *
                                      std::sin((20.0f * t - 11.125f) * (2.0f * PI / 4.5f))
                            : 0.5f * std::pow(2.0f, -20.0f * t + 10.0f) *
                                              std::sin((20.0f * t - 11.125f) * (2.0f * PI / 4.5f)) +
                                      1.0f;
    }
    return t;
}

// ═════════════════════════════════════════════════════════════════════════════
//  paintTreeInto – layout + paint one widget tree into a DrawList.
//  No CPU-side offset — camera is handled GPU-side via DrawPass::camera.
// ═════════════════════════════════════════════════════════════════════════════

// ═════════════════════════════════════════════════════════════════════════════
//  paintTreeInto – fills background then paints widget tree
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::setStageCamera(const String& name, const ig::retained::Camera2D& cam)
{
    s_.stageCameras_[name] = cam;
}

ig::retained::Camera2D WidgetApp::stageCamera(const String& name) const
{
    const ig::retained::Camera2D* camera = s_.stageCameras_.find(name);
    return camera ? *camera : ig::retained::Camera2D{};
}

void WidgetApp::setStageBgColor(const String& name, const Color& c)
{
    s_.stageBgColors_[name] = c;
}

Color WidgetApp::stageBgColor(const String& name) const
{
    const Color* color = s_.stageBgColors_.find(name);
    return color ? *color : bgColor();
}

// ═════════════════════════════════════════════════════════════════════════════
//  paintTreeInto – fills background then paints widget tree
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::paintTreeInto(Widget* tree, ig::retained::DrawList& dl,
        const ig::retained::Font* font, IconAtlas* icons, float w, float h, const Color& bgColor)
{
    if (!tree) return;

    tree->setRect({ 0, 0, w, h });
    for (auto* c: tree->children()) c->setRect({ 0, 0, w, h });
    tree->layout();

    // Fill background so transitions have no holes between stages.
    dl.addRect({ 0, 0, w, h }, bgColor);

    PaintContext ctx{ dl, font, icons };
    ctx.swapStacks(paintClipScratch_, paintFontScratch_);
    tree->paint(ctx);
    ctx.swapStacks(paintClipScratch_, paintFontScratch_);
    paintClipScratch_.clear();
    paintFontScratch_.clear();
}

// ═════════════════════════════════════════════════════════════════════════════
//  paint – layout + paint the full widget tree, pushing DrawPasses into data.
// ═════════════════════════════════════════════════════════════════════════════

bool WidgetApp::addDynamicAtlas(ig::retained::FontAtlas& atlas)
{
    for (auto* known: dynamicAtlases_)
        if (known == &atlas) return atlas.dynamicEnabled();
    if (!uploadTex_ || !atlas.prepareDynamic()) return false;
    const ig::retained::TextureHandle page =
            uploadTex_(atlas.dynamicPixels(), atlas.dynamicWidth(), atlas.dynamicHeight());
    if (!atlas.enableDynamic(page)) return false;
    dynamicAtlases_.push_back(&atlas);
    return true;
}

void WidgetApp::deleteLater(Widget* w)
{
    if (!w) return;
    if (busy_ > 0) pendingWidgetDeletes_.push_back(w);
    else
        delete w;
}

void WidgetApp::flushWidgetDeletes()
{
    // A destructor can remove more widgets (they are deleted at once: nothing is running now).
    while (!pendingWidgetDeletes_.empty())
    {
        Widget* w = pendingWidgetDeletes_.back();
        pendingWidgetDeletes_.pop_back();
        delete w;
    }
}

void WidgetApp::forgetAtlas(const ig::retained::FontAtlas& atlas)
{
    if (font_ && atlas.owns(*font_)) font_ = nullptr;
    for (size_t i = 0; i < dynamicAtlases_.size(); ++i)
    {
        if (dynamicAtlases_[i] != &atlas) continue;
        dynamicAtlases_.erase(dynamicAtlases_.begin() + i);
        break;
    }
}

void WidgetApp::removeDynamicAtlas(ig::retained::FontAtlas& atlas)
{
    for (size_t i = 0; i < dynamicAtlases_.size(); ++i)
    {
        if (dynamicAtlases_[i] != &atlas) continue;
        destroyTexture(atlas.dynamicTexture());
        atlas.disableDynamic();
        dynamicAtlases_.erase(dynamicAtlases_.begin() + static_cast<std::ptrdiff_t>(i));
        return;
    }
}

bool WidgetApp::needsFrame() const
{
    if (s_.forcePaint_ || s_.inputSeen_ || s_.needsLayout_ || s_.relayout_ || gWidgetChanges != s_.paintedChanges_) return true;
    if (s_.transActive_ || s_.popupAnimOpening_ || s_.popupAnimClosing_ || s_.overlay_) return true;
    if (s_.tooltipVisible_ != s_.tooltipPainted_) return true;
    if (Toast::hasActive()) return true;
    if (const Animator* animator = Animator::existing())
        if (animator->count() > 0) return true;
    if (__atomic_load_n(&wakeFromThread_, __ATOMIC_ACQUIRE) != 0) return true;
    return elapsedSeconds_ >= s_.wakeAt_;
}

int WidgetApp::nextFrameInMs() const
{
    if (needsFrame()) return 0;
    double soonest = s_.wakeAt_ - elapsedSeconds_;
    if (s_.hovered_ && s_.hovered_->hasTooltip() && !s_.tooltipVisible_)
    {
        const double due = static_cast<double>(s_.hovered_->tooltipDelay() - s_.tooltipTimer_);
        if (due < soonest) soonest = due;
    }
    if (soonest > 1.0e200) return -1;
    const double ms = soonest * 1000.0 + 0.999;
    return ms < 1.0 ? 1 : (ms > 2.0e9 ? 2000000000 : static_cast<int>(ms));
}

void WidgetApp::paint(ig::retained::DrawData& data, const ig::retained::Font* font,
        IconAtlas* icons)
{
    BusyScope busy(*this);
    // what this frame draws: from here on, what changes (or asks for time) is for the next one. (The layout below is part of this
    // frame: the rectangles it sets are not a change.)
    s_.forcePaint_ = false;
    s_.inputSeen_ = false;
    s_.tooltipPainted_ = s_.tooltipVisible_;
    s_.wakeAt_ = 1.0e300;
    float w = static_cast<float>(s_.width_);
    float h = static_cast<float>(s_.height_);
    if (w <= 0 || h <= 0) return;
    font_ = font; // cache for text measurement in mouse events
    drawList_ = &ig::retained::GetDrawList();
    // a mesh a widget keeps (a PlotChart's layer) is drawn by reference where the renderer keeps meshes, else copied into the list
    drawList_->setMeshByReference(ig::retained::GetIO().retainedMeshes);
    ig::retained::GetTransDrawList().setMeshByReference(ig::retained::GetIO().retainedMeshes);

    // Layout pass
    if ((s_.needsLayout_ || s_.relayout_) && s_.root_)
    {
        if (s_.needsLayout_)
        {
            s_.root_->setRect({ 0, 0, w, h });
            for (auto* c: s_.root_->children()) c->setRect({ 0, 0, w, h });
        }
        else
        {
            // only something under the tree changed: a child of the stage that the application placed keeps its
            // rectangle, and one that was just added (no size yet) takes the window
            for (auto* c: s_.root_->children())
                if (!(c->rect().w > 0.0f && c->rect().h > 0.0f)) c->setRect({ 0, 0, w, h });
        }
        s_.root_->layout();

        if (s_.transActive_ && s_.transOldRoot_)
        {
            if (s_.needsLayout_)
            {
                s_.transOldRoot_->setRect({ 0, 0, w, h });
                for (auto* c: s_.transOldRoot_->children()) c->setRect({ 0, 0, w, h });
            }
            s_.transOldRoot_->layout();
        }
        s_.needsLayout_ = false;
        s_.relayout_ = false;
    }
    s_.paintedChanges_ = gWidgetChanges;

    // Transition
    if (s_.transActive_)
    {
        s_.transProgress_ += dt_ / s_.transDuration_;
        if (s_.transProgress_ >= 1.0f)
        {
            s_.transProgress_ = 1.0f;
            s_.transActive_ = false;
            s_.transOldRoot_ = nullptr;
            s_.transOldStage_.clear();
        }
        float t = applyEasing(s_.transEaseCur_, s_.transProgress_);

        // Each stage gets its own DrawList + Camera2D.
        // The base camera comes from the per-stage camera map;
        // transitions add a pan/scale offset on top.
        ig::retained::Camera2D baseCur = stageCamera(s_.currentStage_);
        ig::retained::Camera2D baseOld = stageCamera(s_.transOldStage_);

        ig::retained::Camera2D camOld = baseOld;
        ig::retained::Camera2D camNew = baseCur;

        ig::retained::DrawList& dlNew = ig::retained::GetDrawList();
        ig::retained::DrawList& dlOld = ig::retained::GetTransDrawList();

        switch (s_.transCurrent_)
        {
            case TransitionType::SlideLeft:
                camOld.x = baseOld.x - t * w;
                camNew.x = baseCur.x + w * (1.0f - t);
                break;
            case TransitionType::SlideRight:
                camOld.x = baseOld.x + t * w;
                camNew.x = baseCur.x - w * (1.0f - t);
                break;
            case TransitionType::SlideUp:
                camOld.y = baseOld.y - t * h;
                camNew.y = baseCur.y + h * (1.0f - t);
                break;
            case TransitionType::SlideDown:
                camOld.y = baseOld.y + t * h;
                camNew.y = baseCur.y - h * (1.0f - t);
                break;
            case TransitionType::CoverLeft:
                // old stays, new slides in from right
                camNew.x = baseCur.x + w * (1.0f - t);
                break;
            case TransitionType::CoverRight:
                camNew.x = baseCur.x - w * (1.0f - t);
                break;
            case TransitionType::CoverUp:
                camNew.y = baseCur.y + h * (1.0f - t);
                break;
            case TransitionType::CoverDown:
                camNew.y = baseCur.y - h * (1.0f - t);
                break;
            case TransitionType::RevealLeft:
                // new stays, old slides out left
                camOld.x = baseOld.x - t * w;
                break;
            case TransitionType::RevealRight:
                camOld.x = baseOld.x + t * w;
                break;
            case TransitionType::RevealUp:
                camOld.y = baseOld.y - t * h;
                break;
            case TransitionType::RevealDown:
                camOld.y = baseOld.y + t * h;
                break;
            case TransitionType::ZoomIn:
                // new zooms in from 0.5 → 1.0 (pivot = screen centre)
                camNew.scale = baseCur.scale * (0.5f + 0.5f * t);
                camNew.pivotX = 0.5f;
                camNew.pivotY = 0.5f;
                break;
            case TransitionType::ZoomOut:
                // old zooms out from 1.0 → 0.5, new behind
                camOld.scale = baseOld.scale * (1.0f - 0.5f * t);
                camOld.pivotX = 0.5f;
                camOld.pivotY = 0.5f;
                break;
            default:
                break;
        }

        // For Reveal: new is below old → push new first.
        // For Cover/Slide: new is above old → push old first, new second.
        bool revealType = (s_.transCurrent_ == TransitionType::RevealLeft ||
                           s_.transCurrent_ == TransitionType::RevealRight ||
                           s_.transCurrent_ == TransitionType::RevealUp ||
                           s_.transCurrent_ == TransitionType::RevealDown ||
                           s_.transCurrent_ == TransitionType::ZoomOut);

        if (revealType)
        {
            // new (behind) first
            paintTreeInto(s_.root_, dlNew, font, icons, w, h, stageBgColor(s_.currentStage_));
            data.passes.push_back({ &dlNew, camNew });
            paintTreeInto(s_.transOldRoot_, dlOld, font, icons, w, h, stageBgColor(s_.transOldStage_));
            data.passes.push_back({ &dlOld, camOld });
        }
        else
        {
            // old (behind) first
            paintTreeInto(s_.transOldRoot_, dlOld, font, icons, w, h, stageBgColor(s_.transOldStage_));
            data.passes.push_back({ &dlOld, camOld });
            paintTreeInto(s_.root_, dlNew, font, icons, w, h, stageBgColor(s_.currentStage_));
            data.passes.push_back({ &dlNew, camNew });
        }
    }
    else
    {
        // Normal render: single pass with this stage's camera
        ig::retained::DrawList& dl = ig::retained::GetDrawList();
        ig::retained::Camera2D cam = stageCamera(s_.currentStage_);

        // Fill stage background first so there are no transparent holes.
        dl.addRect({ 0, 0, w, h }, stageBgColor(s_.currentStage_));

        PaintContext ctx{ dl, font, icons };
        ctx.swapStacks(paintClipScratch_, paintFontScratch_);
        if (s_.root_) s_.root_->paint(ctx);

        // Float windows (above stage, below popup)
        for (auto* fw: s_.floats_)
        {
            if (!fw->isVisible()) continue;
            fw->layout();
            fw->paint(ctx);
        }

        // Popup (always on top) — with open/close animation
        Widget* popupToPaint = s_.popup_ ? s_.popup_ : s_.popupClosing_;
        if (popupToPaint)
        {
            float rawT = s_.popupAnimProgress_;
            bool animating = s_.popupAnimOpening_ || s_.popupAnimClosing_;
            float animT =
                    animating ? applyEasing(s_.popupAnimEase_, std::max(0.0f, std::min(1.0f, rawT)))
                              : rawT;

            if (animating && animT < 0.99f)
            {
                // Scale from 0.92→1.0, slide from top by (1-t)*8px
                float scale = 0.92f + 0.08f * animT;
                float slideY = (1.0f - animT) * -6.0f;

                Rect pr = popupToPaint->rect();
                float pivotX = pr.x + pr.w * 0.5f;
                float pivotY = pr.y;

                float newW = pr.w * scale;
                float newH = pr.h * scale;
                float newX = pivotX - newW * 0.5f;
                float newY = pivotY + slideY;

                // Clip to the final rect so partially-scaled content doesn't overflow
                Rect clipR = { newX, newY, newW, newH };
                ctx.pushClip(clipR);

                popupToPaint->setRect({ newX, newY, newW, newH });
                popupToPaint->paint(ctx);
                popupToPaint->setRect(pr);

                ctx.popClip();
            }
            else
            {
                popupToPaint->paint(ctx);
            }
        }

        data.passes.push_back({ &dl, cam });
        ctx.swapStacks(paintClipScratch_, paintFontScratch_);
        paintClipScratch_.clear();
        paintFontScratch_.clear();
    }

    // Debug layout overlay
    if (debugLayout_ && s_.root_)
    {
        // Debug overlay goes into the current (new) stage's DrawList
        ig::retained::DrawList& dlDbg = ig::retained::GetDrawList();
        paintDebugOverlay(s_.root_, dlDbg, 0);
    }

    // Overlay callback
    if (s_.overlay_)
    {
        ig::retained::DrawList& dlOv = ig::retained::GetDrawList();
        PaintContext octx{ dlOv, font, icons };
        s_.overlay_(octx);
        // If not already in passes (transition case already pushed), ensure
        // the overlay is in the last pass (it shares the drawList).
    }

    // Tooltip
    if (s_.tooltipVisible_ && s_.tooltipWidget_ && font)
    {
        ig::retained::DrawList& dlTt = ig::retained::GetDrawList();
        paintTooltipInto(dlTt, font);
    }

    // Toast notifications
    Toast::tick(dt_);
    if (Toast::hasActive())
    {
        ig::retained::DrawList& dlToast = ig::retained::GetDrawList();
        PaintContext tctx{ dlToast, font, icons };
        Toast::paint(tctx, w, h);
    }

    // A glyph rasterised while painting: its page goes up before the frame is drawn.
    for (auto* atlas: dynamicAtlases_)
    {
        if (!atlas->dynamicDirty()) continue;
        updateTexture(atlas->dynamicTexture(), atlas->dynamicPixels(), atlas->dynamicWidth(),
                atlas->dynamicHeight());
        atlas->clearDynamicDirty();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  paintDebugOverlay
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::paintDebugOverlay(Widget* w, ig::retained::DrawList& dl, int depth)
{
    debugPaintWidget(w, dl, depth);
}

// ═════════════════════════════════════════════════════════════════════════════
//  paintTooltipInto
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::paintTooltipInto(ig::retained::DrawList& dl, const ig::retained::Font* font)
{
    if (!s_.tooltipWidget_ || !s_.tooltipWidget_->hasTooltip()) return;

    const auto& t = Theme::instance();
    const String& text = s_.tooltipWidget_->tooltip();
    const TextureHandle image = s_.tooltipWidget_->tooltipImage();
    float imageW = 0.0f, imageH = 0.0f;
    if (image && s_.tooltipWidget_->tooltipImageWidth() > 0.0f &&
            s_.tooltipWidget_->tooltipImageHeight() > 0.0f)
    {
        const float side = 192.0f;
        const float sw = s_.tooltipWidget_->tooltipImageWidth(),
                    sh = s_.tooltipWidget_->tooltipImageHeight();
        const float scale =
                sw > sh ? (sw > side ? side / sw : 1.0f) : (sh > side ? side / sh : 1.0f);
        imageW = sw * scale;
        imageH = sh * scale;
    }

    float w = static_cast<float>(s_.width_);
    float h = static_cast<float>(s_.height_);

    PaintContext ctx{ dl, font, nullptr };
    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    ctx.font.SetColor(t.tooltipText);
    float textW = text.empty() ? 0.0f : ctx.font.GetTextWidth(text.c_str());
    float textH = text.empty() ? 0.0f : t.fontSize;
    const float gap = !text.empty() && imageH > 0.0f ? 4.0f : 0.0f;

    float padX = 8.0f, padY = 4.0f;
    float tipW = (textW > imageW ? textW : imageW) + padX * 2;
    float tipH = textH + gap + imageH + padY * 2;

    float tx = s_.mouseX_ + 12.0f;
    float ty = s_.mouseY_ + 18.0f;
    if (tx + tipW > w) tx = w - tipW - 2.0f;
    if (ty + tipH > h) ty = s_.mouseY_ - tipH - 4.0f;
    if (tx < 0) tx = 2.0f;
    if (ty < 0) ty = 2.0f;

    ctx.fill.SetColor(t.tooltipBg);
    ctx.fillRoundedRect(tx, ty, tipW, tipH, 3.0f, 4);
    ctx.line.SetColor(t.tooltipBorder);
    ctx.lineRoundedRect(tx, ty, tipW, tipH, 3.0f, 4);
    if (imageH > 0.0f)
    {
        // A checker under the image, so its transparent parts read as such.
        const float ix = tx + padX, iy = ty + padY + textH + gap;
        const float cell = 8.0f;
        for (float cy = 0.0f; cy < imageH; cy += cell)
            for (float cx = 0.0f; cx < imageW; cx += cell)
            {
                const bool dark =
                        (static_cast<int>(cx / cell) + static_cast<int>(cy / cell)) % 2 == 0;
                ctx.fill.SetColor(dark ? Color(70, 70, 76, 255) : Color(100, 100, 108, 255));
                ctx.fillRect(ix + cx, iy + cy, cx + cell > imageW ? imageW - cx : cell,
                        cy + cell > imageH ? imageH - cy : cell);
            }
        ctx.drawImage(image, Rect{ ix, iy, imageW, imageH });
    }
    if (!text.empty())
    {
        ctx.font.SetFontSize(t.fontSize);
        ctx.font.SetBatch(&ctx.text);
        ctx.font.SetColor(t.tooltipText);
        float asc = ctx.font.GetAscender();
        ctx.font.Print(text.c_str(), tx + padX, ty + padY + asc);
    }
}

float WidgetApp::textWidth(const char* text) const
{
    return textWidth(text, nullptr);
}

float WidgetApp::textWidth(const char* text, const ig::retained::Font* font, float scale) const
{
    if (!font && font_) font = font_->atHeight(Theme::instance().fontSize);
    if (!font || !drawList_ || !text) return 0.0f;
    return drawList_->calcTextSize(*font, text, scale).x;
}

ig::retained::TextureHandle WidgetApp::loadImageTexture(const char* path, int& outW, int& outH)
{
    outW = outH = 0;
    if (!uploadTex_ || !path) return ig::retained::TextureHandle{ 0 };

    BuImage img;
    if (!img.Load(path) || !img.IsValid()) return ig::retained::TextureHandle{ 0 };

    // Ensure RGBA
    if (img.components != 4)
    {
        BuImage* rgba = img.ConvertToRGBA();
        if (!rgba || !rgba->IsValid())
        {
            delete rgba;
            return ig::retained::TextureHandle{ 0 };
        }
        outW = rgba->width;
        outH = rgba->height;
        auto tex = uploadTex_(rgba->pixels, rgba->width, rgba->height);
        delete rgba;
        return tex;
    }

    outW = img.width;
    outH = img.height;
    return uploadTex_(img.pixels, img.width, img.height);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Drag & Drop
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::cancelDrag()
{
    if (s_.dragPayload_)
    {
        if (s_.dragSource_) s_.dragSource_->onDragEnd(false);
        delete s_.dragPayload_;
        s_.dragPayload_ = nullptr;
    }
    s_.dragSource_ = nullptr;
    s_.dragPending_ = false;
}

void WidgetApp::dispatchDropEvents(const ct::Vector<ig::retained::IO::DropEvent>& drops)
{
    for (const auto& drop: drops)
    {
        // Find the widget under the drop position
        Widget* target = nullptr;
        for (int i = static_cast<int>(s_.floats_.size()) - 1; i >= 0; --i)
        {
            auto* fw = s_.floats_[i];
            if (!fw->isVisible()) continue;
            target = hitTest(fw, drop.x, drop.y);
            if (target) break;
        }
        if (!target) target = hitTest(s_.root_, drop.x, drop.y);

        if (target && !drop.filePaths.empty())
        {
            // Walk up from target to find a handler
            Widget* w = target;
            while (w)
            {
                bool handled = !w->fileDrop.empty();
                w->onFileDrop(drop.filePaths);
                w->fileDrop.emit(drop.filePaths);
                if (handled) break; // stop bubbling once a handler is found
                w = w->parent();
            }
        }
    }
}

} // namespace retained
} // namespace ig
