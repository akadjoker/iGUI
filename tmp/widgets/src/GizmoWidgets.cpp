#include "pch.hpp"
#include "GizmoWidgets.hpp"
#include <cmath>
#include <cstring>

using BuGUI::clamp;
// Screen-space 2D alias — avoids Widget::Vec2f ambiguity inside member methods
using Pt2 = BuGUI::Vec2f;

static constexpr float kPi = 3.14159265f;

// ─────────────────────────────────────────────────────────────────────────────
//  Shared 2D drawing helpers (static file-scope)
// ─────────────────────────────────────────────────────────────────────────────

static Mat4f inv4x4(const Mat4f& m)
{
    const float* d = m.data;
    float t[16];
    t[0]  =  d[5]*d[10]*d[15] - d[5]*d[11]*d[14] - d[9]*d[6]*d[15] + d[9]*d[7]*d[14]  + d[13]*d[6]*d[11] - d[13]*d[7]*d[10];
    t[4]  = -d[4]*d[10]*d[15] + d[4]*d[11]*d[14] + d[8]*d[6]*d[15] - d[8]*d[7]*d[14]  - d[12]*d[6]*d[11] + d[12]*d[7]*d[10];
    t[8]  =  d[4]*d[9]*d[15]  - d[4]*d[11]*d[13] - d[8]*d[5]*d[15] + d[8]*d[7]*d[13]  + d[12]*d[5]*d[11] - d[12]*d[7]*d[9];
    t[12] = -d[4]*d[9]*d[14]  + d[4]*d[10]*d[13] + d[8]*d[5]*d[14] - d[8]*d[6]*d[13]  - d[12]*d[5]*d[10] + d[12]*d[6]*d[9];
    t[1]  = -d[1]*d[10]*d[15] + d[1]*d[11]*d[14] + d[9]*d[2]*d[15] - d[9]*d[3]*d[14]  - d[13]*d[2]*d[11] + d[13]*d[3]*d[10];
    t[5]  =  d[0]*d[10]*d[15] - d[0]*d[11]*d[14] - d[8]*d[2]*d[15] + d[8]*d[3]*d[14]  + d[12]*d[2]*d[11] - d[12]*d[3]*d[10];
    t[9]  = -d[0]*d[9]*d[15]  + d[0]*d[11]*d[13] + d[8]*d[1]*d[15] - d[8]*d[3]*d[13]  - d[12]*d[1]*d[11] + d[12]*d[3]*d[9];
    t[13] =  d[0]*d[9]*d[14]  - d[0]*d[10]*d[13] - d[8]*d[1]*d[14] + d[8]*d[2]*d[13]  + d[12]*d[1]*d[10] - d[12]*d[2]*d[9];
    t[2]  =  d[1]*d[6]*d[15]  - d[1]*d[7]*d[14]  - d[5]*d[2]*d[15] + d[5]*d[3]*d[14]  + d[13]*d[2]*d[7]  - d[13]*d[3]*d[6];
    t[6]  = -d[0]*d[6]*d[15]  + d[0]*d[7]*d[14]  + d[4]*d[2]*d[15] - d[4]*d[3]*d[14]  - d[12]*d[2]*d[7]  + d[12]*d[3]*d[6];
    t[10] =  d[0]*d[5]*d[15]  - d[0]*d[7]*d[13]  - d[4]*d[1]*d[15] + d[4]*d[3]*d[13]  + d[12]*d[1]*d[7]  - d[12]*d[3]*d[5];
    t[14] = -d[0]*d[5]*d[14]  + d[0]*d[6]*d[13]  + d[4]*d[1]*d[14] - d[4]*d[2]*d[13]  - d[12]*d[1]*d[6]  + d[12]*d[2]*d[5];
    t[3]  = -d[1]*d[6]*d[11]  + d[1]*d[7]*d[10]  + d[5]*d[2]*d[11] - d[5]*d[3]*d[10]  - d[9]*d[2]*d[7]   + d[9]*d[3]*d[6];
    t[7]  =  d[0]*d[6]*d[11]  - d[0]*d[7]*d[10]  - d[4]*d[2]*d[11] + d[4]*d[3]*d[10]  + d[8]*d[2]*d[7]   - d[8]*d[3]*d[6];
    t[11] = -d[0]*d[5]*d[11]  + d[0]*d[7]*d[9]   + d[4]*d[1]*d[11] - d[4]*d[3]*d[9]   - d[8]*d[1]*d[7]   + d[8]*d[3]*d[5];
    t[15] =  d[0]*d[5]*d[10]  - d[0]*d[6]*d[9]   - d[4]*d[1]*d[10] + d[4]*d[2]*d[9]   + d[8]*d[1]*d[6]   - d[8]*d[2]*d[5];
    float det = d[0]*t[0] + d[1]*t[4] + d[2]*t[8] + d[3]*t[12];
    if (std::fabs(det) < 1e-12f) return Mat4f{};
    float inv = 1.0f / det;
    Mat4f out;
    for (int i = 0; i < 16; ++i) out.data[i] = t[i] * inv;
    return out;
}

