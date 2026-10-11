#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  RetainedBase.hpp — self-contained foundation for the ig::retained widget layer.
//  Zero external dependencies beyond <cstdint>.
// ═════════════════════════════════════════════════════════════════════════════

#include <cstdint>
#include <cmath>

#include <igui/Color.hpp>

// ── Raw integer typedefs (global, used everywhere) ───────────────────────────
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;
typedef float     f32;
typedef double    f64;

// ── Platform detection ───────────────────────────────────────────────────────
#if defined(_WIN32) || defined(_WINDOWS)
#  define BUGUI_PLATFORM_WINDOWS
#elif defined(__ANDROID__)
#  define BUGUI_PLATFORM_ANDROID
#elif defined(__EMSCRIPTEN__)
#  define BUGUI_PLATFORM_WEB
#else
#  define BUGUI_PLATFORM_LINUX
#endif

// ─────────────────────────────────────────────────────────────────────────────
namespace ig { namespace retained {
// ─────────────────────────────────────────────────────────────────────────────

// ══════════════════════════════════════════════════════════════════════════════
//  ig::retained::Math — scalar helpers and constants
// ══════════════════════════════════════════════════════════════════════════════
namespace Math {

constexpr float Pi      = 3.14159265358979323846f;
constexpr float TwoPi   = Pi * 2.0f;
constexpr float HalfPi  = Pi * 0.5f;
constexpr float Deg2Rad = Pi / 180.0f;
constexpr float Rad2Deg = 180.0f / Pi;
constexpr float Epsilon = 1e-6f;

inline float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int   clamp(int v,   int lo,   int hi)   { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerp(float a, float b, float t)    { return a + (b - a) * t; }
inline float min2(float a, float b)             { return a < b ? a : b; }
inline float max2(float a, float b)             { return a > b ? a : b; }
inline int   min2(int a, int b)                 { return a < b ? a : b; }
inline int   max2(int a, int b)                 { return a > b ? a : b; }
inline float abs2(float v)                      { return v < 0.f ? -v : v; }
inline float sign(float v)                      { return v < 0.f ? -1.f : (v > 0.f ? 1.f : 0.f); }
inline float toRadians(float deg)               { return deg * Deg2Rad; }
inline float toDegrees(float rad)               { return rad * Rad2Deg; }

} // namespace Math

// Bring scalar helpers into ig::retained scope for convenience
using Math::clamp;
using Math::lerp;
using Math::min2;
using Math::max2;

// ══════════════════════════════════════════════════════════════════════════════
//  Vec2f / Vec3f / Vec4f — simple float vectors, no glm dependency.
//  Widgets pass these to backends; backends may convert to glm internally.
// ══════════════════════════════════════════════════════════════════════════════

struct Vec2f
{
    float x = 0, y = 0;
    Vec2f() = default;
    Vec2f(float x, float y) : x(x), y(y) {}
    Vec2f operator+(const Vec2f& o) const { return {x+o.x, y+o.y}; }
    Vec2f operator-(const Vec2f& o) const { return {x-o.x, y-o.y}; }
    Vec2f operator*(float s)        const { return {x*s,   y*s};   }
    Vec2f operator-()               const { return {-x, -y}; }
    Vec2f& operator+=(const Vec2f& o)    { x+=o.x; y+=o.y; return *this; }
    Vec2f& operator-=(const Vec2f& o)    { x-=o.x; y-=o.y; return *this; }
    float  length()    const { return std::sqrt(x*x + y*y); }
    Vec2f  normalized()const { float l=length(); return l>0.f?Vec2f{x/l,y/l}:Vec2f{}; }
    float  dot(const Vec2f& o) const { return x*o.x + y*o.y; }
    bool   operator==(const Vec2f& o) const { return x==o.x && y==o.y; }
    bool   operator!=(const Vec2f& o) const { return !(*this==o); }
};

struct Vec3f
{
    float x = 0, y = 0, z = 0;
    Vec3f() = default;
    Vec3f(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3f operator+(const Vec3f& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3f operator-(const Vec3f& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3f operator*(float s)        const { return {x*s,   y*s,   z*s};   }
    Vec3f operator-()               const { return {-x, -y, -z}; }
    Vec3f& operator+=(const Vec3f& o)    { x+=o.x; y+=o.y; z+=o.z; return *this; }
    Vec3f& operator-=(const Vec3f& o)    { x-=o.x; y-=o.y; z-=o.z; return *this; }
    float  length()    const { return std::sqrt(x*x + y*y + z*z); }
    Vec3f  normalized()const { float l=length(); return l>0.f?Vec3f{x/l,y/l,z/l}:Vec3f{}; }
    float  dot(const Vec3f& o)   const { return x*o.x + y*o.y + z*o.z; }
    Vec3f  cross(const Vec3f& o) const {
        return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
    }
    bool operator==(const Vec3f& o) const { return x==o.x && y==o.y && z==o.z; }
    bool operator!=(const Vec3f& o) const { return !(*this==o); }
};

struct Vec4f
{
    float x = 0, y = 0, z = 0, w = 0;
    Vec4f() = default;
    Vec4f(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4f(Vec3f v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}
    Vec4f operator+(const Vec4f& o) const { return {x+o.x, y+o.y, z+o.z, w+o.w}; }
    Vec4f operator*(float s)        const { return {x*s,   y*s,   z*s,   w*s};   }
    bool operator==(const Vec4f& o) const { return x==o.x && y==o.y && z==o.z && w==o.w; }
    bool operator!=(const Vec4f& o) const { return !(*this==o); }
};

// ══════════════════════════════════════════════════════════════════════════════
//  Mat4f — column-major 4×4 float matrix (matches OpenGL/glm layout).
//  Widgets pass this to backends; backends interpret/compose with glm.
// ══════════════════════════════════════════════════════════════════════════════

struct Mat4f
{
    float data[16];   // column-major: col0=[0..3], col1=[4..7], col2=[8..11], col3=[12..15]

    Mat4f() { setIdentity(); }
    explicit Mat4f(const float* src) { for(int i=0;i<16;i++) data[i]=src[i]; }

    void setIdentity()
    {
        data[ 0]=1; data[ 1]=0; data[ 2]=0; data[ 3]=0;
        data[ 4]=0; data[ 5]=1; data[ 6]=0; data[ 7]=0;
        data[ 8]=0; data[ 9]=0; data[10]=1; data[11]=0;
        data[12]=0; data[13]=0; data[14]=0; data[15]=1;
    }

    const float* ptr() const { return data; }
    float*       ptr()       { return data; }

    // Access element at row r, col c
    float  operator()(int r, int c) const { return data[c*4+r]; }
    float& operator()(int r, int c)       { return data[c*4+r]; }

    Mat4f operator*(const Mat4f& b) const
    {
        Mat4f out;
        for (int col=0; col<4; col++)
            for (int row=0; row<4; row++) {
                float s = 0;
                for (int k=0; k<4; k++) s += (*this)(row,k) * b(k,col);
                out(row,col) = s;
            }
        return out;
    }

    Vec4f operator*(const Vec4f& v) const
    {
        return {
            data[0]*v.x + data[4]*v.y + data[ 8]*v.z + data[12]*v.w,
            data[1]*v.x + data[5]*v.y + data[ 9]*v.z + data[13]*v.w,
            data[2]*v.x + data[6]*v.y + data[10]*v.z + data[14]*v.w,
            data[3]*v.x + data[7]*v.y + data[11]*v.z + data[15]*v.w,
        };
    }

    static Mat4f identity() { return Mat4f{}; }
};

using Color = ::ig::Color;

// ══════════════════════════════════════════════════════════════════════════════
//  PixelFormat
// ══════════════════════════════════════════════════════════════════════════════

enum class PixelFormat
{
    Grayscale = 1,
    GrayAlpha = 2,
    RGB       = 3,
    RGBA      = 4,
};

// ══════════════════════════════════════════════════════════════════════════════
//  Rectangle<T>
// ══════════════════════════════════════════════════════════════════════════════

template <typename T>
struct Rectangle
{
    T x = 0, y = 0, width = 0, height = 0;

    Rectangle() = default;
    Rectangle(T x, T y, T width, T height) : x(x), y(y), width(width), height(height) {}

    T left()   const { return x; }
    T top()    const { return y; }
    T right()  const { return x + width; }
    T bottom() const { return y + height; }

    bool contains(T px, T py) const
    {
        return px >= x && px < x + width && py >= y && py < y + height;
    }

    bool intersects(const Rectangle& o) const
    {
        return x < o.x + o.width  && x + width  > o.x &&
               y < o.y + o.height && y + height > o.y;
    }

    bool operator==(const Rectangle& o) const
    {
        return x == o.x && y == o.y && width == o.width && height == o.height;
    }
    bool operator!=(const Rectangle& o) const { return !(*this == o); }
};

using IntRect   = Rectangle<int>;
using FloatRect = Rectangle<float>;

// ─────────────────────────────────────────────────────────────────────────────
} // namespace retained
} // namespace ig
// ─────────────────────────────────────────────────────────────────────────────

// All types (Color, Vec2f, IntRect, …) live in namespace ig { namespace retained.
// Consumer code: add  using namespace ig::retained;  or qualify with  ig::retained::
