#pragma once

#include "Signal.hpp"
#include "BuGUI_base.hpp"
#include "IconAtlas.hpp"
#include "BuGUI.hpp"
#include <string>
#include <vector>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <any>


namespace BuGUI
{

class Menu;

// ═════════════════════════════════════════════════════════════════════════════
//  PaintContext
//
//  Self-contained — zero references to GL, RenderBatch, Texture, glm, SDL.
//  All drawing goes into BuGUI::DrawList; the backend owns everything else.
//
//  Backward compatibility:
//    ctx.fill.SetColor(r,g,b,a)      — stores colour, no batch needed
//    ctx.fill.Rectangle(x,y,w,h,fill) — routes to DrawList
//    ctx.line.Circle(cx,cy,r,false)   — routes to DrawList
//    ctx.font.Print(text,x,y)         — routes to DrawList::addText()
//
//  Widgets that call ctx.fill.DrawImageRegion(Texture*,...) will get a
//  compile error — those are migrated one by one to ctx.drawImage(TextureHandle).
// ═════════════════════════════════════════════════════════════════════════════

struct PaintContext;

// ── ColorProxy ───────────────────────────────────────────────────────────────
// Keeps ctx.fill.* / ctx.line.* syntax.  Only pure geometry — no Texture*,
// no GL types, no glm.  Routes through the parent PaintContext.
struct ColorProxy
{
    PaintContext* ctx  = nullptr;
    Color current = {255, 255, 255, 255};

    /// @brief Set current color from RGBA bytes.
    void SetColor(u8 r, u8 g, u8 b, u8 a) { current = {r, g, b, a}; }
    /// @brief Set current color from Color struct.
    void SetColor(const Color& c)          { current = c; }
    /// @brief Set current color from normalized floats (0..1).
    void SetColor(float r, float g, float b) {
        current = {static_cast<u8>(r * 255.f),
                   static_cast<u8>(g * 255.f),
                   static_cast<u8>(b * 255.f), 255};
    }
    /// @brief Set alpha from normalized float (0..1).
    void SetAlpha(float a) { current.a = static_cast<u8>(a * 255.f); }

    /// @brief Draw a rectangle (filled or outline).
    void Rectangle(float x, float y, float w, float h, bool doFill);
    /// @brief Draw a rounded rectangle (filled or outline).
    void RoundedRectangle(float x, float y, float w, float h, float r, int seg, bool doFill);
    /// @brief Draw a circle (filled or outline).
    void Circle(float cx, float cy, float radius, bool doFill);
    /// @brief Draw a triangle (filled or outline).
    void Triangle(float x0, float y0, float x1, float y1, float x2, float y2, bool doFill);
    /// @brief Draw a 2D line segment.
    void Line2D(float x1, float y1, float x2, float y2);

    /// @brief No-op (rendering handled by backend).
    void Render() {}
};

// ── FontProxy ────────────────────────────────────────────────────────────────
// Keeps ctx.font.Print() / SetColor() / GetTextWidth() / SetFontSize() syntax.
struct FontProxy
{
    PaintContext* ctx = nullptr;
    float scale = 1.0f;           // zoom scale factor

    void   SetFontSize(float)             {}   // no-op: font baked at init
    void   SetBatch(void*)                {}   // no-op
    void   SetClip(int, int, int, int)    {}   // no-op: DrawList clip stack handles it
    float  GetFontSize()    const         { return 16.0f * scale; }
    /// @brief Get font ascender height.
    float  GetAscender()    const;
    /// @brief Set text rendering color.
    void   SetColor(const Color& c);
    /// @brief Set text rendering color from RGBA bytes.
    void   SetColor(u8 r, u8 g, u8 b, u8 a);
    /// @brief Measure text width in pixels.
    float  GetTextWidth(const char* text) const;
    /// @brief Draw text at position (x, y).
    void   Print(const char* text, float x, float y,
                 const void* clipHint = nullptr);
};

struct PaintContext
{
    BuGUI::DrawList&    drawList;
    const BuGUI::Font*  buFont  = nullptr;

