#pragma once
#include "IconAtlas.hpp"
#include "ItemModel.hpp"
#include "Widget.hpp"
#include <ct/function.hpp>
#include <ct/hashset.hpp>
#include <ct/vector.hpp>
#include <igui/widgets/String.hpp>
#include <numeric>

namespace ig
{
namespace retained
{
// The memos (ElideMemo) of the cells of a grid that are in sight, and of its headers. A table of `rows` by
// `cols`, found by the position of the row in the list (not in the window: a row keeps its slot as the
// list scrolls) and the column, each modulo the table, so as many as the cells in sight and no more:
// what it holds is bounded by them, not by the rows of the grid. A slot is only used while the text, the
// room and the font are the ones it was made for (PaintContext::printElided), so the one a different row
// takes over is made again, and nothing in it can be taken for what is not.
class ElideMemos
{
public:
    /// Makes room for @p rows by @p cols cells in sight. What it holds is lost when it has to grow, or
    /// when it is more than four times what is asked for.
    void reserve(int rows, int cols)
    {
        if (rows < 1) rows = 1;
        if (cols < 1) cols = 1;
        if (rows <= rows_ && cols <= cols_ && rows_ * cols_ <= 4 * rows * cols) return;
        rows_ = rows;
        cols_ = cols;
        cells_.clear();
        cells_.resize(static_cast<size_t>(rows_) * static_cast<size_t>(cols_));
        headers_.clear();
        headers_.resize(static_cast<size_t>(cols_));
    }
    /// The memos of the row at position @p row of the list (never negative): cols() of them, the one
    /// of column c at c % cols() (a paint walks the columns counting round, and divides once a row).
    ElideMemo* row(int row)
    {
        return &cells_[static_cast<size_t>(row % rows_) * static_cast<size_t>(cols_)];
    }
    /// How many columns a row of memos has.
    int cols() const
    {
        return cols_;
    }
    ElideMemo& header(int col)
    {
        return headers_[static_cast<size_t>(col % cols_)];
    }
    /// How many cells it has room for.
    size_t capacity() const
    {
        return cells_.size();
    }

private:
    ct::Vector<ElideMemo> cells_;
    ct::Vector<ElideMemo> headers_;
    int rows_ = 0;
    int cols_ = 0;
};

// ═════════════════════════════════════════════════════════════════════════════
//  DataGrid — spreadsheet table with sortable columns, inline edit,
//             checkboxes, multi-select, zebra stripes
// ═════════════════════════════════════════════════════════════════════════════
class DataGrid : public Widget
{
public:
    IG_WIDGET_CLASS(DataGrid, Widget);

    /// @brief Sort order for column headers.
    enum class SortOrder
    {
        None,
        Ascending,
        Descending
    };

    DataGrid();
    ~DataGrid() override;

    /// @brief Add a column with name, width and sortability.
    int addColumn(const String& name, float width, bool sortable = true);
    /// @brief Set a column's width.
    void setColumnWidth(int col, float w);
    /// @brief The width a column has now, which is what painting, hit testing and the scroll range use:
    ///        its own width, or its share of the room when it stretches.
    float columnWidth(int col) const;
    /// @brief Make a column stretch (like QHeaderView::Stretch): the stretching columns share
    ///        equally the width the other columns leave over in the grid, and follow the grid as
    ///        it is resized. The width given to a column (addColumn, setColumnWidth) is the least
    ///        it gets: when the columns do not fit in the grid the stretching ones keep it, and
    ///        the grid scrolls sideways as it does for columns that do not stretch. A stretching
    ///        column is not resized by dragging, as in Qt: its edge is not a resize edge (a press
    ///        on it is a press on the header, the column's or its neighbour's, wherever it falls).
    ///        The grid's scrollbars are drawn over it, so a column that reaches the right edge is
    ///        under the vertical one. Off by default.
    void setColumnStretch(int col, bool stretch);
    /// @brief Whether a column was asked to stretch (not whether it is the last and stretches
    ///        because of setStretchLastColumn).
    bool columnStretch(int col) const;
    /// @brief Stretch the last column, whichever it is as columns are added (like
    ///        QHeaderView::setStretchLastSection): it is a stretching column for everything
    ///        setColumnStretch says. Off by default.
    void setStretchLastColumn(bool stretch);
    /// @brief Whether the last column stretches because of setStretchLastColumn.
    bool stretchLastColumn() const
    {
        return stretchLast_;
    }
    /// @brief Set a column as read-only.
    void setColumnReadOnly(int col, bool ro);

