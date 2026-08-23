#include "pch.hpp"
#include <stb_image.h>
#include <stb_image_write.h>

namespace BuGUI {

namespace
{

bool HasExtension(const char *fileName, const char *extension)
{
    if (!fileName || !extension) return false;
    size_t fnLen = std::strlen(fileName);
    size_t exLen = std::strlen(extension);
    if (exLen > fnLen) return false;
    return strcasecmp(fileName + fnLen - exLen, extension) == 0;
}

u8 clampToByte(int value)
{
    return static_cast<u8>(value < 0 ? 0 : (value > 255 ? 255 : value));
}

u8 mixByte(u8 dst, u8 src, float alpha)
{
    const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
    const float invAlpha = 1.0f - clampedAlpha;
    return clampToByte(static_cast<int>(std::lround(static_cast<float>(src) * clampedAlpha + static_cast<float>(dst) * invAlpha)));
}

Color blendSrcOver(const Color &srcColor, const Color &dstColor, float opacity)
{
    const float clampedOpacity = std::clamp(opacity, 0.0f, 1.0f);
    if (clampedOpacity <= 0.0f)
        return dstColor;

    const u32 srcA = clampToByte(static_cast<int>(std::lround((static_cast<float>(srcColor.a) / 255.0f) * clampedOpacity * 255.0f)));
    if (srcA == 0)
        return dstColor;

    const u32 srcR = srcColor.r;
    const u32 srcG = srcColor.g;
    const u32 srcB = srcColor.b;

    u32 dstA = dstColor.a;
    const u32 dstR = dstColor.r;
    const u32 dstG = dstColor.g;
    const u32 dstB = dstColor.b;

    dstA -= (dstA * srcA) / 255;
    const u32 outA = dstA + srcA;
    if (outA == 0)
        return Color(0, 0, 0, 0);

    const u32 outR = (dstR * dstA + srcR * srcA) / outA;
    const u32 outG = (dstG * dstA + srcG * srcA) / outA;
    const u32 outB = (dstB * dstA + srcB * srcA) / outA;
    return Color(static_cast<u8>(outR), static_cast<u8>(outG), static_cast<u8>(outB), static_cast<u8>(outA));
}

int SelectPixmapComponents(int sourceComponents)
{
    switch (sourceComponents)
    {
    case 2:
    case 4:
        return 4;
    case 1:
    case 3:
    default:
        return 3;
    }
}

bool AssignLoadedPixelsToPixmap(BuImage &pixmap,
                                const unsigned char *sourcePixels,
                                int width, int height,
                                int sourceComponents)
{
    if (!sourcePixels || width <= 0 || height <= 0 || sourceComponents < 1 || sourceComponents > 4)
    {
        return false;
    }

    const int newComponents = SelectPixmapComponents(sourceComponents);
    const size_t totalBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    unsigned char *newPixels = static_cast<unsigned char *>(malloc(totalBytes));
    if (!newPixels)
    {
        fprintf(stderr, "[PIXMAP] Failed to allocate %zu bytes for image", totalBytes);
        return false;
    }
    memset(newPixels, 0, totalBytes);

    if (newComponents == sourceComponents)
    {
        memcpy(newPixels, sourcePixels, static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(newComponents));
    }
    else
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const size_t srcIndex = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * static_cast<size_t>(sourceComponents);
                const size_t dstIndex = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * static_cast<size_t>(newComponents);

                const unsigned char r = sourcePixels[srcIndex + 0];
                const unsigned char g = (sourceComponents >= 3) ? sourcePixels[srcIndex + 1] : r;
                const unsigned char b = (sourceComponents >= 3) ? sourcePixels[srcIndex + 2] : r;
                const unsigned char a = (sourceComponents == 2) ? sourcePixels[srcIndex + 1]
                                          : (sourceComponents == 4) ? sourcePixels[srcIndex + 3]
                                                                     : 255;

                if (newComponents == 3)
                {
                    newPixels[dstIndex + 0] = r;
                    newPixels[dstIndex + 1] = g;
                    newPixels[dstIndex + 2] = b;
                }
                else
                {
                    newPixels[dstIndex + 0] = r;
                    newPixels[dstIndex + 1] = g;
                    newPixels[dstIndex + 2] = b;
                    newPixels[dstIndex + 3] = a;
                }
            }
        }
    }

    if (pixmap.pixels)
    {
        free(pixmap.pixels);
    }

    pixmap.pixels = newPixels;
    pixmap.width = width;
    pixmap.height = height;
    pixmap.components = newComponents;
    return true;
}

bool BuildSaveBuffer(const BuImage &pixmap, std::vector<unsigned char> &scratch, int &saveComponents, const unsigned char *&savePixels)
{
    if (!pixmap.pixels || pixmap.width <= 0 || pixmap.height <= 0)
    {
        return false;
    }

    if (pixmap.components == 1 || pixmap.components == 3 || pixmap.components == 4)
    {
        saveComponents = pixmap.components;
        savePixels = pixmap.pixels;
        return true;
    }

    saveComponents = 4;

    scratch.resize(static_cast<size_t>(pixmap.width) * static_cast<size_t>(pixmap.height) * 4);
    for (int y = 0; y < pixmap.height; y++)
    {
        for (int x = 0; x < pixmap.width; x++)
        {
            const size_t dstIndex = static_cast<size_t>(y * pixmap.width + x) * 4;
            const Color color = pixmap.GetPixelColor(x, y);
            scratch[dstIndex + 0] = color.r;
            scratch[dstIndex + 1] = color.g;
            scratch[dstIndex + 2] = color.b;
            scratch[dstIndex + 3] = color.a;
        }
    }

    savePixels = scratch.data();
    return true;
}

