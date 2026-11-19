#pragma once

#include "Retained.hpp"
#include "RetainedBase.hpp"
#include "Theme.hpp"
#include "Widget.hpp"
#include <cstdint>
#include <ct/function.hpp>
#include <ct/hashmap.hpp>
#include <ct/vector.hpp>
#include <igui/WindowStateIni.hpp>
#include <igui/widgets/String.hpp>


namespace ig
{
namespace retained
{

// ═════════════════════════════════════════════════════════════════════════════
//  Easing functions for transitions
// ═════════════════════════════════════════════════════════════════════════════

enum class EaseType
{
    Linear,
    InQuad,
    OutQuad,
    InOutQuad,
    InCubic,
    OutCubic,
    InOutCubic,
    InExpo,
    OutExpo,
    InOutExpo,
    InBack,
    OutBack,
    InOutBack,
    InBounce,
    OutBounce,
    InOutBounce,
    InElastic,
    OutElastic,
    InOutElastic,
};

float applyEasing(EaseType type, float t);

// ═════════════════════════════════════════════════════════════════════════════
//  Transition types for stage switching animations
// ═════════════════════════════════════════════════════════════════════════════

enum class TransitionType
{
    None,        // instant swap (no animation)
    SlideLeft,   // old exits left,  new enters from right
    SlideRight,  // old exits right, new enters from left
    SlideUp,     // old exits up,    new enters from bottom
    SlideDown,   // old exits down,  new enters from top
    CoverLeft,   // new slides in from right over stationary old
    CoverRight,  // new slides in from left  over stationary old
    CoverUp,     // new slides in from bottom over stationary old
    CoverDown,   // new slides in from top    over stationary old
    RevealLeft,  // old slides out left,  revealing stationary new
    RevealRight, // old slides out right, revealing stationary new
    RevealUp,    // old slides out up,    revealing stationary new
    RevealDown,  // old slides out down,  revealing stationary new
    ZoomIn,      // new zooms in from center (small → full)
    ZoomOut,     // old zooms out to center (full → small), new behind
};

class FloatWindow;


// ═════════════════════════════════════════════════════════════════════════════
//  WidgetApp – pure widget manager
//
//  The application (backend) owns the window, GL context and main loop.
//  WidgetApp manages the widget tree, layout, input dispatch and painting.
//
//  Typical frame:
//      widgetApp.update(ig::retained::GetIO());                    // input → widgets
//      widgetApp.paint(*ig::retained::GetDrawData(), font, icons); // layout + draw
//      ig::retained::Render();
//      backend.render(*ig::retained::GetDrawData());
// ═════════════════════════════════════════════════════════════════════════════

class WidgetApp
{
public:
    // A window's share of the state: its widget tree, focus, popups, floats,
    // stages and transitions. An application with several windows keeps one
    // Surface per window and swaps it in (swapSurface) before feeding or
    // painting that window; WidgetApp itself stays one.
    struct Surface
    {
        bool needsLayout_ = true;
        // Something the layout depends on changed (see Widget::invalidateLayout): the containers lay their children
        // out again, but the rectangles of the stage's own children stay as the application left them.
        bool relayout_ = false;
        int width_ = 0;
        int height_ = 0;
        Color bgColor_ = Color(0, 0, 0, 0); // alpha 0 (until setBgColor): the theme's bgColor, whichever the theme is

        // Cursor hint (set by applyCursor, read by backend via wantedCursor())
        CursorType wantedCursor_ = CursorType::Arrow;

        // Previous-frame IO state for press/release delta detection
        float prevMouseX_ = 0.0f;
        float prevMouseY_ = 0.0f;
        bool prevMouseDown_[5] = {};

        // Double/triple-click detection
        uint32_t lastClickMs_ = 0;
        float lastClickX_ = 0, lastClickY_ = 0;
        int lastClickBtn_ = -1;
        int clickSeq_ = 0; // 0→1→2→3 then resets

        Widget* root_ = nullptr;
        Widget* focused_ = nullptr;
        struct FocusLoan
        {
            Widget* holder;
            Widget* previous;
        };
        ct::Vector<FocusLoan> focusLoans_;
        Widget* hovered_ = nullptr;
        Widget* pressed_ = nullptr;
        int pressedButtons_ = 0; // buttons held since a press went to pressed_
        bool focusFromTab_ = false;
        struct DeferredCall
        {
            Widget* owner;
            ct::Function<void()> fn;
        };
        ct::Vector<DeferredCall> deferredCalls_;
        ct::Vector<Widget*> hoverChain_; // root → ... → deepest hovered
        float mouseX_ = 0;
        float mouseY_ = 0;

        // Popup overlay
        Widget* popup_ = nullptr;                 // popup widget (may or may not be owned)
        Widget* popupOwner_ = nullptr;            // widget that opened it (not owned)
        ct::Vector<Widget*> pendingPopupDeletes_; // deferred delete (safe after event loop)
        bool popupOwned_ = true;                  // if true, popup is deleted on close

        // Popup animation
        float popupAnimProgress_ = 1.0f;  // 0→1 open, 1→0 close
        float popupAnimDuration_ = 0.12f; // seconds
        bool popupAnimOpening_ = false;
        bool popupAnimClosing_ = false;
        EaseType popupAnimEase_ = EaseType::OutCubic;
        // Closing state: kept alive during close animation
        Widget* popupClosing_ = nullptr;
        Widget* popupClosingOwner_ = nullptr;
        bool popupClosingOwned_ = true;