    // ── Compatibility proxies (existing widget .cpp files unchanged) ──────
    ColorProxy fill;   // ctx.fill.SetColor / ctx.fill.Rectangle / ...
    ColorProxy line;   // ctx.line.SetColor / ctx.line.Circle / ...
    ColorProxy text;   // ctx.text.Render() → no-op; TexturedQuad → migrate
    FontProxy  font;   // ctx.font.Print / SetColor / GetTextWidth

    Color textColor_ = {220, 220, 220, 255};

    // Optional procedural icon atlas (TextureHandle resolved internally)
    IconAtlas* icons = nullptr;

    PaintContext(BuGUI::DrawList& dl,
                 const BuGUI::Font* f = nullptr,
                 IconAtlas* ico = nullptr);

    /// @brief Push a font onto the font stack.
    void pushFont(const BuGUI::Font* f);
    /// @brief Pop the topmost font from the stack.
    void popFont();
    /// @brief Get the currently active font.
    const BuGUI::Font* currentFont() const { return buFont; }

    /// @brief Push a clip rectangle onto the clip stack.
    void pushClip(const Rect& r);
    /// @brief Pop the topmost clip rectangle.
    void popClip();
    /// @brief Get the current clip rectangle.
    const Rect& clipRect()                               const;
    /// @brief Check if a clip rectangle is active.
    bool        hasClip()                                const;
    /// @brief Check if a rect is fully clipped (invisible).
    bool        isClipped(const Rect& r)                 const;
    /// @brief Intersect rect with current clip; store result in out.
    bool        clipRectIntersect(const Rect& in, Rect& out) const;
    /// @brief Get clip rect as float[4] for font rendering.
    void        fontClip(float out[4])                   const;

    /// @brief Fill a rectangle with the current fill color.
    void fillRect(float x, float y, float w, float h);
    /// @brief Fill a rounded rectangle.
    void fillRoundedRect(float x, float y, float w, float h, float r, int seg = 8);
    /// @brief Fill a circle.
    void fillCircle(float cx, float cy, float radius);
    /// @brief Fill a triangle.
    void fillTriangle(float x1, float y1, float x2, float y2, float x3, float y3);

    /// @brief Outline a rectangle.
    void lineRect(float x, float y, float w, float h);
    /// @brief Outline a rounded rectangle.
    void lineRoundedRect(float x, float y, float w, float h, float r, int seg = 8);
    /// @brief Outline a circle.
    void lineCircle(float cx, float cy, float radius);
    /// @brief Draw a line segment.
    void drawLine(float x1, float y1, float x2, float y2);

    /// @brief Draw text at position (x, y) using the current font.
    void  drawText(float x, float y, const char* text);
    /// @brief Draw text aligned within a bounding rectangle.
    void  drawTextAligned(const Rect& bounds, const char* text,
                          BuGUI::AlignX ax = BuGUI::AlignX::Left,
                          BuGUI::AlignY ay = BuGUI::AlignY::Top);
    /// @brief Measure text width using the current font.
    float textWidth(const char* text)  const;
    /// @brief Get text line height.
    float textHeight()                 const;

    /// @brief Draw a textured image in dst rect with UV and tint.
    void drawImage(BuGUI::TextureHandle tex, const Rect& dst,
                   BuGUI::Rect uv   = {0.0f, 0.0f, 1.0f, 1.0f},
                   Color       tint = Color(255, 255, 255, 255));

