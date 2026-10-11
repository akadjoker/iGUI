// Both widget APIs in one translation unit (IGUI_BOTH_MODES).
//
// This is the case the mode switches exist to keep honest: Theme, GradientStop
// and TimelineTrack are declared by both APIs with different meanings, so
// Widgets.hpp must NOT hoist ig::retained into ig:: here. If that hoist ever
// comes back, every ig::Theme below becomes ambiguous and this file stops
// compiling - which is the point. It also pins the shared types to one identity
// across both APIs, and guards the ct::Hash<String> ODR clash that using both
// headers together used to trigger.
#include <igui/All.hpp>
#include <stdio.h>

static_assert(IGUI_IMMEDIATEMODE == 1, "immediate on");
static_assert(IGUI_RETAINEDMODE == 1, "retained on");
static_assert(IGUI_BOTH_MODES == 1, "both on");

int main()
{
    ig::Theme immediateTheme;              // immediate ig::Theme
    ig::retained::Theme retainedTheme;     // retained, explicit
    ig::GradientStop immStop;
    ig::retained::GradientStop retStop;
    ig::TimelineTrack immTrack;
    ig::retained::TimelineTrack retTrack;

    ig::retained::Widget w;                // retained widget

    // Shared types must be the SAME type in both APIs, not two lookalikes.
    static_assert(sizeof(ig::Color) == sizeof(ig::retained::Color), "shared Color");

    (void)immediateTheme; (void)retainedTheme;
    (void)immStop; (void)retStop; (void)immTrack; (void)retTrack;
    (void)w;
    printf("both modes OK\n");
    return 0;
}
