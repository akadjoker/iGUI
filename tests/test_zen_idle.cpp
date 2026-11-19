// The zen adapter's loop when nothing happens (docs/ROUND11.md, part 1 item 3a): pump() waits in platform_wait_events for an event, a wake
// from another thread, or the time a widget asked a frame for, with no limit when none did. A real window on a throwaway Xvfb (the test
// exits 77, skipped, without one; ThrowawayDisplay.hpp refuses anywhere else), the software rasteriser.
//   - nothing at all: the loop goes round at most twice a second (it was ~120), and paints nothing;
//   - a caret that blinks: one pass and one painted frame for each flip, twice a second, on time;
//   - the application's clock keeps counting the time the loop sleeps (a clock that counted the 0.1 s cap of a frame was off by the
//     sleep: the blink, the double click, the tooltip);
//   - a worker that calls WidgetApp::requestFrameFromAnyThread wakes the loop at once (a frame is painted within a few milliseconds);
//   - a real event (a pointer move made by the XTEST extension from another connection) ends the wait at once: no 8 ms of sleep first.
// No standard library: ct and C only.

#include "ThrowawayDisplay.hpp"

#include <igui_zen/ZenApp.hpp>

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/Theme.hpp>

#include <platform.h>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

#undef None // (Xlib's None is a macro: TransitionType::None is not)
#undef Bool

#include <stdio.h>

using namespace ig::retained;

namespace
{

int gFailures = 0;
void check(bool ok, const char* what)
{
    if (!ok)
    {
        ++gFailures;
        printf("  FAIL %s\n", what);
    }
}

int gPasses = 0; // passes of pump(): phase 0 of the frame probe is called once for each, before the events are read
void probe(int phase, void*)
{
    if (phase == 0) ++gPasses;
}

ig::zen::App* gZen = nullptr;

// What a worker thread does after `delay` seconds
struct Job
{
    double delay = 0.0;
    int what = 0;           // 0: wake (App::wake), 1: requestFrameFromAnyThread, 2: a pointer move by XTEST
    double doneAt = 0.0;    // time_seconds() when the worker did it
};

int worker(void* user)
{
    Job* job = static_cast<Job*>(user);
    const double until = time_seconds() + job->delay;
    while (time_seconds() < until) time_sleep(1);
    if (job->what == 2)
    {
        Display* dpy = XOpenDisplay(nullptr);
        if (dpy)
        {
            XTestFakeMotionEvent(dpy, DefaultScreen(dpy), 60, 40, 0);
            job->doneAt = time_seconds();
            XFlush(dpy);
            XCloseDisplay(dpy);
        }
        return 0;
    }
    job->doneAt = time_seconds();
    if (job->what == 1) gZen->widgets().requestFrameFromAnyThread();
    else
        ig::zen::App::wake();
    return 0;
}

// Pumps for `seconds` (a worker ends the last wait with a wake): returns the passes it took.
int runFor(ig::zen::App& zen, double seconds)
{
    Job job;
    job.delay = seconds;
    job.what = 0;
    PlatformThread* t = create_thread(worker, &job, "idle-wake");
    const int before = gPasses;
    const double end = time_seconds() + seconds;
    while (time_seconds() < end - 0.001) zen.pump();
    join_thread(t);
    return gPasses - before;
}

// Pumps until a frame has been painted, or `limit` seconds
double untilPainted(ig::zen::App& zen, int paintedBefore, double limit)
{
    const double t0 = time_seconds();
    while (zen.paintedFrames() == paintedBefore && time_seconds() - t0 < limit) zen.pump();
    return time_seconds();
}

} // namespace