    /// @brief Draw an icon from the atlas at (x, y) with given size.
    void drawIcon(IconId id, float x, float y, float size);
    /// @brief Draw an icon with a custom tint color.
    void drawIcon(IconId id, float x, float y, float size, const Color& tint);

private:
    std::vector<Rect> clipStack_;
    std::vector<const BuGUI::Font*> fontStack_;
    mutable Rect      cachedClip_ = {0, 0, 99999, 99999};
};

// ═════════════════════════════════════════════════════════════════════════════
//  MouseEvent / KeyEvent
// ═════════════════════════════════════════════════════════════════════════════

struct MouseEvent
{
    float x = 0, y = 0;       // absolute screen coords
    float localX = 0, localY = 0; // relative to widget
    int   button = 0;          // 0=left, 1=right, 2=middle
    int   clickCount = 1;      // 1=single, 2=double, 3=triple
    float scrollX = 0, scrollY = 0;
    bool  ctrl  = false;
    bool  shift = false;
    bool  alt   = false;
    bool  consumed = false;
};

struct KeyEvent
{
    int  key      = 0;
    int  scancode = 0;
    bool shift    = false;
    bool ctrl     = false;
    bool alt      = false;
    bool consumed = false;
    char text[32] = {};
};

// ═════════════════════════════════════════════════════════════════════════════
//  Layout enums & structs
// ═════════════════════════════════════════════════════════════════════════════

enum class Align    { Start, Center, End, Fill };
enum class MainAlign { Start, Center, End, SpaceBetween, SpaceEvenly };

struct Edges
{
    float top = 0, right = 0, bottom = 0, left = 0;

    Edges() = default;
    Edges(float all) : top(all), right(all), bottom(all), left(all) {}
    Edges(float tb, float lr) : top(tb), right(lr), bottom(tb), left(lr) {}
    Edges(float t, float r, float b, float l) : top(t), right(r), bottom(b), left(l) {}

    float horizontal() const { return left + right; }
    float vertical()   const { return top + bottom; }
};

// ═════════════════════════════════════════════════════════════════════════════
//  Text alignment
// ═════════════════════════════════════════════════════════════════════════════

enum class TextAlign { LEFT, CENTER, RIGHT, JUSTIFY };

// ═════════════════════════════════════════════════════════════════════════════
//  Cursor types (maps to SDL system cursors)
// ═════════════════════════════════════════════════════════════════════════════

enum class CursorType
{
    Arrow,          // default pointer
    Hand,           // clickable (buttons, links)
    IBeam,          // text input
    Crosshair,      // precision select
    SizeH,          // horizontal resize  ↔
    SizeV,          // vertical resize    ↕
    SizeAll,        // move / drag all directions
    No,             // not allowed
};

// ═════════════════════════════════════════════════════════════════════════════
//  DragPayload — opaque data carried during a widget-to-widget drag
// ═════════════════════════════════════════════════════════════════════════════

class Widget;   // forward decl for DragPayload::source

struct DragPayload
{
    std::string   type;        // e.g. "color", "file", "widget"
    std::any      data;        // arbitrary typed data
    Widget*       source = nullptr;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Widget - base class for all UI elements (retained mode)
// ═════════════════════════════════════════════════════════════════════════════

class Widget
{
    friend class WidgetApp;   // manages hover state
    friend class UIApp;
public:
    Widget() = default;
    virtual ~Widget();

    // non-copyable, non-movable (tree node)
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;

    // ── Tree ──────────────────────────────────────────────────────────────
    /// @brief Add an existing widget as child (takes ownership).
    template <typename T>
    T* addChild(T* child)
    {
        if (!child) return nullptr;
        if (child->parent_ == this) return child;
        if (child->parent_ != nullptr)
        {
            return child;
        }
        child->parent_ = this;
        children_.push_back(child);
        markDirty();
        return child;
    }

    /// @brief Construct a child widget in-place.
    template <typename T, typename... Args>
    T* createChild(Args&&... args)
    {
        return addChild(new T(std::forward<Args>(args)...));
    }

    /// @brief Remove and delete a child widget.
    void removeChild(Widget* child);
    /// @brief Get the parent widget (nullptr if root).
    Widget* parent() const { return parent_; }
    /// @brief Get the list of child widgets.
    const std::vector<Widget*>& children() const { return children_; }

    // ── Geometry ──────────────────────────────────────────────────────────
    /// @brief Set position (top-left corner) in parent space.
    void setPosition(float x, float y) { rect_.x = x; rect_.y = y; markDirty(); }
    /// @brief Set the width and height.
    void setSize(float w, float h)     { rect_.w = w; rect_.h = h; markDirty(); }
    /// @brief Set the full rectangle (position + size).
    void setRect(const Rect& r)        { rect_ = r; markDirty(); }
    /// @brief Get the widget's rectangle in parent space.
    const Rect& rect() const           { return rect_; }