bool SavePixmapAsTga(const BuImage &pixmap, const char *fileName)
{
    std::ofstream file(fileName, std::ios::binary);
    if (!file)
    {
        fprintf(stderr, "[PIXMAP] Failed to open TGA file for writing: %s", fileName);
        return false;
    }

    const bool grayscale = (pixmap.components == 1);
    const int bitsPerPixel = grayscale ? 8 : ((pixmap.components == 4 || pixmap.components == 2) ? 32 : 24);

    unsigned char header[18] = {};
    header[2] = grayscale ? 3 : 2;
    header[12] = static_cast<unsigned char>(pixmap.width & 0xFF);
    header[13] = static_cast<unsigned char>((pixmap.width >> 8) & 0xFF);
    header[14] = static_cast<unsigned char>(pixmap.height & 0xFF);
    header[15] = static_cast<unsigned char>((pixmap.height >> 8) & 0xFF);
    header[16] = static_cast<unsigned char>(bitsPerPixel);
    header[17] = static_cast<unsigned char>((bitsPerPixel == 32 ? 8 : 0) | 0x20);

    file.write(reinterpret_cast<const char *>(header), sizeof(header));
    if (!file)
    {
        fprintf(stderr, "[PIXMAP] Failed to write TGA header: %s", fileName);
        return false;
    }

    std::vector<unsigned char> row(static_cast<size_t>(pixmap.width) * static_cast<size_t>(bitsPerPixel / 8));
    for (int y = 0; y < pixmap.height; y++)
    {
        if (grayscale)
        {
            for (int x = 0; x < pixmap.width; x++)
            {
                row[static_cast<size_t>(x)] = pixmap.GetPixelColor(x, y).r;
            }
        }
        else if (bitsPerPixel == 24)
        {
            for (int x = 0; x < pixmap.width; x++)
            {
                const Color color = pixmap.GetPixelColor(x, y);
                const size_t offset = static_cast<size_t>(x) * 3;
                row[offset + 0] = color.b;
                row[offset + 1] = color.g;
                row[offset + 2] = color.r;
            }
        }
        else
        {
            for (int x = 0; x < pixmap.width; x++)
            {
                const Color color = pixmap.GetPixelColor(x, y);
                const size_t offset = static_cast<size_t>(x) * 4;
                row[offset + 0] = color.b;
                row[offset + 1] = color.g;
                row[offset + 2] = color.r;
                row[offset + 3] = color.a;
            }
        }

        file.write(reinterpret_cast<const char *>(row.data()), static_cast<std::streamsize>(row.size()));
        if (!file)
        {
            fprintf(stderr, "[PIXMAP] Failed while writing TGA pixel data: %s", fileName);
            return false;
        }
    }

    return true;
}
} // namespace


BuImage::BuImage()
{

    width = 0;
    height = 0;
    components = 0;
    pixels = nullptr;
}

BuImage::~BuImage()
{
    if (pixels)
    {
        free(pixels);
    }
}

BuImage::BuImage(const BuImage &image, const IntRect &crop)
{
    width = crop.width;
    height = crop.height;
    components = image.components;
    pixels = (unsigned char *)malloc(width * height * 4);
    memset(pixels, 0, width * height * 4);
    for (int y = (int)crop.y, offsetSize = 0; y < (int)(crop.y + crop.height); y++)
    {
        memcpy(pixels + offsetSize, ((unsigned char *)image.pixels) + (y * image.width + (int)crop.x) * components, (int)crop.width * components);
        offsetSize += ((int)crop.width * components);
    }
}

BuImage::BuImage(int w, int h, int components)
{

    width = w;
    height = h;
    this->components = components;
    pixels = (unsigned char *)malloc(w * h * 4);
    memset(pixels, 0, w * h * 4);
}

BuImage::BuImage(int w, int h, int components, unsigned char *data)
{

    pixels = (unsigned char *)malloc(w * h * 4);
    memset(pixels, 0, w * h * 4);
    width = w;
    height = h;
    this->components = components;
    memcpy(pixels, data, w * h * components);
}

void BuImage::Clear()
{
    if (pixels)
    {
        for (int i = 0; i < width * height * components; i++)
        {
            pixels[i] = 0;
        }
    }
}

void BuImage::SetPixel(u32 x, u32 y, u8 r, u8 g, u8 b, u8 a)
{

    if (x >= (u32)width || y >= (u32)height)
        return;

    if (components == 1)
    {
        unsigned char gray = (unsigned char)(r * 0.299f + g * 0.587f + b * 0.114f);
        ((u8 *)pixels)[y * width + x] = gray;
    }
    else if (components == 2)
    {
        u8 gray = (u8)(r * 0.299f + g * 0.587f + b * 0.114f);

        ((u8 *)pixels)[(y * width + x) * 2] = gray;
        ((u8 *)pixels)[(y * width + x) * 2 + 1] = a;
    }
    else if (components == 3)
    {

        ((u8 *)pixels)[(y * width + x) * 3] = r;
        ((u8 *)pixels)[(y * width + x) * 3 + 1] = g;
        ((u8 *)pixels)[(y * width + x) * 3 + 2] = b;
    }
    else if (components == 4)
    {
        ((u8 *)pixels)[(y * width + x) * 4] = r;
        ((u8 *)pixels)[(y * width + x) * 4 + 1] = g;
        ((u8 *)pixels)[(y * width + x) * 4 + 2] = b;
        ((u8 *)pixels)[(y * width + x) * 4 + 3] = a;
    }
}