    /// @brief Add a row with cell values.
    int addRow(const ct::Vector<String>& cells);
    /// @brief Set a cell value.
    void setCell(int row, int col, const String& value);
    /// @brief Get a cell value.
    String cell(int row, int col) const;
    /// @brief Remove a row by index.
    void removeRow(int row);
    /// @brief Clear all rows.
    void clearRows();

    /// @brief Check if a row's checkbox is checked.
    bool isRowChecked(int row) const;
    /// @brief Set a row's checkbox state.
    void setRowChecked(int row, bool checked);
    /// @brief Get indices of all checked rows.
    ct::Vector<int> checkedRows() const;

    /// @brief Select a single row.
    void setSelectedRow(int row);
    /// @brief Deselect all rows.
    void clearSelection();
    /// @brief Check if a row is selected.
    bool isSelected(int row) const;
    /// @brief Get the primary selected row index.
    int selectedRow() const
    {
        return selectedRow_;
    }
    /// @brief Get all selected row indices.
    const ct::Vector<int>& selectedRows() const
    {
        return selectedRows_;
    }
    /// @brief Set a data model (overrides internal row storage).
    void setModel(AbstractItemModel* model);
    /// @brief Get the attached data model.
    AbstractItemModel* model() const
    {
        return model_;
    }
    /// @brief Enable multi-row selection.
    void setMultiSelect(bool m)
    {
        multiSelect_ = m;
    }
    /// @brief Show checkbox column.
    void setShowCheckboxes(bool s)
    {
        showCheckboxes_ = s;
        invalidateColumns();
        markDirty();
    }
    /// @brief Enable alternating row colors.
    void setZebraStripes(bool z)
    {
        zebraStripes_ = z;
        markDirty();
    }
    /// @brief Set the entire grid as read-only.
    void setReadOnly(bool r)
    {
        readOnly_ = r;
    }

    /// @brief Emitted when row selection changes.
    Signal<int> selectionChanged;
    /// @brief Emitted when a column sort is applied.
    Signal<int> columnSorted;
    /// @brief Emitted when a cell is edited (row, col).
    Signal<int, int> cellEdited;
    /// @brief Emitted when a row checkbox changes.
    Signal<int, bool> rowCheckChanged;

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onKeyPress(KeyEvent& e) override;
    void onTextInput(KeyEvent& e) override;

private:
    struct Column
    {
        String name;
        float width = 100.f; // the least it has when it stretches
        float minWidth = 40.f;
        bool sortable = true;
        bool readOnly = false;
        bool stretch = false;
    };
    struct Row
    {
        ct::Vector<String> cells;
        bool checked = false;
    };

    void rebuildSortOrder() const;
    void sortByColumn(int col) const;
    void ensureSortOrder() const
    {
        if (sortDirty_) rebuildSortOrder();
    }
    void startEdit(int row, int col);
    void commitEdit();
    void cancelEdit();
    // The width of every column and their total, worked out once and kept until a column changes:
    // the one place that says how wide a column is (colW_ is read by nothing else).
    void ensureColumns() const;
    void invalidateColumns()
    {
        colDirty_ = true;
    }
    void reserveMemos(const Rect& abs); // sizes memos_ for the cells in sight
    float totalColumnsWidth() const;
    float minColumnsWidth() const; // the checkbox column and the own width of every column
    float columnX(int col) const;
    int hitColumn(float localX) const;
    int hitRow(float localY) const;
    int hitResizeEdge(float localX) const;
    int displayRow(int idx) const;
    float contentX() const
    {
        return showCheckboxes_ ? checkboxColW_ : 0.f;
    }

    ct::Vector<Column> columns_;
    ElideMemos memos_; // what paint worked out for the text of the cells and headers in sight
    bool stretchLast_ = false;
    mutable ct::Vector<float> colW_;
    mutable float colTotal_ = 0.f;  // the checkbox column and every column
    mutable float colGridW_ = -1.f; // the width of the grid colW_ was worked out for
    mutable bool colDirty_ = true;
    ct::Vector<Row> rows_;
    mutable ct::Vector<int> sortedIndices_;
    mutable bool sortDirty_ = true; // rows changed: the order is rebuilt before it is read

    int selectedRow_ = -1;
    ct::Vector<int> selectedRows_;
    ct::HashSet<int> selectedSet_; // O(1) lookup for isSelected
    int hoveredRow_ = -1;
    int hoveredCol_ = -1;

    bool multiSelect_ = false;
    bool showCheckboxes_ = false;
    bool zebraStripes_ = true;
    bool readOnly_ = false;

