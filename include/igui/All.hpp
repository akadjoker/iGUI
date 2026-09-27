#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  All.hpp — one entry point that follows the selected widget API.
//
//  Include this and let the mode switches decide what comes in:
//
//      #define IGUI_IMMEDIATEMODE 1     // or IGUI_RETAINEDMODE, or both
//      #include <igui/All.hpp>
//
//  With neither defined, both APIs come in (see igui/Modes.hpp). Linking
//  igui::igui or igui::widgets sets the matching switch for you, so a CMake
//  consumer usually just includes this header.
//
//  This is deliberately not named igui.hpp: <igui/iGUI.hpp> already exists (the
//  older retained compatibility layer, target igui::legacy, which puts its types
//  at global scope and is not one of the two modes below), and two headers
//  differing only in case cannot coexist in one directory on Windows or macOS.
// ═════════════════════════════════════════════════════════════════════════════

#include "Modes.hpp"

// Shared drawing engine (igui_core): present in every mode.
#include "Color.hpp"
#include "Math.hpp"
#include "Types.hpp"
#include "Backend.hpp"
#include "DrawData.hpp"
#include "Events.hpp"
#include "Syntax.hpp"

#if IGUI_IMMEDIATEMODE
// Immediate mode: ig::Context and the per-frame widget calls. Gui.hpp pulls in
// CodeEditor.hpp, FileDialog.hpp and the immediate ig::Theme itself.
#  include "Gui.hpp"
#endif

#if IGUI_RETAINEDMODE
// Retained mode: the ig::retained widget tree. Hoisted into ig:: unless both
// modes are on, in which case it stays explicit - see Widgets.hpp.
#  include "Widgets.hpp"
#endif