void BuImage::SetPixel(u32 x, u32 y, u32 rgba)
{

    if (x >= (u32)width || y >= (u32)height)
        return;

    u8 r = rgba;
    u8 g = rgba >> 8;
    u8 b = rgba >> 16;
    u8 a = rgba >> 24;
    SetPixel(x, y, r, g, b, a);
}

u32 BuImage::GetPixel(u32 x, u32 y) const
{

    if (x >= (u32)width || y >= (u32)height)
        return 0;

    if (components == 1)
    {
        return pixels[y * width + x];
    }
    else if (components == 2)
    {
        return pixels[(y * width + x) * 2] | (pixels[(y * width + x) * 2 + 1] << 8);
    }
    else if (components == 3)
    {
        return pixels[(y * width + x) * 3] | (pixels[(y * width + x) * 3 + 1] << 8) | (pixels[(y * width + x) * 3 + 2] << 16);
    }
    else if (components == 4)
    {
        return pixels[(y * width + x) * 4] | (pixels[(y * width + x) * 4 + 1] << 8) | (pixels[(y * width + x) * 4 + 2] << 16) | (pixels[(y * width + x) * 4 + 3] << 24);
    }

    return 0;
}

Color BuImage::GetPixelColor(u32 x, u32 y) const
{

    Color color = Color::BLACK;

    if ((x < (u32)width) && (y < (u32)height))
    {

        if (components == 1)
        {
            color.r = (u8)((u8 *)pixels)[y * width + x];
            color.g = (u8)((u8 *)pixels)[y * width + x];
            color.b = (u8)((u8 *)pixels)[y * width + x];
            color.a = 255;
        }
        else if (components == 2)
        {
            color.r = (u8)((u8 *)pixels)[(y * width + x) * 2];
            color.g = (u8)((u8 *)pixels)[(y * width + x) * 2];
            color.b = (u8)((u8 *)pixels)[(y * width + x) * 2];
            color.a = (u8)((u8 *)pixels)[(y * width + x) * 2 + 1];
        }
        else if (components == 3)
        {
            color.r = (u8)((u8 *)pixels)[(y * width + x) * 3];
            color.g = (u8)((u8 *)pixels)[(y * width + x) * 3 + 1];
            color.b = (u8)((u8 *)pixels)[(y * width + x) * 3 + 2];
            color.a = 255;
        }
        else if (components == 4)
        {
            color.r = (u8)((u8 *)pixels)[(y * width + x) * 4];
            color.g = (u8)((u8 *)pixels)[(y * width + x) * 4 + 1];
            color.b = (u8)((u8 *)pixels)[(y * width + x) * 4 + 2];
            color.a = (u8)((u8 *)pixels)[(y * width + x) * 4 + 3];
        }
    }
    return color;
}

void BuImage::Fill(u8 r, u8 g, u8 b, u8 a)
{
    for (u32 y = 0; y < (u32)height; y++)
    {
        for (u32 x = 0; x < (u32)width; x++)
        {
            SetPixel(x, y, r, g, b, a);
        }
    }
}

void BuImage::Fill(u32 rgba)
{
    for (u32 y = 0; y < (u32)height; y++)
    {
        for (u32 x = 0; x < (u32)width; x++)
        {
            SetPixel(x, y, rgba);
        }
    }
}

bool BuImage::Load(const char *file_name)
{
    int width = 0;
    int height = 0;
    int sourceComponents = 0;
    unsigned char *loadedPixels = stbi_load(file_name, &width, &height, &sourceComponents, 0);
    if (!loadedPixels)
    {
        fprintf(stderr, "[PIXMAP] Failed to load image: %s (%s)",
                     file_name, stbi_failure_reason());
        return false;
    }

    const bool loaded = AssignLoadedPixelsToPixmap(*this, loadedPixels, width, height, sourceComponents);
    stbi_image_free(loadedPixels);
    if (!loaded)
    {
        fprintf(stderr, "[PIXMAP] Failed to load image: %s", file_name);
        return false;
    }

    fprintf(stdout, "[PIXMAP] Load image: %s (%d,%d) bpp:%d", file_name, width, height, components);

    return true;
}

bool BuImage::LoadFromMemory(const unsigned char *buffer, unsigned int bytesRead)
{
    if (!buffer || bytesRead == 0)
    {
        fprintf(stderr, "[PIXMAP] Invalid image buffer");
        return false;
    }

    if (bytesRead > static_cast<unsigned int>(INT_MAX))
    {
        fprintf(stderr, "[PIXMAP] Image buffer too large: %u bytes", bytesRead);
        return false;
    }

    int width = 0;
    int height = 0;
    int sourceComponents = 0;
    unsigned char *loadedPixels = stbi_load_from_memory(buffer,
                                                        static_cast<int>(bytesRead),
                                                        &width, &height,
                                                        &sourceComponents, 0);
    if (!loadedPixels)
    {
        fprintf(stderr, "[PIXMAP] Failed to load image from memory: %s",
                     stbi_failure_reason());
        return false;
    }

    const bool loaded = AssignLoadedPixelsToPixmap(*this, loadedPixels, width, height, sourceComponents);
    stbi_image_free(loadedPixels);
    return loaded;
}

