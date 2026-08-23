#pragma once

#include "Widget.hpp"
#include "BuGUI_base.hpp"
#include "BuGUI.hpp"
#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include <unordered_map>



namespace BuGUI
{

// ═════════════════════════════════════════════════════════════════════════════
//  Easing functions for transitions
// ═════════════════════════════════════════════════════════════════════════════

enum class EaseType
{
    Linear,
    InQuad,     OutQuad,     InOutQuad,
    InCubic,    OutCubic,    InOutCubic,
    InExpo,     OutExpo,     InOutExpo,
    InBack,     OutBack,     InOutBack,
    InBounce,   OutBounce,   InOutBounce,
    InElastic,  OutElastic,  InOutElastic,
};

float applyEasing(EaseType type, float t);

// ═════════════════════════════════════════════════════════════════════════════
//  Transition types for stage switching animations
// ═════════════════════════════════════════════════════════════════════════════

enum class TransitionType
{
    None,          // instant swap (no animation)
    SlideLeft,     // old exits left,  new enters from right
    SlideRight,    // old exits right, new enters from left
    SlideUp,       // old exits up,    new enters from bottom
    SlideDown,     // old exits down,  new enters from top
    CoverLeft,     // new slides in from right over stationary old
    CoverRight,    // new slides in from left  over stationary old
    CoverUp,       // new slides in from bottom over stationary old
    CoverDown,     // new slides in from top    over stationary old
    RevealLeft,    // old slides out left,  revealing stationary new
    RevealRight,   // old slides out right, revealing stationary new
    RevealUp,      // old slides out up,    revealing stationary new
    RevealDown,    // old slides out down,  revealing stationary new
    ZoomIn,        // new zooms in from center (small → full)
    ZoomOut,       // old zooms out to center (full → small), new behind
};

class FloatWindow;
class StatusBar;


// ═════════════════════════════════════════════════════════════════════════════
//  WidgetApp – pure widget manager 
//
//  The application (backend) owns the window, GL context and main loop.
//  WidgetApp manages the widget tree, layout, input dispatch and painting.
//
//  Typical frame:
//      widgetApp.update(BuGUI::GetIO());                    // input → widgets
//      widgetApp.paint(*BuGUI::GetDrawData(), font, icons); // layout + draw
//      BuGUI::Render();
//      backend.render(*BuGUI::GetDrawData());
// ═════════════════════════════════════════════════════════════════════════════

class WidgetApp
{
public:
    /// @brief Get the singleton instance.
    static WidgetApp& instance();

    /// @brief Initialize the widget system (call after GL context ready).
    bool init();

    /// @brief Release all resources.
    void shutdown();

    // ── Per-frame API ──────────────────────────────────────────────────────
    /// @brief Dispatch IO events to the widget tree.
    void update(const BuGUI::IO& io);

    /// @brief Layout and paint the widget tree into draw data.
    void paint(BuGUI::DrawData& data,
               const BuGUI::Font* font  = nullptr,
               IconAtlas*         icons = nullptr);

    /// @brief Get the cursor type the backend should apply.
    CursorType wantedCursor() const { return wantedCursor_; }

    /// @brief Get the background color.
    const Color& bgColor() const    { return bgColor_; }
    /// @brief Set the background color.
    void setBgColor(const Color& c) { bgColor_ = c; }

    // ── Dimensions (read from IO each frame) ──────────────────────────────
    /// @brief Get the window width in pixels.
    int   width()     const { return width_; }
    /// @brief Get the window height in pixels.
    int   height()    const { return height_; }
    /// @brief Get the frame delta time in seconds.
    float deltaTime() const { return dt_; }

    /// @brief Get elapsed time in milliseconds since init.
    uint32_t elapsedMs() const { return elapsedMs_; }

    // ── Platform-agnostic clipboard (set by backend via IO) ──────────────
    /// @brief Set clipboard text (platform-agnostic).
    void        setClipboardText(const char* text);
    /// @brief Get clipboard text.
    std::string getClipboardText() const;

    /// @brief Get the cached font from the last paint() call.
    const BuGUI::Font* font() const { return font_; }
    /// @brief Measure text width using the cached font.
    float textWidth(const char* text) const;
    /// @brief Get approximate frames per second.
    int   fps()       const { return dt_ > 0.0f ? static_cast<int>(1.0f / dt_) : 0; }
    /// @brief Get the current mouse X position.
    float mouseX()    const { return mouseX_; }
    /// @brief Get the current mouse Y position.
    float mouseY()    const { return mouseY_; }

