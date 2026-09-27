#pragma once

// Public entry point for the complete retained widget library.
// The implementation preserves the original retained widget API while the
// renderer/platform integration is being adapted to iGUI backends.
#include "Modes.hpp"

#if !IGUI_RETAINEDMODE
#  error "iGUI: <igui/Widgets.hpp> is the retained API, but IGUI_RETAINEDMODE is 0 (see igui/Modes.hpp)."
#endif

#include "widgets/Widgets.hpp"

namespace ig
{
// Retained widgets are part of the public ig API. Their implementation stays
// isolated while duplicate renderer types are progressively consolidated.
//
// The hoist only happens when retained is the only widget API in play. With
// both modes on, Theme, GradientStop and TimelineTrack exist in each API with
// different meanings, and hoisting would make those three ambiguous at every
// use - so the retained API then stays in ig::retained:: and is named
// explicitly (or pulled in per-file with `using namespace ig::retained;`).
#if !IGUI_BOTH_MODES
using namespace retained;
#endif
}
