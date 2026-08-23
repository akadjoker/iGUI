#include "DirCache.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

// ═════════════════════════════════════════════════════════════════════════════
//  DirNode
// ═════════════════════════════════════════════════════════════════════════════

DirCache::DirNode** DirCache::DirNode::children(int& outCount)
{
    outCount = kidCount;
    return kids;
}

int DirCache::DirNode::dirCount() const
{
    int n = 0;
    for (int i = 0; i < kidCount; ++i)
        if (kids[i]->isDir) ++n;
    return n;
}

int DirCache::DirNode::fileCount() const
{
    int n = 0;
    for (int i = 0; i < kidCount; ++i)
        if (!kids[i]->isDir) ++n;
    return n;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Arena
// ═════════════════════════════════════════════════════════════════════════════

DirCache::DirNode* DirCache::allocNode()
{
    if (blockUsed_ >= kBlockSize) {
        Block* b = static_cast<Block*>(std::malloc(sizeof(Block)));
        b->next = firstBlock_;
        firstBlock_ = b;
        blockUsed_ = 0;
        // Placement-construct all nodes in the block
        for (int i = 0; i < kBlockSize; ++i)
            new (&b->nodes[i]) DirNode();
    }
    return &firstBlock_->nodes[blockUsed_++];
}

DirCache::DirNode** DirCache::allocKids(int count)
{
    if (count <= 0) return nullptr;
    int need = kidsBufUsed_ + count;
    if (need > kidsBufSize_) {
        int newSize = std::max(need, kidsBufSize_ * 2);
        if (newSize < 512) newSize = 512;
        DirNode** newBuf = static_cast<DirNode**>(std::malloc(newSize * sizeof(DirNode*)));
        if (kidsBuf_) {
            std::memcpy(newBuf, kidsBuf_, kidsBufUsed_ * sizeof(DirNode*));
            std::free(kidsBuf_);
        }
        kidsBuf_ = newBuf;
        kidsBufSize_ = newSize;
    }
    DirNode** ptr = kidsBuf_ + kidsBufUsed_;
    kidsBufUsed_ += count;
    return ptr;
}

void DirCache::freeArena()
{
    // Destruct strings in all used nodes
    Block* b = firstBlock_;
    while (b) {
        Block* next = b->next;
        int used = (b == firstBlock_) ? blockUsed_ : kBlockSize;
        for (int i = 0; i < used; ++i) {
            b->nodes[i].name.~basic_string();
            b->nodes[i].path.~basic_string();
            b->nodes[i].ext.~basic_string();
        }
        std::free(b);
        b = next;
    }
    firstBlock_ = nullptr;
    blockUsed_ = kBlockSize;

    if (kidsBuf_) {
        std::free(kidsBuf_);
        kidsBuf_ = nullptr;
    }
    kidsBufSize_ = 0;
    kidsBufUsed_ = 0;
}

// ═════════════════════════════════════════════════════════════════════════════
//  DirCache
// ═════════════════════════════════════════════════════════════════════════════

DirCache::DirCache() {}

DirCache::~DirCache()
{
    freeArena();
}

void DirCache::setRoot(const std::string& path)
{
    clearCache();
    rootPath_ = FileSystem::normalizePath(path);
}

DirCache::DirNode* DirCache::root()
{
    if (!root_) {
        if (rootPath_.empty() || !FileSystem::isDir(rootPath_))
            return nullptr;

        root_ = allocNode();
        root_->name  = FileSystem::fileName(rootPath_);
        root_->path  = rootPath_;
        root_->isDir = true;
        indexNode(root_);
        ++nodeCount_;
        ++dirCount_;
        doScan(root_);
    }
    return root_;
}

void DirCache::expand(DirNode* node)
{
    if (!node || !node->isDir || node->scanned) return;
    doScan(node);
}

void DirCache::doScan(DirNode* node)
{
    if (!node->isDir) return;

    auto entries = FileSystem::listDir(node->path);

    // Sort: directories first, then alphabetical
    std::sort(entries.begin(), entries.end(),
        [](const FileSystem::Entry& a, const FileSystem::Entry& b) {
            bool aDir = (a.type == FileSystem::EntryType::Directory);
            bool bDir = (b.type == FileSystem::EntryType::Directory);
            if (aDir != bDir) return aDir > bDir;
            return a.name < b.name;
        });

    // Filter hidden if needed
    int count = 0;
    for (auto& e : entries) {
        if (!showHidden_ && e.hidden) continue;
        ++count;
    }

    node->kids = allocKids(count);
    node->kidCount = 0;

    for (auto& e : entries) {
        if (!showHidden_ && e.hidden) continue;

        DirNode* child = allocNode();
        child->name   = e.name;
        child->path   = e.path;
        child->isDir  = (e.type == FileSystem::EntryType::Directory);
        child->size   = e.size;
        child->mtime  = e.mtime;
        child->hidden = e.hidden;
        child->parent = node;

        if (!child->isDir)
            child->ext = toLower(FileSystem::extension(e.name));

        node->kids[node->kidCount++] = child;
        indexNode(child);
        ++nodeCount_;
        if (child->isDir) ++dirCount_;
    }

    node->scanned = true;
}

DirCache::DirNode* DirCache::navigate(const std::string& path)
{
    std::string norm = FileSystem::normalizePath(path);

    auto it = index_.find(norm);
    if (it != index_.end()) return it->second;

    if (!root()) return nullptr;
    if (norm.find(rootPath_) != 0) return nullptr;

    std::string rel = norm.substr(rootPath_.size());
    if (!rel.empty() && rel[0] == '/') rel = rel.substr(1);

    DirNode* cur = root_;
    if (rel.empty()) return cur;

    size_t pos = 0;
    while (pos < rel.size()) {
        size_t slash = rel.find('/', pos);
        std::string seg = (slash == std::string::npos)
            ? rel.substr(pos) : rel.substr(pos, slash - pos);
        pos = (slash == std::string::npos) ? rel.size() : slash + 1;

        if (!cur->scanned) doScan(cur);

        DirNode* found = nullptr;
        for (int i = 0; i < cur->kidCount; ++i) {
            if (cur->kids[i]->name == seg) { found = cur->kids[i]; break; }
        }
        if (!found) return nullptr;
        cur = found;
    }

    return cur;
}

// ── Search ───────────────────────────────────────────────────────────────

DirCache::SearchResult DirCache::search(const std::string& pattern, int maxResults) const
{
    SearchResult out;
    if (root_) collectSearch(root_, toLower(pattern), false, out, maxResults);
    return out;
}

DirCache::SearchResult DirCache::searchByExt(const std::string& ext, int maxResults) const
{
    std::string lext = toLower(ext);
    if (!lext.empty() && lext[0] != '.') lext = "." + lext;
    SearchResult out;
    if (root_) collectSearch(root_, lext, true, out, maxResults);
    return out;
}

void DirCache::deepScan(DirNode* node, int maxDepth)
{
    if (!node || !node->isDir || maxDepth <= 0) return;
    if (!node->scanned) doScan(node);
    for (int i = 0; i < node->kidCount; ++i) {
        if (node->kids[i]->isDir)
            deepScan(node->kids[i], maxDepth - 1);
    }
}

// ── Cache management ─────────────────────────────────────────────────────

void DirCache::invalidate(const std::string& path)
{
    std::string norm = FileSystem::normalizePath(path);
    auto it = index_.find(norm);
    if (it == index_.end()) return;

    DirNode* n = it->second;
    if (n->isDir && n->scanned) {
        // Just mark as not scanned — nodes stay in arena, will be orphaned
        // (arena freed all at once on clearCache)
        n->scanned = false;
        n->kids = nullptr;
        n->kidCount = 0;
    }
}

void DirCache::clearCache()
{
    index_.clear();
    freeArena();
    root_ = nullptr;
    nodeCount_ = 0;
    dirCount_ = 0;
}

// ── Internal helpers ─────────────────────────────────────────────────────

void DirCache::indexNode(DirNode* n)
{
    if (n) index_[n->path] = n;
}

void DirCache::collectSearch(DirNode* n, const std::string& pattern,
                             bool isExtSearch, SearchResult& out, int maxResults) const
{
    if (static_cast<int>(out.size()) >= maxResults) return;

    if (!n->isDir) {
        if (isExtSearch) {
            if (n->ext == pattern)
                out.push_back(n);
        } else {
            if (globMatch(pattern, toLower(n->name)))
                out.push_back(n);
        }
        return;
    }

    if (!n->scanned) return;

    for (int i = 0; i < n->kidCount; ++i) {
        collectSearch(n->kids[i], pattern, isExtSearch, out, maxResults);
        if (static_cast<int>(out.size()) >= maxResults) return;
    }
}

bool DirCache::globMatch(const std::string& pattern, const std::string& text)
{
    int pi = 0, ti = 0;
    int pn = static_cast<int>(pattern.size());
    int tn = static_cast<int>(text.size());
    int starP = -1, starT = -1;

    while (ti < tn) {
        if (pi < pn && (pattern[pi] == '?' || pattern[pi] == text[ti])) {
            ++pi; ++ti;
        } else if (pi < pn && pattern[pi] == '*') {
            starP = pi++; starT = ti;
        } else if (starP >= 0) {
            pi = starP + 1; ti = ++starT;
        } else {
            return false;
        }
    }
    while (pi < pn && pattern[pi] == '*') ++pi;
    return pi == pn;
}

std::string DirCache::toLower(const std::string& s)
{
    std::string r = s;
    for (auto& c : r)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}
