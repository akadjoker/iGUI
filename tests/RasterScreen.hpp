// A headless window whose frame is rasterised by the software rasteriser, so what a frame draws
// can be compared pixel by pixel. Shared by the tests that check where widgets put their text.
// No standard library: ct and C only.
#pragma once

#include "RetainedHarness.hpp"

#include <igui/widgets/IconAtlas.hpp>
#include <igui_zen/SoftRaster.hpp>

namespace rh
{

// What the rasteriser drew, 0xAARRGGBB.
struct Shot
{
    ct::Vector<uint32_t> px;
    int w = 0;
    int h = 0;
};

// A window whose font atlas is a texture of the software rasteriser (baked sizes only: the dynamic
// page is not a texture it can read, unless useDynamicPage() is called), so what a frame draws can be compared pixel by pixel.
class Screen
{
public:
    // releaseAtlasPixels: the RAM copy of the baked page is freed once the rasteriser has its texture, as ZenApp does with
    // its default atlas; what is drawn must not change.
    Screen(float width, float height, bool dynamicFonts = false, bool releaseAtlasPixels = false)
            : h(width, height, dynamicFonts),
              w_(static_cast<int>(width)),
              h_(static_cast<int>(height))
    {
        raster_.setSamples(1);
        atlasTexture_ = raster_.createTexture(h.atlas().width(), h.atlas().height(), h.atlas().pixels());
        h.atlas().setTexture(atlasTexture_);
#ifdef IGUI_FONTATLAS_RELEASE_PIXELS
        if (releaseAtlasPixels) h.atlas().releasePixels();
#else
        (void) releaseAtlasPixels;
#endif
    }
    ~Screen()
    {
        if (dynamic_)
        {
            // the page is the rasteriser's texture: let go of it (and of the callbacks that point at this object) first
            h.app.removeDynamicAtlas(h.atlas());
            h.app.setTextureUpload(ct::Function<TextureHandle(const unsigned char*, int, int)>());
            h.app.setTextureUpdate(ct::Function<bool(TextureHandle, const unsigned char*, int, int)>());
            h.app.setTextureDestroy(ct::Function<void(TextureHandle)>());
        }
        raster_.destroyTexture(atlasTexture_);
    }
    // The dynamic page of the atlas as a texture of the rasteriser too, kept up to date the way an application's extra
    // atlas is (WidgetApp::addDynamicAtlas: a glyph rasterised while measuring or painting goes up before the frame is
    // drawn), so text at a size the baked page does not have, and glyphs it lacks, can be compared pixel by pixel. Call it
    // after the constructor, which is where ZenApp releases the baked pixels: its default atlas gets the page after that.
    bool useDynamicPage()
    {
        h.app.setTextureUpload([this](const unsigned char* rgba, int w, int hh) { return raster_.createTexture(w, hh, rgba); });
        h.app.setTextureUpdate([this](TextureHandle t, const unsigned char* rgba, int w, int hh)
                { return raster_.updateTexture(t, rgba, w, hh); });
        h.app.setTextureDestroy([this](TextureHandle t) { raster_.destroyTexture(t); });
        dynamic_ = h.app.addDynamicAtlas(h.atlas());
        return dynamic_;
    }
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;
    // withIcons: the frame is painted again with an icon atlas (an empty one: icons come out as vector
    // drawings), which a TreeGrid needs to draw the icons of its nodes
    Shot take(bool withIcons = false)
    {
        if (withIcons)
        {
            NewFrame();
            h.app.paint(*GetDrawData(), h.font(), &icons_);
            Render();
        }
        Shot shot;
        shot.w = w_;
        shot.h = h_;
        shot.px.resize(static_cast<size_t>(w_) * static_cast<size_t>(h_));
        raster_.render(*GetDrawData(), shot.px.data(), w_, h_, w_, 0xff000000u);
        return shot;
    }
    Harness h;

private:
    ig::zen::SoftRaster raster_;
    TextureHandle atlasTexture_;
    IconAtlas icons_;
    int w_;
    int h_;
    bool dynamic_ = false;
};

// The pixels of a rectangle (x0, y0 inclusive, x1, y1 exclusive) that are not the same in two shots.
inline int differing(const Shot& a, const Shot& b, int x0, int y0, int x1, int y1)
{
    if (a.w != b.w || a.h != b.h) return 1 << 30;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > a.w) x1 = a.w;
    if (y1 > a.h) y1 = a.h;
    int n = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (a.px[static_cast<size_t>(y) * static_cast<size_t>(a.w) + static_cast<size_t>(x)] !=
                    b.px[static_cast<size_t>(y) * static_cast<size_t>(b.w) + static_cast<size_t>(x)])
                ++n;
    return n;
}

} // namespace rh
