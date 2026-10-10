#include "igui/Gui.hpp"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

#include <ct/sort.hpp>

namespace ig
{

namespace
{

const float GizmoPi = 3.14159265358979323846f;

// A one pixel border drawn as two nested rounded fills: cheaper than stroking
// the outline segment by segment, and both silhouettes are anti-aliased.
void drawFrame(DrawList &list, const Rect &rect, const Color &fill, const Color &border,
               float radius, const Rect &clip)
{
    if (border.a == 0u || rect.width <= 2.0f || rect.height <= 2.0f)
    {
        list.addRectFilledRounded(rect, radius, fill, clip);
        return;
    }
    list.addRectFilledRounded(rect, radius, border, clip);
    list.addRectFilledRounded(Rect(rect.x + 1.0f, rect.y + 1.0f, rect.width - 2.0f, rect.height - 2.0f),
                              radius > 1.0f ? radius - 1.0f : 0.0f, fill, clip);
}

enum GizmoAxis2D : uint8_t
{
    GizmoAxisNone,
    GizmoAxisX,
    GizmoAxisY,
    GizmoAxisXY
};

enum GizmoAxis3D : uint8_t
{
    Gizmo3DAxisNone,
    Gizmo3DAxisX,
    Gizmo3DAxisY,
    Gizmo3DAxisZ,
    Gizmo3DAxisXYZ,
    Gizmo3DAxisXY,
    Gizmo3DAxisXZ,
    Gizmo3DAxisYZ
};

struct Gizmo3DProjector
{
    Rect viewport;
    const float *view;
    const float *projection;
};

float snapGizmoValue(float value, float increment)
{
    return increment > 0.0f ? roundf(value / increment) * increment : value;
}

float absoluteValue(float value)
{
    return value < 0.0f ? -value : value;
}

GizmoAxis2D hitGizmo2D(Gizmo2DMode mode, const Vec2 &pointer, const Vec2 &center,
                       const Vec2 &axisX, const Vec2 &axisY, float axisLength)
{
    const float deltaX = pointer.x - center.x;
    const float deltaY = pointer.y - center.y;
    if (mode == Gizmo2DMode::Rotate)
    {
        const float distance = sqrtf(deltaX * deltaX + deltaY * deltaY);
        return absoluteValue(distance - axisLength * 0.82f) <= 8.0f ? GizmoAxisXY : GizmoAxisNone;
    }

    const float localX = deltaX * axisX.x + deltaY * axisX.y;
    const float localY = deltaX * axisY.x + deltaY * axisY.y;
    if (absoluteValue(localX) <= 9.0f && absoluteValue(localY) <= 9.0f)
        return GizmoAxisXY;
    if (localX >= 8.0f && localX <= axisLength + 10.0f && absoluteValue(localY) <= 8.0f)
        return GizmoAxisX;
    if (localY >= 8.0f && localY <= axisLength + 10.0f && absoluteValue(localX) <= 8.0f)
        return GizmoAxisY;
    return GizmoAxisNone;
}

Vec3 addGizmo3D(const Vec3 &a, const Vec3 &b)
{
    return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec3 scaleGizmo3D(const Vec3 &value, float scale)
{
    return Vec3(value.x * scale, value.y * scale, value.z * scale);
}

Vec3 gizmo3DAxisVector(GizmoAxis3D axis)
{
    if (axis == Gizmo3DAxisX)
        return Vec3(1.0f, 0.0f, 0.0f);
    if (axis == Gizmo3DAxisY)
        return Vec3(0.0f, 1.0f, 0.0f);
    return Vec3(0.0f, 0.0f, 1.0f);
}

bool projectGizmo3D(const Gizmo3DProjector &projector, const Vec3 &world, Vec2 &screen)
{
    const float viewX = projector.view[0] * world.x + projector.view[4] * world.y +
                        projector.view[8] * world.z + projector.view[12];
    const float viewY = projector.view[1] * world.x + projector.view[5] * world.y +
                        projector.view[9] * world.z + projector.view[13];
    const float viewZ = projector.view[2] * world.x + projector.view[6] * world.y +
                        projector.view[10] * world.z + projector.view[14];
    const float viewW = projector.view[3] * world.x + projector.view[7] * world.y +
                        projector.view[11] * world.z + projector.view[15];
    const float clipX = projector.projection[0] * viewX + projector.projection[4] * viewY +
                        projector.projection[8] * viewZ + projector.projection[12] * viewW;
    const float clipY = projector.projection[1] * viewX + projector.projection[5] * viewY +
                        projector.projection[9] * viewZ + projector.projection[13] * viewW;
    const float clipW = projector.projection[3] * viewX + projector.projection[7] * viewY +
                        projector.projection[11] * viewZ + projector.projection[15] * viewW;
    if (clipW < 0.00001f && clipW > -0.00001f)
        return false;
    const float inverseW = 1.0f / clipW;
    screen.x = projector.viewport.x + (clipX * inverseW * 0.5f + 0.5f) * projector.viewport.width;
    screen.y = projector.viewport.y + (0.5f - clipY * inverseW * 0.5f) * projector.viewport.height;
    return true;
}

float gizmo3DSegmentDistance(const Vec2 &point, const Vec2 &from, const Vec2 &to)
{
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 0.0001f)
        return 100000.0f;
    float t = ((point.x - from.x) * dx + (point.y - from.y) * dy) / lengthSquared;
    t = clamp(t, 0.0f, 1.0f);
    const float offsetX = from.x + dx * t - point.x;
    const float offsetY = from.y + dy * t - point.y;
    return sqrtf(offsetX * offsetX + offsetY * offsetY);
}

float gizmo3DCross(const Vec2 &a, const Vec2 &b, const Vec2 &point)
{
    return (a.x - point.x) * (b.y - point.y) - (a.y - point.y) * (b.x - point.x);
}

bool gizmo3DPointInQuad(const Vec2 &point, const Vec2 &a, const Vec2 &b,
                        const Vec2 &c, const Vec2 &d)
{
    const float first = gizmo3DCross(a, b, point);
    const float second = gizmo3DCross(b, c, point);
    const float third = gizmo3DCross(c, d, point);
    const float fourth = gizmo3DCross(d, a, point);
    const bool hasNegative = first < 0.0f || second < 0.0f || third < 0.0f || fourth < 0.0f;
    const bool hasPositive = first > 0.0f || second > 0.0f || third > 0.0f || fourth > 0.0f;
    return !(hasNegative && hasPositive);
}

void gizmo3DRingBasis(GizmoAxis3D axis, Vec3 &u, Vec3 &v)
{
    if (axis == Gizmo3DAxisX)
    {
        u = Vec3(0.0f, 1.0f, 0.0f);
        v = Vec3(0.0f, 0.0f, 1.0f);
    }
    else if (axis == Gizmo3DAxisY)
    {
        u = Vec3(1.0f, 0.0f, 0.0f);
        v = Vec3(0.0f, 0.0f, 1.0f);
    }
    else
    {
        u = Vec3(1.0f, 0.0f, 0.0f);
        v = Vec3(0.0f, 1.0f, 0.0f);
    }
}

// ── Ray helpers ──────────────────────────────────────────────────────────────
// A rotation has to be measured on the ring the user grabbed, not on the screen:
// the screen angle of an ellipse has nothing to do with the turn of the model.
// That needs the pointer's ray, and therefore the inverse of the matrices the
// caller handed in, so the immediate core inverts a 4x4 of its own (nothing
// here may use the STL).

void gizmo3DMultiply(const float *left, const float *right, float *out)
{
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
        {
            float sum = 0.0f;
            for (int step = 0; step < 4; ++step)
                sum += left[step * 4 + row] * right[column * 4 + step];
            out[column * 4 + row] = sum;
        }
}

bool gizmo3DInvert(const float *matrix, float *out)
{
    const float c0 = matrix[0], c1 = matrix[1], c2 = matrix[2], c3 = matrix[3];
    const float c4 = matrix[4], c5 = matrix[5], c6 = matrix[6], c7 = matrix[7];
    const float c8 = matrix[8], c9 = matrix[9], c10 = matrix[10], c11 = matrix[11];
    const float c12 = matrix[12], c13 = matrix[13], c14 = matrix[14], c15 = matrix[15];

    const float s0 = c0 * c5 - c1 * c4;
    const float s1 = c0 * c6 - c2 * c4;
    const float s2 = c0 * c7 - c3 * c4;
    const float s3 = c1 * c6 - c2 * c5;
    const float s4 = c1 * c7 - c3 * c5;
    const float s5 = c2 * c7 - c3 * c6;
    const float s6 = c8 * c13 - c9 * c12;
    const float s7 = c8 * c14 - c10 * c12;
    const float s8 = c8 * c15 - c11 * c12;
    const float s9 = c9 * c14 - c10 * c13;
    const float s10 = c9 * c15 - c11 * c13;
    const float s11 = c10 * c15 - c11 * c14;

    const float determinant = s0 * s11 - s1 * s10 + s2 * s9 + s3 * s8 - s4 * s7 + s5 * s6;
    if (absoluteValue(determinant) < 1e-20f)
        return false;
    const float inverse = 1.0f / determinant;

    out[0] = (c5 * s11 - c6 * s10 + c7 * s9) * inverse;
    out[1] = (c2 * s10 - c1 * s11 - c3 * s9) * inverse;
    out[2] = (c13 * s5 - c14 * s4 + c15 * s3) * inverse;
    out[3] = (c10 * s4 - c9 * s5 - c11 * s3) * inverse;
    out[4] = (c6 * s8 - c4 * s11 - c7 * s7) * inverse;
    out[5] = (c0 * s11 - c2 * s8 + c3 * s7) * inverse;
    out[6] = (c14 * s2 - c12 * s5 - c15 * s1) * inverse;
    out[7] = (c8 * s5 - c10 * s2 + c11 * s1) * inverse;
    out[8] = (c4 * s10 - c5 * s8 + c7 * s6) * inverse;
    out[9] = (c1 * s8 - c0 * s10 - c3 * s6) * inverse;
    out[10] = (c12 * s4 - c13 * s2 + c15 * s0) * inverse;
    out[11] = (c9 * s2 - c8 * s4 - c11 * s0) * inverse;
    out[12] = (c5 * s7 - c4 * s9 - c6 * s6) * inverse;
    out[13] = (c0 * s9 - c1 * s7 + c2 * s6) * inverse;
    out[14] = (c13 * s1 - c12 * s3 - c14 * s0) * inverse;
    out[15] = (c8 * s3 - c9 * s1 + c10 * s0) * inverse;
    return true;
}

// The world ray under a pixel of the gizmo's viewport.
bool gizmo3DRayThrough(const Gizmo3DProjector &projector, const Vec2 &pixel,
                       Vec3 &origin, Vec3 &direction)
{
    float combined[16];
    float inverse[16];
    gizmo3DMultiply(projector.projection, projector.view, combined);
    if (!gizmo3DInvert(combined, inverse))
        return false;

    const float horizontal = (pixel.x - projector.viewport.x) / projector.viewport.width * 2.0f - 1.0f;
    const float vertical = 1.0f - (pixel.y - projector.viewport.y) / projector.viewport.height * 2.0f;
    const float nearPoint[4] = {horizontal, vertical, -1.0f, 1.0f};
    const float farPoint[4] = {horizontal, vertical, 1.0f, 1.0f};
    float nearWorld[4];
    float farWorld[4];
    for (int row = 0; row < 4; ++row)
    {
        nearWorld[row] = inverse[row] * nearPoint[0] + inverse[4 + row] * nearPoint[1] +
                         inverse[8 + row] * nearPoint[2] + inverse[12 + row] * nearPoint[3];
        farWorld[row] = inverse[row] * farPoint[0] + inverse[4 + row] * farPoint[1] +
                        inverse[8 + row] * farPoint[2] + inverse[12 + row] * farPoint[3];
    }
    if (absoluteValue(nearWorld[3]) < 1e-8f || absoluteValue(farWorld[3]) < 1e-8f)
        return false;
    const float nearScale = 1.0f / nearWorld[3];
    const float farScale = 1.0f / farWorld[3];
    origin = Vec3(nearWorld[0] * nearScale, nearWorld[1] * nearScale, nearWorld[2] * nearScale);
    const Vec3 far(farWorld[0] * farScale, farWorld[1] * farScale, farWorld[2] * farScale);
    const float dx = far.x - origin.x;
    const float dy = far.y - origin.y;
    const float dz = far.z - origin.z;
    const float length = sqrtf(dx * dx + dy * dy + dz * dz);
    if (length < 1e-8f)
        return false;
    direction = Vec3(dx / length, dy / length, dz / length);
    return true;
}

// The radius of the ring that goes round every handle, which turns the target
// the way the screen is turned. Zero when the handles do not project at all.
float gizmo3DOuterRadius(const Gizmo3DProjector &projector, const Transform3D &transform,
                         float axisLength)
{
    Vec2 center;
    if (!projectGizmo3D(projector, transform.position, center))
        return 0.0f;
    float outer = 0.0f;
    for (uint8_t index = Gizmo3DAxisX; index <= Gizmo3DAxisZ; ++index)
    {
        Vec2 endpoint;
        if (!projectGizmo3D(projector, addGizmo3D(transform.position,
                scaleGizmo3D(gizmo3DAxisVector(static_cast<GizmoAxis3D>(index)), axisLength)), endpoint))
            continue;
        const float dx = endpoint.x - center.x;
        const float dy = endpoint.y - center.y;
        const float radius = sqrtf(dx * dx + dy * dy);
        if (radius > outer)
            outer = radius;
    }
    return outer * 1.15f;
}

// The direction the camera looks, into the scene: the third row of a view
// matrix is the camera's own z axis, which points from the target back to the
// eye.
Vec3 gizmo3DViewDirection(const Gizmo3DProjector &projector)
{
    return Vec3(-projector.view[2], -projector.view[6], -projector.view[10]);
}

// A corner of the box the handles move, in world space: the transform's own
// matrix, with its rotation read as Euler degrees in X, then Y, then Z order -
// the order the handles write it in.
Vec3 gizmo3DModelPoint(const Transform3D &transform, float cornerX, float cornerY,
                       float cornerZ, float halfSize)
{
    const float rotationX = transform.rotation.x * GizmoPi / 180.0f;
    const float rotationY = transform.rotation.y * GizmoPi / 180.0f;
    const float rotationZ = transform.rotation.z * GizmoPi / 180.0f;
    float x = cornerX * halfSize * transform.scale.x;
    float y = cornerY * halfSize * transform.scale.y;
    float z = cornerZ * halfSize * transform.scale.z;
    float turned = y * cosf(rotationX) - z * sinf(rotationX);
    z = y * sinf(rotationX) + z * cosf(rotationX);
    y = turned;
    turned = x * cosf(rotationY) + z * sinf(rotationY);
    z = -x * sinf(rotationY) + z * cosf(rotationY);
    x = turned;
    turned = x * cosf(rotationZ) - y * sinf(rotationZ);
    y = x * sinf(rotationZ) + y * cosf(rotationZ);
    x = turned;
    return Vec3(transform.position.x + x, transform.position.y + y, transform.position.z + z);
}

// The same turn without the scale, for a face normal.
Vec3 gizmo3DModelNormal(const Transform3D &transform, float normalX, float normalY, float normalZ)
{
    Transform3D rotationOnly;
    rotationOnly.rotation = transform.rotation;
    return gizmo3DModelPoint(rotationOnly, normalX, normalY, normalZ, 1.0f);
}

// The box the handles move: the object a modelling tool would show, so a turn,
// a shift or a stretch is visible while it happens. Six quads sorted back to
// front and filled opaque need no depth buffer; the outline is drawn only round
// the faces that face the camera, the way a wireframe overlay hides the edges
// behind the solid.
void drawGizmo3DModel(const Gizmo3DProjector &projector, const Transform3D &transform,
                      float halfSize, const Rect &clip, DrawList *drawList)
{
    Vec2 corners[8];
    float depth[8];
    for (int corner = 0; corner < 8; ++corner)
    {
        const Vec3 world = gizmo3DModelPoint(transform, (corner & 1) ? 1.0f : -1.0f,
                                            (corner & 2) ? 1.0f : -1.0f,
                                            (corner & 4) ? 1.0f : -1.0f, halfSize);
        if (!projectGizmo3D(projector, world, corners[corner]))
            return;
        depth[corner] = projector.view[2] * world.x + projector.view[6] * world.y +
                        projector.view[10] * world.z + projector.view[14];
    }

    const int faces[6][4] = {{0, 2, 6, 4}, {1, 5, 7, 3}, {0, 4, 5, 1},
                             {2, 3, 7, 6}, {0, 1, 3, 2}, {4, 6, 7, 5}};
    const float normals[6][3] = {{-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                                 {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                                 {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}};
    // A lamp just above the eye, so the top of the box reads brighter than its
    // sides and the faces do not all come out the same grey.
    const Vec3 view = gizmo3DViewDirection(projector);
    float lightX = -view.x;
    float lightY = -view.y + 0.75f;
    float lightZ = -view.z;
    const float lightLength = sqrtf(lightX * lightX + lightY * lightY + lightZ * lightZ);
    lightX /= lightLength;
    lightY /= lightLength;
    lightZ /= lightLength;

    const Color base(206u, 209u, 216u, 255u);
    const Color outline(238u, 150u, 62u, 255u);
    int order[6] = {0, 1, 2, 3, 4, 5};
    float faceDepth[6];
    float faceLight[6];
    bool visible[6];
    for (int face = 0; face < 6; ++face)
    {
        const Vec3 normal = gizmo3DModelNormal(transform, normals[face][0], normals[face][1],
                                               normals[face][2]);
        faceDepth[face] = (depth[faces[face][0]] + depth[faces[face][1]] +
                           depth[faces[face][2]] + depth[faces[face][3]]) * 0.25f;
        visible[face] = normal.x * view.x + normal.y * view.y + normal.z * view.z < 0.0f;
        const float lambert = normal.x * lightX + normal.y * lightY + normal.z * lightZ;
        faceLight[face] = 0.42f + 0.58f * (lambert > 0.0f ? lambert : 0.0f);
    }
    for (int index = 1; index < 6; ++index)
    {
        const int value = order[index];
        int position = index - 1;
        while (position >= 0 && faceDepth[order[position]] > faceDepth[value])
        {
            order[position + 1] = order[position];
            --position;
        }
        order[position + 1] = value;
    }
    for (int index = 0; index < 6; ++index)
    {
        const int face = order[index];
        Vec2 quad[4];
        for (int corner = 0; corner < 4; ++corner)
            quad[corner] = corners[faces[face][corner]];
        float shade = faceLight[face];
        if (shade > 1.0f)
            shade = 1.0f;
        drawList->addPolygonFilled(Span<const Vec2>(quad),
                                   Color(static_cast<uint8_t>(static_cast<float>(base.r) * shade),
                                         static_cast<uint8_t>(static_cast<float>(base.g) * shade),
                                         static_cast<uint8_t>(static_cast<float>(base.b) * shade),
                                         base.a), clip);
    }
    for (int face = 0; face < 6; ++face)
    {
        if (!visible[face])
            continue;
        for (int corner = 0; corner < 4; ++corner)
            drawList->addLine(corners[faces[face][corner]],
                              corners[faces[face][(corner + 1) % 4]], outline, clip, 1.5f);
    }
}

// Where the pointer's ray crosses the plane of a ring, measured from the target.
bool gizmo3DRingPoint(const Gizmo3DProjector &projector, const Vec2 &pixel, const Vec3 &centre,
                      const Vec3 &normal, Vec3 &local)
{
    Vec3 origin;
    Vec3 direction;
    if (!gizmo3DRayThrough(projector, pixel, origin, direction))
        return false;
    const float denominator = direction.x * normal.x + direction.y * normal.y + direction.z * normal.z;
    if (absoluteValue(denominator) < 1e-6f)
        return false;
    const float toCentreX = centre.x - origin.x;
    const float toCentreY = centre.y - origin.y;
    const float toCentreZ = centre.z - origin.z;
    const float distance = (toCentreX * normal.x + toCentreY * normal.y + toCentreZ * normal.z) / denominator;
    local = Vec3(origin.x + direction.x * distance - centre.x,
                 origin.y + direction.y * distance - centre.y,
                 origin.z + direction.z * distance - centre.z);
    return true;
}

// The turn from one vector to the other around `normal`, signed. This is the
// measurement ImGuizmo makes in ComputeAngleOnPlan: an acos of the two unit
// vectors, with the sign taken from the side the cross product falls on.
float gizmo3DSweptAngle(const Vec3 &normal, const Vec3 &from, const Vec3 &to)
{
    const float fromLength = sqrtf(from.x * from.x + from.y * from.y + from.z * from.z);
    const float toLength = sqrtf(to.x * to.x + to.y * to.y + to.z * to.z);
    if (fromLength < 1e-6f || toLength < 1e-6f)
        return 0.0f;
    const Vec3 a(from.x / fromLength, from.y / fromLength, from.z / fromLength);
    const Vec3 b(to.x / toLength, to.y / toLength, to.z / toLength);
    const float dot = a.x * b.x + a.y * b.y + a.z * b.z;
    const float crossX = a.y * b.z - a.z * b.y;
    const float crossY = a.z * b.x - a.x * b.z;
    const float crossZ = a.x * b.y - a.y * b.x;
    const float side = crossX * normal.x + crossY * normal.y + crossZ * normal.z;
    // atan2 of the cross product and the dot, not acos of the dot: two identical
    // vectors have a dot of 1, where acos only gives zero to within its own
    // rounding, and that rounding was enough to nudge the model on a press that
    // did not move.
    const float angle = atan2f(sqrtf(crossX * crossX + crossY * crossY + crossZ * crossZ), dot);
    return side < 0.0f ? -angle : angle;
}

GizmoAxis3D hitGizmo3D(Gizmo3DMode mode, const Vec2 &pointer, const Transform3D &transform,
                       const Gizmo3DProjector &projector, float axisLength)
{
    Vec2 center;
    if (!projectGizmo3D(projector, transform.position, center))
        return Gizmo3DAxisNone;
    const float centerX = pointer.x - center.x;
    const float centerY = pointer.y - center.y;
    if (centerX * centerX + centerY * centerY <= 64.0f)
        return Gizmo3DAxisXYZ;

    // A ring is grabbed where the pointer's ray crosses its plane, not where the
    // nearest point of the drawn curve happens to be: the drawing is an ellipse,
    // so measuring against it would make a ring seen edge-on grabbable along its
    // whole length.
    if (mode == Gizmo3DMode::Rotate)
    {
        float bestRingDistance = 9.0f;
        GizmoAxis3D bestRing = Gizmo3DAxisNone;
        for (uint8_t index = Gizmo3DAxisX; index <= Gizmo3DAxisZ; ++index)
        {
            const GizmoAxis3D axis = static_cast<GizmoAxis3D>(index);
            const Vec3 normal = gizmo3DAxisVector(axis);
            Vec3 local;
            if (!gizmo3DRingPoint(projector, pointer, transform.position, normal, local))
                continue;
            const Vec3 view = gizmo3DViewDirection(projector);
            if (local.x * view.x + local.y * view.y + local.z * view.z > 0.0f)
                continue;   // the far half, where nothing is drawn
            const float localLength = sqrtf(local.x * local.x + local.y * local.y + local.z * local.z);
            if (localLength < 1e-6f)
                continue;
            Vec2 onRing;
            if (!projectGizmo3D(projector, addGizmo3D(transform.position,
                    scaleGizmo3D(local, axisLength / localLength)), onRing))
                continue;
            const float offsetX = onRing.x - pointer.x;
            const float offsetY = onRing.y - pointer.y;
            const float distance = sqrtf(offsetX * offsetX + offsetY * offsetY);
            if (distance < bestRingDistance)
            {
                bestRingDistance = distance;
                bestRing = axis;
            }
        }

        // The ring around every handle turns the target the way the screen is
        // turned: the handle a model that is not axis aligned needs, and the one
        // the eye reaches for first.
        const float outerRadius = gizmo3DOuterRadius(projector, transform, axisLength);
        if (outerRadius > 0.0f)
        {
            const float offsetX = pointer.x - center.x;
            const float offsetY = pointer.y - center.y;
            const float distance = absoluteValue(sqrtf(offsetX * offsetX + offsetY * offsetY) - outerRadius);
            if (distance < bestRingDistance)
                bestRing = Gizmo3DAxisXYZ;
        }
        return bestRing;
    }

    float bestDistance = 9.0f;
    GizmoAxis3D bestAxis = Gizmo3DAxisNone;
    for (uint8_t index = Gizmo3DAxisX; index <= Gizmo3DAxisZ; ++index)
    {
        const GizmoAxis3D axis = static_cast<GizmoAxis3D>(index);
        Vec2 endpoint;
        if (!projectGizmo3D(projector, addGizmo3D(transform.position,
                scaleGizmo3D(gizmo3DAxisVector(axis), axisLength)), endpoint))
            continue;
        const float distance = gizmo3DSegmentDistance(pointer, center, endpoint);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestAxis = axis;
        }
    }
    if (bestAxis != Gizmo3DAxisNone)
        return bestAxis;

    const GizmoAxis3D planeAxes[] = {Gizmo3DAxisXY, Gizmo3DAxisXZ, Gizmo3DAxisYZ};
    const int planeFirst[] = {0, 0, 1};
    const int planeSecond[] = {1, 2, 2};
    for (int plane = 0; plane < 3; ++plane)
    {
        const Vec3 firstAxis = gizmo3DAxisVector(static_cast<GizmoAxis3D>(planeFirst[plane] + Gizmo3DAxisX));
        const Vec3 secondAxis = gizmo3DAxisVector(static_cast<GizmoAxis3D>(planeSecond[plane] + Gizmo3DAxisX));
        Vec2 corners[4];
        bool valid = true;
        const float offsets[][2] = {{0.28f, 0.28f}, {0.62f, 0.28f}, {0.62f, 0.62f}, {0.28f, 0.62f}};
        for (int corner = 0; corner < 4; ++corner)
        {
            const Vec3 world = addGizmo3D(transform.position,
                addGizmo3D(scaleGizmo3D(firstAxis, offsets[corner][0] * axisLength),
                            scaleGizmo3D(secondAxis, offsets[corner][1] * axisLength)));
            if (!projectGizmo3D(projector, world, corners[corner]))
                valid = false;
        }
        if (valid && gizmo3DPointInQuad(pointer, corners[0], corners[1], corners[2], corners[3]))
            return planeAxes[plane];
    }
    return Gizmo3DAxisNone;
}

Color colorFromHsv(float hue, float saturation, float value, uint8_t alpha)
{
    hue = hue < 0.0f ? hue + 1.0f : (hue >= 1.0f ? hue - 1.0f : hue);
    saturation = clamp(saturation, 0.0f, 1.0f);
    value = clamp(value, 0.0f, 1.0f);
    const float scaled = hue * 6.0f;
    const int sector = static_cast<int>(scaled);
    const float fraction = scaled - static_cast<float>(sector);
    const float p = value * (1.0f - saturation);
    const float q = value * (1.0f - saturation * fraction);
    const float t = value * (1.0f - saturation * (1.0f - fraction));
    float red = value;
    float green = t;
    float blue = p;
    switch (sector % 6)
    {
    case 0: red = value; green = t; blue = p; break;
    case 1: red = q; green = value; blue = p; break;
    case 2: red = p; green = value; blue = t; break;
    case 3: red = p; green = q; blue = value; break;
    case 4: red = t; green = p; blue = value; break;
    default: red = value; green = p; blue = q; break;
    }
    return Color(static_cast<uint8_t>(red * 255.0f + 0.5f),
                 static_cast<uint8_t>(green * 255.0f + 0.5f),
                 static_cast<uint8_t>(blue * 255.0f + 0.5f), alpha);
}

void colorToHsv(const Color &color, float &hue, float &saturation, float &value)
{
    const float red = static_cast<float>(color.r) / 255.0f;
    const float green = static_cast<float>(color.g) / 255.0f;
    const float blue = static_cast<float>(color.b) / 255.0f;
    const float maximum = red > green ? (red > blue ? red : blue) : (green > blue ? green : blue);
    const float minimum = red < green ? (red < blue ? red : blue) : (green < blue ? green : blue);
    const float delta = maximum - minimum;
    value = maximum;
    saturation = maximum > 0.0f ? delta / maximum : 0.0f;
    if (delta == 0.0f)
    {
        hue = 0.0f;
        return;
    }
    if (maximum == red)
        hue = (green - blue) / delta;
    else if (maximum == green)
        hue = 2.0f + (blue - red) / delta;
    else
        hue = 4.0f + (red - green) / delta;
    hue /= 6.0f;
    if (hue < 0.0f)
        hue += 1.0f;
}

} // namespace

Context::PointerState::PointerState()
    : position(), wheelX(0.0f), wheelY(0.0f), wheelControl(false)
{
    for (uint32_t i = 0; i < 3u; ++i)
    {
        pressedPosition[i] = Vec2();
        releasedPosition[i] = Vec2();
        down[i] = false;
        pressed[i] = false;
        released[i] = false;
    }
}

Context::Context(Backend &backend, TextProvider *textProvider)
    : backend_(backend), textProvider_(textProvider), theme_(), frame_(), pointer_(), layout_(), events_(), textEvents_(),
      windows_(), windowsById_(), windowButtons_(), listScrolls_(), textScrolls_(), colorPickers_(), childScrolls_(), gizmo2DStates_(), gizmo3DStates_(), dockSpaces_(),
      windowOrder_(), idStack_(), focusOrder_(), childStack_(), virtualListStack_(), virtualTableStack_(), virtualTreeStack_(), dockPanelStack_(), toasts_(), undoStack_(), redoStack_(), dragDrop_(), frameDrawList_(), dragDropDrawList_(), toastDrawList_(), modalDrawList_(), drawData_(),
      currentWindow_(), focusedWindow_(), draggingWindow_(), resizingWindow_(), activeWidget_(InvalidWidgetId),
      hotWidget_(InvalidWidgetId), faderGrabOffset_(0.0f), lastItemId_(InvalidWidgetId),
      focusedWidget_(InvalidWidgetId), textInputWidget_(InvalidWidgetId), openCombo_(InvalidWidgetId),
      openMenu_(InvalidWidgetId), openContextMenu_(InvalidWidgetId), openSubMenu_(InvalidWidgetId),
      activeMenu_(InvalidWidgetId), subMenuParent_(InvalidWidgetId),
      menuWasOpenAtFrameStart_(false), menuPopupBoundsAtFrameStart_(), subMenuPopupBoundsAtFrameStart_(),
      comboWasOpenAtFrameStart_(false), comboBoundsAtFrameStart_(), comboPopupBoundsAtFrameStart_(),
      comboBounds_(), comboPopupBounds_(), activeModalFrame_(0), openMenuFrame_(0), openComboFrame_(0),
      activeModal_(InvalidWidgetId), modalWindowId_(InvalidWidgetId), dragWidget_(InvalidWidgetId), activeDockSpace_(InvalidWidgetId),
      dockDragSpace_(InvalidWidgetId), dockDragTab_(InvalidWidgetId),
      textCursor_(0), frameNumber_(0), nextZOrder_(1), windowDragOffset_(), dockDragStart_(), menuBarBounds_(),
      menuPopupBounds_(), activeMenuBounds_(), subMenuPopupBounds_(), subMenuParentBounds_(),
      menuBarCursorX_(0.0f), menuBarActive_(false), forceWindowBounds_(false),
      table_(), tableResize_(), propertyRow_(), tableActive_(false), propertyRowActive_(false),
      dragStartValue_(0.0f), dragStartX_(0.0f), tooltipDelay_(0.45f), tooltipHoverSeconds_(0.0f),
      tooltipPointerPosition_(), tooltipWidget_(InvalidWidgetId),
      wantsKeyboard_(false), wantsTextInput_(false), backspacePressed_(false), enterPressed_(false),
      homePressed_(false), endPressed_(false), upPressed_(false), downPressed_(false),
      leftPressed_(false), rightPressed_(false), pageUpPressed_(false), pageDownPressed_(false),
      copyRequested_(false), pasteRequested_(false), escapePressed_(false), tabPressed_(false), tabShiftPressed_(false),
      tabConsumedByWidget_(false),
      keyPressed_(), keyControl_(), keyShift_()
{
    events_.reserve(32);
    windows_.reserve(8);
    windowsById_.reserve(8);
    listScrolls_.reserve(8);
    textScrolls_.reserve(8);
    colorPickers_.reserve(8);
    childScrolls_.reserve(8);
    gizmo2DStates_.reserve(4);
    gizmo3DStates_.reserve(4);
    dockSpaces_.reserve(2);
    windowOrder_.reserve(8);
    idStack_.reserve(8);
    focusOrder_.reserve(32);
    childStack_.reserve(4);
    virtualListStack_.reserve(2);
    virtualTableStack_.reserve(2);
    virtualTreeStack_.reserve(2);
    dockPanelStack_.reserve(2);
    toasts_.reserve(4);
    undoStack_.reserve(32);
    redoStack_.reserve(32);
    for (uint32_t i = 0u; i < 32u; ++i)
    {
        keyPressed_[i] = false;
        keyControl_[i] = false;
        keyShift_[i] = false;
    }
    textEvents_.reserve(4);
}

void Context::pushEvent(const Event &event)
{
    events_.push_back(event);
}

void Context::beginFrame(const FrameInfo &frame)
{
    frame_ = frame;
    ++frameNumber_;
    pointer_.wheelX = 0.0f;
    pointer_.wheelY = 0.0f;
    pointer_.wheelControl = false;
    for (uint32_t i = 0; i < 3u; ++i)
    {
        pointer_.pressed[i] = false;
        pointer_.released[i] = false;
    }

    frameDrawList_.clear();
    dragDropDrawList_.clear();
    toastDrawList_.clear();
    modalDrawList_.clear();
    for (ct::Vector<ToastState>::size_type i = 0u; i < toasts_.size(); ++i)
    {
        if (toasts_[i].remaining > 0.0f)
        {
            toasts_[i].remaining -= frame_.deltaSeconds;
            if (toasts_[i].remaining < 0.0f)
                toasts_[i].remaining = 0.0f;
        }
    }
    for (ct::Vector<ToastState>::size_type i = toasts_.size(); i > 0u; --i)
        if (toasts_[i-1u].remaining <= 0.0f)
            toasts_.erase(toasts_.begin() + (i-1u));
    for (ct::SlotMap<WindowState>::iterator it = windows_.begin(); it != windows_.end(); ++it)
    {
        it->drawList.clear();
        it->overlayDrawList.clear();
    }
    hotWidget_ = InvalidWidgetId;
    lastItemId_ = InvalidWidgetId;
    releaseUnsubmittedBlockers();
    // Snapshot taken before anything this frame can touch openMenu_/
    // openContextMenu_ - see pointerBlockedByOpenPopup. A click that closes
    // the menu (beginMenu's "clicked outside" branch) clears openMenu_
    // mid-frame, but the popup was still on screen when the pointer went
    // down, so widgets drawn later this same frame (a symbol tree, ...)
    // must still treat that click as having landed on the menu, not on them.
    menuWasOpenAtFrameStart_ = openMenu_ != InvalidWidgetId || openContextMenu_ != InvalidWidgetId;
    menuPopupBoundsAtFrameStart_ = menuPopupBounds_;
    subMenuPopupBoundsAtFrameStart_ = subMenuPopupBounds_;
    comboWasOpenAtFrameStart_ = openCombo_ != InvalidWidgetId;
    comboBoundsAtFrameStart_ = comboBounds_;
    comboPopupBoundsAtFrameStart_ = comboPopupBounds_;
    activeMenu_ = InvalidWidgetId;
    menuBarActive_ = false;
    childStack_.clear();
    virtualListStack_.clear();
    virtualTableStack_.clear();
    virtualTreeStack_.clear();
    dockPanelStack_.clear();
    activeDockSpace_ = InvalidWidgetId;
    tableActive_ = false;
    propertyRowActive_ = false;
    focusOrder_.clear();
    // Whether a text field had the keyboard last frame - read before the
    // reset below, for the undo shortcuts further down.
    const bool textFieldHadKeyboard = wantsTextInput_;
    wantsKeyboard_ = false;
    wantsTextInput_ = false;
    backspacePressed_ = false;
    enterPressed_ = false;
    homePressed_ = false;
    endPressed_ = false;
    upPressed_ = false;
    downPressed_ = false;
    leftPressed_ = false;
    rightPressed_ = false;
    pageUpPressed_ = false;
    pageDownPressed_ = false;
    copyRequested_ = false;
    pasteRequested_ = false;
    escapePressed_ = false;
    tabPressed_ = false;
    tabShiftPressed_ = false;
    tabConsumedByWidget_ = false;
    for (uint32_t i = 0u; i < 32u; ++i)
    {
        keyPressed_[i] = false;
        keyControl_[i] = false;
        keyShift_[i] = false;
    }
    textEvents_.clear();
    consumeEvents();

    // While a text field has the keyboard, Ctrl+Z/Ctrl+Y are its own (the
    // code editor has its own undo): taking them for the application's
    // history reverted unrelated actions under the user's typing, and in the
    // code editor undid twice.
    if (!textFieldHadKeyboard)
    {
        if (shortcut(KeyCode::Z))
            undo();
        else if (shortcut(KeyCode::Y) || shortcut(KeyCode::Z, true, true))
            redo();
    }

    // A press raises and focuses the window under it - unless a modal owns
    // the pointer: raising a background window over a setModalWindow() one
    // would hide the modal behind the window it is meant to block.
    const uint32_t leftButton = buttonIndex(PointerButton::Left);
    if (pointer_.pressed[leftButton] && activeModal_ == InvalidWidgetId)
    {
        const WindowHandle target = topWindowAt(pointer_.pressedPosition[leftButton]);
        const WindowState *targetWindow = windows_.get(target);
        if (modalWindowId_ == InvalidWidgetId || (targetWindow && targetWindow->id == modalWindowId_))
            focusWindow(target);
    }
}

void Context::releaseUnsubmittedBlockers()
{
    // frameNumber_ was just advanced: a blocker submitted during the previous
    // frame has frame + 1 == frameNumber_.
    if (activeModal_ != InvalidWidgetId && activeModalFrame_ + 1u < frameNumber_)
    {
        activeModal_ = InvalidWidgetId;
        activeWidget_ = InvalidWidgetId;
    }
    if (modalWindowId_ != InvalidWidgetId)
    {
        WindowHandle *handle = windowsById_.find(modalWindowId_);
        const WindowState *window = handle ? windows_.get(*handle) : nullptr;
        if (!window || !window->open || !windowSubmittedRecently(*window))
            modalWindowId_ = InvalidWidgetId;
    }
    if ((openMenu_ != InvalidWidgetId || openContextMenu_ != InvalidWidgetId) &&
        openMenuFrame_ + 1u < frameNumber_)
    {
        openMenu_ = InvalidWidgetId;
        openContextMenu_ = InvalidWidgetId;
        openSubMenu_ = InvalidWidgetId;
    }
    if (openCombo_ != InvalidWidgetId && openComboFrame_ + 1u < frameNumber_)
        openCombo_ = InvalidWidgetId;
}

bool Context::windowSubmittedRecently(const WindowState &window) const
{
    // Submitted this frame, or last frame and not yet this one.
    return window.lastFrame + 1u >= frameNumber_;
}

const DrawData &Context::endFrame()
{
    ct::sort(windowOrder_.begin(), windowOrder_.end(), [this](WindowHandle a, WindowHandle b)
    {
        const WindowState *left = windows_.get(a);
        const WindowState *right = windows_.get(b);
        if (!left || !right)
            return left != nullptr;
        return left->zOrder < right->zOrder;
    });

    frameDrawList_.clear();
    for (ct::Vector<WindowHandle>::size_type i = 0; i < windowOrder_.size(); ++i)
    {
        const WindowState *window = windows_.get(windowOrder_[i]);
        if (window && window->open)
        {
            frameDrawList_.append(window->drawList);
            frameDrawList_.append(window->overlayDrawList);
        }
    }
    drawDragDropPreview();
    frameDrawList_.append(dragDropDrawList_);
    drawToasts();
    frameDrawList_.append(toastDrawList_);
    frameDrawList_.append(modalDrawList_);

    const uint32_t left = buttonIndex(PointerButton::Left);
    if (dragDrop_.armed && pointer_.released[left])
    {
        if (activeWidget_ == dragDrop_.widget)
            activeWidget_ = InvalidWidgetId;
        dragDrop_ = DragDropState();
    }
    advanceFocus();

    currentWindow_ = WindowHandle();
    drawData_ = frameDrawList_.data(frame_.displaySize, frame_.dpiScale);
    return drawData_;
}

bool Context::beginWindow(StringView title, const Rect &initialBounds, bool *open)
{
    const WidgetId id = hashText(title);
    WindowState *window = getOrCreateWindow(id, title, initialBounds);
    if (!window)
        return false;

    if (forceWindowBounds_)
    {
        window->bounds = initialBounds;
        window->showWindowControls = false;
        window->showTitleBar = false;
        window->useClientArea = true;
        window->allowMove = false;
        window->allowResize = false;
    }
    else if (window->clampToViewport && frame_.displaySize.x > 0.0f && frame_.displaySize.y > 0.0f)
    {
        clampWindowToViewport(*window);
        window->clampToViewport = false;
    }

    if (open)
        window->open = *open;

    if (!window->open)
    {
        if (windowsById_[id] == focusedWindow_)
            focusedWindow_ = WindowHandle();
        if (open)
            *open = false;
        return false;
    }

    currentWindow_ = windowsById_[id];
    window->lastFrame = frameNumber_;
    const WindowState *focused = windows_.get(focusedWindow_);
    if (!focused || !focused->open || !windowSubmittedRecently(*focused))
        focusWindow(currentWindow_);
    drawWindow(*window);
    if (open)
        *open = window->open;
    if (!window->open || window->minimized)
    {
        currentWindow_ = WindowHandle();
        idStack_.clear();
        layout_ = LayoutState();
        return false;
    }
    beginLayout(*window);
    idStack_.clear();
    idStack_.push_back(id);
    return true;
}

bool Context::beginDialog(StringView title, bool &open, const Vec2 &size)
{
    const WidgetId id = combineIds(hashText(title), 0x4449414c4f47ull);
    if (!open)
    {
        if (activeModal_ == id)
            activeModal_ = InvalidWidgetId;
        setModalWindow(title, false);
        return false;
    }

    // Escape closes, like messageBox. Checked before the window is drawn so the
    // content never sees the frame that dismissed it.
    if (escapePressed_)
    {
        open = false;
        activeModal_ = InvalidWidgetId;
        activeWidget_ = InvalidWidgetId;
        setModalWindow(title, false);
        return false;
    }

    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const float width = size.x > 0.0f ? size.x : 320.0f;
    const float height = size.y > 0.0f ? size.y : 200.0f;
    const Rect bounds((viewport.width - width) * 0.5f, (viewport.height - height) * 0.5f,
                      width, height);

    // The scrim goes on the frame draw list rather than modalDrawList_, which
    // messageBox owns - a message box raised from inside a dialog must still
    // paint its own scrim over this one.
    frameDrawList_.addRectFilled(viewport, theme_.dialogScrim, viewport);

    forceWindowBounds_ = true;
    const bool opened = beginWindow(title, bounds, &open);
    forceWindowBounds_ = false;
    if (!opened)
    {
        setModalWindow(title, false);
        return false;
    }

    // forceWindowBounds_ strips the title bar, so draw one that matches the
    // message box's.
    WindowState *window = currentWindow();
    if (window)
    {
        const TextMetrics titleMetrics = measureText(theme_.font, title, theme_.fontSize);
        const Rect titleBar(bounds.x, bounds.y, bounds.width,
                            titleMetrics.height + theme_.windowPadding * 1.5f);
        window->drawList.addRectFilled(titleBar, theme_.floatTitleBg, viewport);
        window->drawList.addRect(bounds, theme_.dialogBorder, viewport);
        drawText(window->drawList, theme_.font, title,
                 Vec2(titleBar.x + theme_.windowPadding,
                      titleBar.y + (titleBar.height - titleMetrics.height) * 0.5f),
                 theme_.fontSize, theme_.labelText, viewport);
        layout_.origin.y += titleBar.height;
        layout_.cursor.y += titleBar.height;
    }

    setModalWindow(title, true);
    raiseWindow(title, true);
    return true;
}

void Context::endDialog()
{
    endWindow();
}

bool Context::beginMainWindow(StringView title, bool *open)
{
    forceWindowBounds_ = true;
    const bool opened = beginWindow(title, Rect(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y), open);
    forceWindowBounds_ = false;
    return opened;
}

void Context::endWindow()
{
    WindowState *window = currentWindow();
    if (window && !window->minimized && window->allowResize)
    {
        const float gripSize = 12.0f;
        const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
        const float right = window->bounds.x + window->bounds.width;
        const float bottom = window->bounds.y + window->bounds.height;
        window->drawList.addLine(Vec2(right - gripSize, bottom - 2.0f),
                                 Vec2(right - 2.0f, bottom - gripSize),
                                 theme_.borderColor, viewport, 1.0f);
        window->drawList.addLine(Vec2(right - gripSize * 0.55f, bottom - 2.0f),
                                 Vec2(right - 2.0f, bottom - gripSize * 0.55f),
                                 theme_.borderColor, viewport, 1.0f);
    }
    currentWindow_ = WindowHandle();
    idStack_.clear();
    layout_ = LayoutState();
}

void Context::pushId(uint64_t id)
{
    const WidgetId parent = idStack_.empty() ? InvalidWidgetId : idStack_.back();
    idStack_.push_back(combineIds(parent, id));
}

void Context::maximizeWindow(StringView title)
{
    auto handle = windowsById_.find(hashText(title));
    WindowState* w = handle ? windows_.get(*handle) : nullptr;
    if (!w || !w->open || !w->showWindowControls) return;
    if (!w->hasRestoreBounds) { w->restoreBounds=w->bounds; w->hasRestoreBounds=true; }
    w->maximized=true; w->minimized=false;
    w->bounds=Rect(0,0,frame_.displaySize.x,frame_.displaySize.y);
}

void Context::restoreWindow(StringView title)
{
    auto handle = windowsById_.find(hashText(title));
    WindowState* w = handle ? windows_.get(*handle) : nullptr;
    if (!w || !w->showWindowControls) return;
    if (w->hasRestoreBounds) w->bounds=w->restoreBounds;
    w->hasRestoreBounds=false; w->maximized=false; w->minimized=false;
}

void Context::setWindowButtons(StringView title, bool minimize, bool maximize)
{
    const WidgetId id = hashText(title);
    const uint8_t mask = static_cast<uint8_t>((minimize ? 1u : 0u) | (maximize ? 2u : 0u));
    windowButtons_.put(id, mask);

    WindowHandle *handle = windowsById_.find(id);
    WindowState *window = handle ? windows_.get(*handle) : nullptr;
    if (!window)
        return;
    window->showMinimizeButton = minimize;
    window->showMaximizeButton = maximize;
}

void Context::raiseWindow(StringView title, bool focus)
{
    WindowHandle *handle = windowsById_.find(hashText(title));
    WindowState *window = handle ? windows_.get(*handle) : nullptr;
    if (!window || !window->open)
        return;
    window->zOrder = nextZOrder_++;
    if (focus)
    {
        window->focused = true;
        focusedWindow_ = *handle;
    }
}

void Context::setModalWindow(StringView title, bool modal)
{
    const WidgetId id = hashText(title);
    if (modal)
        modalWindowId_ = id;
    else if (modalWindowId_ == id)
        modalWindowId_ = InvalidWidgetId;
}

// The "all windows" operations act on the windows on screen: a window the
// application stopped submitting keeps its WindowState but is not drawn,
// and must not take a tile/cascade slot.
void Context::maximizeAllWindows()
{
    for (auto h : windowOrder_) { auto w=windows_.get(h); if(w && w->open && windowSubmittedRecently(*w)) maximizeWindow(w->title); }
}
void Context::restoreAllWindows()
{
    for (auto h : windowOrder_) { auto w=windows_.get(h); if(w && w->open && windowSubmittedRecently(*w)) restoreWindow(w->title); }
}
void Context::minimizeAllWindows()
{
    for (auto h : windowOrder_) { auto w=windows_.get(h); if(w && w->open && w->showWindowControls && windowSubmittedRecently(*w)) w->minimized=true; }
}
void Context::tileAllWindows()
{
    int count=0;
    for (auto h : windowOrder_) { auto w=windows_.get(h); if(w && w->open && w->showWindowControls && windowSubmittedRecently(*w)) ++count; }
    if (!count) return;
    const int columns=static_cast<int>(ceilf(sqrtf(static_cast<float>(count))));
    const int rows=(count+columns-1)/columns;
    int i=0;
    for (auto h : windowOrder_) {
        auto w=windows_.get(h); if(!w || !w->open || !w->showWindowControls || !windowSubmittedRecently(*w)) continue;
        if (!w->hasRestoreBounds) { w->restoreBounds=w->bounds; w->hasRestoreBounds=true; }
        w->maximized=false; w->minimized=false;
        w->bounds=Rect((i%columns)*frame_.displaySize.x/columns,(i/columns)*frame_.displaySize.y/rows,
                       frame_.displaySize.x/columns,frame_.displaySize.y/rows); ++i;
    }
}
void Context::cascadeWindows()
{
    int i=0;
    for (auto h : windowOrder_) {
        auto w=windows_.get(h); if(!w || !w->open || !w->showWindowControls || !windowSubmittedRecently(*w)) continue;
        if (!w->hasRestoreBounds) { w->restoreBounds=w->bounds; w->hasRestoreBounds=true; }
        w->maximized=false; w->minimized=false;
        const float offset=(i++%8)*24.f;
        w->bounds=Rect(offset,offset,frame_.displaySize.x*.65f,frame_.displaySize.y*.65f);
    }
}

void Context::pushId(StringView id)
{
    pushId(hashText(id));
}

void Context::popId()
{
    if (!idStack_.empty())
        idStack_.pop_back();
}

void Context::setTheme(const Theme &theme)
{
    theme_ = theme;
}

const Theme &Context::theme() const
{
    return theme_;
}

bool Context::wantsPointer() const
{
    return activeModal_ != InvalidWidgetId || activeWidget_ != InvalidWidgetId || hotWidget_ != InvalidWidgetId;
}

bool Context::wantsKeyboard() const
{
    return wantsKeyboard_;
}

bool Context::wantsTextInput() const
{
    return wantsTextInput_;
}

void Context::pushUndo(StringView label, ct::Function<void()> undoCallback,
                       ct::Function<void()> redoCallback)
{
    if (!undoCallback || !redoCallback)
        return;
    UndoState entry;
    entry.label = String(label.data(), label.size());
    entry.undo = undoCallback;
    entry.redo = redoCallback;
    undoStack_.push_back(entry);
    redoStack_.clear();
}

bool Context::undo()
{
    if (undoStack_.empty())
        return false;
    UndoState entry = undoStack_.back();
    undoStack_.pop_back();
    entry.undo();
    redoStack_.push_back(entry);
    return true;
}

bool Context::redo()
{
    if (redoStack_.empty())
        return false;
    UndoState entry = redoStack_.back();
    redoStack_.pop_back();
    entry.redo();
    undoStack_.push_back(entry);
    return true;
}

bool Context::canUndo() const
{
    return !undoStack_.empty();
}

bool Context::canRedo() const
{
    return !redoStack_.empty();
}

void Context::clearUndoHistory()
{
    undoStack_.clear();
    redoStack_.clear();
}

bool Context::isKeyPressed(KeyCode key) const
{
    if (activeModal_ != InvalidWidgetId || menuWasOpenAtFrameStart_ || windowBlockedByModal())
        return false; // a modal/open menu owns the keyboard this frame - see shortcut()
    const uint32_t index = static_cast<uint32_t>(key);
    return index < 32u && keyPressed_[index];
}

bool Context::isPointerButtonDown(PointerButton button) const
{
    return pointer_.down[buttonIndex(button)];
}

Vec2 Context::pointerPosition() const
{
    return pointer_.position;
}

Vec2 Context::pointerPressedPosition(PointerButton button) const
{
    return pointer_.pressedPosition[buttonIndex(button)];
}

Vec2 Context::pointerContentPosition() const
{
    return toContentSpace(pointer_.position);
}

Vec2 Context::pointerPressedContentPosition(PointerButton button) const
{
    return toContentSpace(pointer_.pressedPosition[buttonIndex(button)]);
}

bool Context::isMenuOpen() const
{
    return menuWasOpenAtFrameStart_;
}

bool Context::shortcut(KeyCode key, bool control, bool shift) const
{
    // A dropdown/context menu or a modal dialog is on screen and gets first
    // say over the keyboard, same as it already does over the pointer (see
    // pointerBlockedByOpenPopup) - otherwise a caller's F5/Ctrl+S-style
    // shortcut() call, which every widget submits unconditionally every
    // frame (Toolbar::draw does, at the bottom, regardless of what else is
    // open), would still fire while the user is navigating a menu with
    // that same key combination free for something else. Uses the same
    // frame-start snapshot as pointerBlockedByOpenPopup: a menu that closes
    // via a key this same frame (Escape) must still win that frame.
    if (activeModal_ != InvalidWidgetId || menuWasOpenAtFrameStart_ || windowBlockedByModal())
        return false;
    const uint32_t index = static_cast<uint32_t>(key);
    return index < 32u && keyPressed_[index] && keyControl_[index] == control &&
           keyShift_[index] == shift;
}

bool Context::button(StringView labelText, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;
    const bool hovered = itemHovered(rect, clip, id);
    const bool clicked = itemClicked(rect, clip, id);
    const Color background = hovered ? theme_.buttonHovered : theme_.buttonBackground;
    drawFrame(*drawList, rect, background, hovered ? theme_.inputBorderHover : theme_.buttonBorder,
              theme_.borderRadius, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const Vec2 textPosition(rect.x + (rect.width - metrics.width) * 0.5f,
                            rect.y + (rect.height - metrics.height) * 0.5f);
    drawText(*drawList, theme_.font, labelText, textPosition, theme_.fontSize,
             theme_.buttonText, clip);
    return clicked;
}

bool Context::smallButton(StringView labelText, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;
    const WidgetId id = combineIds(makeWidgetId(labelText), 0x534d414c4c42544eull);
    const bool hovered = itemHovered(rect, clip, id);
    const bool clicked = itemClicked(rect, clip, id);
    drawFrame(*drawList, rect, hovered ? theme_.buttonHovered : theme_.buttonBackground,
              hovered ? theme_.inputBorderHover : theme_.buttonBorder, theme_.borderRadius, clip);
    const float fontSize = theme_.fontSize * 0.82f;
    const TextMetrics metrics = measureText(theme_.font, labelText, fontSize);
    drawText(*drawList, theme_.font, labelText,
             Vec2(rect.x + (rect.width - metrics.width) * 0.5f,
                  rect.y + (rect.height - metrics.height) * 0.5f),
             fontSize, theme_.buttonText, clip);
    return clicked;
}

bool Context::checkbox(StringView labelText, bool &value, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect widget = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || widget.width <= 0.0f || widget.height <= 0.0f)
        return false;
    const bool hovered = itemHovered(widget, clip, id);
    const bool clicked = itemClicked(widget, clip, id);
    if (clicked)
        value = !value;

    /* A square mark with the label beside it, the way the retained-mode CheckBox
       draws it: the box keeps the input colour and the tick is filled inside. */
    const float boxSize = widget.height < theme_.fontSize ? widget.height : theme_.fontSize;
    const Rect box(widget.x + (widget.width - boxSize) * 0.5f,
                   widget.y + (widget.height - boxSize) * 0.5f, boxSize, boxSize);
    drawFrame(*drawList, box, hovered ? theme_.buttonHovered : theme_.checkboxBackground,
              hovered ? theme_.inputBorderHover : theme_.inputBorder,
              theme_.borderRadius * 0.5f, clip);
    if (value)
    {
        const float margin = 3.0f;
        const float markSize = boxSize - margin * 2.0f;
        if (markSize > 0.0f)
            drawList->addRectFilled(Rect(box.x + margin, box.y + margin, markSize, markSize),
                                    theme_.checkMark, clip);
    }

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const Vec2 textPosition(box.x + boxSize + theme_.windowPadding * 0.5f,
                            widget.y + (widget.height - metrics.height) * 0.5f);
    drawText(*drawList, theme_.font, labelText, textPosition, theme_.fontSize,
             theme_.labelText, clip);
    return clicked;
}

bool Context::toggleSwitch(StringView labelText, bool &value, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const bool hovered = itemHovered(rect, clip, id);
    const bool clicked = itemClicked(rect, clip, id);
    if (clicked)
        value = !value;

    const float trackHeight = rect.height * 0.62f;
    const float radius = trackHeight * 0.5f;
    const Rect track(rect.x, rect.y + (rect.height - trackHeight) * 0.5f,
                     rect.width, trackHeight);
    const Color trackColor = value ? theme_.checkboxChecked
                                   : (hovered ? theme_.buttonHovered : theme_.checkboxBackground);
    if (track.width > radius * 2.0f)
        drawList->addRectFilled(Rect(track.x + radius, track.y,
                                     track.width - radius * 2.0f, track.height), trackColor, clip);
    drawList->addCircleFilled(Vec2(track.x + radius, track.y + radius), radius, trackColor, clip);
    drawList->addCircleFilled(Vec2(track.x + track.width - radius, track.y + radius), radius,
                              trackColor, clip);

    const float handleX = value ? track.x + track.width - radius : track.x + radius;
    drawList->addCircleFilled(Vec2(handleX, track.y + radius), radius * 0.72f,
                              theme_.switchThumb, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    drawText(*drawList, theme_.font, labelText,
             Vec2(rect.x + rect.width + theme_.windowPadding * 0.5f,
                  rect.y + (rect.height - metrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, clip);
    return clicked;
}

bool Context::radioButton(StringView labelText, bool selected, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect box = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;
    const bool hovered = itemHovered(box, clip, id);
    const bool clicked = itemClicked(box, clip, id);
    const float radius = (box.width < box.height ? box.width : box.height) * 0.5f;
    const Vec2 center(box.x + box.width * 0.5f, box.y + box.height * 0.5f);
    const Color background = hovered ? theme_.buttonHovered : theme_.radioBackground;
    drawList->addCircleFilled(center, radius, background, clip);
    if (selected)
        drawList->addCircleFilled(center, radius * 0.52f, theme_.radioChecked, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const Vec2 textPosition(box.x + box.width + theme_.windowPadding * 0.5f,
                            box.y + (box.height - metrics.height) * 0.5f);
    drawText(*drawList, theme_.font, labelText, textPosition, theme_.fontSize,
             theme_.labelText, clip);
    return clicked;
}

bool Context::selectable(StringView labelText, bool selected, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;

    const bool hovered = itemHovered(rect, clip, id);
    const bool clicked = itemClicked(rect, clip, id);
    const Color background = selected ? theme_.selectableSelected
                                      : (hovered ? theme_.selectableHovered
                                                 : theme_.selectableBackground);
    drawList->addRectFilledRounded(rect, theme_.borderRadius, background, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const Vec2 textPosition(rect.x + theme_.windowPadding * 0.5f,
                            rect.y + (rect.height - metrics.height) * 0.5f);
    drawText(*drawList, theme_.font, labelText, textPosition, theme_.fontSize,
             theme_.labelText, clip);
    return clicked;
}

bool Context::collapsingHeader(StringView labelText, bool &expanded, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;

    const bool hovered = itemHovered(rect, clip, id);
    if (itemClicked(rect, clip, id))
        expanded = !expanded;

    drawList->addRectFilledRounded(rect, theme_.borderRadius,
                                   hovered ? theme_.buttonHovered : theme_.collapsibleHeaderBg, clip);
    const float arrowSize = rect.height * 0.22f;
    const float arrowX = rect.x + theme_.windowPadding;
    const float arrowY = rect.y + rect.height * 0.5f;
    const Vec2 arrow[] = {
        expanded ? Vec2(arrowX, arrowY - arrowSize * 0.5f)
                 : Vec2(arrowX - arrowSize * 0.25f, arrowY - arrowSize),
        expanded ? Vec2(arrowX + arrowSize, arrowY - arrowSize * 0.5f)
                 : Vec2(arrowX - arrowSize * 0.25f, arrowY + arrowSize),
        expanded ? Vec2(arrowX + arrowSize * 0.5f, arrowY + arrowSize * 0.5f)
                 : Vec2(arrowX + arrowSize * 0.75f, arrowY)
    };
    drawList->addPolygonFilled(Span<const Vec2>(arrow), theme_.menuSubmenuArrow, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    drawText(*drawList, theme_.font, labelText,
             Vec2(arrowX + arrowSize + theme_.windowPadding,
                  rect.y + (rect.height - metrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, clip);
    return expanded;
}

bool Context::treeNode(StringView labelText, bool &expanded, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;

    const bool hovered = itemHovered(rect, clip, id);
    if (itemClicked(rect, clip, id))
        expanded = !expanded;

    if (hovered)
        drawList->addRectFilled(rect, theme_.buttonHovered, clip);
    const float arrowSize = rect.height * 0.20f;
    const float arrowX = rect.x + theme_.windowPadding * 0.75f;
    const float arrowY = rect.y + rect.height * 0.5f;
    const Vec2 arrow[] = {
        expanded ? Vec2(arrowX, arrowY - arrowSize * 0.5f)
                 : Vec2(arrowX - arrowSize * 0.25f, arrowY - arrowSize),
        expanded ? Vec2(arrowX + arrowSize, arrowY - arrowSize * 0.5f)
                 : Vec2(arrowX - arrowSize * 0.25f, arrowY + arrowSize),
        expanded ? Vec2(arrowX + arrowSize * 0.5f, arrowY + arrowSize * 0.5f)
                 : Vec2(arrowX + arrowSize * 0.75f, arrowY)
    };
    drawList->addPolygonFilled(Span<const Vec2>(arrow), theme_.menuSubmenuArrow, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    drawText(*drawList, theme_.font, labelText,
             Vec2(arrowX + arrowSize + theme_.windowPadding * 0.75f,
                  rect.y + (rect.height - metrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, clip);
    return expanded;
}

bool Context::treeItem(StringView labelText, bool &expanded, const TreeItemStyle &style,
                       const Rect &bounds)
{
    return treeItem(makeWidgetId(labelText), labelText, expanded, style, bounds, nullptr);
}

bool Context::treeItem(WidgetId nodeId, StringView labelText, bool &expanded,
                       const TreeItemStyle &style, const Rect &bounds, TreeDrop *drop)
{
    WindowState *window = currentWindow();
    if (!window || nodeId == InvalidWidgetId)
        return false;

    const WidgetId id = combineIds(makeWidgetId(labelText), nodeId);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const float arrowWidth = rect.height * 0.65f;
    const Rect arrowRect(rect.x, rect.y, arrowWidth, rect.height);
    const WidgetId arrowId = combineIds(id, 0x4152524f57ull);
    const uint32_t left = buttonIndex(PointerButton::Left);
    const Rect visible = intersect(rect, clip);
    const bool pressedOnRow = drop != nullptr && !style.disabled && pointerPressedIn(visible, left) &&
                              activeWidget_ == InvalidWidgetId &&
                              !contains(arrowRect, pointer_.pressedPosition[left]);
    if (pressedOnRow)
    {
        // Keep the regular row click available for selection. The item becomes a
        // drag source only if it moves far enough on a subsequent frame.
        dragDrop_.widget = id;
        dragDrop_.payload.type = 0x545245454954454dull;
        dragDrop_.payload.source = nodeId;
        dragDrop_.payload.data = nodeId;
        dragDrop_.preview = String(labelText.data(), labelText.size());
        dragDrop_.pressedPosition = pointer_.pressedPosition[left];
        dragDrop_.armed = true;
        dragDrop_.active = false;
        dragDrop_.accepted = false;
    }
    if (dragDrop_.armed && dragDrop_.widget == id && activeWidget_ == id)
    {
        const float deltaX = pointer_.position.x - dragDrop_.pressedPosition.x;
        const float deltaY = pointer_.position.y - dragDrop_.pressedPosition.y;
        if (pointer_.down[left] && deltaX * deltaX + deltaY * deltaY >= 16.0f)
            dragDrop_.active = true;
    }
    const bool rowHovered = !style.disabled && itemHovered(rect, clip, id);
    const bool arrowHovered = !style.disabled && !style.leaf && itemHovered(arrowRect, clip, arrowId);
    const bool expansionClicked = !style.disabled && !style.leaf &&
                                  itemClicked(arrowRect, clip, arrowId);
    if (expansionClicked)
        expanded = !expanded;
    const bool selected = !style.disabled && !expansionClicked && itemClicked(rect, clip, id);

    const Color rowColor = style.selected ? theme_.selectableSelected
                         : (rowHovered ? theme_.selectableHovered : theme_.inputBg);
    if (style.selected || rowHovered)
        drawList->addRectFilled(rect, rowColor, clip);

    const float centerY = rect.y + rect.height * 0.5f;
    const float iconSize = rect.height * 0.38f;
    const float iconX = rect.x + arrowWidth + theme_.windowPadding * 0.25f;
    if (!style.leaf)
    {
        const float arrowSize = rect.height * 0.20f;
        const float arrowX = rect.x + arrowWidth * 0.35f;
        const Vec2 arrow[] = {
            expanded ? Vec2(arrowX, centerY - arrowSize * 0.5f)
                     : Vec2(arrowX - arrowSize * 0.25f, centerY - arrowSize),
            expanded ? Vec2(arrowX + arrowSize, centerY - arrowSize * 0.5f)
                     : Vec2(arrowX - arrowSize * 0.25f, centerY + arrowSize),
            expanded ? Vec2(arrowX + arrowSize * 0.5f, centerY + arrowSize * 0.5f)
                     : Vec2(arrowX + arrowSize * 0.75f, centerY)
        };
        drawList->addPolygonFilled(Span<const Vec2>(arrow), theme_.menuSubmenuArrow, clip);
    }
    const Color iconColor = style.disabled
        ? Color(style.typeColor.r, style.typeColor.g, style.typeColor.b, 90u) : style.typeColor;
    drawList->addRectFilled(Rect(iconX, centerY - iconSize * 0.5f, iconSize, iconSize), iconColor, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const Color textColor = style.disabled
        ? Color(theme_.labelText.r, theme_.labelText.g, theme_.labelText.b, 110u) : theme_.labelText;
    drawText(*drawList, theme_.font, labelText,
             Vec2(iconX + iconSize + theme_.windowPadding * 0.5f,
                  rect.y + (rect.height - metrics.height) * 0.5f),
             theme_.fontSize, textColor, clip);
    if (drop && dragDrop_.active && dragDrop_.payload.type == 0x545245454954454dull &&
        dragDrop_.payload.source != nodeId && !style.disabled && contains(visible, pointer_.position))
    {
        const float localY = pointer_.position.y - rect.y;
        const TreeDropPosition position = localY < rect.height * 0.25f ? TreeDropPosition::Before
                                       : (localY >= rect.height * 0.75f ||
                                          (style.leaf && !style.acceptsChildren))
                                           ? TreeDropPosition::After : TreeDropPosition::Inside;
        if (position == TreeDropPosition::Inside)
            drawList->addRect(rect, theme_.dialogBtnPrimary, clip, 2.0f);
        else
        {
            const float lineY = position == TreeDropPosition::Before ? rect.y : rect.y + rect.height;
            drawList->addLine(Vec2(rect.x, lineY), Vec2(rect.x + rect.width, lineY),
                              theme_.dialogBtnPrimary, clip, 2.0f);
        }
        if (pointer_.released[left])
        {
            drop->source = dragDrop_.payload.source;
            drop->target = nodeId;
            drop->position = position;
            dragDrop_.accepted = true;
        }
    }
    (void)arrowHovered;
    return selected;
}

bool Context::tabBar(StringView labelText, int &currentItem, Span<const StringView> items,
                     const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || items.empty())
        return false;

    if (currentItem < 0 || static_cast<Span<const StringView>::size_type>(currentItem) >= items.size())
        currentItem = 0;
    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const float tabWidth = rect.width / static_cast<float>(items.size());
    bool changed = false;
    registerFocusable(id);
    if (hasKeyboardFocus(id))
    {
        const int previous = currentItem;
        if ((leftPressed_ || upPressed_) && currentItem > 0)
            --currentItem;
        if ((rightPressed_ || downPressed_) && currentItem + 1 < static_cast<int>(items.size()))
            ++currentItem;
        if (homePressed_)
            currentItem = 0;
        if (endPressed_)
            currentItem = static_cast<int>(items.size()) - 1;
        changed = currentItem != previous;
    }
    for (Span<const StringView>::size_type i = 0u; i < items.size(); ++i)
    {
        const Rect tab(rect.x + tabWidth * static_cast<float>(i), rect.y,
                       i + 1u == items.size() ? rect.x + rect.width -
                           (rect.x + tabWidth * static_cast<float>(i)) : tabWidth,
                       rect.height);
        const WidgetId tabId = combineIds(id, static_cast<WidgetId>(i + 1u));
        const bool hovered = itemHovered(tab, clip, tabId);
        if (itemClicked(tab, clip, tabId, false))
        {
            focusedWidget_ = id;
            if (currentItem != static_cast<int>(i))
            {
                currentItem = static_cast<int>(i);
                changed = true;
            }
        }
        const Color background = static_cast<int>(i) == currentItem ? theme_.selectableSelected
                               : (hovered ? theme_.selectableHovered : theme_.buttonBackground);
        drawList->addRectFilled(tab, background, clip);
        const TextMetrics metrics = measureText(theme_.font, items[i], theme_.fontSize);
        drawText(*drawList, theme_.font, items[i],
                 Vec2(tab.x + (tab.width - metrics.width) * 0.5f,
                      tab.y + (tab.height - metrics.height) * 0.5f),
                 theme_.fontSize, theme_.buttonText, clip);
    }
    return changed;
}

bool Context::listBox(StringView labelText, int &currentItem, Span<const StringView> items,
                      const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || items.empty())
        return false;

    if (currentItem < 0 || static_cast<Span<const StringView>::size_type>(currentItem) >= items.size())
        currentItem = 0;
    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    const Rect visible = intersect(rect, clip);
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f ||
        visible.width <= 0.0f || visible.height <= 0.0f)
        return false;

    const int rowCount = static_cast<int>(rect.height / theme_.widgetHeight);
    const int visibleRows = rowCount > 0 ? rowCount : 1;
    registerFocusable(id);
    const int maximumScroll = static_cast<int>(items.size()) - visibleRows;
    int *scroll = listScrolls_.find(id);
    if (!scroll)
    {
        listScrolls_.put(id, 0);
        scroll = listScrolls_.find(id);
    }
    if (!scroll)
        return false;
    if (*scroll < 0)
        *scroll = 0;
    if (*scroll > maximumScroll)
        *scroll = maximumScroll > 0 ? maximumScroll : 0;

    const bool hovered = pointerOver(visible);
    if (hovered && pointer_.wheelY != 0.0f && maximumScroll > 0)
    {
        const int delta = pointer_.wheelY > 0.0f ? -1 : 1;
        *scroll += delta;
        if (*scroll < 0)
            *scroll = 0;
        if (*scroll > maximumScroll)
            *scroll = maximumScroll;
    }

    bool changed = false;
    if (hasKeyboardFocus(id) && (upPressed_ || downPressed_ || pageUpPressed_ || pageDownPressed_ ||
                                 homePressed_ || endPressed_))
    {
        const int previous = currentItem;
        if (upPressed_ && currentItem > 0)
            --currentItem;
        if (downPressed_ && currentItem + 1 < static_cast<int>(items.size()))
            ++currentItem;
        if (pageUpPressed_)
            currentItem -= visibleRows;
        if (pageDownPressed_)
            currentItem += visibleRows;
        if (homePressed_)
            currentItem = 0;
        if (endPressed_)
            currentItem = static_cast<int>(items.size()) - 1;
        if (currentItem < 0)
            currentItem = 0;
        if (currentItem >= static_cast<int>(items.size()))
            currentItem = static_cast<int>(items.size()) - 1;
        changed = currentItem != previous;
        if (currentItem < *scroll)
            *scroll = currentItem;
        if (currentItem >= *scroll + visibleRows)
            *scroll = currentItem - visibleRows + 1;
    }

    float scrollbarWidth = 0.0f;
    Rect scrollbar;
    Rect thumb;
    WidgetId scrollbarId = InvalidWidgetId;
    if (maximumScroll > 0)
    {
        scrollbarWidth = theme_.scrollbarWidth > 4.0f ? theme_.scrollbarWidth * 0.35f : 4.0f;
        const float fraction = static_cast<float>(visibleRows) / static_cast<float>(items.size());
        const float minimumThumb = theme_.scrollbarMinThumb < rect.height
                                       ? theme_.scrollbarMinThumb : rect.height;
        const float thumbHeight = rect.height * fraction > minimumThumb
                                      ? rect.height * fraction : minimumThumb;
        const float travel = rect.height - thumbHeight;
        const float offset = travel * static_cast<float>(*scroll) / static_cast<float>(maximumScroll);
        scrollbar = Rect(rect.x + rect.width - scrollbarWidth, rect.y, scrollbarWidth, rect.height);
        thumb = Rect(scrollbar.x, rect.y + offset, scrollbarWidth, thumbHeight);
        scrollbarId = combineIds(id, 0x5343524f4c4cull);
        const uint32_t left = buttonIndex(PointerButton::Left);
        const bool pressedThumb = pointerPressedIn(intersect(thumb, visible), left) &&
                                  activeWidget_ == InvalidWidgetId;
        if (pressedThumb)
        {
            activeWidget_ = scrollbarId;
            focusedWidget_ = id;
        }
        if (activeWidget_ == scrollbarId)
        {
            if (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left])
            {
                const float normalized = travel > 0.0f
                    ? clamp((pointer_.position.y - rect.y - thumb.height * 0.5f) / travel, 0.0f, 1.0f)
                    : 0.0f;
                const int nextScroll = static_cast<int>(normalized * static_cast<float>(maximumScroll) + 0.5f);
                *scroll = nextScroll < 0 ? 0 : (nextScroll > maximumScroll ? maximumScroll : nextScroll);
            }
            if (pointer_.released[left])
                activeWidget_ = InvalidWidgetId;
        }
        thumb.y = rect.y + travel * static_cast<float>(*scroll) / static_cast<float>(maximumScroll);
    }

    drawList->addRectFilled(rect, theme_.selectableBackground, visible);
    const int itemCount = static_cast<int>(items.size());
    for (int row = 0; row < visibleRows; ++row)
    {
        const int itemIndex = *scroll + row;
        if (itemIndex >= itemCount)
            break;
        const Rect item(rect.x, rect.y + theme_.widgetHeight * static_cast<float>(row),
                        rect.width - scrollbarWidth, theme_.widgetHeight);
        const WidgetId itemId = combineIds(id, static_cast<WidgetId>(itemIndex + 1));
        const bool itemHoveredValue = itemHovered(item, visible, itemId);
        if (itemClicked(item, visible, itemId))
        {
            if (currentItem != itemIndex)
            {
                currentItem = itemIndex;
                changed = true;
            }
            focusedWidget_ = id;
        }
        if (itemIndex == currentItem || itemHoveredValue)
            drawList->addRectFilled(item, itemIndex == currentItem
                                            ? theme_.selectableSelected : theme_.selectableHovered,
                                    visible);
        const TextMetrics metrics = measureText(theme_.font,
                                                 items[static_cast<Span<const StringView>::size_type>(itemIndex)],
                                                 theme_.fontSize);
        drawText(*drawList, theme_.font,
                 items[static_cast<Span<const StringView>::size_type>(itemIndex)],
                 Vec2(item.x + theme_.windowPadding * 0.5f,
                      item.y + (item.height - metrics.height) * 0.5f),
                 theme_.fontSize, theme_.labelText, visible);
    }

    if (maximumScroll > 0)
    {
        drawList->addRectFilled(scrollbar, theme_.inputBg, visible);
        drawList->addRectFilled(thumb, theme_.scrollbarThumb, visible);
    }
    return changed;
}

bool Context::comboBox(StringView labelText, int &currentItem, Span<const StringView> items,
                       const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || items.empty())
        return false;

    if (currentItem < 0 || static_cast<Span<const StringView>::size_type>(currentItem) >= items.size())
        currentItem = 0;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;

    bool changed = false;
    if (hasKeyboardFocus(id) && (upPressed_ || downPressed_ || homePressed_ || endPressed_))
    {
        const int previous = currentItem;
        if (upPressed_ && currentItem > 0)
            --currentItem;
        if (downPressed_ && currentItem + 1 < static_cast<int>(items.size()))
            ++currentItem;
        if (homePressed_)
            currentItem = 0;
        if (endPressed_)
            currentItem = static_cast<int>(items.size()) - 1;
        changed = currentItem != previous;
    }

    const bool hovered = itemHovered(rect, clip, id);
    if (itemClicked(rect, clip, id))
        openCombo_ = openCombo_ == id ? InvalidWidgetId : id;

    drawFrame(*drawList, rect, hovered ? theme_.buttonHovered : theme_.buttonBackground,
              hovered ? theme_.inputBorderHover : theme_.buttonBorder, theme_.borderRadius, clip);
    const TextMetrics selectedMetrics = measureText(theme_.font, items[static_cast<Span<const StringView>::size_type>(currentItem)],
                                                    theme_.fontSize);
    drawText(*drawList, theme_.font, items[static_cast<Span<const StringView>::size_type>(currentItem)],
             Vec2(rect.x + theme_.windowPadding * 0.5f,
                  rect.y + (rect.height - selectedMetrics.height) * 0.5f),
             theme_.fontSize, theme_.buttonText, clip);

    const float arrowSize = rect.height * 0.2f;
    const Vec2 arrow[] = {
        Vec2(rect.x + rect.width - theme_.windowPadding - arrowSize, rect.y + rect.height * 0.42f),
        Vec2(rect.x + rect.width - theme_.windowPadding, rect.y + rect.height * 0.42f),
        Vec2(rect.x + rect.width - theme_.windowPadding - arrowSize * 0.5f, rect.y + rect.height * 0.62f)
    };
    drawList->addPolygonFilled(Span<const Vec2>(arrow), theme_.buttonText, clip);

    if (openCombo_ != id)
        return changed;

    const Rect popup(rect.x, rect.y + rect.height, rect.width, rect.height * static_cast<float>(items.size()));
    const Rect popupClip = intersect(popup, clip);
    const uint32_t left = buttonIndex(PointerButton::Left);
    // Any press outside the header and the list closes it, in whatever
    // window it lands; pointerBlockedByOpenPopup keeps that press from also
    // reaching the widget under it.
    if (pointer_.pressed[left] &&
        !contains(rect, pointer_.pressedPosition[left]) &&
        !contains(popupClip, pointer_.pressedPosition[left]))
    {
        openCombo_ = InvalidWidgetId;
        return false;
    }
    openComboFrame_ = frameNumber_;
    comboBounds_ = intersect(rect, clip);
    comboPopupBounds_ = popupClip;

    // Popups are deferred to the window overlay so controls declared after a
    // combo box cannot paint over its open list.
    DrawList &popupDrawList = window->overlayDrawList;
    popupDrawList.addRectFilled(popup, theme_.selectableBackground, popupClip);
    for (Span<const StringView>::size_type i = 0; i < items.size(); ++i)
    {
        const Rect item(rect.x, rect.y + rect.height * static_cast<float>(i + 1u),
                        rect.width, rect.height);
        const WidgetId itemId = combineIds(id, static_cast<WidgetId>(i + 1u));
        const bool itemHoveredValue = itemHovered(item, popupClip, itemId);
        if (itemClicked(item, popupClip, itemId))
        {
            currentItem = static_cast<int>(i);
            openCombo_ = InvalidWidgetId;
            return true;
        }
        if (itemHoveredValue || static_cast<int>(i) == currentItem)
            popupDrawList.addRectFilled(item, static_cast<int>(i) == currentItem
                                            ? theme_.selectableSelected : theme_.selectableHovered,
                                       popupClip);

        const TextMetrics itemMetrics = measureText(theme_.font, items[i], theme_.fontSize);
        drawText(popupDrawList, theme_.font, items[i],
                 Vec2(item.x + theme_.windowPadding * 0.5f,
                      item.y + (item.height - itemMetrics.height) * 0.5f),
                 theme_.fontSize, theme_.labelText, popupClip);
    }
    return changed;
}

bool Context::beginMenuBar(const Rect &bounds)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList)
        return false;
    const Rect rect = contentRect(bounds);
    if (rect.width <= 0.0f || rect.height <= 0.0f)
        return false;
    menuBarBounds_ = rect;
    menuBarCursorX_ = rect.x;
    menuBarActive_ = true;
    drawList->addRectFilled(rect, theme_.menuBarBg, contentClip());
    return true;
}

void Context::endMenuBar()
{
    menuBarActive_ = false;
}

bool Context::beginMenu(StringView labelText)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList || !menuBarActive_)
        return false;

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const float width = metrics.width + theme_.menuItemPadX * 2.0f;
    const Rect button(menuBarCursorX_, menuBarBounds_.y, width, menuBarBounds_.height);
    menuBarCursorX_ += width;
    const WidgetId id = combineIds(makeWidgetId(labelText), 0x4d454e55ull);
    // Size the popup at the width measured for this menu (menuMinWidth until
    // its items have been submitted at least once - see endMenu).
    const float popupWidth = menuWidthId_ == id ? menuPopupWidth_ : theme_.menuMinWidth;
    const uint32_t left = buttonIndex(PointerButton::Left);
    // Any press outside closes the menu - in another window too, where
    // pointerBlockedByOpenPopup already swallows it; checking only this
    // window left the menu open and every click elsewhere eaten.
    if (openMenu_ == id && pointer_.pressed[left] &&
        !contains(button, pointer_.pressedPosition[left]) &&
        !contains(menuPopupBounds_, pointer_.pressedPosition[left]) &&
        !contains(subMenuPopupBounds_, pointer_.pressedPosition[left]))
    {
        openMenu_ = InvalidWidgetId;
        openSubMenu_ = InvalidWidgetId;
    }

    const bool hovered = itemHovered(button, contentClip(), id);
    if (hovered && openMenu_ != InvalidWidgetId && openMenu_ != id &&
        currentWindow_ == focusedWindow_ && !pointer_.pressed[left] && !pointer_.released[left])
    {
        openMenu_ = id;
        openContextMenu_ = InvalidWidgetId;
        openSubMenu_ = InvalidWidgetId;
        subMenuPopupBounds_ = Rect();
        menuPopupBounds_ = Rect(button.x, button.bottom(), popupWidth, 0.0f);
        focusedWidget_ = id;
    }
    if (itemClicked(button, contentClip(), id))
    {
        openMenu_ = openMenu_ == id ? InvalidWidgetId : id;
        openContextMenu_ = InvalidWidgetId;
        openSubMenu_ = InvalidWidgetId;
        if (openMenu_ == id)
            menuPopupBounds_ = Rect(button.x, button.y + button.height, popupWidth, 0.0f);
    }
    drawList->addRectFilled(button, openMenu_ == id || hovered ? theme_.menuBarItemHover : theme_.menuBarBg,
                            contentClip());
    drawText(*drawList, theme_.font, labelText,
             Vec2(button.x + theme_.menuItemPadX,
                  button.y + (button.height - metrics.height) * 0.5f),
             theme_.fontSize, theme_.menuItemText, contentClip());

    if (openMenu_ != id)
        return false;
    openMenuFrame_ = frameNumber_;
    activeMenu_ = id;
    activeMenuBounds_ = menuPopupBounds_;
    activeMenuBounds_.x = button.x;
    activeMenuBounds_.y = button.y + button.height;
    activeMenuBounds_.width = popupWidth;
    activeMenuBounds_.height = 0.0f;
    return true;
}

void Context::endMenu()
{
    if (activeMenu_ == InvalidWidgetId)
        return;
    WindowState *window = currentWindow();
    if (window && activeMenuBounds_.height > 0.0f)
    {
        const Rect clip = contentClip();
        window->overlayDrawList.addRect(activeMenuBounds_, theme_.menuBorder, clip);
        menuPopupBounds_ = activeMenuBounds_;
        // Latch the width measured while this menu's items were submitted:
        // the panel just drawn used the previous frame's number, and the
        // next frame's beginMenu picks this one up.
        if (menuMeasuredWidth_ > 0.0f)
        {
            menuWidthId_ = activeMenu_;
            menuPopupWidth_ = menuMeasuredWidth_ > theme_.menuMinWidth ? menuMeasuredWidth_ : theme_.menuMinWidth;
        }
    }
    menuMeasuredWidth_ = 0.0f;
    activeMenu_ = InvalidWidgetId;
}

bool Context::menuItemInternal(StringView labelText, bool enabled, bool *checked)
{
    WindowState *window = currentWindow();
    if (!window || activeMenu_ == InvalidWidgetId)
        return false;
    const Rect clip = contentClip();
    DrawList &popup = window->overlayDrawList;
    const Rect item(activeMenuBounds_.x, activeMenuBounds_.y + activeMenuBounds_.height,
                    activeMenuBounds_.width, theme_.menuItemHeight);
    activeMenuBounds_.height += item.height;
    const WidgetId id = combineIds(activeMenu_, static_cast<WidgetId>(activeMenuBounds_.height * 100.0f));
    const bool hovered = enabled && itemHovered(item, clip, id);
    const bool clicked = enabled && itemClicked(item, clip, id);
    popup.addRectFilled(item, hovered ? theme_.menuItemHover : theme_.menuBg, clip);
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const float checkWidth = checked ? theme_.menuItemHeight : 0.0f;
    // Remember the widest label drawn this frame so endMenu can size the
    // popup for it. Only this menu's own items count: while a submenu's
    // items are being submitted subMenuParent_ is set, and the parent's
    // width must not grow to fit the submenu's labels.
    const float itemWidth = metrics.width + theme_.menuItemPadX * 2.0f + checkWidth;
    if (subMenuParent_ == InvalidWidgetId)
    {
        if (itemWidth > menuMeasuredWidth_)
            menuMeasuredWidth_ = itemWidth;
    }
    else if (itemWidth > subMenuMeasuredWidth_)
    {
        subMenuMeasuredWidth_ = itemWidth;
    }
    if (checked && *checked)
    {
        const float checkSize = theme_.menuItemHeight * 0.38f;
        const float checkX = item.x + theme_.menuItemPadX + (theme_.menuItemHeight - checkSize) * 0.5f;
        const float checkY = item.y + (item.height - checkSize) * 0.5f;
        popup.addLine(Vec2(checkX, checkY + checkSize * 0.50f),
                      Vec2(checkX + checkSize * 0.35f, checkY + checkSize),
                      theme_.menuCheckMark, clip, 2.0f);
        popup.addLine(Vec2(checkX + checkSize * 0.35f, checkY + checkSize),
                      Vec2(checkX + checkSize, checkY), theme_.menuCheckMark, clip, 2.0f);
    }
    drawText(popup, theme_.font, labelText,
             Vec2(item.x + theme_.menuItemPadX + checkWidth,
                  item.y + (item.height - metrics.height) * 0.5f),
             theme_.fontSize, enabled ? (hovered ? theme_.menuItemTextHover : theme_.menuItemText)
                                : theme_.menuItemDisabled,
             clip);
    if (clicked)
    {
        if (checked)
            *checked = !*checked;
        openMenu_ = InvalidWidgetId;
        openContextMenu_ = InvalidWidgetId;
        openSubMenu_ = InvalidWidgetId;
    }
    return clicked;
}

bool Context::menuItem(StringView labelText, bool enabled)
{
    return menuItemInternal(labelText, enabled, nullptr);
}

bool Context::menuCheckbox(StringView labelText, bool &checked, bool enabled)
{
    return menuItemInternal(labelText, enabled, &checked);
}

bool Context::beginSubMenu(StringView labelText, bool enabled)
{
    WindowState *window = currentWindow();
    if (!window || activeMenu_ == InvalidWidgetId || subMenuParent_ != InvalidWidgetId)
        return false;
    const Rect clip = contentClip();
    DrawList &popup = window->overlayDrawList;
    const Rect item(activeMenuBounds_.x, activeMenuBounds_.y + activeMenuBounds_.height,
                    activeMenuBounds_.width, theme_.menuItemHeight);
    activeMenuBounds_.height += item.height;
    const WidgetId parentId = activeMenu_;
    const WidgetId id = combineIds(parentId, combineIds(makeWidgetId(labelText), 0x5355424d454e55ull));
    const bool hovered = enabled && itemHovered(item, clip, id);
    const bool clicked = enabled && itemClicked(item, clip, id);
    if (enabled && (hovered || clicked))
    {
        openSubMenu_ = id;
        const float popupWidth = subMenuWidthId_ == id ? subMenuPopupWidth_ : theme_.menuMinWidth;
        float popupX = item.x + item.width;
        const float clipRight = clip.x + clip.width;
        if (popupX + popupWidth > clipRight)
        {
            const float leftPopupX = item.x - popupWidth;
            popupX = leftPopupX >= clip.x ? leftPopupX : clipRight - popupWidth;
        }
        if (popupX < clip.x)
            popupX = clip.x;
        subMenuPopupBounds_ = Rect(popupX, item.y, popupWidth, 0.0f);
    }
    popup.addRectFilled(item, openSubMenu_ == id || hovered ? theme_.menuItemHover : theme_.menuBg, clip);
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    drawText(popup, theme_.font, labelText,
             Vec2(item.x + theme_.menuItemPadX, item.y + (item.height - metrics.height) * 0.5f),
             theme_.fontSize, enabled ? theme_.menuItemText : theme_.menuItemDisabled, clip);
    const float arrowSize = item.height * 0.18f;
    const float arrowX = item.x + item.width - theme_.menuItemPadX - arrowSize;
    const float arrowY = item.y + item.height * 0.5f;
    const Vec2 arrow[] = {
        Vec2(arrowX, arrowY - arrowSize), Vec2(arrowX, arrowY + arrowSize),
        Vec2(arrowX + arrowSize, arrowY)
    };
    popup.addPolygonFilled(Span<const Vec2>(arrow), theme_.menuSubmenuArrow, clip);
    if (openSubMenu_ != id)
        return false;

    subMenuParent_ = parentId;
    subMenuParentBounds_ = activeMenuBounds_;
    activeMenu_ = id;
    activeMenuBounds_ = subMenuPopupBounds_;
    activeMenuBounds_.height = 0.0f;
    return true;
}

void Context::endSubMenu()
{
    if (subMenuParent_ == InvalidWidgetId)
        return;
    WindowState *window = currentWindow();
    if (window && activeMenuBounds_.height > 0.0f)
    {
        window->overlayDrawList.addRect(activeMenuBounds_, theme_.menuBorder, contentClip());
        subMenuPopupBounds_ = activeMenuBounds_;
        if (subMenuMeasuredWidth_ > 0.0f)
        {
            subMenuWidthId_ = activeMenu_;
            subMenuPopupWidth_ = subMenuMeasuredWidth_ > theme_.menuMinWidth ? subMenuMeasuredWidth_ : theme_.menuMinWidth;
        }
    }
    subMenuMeasuredWidth_ = 0.0f;
    activeMenu_ = subMenuParent_;
    activeMenuBounds_ = subMenuParentBounds_;
    subMenuParent_ = InvalidWidgetId;
}

void Context::menuSeparator()
{
    WindowState *window = currentWindow();
    if (!window || activeMenu_ == InvalidWidgetId)
        return;
    const Rect separator(activeMenuBounds_.x, activeMenuBounds_.y + activeMenuBounds_.height,
                         activeMenuBounds_.width, theme_.itemSpacing);
    activeMenuBounds_.height += separator.height;
    window->overlayDrawList.addRectFilled(separator, theme_.menuBg, contentClip());
    window->overlayDrawList.addRectFilled(Rect(separator.x + theme_.menuItemPadX,
                                               separator.y + separator.height * 0.5f,
                                               separator.width - theme_.menuItemPadX * 2.0f, 1.0f),
                                         theme_.menuSeparator, contentClip());
}

bool Context::beginContextMenu(StringView idText, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;
    const WidgetId id = combineIds(makeWidgetId(idText), 0x434f4e54455854ull);
    const float popupWidth = menuWidthId_ == id ? menuPopupWidth_ : theme_.menuMinWidth;
    const Rect target = contentRect(bounds);
    const Rect visible = intersect(target, contentClip());
    const uint32_t right = buttonIndex(PointerButton::Right);
    const uint32_t left = buttonIndex(PointerButton::Left);
    if (pointerPressedIn(visible, right))
    {
        openContextMenu_ = id;
        openMenu_ = InvalidWidgetId;
        openSubMenu_ = InvalidWidgetId;
        menuPopupBounds_ = Rect(pointer_.pressedPosition[right].x, pointer_.pressedPosition[right].y,
                                popupWidth, 0.0f);
    }
    if (openContextMenu_ != id)
        return false;
    // Any press outside closes it, in whatever window it lands (see beginMenu).
    if (pointer_.pressed[left] &&
        !contains(menuPopupBounds_, pointer_.pressedPosition[left]) &&
        !contains(subMenuPopupBounds_, pointer_.pressedPosition[left]))
    {
        openContextMenu_ = InvalidWidgetId;
        openSubMenu_ = InvalidWidgetId;
        return false;
    }
    openMenuFrame_ = frameNumber_;
    activeMenu_ = id;
    activeMenuBounds_ = menuPopupBounds_;
    activeMenuBounds_.width = popupWidth;
    activeMenuBounds_.height = 0.0f;
    return true;
}

void Context::endContextMenu()
{
    endMenu();
}

bool Context::sliderFloat(StringView labelText, float &value, float minimum, float maximum,
                          const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || maximum <= minimum)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const float labelWidth = metrics.width + theme_.windowPadding;
    const float usableWidth = rect.width > labelWidth ? rect.width - labelWidth : 0.0f;
    float valueWidth = usableWidth * 0.28f;
    if (valueWidth < 56.0f)
        valueWidth = 56.0f;
    if (valueWidth > 84.0f)
        valueWidth = 84.0f;
    const float requestedTrackWidth = usableWidth - valueWidth - theme_.itemSpacing;
    const float trackWidth = requestedTrackWidth > 28.0f ? requestedTrackWidth : usableWidth;
    const bool hasValueEditor = requestedTrackWidth > 28.0f;
    const Rect track(rect.x + labelWidth, rect.y + rect.height * 0.4f,
                     trackWidth,
                     rect.height * 0.2f);
    const Rect grab(track.x, rect.y, track.width, rect.height);
    const bool sliderChanged = sliderValue(grab, clip, combineIds(id, 0x534c49444552ull),
                                           value, minimum, maximum);
    const float normalized = clamp((value - minimum) / (maximum - minimum), 0.0f, 1.0f);
    const float handleX = track.x + track.width * normalized;
    drawText(*drawList, theme_.font, labelText,
             Vec2(rect.x, rect.y + (rect.height - metrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, clip);
    const float trackRadius = track.height * 0.5f;
    drawList->addRectFilledRounded(track, trackRadius, theme_.sliderBackground, clip);
    drawList->addRectFilledRounded(Rect(track.x, track.y, track.width * normalized, track.height),
                                   trackRadius, theme_.sliderFilled, clip);
    drawList->addCircleFilled(Vec2(handleX, track.y + track.height * 0.5f),
                              rect.height * 0.28f, theme_.sliderHandle, clip);

    bool textChanged = false;
    if (hasValueEditor)
    {
        const Rect valueRect(track.x + track.width + theme_.itemSpacing, rect.y,
                             valueWidth, rect.height);
        pushId(id);
        textChanged = inputFloat("value", value,
                                 Rect(valueRect.x - layout_.origin.x,
                                      valueRect.y - layout_.origin.y,
                                      valueRect.width, valueRect.height), 3);
        popId();
        if (textChanged)
            value = clamp(value, minimum, maximum);
    }
    return sliderChanged || textChanged;
}

bool Context::sliderInt(StringView labelText, int &value, int minimum, int maximum,
                        const Rect &bounds)
{
    if (maximum <= minimum)
        return false;

    float temporary = static_cast<float>(value);
    const bool changed = sliderFloat(labelText, temporary, static_cast<float>(minimum),
                                     static_cast<float>(maximum), bounds);
    if (!changed)
        return false;

    const int rounded = temporary >= 0.0f ? static_cast<int>(temporary + 0.5f)
                                          : static_cast<int>(temporary - 0.5f);
    const int clamped = rounded < minimum ? minimum : (rounded > maximum ? maximum : rounded);
    const bool valueChanged = value != clamped;
    value = clamped;
    return valueChanged;
}

bool Context::dragFloat(StringView labelText, float &value, float minimum, float maximum,
                        float speed, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || maximum <= minimum || speed <= 0.0f)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    NumericEditState *edit = numericEdits_.find(id);
    if (!edit) { numericEdits_.put(id, NumericEditState()); edit = numericEdits_.find(id); }
    if (!edit) return false;
    const uint32_t left = buttonIndex(PointerButton::Left);
    const Rect visible = intersect(rect, clip);
    const bool hovered = itemHovered(rect, clip, id);
    registerFocusable(id);
    if (edit->editing && (escapePressed_ || focusedWidget_ != id))
        edit->editing = false;
    if (!edit->editing && pointerPressedIn(visible, left) && activeWidget_ == InvalidWidgetId)
    {
        activeWidget_ = id;
        focusedWidget_ = id;
        dragWidget_ = id;
        dragStartValue_ = value;
        dragStartX_ = pointer_.pressedPosition[left].x;
        edit->moved = false;
    }
    bool changed = false;
    if (activeWidget_ == id && dragWidget_ == id)
    {
        const float x = pointer_.released[left] ? pointer_.releasedPosition[left].x : pointer_.position.x;
        const float delta = x - dragStartX_;
        if (fabsf(delta) >= 3.0f) edit->moved = true;
        if (edit->moved)
        {
            const float next = clamp(dragStartValue_ + delta * speed, minimum, maximum);
            changed = next != value;
            value = next;
        }
        if (pointer_.released[left])
        {
            activeWidget_ = InvalidWidgetId;
            dragWidget_ = InvalidWidgetId;
            if (!edit->moved && contains(visible, pointer_.releasedPosition[left]))
            {
                edit->editing = true;
                edit->text = String::number(static_cast<double>(value), 3);
                textCursor_ = edit->text.size();
                textInputWidget_ = id;
            }
        }
    }
    if (edit->editing)
    {
        if (inputText(labelText, edit->text, bounds))
        {
            const float next = edit->text.to_float();
            if (isfinite(next))
            {
                const float limited = clamp(next, minimum, maximum);
                changed = changed || limited != value;
                value = limited;
            }
        }
        if (enterPressed_) { edit->editing = false; focusedWidget_ = InvalidWidgetId; }
    }
    else
    {
        char number[48];
        snprintf(number, sizeof(number), "%.3f", static_cast<double>(value));
        drawFrame(*drawList, rect, hovered ? theme_.buttonHovered : theme_.buttonBackground,
                  hovered ? theme_.inputBorderHover : theme_.buttonBorder, theme_.borderRadius, clip);
        drawText(*drawList, theme_.font, labelText, Vec2(rect.x + 6, rect.y + 5),
                 theme_.fontSize, theme_.buttonText, visible);
        const TextMetrics metrics = measureText(theme_.font, number, theme_.fontSize);
        drawText(*drawList, theme_.font, number, Vec2(rect.right() - metrics.width - 6, rect.y + 5),
                 theme_.fontSize, theme_.buttonText, visible);
    }
    return changed;
}

bool Context::dragInt(StringView labelText, int &value, int minimum, int maximum,
                      int speed, const Rect &bounds)
{
    if (maximum <= minimum || speed <= 0)
        return false;
    float temporary = static_cast<float>(value);
    const bool dragged = dragFloat(labelText, temporary, static_cast<float>(minimum),
                                   static_cast<float>(maximum), static_cast<float>(speed), bounds);
    if (!dragged)
        return false;
    const int rounded = temporary >= 0.0f ? static_cast<int>(temporary + 0.5f)
                                          : static_cast<int>(temporary - 0.5f);
    const int clamped = rounded < minimum ? minimum : (rounded > maximum ? maximum : rounded);
    const bool changed = value != clamped;
    value = clamped;
    return changed;
}

bool Context::splitter(StringView idText, float &value, float minimum, float maximum,
                       SplitterAxis axis, const Rect &bounds, float thickness)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList || maximum <= minimum || thickness <= 0.0f)
        return false;
    const Rect area = contentRect(bounds);
    const Rect clip = contentClip();
    const float clampedValue = clamp(value, minimum, maximum);
    const Rect handle = axis == SplitterAxis::Vertical
        ? Rect(area.x + clampedValue - thickness * 0.5f, area.y, thickness, area.height)
        : Rect(area.x, area.y + clampedValue - thickness * 0.5f, area.width, thickness);
    const WidgetId id = combineIds(makeWidgetId(idText), 0x53504c4954544552ull);
    const uint32_t left = buttonIndex(PointerButton::Left);
    const bool hovered = itemHovered(handle, clip, id);
    if (pointerPressedIn(intersect(handle, clip), left) && activeWidget_ == InvalidWidgetId)
        activeWidget_ = id;
    bool changed = false;
    if (activeWidget_ == id)
    {
        if (pointer_.down[left])
        {
            const float next = (axis == SplitterAxis::Vertical ? pointer_.position.x - area.x
                                                                 : pointer_.position.y - area.y);
            const float limited = clamp(next, minimum, maximum);
            changed = limited != value;
            value = limited;
        }
        if (pointer_.released[left])
            activeWidget_ = InvalidWidgetId;
    }
    drawList->addRectFilled(handle, hovered || activeWidget_ == id ? theme_.dialogBtnPrimary : theme_.borderColor, clip);
    return changed;
}

bool Context::gizmo2D(StringView idText, Transform2D &transform, Gizmo2DMode mode,
                      const Rect &bounds, const Gizmo2DOptions &options)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList || bounds.width <= 0.0f || bounds.height <= 0.0f ||
        options.axisLength <= 12.0f)
        return false;

    const Rect canvas = contentRect(bounds);
    const Rect clip = intersect(canvas, contentClip());
    if (clip.width <= 0.0f || clip.height <= 0.0f)
        return false;
    const WidgetId id = combineIds(makeWidgetId(idText), 0x47495a4d4f3244ull);
    Gizmo2DState *state = gizmo2DStates_.find(id);
    if (!state)
    {
        gizmo2DStates_.put(id, Gizmo2DState());
        state = gizmo2DStates_.find(id);
    }
    if (!state)
        return false;

    const float rotationRadians = transform.rotation * GizmoPi / 180.0f;
    const Vec2 axisX(cosf(rotationRadians), sinf(rotationRadians));
    const Vec2 axisY(sinf(rotationRadians), -cosf(rotationRadians));
    const Vec2 center(canvas.x + transform.position.x, canvas.y + transform.position.y);
    const GizmoAxis2D hoveredAxis = hitGizmo2D(mode, pointer_.position, center,
                                                axisX, axisY, options.axisLength);
    const uint32_t left = buttonIndex(PointerButton::Left);
    const bool pressedHere = pointerPressedIn(clip, left) &&
                             activeWidget_ == InvalidWidgetId;
    if (pressedHere && hoveredAxis != GizmoAxisNone)
    {
        activeWidget_ = id;
        focusedWidget_ = id;
        state->axis = static_cast<uint8_t>(hoveredAxis);
        state->pointerStart = pointer_.pressedPosition[left];
        state->transformStart = transform;
    }

    bool changed = false;
    if (activeWidget_ == id)
    {
        if (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left])
        {
            const GizmoAxis2D activeAxis = static_cast<GizmoAxis2D>(state->axis);
            const float deltaX = pointer_.position.x - state->pointerStart.x;
            const float deltaY = pointer_.position.y - state->pointerStart.y;
            const float alongX = deltaX * axisX.x + deltaY * axisX.y;
            const float alongY = deltaX * axisY.x + deltaY * axisY.y;
            const Transform2D previous = transform;
            if (mode == Gizmo2DMode::Translate)
            {
                if (activeAxis == GizmoAxisXY)
                {
                    transform.position.x = snapGizmoValue(state->transformStart.position.x + deltaX,
                                                          options.translateSnap);
                    transform.position.y = snapGizmoValue(state->transformStart.position.y + deltaY,
                                                          options.translateSnap);
                }
                else if (activeAxis == GizmoAxisX)
                {
                    transform.position.x = snapGizmoValue(state->transformStart.position.x + axisX.x * alongX,
                                                          options.translateSnap);
                    transform.position.y = snapGizmoValue(state->transformStart.position.y + axisX.y * alongX,
                                                          options.translateSnap);
                }
                else if (activeAxis == GizmoAxisY)
                {
                    transform.position.x = snapGizmoValue(state->transformStart.position.x + axisY.x * alongY,
                                                          options.translateSnap);
                    transform.position.y = snapGizmoValue(state->transformStart.position.y + axisY.y * alongY,
                                                          options.translateSnap);
                }
            }
            else if (mode == Gizmo2DMode::Rotate)
            {
                const float startAngle = atan2f(state->pointerStart.y - center.y,
                                                state->pointerStart.x - center.x);
                const float currentAngle = atan2f(pointer_.position.y - center.y,
                                                  pointer_.position.x - center.x);
                const float deltaDegrees = (currentAngle - startAngle) * 180.0f / GizmoPi;
                transform.rotation = snapGizmoValue(state->transformStart.rotation + deltaDegrees,
                                                    options.rotateSnap);
            }
            else if (mode == Gizmo2DMode::Scale)
            {
                if (activeAxis == GizmoAxisXY)
                {
                    const float factor = 1.0f + (alongX + alongY) * 0.5f / options.axisLength;
                    transform.scale.x = state->transformStart.scale.x * factor;
                    transform.scale.y = state->transformStart.scale.y * factor;
                }
                else if (activeAxis == GizmoAxisX)
                {
                    transform.scale.x = state->transformStart.scale.x *
                                        (1.0f + alongX / options.axisLength);
                }
                else if (activeAxis == GizmoAxisY)
                {
                    transform.scale.y = state->transformStart.scale.y *
                                        (1.0f + alongY / options.axisLength);
                }
                if (transform.scale.x < 0.01f)
                    transform.scale.x = 0.01f;
                if (transform.scale.y < 0.01f)
                    transform.scale.y = 0.01f;
                transform.scale.x = snapGizmoValue(transform.scale.x, options.scaleSnap);
                transform.scale.y = snapGizmoValue(transform.scale.y, options.scaleSnap);
            }
            changed = previous.position.x != transform.position.x ||
                      previous.position.y != transform.position.y ||
                      previous.rotation != transform.rotation ||
                      previous.scale.x != transform.scale.x || previous.scale.y != transform.scale.y;
        }
        if (pointer_.released[left])
        {
            activeWidget_ = InvalidWidgetId;
            state->axis = static_cast<uint8_t>(GizmoAxisNone);
        }
    }

    const GizmoAxis2D activeAxis = activeWidget_ == id
        ? static_cast<GizmoAxis2D>(state->axis) : GizmoAxisNone;
    const Color xColor = hoveredAxis == GizmoAxisX || activeAxis == GizmoAxisX
        ? theme_.gizmoHighlight : theme_.gizmoAxisX;
    const Color yColor = hoveredAxis == GizmoAxisY || activeAxis == GizmoAxisY
        ? theme_.gizmoHighlight : theme_.gizmoAxisY;
    const Color centerColor = hoveredAxis == GizmoAxisXY || activeAxis == GizmoAxisXY
        ? theme_.gizmoHighlight : Color(245u, 200u, 75u, 255u);
    if (mode == Gizmo2DMode::Rotate)
    {
        const float radius = options.axisLength * 0.82f;
        const Color ringColor = hoveredAxis == GizmoAxisXY || activeAxis == GizmoAxisXY
            ? theme_.gizmoHighlight : theme_.gizmoAxisZ;
        const int segments = 32;
        for (int i = 0; i < segments; ++i)
        {
            const float first = GizmoPi * 2.0f * static_cast<float>(i) / static_cast<float>(segments);
            const float second = GizmoPi * 2.0f * static_cast<float>(i + 1) / static_cast<float>(segments);
            drawList->addLine(Vec2(center.x + cosf(first) * radius, center.y + sinf(first) * radius),
                              Vec2(center.x + cosf(second) * radius, center.y + sinf(second) * radius),
                              ringColor, clip, 2.0f);
        }

        if (activeAxis == GizmoAxisXY)
        {
            const float startAngle = atan2f(state->pointerStart.y - center.y,
                                            state->pointerStart.x - center.x);
            const float endAngle = atan2f(pointer_.position.y - center.y,
                                          pointer_.position.x - center.x);
            const Vec2 startPoint(center.x + cosf(startAngle) * radius,
                                  center.y + sinf(startAngle) * radius);
            const Vec2 endPoint(center.x + cosf(endAngle) * radius,
                                center.y + sinf(endAngle) * radius);
            const Color startColor(185u, 205u, 230u, 220u);
            const Color endColor(theme_.gizmoHighlight.r, theme_.gizmoHighlight.g, theme_.gizmoHighlight.b, 220u);

            drawList->addLine(center, startPoint, startColor, clip, 1.5f);
            drawList->addLine(center, endPoint, endColor, clip, 2.0f);

            char angleText[32];
            snprintf(angleText, sizeof(angleText), "%.1f deg", transform.rotation);
            const TextMetrics metrics = measureText(theme_.font, StringView(angleText), theme_.fontSize);
            const float labelAngle = startAngle + (endAngle - startAngle) * 0.5f;
            const float labelRadius = radius + 14.0f;
            const Vec2 labelPosition(center.x + cosf(labelAngle) * labelRadius - metrics.width * 0.5f,
                                     center.y + sinf(labelAngle) * labelRadius - metrics.height * 0.5f);
            drawText(*drawList, theme_.font, StringView(angleText), labelPosition, theme_.fontSize,
                     theme_.labelText, clip);
        }
        drawList->addCircleFilled(center, 5.0f, centerColor, clip);
    }
    else
    {
        const Vec2 endX(center.x + axisX.x * options.axisLength,
                        center.y + axisX.y * options.axisLength);
        const Vec2 endY(center.x + axisY.x * options.axisLength,
                        center.y + axisY.y * options.axisLength);
        drawList->addLine(center, endX, xColor, clip, 3.0f);
        drawList->addLine(center, endY, yColor, clip, 3.0f);
        if (mode == Gizmo2DMode::Scale)
        {
            drawList->addRectFilled(Rect(endX.x - 5.0f, endX.y - 5.0f, 10.0f, 10.0f), xColor, clip);
            drawList->addRectFilled(Rect(endY.x - 5.0f, endY.y - 5.0f, 10.0f, 10.0f), yColor, clip);
        }
        else
        {
            drawList->addCircleFilled(endX, 5.0f, xColor, clip);
            drawList->addCircleFilled(endY, 5.0f, yColor, clip);
        }
        drawList->addCircleFilled(center, 7.0f, centerColor, clip);
    }
    return changed;
}

bool Context::gizmo3D(StringView idText, Transform3D &transform, Gizmo3DMode mode,
                      const Rect &bounds, const float *view, const float *projection,
                      const Gizmo3DOptions &options)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList || !view || !projection || options.axisLength <= 0.0f)
        return false;
    const Rect viewport = contentRect(bounds);
    const Rect clip = intersect(viewport, contentClip());
    if (viewport.width <= 0.0f || viewport.height <= 0.0f || clip.width <= 0.0f || clip.height <= 0.0f)
        return false;

    const WidgetId id = combineIds(makeWidgetId(idText), 0x47495a4d4f3344ull);
    Gizmo3DState *state = gizmo3DStates_.find(id);
    if (!state)
    {
        gizmo3DStates_.put(id, Gizmo3DState());
        state = gizmo3DStates_.find(id);
    }
    if (!state)
        return false;

    const Gizmo3DProjector projector = {viewport, view, projection};
    const GizmoAxis3D hoveredAxis = hitGizmo3D(mode, pointer_.position, transform, projector,
                                               options.axisLength);
    const uint32_t left = buttonIndex(PointerButton::Left);
    const bool pressedHere = pointerPressedIn(clip, left) &&
                             activeWidget_ == InvalidWidgetId;
    if (pressedHere && hoveredAxis != Gizmo3DAxisNone)
    {
        activeWidget_ = id;
        focusedWidget_ = id;
        state->axis = static_cast<uint8_t>(hoveredAxis);
        state->pointerStart = pointer_.pressedPosition[left];
        state->transformStart = transform;
        state->ringStart = Vec3();
        if (mode == Gizmo3DMode::Rotate && hoveredAxis >= Gizmo3DAxisX && hoveredAxis <= Gizmo3DAxisZ)
            gizmo3DRingPoint(projector, pointer_.pressedPosition[left], transform.position,
                             gizmo3DAxisVector(hoveredAxis), state->ringStart);
    }

    bool changed = false;
    // The angle the live drag has swept on the ring's plane, for the wedge drawn
    // around the centre below.
    float ringSweep = 0.0f;
    const GizmoAxis3D activeAxis = activeWidget_ == id
        ? static_cast<GizmoAxis3D>(state->axis) : Gizmo3DAxisNone;
    if (activeWidget_ == id && (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left]))
    {
        const float deltaX = pointer_.position.x - state->pointerStart.x;
        const float deltaY = pointer_.position.y - state->pointerStart.y;
        const Transform3D previous = transform;
        Vec2 center;
        projectGizmo3D(projector, state->transformStart.position, center);
        if (mode == Gizmo3DMode::Translate)
        {
            if (activeAxis == Gizmo3DAxisXYZ || activeAxis == Gizmo3DAxisXY ||
                activeAxis == Gizmo3DAxisXZ || activeAxis == Gizmo3DAxisYZ)
            {
                const float horizontal = deltaX * options.axisLength / viewport.width;
                const float vertical = -deltaY * options.axisLength / viewport.height;
                transform.position = state->transformStart.position;
                if (activeAxis == Gizmo3DAxisXYZ || activeAxis == Gizmo3DAxisXY || activeAxis == Gizmo3DAxisXZ)
                    transform.position.x = snapGizmoValue(transform.position.x + horizontal, options.translateSnap);
                if (activeAxis == Gizmo3DAxisXYZ || activeAxis == Gizmo3DAxisXY || activeAxis == Gizmo3DAxisYZ)
                    transform.position.y = snapGizmoValue(transform.position.y + vertical, options.translateSnap);
                if (activeAxis == Gizmo3DAxisXZ || activeAxis == Gizmo3DAxisYZ)
                    transform.position.z = snapGizmoValue(transform.position.z + horizontal, options.translateSnap);
            }
            else if (activeAxis >= Gizmo3DAxisX && activeAxis <= Gizmo3DAxisZ)
            {
                Vec2 endpoint;
                projectGizmo3D(projector, addGizmo3D(state->transformStart.position,
                    scaleGizmo3D(gizmo3DAxisVector(activeAxis), options.axisLength)), endpoint);
                const float axisX = endpoint.x - center.x;
                const float axisY = endpoint.y - center.y;
                const float lengthSquared = axisX * axisX + axisY * axisY;
                if (lengthSquared > 0.0001f)
                {
                    const float amount = (deltaX * axisX + deltaY * axisY) * options.axisLength / lengthSquared;
                    const Vec3 axis = gizmo3DAxisVector(activeAxis);
                    transform.position = addGizmo3D(state->transformStart.position,
                        scaleGizmo3D(axis, snapGizmoValue(amount, options.translateSnap)));
                }
            }
        }
        else if (mode == Gizmo3DMode::Rotate)
        {
            // A ring is turned by its own plane's angle. Turning it by the angle
            // the pointer sweeps on the screen instead reads as the model
            // lagging the hand: the drawn ring is an ellipse, so equal steps
            // across the screen are unequal steps around the model.
            const float startAngle = atan2f(state->pointerStart.y - center.y, state->pointerStart.x - center.x);
            const float currentAngle = atan2f(pointer_.position.y - center.y, pointer_.position.x - center.x);
            float swept = currentAngle - startAngle;
            if (activeAxis >= Gizmo3DAxisX && activeAxis <= Gizmo3DAxisZ)
            {
                const Vec3 normal = gizmo3DAxisVector(activeAxis);
                Vec3 local;
                if (gizmo3DRingPoint(projector, pointer_.position, state->transformStart.position,
                                     normal, local))
                    swept = gizmo3DSweptAngle(normal, state->ringStart, local);
            }
            ringSweep = swept;
            const float amount = snapGizmoValue(swept * 180.0f / GizmoPi, options.rotateSnap);
            transform.rotation = state->transformStart.rotation;
            if (activeAxis == Gizmo3DAxisX) transform.rotation.x += amount;
            if (activeAxis == Gizmo3DAxisY || activeAxis == Gizmo3DAxisXYZ) transform.rotation.y += amount;
            if (activeAxis == Gizmo3DAxisZ) transform.rotation.z += amount;
        }
        else if (mode == Gizmo3DMode::Scale)
        {
            const float factor = 1.0f + (deltaX - deltaY) * 0.005f;
            transform.scale = state->transformStart.scale;
            if (activeAxis == Gizmo3DAxisX || activeAxis == Gizmo3DAxisXYZ ||
                activeAxis == Gizmo3DAxisXY || activeAxis == Gizmo3DAxisXZ) transform.scale.x *= factor;
            if (activeAxis == Gizmo3DAxisY || activeAxis == Gizmo3DAxisXYZ ||
                activeAxis == Gizmo3DAxisXY || activeAxis == Gizmo3DAxisYZ) transform.scale.y *= factor;
            if (activeAxis == Gizmo3DAxisZ || activeAxis == Gizmo3DAxisXYZ ||
                activeAxis == Gizmo3DAxisXZ || activeAxis == Gizmo3DAxisYZ) transform.scale.z *= factor;
            if (transform.scale.x < 0.01f) transform.scale.x = 0.01f;
            if (transform.scale.y < 0.01f) transform.scale.y = 0.01f;
            if (transform.scale.z < 0.01f) transform.scale.z = 0.01f;
            transform.scale.x = snapGizmoValue(transform.scale.x, options.scaleSnap);
            transform.scale.y = snapGizmoValue(transform.scale.y, options.scaleSnap);
            transform.scale.z = snapGizmoValue(transform.scale.z, options.scaleSnap);
        }
        changed = previous.position.x != transform.position.x || previous.position.y != transform.position.y ||
                  previous.position.z != transform.position.z || previous.rotation.x != transform.rotation.x ||
                  previous.rotation.y != transform.rotation.y || previous.rotation.z != transform.rotation.z ||
                  previous.scale.x != transform.scale.x || previous.scale.y != transform.scale.y ||
                  previous.scale.z != transform.scale.z;
        if (pointer_.released[left])
        {
            activeWidget_ = InvalidWidgetId;
            state->axis = static_cast<uint8_t>(Gizmo3DAxisNone);
        }
    }

    // The object first, then the handles on top of it: a ring has to stay
    // grabbable where the box covers it.
    if (options.showModel)
        drawGizmo3DModel(projector, transform, options.axisLength, clip, drawList);

    Vec2 center;
    if (!projectGizmo3D(projector, transform.position, center))
        return changed;
    const Color colors[] = {theme_.gizmoAxisX, theme_.gizmoAxisY,
                            theme_.gizmoAxisZ};
    const GizmoAxis3D planeIds[] = {Gizmo3DAxisXY, Gizmo3DAxisXZ, Gizmo3DAxisYZ};
    const int planeFirst[] = {0, 0, 1};
    const int planeSecond[] = {1, 2, 2};
    const Color planeColors[] = {colors[2], colors[1], colors[0]};
    if (mode != Gizmo3DMode::Rotate)
    {
        const float offsets[][2] = {{0.28f, 0.28f}, {0.62f, 0.28f}, {0.62f, 0.62f}, {0.28f, 0.62f}};
        for (int plane = 0; plane < 3; ++plane)
        {
            const Vec3 firstAxis = gizmo3DAxisVector(static_cast<GizmoAxis3D>(planeFirst[plane] + Gizmo3DAxisX));
            const Vec3 secondAxis = gizmo3DAxisVector(static_cast<GizmoAxis3D>(planeSecond[plane] + Gizmo3DAxisX));
            Vec2 corners[4];
            bool valid = true;
            for (int corner = 0; corner < 4; ++corner)
            {
                const Vec3 world = addGizmo3D(transform.position,
                    addGizmo3D(scaleGizmo3D(firstAxis, offsets[corner][0] * options.axisLength),
                                scaleGizmo3D(secondAxis, offsets[corner][1] * options.axisLength)));
                if (!projectGizmo3D(projector, world, corners[corner]))
                    valid = false;
            }
            if (!valid)
                continue;
            const bool highlighted = planeIds[plane] == hoveredAxis || planeIds[plane] == activeAxis;
            const Color color = highlighted ? Color(theme_.gizmoHighlight.r, theme_.gizmoHighlight.g, theme_.gizmoHighlight.b, 100u)
                                            : Color(planeColors[plane].r, planeColors[plane].g,
                                                    planeColors[plane].b, 55u);
            drawList->addPolygonFilled(Span<const Vec2>(corners), color, clip);
            for (int corner = 0; corner < 4; ++corner)
                drawList->addLine(corners[corner], corners[(corner + 1) % 4],
                                  Color(color.r, color.g, color.b, 180u), clip, 1.0f);
        }
    }
    for (uint8_t index = Gizmo3DAxisX; index <= Gizmo3DAxisZ; ++index)
    {
        const GizmoAxis3D axis = static_cast<GizmoAxis3D>(index);
        const Color color = axis == hoveredAxis || axis == activeAxis
            ? theme_.gizmoHighlight : colors[index - Gizmo3DAxisX];
        if (mode == Gizmo3DMode::Rotate)
        {
            Vec3 u;
            Vec3 v;
            gizmo3DRingBasis(axis, u, v);
            Vec2 previous;
            bool hasPrevious = false;
            for (int segment = 0; segment <= 40; ++segment)
            {
                const float angle = GizmoPi * 2.0f * static_cast<float>(segment) / 40.0f;
                Vec2 current;
                if (!projectGizmo3D(projector, addGizmo3D(transform.position,
                    addGizmo3D(scaleGizmo3D(u, cosf(angle) * options.axisLength),
                                scaleGizmo3D(v, sinf(angle) * options.axisLength))), current))
                    continue;
                if (hasPrevious)
                {
                    // The whole ring is drawn, the way a modelling tool draws
                    // it: the far half faint, so the ring reads as one circle
                    // instead of an arc that stops halfway round.
                    const float middle = angle - GizmoPi / 40.0f;
                    const float radialX = u.x * cosf(middle) + v.x * sinf(middle);
                    const float radialY = u.y * cosf(middle) + v.y * sinf(middle);
                    const float radialZ = u.z * cosf(middle) + v.z * sinf(middle);
                    const float facing = radialX * -view[8] + radialY * -view[9] + radialZ * -view[10];
                    if (facing < 0.0f)
                        drawList->addLine(previous, current,
                                          Color(color.r, color.g, color.b, 80u), clip, 2.0f);
                    else
                        drawList->addLine(previous, current, color, clip, 3.0f);
                }
                previous = current;
                hasPrevious = true;
            }
        }
        else
        {
            Vec2 endpoint;
            if (!projectGizmo3D(projector, addGizmo3D(transform.position,
                scaleGizmo3D(gizmo3DAxisVector(axis), options.axisLength)), endpoint))
                continue;
            drawList->addLine(center, endpoint, color, clip, 3.0f);
            if (mode == Gizmo3DMode::Scale)
                drawList->addRectFilled(Rect(endpoint.x - 5.0f, endpoint.y - 5.0f, 10.0f, 10.0f), color, clip);
            else
            {
                const float directionX = endpoint.x - center.x;
                const float directionY = endpoint.y - center.y;
                const float length = sqrtf(directionX * directionX + directionY * directionY);
                if (length > 0.001f)
                {
                    const float unitX = directionX / length;
                    const float unitY = directionY / length;
                    const Vec2 arrow[] = {
                        Vec2(endpoint.x + unitX * 12.0f, endpoint.y + unitY * 12.0f),
                        Vec2(endpoint.x - unitY * 5.0f, endpoint.y + unitX * 5.0f),
                        Vec2(endpoint.x + unitY * 5.0f, endpoint.y - unitX * 5.0f)
                    };
                    drawList->addPolygonFilled(Span<const Vec2>(arrow), color, clip);
                }
            }
        }
    }
    if (mode == Gizmo3DMode::Rotate)
    {
        const float outerRadius = gizmo3DOuterRadius(projector, transform, options.axisLength);
        const Color outerColor = activeAxis == Gizmo3DAxisXYZ || hoveredAxis == Gizmo3DAxisXYZ
            ? theme_.gizmoHighlight : Color(185u, 185u, 190u, 120u);
        for (int segment = 0; segment < 40; ++segment)
        {
            const float first = GizmoPi * 2.0f * static_cast<float>(segment) / 40.0f;
            const float second = GizmoPi * 2.0f * static_cast<float>(segment + 1) / 40.0f;
            drawList->addLine(Vec2(center.x + cosf(first) * outerRadius, center.y + sinf(first) * outerRadius),
                              Vec2(center.x + cosf(second) * outerRadius, center.y + sinf(second) * outerRadius),
                              outerColor, clip, 1.5f);
        }
        const bool turningRing = activeAxis >= Gizmo3DAxisX && activeAxis <= Gizmo3DAxisZ &&
            (state->ringStart.x != 0.0f || state->ringStart.y != 0.0f || state->ringStart.z != 0.0f);
        if (turningRing)
        {
            // The wedge follows the ring, not the screen: it is the source vector
            // turned around the ring's own axis by the angle the drag has swept,
            // which is the angle the transform is being given.
            const Vec3 normal = gizmo3DAxisVector(activeAxis);
            const float sourceLength = sqrtf(state->ringStart.x * state->ringStart.x +
                                             state->ringStart.y * state->ringStart.y +
                                             state->ringStart.z * state->ringStart.z);
            const Vec3 source(state->ringStart.x / sourceLength, state->ringStart.y / sourceLength,
                              state->ringStart.z / sourceLength);
            const int steps = 24;
            Vec2 arc[steps + 1];
            bool arcValid = true;
            for (int step = 0; step <= steps; ++step)
            {
                const float angle = ringSweep * static_cast<float>(step) / static_cast<float>(steps);
                const float cosine = cosf(angle);
                const float sine = sinf(angle);
                const float crossX = normal.y * source.z - normal.z * source.y;
                const float crossY = normal.z * source.x - normal.x * source.z;
                const float crossZ = normal.x * source.y - normal.y * source.x;
                const float dot = normal.x * source.x + normal.y * source.y + normal.z * source.z;
                const Vec3 turned(source.x * cosine + crossX * sine + normal.x * dot * (1.0f - cosine),
                                  source.y * cosine + crossY * sine + normal.y * dot * (1.0f - cosine),
                                  source.z * cosine + crossZ * sine + normal.z * dot * (1.0f - cosine));
                if (!projectGizmo3D(projector, addGizmo3D(transform.position,
                                                          scaleGizmo3D(turned, options.axisLength)), arc[step]))
                    arcValid = false;
            }
            if (arcValid)
            {
                const Color feedback(255u, 160u, 50u, 65u);
                for (int step = 0; step < steps; ++step)
                {
                    const Vec2 wedge[] = {center, arc[step], arc[step + 1]};
                    drawList->addPolygonFilled(Span<const Vec2>(wedge), feedback, clip);
                }
                const Color edge(theme_.gizmoHighlight.r, theme_.gizmoHighlight.g,
                                 theme_.gizmoHighlight.b, 200u);
                drawList->addLine(center, arc[0], edge, clip, 1.5f);
                drawList->addLine(center, arc[steps], edge, clip, 1.5f);
            }
        }
        else if (activeAxis == Gizmo3DAxisXYZ)
        {
            const float start = atan2f(state->pointerStart.y - center.y, state->pointerStart.x - center.x);
            const float end = atan2f(pointer_.position.y - center.y, pointer_.position.x - center.x);
            const float radius = outerRadius * 0.92f;
            const Color feedback(255u, 160u, 50u, 65u);
            for (int segment = 0; segment < 24; ++segment)
            {
                const float first = start + (end - start) * static_cast<float>(segment) / 24.0f;
                const float second = start + (end - start) * static_cast<float>(segment + 1) / 24.0f;
                const Vec2 wedge[] = {center,
                    Vec2(center.x + cosf(first) * radius, center.y + sinf(first) * radius),
                    Vec2(center.x + cosf(second) * radius, center.y + sinf(second) * radius)};
                drawList->addPolygonFilled(Span<const Vec2>(wedge), feedback, clip);
            }
            const Color edge(theme_.gizmoHighlight.r, theme_.gizmoHighlight.g,
                             theme_.gizmoHighlight.b, 200u);
            drawList->addLine(center, Vec2(center.x + cosf(start) * radius, center.y + sinf(start) * radius),
                              edge, clip, 1.5f);
            drawList->addLine(center, Vec2(center.x + cosf(end) * radius, center.y + sinf(end) * radius),
                              edge, clip, 1.5f);
        }
        char rotationText[64];
        snprintf(rotationText, sizeof(rotationText), "X %.1f  Y %.1f  Z %.1f",
                 transform.rotation.x, transform.rotation.y, transform.rotation.z);
        const TextMetrics metrics = measureText(theme_.font, StringView(rotationText), theme_.fontSize);
        const Vec2 textPosition(center.x - metrics.width * 0.5f, center.y + outerRadius + 8.0f);
        drawText(*drawList, theme_.font, StringView(rotationText),
                 Vec2(textPosition.x + 1.0f, textPosition.y + 1.0f), theme_.fontSize,
                 Color(0u, 0u, 0u, 180u), clip);
        drawText(*drawList, theme_.font, StringView(rotationText), textPosition, theme_.fontSize,
                 theme_.labelText, clip);
    }
    drawList->addCircleFilled(center, 6.0f,
                              hoveredAxis == Gizmo3DAxisXYZ || activeAxis == Gizmo3DAxisXYZ
                                  ? theme_.gizmoHighlight : Color(245u, 200u, 75u, 255u), clip);
    return changed;
}

bool Context::stepperInt(StringView labelText, int &value, int minimum, int maximum,
                         const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || maximum < minimum)
        return false;

    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const float buttonWidth = rect.height;
    const float valueWidth = rect.height * 1.7f;
    const float controlsWidth = buttonWidth * 2.0f + valueWidth;
    const float controlsX = rect.x + (rect.width > controlsWidth ? rect.width - controlsWidth : 0.0f);
    const Rect decrement(controlsX, rect.y, buttonWidth, rect.height);
    const Rect valueRect(controlsX + buttonWidth, rect.y, valueWidth, rect.height);
    const Rect increment(controlsX + buttonWidth + valueWidth, rect.y, buttonWidth, rect.height);
    const WidgetId id = makeWidgetId(labelText);
    const WidgetId decrementId = combineIds(id, 1u);
    const WidgetId incrementId = combineIds(id, 2u);
    const bool decrementHovered = itemHovered(decrement, clip, decrementId);
    const bool incrementHovered = itemHovered(increment, clip, incrementId);
    const bool decrementClicked = itemClicked(decrement, clip, decrementId);
    const bool incrementClicked = itemClicked(increment, clip, incrementId);

    bool changed = false;
    if (decrementClicked && value > minimum)
    {
        --value;
        changed = true;
    }
    if (incrementClicked && value < maximum)
    {
        ++value;
        changed = true;
    }

    const TextMetrics labelMetrics = measureText(theme_.font, labelText, theme_.fontSize);
    drawText(*drawList, theme_.font, labelText,
             Vec2(rect.x, rect.y + (rect.height - labelMetrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, clip);
    drawFrame(*drawList, decrement, decrementHovered ? theme_.buttonHovered : theme_.buttonBackground,
              decrementHovered ? theme_.inputBorderHover : theme_.buttonBorder, theme_.borderRadius, clip);
    drawFrame(*drawList, valueRect, theme_.inputBg, theme_.inputBorder, theme_.borderRadius, clip);
    drawFrame(*drawList, increment, incrementHovered ? theme_.buttonHovered : theme_.buttonBackground,
              incrementHovered ? theme_.inputBorderHover : theme_.buttonBorder, theme_.borderRadius, clip);

    const String displayed = String::number(value);
    const TextMetrics valueMetrics = measureText(theme_.font, displayed, theme_.fontSize);
    const TextMetrics minusMetrics = measureText(theme_.font, StringView("-"), theme_.fontSize);
    const TextMetrics plusMetrics = measureText(theme_.font, StringView("+"), theme_.fontSize);
    drawText(*drawList, theme_.font, StringView("-"),
             Vec2(decrement.x + (decrement.width - minusMetrics.width) * 0.5f,
                  decrement.y + (decrement.height - minusMetrics.height) * 0.5f),
             theme_.fontSize, theme_.buttonText, clip);
    drawText(*drawList, theme_.font, displayed,
             Vec2(valueRect.x + (valueRect.width - valueMetrics.width) * 0.5f,
                  valueRect.y + (valueRect.height - valueMetrics.height) * 0.5f),
             theme_.fontSize, theme_.buttonText, clip);
    drawText(*drawList, theme_.font, StringView("+"),
             Vec2(increment.x + (increment.width - plusMetrics.width) * 0.5f,
                  increment.y + (increment.height - plusMetrics.height) * 0.5f),
             theme_.fontSize, theme_.buttonText, clip);
    return changed;
}

namespace
{

bool isUtf8Continuation(char byte)
{
    return (static_cast<uint8_t>(byte) & 0xC0u) == 0x80u;
}

String::size_type previousCodePoint(const String &text, String::size_type offset)
{
    if (offset == 0u)
        return 0u;
    --offset;
    while (offset != 0u && isUtf8Continuation(text[offset]))
        --offset;
    return offset;
}

String::size_type nextCodePoint(const String &text, String::size_type offset)
{
    if (offset >= text.size())
        return text.size();
    ++offset;
    while (offset < text.size() && isUtf8Continuation(text[offset]))
        ++offset;
    return offset;
}

String::size_type lineStart(const String &text, String::size_type offset)
{
    while (offset != 0u && text[offset - 1u] != '\n')
        --offset;
    return offset;
}

String::size_type lineEnd(const String &text, String::size_type offset)
{
    while (offset < text.size() && text[offset] != '\n')
        ++offset;
    return offset;
}

// The offset `column` code points into the line starting at `start`,
// clamped to that line's end.
String::size_type offsetAtColumn(const String &text, String::size_type start, int column)
{
    const String::size_type end = lineEnd(text, start);
    String::size_type offset = start;
    for (int i = 0; i < column && offset < end; ++i)
        offset = nextCodePoint(text, offset);
    return offset;
}

int columnOf(const String &text, String::size_type offset)
{
    int column = 0;
    for (String::size_type i = lineStart(text, offset); i < offset; i = nextCodePoint(text, i))
        ++column;
    return column;
}

// Inserts text at cursor; a single-line field drops line breaks.
bool insertText(String &value, String::size_type &cursor, const char *text, String::size_type length,
                bool multiline)
{
    bool inserted = false;
    for (String::size_type i = 0u; i < length; ++i)
    {
        if (!multiline && (text[i] == '\n' || text[i] == '\r'))
            continue;
        value.insert(cursor, text + i, 1u);
        ++cursor;
        inserted = true;
    }
    return inserted;
}

} // namespace

bool Context::editTextBuffer(String &value, String::size_type &cursor, bool multiline)
{
    bool changed = false;
    if (cursor > value.size())
        cursor = value.size();
    if (homePressed_)
        cursor = multiline ? lineStart(value, cursor) : 0u;
    if (endPressed_)
        cursor = multiline ? lineEnd(value, cursor) : value.size();
    if (leftPressed_)
        cursor = previousCodePoint(value, cursor);
    if (rightPressed_)
        cursor = nextCodePoint(value, cursor);
    if (multiline && (upPressed_ || downPressed_))
    {
        const int column = columnOf(value, cursor);
        const String::size_type start = lineStart(value, cursor);
        if (upPressed_)
            cursor = start == 0u ? 0u : offsetAtColumn(value, lineStart(value, start - 1u), column);
        else
        {
            const String::size_type end = lineEnd(value, cursor);
            cursor = end == value.size() ? end : offsetAtColumn(value, end + 1u, column);
        }
    }
    if (copyRequested_)
        backend_.setClipboardText(value);
    if (pasteRequested_)
    {
        const String clipboard = backend_.clipboardText();
        changed = insertText(value, cursor, clipboard.data(), clipboard.size(), multiline) || changed;
    }
    if (multiline && enterPressed_)
    {
        value.insert(cursor, "\n", 1u);
        ++cursor;
        changed = true;
    }
    for (ct::Vector<Event>::size_type i = 0; i < textEvents_.size(); ++i)
    {
        const Event &event = textEvents_[i];
        changed = insertText(value, cursor, event.text, event.textLength, multiline) || changed;
    }
    if (backspacePressed_ && cursor != 0u)
    {
        const String::size_type eraseBegin = previousCodePoint(value, cursor);
        value.erase(eraseBegin, cursor - eraseBegin);
        cursor = eraseBegin;
        changed = true;
    }
    if (keyPressed_[static_cast<uint32_t>(KeyCode::Delete)] && cursor < value.size())
    {
        value.erase(cursor, nextCodePoint(value, cursor) - cursor);
        changed = true;
    }
    return changed;
}

bool Context::inputText(StringView labelText, String &value, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    const Rect textClip = intersect(rect, clip);
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;
    const bool hovered = itemHovered(rect, clip, id);
    itemClicked(rect, clip, id);
    const bool focused = hasKeyboardFocus(id);
    bool changed = false;
    if (focused)
    {
        if (textInputWidget_ != id)
            textCursor_ = value.size();
        textInputWidget_ = id;
        wantsKeyboard_ = true;
        wantsTextInput_ = true;
        changed = editTextBuffer(value, textCursor_, false);
    }

    const Color background = focused ? theme_.buttonHovered
                                     : (hovered ? theme_.buttonHovered : theme_.buttonBackground);
    drawFrame(*drawList, rect, background,
              focused ? theme_.focusColor : (hovered ? theme_.inputBorderHover : theme_.inputBorder),
              theme_.borderRadius, clip);
    const TextMetrics metrics = measureText(theme_.font, value, theme_.fontSize);
    const float leftPadding = theme_.windowPadding * 0.5f;
    const float availableWidth = rect.width - theme_.windowPadding;
    float horizontalOffset = metrics.width > availableWidth ? availableWidth - metrics.width : 0.0f;
    // textCursor_ belongs to the field that has focus: for any other field the
    // view would run past the end of its string (and the caret math below only
    // uses it while focused).
    StringView::size_type caretBytes = 0u;
    if (focused && textCursor_ > 0)
        caretBytes = static_cast<StringView::size_type>(textCursor_) < value.size()
                         ? static_cast<StringView::size_type>(textCursor_)
                         : value.size();
    const StringView prefix(value.data(), caretBytes);
    const TextMetrics prefixMetrics = measureText(theme_.font, prefix, theme_.fontSize);
    if (focused && prefixMetrics.width + horizontalOffset < 0.0f)
        horizontalOffset = -prefixMetrics.width;
    else if (focused && prefixMetrics.width + horizontalOffset > availableWidth)
        horizontalOffset = availableWidth - prefixMetrics.width;
    const Vec2 textPosition(rect.x + leftPadding + horizontalOffset,
                            rect.y + (rect.height - metrics.height) * 0.5f);
    drawText(*drawList, theme_.font, value, textPosition, theme_.fontSize, theme_.buttonText, textClip);
    if (focused)
    {
        const float caretX = textPosition.x + prefixMetrics.width + 1.0f;
        drawList->addRectFilled(Rect(caretX, rect.y + 5.0f, 1.0f,
                                     rect.height > 10.0f ? rect.height - 10.0f : 0.0f),
                                theme_.buttonText, textClip);
    }
    return changed;
}

bool Context::inputTextMultiline(StringView labelText, String &value, const Rect &bounds,
                                 bool followTail)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    uint32_t lineCount = 1u;
    for (String::size_type i = 0u; i < value.size(); ++i)
    {
        if (value[i] == '\n')
            ++lineCount;
    }
    const TextMetrics lineMetrics = measureText(theme_.font, StringView("M"), theme_.fontSize);
    const float lineHeight = lineMetrics.height > 0.0f ? lineMetrics.height : theme_.fontSize;
    const float padding = theme_.textEditPadding;
    const float innerHeight = rect.height - padding * 2.0f;
    const int visibleLines = innerHeight >= lineHeight
        ? static_cast<int>(innerHeight / lineHeight) : 1;
    const int maximumScroll = static_cast<int>(lineCount) - visibleLines;
    const bool hasScrollbar = maximumScroll > 0;
    const float scrollbarWidth = hasScrollbar ? theme_.scrollbarWidth : 0.0f;
    const Rect textArea(rect.x, rect.y, rect.width - scrollbarWidth, rect.height);
    const Rect textClip = intersect(Rect(textArea.x + padding, textArea.y + padding,
                                         textArea.width - padding * 2.0f,
                                         textArea.height - padding * 2.0f), clip);
    const Rect scrollbar(rect.x + rect.width - scrollbarWidth, rect.y, scrollbarWidth, rect.height);
    const WidgetId scrollbarId = combineIds(id, 0x544558545343524Cull);
    int *scroll = textScrolls_.find(id);
    if (!scroll)
    {
        textScrolls_.put(id, 0);
        scroll = textScrolls_.find(id);
    }
    if (!scroll)
        return false;
    if (*scroll < 0)
        *scroll = 0;
    if (*scroll > maximumScroll)
        *scroll = maximumScroll > 0 ? maximumScroll : 0;

    const bool hovered = itemHovered(textArea, clip, id);
    itemClicked(textArea, clip, id);
    if (hasScrollbar && pointerOver(intersect(rect, clip)) && pointer_.wheelY != 0.0f)
    {
        const int delta = pointer_.wheelY > 0.0f ? -1 : 1;
        *scroll += delta;
        if (*scroll < 0)
            *scroll = 0;
        if (*scroll > maximumScroll)
            *scroll = maximumScroll;
    }

    Rect thumb;
    if (hasScrollbar)
    {
        const float requestedThumb = scrollbar.height * static_cast<float>(visibleLines) /
                                     static_cast<float>(lineCount);
        const float thumbHeight = requestedThumb > theme_.scrollbarMinThumb
            ? requestedThumb : theme_.scrollbarMinThumb;
        const float travel = scrollbar.height - thumbHeight;
        const float offset = maximumScroll > 0
            ? travel * static_cast<float>(*scroll) / static_cast<float>(maximumScroll) : 0.0f;
        thumb = Rect(scrollbar.x, scrollbar.y + offset, scrollbar.width, thumbHeight);
        const uint32_t left = buttonIndex(PointerButton::Left);
        const bool pressedThumb = pointerPressedIn(intersect(thumb, clip), left) &&
                                  activeWidget_ == InvalidWidgetId;
        if (pressedThumb)
        {
            activeWidget_ = scrollbarId;
            focusedWidget_ = id;
        }
        if (activeWidget_ == scrollbarId)
        {
            if (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left])
            {
                const float normalized = travel > 0.0f
                    ? clamp((pointer_.position.y - scrollbar.y - thumb.height * 0.5f) / travel,
                            0.0f, 1.0f) : 0.0f;
                *scroll = static_cast<int>(normalized * static_cast<float>(maximumScroll) + 0.5f);
            }
            if (pointer_.released[left])
                activeWidget_ = InvalidWidgetId;
        }
        const float updatedOffset = maximumScroll > 0
            ? travel * static_cast<float>(*scroll) / static_cast<float>(maximumScroll) : 0.0f;
        thumb.y = scrollbar.y + updatedOffset;
    }

    const bool focused = hasKeyboardFocus(id);
    bool changed = false;
    if (focused)
    {
        if (textInputWidget_ != id)
            textCursor_ = value.size();
        textInputWidget_ = id;
        wantsKeyboard_ = true;
        wantsTextInput_ = true;
        changed = editTextBuffer(value, textCursor_, true);
    }

    if (focused)
    {
        uint32_t cursorLine = 0u;
        for (String::size_type i = 0u; i < textCursor_; ++i)
        {
            if (value[i] == '\n')
                ++cursorLine;
        }
        if (static_cast<int>(cursorLine) < *scroll)
            *scroll = static_cast<int>(cursorLine);
        else if (static_cast<int>(cursorLine) >= *scroll + visibleLines)
            *scroll = static_cast<int>(cursorLine) - visibleLines + 1;
        if (*scroll > maximumScroll)
            *scroll = maximumScroll > 0 ? maximumScroll : 0;
    }

    // Tail follow (followTail callers - the editor's output console): when
    // the text grew and the view was already at the end, keep the last line
    // visible; a view scrolled up to read history is left where it is.
    if (followTail && focusedWidget_ != id)
    {
        int *lastLineCount = textTailLines_.find(id);
        if (!lastLineCount)
        {
            textTailLines_.put(id, 0);
            lastLineCount = textTailLines_.find(id);
        }
        if (lastLineCount)
        {
            const int previousMaximum = *lastLineCount - visibleLines;
            if (static_cast<int>(lineCount) > *lastLineCount &&
                *scroll >= (previousMaximum > 0 ? previousMaximum : 0))
                *scroll = maximumScroll > 0 ? maximumScroll : 0;
            *lastLineCount = static_cast<int>(lineCount);
        }
    }

    const Color background = focused ? theme_.buttonHovered
                                     : (hovered ? theme_.buttonHovered : theme_.inputBg);
    drawFrame(*drawList, rect, background,
              focused ? theme_.focusColor : (hovered ? theme_.inputBorderHover : theme_.inputBorder),
              theme_.borderRadius, clip);
    drawText(*drawList, theme_.font, value,
             Vec2(textArea.x + padding, textArea.y + padding - static_cast<float>(*scroll) * lineHeight),
             theme_.fontSize, theme_.buttonText, textClip);
    if (hasScrollbar)
    {
        drawList->addRectFilled(scrollbar, theme_.sliderBackground, clip);
        drawList->addRectFilled(thumb, theme_.scrollbarThumb, clip);
    }
    if (focused)
    {
        String::size_type lineStart = 0u;
        uint32_t line = 0u;
        for (String::size_type i = 0u; i < textCursor_; ++i)
        {
            if (value[i] == '\n')
            {
                lineStart = i + 1u;
                ++line;
            }
        }
        const StringView linePrefix(value.data() + lineStart, textCursor_ - lineStart);
        const TextMetrics prefixMetrics = measureText(theme_.font, linePrefix, theme_.fontSize);
        const float caretX = textArea.x + padding + prefixMetrics.width;
        const float caretY = textArea.y + padding +
                             (static_cast<float>(line) - static_cast<float>(*scroll)) * lineHeight;
        if (caretY >= textArea.y + padding && caretY < textArea.y + textArea.height - padding)
            drawList->addRectFilled(Rect(caretX, caretY, 1.0f, theme_.fontSize),
                                    theme_.buttonText, textClip);
    }
    return changed;
}

namespace
{

// "[+-]digits", clamped to int's range. Anything else - including the
// in-between states "", "-", "+" - is not a value yet.
bool parseIntText(const String &text, int &result)
{
    String::size_type i = 0u;
    const bool negative = i < text.size() && text[i] == '-';
    if (i < text.size() && (text[i] == '-' || text[i] == '+'))
        ++i;
    if (i == text.size())
        return false;
    long long magnitude = 0;
    for (; i < text.size(); ++i)
    {
        if (text[i] < '0' || text[i] > '9')
            return false;
        if (magnitude <= 4294967296LL) // stop growing well past int's range
            magnitude = magnitude * 10 + (text[i] - '0');
    }
    const long long signedValue = negative ? -magnitude : magnitude;
    result = signedValue > INT_MAX ? INT_MAX : (signedValue < INT_MIN ? INT_MIN : static_cast<int>(signedValue));
    return true;
}

// A plain decimal literal: [+-]digits[.digits][(e|E)[+-]digits], at least one
// mantissa digit. strtof alone would also take "nan", "inf" and hex, and
// stop silently at trailing garbage. Non-finite results are rejected.
bool parseFloatText(const String &text, float &result)
{
    String::size_type i = 0u;
    if (i < text.size() && (text[i] == '-' || text[i] == '+'))
        ++i;
    bool digits = false;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9')
    {
        ++i;
        digits = true;
    }
    if (i < text.size() && text[i] == '.')
    {
        ++i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9')
        {
            ++i;
            digits = true;
        }
    }
    if (!digits)
        return false;
    if (i < text.size() && (text[i] == 'e' || text[i] == 'E'))
    {
        ++i;
        if (i < text.size() && (text[i] == '-' || text[i] == '+'))
            ++i;
        bool exponentDigits = false;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9')
        {
            ++i;
            exponentDigits = true;
        }
        if (!exponentDigits)
            return false;
    }
    if (i != text.size())
        return false;
    const float parsed = text.to_float();
    if (!(parsed == parsed) || parsed > FLT_MAX || parsed < -FLT_MAX)
        return false;
    result = parsed;
    return true;
}

} // namespace

bool Context::editNumericText(StringView labelText, String &text, double value, const Rect &bounds)
{
    const WidgetId id = makeWidgetId(labelText);
    // Resume the user's text unless the application changed the value since;
    // then the field shows the new value, with the caret at its end.
    if (numericEditId_ == id && numericEditValue_ == value)
        text = numericEditText_;
    else if (numericEditId_ == id && textInputWidget_ == id)
        textCursor_ = text.size();
    const bool edited = inputText(labelText, text, bounds);
    if (focusedWidget_ == id)
    {
        numericEditId_ = id;
        numericEditText_ = text;
    }
    else if (numericEditId_ == id)
    {
        numericEditId_ = InvalidWidgetId;
    }
    return edited;
}

bool Context::inputInt(StringView labelText, int &value, const Rect &bounds)
{
    String text = String::number(value);
    const bool edited = editNumericText(labelText, text, static_cast<double>(value), bounds);
    int parsed = value;
    const bool changed = edited && parseIntText(text, parsed) && parsed != value;
    if (changed)
        value = parsed;
    if (numericEditId_ == makeWidgetId(labelText))
        numericEditValue_ = static_cast<double>(value);
    return changed;
}

bool Context::inputFloat(StringView labelText, float &value, const Rect &bounds, int precision)
{
    String text = String::number(static_cast<double>(value), precision);
    const bool edited = editNumericText(labelText, text, static_cast<double>(value), bounds);
    float parsed = value;
    const bool changed = edited && parseFloatText(text, parsed) && parsed != value;
    if (changed)
        value = parsed;
    if (numericEditId_ == makeWidgetId(labelText))
        numericEditValue_ = static_cast<double>(value);
    return changed;
}

bool Context::colorEdit(StringView labelText, Color &value, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const TextMetrics titleMetrics = measureText(theme_.font, labelText, theme_.fontSize);
    const float contentY = rect.y + titleMetrics.height + theme_.itemSpacing;
    const float availableHeight = rect.y + rect.height - contentY - theme_.windowPadding * 0.5f;
    const float maximumPickerWidth = (rect.width - 48.0f) * 0.55f;
    const float pickerSize = availableHeight < maximumPickerWidth ? availableHeight : maximumPickerWidth;
    if (pickerSize <= 0.0f)
        return false;

    const float spacing = theme_.windowPadding;
    const float hueWidth = 14.0f;
    const Rect saturationValue(rect.x, contentY, pickerSize, pickerSize);
    const Rect hueBar(saturationValue.x + saturationValue.width + spacing, contentY,
                      hueWidth, pickerSize);
    const Rect alphaBar(hueBar.x + hueBar.width + spacing, contentY,
                        hueWidth, pickerSize);
    ColorPickerState *picker = colorPickers_.find(id);
    if (!picker)
    {
        colorPickers_.put(id, ColorPickerState());
        picker = colorPickers_.find(id);
    }
    if (!picker)
        return false;

    // RGB cannot represent hue for greys and black.  Keep the last hue chosen
    // by the user so dragging through the left/bottom edge of the SV square
    // does not make the next drag jump back to red.
    if (!picker->initialized || picker->lastColor != value)
    {
        float nextHue = 0.0f;
        float nextSaturation = 0.0f;
        float nextBrightness = 0.0f;
        colorToHsv(value, nextHue, nextSaturation, nextBrightness);
        if (!picker->initialized || nextSaturation > 0.0001f)
            picker->hue = nextHue;
        picker->saturation = nextSaturation;
        picker->brightness = nextBrightness;
        picker->lastColor = value;
        picker->initialized = true;
    }

    float hue = picker->hue;
    float saturation = picker->saturation;
    float brightness = picker->brightness;
    float alpha = static_cast<float>(value.a) / 255.0f;
    bool changed = false;
    const uint32_t left = buttonIndex(PointerButton::Left);
    const WidgetId saturationValueId = combineIds(id, 1u);
    const WidgetId hueId = combineIds(id, 2u);
    const WidgetId alphaId = combineIds(id, 3u);
    itemHovered(saturationValue, clip, saturationValueId);
    itemHovered(hueBar, clip, hueId);
    itemHovered(alphaBar, clip, alphaId);

    const bool pressSaturationValue = pointerPressedIn(intersect(saturationValue, clip), left) &&
                                      activeWidget_ == InvalidWidgetId;
    if (pressSaturationValue)
    {
        activeWidget_ = saturationValueId;
        focusedWidget_ = saturationValueId;
    }
    if (activeWidget_ == saturationValueId)
    {
        if (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left])
        {
            const float nextSaturation = clamp((pointer_.position.x - saturationValue.x) /
                                               saturationValue.width, 0.0f, 1.0f);
            const float nextBrightness = 1.0f - clamp((pointer_.position.y - saturationValue.y) /
                                                       saturationValue.height, 0.0f, 1.0f);
            changed = changed || nextSaturation != saturation || nextBrightness != brightness;
            saturation = nextSaturation;
            brightness = nextBrightness;
        }
        if (pointer_.released[left])
            activeWidget_ = InvalidWidgetId;
    }

    const bool pressHue = pointerPressedIn(intersect(hueBar, clip), left) &&
                          activeWidget_ == InvalidWidgetId;
    if (pressHue)
    {
        activeWidget_ = hueId;
        focusedWidget_ = hueId;
    }
    if (activeWidget_ == hueId)
    {
        if (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left])
        {
            const float nextHue = clamp((pointer_.position.y - hueBar.y) / hueBar.height, 0.0f, 1.0f);
            changed = changed || nextHue != hue;
            hue = nextHue;
        }
        if (pointer_.released[left])
            activeWidget_ = InvalidWidgetId;
    }

    const bool pressAlpha = pointerPressedIn(intersect(alphaBar, clip), left) &&
                            activeWidget_ == InvalidWidgetId && alphaBar.width > 0.0f;
    if (pressAlpha)
    {
        activeWidget_ = alphaId;
        focusedWidget_ = alphaId;
    }
    if (activeWidget_ == alphaId)
    {
        if (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left])
        {
            const float nextAlpha = clamp((pointer_.position.y - alphaBar.y) / alphaBar.height, 0.0f, 1.0f);
            changed = changed || nextAlpha != alpha;
            alpha = nextAlpha;
        }
        if (pointer_.released[left])
            activeWidget_ = InvalidWidgetId;
    }

    if (changed)
    {
        value = colorFromHsv(hue, saturation, brightness,
                             static_cast<uint8_t>(alpha * 255.0f + 0.5f));
        picker->hue = hue;
        picker->saturation = saturation;
        picker->brightness = brightness;
        picker->lastColor = value;
    }

    drawText(*drawList, theme_.font, labelText, Vec2(rect.x, rect.y), theme_.fontSize,
             theme_.labelText, clip);
    // A single quad has a visible diagonal because the GPU splits it into two
    // triangles. Thin horizontal bands preserve the SV curve without a seam.
    const uint32_t saturationValueBands = 32u;
    for (uint32_t band = 0u; band < saturationValueBands; ++band)
    {
        const float topValue = 1.0f - static_cast<float>(band) /
                                       static_cast<float>(saturationValueBands);
        const float bottomValue = 1.0f - static_cast<float>(band + 1u) /
                                          static_cast<float>(saturationValueBands);
        const Rect bandRect(saturationValue.x,
                            saturationValue.y + saturationValue.height * static_cast<float>(band) /
                                                    static_cast<float>(saturationValueBands),
                            saturationValue.width,
                            saturationValue.height / static_cast<float>(saturationValueBands));
        const Color topGray(static_cast<uint8_t>(topValue * 255.0f + 0.5f),
                             static_cast<uint8_t>(topValue * 255.0f + 0.5f),
                             static_cast<uint8_t>(topValue * 255.0f + 0.5f), 255u);
        const Color bottomGray(static_cast<uint8_t>(bottomValue * 255.0f + 0.5f),
                                static_cast<uint8_t>(bottomValue * 255.0f + 0.5f),
                                static_cast<uint8_t>(bottomValue * 255.0f + 0.5f), 255u);
        drawList->addRectGradient(bandRect, topGray,
                                  colorFromHsv(hue, 1.0f, topValue, 255u),
                                  colorFromHsv(hue, 1.0f, bottomValue, 255u), bottomGray, clip);
    }
    for (uint32_t segment = 0u; segment < 6u; ++segment)
    {
        const float segmentHeight = hueBar.height / 6.0f;
        const Rect segmentRect(hueBar.x, hueBar.y + segmentHeight * static_cast<float>(segment),
                               hueBar.width, segmentHeight);
        const Color top = colorFromHsv(static_cast<float>(segment) / 6.0f, 1.0f, 1.0f, 255u);
        const Color bottom = colorFromHsv(static_cast<float>(segment + 1u) / 6.0f, 1.0f, 1.0f, 255u);
        drawList->addRectGradient(segmentRect, top, top, bottom, bottom, clip);
    }
    const float checkerSize = alphaBar.height > 0.0f ? alphaBar.height / 8.0f : 0.0f;
    if (checkerSize > 0.0f)
    {
        for (uint32_t y = 0u; y < 8u; ++y)
        {
            for (uint32_t x = 0u; x < 2u; ++x)
            {
                const Color checker = (x + y) % 2u == 0u ? Color(68u, 68u, 75u, 255u)
                                                           : Color(108u, 108u, 115u, 255u);
                drawList->addRectFilled(Rect(alphaBar.x + alphaBar.width * 0.5f * static_cast<float>(x),
                                             alphaBar.y + checkerSize * static_cast<float>(y),
                                             alphaBar.width * 0.5f, checkerSize), checker, clip);
            }
        }
        const Color transparent(value.r, value.g, value.b, 0u);
        drawList->addRectGradient(alphaBar, transparent, transparent, value, value, clip);
    }

    const Vec2 saturationValueCursor(saturationValue.x + saturation * saturationValue.width,
                                     saturationValue.y + (1.0f - brightness) * saturationValue.height);
    drawList->addCircleFilled(saturationValueCursor, 6.0f, Color(255u, 255u, 255u, 255u), clip);
    drawList->addCircleFilled(saturationValueCursor, 3.5f,
                              colorFromHsv(hue, saturation, brightness, 255u), clip);
    const float hueCursorY = hueBar.y + hue * hueBar.height;
    drawList->addRectFilled(Rect(hueBar.x - 2.0f, hueCursorY - 1.0f, hueBar.width + 4.0f, 2.0f),
                            Color(255u, 255u, 255u, 255u), clip);
    const float alphaCursorY = alphaBar.y + alpha * alphaBar.height;
    drawList->addRectFilled(Rect(alphaBar.x - 2.0f, alphaCursorY - 1.0f, alphaBar.width + 4.0f, 2.0f),
                            Color(255u, 255u, 255u, 255u), clip);
    return changed;
}

bool Context::gradientEditor(StringView idText, ct::Vector<GradientStop> &stops,
                             int &selectedStop, const Rect &bounds)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList || bounds.width <= 12.0f || bounds.height <= 20.0f)
        return false;

    const Rect area = contentRect(bounds);
    const Rect clip = contentClip();
    const Rect bar(area.x + 6.0f, area.y + 5.0f, area.width - 12.0f,
                   area.height - 18.0f);
    if (bar.width <= 0.0f || bar.height <= 0.0f)
        return false;

    for (ct::Vector<GradientStop>::size_type i = 0u; i < stops.size(); ++i)
        stops[i].position = clamp(stops[i].position, 0.0f, 1.0f);
    ct::sort(stops.begin(), stops.end(), [](const GradientStop &a, const GradientStop &b) {
        return a.position < b.position;
    });
    if (selectedStop >= static_cast<int>(stops.size())) selectedStop = -1;

    const auto sample = [&stops](float position) -> Color {
        if (stops.empty()) return Color(255u, 255u, 255u, 255u);
        if (position <= stops[0].position) return stops[0].color;
        for (ct::Vector<GradientStop>::size_type i = 1u; i < stops.size(); ++i) {
            if (position <= stops[i].position) {
                const float span = stops[i].position - stops[i - 1u].position;
                const float t = span > 0.00001f ? (position - stops[i - 1u].position) / span : 0.0f;
                return stops[i - 1u].color.Lerp(stops[i].color, t);
            }
        }
        return stops.back().color;
    };
    const auto handleRect = [&bar](float position) -> Rect {
        const float x = bar.x + clamp(position, 0.0f, 1.0f) * bar.width;
        return Rect(x - 5.0f, bar.bottom() + 2.0f, 10.0f, 10.0f);
    };

    const WidgetId id = combineIds(makeWidgetId(idText), 0x4752414449454e54ull);
    const uint32_t left = buttonIndex(PointerButton::Left);
    const uint32_t right = buttonIndex(PointerButton::Right);
    bool changed = false;
    int hit = -1;
    for (int i = static_cast<int>(stops.size()) - 1; i >= 0; --i) {
        if (contains(handleRect(stops[static_cast<ct::Vector<GradientStop>::size_type>(i)].position), pointer_.position)) {
            hit = i;
            break;
        }
    }
    // Handles sit inside area, so one gate on it covers bar and handles.
    const Rect visibleArea = intersect(area, clip);
    if (pointerPressedIn(visibleArea, right) && hit > 0 &&
        hit + 1 < static_cast<int>(stops.size())) {
        stops.erase(stops.begin() + hit);
        selectedStop = -1;
        changed = true;
    }
    if (pointerPressedIn(visibleArea, left) && activeWidget_ == InvalidWidgetId) {
        if (hit >= 0) {
            selectedStop = hit;
            activeWidget_ = id;
        } else if (contains(bar, pointer_.pressedPosition[left])) {
            const float position = clamp((pointer_.pressedPosition[left].x - bar.x) / bar.width, 0.0f, 1.0f);
            stops.push_back(GradientStop(position, sample(position)));
            ct::sort(stops.begin(), stops.end(), [](const GradientStop &a, const GradientStop &b) {
                return a.position < b.position;
            });
            for (ct::Vector<GradientStop>::size_type i = 0u; i < stops.size(); ++i)
                if (stops[i].position == position) { selectedStop = static_cast<int>(i); break; }
            activeWidget_ = id;
            changed = true;
        }
    }
    if (activeWidget_ == id) {
        if (pointer_.down[left] && selectedStop >= 0 && selectedStop < static_cast<int>(stops.size())) {
            GradientStop &stop = stops[static_cast<ct::Vector<GradientStop>::size_type>(selectedStop)];
            const float next = clamp((pointer_.position.x - bar.x) / bar.width, 0.0f, 1.0f);
            changed = changed || stop.position != next;
            stop.position = next;
        }
        if (pointer_.released[left]) {
            activeWidget_ = InvalidWidgetId;
            if (selectedStop >= 0 && selectedStop < static_cast<int>(stops.size())) {
                const float selectedPosition = stops[static_cast<ct::Vector<GradientStop>::size_type>(selectedStop)].position;
                ct::sort(stops.begin(), stops.end(), [](const GradientStop &a, const GradientStop &b) {
                    return a.position < b.position;
                });
                for (ct::Vector<GradientStop>::size_type i = 0u; i < stops.size(); ++i)
                    if (stops[i].position == selectedPosition) { selectedStop = static_cast<int>(i); break; }
            }
        }
    }

    const int slices = 32;
    for (int i = 0; i < slices; ++i) {
        const float a = static_cast<float>(i) / static_cast<float>(slices);
        const float b = static_cast<float>(i + 1) / static_cast<float>(slices);
        drawList->addRectGradient(Rect(bar.x + a * bar.width, bar.y, (b - a) * bar.width + 1.0f, bar.height),
                                  sample(a), sample(b), sample(b), sample(a), clip);
    }
    drawList->addRect(bar, theme_.inputBorderHover, clip);
    for (ct::Vector<GradientStop>::size_type i = 0u; i < stops.size(); ++i) {
        const Rect handle = handleRect(stops[i].position);
        const Color border = static_cast<int>(i) == selectedStop ? theme_.focusColor : theme_.dialogBorder;
        drawList->addRectFilled(handle, stops[i].color, clip);
        drawList->addRect(handle, border, clip, static_cast<int>(i) == selectedStop ? 2.0f : 1.0f);
    }
    return changed;
}

bool Context::curveEditor(StringView idText, ct::Vector<CurvePoint> &points, int &selectedPoint,
                          const Rect &bounds, const Vec2 &minimum, const Vec2 &maximum)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList || bounds.width <= 24.0f || bounds.height <= 24.0f ||
        maximum.x <= minimum.x || maximum.y <= minimum.y)
        return false;
    const Rect canvas = contentRect(bounds).shrunk(6.0f);
    const Rect clip = contentClip();
    const auto pointToScreen = [&canvas, &minimum, &maximum](const CurvePoint &point) -> Vec2 {
        return Vec2(canvas.x + (point.x - minimum.x) / (maximum.x - minimum.x) * canvas.width,
                    canvas.bottom() - (point.y - minimum.y) / (maximum.y - minimum.y) * canvas.height);
    };
    const auto screenToPoint = [&canvas, &minimum, &maximum](const Vec2 &position) -> CurvePoint {
        return CurvePoint(clamp(minimum.x + (position.x - canvas.x) / canvas.width * (maximum.x - minimum.x), minimum.x, maximum.x),
                          clamp(minimum.y + (canvas.bottom() - position.y) / canvas.height * (maximum.y - minimum.y), minimum.y, maximum.y));
    };
    ct::sort(points.begin(), points.end(), [](const CurvePoint &a, const CurvePoint &b) { return a.x < b.x; });
    if (selectedPoint >= static_cast<int>(points.size())) selectedPoint = -1;
    const WidgetId id = combineIds(makeWidgetId(idText), 0x4355525645454449ull);
    const uint32_t left = buttonIndex(PointerButton::Left);
    const uint32_t right = buttonIndex(PointerButton::Right);
    int hit = -1;
    for (int i = static_cast<int>(points.size()) - 1; i >= 0; --i) {
        const Vec2 p = pointToScreen(points[static_cast<ct::Vector<CurvePoint>::size_type>(i)]);
        if (contains(Rect(p.x - 6.0f, p.y - 6.0f, 12.0f, 12.0f), pointer_.position)) { hit = i; break; }
    }
    bool changed = false;
    // Point handles overhang canvas by 6px, which is exactly the margin
    // canvas was shrunk by, so the widget's own rect covers them.
    const Rect visibleArea = intersect(contentRect(bounds), clip);
    if (pointerPressedIn(visibleArea, right) && hit > 0 && hit + 1 < static_cast<int>(points.size())) {
        points.erase(points.begin() + hit);
        selectedPoint = -1;
        changed = true;
    }
    if (pointerPressedIn(visibleArea, left) && activeWidget_ == InvalidWidgetId) {
        if (hit >= 0) { selectedPoint = hit; activeWidget_ = id; }
        else if (contains(canvas, pointer_.pressedPosition[left])) {
            const CurvePoint point = screenToPoint(pointer_.pressedPosition[left]);
            points.push_back(point);
            ct::sort(points.begin(), points.end(), [](const CurvePoint &a, const CurvePoint &b) { return a.x < b.x; });
            for (ct::Vector<CurvePoint>::size_type i = 0u; i < points.size(); ++i)
                if (points[i].x == point.x && points[i].y == point.y) { selectedPoint = static_cast<int>(i); break; }
            activeWidget_ = id;
            changed = true;
        }
    }
    if (activeWidget_ == id) {
        if (pointer_.down[left] && selectedPoint >= 0 && selectedPoint < static_cast<int>(points.size())) {
            CurvePoint next = screenToPoint(pointer_.position);
            CurvePoint &point = points[static_cast<ct::Vector<CurvePoint>::size_type>(selectedPoint)];
            changed = changed || point.x != next.x || point.y != next.y;
            point = next;
        }
        if (pointer_.released[left]) {
            activeWidget_ = InvalidWidgetId;
            if (selectedPoint >= 0 && selectedPoint < static_cast<int>(points.size())) {
                const CurvePoint selected = points[static_cast<ct::Vector<CurvePoint>::size_type>(selectedPoint)];
                ct::sort(points.begin(), points.end(), [](const CurvePoint &a, const CurvePoint &b) { return a.x < b.x; });
                for (ct::Vector<CurvePoint>::size_type i = 0u; i < points.size(); ++i)
                    if (points[i].x == selected.x && points[i].y == selected.y) { selectedPoint = static_cast<int>(i); break; }
            }
        }
    }
    drawList->addRectFilled(canvas, theme_.inputBg, clip);
    for (int line = 1; line < 4; ++line) {
        const float x = canvas.x + canvas.width * static_cast<float>(line) * 0.25f;
        const float y = canvas.y + canvas.height * static_cast<float>(line) * 0.25f;
        drawList->addLine(Vec2(x, canvas.y), Vec2(x, canvas.bottom()), theme_.borderColor, clip);
        drawList->addLine(Vec2(canvas.x, y), Vec2(canvas.right(), y), theme_.borderColor, clip);
    }
    for (ct::Vector<CurvePoint>::size_type i = 1u; i < points.size(); ++i)
        drawList->addLine(pointToScreen(points[i - 1u]), pointToScreen(points[i]), theme_.dialogBtnPrimary, clip, 2.0f);
    for (ct::Vector<CurvePoint>::size_type i = 0u; i < points.size(); ++i) {
        const Vec2 p = pointToScreen(points[i]);
        drawList->addCircleFilled(p, static_cast<int>(i) == selectedPoint ? 5.0f : 4.0f,
                                  static_cast<int>(i) == selectedPoint ? theme_.focusColor : theme_.dialogBtnPrimary, clip);
    }
    drawList->addRect(canvas, theme_.inputBorderHover, clip);
    return changed;
}

void Context::updateTimeView(WidgetId id, const Rect& area, const Rect& ruler, TimeView& view)
{
    const WidgetId panId = combineIds(id, 0x50414e);
    const auto middle = buttonIndex(PointerButton::Middle);
    const Rect visible = intersect(area, contentClip());
    const auto left = buttonIndex(PointerButton::Left);
    const WidgetId scrollId = combineIds(id, 0x5343524f4c4c);
    const Rect scroll(ruler.x, area.bottom(), ruler.width, 12);
    // The zoom buttons sit inside area; the scroll strip hangs 12px below it.
    const Rect controls = intersect(Rect(area.x, area.y, area.width, area.height + 12.0f), contentClip());
    if (pointerPressedIn(controls, left) && activeWidget_ == InvalidWidgetId) {
        const Vec2 p = pointer_.pressedPosition[left];
        for (int i=0;i<3;++i) {
            const Rect button(area.x+2+i*36,area.y+2,34,20);
            if (contains(intersect(button,contentClip()),p)) {
                const float center = view.offset + .5f/view.zoom;
                view.zoom = i==2 ? 1.f : clamp(view.zoom*(i==0 ? 1.f/1.5f : 1.5f),1,64);
                view.offset = clamp(center-.5f/view.zoom,0,1-1/view.zoom);
                // This press belongs to the viewport controls.
                pointer_.pressed[left] = false;
            }
        }
        if (contains(intersect(scroll,contentClip()),p)) activeWidget_ = scrollId;
    }
    if (activeWidget_ == scrollId) {
        const float x = pointer_.released[left] ? pointer_.releasedPosition[left].x : pointer_.position.x;
        view.offset = clamp((x-scroll.x)/scroll.width-.5f/view.zoom,0,1-1/view.zoom);
        if (pointer_.released[left] || !pointer_.down[left]) {
            activeWidget_ = InvalidWidgetId;
            pointer_.pressed[left] = false;
        }
    }
    if (pointerOver(visible) && activeWidget_ == InvalidWidgetId && pointer_.wheelY != 0) {
        const float anchor = clamp((pointer_.position.x-ruler.x)/ruler.width, 0, 1);
        const float at = view.offset + anchor/view.zoom;
        view.zoom = clamp(view.zoom * powf(1.2f, pointer_.wheelY), 1, 64);
        view.offset = clamp(at-anchor/view.zoom, 0, 1-1/view.zoom);
        pointer_.wheelY = 0;
    }
    if (pointerPressedIn(visible, middle) && activeWidget_ == InvalidWidgetId) {
        activeWidget_ = panId;
        view.panX = pointer_.pressedPosition[middle].x; view.panOffset = view.offset;
    }
    if (activeWidget_ == panId) {
        const float x = pointer_.released[middle] ? pointer_.releasedPosition[middle].x : pointer_.position.x;
        view.offset = clamp(view.panOffset-(x-view.panX)/(ruler.width*view.zoom),0,1-1/view.zoom);
        if (pointer_.released[middle] || !pointer_.down[middle]) activeWidget_ = InvalidWidgetId;
    }
}

void Context::drawTimeRuler(const Rect& area, const Rect& ruler, const TimeView& view, int first, double count)
{
    DrawList* draw = currentDrawList();
    if (!draw || count <= 0) return;
    const Rect clip = intersect(contentClip(),Rect(ruler.x,area.y,ruler.width,area.height));
    const double pixelsPerFrame = ruler.width*view.zoom/count;
    double step = 1;
    while (step*pixelsPerFrame < 60) step *= 2;
    const double visibleFirst = first + view.offset*count;
    const double visibleLast = visibleFirst + count/view.zoom;
    for (double tick=ceil(visibleFirst/step)*step; tick<=visibleLast && tick<=first+count; tick+=step) {
        const float x = ruler.x + static_cast<float>((tick-visibleFirst)*pixelsPerFrame);
        char label[32]; snprintf(label,sizeof(label),"%.0f",tick);
        draw->addLine(Vec2(x,area.y),Vec2(x,area.bottom()),theme_.borderColor,clip);
        drawText(*draw,theme_.font,label,Vec2(x+3,area.y+4),theme_.fontSize*.75f,theme_.labelText,clip);
    }
}

void Context::drawTimeView(const Rect& area, const Rect& ruler, const TimeView& view)
{
    DrawList* draw = currentDrawList();
    if (!draw) return;
    const Rect clip = contentClip();
    const char* labels[] = {"-", "+", "Fit"};
    for (int i=0;i<3;++i) {
        const Rect button(area.x+2+i*36,area.y+2,34,20);
        draw->addRectFilled(button,theme_.buttonBackground,clip);
        drawText(*draw,theme_.font,labels[i],Vec2(button.x+6,button.y+3),theme_.fontSize*.8f,theme_.buttonText,intersect(button,clip));
    }
    const Rect scroll(ruler.x,area.bottom(),ruler.width,12);
    draw->addRectFilled(scroll,theme_.panelColor,clip);
    draw->addRectFilled(Rect(scroll.x+view.offset*scroll.width,scroll.y+2,
                            scroll.width/view.zoom,8),theme_.sliderHandle,clip);
}

bool Context::sequencer(StringView idText, ct::Vector<SequencerTrack>& tracks, int firstFrame, int lastFrame,
                        int& currentFrame, int& selectedTrack, const Rect& bounds)
{
    DrawList* list = currentDrawList();
    if (!currentWindow() || !list || lastFrame < firstFrame || bounds.width < 100 || bounds.height < 48) return false;
    Rect area = contentRect(bounds);
    area.height -= 14;
    const Rect clip = intersect(area, contentClip());
    const float names = area.width < 180 ? area.width * .35f : 140.f, rulerHeight = 24.f, rowHeight = 24.f;
    const Rect ruler(area.x + names, area.y, area.width - names, rulerHeight);
    const int count = lastFrame - firstFrame + 1;
    const WidgetId viewId = makeWidgetId(idText);
    if (!timeViews_.find(viewId)) timeViews_.put(viewId, TimeView());
    TimeView& view = *timeViews_.find(viewId);
    updateTimeView(viewId, area, ruler, view);
    const auto frameX = [&](int frame) { return ruler.x + ((frame-firstFrame)/static_cast<float>(count)-view.offset)*ruler.width*view.zoom; };
    const auto frameAt = [&](float x) { return clamp(firstFrame + static_cast<int>(((x-ruler.x)/(ruler.width*view.zoom)+view.offset)*count), firstFrame,lastFrame); };
    currentFrame = clamp(currentFrame, firstFrame, lastFrame);
    const WidgetId id = makeWidgetId(idText);
    SequenceDrag *drag = sequenceDrags_.find(id);
    if (!drag) { sequenceDrags_.put(id, SequenceDrag()); drag = sequenceDrags_.find(id); }
    if (!drag) return false;
    bool changed = false; const uint32_t left = buttonIndex(PointerButton::Left);
    if (pointerPressedIn(clip, left) && activeWidget_ == InvalidWidgetId) {
        const Vec2 p = pointer_.pressedPosition[left];
        if (contains(ruler, p)) { const int next = frameAt(p.x); changed = next != currentFrame; currentFrame = next; }
        else if (contains(intersect(area, clip), p) && p.y >= ruler.bottom()) {
            const int row = static_cast<int>((p.y-ruler.bottom())/rowHeight);
            if (row >= 0 && row < static_cast<int>(tracks.size())) {
                selectedTrack = row;
                const auto& t = tracks[row];
                const Rect bar(frameX(t.startFrame), ruler.bottom()+row*rowHeight+3,
                               frameX(t.endFrame+1)-frameX(t.startFrame), rowHeight-6);
                if (contains(bar,p) && t.startFrame >= firstFrame && t.endFrame <= lastFrame && t.endFrame >= t.startFrame) {
                    activeWidget_ = id;
                    drag->row = row; drag->start = t.startFrame; drag->end = t.endFrame; drag->x = p.x;
                    drag->mode = p.x < bar.x+5 ? 1 : (p.x > bar.right()-5 ? 2 : 3);
                }
            }
        }
    }
    if (activeWidget_ == id && drag->row >= 0 && drag->row < static_cast<int>(tracks.size())) {
        auto& t = tracks[drag->row];
        const float x = pointer_.released[left] ? pointer_.releasedPosition[left].x : pointer_.position.x;
        const int delta = static_cast<int>(roundf((x-drag->x)*count/(ruler.width*view.zoom)));
        int start = drag->start, end = drag->end;
        if (drag->mode == 1) start = clamp(start+delta, firstFrame, end);
        else if (drag->mode == 2) end = clamp(end+delta, start, lastFrame);
        else { start = clamp(start+delta, firstFrame, lastFrame-(end-start)); end = start+drag->end-drag->start; }
        changed = changed || t.startFrame != start || t.endFrame != end;
        t.startFrame = start; t.endFrame = end;
        if (pointer_.released[left]) { activeWidget_ = InvalidWidgetId; drag->row = -1; }
    }
    list->addRectFilled(area, theme_.inputBg, clip);
    list->addRectFilled(Rect(area.x,area.y,names,area.height), theme_.panelColor, clip);
    const Rect timeClip = intersect(clip, Rect(ruler.x, area.y, ruler.width, area.height));
    drawTimeRuler(area,ruler,view,firstFrame,count);
    for (ct::Vector<SequencerTrack>::size_type i=0;i<tracks.size();++i) { const float y=ruler.bottom()+i*rowHeight; if(y>=area.bottom()) break; const SequencerTrack& t=tracks[i]; drawText(*list,theme_.font,t.label,Vec2(area.x+6,y+4),theme_.fontSize*.85f,theme_.dialogText,clip); Rect bar(frameX(t.startFrame),y+3,frameX(t.endFrame+1)-frameX(t.startFrame),rowHeight-6); list->addRectFilled(bar,t.color,timeClip); if (bar.width >= 10) { list->addRectFilled(Rect(bar.x+2,bar.y+3,2,bar.height-6),theme_.buttonText,timeClip); list->addRectFilled(Rect(bar.right()-4,bar.y+3,2,bar.height-6),theme_.buttonText,timeClip); } list->addRect(bar,static_cast<int>(i)==selectedTrack?theme_.focusColor:theme_.dialogBorder,timeClip); list->addLine(Vec2(area.x,y+rowHeight),Vec2(area.right(),y+rowHeight),theme_.borderColor,clip); }
    const float playhead=frameX(currentFrame); list->addLine(Vec2(playhead,ruler.y),Vec2(playhead,area.bottom()),theme_.focusColor,timeClip,2); list->addRect(area,theme_.inputBorderHover,clip);
    drawTimeView(area,ruler,view);
    return changed;
}

bool Context::timeline(StringView idText, ct::Vector<TimelineTrack>& tracks, int first, int last,
                       int& frame, int& selectedTrack, int& selectedKey, const Rect& bounds)
{
    DrawList* draw = currentDrawList();
    if (!currentWindow() || !draw || last <= first || bounds.width < 180 || bounds.height < 48) return false;
    Rect area = contentRect(bounds);
    area.height -= 14;
    const Rect clip = intersect(area, contentClip());
    const Rect ruler(area.x+120, area.y, area.width-126, 24);
    const WidgetId viewId = makeWidgetId(idText);
    if (!timeViews_.find(viewId)) timeViews_.put(viewId, TimeView());
    TimeView& view = *timeViews_.find(viewId);
    updateTimeView(viewId, area, ruler, view);
    const auto xAt = [&](int f) { return ruler.x + ((static_cast<double>(f)-first)/(static_cast<double>(last)-first)-view.offset)*ruler.width*view.zoom; };
    const auto frameAt = [&](float x) { return static_cast<int>(clamp(roundf(first+((x-ruler.x)/(ruler.width*view.zoom)+view.offset)*(static_cast<double>(last)-first)), first, last)); };
    const WidgetId id = makeWidgetId(idText);
    const auto left = buttonIndex(PointerButton::Left), right = buttonIndex(PointerButton::Right);
    bool changed = false;
    if (activeWidget_ == InvalidWidgetId &&
        (pointerPressedIn(clip, left) || pointerPressedIn(clip, right))) {
        const bool remove = pointer_.pressed[right];
        const Vec2 p = pointer_.pressedPosition[remove ? right : left];
        if (contains(clip,p) && contains(ruler,p) && !remove) {
            frame = frameAt(p.x); selectedTrack = -1; selectedKey = -1; activeWidget_ = id;
        } else if (contains(clip,p) && p.x >= ruler.x && p.y >= ruler.bottom()) {
            const int row = static_cast<int>((p.y-ruler.bottom())/24);
            if (row < static_cast<int>(tracks.size())) {
                auto& keys = tracks[row].keys;
                int hit = -1;
                for (int k=0;k<static_cast<int>(keys.size());++k)
                    if (fabs(static_cast<double>(p.x)-xAt(keys[k])) <= 7.0) { hit=k; break; }
                if (remove && hit >= 0) { keys.erase(keys.begin()+hit); selectedKey=-1; changed=true; }
                else if (!remove) {
                    if (hit < 0) { keys.push_back(frameAt(p.x)); hit=static_cast<int>(keys.size())-1; changed=true; }
                    selectedTrack=row; selectedKey=hit; activeWidget_=id;
                }
            }
        }
    }
    if (activeWidget_ == id) {
        const int next = frameAt(pointer_.released[left] ? pointer_.releasedPosition[left].x : pointer_.position.x);
        if (selectedTrack >= 0 && selectedTrack < static_cast<int>(tracks.size()) &&
            selectedKey >= 0 && selectedKey < static_cast<int>(tracks[selectedTrack].keys.size())) {
            int& key = tracks[selectedTrack].keys[selectedKey]; changed = changed || key!=next; key=next;
        } else { changed = changed || frame!=next; frame=next; }
        if (pointer_.released[left]) activeWidget_=InvalidWidgetId;
    }
    draw->addRectFilled(area,theme_.inputBg,clip);
    const Rect timeClip = intersect(clip, Rect(ruler.x, area.y, ruler.width, area.height));
    drawTimeRuler(area,ruler,view,first,static_cast<double>(last)-first);
    for (int row=0;row<static_cast<int>(tracks.size());++row) {
        const float y=ruler.bottom()+row*24;
        if (y>=area.bottom()) break;
        drawText(*draw,theme_.font,tracks[row].label,Vec2(area.x+4,y+4),theme_.fontSize*.8f,theme_.labelText,intersect(clip,Rect(area.x,y,116,24)));
        for (int k=0;k<static_cast<int>(tracks[row].keys.size());++k) {
            const float x=xAt(tracks[row].keys[k]), cy=y+12;
            const Vec2 diamond[] = {Vec2(x,cy-5),Vec2(x+5,cy),Vec2(x,cy+5),Vec2(x-5,cy)};
            draw->addPolygonFilled(Span<const Vec2>(diamond),row==selectedTrack && k==selectedKey ? theme_.focusColor : theme_.dialogBtnPrimary,timeClip);
        }
    }
    draw->addLine(Vec2(xAt(frame),area.y),Vec2(xAt(frame),area.bottom()),theme_.focusColor,timeClip,2);
    draw->addRect(area,theme_.inputBorderHover,clip);
    drawTimeView(area,ruler,view);
    return changed;
}

void Context::image(TextureId texture, const Rect &bounds, const Vec2 &uvMin,
                    const Vec2 &uvMax, const Color &tint)
{
    if (!currentWindow() || texture.value == 0u)
        return;

    DrawList *drawList = currentDrawList();
    if (!drawList)
        return;

    const Rect rect = contentRect(bounds);
    drawList->addImage(texture, rect, uvMin, uvMax, tint, contentClip());
}

bool Context::imageButton(StringView labelText, TextureId texture, const Rect &bounds,
                          const Vec2 &uvMin, const Vec2 &uvMax, const Color &tint)
{
    WindowState *window = currentWindow();
    if (!window || texture.value == 0u)
        return false;

    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const bool hovered = itemHovered(rect, clip, id);
    const bool clicked = itemClicked(rect, clip, id);
    const float inset = rect.width < rect.height ? rect.width * 0.08f : rect.height * 0.08f;
    const Rect imageRect(rect.x + inset, rect.y + inset,
                         rect.width - inset * 2.0f, rect.height - inset * 2.0f);
    drawList->addRectFilled(rect, hovered ? theme_.buttonHovered : theme_.buttonBackground, clip);
    drawList->addImage(texture, imageRect, uvMin, uvMax, tint, clip);
    if (hovered)
        drawList->addRectFilled(imageRect, Color(255u, 255u, 255u, 24u), clip);
    return clicked;
}

bool Context::smallImageButton(StringView labelText, TextureId texture, const Rect &bounds,
                               const Vec2 &uvMin, const Vec2 &uvMax, const Color &tint)
{
    WindowState *window = currentWindow();
    if (!window || texture.value == 0u)
        return false;
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;
    const WidgetId id = combineIds(makeWidgetId(labelText), 0x534d414c4c494d47ull);
    const bool hovered = itemHovered(rect, clip, id);
    const bool clicked = itemClicked(rect, clip, id);
    const float inset = rect.width < rect.height ? rect.width * 0.16f : rect.height * 0.16f;
    const Rect imageRect(rect.x + inset, rect.y + inset,
                         rect.width - inset * 2.0f, rect.height - inset * 2.0f);
    drawList->addRectFilled(rect, hovered ? theme_.buttonHovered : theme_.buttonBackground, clip);
    drawList->addImage(texture, imageRect, uvMin, uvMax, tint, clip);
    if (hovered)
        drawList->addRect(imageRect, theme_.focusColor, clip);
    return clicked;
}

bool Context::invisibleButton(StringView idText, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;
    const Rect rect = contentRect(bounds);
    const WidgetId id = combineIds(makeWidgetId(idText), 0x494e56495349424cull);
    return itemClicked(rect, contentClip(), id);
}

bool Context::isHovered(StringView idText, const Rect &bounds)
{
    if (!currentWindow())
        return false;
    const Rect rect = contentRect(bounds);
    const WidgetId id = combineIds(makeWidgetId(idText), 0x494e56495349424cull);
    return itemHovered(rect, contentClip(), id);
}

bool Context::isClicked(StringView idText, const Rect &bounds)
{
    return invisibleButton(idText, bounds);
}

void Context::drawRectFilled(const Rect &bounds, const Color &color)
{
    DrawList *drawList = currentDrawList();
    if (drawList)
        drawList->addRectFilled(contentRect(bounds), color, contentClip());
}

void Context::drawRect(const Rect &bounds, const Color &color, float thickness)
{
    DrawList *drawList = currentDrawList();
    if (drawList && thickness > 0.0f)
        drawList->addRect(contentRect(bounds), color, contentClip(), thickness);
}

void Context::drawLine(const Vec2 &from, const Vec2 &to, const Color &color, float thickness)
{
    DrawList *drawList = currentDrawList();
    if (!drawList || thickness <= 0.0f)
        return;
    const Rect origin = contentRect(Rect());
    drawList->addLine(Vec2(origin.x + from.x, origin.y + from.y),
                      Vec2(origin.x + to.x, origin.y + to.y), color, contentClip(), thickness);
}

void Context::drawArc(const Vec2 &center, float radius, float fromRadians,
                      float toRadians, const Color &color, float thickness)
{
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return;
    const Rect origin = contentRect(Rect());
    drawList->addArc(Vec2(origin.x + center.x, origin.y + center.y), radius,
                     fromRadians, toRadians, color, contentClip(), thickness);
}

void Context::drawPolygonFilled(Span<const Vec2> points, const Color &color)
{
    DrawList *drawList = currentDrawList();
    if (!drawList || points.size() < 3u)
        return;
    ct::Vector<Vec2> absolute;
    for (size_t i = 0u; i < points.size(); ++i)
    {
        const Rect placed = contentRect(Rect(points[i].x, points[i].y, 0.0f, 0.0f));
        absolute.push_back(Vec2(placed.x, placed.y));
    }
    drawList->addPolygonFilled(Span<const Vec2>(absolute.data(), absolute.size()), color, contentClip());
}

void Context::drawRectFilledRounded(const Rect &bounds, float radius, const Color &color)
{
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return;
    drawList->addRectFilledRounded(contentRect(bounds), radius, color, contentClip());
}

void Context::drawRectRounded(const Rect &bounds, float radius, const Color &color,
                              float thickness)
{
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return;
    drawList->addRectRounded(contentRect(bounds), radius, color, contentClip(), thickness);
}

void Context::drawCircleFilled(const Vec2 &center, float radius, const Color &color)
{
    DrawList *drawList = currentDrawList();
    if (!drawList || radius <= 0.0f)
        return;
    const Rect origin = contentRect(Rect());
    drawList->addCircleFilled(Vec2(origin.x + center.x, origin.y + center.y), radius, color, contentClip());
}

void Context::progressBar(float value, float maximum, const Rect &bounds)
{
    if (!currentWindow())
        return;

    DrawList *drawList = currentDrawList();
    if (!drawList)
        return;

    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    const float normalized = maximum > 0.0f ? clamp(value / maximum, 0.0f, 1.0f) : 0.0f;
    drawList->addRectFilledRounded(rect, theme_.borderRadius, theme_.progressBackground, clip);
    drawList->addRectFilledRounded(Rect(rect.x, rect.y, rect.width * normalized, rect.height),
                                   theme_.borderRadius, theme_.progressFilled, clip);
}

MessageBoxResult Context::messageBox(StringView title, StringView message, bool &open,
                                     bool showCancel)
{
    MessageBoxOptions options;
    options.showCancel = showCancel;
    return messageBox(title, message, open, options);
}

MessageBoxResult Context::messageBox(StringView title, StringView message, bool &open,
                                     const MessageBoxOptions &options)
{
    const WidgetId id = combineIds(hashText(title), hashText(message));
    if (!open)
    {
        if (activeModal_ == id)
            activeModal_ = InvalidWidgetId;
        return MessageBoxResult::None;
    }

    // On screen since an earlier frame: only then may Enter accept it, so
    // the Enter that opened it (on a focused button) cannot also close it.
    const bool shownLastFrame = activeModal_ == id && activeModalFrame_ + 1u == frameNumber_;
    activeModal_ = id;
    activeModalFrame_ = frameNumber_;
    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const TextMetrics titleMetrics = measureText(theme_.font, title, theme_.fontSize);
    const TextMetrics messageMetrics = measureText(theme_.font, message, theme_.fontSize);
    const float padding = theme_.windowPadding * 2.0f;
    const float desiredWidth = messageMetrics.width + padding;
    const float width = desiredWidth < 300.0f ? 300.0f
                      : (desiredWidth > viewport.width - 24.0f ? viewport.width - 24.0f : desiredWidth);
    const bool inputMode = options.kind == MessageBoxKind::Input && options.inputValue;
    const float inputHeight = inputMode ? theme_.widgetHeight + theme_.itemSpacing : 0.0f;
    const float height = titleMetrics.height + messageMetrics.height + inputHeight + theme_.widgetHeight +
                         padding * 2.0f + theme_.itemSpacing * 2.0f;
    const Rect dialog((viewport.width - width) * 0.5f, (viewport.height - height) * 0.5f,
                      width > 0.0f ? width : 0.0f, height > 0.0f ? height : 0.0f);
    const Rect titleBar(dialog.x, dialog.y, dialog.width, titleMetrics.height + theme_.windowPadding * 1.5f);
    const float buttonWidth = 80.0f;
    const float buttonY = dialog.y + dialog.height - theme_.windowPadding - theme_.widgetHeight;
    const Rect acceptButton(dialog.x + dialog.width - theme_.windowPadding - buttonWidth,
                            buttonY, buttonWidth, theme_.widgetHeight);
    const Rect cancelButton(acceptButton.x - theme_.itemSpacing - buttonWidth,
                            buttonY, buttonWidth, theme_.widgetHeight);
    const Rect discardButton(cancelButton.x-theme_.itemSpacing-buttonWidth,buttonY,buttonWidth,theme_.widgetHeight);
    const WidgetId discardId=combineIds(id,0x44495343415244ull);
    const Rect inputRect(dialog.x + theme_.windowPadding,
                         titleBar.y + titleBar.height + messageMetrics.height + theme_.itemSpacing * 2.0f,
                         dialog.width - theme_.windowPadding * 2.0f, theme_.widgetHeight);
    const WidgetId acceptId = combineIds(id, 0x4f4bull);
    const WidgetId cancelId = combineIds(id, 0x43414e43454cull);
    const uint32_t left = buttonIndex(PointerButton::Left);
    const bool hoveredAccept = contains(acceptButton, pointer_.position);
    const bool hoveredCancel = options.showCancel && contains(cancelButton, pointer_.position);
    if (pointer_.pressed[left] && activeWidget_ == InvalidWidgetId)
    {
        if (contains(acceptButton, pointer_.pressedPosition[left]))
            activeWidget_ = acceptId;
        else if (options.showCancel && contains(cancelButton, pointer_.pressedPosition[left]))
            activeWidget_ = cancelId;
        else if (options.showDiscard && contains(discardButton,pointer_.pressedPosition[left]))
            activeWidget_ = discardId;
    }

    MessageBoxResult result = MessageBoxResult::None;
    if (pointer_.released[left] && activeWidget_ == discardId) {
        if (contains(discardButton,pointer_.releasedPosition[left])) {
            open=false; activeModal_=InvalidWidgetId; result=MessageBoxResult::Discarded;
        }
        activeWidget_=InvalidWidgetId;
    }
    if (escapePressed_ && options.showCancel) {
        open=false; activeModal_=InvalidWidgetId; activeWidget_=InvalidWidgetId; result=MessageBoxResult::Cancelled;
    }
    if (enterPressed_ && shownLastFrame && open)
    {
        open = false;
        activeModal_ = InvalidWidgetId;
        result = MessageBoxResult::Accepted;
    }
    if (pointer_.released[left] && activeWidget_ == acceptId)
    {
        if (contains(acceptButton, pointer_.releasedPosition[left]))
        {
            open = false;
            activeModal_ = InvalidWidgetId;
            result = MessageBoxResult::Accepted;
        }
        activeWidget_ = InvalidWidgetId;
    }
    else if (pointer_.released[left] && activeWidget_ == cancelId)
    {
        if (contains(cancelButton, pointer_.releasedPosition[left]))
        {
            open = false;
            activeModal_ = InvalidWidgetId;
            result = MessageBoxResult::Cancelled;
        }
        activeWidget_ = InvalidWidgetId;
    }

    if (inputMode)
    {
        String &input = *options.inputValue;
        const WidgetId inputId = combineIds(id, 0x494e505554ull);
        if (textInputWidget_ != inputId)
            textCursor_ = input.size();
        textInputWidget_ = inputId;
        wantsKeyboard_ = true;
        wantsTextInput_ = true;
        editTextBuffer(input, textCursor_, false);
    }

    Color kindColor = theme_.dialogBtnPrimary;
    StringView kindText("i");
    if (options.kind == MessageBoxKind::Warning)
    {
        kindColor = theme_.msgBoxWarning;
        kindText = StringView("!");
    }
    else if (options.kind == MessageBoxKind::Error)
    {
        kindColor = theme_.dialogBtnDanger;
        kindText = StringView("x");
    }
    else if (options.kind == MessageBoxKind::Input)
    {
        kindColor = theme_.msgBoxQuestion;
        kindText = StringView(">");
    }

    modalDrawList_.addRectFilled(viewport, theme_.dialogScrim, viewport);
    modalDrawList_.addRectFilled(dialog, theme_.dialogBg, viewport);
    modalDrawList_.addRect(dialog, theme_.dialogBorder, viewport);
    modalDrawList_.addRectFilled(titleBar, theme_.floatTitleBg, viewport);
    modalDrawList_.addRectFilled(Rect(dialog.x, dialog.y, 4.0f, dialog.height), kindColor, viewport);
    modalDrawList_.addCircleFilled(Vec2(dialog.x + theme_.windowPadding * 1.5f,
                                         titleBar.y + titleBar.height * 0.5f),
                                    theme_.fontSize * 0.42f, kindColor, viewport);
    const TextMetrics kindMetrics = measureText(theme_.font, kindText, theme_.fontSize * 0.75f);
    drawText(modalDrawList_, theme_.font, kindText,
             Vec2(dialog.x + theme_.windowPadding * 1.5f - kindMetrics.width * 0.5f,
                  titleBar.y + (titleBar.height - kindMetrics.height) * 0.5f),
             theme_.fontSize * 0.75f, theme_.buttonText, viewport);
    drawText(modalDrawList_, theme_.font, title,
             Vec2(titleBar.x + theme_.windowPadding * 3.0f, titleBar.y + theme_.windowPadding * 0.5f),
             theme_.fontSize, theme_.dialogTitleText, viewport);
    drawText(modalDrawList_, theme_.font, message,
             Vec2(dialog.x + theme_.windowPadding, titleBar.y + titleBar.height + theme_.itemSpacing),
             theme_.fontSize, theme_.dialogText, viewport);
    if (inputMode)
    {
        String &input = *options.inputValue;
        modalDrawList_.addRectFilled(inputRect, theme_.inputBg, viewport);
        modalDrawList_.addRect(inputRect, theme_.inputBorderHover, viewport);
        const float leftPadding = theme_.textEditPadding;
        const StringView prefix(input.data(), textCursor_);
        const TextMetrics allMetrics = measureText(theme_.font, input, theme_.fontSize);
        const TextMetrics prefixMetrics = measureText(theme_.font, prefix, theme_.fontSize);
        const float available = inputRect.width - leftPadding * 2.0f;
        float horizontalOffset = allMetrics.width > available ? available - allMetrics.width : 0.0f;
        if (prefixMetrics.width + horizontalOffset < 0.0f)
            horizontalOffset = -prefixMetrics.width;
        else if (prefixMetrics.width + horizontalOffset > available)
            horizontalOffset = available - prefixMetrics.width;
        const Rect inputClip(inputRect.x + leftPadding, inputRect.y + leftPadding,
                             available, inputRect.height - leftPadding * 2.0f);
        const Vec2 inputTextPosition(inputRect.x + leftPadding + horizontalOffset,
                                     inputRect.y + (inputRect.height - allMetrics.height) * 0.5f);
        drawText(modalDrawList_, theme_.font, input, inputTextPosition, theme_.fontSize,
                 theme_.buttonText, inputClip);
        modalDrawList_.addRectFilled(Rect(inputTextPosition.x + prefixMetrics.width,
                                          inputRect.y + 5.0f, 1.0f, inputRect.height - 10.0f),
                                     theme_.buttonText, inputClip);
    }
    modalDrawList_.addRectFilled(acceptButton,
                                 hoveredAccept ? theme_.dialogBtnHover : theme_.dialogBtnBg,
                                 viewport);
    modalDrawList_.addRect(acceptButton, theme_.dialogBorder, viewport);
    if (options.showDiscard) {
        const bool hoveredDiscard = contains(discardButton, pointer_.position);
        modalDrawList_.addRectFilled(discardButton,
                                    hoveredDiscard ? theme_.dialogBtnHover : theme_.dialogBtnBg,viewport);
        modalDrawList_.addRect(discardButton,theme_.dialogBorder,viewport);
        const TextMetrics discardMetrics = measureText(theme_.font,"Discard",theme_.fontSize);
        drawText(modalDrawList_,theme_.font,"Discard",
                 Vec2(discardButton.x+(discardButton.width-discardMetrics.width)*0.5f,
                      discardButton.y+(discardButton.height-discardMetrics.height)*0.5f),
                 theme_.fontSize,theme_.buttonText,discardButton);
    }
    const StringView acceptText(options.acceptLabel);
    const TextMetrics acceptMetrics = measureText(theme_.font, acceptText, theme_.fontSize);
    drawText(modalDrawList_, theme_.font, acceptText,
             Vec2(acceptButton.x + (acceptButton.width - acceptMetrics.width) * 0.5f,
                  acceptButton.y + (acceptButton.height - acceptMetrics.height) * 0.5f),
             theme_.fontSize, theme_.buttonText, viewport);
    if (options.showCancel)
    {
        modalDrawList_.addRectFilled(cancelButton,
                                     hoveredCancel ? theme_.dialogBtnHover : theme_.dialogBtnBg, viewport);
        modalDrawList_.addRect(cancelButton,theme_.dialogBorder,viewport);
        const StringView cancelText(options.cancelLabel);
        const TextMetrics cancelMetrics = measureText(theme_.font, cancelText, theme_.fontSize);
        drawText(modalDrawList_, theme_.font, cancelText,
                 Vec2(cancelButton.x + (cancelButton.width - cancelMetrics.width) * 0.5f,
                      cancelButton.y + (cancelButton.height - cancelMetrics.height) * 0.5f),
                 theme_.fontSize, theme_.buttonText, viewport);
    }
    return result;
}

void Context::showToast(StringView idText, StringView text, ToastPosition position, float duration)
{
    if (duration <= 0.0f || text.empty())
        return;
    const WidgetId id = hashText(idText);
    ToastState toast;
    toast.id = id;
    toast.text = String(text.data(), text.size());
    toast.position = position;
    toast.remaining = duration;
    toast.duration = duration;
    toasts_.push_back(toast);
}

bool Context::beginDragSource(WidgetId source, WidgetId type, uint64_t data, StringView preview,
                              const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || source == InvalidWidgetId || type == InvalidWidgetId)
        return false;
    const Rect rect = contentRect(bounds);
    const Rect visible = intersect(rect, contentClip());
    const uint32_t left = buttonIndex(PointerButton::Left);
    const WidgetId widget = combineIds(makeWidgetId(StringView("drag source")), source);
    const bool pressedHere = pointerPressedIn(visible, left) && activeWidget_ == InvalidWidgetId;
    if (pressedHere)
    {
        activeWidget_ = widget;
        dragDrop_.widget = widget;
        dragDrop_.payload.type = type;
        dragDrop_.payload.source = source;
        dragDrop_.payload.data = data;
        dragDrop_.preview = String(preview.data(), preview.size());
        dragDrop_.pressedPosition = pointer_.pressedPosition[left];
        dragDrop_.armed = true;
        dragDrop_.active = false;
        dragDrop_.accepted = false;
    }

    if (dragDrop_.armed && dragDrop_.widget == widget && activeWidget_ == widget)
    {
        const float deltaX = pointer_.position.x - dragDrop_.pressedPosition.x;
        const float deltaY = pointer_.position.y - dragDrop_.pressedPosition.y;
        if (pointer_.down[left] && deltaX * deltaX + deltaY * deltaY >= 16.0f)
            dragDrop_.active = true;
        return dragDrop_.active;
    }
    return false;
}

bool Context::acceptDragDropTarget(WidgetId acceptedType, const Rect &bounds,
                                   DragDropPayload &payload)
{
    if (!dragDrop_.active || dragDrop_.payload.type != acceptedType)
        return false;
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList)
        return false;
    const Rect rect = contentRect(bounds);
    const Rect visible = intersect(rect, contentClip());
    const bool hovered = contains(visible, pointer_.position);
    if (hovered)
    {
        drawList->addRect(rect, theme_.dialogBtnPrimary, contentClip(), 2.0f);
        const uint32_t left = buttonIndex(PointerButton::Left);
        if (pointer_.released[left])
        {
            payload = dragDrop_.payload;
            dragDrop_.accepted = true;
            return true;
        }
    }
    return false;
}

void Context::drawText(StringView text, const Vec2 &position, const Color &color)
{
    WindowState *window = currentWindow();
    if (!window)
        return;
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return;
    const Rect resolved = contentRect(Rect(position.x, position.y, 0.0f, 0.0f));
    drawText(*drawList, theme_.font, text, Vec2(resolved.x, resolved.y),
             theme_.fontSize, color, contentClip());
}

float Context::textWidth(StringView text) const
{
    return measureText(theme_.font, text, theme_.fontSize).width;
}

void Context::label(StringView text, const Vec2 &position)
{
    WindowState *window = currentWindow();
    if (!window)
        return;
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return;
    const Rect resolved = contentRect(Rect(position.x, position.y, 0.0f, 0.0f));
    drawText(*drawList, theme_.font, text, Vec2(resolved.x, resolved.y),
             theme_.fontSize, theme_.labelText, contentClip());
}

void Context::tooltip(StringView text)
{
    WindowState *window = currentWindow();
    if (!window || hotWidget_ != lastItemId_ || text.empty())
        return;

    const float pointerDeltaX = pointer_.position.x - tooltipPointerPosition_.x;
    const float pointerDeltaY = pointer_.position.y - tooltipPointerPosition_.y;
    const bool moved = pointerDeltaX * pointerDeltaX + pointerDeltaY * pointerDeltaY > 1.0f;
    if (tooltipWidget_ != lastItemId_ || moved)
    {
        tooltipWidget_ = lastItemId_;
        tooltipHoverSeconds_ = 0.0f;
        tooltipPointerPosition_ = pointer_.position;
        return;
    }
    tooltipHoverSeconds_ += frame_.deltaSeconds;
    if (tooltipHoverSeconds_ < tooltipDelay_)
        return;

    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const TextMetrics metrics = measureText(theme_.font, text, theme_.fontSize);
    const float paddingX = theme_.tooltipPadX;
    const float paddingY = theme_.tooltipPadY;
    float x = pointer_.position.x + 14.0f;
    float y = pointer_.position.y + 18.0f;
    const float width = metrics.width + paddingX * 2.0f;
    const float height = metrics.height + paddingY * 2.0f;
    if (x + width > viewport.width)
        x = viewport.width - width;
    if (y + height > viewport.height)
        y = pointer_.position.y - height - 10.0f;
    if (x < 0.0f)
        x = 0.0f;
    if (y < 0.0f)
        y = 0.0f;
    const Rect bounds(x, y, width, height);
    window->overlayDrawList.addRectFilled(bounds, theme_.tooltipBg, viewport);
    drawText(window->overlayDrawList, theme_.font, text,
             Vec2(x + paddingX, y + paddingY), theme_.fontSize,
             theme_.tooltipText, viewport);
}

void Context::setTooltipDelay(float seconds)
{
    tooltipDelay_ = seconds > 0.0f ? seconds : 0.0f;
}

float Context::tooltipDelay() const
{
    return tooltipDelay_;
}

bool Context::button(StringView labelText)
{
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, autoButtonWidth(labelText), theme_.widgetHeight);
    const bool clicked = button(labelText, Rect(bounds.x - layout_.origin.x,
                                                bounds.y - layout_.origin.y,
                                                bounds.width, bounds.height));
    advanceLayout(bounds);
    return clicked;
}

bool Context::smallButton(StringView labelText)
{
    const float fontSize = theme_.fontSize * 0.82f;
    const TextMetrics metrics = measureText(theme_.font, labelText, fontSize);
    const float paddingX = theme_.padding * 1.25f;
    const float paddingY = theme_.padding * 0.55f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, metrics.width + paddingX * 2.0f,
                      metrics.height + paddingY * 2.0f);
    const bool clicked = smallButton(labelText, Rect(bounds.x - layout_.origin.x,
                                                     bounds.y - layout_.origin.y,
                                                     bounds.width, bounds.height));
    advanceLayout(bounds);
    return clicked;
}

bool Context::checkbox(StringView labelText, bool &value)
{
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, theme_.widgetHeight, theme_.widgetHeight);
    const bool clicked = checkbox(labelText, value, Rect(bounds.x - layout_.origin.x,
                                                         bounds.y - layout_.origin.y,
                                                         bounds.width, bounds.height));
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    advanceLayout(Rect(bounds.x, bounds.y,
                       bounds.width + theme_.windowPadding * 0.5f + metrics.width,
                       bounds.height));
    return clicked;
}

bool Context::toggleSwitch(StringView labelText, bool &value)
{
    const float switchWidth = theme_.widgetHeight * 1.8f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, switchWidth, theme_.widgetHeight);
    const bool clicked = toggleSwitch(labelText, value,
                                      Rect(bounds.x - layout_.origin.x,
                                           bounds.y - layout_.origin.y,
                                           bounds.width, bounds.height));
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    advanceLayout(Rect(bounds.x, bounds.y,
                       bounds.width + theme_.windowPadding * 0.5f + metrics.width,
                       bounds.height));
    return clicked;
}

bool Context::radioButton(StringView labelText, bool selected)
{
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, theme_.widgetHeight, theme_.widgetHeight);
    const bool clicked = radioButton(labelText, selected,
                                     Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                          bounds.width, bounds.height));
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    advanceLayout(Rect(bounds.x, bounds.y,
                       bounds.width + theme_.windowPadding * 0.5f + metrics.width,
                       bounds.height));
    return clicked;
}

bool Context::selectable(StringView labelText, bool selected, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool clicked = selectable(labelText, selected,
                                    Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                         bounds.width, bounds.height));
    advanceLayout(bounds);
    return clicked;
}

bool Context::collapsingHeader(StringView labelText, bool &expanded, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool open = collapsingHeader(labelText, expanded,
                                       Rect(bounds.x - layout_.origin.x,
                                            bounds.y - layout_.origin.y,
                                            bounds.width, bounds.height));
    advanceLayout(bounds);
    return open;
}

bool Context::treeNode(StringView labelText, bool &expanded, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool open = treeNode(labelText, expanded,
                               Rect(bounds.x - layout_.origin.x,
                                    bounds.y - layout_.origin.y,
                                    bounds.width, bounds.height));
    advanceLayout(bounds);
    return open;
}

bool Context::treeItem(StringView labelText, bool &expanded, const TreeItemStyle &style,
                       float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool selected = treeItem(labelText, expanded, style,
                                   Rect(bounds.x - layout_.origin.x,
                                        bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height));
    advanceLayout(bounds);
    return selected;
}

bool Context::treeItem(WidgetId nodeId, StringView labelText, bool &expanded,
                       const TreeItemStyle &style, TreeDrop *drop, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool selected = treeItem(nodeId, labelText, expanded, style,
                                   Rect(bounds.x - layout_.origin.x,
                                        bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height), drop);
    advanceLayout(bounds);
    return selected;
}

bool Context::tabBar(StringView labelText, int &currentItem, Span<const StringView> items,
                     float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = tabBar(labelText, currentItem, items,
                                Rect(bounds.x - layout_.origin.x,
                                     bounds.y - layout_.origin.y,
                                     bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::listBox(StringView labelText, int &currentItem, Span<const StringView> items,
                      float width, int visibleItems)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const int resolvedRows = visibleItems > 0 ? visibleItems : 1;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth,
                      theme_.widgetHeight * static_cast<float>(resolvedRows));
    const bool changed = listBox(labelText, currentItem, items,
                                 Rect(bounds.x - layout_.origin.x,
                                      bounds.y - layout_.origin.y,
                                      bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::comboBox(StringView labelText, int &currentItem, Span<const StringView> items,
                       float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = comboBox(labelText, currentItem, items,
                                  Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                       bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::sliderFloat(StringView labelText, float &value, float minimum, float maximum,
                          float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = sliderFloat(labelText, value, minimum, maximum,
                                     Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                          bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::sliderInt(StringView labelText, int &value, int minimum, int maximum, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = sliderInt(labelText, value, minimum, maximum,
                                   Rect(bounds.x - layout_.origin.x,
                                        bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::dragFloat(StringView labelText, float &value, float minimum, float maximum,
                        float speed, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = dragFloat(labelText, value, minimum, maximum, speed,
                                   Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::dragInt(StringView labelText, int &value, int minimum, int maximum,
                      int speed, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = dragInt(labelText, value, minimum, maximum, speed,
                                 Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                      bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::inputFloat2(StringView labelText, float &x, float &y, float width, int precision)
{
    const float total = width > 0.0f ? width : availableWidth();
    const float cell = (total - theme_.itemSpacing) * 0.5f;
    if (cell <= 0.0f)
        return false;
    pushId(labelText);
    bool changed = inputFloat("x", x, cell, precision);
    sameLine();
    changed = inputFloat("y", y, cell, precision) || changed;
    popId();
    return changed;
}

bool Context::inputFloat3(StringView labelText, float &x, float &y, float &z, float width, int precision)
{
    const float total = width > 0.0f ? width : availableWidth();
    const float cell = (total - theme_.itemSpacing * 2.0f) / 3.0f;
    if (cell <= 0.0f)
        return false;
    pushId(labelText);
    bool changed = inputFloat("x", x, cell, precision);
    sameLine();
    changed = inputFloat("y", y, cell, precision) || changed;
    sameLine();
    changed = inputFloat("z", z, cell, precision) || changed;
    popId();
    return changed;
}

bool Context::inputFloat4(StringView labelText, float &x, float &y, float &z, float &w,
                          float width, int precision)
{
    const float total = width > 0.0f ? width : availableWidth();
    const float cell = (total - theme_.itemSpacing * 3.0f) * 0.25f;
    if (cell <= 0.0f)
        return false;
    pushId(labelText);
    bool changed = inputFloat("x", x, cell, precision);
    sameLine();
    changed = inputFloat("y", y, cell, precision) || changed;
    sameLine();
    changed = inputFloat("z", z, cell, precision) || changed;
    sameLine();
    changed = inputFloat("w", w, cell, precision) || changed;
    popId();
    return changed;
}

bool Context::dragFloat2(StringView labelText, float &x, float &y, float minimum, float maximum,
                         float speed, float width)
{
    const float total = width > 0.0f ? width : availableWidth();
    const float cell = (total - theme_.itemSpacing) * 0.5f;
    if (cell <= 0.0f)
        return false;
    pushId(labelText);
    bool changed = dragFloat("x", x, minimum, maximum, speed, cell);
    sameLine();
    changed = dragFloat("y", y, minimum, maximum, speed, cell) || changed;
    popId();
    return changed;
}

bool Context::dragFloat3(StringView labelText, float &x, float &y, float &z, float minimum, float maximum,
                         float speed, float width)
{
    const float total = width > 0.0f ? width : availableWidth();
    const float cell = (total - theme_.itemSpacing * 2.0f) / 3.0f;
    if (cell <= 0.0f)
        return false;
    pushId(labelText);
    bool changed = dragFloat("x", x, minimum, maximum, speed, cell);
    sameLine();
    changed = dragFloat("y", y, minimum, maximum, speed, cell) || changed;
    sameLine();
    changed = dragFloat("z", z, minimum, maximum, speed, cell) || changed;
    popId();
    return changed;
}

bool Context::dragFloat4(StringView labelText, float &x, float &y, float &z, float &w,
                         float minimum, float maximum, float speed, float width)
{
    const float total = width > 0.0f ? width : availableWidth();
    const float cell = (total - theme_.itemSpacing * 3.0f) * 0.25f;
    if (cell <= 0.0f)
        return false;
    pushId(labelText);
    bool changed = dragFloat("x", x, minimum, maximum, speed, cell);
    sameLine();
    changed = dragFloat("y", y, minimum, maximum, speed, cell) || changed;
    sameLine();
    changed = dragFloat("z", z, minimum, maximum, speed, cell) || changed;
    sameLine();
    changed = dragFloat("w", w, minimum, maximum, speed, cell) || changed;
    popId();
    return changed;
}

bool Context::stepperInt(StringView labelText, int &value, int minimum, int maximum, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = stepperInt(labelText, value, minimum, maximum,
                                    Rect(bounds.x - layout_.origin.x,
                                         bounds.y - layout_.origin.y,
                                         bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::inputText(StringView labelText, String &value, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = inputText(labelText, value,
                                   Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::inputTextMultiline(StringView labelText, String &value, float width, float height,
                                 bool followTail)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const float resolvedHeight = height > theme_.widgetHeight ? height : theme_.widgetHeight;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, resolvedHeight);
    const bool changed = inputTextMultiline(labelText, value,
                                            Rect(bounds.x - layout_.origin.x,
                                                 bounds.y - layout_.origin.y,
                                                 bounds.width, bounds.height),
                                            followTail);
    advanceLayout(bounds);
    return changed;
}

bool Context::inputInt(StringView labelText, int &value, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = inputInt(labelText, value,
                                  Rect(bounds.x - layout_.origin.x,
                                       bounds.y - layout_.origin.y,
                                       bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::inputFloat(StringView labelText, float &value, float width, int precision)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = inputFloat(labelText, value,
                                    Rect(bounds.x - layout_.origin.x,
                                         bounds.y - layout_.origin.y,
                                         bounds.width, bounds.height), precision);
    advanceLayout(bounds);
    return changed;
}

bool Context::colorEdit(StringView labelText, Color &value, float width, float height)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const float minimumHeight = theme_.widgetHeight * 3.0f;
    const float resolvedHeight = height > minimumHeight ? height : minimumHeight;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, resolvedHeight);
    const bool changed = colorEdit(labelText, value,
                                   Rect(bounds.x - layout_.origin.x,
                                        bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

void Context::image(TextureId texture, float width, float height,
                    const Vec2 &uvMin, const Vec2 &uvMax, const Color &tint)
{
    if (width <= 0.0f || height <= 0.0f)
        return;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, width, height);
    image(texture, Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                        bounds.width, bounds.height), uvMin, uvMax, tint);
    advanceLayout(bounds);
}

bool Context::imageButton(StringView labelText, TextureId texture, float width, float height,
                          const Vec2 &uvMin, const Vec2 &uvMax, const Color &tint)
{
    if (width <= 0.0f || height <= 0.0f)
        return false;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, width, height);
    const bool clicked = imageButton(labelText, texture,
                                     Rect(bounds.x - layout_.origin.x,
                                          bounds.y - layout_.origin.y,
                                          bounds.width, bounds.height),
                                     uvMin, uvMax, tint);
    advanceLayout(bounds);
    return clicked;
}

bool Context::smallImageButton(StringView labelText, TextureId texture, float size,
                               const Vec2 &uvMin, const Vec2 &uvMax, const Color &tint)
{
    const float resolvedSize = size > 0.0f ? size : theme_.fontSize + theme_.padding * 1.4f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedSize, resolvedSize);
    const bool clicked = smallImageButton(labelText, texture,
                                          Rect(bounds.x - layout_.origin.x,
                                               bounds.y - layout_.origin.y,
                                               bounds.width, bounds.height),
                                          uvMin, uvMax, tint);
    advanceLayout(bounds);
    return clicked;
}

bool Context::beginChild(StringView idText, float height, bool border, float width)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList || height <= 0.0f || tableActive_ || propertyRowActive_)
        return false;
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    if (resolvedWidth <= 0.0f)
        return false;
    const Rect outer(layout_.cursor.x, layout_.cursor.y, resolvedWidth, height);
    const Rect parentClip = contentClip();
    if (intersect(outer, parentClip).width <= 0.0f || intersect(outer, parentClip).height <= 0.0f)
        return false;

    const WidgetId id = combineIds(makeWidgetId(idText), 0x4348494c44ull);
    ChildScrollState *scroll = childScrolls_.find(id);
    if (!scroll)
    {
        childScrolls_.put(id, ChildScrollState());
        scroll = childScrolls_.find(id);
    }
    if (!scroll)
        return false;

    const float padding = theme_.padding;
    const float usableHeight = height - padding * 2.0f;
    const bool hadScrollbar = scroll->contentHeight > usableHeight;
    const float scrollbarWidth = hadScrollbar ? theme_.scrollbarWidth * 0.65f : 0.0f;
    const Rect content(outer.x + padding, outer.y + padding,
                       resolvedWidth - padding * 2.0f - scrollbarWidth,
                       usableHeight > 0.0f ? usableHeight : 0.0f);
    if (content.width <= 0.0f || content.height <= 0.0f)
        return false;
    const float maximumScroll = scroll->contentHeight > content.height
        ? scroll->contentHeight - content.height : 0.0f;
    if (scroll->offset > maximumScroll)
        scroll->offset = maximumScroll;
    if (scroll->offset < 0.0f)
        scroll->offset = 0.0f;
    if (hadScrollbar)
    {
        const float barWidth = theme_.scrollbarWidth * 0.65f;
        const Rect bar(outer.x + outer.width - padding - barWidth, content.y, barWidth, content.height);
        const float requestedThumb = bar.height * content.height / scroll->contentHeight;
        const float thumbHeight = requestedThumb > theme_.scrollbarMinThumb
            ? requestedThumb : theme_.scrollbarMinThumb;
        const float travel = bar.height - thumbHeight;
        const float thumbY = travel > 0.0f ? bar.y + travel * scroll->offset / maximumScroll : bar.y;
        const Rect thumb(bar.x, thumbY, bar.width, thumbHeight);
        const WidgetId scrollbarId = combineIds(id, 0x5343524f4c4cull);
        const uint32_t left = buttonIndex(PointerButton::Left);
        if (pointerPressedIn(intersect(bar, parentClip), left) && activeWidget_ == InvalidWidgetId)
            activeWidget_ = scrollbarId;
        if (activeWidget_ == scrollbarId)
        {
            if (pointer_.down[left] && travel > 0.0f)
            {
                const float normalized = clamp((pointer_.position.y - bar.y - thumb.height * 0.5f) / travel,
                                               0.0f, 1.0f);
                scroll->offset = normalized * maximumScroll;
            }
            if (pointer_.released[left])
                activeWidget_ = InvalidWidgetId;
        }
    }
    if (pointer_.wheelY != 0.0f && pointerOver(intersect(outer, parentClip)))
    {
        scroll->offset -= pointer_.wheelY * theme_.widgetHeight;
        if (scroll->offset < 0.0f)
            scroll->offset = 0.0f;
        if (scroll->offset > maximumScroll)
            scroll->offset = maximumScroll;
        pointer_.wheelY = 0.0f;
    }

    drawList->addRectFilled(outer, theme_.panelColor, parentClip);
    if (border)
        drawList->addRect(outer, theme_.borderColor, parentClip);
    ChildState state;
    state.parentLayout = layout_;
    state.outer = outer;
    state.content = content;
    state.parentClip = parentClip;
    state.id = id;
    state.border = border;
    childStack_.push_back(state);
    const WidgetId parentId = idStack_.empty() ? InvalidWidgetId : idStack_.back();
    idStack_.push_back(combineIds(parentId, id));
    layout_.origin = Vec2(content.x, content.y);
    layout_.cursor = Vec2(content.x, content.y - scroll->offset);
    layout_.baseOriginX = content.x;
    layout_.lastItem = Rect();
    layout_.hasLastItem = false;
    return true;
}

void Context::endChild()
{
    if (childStack_.empty())
        return;
    const ChildState state = childStack_.back();
    ChildScrollState *scroll = childScrolls_.find(state.id);
    const float offset = scroll ? scroll->offset : 0.0f;
    const float contentBottom = layout_.hasLastItem
        ? layout_.lastItem.y + layout_.lastItem.height + offset : state.content.y;
    const float contentHeight = contentBottom > state.content.y ? contentBottom - state.content.y : 0.0f;
    if (scroll)
    {
        scroll->contentHeight = contentHeight;
        const float maximumScroll = contentHeight > state.content.height
            ? contentHeight - state.content.height : 0.0f;
        if (scroll->offset > maximumScroll)
            scroll->offset = maximumScroll;
        if (scroll->offset < 0.0f)
            scroll->offset = 0.0f;
        if (maximumScroll > 0.0f)
        {
            DrawList *drawList = currentDrawList();
            if (drawList)
            {
                const float barWidth = theme_.scrollbarWidth * 0.65f;
                const Rect bar(state.outer.x + state.outer.width - theme_.padding - barWidth,
                               state.content.y, barWidth, state.content.height);
                const float requestedThumb = bar.height * bar.height / contentHeight;
                const float thumbHeight = requestedThumb > theme_.scrollbarMinThumb
                    ? requestedThumb : theme_.scrollbarMinThumb;
                const float travel = bar.height - thumbHeight;
                const float thumbY = travel > 0.0f ? bar.y + travel * scroll->offset / maximumScroll : bar.y;
                drawList->addRectFilled(bar, theme_.inputBg, state.parentClip);
                drawList->addRectFilled(Rect(bar.x, thumbY, bar.width, thumbHeight),
                                         theme_.scrollbarThumb, state.parentClip);
            }
        }
    }
    childStack_.pop_back();
    if (!idStack_.empty())
        idStack_.pop_back();
    layout_ = state.parentLayout;
    advanceLayout(state.outer);
}

bool Context::beginDockSpace(StringView idText, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;
    return beginDockSpaceInternal(idText, contentRect(bounds), contentClip());
}

bool Context::beginDockSpace(StringView idText)
{
    return beginDockSpace(idText, 0.0f);
}

bool Context::beginDockSpace(StringView idText, float topInset)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;
    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const float titleHeight = window->showTitleBar ? theme_.titleBarHeight : 0.0f;
    const float inset = topInset > 0.0f ? topInset : 0.0f;
    const float clientHeight = window->bounds.height - titleHeight;
    const Rect client(window->bounds.x, window->bounds.y + titleHeight + inset,
                      window->bounds.width, clientHeight > inset ? clientHeight - inset : 0.0f);
    return beginDockSpaceInternal(idText, client, intersect(client, viewport));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Dock arrangement in the state file
//
//  One list of titles per slot, plus which of them is the tab on top:
//
//    [dock:editor]
//    left_width=180
//    tabs_left=Hierarchy|Assets
//    selected_left=Assets
//
//  A title the file remembers for a slot but which the application did not draw
//  this run (a panel the user closed) keeps its place; a title drawn in another
//  slot has been dragged, and moves.
// ─────────────────────────────────────────────────────────────────────────────

struct DockSlotName
{
    DockSlot    slot;
    const char *key;   // name the slot is stored under
};

// One entry per DockSlot: the state file never holds a slot number.
const DockSlotName kDockSlotNames[] = {
    {DockSlot::Left,   "left"},
    {DockSlot::Center, "center"},
    {DockSlot::Right,  "right"},
    {DockSlot::Bottom, "bottom"},
};

String dockKey(const char *prefix, const char *slotName)
{
    String key = prefix;
    key.append(slotName);
    return key;
}

// A title that would break the "a|b" list or the ini line itself is not stored.
bool dockTitleStorable(StringView title)
{
    if (title.empty())
        return false;
    for (StringView::size_type i = 0; i < title.size(); ++i)
        if (title[i] == '|' || title[i] == '\n' || title[i] == '\r')
            return false;
    return true;
}

void dockAppendTitle(String &list, StringView title)
{
    if (!list.empty())
        list.push_back('|');
    list.append(title.data(), title.size());
}

// Next title of an "a|b" list, advancing @p start past it; an empty view once
// the list is exhausted.
StringView dockListNext(const String &list, String::size_type &start)
{
    while (start < list.size())
    {
        String::size_type end = start;
        while (end < list.size() && list[end] != '|')
            ++end;
        const StringView entry(list.data() + start, end - start);
        start = end + 1;
        if (!entry.empty())
            return entry;
    }
    return StringView();
}

bool dockListHas(const String &list, StringView title)
{
    if (list.empty() || title.empty())
        return false;
    String::size_type start = 0;
    for (StringView entry = dockListNext(list, start); !entry.empty();
         entry = dockListNext(list, start))
        if (entry == title)
            return true;
    return false;
}

bool dockDrawnHas(const ct::Vector<String> &drawn, StringView title)
{
    for (const String &candidate : drawn)
        if (StringView(candidate.data(), candidate.size()) == title)
            return true;
    return false;
}

bool Context::beginDockSpaceInternal(StringView idText, const Rect &outer, const Rect &clip)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList || activeDockSpace_ != InvalidWidgetId || !dockPanelStack_.empty() ||
        outer.width <= 0.0f || outer.height <= 0.0f || intersect(outer, clip).width <= 0.0f ||
        intersect(outer, clip).height <= 0.0f)
        return false;

    const WidgetId id = combineIds(makeWidgetId(idText), 0x444f434b53504143ull);
    DockSpaceState *dockSpace = dockSpaces_.find(id);
    if (!dockSpace)
    {
        DockSpaceState state;
        state.name = String(idText.data(), idText.size());
        // Split sizes the user dragged last run; the clamping below still makes
        // sure they fit the window they are opened in now.
        const String section = dockStateSection(state.name);
        state.leftWidth =
            static_cast<float>(windowState_.getNumber(section, "left_width", state.leftWidth));
        state.rightWidth =
            static_cast<float>(windowState_.getNumber(section, "right_width", state.rightWidth));
        state.bottomHeight =
            static_cast<float>(windowState_.getNumber(section, "bottom_height", state.bottomHeight));
        dockSpaces_.put(id, state);
        dockSpace = dockSpaces_.find(id);
    }
    if (!dockSpace)
        return false;

    dockSpace->bounds = outer;
    dockSpace->clip = clip;
    const float minimumCenter = outer.width < 352.0f ? outer.width * (160.0f / 352.0f) : 160.0f;
    const float minimumSide = outer.width < 352.0f ? outer.width * (96.0f / 352.0f) : 96.0f;
    const float maximumSide = outer.width - minimumCenter - minimumSide;
    if (dockSpace->leftWidth < minimumSide)
        dockSpace->leftWidth = minimumSide;
    if (dockSpace->rightWidth < minimumSide)
        dockSpace->rightWidth = minimumSide;
    if (maximumSide > minimumSide)
    {
        if (dockSpace->leftWidth > maximumSide)
            dockSpace->leftWidth = maximumSide;
        if (dockSpace->rightWidth > maximumSide)
            dockSpace->rightWidth = maximumSide;
    }
    else
    {
        dockSpace->leftWidth = minimumSide;
        dockSpace->rightWidth = minimumSide;
    }
    const float sideBudget = outer.width - minimumCenter;
    if (dockSpace->leftWidth + dockSpace->rightWidth > sideBudget)
    {
        dockSpace->rightWidth = sideBudget - dockSpace->leftWidth;
        if (dockSpace->rightWidth < minimumSide)
        {
            dockSpace->rightWidth = minimumSide;
            dockSpace->leftWidth = sideBudget - minimumSide;
        }
    }
    const float minimumBottom = outer.height < theme_.widgetHeight * 5.0f
        ? outer.height * 0.4f : theme_.widgetHeight * 2.0f;
    const float maximumBottom = outer.height - (outer.height < theme_.widgetHeight * 5.0f
        ? outer.height * 0.6f : theme_.widgetHeight * 3.0f);
    const float bottomMaximum = maximumBottom > minimumBottom ? maximumBottom : minimumBottom;
    dockSpace->bottomHeight = clamp(dockSpace->bottomHeight, minimumBottom, bottomMaximum);

    drawList->addRectFilled(outer, theme_.panelColor, clip);
    drawList->addRect(outer, theme_.borderColor, clip);

    const float splitterThickness = 5.0f;
    const Rect leftRegion = dockSlotBounds(*dockSpace, DockSlot::Left);
    const Rect rightRegion = dockSlotBounds(*dockSpace, DockSlot::Right);
    const Rect bottomRegion = dockSlotBounds(*dockSpace, DockSlot::Bottom);
    const Rect leftSplitter(leftRegion.x + leftRegion.width - splitterThickness * 0.5f,
                            outer.y, splitterThickness, outer.height - dockSpace->bottomHeight);
    const Rect rightSplitter(rightRegion.x - splitterThickness * 0.5f,
                             outer.y, splitterThickness, outer.height - dockSpace->bottomHeight);
    const Rect bottomSplitter(outer.x, bottomRegion.y - splitterThickness * 0.5f,
                              outer.width, splitterThickness);
    const WidgetId leftId = combineIds(id, 0x4c454654ull);
    const WidgetId rightId = combineIds(id, 0x5249474854ull);
    const WidgetId bottomId = combineIds(id, 0x424f54544f4dull);
    const uint32_t leftButton = buttonIndex(PointerButton::Left);
    const WidgetId splitterIds[] = {leftId, rightId, bottomId};
    const Rect splitters[] = {leftSplitter, rightSplitter, bottomSplitter};
    for (uint32_t splitter = 0u; splitter < 3u; ++splitter)
    {
        const Rect visible = intersect(splitters[splitter], clip);
        const bool hovered = pointerOver(visible);
        if (pointerPressedIn(visible, leftButton) && activeWidget_ == InvalidWidgetId)
            activeWidget_ = splitterIds[splitter];
        if (activeWidget_ == splitterIds[splitter])
        {
            if (pointer_.down[leftButton] || pointer_.pressed[leftButton])
            {
                if (splitter == 0u)
                    dockSpace->leftWidth = clamp(pointer_.position.x - outer.x, minimumSide,
                                                outer.width - minimumCenter - dockSpace->rightWidth);
                else if (splitter == 1u)
                    dockSpace->rightWidth = clamp(outer.x + outer.width - pointer_.position.x,
                                                  minimumSide, outer.width - minimumCenter - dockSpace->leftWidth);
                else
                    dockSpace->bottomHeight = clamp(outer.y + outer.height - pointer_.position.y,
                                                    minimumBottom, bottomMaximum);
            }
            if (pointer_.released[leftButton])
                activeWidget_ = InvalidWidgetId;
        }
        drawList->addRectFilled(splitters[splitter], hovered || activeWidget_ == splitterIds[splitter]
                                                ? theme_.focusColor : theme_.borderColor, clip);
    }

    activeDockSpace_ = id;
    return true;
}

void Context::endDockSpace()
{
    if (activeDockSpace_ == InvalidWidgetId || !dockPanelStack_.empty())
        return;
    DockSpaceState *dockSpace = dockSpaces_.find(activeDockSpace_);
    if (dockSpace)
    {
        for (ct::Vector<DockTabState>::size_type i = 0u; i < dockSpace->tabs.size();)
        {
            // A panel the application stopped submitting loses its tab without
            // its open flag being read: that pointer belongs to the frame the
            // panel was last drawn in and may be gone by now.
            const bool dropped = dockSpace->tabs[i].lastSeenFrame + 1u < frameNumber_;
            const bool closed  = !dropped && dockSpace->tabs[i].open && !*dockSpace->tabs[i].open;
            if (dropped || closed)
                dockSpace->tabs.erase(dockSpace->tabs.begin() + i);
            else
                ++i;
        }
        for (uint32_t slot = 0u; slot < 4u; ++slot)
        {
            bool selectedExists = false;
            for (ct::Vector<DockTabState>::size_type i = 0u; i < dockSpace->tabs.size(); ++i)
            {
                if (dockSlotIndex(dockSpace->tabs[i].slot) == slot && dockSpace->tabs[i].id == dockSpace->selected[slot])
                {
                    selectedExists = true;
                    break;
                }
            }
            if (!selectedExists)
                dockSpace->selected[slot] = InvalidWidgetId;
        }
        advanceLayout(dockSpace->bounds);
    }
    activeDockSpace_ = InvalidWidgetId;
}

void Context::setDockSizes(float leftWidth, float rightWidth, float bottomHeight)
{
    DockSpaceState *dockSpace = activeDockSpace_ != InvalidWidgetId ? dockSpaces_.find(activeDockSpace_) : nullptr;
    if (!dockSpace)
        return;
    if (leftWidth > 0.0f)
        dockSpace->leftWidth = leftWidth;
    if (rightWidth > 0.0f)
        dockSpace->rightWidth = rightWidth;
    if (bottomHeight > 0.0f)
        dockSpace->bottomHeight = bottomHeight;
}

bool Context::beginDockPanel(StringView title, DockSlot slot, bool *open)
{
    if (activeDockSpace_ == InvalidWidgetId || !dockPanelStack_.empty() || (open && !*open))
        return false;
    DockSpaceState *dockSpace = dockSpaces_.find(activeDockSpace_);
    DrawList *drawList = currentDrawList();
    if (!dockSpace || !drawList)
        return false;

    const WidgetId id = combineIds(activeDockSpace_, hashText(title));
    DockTabState *tabState = nullptr;
    for (ct::Vector<DockTabState>::size_type i = 0u; i < dockSpace->tabs.size(); ++i)
    {
        if (dockSpace->tabs[i].id == id)
        {
            tabState = &dockSpace->tabs[i];
            break;
        }
    }
    if (!tabState)
    {
        DockTabState tab;
        tab.id = id;
        tab.title = String(title.data(), title.size());
        // Where the panel was left wins over the slot the application asks for,
        // so a panel dragged to another region comes back there; so does the tab
        // that was on top of its group.
        tab.slot = storedDockSlot(dockSpace->name, tab.title, slot);
        dockSpace->tabs.push_back(tab);
        tabState = &dockSpace->tabs.back();
        if (storedDockTabOnTop(dockSpace->name, tabState->slot, tabState->title))
            dockSpace->selected[dockSlotIndex(tabState->slot)] = id;
    }
    tabState->lastSeenFrame = frameNumber_;
    tabState->open = open;
    const DockSlot resolvedSlot = tabState->slot;
    const uint32_t slotIndex = dockSlotIndex(resolvedSlot);
    if (dockSpace->selected[slotIndex] == InvalidWidgetId)
        dockSpace->selected[slotIndex] = id;

    const Rect region = dockSlotBounds(*dockSpace, resolvedSlot);
    const Rect clip = contentClip();
    if (region.width <= 0.0f || region.height <= theme_.widgetHeight)
        return false;
    // Every tab of a slot comes through here; the region is drawn by the first one of the frame. A second background
    // would paint over the content of a tab that was built before it.
    if (dockSpace->regionFrame[slotIndex] != frameNumber_)
    {
        dockSpace->regionFrame[slotIndex] = frameNumber_;
        drawList->addRectFilled(region, theme_.inputBg, clip);
        drawList->addRect(region, theme_.borderColor, clip);
    }

    int tabCount = 0;
    for (ct::Vector<DockTabState>::size_type i = 0u; i < dockSpace->tabs.size(); ++i)
    {
        if (dockSpace->tabs[i].slot == resolvedSlot)
            ++tabCount;
    }
    const Rect tabBar(region.x, region.y, region.width, theme_.widgetHeight);
    const bool tabBarVisible = tabCount != 1 || !dockSpace->tabBarHidden[slotIndex];
    const float hiddenTabBarHeight = 10.0f;
    if (!tabBarVisible)
    {
        const Rect hiddenTabBar(region.x, region.y, region.width, hiddenTabBarHeight);
        const Rect showTabButton(region.x, region.y, theme_.widgetHeight, hiddenTabBarHeight);
        const WidgetId showTabId = combineIds(activeDockSpace_, 0x53484f57544142ull + slotIndex);
        const bool hovered = itemHovered(showTabButton, clip, showTabId);
        if (itemClicked(showTabButton, clip, showTabId, false))
            dockSpace->tabBarHidden[slotIndex] = false;
        drawList->addRectFilled(hiddenTabBar, theme_.buttonBackground, clip);
        drawList->addRectFilled(showTabButton, hovered ? theme_.buttonHovered : theme_.buttonBackground, clip);
        drawList->addLine(Vec2(hiddenTabBar.x, hiddenTabBar.y + hiddenTabBar.height - 1.0f),
                          Vec2(hiddenTabBar.x + hiddenTabBar.width, hiddenTabBar.y + hiddenTabBar.height - 1.0f),
                          theme_.borderColor, clip);
        const float arrowSize = showTabButton.height * 0.28f;
        const Vec2 arrow[] = {
            Vec2(showTabButton.x + showTabButton.width * 0.5f - arrowSize, showTabButton.y + showTabButton.height * 0.58f),
            Vec2(showTabButton.x + showTabButton.width * 0.5f + arrowSize, showTabButton.y + showTabButton.height * 0.58f),
            Vec2(showTabButton.x + showTabButton.width * 0.5f, showTabButton.y + showTabButton.height * 0.36f)
        };
        drawList->addPolygonFilled(Span<const Vec2>(arrow), theme_.buttonText, clip);
    }
    else
    {
    const float tabListWidth = tabBar.height;
    const Rect tabListButton(tabBar.x + tabBar.width - tabListWidth, tabBar.y,
                             tabListWidth, tabListWidth);
    const float tabRight = tabListButton.x;
    // One continuous header; controls only gain a background on interaction.
    drawList->addRectFilled(tabBar, theme_.panelColor, clip);
    drawList->addLine(Vec2(tabBar.x, tabBar.bottom() - 1.0f),
                      Vec2(tabBar.right(), tabBar.bottom() - 1.0f), theme_.borderColor, clip);
    const bool handleTabInput = dockSpace->tabBarFrame[slotIndex] != frameNumber_;
    if (handleTabInput)
        dockSpace->tabBarFrame[slotIndex] = frameNumber_;
    float tabX = region.x;
    for (ct::Vector<DockTabState>::size_type i = 0u; i < dockSpace->tabs.size(); ++i)
    {
        const DockTabState &candidate = dockSpace->tabs[i];
        if (candidate.slot != resolvedSlot)
            continue;
        const TextMetrics metrics = measureText(theme_.font, candidate.title, theme_.fontSize);
        const float closeSize = candidate.open ? tabBar.height * 0.55f : 0.0f;
        const float requestedWidth = metrics.width + theme_.windowPadding * 2.0f + closeSize;
        const float remaining = tabRight - tabX;
        const float tabWidth = requestedWidth < remaining ? requestedWidth : remaining;
        if (tabWidth <= 0.0f)
            break;
        const Rect tab(tabX, tabBar.y, tabWidth, tabBar.height);
        const Rect closeButton(tab.x + tab.width - closeSize - theme_.windowPadding * 0.35f,
                               tab.y + (tab.height - closeSize) * 0.5f, closeSize, closeSize);
        const bool hovered = itemHovered(tab, clip, candidate.id);
        const WidgetId closeId = combineIds(candidate.id, 0x444f434b434c4f53ull);
        // Keep processing the close button while it owns the press: the tab's
        // hover test deliberately rejects input captured by a different ID.
        const bool closeVisible = candidate.open && (hovered || activeWidget_ == closeId);
        const bool closeHovered = closeVisible && itemHovered(closeButton, clip, closeId);
        if (closeVisible && handleTabInput && itemClicked(closeButton, clip, closeId, false))
        {
            *candidate.open = false;
            if (dockSpace->selected[slotIndex] == candidate.id)
                dockSpace->selected[slotIndex] = InvalidWidgetId;
            tabX += tabWidth;
            continue;
        }
        const bool pressedHere = handleTabInput &&
            pointerPressedIn(intersect(tab, clip), buttonIndex(PointerButton::Left)) &&
            activeWidget_ == InvalidWidgetId;
        if (pressedHere)
        {
            activeWidget_ = candidate.id;
            dockDragSpace_ = activeDockSpace_;
            dockDragTab_ = candidate.id;
            dockDragStart_ = pointer_.pressedPosition[buttonIndex(PointerButton::Left)];
        }
        const float dragX = pointer_.position.x - dockDragStart_.x;
        const float dragY = pointer_.position.y - dockDragStart_.y;
        const bool draggingTab = dockDragSpace_ == activeDockSpace_ && dockDragTab_ == candidate.id &&
            activeWidget_ == candidate.id && (dragX * dragX + dragY * dragY >= 16.0f);
        if (draggingTab)
        {
            DockSlot target = candidate.slot;
            for (uint32_t targetIndex = 0u; targetIndex < 4u; ++targetIndex)
            {
                const DockSlot targetSlot = static_cast<DockSlot>(targetIndex);
                if (contains(dockSlotBounds(*dockSpace, targetSlot), pointer_.position))
                {
                    target = targetSlot;
                    break;
                }
            }
            const Rect targetBounds = dockSlotBounds(*dockSpace, target);
            WindowState *window = currentWindow();
            if (window)
            {
                const Color preview(theme_.focusColor.r, theme_.focusColor.g, theme_.focusColor.b, 145u);
                window->overlayDrawList.addRectFilled(targetBounds, preview, clip);
                window->overlayDrawList.addRect(targetBounds, theme_.focusColor, clip, 2.0f);
            }
            if (pointer_.released[buttonIndex(PointerButton::Left)])
            {
                const uint32_t targetIndex = dockSlotIndex(target);
                dockSpace->tabs[i].slot = target;
                dockSpace->selected[targetIndex] = candidate.id;
                activeWidget_ = InvalidWidgetId;
                dockDragSpace_ = InvalidWidgetId;
                dockDragTab_ = InvalidWidgetId;
            }
        }
        else if (handleTabInput && itemClicked(tab, clip, candidate.id, false))
        {
            dockSpace->selected[slotIndex] = candidate.id;
            focusedWidget_ = activeDockSpace_;
            dockDragSpace_ = InvalidWidgetId;
            dockDragTab_ = InvalidWidgetId;
        }
        const bool selected = dockSpace->selected[slotIndex] == candidate.id;
        if (selected || hovered)
            drawList->addRectFilled(tab, selected ? theme_.inputBg : theme_.selectableHovered, clip);
        if (selected)
            drawList->addRectFilled(Rect(tab.x + 4.0f, tab.bottom() - 2.0f,
                                         tab.width > 8.0f ? tab.width - 8.0f : 0.0f, 2.0f), theme_.focusColor, clip);
        const Rect titleClip = intersect(clip, Rect(tab.x, tab.y,
            candidate.open ? (closeButton.x > tab.x ? closeButton.x - tab.x : 0.0f) : tab.width, tab.height));
        drawText(*drawList, theme_.font, candidate.title,
                 Vec2(tab.x + theme_.windowPadding,
                      tab.y + (tab.height - metrics.height) * 0.5f),
                 theme_.fontSize, selected || hovered ? theme_.buttonText : theme_.textDisabled, titleClip);
        if (closeVisible)
        {
            if (closeHovered) drawList->addRectFilled(closeButton, theme_.buttonHovered, clip);
            const float cx = closeButton.x + closeButton.width * 0.5f;
            const float cy = closeButton.y + closeButton.height * 0.5f;
            const float radius = closeButton.height * 0.2f;
            const Color color = closeHovered ? theme_.buttonText : theme_.textDisabled;
            drawList->addLine(Vec2(cx - radius, cy - radius), Vec2(cx + radius, cy + radius), color, clip);
            drawList->addLine(Vec2(cx + radius, cy - radius), Vec2(cx - radius, cy + radius), color, clip);
        }
        tabX += tabWidth;
    }

    const WidgetId tabListId = combineIds(activeDockSpace_, 0x5441424c495354ull + slotIndex);
    const bool tabListHovered = itemHovered(tabListButton, clip, tabListId);
    if (handleTabInput && itemClicked(tabListButton, clip, tabListId, false))
        dockSpace->tabListOpen[slotIndex] = !dockSpace->tabListOpen[slotIndex];
    if (tabListHovered || dockSpace->tabListOpen[slotIndex])
        drawList->addRectFilled(Rect(tabListButton.x + 3.0f, tabListButton.y + 3.0f,
                                    tabListButton.width - 6.0f, tabListButton.height - 6.0f), theme_.buttonHovered, clip);
    const float arrowSize = tabListButton.height * 0.14f;
    const Vec2 arrow[] = {
        Vec2(tabListButton.x + tabListButton.width * 0.5f - arrowSize, tabListButton.y + tabListButton.height * 0.42f),
        Vec2(tabListButton.x + tabListButton.width * 0.5f + arrowSize, tabListButton.y + tabListButton.height * 0.42f),
        Vec2(tabListButton.x + tabListButton.width * 0.5f, tabListButton.y + tabListButton.height * 0.64f)
    };
    const Color arrowColor = tabListHovered || dockSpace->tabListOpen[slotIndex] ? theme_.buttonText : theme_.textDisabled;
    drawList->addLine(arrow[0], arrow[2], arrowColor, clip);
    drawList->addLine(arrow[2], arrow[1], arrowColor, clip);

    const float requestedPopupWidth = theme_.menuMinWidth;
    const float popupWidth = requestedPopupWidth < region.width ? requestedPopupWidth : region.width;
    const Rect tabListPopup(tabListButton.x + tabListButton.width - popupWidth,
                            tabListButton.y + tabListButton.height, popupWidth,
                            tabBar.height * static_cast<float>(tabCount + (tabCount == 1 ? 1 : 0)));
    if (handleTabInput && dockSpace->tabListOpen[slotIndex] && pointer_.pressed[buttonIndex(PointerButton::Left)] &&
        !contains(tabListButton, pointer_.pressedPosition[buttonIndex(PointerButton::Left)]) &&
        !contains(tabListPopup, pointer_.pressedPosition[buttonIndex(PointerButton::Left)]))
        dockSpace->tabListOpen[slotIndex] = false;
    if (dockSpace->tabListOpen[slotIndex] && tabCount > 0)
    {
        WindowState *window = currentWindow();
        if (window)
        {
            window->overlayDrawList.addRectFilled(tabListPopup, theme_.menuBg, clip);
            window->overlayDrawList.addRect(tabListPopup, theme_.menuBorder, clip);
            int row = 0;
            for (ct::Vector<DockTabState>::size_type i = 0u; i < dockSpace->tabs.size(); ++i)
            {
                const DockTabState &candidate = dockSpace->tabs[i];
                if (candidate.slot != resolvedSlot)
                    continue;
                const Rect item(tabListPopup.x, tabListPopup.y + tabBar.height * static_cast<float>(row),
                                tabListPopup.width, tabBar.height);
                const WidgetId itemId = combineIds(candidate.id, 0x5441424c495354ull);
                const bool hovered = itemHovered(item, clip, itemId);
                if (handleTabInput && itemClicked(item, clip, itemId, false))
                {
                    dockSpace->selected[slotIndex] = candidate.id;
                    dockSpace->tabListOpen[slotIndex] = false;
                }
                const bool selected = dockSpace->selected[slotIndex] == candidate.id;
                if (selected || hovered)
                    window->overlayDrawList.addRectFilled(item, selected ? theme_.selectableSelected
                                                                          : theme_.menuItemHover, clip);
                const TextMetrics metrics = measureText(theme_.font, candidate.title, theme_.fontSize);
                drawText(window->overlayDrawList, theme_.font, candidate.title,
                         Vec2(item.x + theme_.menuItemPadX,
                              item.y + (item.height - metrics.height) * 0.5f),
                         theme_.fontSize, theme_.menuItemText, clip);
                ++row;
            }
            if (tabCount == 1)
            {
                const Rect hideItem(tabListPopup.x, tabListPopup.y + tabBar.height,
                                    tabListPopup.width, tabBar.height);
                const WidgetId hideId = combineIds(activeDockSpace_, 0x48494445544142ull + slotIndex);
                const bool hovered = itemHovered(hideItem, clip, hideId);
                if (handleTabInput && itemClicked(hideItem, clip, hideId, false))
                {
                    dockSpace->tabBarHidden[slotIndex] = true;
                    dockSpace->tabListOpen[slotIndex] = false;
                }
                if (hovered)
                    window->overlayDrawList.addRectFilled(hideItem, theme_.menuItemHover, clip);
                const StringView hideText("Hide tab bar");
                const TextMetrics metrics = measureText(theme_.font, hideText, theme_.fontSize);
                drawText(window->overlayDrawList, theme_.font, hideText,
                         Vec2(hideItem.x + theme_.menuItemPadX,
                              hideItem.y + (hideItem.height - metrics.height) * 0.5f),
                         theme_.fontSize, theme_.menuItemText, clip);
            }
        }
    }
    }

    if (dockSpace->selected[slotIndex] != id)
        return false;
    DockPanelState panel;
    panel.parentLayout = layout_;
    const float contentTop = region.y + (tabBarVisible ? theme_.widgetHeight : hiddenTabBarHeight);
    panel.content = Rect(region.x + theme_.padding, contentTop + theme_.padding,
                         region.width - theme_.padding * 2.0f,
                         region.height - (tabBarVisible ? theme_.widgetHeight : hiddenTabBarHeight) - theme_.padding * 2.0f);
    panel.id = id;
    if (panel.content.width <= 0.0f || panel.content.height <= 0.0f)
        return false;
    dockPanelStack_.push_back(panel);
    const WidgetId parentId = idStack_.empty() ? InvalidWidgetId : idStack_.back();
    idStack_.push_back(combineIds(parentId, id));
    layout_.origin = Vec2(panel.content.x, panel.content.y);
    layout_.cursor = layout_.origin;
    layout_.baseOriginX = panel.content.x;
    layout_.lastItem = Rect();
    layout_.hasLastItem = false;
    return true;
}

void Context::endDockPanel()
{
    if (dockPanelStack_.empty())
        return;
    const DockPanelState panel = dockPanelStack_.back();
    dockPanelStack_.pop_back();
    if (!idStack_.empty())
        idStack_.pop_back();
    layout_ = panel.parentLayout;
}

bool Context::beginVirtualList(StringView idText, int itemCount, float itemHeight, float height,
                               int &firstVisible, int &lastVisible, bool border, float width)
{
    firstVisible = 0;
    lastVisible = 0;
    if (itemCount < 0 || itemHeight <= 0.0f || !beginChild(idText, height, border, width))
        return false;

    const ChildState &child = childStack_.back();
    ChildScrollState *scroll = childScrolls_.find(child.id);
    if (!scroll)
    {
        endChild();
        return false;
    }

    const int first = static_cast<int>(scroll->offset / itemHeight);
    int last = static_cast<int>((scroll->offset + child.content.height) / itemHeight) + 1;
    firstVisible = first < itemCount ? first : itemCount;
    lastVisible = last < itemCount ? last : itemCount;

    VirtualListState state;
    state.childId = child.id;
    state.content = child.content;
    state.itemCount = itemCount;
    state.itemHeight = itemHeight;
    state.scrollOffset = scroll->offset;
    virtualListStack_.push_back(state);
    return true;
}

Rect Context::virtualListItemRect(int itemIndex) const
{
    if (virtualListStack_.empty())
        return Rect();
    const VirtualListState &state = virtualListStack_.back();
    if (itemIndex < 0 || itemIndex >= state.itemCount)
        return Rect();
    return Rect(state.content.x - layout_.origin.x,
                state.content.y - layout_.origin.y +
                    static_cast<float>(itemIndex) * state.itemHeight - state.scrollOffset,
                state.content.width, state.itemHeight);
}

void Context::endVirtualList()
{
    if (virtualListStack_.empty())
        return;
    const VirtualListState state = virtualListStack_.back();
    virtualListStack_.pop_back();
    if (childStack_.empty() || childStack_.back().id != state.childId)
        return;

    layout_.lastItem = Rect(state.content.x,
                            state.content.y - state.scrollOffset +
                                static_cast<float>(state.itemCount) * state.itemHeight,
                            state.content.width, 0.0f);
    layout_.hasLastItem = true;
    endChild();
}

bool Context::beginVirtualTable(StringView idText, int rowCount, int columns, float rowHeight,
                                float height, int &firstVisibleRow, int &lastVisibleRow,
                                bool border, float width)
{
    firstVisibleRow = 0;
    lastVisibleRow = 0;
    if (columns <= 0 || !beginVirtualList(idText, rowCount, rowHeight, height,
                                          firstVisibleRow, lastVisibleRow, border, width))
        return false;

    VirtualTableState state;
    state.childId = virtualListStack_.back().childId;
    state.columns = columns;
    virtualTableStack_.push_back(state);
    return true;
}

bool Context::beginVirtualTable(StringView idText, int rowCount, Span<const float> columnWeights,
                                float rowHeight, float height,
                                int &firstVisibleRow, int &lastVisibleRow,
                                bool border, float width)
{
    if (columnWeights.empty())
        return false;
    float totalWeight = 0.0f;
    for (Span<const float>::size_type i = 0u; i < columnWeights.size(); ++i)
    {
        if (columnWeights[i] <= 0.0f)
            return false;
        totalWeight += columnWeights[i];
    }
    if (totalWeight <= 0.0f ||
        !beginVirtualTable(idText, rowCount, static_cast<int>(columnWeights.size()), rowHeight, height,
                           firstVisibleRow, lastVisibleRow, border, width))
        return false;

    VirtualTableState &state = virtualTableStack_.back();
    state.columnWeights.reserve(columnWeights.size());
    for (Span<const float>::size_type i = 0u; i < columnWeights.size(); ++i)
        state.columnWeights.push_back(columnWeights[i]);
    state.totalColumnWeight = totalWeight;
    return true;
}

Rect Context::virtualTableCellRect(int row, int column) const
{
    if (virtualTableStack_.empty() || virtualListStack_.empty())
        return Rect();
    const VirtualTableState &table = virtualTableStack_.back();
    if (table.childId != virtualListStack_.back().childId || column < 0 || column >= table.columns)
        return Rect();

    const Rect rowRect = virtualListItemRect(row);
    if (rowRect.width <= 0.0f || rowRect.height <= 0.0f)
        return Rect();
    float cellX = rowRect.x;
    if (!table.columnWeights.empty() && table.totalColumnWeight > 0.0f)
    {
        for (int index = 0; index < column; ++index)
        {
            cellX += rowRect.width *
                     table.columnWeights[static_cast<ct::Vector<float>::size_type>(index)] /
                     table.totalColumnWeight;
        }
    }
    else
    {
        cellX += rowRect.width * static_cast<float>(column) / static_cast<float>(table.columns);
    }
    const float width = column + 1 == table.columns ? rowRect.x + rowRect.width - cellX
                       : (!table.columnWeights.empty() && table.totalColumnWeight > 0.0f
                           ? rowRect.width * table.columnWeights[static_cast<ct::Vector<float>::size_type>(column)] /
                             table.totalColumnWeight
                           : rowRect.width / static_cast<float>(table.columns));
    return Rect(cellX, rowRect.y, width, rowRect.height);
}

void Context::endVirtualTable()
{
    if (virtualTableStack_.empty())
        return;
    const VirtualTableState state = virtualTableStack_.back();
    virtualTableStack_.pop_back();
    if (virtualListStack_.empty() || virtualListStack_.back().childId != state.childId)
        return;
    endVirtualList();
}

bool Context::beginVirtualTree(StringView idText, int rowCount, float rowHeight, float height,
                               int &firstVisibleRow, int &lastVisibleRow, bool border, float width)
{
    firstVisibleRow = 0;
    lastVisibleRow = 0;
    if (!beginVirtualList(idText, rowCount, rowHeight, height,
                          firstVisibleRow, lastVisibleRow, border, width))
        return false;

    VirtualTreeState state;
    state.childId = virtualListStack_.back().childId;
    virtualTreeStack_.push_back(state);
    return true;
}

Rect Context::virtualTreeItemRect(int row, int depth, float indentWidth) const
{
    if (virtualTreeStack_.empty() || virtualListStack_.empty() || depth < 0 || indentWidth < 0.0f ||
        virtualTreeStack_.back().childId != virtualListStack_.back().childId)
        return Rect();

    const Rect rowRect = virtualListItemRect(row);
    const float indentation = static_cast<float>(depth) * indentWidth;
    if (rowRect.width <= indentation || rowRect.height <= 0.0f)
        return Rect();
    return Rect(rowRect.x + indentation, rowRect.y, rowRect.width - indentation, rowRect.height);
}

void Context::endVirtualTree()
{
    if (virtualTreeStack_.empty())
        return;
    const VirtualTreeState state = virtualTreeStack_.back();
    virtualTreeStack_.pop_back();
    if (virtualListStack_.empty() || virtualListStack_.back().childId != state.childId)
        return;
    endVirtualList();
}

bool Context::beginTable(StringView idText, int columns, float width)
{
    if (!currentWindow() || columns <= 0 || tableActive_ || propertyRowActive_)
        return false;
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    if (resolvedWidth <= 0.0f)
        return false;
    table_.parentLayout = layout_;
    table_.bounds = Rect(layout_.cursor.x, layout_.cursor.y, resolvedWidth, 0.0f);
    const WidgetId parentId = idStack_.empty() ? InvalidWidgetId : idStack_.back();
    table_.id = combineIds(parentId, combineIds(makeWidgetId(idText), 0x5441424c45ull));
    table_.columns = columns;
    table_.column = -1;
    table_.rowY = layout_.cursor.y;
    table_.rowHeight = 0.0f;
    tableActive_ = true;
    idStack_.push_back(table_.id);
    return true;
}

bool Context::beginTable(StringView idText, Span<const float> columnWeights, float width)
{
    if (columnWeights.empty())
        return false;
    float totalWeight = 0.0f;
    for (Span<const float>::size_type i = 0u; i < columnWeights.size(); ++i)
    {
        if (columnWeights[i] <= 0.0f)
            return false;
        totalWeight += columnWeights[i];
    }
    if (totalWeight <= 0.0f ||
        !beginTable(idText, static_cast<int>(columnWeights.size()), width))
        return false;

    table_.columnWeights.clear();
    table_.columnWeights.reserve(columnWeights.size());
    for (Span<const float>::size_type i = 0u; i < columnWeights.size(); ++i)
        table_.columnWeights.push_back(columnWeights[i]);
    table_.totalColumnWeight = totalWeight;
    return true;
}

bool Context::beginTable(StringView idText, Span<float> columnWeights, float width)
{
    if (columnWeights.empty())
        return false;
    if (!beginTable(idText, Span<const float>(columnWeights.data(), columnWeights.size()), width))
        return false;
    table_.resizableColumnWeights = columnWeights.data();
    return true;
}

float Context::tableColumnX(int column) const
{
    float x = table_.bounds.x;
    for (int index = 0; index < column; ++index)
        x += tableColumnWidth(index);
    return x;
}

float Context::tableColumnWidth(int column) const
{
    if (column < 0 || column >= table_.columns || table_.columns <= 0)
        return 0.0f;
    if (table_.columnWeights.empty() || table_.totalColumnWeight <= 0.0f)
        return table_.bounds.width / static_cast<float>(table_.columns);
    if (column + 1 == table_.columns)
        return table_.bounds.x + table_.bounds.width - tableColumnX(column);
    return table_.bounds.width * table_.columnWeights[static_cast<ct::Vector<float>::size_type>(column)] /
           table_.totalColumnWeight;
}

bool Context::tableNextColumn()
{
    if (!tableActive_)
        return false;
    if (table_.column >= 0 && layout_.hasLastItem)
    {
        const float itemHeight = layout_.lastItem.y + layout_.lastItem.height - table_.rowY;
        if (itemHeight > table_.rowHeight)
            table_.rowHeight = itemHeight;
    }
    if (table_.column + 1 >= table_.columns)
    {
        const float rowHeight = table_.rowHeight > theme_.widgetHeight ? table_.rowHeight : theme_.widgetHeight;
        table_.rowY += rowHeight + theme_.itemSpacing;
        table_.column = 0;
        table_.rowHeight = 0.0f;
    }
    else
    {
        ++table_.column;
    }
    const float cellX = tableColumnX(table_.column);
    layout_.origin.x = cellX;
    layout_.baseOriginX = cellX;
    layout_.cursor = Vec2(cellX, table_.rowY);
    layout_.lastItem = Rect();
    layout_.hasLastItem = false;
    return true;
}

bool Context::tableHeader(StringView labelText, int &sortColumn, bool &sortAscending)
{
    if (!tableActive_ || table_.column < 0 || table_.column >= table_.columns)
        return false;
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;

    const float width = availableWidth();
    if (width <= 0.0f)
        return false;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, width, theme_.widgetHeight);
    const Rect clip = contentClip();
    const WidgetId columnId = combineIds(makeWidgetId(labelText),
                                         static_cast<uint64_t>(table_.column + 1));
    const WidgetId id = combineIds(columnId, 0x5441424c45484452ull);
    const uint32_t left = buttonIndex(PointerButton::Left);
    const bool canResize = table_.resizableColumnWeights != nullptr && table_.column + 1 < table_.columns;
    const float gripWidth = 6.0f;
    const Rect resizeGrip(bounds.right() - gripWidth * 0.5f, bounds.y, gripWidth, bounds.height);
    const WidgetId resizeId = combineIds(id, 0x54424c5253495a45ull);
    const bool pressedResize = canResize && pointerPressedIn(intersect(resizeGrip, clip), left) &&
                               activeWidget_ == InvalidWidgetId;
    if (pressedResize)
    {
        activeWidget_ = resizeId;
        focusedWidget_ = resizeId;
        tableResize_.tableId = table_.id;
        tableResize_.column = table_.column;
        tableResize_.lastPointerX = pointer_.position.x;
    }

    const bool resizing = activeWidget_ == resizeId && tableResize_.tableId == table_.id &&
                           tableResize_.column == table_.column;
    if (resizing && (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left]))
    {
        const float deltaX = pointer_.position.x - tableResize_.lastPointerX;
        const float leftWidth = tableColumnWidth(table_.column);
        const float rightWidth = tableColumnWidth(table_.column + 1);
        const float pairWidth = leftWidth + rightWidth;
        const float minimumWidth = 32.0f;
        if (pairWidth > minimumWidth * 2.0f && deltaX != 0.0f)
        {
            const float nextLeftWidth = clamp(leftWidth + deltaX, minimumWidth, pairWidth - minimumWidth);
            const float nextRightWidth = pairWidth - nextLeftWidth;
            const float scale = table_.totalColumnWeight / table_.bounds.width;
            const ct::Vector<float>::size_type column = static_cast<ct::Vector<float>::size_type>(table_.column);
            table_.columnWeights[column] = nextLeftWidth * scale;
            table_.columnWeights[column + 1u] = nextRightWidth * scale;
            table_.resizableColumnWeights[table_.column] = table_.columnWeights[column];
            table_.resizableColumnWeights[table_.column + 1] = table_.columnWeights[column + 1u];
        }
        tableResize_.lastPointerX = pointer_.position.x;
    }
    if (resizing && pointer_.released[left])
    {
        activeWidget_ = InvalidWidgetId;
        tableResize_ = TableResizeState();
    }

    const bool hovered = !resizing && itemHovered(bounds, clip, id);
    const bool clicked = !resizing && itemClicked(bounds, clip, id);
    bool changed = false;
    if (clicked)
    {
        if (sortColumn == table_.column)
            sortAscending = !sortAscending;
        else
        {
            sortColumn = table_.column;
            sortAscending = true;
        }
        changed = true;
    }

    const bool sorted = sortColumn == table_.column;
    const Color background = sorted ? theme_.selectableSelected
                           : (hovered ? theme_.buttonHovered : theme_.panelColor);
    drawList->addRectFilled(bounds, background, clip);
    drawList->addRect(bounds, theme_.borderColor, clip);
    if (canResize)
    {
        const bool resizeHovered = resizing || contains(intersect(resizeGrip, clip), pointer_.position);
        const Color gripColor = resizeHovered ? theme_.dialogBtnPrimary : theme_.borderColor;
        drawList->addLine(Vec2(bounds.right(), bounds.y + 2.0f),
                          Vec2(bounds.right(), bounds.bottom() - 2.0f), gripColor, clip,
                          resizeHovered ? 2.0f : 1.0f);
    }

    const float padding = theme_.windowPadding * 0.5f;
    const float arrowSize = bounds.height * 0.28f;
    const float arrowReserve = sorted ? arrowSize + padding : 0.0f;
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    drawText(*drawList, theme_.font, labelText,
             Vec2(bounds.x + padding,
                  bounds.y + (bounds.height - metrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, clip);
    if (sorted && bounds.width > padding * 2.0f + arrowReserve)
    {
        const float centerX = bounds.x + bounds.width - padding - arrowSize * 0.5f;
        const float centerY = bounds.y + bounds.height * 0.5f;
        const Vec2 arrow[] = {
            sortAscending ? Vec2(centerX - arrowSize * 0.5f, centerY + arrowSize * 0.35f)
                          : Vec2(centerX - arrowSize * 0.5f, centerY - arrowSize * 0.35f),
            sortAscending ? Vec2(centerX + arrowSize * 0.5f, centerY + arrowSize * 0.35f)
                          : Vec2(centerX + arrowSize * 0.5f, centerY - arrowSize * 0.35f),
            sortAscending ? Vec2(centerX, centerY - arrowSize * 0.5f)
                          : Vec2(centerX, centerY + arrowSize * 0.5f)
        };
        drawList->addPolygonFilled(Span<const Vec2>(arrow), theme_.labelText, clip);
    }
    advanceLayout(bounds);
    return changed;
}

void Context::endTable()
{
    if (!tableActive_)
        return;
    if (table_.column >= 0 && layout_.hasLastItem)
    {
        const float itemHeight = layout_.lastItem.y + layout_.lastItem.height - table_.rowY;
        if (itemHeight > table_.rowHeight)
            table_.rowHeight = itemHeight;
    }
    float height = 0.0f;
    if (table_.column >= 0)
    {
        const float rowHeight = table_.rowHeight > theme_.widgetHeight ? table_.rowHeight : theme_.widgetHeight;
        height = table_.rowY + rowHeight - table_.bounds.y;
    }
    const LayoutState parent = table_.parentLayout;
    const Rect bounds(parent.cursor.x, parent.cursor.y, table_.bounds.width, height);
    table_ = TableState();
    tableActive_ = false;
    if (!idStack_.empty())
        idStack_.pop_back();
    layout_ = parent;
    if (height > 0.0f)
        advanceLayout(bounds);
}

bool Context::beginPropertyRow(StringView labelText, float labelWidth)
{
    if (!currentWindow() || propertyRowActive_ || tableActive_)
        return false;
    const float width = availableWidth();
    if (width <= 0.0f)
        return false;
    const float resolvedLabelWidth = labelWidth > 0.0f ? labelWidth : width * 0.38f;
    const float valueX = layout_.cursor.x + resolvedLabelWidth;
    if (valueX >= layout_.cursor.x + width)
        return false;
    propertyRow_.parentLayout = layout_;
    propertyRow_.bounds = Rect(layout_.cursor.x, layout_.cursor.y, width, theme_.widgetHeight);
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    drawText(*drawList, theme_.font, labelText,
             Vec2(propertyRow_.bounds.x, propertyRow_.bounds.y +
                  (propertyRow_.bounds.height - metrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, contentClip());
    const WidgetId parentId = idStack_.empty() ? InvalidWidgetId : idStack_.back();
    idStack_.push_back(combineIds(parentId, combineIds(makeWidgetId(labelText), 0x50524f50524f57ull)));
    layout_.origin.x = valueX;
    layout_.baseOriginX = valueX;
    layout_.cursor = Vec2(valueX, propertyRow_.bounds.y);
    layout_.lastItem = Rect();
    layout_.hasLastItem = false;
    propertyRowActive_ = true;
    return true;
}

void Context::endPropertyRow()
{
    if (!propertyRowActive_)
        return;
    float height = propertyRow_.bounds.height;
    if (layout_.hasLastItem)
    {
        const float itemHeight = layout_.lastItem.y + layout_.lastItem.height - propertyRow_.bounds.y;
        if (itemHeight > height)
            height = itemHeight;
    }
    const LayoutState parent = propertyRow_.parentLayout;
    const Rect bounds(parent.cursor.x, parent.cursor.y, propertyRow_.bounds.width, height);
    propertyRow_ = PropertyRowState();
    propertyRowActive_ = false;
    if (!idStack_.empty())
        idStack_.pop_back();
    layout_ = parent;
    advanceLayout(bounds);
}

void Context::progressBar(float value, float maximum, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    progressBar(value, maximum,
                Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                     bounds.width, bounds.height));
    advanceLayout(bounds);
}

void Context::label(StringView text)
{
    const TextMetrics metrics = measureText(theme_.font, text, theme_.fontSize);
    label(text, Vec2(layout_.cursor.x - layout_.origin.x, layout_.cursor.y - layout_.origin.y));
    advanceLayout(Rect(layout_.cursor.x, layout_.cursor.y, metrics.width, metrics.height));
}

void Context::sameLine(float spacingValue)
{
    if (!layout_.hasLastItem)
        return;
    const float itemSpacing = spacingValue >= 0.0f ? spacingValue : theme_.itemSpacing;
    layout_.cursor.x = layout_.lastItem.x + layout_.lastItem.width + itemSpacing;
    layout_.cursor.y = layout_.lastItem.y;
}

void Context::spacing(float pixels)
{
    if (pixels > 0.0f)
        layout_.cursor.y += pixels;
}

void Context::dummy(float width, float height)
{
    if (!currentWindow() || width <= 0.0f || height <= 0.0f)
        return;
    advanceLayout(Rect(layout_.cursor.x, layout_.cursor.y, width, height));
}

void Context::indent(float pixels)
{
    if (!currentWindow() || pixels <= 0.0f)
        return;
    layout_.origin.x += pixels;
    layout_.cursor.x = layout_.origin.x;
    layout_.hasLastItem = false;
}

void Context::unindent(float pixels)
{
    if (!currentWindow() || pixels <= 0.0f)
        return;
    const float nextOrigin = layout_.origin.x - pixels;
    layout_.origin.x = nextOrigin > layout_.baseOriginX ? nextOrigin : layout_.baseOriginX;
    layout_.cursor.x = layout_.origin.x;
    layout_.hasLastItem = false;
}

void Context::separator(float thickness)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList || thickness <= 0.0f)
        return;
    const Rect clip = contentClip();
    const float right = window->bounds.x + window->bounds.width - theme_.windowPadding;
    const Rect line(layout_.cursor.x, layout_.cursor.y,
                    right > layout_.cursor.x ? right - layout_.cursor.x : 0.0f, thickness);
    drawList->addRectFilled(line, theme_.titleBarBackground, clip);
    advanceLayout(line);
}

void Context::separatorText(StringView text, float width)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList)
        return;

    const Rect clip = contentClip();
    const TextMetrics metrics = measureText(theme_.font, text, theme_.fontSize);
    const float available = window->bounds.x + window->bounds.width - theme_.windowPadding - layout_.cursor.x;
    const float resolvedWidth = width > 0.0f && width < available ? width : available;
    const float lineY = layout_.cursor.y + metrics.height * 0.5f;
    drawText(*drawList, theme_.font, text, layout_.cursor, theme_.fontSize, theme_.labelText, clip);
    const float lineX = layout_.cursor.x + metrics.width + theme_.windowPadding;
    if (resolvedWidth > lineX - layout_.cursor.x)
        drawList->addRectFilled(Rect(lineX, lineY, resolvedWidth - (lineX - layout_.cursor.x), 1.0f),
                                theme_.titleBarBackground, clip);
    advanceLayout(Rect(layout_.cursor.x, layout_.cursor.y, resolvedWidth, metrics.height));
}

void Context::setCursor(const Vec2 &localPosition)
{
    layout_.cursor = Vec2(layout_.origin.x + localPosition.x, layout_.origin.y + localPosition.y);
    layout_.hasLastItem = false;
}

Vec2 Context::cursor() const
{
    return Vec2(layout_.cursor.x - layout_.origin.x, layout_.cursor.y - layout_.origin.y);
}

float Context::availableHeight() const
{
    const WindowState *window = currentWindow();
    if (!window) return 0.0f;
    const float padding = window->useClientArea ? 0.0f : theme_.windowPadding;
    float bottom = window->bounds.y + window->bounds.height - padding;
    if (!dockPanelStack_.empty())
        bottom = dockPanelStack_.back().content.y + dockPanelStack_.back().content.height;
    if (!childStack_.empty())
        bottom = childStack_.back().content.y + childStack_.back().content.height;
    return bottom > layout_.cursor.y ? bottom - layout_.cursor.y : 0.0f;
}

float Context::availableWidth() const
{
    const WindowState *window = currentWindow();
    if (!window)
        return 0.0f;
    const float padding = window->useClientArea ? 0.0f : theme_.windowPadding;
    float right = window->bounds.x + window->bounds.width - padding;
    if (!childStack_.empty())
        right = childStack_.back().content.x + childStack_.back().content.width;
    if (!dockPanelStack_.empty())
        right = dockPanelStack_.back().content.x + dockPanelStack_.back().content.width;
    if (tableActive_ && table_.columns > 0 && table_.column >= 0)
        right = tableColumnX(table_.column) + tableColumnWidth(table_.column);
    if (propertyRowActive_)
        right = propertyRow_.bounds.x + propertyRow_.bounds.width;
    return right > layout_.cursor.x ? right - layout_.cursor.x : 0.0f;
}

WidgetId Context::hashText(StringView text)
{
    uint64_t hash = 1469598103934665603ull;
    for (StringView::size_type i = 0; i < text.size(); ++i)
    {
        hash ^= static_cast<uint8_t>(text[i]);
        hash *= 1099511628211ull;
    }
    return hash == InvalidWidgetId ? 1u : hash;
}

WidgetId Context::combineIds(WidgetId a, WidgetId b)
{
    uint64_t hash = a ^ 0x9e3779b97f4a7c15ull;
    hash ^= b + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
    return hash == InvalidWidgetId ? 1u : hash;
}

WidgetId Context::makeWidgetId(StringView labelText) const
{
    const WidgetId parent = idStack_.empty() ? InvalidWidgetId : idStack_.back();
    return combineIds(parent, hashText(labelText));
}

void Context::consumeEvents()
{
    while (!events_.empty())
    {
        const Event event = events_.front();
        events_.pop_front();
        switch (event.type)
        {
        case EventType::PointerMove:
            pointer_.position = event.position;
            break;
        case EventType::PointerDown:
        {
            const uint32_t index = buttonIndex(event.button);
            pointer_.position = event.position;
            pointer_.pressedPosition[index] = event.position;
            pointer_.down[index] = true;
            pointer_.pressed[index] = true;
            break;
        }
        case EventType::PointerUp:
        {
            const uint32_t index = buttonIndex(event.button);
            pointer_.position = event.position;
            pointer_.releasedPosition[index] = event.position;
            pointer_.down[index] = false;
            pointer_.released[index] = true;
            break;
        }
        case EventType::PointerWheel:
            pointer_.wheelX += event.wheelX;
            pointer_.wheelY += event.wheelY;
            pointer_.wheelControl = event.control;
            break;
        case EventType::KeyDown:
        {
            const uint32_t keyIndex = static_cast<uint32_t>(event.key);
            if (keyIndex < 32u)
            {
                keyPressed_[keyIndex] = true;
                keyControl_[keyIndex] = event.control;
                keyShift_[keyIndex] = event.shift;
            }
            if (event.key == KeyCode::Backspace)
                backspacePressed_ = true;
            else if (event.key == KeyCode::Enter)
                enterPressed_ = true;
            else if (event.key == KeyCode::Tab)
            {
                tabPressed_ = true;
                tabShiftPressed_ = event.shift;
            }
            else if (event.key == KeyCode::Home)
                homePressed_ = true;
            else if (event.key == KeyCode::End)
                endPressed_ = true;
            else if (event.key == KeyCode::Up)
                upPressed_ = true;
            else if (event.key == KeyCode::Down)
                downPressed_ = true;
            else if (event.key == KeyCode::Left)
                leftPressed_ = true;
            else if (event.key == KeyCode::Right)
                rightPressed_ = true;
            else if (event.key == KeyCode::PageUp)
                pageUpPressed_ = true;
            else if (event.key == KeyCode::PageDown)
                pageDownPressed_ = true;
            else if (event.key == KeyCode::Escape)
            {
                escapePressed_ = true;
                openCombo_ = InvalidWidgetId;
                openMenu_ = InvalidWidgetId;
                openContextMenu_ = InvalidWidgetId;
                openSubMenu_ = InvalidWidgetId;
                focusedWidget_ = InvalidWidgetId;
                textInputWidget_ = InvalidWidgetId;
            }
            else if (event.control && event.key == KeyCode::C)
                copyRequested_ = true;
            else if (event.control && event.key == KeyCode::V)
                pasteRequested_ = true;
            break;
        }
        case EventType::TextInput:
            if (event.textLength != 0u)
                textEvents_.push_back(event);
            break;
        case EventType::FocusLost:
            pointer_.down[0] = false;
            pointer_.down[1] = false;
            pointer_.down[2] = false;
            pointer_.pressed[0] = false;
            pointer_.pressed[1] = false;
            pointer_.pressed[2] = false;
            pointer_.released[0] = false;
            pointer_.released[1] = false;
            pointer_.released[2] = false;
            activeWidget_ = InvalidWidgetId;
            dockDragSpace_ = InvalidWidgetId;
            dockDragTab_ = InvalidWidgetId;
            draggingWindow_ = WindowHandle();
            resizingWindow_ = WindowHandle();
            focusedWidget_ = InvalidWidgetId;
            textInputWidget_ = InvalidWidgetId;
            openCombo_ = InvalidWidgetId;
            dragDrop_ = DragDropState();
            tableResize_ = TableResizeState();
            break;
        case EventType::ViewportChanged:
            frame_.displaySize = event.viewportSize;
            frame_.dpiScale = event.dpiScale;
            break;
        case EventType::None:
            break;
        }
    }
}

WindowState *Context::currentWindow()
{
    return windows_.get(currentWindow_);
}

DrawList *Context::currentDrawList()
{
    WindowState *window = currentWindow();
    return window ? &window->drawList : nullptr;
}

TextMetrics Context::measureText(FontId font, StringView text, float logicalSize) const
{
    return textProvider_ ? textProvider_->measureText(font, text, logicalSize)
                         : backend_.measureText(font, text, logicalSize, frame_.dpiScale);
}

void Context::drawText(DrawList &drawList, FontId font, StringView text, const Vec2 &position,
                       float logicalSize, const Color &color, const Rect &clip)
{
    if (!textProvider_ || !textProvider_->appendText(drawList, font, text, position,
                                                      logicalSize, color, clip))
        drawList.addText(text, position, font, logicalSize, color, clip);
}

void Context::drawDragDropPreview()
{
    if (!dragDrop_.active || dragDrop_.preview.empty())
        return;
    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const TextMetrics metrics = measureText(theme_.font, dragDrop_.preview, theme_.fontSize);
    const float padding = theme_.tooltipPadX;
    const Rect preview(pointer_.position.x + 14.0f, pointer_.position.y + 16.0f,
                       metrics.width + padding * 2.0f, metrics.height + theme_.tooltipPadY * 2.0f);
    dragDropDrawList_.addRectFilled(preview, theme_.tooltipBg, viewport);
    dragDropDrawList_.addRect(preview, theme_.dialogBtnPrimary, viewport);
    drawText(dragDropDrawList_, theme_.font, dragDrop_.preview,
             Vec2(preview.x + padding, preview.y + theme_.tooltipPadY),
             theme_.fontSize, theme_.tooltipText, viewport);
}

void Context::drawToasts()
{
    struct ToastStack { float offset = 0.0f; };
    ToastStack stacks[9];
    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const float padding = theme_.tooltipPadX;
    for (ct::Vector<ToastState>::size_type i = toasts_.size(); i > 0u; --i)
    {
        const ToastState &toast = toasts_[i-1u];
        if (toast.remaining <= 0.0f)
            continue;
        const TextMetrics metrics = measureText(theme_.font, toast.text, theme_.fontSize);
        const float width = metrics.width + padding * 2.0f + 4.0f;
        const float height = metrics.height + theme_.tooltipPadY * 2.0f;
        float x = padding;
        float y = padding;
        const uint8_t anchor = static_cast<uint8_t>(toast.position);
        const uint8_t horizontal = anchor % 3u;
        const uint8_t vertical = anchor / 3u;
        if (anchor >= 9u) continue;
        if (horizontal == 1u)
            x = (viewport.width - width) * 0.5f;
        else if (horizontal == 2u)
            x = viewport.width - width - padding;
        if (vertical == 1u)
            y = (viewport.height - height) * 0.5f;
        else if (vertical == 2u)
            y = viewport.height - height - padding;
        y += vertical == 2u ? -stacks[anchor].offset : stacks[anchor].offset;
        stacks[anchor].offset += height + 8.0f;
        const float fadeTime = toast.duration < 0.4f ? toast.duration * 0.5f : 0.2f;
        const float entering = clamp((toast.duration-toast.remaining)/fadeTime,0.0f,1.0f);
        const float leaving = clamp(toast.remaining/fadeTime,0.0f,1.0f);
        const float alpha = entering < leaving ? entering : leaving;
        const auto faded = [alpha](Color color) {
            color.a = static_cast<uint8_t>(color.a * alpha);
            return color;
        };
        const Rect bounds(x, y, width, height);
        toastDrawList_.addRectFilled(bounds, faded(theme_.tooltipBg), viewport);
        toastDrawList_.addRect(bounds, faded(theme_.tooltipBorder), viewport);
        toastDrawList_.addRectFilled(Rect(bounds.x, bounds.y, 4.0f, bounds.height),
                                     faded(theme_.dialogBtnPrimary), viewport);
        drawText(toastDrawList_, theme_.font, toast.text,
                 Vec2(bounds.x + padding + 4.0f, bounds.y + theme_.tooltipPadY),
                 theme_.fontSize, faded(theme_.tooltipText), viewport);
    }
}

void Context::beginLayout(const WindowState &window)
{
    const float titleHeight = window.showTitleBar ? theme_.titleBarHeight : 0.0f;
    const float padding = window.useClientArea ? 0.0f : theme_.windowPadding;
    const Vec2 origin(window.bounds.x + padding, window.bounds.y + titleHeight + padding);
    layout_.origin = origin;
    layout_.cursor = origin;
    layout_.baseOriginX = origin.x;
    layout_.lastItem = Rect();
    layout_.hasLastItem = false;
}

void Context::advanceLayout(const Rect &item)
{
    layout_.lastItem = item;
    layout_.hasLastItem = true;
    layout_.cursor.x = layout_.origin.x;
    layout_.cursor.y = item.y + item.height + theme_.itemSpacing;
}

float Context::autoButtonWidth(StringView labelText) const
{
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const float width = metrics.width + theme_.windowPadding * 2.0f;
    return width > 64.0f ? width : 64.0f;
}

const WindowState *Context::currentWindow() const
{
    return windows_.get(currentWindow_);
}

WindowState *Context::getOrCreateWindow(WidgetId id, StringView title,
                                        const Rect &bounds)
{
    WindowHandle *existing = windowsById_.find(id);
    if (existing)
        return windows_.get(*existing);

    WindowState state;
    state.id = id;
    state.title = String(title.data(), title.size());
    state.bounds = bounds;
    state.zOrder = nextZOrder_++;
    // A choice recorded by setWindowButtons before this window existed.
    uint8_t *mask = windowButtons_.find(id);
    if (mask)
    {
        state.showMinimizeButton = (*mask & 1u) != 0;
        state.showMaximizeButton = (*mask & 2u) != 0;
    }
    const WindowHandle handle = windows_.emplace(state);
    windowsById_.put(id, handle);
    windowOrder_.push_back(handle);
    WindowState *created = windows_.get(handle);

    // A window the state file knows is created where it was left rather than
    // at the bounds the application asked for.
    WindowGeometry saved;
    saved.x = bounds.x;
    saved.y = bounds.y;
    saved.w = bounds.width;
    saved.h = bounds.height;
    if (created && windowState_.getWindow(created->title, saved))
        applyWindowGeometry(*created, saved);
    return created;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Window state (.ini)
// ─────────────────────────────────────────────────────────────────────────────

Context::~Context()
{
    saveWindowState();
}

void Context::setWindowStatePath(const String &path)
{
    windowState_.setPath(path);
    if (!windowState_.path().empty())
        loadWindowState();
}

void Context::windowGeometryOf(const WindowState &window, WindowGeometry &out)
{
    const Rect &source = window.maximized && window.hasRestoreBounds ? window.restoreBounds
                                                                    : window.bounds;
    out.x = source.x;
    out.y = source.y;
    out.w = source.width;
    out.h = source.height;
    out.minimized = window.minimized;
    out.maximized = window.maximized;
}

void Context::applyWindowGeometry(WindowState &window, const WindowGeometry &geometry)
{
    window.bounds = Rect(geometry.x, geometry.y, geometry.w, geometry.h);
    window.restoreBounds = window.bounds;
    // Only a maximized window needs the bounds it should return to; a normal
    // one has no restore state at all.
    window.hasRestoreBounds = geometry.maximized;
    window.minimized = geometry.minimized;
    window.maximized = geometry.maximized;

    // The file was written on whatever screen the application had then: a window
    // it puts outside the one used now could never be grabbed again (there is no
    // public way to move it), so it is fitted as soon as the viewport is known.
    if (frame_.displaySize.x > 0.0f && frame_.displaySize.y > 0.0f)
        clampWindowToViewport(window);
    else
        window.clampToViewport = true;
}

void Context::clampWindowToViewport(WindowState &window)
{
    const float titleHeight = window.showTitleBar ? theme_.titleBarHeight : 0.0f;
    const float controlWidth = theme_.titleBarHeight;
    const float controlsCount = 1.0f + (window.showMinimizeButton ? 1.0f : 0.0f) +
                                (window.showMaximizeButton ? 1.0f : 0.0f);
    const float controls = window.showWindowControls ? controlWidth * controlsCount : 0.0f;
    const float grip = 40.0f;
    const float minimumX = grip + controls - window.bounds.width;
    const float maximumX = frame_.displaySize.x - grip;
    const float maximumY = frame_.displaySize.y - titleHeight;
    if (window.bounds.x > maximumX) window.bounds.x = maximumX;
    if (window.bounds.x < minimumX) window.bounds.x = minimumX;
    if (window.bounds.y > maximumY) window.bounds.y = maximumY;
    if (window.bounds.y < 0.0f) window.bounds.y = 0.0f;
}

bool Context::loadWindowState()
{
    const bool loaded = windowState_.load();
    for (WindowState &window : windows_.items())
    {
        WindowGeometry geometry;
        if (windowState_.getWindow(window.title, geometry))
            applyWindowGeometry(window, geometry);
    }
    return loaded;
}

bool Context::saveWindowState()
{
    for (const WindowState &window : windows_.items())
    {
        WindowGeometry geometry;
        windowGeometryOf(window, geometry);
        windowState_.putWindow(window.title, geometry);
    }
    // A dock space keeps the sizes its regions were dragged to and which panel
    // sits in which of them.
    for (auto& entry : dockSpaces_)
    {
        const DockSpaceState &space = entry.value;
        const String section = dockStateSection(space.name);
        windowState_.putNumber(section, "left_width", space.leftWidth);
        windowState_.putNumber(section, "right_width", space.rightWidth);
        windowState_.putNumber(section, "bottom_height", space.bottomHeight);
        captureDockArrangement(space);
    }
    return windowState_.save();
}

DockSlot Context::storedDockSlot(StringView spaceName, StringView title, DockSlot fallback) const
{
    const String section = dockStateSection(spaceName);
    if (section.empty())
        return fallback;
    for (const DockSlotName &slotName : kDockSlotNames)
    {
        const String key = dockKey("tabs_", slotName.key);
        if (dockListHas(windowState_.get(section, key.c_str()), title))
            return slotName.slot;
    }
    return fallback;
}

bool Context::storedDockTabOnTop(StringView spaceName, DockSlot slot, StringView title) const
{
    const String section = dockStateSection(spaceName);
    if (section.empty())
        return false;
    for (const DockSlotName &slotName : kDockSlotNames)
    {
        if (slotName.slot != slot)
            continue;
        const String key = dockKey("selected_", slotName.key);
        return windowState_.get(section, key.c_str()) == String(title.data(), title.size());
    }
    return false;
}

void Context::captureDockArrangement(const DockSpaceState &space)
{
    const String section = dockStateSection(space.name);
    if (section.empty())
        return;

    // Titles drawn this run: one the file remembers but that is not here now is
    // a panel the application stopped drawing, while one drawn in another slot
    // has been dragged.
    ct::Vector<String> drawn;
    for (const DockTabState &tab : space.tabs)
        if (dockTitleStorable(tab.title))
            drawn.push_back(tab.title);

    for (const DockSlotName &slotName : kDockSlotNames)
    {
        const uint32_t slotIndex = dockSlotIndex(slotName.slot);
        const String tabsKey = dockKey("tabs_", slotName.key);
        const String selectedKey = dockKey("selected_", slotName.key);

        String tabs;
        String selected;
        for (const DockTabState &tab : space.tabs)
        {
            if (dockSlotIndex(tab.slot) != slotIndex || !dockTitleStorable(tab.title))
                continue;
            dockAppendTitle(tabs, tab.title);
            if (tab.id == space.selected[slotIndex])
                selected = tab.title;
        }

        const String remembered = windowState_.get(section, tabsKey.c_str());
        String::size_type start = 0;
        for (StringView title = dockListNext(remembered, start); !title.empty();
             title = dockListNext(remembered, start))
            if (!dockDrawnHas(drawn, title) && !dockListHas(tabs, title))
                dockAppendTitle(tabs, title);

        if (selected.empty())
        {
            // Nothing is docked there right now: whichever of the remembered
            // tabs is still in the list stays on top.
            const String rememberedSelected = windowState_.get(section, selectedKey.c_str());
            if (dockListHas(tabs, rememberedSelected))
                selected = rememberedSelected;
        }

        windowState_.put(section, tabsKey.c_str(), tabs);
        windowState_.put(section, selectedKey.c_str(), selected);
    }
}

Rect Context::windowBounds(StringView title) const
{
    const WindowHandle *handle = windowsById_.find(hashText(title));
    if (!handle)
        return Rect();
    const WindowState *window = windows_.get(*handle);
    return window ? window->bounds : Rect();
}

Rect Context::contentRect(const Rect &local) const
{
    if (!currentWindow())
        return Rect();
    return Rect(layout_.origin.x + local.x, layout_.origin.y + local.y,
                local.width, local.height);
}

Vec2 Context::toContentSpace(const Vec2 &absolute) const
{
    if (!currentWindow())
        return absolute;
    return Vec2(absolute.x - layout_.origin.x, absolute.y - layout_.origin.y);
}

Rect Context::contentClip() const
{
    const WindowState *window = currentWindow();
    if (!window)
        return Rect();
    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const float titleHeight = window->showTitleBar ? theme_.titleBarHeight : 0.0f;
    const float padding = window->useClientArea ? 0.0f : theme_.windowPadding;
    const float contentWidth = window->bounds.width - padding * 2.0f;
    const float contentHeight = window->bounds.height - titleHeight - padding * 2.0f;
    const Rect content(window->bounds.x + padding, window->bounds.y + titleHeight + padding,
                       contentWidth > 0.0f ? contentWidth : 0.0f,
                       contentHeight > 0.0f ? contentHeight : 0.0f);
    const Rect windowClip = intersect(content, viewport);
    Rect clip = windowClip;
    if (activeDockSpace_ != InvalidWidgetId)
    {
        const DockSpaceState *dockSpace = dockSpaces_.find(activeDockSpace_);
        if (dockSpace)
            clip = dockSpace->clip;
    }
    if (!childStack_.empty())
        clip = intersect(clip, childStack_.back().content);
    if (!dockPanelStack_.empty())
        clip = intersect(clip, dockPanelStack_.back().content);
    return clip;
}

bool Context::itemHovered(const Rect &rect, const Rect &clip, WidgetId id)
{
    lastItemId_ = id;
    if (activeWidget_ != InvalidWidgetId && activeWidget_ != id)
        return false;
    const bool hovered = pointerOver(intersect(rect, clip));
    if (hovered)
        hotWidget_ = id;
    return hovered;
}

bool Context::itemClicked(const Rect &rect, const Rect &clip, WidgetId id, bool focusable)
{
    if (focusable)
        registerFocusable(id);
    if (activeWidget_ != InvalidWidgetId && activeWidget_ != id)
        return false;
    if (inputBlockedByModal())
        return false;
    if (windowBlockedByModal())
        return false;
    const uint32_t left = buttonIndex(PointerButton::Left);
    const Rect visible = intersect(rect, clip);
    const bool pressedHere = pointerPressedIn(visible, left);
    if (pressedHere)
    {
        activeWidget_ = id;
        focusedWidget_ = id;
    }

    const bool releasedHere = pointer_.released[left] &&
                              contains(visible, pointer_.releasedPosition[left]);
    const bool clicked = releasedHere && activeWidget_ == id;
    if (pointer_.released[left] && activeWidget_ == id)
        activeWidget_ = InvalidWidgetId;
    return clicked || (focusedWidget_ == id && enterPressed_);
}

void Context::registerFocusable(WidgetId id)
{
    // Behind a modal a widget is out of the Tab order, so Tab cannot walk
    // focus (and then typing) into the blocked background.
    if (id == InvalidWidgetId || inputBlockedByModal())
        return;
    for (ct::Vector<WidgetId>::size_type i = 0u; i < focusOrder_.size(); ++i)
    {
        if (focusOrder_[i] == id)
            return;
    }
    focusOrder_.push_back(id);
}

void Context::advanceFocus()
{
    // A widget that used Tab itself this frame (codeEditor indenting or
    // accepting an autocomplete suggestion, say) sets tabConsumedByWidget_
    // so Tab does not ALSO silently move focus away from it here - without
    // this, the widget would edit correctly but the very next keystroke
    // would go to some other widget instead. inputText/inputTextMultiline
    // never set this, so Tab still moves between ordinary text fields.
    if (!tabPressed_ || focusOrder_.empty() || tabConsumedByWidget_)
        return;

    ct::Vector<WidgetId>::size_type selected = 0u;
    bool found = false;
    for (ct::Vector<WidgetId>::size_type i = 0u; i < focusOrder_.size(); ++i)
    {
        if (focusOrder_[i] == focusedWidget_)
        {
            selected = i;
            found = true;
            break;
        }
    }
    if (tabShiftPressed_)
    {
        if (found)
            selected = selected == 0u ? focusOrder_.size() - 1u : selected - 1u;
        else
            selected = focusOrder_.size() - 1u;
    }
    else if (found)
    {
        selected = selected + 1u == focusOrder_.size() ? 0u : selected + 1u;
    }
    focusedWidget_ = focusOrder_[selected];
    textInputWidget_ = InvalidWidgetId;
}

bool Context::sliderValue(const Rect &rect, const Rect &clip, WidgetId id,
                          float &value, float minimum, float maximum)
{
    if (rect.width <= 0.0f || maximum <= minimum)
        return false;
    registerFocusable(id);
    if (activeWidget_ != InvalidWidgetId && activeWidget_ != id)
        return false;

    const uint32_t left = buttonIndex(PointerButton::Left);
    const bool pressedHere = pointerPressedIn(intersect(rect, clip), left);
    if (pressedHere)
    {
        activeWidget_ = id;
        focusedWidget_ = id;
    }

    bool changed = false;
    if (activeWidget_ == id && (pointer_.down[left] || pointer_.pressed[left]))
    {
        const float normalized = clamp((pointer_.position.x - rect.x) / rect.width, 0.0f, 1.0f);
        const float nextValue = minimum + (maximum - minimum) * normalized;
        changed = nextValue != value;
        value = nextValue;
    }
    if (pointer_.released[left] && activeWidget_ == id)
        activeWidget_ = InvalidWidgetId;
    return changed;
}

// Where the cap of a fader sits along its travel, at a normalized value: the
// bottom for a vertical fader, the left for a horizontal one.
static float faderCapCentre(const Rect &rect, float capAlong, float travel, float norm, bool vertical)
{
    return vertical ? rect.y + rect.height - capAlong * 0.5f - travel * norm
                    : rect.x + capAlong * 0.5f + travel * norm;
}

// The lit top and the shaded bottom of a moulded cap. Cheaper than a gradient
// and enough to read as something with a thickness you can push.
static Color shade(const Color &color, float amount)
{
    float r = static_cast<float>(color.r) * amount;
    float g = static_cast<float>(color.g) * amount;
    float b = static_cast<float>(color.b) * amount;
    r = r > 255.0f ? 255.0f : r;
    g = g > 255.0f ? 255.0f : g;
    b = b > 255.0f ? 255.0f : b;
    return Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), color.a);
}

FaderStyle FaderStyle::accent(const Color &color)
{
    FaderStyle style;
    style.fill = color;
    style.hasFill = true;
    return style;
}

bool Context::fader(StringView idText, float &value, float minimum, float maximum,
                    const Rect &bounds, FaderOrientation orientation, const FaderStyle &style)
{
    WindowState *window = currentWindow();
    if (!window || maximum <= minimum)
        return false;

    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const WidgetId id = combineIds(makeWidgetId(idText), 0x4641444552000001ull);
    registerFocusable(id);

    const bool vertical = orientation == FaderOrientation::Vertical;
    const float span = maximum - minimum;
    float norm = clamp((value - minimum) / span, 0.0f, 1.0f);

    // The groove runs the length of the control. The cap is the thicker piece
    // in the middle that a finger pushes, and travels the groove minus its own
    // thickness so it never hangs off either end.
    const float grooveThickness = clamp((vertical ? rect.width : rect.height) * 0.24f, 6.0f, 12.0f);
    const float capAlong = 14.0f;
    const float capAcross = clamp(vertical ? rect.width : rect.height, capAlong + 6.0f, 30.0f);
    float travel = (vertical ? rect.height : rect.width) - capAlong;
    if (travel < 1.0f)
        travel = 1.0f;

    // The press is taken over the whole control, which is what makes it usable
    // with a finger - but a press that lands on the cap keeps the gap it landed
    // with, so the cap does not jump under the hand.
    const uint32_t left = buttonIndex(PointerButton::Left);
    const Vec2 pointer = pointer_.position;
    float capCentre = faderCapCentre(rect, capAlong, travel, norm, vertical);
    if (pointerPressedIn(rect, left))
    {
        activeWidget_ = id;
        focusedWidget_ = id;
        const Rect cap = vertical
            ? Rect(rect.x + (rect.width - capAcross) * 0.5f, capCentre - capAlong * 0.5f,
                   capAcross, capAlong)
            : Rect(capCentre - capAlong * 0.5f, rect.y + (rect.height - capAcross) * 0.5f,
                   capAlong, capAcross);
        faderGrabOffset_ = cap.contains(pointer.x, pointer.y)
            ? capCentre - (vertical ? pointer.y : pointer.x)
            : 0.0f;
    }

    bool changed = false;
    if (activeWidget_ == id && (pointer_.down[left] || pointer_.pressed[left]))
    {
        const float along = (vertical ? pointer.y : pointer.x) + faderGrabOffset_;
        const float from = vertical
            ? (rect.y + rect.height - capAlong * 0.5f - along) / travel
            : (along - rect.x - capAlong * 0.5f) / travel;
        norm = clamp(from, 0.0f, 1.0f);
        const float nextValue = minimum + span * norm;
        changed = nextValue != value;
        value = nextValue;
        capCentre = faderCapCentre(rect, capAlong, travel, norm, vertical);
    }
    if (pointer_.released[left] && activeWidget_ == id)
        activeWidget_ = InvalidWidgetId;

    const bool hovered = itemHovered(rect, clip, id);
    const Color accent = style.hasFill ? style.fill : theme_.focusColor;
    const Color grooveColour = style.hasGroove ? style.groove : theme_.sliderBackground;
    const Color capColour = style.hasCap ? style.cap
                                         : (hovered ? theme_.buttonHovered : theme_.buttonBackground);
    const Color capLine = style.hasCapLine ? style.capLine : accent;

    const Rect groove = vertical
        ? Rect(rect.x + (rect.width - grooveThickness) * 0.5f, rect.y, grooveThickness, rect.height)
        : Rect(rect.x, rect.y + (rect.height - grooveThickness) * 0.5f, rect.width, grooveThickness);
    const float grooveRadius = grooveThickness * 0.5f;
    drawList->addRectFilledRounded(groove, grooveRadius, grooveColour, clip);

    // The value reads as the filled part of the groove, in the accent colour.
    if (vertical)
        drawList->addRectFilledRounded(
            Rect(groove.x, capCentre, groove.width, rect.y + rect.height - capCentre),
            grooveRadius, accent, clip);
    else
        drawList->addRectFilledRounded(
            Rect(groove.x, groove.y, capCentre - groove.x, groove.height),
            grooveRadius, accent, clip);

    // Marks down the side of the groove, wherever the control is wide enough to
    // hold them without reaching into a neighbour.
    const float margin = ((vertical ? rect.width : rect.height) - grooveThickness) * 0.5f;
    if (style.ticks && margin >= 8.0f && travel >= 48.0f)
    {
        const float tickLength = margin - 4.0f < 8.0f ? margin - 4.0f : 8.0f;
        const float tickStart = vertical ? groove.right() + 3.0f : groove.bottom() + 3.0f;
        for (int i = 0; i <= 10; ++i)
        {
            const float along = faderCapCentre(rect, capAlong, travel, static_cast<float>(i) / 10.0f,
                                               vertical);
            const float length = (i % 5 == 0) ? tickLength : tickLength * 0.6f;
            if (vertical)
                drawList->addLine(Vec2(tickStart, along), Vec2(tickStart + length, along),
                                  theme_.borderColor, clip, 1.0f);
            else
                drawList->addLine(Vec2(along, tickStart), Vec2(along, tickStart + length),
                                  theme_.borderColor, clip, 1.0f);
        }
    }

    const Rect cap = vertical
        ? Rect(rect.x + (rect.width - capAcross) * 0.5f, capCentre - capAlong * 0.5f,
               capAcross, capAlong)
        : Rect(capCentre - capAlong * 0.5f, rect.y + (rect.height - capAcross) * 0.5f,
               capAlong, capAcross);
    drawList->addRectFilledRounded(cap, 3.0f, capColour, clip);
    drawList->addRectFilled(vertical ? Rect(cap.x + 2.0f, cap.y, cap.width - 4.0f, 1.0f)
                                     : Rect(cap.x, cap.y + 2.0f, 1.0f, cap.height - 4.0f),
                            shade(capColour, 1.6f), clip);
    drawList->addRectFilled(vertical ? Rect(cap.x + 2.0f, cap.bottom() - 1.0f, cap.width - 4.0f, 1.0f)
                                     : Rect(cap.right() - 1.0f, cap.y + 2.0f, 1.0f, cap.height - 4.0f),
                            shade(capColour, 0.55f), clip);
    // The line across the cap is what the eye lines up with the marks.
    drawList->addRectFilled(vertical ? Rect(cap.x + 3.0f, capCentre - 1.0f, cap.width - 6.0f, 2.0f)
                                     : Rect(capCentre - 1.0f, cap.y + 3.0f, 2.0f, cap.height - 6.0f),
                            capLine, clip);
    return changed;
}

void Context::drawWindow(WindowState &window)
{
    if (window.maximized) window.bounds=Rect(0,0,frame_.displaySize.x,frame_.displaySize.y);
    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const float titleHeight = window.showTitleBar ? theme_.titleBarHeight : 0.0f;
    const float controlWidth = theme_.titleBarHeight;
    const bool showMinimize = window.showWindowControls && window.showMinimizeButton;
    const bool showMaximize = window.showWindowControls && window.showMaximizeButton;
    const float controlsCount = 1.0f + (showMinimize ? 1.0f : 0.0f) + (showMaximize ? 1.0f : 0.0f);
    const float controlsWidth = window.showWindowControls ? controlWidth * controlsCount : 0.0f;
    const Rect closeButton(window.bounds.x + window.bounds.width - controlWidth,
                           window.bounds.y, controlWidth, titleHeight);
    float controlsX = closeButton.x;
    Rect minimizeButton;
    Rect maximizeButton;
    if (showMinimize)
    {
        controlsX -= controlWidth;
        minimizeButton = Rect(controlsX, window.bounds.y, controlWidth, titleHeight);
    }
    if (showMaximize)
    {
        controlsX -= controlWidth;
        maximizeButton = Rect(controlsX, window.bounds.y, controlWidth, titleHeight);
    }
    const WidgetId maximizeId=combineIds(window.id,0x4d415849);
    if (showMaximize && itemClicked(maximizeButton,viewport,maximizeId,false)) {
        if(window.maximized) restoreWindow(window.title); else maximizeWindow(window.title);
    }
    const Rect dragArea(window.bounds.x, window.bounds.y,
                        window.bounds.width > controlsWidth ? window.bounds.width - controlsWidth : 0.0f,
                        titleHeight);
    const float resizeGripSize = 12.0f;
    const Rect resizeGrip(window.bounds.x + window.bounds.width - resizeGripSize,
                          window.bounds.y + window.bounds.height - resizeGripSize,
                          resizeGripSize, resizeGripSize);
    const WidgetId closeId = combineIds(window.id, 0x434c4f5345ull);
    const WidgetId minimizeId = combineIds(window.id, 0x4d494e494d495a45ull);
    const WidgetId dragId = combineIds(window.id, 0x44524147ull);
    const WidgetId resizeId = combineIds(window.id, 0x524553495a45ull);
    const bool closeHovered = window.showWindowControls && itemHovered(closeButton, viewport, closeId);
    const bool minimizeHovered = showMinimize && itemHovered(minimizeButton, viewport, minimizeId);

    if (window.showWindowControls && itemClicked(closeButton, viewport, closeId, false))
    {
        window.open = false;
        window.minimized = false;
        draggingWindow_ = WindowHandle();
        resizingWindow_ = WindowHandle();
        focusedWidget_ = InvalidWidgetId;
        textInputWidget_ = InvalidWidgetId;
        openCombo_ = InvalidWidgetId;
        return;
    }
    if (showMinimize && itemClicked(minimizeButton, viewport, minimizeId, false))
        window.minimized = !window.minimized;

    const uint32_t left = buttonIndex(PointerButton::Left);
    const bool pressedResize = window.allowResize && !window.maximized && !window.minimized && pointerPressedIn(intersect(resizeGrip, viewport), left) &&
                               activeWidget_ == InvalidWidgetId;
    if (pressedResize)
    {
        activeWidget_ = resizeId;
        resizingWindow_ = currentWindow_;
    }
    if (resizingWindow_ == currentWindow_ && activeWidget_ == resizeId)
    {
        if (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left])
        {
            const float minimumWidth = titleHeight * 4.0f;
            const float minimumHeight = titleHeight + theme_.windowPadding * 2.0f + theme_.widgetHeight;
            const float width = pointer_.position.x - window.bounds.x;
            const float height = pointer_.position.y - window.bounds.y;
            window.bounds.width = width > minimumWidth ? width : minimumWidth;
            window.bounds.height = height > minimumHeight ? height : minimumHeight;
        }
        if (pointer_.released[left])
        {
            activeWidget_ = InvalidWidgetId;
            resizingWindow_ = WindowHandle();
        }
    }
    const bool pressedTitle = window.allowMove && !window.maximized && pointerPressedIn(intersect(dragArea, viewport), left) &&
                              activeWidget_ == InvalidWidgetId;
    if (pressedTitle)
    {
        activeWidget_ = dragId;
        draggingWindow_ = currentWindow_;
        windowDragOffset_ = Vec2(pointer_.pressedPosition[left].x - window.bounds.x,
                                 pointer_.pressedPosition[left].y - window.bounds.y);
    }
    if (draggingWindow_ == currentWindow_ && activeWidget_ == dragId)
    {
        if (pointer_.down[left] || pointer_.pressed[left])
        {
            window.bounds.x = pointer_.position.x - windowDragOffset_.x;
            window.bounds.y = pointer_.position.y - windowDragOffset_.y;
            // Backends keep reporting the pointer outside the viewport while
            // a button is held, so a fling could park the title bar off
            // screen, leaving nothing to grab the window by.
            clampWindowToViewport(window);
        }
        if (pointer_.released[left])
        {
            activeWidget_ = InvalidWidgetId;
            draggingWindow_ = WindowHandle();
        }
    }

    const Rect resolvedTitleBar(window.bounds.x, window.bounds.y, window.bounds.width, titleHeight);
    if (!window.minimized)
        window.drawList.addRectFilled(window.bounds, theme_.windowBackground, viewport);
    if (window.showTitleBar)
        window.drawList.addRectFilled(resolvedTitleBar, theme_.titleBarBackground, viewport);
    const Rect resolvedCloseButton(window.bounds.x + window.bounds.width - controlWidth,
                                   window.bounds.y, controlWidth, titleHeight);
    float resolvedControlsX = resolvedCloseButton.x;
    Rect resolvedMinimizeButton;
    Rect resolvedMaximizeButton;
    if (showMinimize)
    {
        resolvedControlsX -= controlWidth;
        resolvedMinimizeButton = Rect(resolvedControlsX, window.bounds.y, controlWidth, titleHeight);
    }
    if (showMaximize)
    {
        resolvedControlsX -= controlWidth;
        resolvedMaximizeButton = Rect(resolvedControlsX, window.bounds.y, controlWidth, titleHeight);
    }
    if (showMinimize && minimizeHovered)
        window.drawList.addRectFilled(resolvedMinimizeButton, theme_.buttonHovered, viewport);
    if (window.showWindowControls && closeHovered)
        window.drawList.addRectFilled(resolvedCloseButton, theme_.buttonHovered, viewport);
    if (window.showTitleBar)
    {
        const TextMetrics metrics = measureText(theme_.font, window.title, theme_.fontSize);
        const Vec2 titlePosition(window.bounds.x + theme_.windowPadding,
                                 window.bounds.y + (titleHeight - metrics.height) * 0.5f);
        drawText(window.drawList, theme_.font, window.title, titlePosition, theme_.fontSize,
                 theme_.labelText, viewport);
    }
    if (window.showWindowControls)
    {
        if (showMaximize)
        {
            const Rect maximizeGlyph(resolvedMaximizeButton.x + 8.0f, resolvedMaximizeButton.y + 8.0f,
                                     controlWidth - 16, titleHeight - 16);
            window.drawList.addRect(maximizeGlyph,theme_.labelText,viewport);
            if(window.maximized) window.drawList.addRect(Rect(maximizeGlyph.x+3,maximizeGlyph.y-3,maximizeGlyph.width,maximizeGlyph.height),theme_.labelText,viewport);
        }
        const TextMetrics closeMetrics = measureText(theme_.font, StringView("x"), theme_.fontSize);
        if (showMinimize)
        {
            const TextMetrics minimizeMetrics = measureText(theme_.font, StringView("-"), theme_.fontSize);
            drawText(window.drawList, theme_.font, StringView("-"),
                     Vec2(resolvedMinimizeButton.x + (controlWidth - minimizeMetrics.width) * 0.5f,
                          resolvedMinimizeButton.y + (titleHeight - minimizeMetrics.height) * 0.5f),
                     theme_.fontSize, theme_.labelText, viewport);
        }
        drawText(window.drawList, theme_.font, StringView("x"),
                 Vec2(resolvedCloseButton.x + (controlWidth - closeMetrics.width) * 0.5f,
                      resolvedCloseButton.y + (titleHeight - closeMetrics.height) * 0.5f),
                 theme_.fontSize, theme_.labelText, viewport);
    }
}

WindowHandle Context::topWindowAt(const Vec2 &position) const
{
    WindowHandle result;
    uint64_t highestZ = 0;
    for (ct::Vector<WindowHandle>::size_type i = 0; i < windowOrder_.size(); ++i)
    {
        const WindowState *window = windows_.get(windowOrder_[i]);
        const Rect hitBounds(window ? window->bounds.x : 0.0f, window ? window->bounds.y : 0.0f,
                             window ? window->bounds.width : 0.0f,
                             window && window->minimized ? theme_.titleBarHeight
                                                          : (window ? window->bounds.height : 0.0f));
        if (window && window->open && windowSubmittedRecently(*window) &&
            contains(hitBounds, position) && window->zOrder >= highestZ)
        {
            highestZ = window->zOrder;
            result = windowOrder_[i];
        }
    }
    return result;
}

void Context::focusWindow(WindowHandle handle)
{
    if (!handle)
        return;
    for (ct::SlotMap<WindowState>::iterator it = windows_.begin(); it != windows_.end(); ++it)
        it->focused = false;
    WindowState *window = windows_.get(handle);
    if (!window)
        return;
    window->focused = true;
    window->zOrder = nextZOrder_++;
    focusedWindow_ = handle;
}

bool Context::currentWindowReceivesPointer() const
{
    if (activeModal_ != InvalidWidgetId || windowBlockedByModal())
        return false;
    return currentWindow_ && currentWindow_ == topWindowAt(pointer_.position);
}

bool Context::windowBlockedByModal() const
{
    if (modalWindowId_ == InvalidWidgetId)
        return false;
    const WindowState *window = currentWindow();
    return !window || window->id != modalWindowId_;
}

// A menu bar dropdown/context menu is drawn into its window's overlay list
// (endMenu), not a window of its own - topWindowAt() never sees it, so
// currentWindowReceivesPointer() alone lets whatever sits underneath the
// popup keep hovering/clicking through it (a symbol tree row, a tab, ...).
// While a menu is open, any point outside its popup rect (and any open
// submenu's) is blocked here; itemHovered/itemClicked call this so every
// widget gets the guard for free. The menu's own items are unaffected:
// their rects live inside menuPopupBounds_/subMenuPopupBounds_ by
// construction, so they never hit this early return - this used to also
// short-circuit on activeMenu_ != InvalidWidgetId ("submitting the open
// menu's own items right now"), but activeMenu_ is only set once
// beginContextMenu/beginMenu itself has run this frame; a widget submitted
// BEFORE that call in the same frame (e.g. Editor.cpp's codeEditor(), drawn
// ahead of its own beginContextMenu) saw activeMenu_ still
// InvalidWidgetId and slipped through unblocked even while sitting right
// under the popup, stealing activeWidget_ out from under the menu item the
// pointer was actually over. The bounds check below already covers the
// menu's own items on its own, so the shortcut added nothing but this gap.
//
// Uses the frame-start snapshot (menuWasOpenAtFrameStart_/
// menuPopupBoundsAtFrameStart_/subMenuPopupBoundsAtFrameStart_), not the
// live openMenu_/menuPopupBounds_: beginMenu's "clicked outside the menu"
// branch clears openMenu_ the instant that click is processed, which runs
// well before later widgets (a symbol tree, further down the same frame)
// get to see the same click via itemClicked - reading openMenu_ live would
// have already forgotten the menu was open when that exact click landed,
// so the click would fall through to whatever is underneath it instead of
// being consumed by closing the menu.
//
// An open comboBox list is a popup of the same kind and gets the same rule:
// only its header and its list stay live; a press anywhere else closes it
// (comboBox) and is consumed by that.
bool Context::pointerBlockedByOpenPopup(const Vec2 &point) const
{
    if (comboWasOpenAtFrameStart_ &&
        !contains(comboBoundsAtFrameStart_, point) &&
        !contains(comboPopupBoundsAtFrameStart_, point))
        return true;
    if (!menuWasOpenAtFrameStart_)
        return false;
    if (contains(menuPopupBoundsAtFrameStart_, point))
        return false;
    if (contains(subMenuPopupBoundsAtFrameStart_, point))
        return false;
    // The menu bar strip itself stays live while a dropdown is open - that's
    // what lets hovering "Edit" switch the open menu away from "File"
    // (beginMenu's own hover-to-switch branch) and a click on another
    // top-level entry work in the same frame it closes this one.
    if (contains(menuBarBounds_, point))
        return false;
    return true;
}

bool Context::inputBlockedByModal() const
{
    return activeModal_ != InvalidWidgetId || windowBlockedByModal();
}

bool Context::pointerPressedIn(const Rect &visible, uint32_t button) const
{
    if (!pointer_.pressed[button] || inputBlockedByModal())
        return false;
    const Vec2 &position = pointer_.pressedPosition[button];
    // beginFrame focuses the window under a left press, so for the left
    // button "focused" means "on top at the press". Right/middle presses do
    // not change focus, so ask for the top window at the press directly.
    const bool ownWindow = button == buttonIndex(PointerButton::Left)
        ? currentWindow_ == focusedWindow_
        : currentWindow_ && currentWindow_ == topWindowAt(position);
    return ownWindow && contains(visible, position) && !pointerBlockedByOpenPopup(position);
}

bool Context::pointerOver(const Rect &visible) const
{
    return currentWindowReceivesPointer() && contains(visible, pointer_.position) &&
           !pointerBlockedByOpenPopup(pointer_.position);
}

bool Context::hasKeyboardFocus(WidgetId id) const
{
    return focusedWidget_ == id && !inputBlockedByModal();
}

uint32_t Context::dockSlotIndex(DockSlot slot)
{
    return static_cast<uint32_t>(slot);
}

Rect Context::dockSlotBounds(const DockSpaceState &dockSpace, DockSlot slot) const
{
    const Rect &outer = dockSpace.bounds;
    const float topHeight = outer.height - dockSpace.bottomHeight;
    const float centerWidth = outer.width - dockSpace.leftWidth - dockSpace.rightWidth;
    switch (slot)
    {
    case DockSlot::Left:
        return Rect(outer.x, outer.y, dockSpace.leftWidth, topHeight);
    case DockSlot::Right:
        return Rect(outer.x + outer.width - dockSpace.rightWidth, outer.y,
                    dockSpace.rightWidth, topHeight);
    case DockSlot::Bottom:
        return Rect(outer.x, outer.y + topHeight, outer.width, dockSpace.bottomHeight);
    case DockSlot::Center:
    default:
        return Rect(outer.x + dockSpace.leftWidth, outer.y, centerWidth, topHeight);
    }
}

uint32_t Context::buttonIndex(PointerButton button)
{
    const uint32_t index = static_cast<uint32_t>(button);
    return index < 3u ? index : 0u;
}

} // namespace ig
