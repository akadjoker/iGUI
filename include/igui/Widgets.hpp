#pragma once

// Public entry point for the complete retained widget library.
// The implementation preserves the original retained widget API while the
// renderer/platform integration is being adapted to iGUI backends.
#include "widgets/Widgets.hpp"

namespace ig
{
// Retained widgets are part of the public ig API. Their implementation stays
// isolated while duplicate renderer types are progressively consolidated.
using namespace retained;
}