        // Float windows (painted above stage, below popup)
        ct::Vector<FloatWindow*> floats_;
        ct::Vector<FloatWindow*> pendingFloatDeletes_; // deferred delete (safe after event loop)

        // Float window state, kept in memory between loads and saves
        WindowStateIni windowState_;
        // Dock layouts are applied once the stages they live in have been built.
        bool dockLayoutPending_ = false;

        // Tooltip
        float tooltipTimer_ = 0.0f;
        bool tooltipVisible_ = false;
        Widget* tooltipWidget_ = nullptr;

        // What the next frame needs (WidgetApp::needsFrame): the state of what was last painted
        uint32_t paintedChanges_ = 0;   // gWidgetChanges when the last frame was painted
        bool forcePaint_ = true;        // a frame must be painted whatever else says: the first, a resize, requestFrame()
        bool inputSeen_ = false;        // update() dispatched an event, or the pointer or a button had moved, since the last paint
        bool tooltipPainted_ = false;   // whether the last frame painted the tooltip
        double wakeAt_ = 1.0e300;       // the application clock at which time alone needs a frame (requestFrameIn), 1e300 for never

        // Stage system
        ct::HashMap<String, Widget*> stages_;                      // name → root widget
        ct::HashMap<String, ig::retained::Camera2D> stageCameras_; // per-stage camera
        ct::HashMap<String, Color> stageBgColors_;                 // per-stage background
        String currentStage_;

        // Transition
        TransitionType transType_ = TransitionType::SlideLeft;
        EaseType transEase_ = EaseType::InOutCubic;
        float transDuration_ = 0.3f; // seconds
        bool transActive_ = false;
        float transProgress_ = 0.0f;                         // 0..1
        TransitionType transCurrent_ = TransitionType::None; // type of active transition
        EaseType transEaseCur_ = EaseType::InOutCubic;       // ease of active transition
        Widget* transOldRoot_ = nullptr;
        String transOldStage_;

        // Optional overlay callback
        ct::Function<void(PaintContext&)> overlay_;

        // ── Drag & Drop state ─────────────────────────────────────────────────
        DragPayload* dragPayload_ = nullptr; // active payload (owned)
        Widget* dragSource_ = nullptr;       // widget that started the drag
        float dragStartX_ = 0, dragStartY_ = 0;
        bool dragPending_ = false;   // mouse down on dragSource, waiting threshold

    };

    // Exchanges the live per-window state with `other`.
    void swapSurface(Surface& other);
    // Deletes the live per-window state's widgets (tree, stages, popups,
    // floats), leaving an empty surface.
    void releaseSurface();

    /// @brief Get the singleton instance.
    static WidgetApp& instance();

    /// @brief The instance when it exists, nullptr otherwise. Used by widgets
    ///        that are destroyed on their own (a `delete` by the application,
    ///        a stage going away) to tell the manager about it without creating
    ///        it again during static destruction.
    static WidgetApp* existing();

    /// @brief Initialize the widget system (call after GL context ready).
    bool init();

    /// @brief Release all resources.
    void shutdown();

    // ── Per-frame API ──────────────────────────────────────────────────────
    /// @brief Dispatch IO events to the widget tree. The frame's events (mouseEvents, keyEvents, inputChars,
    ///        dropEvents) are taken out of @p io first: what a handler adds to the IO (the frames of a
    ///        nested loop it runs do) belongs to the next update() to take it, never to this one, and
    ///        nothing this update walks can be changed by a handler.
    void update(ig::retained::IO& io);

    // ── Synthetic input ───────────────────────────────────────────────────
    // Drive the widgets with no window and no backend: a test, a script, a tool that clicks for a model. Each call
    // delivers ONE event now, through update() as a backend's frame would have (the same dispatch, the same
    // hit test, drag, focus and popup rules), with the state of the pointer and of the buttons that update() last
    // saw, the size of the window as it is, and no time passing (the delta time of that update is zero: popups do
    // not advance their fade, tooltips do not count, and two clicks on the same spot with no frame between them
    // are a double click). The layout is what the last paint() made of it: paint a frame (or call requestLayout()
    // and paint) before the first event, and after a change that moves widgets. The IO of the backend, if
    // there is one, is not given events; only its polled pointer and buttons are brought up to date, as a backend
    // would, so that the next frame's update(GetIO()) finds the state the events left and does not move the pointer
    // back. Nothing here costs anything when it is not called.
    //
    //     app.feedMouseMove(x, y);
    //     app.feedMouseButton(0, true);   // press the left button where the pointer is
    //     app.feedMouseButton(0, false);  // and let go: a click
    //     app.feedKey(Key::Return, true); app.feedKey(Key::Return, false);
    //     app.feedText("Música");         // UTF-8
    //
    // Buttons number as the widgets' MouseEvent::button: 0 left, 1 right, 2 middle. A press of a button that is
    // already down, or a release of one that is not, does nothing (as in update()).

    /// @brief The pointer moved to (x, y), window coordinates.
    void feedMouseMove(float x, float y);
    /// @brief A button was pressed (down) or released, with the pointer where it was last put.
    void feedMouseButton(int button, bool down, bool shift = false, bool ctrl = false, bool alt = false);
    /// @brief The wheel turned (dx, dy as the backend reports them, positive up), with the pointer where
    ///        it was last put.
    void feedMouseWheel(float dx, float dy, bool shift = false, bool ctrl = false, bool alt = false);
    /// @brief A key went down or up (@p key as ig::retained::Key::..., the platform key code), with the modifiers
    ///        given held for this event only. Typing text is feedText(): a key press does not type.
    void feedKey(int key, bool down = true, bool shift = false, bool ctrl = false, bool alt = false);
    /// @brief Text was typed (UTF-8, any length: each character goes in as one typed character).
    void feedText(const char* utf8);