    float rowHeight_ = 24.f;
    float headerHeight_ = 28.f;
    float checkboxColW_ = 24.f;
    mutable float scrollX_ = 0.f; // kept inside the columns by ensureColumns
    float scrollY_ = 0.f;

    int sortCol_ = -1;
    SortOrder sortOrder_ = SortOrder::None;

    bool editing_ = false;
    int editRow_ = -1;
    int editCol_ = -1;
    String editBuf_;
    int editCursor_ = 0;
    float editScroll_ = 0.f; // how far the text of the edited cell is moved left to keep the cursor in sight

    int resizeCol_ = -1;
    float resizeStartX_ = 0.f;
    float resizeStartW_ = 0.f;

    bool hScrollDrag_ = false;
    float hScrollDragStartX_ = 0.f;
    float hScrollDragStartSX_ = 0.f;

    // Double-click detection (per-instance)
    uint32_t lastClickTime_ = 0;
    int lastClickRow_ = -1;

    // Model/View
    AbstractItemModel* model_ = nullptr;
    AbstractItemModel::Connections modelSlots_;
    void detachModel();

    // Model data accessors (use model if set, otherwise internal rows_)
    int modelRowCount() const;
    int modelColCount() const;
    String modelCell(int row, int col) const;
    // The same text without a copy when the grid holds the rows itself; a model's comes by value
    // into `scratch`.
    const String& cellRef(int row, int col, String& scratch) const;
    String modelHeader(int col) const;
    bool modelIsChecked(int row) const;
    bool modelIsEditable(int row, int col) const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  TreeGrid — hierarchical table (tree + columns)
// ═════════════════════════════════════════════════════════════════════════════
class TreeGrid : public Widget
{
public:
    IG_WIDGET_CLASS(TreeGrid, Widget);

    struct Node
    {
        ct::Vector<String> cells;
        ct::Vector<Node*> children;
        Node* parent = nullptr;
        TreeGrid* owner_ = nullptr;
        int depth = 0;
        bool expanded = true;
        bool readOnly = false;
        IconId iconId = IconId::None;

        /// @brief Add a child node with cell data.
        Node* addChild(const ct::Vector<String>& cellData);
        /// @brief Set whether this node is expanded or collapsed.
        void setExpanded(bool e);
    };

    TreeGrid();
    ~TreeGrid() override;

    /// @brief Add a root-level node with cell data.
    Node* addRoot(const ct::Vector<String>& cells);
    /// @brief Clear all nodes.
    void clearAll();
    /// @brief Get total visible node count.
    int nodeCount() const;
    /// @brief Rebuild the flat list after structural changes.
    void rebuild()
    {
        rebuildFlatList();
        markDirty();
    }
    /// @brief Get the root nodes.
    const ct::Vector<Node*>& roots() const
    {
        return roots_;
    }

    // ── Columns ───────────────────────────────────────────────────────────
    /// @brief Add a column with name, width and sortability.
    int addColumn(const String& name, float width, bool sortable = true);
    /// @brief Set a column as read-only.
    void setColumnReadOnly(int col, bool ro);
    /// @brief The width a column has now, which is what painting, hit testing and the scroll range use:
    ///        its own width, or its share of the room when it stretches.
    float columnWidth(int col) const;
    /// @brief Make a column stretch (like QHeaderView::Stretch): the stretching columns share
    ///        equally the width the other columns leave over in the grid, and follow the grid as
    ///        it is resized. The width given to a column (addColumn) is the least it gets: when
    ///        the columns do not fit in the grid the stretching ones keep it, and the grid scrolls
    ///        sideways as it does for columns that do not stretch. A stretching column is not
    ///        resized by dragging, as in Qt: its edge is not a resize edge (a press on it is a
    ///        press on the header, the column's or its neighbour's, wherever it falls). The
    ///        grid's scrollbars are drawn over it, so a column that reaches the right edge is under
    ///        the vertical one. Off by default.
    void setColumnStretch(int col, bool stretch);
    /// @brief Whether a column was asked to stretch (not whether it is the last and stretches
    ///        because of setStretchLastColumn).
    bool columnStretch(int col) const;
    /// @brief Stretch the last column, whichever it is as columns are added (like
    ///        QHeaderView::setStretchLastSection): it is a stretching column for everything
    ///        setColumnStretch says. Off by default.
    void setStretchLastColumn(bool stretch);
    /// @brief Whether the last column stretches because of setStretchLastColumn.
    bool stretchLastColumn() const
    {
        return stretchLast_;
    }

