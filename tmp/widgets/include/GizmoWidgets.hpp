#pragma once

#include "Widget.hpp"
#include "Theme.hpp"
#include "Signal.hpp"
#include "BuGUI_base.hpp"   // Vec3f, Mat4f


namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  Gizmo2D - interactive 2D transform handles (Translate / Rotate / Scale)
//
//  Drawn as a transparent overlay inside any parent widget.
//  All coordinates are in the parent's local space (pixels).
//
//  Usage:
//    auto* gizmo = canvas->createChild<Gizmo2D>();
//    gizmo->setPosition(200, 150);
//    gizmo->setMode(GizmoMode::Translate);
//    gizmo->onTranslate.connect([](float dx, float dy) { ... });
// ═════════════════════════════════════════════════════════════════════════════

enum class GizmoMode  { Translate, Rotate, Scale };
enum class GizmoAxis  { None, X, Y, XY };   // XY = center/free move

class Gizmo2D : public Widget
{
public:
    Gizmo2D();

    /// @brief Set the gizmo position in parent space.
    void  setPosition(float x, float y) { posX_ = x; posY_ = y; markDirty(); }
    /// @brief Get the X position.
    float positionX() const { return posX_; }
    /// @brief Get the Y position.
    float positionY() const { return posY_; }

    /// @brief Set the rotation angle in degrees.
    void  setRotation(float deg) { rotation_ = deg; markDirty(); }
    /// @brief Get the rotation angle in degrees.
    float rotation() const       { return rotation_; }

    /// @brief Set the scale factors.
    void  setScale(float sx, float sy) { scaleX_ = sx; scaleY_ = sy; markDirty(); }
    /// @brief Get the horizontal scale factor.
    float scaleX() const { return scaleX_; }
    /// @brief Get the vertical scale factor.
    float scaleY() const { return scaleY_; }

    /// @brief Set the transform mode.
    void      setMode(GizmoMode m) { mode_ = m; markDirty(); }
    /// @brief Get the current transform mode.
    GizmoMode mode() const         { return mode_; }

    /// @brief Set the translation snap grid size.
    void  setSnapTranslate(float grid) { snapTranslate_ = grid; }
    /// @brief Set the rotation snap in degrees.
    void  setSnapRotate(float deg)     { snapRotate_    = deg;  }
    /// @brief Set the scale snap step size.
    void  setSnapScale(float step)     { snapScale_     = step; }

    /// @brief Set the axis line length in pixels.
    void  setAxisLength(float len) { axisLen_    = len; markDirty(); }
    /// @brief Set the handle square size in pixels.
    void  setHandleSize(float s)   { handleSize_ = s;   markDirty(); }

    // ── Signals ──────────────────────────────────────────────────────────
    Signal<float, float> onTranslate;   ///< delta dx, dy (parent pixels)
    Signal<float>        onRotate;      ///< cumulative angle (degrees)
    Signal<float, float> onScale;       ///< absolute scale x, y
    /// @brief Emitted when a drag operation begins.
    Signal<>             onDragStart;
    /// @brief Emitted when a drag operation ends.
    Signal<>             onDragEnd;

    /// @brief Get which axis is currently active.
    GizmoAxis activeAxis() const { return activeAxis_; }
    /// @brief Check if a drag operation is in progress.
    bool      isDragging()  const { return dragging_; }

    // ── Overrides ────────────────────────────────────────────────────────
    void  layout() override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;

private:
    float posX_ = 0, posY_ = 0;
    float rotation_ = 0.0f;
    float scaleX_   = 1.0f, scaleY_ = 1.0f;
    GizmoMode mode_  = GizmoMode::Translate;
    GizmoAxis activeAxis_ = GizmoAxis::None;
    GizmoAxis hoverAxis_  = GizmoAxis::None;
    bool  dragging_       = false;
    float dragStartX_     = 0, dragStartY_     = 0;
    float dragStartAngle_ = 0;
    float dragStartScaleX_ = 0, dragStartScaleY_ = 0;

    float snapTranslate_ = 0, snapRotate_ = 0, snapScale_ = 0;
    float axisLen_    = 60.0f;
    float handleSize_ = 8.0f;

    static inline const Color kAxisX  {200,  60,  60, 255};
    static inline const Color kAxisY  { 60, 200,  60, 255};
    static inline const Color kCenter {220, 180,  60, 255};
    static inline const Color kHover  {255, 128,  16, 220};

    void toGizmoSpace(float mx, float my, float& gx, float& gy) const;
    GizmoAxis hitTest(float mx, float my) const;
    float snap(float val, float grid) const;

    void paintTranslate(PaintContext& ctx, float cx, float cy);
    void paintRotate(PaintContext& ctx, float cx, float cy);
    void paintScale(PaintContext& ctx, float cx, float cy);
};