    /// @brief Layout and paint the widget tree into draw data.
    void paint(ig::retained::DrawData& data, const ig::retained::Font* font = nullptr,
            IconAtlas* icons = nullptr);

    /// @brief Get the cursor type the backend should apply.
    CursorType wantedCursor() const
    {
        return s_.wantedCursor_;
    }

    /// @brief Get the background color.
    const Color& bgColor() const
    {
        return s_.bgColor_.a > 0 ? s_.bgColor_ : Theme::instance().bgColor;
    }
    /// @brief Set the background color.
    void setBgColor(const Color& c)
    {
        s_.bgColor_ = c;
    }

    // ── Dimensions (read from IO each frame) ──────────────────────────────
    /// @brief Get the window width in pixels.
    int width() const
    {
        return s_.width_;
    }
    /// @brief Get the window height in pixels.
    int height() const
    {
        return s_.height_;
    }
    /// @brief Get the frame delta time in seconds.
    float deltaTime() const
    {
        return dt_;
    }

    /// @brief Get elapsed time in milliseconds since init.
    uint32_t elapsedMs() const
    {
        return elapsedMs_;
    }

    // ── Platform-agnostic clipboard (set by backend via IO) ──────────────
    /// @brief Set clipboard text (platform-agnostic).
    void setClipboardText(const char* text);
    /// @brief Get clipboard text.
    String getClipboardText() const;
    /// @brief Set the PRIMARY selection (what a selection of text is, to a middle click): a no-op where the backend has none (IO::setPrimaryText).
    void setPrimaryText(const char* text);
    /// @brief The PRIMARY selection; where the backend has none, the normal clipboard (so a middle click pastes something).
    String getPrimaryText() const;

    // ── Mouse mode (set by backend via IO) ───────────────────────────────
    /// @brief Hide the pointer and keep reporting its motion past the window
    ///        edges (true), or show it again (false).
    void setMouseCaptured(bool captured);
    /// @brief Put the pointer at x, y (window coordinates).
    void warpMouse(float x, float y);

    /// @brief Get the cached font from the last paint() call.
    const ig::retained::Font* font() const
    {
        return font_;
    }
    /// @brief Measure text width using the cached font.
    float textWidth(const char* text) const;
    /// @brief Measure text width with a font (nullptr: the cached font) and a scale.
    float textWidth(const char* text, const ig::retained::Font* font, float scale = 1.0f) const;
    /// @brief Get approximate frames per second.
    int fps() const
    {
        return dt_ > 0.0f ? static_cast<int>(1.0f / dt_) : 0;
    }
    /// @brief Get the current mouse X position.
    float mouseX() const
    {
        return s_.mouseX_;
    }
    /// @brief Get the current mouse Y position.
    float mouseY() const
    {
        return s_.mouseY_;
    }

    /// @brief Toggle debug layout wireframe overlay.
    bool debugLayout() const
    {
        return debugLayout_;
    }
    /// @brief Enable/disable debug layout wireframe.
    void setDebugLayout(bool d)
    {
        debugLayout_ = d;
    }
    /// @brief Scancode that toggles the debug layout (0, the default, for none).
    ///        While set, that key does not reach the widgets.
    void setDebugLayoutKey(int scancode)
    {
        debugLayoutKey_ = scancode;
    }

    /// @brief Set per-frame overlay paint callback.
    void setOverlayCallback(ct::Function<void(PaintContext&)> cb)
    {
        s_.overlay_ = std::move(cb);
    }

    // ── Drag & Drop state (read by widgets/app during frame) ──────────────
    /// @brief Check if a widget drag is in progress.
    bool isDragging() const
    {
        return s_.dragPayload_ != nullptr;
    }
    /// @brief Get the current drag payload.
    const DragPayload* dragPayload() const
    {
        return s_.dragPayload_;
    }
    /// @brief Get the drag source widget.
    Widget* dragSource() const
    {
        return s_.dragSource_;
    }

    /// @brief Set the drag threshold in pixels.
    void setDragThreshold(float px)
    {
        dragThreshold_ = px;
    }
    /// @brief Get the drag threshold.
    float dragThreshold() const
    {
        return dragThreshold_;
    }

    // ── Texture services (set by backend at init) ─────────────────────────
    // The backend registers these so widgets can create/destroy GPU textures
    // without depending on any specific graphics API.
    using UploadTextureFn =
            ct::Function<ig::retained::TextureHandle(const unsigned char* rgba, int w, int h)>;
    using DestroyTextureFn = ct::Function<void(ig::retained::TextureHandle)>;
    using UpdateTextureFn = ct::Function<bool(ig::retained::TextureHandle,
            const unsigned char* rgba, int w, int h)>;

    /// @brief Register a texture upload function.
    void setTextureUpload(UploadTextureFn fn)
    {
        uploadTex_ = std::move(fn);
    }
    /// @brief Register a texture destroy function.
    void setTextureDestroy(DestroyTextureFn fn)
    {
        destroyTex_ = std::move(fn);
    }

    /// @brief Register a function that replaces the pixels of an existing texture.
    void setTextureUpdate(UpdateTextureFn fn)
    {
        updateTex_ = std::move(fn);
    }

    /// @brief Upload RGBA pixels to GPU texture.
    ig::retained::TextureHandle uploadTexture(const unsigned char* rgba, int w, int h)
    {
        return uploadTex_ ? uploadTex_(rgba, w, h) : ig::retained::TextureHandle{ 0 };
    }

