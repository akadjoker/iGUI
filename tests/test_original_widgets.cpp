#include <igui/Widgets.hpp>
#include <igui/widgets/CodeEditor.hpp>
#include <igui/widgets/DirCache.hpp>
#include <igui/widgets/DockPanel.hpp>
#include <igui/widgets/FileDialog.hpp>
#include <igui/widgets/GizmoWidgets.hpp>
#include <igui/widgets/Theme.hpp>
#include <igui/widgets/Timeline.hpp>
#include <igui/widgets/ToolBar.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ct/ini.hpp>
#include <type_traits>

static_assert(std::is_same<ig::Color, ig::retained::Color>::value,
              "retained widgets must use ig::Color");
static_assert(std::is_same<ig::Vec2, ig::retained::Vec2>::value,
              "retained widgets must use ig::Vec2");
static_assert(std::is_same<ig::Rect, ig::retained::Rect>::value,
              "retained widgets must use ig::Rect");
static_assert(std::is_same<ig::Widget, ig::retained::Widget>::value,
              "retained widgets must be exposed as ig::Widget");

// The rounded rectangle used to be built from a fixed 30 degree table: four
// points per corner, which leaves a radius above a few pixels visibly faceted
// next to the immediate-mode rectangles. The corner must now be a curve.
static void test_round_rect_corner_is_a_curve()
{
    ig::retained::DrawList list;
    const float radius = 12.0f;
    const ig::Rect rect = {0.0f, 0.0f, 100.0f, 40.0f};
    list.addRoundRectFilled(rect, radius, ig::Color(255u, 255u, 255u, 255u), 8);

    // Outline plus the fringe ring the anti-aliasing adds, 4 * (8 + 1) each.
    const ct::Vector<ig::retained::DrawVertex> &vertices = list.vertices();
    assert(vertices.size() == 2u * 4u * 9u);

    const ig::Vec2 centres[4] = {
        {rect.x + rect.w - radius, rect.y + radius},
        {rect.x + rect.w - radius, rect.y + rect.h - radius},
        {rect.x + radius, rect.y + rect.h - radius},
        {rect.x + radius, rect.y + radius}
    };
    for (int corner = 0; corner < 4; ++corner)
    {
        float minDistance = 1e9f;
        float maxDistance = 0.0f;
        float maxGap = 0.0f;
        ig::Vec2 previous;
        for (int i = 0; i <= 8; ++i)
        {
            // Odd vertices are the transparent fringe; the solid corner points
            // are the even ones.
            const ig::retained::DrawVertex &vertex =
                vertices[static_cast<size_t>(2 * (corner * 9 + i))];
            const float dx = vertex.x - centres[corner].x;
            const float dy = vertex.y - centres[corner].y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            minDistance = distance < minDistance ? distance : minDistance;
            maxDistance = distance > maxDistance ? distance : maxDistance;
            if (i > 0)
            {
                const float gapX = vertex.x - previous.x;
                const float gapY = vertex.y - previous.y;
                const float gap = std::sqrt(gapX * gapX + gapY * gapY);
                maxGap = gap > maxGap ? gap : maxGap;
            }
            previous = {vertex.x, vertex.y};
        }
        // The nine points lie on one circle centred on the corner.
        assert(maxDistance - minDistance < 0.01f);
        assert(minDistance > radius - 1.0f && maxDistance < radius + 1.0f);
        // Eight steps of 11.25 degrees: a chord of 2 * 12 * sin(5.625) = 2.35 px.
        // The old 30 degree table stepped 6.2 px, and that is what read as a bevel.
        assert(maxGap < 3.0f);
    }
}

// The knob's arc is open at the bottom, like a hardware pot and like the
// immediate-mode knob. The old table started at 225 degrees, which left the
// opening on the left instead - the two knobs did not match.
static void test_knob_arc_is_open_at_the_bottom()
{
    ig::retained::DrawList list;
    ig::retained::PaintContext ctx(list);
    ig::retained::Knob knob;
    knob.setRect({0.0f, 0.0f, 64.0f, 64.0f});
    knob.setShowValue(false);
    knob.setValue(1.0f);
    knob.paint(ctx);

    const float cx = 32.0f;
    const float cy = 32.0f;
    const ig::Color track(52u, 58u, 82u, 255u);
    int trackPoints = 0;
    bool inBottomGap = false;
    bool nearLeft = false;
    bool nearTop = false;
    bool nearRight = false;
    const ct::Vector<ig::retained::DrawVertex> &vertices = list.vertices();
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        const ig::retained::DrawVertex &vertex = vertices[i];
        if (vertex.color.r != track.r || vertex.color.g != track.g || vertex.color.b != track.b)
            continue;
        const float dx = vertex.x - cx;
        const float dy = vertex.y - cy;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance < 20.0f || distance > 34.0f)   // the value ring only
            continue;
        ++trackPoints;
        float angle = std::atan2(dy, dx) * 180.0f / 3.14159265f;
        if (angle < 0.0f) angle += 360.0f;
        // Y grows down, so the bottom of the dial is 90 degrees.
        if (angle > 50.0f && angle < 130.0f) inBottomGap = true;
        if (angle > 165.0f && angle < 195.0f) nearLeft = true;
        if (angle > 255.0f && angle < 285.0f) nearTop = true;
        if (angle < 15.0f || angle > 345.0f) nearRight = true;
    }
    assert(trackPoints > 40);
    assert(!inBottomGap);
    assert(nearLeft && nearTop && nearRight);
}

// The knob takes its accent from the caller: with none set it follows the theme
// accent instead of a colour fixed inside the widget, so a voice picks its own.
static void test_knob_accent_comes_from_the_caller()
{
    const ig::Color accent = ig::retained::Theme::instance().focusColor;

    ig::retained::DrawList themed;
    ig::retained::PaintContext themedCtx(themed);
    ig::retained::Knob plain;
    plain.setRect({0.0f, 0.0f, 64.0f, 64.0f});
    plain.setShowValue(false);
    plain.setValue(1.0f);
    plain.paint(themedCtx);

    bool foundThemeAccent = false;
    for (size_t i = 0; i < themed.vertices().size(); ++i)
    {
        if (themed.vertices()[i].color == accent)
            foundThemeAccent = true;
    }
    assert(foundThemeAccent);

    ig::retained::DrawList coloured;
    ig::retained::PaintContext colouredCtx(coloured);
    ig::retained::Knob picked;
    picked.setRect({0.0f, 0.0f, 64.0f, 64.0f});
    picked.setShowValue(false);
    picked.setArcColor(ig::Color(10u, 20u, 30u, 255u));
    picked.setValue(1.0f);
    picked.paint(colouredCtx);

    bool foundPicked = false;
    bool foundTheme = false;
    for (size_t i = 0; i < coloured.vertices().size(); ++i)
    {
        const ig::Color &colour = coloured.vertices()[i].color;
        if (colour.r == 10u && colour.g == 20u && colour.b == 30u)
            foundPicked = true;
        if (colour == accent)
            foundTheme = true;
    }
    assert(foundPicked);
    assert(!foundTheme);
}

// A mixer fader takes the press anywhere: a finger never lands on the cap, so
// the whole control has to work, and the value has to go where the pointer is.
static void test_fader_takes_a_press_anywhere()
{
    // Deliberately away from the origin: a press arrives in the widget's own
    // coordinates, so the drag has to be measured from the widget, not from the
    // top left of the screen.
    ig::retained::Fader fader;
    fader.setRect({30.0f, 210.0f, 40.0f, 120.0f});
    fader.setShowTicks(false);
    fader.setValue(0.5f);

    // The control keeps 14 pixels for the value row, so the travel runs from
    // y 7 to y 99 with the cap 14 thick: the bottom of it is the minimum.
    ig::MouseEvent press;
    press.button = 0;
    press.localX = 20.0f;
    press.localY = 99.0f;
    fader.onMousePress(press);
    assert(fader.value() == 0.0f);

    // The top of the travel is the maximum, so a vertical fader reads upwards.
    press.localY = 7.0f;
    fader.onMousePress(press);
    assert(fader.value() == 1.0f);

    fader.onMouseRelease(press);
    assert(fader.value() == 1.0f);

    // A press on the cap keeps the gap it landed with. At half way the cap is
    // centred on y 53, so a grab two pixels above that leaves the value alone.
    fader.setValue(0.5f);
    press.localY = 51.0f;
    fader.onMousePress(press);
    assert(fader.value() == 0.5f);

    // Carrying the grab ten pixels up raises the value by ten pixels worth of
    // travel (106 - 14).
    press.localY = 41.0f;
    fader.onMouseMove(press);
    assert(fader.value() > 0.60f && fader.value() < 0.62f);
    fader.onMouseRelease(press);

    // Horizontal faders read left to right.
    ig::retained::Fader across;
    across.setRect({30.0f, 340.0f, 120.0f, 40.0f});
    across.setOrientation(ig::LayoutDir::Horizontal);
    across.setShowTicks(false);
    across.setValue(0.0f);
    press.localX = 113.0f;   // the right end of the travel
    press.localY = 13.0f;
    across.onMousePress(press);
    assert(across.value() == 1.0f);
    press.localX = 7.0f;     // the left end of the travel
    across.onMousePress(press);
    assert(across.value() == 0.0f);
    across.onMouseRelease(press);
}

// The box the gizmo outlines, in pixels: the model outline is the only geometry
// drawn in its colour, so its vertices measure the box.
static ig::Rect modelOutlineBounds(const ig::retained::DrawList &list)
{
    ig::Rect bounds = {0.0f, 0.0f, 0.0f, 0.0f};
    float lowX = 1e9f, lowY = 1e9f, highX = -1e9f, highY = -1e9f;
    const ct::Vector<ig::retained::DrawVertex> &vertices = list.vertices();
    for (ct::Vector<ig::retained::DrawVertex>::size_type index = 0; index < vertices.size(); ++index)
    {
        const ig::retained::DrawVertex &vertex = vertices[index];
        if (vertex.color.r != 238u || vertex.color.g != 150u || vertex.color.b != 62u)
            continue;
        lowX = vertex.x < lowX ? vertex.x : lowX;
        lowY = vertex.y < lowY ? vertex.y : lowY;
        highX = vertex.x > highX ? vertex.x : highX;
        highY = vertex.y > highY ? vertex.y : highY;
    }
    if (highX < lowX)
        return bounds;
    bounds.x = lowX;
    bounds.y = lowY;
    bounds.width = highX - lowX;
    bounds.height = highY - lowY;
    return bounds;
}

