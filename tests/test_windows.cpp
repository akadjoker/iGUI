// Window management: dragging, tiling and maximizing.

#include "InteractionHarness.hpp"

#include <ct/ini.hpp>

#include <stdio.h>

namespace
{

int gFailures = 0;

void check(bool condition, const char *what)
{
    if (!condition)
    {
        ++gFailures;
        printf("FAIL %s\n", what);
    }
}

struct Window
{
    const char *title;
    ig::Rect initial;
    bool shown;
    ig::Rect content; // available content size, read back each frame
};

struct Scene
{
    ig::TestBackend backend;
    ig::Context context;
    ig::Harness harness;
    Window windows[3];

    Scene() : backend(), context(backend), harness(context)
    {
        windows[0] = Window{"A", ig::Rect(50.0f, 50.0f, 300.0f, 200.0f), true, ig::Rect()};
        windows[1] = Window{"B", ig::Rect(400.0f, 50.0f, 300.0f, 200.0f), true, ig::Rect()};
        windows[2] = Window{"Gone", ig::Rect(100.0f, 400.0f, 300.0f, 200.0f), true, ig::Rect()};
        frame();
        frame();
    }

    void build(ig::Context &c)
    {
        for (int i = 0; i < 3; ++i)
        {
            Window &w = windows[i];
            if (w.shown && c.beginWindow(w.title, w.initial))
            {
                w.content = ig::Rect(0.0f, 0.0f, c.availableWidth(), c.availableHeight());
                c.endWindow();
            }
        }
    }

    void frame() { last = &harness.frame([this](ig::Context &c) { build(c); }); }

    // Top-left of the first window background drawn: with one window shown,
    // that window's position on screen.
    ig::Vec2 origin() const
    {
        const ig::Color background = context.theme().windowBackground;
        for (size_t i = 0; i < last->vertices.size(); ++i)
            if (last->vertices[i].color == background)
                return last->vertices[i].position;
        return ig::Vec2(-1e9f, -1e9f);
    }