    /// @brief Replace the pixels of a texture from uploadTexture; w and h must match its size.
    /// Meant for video: call it once per frame (glTexSubImage2D on a GL backend).
    bool updateTexture(ig::retained::TextureHandle tex, const unsigned char* rgba, int w, int h)
    {
        return updateTex_ && tex ? updateTex_(tex, rgba, w, h) : false;
    }

    /// @brief Gives a built FontAtlas (see FontAtlas::build, its texture already set) the dynamic
    /// page: from now on the glyphs the baked ranges lack, and every size asked of
    /// Font::atHeight, are rasterised when first drawn and the page is uploaded again after the
    /// paint() that baked something. Needs the texture upload / update functions registered. An
    /// atlas is added once; it must outlive this WidgetApp (or be dropped with
    /// removeDynamicAtlas before it goes). False when there is no upload function or the atlas
    /// has no faces; the atlas then keeps working as before.
    bool addDynamicAtlas(ig::retained::FontAtlas& atlas);
    /// @brief Stops uploading the page of `atlas` (and frees its texture).
    void removeDynamicAtlas(ig::retained::FontAtlas& atlas);

    /// @brief Drops what the manager keeps of @p atlas: the font cached by the last paint() when
    /// it belongs to the atlas, and the atlas itself from the dynamic list. FontAtlas calls it as
    /// it is destroyed or rebuilt, so measuring text afterwards never reads a freed font.
    void forgetAtlas(const ig::retained::FontAtlas& atlas);
    /// @brief Drops the draw list cached by the last paint() when it is @p list (DestroyContext
    /// calls it for the list of the context going away).
    void forgetDrawList(const ig::retained::DrawList* list)
    {
        if (drawList_ == list) drawList_ = nullptr;
    }

    /// @brief Free a previously uploaded texture.
    void destroyTexture(ig::retained::TextureHandle tex)
    {
        if (destroyTex_ && tex) destroyTex_(tex);
    }

    /// @brief Load an image file to GPU texture.
    ig::retained::TextureHandle loadImageTexture(const char* path, int& outW, int& outH);

    // ── Root widget ───────────────────────────────────────────────────────
    /// @brief Get the root widget.
    Widget* root() const
    {
        return s_.root_;
    }

    /// @brief Replace the root widget (takes ownership, deletes old).
    void setRoot(Widget* r);

    // ── Stage management ──────────────────────────────────────────────────
    // A Stage is a named scene (widget tree). Only one stage is active/visible.
    // Inactive stages are kept in memory for instant switching.
    //
    //   app.addStage("menu");
    //   app.stage("menu")->createChild<Label>("Main Menu");
    //   app.addStage("game");
    //   app.stage("game")->createChild<Canvas>(...);
    //   app.setStage("menu");  // shows menu
    //   app.setStage("game");  // switches to game, menu preserved

    /// @brief Create a named stage. Returns its root widget.
    Widget* addStage(const String& name);

    /// @brief Switch to a named stage.
    bool setStage(const String& name);
    /// @brief Switch with a specific transition.
    bool setStage(const String& name, TransitionType transition);
    /// @brief Switch with transition and easing.
    bool setStage(const String& name, TransitionType transition, EaseType ease);

    /// @brief Set default transition type, easing and duration.
    void setTransition(TransitionType type, float durationSec = 0.3f,
            EaseType ease = EaseType::InOutCubic);
    /// @brief Get the transition type.
    TransitionType transitionType() const
    {
        return s_.transType_;
    }
    /// @brief Get the transition easing.
    EaseType transitionEase() const
    {
        return s_.transEase_;
    }
    /// @brief Get the transition duration in seconds.
    float transitionDuration() const
    {
        return s_.transDuration_;
    }
    /// @brief Check if a stage transition is active.
    bool isTransitioning() const
    {
        return s_.transActive_;
    }

    /// @brief Get a stage's root widget by name.
    Widget* stage(const String& name) const;

    /// @brief Get the current stage name (empty if unused).
    const String& currentStageName() const
    {
        return s_.currentStage_;
    }

    /// @brief Remove a stage and delete its widget tree.
    bool removeStage(const String& name);

    /// @brief Set a 2D camera for a named stage.
    void setStageCamera(const String& name, const ig::retained::Camera2D& cam);
    /// @brief Get the camera for a named stage.
    ig::retained::Camera2D stageCamera(const String& name) const;

    /// @brief Set a per-stage background color.
    void setStageBgColor(const String& name, const Color& c);
    /// @brief Get a stage's background color.
    Color stageBgColor(const String& name) const;

    /// @brief Request a layout pass on the next frame: the window size is applied again, so every child of
    ///        the stage takes the whole window (a resize, a stage change and the widgets that always asked
    ///        for it do this).
    void requestLayout()
    {
        s_.needsLayout_ = true;
    }
    /// @brief Request a layout pass on the next frame that lays the containers out again and leaves the
    ///        rectangles of the stage's children alone: a rectangle the application gave a widget that sits
    ///        directly on the stage (a panel or an overlay placed by hand) stays; only a child that has no
    ///        size yet (just added) takes the whole window. Widget::invalidateLayout() asks for this.
    void requestRelayout()
    {
        s_.relayout_ = true;
    }

    /// @brief Move the focus from @p from to the next widget Tab reaches, or with
    ///        @p backward to the previous one, as Tab and Shift+Tab do.
    void focusNeighbor(Widget* from, bool backward);
    /// @brief Check if the focus is moving by Tab or Shift+Tab (true while the
    ///        widgets hear about that move).
    bool focusFromTab() const
    {
        return s_.focusFromTab_;
    }

