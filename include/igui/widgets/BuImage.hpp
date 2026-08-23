#pragma once
#include <vector>
#include "BuGUI_base.hpp"

namespace BuGUI {

class BuImage
{
public:
    /// @brief Pixel blending mode.
    enum class BlendMode
    {
        Copy,
        Alpha,
        Add,
        Multiply
    };

    BuImage();
    ~BuImage();
    BuImage(int w, int h, int components);
    BuImage(int w, int h, int components, unsigned char *data);
    BuImage(const BuImage &image, const IntRect &crop);
    BuImage(const BuImage &other) = delete;
    BuImage &operator=(const BuImage &other) = delete;

    // Pixel operations
    /// @brief Set a pixel using individual RGBA components.
    void SetPixel(u32 x, u32 y, u8 r, u8 g, u8 b, u8 a);
    /// @brief Set a pixel using a packed RGBA value.
    void SetPixel(u32 x, u32 y, u32 rgba);
    /// @brief Get the packed RGBA value of a pixel.
    u32 GetPixel(u32 x, u32 y) const;
    /// @brief Get the color of a pixel.
    Color GetPixelColor(u32 x, u32 y) const;

    /// @brief Create a copy converted to RGBA format.
    BuImage* ConvertToRGBA() const;
    /// @brief Apply a color tint to all pixels.
    void Tint(u8 r, u8 g, u8 b);

    /// @brief Create a resized copy of the image.
    BuImage* Resize(int newWidth, int newHeight) const;
    /// @brief Create a cropped copy from the given rectangle.
     BuImage* Crop(const IntRect &rect) const;
    /// @brief Create a cropped copy from position and size.
    BuImage* Crop(int x, int y, int w, int h) const;
    /// @brief Create a cropped copy with optional transparent fill.
    BuImage* CropExtended(const IntRect &rect, bool fillTransparent = true) const;
  

    // Fill operations
    /// @brief Fill the entire image with an RGBA color.
    void Fill(u8 r, u8 g, u8 b, u8 a);
    /// @brief Fill the entire image with a packed RGBA value.
    void Fill(u32 rgba);
    /// @brief Clear the image to transparent black.
    void Clear();

    // File operations
    /// @brief Save the image to a file.
    bool Save(const char *file_name);
    /// @brief Load an image from a file.
    bool Load(const char *file_name);
    /// @brief Load an image from a memory buffer.
    bool LoadFromMemory(const unsigned char *buffer, unsigned int bytesRead);

    // Transform operations
    /// @brief Flip the image vertically.
    void FlipVertical();
    /// @brief Flip the image horizontally.
    void FlipHorizontal();

 
    /// @brief Draw a line between two points.
    void DrawLine(int x1, int y1, int x2, int y2, const Color &color);
    /// @brief Draw a rectangle, optionally filled.
    void DrawRect(int x, int y, int w, int h, const Color &color, bool fill = false);
    /// @brief Draw a circle, optionally filled.
    void DrawCircle(int cx, int cy, int radius, const Color &color, bool fill = false);
    /// @brief Draw another image onto this one at the given position.
    void DrawPixmap(const BuImage &source, int x, int y);
    /// @brief Draw a region of another image onto this one.
    void DrawPixmap(const BuImage &source, int x, int y, const IntRect &srcRect);
    /// @brief Blend a color onto a single pixel.
    void BlendPixel(u32 x, u32 y, const Color &color, float opacity = 1.0f, BlendMode mode = BlendMode::Alpha);
    /// @brief Draw another image with blending.
    void DrawPixmapBlended(const BuImage &source, int x, int y, float opacity = 1.0f, BlendMode mode = BlendMode::Alpha);
    /// @brief Draw a region of another image with blending.
    void DrawPixmapBlended(const BuImage &source, int x, int y, const IntRect &srcRect, float opacity = 1.0f, BlendMode mode = BlendMode::Alpha);

 
    /// @brief Copy a rectangular region from another image.
    void CopyRegion(const BuImage &source, const IntRect &srcRect, int dstX, int dstY);

 
    /// @brief Replace one color with another within a threshold.
    void ReplaceColor(const Color &from, const Color &to, float threshold = 0.0f);
    /// @brief Make a color fully transparent (color keying).
    void SetColorKey(const Color &key, float threshold = 0.0f);

   
    /// @brief Create a box-blurred copy of the image.
    BuImage *ApplyBlur(int radius) const;
    /// @brief Create a Gaussian-blurred copy of the image.
    BuImage *ApplyGaussianBlur(int radius) const;
    /// @brief Create a sharpened copy of the image.
    BuImage *ApplySharpen() const;
    /// @brief Create an edge-detected copy of the image.
    BuImage *ApplyEdgeDetection() const;
    /// @brief Create an embossed copy of the image.
    BuImage *ApplyEmboss() const;

    /// @brief Check if the image has valid pixel data.
    bool IsValid() const { return pixels != nullptr; }
    /// @brief Get the total pixel data size in bytes.
    int GetSize() const { return width * height * components; }
    /// @brief Check if the image has an alpha channel.
    bool HasAlpha() const { return components == 2 || components == 4; }

    unsigned char *pixels;
    int components;
    int width;
    int height;
}; // class BuImage


// Global alias so existing code compiles without changes
using BuGUI::BuImage;

} // namespace BuGUI
 