// ═════════════════════════════════════════════════════════════════════════════
//  Gizmo3D - interactive 3D transform handles (Translate / Rotate / Scale)
//
//  Renders a 2D overlay by projecting 3D axis handles to screen space.
//  Internally uses Vec3f/Mat4f .
//  The API accepts raw float[16]/float[3] arrays so callers can pass
//  any math library (GLM, Eigen, custom) via their pointer:
//
//    glm::mat4 view = ..., proj = ...;
//    gizmo->setViewProjection(
//        glm::value_ptr(view), glm::value_ptr(proj), vpW, vpH);
//
//    glm::vec3 pos = ..., rot = ..., scale = ...;
//    gizmo->setTarget(&pos.x, &rot.x, &scale.x);
//
//  Signals emit Vec3f (our lightweight type).
// ═════════════════════════════════════════════════════════════════════════════

enum class GizmoMode3D { Translate, Rotate, Scale };
enum class GizmoAxis3D { None, X, Y, Z, XY, XZ, YZ, XYZ };

class Gizmo3D : public Widget
{
public:
    Gizmo3D();

    /// @brief Set the view and projection matrices (column-major).
    void setViewProjection(const float* view4x4, const float* proj4x4,
                           int vpWidth, int vpHeight);

    /// @brief Set the transform target from float[3] arrays.
    void setTarget(const float* pos3, const float* rotDeg3, const float* scale3);

    /// @brief Get the target position.
    const Vec3f& targetPosition() const { return targetPos_; }
    /// @brief Get the target rotation in degrees.
    const Vec3f& targetRotation() const { return targetRot_; }
    /// @brief Get the target scale.
    const Vec3f& targetScale()    const { return targetScale_; }

    /// @brief Set the 3D transform mode.
    void        setMode(GizmoMode3D m) { mode_ = m; markDirty(); }
    /// @brief Get the current 3D transform mode.
    GizmoMode3D mode() const           { return mode_; }

    /// @brief Set the 3D translation snap grid.
    void setSnapTranslate(float grid) { snapT_ = grid; }
    /// @brief Set the 3D rotation snap in degrees.
    void setSnapRotate(float deg)     { snapR_ = deg;  }
    /// @brief Set the 3D scale snap step.
    void setSnapScale(float step)     { snapS_ = step; }

    /// @brief Set the screen-space scale factor for the gizmo.
    void  setScreenScale(float s) { screenScale_ = s; }
    /// @brief Get the screen-space scale factor.
    float screenScale() const     { return screenScale_; }

    // ── Signals ──
    Signal<Vec3f> onTranslate3D;   ///< delta in world space
    Signal<Vec3f> onRotate3D;      ///< cumulative euler (degrees)
    Signal<Vec3f> onScale3D;       ///< absolute scale
    /// @brief Emitted when a 3D drag operation begins.
    Signal<>      onDragStart;
    /// @brief Emitted when a 3D drag operation ends.
    Signal<>      onDragEnd;

    /// @brief Get which 3D axis is currently active.
    GizmoAxis3D activeAxis() const { return activeAxis_; }
    /// @brief Check if a 3D drag operation is in progress.
    bool        isDragging()  const { return dragging_; }

    // ── Overrides ────────────────────────────────────────────────────────
    void  layout() override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;

private:
    // Camera (column-major Mat4f)
    Mat4f view_;
    Mat4f proj_;
    int   vpW_ = 800;
    int   vpH_ = 600;

    // Target transform
    Vec3f targetPos_   = {};
    Vec3f targetRot_   = {};   // euler degrees
    Vec3f targetScale_ = {1,1,1};

    // Mode & interaction
    GizmoMode3D mode_       = GizmoMode3D::Translate;
    GizmoAxis3D activeAxis_ = GizmoAxis3D::None;
    GizmoAxis3D hoverAxis_  = GizmoAxis3D::None;
    bool  dragging_     = false;
    float dragStartX_   = 0, dragStartY_ = 0;
    Vec3f dragStartPos_;
    Vec3f dragStartRot_;
    Vec3f dragStartScale_;

    // Snapping
    float snapT_ = 0, snapR_ = 0, snapS_ = 0;

    // Visual
    float screenScale_ = 80.0f;

    static inline const Color kRed    {170,  50,  50, 255};
    static inline const Color kGreen  { 50, 170,  50, 255};
    static inline const Color kBlue   { 50,  80, 180, 255};
    static inline const Color kYellow {255, 255, 100, 255};
    static inline const Color kHover  {255, 128,  16, 220};

    BuGUI::Vec2f project(const Vec3f& world) const;
    void    unproject(float sx, float sy, Vec3f& rayOrig, Vec3f& rayDir) const;
    float computeScale() const;
    float snap(float val, float grid) const;

    GizmoAxis3D hitTest(float mx, float my) const;

    void paintTranslate3D(PaintContext& ctx);
    void paintRotate3D(PaintContext& ctx);
    void paintScale3D(PaintContext& ctx);
};

} // namespace BuGUI
