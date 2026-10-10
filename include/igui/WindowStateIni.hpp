#pragma once

#include <ct/ini.hpp>

#include "Types.hpp"

// ═════════════════════════════════════════════════════════════════════════════
//  Window state persistence - the .ini an application keeps its windows in
//
//  Both widget APIs remember what the user did to their windows and write it
//  to one file, the way ImGui does:
//
//    immediate mode   Context::setWindowStatePath("app.ini")
//    retained         WidgetApp::setWindowStatePath("app.ini")
//
//  A window is stored under a section named after its title, holding its
//  position, its size and whether it was minimized/maximized. A dock layout is
//  stored the same way, under a "dock:" section named after the panel's
//  settings key. Nothing happens until a path is set, and a window the file
//  does not know keeps the geometry the application asked for.
//
//  Desktop only. A browser tab has no file system to keep the file in, and an
//  Android app has no writable working directory - a relative path would land
//  in "/" and silently fail to open. Both therefore neither read nor write the
//  file; define IGUI_WINDOW_STATE_INI to 1 or 0 to force the choice.
// ═════════════════════════════════════════════════════════════════════════════

#ifndef IGUI_WINDOW_STATE_INI
#  if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
#    define IGUI_WINDOW_STATE_INI 0
#  else
#    define IGUI_WINDOW_STATE_INI 1
#  endif
#endif

namespace ig
{

// ─────────────────────────────────────────────────────────────────────────────
//  Names
// ─────────────────────────────────────────────────────────────────────────────
/// @brief Section name a window is stored under, "" when it cannot be stored.
///        ']' and newlines cannot appear in a section name, and ct::Ini refuses
///        - aborts on - one with a leading or trailing space or tab (which the
///        parser would trim anyway, so it would not read back): all of them fold
///        to '_'. A title with nothing but blanks has no section at all.
inline String windowStateSection(StringView title)
{
    String section;
    section.reserve(title.size());
    for (String::size_type i = 0; i < title.size(); ++i)
    {
        const char c = title[i];
        section.push_back(c == ']' || c == '\n' || c == '\r' ? '_' : c);
    }
    if (section.empty() || section.find_first_not_of(" \t") == String::npos)
        return String();
    if (section.front() == ' ' || section.front() == '\t')
        section[0] = '_';
    if (section.back() == ' ' || section.back() == '\t')
        section[section.size() - 1] = '_';
    return section;
}

/// @brief True when ct::Ini accepts @p name as a section: it aborts on a name
///        with a ']', a newline or a blank at either end, and one the parser
///        would trim cannot be read back. Everything reaching ct::Ini goes
///        through here, so a stray name can never abort the application.
inline bool windowStateSectionUsable(StringView name)
{
    if (name.empty() || name.front() == ' ' || name.front() == '\t' ||
        name.back() == ' ' || name.back() == '\t')
        return false;
    for (StringView::size_type i = 0; i < name.size(); ++i)
    {
        const char c = name[i];
        if (c == ']' || c == '\n' || c == '\r' || c == '\0')
            return false;
    }
    return true;
}

/// @brief Section name a dock layout is stored under, "" when it has no key.
inline String dockStateSection(StringView key)
{
    if (key.empty())
        return String();
    String section = "dock:";
    section.append(key.data(), key.size());
    return windowStateSection(section);
}

// ─────────────────────────────────────────────────────────────────────────────
//  WindowGeometry - what a window keeps between runs
// ─────────────────────────────────────────────────────────────────────────────
/// @brief Position, size and minimized/maximized state of one window.
struct WindowGeometry
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    bool  minimized = false;
    bool  maximized = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  WindowStateIni - windows and dock layouts of one application
// ─────────────────────────────────────────────────────────────────────────────
class WindowStateIni
{
public:
    /// @brief Point at @p path and forget the file read before ("" turns
    ///        persistence off). The file itself is only touched by load()/save().
    void setPath(const String& path)
    {
        path_ = path;
        ini_.clear();
    }

