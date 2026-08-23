#pragma once

// Legacy widget-system icon atlas.
//
// This belongs to the old Texture/BuImage/RenderBatch path and is only kept for
// BUGUI_BUILD_EXTRA_WIDGETS while the migration is in progress. New BuGUI code
// should use BuGUI::DrawList::addIcon() for vector icons or FontAtlas merged
// icon glyphs for richer icon sets.

#include "BuGUI_base.hpp"
#include "BuGUI.hpp"     // BuGUI::TextureHandle
#include "BuImage.hpp"   // BuGUI::BuImage
#include <vector>


namespace BuGUI
{

// ═════════════════════════════════════════════════════════════════════════════
//  IconAtlas - renders all icons into a single RGBA BuImage at startup,
//  uploads as one GL texture.  Widgets draw icons as textured quads.
//
//  Usage:
//      atlas.init(24);                    // 24×24 px per icon cell
//      atlas.texture();                   // the GL texture
//      atlas.srcRect(IconId::Folder);     // FloatRect in pixel coords
//
//  Drawing (through PaintContext):
//      ctx.drawIcon(IconId::Folder, x, y, 16);
// ═════════════════════════════════════════════════════════════════════════════

class IconAtlas
{
public:
    IconAtlas() = default;
    ~IconAtlas();

    /// @brief Generate all icons into a BuImage (caller uploads to GPU).
    BuGUI::BuImage* buildImage(int cellSize = 24);

    /// @brief Check if the atlas has been initialized.
    bool ready() const { return (bool)tex_; }

    /// @brief Get the opaque GPU texture handle.
    BuGUI::TextureHandle texture() const { return tex_; }

    /// @brief Set the texture handle after GPU upload.
    void setTexture(BuGUI::TextureHandle h, int w, int h_) { tex_ = h; atlasW_ = w; atlasH_ = h_; }

    /// @brief Get the atlas texture width in pixels.
    int textureWidth()  const { return atlasW_; }
    /// @brief Get the atlas texture height in pixels.
    int textureHeight() const { return atlasH_; }

    /// @brief Get the source rect for an icon within the atlas.
    FloatRect srcRect(IconId id) const;

    /// @brief Get the icon cell size in pixels.
    int cellSize() const { return cellSize_; }

private:
    void drawAllIcons(BuImage& pm);
    void drawIcon(BuImage& pm, IconId id, int ox, int oy, int sz);

    // Per-icon drawing helpers (ox,oy = cell origin, sz = cell size)
    void drawFolder     (BuImage& pm, int ox, int oy, int sz);
    void drawFolderOpen (BuImage& pm, int ox, int oy, int sz);
    void drawFile       (BuImage& pm, int ox, int oy, int sz);
    void drawFileCode   (BuImage& pm, int ox, int oy, int sz);
    void drawBook       (BuImage& pm, int ox, int oy, int sz);
    void drawGear       (BuImage& pm, int ox, int oy, int sz);
    void drawStar       (BuImage& pm, int ox, int oy, int sz);
    void drawHeart      (BuImage& pm, int ox, int oy, int sz);
    void drawSearch     (BuImage& pm, int ox, int oy, int sz);
    void drawPlus       (BuImage& pm, int ox, int oy, int sz);
    void drawMinus      (BuImage& pm, int ox, int oy, int sz);
    void drawCheck      (BuImage& pm, int ox, int oy, int sz);
    void drawCross      (BuImage& pm, int ox, int oy, int sz);
    void drawArrowRight (BuImage& pm, int ox, int oy, int sz);
    void drawArrowDown  (BuImage& pm, int ox, int oy, int sz);
    void drawArrowUp    (BuImage& pm, int ox, int oy, int sz);
    void drawArrowLeft  (BuImage& pm, int ox, int oy, int sz);
    void drawEye        (BuImage& pm, int ox, int oy, int sz);
    void drawEyeOff     (BuImage& pm, int ox, int oy, int sz);
    void drawLock       (BuImage& pm, int ox, int oy, int sz);
    void drawUnlock     (BuImage& pm, int ox, int oy, int sz);
    void drawRefresh    (BuImage& pm, int ox, int oy, int sz);
    void drawTrash      (BuImage& pm, int ox, int oy, int sz);
    void drawEdit       (BuImage& pm, int ox, int oy, int sz);
    void drawHome       (BuImage& pm, int ox, int oy, int sz);
    void drawUser       (BuImage& pm, int ox, int oy, int sz);
    void drawWarning    (BuImage& pm, int ox, int oy, int sz);
    void drawInfo       (BuImage& pm, int ox, int oy, int sz);
    void drawError      (BuImage& pm, int ox, int oy, int sz);
    void drawFileImage  (BuImage& pm, int ox, int oy, int sz);
    void drawFileArchive(BuImage& pm, int ox, int oy, int sz);
    void drawViewDetail (BuImage& pm, int ox, int oy, int sz);
    void drawViewList   (BuImage& pm, int ox, int oy, int sz);
    void drawViewGrid   (BuImage& pm, int ox, int oy, int sz);
    void drawPlay       (BuImage& pm, int ox, int oy, int sz);
    void drawPause      (BuImage& pm, int ox, int oy, int sz);
    void drawStop       (BuImage& pm, int ox, int oy, int sz);
    void drawStepForward(BuImage& pm, int ox, int oy, int sz);
    void drawStepBack   (BuImage& pm, int ox, int oy, int sz);
    void drawRecord     (BuImage& pm, int ox, int oy, int sz);

    BuGUI::TextureHandle tex_;
    int atlasW_    = 0;
    int atlasH_    = 0;
    int cellSize_  = 24;
    int cols_      = 0;
};

} // namespace BuGUI