bool BuImage::Save(const char *file_name)
{
    if (pixels == nullptr)
    {
        fprintf(stderr, "[PIXMAP] Failed to save image: %s", file_name);
        return false;
    }

    if (HasExtension(file_name, ".tga"))
    {
        return SavePixmapAsTga(*this, file_name);
    }

    std::vector<unsigned char> scratch;
    int saveComponents = 0;
    const unsigned char *savePixels = nullptr;
    if (!BuildSaveBuffer(*this, scratch, saveComponents, savePixels))
    {
        fprintf(stderr, "[PIXMAP] Failed to prepare image buffer for saving: %s", file_name);
        return false;
    }

    bool saved = false;
    if (HasExtension(file_name, ".bmp"))
    {
        saved = (stbi_write_bmp(file_name, width, height, saveComponents, savePixels) != 0);
        if (!saved)
        {
            fprintf(stderr, "[PIXMAP] Failed to save BMP: %s", file_name);
        }
    }
    else if (HasExtension(file_name, ".png"))
    {
        const int strideBytes = width * saveComponents;
        saved = (stbi_write_png(file_name, width, height, saveComponents, savePixels, strideBytes) != 0);
        if (!saved)
        {
            fprintf(stderr, "[PIXMAP] Failed to save PNG: %s", file_name);
        }
    }
    else if (HasExtension(file_name, ".jpg") || HasExtension(file_name, ".jpeg"))
    {
        saved = (stbi_write_jpg(file_name, width, height, saveComponents, savePixels, 95) != 0);
        if (!saved)
        {
            fprintf(stderr, "[PIXMAP] Failed to save JPG: %s", file_name);
        }
    }
    else
    {
        fprintf(stderr, "[PIXMAP] Unsupported output format: %s", file_name);
    }

    return saved;
}

void BuImage::FlipVertical()
{
    if (pixels == nullptr)
    {
        fprintf(stderr, "[PIXMAP] Failed to flip image");
        return;
    }
    int rowSize = width * components;
    unsigned char *row = (unsigned char *)malloc(rowSize);
    for (int y = 0; y < height / 2; y++)
    {
        unsigned char *src = ((unsigned char *)pixels) + y * rowSize;
        unsigned char *dst = ((unsigned char *)pixels) + (height - y - 1) * rowSize;
        memcpy(row, src, rowSize);
        memcpy(src, dst, rowSize);
        memcpy(dst, row, rowSize);
    }
    free(row);
}

void BuImage::FlipHorizontal()
{
    if (pixels == nullptr)
    {
        fprintf(stderr, "[PIXMAP] Failed to flip image");
        return;
    }
    int rowSize = width * components;
    unsigned char *row = (unsigned char *)malloc(rowSize);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width / 2; x++)
        {
            unsigned char *src = ((unsigned char *)pixels) + y * rowSize + x * components;
            unsigned char *dst = ((unsigned char *)pixels) + y * rowSize + (width - x - 1) * components;
            memcpy(row, src, components);
            memcpy(src, dst, components);
            memcpy(dst, row, components);
        }
    }
    free(row);
}

void BuImage::DrawLine(int x1, int y1, int x2, int y2, const Color &color)
{
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (true)
    {
        SetPixel(x1, y1, color.r, color.g, color.b, color.a);
        
        if (x1 == x2 && y1 == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx)  { err += dx; y1 += sy; }
    }
}

void BuImage::BlendPixel(u32 x, u32 y, const Color &color, float opacity, BlendMode mode)
{
    if (x >= static_cast<u32>(width) || y >= static_cast<u32>(height) || !pixels)
        return;

    const float clampedOpacity = std::clamp(opacity, 0.0f, 1.0f);
    if (clampedOpacity <= 0.0f)
        return;

    const Color dst = GetPixelColor(x, y);
    Color out = dst;

    switch (mode)
    {
    case BlendMode::Copy:
        out = color;
        out.a = mixByte(dst.a, color.a, clampedOpacity);
        break;
    case BlendMode::Add:
        out.r = clampToByte(dst.r + static_cast<int>(std::lround(static_cast<float>(color.r) * clampedOpacity)));
        out.g = clampToByte(dst.g + static_cast<int>(std::lround(static_cast<float>(color.g) * clampedOpacity)));
        out.b = clampToByte(dst.b + static_cast<int>(std::lround(static_cast<float>(color.b) * clampedOpacity)));
        out.a = clampToByte(dst.a + static_cast<int>(std::lround(static_cast<float>(color.a) * clampedOpacity)));
        break;
    case BlendMode::Multiply:
    {
        const u8 mulR = clampToByte(static_cast<int>((static_cast<float>(dst.r) * static_cast<float>(color.r)) / 255.0f));
        const u8 mulG = clampToByte(static_cast<int>((static_cast<float>(dst.g) * static_cast<float>(color.g)) / 255.0f));
        const u8 mulB = clampToByte(static_cast<int>((static_cast<float>(dst.b) * static_cast<float>(color.b)) / 255.0f));
        out.r = mixByte(dst.r, mulR, clampedOpacity);
        out.g = mixByte(dst.g, mulG, clampedOpacity);
        out.b = mixByte(dst.b, mulB, clampedOpacity);
        out.a = mixByte(dst.a, color.a, clampedOpacity);
        break;
    }
    case BlendMode::Alpha:
    default:
        out = blendSrcOver(color, dst, clampedOpacity);
        break;
    }

    SetPixel(x, y, out.r, out.g, out.b, out.a);
}

void BuImage::Tint(u8 r, u8 g, u8 b)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            Color c = GetPixelColor(x, y);
            c.r = (c.r * r) / 255;
            c.g = (c.g * g) / 255;
            c.b = (c.b * b) / 255;
            SetPixel(x, y, c.r, c.g, c.b, c.a);
        }
    }
}