    /// @brief Call @p fn at the end of this update(), after every event of the
    ///        frame. The call is dropped when @p owner goes away first.
    void callDeferred(Widget* owner, ct::Function<void()> fn);

    /// @brief Clear internal refs to a widget being destroyed.
    void notifyWidgetRemoved(Widget* w);

    /// @brief Deletes a widget already taken out of its tree (Widget::removeChild): right away,
    ///        or, while update() or paint() is running, when it is done. A handler can then remove
    ///        the widget it is running in - a combo box whose selection rebuilds the panel it is
    ///        on - and the code after the signal, which still touches the widget, reads memory
    ///        that is there.
    void deleteLater(Widget* w);

    /// @brief How many widgets the manager has been told about going away.
    ///        A caller that runs application code (an event handler, a signal)
    ///        compares it before and after: when it changed, the widgets it was
    ///        holding may be gone, so it can drop them instead of touching them.
    uint32_t deletions() const
    {
        return deletions_;
    }
    /// @brief The question a widget asks after it ran a signal and has more to do: with @p seen being deletions() as it was
    ///        before the signal, true when something went away meanwhile and @p w is no longer in the tree that is drawn (a
    ///        float window, a popup) - a handler deleted it, or removed it (Widget::removeChild), or the panel it is on. The
    ///        widget must then touch nothing of its own and return; with nothing deleted it costs the comparison. @p w is
    ///        only compared, never read.
    ///
    ///        THE RULE for a handler that removes the widget it runs in (a button that closes its own panel, a list that
    ///        rebuilds itself on a selection): it does it with parent->removeChild(w), the only way to delete a widget that
    ///        is in a tree (a plain `delete` leaves a pointer to freed memory in the parent). The memory goes when the
    ///        frame phase that is running (update(), paint()) is over, or at once when none is (the program calling
    ///        list->setSelectedIndex(), slider->setValue()). The widgets that run a handler and then have more to do ask
    ///        hasGone() after it and touch nothing of their own, emit nothing more, when it says gone: every signal of
    ///        Widget (pressed, released, clicked, moved, hover), PianoRoll's onNote*, the selectionChanged of ListBox and
    ///        ListWidget, Slider's onValueChanged, and the actions of a Menu (which is not in a parent: it is a popup, and
    ///        its owner deletes it; the action's own `gone_` says the action went). Round 7 went through the rest (docs/ROUND7.md
    ///        says which signals were run, and which were not): most widgets now emit as the LAST thing a method does
    ///        (emitGone() where it is not), the CodeEditor holds the deletion (DeletionHold). Where a widget is not on that
    ///        list, a handler that must remove it asks for it to be done after the frame's events (WidgetApp::callDeferred).
    bool hasGone(const Widget* w, uint32_t seen) const
    {
        return deletions_ != seen && !isWidgetInTrees(w);
    }
    /// @brief For a widget whose work after a signal is too long to check step by step (a code editor: a hundred paths from a
    ///        key to the text, each emitting from the middle of a sequence): while one of these is alive, a widget taken out of a
    ///        tree (Widget::removeChild) is not deleted, it waits, as in a frame phase, until the last one ends. The widget holds
    ///        one in each of its entry points (a key, a click, text, undo) and the handler that removes it, in a signal of the
    ///        editor or in what the editor was running, can no longer pull the memory from under the rest of the entry point.
    ///        What it does not do is stop the work: the editor may emit what comes next, for a widget that is already out.
    class DeletionHold
    {
    public:
        DeletionHold() : app_(WidgetApp::instance())
        {
            ++app_.busy_;
        }
        ~DeletionHold()
        {
            if (--app_.busy_ == 0) app_.flushWidgetDeletes();
        }
        DeletionHold(const DeletionHold&) = delete;
        DeletionHold& operator=(const DeletionHold&) = delete;

    private:
        WidgetApp& app_;
    };
    /// @brief Emits @p signal of the widget @p w with @p args and says whether a handler took @p w out of the tree (hasGone):
    ///        true, and the widget touches nothing of its own after it, emits nothing more. For a signal that is not the last
    ///        thing a method does; one that is last needs none of this (`markDirty();` and the like go BEFORE it).
    template<typename... Params, typename... Args>
    bool emitGone(const Widget* w, const Signal<Params...>& signal, Args&&... args)
    {
        const uint32_t seen = deletions_;
        signal.emit(static_cast<Args&&>(args)...);
        return hasGone(w, seen);
    }

    // ── Float windows ───────────────────────────────────────────────────
    // Floating windows always render on top of the stage (below popups).
    template<typename T = FloatWindow, typename... Args>
    T* addFloat(Args&&... args)
    {
        auto* fw = new T(std::forward<Args>(args)...);
        addFloatImpl(fw);
        return fw;
    }
    /// @brief Remove and delete a float window.
    void removeFloat(FloatWindow* fw);
    /// @brief Bring a float window to the top of the stack.
    void bringToFront(FloatWindow* fw);
    /// @brief Get the list of float windows.
    const ct::Vector<FloatWindow*>& floats() const
    {
        return s_.floats_;
    }

    // ── Window state (.ini, like ImGui's imgui.ini) ───────────────────────
    // Float windows keep a section named after their title holding their
    // position, size and minimized flag; dock panels keep their layout under a
    // "dock:" section named after DockPanel::settingsKey(). The file is read as
    // soon as a path is set, applied to every float as it is added (and to the
    // dock panels once the stages are built), and rewritten on shutdown() - or
    // on demand with saveWindowState().
    //
    // Desktop only: neither a browser tab nor an Android app can keep the file,
    // so there the state stays in memory and the disk is never touched.