static void thickLine(PaintContext& ctx, float x0, float y0, float x1, float y1,
                      float thickness, const Color& c)
{
    float dx = x1 - x0, dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.5f) return;
    float nx = -dy / len * thickness * 0.5f;
    float ny =  dx / len * thickness * 0.5f;

    ctx.fill.SetColor(c.r, c.g, c.b, c.a);
    ctx.fillTriangle(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 - nx, y1 - ny);
    ctx.fillTriangle(x0 + nx, y0 + ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Gizmo2D
// ═════════════════════════════════════════════════════════════════════════════

Gizmo2D::Gizmo2D()
{
    setSize(0, 0);
}

void Gizmo2D::layout()
{
    if (parent_) {
        rect_.x = 0;
        rect_.y = 0;
        rect_.w = parent_->rect().w;
        rect_.h = parent_->rect().h;
    }
}

void Gizmo2D::toGizmoSpace(float mx, float my, float& gx, float& gy) const
{
    Rect pAbs = parent_ ? parent_->absoluteRect() : Rect{0,0,0,0};
    float lx = mx - pAbs.x - posX_;
    float ly = my - pAbs.y - posY_;

    if (rotation_ != 0.0f) {
        float rad = -rotation_ * kPi / 180.0f;
        float cs = std::cos(rad), sn = std::sin(rad);
        gx = lx * cs - ly * sn;
        gy = lx * sn + ly * cs;
    } else {
        gx = lx; gy = ly;
    }
}

GizmoAxis Gizmo2D::hitTest(float mx, float my) const
{
    float gx, gy;
    toGizmoSpace(mx, my, gx, gy);

    float hs = handleSize_;
    float al = axisLen_;

    if (mode_ == GizmoMode::Translate || mode_ == GizmoMode::Scale) {
        float cs = hs * 1.5f;
        if (gx >= -cs && gx <= cs && gy >= -cs && gy <= cs)
            return GizmoAxis::XY;
        if (gx > cs && gx <= al + hs && std::fabs(gy) <= hs)
            return GizmoAxis::X;
        if (gy < -cs && gy >= -(al + hs) && std::fabs(gx) <= hs)
            return GizmoAxis::Y;
    }
    else if (mode_ == GizmoMode::Rotate) {
        float dist = std::sqrt(gx * gx + gy * gy);
        float ringR = axisLen_ * 0.8f;
        if (std::fabs(dist - ringR) <= hs)
            return GizmoAxis::XY;
    }

    return GizmoAxis::None;
}

float Gizmo2D::snap(float val, float grid) const
{
    if (grid <= 0) return val;
    return std::round(val / grid) * grid;
}

void Gizmo2D::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;
    GizmoAxis hit = hitTest(e.x, e.y);
    if (hit == GizmoAxis::None) return;

    dragging_        = true;
    activeAxis_      = hit;
    dragStartX_      = e.x;
    dragStartY_      = e.y;
    dragStartAngle_  = rotation_;
    dragStartScaleX_ = scaleX_;
    dragStartScaleY_ = scaleY_;
    e.consumed       = true;
    onDragStart.emit();
}

void Gizmo2D::onMouseRelease(MouseEvent& e)
{
    if (!dragging_) return;
    dragging_   = false;
    activeAxis_ = GizmoAxis::None;
    e.consumed  = true;
    onDragEnd.emit();
}

void Gizmo2D::onMouseMove(MouseEvent& e)
{
    if (!dragging_) {
        hoverAxis_ = hitTest(e.x, e.y);
        return;
    }

    e.consumed = true;
    float dx = e.x - dragStartX_;
    float dy = e.y - dragStartY_;

    if (mode_ == GizmoMode::Translate) {
        float tx = 0, ty = 0;
        if (activeAxis_ == GizmoAxis::X  || activeAxis_ == GizmoAxis::XY) tx = dx;
        if (activeAxis_ == GizmoAxis::Y  || activeAxis_ == GizmoAxis::XY) ty = dy;
        if (snapTranslate_ > 0) { tx = snap(tx, snapTranslate_); ty = snap(ty, snapTranslate_); }
        posX_ += tx;  posY_ += ty;
        dragStartX_ = e.x;  dragStartY_ = e.y;
        onTranslate.emit(tx, ty);
    }
    else if (mode_ == GizmoMode::Rotate) {
        Rect pAbs = parent_ ? parent_->absoluteRect() : Rect{0,0,0,0};
        float cx = pAbs.x + posX_;
        float cy = pAbs.y + posY_;
        float a1 = std::atan2(dragStartY_ - cy, dragStartX_ - cx);
        float a2 = std::atan2(e.y - cy, e.x - cx);
        float da = (a2 - a1) * 180.0f / kPi;
        if (snapRotate_ > 0) da = snap(da, snapRotate_);
        rotation_ = dragStartAngle_ + da;
        onRotate.emit(rotation_);
    }
    else if (mode_ == GizmoMode::Scale) {
        float fx = 1.0f, fy = 1.0f;
        if (activeAxis_ == GizmoAxis::XY) {
            float uniform = 1.0f + (dx - dy) * 0.01f;
            fx = fy = uniform;
        } else {
            if (activeAxis_ == GizmoAxis::X) fx = 1.0f + dx * 0.01f;
            if (activeAxis_ == GizmoAxis::Y) fy = 1.0f + (-dy) * 0.01f;
        }
        if (snapScale_ > 0) { fx = snap(fx, snapScale_); fy = snap(fy, snapScale_); }
        scaleX_ = dragStartScaleX_ * fx;
        scaleY_ = dragStartScaleY_ * fy;
        onScale.emit(scaleX_, scaleY_);
    }
}

void Gizmo2D::paint(PaintContext& ctx)
{
    if (!visible_) return;

    Rect pAbs = parent_ ? parent_->absoluteRect() : Rect{0,0,0,0};
    float cx = pAbs.x + posX_;
    float cy = pAbs.y + posY_;

    switch (mode_) {
        case GizmoMode::Translate: paintTranslate(ctx, cx, cy); break;
        case GizmoMode::Rotate:    paintRotate(ctx, cx, cy);    break;
        case GizmoMode::Scale:     paintScale(ctx, cx, cy);     break;
    }

    // Info text with shadow
    char buf[64];
    if (mode_ == GizmoMode::Translate)
        std::snprintf(buf, sizeof(buf), "%.0f, %.0f", posX_, posY_);
    else if (mode_ == GizmoMode::Rotate)
        std::snprintf(buf, sizeof(buf), "%.1f\xC2\xB0", rotation_);
    else
        std::snprintf(buf, sizeof(buf), "%.2f, %.2f", scaleX_, scaleY_);

    ctx.font.SetFontSize(11.0f);
    ctx.font.SetBatch(&ctx.text);
    float ty = cy + 16.0f + ctx.font.GetAscender();
    ctx.font.SetColor(Color(0, 0, 0, 180));
    ctx.font.Print(buf, cx + 17.0f, ty);
    ctx.font.SetColor(Color(220, 220, 225, 230));
    ctx.font.Print(buf, cx + 16.0f, ty);
}

