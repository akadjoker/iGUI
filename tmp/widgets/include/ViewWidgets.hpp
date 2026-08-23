#pragma once

#include "Widget.hpp"
#include "Theme.hpp"
#include "Signal.hpp"
#include <string>
#include <functional>

// ═════════════════════════════════════════════════════════════════════════════
//  FloatWindow - draggable, resizable floating panel with title bar
//
//  Usage:
//    auto* fw = app.addFloat<FloatWindow>("Inspector");
//    auto* vbox = fw->setContent<BoxLayout>(LayoutDir::Vertical);
//    fw->setFloatPos(100, 100);
//    fw->setFloatSize(300, 200);
//    fw->closed.connect([&app, fw]() { app.removeFloat(fw); });
//
//  The FloatWindow lives in WidgetApp's float layer (above stages, below popups).
// ═════════════════════════════════════════════════════════════════════════════

namespace BuGUI
{
class FloatWindow : public Widget
{
public:
    explicit FloatWindow(const std::string& title = "Window");
    ~FloatWindow() override;

    // ── Title bar ────────────────────────────────────────────────────────
    /// @brief Set the window title.
    void setTitle(const std::string& t) { title_ = t; markDirty(); }
    /// @brief Get the window title.
    const std::string& title() const    { return title_; }

    // ── Floating position (absolute screen coords) ───────────────────────
    /// @brief Set absolute screen position.
    void  setFloatPos(float x, float y) { floatX_ = x; floatY_ = y; markDirty(); }
    /// @brief Get the X position.
    float floatX() const { return floatX_; }
    /// @brief Get the Y position.
    float floatY() const { return floatY_; }

    // ── Floating size ────────────────────────────────────────────────────
    /// @brief Set the floating window size.
    void  setFloatSize(float w, float h) { floatW_ = w; floatH_ = h; markDirty(); }
    /// @brief Get the width.
    float floatW() const { return floatW_; }
    /// @brief Get the height.
    float floatH() const { return floatH_; }

    // ── Content widget ───────────────────────────────────────────────────
    /// @brief Create and set a content widget of type T.
    template <typename T, typename... Args>
    T* setContent(Args&&... args)
    {
        auto* w = new T(std::forward<Args>(args)...);
        setContentWidget(w);
        return w;
    }
    /// @brief Set the content widget (takes ownership).
    void    setContentWidget(Widget* w);
    /// @brief Get the content widget.
    Widget* content() const { return content_; }

    // ── Features ─────────────────────────────────────────────────────────
    /// @brief Enable the close button.
    void setClosable(bool c)    { closable_    = c; }
    /// @brief Enable the minimize button.
    void setMinimizable(bool m) { minimizable_ = m; }
    /// @brief Enable window resizing.
    void setResizable(bool r)   { resizable_   = r; }
    /// @brief Check if the window is minimized.
    bool isMinimized() const    { return minimized_; }
    /// @brief Set the minimized state.
    void setMinimized(bool m);

    /// @brief Set the title bar height.
    void  setTitleBarHeight(float h) { titleBarH_ = h; markDirty(); }
    /// @brief Get the title bar height.
    float titleBarHeight() const     { return titleBarH_; }

    // ── Signals ──────────────────────────────────────────────────────────
    Signal<>    closed;           ///< close button clicked
    Signal<bool> minimizedChanged; ///< minimized state changed

    // ── Widget overrides ─────────────────────────────────────────────────
    void  layout() override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;
    Vec2f sizeHint() const override;

private:
    std::string title_;
    Widget*     content_    = nullptr;
    float floatX_     = 50,  floatY_    = 50;
    float floatW_     = 300, floatH_    = 200;
    float titleBarH_  = 28.0f;
    bool  closable_    = true;
    bool  minimizable_ = true;
    bool  resizable_   = true;
    bool  minimized_   = false;

    // Drag state
    bool  dragging_  = false;
    float dragOffX_  = 0, dragOffY_ = 0;

    // Resize state
    bool  resizing_      = false;
    float resizeOffX_    = 0, resizeOffY_    = 0;
    float resizeStartW_  = 0, resizeStartH_  = 0;
    static constexpr float kResizeGrip = 12.0f;

    // Button hover: 0=none, 1=close, 2=minimize
    int hoveredBtn_ = 0;

