#pragma once

#include "Signal.hpp"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <any>

namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  Model/View Architecture  (Qt-inspired)
//
//  AbstractItemModel   → interface for data sources
//  ListModel           → flat list (rows × columns)
//  TreeModel           → hierarchical data
//  ModelIndex           → lightweight handle into the model
//  SortFilterProxyModel → filter/sort wrapper
//
//  DataGrid, TreeView, ListView attach to a model via setModel().
//  The model emits signals when data changes; views react automatically.
// ═════════════════════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────────────────────
//  ModelIndex — a (row, column, parentPtr) triple identifying one cell
// ─────────────────────────────────────────────────────────────────────────────
class ModelIndex
{
public:
    ModelIndex() = default;
    ModelIndex(int row, int col, void* internal = nullptr)
        : row_(row), col_(col), internal_(internal) {}

    /// @brief Get the row index.
    int   row()      const { return row_; }
    /// @brief Get the column index.
    int   column()   const { return col_; }
    /// @brief Get the internal data pointer.
    void* internal() const { return internal_; }

    /// @brief Check if the index points to a valid cell.
    bool isValid() const { return row_ >= 0 && col_ >= 0; }

    bool operator==(const ModelIndex& o) const
    { return row_ == o.row_ && col_ == o.col_ && internal_ == o.internal_; }

    bool operator!=(const ModelIndex& o) const { return !(*this == o); }

private:
    int   row_      = -1;
    int   col_      = -1;
    void* internal_ = nullptr;  // opaque node pointer for trees
};

// ─────────────────────────────────────────────────────────────────────────────
//  Data roles
// ─────────────────────────────────────────────────────────────────────────────
enum class ItemRole
{
    Display  = 0,    // string shown in the cell
    Edit     = 1,    // editable string (defaults to Display)
    ToolTip  = 2,
    Icon     = 3,    // icon name (string)
    Check    = 4,    // "0" / "1"
    UserRole = 256,  // start of custom roles
};

// ═════════════════════════════════════════════════════════════════════════════
//  AbstractItemModel — base class for all data models
// ═════════════════════════════════════════════════════════════════════════════
class AbstractItemModel
{
public:
    virtual ~AbstractItemModel() = default;

    /// @brief Get the row count under a parent index.
    virtual int  rowCount(const ModelIndex& parent = {})    const = 0;
    /// @brief Get the column count under a parent index.
    virtual int  columnCount(const ModelIndex& parent = {}) const = 0;

    /// @brief Retrieve cell data for a given role.
    virtual std::string data(const ModelIndex& index,
                              ItemRole role = ItemRole::Display) const = 0;

    /// @brief Set cell data (returns true on success).
    virtual bool setData(const ModelIndex& index, const std::string& value,
                          ItemRole role = ItemRole::Edit)
    { (void)index; (void)value; (void)role; return false; }

    /// @brief Get the header label for a section.
    virtual std::string headerData(int section, bool horizontal = true) const
    { (void)horizontal; return std::to_string(section); }

    /// @brief Get item flags for a cell index.
    enum Flag { None = 0, Selectable = 1, Editable = 2, Checkable = 4, Enabled = 8 };
    virtual int flags(const ModelIndex& index) const
    { (void)index; return Selectable | Enabled; }

    /// @brief Get the parent index for a child.
    virtual ModelIndex parent(const ModelIndex& index)  const
    { (void)index; return {}; }

    virtual ModelIndex index(int row, int col,
                              const ModelIndex& parent = {}) const
    { (void)parent; return ModelIndex(row, col); }

    virtual bool hasChildren(const ModelIndex& parent) const
    { return rowCount(parent) > 0; }

    /// @brief Sort the model by a column.
    virtual void sort(int column, bool ascending = true)
    { (void)column; (void)ascending; }

    /// @brief Notify that a full model reset is about to happen.
    void beginResetModel() {}
    /// @brief Emit the modelReset signal after a full reset.
    void endResetModel()   { modelReset.emit(); }

