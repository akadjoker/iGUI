#pragma once

#include "BuGUI_base.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>

namespace BuGUI
{
    // ── Platform-agnostic key constants ──────────────────────────────────
    // Values chosen so the SDL backend can pass its keycodes directly.
    // Other backends must map their native codes to these values.
    namespace Key {
        constexpr int Return    = 13;
        constexpr int Escape    = 27;
        constexpr int Backspace = 8;
        constexpr int Tab       = 9;
        constexpr int Delete    = 127;
        constexpr int Right     = 1073741903;
        constexpr int Left      = 1073741904;
        constexpr int Down      = 1073741905;
        constexpr int Up        = 1073741906;
        constexpr int Home      = 1073741898;
        constexpr int End       = 1073741901;
        constexpr int PageUp    = 1073741899;
        constexpr int PageDown  = 1073741902;
        constexpr int KPEnter   = 1073741912;
        constexpr int Space     = ' ';
        constexpr int F1        = 1073741882;
        constexpr int F12       = 1073741893;
        // Letters (lowercase ASCII)
        constexpr int A = 'a', B = 'b', C = 'c', D = 'd', E = 'e';
        constexpr int F = 'f', G = 'g', H = 'h', I = 'i', J = 'j';
        constexpr int K = 'k', L = 'l', M = 'm', N = 'n', O = 'o';
        constexpr int P = 'p', Q = 'q', R = 'r', S = 's', T = 't';
        constexpr int U = 'u', V = 'v', W = 'w', X = 'x', Y = 'y';
        constexpr int Z = 'z';
        // Punctuation
        constexpr int Slash = '/';
    }

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

    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Rect
    {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;

        float right()  const { return x + w; }
        float bottom() const { return y + h; }

        bool contains(float px, float py) const
        {
            return px >= x && px < x + w && py >= y && py < y + h;
        }

        Rect shrunk(float pad) const
        {
            return {x + pad, y + pad, w - pad * 2, h - pad * 2};
        }
    };

    struct TextureHandle
    {
        uintptr_t value = 0;

        explicit operator bool() const { return value != 0; }
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

        std::vector<uint32_t> inputChars;

        // Per-frame key press / release events (cleared by NewFrame)
        struct KeyEvent
        {
            int  key      = 0;      // platform key code
            int  scancode = 0;      // hardware scancode
            bool shift    = false;
            bool ctrl     = false;
            bool alt      = false;
            bool down     = true;   // true = press, false = release
        };
        std::vector<KeyEvent> keyEvents;

        // Cursor hint written by WidgetApp::update(), read by backend.
        // Matches CursorType: 0=Arrow 1=Hand 2=IBeam 3=Crosshair
        //                     4=SizeH 5=SizeV 6=SizeAll 7=No
        int wantedCursor = 0;

        // ── Clipboard (set by backend) ───────────────────────────────────
        // Assign captureless lambdas or plain function pointers.
        void        (*setClipboardText)(const char*)        = nullptr;
        std::string (*getClipboardText)()                   = nullptr;

        // ── File I/O (set by backend) ────────────────────────────────────
        // readFile:  path → file contents (empty string on failure)
        // writeFile: path, data → true on success
        std::string (*readFile )(const std::string& path)                          = nullptr;
        bool        (*writeFile)(const std::string& path, const std::string& data) = nullptr;

        // ── OS-level file/text drop (set by backend) ─────────────────────
        // Backend pushes drop events each frame; WidgetApp consumes them.
        struct DropEvent {
            float x = 0, y = 0;                   // mouse position at drop
            std::vector<std::string> filePaths;    // files dropped (OS DnD)
            std::string text;                      // text dropped (if any)
        };
        std::vector<DropEvent> dropEvents;

        void addDropEvent(float x, float y, const std::vector<std::string>& paths,
                          const std::string& txt = "")
        {
            dropEvents.push_back({x, y, paths, txt});
        }

        void addInputCharacter(uint32_t c) { inputChars.push_back(c); }
        void addKeyEvent(int key, int scancode, bool shift, bool ctrl, bool alt, bool down)
        {
            keyEvents.push_back({key, scancode, shift, ctrl, alt, down});
        }
    };

