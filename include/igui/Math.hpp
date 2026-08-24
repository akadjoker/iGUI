#pragma once

namespace ig
{

struct Vec2
{
    float x;
    float y;

    Vec2() : x(0.0f), y(0.0f) {}
    Vec2(float xValue, float yValue) : x(xValue), y(yValue) {}
};

struct Vec3
{
    float x;
    float y;
    float z;

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float xValue, float yValue, float zValue) : x(xValue), y(yValue), z(zValue) {}
};

struct Rect
{
    float x;
    float y;
    union { float width; float w; };
    union { float height; float h; };

    Rect() : x(0.0f), y(0.0f), width(0.0f), height(0.0f) {}
    Rect(float xValue, float yValue, float widthValue, float heightValue)
        : x(xValue), y(yValue), width(widthValue), height(heightValue)
    {
    }

    float right() const { return x + width; }
    float bottom() const { return y + height; }
    bool contains(float pointX, float pointY) const
    {
        return pointX >= x && pointX < right() && pointY >= y && pointY < bottom();
    }
    Rect shrunk(float padding) const
    {
        return Rect(x + padding, y + padding, width - padding * 2.0f,
                    height - padding * 2.0f);
    }
};

inline float clamp(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

inline bool contains(const Rect &rect, const Vec2 &point)
{
    return point.x >= rect.x && point.y >= rect.y &&
           point.x < rect.x + rect.width && point.y < rect.y + rect.height;
}

inline Rect intersect(const Rect &a, const Rect &b)
{
    const float left = a.x > b.x ? a.x : b.x;
    const float top = a.y > b.y ? a.y : b.y;
    const float rightA = a.x + a.width;
    const float rightB = b.x + b.width;
    const float bottomA = a.y + a.height;
    const float bottomB = b.y + b.height;
    const float right = rightA < rightB ? rightA : rightB;
    const float bottom = bottomA < bottomB ? bottomA : bottomB;
    return Rect(left, top, right > left ? right - left : 0.0f,
                bottom > top ? bottom - top : 0.0f);
}

} // namespace ig