    void beginInsertRows(const ModelIndex& parent, int first, int last)
    { (void)parent; (void)first; (void)last; }
    void endInsertRows() { rowsInserted.emit(); }

    void beginRemoveRows(const ModelIndex& parent, int first, int last)
    { (void)parent; (void)first; (void)last; }
    void endRemoveRows() { rowsRemoved.emit(); }

    /// @brief Emitted after a full model reset.
    Signal<>                   modelReset;
    /// @brief Emitted after rows are inserted.
    Signal<>                   rowsInserted;
    /// @brief Emitted after rows are removed.
    Signal<>                   rowsRemoved;
    /// @brief Emitted when a single cell changes.
    Signal<ModelIndex>         dataChanged;
    /// @brief Emitted when sort/reorder occurs.
    Signal<>                   layoutChanged;
};

// ═════════════════════════════════════════════════════════════════════════════
//  ListModel — simple flat list backed by vector<vector<string>>
// ═════════════════════════════════════════════════════════════════════════════
class ListModel : public AbstractItemModel
{
public:
    ListModel() = default;
    explicit ListModel(int columns) : numCols_(columns) {}

    // ── Structure ─────────────────────────────────────────────────────────
    int rowCount(const ModelIndex& = {})    const override { return static_cast<int>(rows_.size()); }
    int columnCount(const ModelIndex& = {}) const override { return numCols_; }

    // ── Data ──────────────────────────────────────────────────────────────
    std::string data(const ModelIndex& idx, ItemRole role = ItemRole::Display) const override
    {
        if (role == ItemRole::Check && idx.row() >= 0 && idx.row() < static_cast<int>(checked_.size()))
            return checked_[idx.row()] ? "1" : "0";
        if (idx.row() < 0 || idx.row() >= static_cast<int>(rows_.size())) return {};
        if (idx.column() < 0 || idx.column() >= static_cast<int>(rows_[idx.row()].size())) return {};
        return rows_[idx.row()][idx.column()];
    }

    bool setData(const ModelIndex& idx, const std::string& value,
                  ItemRole role = ItemRole::Edit) override
    {
        if (role == ItemRole::Check) {
            if (idx.row() >= 0 && idx.row() < static_cast<int>(checked_.size())) {
                checked_[idx.row()] = (value == "1");
                dataChanged.emit(idx);
                return true;
            }
            return false;
        }
        if (idx.row() < 0 || idx.row() >= static_cast<int>(rows_.size())) return false;
        if (idx.column() < 0 || idx.column() >= static_cast<int>(rows_[idx.row()].size())) return false;
        rows_[idx.row()][idx.column()] = value;
        dataChanged.emit(idx);
        return true;
    }

    // ── Headers ───────────────────────────────────────────────────────────
    std::string headerData(int section, bool horizontal = true) const override
    {
        if (horizontal && section >= 0 && section < static_cast<int>(headers_.size()))
            return headers_[section];
        return AbstractItemModel::headerData(section, horizontal);
    }

    void setHeaders(const std::vector<std::string>& h) { headers_ = h; }

    // ── Flags ─────────────────────────────────────────────────────────────
    int flags(const ModelIndex& idx) const override
    {
        int f = Selectable | Enabled;
        if (!readOnly_.empty() && idx.column() >= 0 &&
            idx.column() < static_cast<int>(readOnly_.size()) && !readOnly_[idx.column()])
            f |= Editable;
        else if (readOnly_.empty())
            f |= Editable;
        if (checkable_) f |= Checkable;
        return f;
    }

    /// @brief Add a row with cell data.
    int addRow(const std::vector<std::string>& cells)
    {
        beginInsertRows({}, static_cast<int>(rows_.size()), static_cast<int>(rows_.size()));
        rows_.push_back(cells);
        checked_.push_back(false);
        endInsertRows();
        return static_cast<int>(rows_.size()) - 1;
    }