    /// @brief Path set by setPath() ("" when persistence is off).
    const String& path() const { return path_; }

    /// @brief Read the file. False when persistence is off or the file is
    ///        missing - there is then simply nothing to apply.
    bool load()
    {
        if (path_.empty())
            return false;
        ini_.clear();
#if IGUI_WINDOW_STATE_INI
        return ini_.load(path_);
#else
        return false;
#endif
    }

    /// @brief Write the file. False when persistence is off or nothing was
    ///        ever stored, so an absent file is not created empty and an
    ///        existing one is not truncated.
    bool save() const
    {
        if (path_.empty() || ini_.empty())
            return false;
#if IGUI_WINDOW_STATE_INI
        return ini_.save(path_);
#else
        return false;
#endif
    }

    /// @brief True when nothing is known about any window.
    bool empty() const { return ini_.empty(); }
    /// @brief Number of stored sections (windows and dock layouts).
    ct::Ini::size_type size() const { return ini_.size(); }

    // ── Windows ──────────────────────────────────────────────────────────
    /// @brief Store @p geometry under a window @p title.
    void putWindow(StringView title, const WindowGeometry& geometry)
    {
        const String section = windowStateSection(title);
        if (section.empty())
            return;
        ini_.set(section.c_str(), "pos_x", static_cast<double>(geometry.x));
        ini_.set(section.c_str(), "pos_y", static_cast<double>(geometry.y));
        ini_.set(section.c_str(), "size_w", static_cast<double>(geometry.w));
        ini_.set(section.c_str(), "size_h", static_cast<double>(geometry.h));
        ini_.set(section.c_str(), "minimized", geometry.minimized);
        ini_.set(section.c_str(), "maximized", geometry.maximized);
    }

    /// @brief Read what putWindow() stored for a window @p title.
    /// @return false when the window is unknown (the first-run case); fields
    ///         the file omits keep the values already in @p out.
    bool getWindow(StringView title, WindowGeometry& out) const
    {
        const String section = windowStateSection(title);
        if (section.empty() || !ini_.has_section(section))
            return false;
        out.x = static_cast<float>(ini_.get_double(section, "pos_x", out.x));
        out.y = static_cast<float>(ini_.get_double(section, "pos_y", out.y));
        out.w = static_cast<float>(ini_.get_double(section, "size_w", out.w));
        out.h = static_cast<float>(ini_.get_double(section, "size_h", out.h));
        out.minimized = ini_.get_bool(section, "minimized", out.minimized);
        out.maximized = ini_.get_bool(section, "maximized", out.maximized);
        return true;
    }

    // ── Any other section (dock layouts) ─────────────────────────────────
    /// @brief Store a value. An empty section is ignored (ct::Ini aborts on it).
    void put(const String& section, const char* key, const String& value)
    {
        if (!windowStateSectionUsable(section) || !key || !*key)
            return;
        ini_.set(section.c_str(), key, value);
    }

    /// @brief Read a value, or @p fallback when the section or key is absent.
    String get(const String& section, const char* key, const char* fallback = "") const
    {
        if (!windowStateSectionUsable(section) || !key || !*key)
            return String(fallback);
        return ini_.get(section, key, fallback);
    }

    /// @brief Read a number, or @p fallback when the section or key is absent.
    double getNumber(const String& section, const char* key, double fallback) const
    {
        if (!windowStateSectionUsable(section) || !key || !*key)
            return fallback;
        return ini_.get_double(section, key, fallback);
    }

    /// @brief Store a number. An empty section is ignored (ct::Ini aborts on it).
    void putNumber(const String& section, const char* key, double value)
    {
        if (!windowStateSectionUsable(section) || !key || !*key)
            return;
        ini_.set(section.c_str(), key, value);
    }

    /// @brief True when @p section holds @p key.
    bool has(const String& section, const char* key) const
    {
        if (!windowStateSectionUsable(section) || !key || !*key)
            return false;
        return ini_.has(section, key);
    }

private:
    String  path_;
    ct::Ini ini_;
};

} // namespace ig