    /// @brief Compute absolute rect in screen coordinates.
    Rect absoluteRect() const;

    struct Vec2f { float x = 0, y = 0; };

    // ── Minimum / preferred size ──────────────────────────────────────────
    virtual Vec2f sizeHint() const { return {rect_.w, rect_.h}; }

    // ── Visibility / Enabled ──────────────────────────────────────────────
    /// @brief Set widget visibility.
    void setVisible(bool v) { visible_ = v; }
    /// @brief Check if the widget is visible.
    bool isVisible() const  { return visible_; }
    /// @brief Enable or disable the widget.
    void setEnabled(bool e) { enabled_ = e; }
    /// @brief Check if the widget is enabled.
    bool isEnabled() const  { return enabled_; }

    // ── Focus ─────────────────────────────────────────────────────────────
    /// @brief Check if the widget has keyboard focus.
    bool isFocused() const  { return focused_; }
    /// @brief Set keyboard focus on this widget.
    void setFocused(bool f) { focused_ = f; }
    /// @brief Check if this widget can receive keyboard focus.
    bool acceptsFocus() const { return acceptsFocus_; }

    // ── Dirty (needs repaint) ─────────────────────────────────────────────
    /// @brief Check if the widget needs repainting.
    bool isDirty() const { return dirty_; }
    /// @brief Mark the widget as needing repaint.
    void markDirty()     { dirty_ = true; }
    /// @brief Clear the dirty flag after painting.
    void clearDirty()    { dirty_ = false; }

    // ── Layout ────────────────────────────────────────────────────────────
    virtual void layout();

    // ── Scroll offset (overridden by ScrollView) ──────────────────────────
    // Returns how much this widget's children are scrolled (dx, dy).
    virtual Vec2f scrollOffset() const { return {0, 0}; }

    // ── Popup hit test (overridden by Menu for submenu support) ──────────
    // Returns true if (x,y) is inside this popup or any sub-popup it owns.
    virtual bool popupContains(float x, float y) const
    { return absoluteRect().contains(x, y); }

    // Called when popup is closed - override to reset internal state.
    virtual void resetPopupState() {}

    /// @brief Set whether parent scroll offset affects this widget.
    void setScrollable(bool s) { scrollable_ = s; }
    /// @brief Check if parent scroll offset affects this widget.
    bool isScrollable() const  { return scrollable_; }

    // ── Paint ─────────────────────────────────────────────────────────────
    // Single pass: widget draws into ctx.fill, ctx.line, ctx.text as needed.
    // WidgetApp renders the 3 batches in order: fill → line → text.
    virtual void paint(PaintContext& ctx);

    // ── Events (override in subclasses) ───────────────────────────────────
    virtual void onMousePress(MouseEvent& e);
    virtual void onMouseRelease(MouseEvent& e);
    virtual void onMouseMove(MouseEvent& e);
    virtual void onMouseScroll(MouseEvent& e)  { (void)e; }
    virtual void onMouseEnter();
    virtual void onMouseLeave();
    virtual void onKeyPress(KeyEvent& e)       { (void)e; }
    virtual void onKeyRelease(KeyEvent& e)     { (void)e; }
    virtual void onTextInput(KeyEvent& e)      { (void)e; }
    virtual void onFocusGained()               {}
    virtual void onFocusLost()                 {}

    // ── Drag & Drop (override in subclasses) ──────────────────────────────
    // Widget-to-widget DnD: source returns a payload; target accepts or rejects.
    // OS file drops are delivered to the widget under the cursor.
    virtual bool        isDragSource() const             { return false; }
    virtual DragPayload onDragBegin()                    { return {}; }
    virtual void        onDragEnd(bool accepted)         { (void)accepted; }
    virtual bool        acceptsDrop(const DragPayload& p){ (void)p; return false; }
    virtual void        onDropReceive(const DragPayload& p) { (void)p; }
    virtual void        onFileDrop(const std::vector<std::string>& paths) { (void)paths; }