    /// @brief Remove a row by index.
    void removeRow(int row)
    {
        if (row < 0 || row >= static_cast<int>(rows_.size())) return;
        beginRemoveRows({}, row, row);
        rows_.erase(rows_.begin() + row);
        checked_.erase(checked_.begin() + row);
        endRemoveRows();
    }

    void clear()
    {
        beginResetModel();
        rows_.clear();
        checked_.clear();
        endResetModel();
    }

    /// @brief Set a column as read-only.
    void setColumnReadOnly(int col, bool ro)
    {
        if (static_cast<int>(readOnly_.size()) <= col)
            readOnly_.resize(col + 1, false);
        readOnly_[col] = ro;
    }

    /// @brief Enable checkboxes for all rows.
    void setCheckable(bool c) { checkable_ = c; }

    // ── Sorting ───────────────────────────────────────────────────────────
    void sort(int column, bool ascending = true) override;

    /// @brief Get all rows as string vectors.
    const std::vector<std::vector<std::string>>& rows() const { return rows_; }

private:
    int numCols_ = 0;
    std::vector<std::string>              headers_;
    std::vector<std::vector<std::string>> rows_;
    std::vector<bool>                     checked_;
    std::vector<bool>                     readOnly_;
    bool checkable_ = false;
};

// ═════════════════════════════════════════════════════════════════════════════
//  TreeModel — hierarchical model backed by a node tree
// ═════════════════════════════════════════════════════════════════════════════
class TreeModel : public AbstractItemModel
{
public:
    struct Node {
        std::vector<std::string> cells;
        std::vector<Node*>       children;
        Node*                    parent  = nullptr;
        bool                     expanded = true;
        std::string              iconName;

        ~Node() { for (auto* c : children) delete c; }

        Node* addChild(const std::vector<std::string>& data)
        {
            auto* n = new Node();
            n->cells  = data;
            n->parent = this;
            children.push_back(n);
            return n;
        }
    };

    TreeModel() = default;
    explicit TreeModel(int columns) : numCols_(columns) {}
    ~TreeModel() override { clear(); }

    // ── Structure ─────────────────────────────────────────────────────────
    int rowCount(const ModelIndex& parent = {}) const override
    {
        if (!parent.isValid()) return static_cast<int>(roots_.size());
        auto* pn = nodeFromIndex(parent);
        return pn ? static_cast<int>(pn->children.size()) : 0;
    }

    int columnCount(const ModelIndex& = {}) const override { return numCols_; }

    // ── Data ──────────────────────────────────────────────────────────────
    std::string data(const ModelIndex& idx, ItemRole role = ItemRole::Display) const override
    {
        (void)role;
        auto* n = nodeFromIndex(idx);
        if (!n || idx.column() < 0 || idx.column() >= static_cast<int>(n->cells.size())) return {};
        return n->cells[idx.column()];
    }

    bool setData(const ModelIndex& idx, const std::string& value, ItemRole = ItemRole::Edit) override
    {
        auto* n = nodeFromIndex(idx);
        if (!n || idx.column() < 0 || idx.column() >= static_cast<int>(n->cells.size())) return false;
        n->cells[idx.column()] = value;
        dataChanged.emit(idx);
        return true;
    }

    // ── Headers ───────────────────────────────────────────────────────────
    std::string headerData(int section, bool horizontal = true) const override
    {
        if (horizontal && section >= 0 && section < static_cast<int>(headers_.size()))
            return headers_[section];
        return AbstractItemModel::headerData(section, horizontal);
    }

    void setHeaders(const std::vector<std::string>& h) { headers_ = h; }

