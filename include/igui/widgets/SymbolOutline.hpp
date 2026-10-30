#pragma once

#include "CodeEditor.hpp"
#include "TreePropertyColorWidgets.hpp"

namespace ig { namespace retained
{

// ═════════════════════════════════════════════════════════════════════════════
//  SymbolOutline — the declarations of a CodeEditor's text as a tree
//
//  Classes with their methods and fields, functions, imports under one
//  folded row: the "functions / types" pane of an IDE. A press on a row puts
//  the editor's cursor on the declaration (the row stays active, so a second
//  press on it navigates again), and the row whose block holds the cursor is
//  the selected one while the user moves through the text.
//
//  Cheap by design: the editor scans its symbols again only after an edit;
//  the outline reads them once per painted frame at most, and rebuilds its
//  rows only when the set of declarations changed, so typing inside a method
//  moves line numbers without touching the tree.
//
//  Usage:
//      auto* outline = panel->createChild<SymbolOutline>();
//      outline->setEditor(editor);
// ═════════════════════════════════════════════════════════════════════════════

class SymbolOutline : public TreeView
{
public:
    SymbolOutline();
    ~SymbolOutline() override;

    void setEditor(CodeEditor* editor);
    CodeEditor* editor() const
    {
        return editor_;
    }

    /// @brief Select the row of the symbol whose block holds the cursor
    /// (on by default).
    void setFollowsCursor(bool on)
    {
        followsCursor_ = on;
    }
    bool followsCursor() const
    {
        return followsCursor_;
    }

    /// @brief Rebuild now from the editor's symbols; otherwise it happens on
    /// the next paint after an edit.
    void refresh();

    /// @brief The row of a symbol (an index into editor()->symbols()), or nullptr.
    TreeNode* nodeOf(int symbolIndex) const;
    /// @brief The symbol of a row, or -1 for the group rows.
    int symbolOf(const TreeNode* node) const;

    /// @brief Emitted with the symbol's index when a row is pressed, after
    /// the editor's cursor moved to it.
    Signal<int> activated;

    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;

private:
    void rebuild(const ct::Vector<syntax::Symbol>& symbols);
    void syncToCursor();
    void detach();

    CodeEditor* editor_ = nullptr;
    Signal<>::SlotId textSlot_ = 0;
    Signal<TextEdit::TextPos>::SlotId cursorSlot_ = 0;
    Signal<>::SlotId destroyedSlot_ = 0;
    bool followsCursor_ = true;
    bool dirty_ = true;       // the text changed since the rows were built
    bool cursorDirty_ = true; // the cursor moved since the selection followed it
    bool syncing_ = false;    // a selection made by the outline itself, not a press
    // What the rows were built from, to tell a changed line from a changed outline.
    struct Row
    {
        syntax::SymbolKind kind;
        String name;
        int parent;
    };
    ct::Vector<Row> rows_;
    ct::Vector<TreeNode*> nodes_; // by symbol index
};

}} // namespace ig::retained
