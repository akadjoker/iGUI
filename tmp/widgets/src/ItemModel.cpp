#include "ItemModel.hpp"
#include <algorithm>
#include <cctype>

// ═════════════════════════════════════════════════════════════════════════════
//  ListModel::sort
// ═════════════════════════════════════════════════════════════════════════════

void ListModel::sort(int column, bool ascending)
{
    if (column < 0 || column >= numCols_) return;

    // Build index vector
    std::vector<int> idx(rows_.size());
    for (int i = 0; i < static_cast<int>(idx.size()); ++i) idx[i] = i;

    std::sort(idx.begin(), idx.end(), [&](int a, int b) {
        const auto& va = (column < static_cast<int>(rows_[a].size())) ? rows_[a][column] : "";
        const auto& vb = (column < static_cast<int>(rows_[b].size())) ? rows_[b][column] : "";
        return ascending ? (va < vb) : (va > vb);
    });

    // Reorder rows and checked according to idx
    std::vector<std::vector<std::string>> newRows(rows_.size());
    std::vector<bool> newChecked(checked_.size());
    for (int i = 0; i < static_cast<int>(idx.size()); ++i) {
        newRows[i]    = std::move(rows_[idx[i]]);
        newChecked[i] = checked_[idx[i]];
    }
    rows_    = std::move(newRows);
    checked_ = std::move(newChecked);
    layoutChanged.emit();
}

// ═════════════════════════════════════════════════════════════════════════════
//  SortFilterProxyModel
// ═════════════════════════════════════════════════════════════════════════════

SortFilterProxyModel::SortFilterProxyModel(AbstractItemModel* source)
    : source_(source)
{
    // Re-invalidate when source changes
    source_->modelReset.connect([this] { invalidate(); });
    source_->rowsInserted.connect([this] { invalidate(); });
    source_->rowsRemoved.connect([this] { invalidate(); });
    source_->layoutChanged.connect([this] { invalidate(); });
}

int SortFilterProxyModel::rowCount(const ModelIndex&) const
{
    rebuild();
    return static_cast<int>(mapping_.size());
}

int SortFilterProxyModel::columnCount(const ModelIndex& parent) const
{
    return source_->columnCount(parent);
}

std::string SortFilterProxyModel::data(const ModelIndex& idx, ItemRole role) const
{
    return source_->data(mapToSource(idx), role);
}

bool SortFilterProxyModel::setData(const ModelIndex& idx, const std::string& value, ItemRole role)
{
    return source_->setData(mapToSource(idx), value, role);
}

std::string SortFilterProxyModel::headerData(int section, bool horizontal) const
{
    return source_->headerData(section, horizontal);
}

int SortFilterProxyModel::flags(const ModelIndex& idx) const
{
    return source_->flags(mapToSource(idx));
}

void SortFilterProxyModel::sort(int column, bool ascending)
{
    sortCol_       = column;
    sortAscending_ = ascending;
    invalidate();
    layoutChanged.emit();
}

ModelIndex SortFilterProxyModel::mapToSource(const ModelIndex& proxyIdx) const
{
    rebuild();
    int row = proxyIdx.row();
    if (row < 0 || row >= static_cast<int>(mapping_.size())) return {};
    return source_->index(mapping_[row], proxyIdx.column());
}

ModelIndex SortFilterProxyModel::mapFromSource(const ModelIndex& sourceIdx) const
{
    rebuild();
    for (int i = 0; i < static_cast<int>(mapping_.size()); ++i)
        if (mapping_[i] == sourceIdx.row())
            return ModelIndex(i, sourceIdx.column());
    return {};
}

void SortFilterProxyModel::invalidate()
{
    dirty_ = true;
    modelReset.emit();
}

static bool containsCI(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
                          needle.begin(), needle.end(),
                          [](char a, char b) {
                              return std::tolower(static_cast<unsigned char>(a))
                                  == std::tolower(static_cast<unsigned char>(b));
                          });
    return it != haystack.end();
}

void SortFilterProxyModel::rebuild() const
{
    if (!dirty_) return;
    dirty_ = false;
    mapping_.clear();

    int rows = source_->rowCount();
    for (int i = 0; i < rows; ++i)
    {
        if (filterCol_ >= 0 && !filterText_.empty()) {
            auto cellVal = source_->data(source_->index(i, filterCol_));
            if (caseSensitive_) {
                if (cellVal.find(filterText_) == std::string::npos) continue;
            } else {
                if (!containsCI(cellVal, filterText_)) continue;
            }
        }
        mapping_.push_back(i);
    }

    // Sort
    if (sortCol_ >= 0) {
        std::sort(mapping_.begin(), mapping_.end(), [&](int a, int b) {
            auto va = source_->data(source_->index(a, sortCol_));
            auto vb = source_->data(source_->index(b, sortCol_));
            return sortAscending_ ? (va < vb) : (va > vb);
        });
    }
}