BuImage* BuImage::ConvertToRGBA() const
{
    if (components == 4) return new BuImage(this->width, this->height, this->components, this->pixels);
    
    BuImage* result = new BuImage(width, height, 4);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            Color c = GetPixelColor(x, y);
            result->SetPixel(x, y, c.r, c.g, c.b, c.a);
        }
    }
    return result;
}

 
BuImage* BuImage::Resize(int newWidth, int newHeight ) const
{
    BuImage* result = new BuImage(newWidth, newHeight, components);
    
    for (int y = 0; y < newHeight; y++)
    {
        for (int x = 0; x < newWidth; x++)
        {
            int srcX = (x * width) / newWidth;
            int srcY = (y * height) / newHeight;
            
            u32 pixel = GetPixel(srcX, srcY);
            result->SetPixel(x, y, pixel);
        }
    }
    return result;
}

void BuImage::CopyRegion(const BuImage &source, const IntRect &srcRect, int dstX, int dstY)
{
    // Validação
    if (!source.pixels || !pixels) return;
    if (source.components != components) return;
    
    int srcX = srcRect.x;
    int srcY = srcRect.y;
    int srcW = srcRect.width;
    int srcH = srcRect.height;
    
    // Clamp source rect
    if (srcX < 0) { srcW += srcX; dstX -= srcX; srcX = 0; }
    if (srcY < 0) { srcH += srcY; dstY -= srcY; srcY = 0; }
    if (srcX + srcW > source.width) srcW = source.width - srcX;
    if (srcY + srcH > source.height) srcH = source.height - srcY;
    
    // Copy pixel by pixel
    for (int y = 0; y < srcH; y++)
    {
        for (int x = 0; x < srcW; x++)
        {
            int sx = srcX + x;
            int sy = srcY + y;
            int dx = dstX + x;
            int dy = dstY + y;
            
            // Check destination bounds
            if (dx >= 0 && dx < width && dy >= 0 && dy < height)
            {
                Color c = source.GetPixelColor(sx, sy);
                SetPixel(dx, dy, c.r, c.g, c.b, c.a);
            }
        }
    }
}

void BuImage::DrawPixmapBlended(const BuImage &source, int x, int y, float opacity, BlendMode mode)
{
    DrawPixmapBlended(source, x, y, IntRect(0, 0, source.width, source.height), opacity, mode);
}

void BuImage::DrawPixmapBlended(const BuImage &source, int x, int y, const IntRect &srcRect, float opacity, BlendMode mode)
{
    if (!source.pixels || !pixels)
        return;

    int srcX = srcRect.x;
    int srcY = srcRect.y;
    int srcW = srcRect.width;
    int srcH = srcRect.height;

    if (srcX < 0) { srcW += srcX; x -= srcX; srcX = 0; }
    if (srcY < 0) { srcH += srcY; y -= srcY; srcY = 0; }
    if (srcX + srcW > source.width) srcW = source.width - srcX;
    if (srcY + srcH > source.height) srcH = source.height - srcY;

    for (int iy = 0; iy < srcH; ++iy)
    {
        for (int ix = 0; ix < srcW; ++ix)
        {
            const int dstX = x + ix;
            const int dstY = y + iy;
            if (dstX < 0 || dstX >= width || dstY < 0 || dstY >= height)
                continue;

            const Color srcColor = source.GetPixelColor(static_cast<u32>(srcX + ix), static_cast<u32>(srcY + iy));
            BlendPixel(static_cast<u32>(dstX), static_cast<u32>(dstY), srcColor, opacity, mode);
        }
    }
}

void BuImage::ReplaceColor(const Color &from, const Color &to, float threshold)
{
    if (!pixels) return;
    
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            Color c = GetPixelColor(x, y);
            
            // Calcular diferença de cor
            float dr = abs(c.r - from.r) / 255.0f;
            float dg = abs(c.g - from.g) / 255.0f;
            float db = abs(c.b - from.b) / 255.0f;
            float da = abs(c.a - from.a) / 255.0f;
            
            float diff = (dr + dg + db + da) / 4.0f;
            
            if (diff <= threshold)
            {
                SetPixel(x, y, to.r, to.g, to.b, to.a);
            }
        }
    }
}

void BuImage::SetColorKey(const Color &key, float threshold)
{
    if (!pixels) return;
    
    // Se não tem alpha, converter para RGBA
    if (components != 4 && components != 2)
    {
        fprintf(stderr, "[PIXMAP] SetColorKey requires alpha channel");
        return;
    }
    
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            Color c = GetPixelColor(x, y);
            
            // Calcular distância da cor key
            float dr = abs(c.r - key.r) / 255.0f;
            float dg = abs(c.g - key.g) / 255.0f;
            float db = abs(c.b - key.b) / 255.0f;
            
            float diff = (dr + dg + db) / 3.0f;
            
            if (diff <= threshold)
            {
                // Tornar transparente
                SetPixel(x, y, c.r, c.g, c.b, 0);
            }
        }
    }
}