    // ── Signals (connect from user code) ──────────────────────────────────
    Signal<int>        pressed;       // mouse button down (button id)
    Signal<int>        released;      // mouse button up   (button id)
    Signal<>           clicked;       // press + release inside widget (left)
    Signal<>           hoverEnter;    // mouse entered widget area
    Signal<>           hoverLeave;    // mouse left widget area
    Signal<float,float> moved;        // mouse moved (localX, localY)
    Signal<const std::vector<std::string>&> fileDrop; // OS file drop

    // ── Hover state (set by WidgetApp) ────────────────────────────────────
    /// @brief Check if the mouse is currently over this widget.
    bool isHovered() const { return hovered_; }

    // ── ID (optional, for lookups) ────────────────────────────────────────
    /// @brief Set a unique string identifier for this widget.
    void setId(const std::string& id) { id_ = id; }
    /// @brief Get the widget's unique identifier.
    const std::string& id() const     { return id_; }

    /// @brief Find a descendant widget by ID (recursive).
    Widget* findById(const std::string& id);

    /// @brief Find by ID and cast to type T.
    template <typename T>
    T* findById(const std::string& id)
    {
        return dynamic_cast<T*>(findById(id));
    }

    // ── Tags (for grouping/filtering) ─────────────────────────────────────
    /// @brief Add a tag string for grouping/filtering.
    void addTag(const std::string& tag)    { tags_.insert(tag); }
    /// @brief Remove a tag.
    void removeTag(const std::string& tag) { tags_.erase(tag); }
    /// @brief Check if this widget has a given tag.
    bool hasTag(const std::string& tag) const { return tags_.count(tag) > 0; }
    /// @brief Get all tags.
    const std::unordered_set<std::string>& tags() const { return tags_; }

    /// @brief Find all descendants with a given tag.
    void findByTag(const std::string& tag, std::vector<Widget*>& result);

    /// @brief Find descendants by tag, cast to type T.
    template <typename T>
    std::vector<T*> findByTag(const std::string& tag)
    {
        std::vector<Widget*> all;
        findByTag(tag, all);
        std::vector<T*> typed;
        for (auto* w : all)
            if (auto* t = dynamic_cast<T*>(w))
                typed.push_back(t);
        return typed;
    }

    // ── User data (arbitrary key-value storage) ───────────────────────────
    /// @brief Store arbitrary user data by key.
    void setData(const std::string& key, std::any value) { data_[key] = std::move(value); }
    /// @brief Check if user data exists for a key.
    bool hasData(const std::string& key) const { return data_.count(key) > 0; }
    /// @brief Remove user data by key.
    void removeData(const std::string& key)    { data_.erase(key); }

    // ── Tooltip ───────────────────────────────────────────────────────────
    /// @brief Set the tooltip text shown on hover.
    void setTooltip(const std::string& tip) { tooltip_ = tip; }
    /// @brief Get the tooltip text.
    const std::string& tooltip() const      { return tooltip_; }
    /// @brief Set delay before tooltip appears (seconds).
    void setTooltipDelay(float seconds)     { tooltipDelay_ = seconds; }
    /// @brief Get tooltip delay in seconds.
    float tooltipDelay() const              { return tooltipDelay_; }

    // ── Custom font (per-widget override) ─────────────────────────────────
    /// @brief Override the font for this widget.
    void setFont(const BuGUI::Font* f) { overrideFont_ = f; markDirty(); }
    /// @brief Get the per-widget font override (nullptr = inherit).
    const BuGUI::Font* overrideFont() const { return overrideFont_; }

    // ── Cursor ────────────────────────────────────────────────────────────
    /// @brief Set the mouse cursor type for this widget.
    void setCursor(CursorType c)  { cursor_ = c; }
    /// @brief Get the mouse cursor type.
    CursorType cursor() const     { return cursor_; }

    // ── Context menu (right-click) ────────────────────────────────────────
    /// @brief Set the right-click context menu (owned — deleted in ~Widget).
    void setContextMenu(Menu* m);
    /// @brief Get the context menu.
    Menu* contextMenu() const     { return contextMenu_; }

