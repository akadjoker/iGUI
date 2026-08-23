#pragma once

#include "FileSystem.hpp"

// ═════════════════════════════════════════════════════════════════════════════
//  DirCache — Cached directory tree for fast browsing & search
//
//  Arena-allocated nodes — zero individual allocs per file entry.
//  Nodes are pooled in blocks of 256; the whole tree is freed at once.
//  Lazy scanning: each folder only hits the OS on first access.
//
//  Usage:
//    DirCache cache;
//    cache.setRoot("/home/user/project");
//    auto* node = cache.root();
//    for (auto* child : node->children())
//        if (child->isDir()) child->expand();
//    auto hits = cache.search("*.png");
//    cache.invalidate("/home/user/project/textures");
// ═════════════════════════════════════════════════════════════════════════════

namespace BuGUI
{
class DirCache
{
public:
    // ── DirNode — single node in the cached tree ─────────────────────────
    struct DirNode
    {
        std::string name;
        std::string path;
        std::string ext;        // lowercase, with dot: ".png"
        uint64_t    size    = 0;
        time_t      mtime   = 0;
        DirNode*    parent  = nullptr;
        DirNode**   kids    = nullptr;   // pointer into kidsBuf_
        int         kidCount = 0;
        bool        isDir   = false;
        bool        hidden  = false;
        bool        scanned = false;

        /// @brief Get children, expanding the directory on first call.
        DirNode** children(int& outCount);

        /// @brief Get the number of child directories.
        int dirCount() const;
        /// @brief Get the number of child files.
        int fileCount() const;
    };

    DirCache();
    ~DirCache();

    // Non-copyable
    DirCache(const DirCache&) = delete;
    DirCache& operator=(const DirCache&) = delete;

    /// @brief Set the root directory path and reset the cache.
    void setRoot(const std::string& path);
    /// @brief Get the root directory path.
    const std::string& rootPath() const { return rootPath_; }

    /// @brief Get the root node of the cached tree.
    DirNode* root();

    /// @brief Navigate to a path, expanding intermediates as needed.
    DirNode* navigate(const std::string& path);

    /// @brief Expand a directory node (no-op if already scanned).
    void expand(DirNode* node);

    // ── Search ───────────────────────────────────────────────────────────
    using SearchResult = std::vector<DirNode*>;
    /// @brief Search for files matching a glob pattern.
    SearchResult search(const std::string& pattern, int maxResults = 200) const;
    /// @brief Search for files by extension (e.g. ".png").
    SearchResult searchByExt(const std::string& ext, int maxResults = 200) const;

    /// @brief Recursively scan directories up to the given depth.
    void deepScan(DirNode* node, int maxDepth = 4);

    // ── Cache management ─────────────────────────────────────────────────
    /// @brief Invalidate the cache for a specific path.
    void invalidate(const std::string& path);
    /// @brief Clear the entire directory cache.
    void clearCache();

    /// @brief Set whether hidden files are included.
    void setShowHidden(bool s) { showHidden_ = s; }
    /// @brief Get whether hidden files are included.
    bool showHidden() const    { return showHidden_; }

    /// @brief Get the total number of cached nodes.
    int cachedNodeCount() const { return nodeCount_; }
    /// @brief Get the number of cached directories.
    int cachedDirCount() const  { return dirCount_; }

private:
    // ── Arena pool ───────────────────────────────────────────────────────
    static constexpr int kBlockSize = 256;

    struct Block {
        DirNode  nodes[kBlockSize];
        Block*   next = nullptr;
    };

    Block*   firstBlock_  = nullptr;
    int      blockUsed_   = kBlockSize; // force alloc on first use

    DirNode* allocNode();

    // Kids buffer — flat array of DirNode* pointers, grown as needed
    DirNode**  kidsBuf_     = nullptr;
    int        kidsBufSize_ = 0;
    int        kidsBufUsed_ = 0;

    DirNode** allocKids(int count);

    // ── Tree data ────────────────────────────────────────────────────────
    std::string rootPath_;
    DirNode*    root_       = nullptr;
    bool        showHidden_ = false;
    int         nodeCount_  = 0;
    int         dirCount_   = 0;

    // Path → node index (fast lookup)
    std::unordered_map<std::string, DirNode*> index_;

    void doScan(DirNode* node);
    void indexNode(DirNode* n);

    void collectSearch(DirNode* n, const std::string& pattern,
                       bool isExtSearch, SearchResult& out, int maxResults) const;
    static bool globMatch(const std::string& pattern, const std::string& text);
    static std::string toLower(const std::string& s);

    void freeArena();
};

} // namespace BuGUI