    /// @brief Toggle debug layout wireframe overlay.
    bool debugLayout() const    { return debugLayout_; }
    /// @brief Enable/disable debug layout wireframe.
    void setDebugLayout(bool d) { debugLayout_ = d; }

    /// @brief Set per-frame overlay paint callback.
    void setOverlayCallback(std::function<void(PaintContext&)> cb) { overlay_ = std::move(cb); }

    // ── Drag & Drop state (read by widgets/app during frame) ──────────────
    /// @brief Check if a widget drag is in progress.
    bool isDragging() const { return dragPayload_ != nullptr; }
    /// @brief Get the current drag payload.
    const DragPayload* dragPayload() const { return dragPayload_; }
    /// @brief Get the drag source widget.
    Widget* dragSource() const { return dragSource_; }

    /// @brief Set the drag threshold in pixels.
    void  setDragThreshold(float px) { dragThreshold_ = px; }
    /// @brief Get the drag threshold.
    float dragThreshold() const      { return dragThreshold_; }

    // ── Texture services (set by backend at init) ─────────────────────────
    // The backend registers these so widgets can create/destroy GPU textures
    // without depending on any specific graphics API.
    using UploadTextureFn  = std::function<BuGUI::TextureHandle(const unsigned char* rgba, int w, int h)>;
    using DestroyTextureFn = std::function<void(BuGUI::TextureHandle)>;

    /// @brief Register a texture upload function.
    void setTextureUpload(UploadTextureFn fn)   { uploadTex_ = std::move(fn); }
    /// @brief Register a texture destroy function.
    void setTextureDestroy(DestroyTextureFn fn)  { destroyTex_ = std::move(fn); }

    /// @brief Upload RGBA pixels to GPU texture.
    BuGUI::TextureHandle uploadTexture(const unsigned char* rgba, int w, int h)
    { return uploadTex_ ? uploadTex_(rgba, w, h) : BuGUI::TextureHandle{0}; }

    /// @brief Free a previously uploaded texture.
    void destroyTexture(BuGUI::TextureHandle tex)
    { if (destroyTex_ && tex) destroyTex_(tex); }

    /// @brief Load an image file to GPU texture.
    BuGUI::TextureHandle loadImageTexture(const char* path, int& outW, int& outH);

    // ── Root widget ───────────────────────────────────────────────────────
    /// @brief Get the root widget.
    Widget* root() const { return root_; }

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
    Widget* addStage(const std::string& name);

    /// @brief Switch to a named stage.
    bool setStage(const std::string& name);
    /// @brief Switch with a specific transition.
    bool setStage(const std::string& name, TransitionType transition);
    /// @brief Switch with transition and easing.
    bool setStage(const std::string& name, TransitionType transition, EaseType ease);

    /// @brief Set default transition type, easing and duration.
    void setTransition(TransitionType type, float durationSec = 0.3f, EaseType ease = EaseType::InOutCubic);
    /// @brief Get the transition type.
    TransitionType transitionType() const { return transType_; }
    /// @brief Get the transition easing.
    EaseType       transitionEase() const { return transEase_; }
    /// @brief Get the transition duration in seconds.
    float transitionDuration() const { return transDuration_; }
    /// @brief Check if a stage transition is active.
    bool  isTransitioning()   const { return transActive_; }

    /// @brief Get a stage's root widget by name.
    Widget* stage(const std::string& name) const;

    /// @brief Get the current stage name (empty if unused).
    const std::string& currentStageName() const { return currentStage_; }

    /// @brief Remove a stage and delete its widget tree.
    bool removeStage(const std::string& name);

    /// @brief Set a 2D camera for a named stage.
    void setStageCamera(const std::string& name, const BuGUI::Camera2D& cam);
    /// @brief Get the camera for a named stage.
    BuGUI::Camera2D stageCamera(const std::string& name) const;

    /// @brief Set a per-stage background color.
    void setStageBgColor(const std::string& name, const Color& c);
    /// @brief Get a stage's background color.
    Color stageBgColor(const std::string& name) const;

    // ── Status bar (managed by WidgetApp, pinned to window bottom) ─────
    // One shared StatusBar across all stages. Painted and hit-tested by
    // WidgetApp after the stage tree. Stages access it via statusBar().
    /// @brief Get the shared status bar widget.
    StatusBar* statusBar() const { return statusBar_; }
    /// @brief Set the status bar text.
    void setStatusText(const std::string& text);
    /// @brief Show or hide the status bar.
    void setShowStatusBar(bool show) { showStatusBar_ = show; needsLayout_ = true; }
    /// @brief Check if the status bar is shown.
    bool showStatusBar() const       { return showStatusBar_; }
    /// @brief Get the status bar height.
    float statusBarHeight() const;

