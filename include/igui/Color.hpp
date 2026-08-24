#pragma once

#include <stdint.h>

namespace ig
{

struct Color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    Color() : r(255), g(255), b(255), a(255) {}
    Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha)
    {
    }

    explicit Color(uint32_t rgba)
        : r(static_cast<uint8_t>((rgba >> 24) & 0xffu)),
          g(static_cast<uint8_t>((rgba >> 16) & 0xffu)),
          b(static_cast<uint8_t>((rgba >> 8) & 0xffu)),
          a(static_cast<uint8_t>(rgba & 0xffu))
    {
    }

    void Set(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
    {
        r = red;
        g = green;
        b = blue;
        a = alpha;
    }

    Color Lerp(const Color &other, float amount) const
    {
        const float inverse = 1.0f - amount;
        return Color(static_cast<uint8_t>(r * inverse + other.r * amount),
                     static_cast<uint8_t>(g * inverse + other.g * amount),
                     static_cast<uint8_t>(b * inverse + other.b * amount),
                     static_cast<uint8_t>(a * inverse + other.a * amount));
    }

    static Color FromFloat(float red, float green, float blue, float alpha = 1.0f)
    {
        return Color(static_cast<uint8_t>(red * 255.0f),
                     static_cast<uint8_t>(green * 255.0f),
                     static_cast<uint8_t>(blue * 255.0f),
                     static_cast<uint8_t>(alpha * 255.0f));
    }

    uint32_t ToRGBA() const
    {
        return (static_cast<uint32_t>(r) << 24) | (static_cast<uint32_t>(g) << 16) |
               (static_cast<uint32_t>(b) << 8) | static_cast<uint32_t>(a);
    }
    uint32_t ToUInt() const { return ToRGBA(); }

    bool operator==(const Color &other) const
    {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
    bool operator!=(const Color &other) const { return !(*this == other); }

    static Color FromHSV(float hue, float saturation, float value, float alpha = 1.0f)
    {
        float red = value;
        float green = value;
        float blue = value;
        if (saturation > 0.0f)
        {
            hue /= 60.0f;
            const int sector = static_cast<int>(hue);
            const float fraction = hue - static_cast<float>(sector);
            const float p = value * (1.0f - saturation);
            const float q = value * (1.0f - saturation * fraction);
            const float t = value * (1.0f - saturation * (1.0f - fraction));
            switch (sector % 6)
            {
            case 0: red = value; green = t; blue = p; break;
            case 1: red = q; green = value; blue = p; break;
            case 2: red = p; green = value; blue = t; break;
            case 3: red = p; green = q; blue = value; break;
            case 4: red = t; green = p; blue = value; break;
            default: red = value; green = p; blue = q; break;
            }
        }
        return FromFloat(red, green, blue, alpha);
    }

    void ToHSV(float &hue, float &saturation, float &value) const
    {
        const float red = r / 255.0f;
        const float green = g / 255.0f;
        const float blue = b / 255.0f;
        const float maximum = red > green ? (red > blue ? red : blue) : (green > blue ? green : blue);
        const float minimum = red < green ? (red < blue ? red : blue) : (green < blue ? green : blue);
        const float delta = maximum - minimum;
        value = maximum;
        saturation = maximum > 0.00001f ? delta / maximum : 0.0f;
        if (delta < 0.00001f)
        {
            hue = 0.0f;
            return;
        }
        if (maximum == red)
            hue = 60.0f * (green - blue) / delta + (green < blue ? 360.0f : 0.0f);
        else if (maximum == green)
            hue = 60.0f * (blue - red) / delta + 120.0f;
        else
            hue = 60.0f * (red - green) / delta + 240.0f;
    }
};

} // namespace ig
