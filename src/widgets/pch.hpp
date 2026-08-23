#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  pch.hpp — BuGUI widgets precompiled header
//
//  Included automatically by all widgets/src/*.cpp via CMake
//  target_precompile_headers().  Keep to stable, rarely-changing headers.
//  Project headers go first (most likely to benefit from PCH), then STL.
// ═════════════════════════════════════════════════════════════════════════════

// ── BuGUI public API ─────────────────────────────────────────────────────────
#include "BuGUI_base.hpp"   // Vec2f/3f/4f, Mat4f, Color, Math, Rect, types
#include "BuGUI.hpp"        // DrawList, IO, NewFrame/Render, TextureHandle
#include "Widget.hpp"       // Widget base class
#include "Theme.hpp"        // Theme struct (colors, sizes)
#include "WidgetApp.hpp"    // WidgetApp manager
#include "FileSystem.hpp"   // FileSystem::listDir, joinPath, exists…
#include "BuImage.hpp"       // BuGUI::BuImage — CPU pixel buffer
#include "Utf8.hpp"          // utf8Length/Substr/ByteOffset helpers

// ── C++ standard library ─────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// BuGUI internal code uses bare Color/Vec2f/etc. without BuGUI:: prefix.
using namespace BuGUI;