void BuImage::DrawRect(int x, int y, int w, int h, const Color &color, bool fill)
{
    if (!pixels) return;
    
    if (fill)
    {
        // Preencher retângulo
        for (int dy = 0; dy < h; dy++)
        {
            for (int dx = 0; dx < w; dx++)
            {
                int px = x + dx;
                int py = y + dy;
                
                if (px >= 0 && px < width && py >= 0 && py < height)
                {
                    SetPixel(px, py, color.r, color.g, color.b, color.a);
                }
            }
        }
    }
    else
    {
        // Desenhar apenas bordas
        // Linha superior
        for (int dx = 0; dx < w; dx++)
        {
            if (x + dx >= 0 && x + dx < width && y >= 0 && y < height)
                SetPixel(x + dx, y, color.r, color.g, color.b, color.a);
        }
        
        // Linha inferior
        for (int dx = 0; dx < w; dx++)
        {
            if (x + dx >= 0 && x + dx < width && y + h - 1 >= 0 && y + h - 1 < height)
                SetPixel(x + dx, y + h - 1, color.r, color.g, color.b, color.a);
        }
        
        // Linha esquerda
        for (int dy = 0; dy < h; dy++)
        {
            if (x >= 0 && x < width && y + dy >= 0 && y + dy < height)
                SetPixel(x, y + dy, color.r, color.g, color.b, color.a);
        }
        
        // Linha direita
        for (int dy = 0; dy < h; dy++)
        {
            if (x + w - 1 >= 0 && x + w - 1 < width && y + dy >= 0 && y + dy < height)
                SetPixel(x + w - 1, y + dy, color.r, color.g, color.b, color.a);
        }
    }
}

void BuImage::DrawCircle(int cx, int cy, int radius, const Color &color, bool fill)
{
    if (!pixels) return;
    
    if (fill)
    {
        // Círculo preenchido
        int radiusSq = radius * radius;
        
        for (int y = -radius; y <= radius; y++)
        {
            for (int x = -radius; x <= radius; x++)
            {
                if (x * x + y * y <= radiusSq)
                {
                    int px = cx + x;
                    int py = cy + y;
                    
                    if (px >= 0 && px < width && py >= 0 && py < height)
                    {
                        SetPixel(px, py, color.r, color.g, color.b, color.a);
                    }
                }
            }
        }
    }
    else
    {
        // Círculo outline (Bresenham)
        int x = 0;
        int y = radius;
        int d = 3 - 2 * radius;
        
        auto drawCirclePoints = [&](int px, int py)
        {
            if (px >= 0 && px < width && py >= 0 && py < height)
                SetPixel(px, py, color.r, color.g, color.b, color.a);
        };
        
        while (x <= y)
        {
            // Desenhar 8 pontos simétricos
            drawCirclePoints(cx + x, cy + y);
            drawCirclePoints(cx - x, cy + y);
            drawCirclePoints(cx + x, cy - y);
            drawCirclePoints(cx - x, cy - y);
            drawCirclePoints(cx + y, cy + x);
            drawCirclePoints(cx - y, cy + x);
            drawCirclePoints(cx + y, cy - x);
            drawCirclePoints(cx - y, cy - x);
            
            if (d < 0)
            {
                d = d + 4 * x + 6;
            }
            else
            {
                d = d + 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
    }
}


void BuImage::DrawPixmap(const BuImage &source, int x, int y)
{
    if (!pixels || !source.pixels) return;
    
    for (int sy = 0; sy < source.height; sy++)
    {
        for (int sx = 0; sx < source.width; sx++)
        {
            int dx = x + sx;
            int dy = y + sy;
            
            if (dx >= 0 && dx < width && dy >= 0 && dy < height)
            {
                Color c = source.GetPixelColor(sx, sy);
                
                // Se source tem alpha, fazer blend
                if (source.components == 4 || source.components == 2)
                {
                    if (c.a == 255)
                    {
                        SetPixel(dx, dy, c.r, c.g, c.b, c.a);
                    }
                    else if (c.a > 0)
                    {
                        // Alpha blend
                        Color dst = GetPixelColor(dx, dy);
                        float alpha = c.a / 255.0f;
                        
                        u8 r = (u8)(c.r * alpha + dst.r * (1.0f - alpha));
                        u8 g = (u8)(c.g * alpha + dst.g * (1.0f - alpha));
                        u8 b = (u8)(c.b * alpha + dst.b * (1.0f - alpha));
                        
                        SetPixel(dx, dy, r, g, b, dst.a);
                    }
                }
                else
                {
                    SetPixel(dx, dy, c.r, c.g, c.b, 255);
                }
            }
        }
    }
}

// Versão com source rect
void BuImage::DrawPixmap(const BuImage &source, int x, int y, const IntRect &srcRect)
{
    if (!pixels || !source.pixels) return;
    
    int srcX = srcRect.x;
    int srcY = srcRect.y;
    int srcW = srcRect.width;
    int srcH = srcRect.height;
    
    // Clamp source rect
    if (srcX < 0) { srcW += srcX; x -= srcX; srcX = 0; }
    if (srcY < 0) { srcH += srcY; y -= srcY; srcY = 0; }
    if (srcX + srcW > source.width) srcW = source.width - srcX;
    if (srcY + srcH > source.height) srcH = source.height - srcY;
    
    for (int sy = 0; sy < srcH; sy++)
    {
        for (int sx = 0; sx < srcW; sx++)
        {
            int dx = x + sx;
            int dy = y + sy;
            
            if (dx >= 0 && dx < width && dy >= 0 && dy < height)
            {
                Color c = source.GetPixelColor(srcX + sx, srcY + sy);
                
                // Alpha blend se necessário
                if (source.components == 4 || source.components == 2)
                {
                    if (c.a == 255)
                    {
                        SetPixel(dx, dy, c.r, c.g, c.b, c.a);
                    }
                    else if (c.a > 0)
                    {
                        Color dst = GetPixelColor(dx, dy);
                        float alpha = c.a / 255.0f;
                        
                        u8 r = (u8)(c.r * alpha + dst.r * (1.0f - alpha));
                        u8 g = (u8)(c.g * alpha + dst.g * (1.0f - alpha));
                        u8 b = (u8)(c.b * alpha + dst.b * (1.0f - alpha));
                        
                        SetPixel(dx, dy, r, g, b, dst.a);
                    }
                }
                else
                {
                    SetPixel(dx, dy, c.r, c.g, c.b, 255);
                }
            }
        }
    }
}

// Box Blur
BuImage* BuImage::ApplyBlur(int radius) const
{
    if (!pixels || radius < 1) return nullptr;
    
    BuImage* result = new BuImage(width, height, components);
    
    int kernelSize = radius * 2 + 1;
    int kernelArea = kernelSize * kernelSize;
    
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int r = 0, g = 0, b = 0, a = 0;
            
            for (int ky = -radius; ky <= radius; ky++)
            {
                for (int kx = -radius; kx <= radius; kx++)
                {
                    int sx = x + kx;
                    int sy = y + ky;
                    
                    // Clamp aos limites
                    sx = (sx < 0) ? 0 : (sx >= width ? width - 1 : sx);
                    sy = (sy < 0) ? 0 : (sy >= height ? height - 1 : sy);
                    
                    Color c = GetPixelColor(sx, sy);
                    r += c.r;
                    g += c.g;
                    b += c.b;
                    a += c.a;
                }
            }
            
            result->SetPixel(x, y, r / kernelArea, g / kernelArea, 
                           b / kernelArea, a / kernelArea);
        }
    }
    
    return result;
}