    struct DrawVertex
    {
        float x = 0.0f;
        float y = 0.0f;
        float u = 0.0f;
        float v = 0.0f;
        Color color = Color(255, 255, 255, 255);
    };

    struct DrawCmd
    {
        TextureHandle texture;
        Rect clip;
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
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
        COUNT  // must be last
    };

    struct FontGlyph
    {
        uint32_t codepoint = 0;
        Rect uv;
        Vec2 size;
        Vec2 offset;
        float advanceX = 0.0f;
    };

    class Font
    {
    public:
        const FontGlyph *findGlyph(uint32_t codepoint) const;
        TextureHandle texture() const { return texture_; }
        float lineHeight() const { return lineHeight_; }
        float ascender()   const { return ascender_; }

    private:
        friend class FontAtlas;

        TextureHandle texture_;
        float lineHeight_ = 0.0f;
        float ascender_   = 0.0f;
        std::unordered_map<uint32_t, FontGlyph> glyphs_;
    };

    class FontAtlas
    {
    public:
        // ── Multi-font API ────────────────────────────────────────────────
        // Add fonts before calling build(). Returns a pointer valid after build().
        struct FontConfig {
            const unsigned char* ttfData  = nullptr;
            int                  ttfSize  = 0;
            float                pixelHeight = 16.0f;
            // If ttfData is nullptr, uses embedded DejaVuSans
        };

        // Add a font to be packed. Returns the font index (0-based).
        int  addFont(const FontConfig& cfg);
        int  addFontDefault(float pixelHeight = 16.0f);

        // Build the atlas (packs all added fonts into one texture).
        // Must be called after all addFont() calls.
        bool build();

        // ── Legacy single-font API (calls addFont + build internally) ─────
        bool buildDefault();
        bool buildFromTTFMemory(const unsigned char *data, int dataSize, float pixelHeight = 16.0f);

        void setTexture(TextureHandle texture);
        TextureHandle texture()     const { return texture_; }
        const Font   &defaultFont() const { return fonts_.empty() ? fallback_ : fonts_[0]; }
        const Font   *font(int idx) const { return (idx >= 0 && idx < (int)fonts_.size()) ? &fonts_[idx] : nullptr; }
        int           fontCount()   const { return static_cast<int>(fonts_.size()); }
        Vec2          whitePixelUV()const { return whitePixelUV_; }

        int width()  const { return width_; }
        int height() const { return height_; }
        const unsigned char *pixels() const { return pixels_.data(); }
        size_t pixelSize() const { return pixels_.size(); }

    private:
        int width_ = 0;
        int height_ = 0;
        TextureHandle texture_;
        Vec2          whitePixelUV_ = {0.0f, 0.0f};
        Font          fallback_;          // empty sentinel
        std::vector<Font>       fonts_;
        std::vector<FontConfig> pending_; // configs waiting for build()
        std::vector<unsigned char> pixels_;
    };

    class DrawList
    {
    public:
        void clear();

        void pushClip(const Rect &rect);
        void popClip();

        void addRect(const Rect &rect, const Color &color);
        void addRectOutline(const Rect &rect, const Color &color, float thickness = 1.0f);
        void addRoundRectFilled(const Rect &rect, float radius, const Color &color, int segments = 8);
        void addRoundRect(const Rect &rect, float radius, const Color &color, float thickness = 1.0f, int segments = 8);
        void addCircleFilled(Vec2 center, float radius, const Color &color, int segments = 32);
        void addCircle(Vec2 center, float radius, const Color &color, float thickness = 1.0f, int segments = 32);
        void addTriangleFilled(Vec2 a, Vec2 b, Vec2 c, const Color &color);
        void addTriangle(Vec2 a, Vec2 b, Vec2 c, const Color &color, float thickness = 1.0f);
        void addImage(TextureHandle texture, const Rect &rect, const Rect &uv, const Color &color = Color::WHITE);
        void addText(const Font &font, Vec2 pos, const Color &color, const char *text, float scale = 1.0f);


        Vec2 calcTextSize(const Font& font, const char* text, float scale = 1.0f);

        void addTextAligned(const Font&  font,
                            const Rect&  bounds,
                            const Color& color,
                            const char*  text,
                            AlignX       ax = AlignX::Left,
                            AlignY       ay = AlignY::Top);

