#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  pch.hpp — ig::retained widgets precompiled header
//
//  Included automatically by all widgets/src/*.cpp via CMake
//  target_precompile_headers().  Keep to stable, rarely-changing headers.
//  Project headers go first (most likely to benefit from PCH), then STL.
// ═════════════════════════════════════════════════════════════════════════════

// ── ig::retained public API ─────────────────────────────────────────────────────────
#include "RetainedBase.hpp"   // Vec2f/3f/4f, Mat4f, Color, Math, Rect, types
#include "Retained.hpp"        // DrawList, IO, NewFrame/Render, TextureHandle
#include "Widget.hpp"       // Widget base class
#include "Theme.hpp"        // Theme struct (colors, sizes)
#include "WidgetApp.hpp"    // WidgetApp manager
#include "FileSystem.hpp"   // FileSystem::listDir, joinPath, exists…
#include "BuImage.hpp"       // ig::retained::BuImage — CPU pixel buffer
#include "Utf8.hpp"          // utf8Length/Substr/ByteOffset helpers

// ── C++ standard library ─────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <ct/function.hpp>
#include <igui/widgets/String.hpp>
#include <ct/hashmap.hpp>
#include <ct/vector.hpp>

// ig::retained internal code uses bare Color/Vec2f/etc. without ig::retained:: prefix.
using namespace ig::retained;
