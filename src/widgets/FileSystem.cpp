#include "pch.hpp"

// ═════════════════════════════════════════════════════════════════════════════
//  Platform includes
// ═════════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <shlobj.h>   // SHGetFolderPathA
    #include <direct.h>   // _getcwd, _mkdir
#else
    #include <dirent.h>
    #include <sys/stat.h>
    #include <unistd.h>
    #include <pwd.h>
#endif


namespace BuGUI
{

namespace FileSystem {

// ═════════════════════════════════════════════════════════════════════════════
//  Path manipulation (pure string, no I/O)
// ═════════════════════════════════════════════════════════════════════════════

static char pathSep()
{
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}

std::string fileName(const std::string& path)
{
    auto pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

std::string extension(const std::string& path)
{
    std::string name = fileName(path);
    auto pos = name.rfind('.');
    return (pos == std::string::npos || pos == 0) ? "" : name.substr(pos);
}

std::string parentPath(const std::string& path)
{
    std::string p = normalizePath(path);
    if (p.empty()) return ".";
    auto pos = p.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    if (pos == 0) return p.substr(0, 1); // root "/"
    return p.substr(0, pos);
}

std::string joinPath(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    char last = a.back();
    if (last == '/' || last == '\\') return a + b;
    return a + pathSep() + b;
}

std::string normalizePath(const std::string& path)
{
    if (path.empty()) return path;
    std::string result;
    result.reserve(path.size());
    char prev = 0;
    for (char c : path) {
        char sep = (c == '\\') ? '/' : c;
        if (sep == '/' && prev == '/') continue; // collapse //
        result += sep;
        prev = sep;
    }
    // Remove trailing slash (but keep root "/")
    if (result.size() > 1 && result.back() == '/')
        result.pop_back();
    return result;
}

bool isAbsolute(const std::string& path)
{
    if (path.empty()) return false;
#if defined(_WIN32)
    // C:\ or C:/
    if (path.size() >= 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
        return true;
    // UNC path
    if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\')
        return true;
#endif
    return path[0] == '/';
}

// ═════════════════════════════════════════════════════════════════════════════
//  Human-readable helpers
// ═════════════════════════════════════════════════════════════════════════════

std::string humanSize(uint64_t bytes)
{
    char buf[32];
    if (bytes < 1024)
        snprintf(buf, sizeof(buf), "%u B", static_cast<unsigned>(bytes));
    else if (bytes < 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else if (bytes < 1024ULL * 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    else
        snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    return buf;
}

std::string humanDate(time_t t)
{
    if (t == 0) return "";
    struct tm tm;
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min);
    return buf;
}

// ═════════════════════════════════════════════════════════════════════════════
//  POSIX implementation
// ═════════════════════════════════════════════════════════════════════════════

#if !defined(_WIN32)

std::vector<Entry> listDir(const std::string& path)
{
    std::vector<Entry> entries;
    DIR* dir = opendir(path.c_str());
    if (!dir) return entries;

    struct dirent* dp;
    while ((dp = readdir(dir)) != nullptr) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue;

        Entry e;
        e.name = dp->d_name;
        e.path = joinPath(path, e.name);
        e.hidden = (e.name.size() > 0 && e.name[0] == '.');

        struct stat st;
        if (stat(e.path.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode))      e.type = EntryType::Directory;
            else if (S_ISLNK(st.st_mode)) e.type = EntryType::Symlink;
            else if (S_ISREG(st.st_mode)) e.type = EntryType::File;
            else                           e.type = EntryType::Other;
            e.size = static_cast<uint64_t>(st.st_size);
            e.mtime = st.st_mtime;
        }
        entries.push_back(std::move(e));
    }
    closedir(dir);
    return entries;
}

bool exists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool isDir(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

bool isFile(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

uint64_t fileSize(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    return static_cast<uint64_t>(st.st_size);
}

time_t modTime(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    return st.st_mtime;
}

std::string homePath()
{
    const char* h = getenv("HOME");
    if (h) return h;
    struct passwd* pw = getpwuid(getuid());
    return pw ? pw->pw_dir : "/";
}

std::string currentDir()
{
    char buf[4096];
    if (getcwd(buf, sizeof(buf)))
        return buf;
    return ".";
}

bool createDir(const std::string& path)
{
    if (path.empty()) return false;
    if (isDir(path)) return true;

    // Recursive: ensure parent exists
    std::string parent = parentPath(path);
    if (!parent.empty() && parent != path && !isDir(parent)) {
        if (!createDir(parent)) return false;
    }
    return mkdir(path.c_str(), 0755) == 0;
}

bool removeFile(const std::string& path)
{
    return unlink(path.c_str()) == 0;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Win32 implementation
// ═════════════════════════════════════════════════════════════════════════════

#else // _WIN32

std::vector<Entry> listDir(const std::string& path)
{
    std::vector<Entry> entries;
    std::string search = joinPath(path, "*");

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return entries;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        Entry e;
        e.name = fd.cFileName;
        e.path = joinPath(path, e.name);
        e.hidden = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            e.type = EntryType::Directory;
        else
            e.type = EntryType::File;

        ULARGE_INTEGER uli;
        uli.LowPart = fd.nFileSizeLow;
        uli.HighPart = fd.nFileSizeHigh;
        e.size = uli.QuadPart;

        // Convert FILETIME to time_t
        ULARGE_INTEGER ft;
        ft.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        ft.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        e.mtime = static_cast<time_t>((ft.QuadPart - 116444736000000000ULL) / 10000000ULL);

        entries.push_back(std::move(e));
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    return entries;
}

bool exists(const std::string& path)
{
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool isDir(const std::string& path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool isFile(const std::string& path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

uint64_t fileSize(const std::string& path)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad))
        return 0;
    ULARGE_INTEGER uli;
    uli.LowPart = fad.nFileSizeLow;
    uli.HighPart = fad.nFileSizeHigh;
    return uli.QuadPart;
}

time_t modTime(const std::string& path)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad))
        return 0;
    ULARGE_INTEGER ft;
    ft.LowPart = fad.ftLastWriteTime.dwLowDateTime;
    ft.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    return static_cast<time_t>((ft.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

std::string homePath()
{
    char buf[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, buf) == S_OK)
        return buf;
    const char* h = getenv("USERPROFILE");
    return h ? h : "C:\\";
}

std::string currentDir()
{
    char buf[4096];
    if (_getcwd(buf, sizeof(buf)))
        return buf;
    return ".";
}

bool createDir(const std::string& path)
{
    if (path.empty()) return false;
    if (isDir(path)) return true;
    std::string parent = parentPath(path);
    if (!parent.empty() && parent != path && !isDir(parent)) {
        if (!createDir(parent)) return false;
    }
    return CreateDirectoryA(path.c_str(), NULL) != 0;
}

bool removeFile(const std::string& path)
{
    return DeleteFileA(path.c_str()) != 0;
}

#endif // _WIN32

} // namespace FileSystem

} // namespace BuGUI