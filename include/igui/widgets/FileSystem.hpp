#pragma once

#include <igui/widgets/String.hpp>
#include <ct/vector.hpp>
#include <cstdint>
#include <ctime>


namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  FileSystem — cross-platform file/directory operations
//
//  Uses POSIX (dirent/stat) on Linux, macOS, Android, RPi, Jetson, Emscripten.
//  Uses Win32 API on Windows.
//  No dependency on <filesystem> (not available on Emscripten).
// ═════════════════════════════════════════════════════════════════════════════

namespace FileSystem {

// ── Entry type ───────────────────────────────────────────────────────────

enum class EntryType { File, Directory, Symlink, Other };

struct Entry {
    String name;       // filename only (no path)
    String path;       // full absolute path
    EntryType   type   = EntryType::File;
    uint64_t    size   = 0; // bytes (0 for directories)
    time_t      mtime  = 0; // last modification time
    bool        hidden = false;
};

// ── Directory listing ────────────────────────────────────────────────────

// List directory contents. Returns empty vector on error.
// Does NOT include "." or "..".
ct::Vector<Entry> listDir(const String& path);

// ── Queries ──────────────────────────────────────────────────────────────

bool        exists(const String& path);
bool        isDir(const String& path);
bool        isFile(const String& path);
uint64_t    fileSize(const String& path);
time_t      modTime(const String& path);

// ── Path manipulation (pure string, no I/O) ─────────────────────────────

String fileName(const String& path);     // "foo.cpp"
String extension(const String& path);    // ".cpp" (with dot)
String parentPath(const String& path);   // "/home/user/src" → "/home/user"
String joinPath(const String& a, const String& b);
String normalizePath(const String& path); // collapse "//", trailing "/"
bool        isAbsolute(const String& path);

// ── Special directories ─────────────────────────────────────────────────

String homePath();     // user home directory
String currentDir();   // current working directory

// ── Modification ─────────────────────────────────────────────────────────

bool createDir(const String& path);    // mkdir -p (recursive)
bool removeFile(const String& path);

// ── Human-readable size ──────────────────────────────────────────────────

String humanSize(uint64_t bytes);  // "3.2 KB", "1.5 MB", etc.

// ── Human-readable date ──────────────────────────────────────────────────

String humanDate(time_t t);  // "2025-01-15 14:30"

} // namespace FileSystem

} // namespace BuGUI