    /// @brief Point the state file at @p path (already-loaded windows move).
    ///        An empty path turns persistence off.
    void setWindowStatePath(const String& path);
    /// @brief Get the state file path (empty when persistence is off).
    const String& windowStatePath() const
    {
        return s_.windowState_.path();
    }
    /// @brief (Re)read the state file; existing floats and docks snap to it.
    /// @return true when the file was read.
    bool loadWindowState();
    /// @brief Capture the current floats and dock layouts, then write the file.
    /// @return true when the file was written.
    bool saveWindowState();

    // ── Focus ─────────────────────────────────────────────────────────────
    /// @brief Set the focused widget.
    void setFocused(Widget* w);
    /// @brief Get the currently focused widget.
    Widget* focused() const
    {
        return s_.focused_;
    }
    /// @brief Focus @p target while @p holder is open; when @p holder closes
    ///        or goes away, the focus goes back to the widget that had it.
    void lendFocus(Widget* holder, Widget* target);

    // ── Widget lookup ─────────────────────────────────────────────────────
    /// @brief Find a widget by ID.
    Widget* widget(const String& id)
    {
        return s_.root_ ? s_.root_->findById(id) : nullptr;
    }

    /// @brief Find a widget by ID and cast to type T.
    template<typename T>
    T* widget(const String& id)
    {
        return s_.root_ ? s_.root_->findById<T>(id) : nullptr;
    }

    /// @brief Find all widgets with a given tag.
    ct::Vector<Widget*> widgetsByTag(const String& tag);

    template<typename T>
    ct::Vector<T*> widgetsByTag(const String& tag)
    {
        return s_.root_ ? s_.root_->findByTag<T>(tag) : ct::Vector<T*>{};
    }

    // ── Global event dispatcher ───────────────────────────────────────────
    // Register callbacks for widgets by ID. Works with JSON-loaded trees.
    //   app.on("click",   "save-btn",       [](Widget* w){ ... });
    //   app.on("press",   "my-button",      [](Widget* w, int btn){ ... });
    //   app.on("release", "my-button",      [](Widget* w, int btn){ ... });
    //   app.on("change",  "opacity-slider", [](Widget* w, float v){ ... });
    //   app.on("toggle",  "wireframe-chk",  [](Widget* w, bool v){ ... });
    //   app.on("hover",   "panel",          [](Widget* w){ ... });
    //   app.on("leave",   "panel",          [](Widget* w){ ... });
    //   app.on("move",    "canvas",         [](Widget* w, float lx, float ly){ ... });
    using EventCallback = ct::Function<void(Widget*)>;
    void on(const String& event, const String& widgetId, EventCallback cb);

    // Global catch-all: fires for every widget on that event
    //   app.onAny("click", [](Widget* w){ if (w->hasTag("btn")) ... });
    void onAny(const String& event, EventCallback cb);

    // Fire a global event (called internally by WidgetApp after signal emit)
    void fireEvent(const String& event, Widget* w);

    // ── Popup overlay ─────────────────────────────────────────────────
    /// @brief Show a popup widget on top of everything.
    void showPopup(Widget* popup, Widget* owner = nullptr, bool owned = true);
    /// @brief Drop every popup reference to @p w without deleting it.
    void forgetPopup(Widget* w);
    /// @brief Close the current popup.
    void closePopup();
    /// @brief Get the active popup widget.
    Widget* popup() const
    {
        return s_.popup_;
    }
    /// @brief Get the widget that opened the popup.
    Widget* popupOwner() const
    {
        return s_.popupOwner_;
    }

    /// @brief Get the shared icon atlas.
    IconAtlas& iconAtlas();