// Gaussian Blur (mais suave)
BuImage* BuImage::ApplyGaussianBlur(int radius) const
{
    if (!pixels || radius < 1) return nullptr;
    
    BuImage* result = new BuImage(width, height, components);
    
    // Kernel gaussiano 3x3 simples
    float kernel[3][3] = {
        {1.0f/16, 2.0f/16, 1.0f/16},
        {2.0f/16, 4.0f/16, 2.0f/16},
        {1.0f/16, 2.0f/16, 1.0f/16}
    };
    
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float r = 0, g = 0, b = 0, a = 0;
            
            for (int ky = -1; ky <= 1; ky++)
            {
                for (int kx = -1; kx <= 1; kx++)
                {
                    int sx = x + kx;
                    int sy = y + ky;
                    
                    sx = (sx < 0) ? 0 : (sx >= width ? width - 1 : sx);
                    sy = (sy < 0) ? 0 : (sy >= height ? height - 1 : sy);
                    
                    Color c = GetPixelColor(sx, sy);
                    float weight = kernel[ky + 1][kx + 1];
                    
                    r += c.r * weight;
                    g += c.g * weight;
                    b += c.b * weight;
                    a += c.a * weight;
                }
            }
            
            result->SetPixel(x, y, (u8)r, (u8)g, (u8)b, (u8)a);
        }
    }
    
    return result;
}

// Sharpen
BuImage* BuImage::ApplySharpen() const
{
    if (!pixels) return nullptr;
    
    BuImage* result = new BuImage(width, height, components);
    
    // Kernel de sharpening
    float kernel[3][3] = {
        { 0, -1,  0},
        {-1,  5, -1},
        { 0, -1,  0}
    };
    
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float r = 0, g = 0, b = 0;
            
            for (int ky = -1; ky <= 1; ky++)
            {
                for (int kx = -1; kx <= 1; kx++)
                {
                    int sx = x + kx;
                    int sy = y + ky;
                    
                    sx = (sx < 0) ? 0 : (sx >= width ? width - 1 : sx);
                    sy = (sy < 0) ? 0 : (sy >= height ? height - 1 : sy);
                    
                    Color c = GetPixelColor(sx, sy);
                    float weight = kernel[ky + 1][kx + 1];
                    
                    r += c.r * weight;
                    g += c.g * weight;
                    b += c.b * weight;
                }
            }
            
            // Clamp valores
            r = (r < 0) ? 0 : (r > 255 ? 255 : r);
            g = (g < 0) ? 0 : (g > 255 ? 255 : g);
            b = (b < 0) ? 0 : (b > 255 ? 255 : b);
            
            Color original = GetPixelColor(x, y);
            result->SetPixel(x, y, (u8)r, (u8)g, (u8)b, original.a);
        }
    }
    
    return result;
}

// Edge Detection (Sobel)
BuImage* BuImage::ApplyEdgeDetection() const
{
    if (!pixels) return nullptr;
    
    BuImage* result = new BuImage(width, height, components);
    
    // Sobel kernels
    float sobelX[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    
    float sobelY[3][3] = {
        {-1, -2, -1},
        { 0,  0,  0},
        { 1,  2,  1}
    };
    
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float gx = 0, gy = 0;
            
            for (int ky = -1; ky <= 1; ky++)
            {
                for (int kx = -1; kx <= 1; kx++)
                {
                    int sx = x + kx;
                    int sy = y + ky;
                    
                    sx = (sx < 0) ? 0 : (sx >= width ? width - 1 : sx);
                    sy = (sy < 0) ? 0 : (sy >= height ? height - 1 : sy);
                    
                    Color c = GetPixelColor(sx, sy);
                    // Converter para grayscale
                    float gray = c.r * 0.299f + c.g * 0.587f + c.b * 0.114f;
                    
                    gx += gray * sobelX[ky + 1][kx + 1];
                    gy += gray * sobelY[ky + 1][kx + 1];
                }
            }
            
            float magnitude = sqrt(gx * gx + gy * gy);
            magnitude = (magnitude > 255) ? 255 : magnitude;
            
            u8 edge = (u8)magnitude;
            result->SetPixel(x, y, edge, edge, edge, 255);
        }
    }
    
    return result;
}

