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

// A child whose content fits has nothing to scroll: it used to take the wheel
// anyway, which left a list drawn inside it reachable only by its own
// scrollbar. The inner scroll area must get the event instead.
void test_a_child_that_cannot_scroll_leaves_the_wheel_to_the_list_inside_it()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    int firstVisible = -1;
    int lastVisible = -1;
    auto frame = [&]() {
        harness.frame([&](ig::Context &c) {
            if (!c.beginWindow("Wheel", ig::Rect(40.0f, 40.0f, 400.0f, 360.0f)))
                return;
            if (c.beginChild("panel", 300.0f))   // 300 tall, holds 200: no scrollbar
            {
                c.beginVirtualList("rows", 200, 20.0f, 200.0f, firstVisible, lastVisible);
                c.endVirtualList();
                c.endChild();
            }
            c.endWindow();
        });
    };
    frame();
    frame();                                    // the child knows its content by now
    check(firstVisible == 0, "the list started scrolled");

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.position = ig::Vec2(120.0f, 150.0f);
    wheel.wheelY = -4.0f;                       // wheel down
    harness.queue(ig::Event::pointerMove(120.0f, 150.0f));
    harness.queue(wheel);
    frame();

    check(firstVisible > 0, "the wheel did not reach the list inside a child that cannot scroll");
}

// The wheel goes to the innermost scroll area under the pointer: a virtual list
// inside a panel that also scrolls used to lose it to the panel.
void test_the_wheel_goes_to_the_innermost_scroll_area()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    int firstVisible = -1;
    int lastVisible = -1;
    auto frame = [&]() {
        harness.frame([&](ig::Context &c) {
            if (!c.beginWindow("Nested", ig::Rect(40.0f, 40.0f, 400.0f, 360.0f)))
                return;
            if (c.beginChild("panel", 200.0f))   // the filler overflows it: it can scroll
            {
                c.beginVirtualList("rows", 200, 20.0f, 150.0f, firstVisible, lastVisible);
                c.endVirtualList();
                c.dummy(20.0f, 120.0f);
                c.endChild();
            }
            c.endWindow();
        });
    };
    frame();
    frame();
    check(firstVisible == 0, "the list started scrolled");

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.position = ig::Vec2(100.0f, 130.0f);
    wheel.wheelY = -4.0f;
    harness.queue(ig::Event::pointerMove(100.0f, 130.0f));
    harness.queue(wheel);
    frame();
    check(firstVisible > 0, "the panel took the wheel from the list inside it");

    context.clearWidgetState();
    frame();
    check(firstVisible == 0, "clearWidgetState left the list scrolled");
}

// Windows live as long as the Context, which an application opening one per
// document would grow forever: removeWindow gets rid of one for good.
void test_remove_window_forgets_it_everywhere()
{
    const char *path = "igui_remove_window_test.ini";
    remove(path);

    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    context.setWindowStatePath(path);

    auto frame = [&](ig::Rect initial) {
        harness.frame([&](ig::Context &c) {
            if (c.beginWindow("Doc", initial))
                c.endWindow();
        });
    };
    frame(ig::Rect(50.0f, 60.0f, 300.0f, 200.0f));
    context.saveWindowState();
    check(context.windowBounds("Doc").x == 50.0f, "the window was not created where asked");
    check(!context.removeWindow("Nothing"), "removeWindow claimed to remove a window that never existed");

    check(context.removeWindow("Doc"), "removeWindow did not find the window it was given");
    check(context.windowBounds("Doc").width == 0.0f, "the removed window is still known");

    // Reopening starts from what the application asks for, not from the .ini
    // section the window left behind.
    frame(ig::Rect(10.0f, 20.0f, 120.0f, 90.0f));
    const ig::Rect reopened = context.windowBounds("Doc");
    check(reopened.x == 10.0f && reopened.y == 20.0f && reopened.width == 120.0f,
          "a reopened window came back where the removed one was");

    context.setWindowStatePath(ig::String());
    remove(path);
}

// An undo callback is allowed to record an action of its own; that action is
// what redo means then, not the entry the callback superseded.
void test_an_undo_callback_that_records_an_action_takes_over_redo()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    int undone = 0;
    int redone = 0;
    auto counter = [](int &value) { return [&value]() { ++value; }; };

    context.pushUndo("first", counter(undone), counter(redone));
    context.pushUndo("second", [&]() {
        ++undone;
        context.pushUndo("third", counter(undone), counter(redone));
    }, counter(redone));

    check(context.canUndo(), "the history stopped offering the undo");
    check(context.undo(), "undo() refused the entry it had");
    check(undone == 1, "the callback of the undone entry did not run");
    check(!context.canRedo(), "redo offers the action the callback superseded");
    check(context.canUndo(), "the action the callback recorded was not kept");
}

