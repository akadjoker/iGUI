#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  Modes.hpp — which widget API the consumer is compiling against.
//
//  iGUI ships two independent widget APIs on top of one shared drawing engine
//  (igui_core: DrawList/DrawData/Backend/Color/Math/Syntax):
//
//    IGUI_IMMEDIATEMODE  ig::Context   — immediate mode. Widgets are calls made
//                                        every frame; no objects survive it.
//                                        Library target: igui.
//    IGUI_RETAINEDMODE   ig::retained  — retained mode. Widgets are long-lived
//                                        objects in a tree, driven by events.
//                                        Library target: igui_widgets.
//
//  Neither one includes the other's headers, so a program can select one, the
//  other, or both. Pick by defining a macro before including <igui/All.hpp>
//  (or on the compiler command line / via the CMake options below):
//
//      #define IGUI_IMMEDIATEMODE 1
//      #include <igui/All.hpp>
//
//  Each switch is set to 0 or 1 (a valueless `-DIGUI_IMMEDIATEMODE` is 1 too).
//
//  Defining neither is the same as defining both: the umbrella header then
//  exposes whichever APIs were actually built. See "Using both" below for
//  what changes when both are on.
//
//  ── CMake ───────────────────────────────────────────────────────────────────
//  The IGUI_BUILD_IMMEDIATE / IGUI_BUILD_RETAINED options decide which library
//  gets *compiled*; linking one of these targets defines the matching macro for
//  you, so consumers normally need no #define at all:
//
//      target_link_libraries(app PRIVATE igui::igui)     # immediate only
//      target_link_libraries(app PRIVATE igui::widgets)  # retained only
//      target_link_libraries(app PRIVATE igui::igui igui::widgets)  # both
//
//  ── Using both in one program ───────────────────────────────────────────────
//  Three type names exist in both APIs and mean different things: Theme,
//  GradientStop and TimelineTrack. With a single mode selected, the retained
//  names are hoisted into ig:: and existing code is unaffected. With both
//  modes on, that hoist would be ambiguous, so it is dropped: the retained
//  API stays in its own namespace and is reached explicitly.
//
//      ig::Context ctx;                    // immediate
//      ig::retained::Button button;        // retained
//
//      using namespace ig::retained;       // per-file, in retained code
//
//  IGUI_BOTH_MODES is defined when that applies, so code can adapt:
//
//      #if IGUI_BOTH_MODES
//      using Theme = ig::retained::Theme;
//      #endif
//
//  Color, String, Vec2 and Rect are aliases of one and the same type in both
//  APIs, so they never collide.
// ═════════════════════════════════════════════════════════════════════════════

// ── The switches take a value ────────────────────────────────────────────────
// Each switch is defined to 0 or 1, and that value is what everything below
// tests. `-DIGUI_IMMEDIATEMODE` on a command line also works, since compilers
// define a valueless -D as 1; what is rejected is a switch forced to a truly
// empty value (`-DIGUI_IMMEDIATEMODE=`), because the preprocessor reads that as
// 0 in an `#if` - silently meaning the opposite of what it looks like it says.
#if defined(IGUI_IMMEDIATEMODE) && !defined(IGUI_RETAINEDMODE)
#  define IGUI_RETAINEDMODE 0
#elif defined(IGUI_RETAINEDMODE) && !defined(IGUI_IMMEDIATEMODE)
#  define IGUI_IMMEDIATEMODE 0
#elif !defined(IGUI_IMMEDIATEMODE) && !defined(IGUI_RETAINEDMODE)
// Neither selected: expose everything that was built. The library targets
// define these, so this path is for consumers building the sources directly.
#  define IGUI_IMMEDIATEMODE 1
#  define IGUI_RETAINEDMODE 1
#endif

// Catch a switch defined to an empty value: `IGUI_IMMEDIATEMODE + 0` is a
// syntax error when the macro expands to nothing, whereas 0 and 1 both pass.
// The check must come before the first `#if IGUI_IMMEDIATEMODE` below, where an
// empty value would quietly read as 0.
#if (IGUI_IMMEDIATEMODE + 0 == 0 || IGUI_IMMEDIATEMODE + 0 == 1) \
    && (IGUI_RETAINEDMODE + 0 == 0 || IGUI_RETAINEDMODE + 0 == 1)
// Both switches carry a usable 0/1 value.
#else
#  error "iGUI: define IGUI_IMMEDIATEMODE / IGUI_RETAINEDMODE to 0 or 1, not to an empty value (see igui/Modes.hpp)."
#endif

#if !IGUI_IMMEDIATEMODE && !IGUI_RETAINEDMODE
#  error "iGUI: IGUI_IMMEDIATEMODE and IGUI_RETAINEDMODE are both 0 - select at least one widget API (see igui/Modes.hpp)."
#endif

// ── Derived: are both APIs visible at once? ──────────────────────────────────
// Guards the `using namespace retained` hoist, and lets consumer code branch
// on whether ig::Theme is the immediate or the retained one.
#if IGUI_IMMEDIATEMODE && IGUI_RETAINEDMODE
#  define IGUI_BOTH_MODES 1
#else
#  define IGUI_BOTH_MODES 0
#endif
