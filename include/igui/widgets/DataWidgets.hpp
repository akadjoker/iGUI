#pragma once
#include "Widget.hpp"
#include "IconAtlas.hpp"
#include "ItemModel.hpp"
#include <functional>
#include <string>
#include <vector>
#include <numeric>
#include <unordered_set>

namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  DataGrid — spreadsheet table with sortable columns, inline edit,
//             checkboxes, multi-select, zebra stripes
// ═════════════════════════════════════════════════════════════════════════════
class DataGrid : public Widget
{
public:
    /// @brief Sort order for column headers.
    enum class SortOrder { None, Ascending, Descending };

    DataGrid();

    /// @brief Add a column with name, width and sortability.
    int  addColumn(const std::string& name, float width, bool sortable = true);
    /// @brief Set a column's width.
    void setColumnWidth(int col, float w);
    /// @brief Set a column as read-only.
    void setColumnReadOnly(int col, bool ro);

    /// @brief Add a row with cell values.
    int         addRow(const std::vector<std::string>& cells);
    /// @brief Set a cell value.
    void        setCell(int row, int col, const std::string& value);
    /// @brief Get a cell value.
    std::string cell(int row, int col) const;
    /// @brief Remove a row by index.
    void        removeRow(int row);
    /// @brief Clear all rows.
    void        clearRows();

    /// @brief Check if a row's checkbox is checked.
    bool             isRowChecked(int row) const;
    /// @brief Set a row's checkbox state.
    void             setRowChecked(int row, bool checked);
    /// @brief Get indices of all checked rows.
    std::vector<int> checkedRows() const;

    /// @brief Select a single row.
    void setSelectedRow(int row);
    /// @brief Deselect all rows.
    void clearSelection();
    /// @brief Check if a row is selected.
    bool isSelected(int row) const;
    /// @brief Get the primary selected row index.
    int  selectedRow() const { return selectedRow_; }
    /// @brief Get all selected row indices.
    const std::vector<int>& selectedRows() const { return selectedRows_; }
    /// @brief Set a data model (overrides internal row storage).
    void setModel(AbstractItemModel* model);
    /// @brief Get the attached data model.
    AbstractItemModel* model() const { return model_; }
    /// @brief Enable multi-row selection.
    void setMultiSelect   (bool m) { multiSelect_    = m; }
    /// @brief Show checkbox column.
    void setShowCheckboxes(bool s) { showCheckboxes_ = s; markDirty(); }
    /// @brief Enable alternating row colors.
    void setZebraStripes  (bool z) { zebraStripes_   = z; markDirty(); }
    /// @brief Set the entire grid as read-only.
    void setReadOnly      (bool r) { readOnly_        = r; }

    /// @brief Emitted when row selection changes.
    Signal<int>       selectionChanged;
    /// @brief Emitted when a column sort is applied.
    Signal<int>       columnSorted;
    /// @brief Emitted when a cell is edited (row, col).
    Signal<int, int>  cellEdited;
    /// @brief Emitted when a row checkbox changes.
    Signal<int, bool> rowCheckChanged;

    Vec2f sizeHint()                    const override;
    void  paint(PaintContext& ctx)            override;
    void  onMousePress(MouseEvent& e)         override;
    void  onMouseRelease(MouseEvent& e)       override;
    void  onMouseMove(MouseEvent& e)          override;
    void  onMouseScroll(MouseEvent& e)        override;
    void  onKeyPress(KeyEvent& e)             override;
    void  onTextInput(KeyEvent& e)            override;

private:
    struct Column {
        std::string name;
        float       width    = 100.f;
        float       minWidth =  40.f;
        bool        sortable = true;
        bool        readOnly = false;
    };
    struct Row {
        std::vector<std::string> cells;
        bool checked = false;
    };

    void  rebuildSortOrder();
    void  sortByColumn(int col);
    void  startEdit(int row, int col);
    void  commitEdit();
    void  cancelEdit();
    float totalColumnsWidth() const;
    float columnX(int col) const;
    int   hitColumn(float localX) const;
    int   hitRow(float localY) const;
    int   hitResizeEdge(float localX) const;
    int   displayRow(int idx) const;
    float contentX() const { return showCheckboxes_ ? checkboxColW_ : 0.f; }

    std::vector<Column> columns_;
    std::vector<Row>    rows_;
    std::vector<int>    sortedIndices_;

    int  selectedRow_   = -1;
    std::vector<int> selectedRows_;
    std::unordered_set<int> selectedSet_;  // O(1) lookup for isSelected
    int  hoveredRow_    = -1;
    int  hoveredCol_    = -1;

    bool multiSelect_    = false;
    bool showCheckboxes_ = false;
    bool zebraStripes_   = true;
    bool readOnly_       = false;

    float rowHeight_     = 24.f;
    float headerHeight_  = 28.f;
    float checkboxColW_  = 24.f;
    float scrollX_       = 0.f;
    float scrollY_       = 0.f;

    int       sortCol_   = -1;
    SortOrder sortOrder_ = SortOrder::None;

    bool        editing_    = false;
    int         editRow_    = -1;
    int         editCol_    = -1;
    std::string editBuf_;
    int         editCursor_ = 0;

    int   resizeCol_    = -1;
    float resizeStartX_ = 0.f;
    float resizeStartW_ = 0.f;

    bool  hScrollDrag_        = false;
    float hScrollDragStartX_  = 0.f;
    float hScrollDragStartSX_ = 0.f;