    // ── Hierarchy ─────────────────────────────────────────────────────────
    ModelIndex parent(const ModelIndex& idx) const override
    {
        auto* n = nodeFromIndex(idx);
        if (!n || !n->parent) return {};
        auto* pp = n->parent->parent;
        const auto& siblings = pp ? pp->children : roots_;
        for (int i = 0; i < static_cast<int>(siblings.size()); ++i)
            if (siblings[i] == n->parent) return indexFromNode(n->parent, i);
        return {};
    }

    ModelIndex index(int row, int col, const ModelIndex& parent = {}) const override
    {
        const auto& children = parent.isValid()
            ? nodeFromIndex(parent)->children : roots_;
        if (row < 0 || row >= static_cast<int>(children.size())) return {};
        return ModelIndex(row, col, children[row]);
    }

    bool hasChildren(const ModelIndex& parent) const override
    {
        if (!parent.isValid()) return !roots_.empty();
        auto* n = nodeFromIndex(parent);
        return n && !n->children.empty();
    }

    // ── Convenience mutations ─────────────────────────────────────────────
    Node* addRoot(const std::vector<std::string>& cells)
    {
        auto* n = new Node();
        n->cells = cells;
        beginInsertRows({}, static_cast<int>(roots_.size()), static_cast<int>(roots_.size()));
        roots_.push_back(n);
        endInsertRows();
        return n;
    }

    void clear()
    {
        beginResetModel();
        for (auto* r : roots_) delete r;
        roots_.clear();
        endResetModel();
    }

    // ── Node ↔ Index ──────────────────────────────────────────────────────
    Node* nodeFromIndex(const ModelIndex& idx) const
    {
        return static_cast<Node*>(idx.internal());
    }

    ModelIndex indexFromNode(Node* n, int row = -1) const
    {
        if (!n) return {};
        if (row < 0) {
            const auto& siblings = n->parent ? n->parent->children : roots_;
            for (int i = 0; i < static_cast<int>(siblings.size()); ++i)
                if (siblings[i] == n) { row = i; break; }
        }
        return ModelIndex(row, 0, n);
    }

    const std::vector<Node*>& roots() const { return roots_; }

private:
    int numCols_ = 0;
    std::vector<std::string> headers_;
    std::vector<Node*>       roots_;
};

// ═════════════════════════════════════════════════════════════════════════════
//  SortFilterProxyModel — wraps another model with sort + text filter
// ═════════════════════════════════════════════════════════════════════════════
class SortFilterProxyModel : public AbstractItemModel
{
public:
    explicit SortFilterProxyModel(AbstractItemModel* source);
    ~SortFilterProxyModel() override = default;

    void setFilterColumn(int col)           { filterCol_ = col; invalidate(); }
    void setFilterText(const std::string& t){ filterText_ = t; invalidate(); }
    void setFilterCaseSensitive(bool cs)    { caseSensitive_ = cs; invalidate(); }

    // ── AbstractItemModel interface ───────────────────────────────────────
    int  rowCount(const ModelIndex& parent = {})    const override;
    int  columnCount(const ModelIndex& parent = {}) const override;
    std::string data(const ModelIndex& idx, ItemRole role = ItemRole::Display) const override;
    bool setData(const ModelIndex& idx, const std::string& value, ItemRole role = ItemRole::Edit) override;
    std::string headerData(int section, bool horizontal = true) const override;
    int  flags(const ModelIndex& idx) const override;

    void sort(int column, bool ascending = true) override;

    // Map between proxy and source indices
    ModelIndex mapToSource(const ModelIndex& proxyIdx) const;
    ModelIndex mapFromSource(const ModelIndex& sourceIdx) const;

    AbstractItemModel* sourceModel() const { return source_; }

private:
    void invalidate();
    void rebuild() const;

    AbstractItemModel*    source_;
    int                   filterCol_      = -1;
    std::string           filterText_;
    bool                  caseSensitive_  = false;
    int                   sortCol_        = -1;
    bool                  sortAscending_  = true;
    mutable bool          dirty_          = true;
    mutable std::vector<int> mapping_;  // proxy row → source row
};

} // namespace BuGUI
