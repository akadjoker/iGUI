#include <igui/Widgets.hpp>
#include <igui/widgets/CodeEditor.hpp>
#include <igui/widgets/Timeline.hpp>
#include <igui/widgets/Theme.hpp>
#include <igui/widgets/GizmoWidgets.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
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

    app.setWindowStatePath(ig::retained::String());
    std::remove(path);
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