    // Geometry helpers
    Rect titleBarRect()     const;
    Rect contentRect()      const;
    Rect closeButtonRect()  const;
    Rect minimizeButtonRect() const;
    Rect resizeGripRect()   const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Canvas - panel that clips children + custom paint callback
//
//    auto* cv = parent->createChild<Canvas>();
//    cv->setOnPaint([](PaintContext& ctx, const Rect& b) {
//        ctx.fill.SetColor(200, 50, 50, 255);
//        ctx.fillCircle(b.x + b.w * 0.5f, b.y + b.h * 0.5f, 40.f);
//    });
// ═════════════════════════════════════════════════════════════════════════════

class Canvas : public Widget
{
public:
    Canvas() = default;

    /// @brief Set the background color.
    void setBgColor(const Color& c) { bgColor_ = c; markDirty(); }
    /// @brief Get the background color.
    const Color& bgColor() const    { return bgColor_; }

    /// @brief Set a custom paint callback.
    using PaintCallback = std::function<void(PaintContext& ctx, const Rect& bounds)>;
    void setOnPaint(PaintCallback cb) { onPaint_ = std::move(cb); }

    void layout() override;
    void paint(PaintContext& ctx) override;

private:
    Color bgColor_ = Theme::instance().panelColor;
    PaintCallback onPaint_;
};

// ═════════════════════════════════════════════════════════════════════════════
//  ImageView - draws a TextureHandle with offset (pure display widget)
//
//    auto* iv = parent->createChild<ImageView>();
//    iv->setTexture(tex, w, h);
//    iv->setOffset(10, 20);
// ═════════════════════════════════════════════════════════════════════════════

class ImageView : public Widget
{
public:
    ImageView() = default;

    /// @brief Set the texture, width and height.
    void setTexture(BuGUI::TextureHandle tex, int w, int h);
    /// @brief Get the texture handle.
    BuGUI::TextureHandle texture() const { return texture_; }

    /// @brief Set the image offset.
    void setOffset(float ox, float oy) { offsetX_ = ox; offsetY_ = oy; markDirty(); }
    /// @brief Get the horizontal offset.
    float offsetX() const { return offsetX_; }
    /// @brief Get the vertical offset.
    float offsetY() const { return offsetY_; }

    /// @brief Set a tint color.
    void setTint(const Color& c) { tint_ = c; markDirty(); }

    void paint(PaintContext& ctx) override;

private:
    BuGUI::TextureHandle texture_;
    int   texW_ = 0, texH_ = 0;
    float offsetX_ = 0, offsetY_ = 0;
    Color tint_ = Color(255, 255, 255, 255);
};

// ═════════════════════════════════════════════════════════════════════════════
//  PageView – Container showing one page at a time with transitions
// ═════════════════════════════════════════════════════════════════════════════

class PageView : public Widget
{
public:
    enum Transition { None, Fade, SlideLeft, SlideRight, SlideAuto };

    PageView() = default;

    /// @brief Add a new page of the given widget type.
    template<typename T, typename... Args>
    T* addPage(Args&&... args)
    {
        T* w = new T(std::forward<Args>(args)...);
        addPageImpl(w);
        return w;
    }

    /// @brief Get the number of pages.
    int  pageCount() const { return static_cast<int>(pages_.size()); }
    /// @brief Get the index of the current page.
    int  currentPage() const { return currentIdx_; }
    /// @brief Get a page widget by index.
    Widget* page(int idx) const;

    /// @brief Switch to a page with an optional transition.
    void setPage(int idx, Transition tr = SlideAuto);
    /// @brief Switch to a page widget with an optional transition.
    void setPage(Widget* pg, Transition tr = SlideAuto);

    /// @brief Set the transition animation duration in seconds.
    void setTransitionDuration(float sec) { transDuration_ = sec; }

    /// @brief Emitted when the current page changes.
    Signal<int> pageChanged;

    void layout() override;
    void paint(PaintContext& ctx) override;

private:
    void addPageImpl(Widget* w);

    std::vector<Widget*> pages_;
    int   currentIdx_   = -1;
    int   prevIdx_      = -1;
    float transProgress_ = 1.0f;
    float transDuration_ = 0.25f;
    Transition transType_ = None;
};

} // namespace BuGUI