void Gizmo2D::paintTranslate(PaintContext& ctx, float cx, float cy)
{
    float al = axisLen_;
    float rad = rotation_ * kPi / 180.0f;
    float cs = std::cos(rad), sn = std::sin(rad);

    // Axis directions
    float xxD = cs, xyD = sn;    // +X in rotated space
    float yxD = -sn, yyD = cs;   // +Y in rotated space (screen up = -)

    float gap = al * 0.1f;
    float xsX = cx + xxD * gap,  xsY = cy + xyD * gap;
    float xeX = cx + xxD * al,   xeY = cy + xyD * al;
    float ysX = cx - yxD * gap,  ysY = cy - yyD * gap;
    float yeX = cx - yxD * al,   yeY = cy - yyD * al;

    bool hX  = (hoverAxis_ == GizmoAxis::X  || activeAxis_ == GizmoAxis::X);
    bool hY  = (hoverAxis_ == GizmoAxis::Y  || activeAxis_ == GizmoAxis::Y);
    bool hXY = (hoverAxis_ == GizmoAxis::XY || activeAxis_ == GizmoAxis::XY);

    float ahLen = 14.0f, ahW = 6.0f;

    // X axis
    Color cX = hX ? kHover : kAxisX;
    thickLine(ctx, xsX, xsY, xeX, xeY, 3.0f, cX);
    ctx.fill.SetColor(cX.r, cX.g, cX.b, cX.a);
    ctx.fillTriangle(xeX + xxD * ahLen,           xeY + xyD * ahLen,
                     xeX + xyD * ahW,             xeY - xxD * ahW,
                     xeX - xyD * ahW,             xeY + xxD * ahW);

    // Y axis (points screen-up, i.e. negative yyD direction)
    Color cY = hY ? kHover : kAxisY;
    thickLine(ctx, ysX, ysY, yeX, yeY, 3.0f, cY);
    ctx.fill.SetColor(cY.r, cY.g, cY.b, cY.a);
    ctx.fillTriangle(yeX - yxD * ahLen,           yeY - yyD * ahLen,
                     yeX + yyD * ahW,             yeY - yxD * ahW,
                     yeX - yyD * ahW,             yeY + yxD * ahW);

    // Center circle
    Color cC = hXY ? kHover : kCenter;
    ctx.fill.SetColor(cC.r, cC.g, cC.b, 180);
    ctx.fillCircle(cx, cy, 6.0f);
    ctx.line.SetColor(cC.r, cC.g, cC.b, cC.a);
    ctx.lineCircle(cx, cy, 6.0f);
}

void Gizmo2D::paintRotate(PaintContext& ctx, float cx, float cy)
{
    float ringR = axisLen_ * 0.8f;
    int segments = 64;
    bool hover = (hoverAxis_ == GizmoAxis::XY || activeAxis_ == GizmoAxis::XY);
    Color cR = hover ? kHover : Color(100, 160, 240, 255);

    for (int i = 0; i < segments; ++i) {
        float a0 = (float(i)   / segments) * 2.0f * kPi;
        float a1 = (float(i+1) / segments) * 2.0f * kPi;
        thickLine(ctx,
                  cx + std::cos(a0) * ringR, cy + std::sin(a0) * ringR,
                  cx + std::cos(a1) * ringR, cy + std::sin(a1) * ringR,
                  2.5f, cR);
    }

    // Pie wedge feedback during drag
    if (dragging_) {
        float startRad = dragStartAngle_ * kPi / 180.0f;
        float curRad   = rotation_ * kPi / 180.0f;
        Color wedge(255, 128, 16, 80);
        int wedgeSegs = 32;
        float dAngle = curRad - startRad;
        for (int i = 0; i < wedgeSegs; ++i) {
            float a0 = startRad + dAngle * (float(i)   / wedgeSegs);
            float a1 = startRad + dAngle * (float(i+1) / wedgeSegs);
            ctx.fill.SetColor(wedge.r, wedge.g, wedge.b, wedge.a);
            ctx.fillTriangle(cx, cy,
                             cx + std::cos(a0) * ringR, cy + std::sin(a0) * ringR,
                             cx + std::cos(a1) * ringR, cy + std::sin(a1) * ringR);
        }
        Color wol(255, 128, 16, 200);
        thickLine(ctx, cx, cy, cx + std::cos(startRad) * ringR, cy + std::sin(startRad) * ringR, 1.5f, wol);
        thickLine(ctx, cx, cy, cx + std::cos(curRad)   * ringR, cy + std::sin(curRad)   * ringR, 1.5f, wol);
    }

    // Angle indicator
    float indRad = rotation_ * kPi / 180.0f;
    Color indic(255, 200, 60, 255);
    thickLine(ctx, cx, cy, cx + std::cos(indRad) * ringR, cy + std::sin(indRad) * ringR, 2.0f, indic);

    ctx.fill.SetColor(255, 200, 60, 220);
    ctx.fillCircle(cx, cy, 5.0f);
}

