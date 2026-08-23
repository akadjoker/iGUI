#include "pch.hpp"
#include "ViewWidgets.hpp"   // FloatWindow (real implementation)
#include "LayoutWidgets.hpp" // StatusBar
#include "Animation.hpp"     // Animator
#include "MenuWidgets.hpp"   // Menu::exec (for context menu)
#include "DialogWidgets.hpp" // Toast

namespace BuGUI {

struct ContextMenu {
    static void show(Menu* menu, float x, float y, Widget* /*owner*/) {
        if (menu) menu->exec(x, y);
    }
};

#include <cstring>

// ── helpers ───────────────────────────────────────────────────────────────

static void collectFocusable(Widget* w, std::vector<Widget*>& out)
{
    if (!w || !w->isVisible() || !w->isEnabled()) return;
    if (w->acceptsFocus()) out.push_back(w);
    for (auto* c : w->children())
        collectFocusable(c, out);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Singleton
// ═════════════════════════════════════════════════════════════════════════════

WidgetApp& WidgetApp::instance()
{
    static WidgetApp app;
    return app;
}

WidgetApp::~WidgetApp()
{
    shutdown();
}

void WidgetApp::shutdown()
{
    inited_ = false;

    // Clean up popups BEFORE root
    if (popupOwned_)
        delete popup_;
    popup_ = nullptr; popupOwner_ = nullptr; popupOwned_ = true;

    if (popupClosingOwned_)
        delete popupClosing_;
    popupClosing_ = nullptr; popupClosingOwner_ = nullptr; popupClosingOwned_ = true;

    delete pendingPopupDelete_; pendingPopupDelete_ = nullptr;

    for (auto& [name, stageRoot] : stages_) {
        if (stageRoot != root_)
            delete stageRoot;
    }
    stages_.clear();

    delete root_;    root_  = nullptr;
    for (auto* fw : floats_) delete fw;
    floats_.clear();
    delete iconAtlas_; iconAtlas_ = nullptr;  // stub
}

// ═════════════════════════════════════════════════════════════════════════════
//  Init
// ═════════════════════════════════════════════════════════════════════════════

bool WidgetApp::init()
{
    if (inited_) return true;

    iconAtlas_ = nullptr;  // IconAtlas not yet compiled

    root_ = nullptr;
    // WidgetSerializer::registerBuiltinTypes();  // re-enable when migrated

    inited_ = true;
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Per-frame input dispatch
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::update(const BuGUI::IO& io)
{
    dt_     = io.deltaTime;
    elapsedMs_ += static_cast<uint32_t>(dt_ * 1000.0f);

    // Cache modifier keys & clipboard callbacks from IO
    ioKeyCtrl_  = io.keyCtrl;
    ioKeyShift_ = io.keyShift;
    ioKeyAlt_   = io.keyAlt;
    clipSet_    = io.setClipboardText;
    clipGet_    = io.getClipboardText;

    int newW = static_cast<int>(io.displayWidth);
    int newH = static_cast<int>(io.displayHeight);
    if (newW != width_ || newH != height_) {
        width_  = newW;
        height_ = newH;
        if (width_ != 0 && height_ != 0)
            needsLayout_ = true;
    }

    float mx = io.mouseX;
    float my = io.mouseY;

    // Mouse move
    if (mx != prevMouseX_ || my != prevMouseY_)
        dispatchMouseMove(mx, my);

    // Mouse buttons (delta detection)
    for (int i = 0; i < 5; ++i)
    {
        if (io.mouseDown[i] && !prevMouseDown_[i])
            dispatchMousePress(mx, my, i);
        else if (!io.mouseDown[i] && prevMouseDown_[i])
            dispatchMouseRelease(mx, my, i);
    }

    // Scroll
    if (io.mouseWheelX != 0.0f || io.mouseWheelY != 0.0f)
        dispatchMouseScroll(mx, my, io.mouseWheelX, io.mouseWheelY);

    // Key press / release events
    for (const auto& ke : io.keyEvents)
    {
        // F1 → toggle debug layout
        if (ke.down && ke.scancode == 58 /*F1*/)
            debugLayout_ = !debugLayout_;
        else
            dispatchKeyEvent(ke);
    }

    // Text input
    if (!io.inputChars.empty())
        dispatchTextInput(io.inputChars);

    // OS-level file drops
    if (!io.dropEvents.empty())
        dispatchDropEvents(io.dropEvents);

    // Tooltip timer
    if (hovered_ && !hovered_->tooltip().empty())
    {
        tooltipTimer_ += dt_;
        if (!tooltipVisible_ && tooltipTimer_ >= hovered_->tooltipDelay()) {
            tooltipVisible_ = true;
            tooltipWidget_  = hovered_;
        }
    }
    else
    {
        tooltipTimer_   = 0.0f;
        tooltipVisible_ = false;
        tooltipWidget_  = nullptr;
    }

    // Flush deferred deletes
    for (auto* fw : pendingFloatDeletes_) delete fw;
    pendingFloatDeletes_.clear();
    delete pendingPopupDelete_; pendingPopupDelete_ = nullptr;

    // Tick animations
    Animator::instance().tick(dt_);

    // Tick popup animation
    if (popupAnimOpening_) {
        popupAnimProgress_ += dt_ / popupAnimDuration_;
        if (popupAnimProgress_ >= 1.0f) {
            popupAnimProgress_ = 1.0f;
            popupAnimOpening_  = false;
        }
    }
    if (popupAnimClosing_) {
        popupAnimProgress_ -= dt_ / popupAnimDuration_;
        if (popupAnimProgress_ <= 0.0f) {
            popupAnimProgress_ = 0.0f;
            popupAnimClosing_  = false;
            // Now actually destroy
            if (popupClosing_) {
                popupClosing_->resetPopupState();
                notifyWidgetRemoved(popupClosing_);
                if (popupClosingOwned_) {
                    delete pendingPopupDelete_;
                    pendingPopupDelete_ = popupClosing_;
                }
                popupClosing_     = nullptr;
                popupClosingOwner_= nullptr;
                popupClosingOwned_= true;
            }
        }
    }

    // Store prev state
    prevMouseX_ = mx;
    prevMouseY_ = my;
    for (int i = 0; i < 5; ++i)
        prevMouseDown_[i] = io.mouseDown[i];

    // Write cursor hint into IO so backend can read it
    BuGUI::GetIO().wantedCursor = static_cast<int>(wantedCursor_);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Input dispatch helpers (formerly SDL event handlers)
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::dispatchMouseMove(float x, float y)
{
    mouseX_ = x;
    mouseY_ = y;

    if (pressed_)
    {
        // Check drag threshold
        if (dragPending_ && !dragPayload_ && dragSource_) {
            float dx = x - dragStartX_;
            float dy = y - dragStartY_;
            if (dx * dx + dy * dy >= dragThreshold_ * dragThreshold_) {
                auto* p = new DragPayload(dragSource_->onDragBegin());
                p->source = dragSource_;
                dragPayload_ = p;
                dragPending_ = false;
            }
        }
        applyCursor(dragPayload_ ? CursorType::SizeAll : pressed_->cursor());
        MouseEvent me = makeMouseEvent(x, y);
        bubble(pressed_, me, &Widget::onMouseMove);
        return;
    }

    Widget* hit = popup_ ? hitTest(popup_, x, y) : nullptr;
    if (!hit && popup_ && popup_->popupContains(x, y))
        hit = popup_;
    if (!hit && popup_) {
        if (hovered_) updateHover(nullptr);
        return;
    }
    if (!hit) {
        for (int i = static_cast<int>(floats_.size()) - 1; i >= 0; --i) {
            auto* fw = floats_[i];
            if (!fw->isVisible()) continue;
            hit = hitTest(fw, x, y);
            if (hit) break;
        }
    }
    if (!hit)
        hit = hitTest(root_, x, y);

    if (hit != hovered_)
        updateHover(hit);

    if (hovered_) {
        MouseEvent me = makeMouseEvent(x, y);
        bubble(hovered_, me, &Widget::onMouseMove);
        // Re-read cursor — walk chain leaf→root, pick first non-Arrow
        CursorType cur = CursorType::Arrow;
        for (auto it = hoverChain_.rbegin(); it != hoverChain_.rend(); ++it) {
            if ((*it)->cursor() != CursorType::Arrow) {
                cur = (*it)->cursor();
                break;
            }
        }
        applyCursor(cur);
    }
}

void WidgetApp::dispatchMousePress(float x, float y, int btn)
{
    mouseX_ = x;
    mouseY_ = y;

    // ── Double/triple-click detection ────────────────────────────────────
    constexpr uint32_t kDblClickMs  = 400;  // max ms between clicks
    constexpr float    kDblClickDist = 5.0f; // max pixel drift
    float dx = x - lastClickX_, dy = y - lastClickY_;
    bool  sameSpot = (dx*dx + dy*dy) < kDblClickDist * kDblClickDist;
    bool  sameBtn  = (btn == lastClickBtn_);
    bool  inTime   = (elapsedMs_ - lastClickMs_) < kDblClickMs;
    if (sameBtn && sameSpot && inTime)
        clickSeq_ = std::min(clickSeq_ + 1, 3);
    else
        clickSeq_ = 1;
    lastClickMs_  = elapsedMs_;
    lastClickX_   = x;
    lastClickY_   = y;
    lastClickBtn_ = btn;

    if (popup_)
    {
        Widget* popupHit = hitTest(popup_, x, y);
        if (!popupHit && popup_->popupContains(x, y))
            popupHit = popup_;
        if (popupHit) {
            MouseEvent me = makeMouseEvent(x, y, btn);
            bubble(popupHit, me, &Widget::onMousePress);
            if (popup_) pressed_ = popupHit;
            return;
        }
        bool ownerToggle = (btn == 0) && popupOwner_ &&
                           hitTest(popupOwner_, x, y) != nullptr;
        if (!ownerToggle)
            closePopup();
    }

    for (int i = static_cast<int>(floats_.size()) - 1; i >= 0; --i) {
        auto* fw = floats_[i];
        if (!fw->isVisible()) continue;
        Widget* fwHit = hitTest(fw, x, y);
        if (fwHit) {
            bringToFront(fw);
            MouseEvent me = makeMouseEvent(x, y, btn);
            bubble(fwHit, me, &Widget::onMousePress);
            // fw may have been removed (and queued for deletion) during the event —
            // only keep pressed_ if the window is still alive in the float list.
            if (std::find(floats_.begin(), floats_.end(), fw) != floats_.end())
                pressed_ = fwHit;
            return;
        }
    }

    Widget* hit = hitTest(root_, x, y);
    if (hit) {
        if (btn == 1) {
            Widget* w = hit;
            while (w) {
                if (w->contextMenu()) {
                    ContextMenu::show(w->contextMenu(), x, y, w);
                    return;
                }
                w = w->parent();
            }
        }
        if (hit->acceptsFocus() && hit != focused_)
            setFocused(hit);

        // Check for drag source on left-button press
        if (btn == 0 && !dragPending_) {
            Widget* ds = hit;
            while (ds) {
                if (ds->isDragSource()) {
                    dragPending_ = true;
                    dragSource_  = ds;
                    dragStartX_  = x;
                    dragStartY_  = y;
                    break;
                }
                ds = ds->parent();
            }
        }

        MouseEvent me = makeMouseEvent(x, y, btn);
        bubble(hit, me, &Widget::onMousePress);
        pressed_ = hit;
    } else {
        setFocused(nullptr);
    }
}

void WidgetApp::dispatchMouseRelease(float x, float y, int btn)
{
    mouseX_ = x;
    mouseY_ = y;

    // Complete widget-to-widget drag & drop on left-button release
    if (btn == 0 && dragPayload_) {
        Widget* target = nullptr;
        for (int i = static_cast<int>(floats_.size()) - 1; i >= 0; --i) {
            auto* fw = floats_[i];
            if (!fw->isVisible()) continue;
            target = hitTest(fw, x, y);
            if (target) break;
        }
        if (!target)
            target = hitTest(root_, x, y);

        bool accepted = false;
        if (target) {
            // Walk up from target to find an acceptor
            Widget* w = target;
            while (w) {
                if (w->acceptsDrop(*dragPayload_)) {
                    w->onDropReceive(*dragPayload_);
                    accepted = true;
                    break;
                }
                w = w->parent();
            }
        }
        if (dragSource_)
            dragSource_->onDragEnd(accepted);
        delete dragPayload_;
        dragPayload_ = nullptr;
        dragSource_  = nullptr;
        dragPending_ = false;
    }
    if (btn == 0) {
        dragPending_ = false;
        dragSource_  = nullptr;
    }

    if (pressed_) {
        MouseEvent me = makeMouseEvent(x, y, btn);
        bubble(pressed_, me, &Widget::onMouseRelease);
        pressed_ = nullptr;
        Widget* hit2 = hitTest(root_, x, y);
        if (hit2 != hovered_) updateHover(hit2);
    }
}

// ── MouseEvent with current IO modifiers ─────────────────────────────────
MouseEvent WidgetApp::makeMouseEvent(float x, float y, int btn) const
{
    MouseEvent me;
    me.x = x; me.y = y; me.button = btn;
    me.clickCount = clickSeq_;
    me.ctrl  = ioKeyCtrl_;
    me.shift = ioKeyShift_;
    me.alt   = ioKeyAlt_;
    return me;
}

// ── Clipboard forwarding ─────────────────────────────────────────────────
void WidgetApp::setClipboardText(const char* text)
{
    if (clipSet_) clipSet_(text);
}

std::string WidgetApp::getClipboardText() const
{
    if (clipGet_) return clipGet_();
    return {};
}

void WidgetApp::dispatchMouseScroll(float x, float y, float sx, float sy)
{
    if (!hovered_) return;
    MouseEvent me = makeMouseEvent(x, y);
    me.scrollX = sx; me.scrollY = sy;
    bubble(hovered_, me, &Widget::onMouseScroll);
}

void WidgetApp::dispatchKeyEvent(const BuGUI::IO::KeyEvent& ke)
{
    // ── Tab / Shift+Tab focus cycling ────────────────────────────────────
    if (ke.down && ke.key == BuGUI::Key::Tab && !ke.ctrl && !ke.alt) {
        // First give the focused widget a chance to consume Tab
        if (focused_) {
            KeyEvent we;
            we.key = ke.key; we.scancode = ke.scancode;
            we.shift = ke.shift; we.ctrl = ke.ctrl; we.alt = ke.alt;
            focused_->onKeyPress(we);
            if (we.consumed) return;
        }
        // Collect all focusable widgets in DFS order
        std::vector<Widget*> focusable;
        collectFocusable(root_, focusable);
        for (auto* fw : floats_)
            collectFocusable(fw, focusable);
        if (focusable.empty()) return;

        auto it = std::find(focusable.begin(), focusable.end(), focused_);
        if (ke.shift) {
            if (it == focusable.begin() || it == focusable.end())
                it = focusable.end();
            --it;
        } else {
            if (it != focusable.end()) ++it;
            if (it == focusable.end()) it = focusable.begin();
        }
        setFocused(*it);
        return;
    }

    if (!focused_) return;
    KeyEvent we;
    we.key      = ke.key;
    we.scancode = ke.scancode;
    we.shift    = ke.shift;
    we.ctrl     = ke.ctrl;
    we.alt      = ke.alt;
    Widget* w = focused_;
    while (w && !we.consumed) {
        if (ke.down)  w->onKeyPress(we);
        else          w->onKeyRelease(we);
        w = w->parent();
    }
}

void WidgetApp::dispatchTextInput(const std::vector<uint32_t>& chars)
{
    if (!focused_) return;
    KeyEvent ke;
    // Convert UTF-32 codepoints to UTF-8 text[]
    int off = 0;
    for (uint32_t cp : chars) {
        if (cp < 0x80)        { if (off+1>28) break; ke.text[off++] = static_cast<char>(cp); }
        else if (cp < 0x800)  { if (off+2>28) break; ke.text[off++] = static_cast<char>(0xC0 | (cp >> 6)); ke.text[off++] = static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000){ if (off+3>28) break; ke.text[off++] = static_cast<char>(0xE0 | (cp >> 12)); ke.text[off++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); ke.text[off++] = static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp<=0x10FFFF){ if (off+4>28) break; ke.text[off++] = static_cast<char>(0xF0 | (cp >> 18)); ke.text[off++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); ke.text[off++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); ke.text[off++] = static_cast<char>(0x80 | (cp & 0x3F)); }
    }
    ke.text[off] = 0;
    Widget* w = focused_;
    while (w && !ke.consumed) { w->onTextInput(ke); w = w->parent(); }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Root management
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::setRoot(Widget* r)
{
    if (root_ != r)
    {
        delete root_;
        root_ = r;
    }
    focused_ = nullptr;
    hovered_ = nullptr;
    pressed_ = nullptr;
    hoverChain_.clear();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Stage management
// ═════════════════════════════════════════════════════════════════════════════

Widget* WidgetApp::addStage(const std::string& name)
{
    auto it = stages_.find(name);
    if (it != stages_.end())
        return it->second;

    auto* stageRoot = new Widget();
    stages_[name] = stageRoot;
    return stageRoot;
}

void WidgetApp::setTransition(TransitionType type, float durationSec, EaseType ease)
{
    transType_     = type;
    transDuration_ = durationSec;
    transEase_     = ease;
}

bool WidgetApp::setStage(const std::string& name)
{
    return setStage(name, transType_, transEase_);
}

bool WidgetApp::setStage(const std::string& name, TransitionType transition)
{
    return setStage(name, transition, transEase_);
}

bool WidgetApp::setStage(const std::string& name, TransitionType transition, EaseType ease)
{
    auto it = stages_.find(name);
    if (it == stages_.end()) return false;
    if (name == currentStage_) return true;

    // Clear interaction state (old tree pointers become invalid)
    focused_ = nullptr;
    hovered_ = nullptr;
    pressed_ = nullptr;
    hoverChain_.clear();
    needsLayout_ = true;

    // Start transition or instant swap
    if (transition != TransitionType::None && root_)
    {
        transActive_   = true;
        transProgress_ = 0.0f;
        transCurrent_  = transition;
        transEaseCur_  = ease;
        transOldRoot_  = root_;
        transOldStage_ = currentStage_;
    }

    // Swap root to the new stage's widget
    root_ = it->second;
    currentStage_ = name;
    return true;
}

Widget* WidgetApp::stage(const std::string& name) const
{
    auto it = stages_.find(name);
    return (it != stages_.end()) ? it->second : nullptr;
}

bool WidgetApp::removeStage(const std::string& name)
{
    if (name == currentStage_) return false;  // cannot remove active stage
    auto it = stages_.find(name);
    if (it == stages_.end()) return false;

    delete it->second;
    stages_.erase(it);
    return true;
}

IconAtlas& WidgetApp::iconAtlas()
{
    if (!iconAtlas_) { iconAtlas_ = new IconAtlas(); /* backend calls buildImage()+setTexture() */ }
    return *iconAtlas_;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Widget lookup
// ═════════════════════════════════════════════════════════════════════════════

std::vector<Widget*> WidgetApp::widgetsByTag(const std::string& tag)
{
    std::vector<Widget*> result;
    if (root_) root_->findByTag(tag, result);
    return result;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Global event dispatcher
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::on(const std::string& event, const std::string& widgetId, EventCallback cb)
{
    globalHandlers_[event + ":" + widgetId].push_back(std::move(cb));
}

void WidgetApp::onAny(const std::string& event, EventCallback cb)
{
    globalHandlers_[event + ":*"].push_back(std::move(cb));
}

void WidgetApp::fireEvent(const std::string& event, Widget* w)
{
    if (!w) return;

    // Targeted: by widget ID
    if (!w->id().empty())
    {
        auto it = globalHandlers_.find(event + ":" + w->id());
        if (it != globalHandlers_.end())
            for (auto& cb : it->second)
                cb(w);
    }

    // Catch-all
    auto it = globalHandlers_.find(event + ":*");
    if (it != globalHandlers_.end())
        for (auto& cb : it->second)
            cb(w);
}

// ═════════════════════════════════════════════════════════════════════════════
//  notifyWidgetRemoved - clear dangling pointers before widget deletion
// ═════════════════════════════════════════════════════════════════════════════

static bool isDescendantOf(Widget* candidate, Widget* root)
{
    Widget* w = candidate;
    while (w) {
        if (w == root) return true;
        w = w->parent();
    }
    return false;
}

void WidgetApp::notifyWidgetRemoved(Widget* w)
{
    if (!w) return;
    if (pressed_ && isDescendantOf(pressed_, w))
        pressed_ = nullptr;
    if (focused_ && isDescendantOf(focused_, w))
        focused_ = nullptr;
    if (dragSource_ && isDescendantOf(dragSource_, w))
        cancelDrag();
    if (hovered_ && isDescendantOf(hovered_, w))
    {
        hovered_ = nullptr;
        hoverChain_.clear();
    }
    else
    {
        // Remove any entries in hover chain that point to the subtree
        hoverChain_.erase(
            std::remove_if(hoverChain_.begin(), hoverChain_.end(),
                           [w](Widget* h) { return isDescendantOf(h, w); }),
            hoverChain_.end());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Float windows
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::addFloatImpl(FloatWindow* fw)
{
    if (!fw) return;
    floats_.push_back(fw);
    fw->layout();
}

void WidgetApp::removeFloat(FloatWindow* fw)
{
    if (!fw) return;
    notifyWidgetRemoved(fw);
    floats_.erase(std::remove(floats_.begin(), floats_.end(), fw), floats_.end());
    // Defer delete - may be called during signal/bubble handling
    pendingFloatDeletes_.push_back(fw);
}

void WidgetApp::bringToFront(FloatWindow* fw)
{
    auto it = std::find(floats_.begin(), floats_.end(), fw);
    if (it != floats_.end() && it != floats_.end() - 1)
    {
        floats_.erase(it);
        floats_.push_back(fw);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Popup overlay
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::showPopup(Widget* popup, Widget* owner, bool owned)
{
    // If a close animation is running, finish it immediately
    if (popupClosing_) {
        popupClosing_->resetPopupState();
        notifyWidgetRemoved(popupClosing_);
        if (popupClosingOwned_) {
            delete pendingPopupDelete_;
            pendingPopupDelete_ = popupClosing_;
        }
        popupClosing_     = nullptr;
        popupAnimClosing_ = false;
    }

    // Close previous popup instantly if any
    if (popup_) {
        popup_->resetPopupState();
        notifyWidgetRemoved(popup_);
        if (popupOwned_) {
            delete pendingPopupDelete_;
            pendingPopupDelete_ = popup_;
        }
    }

    popup_      = popup;
    popupOwner_ = owner;
    popupOwned_ = owned;

    // Start open animation
    popupAnimProgress_ = 0.0f;
    popupAnimOpening_  = true;
    popupAnimClosing_  = false;
    popupAnimEase_     = EaseType::OutCubic;
}

void WidgetApp::closePopup()
{
    if (popup_)
    {
        // Start close animation — keep popup alive during the tween
        popupClosing_      = popup_;
        popupClosingOwner_ = popupOwner_;
        popupClosingOwned_ = popupOwned_;

        popupAnimProgress_ = 1.0f;
        popupAnimClosing_  = true;
        popupAnimOpening_  = false;
        popupAnimEase_     = EaseType::InCubic;

        // Clear the active popup pointer so input goes through
        popup_      = nullptr;
        popupOwner_ = nullptr;
        popupOwned_ = true;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Hit testing
// ═════════════════════════════════════════════════════════════════════════════

Widget* WidgetApp::hitTest(Widget* w, float x, float y)
{
    if (!w || !w->isVisible())
        return nullptr;

    Rect abs = w->absoluteRect();
    if (!abs.contains(x, y))
        return nullptr;

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

template <typename Func>
void WidgetApp::bubble(Widget* target, MouseEvent& e, Func handler)
{
    Widget* w = target;
    while (w && !e.consumed)
    {
        fillLocal(w, e);
        (w->*handler)(e);
        w = w->parent();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Hover chain - track enter/leave for the full widget path
// ═════════════════════════════════════════════════════════════════════════════

std::vector<Widget*> WidgetApp::buildChain(Widget* w)
{
    std::vector<Widget*> chain;
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
    wantedCursor_ = type;
}

void WidgetApp::updateHover(Widget* newLeaf)
{
    std::vector<Widget*> newChain = newLeaf ? buildChain(newLeaf)
                                            : std::vector<Widget*>{};

    // Find common prefix length
    size_t common = 0;
    size_t minLen = std::min(hoverChain_.size(), newChain.size());
    while (common < minLen && hoverChain_[common] == newChain[common])
        ++common;

    // Leave: old widgets that are no longer in the chain (deepest first)
    for (size_t i = hoverChain_.size(); i > common; --i)
    {
        Widget* w = hoverChain_[i - 1];
        w->hovered_ = false;
        w->onMouseLeave();
        w->markDirty();
    }

    // Enter: new widgets that just appeared in the chain (shallowest first)
    for (size_t i = common; i < newChain.size(); ++i)
    {
        Widget* w = newChain[i];
        w->hovered_ = true;
        w->onMouseEnter();
        w->markDirty();
    }

    hoverChain_ = std::move(newChain);
    hovered_ = newLeaf;

    // Update cursor to match deepest hovered widget
    applyCursor(hovered_ ? hovered_->cursor() : CursorType::Arrow);

    // Reset tooltip when hover target changes
    tooltipTimer_   = 0.0f;
    tooltipVisible_ = false;
    tooltipWidget_  = nullptr;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Focus
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::setFocused(Widget* w)
{
    if (focused_) {
        focused_->setFocused(false);
        focused_->onFocusLost();
    }
    focused_ = w;
    if (focused_) {
        focused_->setFocused(true);
        focused_->onFocusGained();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Debug layout overlay
// ═════════════════════════════════════════════════════════════════════════════

namespace {

// Cycle through distinct colors per depth level
static Color debugColor(int depth)
{
    static const Color colors[] = {
        {255, 50,  50,  180},  // red
        {50,  255, 50,  180},  // green
        {80,  80,  255, 180},  // blue
        {255, 255, 50,  180},  // yellow
        {255, 50,  255, 180},  // magenta
        {50,  255, 255, 180},  // cyan
        {255, 160, 50,  180},  // orange
    };
    return colors[depth % 7];
}

void debugPaintWidget(Widget* w, BuGUI::DrawList& dl, int depth)
{
    if (!w || !w->isVisible()) return;

    BuGUI::Rect abs = { w->absoluteRect().x, w->absoluteRect().y,
                        w->absoluteRect().w, w->absoluteRect().h };
    Color c = debugColor(depth);
    dl.addRectOutline(abs, c);

    const Edges& m = w->margins();
    if (m.top > 0 || m.right > 0 || m.bottom > 0 || m.left > 0)
    {
        BuGUI::Rect outer = { abs.x - m.left, abs.y - m.top,
                              abs.w + m.left + m.right, abs.h + m.top + m.bottom };
        Color dim = {c.r, c.g, c.b, 80};
        dl.addRectOutline(outer, dim);
    }

    for (auto* child : w->children())
        debugPaintWidget(child, dl, depth + 1);
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
//  Easing functions
// ═════════════════════════════════════════════════════════════════════════════

static constexpr float PI = 3.14159265358979323846f;

static float bounceOut(float t)
{
    if (t < 1.0f / 2.75f)       return 7.5625f * t * t;
    if (t < 2.0f / 2.75f)     { t -= 1.5f   / 2.75f; return 7.5625f * t * t + 0.75f; }
    if (t < 2.5f / 2.75f)     { t -= 2.25f  / 2.75f; return 7.5625f * t * t + 0.9375f; }
    t -= 2.625f / 2.75f;        return 7.5625f * t * t + 0.984375f;
}

float applyEasing(EaseType type, float t)
{
    switch (type)
    {
    case EaseType::Linear:       return t;
    case EaseType::InQuad:       return t * t;
    case EaseType::OutQuad:      return t * (2.0f - t);
    case EaseType::InOutQuad:    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
    case EaseType::InCubic:      return t * t * t;
    case EaseType::OutCubic:     { float u = t - 1.0f; return u * u * u + 1.0f; }
    case EaseType::InOutCubic:   return t < 0.5f ? 4.0f * t * t * t
                                     : 1.0f - 0.5f * ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f));
    case EaseType::InExpo:       return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
    case EaseType::OutExpo:      return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
    case EaseType::InOutExpo:
        if (t == 0.0f || t == 1.0f) return t;
        return t < 0.5f ? 0.5f * std::pow(2.0f, 20.0f * t - 10.0f)
                        : 1.0f - 0.5f * std::pow(2.0f, -20.0f * t + 10.0f);
    case EaseType::InBack:       { constexpr float s = 1.70158f; return t * t * ((s + 1.0f) * t - s); }
    case EaseType::OutBack:      { constexpr float s = 1.70158f; float u = t - 1.0f; return u * u * ((s + 1.0f) * u + s) + 1.0f; }
    case EaseType::InOutBack:
    {
        constexpr float s = 1.70158f * 1.525f;
        float u = t * 2.0f;
        if (u < 1.0f) return 0.5f * (u * u * ((s + 1.0f) * u - s));
        u -= 2.0f;
        return 0.5f * (u * u * ((s + 1.0f) * u + s) + 2.0f);
    }
    case EaseType::InBounce:     return 1.0f - bounceOut(1.0f - t);
    case EaseType::OutBounce:    return bounceOut(t);
    case EaseType::InOutBounce:  return t < 0.5f ? 0.5f * (1.0f - bounceOut(1.0f - 2.0f * t))
                                                  : 0.5f * bounceOut(2.0f * t - 1.0f) + 0.5f;
    case EaseType::InElastic:
        return t == 0.0f || t == 1.0f ? t
             : -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * (2.0f * PI / 3.0f));
    case EaseType::OutElastic:
        return t == 0.0f || t == 1.0f ? t
             : std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * (2.0f * PI / 3.0f)) + 1.0f;
    case EaseType::InOutElastic:
        if (t == 0.0f || t == 1.0f) return t;
        return t < 0.5f
            ? -0.5f * std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * (2.0f * PI / 4.5f))
            :  0.5f * std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * (2.0f * PI / 4.5f)) + 1.0f;
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

void WidgetApp::setStageCamera(const std::string& name, const BuGUI::Camera2D& cam)
{
    stageCameras_[name] = cam;
}

BuGUI::Camera2D WidgetApp::stageCamera(const std::string& name) const
{
    auto it = stageCameras_.find(name);
    return it != stageCameras_.end() ? it->second : BuGUI::Camera2D{};
}

void WidgetApp::setStageBgColor(const std::string& name, const Color& c)
{
    stageBgColors_[name] = c;
}

Color WidgetApp::stageBgColor(const std::string& name) const
{
    auto it = stageBgColors_.find(name);
    return it != stageBgColors_.end() ? it->second : bgColor_;
}

// ═════════════════════════════════════════════════════════════════════════════
//  paintTreeInto – fills background then paints widget tree
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::paintTreeInto(Widget* tree, BuGUI::DrawList& dl,
                               const BuGUI::Font* font, IconAtlas* icons,
                               float w, float h, const Color& bgColor)
{
    if (!tree) return;

    tree->setRect({0, 0, w, h});
    for (auto* c : tree->children())
        c->setRect({0, 0, w, h});
    tree->layout();

    // Fill background so transitions have no holes between stages.
    dl.addRect({0, 0, w, h}, bgColor);

    PaintContext ctx{dl, font, icons};
    tree->paint(ctx);
}

// ═════════════════════════════════════════════════════════════════════════════
//  paint – layout + paint the full widget tree, pushing DrawPasses into data.
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::paint(BuGUI::DrawData& data,
                      const BuGUI::Font* font,
                      IconAtlas* icons)
{
    float w = static_cast<float>(width_);
    float h = static_cast<float>(height_);
    if (w <= 0 || h <= 0) return;
    font_  = font;  // cache for text measurement in mouse events
    drawList_ = &BuGUI::GetDrawList();

    // Layout pass
    if (needsLayout_ && root_)
    {
        root_->setRect({0, 0, w, h});
        for (auto* c : root_->children())
            c->setRect({0, 0, w, h});
        root_->layout();

        if (transActive_ && transOldRoot_)
        {
            transOldRoot_->setRect({0, 0, w, h});
            for (auto* c : transOldRoot_->children())
                c->setRect({0, 0, w, h});
            transOldRoot_->layout();
        }
        needsLayout_ = false;
    }

    // Transition
    if (transActive_)
    {
        transProgress_ += dt_ / transDuration_;
        if (transProgress_ >= 1.0f) {
            transProgress_ = 1.0f; transActive_ = false;
            transOldRoot_ = nullptr; transOldStage_.clear();
        }
        float t = applyEasing(transEaseCur_, transProgress_);

        // Each stage gets its own DrawList + Camera2D.
        // The base camera comes from the per-stage camera map;
        // transitions add a pan/scale offset on top.
        BuGUI::Camera2D baseCur = stageCamera(currentStage_);
        BuGUI::Camera2D baseOld = stageCamera(transOldStage_);

        BuGUI::Camera2D camOld = baseOld;
        BuGUI::Camera2D camNew = baseCur;

        BuGUI::DrawList& dlNew  = BuGUI::GetDrawList();
        BuGUI::DrawList& dlOld  = BuGUI::GetTransDrawList();

        switch (transCurrent_)
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
            camNew.pivotX = 0.5f; camNew.pivotY = 0.5f;
            break;
        case TransitionType::ZoomOut:
            // old zooms out from 1.0 → 0.5, new behind
            camOld.scale = baseOld.scale * (1.0f - 0.5f * t);
            camOld.pivotX = 0.5f; camOld.pivotY = 0.5f;
            break;
        default:
            break;
        }

        // For Reveal: new is below old → push new first.
        // For Cover/Slide: new is above old → push old first, new second.
        bool revealType = (transCurrent_ == TransitionType::RevealLeft  ||
                           transCurrent_ == TransitionType::RevealRight ||
                           transCurrent_ == TransitionType::RevealUp    ||
                           transCurrent_ == TransitionType::RevealDown  ||
                           transCurrent_ == TransitionType::ZoomOut);

        if (revealType)
        {
            // new (behind) first
            paintTreeInto(root_, dlNew, font, icons, w, h, stageBgColor(currentStage_));
            data.passes.push_back({&dlNew, camNew});
            paintTreeInto(transOldRoot_, dlOld, font, icons, w, h, stageBgColor(transOldStage_));
            data.passes.push_back({&dlOld, camOld});
        }
        else
        {
            // old (behind) first
            paintTreeInto(transOldRoot_, dlOld, font, icons, w, h, stageBgColor(transOldStage_));
            data.passes.push_back({&dlOld, camOld});
            paintTreeInto(root_, dlNew, font, icons, w, h, stageBgColor(currentStage_));
            data.passes.push_back({&dlNew, camNew});
        }
    }
    else
    {
        // Normal render: single pass with this stage's camera
        BuGUI::DrawList& dl = BuGUI::GetDrawList();
        BuGUI::Camera2D  cam = stageCamera(currentStage_);

        // Fill stage background first so there are no transparent holes.
        dl.addRect({0, 0, w, h}, stageBgColor(currentStage_));

        PaintContext ctx{dl, font, icons};
        if (root_) root_->paint(ctx);

        // Float windows (above stage, below popup)
        for (auto* fw : floats_)
        {
            if (!fw->isVisible()) continue;
            fw->layout();
            fw->paint(ctx);
        }

        // Popup (always on top) — with open/close animation
        Widget* popupToPaint = popup_ ? popup_ : popupClosing_;
        if (popupToPaint)
        {
            float rawT = popupAnimProgress_;
            bool animating = popupAnimOpening_ || popupAnimClosing_;
            float animT = animating
                          ? applyEasing(popupAnimEase_, std::max(0.0f, std::min(1.0f, rawT)))
                          : rawT;

            if (animating && animT < 0.99f) {
                // Scale from 0.92→1.0, slide from top by (1-t)*8px
                float scale  = 0.92f + 0.08f * animT;
                float slideY = (1.0f - animT) * -6.0f;

                Rect pr = popupToPaint->rect();
                float pivotX = pr.x + pr.w * 0.5f;
                float pivotY = pr.y;

                float newW = pr.w * scale;
                float newH = pr.h * scale;
                float newX = pivotX - newW * 0.5f;
                float newY = pivotY + slideY;

                // Clip to the final rect so partially-scaled content doesn't overflow
                Rect clipR = {newX, newY, newW, newH};
                ctx.pushClip(clipR);

                popupToPaint->setRect({newX, newY, newW, newH});
                popupToPaint->paint(ctx);
                popupToPaint->setRect(pr);

                ctx.popClip();
            } else {
                popupToPaint->paint(ctx);
            }
        }

        data.passes.push_back({&dl, cam});
    }

    // Debug layout overlay
    if (debugLayout_ && root_)
    {
        // Debug overlay goes into the current (new) stage's DrawList
        BuGUI::DrawList& dlDbg = BuGUI::GetDrawList();
        paintDebugOverlay(root_, dlDbg, 0);
    }

    // Overlay callback
    if (overlay_)
    {
        BuGUI::DrawList& dlOv = BuGUI::GetDrawList();
        PaintContext octx{dlOv, font, icons};
        overlay_(octx);
        // If not already in passes (transition case already pushed), ensure
        // the overlay is in the last pass (it shares the drawList).
    }

    // Tooltip
    if (tooltipVisible_ && tooltipWidget_ && font)
    {
        BuGUI::DrawList& dlTt = BuGUI::GetDrawList();
        paintTooltipInto(dlTt, font);
    }

    // Toast notifications
    Toast::tick(dt_);
    if (Toast::hasActive())
    {
        BuGUI::DrawList& dlToast = BuGUI::GetDrawList();
        PaintContext tctx{dlToast, font, icons};
        Toast::paint(tctx, w, h);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  paintDebugOverlay
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::paintDebugOverlay(Widget* w, BuGUI::DrawList& dl, int depth)
{
    debugPaintWidget(w, dl, depth);
}

// ═════════════════════════════════════════════════════════════════════════════
//  paintTooltipInto
// ═════════════════════════════════════════════════════════════════════════════

void WidgetApp::paintTooltipInto(BuGUI::DrawList& dl, const BuGUI::Font* font)
{
    if (!tooltipWidget_ || tooltipWidget_->tooltip().empty()) return;

    const auto& t = Theme::instance();
    const std::string& text = tooltipWidget_->tooltip();

    float w = static_cast<float>(width_);
    float h = static_cast<float>(height_);

    PaintContext ctx{dl, font, nullptr};
    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    ctx.font.SetColor(t.tooltipText);
    float textW = ctx.font.GetTextWidth(text.c_str());
    float textH = t.fontSize;

    float padX = 8.0f, padY = 4.0f;
    float tipW = textW + padX * 2;
    float tipH = textH + padY * 2;

    float tx = mouseX_ + 12.0f;
    float ty = mouseY_ + 18.0f;
    if (tx + tipW > w) tx = w - tipW - 2.0f;
    if (ty + tipH > h) ty = mouseY_ - tipH - 4.0f;
    if (tx < 0) tx = 2.0f;
    if (ty < 0) ty = 2.0f;

    ctx.fill.SetColor(t.tooltipBg);
    ctx.fillRoundedRect(tx, ty, tipW, tipH, 3.0f, 4);
    ctx.line.SetColor(t.tooltipBorder);
    ctx.lineRoundedRect(tx, ty, tipW, tipH, 3.0f, 4);
    ctx.font.SetFontSize(t.fontSize);
    ctx.font.SetBatch(&ctx.text);
    ctx.font.SetColor(t.tooltipText);
    float asc = ctx.font.GetAscender();
    ctx.font.Print(text.c_str(), tx + padX, ty + padY + asc);
}

float WidgetApp::textWidth(const char* text) const
{
    if (!font_ || !drawList_ || !text) return 0.0f;
    return drawList_->calcTextSize(*font_, text).x;
}

BuGUI::TextureHandle WidgetApp::loadImageTexture(const char* path, int& outW, int& outH)
{
    outW = outH = 0;
    if (!uploadTex_ || !path) return BuGUI::TextureHandle{0};

    BuImage img;
    if (!img.Load(path) || !img.IsValid()) return BuGUI::TextureHandle{0};

    // Ensure RGBA
    if (img.components != 4) {
        BuImage* rgba = img.ConvertToRGBA();
        if (!rgba || !rgba->IsValid()) { delete rgba; return BuGUI::TextureHandle{0}; }
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
    if (dragPayload_) {
        if (dragSource_)
            dragSource_->onDragEnd(false);
        delete dragPayload_;
        dragPayload_ = nullptr;
    }
    dragSource_  = nullptr;
    dragPending_ = false;
}

void WidgetApp::dispatchDropEvents(const std::vector<BuGUI::IO::DropEvent>& drops)
{
    for (const auto& drop : drops) {
        // Find the widget under the drop position
        Widget* target = nullptr;
        for (int i = static_cast<int>(floats_.size()) - 1; i >= 0; --i) {
            auto* fw = floats_[i];
            if (!fw->isVisible()) continue;
            target = hitTest(fw, drop.x, drop.y);
            if (target) break;
        }
        if (!target)
            target = hitTest(root_, drop.x, drop.y);

        if (target && !drop.filePaths.empty()) {
            // Walk up from target to find a handler
            Widget* w = target;
            while (w) {
                bool handled = !w->fileDrop.empty();
                w->onFileDrop(drop.filePaths);
                w->fileDrop.emit(drop.filePaths);
                if (handled) break;  // stop bubbling once a handler is found
                w = w->parent();
            }
        }
    }
}

} // namespace BuGUI