    // ── Theme ────────────────────────────────────────────────────────
    /// @brief Replace the theme (Theme::instance()) with @p theme, then tell everything that depends on it:
    ///        themeChanged is emitted once and the containers are laid out again (the sizes that come from the theme:
    ///        font size, padding, bar thickness). Colours an application set explicitly on a widget (setBgColor,
    ///        ConsoleWidget::setColors, ...) are left as they are; themeChanged is where the application sets them again.
    ///        @p parts: ThemeParts::All takes the whole theme (the default); ThemeParts::Colors takes its colours and looks and
    ///        keeps the sizes the application's layouts were made with (Theme::applyColors): themeChanged is emitted once and NO
    ///        layout is asked for, because nothing a layout reads changed (a macOS palette over the metrics of the dark theme: the
    ///        rectangles of every widget stay where they are); ThemeParts::Metrics takes only the sizes.
    void setTheme(const Theme& theme, ThemeParts parts = ThemeParts::All);
    /// @brief setTheme() with a preset by name (Theme::presetName: "dark", "light", "xp", "macos", "macos-dark").
    /// @return false, with the theme left alone, when @p name is not a preset.
    bool setThemePreset(const char* name, ThemeParts parts = ThemeParts::All);
    /// @brief Tell the manager that Theme::instance() was changed by hand (a field written, the whole theme assigned): the
    ///        same as setTheme() after the fact. update() also finds such a change by comparing the theme with the one it
    ///        saw last, so a frame after an assignment emits themeChanged even when this was not called; a change that
    ///        has been notified (by this, by setTheme() or by update()) is not emitted again.
    void notifyThemeChanged();
    // ── Frames that are not needed ───────────────────────────────────
    /// @brief Whether a frame has to be painted (and presented) now: something changed since the last one that was painted, or time
    ///        alone asks for one. A frame that is not needed draws what the last one drew, so an application that loops
    ///        (NewFrame, update, paint, present) can skip the paint and the present of the others, and sleep (see nextFrameInMs).
    ///        Counts as a change: any property set on a widget that marks it dirty (every setter of the library does), a layout
    ///        asked for, a theme replaced, a window resized, every event update() dispatched (the pointer moving, a button, a
    ///        key, text, the wheel, a drop), an animation or a transition running, a popup opening or closing, a toast, a
    ///        tooltip that appeared or went, a widget that asked (a caret to blink: requestFrameIn), and requestFrame().
    ///        An application that changes what is drawn without going through a widget (an overlay callback paints: while one is
    ///        set every frame is needed; a texture a VideoCanvas shows, a value only it keeps) calls requestFrame().
    bool needsFrame() const;
    /// @brief 0 when a frame is needed now (needsFrame); otherwise the milliseconds until time alone needs one (a caret that blinks, a
    ///        tooltip that is due), at least 1; -1 when nothing is waiting for time: only an event or a requestFrame() will.
    int nextFrameInMs() const;
    /// @brief The next frame is painted, whatever else says. From the thread that drives the window.
    void requestFrame()
    {
        s_.forcePaint_ = true;
    }
    /// @brief A frame is needed in @p seconds (earlier if something else asks): what a widget that changes with time and not with
    ///        input does at each paint it is shown in (a caret: until the next time it flips).
    void requestFrameIn(float seconds)
    {
        const double at = elapsedSeconds_ + static_cast<double>(seconds > 0.0f ? seconds : 0.0f);
        if (at < s_.wakeAt_) s_.wakeAt_ = at;
    }
    /// @brief requestFrame() from any thread (a worker that finished, a network callback, a timer thread): the next update() on the
    ///        thread that drives the window sees it, and the loop is woken if it is asleep: it calls IO::wakeLoop (the zen adapter
    ///        sets it to platform_wake(), which ends the platform's wait for events at once), so a loop that sleeps with nothing to do
    ///        and no time limit still paints within a frame of this call. This is the ONLY function of the library that may be called
    ///        from another thread (requestFrame(), requestFrameIn() and every widget call are for the thread that drives the window:
    ///        write the data first, then call this; the loop reads it on its next frame). Not before the first update() (the hook is
    ///        read there; the first frame is painted anyway), not after the application is destroyed.
    void requestFrameFromAnyThread()
    {
        __atomic_store_n(&wakeFromThread_, 1, __ATOMIC_RELEASE);
        void (*const wake)() = __atomic_load_n(&wakeLoop_, __ATOMIC_ACQUIRE);
        if (wake) wake();
    }
    /// @brief The application's clock, in seconds: the time update() has been given (the sum of the delta times), which keeps
    ///        running while frames are skipped. What a blink is counted by.
    double clockSeconds() const
    {
        return elapsedSeconds_;
    }
    /// @brief A caret's blink timer (seconds since it was last made visible, 0 right after an edit or the first paint) brought to
    ///        now: @p last is the clock at the last call, which paints that may be far apart (skipped frames) leave behind.
    float blinkTime(float timer, double& last) const
    {
        const double was = last;
        last = elapsedSeconds_;
        // just made visible (the edit that did it set the timer to 0): counted from now. The timer is left a hair above 0, so that the
        // next paint does not take it for another reset
        if (timer == 0.0f || was < 0.0) return 1.0e-6f;
        float t = timer + static_cast<float>(elapsedSeconds_ - was);
        if (t > 1.0f) t -= static_cast<float>(static_cast<int>(t));
        return t;
    }

    /// @brief Whether update() looks for a theme that was changed by hand (default: yes). An application that writes a field of
    ///        the theme every frame (an accent that pulses) and does not want a layout for each, turns it off and calls
    ///        notifyThemeChanged() when it wants the widgets told; setTheme() and setThemePreset() always do.
    void watchTheme(bool on)
    {
        watchTheme_ = on;
    }
    /// @brief Emitted once for each change of the theme, after the change and before the layout it asks for (see setTheme).
    Signal<> themeChanged;

    /// @brief Key presses that no widget consumed, or that came while nothing had
    ///        the focus: where an application puts its shortcuts (Ctrl+S, Delete...).
    Signal<KeyEvent&> unhandledKey;

private:
    WidgetApp() = default;
    void addFloatImpl(FloatWindow* fw);
    ~WidgetApp();

    WidgetApp(const WidgetApp&) = delete;
    WidgetApp& operator=(const WidgetApp&) = delete;

    // The IO of one synthetic event (see feedMouseMove): the state update() last saw, no time, no events yet.
    void makeSyntheticIO(ig::retained::IO& io, bool shift, bool ctrl, bool alt) const;
    void syncPointerToGlobalIO();

    // Input dispatch helpers
    void dispatchMouseMove(float x, float y);
    void dispatchMousePress(float x, float y, int btn);
    void dispatchMouseRelease(float x, float y, int btn);
    void dispatchMouseScroll(float x, float y, float sx, float sy);
    void dispatchKeyEvent(const ig::retained::IO::KeyEvent& ke);
    void dispatchTextInput(const ct::Vector<uint32_t>& chars);

    // Build a MouseEvent pre-filled with current IO modifier state
    MouseEvent makeMouseEvent(float x, float y, int btn = 0) const;

    // Hit test
    Widget* hitTest(Widget* w, float x, float y);
    Widget* topWidgetAt(float x, float y);
    void fillLocal(Widget* target, MouseEvent& e);

    template<typename Func>
    void bubble(Widget* target, MouseEvent& e, Func handler);