// Emboss
BuImage* BuImage::ApplyEmboss() const
{
    if (!pixels) return nullptr;
    
    BuImage* result = new BuImage(width, height, components);
    
    // Kernel de emboss
    float kernel[3][3] = {
        {-2, -1,  0},
        {-1,  1,  1},
        { 0,  1,  2}
    };
    
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float r = 0, g = 0, b = 0;
            
            for (int ky = -1; ky <= 1; ky++)
            {
                for (int kx = -1; kx <= 1; kx++)
                {
                    int sx = x + kx;
                    int sy = y + ky;
                    
                    sx = (sx < 0) ? 0 : (sx >= width ? width - 1 : sx);
                    sy = (sy < 0) ? 0 : (sy >= height ? height - 1 : sy);
                    
                    Color c = GetPixelColor(sx, sy);
                    float weight = kernel[ky + 1][kx + 1];
                    
                    r += c.r * weight;
                    g += c.g * weight;
                    b += c.b * weight;
                }
            }
            
            // Adicionar 128 para centrar o resultado
            r = r + 128;
            g = g + 128;
            b = b + 128;
            
            r = (r < 0) ? 0 : (r > 255 ? 255 : r);
            g = (g < 0) ? 0 : (g > 255 ? 255 : g);
            b = (b < 0) ? 0 : (b > 255 ? 255 : b);
            
            Color original = GetPixelColor(x, y);
            result->SetPixel(x, y, (u8)r, (u8)g, (u8)b, original.a);
        }
    }
    
    return result;
}
 
BuImage* BuImage::Crop(const IntRect &rect) const
{
    if (!pixels) 
    {
        fprintf(stderr, "[PIXMAP] Cannot crop: pixels is null");
        return nullptr;
    }
    
    // Validar e ajustar rect aos limites da imagem
    int x = rect.x;
    int y = rect.y;
    int w = rect.width;
    int h = rect.height;
    
    // Clamp aos limites
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= width || y >= height)
    {
        fprintf(stderr, "[PIXMAP] Crop rect is outside image bounds");
        return nullptr;
    }
    if (x + w > width) w = width - x;
    if (y + h > height) h = height - y;
    
    // Verificar se o rect resultante é válido
    if (w <= 0 || h <= 0)
    {
        fprintf(stderr, "[PIXMAP] Invalid crop rect: width=%d, height=%d", w, h);
        return nullptr;
    }
    
    // Criar nova pixmap
    BuImage* result = new BuImage(w, h, components);
    if (!result || !result->pixels)
    {
        fprintf(stderr, "[PIXMAP] Failed to create cropped pixmap");
        if (result) delete result;
        return nullptr;
    }
    
    // Copiar dados linha por linha (otimizado)
    int bytesPerRow = w * components;
    
    for (int row = 0; row < h; row++)
    {
        unsigned char* srcRow = pixels + ((y + row) * width + x) * components;
        unsigned char* dstRow = result->pixels + row * bytesPerRow;
        memcpy(dstRow, srcRow, bytesPerRow);
    }
    
    return result;
}
 
BuImage* BuImage::Crop(int x, int y, int w, int h) const
{
    IntRect rect;
    rect.x = x;
    rect.y = y;
    rect.width = w;
    rect.height = h;
    
    return Crop(rect);
}

 
BuImage* BuImage::CropExtended(const IntRect &rect, bool fillTransparent) const
{
    if (!pixels) 
    {
        fprintf(stderr, "[PIXMAP] Cannot crop: pixels is null");
        return nullptr;
    }
    
    int x = rect.x;
    int y = rect.y;
    int w = rect.width;
    int h = rect.height;
    
    if (w <= 0 || h <= 0)
    {
        fprintf(stderr, "[PIXMAP] Invalid crop dimensions: width=%d, height=%d", w, h);
        return nullptr;
    }
    
    // Criar nova pixmap
    BuImage* result = new BuImage(w, h, components);
    if (!result || !result->pixels)
    {
        fprintf(stderr, "[PIXMAP] Failed to create cropped pixmap");
        if (result) delete result;
        return nullptr;
    }
    
    // Preencher com cor padrão
    if (fillTransparent && (components == 4 || components == 2))
    {
        result->Clear(); // Preencher com transparente (0,0,0,0)
    }
    else
    {
        result->Fill(0, 0, 0, 255); // Preencher com preto opaco
    }
    
    // Calcular região de overlap (área que realmente existe na imagem fonte)
    int srcStartX = std::max(0, x);
    int srcStartY = std::max(0, y);
    int srcEndX = std::min(width, x + w);
    int srcEndY = std::min(height, y + h);
    
    // Se não há overlap, retornar imagem vazia preenchida
    if (srcStartX >= srcEndX || srcStartY >= srcEndY)
    {
        return result;
    }
    
    // Calcular offset no destino
    int dstOffsetX = srcStartX - x;
    int dstOffsetY = srcStartY - y;
    
    // Dimensões da área a copiar
    int copyWidth = srcEndX - srcStartX;
    int copyHeight = srcEndY - srcStartY;
    
    // Copiar linha por linha (otimizado)
    int bytesPerRow = copyWidth * components;
    
    for (int row = 0; row < copyHeight; row++)
    {
        unsigned char* srcRow = pixels + ((srcStartY + row) * width + srcStartX) * components;
        unsigned char* dstRow = result->pixels + ((dstOffsetY + row) * w + dstOffsetX) * components;
        memcpy(dstRow, srcRow, bytesPerRow);
    }
    
    return result;
}

} // namespace BuGUI
