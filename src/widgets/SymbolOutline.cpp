#include "SymbolOutline.hpp"
#include "WidgetApp.hpp"
#include "pch.hpp"

namespace ig { namespace retained
{

namespace
{

const char* iconFor(syntax::SymbolKind kind)
{
    switch (kind)
    {
        case syntax::SymbolKind::Namespace: return "folder";
        case syntax::SymbolKind::Class:
        case syntax::SymbolKind::Struct:
        case syntax::SymbolKind::Interface: return "book";
        case syntax::SymbolKind::Enum: return "star";
        case syntax::SymbolKind::Function: return "file_code";
        case syntax::SymbolKind::Method: return "gear";
        case syntax::SymbolKind::Variable:
        case syntax::SymbolKind::Constant: return "minus";
        case syntax::SymbolKind::Label: return "arrow_right";
        case syntax::SymbolKind::Import: return "file";
    }
    return "file";
}

bool grouped(syntax::SymbolKind kind)
{
    return kind == syntax::SymbolKind::Import;
}

} // namespace

SymbolOutline::SymbolOutline() = default;

SymbolOutline::~SymbolOutline()
{
    detach();
}

void SymbolOutline::detach()
{
    if (!editor_) return;
    editor_->textChanged.disconnect(textSlot_);
    editor_->cursorMoved.disconnect(cursorSlot_);
    editor_->destroyed.disconnect(destroyedSlot_);
    editor_ = nullptr;
}

void SymbolOutline::setEditor(CodeEditor* editor)
{
    if (editor == editor_) return;
    detach();
    editor_ = editor;
    rows_.clear();
    nodes_.clear();
    clear();
    dirty_ = true;
    cursorDirty_ = true;
    if (!editor_) return;
    // Only flags here: the work waits for the next paint, once per frame
    // however many keys were typed.
    textSlot_ = editor_->textChanged.connect([this] { dirty_ = true; cursorDirty_ = true; markDirty(); });
    cursorSlot_ = editor_->cursorMoved.connect([this](TextEdit::TextPos) { cursorDirty_ = true; markDirty(); });
    // An editor deleted before the outline: its signals go with it, so only
    // the pointer is dropped, and the rows with it.
    destroyedSlot_ = editor_->destroyed.connect([this] {
        editor_ = nullptr;
        rows_.clear();
        nodes_.clear();
        clear();
    });
    markDirty();
}

void SymbolOutline::refresh()
{
    if (!editor_) return;
    const ct::Vector<syntax::Symbol>& symbols = editor_->symbols();
    bool same = symbols.size() == rows_.size();
    for (size_t i = 0; same && i < symbols.size(); ++i)
        same = symbols[i].kind == rows_[i].kind && symbols[i].parent == rows_[i].parent && symbols[i].name == rows_[i].name;
    if (!same) rebuild(symbols);
    dirty_ = false;
    if (followsCursor_) syncToCursor();
    cursorDirty_ = false;
}

void SymbolOutline::rebuild(const ct::Vector<syntax::Symbol>& symbols)
{
    syncing_ = true;
    clear();
    rows_.clear();
    nodes_.clear();
    rows_.reserve(symbols.size());
    nodes_.reserve(symbols.size());
    TreeNode* imports = nullptr;
    for (size_t i = 0; i < symbols.size(); ++i)
    {
        const syntax::Symbol& s = symbols[i];
        TreeNode* node = nullptr;
        if (grouped(s.kind) && s.parent < 0)
        {
            if (!imports)
            {
                imports = addRoot("imports");
                imports->setIcon("folder");
                imports->setExpanded(false);
            }
            node = imports->addChild(s.name);
        }
        else
        {
            TreeNode* parent = s.parent >= 0 ? nodes_[static_cast<size_t>(s.parent)] : nullptr;
            node = parent ? parent->addChild(s.name) : addRoot(s.name);
        }
        node->setIcon(iconFor(s.kind));
        nodes_.push_back(node);
        rows_.push_back({ s.kind, s.name, s.parent });
    }
    TreeNode::bumpRevision();
    syncing_ = false;
}

void SymbolOutline::syncToCursor()
{
    const int index = syntax::enclosingSymbol(editor_->symbols(), editor_->cursorPos().line);
    TreeNode* node = index >= 0 ? nodes_[static_cast<size_t>(index)] : nullptr;
    if (selectedNode() == node) return;
    syncing_ = true;
    setSelectedNode(node);
    syncing_ = false;
}

TreeNode* SymbolOutline::nodeOf(int symbolIndex) const
{
    if (symbolIndex < 0 || symbolIndex >= static_cast<int>(nodes_.size())) return nullptr;
    return nodes_[static_cast<size_t>(symbolIndex)];
}

int SymbolOutline::symbolOf(const TreeNode* node) const
{
    for (size_t i = 0; i < nodes_.size(); ++i)
        if (nodes_[i] == node) return static_cast<int>(i);
    return -1;
}

void SymbolOutline::paint(PaintContext& ctx)
{
    if (!visible_) return;
    if (editor_ && (dirty_ || (cursorDirty_ && followsCursor_))) refresh();
    TreeView::paint(ctx);
}

void SymbolOutline::onMousePress(MouseEvent& e)
{
    // A press on a fold triangle bumps the node revision and changes no
    // selection: that is not a navigation. A press on the selected row is.
    const unsigned revision = TreeNode::revision();
    TreeView::onMousePress(e);
    if (!e.consumed || revision != TreeNode::revision() || !editor_) return;
    const int index = symbolOf(selectedNode());
    if (index < 0) return;
    const syntax::Symbol& s = editor_->symbols()[static_cast<size_t>(index)];
    editor_->reveal(s.line, s.indent);
    WidgetApp::instance().setFocused(editor_);
    activated.emit(index);
}

}} // namespace ig::retained