    /// @brief Get user data by key, with type cast.
    template <typename T>
    T data(const std::string& key) const
    {
        auto it = data_.find(key);
        if (it != data_.end())
            return std::any_cast<T>(it->second);
        return T{};
    }

    /// @brief Get user data with fallback default.
    template <typename T>
    T data(const std::string& key, const T& fallback) const
    {
        auto it = data_.find(key);
        if (it != data_.end()) {
            try { return std::any_cast<T>(it->second); }
            catch (...) {}
        }
        return fallback;
    }

    // ── Layout properties (used by parent BoxLayout) ──────────────────────
    /// @brief Set margins for layout positioning.
    void setMargins(const Edges& m) { margins_ = m; markDirty(); }
    /// @brief Set uniform margins.
    void setMargins(float all)      { margins_ = Edges(all); markDirty(); }
    /// @brief Set vertical and horizontal margins.
    void setMargins(float tb, float lr) { margins_ = Edges(tb, lr); markDirty(); }
    /// @brief Set individual margins.
    void setMargins(float t, float r, float b, float l) { margins_ = Edges(t,r,b,l); markDirty(); }
    /// @brief Get the current margins.
    const Edges& margins() const { return margins_; }

    /// @brief Set cross-axis alignment within parent layout.
    void setLayoutAlign(Align a)    { layoutAlign_ = a; markDirty(); }
    /// @brief Get cross-axis alignment.
    Align layoutAlign() const       { return layoutAlign_; }

    /// @brief Set stretch factor for parent layout expansion.
    void setStretch(float s)  { stretch_ = s; markDirty(); }
    /// @brief Get the stretch factor.
    float stretch() const     { return stretch_; }

protected:
    Rect rect_;
    Widget* parent_ = nullptr;
    std::vector<Widget*> children_;

    bool visible_      = true;
    bool enabled_      = true;
    bool focused_      = false;
    bool hovered_      = false;
    bool pressed_      = false;
    bool acceptsFocus_ = false;
    bool dirty_        = true;
    bool scrollable_   = true;   // affected by parent's scroll offset

    // Layout
    Edges margins_;
    Align layoutAlign_ = Align::Fill;
    float stretch_     = 0.0f;

    std::string id_;
    std::unordered_set<std::string> tags_;
    std::unordered_map<std::string, std::any> data_;
    std::string tooltip_;
    float tooltipDelay_ = 0.6f;  // seconds before tooltip appears
    CursorType cursor_ = CursorType::Arrow;
    Menu* contextMenu_ = nullptr;  // owned - deleted in ~Widget
    const BuGUI::Font* overrideFont_ = nullptr;  // optional per-widget font
};

// ═════════════════════════════════════════════════════════════════════════════
//  Layouts
// ═════════════════════════════════════════════════════════════════════════════

enum class LayoutDir { Vertical, Horizontal };

class BoxLayout : public Widget
{
public:
    explicit BoxLayout(LayoutDir dir) : dir_(dir) {}

    void setSpacing(float s) { spacing_ = s; }
    float spacing() const    { return spacing_; }
    LayoutDir dir() const    { return dir_; }

    void setPadding(const Edges& e)                          { padding_ = e; }
    void setPadding(float all)                              { padding_ = Edges(all); }
    void setPadding(float tb, float lr)                     { padding_ = Edges(tb, lr); }
    void setPadding(float t, float r, float b, float l)     { padding_ = Edges(t, r, b, l); }
    const Edges& padding() const { return padding_; }

    void setMainAlign(MainAlign a) { mainAlign_ = a; }
    MainAlign mainAlign() const    { return mainAlign_; }

    void layout() override;
    Vec2f sizeHint() const override;

private:
    LayoutDir dir_;
    float spacing_      = 4.0f;
    Edges padding_      = Edges(4.0f);
    MainAlign mainAlign_ = MainAlign::Start;
};

using VBoxLayout = BoxLayout;   // construct with LayoutDir::Vertical
using HBoxLayout = BoxLayout;   // construct with LayoutDir::Horizontal

} // namespace BuGUI
