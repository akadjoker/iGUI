// The case runner of rounds 7 and 8 (tests/test_retained_round7.cpp, tests/test_retained_round8.cpp): a widget in a holder, a handler
// connected to the signal under test, and something that makes the signal come; run as a CONTROL (the handler only counts) and with
// the handler of the rule (parent->removeChild(widget)), in a frame and outside one. See the top of test_retained_round7.cpp for the
// method. No standard library: ct and C only.
#pragma once

#include "RetainedHarness.hpp"
#include "StageHarness.hpp"

namespace rh
{

struct VictimState
{
    const char* only = nullptr; // --only=TEXT
    int cases = 0;
    bool list = false; // --list
};
inline VictimState& victimState()
{
    static VictimState s;
    return s;
}

inline bool wanted(const char* name)
{
    if (victimState().list)
    {
        printf("%s\n", name);
        return false;
    }
    return !victimState().only || strstr(name, victimState().only) != nullptr;
}

// How a case was told about the widget's later signals: a witness is a signal of the widget (or a global event) that comes after
// the one the handler removes the widget in, when nothing removes it.
struct Witness
{
    int late = 0;
};

// Builds the widget in the holder of the stage.
// make(Stage&) -> W*; hook(W*, kill, late) connects kill to the signal under test and late to the later signals (may do neither);
// drive(Stage&, W*) makes the signal come. events: a global event counter that must come in the control and not in the case.
// killOn: the handler takes the widget out the killOn-th time the signal comes (a drag emits at each move, the first one may be
// the press); the widget must not emit again after it.
template<class W, class Make, class Hook, class Drive>
void victim(const char* name, Make make, Hook hook, Drive drive, int witnesses = 0, const int* events = nullptr, int killOn = 1)
{
    if (!wanted(name)) return;
    ++victimState().cases;
    bool driverOk = true;
    for (int mode = 0; mode < 2; ++mode) // the control: the handler only counts
    {
        Stage st(mode);
        W* w = make(st);
        st.watch(w);
        int calls = 0, late = 0;
        const int e0 = events ? *events : 0;
        hook(w, [&calls](auto&&...) { ++calls; }, [&late](auto&&...) { ++late; });
        drive(st, w);
        st.h.frames(3);
        const bool ok = calls >= killOn && (witnesses == 0 || late >= 1) && (!events || *events > e0);
        check(ok, say((String("CONTROL ") + name + " (the signal comes, and what follows it)").c_str(), mode).c_str());
        if (!ok)
        {
            printf("    signal %d times, witnesses %d (%d wanted), global event %d times\n", calls, late, witnesses, events ? *events - e0 : -1);
            driverOk = false;
        }
    }
    if (!driverOk) return;
    for (int mode = 0; mode < 2; ++mode)
    {
        Stage st(mode);
        W* w = make(st);
        st.watch(w);
        const uint32_t seen = st.app.deletions();
        int calls = 0, late = 0;
        const int e0 = events ? *events : 0;
        Widget* parent = w->parent();
        FloatWindow* floating = parent ? nullptr : widget_cast<FloatWindow>(w); // (a float window is not in a parent: the app removes it)
        hook(w, [&calls, &st, w, parent, floating, killOn](auto&&...) {
                    if (++calls != killOn) return;
                    if (floating) st.app.removeFloat(floating);
                    else
                        parent->removeChild(w);
                }, [&late](auto&&...) { ++late; });
        drive(st, w);
        st.h.frames(3);
        const bool out = st.app.hasGone(w, seen);
        const bool ok = calls == killOn && out && late == 0 && (!events || *events == e0);
        check(ok, say(name, mode).c_str());
        if (!ok) printf("    handler ran %d times, out of the tree %d, later signals %d, global events %d\n", calls, out, late, events ? *events - e0 : 0);
    }
}

// A widget of the holder: its rectangle, then a layout pass so the widget has its geometry.
template<class W, class... A>
W* put(Stage& st, float x, float y, float w, float h, A&&... args)
{
    W* widget = st.holder->createChild<W>(static_cast<A&&>(args)...);
    widget->setRect(Rect(x, y, w, h));
    st.h.relayout();
    return widget;
}

} // namespace rh