// The gizmo draws the box it moves, and the box follows the turn: without it a
// ring drag changes only the text under the centre, because the rings
// themselves are pinned to the world.
static void test_gizmo3d_draws_the_box_it_moves()
{
    const float view[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -4, 1};
    const float projection[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const float position[3] = {0.0f, 0.0f, 0.0f};
    const float scale[3] = {1.0f, 1.0f, 1.0f};
    const float upright[3] = {0.0f, 0.0f, 0.0f};

    ig::retained::Gizmo3D gizmo;
    gizmo.setRect({0.0f, 0.0f, 400.0f, 300.0f});
    gizmo.setViewProjection(view, projection, 400, 300);
    gizmo.setScreenScale(50.0f);
    gizmo.setMode(ig::retained::GizmoMode3D::Rotate);
    gizmo.setTarget(position, upright, scale);

    // The camera sits four units away, so the handles - and the box they move -
    // measure 50 * 4 / 300 world units either side of the centre at (200, 150).
    ig::retained::DrawList first;
    ig::retained::PaintContext firstContext(first);
    gizmo.paint(firstContext);
    const ig::Rect uprightBox = modelOutlineBounds(first);
    // The widget is 400 by 300, so a world unit is 200 pixels across and 150
    // down: the box is 266 by 200 of them, its outline's anti-aliased fringe
    // included.
    assert(uprightBox.width > 266.0f && uprightBox.width < 272.0f);
    assert(uprightBox.height > 197.0f && uprightBox.height < 204.0f);
    assert(uprightBox.x > 64.0f && uprightBox.x < 70.0f);

    // Turned 45 degrees towards the eye, its corners reach further across.
    const float turned[3] = {0.0f, 45.0f, 0.0f};
    ig::retained::Gizmo3D turning;
    turning.setRect({0.0f, 0.0f, 400.0f, 300.0f});
    turning.setViewProjection(view, projection, 400, 300);
    turning.setScreenScale(50.0f);
    turning.setMode(ig::retained::GizmoMode3D::Rotate);
    turning.setTarget(position, turned, scale);
    ig::retained::DrawList second;
    ig::retained::PaintContext secondContext(second);
    turning.paint(secondContext);
    assert(modelOutlineBounds(second).width > uprightBox.width + 80.0f);

    // The box can be turned off, and then the rings are on their own.
    gizmo.setModelVisible(false);
    ig::retained::DrawList third;
    ig::retained::PaintContext thirdContext(third);
    gizmo.paint(thirdContext);
    assert(modelOutlineBounds(third).width == 0.0f);
}

// A 3D gizmo is grabbed by the handle under the pointer, and the drag has to
// move the target on that axis alone. The camera looks down the Z axis, so the
// X handle runs right from the centre and the Z ring is the ellipse the test
// clicks.
static void test_gizmo3d_grabs_the_handle_under_the_pointer()
{
    const float view[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -4, 1};
    const float projection[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const float position[3] = {0.0f, 0.0f, 0.0f};
    const float rotation[3] = {0.0f, 0.0f, 0.0f};
    const float scale[3] = {1.0f, 1.0f, 1.0f};

    ig::retained::Gizmo3D gizmo;
    gizmo.setRect({0.0f, 0.0f, 400.0f, 300.0f});
    gizmo.setViewProjection(view, projection, 400, 300);
    gizmo.setTarget(position, rotation, scale);
    // The camera sits four units away, so the handles measure 50 * 4 / 300 world
    // units and the X one ends 133 pixels right of the centre at (200, 150).
    gizmo.setScreenScale(50.0f);
    gizmo.setMode(ig::retained::GizmoMode3D::Translate);

    ig::MouseEvent press;
    press.button = 0;
    press.x = 280.0f;
    press.y = 150.0f;
    gizmo.onMousePress(press);
    assert(gizmo.isDragging());
    assert(gizmo.activeAxis() == ig::retained::GizmoAxis3D::X);

    ig::MouseEvent move;
    move.button = 0;
    move.x = 300.0f;
    move.y = 150.0f;
    gizmo.onMouseMove(move);
    assert(gizmo.targetPosition().x > 0.25f && gizmo.targetPosition().x < 0.28f);
    assert(gizmo.targetPosition().y == 0.0f && gizmo.targetPosition().z == 0.0f);
    gizmo.onMouseRelease(move);
    assert(!gizmo.isDragging());

    // A press away from every handle grabs nothing.
    ig::MouseEvent away;
    away.button = 0;
    away.x = 380.0f;
    away.y = 280.0f;
    gizmo.onMousePress(away);
    assert(!gizmo.isDragging());

    // Scale takes the axis it was grabbed on too.
    gizmo.setMode(ig::retained::GizmoMode3D::Scale);
    gizmo.onMousePress(press);
    assert(gizmo.isDragging());
    gizmo.onMouseMove(move);
    assert(gizmo.targetScale().x > 1.05f);
    assert(gizmo.targetScale().y == 1.0f && gizmo.targetScale().z == 1.0f);
    gizmo.onMouseRelease(move);

    // The rotation rings are curves, not a ring of sample points: a press
    // between two samples of the same ring has to take hold as well. The target
    // goes back to the origin first, so the ring is where the test expects it.
    gizmo.setTarget(position, rotation, scale);
    gizmo.setMode(ig::retained::GizmoMode3D::Rotate);
    // The Z ring projects to the ellipse 133 by 100 pixels around the centre.
    // These are points on it at 30 and 55 degrees round the centre, so the drag
    // turns the target by the 25 degrees between them - and 45 degrees, which is
    // between two of the samples the ring used to be tested at, has to be on the
    // ring as well.
    const auto ringPoint = [](float degrees, float &x, float &y) {
        const float radians = degrees * 3.14159265f / 180.0f;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        const float radius = 1.0f / std::sqrt(cosine * cosine / (133.0f * 133.0f) +
                                              sine * sine / (100.0f * 100.0f));
        x = 200.0f + radius * cosine;
        y = 150.0f + radius * sine;
    };

    ig::MouseEvent between;
    between.button = 0;
    ringPoint(45.0f, between.x, between.y);
    gizmo.onMousePress(between);
    assert(gizmo.isDragging());
    assert(gizmo.activeAxis() == ig::retained::GizmoAxis3D::Z);
    gizmo.onMouseRelease(between);

    ig::MouseEvent ring;
    ring.button = 0;
    ringPoint(30.0f, ring.x, ring.y);
    gizmo.onMousePress(ring);
    assert(gizmo.activeAxis() == ig::retained::GizmoAxis3D::Z);

    ig::MouseEvent along;
    along.button = 0;
    ringPoint(55.0f, along.x, along.y);
    gizmo.onMouseMove(along);
    // The turn is measured on the ring's plane, not on the screen: with this
    // camera a pixel maps to the world point the ring lives in, so the expected
    // angle is the angle between the two world vectors, which the ellipse's
    // 25 degrees of screen are not.
    const auto worldOnRing = [](float px, float py, float &x, float &y) {
        x = (px / 400.0f) * 2.0f - 1.0f;
        y = 1.0f - (py / 300.0f) * 2.0f;
    };
    float fromX = 0.0f, fromY = 0.0f, toX = 0.0f, toY = 0.0f;
    worldOnRing(ring.x, ring.y, fromX, fromY);
    worldOnRing(along.x, along.y, toX, toY);
    const float expected = std::atan2(fromX * toY - fromY * toX, fromX * toX + fromY * toY) * 180.0f / 3.14159265f;
    assert(gizmo.targetRotation().z > expected - 0.5f && gizmo.targetRotation().z < expected + 0.5f);
    assert(gizmo.targetRotation().x == 0.0f && gizmo.targetRotation().y == 0.0f);
    gizmo.onMouseRelease(along);
}

// The fader takes its accent from the caller like the knob does: with none set
// it follows the theme rather than fixing a colour of its own.
static void test_fader_accent_comes_from_the_caller()
{
    const ig::Color accent = ig::retained::Theme::instance().focusColor;

    ig::retained::DrawList themed;
    ig::retained::PaintContext themedCtx(themed);
    ig::retained::Fader plain;
    plain.setRect({0.0f, 0.0f, 40.0f, 120.0f});
    plain.setShowValue(false);
    plain.setShowTicks(false);
    plain.setValue(1.0f);
    plain.paint(themedCtx);

    bool foundThemeAccent = false;
    for (size_t i = 0; i < themed.vertices().size(); ++i)
    {
        if (themed.vertices()[i].color == accent)
            foundThemeAccent = true;
    }
    assert(foundThemeAccent);

    ig::retained::DrawList coloured;
    ig::retained::PaintContext colouredCtx(coloured);
    ig::retained::Fader picked;
    picked.setRect({0.0f, 0.0f, 40.0f, 120.0f});
    picked.setShowValue(false);
    picked.setShowTicks(false);
    picked.setFillColor(ig::Color(10u, 20u, 30u, 255u));
    picked.setValue(1.0f);
    picked.paint(colouredCtx);

    bool foundPicked = false;
    bool foundTheme = false;
    for (size_t i = 0; i < coloured.vertices().size(); ++i)
    {
        const ig::Color &colour = coloured.vertices()[i].color;
        if (colour.r == 10u && colour.g == 20u && colour.b == 30u)
            foundPicked = true;
        if (colour == accent)
            foundTheme = true;
    }
    assert(foundPicked);
    assert(!foundTheme);
}

// Float windows keep their geometry in an .ini, the way ImGui does: one
// section per window title, applied when the window is created again.
static void test_float_window_state_round_trips_through_ini()
{
    const char *path = "igui_window_state_test.ini";
    std::remove(path);

    ig::retained::WidgetApp &app = ig::retained::WidgetApp::instance();
    app.setWindowStatePath(path);
    assert(app.windowStatePath() == path);

    auto *inspector = app.addFloat<ig::retained::FloatWindow>("Inspector");
    inspector->setFloatPos(120.0f, 80.0f);
    inspector->setFloatSize(360.0f, 240.0f);
    inspector->setMinimized(true);

    // ']' cannot appear in a section name, so a title carrying one is folded.
    auto *console = app.addFloat<ig::retained::FloatWindow>("Console]");
    console->setFloatPos(12.0f, 34.0f);
    console->setFloatSize(100.0f, 50.0f);

    // A blank title has no section to live in, so it is left out.
    auto *untitled = app.addFloat<ig::retained::FloatWindow>("   ");
    untitled->setFloatPos(7.0f, 7.0f);

    assert(app.saveWindowState());

    ct::Ini written;
    assert(written.load(path));
    assert(written.size() == 2);
    assert(written.get_double("Inspector", "pos_x") == 120.0);
    assert(written.get_double("Inspector", "pos_y") == 80.0);
    assert(written.get_double("Inspector", "size_w") == 360.0);
    assert(written.get_double("Inspector", "size_h") == 240.0);
    assert(written.get_bool("Inspector", "minimized"));
    assert(written.get_double("Console_", "pos_x") == 12.0);

    // Closing every window must not drop what was written for them.
    app.removeFloat(untitled);
    app.removeFloat(console);
    app.removeFloat(inspector);
    assert(app.floats().empty());
    assert(app.saveWindowState());
    written.load(path);
    assert(written.size() == 2);

    // A window created after the file has been read comes back where it was.
    assert(app.loadWindowState());
    auto *reopened = app.addFloat<ig::retained::FloatWindow>("Inspector");
    assert(reopened->floatX() == 120.0f && reopened->floatY() == 80.0f);
    assert(reopened->floatW() == 360.0f && reopened->floatH() == 240.0f);
    assert(reopened->isMinimized());

    // A title the file knows nothing about keeps its own geometry.
    auto *fresh = app.addFloat<ig::retained::FloatWindow>("Never saved");
    assert(fresh->floatX() == 50.0f && fresh->floatY() == 50.0f);
    assert(fresh->floatW() == 300.0f && fresh->floatH() == 200.0f);

    app.removeFloat(fresh);
    app.removeFloat(reopened);

    // Persistence off means no file is touched.
    app.setWindowStatePath(ig::retained::String());
    assert(app.windowStatePath().empty());
    assert(!app.saveWindowState());

    std::remove(path);
}

// Dock layouts ride along in the same file: a panel with a settings key is
// remembered, and a panel built fresh comes back to that arrangement.
static void test_dock_layout_is_remembered_in_the_state_file()
{
    const char *path = "igui_dock_state_test.ini";
    std::remove(path);

    ig::retained::WidgetApp &app = ig::retained::WidgetApp::instance();
    app.setWindowStatePath(path);

    ig::retained::Widget *stage = app.addStage("dockstate");
    auto *dock = stage->createChild<ig::retained::DockPanel>();
    dock->setSettingsKey("main");
    dock->addPanel("Scene", new ig::retained::Panel());
    dock->addPanel("Properties", new ig::retained::Panel());
    dock->addPanel("Console", new ig::retained::Panel());
    dock->splitOff("Properties", ig::retained::DockSide::Right, 0.3f);
    // A panel without a key is never remembered.
    auto *loose = stage->createChild<ig::retained::DockPanel>();
    loose->addPanel("Loose", new ig::retained::Panel());
    const ig::retained::String arranged = dock->saveLayout();

    assert(app.saveWindowState());

    ct::Ini written;
    assert(written.load(path));
    const ig::retained::String section = ig::dockStateSection("main");
    assert(section == "dock:main");
    assert(written.get(section, "layout") == arranged);
    assert(written.size() == 1u);   // the keyless panel is not in the file

    // The panel the next run builds starts flat and takes the layout back.
    auto *fresh = stage->createChild<ig::retained::DockPanel>();
    fresh->setSettingsKey("main");
    fresh->addPanel("Scene", new ig::retained::Panel());
    fresh->addPanel("Properties", new ig::retained::Panel());
    fresh->addPanel("Console", new ig::retained::Panel());
    assert(fresh->saveLayout() != arranged);
    assert(app.loadWindowState());
    assert(fresh->saveLayout() == arranged);

    // A stage built later must not throw away an arrangement the user has just
    // changed: the file is applied to a panel once.
    fresh->splitOff("Console", ig::retained::DockSide::Bottom, 0.4f);
    const ig::retained::String changed = fresh->saveLayout();
    assert(changed != arranged);
    app.addStage("later");
    ig::retained::IO io;
    app.update(io);
    assert(fresh->saveLayout() == changed);

    app.setWindowStatePath(ig::retained::String());
    std::remove(path);
}

// closePanel() hands the content widget to removeChild(), which already deletes
// it: a second delete corrupts the heap (AddressSanitizer aborts right here).
// The final version of the old code wrote a comment saying "deletes it" - a
// regression here is a crash, not a wrong pixel.
static void test_closing_a_dock_panel_deletes_its_content_once()
{
    ig::retained::DockPanel dock;
    dock.setRect({0.0f, 0.0f, 600.0f, 400.0f});
    dock.addPanel("Inspector", new ig::retained::Panel());
    dock.addPanel("Console", new ig::retained::Panel());
    dock.splitOff("Console", ig::retained::DockSide::Bottom, 0.3f);
    dock.layout();

    const ig::retained::String before = dock.saveLayout();
    assert(strstr(before.c_str(), "Console") != nullptr);

    dock.closePanel("Console");
    dock.layout();
    const ig::retained::String after = dock.saveLayout();
    assert(strstr(after.c_str(), "Console") == nullptr);
    assert(strstr(after.c_str(), "Inspector") != nullptr);

    // Closing what is not there any more is a no-op, not a second delete.
    dock.closePanel("Console");
    dock.closePanel("Never added");
    dock.layout();

    dock.closePanel("Inspector");
    dock.layout();
    assert(strstr(dock.saveLayout().c_str(), "Inspector") == nullptr);
}

// A dialog that closes itself already removed the float, and the application's
// own result handler removes it again: the second call must not queue the same
// pointer, or the deferred delete frees it twice.
static void test_removing_a_float_twice_is_harmless()
{
    ig::retained::WidgetApp &app = ig::retained::WidgetApp::instance();
    const size_t floatsBefore = app.floats().size();

    auto *dialog = app.addFloat<ig::retained::FloatWindow>("Dialog");
    assert(app.floats().size() == floatsBefore + 1u);
    app.removeFloat(dialog);
    app.removeFloat(dialog);
    assert(app.floats().size() == floatsBefore);

    ig::retained::IO io;
    app.update(io);   // the deferred deletes are flushed here
    assert(app.floats().size() == floatsBefore);
}

// A column added to a grid that already has rows must leave every row with a
// cell for it: the accessors index cells up to columns_.size().
static void test_a_column_added_after_rows_has_cells()
{
    ig::retained::DataGrid grid;
    grid.setRect({0.0f, 0.0f, 400.0f, 200.0f});
    grid.addColumn("Name", 120.0f);
    grid.addRow({"first"});
    grid.addColumn("Note", 120.0f);      // the row predates this column

    grid.setCell(0, 1, "hello");         // used to write past the row's cells
    assert(grid.cell(0, 1) == "hello");
    assert(grid.cell(0, 0) == "first");
    assert(grid.cell(1, 1) == "");

    // A click on the second header sorts by it, which used to read past the
    // row's cells. (Nothing to assert if the click misses the header; the write
    // above is the memory-unsafe half and AddressSanitizer owns it.)
    grid.layout();
    ig::MouseEvent headerClick;
    headerClick.button = 0;
    headerClick.x = 150.0f;
    headerClick.y = 10.0f;
    grid.onMousePress(headerClick);
    grid.onMouseRelease(headerClick);
    assert(grid.cell(0, 0) == "first");
    assert(grid.cell(0, 1) == "hello");
    grid.layout();
}

// Remove lines, then undo: the replayed action must land inside the buffer the
// editor has now, and it must put the removed line back.
static void test_removing_a_line_can_be_undone()
{
    ig::retained::CodeEditor editor;
    editor.setText("l0\nl1\nl2\nl3");
    editor.setCursorPos(3, 0);

    ig::retained::KeyEvent typed;
    typed.text[0] = 'x';
    editor.onTextInput(typed);           // an undoable edit on the last line
    const ig::retained::String edited = editor.text();
    assert(edited == "l0\nl1\nl2\nxl3");

    editor.removeLine();                 // the last line: no break to take with it
    editor.removeLine();                 // and now the one that followed it
    assert(editor.lineCount() == 2);

    editor.undo();                       // used to replay the insert at line 3 of a 2-line buffer
    assert(editor.lineCount() == 3);
    editor.undo();
    assert(editor.lineCount() == 4);
    assert(editor.text() == edited);

    editor.undo();
    assert(editor.text() == "l0\nl1\nl2\nl3");
    assert(!editor.canUndo());
}

// A window title ct::Ini would refuse - one with a trailing space, say - must not
// abort the application: it folds into a name the file can hold and read back.
static void test_odd_window_titles_do_not_abort_the_state_file()
{
    const char *path = "igui_odd_title_test.ini";
    std::remove(path);

    ig::WindowStateIni store;
    store.setPath(path);
    ig::WindowGeometry geometry;
    geometry.x = 5.0f;
    geometry.y = 6.0f;
    geometry.w = 70.0f;
    geometry.h = 80.0f;
    store.putWindow("Console ", geometry);   // trailing space
    store.putWindow(" Frame", geometry);     // leading space
    store.putWindow("   ", geometry);        // nothing but blanks: left out
    assert(store.save());

    ct::Ini written;
    assert(written.load(path));
    assert(written.size() == 2u);
    assert(written.get_double("Console_", "pos_x") == 5.0);
    assert(written.get_double("_Frame", "size_h") == 80.0);

    ig::WindowGeometry read;
    assert(store.load());
    assert(store.getWindow("Console ", read));
    assert(read.x == 5.0f && read.h == 80.0f);
    assert(store.getWindow(" Frame", read));

    store.setPath(ig::retained::String());
    std::remove(path);
}

// A handler is allowed to remove the widget it was called on - that is what
// Widget::removeChild documents. The manager must not touch that widget again:
// not the pressed pointer, not the parent walk, not the global event it fires
// after the signal.
static void test_a_widget_can_be_removed_by_its_own_handler()
{
    ig::retained::WidgetApp &app = ig::retained::WidgetApp::instance();
    ig::retained::Widget *stage = app.addStage("selfremove");
    auto *panel = stage->createChild<ig::retained::Panel>();
    panel->setRect({0.0f, 0.0f, 200.0f, 100.0f});
    auto *button = panel->createChild<ig::retained::Button>("Close");
    button->setRect({10.0f, 10.0f, 80.0f, 30.0f});
    button->clicked.connect([panel, button]() { panel->removeChild(button); });

    ig::MouseEvent press;
    press.button = 0;
    press.x = 20.0f;
    press.y = 20.0f;
    button->onMousePress(press);

    ig::MouseEvent release = press;
    // The click removes the button; firing the global click event afterwards
    // used to read the freed widget.
    button->onMouseRelease(release);

    assert(panel->children().empty());
}

// Every carousel page keeps the content rect, so the ones under the page on
// screen have to stay hidden: a visible one would take its clicks.
static void test_a_carousel_shows_one_page_at_a_time()
{
    ig::retained::Carousel carousel;
    carousel.setRect({0.0f, 0.0f, 400.0f, 200.0f});
    auto *first  = new ig::retained::Panel();
    auto *second = new ig::retained::Panel();
    carousel.addPageWidget(first);
    carousel.addPageWidget(second);
    carousel.layout();

    assert(first->isVisible());
    assert(!second->isVisible());

    carousel.removePage(0);              // the page on screen goes away
    carousel.layout();
    assert(second->isVisible());         // the one left takes over
}

// Setting the content the window already has must not delete it: removeChild()
// deletes what it takes out, so re-adding the same pointer reads freed memory.
static void test_setting_the_same_float_content_again_keeps_it()
{
    ig::retained::FloatWindow window("Content");
    auto *content = new ig::retained::Panel();
    window.setContentWidget(content);
    assert(window.content() == content);

    window.setContentWidget(content);
    assert(window.content() == content);
    assert(content->parent() == &window);

    window.setContentWidget(nullptr);    // clearing still deletes it
    assert(window.content() == nullptr);
}

// A view that caches a flattened tree compares this counter to know the cache
// no longer describes the tree: a node added or removed behind its back does
// not tell the view about it any other way.
static void test_tree_nodes_report_structural_changes()
{
    const unsigned int before = ig::retained::TreeNode::revision();
    ig::retained::TreeNode root("A");
    ig::retained::TreeNode *child = root.addChild("B");

    const unsigned int afterAdd = ig::retained::TreeNode::revision();
    assert(afterAdd != before);

    child->setExpanded(false);
    const unsigned int afterExpand = ig::retained::TreeNode::revision();
    assert(afterExpand != afterAdd);

    root.removeChild(0);
    assert(ig::retained::TreeNode::revision() != afterExpand);

    root.addChild("C");
    const unsigned int afterSecondAdd = ig::retained::TreeNode::revision();
    root.clear();
    assert(ig::retained::TreeNode::revision() != afterSecondAdd);
}

// The widget that opened a popup going away must take the popup with it: the
// popup holds a pointer back to its owner and keeps calling it.
static void test_a_popup_goes_away_with_its_owner()
{
    ig::retained::WidgetApp &app = ig::retained::WidgetApp::instance();
    ig::retained::Widget *stage = app.addStage("popupowner");
    auto *panel = stage->createChild<ig::retained::Panel>();
    panel->setRect({0.0f, 0.0f, 200.0f, 100.0f});
    auto *owner = panel->createChild<ig::retained::Button>("Menu");

    auto *popup = new ig::retained::Panel();     // owned by the test: `owned` is false
    app.showPopup(popup, owner, false);
    assert(app.popup() == popup);
    assert(app.popupOwner() == owner);

    panel->removeChild(owner);                   // the owner goes away
    assert(app.popup() == nullptr);              // and its popup with it

    delete popup;
}

// A toolbar handler may add or remove items while the release walk is running:
// the walk must not keep reading the list it is changing.
static void test_a_toolbar_item_can_remove_itself_when_clicked()
{
    ig::retained::ToolBar bar;
    bar.setRect({0.0f, 0.0f, 400.0f, 40.0f});
    const int first  = bar.addButton(0, "New");
    bar.addButton(1, "Open");
    bar.layout();

    bool handled = false;
    bar.buttonClicked.connect([&bar, &handled](int id) { handled = true; bar.removeItem(id); });

    ig::MouseEvent press;
    press.button = 0;
    press.x = 10.0f;                 // the first item sits at (2, 2, 28, 28)
    press.y = 10.0f;
    bar.onMousePress(press);
    ig::MouseEvent release = press;
    bar.onMouseRelease(release);

    assert(handled);
}

// Reading a large folder grows the buffer that holds the children of every
// folder: the folders read before must still list their own children.
static void test_dir_cache_keeps_children_when_it_grows()
{
    using ig::retained::DirCache;
    using ig::retained::String;
    namespace fs = ig::retained::FileSystem;

    const String root = "igui_dir_cache_test";
    const String big = fs::joinPath(root, "big");
    const String small = fs::joinPath(root, "small");
    const bool madeBig = fs::createDir(big);
    const bool madeSmall = fs::createDir(small);
    assert(madeBig && madeSmall);

    const int fileCount = 600;
    char name[16];
    for (int i = 0; i < fileCount; ++i)
    {
        std::snprintf(name, sizeof(name), "f%03d", i);
        FILE* file = std::fopen(fs::joinPath(big, name).c_str(), "wb");
        assert(file != nullptr);
        std::fclose(file);
    }

    {
        DirCache cache;
        cache.setRoot(root);
        DirCache::DirNode* top = cache.root();
        assert(top != nullptr && top->kidCount == 2);

        DirCache::DirNode* bigNode = cache.navigate(big);
        assert(bigNode != nullptr && bigNode->kidCount == 0);
        cache.expand(bigNode);
        assert(bigNode->kidCount == fileCount);

        assert(top->kids[0]->name == "big");
        assert(top->kids[1]->name == "small");
        assert(top->kids[0] == bigNode);
    }

    for (int i = 0; i < fileCount; ++i)
    {
        std::snprintf(name, sizeof(name), "f%03d", i);
        fs::removeFile(fs::joinPath(big, name));
    }
    std::remove(big.c_str());
    std::remove(small.c_str());
    std::remove(root.c_str());
}

// A stage can be removed while a transition is still leaving it: the
// transition must stop painting the tree that was just deleted.
static void test_removing_the_stage_being_left_stops_its_transition()
{
    using ig::retained::TransitionType;
    ig::retained::WidgetApp& app = ig::retained::WidgetApp::instance();
    ig::retained::Widget* menu = app.addStage("transitionmenu");
    menu->createChild<ig::retained::Button>("Play");
    app.addStage("transitiongame");

    ig::retained::IO io;
    io.displayWidth = 320.0f;
    io.displayHeight = 240.0f;
    app.update(io);
    assert(app.setStage("transitionmenu", TransitionType::None));
    assert(app.setStage("transitiongame", TransitionType::SlideLeft));
    assert(app.isTransitioning());

    assert(app.removeStage("transitionmenu"));
    assert(!app.isTransitioning());

    ig::retained::DrawData data;
    app.paint(data);
    assert(data.passes.size() == 1u);
}

// A view and its model can go away in either order: the view must not stay
// connected to a model it left, and must not reach a model that is gone.
static void test_views_let_go_of_their_models()
{
    using namespace ig::retained;

    ListModel shared(2);
    auto* grid = new DataGrid();
    grid->setModel(&shared);
    assert(!shared.rowsInserted.empty());
    delete grid;
    assert(shared.modelReset.empty() && shared.rowsInserted.empty() && shared.rowsRemoved.empty() &&
            shared.dataChanged.empty() && shared.layoutChanged.empty());
    shared.endInsertRows();

    ListModel other(2);
    DataGrid switched;
    switched.setModel(&shared);
    switched.setModel(&other);
    assert(shared.rowsInserted.empty());
    assert(!other.rowsInserted.empty());

    DataGrid outlives;
    {
        ListModel shortLived(2);
        outlives.setModel(&shortLived);
    }
    assert(outlives.model() == nullptr);

    TreeModel tree(1);
    auto* view = new TreeView();
    view->setModel(&tree);
    assert(!tree.modelReset.empty());
    delete view;
    assert(tree.modelReset.empty() && tree.rowsInserted.empty() && tree.dataChanged.empty());
    tree.endResetModel();

    TreeView treeOutlives;
    {
        TreeModel shortLived(1);
        treeOutlives.setModel(&shortLived);
    }
    assert(treeOutlives.model() == nullptr);

    {
        SortFilterProxyModel proxy(&shared);
        assert(!shared.rowsInserted.empty());
    }
    assert(shared.rowsInserted.empty());
    shared.endInsertRows();
}

namespace
{
struct CountedPopup : ig::retained::Widget
{
    explicit CountedPopup(int* gone)
            : gone_(gone)
    {
    }
    ~CountedPopup() override
    {
        ++*gone_;
    }
    int* gone_;
};

void runFrames(ig::retained::WidgetApp& app, int frames)
{
    ig::retained::IO io;
    for (int i = 0; i < frames; ++i) app.update(io);
}
} // namespace

// A popup the manager owns is deleted once it is gone: after its close
// animation, when another popup replaces it, and when its owner goes away.
static void test_owned_popups_are_deleted()
{
    ig::retained::WidgetApp& app = ig::retained::WidgetApp::instance();
    int gone = 0;

    app.showPopup(new CountedPopup(&gone), nullptr, true);
    app.closePopup();
    runFrames(app, 60);
    assert(gone == 1);

    app.showPopup(new CountedPopup(&gone), nullptr, true);
    app.showPopup(new CountedPopup(&gone), nullptr, true);
    runFrames(app, 2);
    assert(gone == 2);
    app.closePopup();
    runFrames(app, 60);
    assert(gone == 3);

    ig::retained::Widget* stage = app.addStage("popupownedowner");
    auto* owner = stage->createChild<ig::retained::Button>("Pick");
    app.showPopup(new CountedPopup(&gone), owner, true);
    stage->removeChild(owner);
    assert(app.popup() == nullptr);
    runFrames(app, 2);
    assert(gone == 4);

    int kept = 0;
    CountedPopup notOwned(&kept);
    app.showPopup(&notOwned, nullptr, false);
    app.closePopup();
    runFrames(app, 60);
    assert(kept == 0);
}

// The file dialog opens a folder on a double click, which the manager counts
// in clickCount: two single clicks on the same row, however far apart in
// time, are not one.
static void test_file_dialog_opens_only_on_a_double_click()
{
    namespace fs = ig::retained::FileSystem;
    using ig::retained::String;

    const String root = fs::joinPath(fs::currentDir(), "igui_file_dialog_test");
    const String folder = fs::joinPath(root, "a");
    const bool made = fs::createDir(folder);
    assert(made);

    ig::retained::FileDialog dialog;
    dialog.setRect({ 0.0f, 0.0f, 700.0f, 480.0f });
    dialog.layout();
    dialog.setPath(root);
    dialog.layout();

    auto press = [&dialog](float x, float y, int clicks)
    {
        ig::retained::MouseEvent e;
        e.button = 0;
        e.x = x;
        e.y = y;
        e.clickCount = clicks;
        dialog.onMousePress(e);
        ig::retained::MouseEvent release = e;
        dialog.onMouseRelease(release);
        dialog.layout();
    };

    float rowX = -1.0f;
    float rowY = -1.0f;
    for (float y = 0.0f; y < 480.0f && rowY < 0.0f; y += 4.0f)
    {
        press(450.0f, y, 1);
        press(450.0f, y, 2);
        if (dialog.path() == folder)
        {
            rowX = 450.0f;
            rowY = y;
        }
    }
    assert(rowY >= 0.0f);

    dialog.setPath(root);
    dialog.layout();
    press(rowX, rowY, 1);
    press(rowX, rowY, 1);
    assert(dialog.path() == root);

    press(rowX, rowY, 2);
    assert(dialog.path() == folder);

    std::remove(folder.c_str());
    std::remove(root.c_str());
}

// A ListWidget rola de verdade: com a lista rolada, um clique no botão da linha
// que está no topo chega a esse botão e não ao da linha 0. E o clique numa linha
// seleciona-a pelo índice que ela tem agora, mesmo depois de um removeRow.
static void test_list_widget_scrolls_its_rows_for_the_pointer_too()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("listscroll");
    auto* list = stage->createChild<ListWidget>();
    int clickedRow = -1;
    for (int i = 0; i < 20; ++i)
    {
        auto* row = list->addRow<Button>("row");
        row->clicked.connect([&clickedRow, i]() { clickedRow = i; });
    }
    assert(app.setStage("listscroll", TransitionType::None));

    IO io;
    io.displayWidth = 400.0f;
    io.displayHeight = 300.0f;
    app.update(io);
    DrawData data;
    app.paint(data);

    MouseEvent wheel;
    wheel.scrollY = -2.0f;
    list->onMouseScroll(wheel);
    // Dois entalhes de 1,5 linhas cada: a linha 3 fica no topo.
    const float rowHeight = Theme::instance().buttonHeight + 8.0f;
    const int topRow = 3;

    io.mouseX = 100.0f;
    io.mouseY = rowHeight * 0.5f;
    app.update(io);
    io.mouseDown[0] = true;
    app.update(io);
    io.mouseDown[0] = false;
    app.update(io);
    assert(clickedRow == topRow);
    assert(list->selectedIndex() == topRow);

    auto* listAfter = stage->createChild<ListWidget>();
    listAfter->addRow<Button>("a");
    listAfter->addRow<Button>("b");
    auto* third = listAfter->addRow<Button>("c");
    listAfter->removeRow(0);
    third->pressed.emit(0);
    assert(listAfter->selectedIndex() == 1);
}

// setVisible(false) esconde mesmo: um Canvas escondido não corre o seu desenho e
// um DockPanel escondido não desenha nada (o layout já os saltava, o paint não).
static void test_hidden_widgets_do_not_paint()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("hiddenpaint");
    auto* canvas = stage->createChild<Canvas>();
    int canvasPaints = 0;
    canvas->setOnPaint([&canvasPaints](PaintContext&, const Rect&) { ++canvasPaints; });
    auto* dock = stage->createChild<DockPanel>();
    dock->addPanel<Label>("Scene", "scene");
    assert(app.setStage("hiddenpaint", TransitionType::None));

    IO io;
    io.displayWidth = 400.0f;
    io.displayHeight = 300.0f;
    app.update(io);
    auto vertexCount = [&app]()
    {
        // O backend limpa a lista depois de a desenhar; aqui limpa-se antes.
        GetDrawList().clear();
        DrawData data;
        app.paint(data);
        size_t count = 0;
        for (const DrawPass& pass : data.passes)
            if (pass.list)
                count += pass.list->vertices().size();
        return count;
    };

    canvas->setVisible(false);
    const size_t withDock = vertexCount();
    dock->setVisible(false);
    const size_t withoutDock = vertexCount();
    assert(canvasPaints == 0);
    assert(withoutDock < withDock);
    const size_t emptyStage = vertexCount();
    assert(emptyStage == withoutDock);

    canvas->setVisible(true);
    vertexCount();
    assert(canvasPaints == 1);
}