    // ── Selection ─────────────────────────────────────────────────────────
    /// @brief Deselect all nodes.
    void clearSelection();
    /// @brief Check if a node is selected.
    bool isSelected(Node* n) const;
    /// @brief Get the primary selected node.
    Node* selectedNode() const
    {
        return selectedNode_;
    }
    /// @brief Get all selected nodes.
    const ct::Vector<Node*>& selectedNodes() const
    {
        return selectedNodes_;
    }

    // ── Config ────────────────────────────────────────────────────────────
    /// @brief Enable multi-node selection.
    void setMultiSelect(bool m)
    {
        multiSelect_ = m;
    }
    /// @brief Enable alternating row colors.
    void setZebraStripes(bool z)
    {
        zebraStripes_ = z;
        markDirty();
    }
    /// @brief Set the entire tree as read-only.
    void setReadOnly(bool r)
    {
        readOnly_ = r;
    }
    /// @brief Set the indent width per depth level.
    void setIndentWidth(float w)
    {
        indentWidth_ = w;
    }

    // ── Signals ───────────────────────────────────────────────────────────
    /// @brief Emitted when node selection changes.
    Signal<Node*> selectionChanged;
    /// @brief Emitted when a node is expanded.
    Signal<Node*> nodeExpanded;
    /// @brief Emitted when a node is collapsed.
    Signal<Node*> nodeCollapsed;
    /// @brief Emitted when a column sort is applied.
    Signal<int> columnSorted;
    /// @brief Emitted when a tree cell is edited.
    Signal<Node*, int> cellEdited; // node, col

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onKeyPress(KeyEvent& e) override;
    void onTextInput(KeyEvent& e) override;

private:
    struct Column
    {
        String name;
        float width = 100.f; // the least it has when it stretches
        float minWidth = 40.f;
        bool sortable = true;
        bool readOnly = false;
        bool stretch = false;
    };
    struct FlatEntry
    {
        Node* node;
        int depth;
    };

    void rebuildFlatList() const;
    void flattenNode(Node* n, int depth) const;
    void ensureFlatList() const
    {
        if (flatDirty_) rebuildFlatList();
    }
    void deleteNodes(Node* n);
    void startEdit(Node* node, int col);
    void commitEdit();
    void cancelEdit();
    // The width of every column and their total, worked out once and kept until a column changes:
    // the one place that says how wide a column is (colW_ is read by nothing else).
    void ensureColumns() const;
    void invalidateColumns()
    {
        colDirty_ = true;
    }
    void reserveMemos(const Rect& abs); // sizes memos_ for the cells in sight
    float totalColumnsWidth() const;
    float minColumnsWidth() const; // the own width of every column
    float columnX(int col) const;
    int hitColumn(float localX) const;
    int hitRow(float localY) const;
    int hitResizeEdge(float localX) const;

    ct::Vector<Node*> roots_;
    ct::Vector<Column> columns_;
    ElideMemos memos_; // what paint worked out for the text of the cells and headers in sight
    bool stretchLast_ = false;
    mutable ct::Vector<float> colW_;
    mutable float colTotal_ = 0.f;
    mutable float colGridW_ = -1.f; // the width of the grid colW_ was worked out for
    mutable bool colDirty_ = true;
    mutable ct::Vector<FlatEntry> flatList_;
    mutable bool flatDirty_ = true; // a node was added: the flat list is rebuilt before it is read

    Node* selectedNode_ = nullptr;
    ct::Vector<Node*> selectedNodes_;
    int hoveredRow_ = -1;

    bool multiSelect_ = false;
    bool zebraStripes_ = true;
    bool readOnly_ = false;
    float indentWidth_ = 16.f;

    float rowHeight_ = 24.f;
    float headerHeight_ = 28.f;
    mutable float scrollX_ = 0.f; // kept inside the columns by ensureColumns
    float scrollY_ = 0.f;

    int sortCol_ = -1;
    DataGrid::SortOrder sortOrder_ = DataGrid::SortOrder::None;

    void sortChildren(Node* parent);

    bool editing_ = false;
    Node* editNode_ = nullptr;
    int editCol_ = -1;
    String editBuf_;
    int editCursor_ = 0;
    float editScroll_ = 0.f; // how far the text of the edited cell is moved left to keep the cursor in sight

    int resizeCol_ = -1;
    float resizeStartX_ = 0.f;
    float resizeStartW_ = 0.f;
};

} // namespace retained
} // namespace ig
