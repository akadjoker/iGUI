#pragma once

#include <string>
#include <vector>
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
    std::string name;       // filename only (no path)
    std::string path;       // full absolute path
    EntryType   type   = EntryType::File;
    uint64_t    size   = 0; // bytes (0 for directories)
    time_t      mtime  = 0; // last modification time
    bool        hidden = false;
};

// ── Directory listing ────────────────────────────────────────────────────

// List directory contents. Returns empty vector on error.
// Does NOT include "." or "..".
std::vector<Entry> listDir(const std::string& path);

// ── Queries ──────────────────────────────────────────────────────────────

bool        exists(const std::string& path);
bool        isDir(const std::string& path);
bool        isFile(const std::string& path);
uint64_t    fileSize(const std::string& path);
time_t      modTime(const std::string& path);

// ── Path manipulation (pure string, no I/O) ─────────────────────────────

std::string fileName(const std::string& path);     // "foo.cpp"
std::string extension(const std::string& path);    // ".cpp" (with dot)
std::string parentPath(const std::string& path);   // "/home/user/src" → "/home/user"
std::string joinPath(const std::string& a, const std::string& b);
std::string normalizePath(const std::string& path); // collapse "//", trailing "/"
bool        isAbsolute(const std::string& path);

// ── Special directories ─────────────────────────────────────────────────

std::string homePath();     // user home directory
std::string currentDir();   // current working directory

// ── Modification ─────────────────────────────────────────────────────────

bool createDir(const std::string& path);    // mkdir -p (recursive)
bool removeFile(const std::string& path);

// ── Human-readable size ──────────────────────────────────────────────────

std::string humanSize(uint64_t bytes);  // "3.2 KB", "1.5 MB", etc.

// ── Human-readable date ──────────────────────────────────────────────────

std::string humanDate(time_t t);  // "2025-01-15 14:30"

} // namespace FileSystem

} // namespace BuGUI
