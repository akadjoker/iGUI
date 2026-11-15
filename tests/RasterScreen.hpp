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
// page is not a texture it can read), so what a frame draws can be compared pixel by pixel.
class Screen
{
public:
    Screen(float width, float height, bool dynamicFonts = false)
            : h(width, height, dynamicFonts),
              w_(static_cast<int>(width)),
              h_(static_cast<int>(height))
    {
        raster_.setSamples(1);
        atlasTexture_ = raster_.createTexture(h.atlas().width(), h.atlas().height(), h.atlas().pixels());
        h.atlas().setTexture(atlasTexture_);
    }
    ~Screen()
    {
        raster_.destroyTexture(atlasTexture_);
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