    const ig::DrawData *last = nullptr;
    void drag(float x0, float y0, float x1, float y1)
    {
        harness.drag(x0, y0, x1, y1, [this](ig::Context &c) { build(c); }, 4);
    }
};

void test_tile_ignores_windows_no_longer_submitted()
{
    Scene s;
    s.windows[2].shown = false;
    s.frame();
    s.frame();
    s.context.tileAllWindows();
    s.frame();
    // Two visible windows on 1280x800: two 640x800 columns, not a 2x2 grid
    // with a slot kept for the window that is gone.
    check(s.windows[0].content.height > 600.0f && s.windows[1].content.height > 600.0f,
          "tileAllWindows gave a slot to a window that is no longer drawn");
}

void test_drag_keeps_title_bar_reachable()
{
    const float far[][2] = {{5000.0f, 5000.0f}, {-5000.0f, -5000.0f}, {5000.0f, -5000.0f}, {-5000.0f, 300.0f}};
    for (int i = 0; i < 4; ++i)
    {
        Scene s;
        s.windows[1].shown = false;
        s.windows[2].shown = false;
        s.frame();
        // Grab A's title bar (A at 50,50; title bar 24px) and fling it away.
        s.drag(200.0f, 62.0f, far[i][0], far[i][1]);
        s.frame();
        // Some part of the title bar must still be on screen: sweep the
        // screen edges for a point that grabs the window again.
        bool recovered = false;
        for (float y = 2.0f; y < 800.0f && !recovered; y += 8.0f)
        {
            for (float x = 2.0f; x < 1280.0f && !recovered; x += 8.0f)
            {
                const ig::Vec2 before = s.origin();
                // Toward the middle: toward the edge the window is already
                // as far as it may go and would rightly not move.
                s.drag(x, y, x + (x < 640.0f ? 30.0f : -30.0f), y + (y < 400.0f ? 30.0f : -30.0f));
                s.frame();
                recovered = s.origin().x != before.x || s.origin().y != before.y;
            }
        }
        char message[96];
        snprintf(message, sizeof(message), "window dragged to (%g, %g) can no longer be grabbed", far[i][0],
                 far[i][1]);
        check(recovered, message);
    }
}

void test_drag_inside_screen_is_unchanged()
{
    Scene s;
    s.windows[1].shown = false;
    s.windows[2].shown = false;
    s.frame();
    s.drag(200.0f, 62.0f, 300.0f, 162.0f);
    s.frame();
    // Moved by exactly (100, 100): no clamping when nothing leaves the screen.
    check(s.origin().x == 150.0f && s.origin().y == 150.0f,
          "a drag inside the screen did not follow the pointer exactly");
}

void test_maximized_window_follows_viewport()
{
    Scene s;
    s.context.maximizeWindow("A");
    s.frame();
    s.harness.setViewport(800.0f, 600.0f);
    s.frame();
    s.frame();
    check(s.windows[0].content.width <= 800.0f && s.windows[0].content.height <= 600.0f,
          "maximized window larger than the viewport after a resize");
}

void test_window_state_round_trips_through_ini()
{
    const char *path = "igui_window_state_test.ini";
    remove(path);

    // What an earlier run of the application left behind: the window was moved
    // and sized by the user.
    ct::Ini previous;
    previous.set("A", "pos_x", 240.0);
    previous.set("A", "pos_y", 120.0);
    previous.set("A", "size_w", 420.0);
    previous.set("A", "size_h", 260.0);
    check(previous.save(path), "the state file the test seeds could not be written");

    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    context.setWindowStatePath(path);
    check(context.windowStatePath() == path, "the state file path was not kept");

    // The window is asked for at (10, 10, 100, 100): the file wins.
    bool submitted = false;
    harness.frame([&](ig::Context &c) {
        submitted = c.beginWindow("A", ig::Rect(10.0f, 10.0f, 100.0f, 100.0f));
        if (submitted) c.endWindow();
    });
    check(submitted, "the window was not submitted");
    const ig::Rect stored = context.windowBounds("A");
    check(stored.x == 240.0f && stored.y == 120.0f,
          "a window the state file knows did not open where it was left");
    check(stored.width == 420.0f && stored.height == 260.0f,
          "a window the state file knows did not open at its stored size");

    // A title the file does not know keeps the bounds the application asks for.
    harness.frame([&](ig::Context &c) {
        if (c.beginWindow("Fresh", ig::Rect(10.0f, 10.0f, 100.0f, 100.0f))) c.endWindow();
    });
    const ig::Rect fresh = context.windowBounds("Fresh");
    check(fresh.x == 10.0f && fresh.width == 100.0f,
          "a window the file does not know did not open where the application asked");

    // A maximized window fills the viewport every frame, so what is worth
    // storing is the geometry it would come back to.
    context.maximizeWindow("A");
    harness.frame([&](ig::Context &c) {
        if (c.beginWindow("A", ig::Rect(10.0f, 10.0f, 100.0f, 100.0f))) c.endWindow();
    });
    check(context.saveWindowState(), "the state file was not written");
    ct::Ini saved;
    check(saved.load(path), "the written state file could not be read back");
    check(saved.get_double("A", "pos_x") == 240.0 && saved.get_double("A", "size_w") == 420.0,
          "a maximized window was stored at the viewport instead of its restore bounds");
    check(saved.get_bool("A", "maximized"), "the maximized state was not stored");

    // Next run: a new context, the same file. The window opens maximized and
    // comes back to the geometry the file remembered.
    ig::TestBackend nextBackend;
    ig::Context next(nextBackend);
    ig::Harness nextHarness(next);
    next.setWindowStatePath(path);
    nextHarness.frame([&](ig::Context &c) {
        if (c.beginWindow("A", ig::Rect(10.0f, 10.0f, 100.0f, 100.0f))) c.endWindow();
    });
    check(next.windowBounds("A").width == 1280.0f, "a maximized window did not reopen maximized");
    next.restoreWindow("A");
    nextHarness.frame([&](ig::Context &c) {
        if (c.beginWindow("A", ig::Rect(10.0f, 10.0f, 100.0f, 100.0f))) c.endWindow();
    });
    const ig::Rect restored = next.windowBounds("A");
    check(restored.x == 240.0f && restored.y == 120.0f && restored.width == 420.0f,
          "restoring did not come back to the geometry the file remembered");

    // Persistence off writes nothing, destructor included - which is why both
    // contexts are switched off before they go out of scope.
    context.setWindowStatePath(ig::String());
    next.setWindowStatePath(ig::String());
    check(!next.saveWindowState(), "a context without a path wrote the state file");
    remove(path);
}

// The dock arrangement is remembered as well: a panel dragged to another region
// comes back there, and the region size the user dragged comes back with it.
void test_dock_arrangement_round_trips_through_ini()
{
    const char *path = "igui_dock_state_test.ini";
    remove(path);

    // What an earlier run left behind: Hierarchy was dragged to the left region
    // and that region was widened.
    ct::Ini previous;
    previous.set("dock:editor", "tabs_left", "Hierarchy");
    previous.set("dock:editor", "selected_left", "Hierarchy");
    previous.set("dock:editor", "left_width", 260.0);
    check(previous.save(path), "the state file the test seeds could not be written");

    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    context.setWindowStatePath(path);

    // The application asks for the center region every frame: the file wins.
    harness.frame([](ig::Context &c) {
        if (!c.beginWindow("host", ig::Rect(0.0f, 0.0f, 640.0f, 480.0f)))
            return;
        if (c.beginDockSpace("editor", ig::Rect(0.0f, 0.0f, 640.0f, 480.0f)))
        {
            if (c.beginDockPanel("Hierarchy", ig::DockSlot::Center))
            {
                c.label("scene");
                c.endDockPanel();
            }
            c.endDockSpace();
        }
        c.endWindow();
    });
    check(context.saveWindowState(), "the state file was not written after the dock frame");

    ct::Ini saved;
    check(saved.load(path), "the written state file could not be read back");
    check(saved.get("dock:editor", "tabs_left") == "Hierarchy",
          "a panel left in the left region did not come back there");
    check(saved.get("dock:editor", "tabs_center").empty(),
          "the panel was stored in the region the application asked for");
    check(saved.get_double("dock:editor", "left_width") == 260.0,
          "the region size the user dragged was not kept");

    // Persistence off, so the destructor of the context writes nothing.
    context.setWindowStatePath(ig::String());
    remove(path);
}

// A state file written on a bigger screen can put a window where none of it is
// reachable, and there is no public way to move a window from outside: it has to
// be fitted to the viewport it is opened in.
void test_a_window_restored_off_screen_is_fitted_to_the_viewport()
{
    const char *path = "igui_offscreen_test.ini";
    remove(path);

    ct::Ini previous;
    previous.set("A", "pos_x", 4000.0);
    previous.set("A", "pos_y", 3000.0);
    previous.set("A", "size_w", 420.0);
    previous.set("A", "size_h", 260.0);
    check(previous.save(path), "the state file the test seeds could not be written");

    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    context.setWindowStatePath(path);      // before any frame: the viewport is unknown

    harness.frame([](ig::Context &c) {
        if (c.beginWindow("A", ig::Rect(10.0f, 10.0f, 100.0f, 100.0f)))
            c.endWindow();
    });

    const ig::Rect bounds = context.windowBounds("A");
    check(bounds.x < 1280.0f && bounds.y < 800.0f,
          "a window restored outside the screen was left outside it");
    check(bounds.x + bounds.width > 0.0f && bounds.y + bounds.height > 0.0f,
          "no part of the restored window is on screen");
    check(bounds.width == 420.0f && bounds.height == 260.0f,
          "the restored window did not take the size from the state file");

    context.setWindowStatePath(ig::String());
    remove(path);
}

} // namespace

int main()
{
    test_tile_ignores_windows_no_longer_submitted();
    test_drag_keeps_title_bar_reachable();
    test_drag_inside_screen_is_unchanged();
    test_maximized_window_follows_viewport();
    test_window_state_round_trips_through_ini();
    test_dock_arrangement_round_trips_through_ini();
    test_a_window_restored_off_screen_is_fitted_to_the_viewport();
    if (gFailures != 0)
    {
        printf("test_windows: %d failure(s)\n", gFailures);
        return 1;
    }
    printf("test_windows: all tests passed\n");
    return 0;
}