    /// @brief Request a layout pass on the next frame.
    void requestLayout() { needsLayout_ = true; }

    /// @brief Clear internal refs to a widget being destroyed.
    void notifyWidgetRemoved(Widget* w);

    // ── Float windows ───────────────────────────────────────────────────
    // Floating windows always render on top of the stage (below popups).
    template <typename T = FloatWindow, typename... Args>
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
    const std::vector<FloatWindow*>& floats() const { return floats_; }

    // ── Focus ─────────────────────────────────────────────────────────────
    /// @brief Set the focused widget.
    void setFocused(Widget* w);
    /// @brief Get the currently focused widget.
    Widget* focused() const { return focused_; }

    // ── Widget lookup ─────────────────────────────────────────────────────
    /// @brief Find a widget by ID.
    Widget* widget(const std::string& id) { return root_ ? root_->findById(id) : nullptr; }

    /// @brief Find a widget by ID and cast to type T.
    template <typename T>
    T* widget(const std::string& id) { return root_ ? root_->findById<T>(id) : nullptr; }

    /// @brief Find all widgets with a given tag.
    std::vector<Widget*> widgetsByTag(const std::string& tag);

    template <typename T>
    std::vector<T*> widgetsByTag(const std::string& tag)
    {
        return root_ ? root_->findByTag<T>(tag) : std::vector<T*>{};
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
    using EventCallback = std::function<void(Widget*)>;
    void on(const std::string& event, const std::string& widgetId, EventCallback cb);

    // Global catch-all: fires for every widget on that event
    //   app.onAny("click", [](Widget* w){ if (w->hasTag("btn")) ... });
    void onAny(const std::string& event, EventCallback cb);

    // Fire a global event (called internally by WidgetApp after signal emit)
    void fireEvent(const std::string& event, Widget* w);

    // ── Popup overlay ─────────────────────────────────────────────────
    /// @brief Show a popup widget on top of everything.
    void showPopup(Widget* popup, Widget* owner = nullptr, bool owned = true);
    /// @brief Close the current popup.
    void closePopup();
    /// @brief Get the active popup widget.
    Widget* popup() const { return popup_; }
    /// @brief Get the widget that opened the popup.
    Widget* popupOwner() const { return popupOwner_; }

    /// @brief Get the shared icon atlas.
    IconAtlas& iconAtlas();

private:
    WidgetApp() = default;
    void addFloatImpl(FloatWindow* fw);
    ~WidgetApp();

    WidgetApp(const WidgetApp&) = delete;
    WidgetApp& operator=(const WidgetApp&) = delete;

    // Input dispatch helpers
    void dispatchMouseMove(float x, float y);
    void dispatchMousePress(float x, float y, int btn);
    void dispatchMouseRelease(float x, float y, int btn);
    void dispatchMouseScroll(float x, float y, float sx, float sy);
    void dispatchKeyEvent(const BuGUI::IO::KeyEvent& ke);
    void dispatchTextInput(const std::vector<uint32_t>& chars);

    // Build a MouseEvent pre-filled with current IO modifier state
    MouseEvent makeMouseEvent(float x, float y, int btn = 0) const;

    // Hit test
    Widget* hitTest(Widget* w, float x, float y);
    void fillLocal(Widget* target, MouseEvent& e);

    template <typename Func>
    void bubble(Widget* target, MouseEvent& e, Func handler);

    static std::vector<Widget*> buildChain(Widget* w);
    void updateHover(Widget* newLeaf);
    void applyCursor(CursorType type);

    // Paint helpers
    void paintTreeInto(Widget* tree, BuGUI::DrawList& dl,
                       const BuGUI::Font* font, IconAtlas* icons,
                       float w, float h, const Color& bgColor = Color(30,30,30,255));
    void paintDebugOverlay(Widget* w, BuGUI::DrawList& dl, int depth = 0);
    void paintTooltipInto(BuGUI::DrawList& dl, const BuGUI::Font* font);

    // State
    bool  inited_      = false;
    bool  needsLayout_ = true;
    int   width_       = 0;
    int   height_      = 0;
    float dt_          = 0.0f;
    uint32_t elapsedMs_ = 0;
    Color bgColor_     = Color(30, 30, 30, 255);
    bool  debugLayout_ = false;

    // Cursor hint (set by applyCursor, read by backend via wantedCursor())
    CursorType wantedCursor_ = CursorType::Arrow;

    // Previous-frame IO state for press/release delta detection
    float prevMouseX_      = 0.0f;
    float prevMouseY_      = 0.0f;
    bool  prevMouseDown_[5] = {};
    bool  ioKeyCtrl_  = false;
    bool  ioKeyShift_ = false;
    bool  ioKeyAlt_   = false;

    // Double/triple-click detection
    uint32_t lastClickMs_  = 0;
    float    lastClickX_   = 0, lastClickY_ = 0;
    int      lastClickBtn_ = -1;
    int      clickSeq_     = 0;   // 0→1→2→3 then resets

    void        (*clipSet_)(const char*) = nullptr;
    std::string (*clipGet_)()            = nullptr;

    // Subsystems
    IconAtlas* iconAtlas_ = nullptr;
    const BuGUI::Font*  font_      = nullptr;  // cached from last paint()
    BuGUI::DrawList*    drawList_  = nullptr;  // cached from last paint()

    // Widget tree
    Widget* root_    = nullptr;
    Widget* focused_ = nullptr;
    Widget* hovered_ = nullptr;
    Widget* pressed_ = nullptr;
    std::vector<Widget*> hoverChain_;   // root → ... → deepest hovered
    float   mouseX_  = 0;
    float   mouseY_  = 0;

    // Popup overlay
    Widget* popup_              = nullptr;   // popup widget (may or may not be owned)
    Widget* popupOwner_         = nullptr;   // widget that opened it (not owned)
    Widget* pendingPopupDelete_ = nullptr;   // deferred delete (safe after event loop)
    bool    popupOwned_         = true;      // if true, popup is deleted on close

    // Popup animation
    float   popupAnimProgress_  = 1.0f;     // 0→1 open, 1→0 close
    float   popupAnimDuration_  = 0.12f;    // seconds
    bool    popupAnimOpening_   = false;
    bool    popupAnimClosing_   = false;
    EaseType popupAnimEase_     = EaseType::OutCubic;
    // Closing state: kept alive during close animation
    Widget* popupClosing_       = nullptr;
    Widget* popupClosingOwner_  = nullptr;
    bool    popupClosingOwned_  = true;

    // Float windows (painted above stage, below popup)
    std::vector<FloatWindow*> floats_;
    std::vector<FloatWindow*> pendingFloatDeletes_;  // deferred delete (safe after event loop)

    // Tooltip
    float   tooltipTimer_   = 0.0f;
    bool    tooltipVisible_ = false;
    Widget* tooltipWidget_  = nullptr;

    // Stage system
    std::unordered_map<std::string, Widget*>          stages_;       // name → root widget
    std::unordered_map<std::string, BuGUI::Camera2D>  stageCameras_; // per-stage camera
    std::unordered_map<std::string, Color>            stageBgColors_;// per-stage background
    std::string currentStage_;

    // Transition
    TransitionType transType_     = TransitionType::SlideLeft;
    EaseType       transEase_     = EaseType::InOutCubic;
    float          transDuration_ = 0.3f;   // seconds
    bool           transActive_   = false;
    float          transProgress_ = 0.0f;   // 0..1
    TransitionType transCurrent_  = TransitionType::None; // type of active transition
    EaseType       transEaseCur_  = EaseType::InOutCubic; // ease of active transition
    Widget*        transOldRoot_  = nullptr;
    std::string    transOldStage_;

    // Status bar (owned, painted after stage)
    StatusBar* statusBar_    = nullptr;
    bool       showStatusBar_ = true;

    // Optional overlay callback
    std::function<void(PaintContext&)> overlay_;

    // Texture service callbacks (set by backend)
    UploadTextureFn  uploadTex_;
    DestroyTextureFn destroyTex_;

    // Global event dispatcher
    // Key = "event:widgetId" for targeted, "event:*" for catch-all
    std::unordered_map<std::string, std::vector<EventCallback>> globalHandlers_;

    // ── Drag & Drop state ─────────────────────────────────────────────────
    DragPayload* dragPayload_  = nullptr;   // active payload (owned)
    Widget*      dragSource_   = nullptr;   // widget that started the drag
    float        dragStartX_   = 0, dragStartY_ = 0;
    bool         dragPending_  = false;     // mouse down on dragSource, waiting threshold
    float        dragThreshold_= 5.0f;     // pixels before drag begins

    void dispatchDropEvents(const std::vector<BuGUI::IO::DropEvent>& drops);
    void cancelDrag();
};

} // namespace BuGUI
