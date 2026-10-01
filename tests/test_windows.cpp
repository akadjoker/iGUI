// Window management: dragging, tiling and maximizing.

#include "InteractionHarness.hpp"

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

} // namespace

int main()
{
    test_tile_ignores_windows_no_longer_submitted();
    test_drag_keeps_title_bar_reachable();
    test_drag_inside_screen_is_unchanged();
    test_maximized_window_follows_viewport();
    if (gFailures != 0)
    {
        printf("test_windows: %d failure(s)\n", gFailures);
        return 1;
    }
    printf("test_windows: all tests passed\n");
    return 0;
}