int main()
{
    if (!igui_test::onThrowawayDisplay("test_zen_idle")) return 77;
    ig::zen::App zen;
    gZen = &zen;
    ig::zen::App::Config config;
    config.title = "idle";
    config.width = 320;
    config.height = 200;
    config.showStats = false;
    config.preferOpenGL = false;
    config.vsync = false;
    if (!zen.create(config))
    {
        printf("test_zen_idle: no window here, skipped\n");
        return 77;
    }
    zen.setFrameProbe(&probe, nullptr);
    WidgetApp& app = zen.widgets();
    Widget* root = app.addStage("idle");
    auto* box = root->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(8.0f);
    box->createChild<Label>("nothing happens here");
    Button* button = box->createChild<Button>("OK");
    TextInput* input = box->createChild<TextInput>("text");
    app.setStage("idle", TransitionType::None);
    (void)button;

    // settle: the first frames (the window's own events, the layout) with a bound on the wait
    zen.setMaxIdleWait(20);
    for (int i = 0; i < 40; ++i) zen.pump();
    zen.setMaxIdleWait(-1); // the default: until an event, a wake or the time a widget asked for

    // ── nothing at all ────────────────────────────────────────────────────────────
    {
        const int painted = zen.paintedFrames();
        const double clock = app.clockSeconds();
        const double t0 = time_seconds();
        const int passes = runFor(zen, 3.0);
        const double real = time_seconds() - t0;
        printf("  nothing: %d passes in %.2f s, %d frames painted, the clock advanced %.2f s\n", passes, real, zen.paintedFrames() - painted,
                app.clockSeconds() - clock);
        check(passes <= 6, "nothing happens: at most 2 passes a second (it was about 120)");
        check(zen.paintedFrames() == painted, "nothing happens: nothing is painted");
        const double advanced = app.clockSeconds() - clock;
        check(advanced > real - 0.25 && advanced < real + 0.25, "the application's clock counted the whole sleep (not 0.1 s for each pass)");
    }

    // ── a caret that blinks ───────────────────────────────────────────────────────
    {
        app.setFocused(input);
        zen.setMaxIdleWait(20);
        for (int i = 0; i < 10; ++i) zen.pump();
        zen.setMaxIdleWait(-1);
        const int painted = zen.paintedFrames();
        const int passes = runFor(zen, 3.0);
        const int frames = zen.paintedFrames() - painted;
        printf("  caret: %d passes, %d frames painted in 3 s (a flip every half second)\n", passes, frames);
        check(frames >= 5 && frames <= 7, "a caret flips twice a second: 6 frames painted in 3 s");
        check(passes <= 9, "and the loop goes round only for the flips");
        app.setFocused(nullptr);
        zen.setMaxIdleWait(20);
        for (int i = 0; i < 10; ++i) zen.pump();
        zen.setMaxIdleWait(-1);
    }

    // ── a worker that has data for the interface ──────────────────────────────────
    {
        Job job;
        job.delay = 0.4;
        job.what = 1;
        const int painted = zen.paintedFrames();
        PlatformThread* t = create_thread(worker, &job, "idle-request");
        const int before = gPasses;
        const double done = untilPainted(zen, painted, 5.0);
        join_thread(t);
        const double late = done - job.doneAt;
        printf("  worker: requestFrameFromAnyThread at +0.4 s, a frame painted %.1f ms later (%d passes)\n", late * 1000.0, gPasses - before);
        check(zen.paintedFrames() > painted, "requestFrameFromAnyThread paints a frame although the loop was waiting with no limit");
        check(late >= 0.0 && late < 0.05, "and within 50 ms of the call");
        check(gPasses - before <= 3, "it took a pass or two, not a polling loop");
    }

    // ── a real event ──────────────────────────────────────────────────────────────
    {
        Job job;
        job.delay = 0.4;
        job.what = 2;
        const int painted = zen.paintedFrames();
        PlatformThread* t = create_thread(worker, &job, "idle-xtest");
        const double done = untilPainted(zen, painted, 5.0);
        join_thread(t);
        if (job.doneAt == 0.0)
            printf("  event: the XTEST extension could not be opened here, skipped\n");
        else
        {
            const double late = done - job.doneAt;
            printf("  event: a pointer move made by XTEST, a frame painted %.1f ms later\n", late * 1000.0);
            check(zen.paintedFrames() > painted, "a pointer move paints a frame (the loop was waiting with no limit)");
            check(late >= 0.0 && late < 0.05, "and within 50 ms of the move");
        }
    }

    // ── pump() can still be bound ─────────────────────────────────────────────────
    {
        zen.setMaxIdleWait(5);
        const double t0 = time_seconds();
        for (int i = 0; i < 20; ++i) zen.pump();
        const double took = time_seconds() - t0;
        check(took < 1.0, "setMaxIdleWait(5): twenty passes with nothing to do come back in well under a second");
    }

    if (gFailures)
    {
        printf("test_zen_idle: %d failure(s)\n", gFailures);
        return 1;
    }
    printf("test_zen_idle: all checks passed\n");
    return 0;
}