        void addIcon(IconId icon, const Rect &rect, const Color &color, float thickness = 2.0f);
        void addLine(Vec2 a, Vec2 b, const Color &color, float thickness = 1.0f);

        // ── Single-texture mode (white pixel in font atlas) ───────────────
        // Call once after uploading the font atlas texture. All geometry will
        // reuse the same texture handle (sampling the white pixel), so that
        // geometry and text commands can be merged into one draw call.
        void setWhitePixel(TextureHandle atlas, Vec2 whiteUV);

        // ── Path builder (accumulate → flush) ─────────────────────────────
        // pathArcToFast: aMinOf12 / aMaxOf12 are indices into a 12-step
        // unit-circle LUT (each step = 30°). Range [0..12] covers 0°–360°.
        void pathLineTo(Vec2 p);
        void pathArcToFast(Vec2 center, float radius, int aMinOf12, int aMaxOf12);
        void pathFillConvex(const Color &color);
        void pathStroke(const Color &color, float thickness, bool closed);
        void pathClear();

        const std::vector<DrawVertex> &vertices() const { return vertices_; }
        const std::vector<uint32_t> &indices() const { return indices_; }
        const std::vector<DrawCmd> &commands() const { return commands_; }
        int pushClipCount() const { return pushClipCount_; }

    private:
        Rect currentClip() const;
        void addCmd(uint32_t indexOffset, uint32_t indexCount, TextureHandle texture = {});
        uint32_t addVertex(float x, float y, const Color &color);
        uint32_t addVertex(float x, float y, float u, float v, const Color &color);
        void addConvexPolyFilled(const std::vector<Vec2> &points, const Color &color);
        void addPolyline(const std::vector<Vec2> &points, const Color &color, float thickness, bool closed);

        std::vector<Rect>       clipStack_;
        std::vector<Vec2>       path_;
        TextureHandle           fontTexture_;
        Vec2                    whiteUV_;
        std::vector<DrawVertex> vertices_;
        std::vector<uint32_t>   indices_;
        std::vector<DrawCmd>    commands_;
        int                     pushClipCount_ = 0;
           std::unordered_map<uint32_t, FontGlyph> glyphs_;
    };

    struct RenderStats
    {
        int drawCalls        = 0;
        int vertices         = 0;
        int triangles        = 0;
        int textureSwitches  = 0;
        int clipChanges      = 0;  // pushClip calls per frame

        void reset() { drawCalls = 0; vertices = 0; triangles = 0; textureSwitches = 0; clipChanges = 0; }
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
        float x      = 0.0f;
        float y      = 0.0f;
        float scale  = 1.0f;
        float angle  = 0.0f;   // radians, CCW
        float pivotX = 0.5f;   // normalised (0=left, 1=right)
        float pivotY = 0.5f;   // normalised (0=top,  1=bottom)
    };

    // One render pass: a DrawList with its own GPU camera transform.
    // Passes are rendered back-to-front (first = bottom, last = top).
    struct DrawPass
    {
        DrawList* list   = nullptr;
        Camera2D  camera;
    };

    struct DrawData
    {
        std::vector<DrawPass> passes;          // populated by WidgetApp::paint()
        float  displayWidth      = 0.0f;
        float  displayHeight     = 0.0f;
        float  framebufferScaleX = 1.0f;
        float  framebufferScaleY = 1.0f;
        RenderStats  stats;   // filled by the backend after rendering
    };

    struct Context;

    Context *CreateContext();
    void DestroyContext(Context *ctx);
    void SetCurrentContext(Context *ctx);
    Context *GetCurrentContext();

    IO &GetIO();
    DrawList &GetDrawList();          // current / new-stage DrawList
    DrawList &GetTransDrawList();     // old-stage DrawList (valid during transitions)
    DrawList &GetOverlayDrawList();   // always rendered last, on top of all stages

    // Call once after uploading the font atlas texture.
    // Sets the white pixel on ALL DrawLists (stage, trans, overlay).
    void SetWhitePixel(TextureHandle atlas, Vec2 whiteUV);

    void NewFrame();
    void Render();
    DrawData *GetDrawData();


} // namespace BuGUI