// O relógio do WidgetApp não perde os frames curtos: 2 s a 2000 fps são 2000 ms
// (antes cada frame de 0,5 ms somava 0 ms, e o duplo clique deixava de ter tempo).
static void test_widget_app_clock_keeps_short_frames()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    IO io;
    io.deltaTime = 0.0005f;
    const uint32_t start = app.elapsedMs();
    for (int i = 0; i < 4000; ++i)
        app.update(io);
    const uint32_t elapsed = app.elapsedMs() - start;
    assert(elapsed >= 1999u && elapsed <= 2001u);
}

namespace
{
// Um widget que se remove a si próprio quando recebe uma tecla ou texto, sem os
// consumir: o bubble continua para o pai depois de o widget ter sido apagado.
struct RemovesItselfOnKey : ig::retained::Widget
{
    RemovesItselfOnKey() { acceptsFocus_ = true; }
    void onKeyPress(ig::retained::KeyEvent&) override { parent()->removeChild(this); }
    void onTextInput(ig::retained::KeyEvent&) override { parent()->removeChild(this); }
};
} // namespace

// Uma tecla ou um texto cujo handler apaga o widget focado não lê o widget
// apagado a seguir (o bubble do rato já tinha esta guarda).
static void test_key_handler_can_remove_its_widget()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("keyremove");
    assert(app.setStage("keyremove", TransitionType::None));

    auto* first = stage->createChild<RemovesItselfOnKey>();
    app.setFocused(first);
    IO io;
    IO::KeyEvent escape;
    escape.key = Key::Escape;
    io.keyEvents.push_back(escape);
    app.update(io);
    assert(stage->children().empty());

    auto* second = stage->createChild<RemovesItselfOnKey>();
    app.setFocused(second);
    IO typing;
    typing.inputChars.push_back(static_cast<uint32_t>('a'));
    app.update(typing);
    assert(stage->children().empty());
}

// Um handler pode ligar e desligar ligações do sinal que o está a chamar: a que
// se desliga a si própria acaba de correr, a seguinte continua a ser chamada, e
// as que se ligam durante o emit só contam no próximo.
static void test_signal_handlers_can_change_the_signal()
{
    using ig::retained::Signal;
    Signal<int> signal;
    Signal<int>::SlotId once = 0;
    int onceCalls = 0;
    int otherCalls = 0;
    int addedCalls = 0;
    int captured = 41;
    once = signal.connect([&signal, &once, &onceCalls, captured](int) mutable {
        signal.disconnect(once);
        ++captured;
        onceCalls += captured - 41;
    });
    signal.connect([&signal, &otherCalls, &addedCalls](int) {
        ++otherCalls;
        for (int i = 0; i < 16; ++i)
            signal.connect([&addedCalls](int) { ++addedCalls; });
    });
    signal.connect([&otherCalls](int) { ++otherCalls; });

    signal.emit(1);
    assert(onceCalls == 1);
    assert(otherCalls == 2);
    assert(addedCalls == 0);

    signal.emit(2);
    assert(onceCalls == 1);
    assert(addedCalls == 16);

    auto* owned = new Signal<>();
    bool after = false;
    owned->connect([&owned]() { delete owned; owned = nullptr; });
    owned->connect([&after]() { after = true; });
    owned->emit();
    assert(owned == nullptr);
    assert(!after);
}

namespace
{
struct TallContent : ig::retained::Widget
{
    explicit TallContent(float height)
            : height_(height)
    {
    }
    Vec2f sizeHint() const override
    {
        return { rect_.w, height_ };
    }
    float height_;
};
} // namespace

// A roda rola só a ScrollView de dentro, e um clique na calha da barra de uma
// ListBox move a lista sem selecionar a linha que está àquela altura. Uma
// ScrollView sem nada para rolar deixa a roda passar para a de fora.
static void test_scrolling_stays_in_the_view_that_scrolls()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("nestedscroll");
    auto* screen = stage->createChild<Widget>();
    auto* outer = screen->createChild<ScrollView>();
    outer->setRect({ 0.0f, 0.0f, 300.0f, 200.0f });
    auto* outerContent = outer->setContent<TallContent>(1000.0f);
    auto* inner = outerContent->createChild<ScrollView>();
    inner->setRect({ 0.0f, 0.0f, 200.0f, 100.0f });
    inner->setContent<TallContent>(500.0f);
    auto* fits = outerContent->createChild<ScrollView>();
    fits->setRect({ 0.0f, 120.0f, 200.0f, 60.0f });
    fits->setContent<TallContent>(20.0f);
    auto* list = screen->createChild<ListBox>();
    list->setRect({ 0.0f, 250.0f, 200.0f, 100.0f });
    for (int i = 0; i < 50; ++i) list->addItem("item");
    int selections = 0;
    list->selectionChanged.connect([&selections](int) { ++selections; });
    assert(app.setStage("nestedscroll", TransitionType::None));

    IO io;
    io.displayWidth = 400.0f;
    io.displayHeight = 400.0f;
    app.update(io);
    DrawData data;
    app.paint(data);
    io.mouseX = 50.0f;
    io.mouseY = 50.0f;
    app.update(io);
    io.mouseWheelY = -1.0f;
    app.update(io);
    io.mouseWheelY = 0.0f;
    assert(inner->scrollY() > 0.0f);
    assert(outer->scrollY() == 0.0f);

    io.mouseY = 150.0f;
    app.update(io);
    io.mouseWheelY = -1.0f;
    app.update(io);
    io.mouseWheelY = 0.0f;
    assert(outer->scrollY() > 0.0f);

    io.mouseX = 196.0f;
    io.mouseY = 340.0f;
    app.update(io);
    io.mouseDown[0] = true;
    app.update(io);
    io.mouseDown[0] = false;
    app.update(io);
    assert(selections == 0);
    assert(list->selectedIndex() == -1);
}

// Uma InputBox, um FileDialog, uma MessageBox e um Dialog modal ficam com o
// teclado quando abrem: o que se escreve vai para o diálogo e não para o campo
// de trás, Enter aceita, Escape cancela (mesmo com o texto selecionado), e ao
// fechar o teclado volta ao campo que o tinha. Um Enter a meio da animação de
// fecho do Dialog não repete a ação.
static void test_dialogs_take_the_keyboard_while_open()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("dialogkeys");
    auto* editor = stage->createChild<TextInput>();
    assert(app.setStage("dialogkeys", TransitionType::None));

    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
    app.setFocused(editor);
    auto type = [&app, &io](const char* text)
    {
        for (const char* c = text; *c; ++c) io.inputChars.push_back(static_cast<uint32_t>(*c));
        app.update(io);
        io.inputChars.clear();
    };
    auto key = [&app, &io](int code)
    {
        IO::KeyEvent press;
        press.key = code;
        io.keyEvents.push_back(press);
        app.update(io);
        io.keyEvents.clear();
    };

    String accepted;
    auto* rename = app.addFloat<InputBox>("Rename");
    rename->accepted.connect([&accepted](const String& text) { accepted = text; });
    type("ab");
    assert(rename->text() == "ab");
    assert(editor->text().empty());
    key(Key::Return);
    assert(accepted == "ab");
    assert(app.focused() == editor);

    int cancels = 0;
    auto* other = app.addFloat<InputBox>("Rename");
    other->cancelled.connect([&cancels]() { ++cancels; });
    other->setText("old");
    key(Key::Escape);
    assert(cancels == 1);
    assert(app.focused() == editor);

    String saved;
    auto* save = app.addFloat<FileDialog>("Save");
    save->setMode(FileDialog::Mode::Save);
    save->accepted.connect([&saved](const String& path) { saved = path; });
    type("out.txt");
    key(Key::Return);
    assert(FileSystem::fileName(saved) == "out.txt");
    assert(app.focused() == editor);

    int answer = -1;
    auto* message = app.addFloat<MessageBox>("Close", "Save first?", MessageBox::YesNo);
    message->result.connect([&answer](MessageBox::Result result) { answer = result; });
    key(Key::Return);
    assert(answer == MessageBox::ResultYes);
    assert(app.focused() == editor);

    int accepts = 0;
    auto* dialog = new Dialog("Delete", "Sure?");
    dialog->addButton("Cancel", Dialog::Role::Cancel);
    dialog->addButton("OK", Dialog::Role::Accept, [&accepts]() { ++accepts; });
    dialog->show();
    type("z");
    key(Key::Return);
    key(Key::Return);
    assert(accepts == 1);
    assert(app.focused() == editor);
    assert(editor->text().empty());
    runFrames(app, 60);
}

// Um popup fechado pelo WidgetApp com um clique fora fica fechado também para
// quem o abriu: o clique seguinte na ComboBox ou no DatePicker volta a abri-lo
// logo. Na MenuBar, clicar no título do menu aberto fecha-o, tal como um clique
// numa parte vazia da barra.
static void test_popups_closed_from_outside_are_closed_for_their_owner()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("popupowners");
    auto* screen = stage->createChild<Widget>();
    auto* combo = screen->createChild<ComboBox>();
    combo->setRect({ 20.0f, 300.0f, 160.0f, 28.0f });
    combo->addItem("one");
    combo->addItem("two");
    auto* date = screen->createChild<DatePicker>();
    date->setRect({ 220.0f, 300.0f, 160.0f, 28.0f });
    auto* bar = screen->createChild<MenuBar>();
    bar->setRect({ 0.0f, 0.0f, 600.0f, 28.0f });
    bar->addMenu("File")->addAction("Open");
    assert(app.setStage("popupowners", TransitionType::None));

    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
    DrawData data;
    app.paint(data);
    auto click = [&app, &io](float x, float y)
    {
        io.mouseX = x;
        io.mouseY = y;
        app.update(io);
        io.mouseDown[0] = true;
        app.update(io);
        io.mouseDown[0] = false;
        app.update(io);
    };

    click(100.0f, 314.0f);
    assert(app.popupOwner() == combo);
    click(700.0f, 550.0f);
    assert(app.popup() == nullptr);
    click(100.0f, 314.0f);
    assert(app.popupOwner() == combo);
    click(100.0f, 314.0f);
    assert(app.popup() == nullptr);

    click(300.0f, 314.0f);
    assert(date->isOpen());
    click(700.0f, 550.0f);
    assert(app.popup() == nullptr);
    assert(!date->isOpen());
    click(300.0f, 314.0f);
    assert(date->isOpen());
    click(300.0f, 314.0f);
    assert(app.popup() == nullptr);

    click(20.0f, 14.0f);
    assert(app.popup() == bar->menu(0));
    click(20.0f, 14.0f);
    assert(app.popup() == nullptr);
    click(20.0f, 14.0f);
    assert(app.popup() == bar->menu(0));
    click(500.0f, 14.0f);
    assert(app.popup() == nullptr);
    runFrames(app, 30);
}

// Um PageView só deixa visíveis a página atual e, durante a animação, a que
// está a sair: sem transição a anterior esconde-se logo, e uma troca a meio de
// um slide esconde a página que já estava a sair.
static void test_page_view_hides_the_pages_it_leaves()
{
    using namespace ig::retained;
    PageView view;
    view.setRect({ 0.0f, 0.0f, 300.0f, 200.0f });
    Widget* pages[3] = { view.addPage<Widget>(), view.addPage<Widget>(), view.addPage<Widget>() };
    auto visible = [&pages](bool first, bool second, bool third)
    {
        return pages[0]->isVisible() == first && pages[1]->isVisible() == second &&
               pages[2]->isVisible() == third;
    };

    view.setPage(1, PageView::None);
    assert(visible(false, true, false));
    view.setPage(2, PageView::SlideLeft);
    assert(visible(false, true, true));
    view.setPage(0, PageView::SlideRight);
    assert(visible(true, false, true));
    view.setPage(1, PageView::None);
    assert(visible(false, true, false));
    assert(view.currentPage() == 1);
}