// What a window defers to its overlay - an open menu, a combo list, a tooltip -
// is a layer: it has to be painted above the content of every window, not only
// above the window it belongs to. Interleaving the overlay with its own window
// left the menu of a background panel underneath the next window, which painted
// over it.
void test_an_open_menu_is_painted_above_the_windows_drawn_after_it()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    const ig::Color menuColor = context.theme().menuBg;
    const ig::Color marker(1u, 2u, 3u, 255u);      // nothing else uses this colour

    auto frame = [&]() {
        return harness.frame([&](ig::Context &c) {
            if (c.beginWindow("A", ig::Rect(40.0f, 40.0f, 420.0f, 320.0f)))
            {
                if (c.beginMenuBar(ig::Rect(0.0f, 0.0f, 200.0f, 24.0f)))
                {
                    if (c.beginMenu("File"))
                    {
                        c.menuItem("Open");
                        c.menuItem("Save");
                    }
                    c.endMenu();
                    c.endMenuBar();
                }
                c.endWindow();
            }
            // Drawn after A, so above it, and covering where the menu opens.
            if (c.beginWindow("B", ig::Rect(100.0f, 100.0f, 320.0f, 260.0f)))
            {
                c.drawRectFilled(ig::Rect(0.0f, 0.0f, 260.0f, 180.0f), marker);
                c.endWindow();
            }
        });
    };
    frame();
    const ig::Rect window = context.windowBounds("A");
    // The first menu bar entry: the menu bar starts at the content origin.
    const ig::Vec2 entry(window.x + context.theme().windowPadding + 10.0f,
                         window.y + context.theme().titleBarHeight + context.theme().windowPadding + 12.0f);

    harness.queue(ig::Event::pointerMove(entry.x, entry.y));
    harness.queue(ig::Event::pointerDown(ig::PointerButton::Left, entry.x, entry.y));
    harness.queue(ig::Event::pointerUp(ig::PointerButton::Left, entry.x, entry.y));
    frame();                       // the click opens the menu and raises A
    // A must not stay on top: the click raised it, and with A above B the order
    // the overlay is appended in makes no difference at all.
    context.raiseWindow("B", false);
    const ig::DrawData &draw = frame();

    size_t lastMenuVertex = 0;
    size_t lastMarkerVertex = 0;
    bool sawMenu = false;
    bool sawMarker = false;
    // The last menu-coloured vertex belongs to the open popup (the items), not
    // to the menu bar strip the window draws as part of its own content.
    for (size_t i = 0; i < draw.vertices.size(); ++i)
    {
        if (draw.vertices[i].color == menuColor) { lastMenuVertex = i; sawMenu = true; }
        if (draw.vertices[i].color == marker) { lastMarkerVertex = i; sawMarker = true; }
    }
    check(sawMenu, "the open menu was never drawn");
    check(sawMarker, "the window drawn last was never drawn");
    check(lastMenuVertex > lastMarkerVertex,
          "a window painted over the open menu of another one");
}

// A popup hangs from its window, it does not live inside it: the list of a combo
// near the bottom of a panel has to be drawn whole (past the window's edge) and
// its rows have to stay clickable there. Clipping it to the window's content cut
// the list off - and with nothing of a row inside the window, no row at all was
// clickable, so the entries past the first were unreachable.
void test_a_combo_popup_escapes_the_window_it_hangs_from()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const ig::StringView items[4] = {"one", "two", "three", "four"};
    int selected = 0;
    const ig::Rect comboBounds(10.0f, 150.0f, 140.0f, 24.0f);
    auto frame = [&]() {
        return harness.frame([&](ig::Context &c) {
            if (c.beginWindow("Combo", ig::Rect(40.0f, 40.0f, 320.0f, 220.0f)))
            {
                c.comboBox("pick", selected, ig::Span<const ig::StringView>(items, 4), comboBounds);
                c.endWindow();
            }
        });
    };
    frame();
    const ig::Rect window = context.windowBounds("Combo");
    const ig::Rect header(window.x + context.theme().windowPadding + comboBounds.x,
                          window.y + context.theme().titleBarHeight + context.theme().windowPadding +
                              comboBounds.y,
                          comboBounds.width, comboBounds.height);
    const ig::Rect popup(header.x, header.y + header.height, header.width,
                         header.height * 4.0f);
    check(header.bottom() <= window.bottom(), "setup: the combo header itself must be inside the window");
    check(popup.bottom() > window.bottom(), "setup: the list must reach past the window's bottom");

    const ig::Vec2 headerPoint(header.x + 8.0f, header.y + 8.0f);
    harness.queue(ig::Event::pointerMove(headerPoint.x, headerPoint.y));
    harness.queue(ig::Event::pointerDown(ig::PointerButton::Left, headerPoint.x, headerPoint.y));
    harness.queue(ig::Event::pointerUp(ig::PointerButton::Left, headerPoint.x, headerPoint.y));
    frame();

    // The last row: below the window's bottom edge.
    const ig::Vec2 lastRow(popup.x + 8.0f, popup.y + comboBounds.height * 3.0f + comboBounds.height * 0.5f);
    check(lastRow.y > window.bottom(), "setup: the row must be outside the window");
    harness.queue(ig::Event::pointerMove(lastRow.x, lastRow.y));
    harness.queue(ig::Event::pointerDown(ig::PointerButton::Left, lastRow.x, lastRow.y));
    harness.queue(ig::Event::pointerUp(ig::PointerButton::Left, lastRow.x, lastRow.y));
    const ig::DrawData &draw = frame();
    check(selected == 3, "a row of a combo list outside its window could not be clicked");

    bool drawnOutside = false;
    for (size_t i = 0; i < draw.vertices.size(); ++i)
        if (draw.vertices[i].position.y > window.bottom() + 1.0f) { drawnOutside = true; break; }
    check(drawnOutside, "the list was cut off at the window's edge");
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
    test_a_child_that_cannot_scroll_leaves_the_wheel_to_the_list_inside_it();
    test_the_wheel_goes_to_the_innermost_scroll_area();
    test_remove_window_forgets_it_everywhere();
    test_an_undo_callback_that_records_an_action_takes_over_redo();
    test_an_open_menu_is_painted_above_the_windows_drawn_after_it();
    test_a_combo_popup_escapes_the_window_it_hangs_from();
    if (gFailures != 0)
    {
        printf("test_windows: %d failure(s)\n", gFailures);
        return 1;
    }
    printf("test_windows: all tests passed\n");
    return 0;
}
