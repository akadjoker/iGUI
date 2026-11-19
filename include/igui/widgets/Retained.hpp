#pragma once

#include "RetainedBase.hpp"
#include <cstddef>
#include <cstdint>
#include <ct/hashmap.hpp>
#include <ct/vector.hpp>
#include <igui/Math.hpp>
#include <igui/widgets/String.hpp>

// String here is ct::String, which carries its own .hash() - exactly what ct's
// primary Hash<K> template calls. This used to specialize ct::Hash<String> with
// an identical FNV-1a byte hash; that specialization was not just redundant but
// an ODR hazard, because a translation unit using both widget APIs reaches the
// immediate headers (Syntax.hpp -> ct/hashset.hpp) first and instantiates the
// primary template before this specialization is seen.

namespace ig
{
namespace retained
{
// ── Platform-agnostic key constants ──────────────────────────────────
// Values chosen so the SDL backend can pass its keycodes directly.
// Other backends must map their native codes to these values.
namespace Key
{
constexpr int Return = 13;
constexpr int Escape = 27;
constexpr int Backspace = 8;
constexpr int Tab = 9;
constexpr int Delete = 127;
constexpr int Right = 1073741903;
constexpr int Left = 1073741904;
constexpr int Down = 1073741905;
constexpr int Up = 1073741906;
constexpr int Home = 1073741898;
constexpr int End = 1073741901;
constexpr int PageUp = 1073741899;
constexpr int PageDown = 1073741902;
constexpr int KPEnter = 1073741912;
constexpr int Space = ' ';
constexpr int F1 = 1073741882;
constexpr int F2 = 1073741883;
constexpr int F3 = 1073741884;
constexpr int F4 = 1073741885;
constexpr int F5 = 1073741886;
constexpr int F6 = 1073741887;
constexpr int F7 = 1073741888;
constexpr int F8 = 1073741889;
constexpr int F9 = 1073741890;
constexpr int F10 = 1073741891;
constexpr int F11 = 1073741892;
constexpr int F12 = 1073741893;
// Letters (lowercase ASCII)
constexpr int A = 'a', B = 'b', C = 'c', D = 'd', E = 'e';
constexpr int F = 'f', G = 'g', H = 'h', I = 'i', J = 'j';
constexpr int K = 'k', L = 'l', M = 'm', N = 'n', O = 'o';
constexpr int P = 'p', Q = 'q', R = 'r', S = 's', T = 't';
constexpr int U = 'u', V = 'v', W = 'w', X = 'x', Y = 'y';
constexpr int Z = 'z';
// Punctuation
constexpr int Slash = '/';
} // namespace Key

enum class AlignX
{
    Left,
    Center,
    Right
};
enum class AlignY
{
    Top,
    Middle,
    Bottom
};

using Vec2 = ::ig::Vec2;
using Rect = ::ig::Rect;

struct TextureHandle
{
    uintptr_t value = 0;

    explicit operator bool() const
    {
        return value != 0;
    }
};

struct IO
{
    float deltaTime = 1.0f / 60.0f;
    float displayWidth = 0.0f;
    float displayHeight = 0.0f;
    float framebufferScaleX = 1.0f;
    float framebufferScaleY = 1.0f;

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    float mouseWheelX = 0.0f;
    float mouseWheelY = 0.0f;
    bool mouseDown[5] = {};

    bool keysDown[512] = {};
    bool keyCtrl = false;
    bool keyShift = false;
    bool keyAlt = false;

    ct::Vector<uint32_t> inputChars;

    // Per-frame key press / release events (taken out by WidgetApp::update, cleared by Render)
    struct KeyEvent
    {
        int key = 0;      // platform key code
        int scancode = 0; // hardware scancode
        bool shift = false;
        bool ctrl = false;
        bool alt = false;
        bool down = true; // true = press, false = release
        // How many of inputChars were added before this event: the letters of the frame that were typed
        // before the key are put in before it acts (addKeyEvent fills it in).
        unsigned charsBefore = 0;
    };
    ct::Vector<KeyEvent> keyEvents;

    // Cursor hint written by WidgetApp::update(), read by backend.
    // Matches CursorType: 0=Arrow 1=Hand 2=IBeam 3=Crosshair
    //                     4=SizeH 5=SizeV 6=SizeAll 7=No
    int wantedCursor = 0;

    // ── What the renderer can do (set by backend) ────────────────────────
    // Read BEFORE painting, by a widget that chooses how to draw (see NativeDrawAvailable()). A fresh IO, and a backend that
    // sets nothing (the software rasteriser, a test with no renderer), say "no native drawing".
    // nativeBackend: a NativeBackend value (0: draw callbacks are not run by this renderer).
    // nativeSamples: the multisample count of the target the callbacks draw into (0 or 1: none).
    int nativeBackend = 0;
    int nativeSamples = 0;

    // ── Clipboard (set by backend) ───────────────────────────────────
    // Assign captureless lambdas or plain function pointers.
    void (*setClipboardText)(const char*) = nullptr;
    String (*getClipboardText)() = nullptr;

    // ── File I/O (set by backend) ────────────────────────────────────
    // readFile:  path → file contents (empty string on failure)
    // writeFile: path, data → true on success
    String (*readFile)(const String& path) = nullptr;
    bool (*writeFile)(const String& path, const String& data) = nullptr;