    static ct::Vector<Widget*> buildChain(Widget* w);

    /// @brief True when @p w is still part of what the frame draws. Only
    ///        pointers are compared - @p w may have been deleted, so it is
    ///        never dereferenced.
    bool isWidgetInTrees(const Widget* w) const;
    void updateHover(Widget* newLeaf);
    void applyCursor(CursorType type);

    // Paint helpers
    void paintTreeInto(Widget* tree, ig::retained::DrawList& dl, const ig::retained::Font* font,
            IconAtlas* icons, float w, float h, const Color& bgColor = Color(30, 30, 30, 255));
    void paintDebugOverlay(Widget* w, ig::retained::DrawList& dl, int depth = 0);
    void paintTooltipInto(ig::retained::DrawList& dl, const ig::retained::Font* font);
    // Bubbles a key or text event up from the focused widget, with the same
    // guard as the mouse bubble against a handler that deletes a widget.
    void bubbleKey(KeyEvent& e, void (Widget::*handler)(KeyEvent&));
    void retirePopup(Widget* popup, bool owned);
    void returnFocus(Widget* holder);
    void queuePopupDelete(Widget* popup);
    void flushPopupDeletes();
    void flushWidgetDeletes();
    // The clip and font stacks of the paint context, kept from frame to frame (see swapStacks).
    ct::Vector<Rect> paintClipScratch_;
    ct::Vector<const ig::retained::Font*> paintFontScratch_;
    // Counts the frame phases (update, paint) that are running: widgets removed meanwhile wait.
    int busy_ = 0;
    // True while releaseSurface() deletes the window's widgets (notifyWidgetRemoved does nothing then).
    bool releasing_ = false;
    ct::Vector<Widget*> pendingWidgetDeletes_; // taken out of their trees during a phase
    struct BusyScope
    {
        explicit BusyScope(WidgetApp& a) : app(a) { ++app.busy_; }
        ~BusyScope()
        {
            if (--app.busy_ == 0) app.flushWidgetDeletes();
        }
        WidgetApp& app;
    };

    // notifyThemeChanged(), with or without the layout it asks for (a change of colours only asks for none)
    void themeNotify(bool relayout);
    // The theme as update() last saw it (see notifyThemeChanged).
    Theme themeSeen_;
    bool themeSeenSet_ = false;
    bool watchTheme_ = true;
    int wakeFromThread_ = 0; // set by requestFrameFromAnyThread, taken by update()
    void (*wakeLoop_)() = nullptr; // IO::wakeLoop as update() last saw it (an atomic: requestFrameFromAnyThread reads it from other threads)
    float idle_ = 0.0f;            // IO::idleSeconds of this update()

    // State
    bool inited_ = false;
    float dt_ = 0.0f;
    double elapsedSeconds_ = 0.0; // accumulated here: a short frame never rounds to 0
    uint32_t elapsedMs_ = 0;
    bool debugLayout_ = false;
    int debugLayoutKey_ = 0;
    bool ioKeyCtrl_ = false;
    bool ioKeyShift_ = false;
    bool ioKeyAlt_ = false;
    void (*clipSet_)(const char*) = nullptr;
    String (*clipGet_)() = nullptr;
    void (*primarySet_)(const char*) = nullptr;
    String (*primaryGet_)() = nullptr;
    void (*mouseCapture_)(bool) = nullptr;
    void (*mouseWarp_)(float, float) = nullptr;
    // Subsystems
    IconAtlas* iconAtlas_ = nullptr;
    const ig::retained::Font* font_ = nullptr;   // cached from last paint()
    ig::retained::DrawList* drawList_ = nullptr; // cached from last paint()
    // Widget tree
    uint32_t deletions_ = 0; // bumped whenever a widget is reported gone
    void dropMouseFocus();
    void captureWindowState(FloatWindow* fw);
    void applyWindowState(FloatWindow* fw);
    void keepOnScreen(FloatWindow* fw);
    void captureDockLayouts();
    void restoreDockLayouts();
    // Texture service callbacks (set by backend)
    UploadTextureFn uploadTex_;
    DestroyTextureFn destroyTex_;
    UpdateTextureFn updateTex_;
    ct::Vector<ig::retained::FontAtlas*> dynamicAtlases_; // see addDynamicAtlas
    // Global event dispatcher: per event, the handlers by widget id and the
    // catch-all ones.
    struct EventHandlers
    {
        ct::HashMap<String, ct::Vector<EventCallback>> byId;
        ct::Vector<EventCallback> any;
    };
    ct::HashMap<String, EventHandlers> globalHandlers_;
    float dragThreshold_ = 5.0f; // pixels before drag begins
    void dispatchDropEvents(const ct::Vector<ig::retained::IO::DropEvent>& drops);
    void cancelDrag();

    Surface s_;

    // The events of the frame update() is dispatching, taken out of the IO (see there), the last of the members
    // so that every other one stays where it was. Kept between frames so that the IO and this trade their
    // storage and a frame with events allocates nothing; a nested update() (a handler that runs a loop of
    // frames) finds it busy and uses a buffer of its own.
    struct FrameEvents
    {
        ct::Vector<ig::retained::IO::MouseInput> mouse;
        ct::Vector<ig::retained::IO::KeyEvent> keys;
        ct::Vector<uint32_t> chars;
        ct::Vector<ig::retained::IO::DropEvent> drops;
    };
    FrameEvents frameEvents_;
    FrameEvents noEvents_; // always empty: what a frame with no events walks
    bool frameEventsBusy_ = false;
};

} // namespace retained
} // namespace ig
