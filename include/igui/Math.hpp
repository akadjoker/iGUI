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

struct Rect
{
    float x;
    float y;
    float width;
    float height;

    Rect() : x(0.0f), y(0.0f), width(0.0f), height(0.0f) {}
    Rect(float xValue, float yValue, float widthValue, float heightValue)
        : x(xValue), y(yValue), width(widthValue), height(heightValue)
    {
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