    // ── Mouse mode (set by backend) ──────────────────────────────────
    // setMouseCaptured(true): hide the pointer and keep its motion counting
    // in mouseX/mouseY past the window edges; false shows it again.
    // warpMouse: put the pointer at x, y and report that place from then on.
    void (*setMouseCaptured)(bool captured) = nullptr;
    void (*warpMouse)(float x, float y) = nullptr;

    // ── OS-level file/text drop (set by backend) ─────────────────────
    // Backend pushes drop events each frame; WidgetApp consumes them.
    struct DropEvent
    {
        float x = 0, y = 0;           // mouse position at drop
        ct::Vector<String> filePaths; // files dropped (OS DnD)
        String text;                  // text dropped (if any)
    };
    ct::Vector<DropEvent> dropEvents;

    // Per-frame mouse events, in the order they happened (taken out by WidgetApp::update, cleared by Render), for
    // a backend that reads the platform's event stream: a press and a release that both fall between two frames
    // are a click, a press happens where the pointer was when it was pressed, a move and a press somewhere else
    // are two things. The polled mouseX, mouseY and mouseDown are still what the frame ends with, and are all a
    // backend that adds no events needs to fill in. After the members that were there before, which stay where
    // they were.
    struct MouseInput
    {
        float x = 0.0f;
        float y = 0.0f;
        int button = -1;   // -1: the pointer moved to x, y; else the button that changed
        bool down = false; // with a button: pressed (true) or released
    };
    ct::Vector<MouseInput> mouseEvents;

    void addDropEvent(float x, float y, const ct::Vector<String>& paths, const String& txt = "")
    {
        dropEvents.push_back({ x, y, paths, txt });
    }

    void addInputCharacter(uint32_t c)
    {
        inputChars.push_back(c);
    }
    void addKeyEvent(int key, int scancode, bool shift, bool ctrl, bool alt, bool down)
    {
        keyEvents.push_back({ key, scancode, shift, ctrl, alt, down, static_cast<unsigned>(inputChars.size()) });
    }
    /// The pointer moved to x, y. Moves with no press or release between them are one move, to the last: a
    /// mouse that reports thousands of times a second is not hit tested for each of them.
    void addMouseMoveEvent(float x, float y)
    {
        if (!mouseEvents.empty() && mouseEvents.back().button < 0)
        {
            mouseEvents.back().x = x;
            mouseEvents.back().y = y;
            return;
        }
        mouseEvents.push_back({ x, y, -1, false });
    }
    /// A button was pressed or released with the pointer at x, y.
    void addMouseButtonEvent(float x, float y, int button, bool down)
    {
        mouseEvents.push_back({ x, y, button, down });
    }
};

struct DrawVertex
{
    float x;
    float y;
    float u;
    float v;
    union
    {
        Color color; // (in a union only so that making a vertex with no values does not run the constructor of Color, which sets white)
    };