// Um campo desativado, ou dentro de um painel desativado, não ganha foco com um
// clique nem aceita texto e teclas, e o SpinBox não muda com os botões nem com a
// roda. Desativar o campo que tem o foco tira-lho; voltar a ativar o painel
// devolve tudo ao normal.
static void test_disabled_fields_ignore_input()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("disabledfields");
    auto* screen = stage->createChild<Widget>();
    auto* panel = screen->createChild<Widget>();
    panel->setRect({ 0.0f, 0.0f, 400.0f, 300.0f });
    auto* field = panel->createChild<TextInput>();
    field->setRect({ 10.0f, 10.0f, 200.0f, 28.0f });
    auto* spin = panel->createChild<SpinBox>(0.0f, 100.0f, 50.0f, 1.0f);
    spin->setRect({ 10.0f, 50.0f, 200.0f, 28.0f });
    auto* edit = panel->createChild<TextEdit>();
    edit->setRect({ 10.0f, 100.0f, 300.0f, 150.0f });
    auto* list = panel->createChild<ListBox>();
    list->setRect({ 10.0f, 260.0f, 200.0f, 30.0f });
    list->addItem("only");
    auto* clock = panel->createChild<TimePicker>();
    clock->setRect({ 220.0f, 260.0f, 150.0f, 30.0f });
    clock->setTime(10, 0);
    assert(app.setStage("disabledfields", TransitionType::None));

    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
    DrawData data;
    app.paint(data);
    auto click = [&app, &io](float x, float y)
    {
        io.mouseX = x;
        io.mouseY = y;
        app.update(io);
        io.mouseDown[0] = true;
        app.update(io);
        io.mouseDown[0] = false;
        app.update(io);
    };
    auto type = [&app, &io](const char* text)
    {
        for (const char* c = text; *c; ++c) io.inputChars.push_back(static_cast<uint32_t>(*c));
        app.update(io);
        io.inputChars.clear();
        IO::KeyEvent backspace;
        backspace.key = Key::Backspace;
        io.keyEvents.push_back(backspace);
        app.update(io);
        io.keyEvents.clear();
    };

    field->setText("keep");
    field->setEnabled(false);
    assert(!field->isEnabledInTree());
    click(100.0f, 24.0f);
    assert(app.focused() != field);
    app.setFocused(field);
    type("x");
    assert(field->text() == "keep");

    field->setEnabled(true);
    panel->setEnabled(false);
    assert(field->isEnabled() && !field->isEnabledInTree());
    click(100.0f, 24.0f);
    assert(app.focused() != field);
    const float plusX = 10.0f + 200.0f - 3.0f;
    click(plusX, 64.0f);
    io.mouseWheelY = 1.0f;
    app.update(io);
    io.mouseWheelY = 0.0f;
    assert(spin->value() == 50.0f);
    click(100.0f, 150.0f);
    assert(app.focused() != edit);
    app.setFocused(edit);
    type("y");
    assert(edit->text().empty());
    click(50.0f, 270.0f);
    assert(list->selectedIndex() == -1);
    app.setFocused(clock);
    IO::KeyEvent up;
    up.key = Key::Up;
    io.keyEvents.push_back(up);
    app.update(io);
    io.keyEvents.clear();
    assert(clock->time().hour == 10);

    panel->setEnabled(true);
    field->setText("");
    click(100.0f, 24.0f);
    assert(app.focused() == field);
    type("ab");
    assert(field->text() == "a");
    click(plusX, 64.0f);
    assert(spin->value() == 51.0f);
    click(100.0f, 24.0f);
    panel->setEnabled(false);
    assert(app.focused() == nullptr);

    auto painted = [&app](const Color& color)
    {
        GetDrawList().clear();
        DrawData frame;
        app.paint(frame);
        size_t count = 0;
        for (const DrawPass& pass: frame.passes)
            if (pass.list)
                for (const DrawVertex& vertex: pass.list->vertices())
                    if (vertex.color.r == color.r && vertex.color.g == color.g &&
                            vertex.color.b == color.b && vertex.color.a == color.a)
                        ++count;
        return count;
    };
    const Theme& theme = Theme::instance();
    const Color fill = theme.panelColor;
    edit->setVisible(false);
    const size_t fieldOff = painted(fill);
    panel->setEnabled(true);
    assert(fieldOff > painted(fill));
    edit->setVisible(true);
    field->setVisible(false);
    const size_t editOn = painted(fill);
    panel->setEnabled(false);
    assert(painted(fill) > editOn);
}

// Texto que não é UTF-8 válido (um nome em Latin-1, uma sequência cortada no
// fim) conta um carácter por byte inválido, os offsets nunca passam do fim, e
// escrever ou partir a linha no fim desse texto não aborta.
static void test_invalid_utf8_stays_inside_the_text()
{
    using namespace ig::retained;
    const String latin1("Bal\xE3o");
    assert(utf8Length(latin1) == 5);
    assert(utf8ByteOffset(latin1, 5) == latin1.size());
    const String truncated("ab\xE2\x82");
    assert(utf8Length(truncated) == 4);
    assert(utf8ByteOffset(truncated, 9) == truncated.size());
    assert(utf8Length(String("\xF0")) == 1);
    assert(utf8Length(String("a\xC3\xA3")) == 2);
    int length = 0;
    assert(utf8Decode("\xC3\xA3", length) == 0xE3u && length == 2);
    assert(utf8Decode("\xE3o", length) == 0xFFFDu && length == 1);

    TextInput input;
    input.setText(latin1);
    KeyEvent end;
    end.key = Key::End;
    input.onKeyPress(end);
    KeyEvent typed;
    typed.text[0] = 'x';
    input.onTextInput(typed);
    assert(input.text() == "Bal\xE3ox");

    TextEdit edit;
    edit.setText("Bal\xE3o\nab\xE2\x82");
    edit.setSelection({ 1, 0 }, { 1, 4 });
    assert(edit.selectedText() == "ab\xE2\x82");
    edit.setCursorPos(0, 5);
    KeyEvent enter;
    enter.key = Key::Return;
    edit.onKeyPress(enter);
    assert(edit.lineAt(0) == latin1);
    assert(edit.lineAt(1).empty());
}

// O TextEdit com word-wrap refaz as linhas visuais depois de qualquer edição:
// juntar duas linhas com Backspace, apagar uma seleção de várias linhas e
// apagar dentro de uma linha que deixa de precisar de quebra. O clique usa as
// linhas visuais, por isso cai onde o texto está de facto.
static void test_text_edit_rewraps_after_every_edit()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("rewrap");
    auto* screen = stage->createChild<Widget>();
    auto* edit = screen->createChild<TextEdit>();
    edit->setRect({ 0.0f, 0.0f, 200.0f, 200.0f });
    edit->setShowLineNumbers(false);
    edit->setWordWrap(true);
    assert(app.setStage("rewrap", TransitionType::None));

    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
    auto paint = [&app, font]()
    {
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
    };
    const Theme& theme = Theme::instance();
    const float rowHeight = theme.fontSize + theme.lineSpacing;
    auto clickAt = [edit](float x, float y)
    {
        MouseEvent press;
        press.button = 0;
        press.clickCount = 1;
        press.localX = x;
        press.localY = y;
        edit->onMousePress(press);
        MouseEvent release = press;
        edit->onMouseRelease(release);
        return edit->cursorPos();
    };
    auto key = [edit](int code)
    {
        KeyEvent press;
        press.key = code;
        edit->onKeyPress(press);
    };

    edit->setText("abc\ndefgh\nijklmnop");
    paint();
    edit->setCursorPos(1, 0);
    key(Key::Backspace);
    paint();
    TextEdit::TextPos endOfFirst{ 0, 8 };
    assert(clickAt(190.0f, rowHeight * 0.5f) == endOfFirst);

    edit->setText("abc\ndefgh\nij");
    paint();
    edit->setSelection({ 0, 1 }, { 1, 2 });
    key(Key::Delete);
    paint();
    TextEdit::TextPos endOfJoined{ 0, 4 };
    assert(clickAt(190.0f, rowHeight * 0.5f) == endOfJoined);

    edit->setText(String(40, 'a'));
    paint();
    TextEdit::TextPos secondRow = clickAt(1.0f, rowHeight * 1.5f);
    assert(secondRow.line == 0 && secondRow.col > 0);
    edit->setCursorPos(0, 40);
    for (int i = 0; i < 35; ++i) key(Key::Backspace);
    paint();
    TextEdit::TextPos start{ 0, 0 };
    assert(clickAt(1.0f, rowHeight * 1.5f) == start);

    edit->setText(String(40, 'a'));
    paint();
    edit->setCursorPos(0, 0);
    for (int i = 0; i < 35; ++i) key(Key::Delete);
    paint();
    assert(clickAt(1.0f, rowHeight * 1.5f) == start);
}

// Depois de largar o botão, o hover volta a ser o widget de cima: num
// FloatWindow por cima da stage, o botão da janela continua com o hover e o da
// stage por baixo não o ganha.
static void test_hover_after_a_click_stays_on_the_float()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("floathover");
    auto* screen = stage->createChild<Widget>();
    auto* below = screen->createChild<Button>("Below");
    below->setRect({ 0.0f, 0.0f, 800.0f, 600.0f });
    assert(app.setStage("floathover", TransitionType::None));
    auto* window = app.addFloat<FloatWindow>("Above");
    window->setFloatPos(100.0f, 100.0f);
    window->setFloatSize(300.0f, 200.0f);
    auto* above = window->setContent<Button>("Above");

    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
    DrawData data;
    app.paint(data);
    io.mouseX = 250.0f;
    io.mouseY = 200.0f;
    app.update(io);
    assert(above->isHovered() && !below->isHovered());
    io.mouseDown[0] = true;
    app.update(io);
    io.mouseDown[0] = false;
    app.update(io);
    assert(above->isHovered());
    assert(!below->isHovered());
    app.removeFloat(window);
    app.update(io);
}

// Fechar e reabrir um Collapsible não muda o tamanho dos filhos (um Canvas, que
// usa o sizeHint por omissão, voltava com altura 0). Fechado, mesmo esticado
// pelo pai, os filhos não recebem cliques nem o Tab.
static void test_collapsible_keeps_its_children_while_closed()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("collapsible");
    auto* screen = stage->createChild<Widget>();
    auto* section = screen->createChild<Collapsible>("Section", true);
    section->setRect({ 0.0f, 0.0f, 300.0f, 400.0f });
    auto* canvas = section->createChild<Canvas>();
    canvas->setRect({ 0.0f, 0.0f, 280.0f, 80.0f });
    auto* field = section->createChild<TextInput>();
    field->setRect({ 0.0f, 0.0f, 280.0f, 28.0f });
    assert(app.setStage("collapsible", TransitionType::None));

    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
    DrawData data;
    app.paint(data);
    section->layout();
    assert(canvas->rect().h == 80.0f);
    const Rect fieldRect = field->absoluteRect();
    const float fieldHeight = field->rect().h;

    section->setExpanded(false);
    section->layout();
    section->setRect({ 0.0f, 0.0f, 300.0f, 400.0f });
    io.mouseX = fieldRect.x + 10.0f;
    io.mouseY = fieldRect.y + 10.0f;
    app.update(io);
    io.mouseDown[0] = true;
    app.update(io);
    io.mouseDown[0] = false;
    app.update(io);
    assert(app.focused() != field);
    app.setFocused(nullptr);
    IO::KeyEvent tab;
    tab.key = Key::Tab;
    for (int i = 0; i < 3; ++i)
    {
        io.keyEvents.push_back(tab);
        app.update(io);
        io.keyEvents.clear();
        assert(app.focused() != field);
    }

    section->setExpanded(true);
    section->layout();
    assert(canvas->rect().h == 80.0f);
    assert(field->rect().h == fieldHeight);
}

// Pôr no BorderLayout o widget que já lá está não o apaga, e mudá-lo de região
// deixa a antiga vazia em vez de ficar com um ponteiro para ele.
static void test_border_layout_keeps_a_widget_set_again()
{
    using namespace ig::retained;
    BorderLayout border;
    border.setRect({ 0.0f, 0.0f, 400.0f, 300.0f });
    auto* centre = border.set<Label>(BorderRegion::Center, "centre");
    border.setWidget(BorderRegion::Center, centre);
    assert(border.get(BorderRegion::Center) == centre);
    assert(border.children().size() == 1u);
    border.layout();
    assert(centre->text() == "centre");

    border.setWidget(BorderRegion::Left, centre);
    assert(border.get(BorderRegion::Center) == nullptr);
    assert(border.get(BorderRegion::Left) == centre);
    auto* other = border.set<Label>(BorderRegion::Center, "other");
    assert(border.children().size() == 2u);
    border.layout();
    assert(centre->text() == "centre" && other->text() == "other");
}

namespace
{
struct CountsKeys : ig::retained::Widget
{
    CountsKeys()
    {
        acceptsFocus_ = true;
    }
    void onKeyPress(ig::retained::KeyEvent&) override
    {
        ++presses;
    }
    int presses = 0;
};
} // namespace

// O F1 chega aos widgets como qualquer tecla; só liga o overlay de debug numa
// aplicação que escolha essa tecla com setDebugLayoutKey.
static void test_debug_layout_key_is_opt_in()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("debugkey");
    auto* target = stage->createChild<CountsKeys>();
    assert(app.setStage("debugkey", TransitionType::None));
    app.setFocused(target);

    IO io;
    IO::KeyEvent f1;
    f1.scancode = 58;
    io.keyEvents.push_back(f1);
    app.update(io);
    assert(target->presses == 1);
    assert(!app.debugLayout());

    app.setDebugLayoutKey(58);
    app.update(io);
    assert(target->presses == 1);
    assert(app.debugLayout());
    app.update(io);
    assert(!app.debugLayout());
    app.setDebugLayoutKey(0);
}

// Tab durante a edição de uma célula da DataGrid guarda-a e passa à seguinte
// que se pode editar (salta colunas só de leitura e passa à linha de baixo);
// Shift+Tab volta para trás.
static void test_data_grid_tab_moves_the_edit()
{
    using namespace ig::retained;
    DataGrid grid;
    grid.setRect({ 0.0f, 0.0f, 400.0f, 200.0f });
    grid.addColumn("a", 100.0f);
    grid.addColumn("b", 100.0f);
    grid.addColumn("c", 100.0f);
    grid.setColumnReadOnly(1, true);
    grid.addRow({ "a0", "b0", "c0" });
    grid.addRow({ "a1", "b1", "c1" });

    auto pressCell = [&grid](float x, float y)
    {
        MouseEvent press;
        press.button = 0;
        press.x = x;
        press.y = y;
        grid.onMousePress(press);
        MouseEvent release = press;
        grid.onMouseRelease(release);
    };
    auto key = [&grid](int code, bool shift)
    {
        KeyEvent press;
        press.key = code;
        press.shift = shift;
        grid.onKeyPress(press);
    };
    auto type = [&grid](char c)
    {
        KeyEvent text;
        text.text[0] = c;
        grid.onTextInput(text);
    };

    pressCell(50.0f, 40.0f);
    pressCell(50.0f, 40.0f);
    type('1');
    key(Key::Tab, false);
    type('2');
    key(Key::Tab, false);
    type('3');
    key(Key::Tab, true);
    type('4');
    key(Key::Return, false);
    assert(grid.cell(0, 0) == "a01");
    assert(grid.cell(0, 1) == "b0");
    assert(grid.cell(0, 2) == "c024");
    assert(grid.cell(1, 0) == "a13");

    pressCell(50.0f, 10.0f);
    pressCell(50.0f, 10.0f);
    pressCell(50.0f, 40.0f);
    pressCell(50.0f, 40.0f);
    key(Key::Tab, false);
    key(Key::Tab, false);
    type('5');
    key(Key::Return, false);
    assert(grid.cell(0, 0) == "a015");
    assert(grid.cell(1, 0) == "a13");
}

// Remover uma tab à esquerda da ativa mantém a mesma tab ativa (o índice desce)
// e avisa com currentChanged; remover a ativa também avisa, mesmo quando o
// índice fica igual.
static void test_removing_a_tab_reports_the_current_one()
{
    using namespace ig::retained;
    TabLayout tabs;
    tabs.addTab<Label>("one", "one");
    auto* second = tabs.addTab<Label>("two", "two");
    auto* third = tabs.addTab<Label>("three", "three");
    tabs.setCurrentIndex(2);
    ct::Vector<int> reported;
    tabs.currentChanged.connect([&reported](int index) { reported.push_back(index); });

    tabs.removeTab(0);
    assert(tabs.currentIndex() == 1 && tabs.currentWidget() == third);
    assert(reported.size() == 1u && reported[0] == 1);

    tabs.setCurrentIndex(0);
    tabs.removeTab(0);
    assert(tabs.currentIndex() == 0 && tabs.currentWidget() == third);
    assert(reported.size() == 3u && reported[2] == 0);
    (void) second;

    tabs.removeTab(0);
    assert(tabs.currentIndex() == -1 && tabs.currentWidget() == nullptr);
    assert(reported.size() == 4u && reported[3] == -1);

    TabLayout middle;
    middle.addTab<Label>("a", "a");
    middle.addTab<Label>("b", "b");
    auto* active = middle.addTab<Label>("c", "c");
    middle.addTab<Label>("d", "d");
    middle.setCurrentIndex(2);
    middle.removeTab(0);
    assert(middle.currentIndex() == 1 && middle.currentWidget() == active);
}

// O duplo clique do TextEdit seleciona a palavra inteira e o triplo a linha
// inteira, contando em caracteres e não em bytes ("olá mundo" selecionava
// "undo").
static void test_text_edit_word_and_line_clicks_count_characters()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("wordclick");
    auto* screen = stage->createChild<Widget>();
    auto* edit = screen->createChild<TextEdit>();
    edit->setRect({ 0.0f, 0.0f, 400.0f, 200.0f });
    edit->setShowLineNumbers(false);
    edit->setText("olá mundo");
    assert(app.setStage("wordclick", TransitionType::None));

    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
    DrawData data;
    app.paint(data, font);
    const Theme& theme = Theme::instance();
    const float rowY = (theme.fontSize + theme.lineSpacing) * 0.5f;
    auto clicks = [edit, rowY](float x, int count)
    {
        MouseEvent press;
        press.button = 0;
        press.clickCount = count;
        press.localX = x;
        press.localY = rowY;
        edit->onMousePress(press);
        MouseEvent release = press;
        edit->onMouseRelease(release);
    };

    const float mundoX = theme.textEditPadding + app.textWidth("olá ") + 1.0f;
    clicks(mundoX, 2);
    assert(edit->selectedText() == "mundo");
    clicks(theme.textEditPadding + 1.0f, 2);
    assert(edit->selectedText() == "olá");
    clicks(mundoX, 3);
    assert(edit->selectedText() == "olá mundo");
    TextEdit::TextPos lineEnd{ 0, 9 };
    assert(edit->cursorPos() == lineEnd);

    CodeEditor code;
    code.setText("olá mundo");
    code.setCursorPos(0, 1);
    code.addCursorForNextOccurrence();
    assert(code.selectedText() == "olá");
}

// A DataGrid, a TreeGrid, a PropertyGrid e a ConsoleWidget apagam e andam um
// carácter inteiro de cada vez: um Backspace sobre "ç" não deixa meio carácter.
static void test_cell_editors_work_on_whole_characters()
{
    using namespace ig::retained;
    auto key = [](Widget& widget, int code)
    {
        KeyEvent press;
        press.key = code;
        widget.onKeyPress(press);
    };
    auto type = [](Widget& widget, const char* text)
    {
        KeyEvent typed;
        std::memcpy(typed.text, text, std::strlen(text) + 1);
        widget.onTextInput(typed);
    };
    auto doublePress = [](Widget& widget, float x, float y)
    {
        for (int i = 0; i < 2; ++i)
        {
            MouseEvent press;
            press.button = 0;
            press.x = x;
            press.y = y;
            widget.onMousePress(press);
            MouseEvent release = press;
            widget.onMouseRelease(release);
        }
    };

    DataGrid grid;
    grid.setRect({ 0.0f, 0.0f, 400.0f, 200.0f });
    grid.addColumn("a", 100.0f);
    grid.addRow({ "a\xC3\xA7" });
    grid.addRow({ "\xC3\xA7"
                  "b" });
    doublePress(grid, 50.0f, 40.0f);
    key(grid, Key::Backspace);
    key(grid, Key::Return);
    assert(grid.cell(0, 0) == "a");
    doublePress(grid, 50.0f, 64.0f);
    key(grid, Key::Left);
    key(grid, Key::Left);
    key(grid, Key::Right);
    type(grid, "x");
    key(grid, Key::Home);
    key(grid, Key::Delete);
    key(grid, Key::Return);
    assert(grid.cell(1, 0) == "xb");

    TreeGrid tree;
    tree.setRect({ 0.0f, 0.0f, 400.0f, 200.0f });
    tree.addColumn("name", 100.0f);
    tree.addColumn("value", 100.0f);
    TreeGrid::Node* node = tree.addRoot({ "n", "a\xC3\xA7" });
    doublePress(tree, 150.0f, 40.0f);
    key(tree, Key::Left);
    key(tree, Key::Delete);
    key(tree, Key::Return);
    assert(node->cells[1] == "a");

    PropertyGrid props;
    props.setRect({ 0.0f, 0.0f, 400.0f, 180.0f });
    String value;
    props.addString("Name", "a\xC3\xA7", [&value](const String& text) { value = text; });
    MouseEvent press;
    press.button = 0;
    press.x = 180.0f;
    press.y = 12.0f;
    props.onMousePress(press);
    props.onMouseRelease(press);
    key(props, Key::End);
    key(props, Key::Backspace);
    key(props, Key::Return);
    assert(value == "a");

    ConsoleWidget console;
    String command;
    console.onCommand.connect([&command](const String& text) { command = text; });
    KeyEvent find;
    find.key = Key::F;
    find.ctrl = true;
    console.onKeyPress(find);
    type(console, "a\xC3\xA7");
    key(console, Key::Backspace);
    assert(console.searchText() == "a");
    console.onKeyPress(find);
    type(console, "a\xC3\xA7");
    key(console, Key::Backspace);
    key(console, Key::Return);
    assert(command == "a");
}