    // Double-click detection (per-instance)
    uint32_t lastClickTime_ = 0;
    int      lastClickRow_  = -1;

    // Model/View
    AbstractItemModel* model_ = nullptr;

    // Model data accessors (use model if set, otherwise internal rows_)
    int   modelRowCount() const;
    int   modelColCount() const;
    std::string modelCell(int row, int col) const;
    std::string modelHeader(int col) const;
    bool  modelIsChecked(int row) const;
    bool  modelIsEditable(int row, int col) const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  TreeGrid — hierarchical table (tree + columns)
// ═════════════════════════════════════════════════════════════════════════════
class TreeGrid : public Widget
{
public:
    struct Node {
        std::vector<std::string> cells;
        std::vector<Node*>       children;
        Node*      parent   = nullptr;
        TreeGrid*  owner_   = nullptr;
        int        depth    = 0;
        bool       expanded = true;
        bool       readOnly = false;
        IconId     iconId   = IconId::None;

        /// @brief Add a child node with cell data.
        Node* addChild(const std::vector<std::string>& cellData);
        /// @brief Set whether this node is expanded or collapsed.
        void  setExpanded(bool e);
    };

    TreeGrid();
    ~TreeGrid() override;

    /// @brief Add a root-level node with cell data.
    Node* addRoot(const std::vector<std::string>& cells);
    /// @brief Clear all nodes.
    void  clearAll();
    /// @brief Get total visible node count.
    int   nodeCount() const;
    /// @brief Rebuild the flat list after structural changes.
    void  rebuild() { rebuildFlatList(); markDirty(); }
    /// @brief Get the root nodes.
    const std::vector<Node*>& roots() const { return roots_; }

    // ── Columns ───────────────────────────────────────────────────────────
    /// @brief Add a column with name, width and sortability.
    int  addColumn(const std::string& name, float width, bool sortable = true);
    /// @brief Set a column as read-only.
    void setColumnReadOnly(int col, bool ro);

    // ── Selection ─────────────────────────────────────────────────────────
    /// @brief Deselect all nodes.
    void clearSelection();
    /// @brief Check if a node is selected.
    bool isSelected(Node* n) const;
    /// @brief Get the primary selected node.
    Node* selectedNode() const { return selectedNode_; }
    /// @brief Get all selected nodes.
    const std::vector<Node*>& selectedNodes() const { return selectedNodes_; }

    // ── Config ────────────────────────────────────────────────────────────
    /// @brief Enable multi-node selection.
    void setMultiSelect  (bool m) { multiSelect_  = m; }
    /// @brief Enable alternating row colors.
    void setZebraStripes (bool z) { zebraStripes_ = z; markDirty(); }
    /// @brief Set the entire tree as read-only.
    void setReadOnly     (bool r) { readOnly_      = r; }
    /// @brief Set the indent width per depth level.
    void setIndentWidth  (float w){ indentWidth_   = w; }

    // ── Signals ───────────────────────────────────────────────────────────
    /// @brief Emitted when node selection changes.
    Signal<Node*>       selectionChanged;
    /// @brief Emitted when a node is expanded.
    Signal<Node*>       nodeExpanded;
    /// @brief Emitted when a node is collapsed.
    Signal<Node*>       nodeCollapsed;
    /// @brief Emitted when a column sort is applied.
    Signal<int>         columnSorted;
    /// @brief Emitted when a tree cell is edited.
    Signal<Node*, int>  cellEdited;   // node, col

    Vec2f sizeHint()                    const override;
    void  paint(PaintContext& ctx)            override;
    void  onMousePress(MouseEvent& e)         override;
    void  onMouseRelease(MouseEvent& e)       override;
    void  onMouseMove(MouseEvent& e)          override;
    void  onMouseScroll(MouseEvent& e)        override;
    void  onKeyPress(KeyEvent& e)             override;
    void  onTextInput(KeyEvent& e)            override;

private:
    struct Column {
        std::string name;
        float       width    = 100.f;
        float       minWidth =  40.f;
        bool        sortable = true;
        bool        readOnly = false;
    };
    struct FlatEntry { Node* node; int depth; };

    void  rebuildFlatList();
    void  flattenNode(Node* n, int depth);
    void  deleteNodes(Node* n);
    void  startEdit(Node* node, int col);
    void  commitEdit();
    void  cancelEdit();
    float totalColumnsWidth() const;
    float columnX(int col) const;
    int   hitColumn(float localX) const;
    int   hitRow(float localY) const;
    int   hitResizeEdge(float localX) const;

    std::vector<Node*>    roots_;
    std::vector<Column>   columns_;
    std::vector<FlatEntry> flatList_;

    Node* selectedNode_  = nullptr;
    std::vector<Node*> selectedNodes_;
    int   hoveredRow_    = -1;

    bool  multiSelect_   = false;
    bool  zebraStripes_  = true;
    bool  readOnly_      = false;
    float indentWidth_   = 16.f;

    float rowHeight_     = 24.f;
    float headerHeight_  = 28.f;
    float scrollX_       = 0.f;
    float scrollY_       = 0.f;

    int   sortCol_       = -1;
    DataGrid::SortOrder sortOrder_ = DataGrid::SortOrder::None;

    void  sortChildren(Node* parent);

    bool        editing_    = false;
    Node*       editNode_   = nullptr;
    int         editCol_    = -1;
    std::string editBuf_;
    int         editCursor_ = 0;

    int   resizeCol_    = -1;
    float resizeStartX_ = 0.f;
    float resizeStartW_ = 0.f;
};

} // namespace BuGUI