    // A vertex made with no values has none (position, texture coordinates and colour are whatever the memory held): DrawList makes
    // room for the vertices of a shape in one go and writes every one of them in full straight away, so the writing of initial
    // values into that room, which a vertex with initial values asks for, was a twelfth of the cost of a window full of buttons.
    DrawVertex() {}
    DrawVertex(float px, float py, float pu, float pv, const Color& c) : x(px), y(py), u(pu), v(pv), color(c) {}
};

/// A rounded rectangle a draw command is clipped to (see DrawList::pushClipRoundedRect). The
/// rectangle and radii are in the DrawList's own coordinates, before the pass camera; the radii
/// are the corners' in the order top-left, top-right, bottom-right, bottom-left, already clamped
/// so that neighbouring corners fit on each side.
struct RoundClip
{
    Rect rect;
    float radii[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

/// Which GL a renderer that runs native draw callbacks speaks. WebGL 2 is OpenGLES3 (the calls a callback may make there are
/// the ES 3.0 ones, which a 3.3 core context accepts too).
enum class NativeBackend
{
    None = 0,
    OpenGL33Core = 1,
    OpenGLES3 = 2
};

/// What a native draw callback is given (DrawList::addNativeCallback, docs/DRAW-CALLBACK.md). Rectangles are in framebuffer
/// pixels with the origin at the TOP LEFT of the target and y growing downward (the convention of every other coordinate in
/// the library, multiplied by the pixel scale); the *Gl members are the same boxes as OpenGL wants them: integers, origin at the
/// BOTTOM LEFT.
struct NativeDrawInfo
{
    void* user = nullptr; // what addNativeCallback was given
    int backend = 0;      // a NativeBackend
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    int samples = 0;                       // multisample count of the target (0 or 1: none)
    float pixelScaleX = 1.0f;              // framebuffer pixels per logical unit
    float pixelScaleY = 1.0f;
    Rect rect = { 0, 0, 0, 0 };            // the widget's rectangle, framebuffer pixels, top left origin (not cut to the clip)
    Rect clip = { 0, 0, 0, 0 };            // the clip in force, the same convention; it is already the scissor
    int rectGl[4] = { 0, 0, 0, 0 };        // rect as x, y, w, h for glViewport (rounded outward; may reach past the target)
    int scissorGl[4] = { 0, 0, 0, 0 };     // the box glScissor was given: x, y, w, h, origin bottom left
    float projection[16] = {};             // column major: logical units (the widgets' coordinates) to clip space, the camera in
    float pixelProjection[16] = {};        // column major: framebuffer pixels, top left origin, to clip space
    bool restoreState = true;              // what addNativeCallback was asked for
};

/// A native draw callback: it runs on the thread of the frame, with the context current, at its place in the draw order.
using NativeDrawFn = void (*)(const NativeDrawInfo& info);

struct DrawCmd
{
    /// How many rounded clips one command can carry. Nested rounded clips beyond it lose the
    /// outermost ones (their rectangle still scissors the command).
    enum
    {
        MaxRoundClips = 4
    };

    /// True for a command that is not triangles (a native draw callback; DrawList::special() has it): `roundClipCount` is -1 and
    /// `indexOffset` is the place of its entry in DrawList::specials(), `indexCount` is 0. A renderer that does not know the
    /// kind skips it like any command with no indices; nothing merges with it.
    bool isSpecial() const
    {
        return roundClipCount < 0;
    }

    TextureHandle texture;
    /// Scissor rectangle: the intersection of every clip rectangle pushed, rounded or not.
    Rect clip;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    /// Rounded clips on top of `clip`, outermost first. A pixel is drawn with the product of its
    /// coverage in each; a backend that cannot evaluate them can use ClipToRoundClips.
    int roundClipCount = 0;
    RoundClip roundClips[MaxRoundClips];
};

enum class IconId
{
    None = 0,
    Folder,
    FolderOpen,
    File,
    FileCode,
    Book,
    Gear,
    Star,
    Heart,
    Search,
    Plus,
    Minus,
    Check,
    Cross,
    ArrowRight,
    ArrowDown,
    ArrowUp,
    ArrowLeft,
    Eye,
    EyeOff,
    Lock,
    Unlock,
    Refresh,
    Trash,
    Edit,
    Home,
    User,
    Warning,
    Info,
    Error,
    FileImage,
    FileArchive,
    ViewDetail,
    ViewList,
    ViewGrid,
    // Transport / media controls
    Play,
    Pause,
    Stop,
    StepForward,
    StepBack,
    Record,
    FolderNew,
    Scissors,    // cut / split
    Import,      // arrow into a tray
    Magnet,      // snapping
    MarkIn,      // a bracket opening a range
    MarkOut,     // a bracket closing it
    PaintBrush,  // paint
    Eraser,      // rub out
    PaintBucket, // flood fill
    SelectRect,  // a dashed rectangle
    Eyedropper,  // pick a colour or a tile
    COUNT        // must be last
};

// Icons drawn from a texture region. Each call returns a new IconId past
// COUNT that every widget taking an IconId can show (trees, tool bars,
// buttons); PaintContext::drawIcon draws the region tinted.
IconId registerImageIcon(TextureHandle texture, const Rect& uv);
bool imageIcon(IconId id, TextureHandle& texture, Rect& uv);

struct FontGlyph
{
    uint32_t codepoint = 0;
    Rect uv;
    Vec2 size;
    Vec2 offset;
    float advanceX = 0.0f;
};

class FontAtlas;
#define IGUI_FONTATLAS_RELEASE_PIXELS 1 // FontAtlas::releasePixels() exists

class Font
{
public:
    // A glyph of this font; for a font made by atHeight() it is rasterised
    // into the atlas's dynamic page the first time it is asked for. The
    // pointer is good until the next findGlyph on the same font.
    const FontGlyph* findGlyph(uint32_t codepoint) const;
    // True when the face has a glyph of its own for the code point (findGlyph would draw the
    // question mark otherwise). For a font of a FontAtlas that is the face's character map, so
    // it also answers for code points outside the baked ranges; a font made by hand answers from
    // its glyph table.
    bool hasGlyph(uint32_t codepoint) const;
    // True when findGlyph would not need to rasterise the code point: it is in the glyph table
    // (the baked ranges, or a glyph a font made by atHeight() drew before).
    bool hasBakedGlyph(uint32_t codepoint) const
    {
        return glyphs_.find(codepoint) != nullptr;
    }
    TextureHandle texture() const
    {
        return texture_;
    }
    float lineHeight() const
    {
        return lineHeight_;
    }
    float ascender() const
    {
        return ascender_;
    }
    // The height the face was rasterised at (ascent plus descent, in pixels).
    float pixelHeight() const
    {
        return pixelHeight_;
    }
    // The same face at another height, rasterised exactly at that size on
    // first use. Without a dynamic page (FontAtlas::enableDynamic) this font
    // itself, so a backend that never uploads one keeps drawing as before.
    const Font* atHeight(float pixelHeight) const;
    // Device pixels per unit of this font's sizes: 1 unless the atlas was given a pixel scale
    // (FontAtlas::setPixelScale). The glyphs of a font made by atHeight() are rasterised at
    // pixelHeight() times this and measured back in units, so text is as sharp as the screen.
    float pixelScale() const
    {
        return pixelScale_;
    }
    // What to add to the pen between two glyphs, in pixels at this font's
    // height: negative closes the gap (A and V). The face's kern or GPOS table.
    // Only for a font made by atHeight(): a baked font answers 0, so code that adds its advances by
    // hand (the code editor) keeps agreeing with the drawing.
    float kerning(uint32_t left, uint32_t right) const;
    // The same for any font of an atlas, a baked one too (0 for a font made by hand). What
    // DrawList::addText / calcTextSize apply to a baked font after DrawList::setKerning(All).
    // The cached pair inline (the drawing asks for every pair of every text); the face is read only
    // for a pair not seen yet.
    float pairKerning(uint32_t left, uint32_t right) const
    {
        if (!atlas_) return 0.0f;
        const uint64_t key = (static_cast<uint64_t>(left) << 32) | static_cast<uint64_t>(right);
        if (const float* k = kern_.find(key)) return *k;
        return pairKerningFromFace(key, left, right);
    }
    /// True for a font made by atHeight(): its glyphs come from the dynamic page, and it is kerned.
    bool isDynamic() const
    {
        return dynamic_;
    }

private:
    friend class FontAtlas;

    TextureHandle texture_;
    float lineHeight_ = 0.0f;
    float ascender_ = 0.0f;
    float pixelHeight_ = 0.0f;
    float pixelScale_ = 1.0f;
    FontAtlas* atlas_ = nullptr; // the atlas that holds the face; null for a Font made by hand
    int face_ = -1;              // its index there
    bool dynamic_ = false;       // glyphs come from the dynamic page, on demand
    mutable ct::HashMap<uint32_t, FontGlyph> glyphs_;
    // The glyphs of the first 128 code points again, in a table, so that finding the glyph of a letter is a load and not a search of
    // the map (a text asks twice for every letter in it, once to measure it and once to draw it). Kept with glyphs_ by storeGlyph.
    mutable FontGlyph ascii_[128];
    mutable bool asciiHas_[128] = {};
    void storeGlyph(uint32_t codepoint, const FontGlyph& glyph) const
    {
        glyphs_[codepoint] = glyph;
        if (codepoint < 128)
        {
            ascii_[codepoint] = glyph;
            asciiHas_[codepoint] = true;
        }
    }
    void clearGlyphs() const
    {
        glyphs_.clear();
        for (int i = 0; i < 128; ++i) asciiHas_[i] = false;
    }
    mutable ct::HashMap<uint64_t, float> kern_;
    float pairKerningFromFace(uint64_t key, uint32_t left, uint32_t right) const;
};

class FontAtlas
{
public:
    // ── Multi-font API ────────────────────────────────────────────────
    // Add fonts before calling build(). Returns a pointer valid after build().
    struct FontConfig
    {
        const unsigned char* ttfData = nullptr;
        int ttfSize = 0;
        float pixelHeight = 16.0f;
        // Which face of a font collection (.ttc / .otc) to use; 0 for a plain font file.
        int faceIndex = 0;
        // false: nothing of this face is baked into the atlas (no space used); its glyphs only
        // come from the dynamic page (FontAtlas::enableDynamic), at the size asked for. For a face
        // that is a fallback for the glyphs another one lacks (CJK).
        bool baked = true;
        // If ttfData is nullptr, uses embedded DejaVuSans
    };

    // Add a font to be packed. Returns the font index (0-based).
    int addFont(const FontConfig& cfg);
    int addFontDefault(float pixelHeight = 16.0f);

    // Build the atlas (packs all added fonts into one texture).
    // Must be called after all addFont() calls.
    bool build();

    // ── Legacy single-font API (calls addFont + build internally) ─────
    bool buildDefault();
    bool buildFromTTFMemory(const unsigned char* data, int dataSize, float pixelHeight = 16.0f);

    // ── Sizes on demand ──────────────────────────────────────────────
    // A page of glyphs rasterised at the exact size asked for, so text is
    // sharp at any size and not a 16 pixel bitmap stretched. The adapter
    // calls prepareDynamic(), makes a texture from dynamicPixels() and gives
    // it to enableDynamic(); after drawing it uploads the page again when
    // dynamicDirty().
    bool prepareDynamic();
    bool enableDynamic(TextureHandle texture);
    // Stops rasterising on demand (the adapter calls it before it frees the texture).
    void disableDynamic();
    bool dynamicEnabled() const;
    TextureHandle dynamicTexture() const;
    int dynamicWidth() const;
    int dynamicHeight() const;
    const unsigned char* dynamicPixels() const;
    /// The same page as one byte of coverage a pixel (the page is white wherever it is drawn), what it is kept as: an adapter that can
    /// make a one-byte texture (ig::zen::SoftRaster, GlRaster: createTextureA8) uses this, and the page costs a quarter of the memory
    /// (dynamicPixels() makes an RGBA copy of it only when it is asked for). null while there is no page.
    const unsigned char* dynamicAlpha() const;
    bool dynamicDirty() const;
    void clearDynamicDirty();
    // Device pixels per unit of the sizes asked for (a window on a 2x screen with the UI in
    // CSS-sized units: 2). The dynamic glyphs are rasterised at that scale and measured in units;
    // changing it bakes them again. Default 1.
    void setPixelScale(float scale);
    float pixelScale() const;

    FontAtlas();
    ~FontAtlas();
    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;

    void setTexture(TextureHandle texture);
    TextureHandle texture() const
    {
        return texture_;
    }
    /// True when @p font is one of this atlas's fonts: a baked one or one made by Font::atHeight.
    bool owns(const Font& font) const
    {
        return font.atlas_ == this;
    }
    const Font& defaultFont() const
    {
        return fonts_.empty() ? fallback_ : fonts_[0];
    }
    const Font* font(int idx) const
    {
        return (idx >= 0 && idx < (int) fonts_.size()) ? &fonts_[idx] : nullptr;
    }
    int fontCount() const
    {
        return static_cast<int>(fonts_.size());
    }
    Vec2 whitePixelUV() const
    {
        return whitePixelUV_;
    }

    int width() const
    {
        return width_;
    }
    int height() const
    {
        return height_;
    }
    const unsigned char* pixels() const
    {
        return pixels_.data();
    }
    // The RAM copy of the baked page: once the host uploaded it (the backend keeps its own), it is only kept to be read by
    // pixels() / pixelSize(). Frees it; pixels() is null from then on. The glyph table, the UVs and the dynamic page are unchanged.
    void releasePixels();
    size_t pixelSize() const
    {
        return pixels_.size();
    }

private:
    friend class Font;
    struct Impl;
    Impl* impl_ = nullptr;
    const Font* fontAtHeight(int face, float pixelHeight);
    bool bakeGlyph(const Font& font, uint32_t codepoint);
    bool faceHasGlyph(int face, uint32_t codepoint) const;
    float kerningOf(const Font& font, uint32_t left, uint32_t right) const;

    int width_ = 0;
    int height_ = 0;
    TextureHandle texture_;
    Vec2 whitePixelUV_ = { 0.0f, 0.0f };
    Font fallback_; // empty sentinel
    ct::Vector<Font> fonts_;
    ct::Vector<FontConfig> pending_; // configs waiting for build()
    ct::Vector<unsigned char> pixels_;
};

class DrawList
{
public:
    void clear();

    void pushClip(const Rect& rect);
    /// Clips what follows to the rounded rectangle `rect` (radii: top-left, top-right,
    /// bottom-right, bottom-left; negative values count as 0 and radii that do not fit a side are
    /// scaled down together, as in CSS). The commands carry the shape in DrawCmd::roundClips so
    /// the backend can anti-alias the curve; the scissor is the intersection of all clips pushed.
    /// Nesting keeps every rounded clip (up to DrawCmd::MaxRoundClips, the innermost kept).
    /// Ended with popClip(), like pushClip(). With no radius it is a plain pushClip().
    void pushClipRoundedRect(const Rect& rect, const float radii[4]);
    void popClip();

    /// Maps the vertex positions of everything added from now on by an affine matrix in the CSS
    /// matrix(a, b, c, d, e, f) convention: x' = a*x + c*y + e, y' = b*x + d*y + f. The matrix is
    /// composed with the transform in force (new = current * given: a nested push is in the
    /// coordinates of the outer one). UVs and colours are untouched. Clips pushed while a
    /// transform is in force are mapped too: the rectangle becomes the bounding box of its four
    /// mapped corners (exact for translations and scales, over-inclusive for rotations and
    /// skews) and its rounded part is kept only when the matrix has no rotation or skew (radii
    /// scaled by min(|a|, |d|)). Clips pushed before stay as they are: the clip stack and the
    /// transform stack are independent. Ended with popTransform(); clear() empties the stack.
    void pushTransform(const float matrix[6]);
    void popTransform();
    bool hasTransform() const
    {
        return !transformStack_.empty();
    }
    /// The matrix in force (identity when none).
    const float* currentTransform() const
    {
        return transform_;
    }

    void addRect(const Rect& rect, const Color& color);
    void addRectOutline(const Rect& rect, const Color& color, float thickness = 1.0f);
    void addRoundRectFilled(const Rect& rect, float radius, const Color& color, int segments = 0);
    void addRoundRect(const Rect& rect, float radius, const Color& color, float thickness = 1.0f,
            int segments = 0);
    void addCircleFilled(Vec2 center, float radius, const Color& color, int segments = 32);
    void addCircle(Vec2 center, float radius, const Color& color, float thickness = 1.0f,
            int segments = 32);
    void addTriangleFilled(Vec2 a, Vec2 b, Vec2 c, const Color& color);
    void addTriangle(Vec2 a, Vec2 b, Vec2 c, const Color& color, float thickness = 1.0f);
    void addImage(TextureHandle texture, const Rect& rect, const Rect& uv,
            const Color& color = Color(255, 255, 255, 255));
    void addText(const Font& font, Vec2 pos, const Color& color, const char* text,
            float scale = 1.0f);


    /// A native draw callback at this place of the draw order (docs/DRAW-CALLBACK.md): a renderer that can issue GL calls of its
    /// own (NativeDrawAvailable()) calls `fn(info)` between the commands drawn before and after this call, with the clip in force
    /// already its scissor; `rect` (the widget's rectangle, in the units of this list, under the transform in force) is given to it
    /// in framebuffer pixels. Nothing happens when the clip is empty or `rect` does not touch it. A renderer that cannot run it
    /// skips it (the command is a no-op to it). The callback is called only in frames that are painted, and calling it marks
    /// nothing dirty: a widget whose native content changes asks for a frame (WidgetApp::requestFrame). `restoreState`: iGUI puts
    /// back every piece of GL state it relies on after the call (the exact list is in the document); false: the callback promises to
    /// have left the state as it found it. Not run under a camera that rotates; rounded clips do not apply to it (the scissor does).
    void addNativeCallback(NativeDrawFn fn, void* user, const Rect& rect, bool restoreState = true);

    /// What a special command (DrawCmd::isSpecial) is: a native draw callback.
    struct Special
    {
        NativeDrawFn fn = nullptr;
        void* user = nullptr;
        Rect rect = { 0, 0, 0, 0 };
        bool restoreState = true;
    };
    const ct::Vector<Special>& specials() const
    {
        return specials_;
    }
    /// The entry of a special command.
    const Special& special(const DrawCmd& command) const
    {
        return specials_[command.indexOffset];
    }

    Vec2 calcTextSize(const Font& font, const char* text, float scale = 1.0f);

    void addTextAligned(const Font& font, const Rect& bounds, const Color& color, const char* text,
            AlignX ax = AlignX::Left, AlignY ay = AlignY::Top);

    /// A filled convex polygon (points in order, either winding), anti-aliased like the other
    /// fills.
    void addConvexPolyFilled(const ct::Vector<Vec2>& points, const Color& color);
    /// A filled convex polygon with one colour per point, shaded linearly across its triangles
    /// (a gradient quad). Edges are anti-aliased like addConvexPolyFilled; `count` points.
    void addConvexPolyFilledMultiColor(const Vec2* points, const Color* colors, int count);
    /// A stroked path through the points, closed back to the first when asked.
    void addPolyline(const ct::Vector<Vec2>& points, const Color& color, float thickness,
            bool closed);
    void addIcon(IconId icon, const Rect& rect, const Color& color, float thickness = 2.0f);
    /// True when addIcon has a vector drawing for the icon.
    static bool hasVectorIcon(IconId icon);
    void addLine(Vec2 a, Vec2 b, const Color& color, float thickness = 1.0f);

    // ── Single-texture mode (white pixel in font atlas) ───────────────
    // Call once after uploading the font atlas texture. All geometry will
    // reuse the same texture handle (sampling the white pixel), so that
    // geometry and text commands can be merged into one draw call.
    void setWhitePixel(TextureHandle atlas, Vec2 whiteUV);
    /// A second page of the font atlas that has a white texel too, at @p whiteUV (the dynamic page, which reserves its pixel (0, 0)
    /// for it): the text of a window is on that page, so shapes are drawn with it from now on (setWhitePixel's page stays their
    /// other one), and a shape between texts of that page is one draw command with them instead of three. Text of the first page
    /// (a size the atlas baked), and images, are not affected: shapes next to them go to the first page, with its coordinates.
    /// Call after setWhitePixel (which forgets it). Off (a null handle) by default.
    void setSolidAlternate(TextureHandle texture, Vec2 whiteUV)
    {
        altTexture_ = fontTexture_;
        altUV_ = whiteUV_;
        fontTexture_ = texture;
        whiteUV_ = whiteUV;
    }
    /// Whether commands that can be one are one (on by default). A command is added to the one before it when it has the same
    /// texture (a shape on either page of the atlas counts as having both) and the same rounded clips, and either the same clip, or a
    /// clip that does not matter: the vertices of each lie inside its own clip, or inside the clip it is merged under, so
    /// drawing them under the other clip, or under both clips united, changes no pixel. Switched off, every change of clip, texture
    /// or rounded clip starts a command, as it used to (the pictures of both must be the same: tests/test_retained_drawlist.cpp).
    void setCommandMerging(bool on)
    {
        mergeCommands_ = on;
    }

    // ── Path builder (accumulate → flush) ─────────────────────────────
    // pathArcToFast: aMinOf12 / aMaxOf12 are indices into a 12-step
    // unit-circle LUT (each step = 30°). Range [0..12] covers 0°–360°.
    void pathLineTo(Vec2 p);
    void pathArcToFast(Vec2 center, float radius, int aMinOf12, int aMaxOf12);
    void pathFillConvex(const Color& color);
    void pathStroke(const Color& color, float thickness, bool closed);
    /// Anti-aliased edges on filled paths, strokes and lines (on by default).
    void setAntiAliasing(bool on)
    {
        antiAliasing_ = on;
    }
    bool antiAliasing() const
    {
        return antiAliasing_;
    }
    /// Glyph quads of addText land on whole device pixels (default on: a bitmap drawn between
    /// pixels blurs under linear filtering). Off keeps the fractional pen + offset position.
    /// A configuration: clear() leaves it alone.
    void setTextSnapping(bool on)
    {
        textSnapping_ = on;
    }
    bool textSnapping() const
    {
        return textSnapping_;
    }
    /// Which fonts addText / calcTextSize kern. Dynamic (the default): only the fonts made by
    /// Font::atHeight, as always. All: the baked fonts too (Font::pairKerning), so text of any font
    /// is as wide as its face's kern / GPOS pairs say. None: no font. A configuration:
    /// clear() leaves it alone.
    enum class Kerning
    {
        Dynamic,
        All,
        None
    };
    void setKerning(Kerning mode)
    {
        kerning_ = mode;
    }
    Kerning kerning() const
    {
        return kerning_;
    }
    void pathClear();

    const ct::Vector<DrawVertex>& vertices() const
    {
        return vertices_;
    }
    const ct::Vector<uint32_t>& indices() const
    {
        return indices_;
    }
    const ct::Vector<DrawCmd>& commands() const
    {
        return commands_;
    }
    int pushClipCount() const
    {
        return pushClipCount_;
    }

private:
    struct ClipEntry
    {
        Rect rect;
        int roundCount = 0;
        RoundClip rounds[DrawCmd::MaxRoundClips];
    };

    struct TransformEntry
    {
        float m[6];
    };

    Rect currentClip() const;
    Vec2 mapPoint(float x, float y) const
    {
        if (transformStack_.empty()) return { x, y };
        return { transform_[0] * x + transform_[2] * y + transform_[4], transform_[1] * x + transform_[3] * y + transform_[5] };
    }
    void pushClipScreen(const Rect& rect);
    void fillConvexPath(const Color* colors, const Color& color);
    // A rounded rectangle (corner radius `radius`, `steps` segments a corner), filled or stroked, with anti-aliased edges, from
    // tables of the directions of its outline (the same shape fillConvexPath and pathStroke make of the same points, without
    // working the directions out again for every one of them). False when the shape is one the tables do not cover (no
    // anti-aliasing, a side no longer than the corners, a corner of a size the tables do not have): the caller then builds it as a path.
    bool fillRoundRectFast(float x0, float y0, float x1, float y1, float radius, int steps, const Color& color);
    bool strokeRoundRectFast(float x0, float y0, float x1, float y1, float radius, int steps, const Color& color, float thickness);
    // `solid`: the vertices of the command are shapes (the white texel of the atlas), which any page that has one can draw.
    void addCmd(uint32_t indexOffset, uint32_t indexCount, TextureHandle texture = {}, bool solid = false);
    // The bounds of the vertices from..to (false when one is not a number): what clip merging looks at.
    bool boundsOf(size_t from, size_t to, Rect& out) const;
    // A shape that knows its bounds says so (`count` vertices inside x0, y0, x1, y1), so that addCmd does not have to look at each
    // vertex; one that does not say, or is drawn under a transform, is looked at by addCmd when it has to be.
    void note(size_t count, float x0, float y0, float x1, float y1)
    {
        if (!transformStack_.empty()) return;
        if (noted_ == 0)
        {
            pendBounds_[0] = x0;
            pendBounds_[1] = y0;
            pendBounds_[2] = x1;
            pendBounds_[3] = y1;
        }
        else
        {
            if (x0 < pendBounds_[0]) pendBounds_[0] = x0;
            if (y0 < pendBounds_[1]) pendBounds_[1] = y0;
            if (x1 > pendBounds_[2]) pendBounds_[2] = x1;
            if (y1 > pendBounds_[3]) pendBounds_[3] = y1;
        }
        noted_ += count;
    }
    DrawVertex* appendVertices(size_t count);
    uint32_t* appendIndices(size_t count);
    uint32_t addVertex(float x, float y, const Color& color);
    uint32_t addVertex(float x, float y, float u, float v, const Color& color);

    ct::Vector<ClipEntry> clipStack_;
    ct::Vector<TransformEntry> transformStack_;
    float transform_[6] = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    ct::Vector<Vec2> path_;
    // the unit direction of each edge of path_, worked out once when the outline is built (an edge is the "next" of one point and the
    // "previous" of the following: it used to be worked out, with its square root, for both), and kept to not allocate every frame
    struct EdgeDir
    {
        float x = 0.0f, y = 0.0f, len = 0.0f;
    };
    ct::Vector<EdgeDir> edgeScratch_;
    bool antiAliasing_ = true;
    bool textSnapping_ = true;
    // Whether addText / calcTextSize kern this font, by the mode: All, or Dynamic for a font made
    // by atHeight (Font::kerning's rule). Asked once per text, so a font that is not kerned costs
    // nothing per character.
    bool kerns(const Font& font) const
    {
        return kerning_ == Kerning::All || (kerning_ == Kerning::Dynamic && font.isDynamic());
    }
    TextureHandle fontTexture_;
    TextureHandle altTexture_; // the page shapes had before setSolidAlternate, and its white texel
    Vec2 altUV_ = { 0.0f, 0.0f };
    bool mergeCommands_ = true;
    bool lastSolidOnly_ = false; // the last command holds shapes and nothing else
    size_t newVertices_ = 0;     // the vertices after this one belong to the command being added
    size_t lastStart_ = 0;       // the first vertex of the last command
    Rect lastBounds_ = { 0, 0, 0, 0 };
    bool lastBoundsValid_ = false; // lastBounds_ is the bounds of every vertex of the last command
    Rect mergedClip_ = { 0, 0, 0, 0 };  // a clip that the last command has taken vertices under (inside both it and the command's clip)
    bool mergedClipValid_ = false;
    float pendBounds_[4] = { 0, 0, 0, 0 }; // x0, y0, x1, y1 of the vertices noted since the last command
    size_t noted_ = 0;                     // how many vertices were noted
    Vec2 whiteUV_;
    ct::Vector<DrawVertex> vertices_;
    ct::Vector<uint32_t> indices_;
    ct::Vector<DrawCmd> commands_;
    ct::Vector<Special> specials_;
    int pushClipCount_ = 0;
    ct::HashMap<uint32_t, FontGlyph> glyphs_;
    // last, so the members the drawing calls use all the time keep their place: the outlines addIcon
    // builds, kept from icon to icon (an icon used to allocate its own)
    ct::Vector<Vec2> iconPoints_[2];
    // last too: in the middle it moved the members the drawing calls use all the time (text
    // measuring 7-9% slower)
    Kerning kerning_ = Kerning::Dynamic;
};

/// Triangles that went through ClipToRoundClips.
struct ClippedGeometry
{
    ct::Vector<DrawVertex> vertices;
    ct::Vector<uint32_t> indices;
    void clear()
    {
        vertices.clear();
        indices.clear();
    }
};

/// CPU fallback for backends that cannot clip to a DrawCmd's rounded clips: appends to `out` the
/// triangles `indices[command.indexOffset, +command.indexCount)` of `vertices`, cut to the
/// intersection of command.roundClips (each polygonised to within a tenth of a pixel, then
/// Sutherland-Hodgman; position, UV and colour are interpolated). Coverage is all or nothing, so
/// the curve is not anti-aliased; the scissor `command.clip` is still the backend's to apply.
/// Positions stay in the DrawList's coordinates: transform them afterwards.
void ClipToRoundClips(const DrawVertex* vertices, size_t vertexCount, const uint32_t* indices,
        size_t indexCount, const DrawCmd& command, ClippedGeometry& out);

struct RenderStats
{
    int drawCalls = 0;
    int vertices = 0;
    int triangles = 0;
    int textureSwitches = 0;
    int clipChanges = 0; // pushClip calls per frame
    int nativeCallbacks = 0; // native draw callbacks run (a renderer that cannot run them leaves it 0)

    void reset()
    {
        drawCalls = 0;
        vertices = 0;
        triangles = 0;
        textureSwitches = 0;
        clipChanges = 0;
        nativeCallbacks = 0;
    }
};

// ── Per-stage camera (GPU-side transform) ──────────────────────────────
// x/y   = pan offset in pixels (positive = move content right/down)
// scale = zoom (1 = normal)
// angle = rotation in radians (CCW)
// pivotX/Y = normalised screen coords of rotation+scale centre:
//            0.0 = left/top, 0.5 = centre (default), 1.0 = right/bottom
//
// Transform applied GPU-side:
//   1. Translate by (x, y)
//   2. Translate to pivot in screen pixels (pivotX*W, pivotY*H)
//   3. Scale + rotate around that pivot
//   4. Translate pivot back
struct Camera2D
{
    float x = 0.0f;
    float y = 0.0f;
    float scale = 1.0f;
    float angle = 0.0f;  // radians, CCW
    float pivotX = 0.5f; // normalised (0=left, 1=right)
    float pivotY = 0.5f; // normalised (0=top,  1=bottom)
};

// One render pass: a DrawList with its own GPU camera transform.
// Passes are rendered back-to-front (first = bottom, last = top).
struct DrawPass
{
    DrawList* list = nullptr;
    Camera2D camera;
};

struct DrawData
{
    ct::Vector<DrawPass> passes; // populated by WidgetApp::paint()
    float displayWidth = 0.0f;
    float displayHeight = 0.0f;
    float framebufferScaleX = 1.0f;
    float framebufferScaleY = 1.0f;
    RenderStats stats; // filled by the backend after rendering
};

struct Context;

/// The font built into the library (DejaVu Sans, with accents): its TrueType data, for applications
/// that draw text themselves.
void GetDefaultFont(const unsigned char** data, int* size);

Context* CreateContext();
void DestroyContext(Context* ctx);
void SetCurrentContext(Context* ctx);
Context* GetCurrentContext();

IO& GetIO();
/// Whether the renderer of this window runs native draw callbacks (DrawList::addNativeCallback): the OpenGL renderer does, the
/// software rasteriser and a test with no renderer do not. Meant to be asked BEFORE painting, by a widget that picks a texture or
/// PaintContext fallback up front; a callback added where it is false is skipped, never a crash. Same as GetIO().nativeBackend != 0.
inline bool NativeDrawAvailable()
{
    return GetIO().nativeBackend != 0;
}
DrawList& GetDrawList();        // current / new-stage DrawList
DrawList& GetTransDrawList();   // old-stage DrawList (valid during transitions)
DrawList& GetOverlayDrawList(); // always rendered last, on top of all stages

// Call once after uploading the font atlas texture.
// Sets the white pixel on ALL DrawLists (stage, trans, overlay).
void SetWhitePixel(TextureHandle atlas, Vec2 whiteUV);
// The alternate texture of shapes (see DrawList::setSolidAlternate), on ALL DrawLists: the dynamic page of the font atlas, after
// it was made and uploaded.
void SetSolidAlternate(TextureHandle texture, Vec2 whiteUV);

void NewFrame();
void Render();
DrawData* GetDrawData();


} // namespace retained
} // namespace ig