void Gizmo2D::paintScale(PaintContext& ctx, float cx, float cy)
{
    float al = axisLen_;
    float rad = rotation_ * kPi / 180.0f;
    float cs = std::cos(rad), sn = std::sin(rad);

    float xxD = cs, xyD = sn;
    float yxD = -sn, yyD = cs;

    float gap = al * 0.1f;
    float xsX = cx + xxD * gap, xsY = cy + xyD * gap;
    float xeX = cx + xxD * al,  xeY = cy + xyD * al;
    float ysX = cx - yxD * gap, ysY = cy - yyD * gap;
    float yeX = cx - yxD * al,  yeY = cy - yyD * al;

    bool hX  = (hoverAxis_ == GizmoAxis::X  || activeAxis_ == GizmoAxis::X);
    bool hY  = (hoverAxis_ == GizmoAxis::Y  || activeAxis_ == GizmoAxis::Y);
    bool hXY = (hoverAxis_ == GizmoAxis::XY || activeAxis_ == GizmoAxis::XY);

    Color cX = hX ? kHover : kAxisX;
    thickLine(ctx, xsX, xsY, xeX, xeY, 3.0f, cX);
    ctx.fill.SetColor(cX.r, cX.g, cX.b, cX.a);
    ctx.fillCircle(xeX, xeY, 5.0f);

    Color cY = hY ? kHover : kAxisY;
    thickLine(ctx, ysX, ysY, yeX, yeY, 3.0f, cY);
    ctx.fill.SetColor(cY.r, cY.g, cY.b, cY.a);
    ctx.fillCircle(yeX, yeY, 5.0f);

    Color cC = hXY ? kHover : kCenter;
    ctx.fill.SetColor(cC.r, cC.g, cC.b, 180);
    ctx.fillCircle(cx, cy, 6.0f);
    ctx.line.SetColor(cC.r, cC.g, cC.b, cC.a);
    ctx.lineCircle(cx, cy, 6.0f);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Gizmo3D
// ═════════════════════════════════════════════════════════════════════════════

Gizmo3D::Gizmo3D()
{
    setSize(0, 0);
}

void Gizmo3D::layout()
{
    if (parent_) {
        rect_.x = 0;
        rect_.y = 0;
        rect_.w = parent_->rect().w;
        rect_.h = parent_->rect().h;
    }
}

 

void Gizmo3D::setViewProjection(const float* view4x4, const float* proj4x4,
                                 int vpWidth, int vpHeight)
{
    std::memcpy(view_.data, view4x4, 64);
    std::memcpy(proj_.data, proj4x4, 64);
    vpW_ = vpWidth;
    vpH_ = vpHeight;
}

void Gizmo3D::setTarget(const float* pos3, const float* rotDeg3, const float* scale3)
{
    targetPos_   = Vec3f(pos3[0],    pos3[1],    pos3[2]);
    targetRot_   = Vec3f(rotDeg3[0], rotDeg3[1], rotDeg3[2]);
    targetScale_ = Vec3f(scale3[0],  scale3[1],  scale3[2]);
}

// ── Projection helpers ────────────────────────────────────────────────────────

BuGUI::Vec2f Gizmo3D::project(const Vec3f& world) const
{
    Rect pAbs = parent_ ? parent_->absoluteRect() : Rect{0, 0, (float)vpW_, (float)vpH_};

    // clip = proj * view * vec4(world, 1)
    Vec4f v4 = view_ * Vec4f(world, 1.0f);
    Vec4f clip = proj_ * v4;

    if (std::fabs(clip.w) < 1e-6f) return {-9999.0f, -9999.0f};

    float invW = 1.0f / clip.w;
    float ndcX = clip.x * invW;
    float ndcY = clip.y * invW;

    float sx = pAbs.x + (ndcX * 0.5f + 0.5f) * pAbs.w;
    float sy = pAbs.y + (1.0f - (ndcY * 0.5f + 0.5f)) * pAbs.h;
    return {sx, sy};
}

void Gizmo3D::unproject(float sx, float sy, Vec3f& rayOrig, Vec3f& rayDir) const
{
    Rect pAbs = parent_ ? parent_->absoluteRect() : Rect{0, 0, (float)vpW_, (float)vpH_};

    float ndcX = ((sx - pAbs.x) / pAbs.w) * 2.0f - 1.0f;
    float ndcY = 1.0f - ((sy - pAbs.y) / pAbs.h) * 2.0f;

    Mat4f vp = proj_ * view_;
    Mat4f invVP = inv4x4(vp);

    Vec4f near4 = invVP * Vec4f(ndcX, ndcY, -1.0f, 1.0f);
    Vec4f far4  = invVP * Vec4f(ndcX, ndcY,  1.0f, 1.0f);

    if (std::fabs(near4.w) > 1e-8f) { float iw = 1.0f/near4.w; near4.x *= iw; near4.y *= iw; near4.z *= iw; }
    if (std::fabs(far4.w)  > 1e-8f) { float iw = 1.0f/far4.w;  far4.x  *= iw; far4.y  *= iw; far4.z  *= iw; }

    rayOrig = Vec3f(near4.x, near4.y, near4.z);
    Vec3f dir(far4.x - near4.x, far4.y - near4.y, far4.z - near4.z);
    rayDir = dir.normalized();
}

float Gizmo3D::computeScale() const
{
    // Project target through view matrix to get camera-space Z
    Vec4f viewPos = view_ * Vec4f(targetPos_, 1.0f);
    float distToCam = std::fabs(viewPos.z);
    float s = distToCam * screenScale_ / static_cast<float>(vpH_);
    return std::max(s, 0.01f);
}

float Gizmo3D::snap(float val, float grid) const
{
    if (grid <= 0) return val;
    return std::round(val / grid) * grid;
}

// ── Hit testing ───────────────────────────────────────────────────────────────

GizmoAxis3D Gizmo3D::hitTest(float mx, float my) const
{
    Pt2 center = project(targetPos_);
    float scale  = computeScale();
    float hitR   = 12.0f;

    const Vec3f axes[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
    const GizmoAxis3D axisIds[3] = { GizmoAxis3D::X, GizmoAxis3D::Y, GizmoAxis3D::Z };

    // Center — always tested first (exclusive zone)
    float cdx = mx - center.x, cdy = my - center.y;
    if (std::sqrt(cdx*cdx + cdy*cdy) < 12.0f) return GizmoAxis3D::XYZ;

    // Individual axes (point-to-segment distance)
    float bestDist = 9999.0f;
    GizmoAxis3D bestAxis = GizmoAxis3D::None;

    for (int i = 0; i < 3; ++i) {
        Pt2 s2 = project(targetPos_ + axes[i] * (scale * 0.1f));
        Pt2 e2 = project(targetPos_ + axes[i] * scale);

        float segX = e2.x - s2.x, segY = e2.y - s2.y;
        float segLen2 = segX*segX + segY*segY;
        if (segLen2 < 1.0f) continue;

        float dx = mx - s2.x, dy = my - s2.y;
        float t = (dx*segX + dy*segY) / segLen2;
        t = clamp(t, 0.0f, 1.0f);
        float cx = s2.x + segX*t - mx;
        float cy = s2.y + segY*t - my;
        float lineDist = std::sqrt(cx*cx + cy*cy);

        if (lineDist < hitR && lineDist < bestDist) {
            bestDist = lineDist;
            bestAxis = axisIds[i];
        }
    }
    if (bestAxis != GizmoAxis3D::None) return bestAxis;

    // Plane quadrants (Translate / Scale)
    if (mode_ != GizmoMode3D::Rotate) {
        const GizmoAxis3D planeIds[3]  = { GizmoAxis3D::XY, GizmoAxis3D::XZ, GizmoAxis3D::YZ };
        const int planeAxes[3][2]      = { {0,1}, {0,2}, {1,2} };

        auto inTri = [](float px, float py,
                        float ax, float ay,
                        float bx, float by,
                        float cx2, float cy2) -> bool
        {
            auto sign = [](float x0,float y0,float x1,float y1,float x2,float y2){
                return (x0-x2)*(y1-y2) - (x1-x2)*(y0-y2);
            };
            float d1 = sign(px,py,ax,ay,bx,by);
            float d2 = sign(px,py,bx,by,cx2,cy2);
            float d3 = sign(px,py,cx2,cy2,ax,ay);
            bool neg = (d1<0)||(d2<0)||(d3<0);
            bool pos = (d1>0)||(d2>0)||(d3>0);
            return !(neg && pos);
        };

        for (int p = 0; p < 3; ++p) {
            int a = planeAxes[p][0], b = planeAxes[p][1];
            Pt2 s00 = project(targetPos_ + axes[a]*(scale*0.3f) + axes[b]*(scale*0.3f));
            Pt2 s10 = project(targetPos_ + axes[a]*(scale*0.6f) + axes[b]*(scale*0.3f));
            Pt2 s11 = project(targetPos_ + axes[a]*(scale*0.6f) + axes[b]*(scale*0.6f));
            Pt2 s01 = project(targetPos_ + axes[a]*(scale*0.3f) + axes[b]*(scale*0.6f));

            if (inTri(mx,my,s00.x,s00.y,s10.x,s10.y,s11.x,s11.y) ||
                inTri(mx,my,s00.x,s00.y,s11.x,s11.y,s01.x,s01.y))
                return planeIds[p];
        }
    }

    // Rotation rings
    if (mode_ == GizmoMode3D::Rotate) {
        for (int i = 0; i < 3; ++i) {
            Vec3f normal = axes[i];
            Vec3f u, v;
            if (std::fabs(normal.x) < 0.9f)
                u = normal.cross(Vec3f(1,0,0)).normalized();
            else
                u = normal.cross(Vec3f(0,1,0)).normalized();
            v = normal.cross(u);

            int segs = 36;
            float bestRingDist = 9999.0f;
            for (int s = 0; s < segs; ++s) {
                float a0 = (float(s) / segs) * 2.0f * kPi;
                Vec3f rp = targetPos_ + (u * std::cos(a0) + v * std::sin(a0)) * scale;
                Pt2 sp = project(rp);
                float dx = mx - sp.x, dy = my - sp.y;
                float d = std::sqrt(dx*dx + dy*dy);
                if (d < bestRingDist) bestRingDist = d;
            }
            if (bestRingDist < hitR) return axisIds[i];
        }
    }

    return GizmoAxis3D::None;
}

// ── Mouse interaction ─────────────────────────────────────────────────────────

void Gizmo3D::onMousePress(MouseEvent& e)
{
    if (e.button != 0) return;
    GizmoAxis3D hit = hitTest(e.x, e.y);
    if (hit == GizmoAxis3D::None) return;

    dragging_       = true;
    activeAxis_     = hit;
    dragStartX_     = e.x;
    dragStartY_     = e.y;
    dragStartPos_   = targetPos_;
    dragStartRot_   = targetRot_;
    dragStartScale_ = targetScale_;
    e.consumed      = true;
    onDragStart.emit();
}

void Gizmo3D::onMouseRelease(MouseEvent& e)
{
    if (!dragging_) return;
    dragging_   = false;
    activeAxis_ = GizmoAxis3D::None;
    e.consumed  = true;
    onDragEnd.emit();
}

void Gizmo3D::onMouseMove(MouseEvent& e)
{
    if (!dragging_) {
        hoverAxis_ = hitTest(e.x, e.y);
        return;
    }
    e.consumed = true;

    float dx = e.x - dragStartX_;
    float dy = e.y - dragStartY_;

    Vec4f viewPos = view_ * Vec4f(targetPos_, 1.0f);
    float distToCam = std::fabs(viewPos.z);
    float pixelToWorld = distToCam / static_cast<float>(vpH_);

    if (mode_ == GizmoMode3D::Translate) {
        const Vec3f axes[3] = {{1,0,0},{0,1,0},{0,0,1}};
        Vec3f delta = {};
        Pt2 center = project(targetPos_);

        auto axisMove = [&](const Vec3f& axis) {
            Pt2 axEnd = project(targetPos_ + axis);
            float axX = axEnd.x - center.x, axY = axEnd.y - center.y;
            float axLen = std::sqrt(axX*axX + axY*axY);
            if (axLen < 1.0f) return 0.0f;
            axX /= axLen; axY /= axLen;
            return (dx*axX + dy*axY) * pixelToWorld;
        };

        bool hasX = (activeAxis_==GizmoAxis3D::X||activeAxis_==GizmoAxis3D::XY||
                     activeAxis_==GizmoAxis3D::XZ||activeAxis_==GizmoAxis3D::XYZ);
        bool hasY = (activeAxis_==GizmoAxis3D::Y||activeAxis_==GizmoAxis3D::XY||
                     activeAxis_==GizmoAxis3D::YZ||activeAxis_==GizmoAxis3D::XYZ);
        bool hasZ = (activeAxis_==GizmoAxis3D::Z||activeAxis_==GizmoAxis3D::XZ||
                     activeAxis_==GizmoAxis3D::YZ||activeAxis_==GizmoAxis3D::XYZ);

        if (hasX) delta.x = axisMove(axes[0]);
        if (hasY) delta.y = axisMove(axes[1]);
        if (hasZ) delta.z = axisMove(axes[2]);

        if (snapT_ > 0) {
            delta.x = snap(delta.x, snapT_);
            delta.y = snap(delta.y, snapT_);
            delta.z = snap(delta.z, snapT_);
        }
        targetPos_ = dragStartPos_ + delta;
        onTranslate3D.emit(delta);
    }
    else if (mode_ == GizmoMode3D::Rotate) {
        Pt2 center = project(targetPos_);
        float a1 = std::atan2(dragStartY_ - center.y, dragStartX_ - center.x);
        float a2 = std::atan2(e.y - center.y, e.x - center.x);
        float da = (a2 - a1) * 180.0f / kPi;
        if (snapR_ > 0) da = snap(da, snapR_);

        Vec3f rot = dragStartRot_;
        if (activeAxis_ == GizmoAxis3D::X)      rot.x += da;
        else if (activeAxis_ == GizmoAxis3D::Y) rot.y += da;
        else if (activeAxis_ == GizmoAxis3D::Z) rot.z += da;
        else                                    rot.y += da;

        targetRot_ = rot;
        onRotate3D.emit(rot);
    }
    else if (mode_ == GizmoMode3D::Scale) {
        Vec3f s = dragStartScale_;
        if (activeAxis_ == GizmoAxis3D::XYZ) {
            float uniform = 1.0f + (dx - dy) * 0.005f;
            s = s * uniform;
        } else {
            float factor = 1.0f + dx * 0.005f;
            bool hasX = (activeAxis_==GizmoAxis3D::X||activeAxis_==GizmoAxis3D::XY||activeAxis_==GizmoAxis3D::XZ);
            bool hasY = (activeAxis_==GizmoAxis3D::Y||activeAxis_==GizmoAxis3D::XY||activeAxis_==GizmoAxis3D::YZ);
            bool hasZ = (activeAxis_==GizmoAxis3D::Z||activeAxis_==GizmoAxis3D::XZ||activeAxis_==GizmoAxis3D::YZ);
            if (hasX) s.x *= factor;
            if (hasY) s.y *= (1.0f + (-dy) * 0.005f);
            if (hasZ) s.z *= factor;
        }
        if (snapS_ > 0) { s.x = snap(s.x, snapS_); s.y = snap(s.y, snapS_); s.z = snap(s.z, snapS_); }
        targetScale_ = s;
        onScale3D.emit(s);
    }
}

// ── Painting ──────────────────────────────────────────────────────────────────

void Gizmo3D::paint(PaintContext& ctx)
{
    if (!visible_) return;

    switch (mode_) {
        case GizmoMode3D::Translate: paintTranslate3D(ctx); break;
        case GizmoMode3D::Rotate:    paintRotate3D(ctx);    break;
        case GizmoMode3D::Scale:     paintScale3D(ctx);     break;
    }

    // Info text with shadow
    Pt2 c = project(targetPos_);
    char buf[80];
    if (mode_ == GizmoMode3D::Translate)
        std::snprintf(buf,sizeof(buf),"%.1f, %.1f, %.1f",targetPos_.x,targetPos_.y,targetPos_.z);
    else if (mode_ == GizmoMode3D::Rotate)
        std::snprintf(buf,sizeof(buf),"%.1f\xC2\xB0, %.1f\xC2\xB0, %.1f\xC2\xB0",targetRot_.x,targetRot_.y,targetRot_.z);
    else
        std::snprintf(buf,sizeof(buf),"%.2f, %.2f, %.2f",targetScale_.x,targetScale_.y,targetScale_.z);

    ctx.font.SetFontSize(11.0f);
    ctx.font.SetBatch(&ctx.text);
    float ty = c.y + 16.0f + ctx.font.GetAscender();
    ctx.font.SetColor(Color(0, 0, 0, 180));
    ctx.font.Print(buf, c.x + 17.0f, ty);
    ctx.font.SetColor(Color(220, 220, 225, 230));
    ctx.font.Print(buf, c.x + 16.0f, ty);
}

void Gizmo3D::paintTranslate3D(PaintContext& ctx)
{
    float scale = computeScale();
    Pt2 center = project(targetPos_);

    const Vec3f axes[3]  = { {1,0,0}, {0,1,0}, {0,0,1} };
    const Color colors[3] = { kRed, kGreen, kBlue };
    const GizmoAxis3D axisIds[3] = { GizmoAxis3D::X, GizmoAxis3D::Y, GizmoAxis3D::Z };

    // Plane quadrants (behind axes)
    const GizmoAxis3D planeIds[3] = { GizmoAxis3D::XY, GizmoAxis3D::XZ, GizmoAxis3D::YZ };
    const int planeAxes[3][2]     = { {0,1}, {0,2}, {1,2} };
    const Color planeColors[3]    = { kBlue, kGreen, kRed };

    for (int p = 0; p < 3; ++p) {
        int a = planeAxes[p][0], b = planeAxes[p][1];
        bool hPlane = (hoverAxis_==planeIds[p] || activeAxis_==planeIds[p]);

        Pt2 s00 = project(targetPos_ + axes[a]*(scale*0.3f) + axes[b]*(scale*0.3f));
        Pt2 s10 = project(targetPos_ + axes[a]*(scale*0.6f) + axes[b]*(scale*0.3f));
        Pt2 s11 = project(targetPos_ + axes[a]*(scale*0.6f) + axes[b]*(scale*0.6f));
        Pt2 s01 = project(targetPos_ + axes[a]*(scale*0.3f) + axes[b]*(scale*0.6f));

        Color fc = hPlane ? kHover : planeColors[p];
        uint8_t alpha = hPlane ? 100 : 50;
        ctx.fill.SetColor(fc.r, fc.g, fc.b, alpha);
        ctx.fillTriangle(s00.x,s00.y, s10.x,s10.y, s11.x,s11.y);
        ctx.fillTriangle(s00.x,s00.y, s11.x,s11.y, s01.x,s01.y);

        ctx.line.SetColor(fc.r, fc.g, fc.b, 180);
        ctx.drawLine(s00.x,s00.y, s10.x,s10.y);
        ctx.drawLine(s10.x,s10.y, s11.x,s11.y);
        ctx.drawLine(s11.x,s11.y, s01.x,s01.y);
        ctx.drawLine(s01.x,s01.y, s00.x,s00.y);
    }

    // Axis lines + arrow heads
    for (int i = 0; i < 3; ++i) {
        bool h = (hoverAxis_==axisIds[i] || activeAxis_==axisIds[i]);
        Color c = h ? kHover : colors[i];

        Pt2 s0 = project(targetPos_ + axes[i]*(scale*0.1f));
        Pt2 s1 = project(targetPos_ + axes[i]*scale);

        thickLine(ctx, s0.x,s0.y, s1.x,s1.y, 3.0f, c);

        float dX = s1.x - s0.x, dY = s1.y - s0.y;
        float dLen = std::sqrt(dX*dX + dY*dY);
        if (dLen < 2.0f) continue;
        dX /= dLen; dY /= dLen;

        // Arrow head
        ctx.fill.SetColor(c.r, c.g, c.b, c.a);
        ctx.fillTriangle(s1.x + dX*14.0f, s1.y + dY*14.0f,
                         s1.x + dY*6.0f,  s1.y - dX*6.0f,
                         s1.x - dY*6.0f,  s1.y + dX*6.0f);
    }

    // Center circle
    bool hXYZ = (hoverAxis_==GizmoAxis3D::XYZ || activeAxis_==GizmoAxis3D::XYZ);
    Color cc = hXYZ ? kHover : kYellow;
    ctx.fill.SetColor(cc.r, cc.g, cc.b, 200);
    ctx.fillCircle(center.x, center.y, 6.0f);
}

void Gizmo3D::paintRotate3D(PaintContext& ctx)
{
    float scale = computeScale();
    Pt2 center = project(targetPos_);

    // Camera position from inverse view (col-major: view[3] = {data[12],data[13],data[14]})
    // The camera world position is -R^T * t for a standard view matrix.
    // Simpler: invView col3 = camera world pos.
    // For the view-dependent half-ring, approximate: use the view Z axis.
    // view[8],view[9],view[10] = Z column of view = view forward
    Vec3f viewFwd(-view_.data[8], -view_.data[9], -view_.data[10]);

    const Vec3f axes[3]  = { {1,0,0}, {0,1,0}, {0,0,1} };
    const Color colors[3] = { kRed, kGreen, kBlue };
    const GizmoAxis3D axisIds[3] = { GizmoAxis3D::X, GizmoAxis3D::Y, GizmoAxis3D::Z };

    int segments = 64;

    for (int ri = 0; ri < 3; ++ri) {
        bool h = (hoverAxis_==axisIds[ri] || activeAxis_==axisIds[ri]);
        Color c = h ? kHover : colors[ri];

        Vec3f normal = axes[ri];
        Vec3f u, v;
        if (std::fabs(normal.x) < 0.9f)
            u = normal.cross(Vec3f(1,0,0)).normalized();
        else
            u = normal.cross(Vec3f(0,1,0)).normalized();
        v = normal.cross(u);

        for (int si = 0; si < segments; ++si) {
            float a0 = (float(si)   / segments) * 2.0f * kPi;
            float a1 = (float(si+1) / segments) * 2.0f * kPi;

            Vec3f p0 = targetPos_ + (u*std::cos(a0) + v*std::sin(a0)) * scale;
            Vec3f p1 = targetPos_ + (u*std::cos(a1) + v*std::sin(a1)) * scale;

            Vec3f midDir = ((p0 + p1) * 0.5f - targetPos_).normalized();
            float faceDot = midDir.dot(viewFwd);

            Pt2 sp0 = project(p0), sp1 = project(p1);
            if (faceDot < 0) {
                thickLine(ctx, sp0.x,sp0.y, sp1.x,sp1.y, 2.5f, c);
            } else {
                Color faded(c.r, c.g, c.b, 60);
                ctx.line.SetColor(faded.r, faded.g, faded.b, faded.a);
                ctx.drawLine(sp0.x, sp0.y, sp1.x, sp1.y);
            }
        }
    }

    // Drag pie wedge
    if (dragging_ && activeAxis_ != GizmoAxis3D::None) {
        float startAngle = std::atan2(dragStartY_ - center.y, dragStartX_ - center.x);

        // Approximate ring radius on screen
        float ringR = 0;
        for (int i = 0; i < 3; ++i) {
            Pt2 rp = project(targetPos_ + axes[i] * scale);
            float dx = rp.x - center.x, dy = rp.y - center.y;
            float d = std::sqrt(dx*dx + dy*dy);
            if (d > ringR) ringR = d;
        }

        float deltaAngle = 0.0f;
        if (activeAxis_ == GizmoAxis3D::X)      deltaAngle = (targetRot_.x - dragStartRot_.x) * kPi / 180.0f;
        else if (activeAxis_ == GizmoAxis3D::Y) deltaAngle = (targetRot_.y - dragStartRot_.y) * kPi / 180.0f;
        else if (activeAxis_ == GizmoAxis3D::Z) deltaAngle = (targetRot_.z - dragStartRot_.z) * kPi / 180.0f;

        if (std::fabs(deltaAngle) > 0.001f) {
            int wedgeSegs = 32;
            Color wedge(255, 128, 16, 60);
            for (int i = 0; i < wedgeSegs; ++i) {
                float a0 = startAngle + deltaAngle * (float(i)   / wedgeSegs);
                float a1 = startAngle + deltaAngle * (float(i+1) / wedgeSegs);
                ctx.fill.SetColor(wedge.r, wedge.g, wedge.b, wedge.a);
                ctx.fillTriangle(center.x, center.y,
                                 center.x + std::cos(a0)*ringR, center.y + std::sin(a0)*ringR,
                                 center.x + std::cos(a1)*ringR, center.y + std::sin(a1)*ringR);
            }
            Color wol(255, 128, 16, 180);
            thickLine(ctx, center.x, center.y,
                      center.x + std::cos(startAngle)*ringR,
                      center.y + std::sin(startAngle)*ringR, 1.5f, wol);
            thickLine(ctx, center.x, center.y,
                      center.x + std::cos(startAngle + deltaAngle)*ringR,
                      center.y + std::sin(startAngle + deltaAngle)*ringR, 1.5f, wol);
        }
    }

    // Outer screen-space ring
    float outerR = 0;
    for (int i = 0; i < 3; ++i) {
        Pt2 rp = project(targetPos_ + axes[i] * scale);
        float dx = rp.x - center.x, dy = rp.y - center.y;
        float d = std::sqrt(dx*dx + dy*dy);
        if (d > outerR) outerR = d;
    }
    outerR *= 1.15f;
    bool hXYZ = (hoverAxis_==GizmoAxis3D::XYZ || activeAxis_==GizmoAxis3D::XYZ);
    Color outerC = hXYZ ? kHover : Color(180,180,180,120);
    for (int i = 0; i < segments; ++i) {
        float a0 = (float(i)   / segments) * 2.0f * kPi;
        float a1 = (float(i+1) / segments) * 2.0f * kPi;
        thickLine(ctx,
                  center.x + std::cos(a0)*outerR, center.y + std::sin(a0)*outerR,
                  center.x + std::cos(a1)*outerR, center.y + std::sin(a1)*outerR,
                  1.5f, outerC);
    }

    // Center dot
    Color cc = hXYZ ? kHover : kYellow;
    ctx.fill.SetColor(cc.r, cc.g, cc.b, 200);
    ctx.fillCircle(center.x, center.y, 5.0f);
}

void Gizmo3D::paintScale3D(PaintContext& ctx)
{
    float scale = computeScale();
    Pt2 center = project(targetPos_);

    const Vec3f axes[3]  = { {1,0,0}, {0,1,0}, {0,0,1} };
    const Color colors[3] = { kRed, kGreen, kBlue };
    const GizmoAxis3D axisIds[3] = { GizmoAxis3D::X, GizmoAxis3D::Y, GizmoAxis3D::Z };
    const GizmoAxis3D planeIds[3] = { GizmoAxis3D::XY, GizmoAxis3D::XZ, GizmoAxis3D::YZ };
    const int planeAxes[3][2]     = { {0,1}, {0,2}, {1,2} };
    const Color planeColors[3]    = { kBlue, kGreen, kRed };

    for (int p = 0; p < 3; ++p) {
        int a = planeAxes[p][0], b = planeAxes[p][1];
        bool hPlane = (hoverAxis_==planeIds[p] || activeAxis_==planeIds[p]);

        Pt2 s00 = project(targetPos_ + axes[a]*(scale*0.3f) + axes[b]*(scale*0.3f));
        Pt2 s10 = project(targetPos_ + axes[a]*(scale*0.6f) + axes[b]*(scale*0.3f));
        Pt2 s11 = project(targetPos_ + axes[a]*(scale*0.6f) + axes[b]*(scale*0.6f));
        Pt2 s01 = project(targetPos_ + axes[a]*(scale*0.3f) + axes[b]*(scale*0.6f));

        Color fc = hPlane ? kHover : planeColors[p];
        uint8_t alpha = hPlane ? 100 : 50;
        ctx.fill.SetColor(fc.r, fc.g, fc.b, alpha);
        ctx.fillTriangle(s00.x,s00.y, s10.x,s10.y, s11.x,s11.y);
        ctx.fillTriangle(s00.x,s00.y, s11.x,s11.y, s01.x,s01.y);
    }

    for (int i = 0; i < 3; ++i) {
        bool h = (hoverAxis_==axisIds[i] || activeAxis_==axisIds[i]);
        Color c = h ? kHover : colors[i];

        Pt2 s0 = project(targetPos_ + axes[i]*(scale*0.1f));
        Pt2 s1 = project(targetPos_ + axes[i]*scale);

        thickLine(ctx, s0.x,s0.y, s1.x,s1.y, 3.0f, c);
        ctx.fill.SetColor(c.r, c.g, c.b, c.a);
        ctx.fillCircle(s1.x, s1.y, 5.0f);
    }

    bool hXYZ = (hoverAxis_==GizmoAxis3D::XYZ || activeAxis_==GizmoAxis3D::XYZ);
    Color cc = hXYZ ? kHover : kYellow;
    ctx.fill.SetColor(cc.r, cc.g, cc.b, 200);
    ctx.fillCircle(center.x, center.y, 6.0f);
}