// O DatePicker só guarda datas que existem: um mês fora de 1-12 ou um dia
// para lá do fim do mês ficam no limite (o calendário indexava tabelas com o
// mês tal como vinha).
static void test_date_picker_keeps_real_dates()
{
    using namespace ig::retained;
    DatePicker picker;
    picker.setDate(2026, 13, 40);
    assert(picker.date().month == 12 && picker.date().day == 31);
    picker.setDate(2024, 2, 30);
    assert(picker.date().month == 2 && picker.date().day == 29);
    picker.setDate(2026, 0, 0);
    assert(picker.date().month == 1 && picker.date().day == 1);
    int changes = 0;
    picker.onDateChanged.connect([&changes](int, int, int) { ++changes; });
    picker.setDate(2026, -5, -5);
    assert(changes == 0);
}

// Uma janela gravada num ecrã maior volta alcançável: cabe no ecrã atual e a
// barra de título fica à vista. O mesmo quando o ecrã encolhe depois.
static void test_restored_windows_stay_on_screen()
{
    using namespace ig::retained;
    const char* path = "igui_window_offscreen_test.ini";
    ct::Ini saved;
    saved.set("Far", "pos_x", 3000.0);
    saved.set("Far", "pos_y", 2000.0);
    saved.set("Far", "size_w", 1200.0);
    saved.set("Far", "size_h", 900.0);
    assert(saved.save(path));

    WidgetApp& app = WidgetApp::instance();
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
    app.setWindowStatePath(path);
    assert(app.loadWindowState());
    auto* far = app.addFloat<FloatWindow>("Far");
    auto reachable = [far](float width, float height)
    {
        return far->floatW() <= width && far->floatH() <= height && far->floatY() >= 0.0f &&
               far->floatY() + far->titleBarHeight() <= height &&
               far->floatX() + far->floatW() >= 40.0f && far->floatX() <= width - 40.0f;
    };
    assert(reachable(800.0f, 600.0f));

    far->setFloatPos(700.0f, 500.0f);
    io.displayWidth = 400.0f;
    io.displayHeight = 300.0f;
    app.update(io);
    assert(reachable(400.0f, 300.0f));

    app.removeFloat(far);
    app.setWindowStatePath(String());
    std::remove(path);
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
}

// Os breadcrumbs partem o caminho pela raiz que ele tem ("/", uma letra de
// drive ou um caminho UNC), com o caminho de cada pasta, e um nome longo é
// cortado num carácter inteiro.
static void test_file_dialog_breadcrumbs_follow_the_path_root()
{
    namespace fs = ig::retained::FileSystem;
    using ig::retained::String;
    auto names = [](const ct::Vector<fs::PathCrumb>& crumbs)
    {
        String joined;
        for (const fs::PathCrumb& crumb: crumbs) joined += crumb.name + "|" + crumb.path + ";";
        return joined;
    };
    assert(names(fs::pathCrumbs("/home/me")) == "/|/;home|/home;me|/home/me;");
    assert(names(fs::pathCrumbs("/")) == "/|/;");
    assert(names(fs::pathCrumbs("D:\\Users\\me")) == "D:|D:/;Users|D:/Users;me|D:/Users/me;");
    assert(names(fs::pathCrumbs("D:/")) == "D:|D:/;");
    assert(names(fs::pathCrumbs("\\\\server\\share\\docs")) ==
            "//server/share|//server/share;docs|//server/share/docs;");

    const String root = fs::joinPath(fs::currentDir(), "igui_crumbs_test");
    const String deep =
            fs::joinPath(root, "a\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7"
                               "\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7");
    assert(fs::createDir(deep));
    ig::retained::FileDialog dialog;
    dialog.setRect({ 0.0f, 0.0f, 700.0f, 480.0f });
    dialog.layout();
    dialog.setPath(deep);
    dialog.layout();
    int checked = 0;
    ct::Vector<ig::retained::Widget*> pending;
    pending.push_back(&dialog);
    while (!pending.empty())
    {
        ig::retained::Widget* widget = pending.back();
        pending.pop_back();
        for (ig::retained::Widget* child: widget->children()) pending.push_back(child);
        auto* button = ig::retained::widget_cast<ig::retained::Button>(widget);
        if (!button || button->text().find("\xC3") == String::npos) continue;
        const String& label = button->text();
        for (size_t i = 0; i < label.size(); i += ig::retained::utf8CharLen(label.c_str() + i))
        {
            int length = 0;
            assert(ig::retained::utf8Decode(label.c_str() + i, length) != 0xFFFDu);
        }
        ++checked;
    }
    assert(checked == 1);
    std::remove(deep.c_str());
    std::remove(root.c_str());
}

// Clarear ou escurecer uma cor fica nos limites de cada canal: com o tema
// claro (painel 250) as barras do FileDialog não dão a volta para preto.
static void test_shaded_colors_stay_in_range()
{
    using namespace ig::retained;
    assert(ig::Color(250, 10, 128, 7).Shifted(15) == ig::Color(255, 25, 143, 7));
    assert(ig::Color(3, 250, 128, 255).Shifted(-5) == ig::Color(0, 245, 123, 255));

    Theme& theme = Theme::instance();
    const Theme saved = theme;
    theme = Theme::light();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("lightdialog");
    assert(app.setStage("lightdialog", TransitionType::None));
    auto* dialog = app.addFloat<FileDialog>("Open");
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
    auto darkVertices = [&app]()
    {
        GetDrawList().clear();
        DrawData data;
        app.paint(data);
        size_t dark = 0;
        for (const DrawPass& pass: data.passes)
            if (pass.list)
                for (const DrawVertex& vertex: pass.list->vertices())
                    if (vertex.color.r < 20 && vertex.color.g < 20 && vertex.color.b < 20) ++dark;
        return dark;
    };
    assert(darkVertices() == 0);
    app.removeFloat(dialog);
    app.update(io);

    theme.scrollbarThumb = ig::Color(250, 250, 250, 100);
    auto* screen = stage->createChild<Widget>();
    auto* list = screen->createChild<ListBox>();
    list->setRect({ 0.0f, 0.0f, 200.0f, 100.0f });
    for (int i = 0; i < 50; ++i) list->addItem("item");
    auto* assets = screen->createChild<AssetBrowser>();
    assets->setRect({ 300.0f, 0.0f, 400.0f, 300.0f });
    AssetItem white;
    white.name = "white";
    white.thumbColor = ig::Color(250, 250, 250, 255);
    assets->addItem(white);
    int picked = 0;
    assets->onSelect.connect([&picked](AssetItem) { ++picked; });
    darkVertices();
    for (float y = 30.0f; y < 300.0f && picked == 0; y += 10.0f)
        for (float x = 310.0f; x < 700.0f && picked == 0; x += 10.0f)
        {
            MouseEvent press;
            press.button = 0;
            press.x = x;
            press.y = y;
            assets->onMousePress(press);
        }
    assert(picked == 1);
    io.mouseX = 196.0f;
    io.mouseY = 50.0f;
    app.update(io);
    assert(darkVertices() == 0);
    theme = saved;
}

// O fadeOut e o slideIn param quando o widget que animam desaparece, em vez de
// lhe tocarem depois de apagado.
static void test_widget_animations_stop_with_their_widget()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("animgone");
    auto* faded = stage->createChild<Label>("faded");
    auto* slid = stage->createChild<Label>("slid");
    slid->setRect({ 0.0f, 0.0f, 100.0f, 20.0f });
    auto* kept = stage->createChild<Label>("kept");
    kept->setRect({ 0.0f, 50.0f, 100.0f, 20.0f });
    assert(app.setStage("animgone", TransitionType::None));

    Animator& animator = Animator::instance();
    const int before = animator.count();
    Animator::fadeOut(faded, 0.5f);
    Animator::slideIn(slid, 100.0f, 0.0f, 0.5f);
    Animator::slideIn(kept, 100.0f, 0.0f, 0.5f);
    IO io;
    io.deltaTime = 0.01f;
    app.update(io);
    assert(animator.count() == before + 3);

    stage->removeChild(faded);
    stage->removeChild(slid);
    app.update(io);
    assert(animator.count() == before + 1);
    for (int i = 0; i < 60; ++i) app.update(io);
    assert(animator.count() == before);
    assert(kept->rect().x == 0.0f);
}

// O TextEdit sem word-wrap rola na horizontal: o End leva a vista ao fim de uma
// linha comprida, o Home volta ao princípio, e a roda horizontal (ou Shift+roda)
// também rola. Com os números de linha à vista, o texto rolado não passa para a
// coluna dos números: o que se desenha com o clip dessa coluna não se mexe.
static void test_text_edit_scrolls_sideways_without_wrap()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("sideways");
    auto* screen = stage->createChild<Widget>();
    auto* edit = screen->createChild<TextEdit>();
    edit->setRect({ 0.0f, 0.0f, 200.0f, 120.0f });
    edit->setText(String(100, 'a'));
    assert(app.setStage("sideways", TransitionType::None));

    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
    auto gutterVertices = [&app, font]()
    {
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
        ct::Vector<float> xs;
        for (const DrawPass& pass: data.passes)
        {
            if (!pass.list) continue;
            const DrawList& list = *pass.list;
            for (const DrawCmd& cmd: list.commands())
            {
                if (cmd.clip.x != 0.0f || cmd.clip.w != 200.0f) continue;
                for (uint32_t i = 0; i < cmd.indexCount; ++i)
                    xs.push_back(list.vertices()[list.indices()[cmd.indexOffset + i]].x);
            }
        }
        return xs;
    };
    const ct::Vector<float> unscrolled = gutterVertices();
    const Theme& theme = Theme::instance();
    const float rowY = (theme.fontSize + theme.lineSpacing) * 0.5f;
    auto clickColumn = [edit, rowY](float x)
    {
        MouseEvent press;
        press.button = 0;
        press.clickCount = 1;
        press.localX = x;
        press.localY = rowY;
        edit->onMousePress(press);
        MouseEvent release = press;
        edit->onMouseRelease(release);
        return edit->cursorPos().col;
    };
    auto key = [edit](int code)
    {
        KeyEvent press;
        press.key = code;
        edit->onKeyPress(press);
    };

    const int startCol = clickColumn(190.0f);
    assert(startCol < 40);
    key(Key::End);
    const ct::Vector<float> scrolled = gutterVertices();
    assert(scrolled.size() == unscrolled.size());
    for (size_t i = 0; i < scrolled.size(); ++i) assert(scrolled[i] == unscrolled[i]);
    assert(clickColumn(190.0f) >= 90);
    key(Key::Home);
    gutterVertices();
    assert(clickColumn(190.0f) == startCol);

    MouseEvent wheel;
    wheel.scrollX = -3.0f;
    edit->onMouseScroll(wheel);
    gutterVertices();
    assert(clickColumn(190.0f) > startCol);
    MouseEvent back;
    back.scrollY = 3.0f;
    back.shift = true;
    edit->onMouseScroll(back);
    gutterVertices();
    assert(clickColumn(190.0f) == startCol);

    auto* code = screen->createChild<CodeEditor>();
    code->setRect({ 0.0f, 200.0f, 200.0f, 120.0f });
    code->setShowMinimap(false);
    code->setShowWhitespace(true);
    code->setShowIndentGuides(true);
    String spaced;
    for (int i = 0; i < 40; ++i) spaced += "    a b";
    code->setText(spaced);
    const ct::Vector<float> codeUnscrolled = gutterVertices();
    KeyEvent end;
    end.key = Key::End;
    code->onKeyPress(end);
    const ct::Vector<float> codeScrolled = gutterVertices();
    assert(codeScrolled.size() == codeUnscrolled.size());
    for (size_t i = 0; i < codeScrolled.size(); ++i) assert(codeScrolled[i] == codeUnscrolled[i]);
}

// As repetições colapsadas de uma mensagem contam para o total do nível, e o
// corte das linhas antigas tira exatamente o que elas somaram.
static void test_console_counts_collapsed_repeats()
{
    using namespace ig::retained;
    ConsoleWidget console;
    console.setCollapseDuplicates(true);
    console.setMaxLines(2);
    for (int i = 0; i < 3; ++i) console.log(LogLevel::Error, "disk full");
    assert(console.messageCount(LogLevel::Error) == 3);
    console.log(LogLevel::Info, "a");
    console.log(LogLevel::Info, "b");
    assert(console.messageCount(LogLevel::Error) == 0);
    assert(console.messageCount(LogLevel::Info) == 2);
}

// Os eventos globais: um handler por id só ouve esse widget, o catch-all ouve
// todos, o do id corre antes do catch-all, e um handler pode registar outro
// durante o evento (o novo só conta no seguinte).
static void test_global_events_reach_their_handlers()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    Widget named;
    named.setId("events-named");
    Widget other;
    other.setId("events-other");
    Widget anonymous;
    String order;
    app.on("events-test", "events-named", [&order](Widget*) { order += "n"; });
    app.onAny("events-test",
            [&order, &app](Widget* w)
            {
                order += w->id().empty() ? "a" : "*";
                if (order.size() == 2)
                    app.onAny("events-test", [&order](Widget*) { order += "+"; });
            });

    app.fireEvent("events-test", &named);
    assert(order == "n*");
    app.fireEvent("events-test", &other);
    assert(order == "n**+");
    app.fireEvent("events-test", &anonymous);
    assert(order == "n**+a+");
    app.fireEvent("events-other-name", &named);
    assert(order == "n**+a+");
}

// As linhas visuais do word-wrap: com palavras, tabs e UTF-8, cada linha visual
// começa na coluna esperada, e editar uma linha não muda as outras.
static void test_text_edit_wrap_rows_are_stable()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("wraprows");
    auto* screen = stage->createChild<Widget>();
    auto* edit = screen->createChild<TextEdit>();
    edit->setRect({ 0.0f, 0.0f, 160.0f, 400.0f });
    edit->setShowLineNumbers(false);
    edit->setWordWrap(true);
    edit->setText("the quick\tbrown fox jumps over the lazy dog\n"
                  "a\xC3\xA7\xC3\xA3o r\xC3\xA1pida sobre o c\xC3\xA3o pregui\xC3\xA7oso e mais\n"
                  "nospacesatallinthisverylonglinethatmustbreakanyway");
    assert(app.setStage("wraprows", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
    auto paint = [&app, font]()
    {
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
    };
    paint();
    const Theme& theme = Theme::instance();
    const float rowHeight = theme.fontSize + theme.lineSpacing;
    auto rowStarts = [edit, rowHeight]()
    {
        String starts;
        for (int row = 0; row < 14; ++row)
        {
            MouseEvent press;
            press.button = 0;
            press.clickCount = 1;
            press.localX = 1.0f;
            press.localY = rowHeight * (static_cast<float>(row) + 0.5f);
            edit->onMousePress(press);
            MouseEvent release = press;
            edit->onMouseRelease(release);
            const TextEdit::TextPos pos = edit->cursorPos();
            starts += String::number(pos.line) + ":" + String::number(pos.col) + " ";
        }
        return starts;
    };
    const String before = rowStarts();
    assert(before == "0:0 0:20 0:40 1:0 1:20 1:37 2:0 2:21 2:41 2:41 2:41 2:41 2:41 2:41 ");

    edit->setCursorPos(2, 0);
    KeyEvent typed;
    typed.text[0] = 'x';
    edit->onTextInput(typed);
    paint();
    const String after = rowStarts();
    assert(after.substr(0, after.find("2:")) == before.substr(0, before.find("2:")));

    edit->setRect({ 0.0f, 0.0f, 600.0f, 400.0f });
    paint();
    const String wide = rowStarts();
    assert(wide.substr(0, 8) == "0:0 1:0 ");
}

namespace
{
struct BigContent : ig::retained::Widget
{
    Vec2f sizeHint() const override
    {
        return { 1000.0f, 1000.0f };
    }
};
} // namespace

// Com as duas barras à vista, a roda chega ao fim do conteúdo nos dois eixos:
// o limite conta com a barra do outro eixo, como a própria barra conta.
// A altura da linha não espera pela primeira pintura: o scrollToLine logo depois
// do setText leva a vista à linha pedida, e depois de mudar o zoom usa logo a
// altura nova, que a pintura seguinte mantém. Sem altura ainda, mover o cursor
// não rola a vista.
static void test_text_edit_scrolls_to_a_line_before_painting()
{
    using namespace ig::retained;
    String text;
    for (int i = 0; i < 200; ++i) text += "line " + String::number(i) + "\n";
    TextEdit lonely;
    lonely.setRect({ 0.0f, 0.0f, 200.0f, 100.0f });
    lonely.setText(text);
    lonely.scrollToLine(150);
    assert(lonely.firstVisibleLine() == 150);
    assert(lonely.visibleLineCount() > 2);
    TextEdit unplaced;
    unplaced.setText(text);
    KeyEvent down;
    down.key = Key::Down;
    unplaced.onKeyPress(down);
    unplaced.onKeyPress(down);
    assert(unplaced.cursorPos().line == 2);
    assert(unplaced.firstVisibleLine() == 0);

    FontAtlas atlas;
    assert(atlas.buildDefault());
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("scrolltoline");
    auto* screen = stage->createChild<Widget>();
    auto* edit = screen->createChild<TextEdit>();
    edit->setRect({ 0.0f, 0.0f, 200.0f, 100.0f });
    edit->setText(text);
    assert(app.setStage("scrolltoline", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    auto paint = [&app, &io, font]()
    {
        app.update(io);
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
    };
    paint();
    edit->setFontScale(2.0f);
    edit->scrollToLine(100);
    paint();
    assert(edit->firstVisibleLine() == 100);
}

// Os campos de texto medem os cliques com a fonte e o zoom com que pintam: um
// clique a um quarto do 11.º carácter põe o cursor na coluna 10 num TextEdit
// com zoom 2, num TextInput e num TextEdit com fonte própria (também com os
// números de linha à vista), e o Alt+clique do CodeEditor com fonte própria
// acrescenta um cursor na coluna 10. O End leva a vista ao fim da linha.
static void test_text_fields_hit_test_with_their_paint_font()
{
    using namespace ig::retained;
    FontAtlas atlas;
    atlas.addFontDefault(16.0f);
    atlas.addFontDefault(32.0f);
    assert(atlas.build());
    const Font* font = atlas.font(0);
    const Font* big = atlas.font(1);
    const float smallAdvance = font->findGlyph('a')->advanceX;
    const float bigAdvance = big->findGlyph('a')->advanceX;
    assert(bigAdvance > smallAdvance * 1.5f);
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("hitfont");
    auto* screen = stage->createChild<Widget>();
    auto* zoomed = screen->createChild<TextEdit>();
    zoomed->setRect({ 0.0f, 0.0f, 600.0f, 80.0f });
    zoomed->setShowLineNumbers(false);
    zoomed->setFontScale(2.0f);
    zoomed->setText(String(40, 'a'));
    auto* field = screen->createChild<TextInput>();
    field->setRect({ 0.0f, 100.0f, 600.0f, 40.0f });
    field->setFont(big);
    field->setText(String(40, 'a'));
    auto* edit = screen->createChild<TextEdit>();
    edit->setRect({ 0.0f, 150.0f, 600.0f, 80.0f });
    edit->setShowLineNumbers(false);
    edit->setFont(big);
    edit->setText(String(40, 'a'));
    auto* code = screen->createChild<CodeEditor>();
    code->setRect({ 0.0f, 250.0f, 600.0f, 80.0f });
    code->setShowLineNumbers(false);
    code->setShowFolding(false);
    code->setFont(big);
    code->setText(String(40, 'a'));
    assert(app.setStage("hitfont", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
    GetDrawList().clear();
    DrawData data;
    app.paint(data, font);

    const Theme& theme = Theme::instance();
    auto click = [](Widget* widget, float localX, float localY, bool alt)
    {
        MouseEvent press;
        press.button = 0;
        press.clickCount = 1;
        press.alt = alt;
        press.localX = localX;
        press.localY = localY;
        press.x = widget->absoluteRect().x + localX;
        press.y = widget->absoluteRect().y + localY;
        widget->onMousePress(press);
        MouseEvent release = press;
        widget->onMouseRelease(release);
    };
    const float pad = theme.textEditPadding;
    click(zoomed, pad + smallAdvance * 2.0f * 10.25f, 4.0f, false);
    assert(zoomed->cursorPos().col == 10);
    click(field, theme.padding + bigAdvance * 10.25f, 20.0f, false);
    assert(field->cursorPos() == 10);
    click(edit, pad + bigAdvance * 10.25f, 4.0f, false);
    assert(edit->cursorPos().col == 10);
    edit->setShowLineNumbers(true);
    edit->setText(String(200, 'a'));
    GetDrawList().clear();
    DrawData numbered;
    app.paint(numbered, font);
    const float gutter = big->findGlyph('9')->advanceX * 2.0f + theme.gutterPadding * 2.0f;
    click(edit, gutter + pad + bigAdvance * 10.25f, 4.0f, false);
    assert(edit->cursorPos().col == 10);
    KeyEvent endKey;
    endKey.key = Key::End;
    edit->onKeyPress(endKey);
    GetDrawList().clear();
    DrawData scrolled;
    app.paint(scrolled, font);
    click(edit, 590.0f, 4.0f, false);
    assert(edit->cursorPos().col >= 195);
    code->setCursorPos(0, 30);
    click(code, pad + bigAdvance * 10.25f, 4.0f, true);
    assert(code->cursorCount() == 2);
    assert(code->cursorPosAt(1).col == 10);
}

// Sem word-wrap, a largura do texto (o limite do scroll horizontal) volta a ser
// medida quando o zoom ou o tamanho do tab mudam: o End mostra o fim da linha.
static void test_text_edit_measures_again_after_zoom_and_tab_size()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("remeasure");
    auto* screen = stage->createChild<Widget>();
    auto* edit = screen->createChild<TextEdit>();
    edit->setRect({ 0.0f, 0.0f, 200.0f, 120.0f });
    edit->setShowLineNumbers(false);
    assert(app.setStage("remeasure", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    auto paint = [&app, &io, font]()
    {
        app.update(io);
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
    };
    auto columnAtRightEdge = [edit]()
    {
        MouseEvent press;
        press.button = 0;
        press.clickCount = 1;
        press.localX = 190.0f;
        press.localY = 2.0f;
        edit->onMousePress(press);
        MouseEvent release = press;
        edit->onMouseRelease(release);
        return edit->cursorPos().col;
    };
    auto end = [edit]()
    {
        KeyEvent press;
        press.key = Key::End;
        edit->onKeyPress(press);
    };

    edit->setText(String(300, 'a'));
    paint();
    edit->setFontScale(2.0f);
    paint();
    end();
    paint();
    assert(columnAtRightEdge() >= 295);

    edit->setFontScale(1.0f);
    edit->setText(String(60, '\t') + "x");
    paint();
    edit->setTabSize(8);
    paint();
    end();
    paint();
    assert(columnAtRightEdge() >= 60);
}

// As edições só voltam a medir as linhas que mudaram: depois de cada passo
// (escrever, Enter no meio, no início e no fim de uma linha, apagar e juntar
// linhas com Backspace e Delete, também com linhas vazias, substituir
// uma seleção de várias linhas, colar várias linhas, várias edições sem pintar
// no meio), o TextEdit pinta exatamente o mesmo que com o mesmo texto medido do
// zero, com e sem word-wrap.
static void test_text_edit_wraps_only_the_edited_lines()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    atlas.setTexture(TextureHandle{ 1 });
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("editwrap");
    auto* screen = stage->createChild<Widget>();
    auto* edit = screen->createChild<TextEdit>();
    edit->setRect({ 0.0f, 0.0f, 260.0f, 590.0f });
    assert(app.setStage("editwrap", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    auto snapshot = [&]()
    {
        app.update(io);
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
        ct::Vector<float> values;
        for (const DrawPass& pass: data.passes)
        {
            if (!pass.list) continue;
            for (const DrawVertex& vertex: pass.list->vertices())
            {
                values.push_back(vertex.x);
                values.push_back(vertex.y);
                values.push_back(vertex.u);
            }
        }
        return values;
    };
    auto views = [&]()
    {
        KeyEvent top;
        top.key = Key::Home;
        top.ctrl = true;
        edit->onKeyPress(top);
        ct::Vector<float> values = snapshot();
        MouseEvent wheel;
        wheel.scrollX = -1000.0f;
        edit->onMouseScroll(wheel);
        const ct::Vector<float> right = snapshot();
        for (float value: right) values.push_back(value);
        return values;
    };
    auto matchesFresh = [&]()
    {
        const ct::Vector<float> edited = views();
        edit->setText(edit->text());
        const ct::Vector<float> fresh = views();
        if (edited.size() != fresh.size()) return false;
        for (size_t i = 0; i < edited.size(); ++i)
            if (edited[i] != fresh[i]) return false;
        return true;
    };
    auto key = [edit](int code)
    {
        KeyEvent press;
        press.key = code;
        edit->onKeyPress(press);
    };
    auto type = [edit](const char* text)
    {
        for (const char* c = text; *c; ++c)
        {
            KeyEvent typed;
            typed.text[0] = *c;
            edit->onTextInput(typed);
        }
    };

    for (int wrap = 1; wrap >= 0; --wrap)
    {
        edit->setWordWrap(wrap == 1);
        edit->setText("the quick brown fox jumps over the lazy dog again and again\n"
                      "short\n"
                      "a\xC3\xA7\xC3\xA3o r\xC3\xA1pida sobre o c\xC3\xA3o pregui\xC3\xA7oso e "
                      "mais nada\n"
                      "\tindented line with a tab and some words to wrap around\n"
                      "line four is here with enough words to wrap twice in this box\n"
                      "five\n"
                      "six six six six six six six six six six six six six six six six\n"
                      "last");
        snapshot();
        edit->setCursorPos(0, 10);
        type("hello world ");
        assert(matchesFresh());
        edit->setCursorPos(0, 40);
        key(Key::Return);
        assert(edit->lineCount() == 9);
        assert(matchesFresh());
        edit->setCursorPos(1, 0);
        key(Key::Backspace);
        assert(matchesFresh());
        edit->setCursorPos(1, edit->lineAt(1).size());
        key(Key::Delete);
        assert(edit->lineCount() == 7);
        assert(matchesFresh());
        edit->setSelection({ 1, 2 }, { 2, 3 });
        type("X");
        assert(edit->lineCount() == 6);
        assert(matchesFresh());
        edit->insertTextAt({ 1, 2 },
                "abc\ndef ghi jkl mno pqr stu vwx yz abc def ghi jkl mno\nend ");
        assert(edit->lineCount() == 8);
        assert(matchesFresh());

        edit->setCursorPos(4, 4);
        type("more words that make the line wrap one more time ");
        edit->setCursorPos(0, 0);
        key(Key::Return);
        edit->setCursorPos(edit->lineCount() - 1, 0);
        key(Key::Backspace);
        type("tail ");
        key(Key::Backspace);
        key(Key::Backspace);
        edit->setCursorPos(2, 1);
        key(Key::Delete);
        edit->setCursorPos(1, 0);
        key(Key::Tab);
        assert(edit->lineCount() == 8);
        assert(matchesFresh());
        edit->setCursorPos(2, 0);
        key(Key::Return);
        edit->setCursorPos(4, static_cast<int>(edit->lineAt(4).size()));
        key(Key::Return);
        assert(edit->lineCount() == 10);
        assert(matchesFresh());
        auto length = [edit](int line) { return static_cast<int>(edit->lineAt(line).size()); };
        edit->setCursorPos(3, 0);
        key(Key::Backspace);
        assert(edit->lineCount() == 9);
        edit->setCursorPos(4, 0);
        assert(edit->lineAt(4).empty());
        key(Key::Backspace);
        assert(edit->lineCount() == 8);
        assert(matchesFresh());
        edit->setCursorPos(1, length(1));
        key(Key::Return);
        snapshot();
        edit->setCursorPos(1, length(1));
        key(Key::Delete);
        edit->setCursorPos(1, 0);
        key(Key::Return);
        snapshot();
        edit->setCursorPos(1, 0);
        assert(edit->lineAt(1).empty());
        key(Key::Delete);
        assert(edit->lineCount() == 8);
        assert(matchesFresh());
        edit->insertTextAt({ 2, 0 }, "front line\n");
        snapshot();
        edit->insertTextAt({ 4, length(4) }, "\nback");
        assert(edit->lineCount() == 10);
        assert(matchesFresh());
        edit->setSelection({ 1, 0 }, { 3, 0 });
        key(Key::Delete);
        edit->setSelection({ 3, length(3) }, { 5, length(5) });
        key(Key::Delete);
        assert(edit->lineCount() == 6);
        assert(matchesFresh());
        edit->setSelection({ 0, 0 }, { edit->lineCount() - 1, 2 });
        key(Key::Delete);
        assert(edit->lineCount() == 1);
        assert(matchesFresh());
    }
}

// Um clique antes da primeira pintura, ou depois de um setText com mais linhas
// e antes de a pintura voltar a medir, não lê o wrap fora dos limites e põe o
// cursor numa posição válida.
static void test_text_edit_click_before_the_wrap_is_measured()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("clickfirst");
    auto* screen = stage->createChild<Widget>();
    auto* edit = screen->createChild<TextEdit>();
    edit->setRect({ 0.0f, 0.0f, 300.0f, 200.0f });
    edit->setText("first line\nsecond line");
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    auto paint = [&app, &io, font]()
    {
        app.update(io);
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
    };
    paint();
    auto click = [edit](float x, float y)
    {
        MouseEvent press;
        press.button = 0;
        press.clickCount = 1;
        press.localX = x;
        press.localY = y;
        edit->onMousePress(press);
        MouseEvent release = press;
        edit->onMouseRelease(release);
        return edit->cursorPos();
    };
    TextEdit::TextPos pos = click(60.0f, 30.0f);
    assert(pos.line >= 0 && pos.line < edit->lineCount());
    assert(pos.col >= 0 && pos.col <= static_cast<int>(edit->lineAt(pos.line).size()));

    assert(app.setStage("clickfirst", TransitionType::None));
    paint();
    String longer;
    for (int i = 0; i < 400; ++i) longer += "line\n";
    edit->setText(longer);
    pos = click(60.0f, 150.0f);
    assert(pos.line >= 0 && pos.line < edit->lineCount());
    assert(pos.col >= 0 && pos.col <= static_cast<int>(edit->lineAt(pos.line).size()));
}

// Escrever no fim de uma linha com word-wrap, no fundo da vista, até ela passar
// para uma fila nova: depois da pintura, o cursor continua dentro da vista. Um
// scroll pedido depois da tecla (scrollToLine, roda) não é desfeito.
static void test_text_edit_keeps_the_cursor_visible_while_typing_wrapped()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("typewrap");
    auto* screen = stage->createChild<Widget>();
    auto* edit = screen->createChild<TextEdit>();
    const Theme& theme = Theme::instance();
    const float rowHeight = theme.fontSize + theme.lineSpacing;
    edit->setRect({ 0.0f, 0.0f, 200.0f, rowHeight * 4.0f });
    edit->setShowLineNumbers(false);
    edit->setWordWrap(true);
    edit->setText("one\ntwo\nthree\nfour");
    assert(app.setStage("typewrap", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    auto paint = [&app, &io, font]()
    {
        app.update(io);
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
    };
    paint();
    edit->setCursorPos(3, 4);
    for (int i = 0; i < 120; ++i)
    {
        KeyEvent typed;
        typed.text[0] = (i % 6 == 5) ? ' ' : 'w';
        edit->onTextInput(typed);
        paint();
        const int length = static_cast<int>(edit->lineAt(3).size());
        MouseEvent press;
        press.button = 0;
        press.clickCount = 1;
        press.localX = 199.0f;
        press.localY = rowHeight * 3.5f;
        edit->onMousePress(press);
        MouseEvent release = press;
        edit->onMouseRelease(release);
        assert(edit->cursorPos().line == 3);
        assert(edit->cursorPos().col == length);
        edit->setCursorPos(3, length);
    }
    assert(edit->firstVisibleLine() > 0);

    KeyEvent typed;
    typed.text[0] = 'w';
    edit->onTextInput(typed);
    edit->scrollToLine(0);
    paint();
    assert(edit->firstVisibleLine() == 0);
    edit->onTextInput(typed);
    MouseEvent wheel;
    wheel.scrollY = 100.0f;
    edit->onMouseScroll(wheel);
    paint();
    assert(edit->firstVisibleLine() == 0);
}

// O mesmo no CodeEditor: o cursor é desenhado em todas as pinturas, e o realce
// da linha atual fica sempre à mesma distância do número da linha na goteira,
// mesmo quando a pintura teve de rolar a vista.
static void test_code_editor_highlights_the_cursor_row_while_typing_wrapped()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    atlas.setTexture(TextureHandle{ 1 });
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("codetypewrap");
    auto* screen = stage->createChild<Widget>();
    auto* editor = screen->createChild<CodeEditor>();
    const Theme& theme = Theme::instance();
    const float rowHeight = theme.fontSize + theme.lineSpacing;
    editor->setRect({ 0.0f, 0.0f, 240.0f, rowHeight * 6.0f });
    editor->setShowFolding(false);
    editor->setShowMinimap(false);
    editor->setHighlightCurrentLine(true);
    editor->setWordWrap(true);
    editor->setText("one\ntwo\nthree\nfour");
    assert(app.setStage("codetypewrap", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    io.deltaTime = 0.0f;
    app.setFocused(editor);
    editor->setCursorPos(3, 4);
    auto same = [](Color a, Color b)
    { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; };
    const Vec2 white = atlas.whitePixelUV();
    GetDrawList().setWhitePixel(TextureHandle{ 1 }, white);
    float firstGap = 0.0f;
    for (int i = 0; i < 90; ++i)
    {
        KeyEvent typed;
        typed.text[0] = (i % 6 == 5) ? ' ' : 'w';
        editor->onTextInput(typed);
        app.update(io);
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
        float highlightTop = 1.0e9f, numberTop = 1.0e9f;
        bool caret = false;
        for (const DrawPass& pass: data.passes)
        {
            if (!pass.list) continue;
            for (const DrawVertex& vertex: pass.list->vertices())
            {
                const bool solid = vertex.u == white.x && vertex.v == white.y;
                if (solid && same(vertex.color, Color(45, 50, 60, 255)) && vertex.y < highlightTop)
                    highlightTop = vertex.y;
                if (!solid && vertex.x < 30.0f && same(vertex.color, theme.lineNumberActive) &&
                        vertex.y < numberTop)
                    numberTop = vertex.y;
                if (solid && vertex.x > 40.0f && same(vertex.color, theme.textColor)) caret = true;
            }
        }
        assert(caret);
        assert(highlightTop < 1.0e9f && numberTop < 1.0e9f);
        if (i == 0) firstGap = numberTop - highlightTop;
        assert(numberTop - highlightTop == firstGap);
    }
}

// Com um bloco dobrado, as linhas escondidas não ocupam filas: um clique em cada
// fila cai na linha visível certa, antes e depois de editar o texto.
static void test_code_editor_rows_skip_folded_lines()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("foldrows");
    auto* screen = stage->createChild<Widget>();
    auto* editor = screen->createChild<CodeEditor>();
    editor->setRect({ 0.0f, 0.0f, 400.0f, 300.0f });
    editor->setShowMinimap(false);
    editor->setHighlighterForFile("main.cpp");
    editor->setText("int a()\n{\n    return 1;\n}\nint b()\n{\n    return 2;\n}\nend");
    assert(app.setStage("foldrows", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    auto paint = [&app, &io, font]()
    {
        app.update(io);
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
    };
    const Theme& theme = Theme::instance();
    const float rowHeight = theme.fontSize + theme.lineSpacing;
    auto rowLines = [&]()
    {
        String lines;
        for (int row = 0; row < 9; ++row)
        {
            MouseEvent press;
            press.button = 0;
            press.clickCount = 1;
            press.localX = 300.0f;
            press.localY = rowHeight * (static_cast<float>(row) + 0.5f);
            press.x = press.localX;
            press.y = press.localY;
            editor->onMousePress(press);
            MouseEvent release = press;
            editor->onMouseRelease(release);
            lines += String::number(editor->cursorPos().line) + " ";
        }
        return lines;
    };
    paint();
    assert(rowLines() == "0 1 2 3 4 5 6 7 8 ");
    editor->foldAt(1);
    paint();
    assert(editor->isFolded(1));
    assert(rowLines() == "0 1 4 5 6 7 8 8 8 ");
    editor->foldAt(5);
    paint();
    assert(rowLines() == "0 1 4 5 8 8 8 8 8 ");
    editor->setCursorPos(8, 3);
    KeyEvent typed;
    typed.text[0] = 'x';
    editor->onTextInput(typed);
    paint();
    assert(editor->lineAt(8) == "endx");
    assert(rowLines() == "0 1 4 5 8 8 8 8 8 ");
}

// Quando as dobras são refeitas depois de uma edição, as filas seguem o que está
// escondido agora: um undo com um bloco dobrado não deixa linhas sem filas.
static void test_code_editor_rows_follow_the_folds_after_undo()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("foldundo");
    auto* screen = stage->createChild<Widget>();
    auto* editor = screen->createChild<CodeEditor>();
    editor->setRect({ 0.0f, 0.0f, 400.0f, 300.0f });
    editor->setShowMinimap(false);
    editor->setHighlighterForFile("main.cpp");
    editor->setText("int a()\n{\n    return 1;\n}\nint b()\n{\n    return 2;\n}\nend");
    assert(app.setStage("foldundo", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    auto paint = [&app, &io, font]()
    {
        app.update(io);
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
    };
    const Theme& theme = Theme::instance();
    const float rowHeight = theme.fontSize + theme.lineSpacing;
    auto rowLines = [&]()
    {
        String lines;
        for (int row = 0; row < 9; ++row)
        {
            MouseEvent press;
            press.button = 0;
            press.clickCount = 1;
            press.localX = 300.0f;
            press.localY = rowHeight * (static_cast<float>(row) + 0.5f);
            press.x = press.localX;
            press.y = press.localY;
            editor->onMousePress(press);
            MouseEvent release = press;
            editor->onMouseRelease(release);
            lines += String::number(editor->cursorPos().line) + " ";
        }
        return lines;
    };
    paint();
    editor->foldAt(1);
    paint();
    assert(rowLines() == "0 1 4 5 6 7 8 8 8 ");
    app.setFocused(editor);
    editor->setCursorPos(6, 4);
    KeyEvent typed;
    typed.text[0] = 'x';
    editor->onTextInput(typed);
    paint();
    editor->undo();
    paint();
    assert(editor->lineAt(6) == "    return 2;");
    assert(rowLines() == (editor->isFolded(1) ? "0 1 4 5 6 7 8 8 8 " : "0 1 2 3 4 5 6 7 8 "));
}

// Um bloco dobrado continua dobrado quando o texto muda à volta dele, e anda com
// a sua linha: Enter noutra linha, Enter no início do documento ou da própria
// linha, juntar a linha vazia de volta, undo, remover e duplicar uma linha
// acima. Um setText novo não herda dobras, e as do texto novo dobram.
static void test_code_editor_folds_survive_edits()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("foldedits");
    auto* screen = stage->createChild<Widget>();
    auto* editor = screen->createChild<CodeEditor>();
    editor->setRect({ 0.0f, 0.0f, 400.0f, 300.0f });
    editor->setShowMinimap(false);
    editor->setHighlighterForFile("main.cpp");
    editor->setText("int a()\n{\n    return 1;\n}\nint b()\n{\n    return 2;\n}\nend");
    assert(app.setStage("foldedits", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    auto paint = [&app, &io, font]()
    {
        app.update(io);
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
    };
    const Theme& theme = Theme::instance();
    const float rowHeight = theme.fontSize + theme.lineSpacing;
    auto rowLines = [&](int rows)
    {
        const TextEdit::TextPos keep = editor->cursorPos();
        String lines;
        for (int row = 0; row < rows; ++row)
        {
            MouseEvent press;
            press.button = 0;
            press.clickCount = 1;
            press.localX = 300.0f;
            press.localY = rowHeight * (static_cast<float>(row) + 0.5f);
            press.x = press.localX;
            press.y = press.localY;
            editor->onMousePress(press);
            MouseEvent release = press;
            editor->onMouseRelease(release);
            lines += String::number(editor->cursorPos().line) + " ";
        }
        editor->setCursorPos(keep);
        return lines;
    };
    auto key = [editor](int code)
    {
        KeyEvent press;
        press.key = code;
        editor->onKeyPress(press);
    };
    paint();
    editor->foldAt(1);
    paint();
    app.setFocused(editor);

    editor->setCursorPos(6, 13);
    key(Key::Return);
    paint();
    assert(editor->lineCount() == 10);
    assert(editor->isFolded(1));
    assert(rowLines(8) == "0 1 4 5 6 7 8 9 ");

    editor->setCursorPos(0, 0);
    key(Key::Return);
    paint();
    assert(editor->lineAt(2) == "{");
    assert(!editor->isFolded(1) && editor->isFolded(2));
    assert(rowLines(8) == "0 1 2 5 6 7 8 9 ");

    editor->setCursorPos(1, 0);
    key(Key::Backspace);
    paint();
    assert(editor->lineAt(1) == "{");
    assert(editor->isFolded(1));
    assert(rowLines(8) == "0 1 4 5 6 7 8 9 ");

    editor->undo();
    paint();
    assert(editor->isFolded(2));
    editor->redo();
    paint();
    assert(editor->isFolded(1));
    assert(rowLines(8) == "0 1 4 5 6 7 8 9 ");

    editor->setCursorPos(0, 0);
    editor->duplicateLine();
    paint();
    assert(editor->isFolded(2));
    editor->setCursorPos(0, 0);
    editor->removeLine();
    paint();
    assert(editor->isFolded(1));
    assert(rowLines(8) == "0 1 4 5 6 7 8 9 ");

    editor->setCursorPos(1, 0);
    key(Key::Return);
    paint();
    assert(editor->lineAt(2) == "{");
    assert(editor->isFolded(2));
    assert(rowLines(8) == "0 1 2 5 6 7 8 9 ");
    editor->setCursorPos(2, 0);
    key(Key::Backspace);
    paint();
    assert(editor->isFolded(1));

    editor->setText("x\ny\nint c()\n{\n    return 3;\n}\nend");
    paint();
    assert(!editor->isFolded(1) && !editor->isFolded(3));
    assert(rowLines(7) == "0 1 2 3 4 5 6 ");
    editor->foldAt(3);
    paint();
    assert(editor->isFolded(3));
    assert(rowLines(5) == "0 1 2 3 6 ");
}

// As edições do CodeEditor só voltam a realçar a partir das linhas que mudaram:
// abrir e fechar um comentário de bloco (que muda o estado das linhas seguintes),
// Enter, juntar linhas, undo e redo, mover, duplicar e remover linhas, escrever
// com vários cursores, editar acima de uma linha ainda por realçar. Depois de
// cada passo pinta exatamente o mesmo (cores dos glifos e ícones das dobras),
// com a vista rolada e no topo, que o mesmo texto realçado do zero.
static void test_code_editor_highlights_only_the_edited_lines()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    atlas.setTexture(TextureHandle{ 1 });
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("codehl");
    auto* screen = stage->createChild<Widget>();
    auto* editor = screen->createChild<CodeEditor>();
    editor->setRect({ 0.0f, 0.0f, 600.0f, 90.0f });
    editor->setShowMinimap(false);
    editor->setHighlighterForFile("main.cpp");
    editor->setText("int a()\n{\n    return 1; // one\n}\n\nint b()\n{\n    if (x)\n    {\n        "
                    "y();\n    }\n}\n"
                    "const char* s = \"text\";\nint c = 3;\nend");
    assert(app.setStage("codehl", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    auto snapshot = [&]()
    {
        app.update(io);
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
        ct::Vector<float> values;
        for (const DrawPass& pass: data.passes)
        {
            if (!pass.list) continue;
            for (const DrawVertex& vertex: pass.list->vertices())
            {
                values.push_back(vertex.x);
                values.push_back(vertex.y);
                values.push_back(vertex.u);
                values.push_back(static_cast<float>(vertex.color.r));
                values.push_back(static_cast<float>(vertex.color.g));
                values.push_back(static_cast<float>(vertex.color.b));
            }
        }
        return values;
    };
    auto views = [&]()
    {
        editor->scrollToLine(10);
        assert(editor->firstVisibleLine() == 10);
        ct::Vector<float> values = snapshot();
        editor->scrollToLine(0);
        const ct::Vector<float> top = snapshot();
        for (float value: top) values.push_back(value);
        return values;
    };
    auto matchesFresh = [&]()
    {
        editor->clearExtraCursors();
        editor->setCursorPos(0, 0);
        const ct::Vector<float> edited = views();
        editor->setText(editor->text());
        const ct::Vector<float> fresh = views();
        if (edited.size() != fresh.size()) return false;
        for (size_t i = 0; i < edited.size(); ++i)
            if (edited[i] != fresh[i]) return false;
        return true;
    };
    auto key = [editor](int code)
    {
        KeyEvent press;
        press.key = code;
        editor->onKeyPress(press);
    };
    auto type = [editor](const char* text)
    {
        for (const char* c = text; *c; ++c)
        {
            KeyEvent typed;
            typed.text[0] = *c;
            editor->onTextInput(typed);
        }
    };
    snapshot();
    editor->setCursorPos(2, 0);
    type("/*");
    assert(matchesFresh());
    editor->setCursorPos(9, 0);
    type("*/");
    assert(matchesFresh());
    editor->setCursorPos(2, 0);
    key(Key::Delete);
    key(Key::Delete);
    assert(matchesFresh());
    editor->setCursorPos(6, 1);
    key(Key::Return);
    type("{ z();");
    snapshot();
    editor->setCursorPos(5, static_cast<int>(editor->lineAt(5).size()));
    key(Key::Delete);
    assert(matchesFresh());
    editor->undo();
    editor->undo();
    assert(matchesFresh());
    editor->redo();
    assert(matchesFresh());
    editor->setCursorPos(12, 0);
    editor->moveLineUp();
    editor->duplicateLine();
    assert(matchesFresh());
    editor->setCursorPos(1, 0);
    editor->removeLine();
    editor->setCursorPos(0, 0);
    type("/* ");
    assert(matchesFresh());
    editor->setCursorPos(3, 0);
    editor->addCursor(7, 0);
    editor->addCursor(10, 0);
    type("*/");
    assert(matchesFresh());
    editor->setCursorPos(8, 0);
    type("/*");
    editor->setCursorPos(0, 0);
    key(Key::Return);
    assert(matchesFresh());
}

// O auto-indent do Enter conta só o texto antes do cursor: Enter no início ou no
// meio da indentação de uma linha empurra-a para baixo sem lhe mudar a
// indentação, e um "{" só dá indentação extra quando fica antes do cursor.
static void test_code_editor_auto_indent_uses_the_text_before_the_cursor()
{
    using namespace ig::retained;
    CodeEditor editor;
    auto enterAt = [&editor](const char* text, int col)
    {
        editor.setText(text);
        editor.setCursorPos(0, col);
        KeyEvent press;
        press.key = Key::Return;
        editor.onKeyPress(press);
        String lines;
        for (int i = 0; i < editor.lineCount(); ++i) lines += "[" + editor.lineAt(i) + "]";
        return lines;
    };
    assert(enterAt("    foo", 0) == "[][    foo]");
    assert(enterAt("    foo", 2) == "[  ][    foo]");
    assert(enterAt("{", 0) == "[][{]");
    assert(enterAt("int a() {", 9) == "[int a() {][\t]");
    assert(enterAt("    x = 1;", 10) == "[    x = 1;][    ]");
    assert(enterAt("    x = 1;", 6) == "[    x ][    = 1;]");
    editor.undo();
    assert(editor.lineCount() == 1 && editor.lineAt(0) == "    x = 1;");
}

// Um filho acrescentado à TreeGrid aparece sem um rebuild() explícito: a seta
// para baixo passa da raiz para o filho novo.
static void test_tree_grid_shows_new_children_without_rebuild()
{
    using namespace ig::retained;
    TreeGrid tree;
    tree.addColumn("Name", 120.0f);
    TreeGrid::Node* root = tree.addRoot({ "root" });
    tree.setFocused(true);
    KeyEvent down;
    down.key = Key::Down;
    tree.onKeyPress(down);
    assert(tree.selectedNode() == root);
    TreeGrid::Node* child = root->addChild({ "child" });
    tree.onKeyPress(down);
    assert(tree.selectedNode() == child);
}

// Com uma coluna ordenada, uma linha acrescentada entra logo no seu lugar: um
// clique na primeira linha do ecrã seleciona a linha com o valor mais pequeno.
static void test_data_grid_sorts_new_rows_into_place()
{
    using namespace ig::retained;
    DataGrid grid;
    grid.setRect({ 0.0f, 0.0f, 300.0f, 200.0f });
    grid.addColumn("Name", 100.0f);
    grid.addColumn("Value", 100.0f);
    grid.addRow({ "a", "30" });
    grid.addRow({ "b", "10" });
    grid.addRow({ "c", "20" });
    auto click = [&grid](float x, float y)
    {
        MouseEvent press;
        press.button = 0;
        press.clickCount = 1;
        press.x = press.localX = x;
        press.y = press.localY = y;
        grid.onMousePress(press);
        MouseEvent release = press;
        grid.onMouseRelease(release);
    };
    click(130.0f, 10.0f);
    const float firstRow = 28.0f + 4.0f;
    click(50.0f, firstRow);
    assert(grid.selectedRow() == 1);
    grid.addRow({ "d", "5" });
    click(50.0f, firstRow);
    assert(grid.selectedRow() == 3);
    grid.removeRow(3);
    click(50.0f, firstRow);
    assert(grid.selectedRow() == 1);
}

// O realce dos parênteses conta caracteres e não bytes: em "ação(x)" com o
// cursor no ")", os dois realces ficam sobre o "(" e o ")", e um Alt+clique no
// meio de cada um põe um cursor nessa coluna. O mesmo com o par em duas linhas.
static void test_code_editor_matches_brackets_after_multibyte_text()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("codebrackets");
    auto* screen = stage->createChild<Widget>();
    auto* editor = screen->createChild<CodeEditor>();
    editor->setRect({ 0.0f, 0.0f, 400.0f, 200.0f });
    editor->setShowBracketMatch(true);
    editor->setText("a\xC3\xA7\xC3\xA3o(x)");
    assert(app.setStage("codebrackets", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    const Theme& theme = Theme::instance();
    const float rowHeight = theme.fontSize + theme.lineSpacing;
    auto highlightColumns = [&](int cursorLine, int cursorCol, int closeLine)
    {
        editor->clearExtraCursors();
        editor->setCursorPos(cursorLine, cursorCol);
        app.update(io);
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
        ct::Vector<float> xs;
        for (const DrawPass& pass: data.passes)
        {
            if (!pass.list) continue;
            for (const DrawVertex& vertex: pass.list->vertices())
            {
                if (vertex.color.r != 255 || vertex.color.g != 210 || vertex.color.b != 80 ||
                        vertex.color.a != 60)
                    continue;
                bool seen = false;
                for (float x: xs) seen = seen || x == vertex.x;
                if (!seen) xs.push_back(vertex.x);
            }
        }
        String columns;
        if (xs.size() != 4) return columns;
        editor->setCursorPos(0, 0);
        for (size_t i = 0; i < xs.size(); ++i)
            for (size_t j = i + 1; j < xs.size(); ++j)
                if (xs[j] < xs[i])
                {
                    const float swap = xs[i];
                    xs[i] = xs[j];
                    xs[j] = swap;
                }
        for (size_t rect = 0; rect < 2; ++rect)
        {
            MouseEvent press;
            press.button = 0;
            press.alt = true;
            press.x = (xs[rect * 2] + xs[rect * 2 + 1]) * 0.5f;
            press.y = rowHeight * (static_cast<float>(rect == 0 ? 0 : closeLine) + 0.5f);
            press.localX = press.x;
            press.localY = press.y;
            editor->onMousePress(press);
            const TextEdit::TextPos added = editor->cursorPosAt(editor->cursorCount() - 1);
            columns += String::number(added.line) + ":" + String::number(added.col) + " ";
        }
        return columns;
    };
    assert(highlightColumns(0, 6, 0) == "0:4 0:6 ");
    assert(highlightColumns(0, 4, 0) == "0:4 0:6 ");
    assert(highlightColumns(0, 7, 0) == "0:4 0:6 ");
    assert(highlightColumns(0, 2, 0) == "");
    editor->setText("(\n\xC3\xA7\xC3\xA7)");
    assert(highlightColumns(0, 0, 1) == "0:0 1:2 ");
    assert(highlightColumns(1, 2, 1) == "0:0 1:2 ");
}

// Um byte que não começa uma sequência completa desenha-se como U+FFFD. O
// CodeEditor mede a linha com essa mesma regra nos cursores extra, no realce dos
// parênteses, nos pontos dos espaços e no minimapa: o texto com bytes inválidos
// pinta exatamente o mesmo que o texto com U+FFFD no lugar deles.
static void test_code_editor_draws_invalid_utf8_as_the_replacement()
{
    using namespace ig::retained;
    FontAtlas atlas;
    assert(atlas.buildDefault());
    const Font* font = &atlas.defaultFont();
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("codeutf8");
    auto* screen = stage->createChild<Widget>();
    auto* editor = screen->createChild<CodeEditor>();
    editor->setRect({ 0.0f, 0.0f, 400.0f, 200.0f });
    editor->setShowWhitespace(true);
    editor->setShowMinimap(true);
    editor->setShowBracketMatch(true);
    assert(app.setStage("codeutf8", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    io.deltaTime = 0.0f;
    auto paintText = [&](const String& text)
    {
        editor->setText(text);
        editor->clearExtraCursors();
        editor->setCursorPos(0, 5);
        editor->addCursor(0, 8);
        editor->addCursor(2, 4);
        app.setFocused(editor);
        app.update(io);
        GetDrawList().clear();
        DrawData data;
        app.paint(data, font);
        ct::Vector<float> values;
        for (const DrawPass& pass: data.passes)
        {
            if (!pass.list) continue;
            const DrawList& list = *pass.list;
            for (const DrawCmd& cmd: list.commands())
            {
                values.push_back(cmd.clip.x);
                values.push_back(cmd.clip.w);
                for (uint32_t i = 0; i < cmd.indexCount; ++i)
                {
                    const DrawVertex& vertex = list.vertices()[list.indices()[cmd.indexOffset + i]];
                    values.push_back(vertex.x);
                    values.push_back(vertex.y);
                    values.push_back(vertex.u);
                    values.push_back(vertex.v);
                    values.push_back(static_cast<float>(vertex.color.a));
                }
            }
        }
        return values;
    };
    const String invalid = "Bal\xE3o() x\nab\xE3\n\t\xC3x y";
    const String replaced = "Bal\xEF\xBF\xBDo() x\nab\xEF\xBF\xBD\n\t\xEF\xBF\xBDx y";
    const ct::Vector<float> drawn = paintText(invalid);
    const ct::Vector<float> expected = paintText(replaced);
    const ct::Vector<float> again = paintText(invalid);
    assert(!expected.empty());
    assert(again.size() == drawn.size());
    for (size_t i = 0; i < drawn.size(); ++i) assert(again[i] == drawn[i]);
    assert(drawn.size() == expected.size());
    for (size_t i = 0; i < drawn.size(); ++i) assert(drawn[i] == expected[i]);
}

static void test_scroll_view_wheel_reaches_the_end_with_both_bars()
{
    using namespace ig::retained;
    WidgetApp& app = WidgetApp::instance();
    Widget* stage = app.addStage("bothbars");
    auto* screen = stage->createChild<Widget>();
    auto* view = screen->createChild<ScrollView>();
    view->setRect({ 0.0f, 0.0f, 300.0f, 200.0f });
    view->setContent<BigContent>();
    assert(app.setStage("bothbars", TransitionType::None));
    IO io;
    io.displayWidth = 800.0f;
    io.displayHeight = 600.0f;
    app.update(io);
    DrawData data;
    app.paint(data);

    const float bar = ScrollBar::kBarThickness + 2.0f;
    MouseEvent down;
    down.scrollY = -1000.0f;
    view->onMouseScroll(down);
    assert(view->scrollY() == 1000.0f - (200.0f - bar));
    MouseEvent right;
    right.scrollX = -1000.0f;
    view->onMouseScroll(right);
    assert(view->scrollX() == 1000.0f - (300.0f - bar));
}

static_assert(std::is_same<ig::retained::CodeEditor::SuperType, ig::retained::TextEdit>::value,
              "SuperType must name the declared base");
static_assert(std::is_same<ig::retained::TextEdit::SuperType, ig::retained::Widget>::value,
              "SuperType must name the declared base");

// widget_cast e typeName seguem a cadeia declarada em IG_WIDGET_CLASS, sem RTTI.
static void test_widget_cast_follows_the_declared_chain()
{
    using namespace ig::retained;
    Widget root;
    auto* label = root.createChild<Label>("a");
    auto* button = root.createChild<Button>("b");
    auto* editor = root.createChild<CodeEditor>();
    auto* alert = root.createChild<AlertDialog>("t", "m");
    editor->setId("editor");
    editor->addTag("code");
    label->setId("label");

    Widget* asWidget = editor;
    assert(widget_cast<Widget>(asWidget) == asWidget);
    assert(widget_cast<CodeEditor>(asWidget) == editor);
    assert(widget_cast<TextEdit>(asWidget) == static_cast<TextEdit*>(editor));
    assert(widget_cast<Label>(asWidget) == nullptr);
    assert(widget_cast<Label>(static_cast<Widget*>(button)) == nullptr);
    assert(widget_cast<Button>(static_cast<Widget*>(button)) == button);
    assert(widget_cast<TextEdit>(static_cast<Widget*>(label)) == nullptr);

    TextEdit plain;
    assert(widget_cast<CodeEditor>(static_cast<Widget*>(&plain)) == nullptr);
    assert(widget_cast<TextEdit>(static_cast<Widget*>(&plain)) == &plain);

    assert(widget_cast<Dialog>(static_cast<Widget*>(alert)) == static_cast<Dialog*>(alert));
    assert(widget_cast<AlertDialog>(static_cast<Widget*>(alert)) == alert);
    assert(widget_cast<ConfirmDialog>(static_cast<Widget*>(alert)) == nullptr);

    assert(widget_cast<Label>(static_cast<Widget*>(nullptr)) == nullptr);

    const Widget* constWidget = label;
    assert(widget_cast<Label>(constWidget) == label);
    assert(widget_cast<Button>(constWidget) == nullptr);
    const Label* constLabel = widget_cast<Label>(constWidget);
    assert(constLabel != nullptr);

    assert(String(label->typeName()) == "Label");
    assert(String(asWidget->typeName()) == "CodeEditor");
    assert(String(static_cast<Widget*>(alert)->typeName()) == "AlertDialog");
    assert(String(root.typeName()) == "Widget");
    assert(String(CodeEditor::typeNameStatic()) == "CodeEditor");

    assert(root.findById<CodeEditor>("editor") == editor);
    assert(root.findById<Label>("editor") == nullptr);
    assert(root.findById<Label>("label") == label);
    assert(root.findByTag<TextEdit>("code").size() == 1u);
    assert(root.findByTag<Label>("code").empty());
}

// O serializador grava e lê os 27 tipos da lista com os mesmos nomes de sempre; um
// widget fora da lista continua a gravar-se como "Widget" (e a ler-se como tal).
static void test_serializer_round_trips_every_listed_type()
{
    using namespace ig::retained;
    WidgetSerializer::registerBuiltinTypes();

    auto* root = new BoxLayout(LayoutDir::Vertical);
    static const char* const expected[] = {
        "ImageView", "ScrollView", "ScrollBar", "Canvas", "ProgressBar", "Slider",
        "ComboBox", "ListWidget", "ListBox", "RadioButton", "Switch", "CheckBox",
        "Button", "Line", "Spacer", "Label", "Panel", "StatusBar", "Collapsible",
        "StackLayout", "FormLayout", "FlowLayout", "Overlay", "Splitter",
        "GridLayout", "BorderLayout", "BoxLayout"};
    root->createChild<ImageView>();
    root->createChild<ScrollView>();
    root->createChild<ScrollBar>();
    root->createChild<Canvas>();
    root->createChild<ProgressBar>();
    root->createChild<Slider>();
    root->createChild<ComboBox>();
    root->createChild<ListWidget>();
    root->createChild<ListBox>();
    root->createChild<RadioButton>("r");
    root->createChild<Switch>("s");
    root->createChild<CheckBox>("c");
    root->createChild<Button>("b");
    root->createChild<Line>();
    root->createChild<Spacer>(8.0f);
    root->createChild<Label>("l");
    root->createChild<Panel>();
    root->createChild<StatusBar>();
    root->createChild<Collapsible>("c");
    root->createChild<StackLayout>();
    root->createChild<FormLayout>();
    root->createChild<FlowLayout>();
    root->createChild<Overlay>();
    root->createChild<Splitter>();
    root->createChild<GridLayout>();
    root->createChild<BorderLayout>();
    root->createChild<BoxLayout>(LayoutDir::Horizontal);
    const size_t listed = sizeof(expected) / sizeof(expected[0]);
    assert(listed == 27u);
    assert(root->children().size() == listed);

    root->createChild<ImageButton>();
    auto* tabs = root->createChild<TabLayout>();
    tabs->setTabPosition(TabPosition::Left);
    tabs->setTabsClosable(true);
    tabs->setTabBarSize(40.0f);
    auto* anchors = root->createChild<AnchorLayout>();
    anchors->setPadding(1.0f, 2.0f, 3.0f, 4.0f);

    const json saved = WidgetSerializer::save(root);
    assert(String(saved["type"].str()) == "BoxLayout");
    assert(saved["children"].size() == listed + 3u);
    for (size_t i = 0; i < listed; ++i)
    {
        assert(WidgetSerializer::typeName(root->children()[i]) == expected[i]);
        assert(String(saved["children"][i]["type"].str()) == expected[i]);
    }
    assert(WidgetSerializer::typeName(root->children()[listed]) == "Widget");
    assert(WidgetSerializer::typeName(root->children()[listed + 1]) == "TabLayout");
    assert(WidgetSerializer::typeName(root->children()[listed + 2]) == "AnchorLayout");
    assert(WidgetSerializer::typeName(nullptr) == "Widget");

    Widget destination;
    Widget* loaded = WidgetSerializer::load(saved, &destination);
    assert(loaded != nullptr);
    assert(loaded->children().size() == listed + 3u);
    for (size_t i = 0; i < listed; ++i)
        assert(WidgetSerializer::typeName(loaded->children()[i]) == expected[i]);
    auto* loadedTabs = widget_cast<TabLayout>(loaded->children()[listed + 1]);
    assert(loadedTabs != nullptr);
    assert(loadedTabs->tabPosition() == TabPosition::Left);
    assert(loadedTabs->tabsClosable());
    assert(loadedTabs->tabBarSize() == 40.0f);
    auto* loadedAnchors = widget_cast<AnchorLayout>(loaded->children()[listed + 2]);
    assert(loadedAnchors != nullptr);
    assert(loadedAnchors->padding().top == 1.0f && loadedAnchors->padding().right == 2.0f);
    assert(loadedAnchors->padding().bottom == 3.0f && loadedAnchors->padding().left == 4.0f);

    delete root;
}

int main()
{
    using namespace ig;

    using namespace ig::retained;
    test_round_rect_corner_is_a_curve();
    test_knob_arc_is_open_at_the_bottom();
    test_knob_accent_comes_from_the_caller();
    test_fader_takes_a_press_anywhere();
    test_fader_accent_comes_from_the_caller();
    test_gizmo3d_draws_the_box_it_moves();
    test_gizmo3d_grabs_the_handle_under_the_pointer();
    test_float_window_state_round_trips_through_ini();
    test_dock_layout_is_remembered_in_the_state_file();
    test_odd_window_titles_do_not_abort_the_state_file();
    test_closing_a_dock_panel_deletes_its_content_once();
    test_removing_a_float_twice_is_harmless();
    test_a_column_added_after_rows_has_cells();
    test_removing_a_line_can_be_undone();
    test_a_widget_can_be_removed_by_its_own_handler();
    test_a_carousel_shows_one_page_at_a_time();
    test_setting_the_same_float_content_again_keeps_it();
    test_tree_nodes_report_structural_changes();
    test_a_popup_goes_away_with_its_owner();
    test_a_toolbar_item_can_remove_itself_when_clicked();
    test_dir_cache_keeps_children_when_it_grows();
    test_removing_the_stage_being_left_stops_its_transition();
    test_views_let_go_of_their_models();
    test_owned_popups_are_deleted();
    test_file_dialog_opens_only_on_a_double_click();
    test_list_widget_scrolls_its_rows_for_the_pointer_too();
    test_hidden_widgets_do_not_paint();
    test_widget_app_clock_keeps_short_frames();
    test_key_handler_can_remove_its_widget();
    test_signal_handlers_can_change_the_signal();
    test_scrolling_stays_in_the_view_that_scrolls();
    test_dialogs_take_the_keyboard_while_open();
    test_popups_closed_from_outside_are_closed_for_their_owner();
    test_page_view_hides_the_pages_it_leaves();
    test_disabled_fields_ignore_input();
    test_invalid_utf8_stays_inside_the_text();
    test_text_edit_rewraps_after_every_edit();
    test_hover_after_a_click_stays_on_the_float();
    test_collapsible_keeps_its_children_while_closed();
    test_border_layout_keeps_a_widget_set_again();
    test_debug_layout_key_is_opt_in();
    test_data_grid_tab_moves_the_edit();
    test_removing_a_tab_reports_the_current_one();
    test_text_edit_word_and_line_clicks_count_characters();
    test_cell_editors_work_on_whole_characters();
    test_date_picker_keeps_real_dates();
    test_restored_windows_stay_on_screen();
    test_file_dialog_breadcrumbs_follow_the_path_root();
    test_shaded_colors_stay_in_range();
    test_widget_animations_stop_with_their_widget();
    test_text_edit_scrolls_sideways_without_wrap();
    test_console_counts_collapsed_repeats();
    test_global_events_reach_their_handlers();
    test_text_edit_wrap_rows_are_stable();
    test_text_edit_scrolls_to_a_line_before_painting();
    test_text_fields_hit_test_with_their_paint_font();
    test_text_edit_measures_again_after_zoom_and_tab_size();
    test_text_edit_wraps_only_the_edited_lines();
    test_text_edit_click_before_the_wrap_is_measured();
    test_text_edit_keeps_the_cursor_visible_while_typing_wrapped();
    test_code_editor_highlights_the_cursor_row_while_typing_wrapped();
    test_code_editor_rows_skip_folded_lines();
    test_code_editor_rows_follow_the_folds_after_undo();
    test_code_editor_folds_survive_edits();
    test_code_editor_highlights_only_the_edited_lines();
    test_code_editor_auto_indent_uses_the_text_before_the_cursor();
    test_tree_grid_shows_new_children_without_rebuild();
    test_data_grid_sorts_new_rows_into_place();
    test_code_editor_matches_brackets_after_multibyte_text();
    test_code_editor_draws_invalid_utf8_as_the_replacement();
    test_scroll_view_wheel_reaches_the_end_with_both_bars();
    test_widget_cast_follows_the_declared_chain();
    test_serializer_round_trips_every_listed_type();
    PropertyGrid transformGrid;
    transformGrid.setRect({0, 0, 400, 180});
    float editedX = 1, editedY = 2, editedZ = 3;
    int changes = 0;
    transformGrid.addVec3("Position", 1, 2, 3, -100, 100,
        [&](float x, float y, float z) { editedX=x; editedY=y; editedZ=z; ++changes; });
    MouseEvent numericMouse;
    numericMouse.x=180; numericMouse.y=12;
    transformGrid.onMousePress(numericMouse);
    assert(changes == 0); // pressing must not jump to the pointer's absolute value
    transformGrid.onMouseRelease(numericMouse);
    KeyEvent editKey;
    editKey.key=ig::retained::Key::Home;
    transformGrid.onKeyPress(editKey);
    editKey.key=ig::retained::Key::Delete;
    for (int i=0; i<32; ++i) transformGrid.onKeyPress(editKey);
    KeyEvent numericText;
    numericText.text[0]='4'; numericText.text[1]='.'; numericText.text[2]='5';
    transformGrid.onTextInput(numericText);
    editKey.key=ig::retained::Key::Return;
    transformGrid.onKeyPress(editKey);
    assert(editedX == 4.5f && editedY == 2 && editedZ == 3 && changes == 1);
    transformGrid.onMousePress(numericMouse);
    numericMouse.x += 20;
    transformGrid.onMouseMove(numericMouse);
    transformGrid.onMouseRelease(numericMouse);
    assert(editedX > 4.5f && editedY == 2 && editedZ == 3);
    Timeline timeline;
    timeline.setRect({0, 0, 600, 240});
    timeline.setTimeRange(0, 5);
    const int rootJoint = timeline.addJoint("Root");
    const int handJoint = timeline.addJoint("Hand", rootJoint);
    const int fingerJoint = timeline.addJoint("Finger", handJoint);
    assert(fingerJoint == 2 && timeline.visibleTracks().size() == 3);
    timeline.setExpanded(handJoint, false);
    assert(timeline.visibleTracks().size() == 2);
    timeline.setExpanded(handJoint, true);
    timeline.addKeyframe(rootJoint, 0);
    timeline.addKeyframe(rootJoint, 5);
    MouseEvent mouse;
    mouse.x = 176; mouse.y = 38;
    timeline.onMousePress(mouse);
    assert(timeline.track(rootJoint).keyframes[0].selected);
    mouse.x = 190;
    timeline.onMouseMove(mouse);
    timeline.onMouseRelease(mouse);
    assert(timeline.track(rootJoint).keyframes[0].time > 0);
    mouse.x = 584;
    timeline.onMousePress(mouse);
    assert(timeline.track(rootJoint).keyframes[1].selected);
    mouse.x = 599;
    timeline.onMouseMove(mouse);
    timeline.onMouseRelease(mouse);
    assert(timeline.track(rootJoint).keyframes[1].time == 5);
    mouse.x = 380; mouse.y = 66; mouse.clickCount = 2;
    timeline.onMousePress(mouse);
    assert(timeline.track(handJoint).keyframes.size() == 1);
    timeline.setTimeRange(2, 2);
    assert(timeline.viewStart() == 0 && timeline.viewEnd() == 5);
    SyntaxLanguage language;
    language.name = "Example";
    language.extensions = {"example"};
    language.keywords = {"emit"};
    language.constants = {"yes"};
    DefinedHighlighter defined(language);
    auto tokens = defined.highlightLine(0, "emit yes // comment", 0);
    assert(tokens.spans[0].type == SyntaxHighlighter::TokenType::Keyword);
    assert(tokens.spans[2].type == SyntaxHighlighter::TokenType::Constant);
    assert(tokens.spans.back().type == SyntaxHighlighter::TokenType::Comment);
    auto block = defined.highlightLine(0, "/* start", 0);
    assert(block.state == 1);
    auto endBlock = defined.highlightLine(1, "end */ emit", block.state);
    assert(endBlock.state == 0 && endBlock.spans.back().type == SyntaxHighlighter::TokenType::Keyword);
    auto quoted = defined.highlightLine(0, "\"// text\"", 0);
    assert(quoted.spans.size() == 1 && quoted.spans[0].type == SyntaxHighlighter::TokenType::String);
    assert(defined.foldDelta(0, "{ \"}\" // }") == 1);
    CodeEditor editor;
    CodeEditor::registerLanguage(language);
    assert(editor.setHighlighterForFile("test.EXAMPLE") != nullptr);
    assert(String(editor.highlighter()->languageName()) == "Example");
    assert(editor.setHighlighterForFile("query.sql") != nullptr);
    auto sql = editor.highlighter()->highlightLine(0, "SELECT null", 0);
    assert(sql.spans[0].type == SyntaxHighlighter::TokenType::Keyword);
    assert(sql.spans.back().type == SyntaxHighlighter::TokenType::Constant);
    assert(editor.setHighlighterForFile("data.json") != nullptr);
    assert(editor.setHighlighterForFile("data.jsonc") != nullptr);
    assert(editor.setHighlighterForFile("main.go") != nullptr);
    assert(editor.setHighlighterForFile("notes.txt") == nullptr && editor.highlighter() == nullptr);

    // ct::Regex-backed find/replace (see third_party/containers submodule)
    editor.setText("foo bar foo baz");
    TextEdit::TextPos expectCol3{0, 3};
    TextEdit::TextPos expectCol11{0, 11};
    assert(editor.findNext("foo", true, false, false));
    assert(editor.cursorPos() == expectCol3);
    assert(editor.findNext("foo", true, false, false));
    assert(editor.cursorPos() == expectCol11);
    assert(editor.findNext("foo", true, false, false)); // wraps
    assert(editor.cursorPos() == expectCol3);

    editor.setText("value1 = 10\nvalue2 = 20");
    assert(editor.replaceAll("(\\w+) = (\\d+)", "\\2 -> \\1", false, true) == 2);
    assert(editor.lineAt(0) == "10 -> value1");
    assert(editor.lineAt(1) == "20 -> value2");

    editor.setText("abc");
    assert(!editor.findNext("(unterminated", true, false, true)); // invalid pattern, no crash
    assert(editor.replaceAll("(unterminated", "x", true, true) == 0);
    assert(editor.lineAt(0) == "abc");

    editor.setText("Hello WORLD hello");
    assert(editor.replaceAll("hello", "hi", false, true) == 2);
    assert(editor.lineAt(0) == "hi WORLD hi");

    String edited("abc");
    edited.append(2, '!');
    assert(edited == "abc!!");
    edited.insert(3, 2, '-');
    assert(edited == "abc--!!");
    edited.erase(edited.begin() + 3, edited.begin() + 5);
    assert(edited == "abc!!");
    assert(edited.compare(0, 3, "abc") == 0);
    assert(String("  x").find_first_not_of(" ") == 2u);
    assert(String("   ").find_first_not_of(" ") == String::npos);
    assert(String("file.json").find_last_of(".") == 4u);
    assert(String("42").to_int() == 42);
    assert(std::fabs(String("3.5").to_float() - 3.5f) < 0.001f);

    assert(FileSystem::exists("include/igui/widgets"));
    assert(FileSystem::isDir("include/igui/widgets"));
    assert(FileSystem::isFile("include/igui/widgets/CodeEditor.hpp"));
    assert(FileSystem::fileName("include/igui/widgets/CodeEditor.hpp") == "CodeEditor.hpp");
    assert(FileSystem::extension("include/igui/widgets/CodeEditor.hpp") == ".hpp");
    const ct::Vector<FileSystem::Entry> entries = FileSystem::listDir("include/igui/widgets");
    assert(!entries.empty());

    WidgetSerializer::registerBuiltinTypes();
    auto* source = new BoxLayout(LayoutDir::Vertical);
    source->setPadding(12.0f);
    source->setSpacing(6.0f);
    source->createChild<Label>("Serializer round-trip");
    source->createChild<Button>("Run");
    source->createChild<CheckBox>("Enabled")->setChecked(true);
    source->createChild<Slider>(0.0f, 100.0f, 42.0f);

    const json saved = WidgetSerializer::save(source);
    assert(String(saved["type"].str()) == "BoxLayout");
    assert(saved.contains("children"));
    assert(saved["children"].size() == 4u);

    json::Error parseError;
    const json parsed = json::parse(saved.dump(2), &parseError);
    assert(!parseError);
    assert(parsed == saved);

    Widget destination;
    Widget* loaded = WidgetSerializer::load(saved, &destination);
    assert(loaded != nullptr);
    assert(WidgetSerializer::typeName(loaded) == "BoxLayout");
    assert(loaded->children().size() == 4u);
    assert(WidgetSerializer::typeName(loaded->children()[0]) == "Label");
    assert(WidgetSerializer::typeName(loaded->children()[1]) == "Button");
    assert(WidgetSerializer::typeName(loaded->children()[2]) == "CheckBox");
    assert(WidgetSerializer::typeName(loaded->children()[3]) == "Slider");

    json malformed = json::object();
    malformed["type"] = "Label";
    malformed["rect"] = "not an array";
    malformed["tags"] = "also not an array";
    Widget* malformedLoaded = WidgetSerializer::load(malformed, &destination);
    assert(malformedLoaded != nullptr);
    assert(malformedLoaded->rect().w == 0.0f && malformedLoaded->rect().h == 0.0f);
    assert(malformedLoaded->tags().empty());

    malformed["rect"] = json::array();
    malformed["rect"].push_back(1.0f);
    malformed["tags"] = json::array();
    malformed["tags"].push_back("safe");
    Widget* shortRectLoaded = WidgetSerializer::load(malformed, &destination);
    assert(shortRectLoaded != nullptr);
    assert(shortRectLoaded->rect().w == 0.0f && shortRectLoaded->rect().h == 0.0f);
    assert(shortRectLoaded->tags().size() == 1u);

    Animation animation(0.0f, 10.0f, 1.0f, EaseType::Linear);
    animation.setAutoDelete(false);
    animation.start();
    animation.tick(0.5f);
    assert(animation.state() == Animation::State::Running);
    assert(std::fabs(animation.value() - 5.0f) < 0.001f);
    animation.tick(0.5f);
    assert(animation.state() == Animation::State::Finished);
    assert(std::fabs(animation.value() - 10.0f) < 0.001f);

    delete source;
    return 0;
}
