 #pragma once
#include "Widget.hpp"
#include "IconAtlas.hpp"
#include "ItemModel.hpp"
#include <ct/function.hpp>
#include <igui/widgets/String.hpp>
#include <variant>
#include <ct/vector.hpp>


namespace ig { namespace retained
{
// ─────────────────────────────────────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────────────────────────────────────
class ColorPicker;
class PropertyGrid;

// ═════════════════════════════════════════════════════════════════════════════
//  TreeNode — node in a tree hierarchy (not a Widget)
// ═════════════════════════════════════════════════════════════════════════════
class TreeNode
{
public:
    explicit TreeNode(const String& text);
    ~TreeNode();

    /// @brief Add a child node with the given text.
    TreeNode*  addChild(const String& text);
    /// @brief Remove a child by index.
    void       removeChild(int index);
    /// @brief Remove all children.
    void       clear();
    /// @brief Set the icon by name.
    void       setIcon(const String& name);

    /// @brief Get the node's display text.
    const String&             text()        const { return text_; }
    /// @brief Check if the node has children.
    bool                           hasChildren() const { return !children_.empty(); }
    /// @brief Get child nodes.
    const ct::Vector<TreeNode*>&  children()    const { return children_; }
    /// @brief Check if the node is expanded.
    bool                           isExpanded()  const { return expanded_; }
    /// @brief Set expanded state.
    void                           setExpanded(bool e) { expanded_ = e; }
    /// @brief Get the icon identifier.
    IconId                         iconId()      const { return iconId_; }
    /// @brief Get the parent node.
    TreeNode*                      parent()            { return parent_; }

private:
    String           text_;
    String           iconStr_;
    IconId                iconId_   = IconId::None;
    TreeNode*             parent_   = nullptr;
    ct::Vector<TreeNode*> children_;
    bool                  expanded_ = true;
};

// ═════════════════════════════════════════════════════════════════════════════
//  TreeView
// ═════════════════════════════════════════════════════════════════════════════
class TreeView : public Widget
{
public:
    TreeView();
    ~TreeView() override;

    /// @brief Add a root-level tree node.
    TreeNode* addRoot(const String& text);
    /// @brief Clear all tree nodes.
    void      clear();
    /// @brief Set the selected node.
    void      setSelectedNode(TreeNode* n);

    /// @brief Set a tree data model.
    void setModel(TreeModel* model);
    /// @brief Get the tree data model.
    TreeModel* model() const { return model_; }

    /// @brief Get the selected node.
    TreeNode*                     selectedNode() const { return selected_; }
    /// @brief Get root-level nodes.
    const ct::Vector<TreeNode*>& roots()        const { return roots_; }

    /// @brief Emitted when the selection changes.
    Signal<TreeNode*> selectionChanged;
    /// @brief Emitted when a node is expanded.
    Signal<TreeNode*> nodeExpanded;
    /// @brief Emitted when a node is collapsed.
    Signal<TreeNode*> nodeCollapsed;

    Vec2f sizeHint()                   const override;
    void  paint(PaintContext& ctx)           override;
    void  onMousePress(MouseEvent& e)        override;
    void  onMouseScroll(MouseEvent& e)       override;

private:
    struct FlatRow { TreeNode* node; int depth; };

    void  rebuildFlat();
    void  flattenNode(TreeNode* n, int depth);
    float maxScroll() const;

    ct::Vector<TreeNode*> roots_;
    ct::Vector<FlatRow>   flatRows_;
    TreeNode*              selected_      = nullptr;
    TreeModel*             model_         = nullptr;
    float                  scrollOffset_  = 0.0f;
    float                  rowHeight_     = 24.0f;
    float                  indent_        = 16.0f;
    bool                   flatDirty_     = true;
};

// ═════════════════════════════════════════════════════════════════════════════
//  PropertyGrid
// ═════════════════════════════════════════════════════════════════════════════
class PropertyGrid : public Widget
{
public:
    PropertyGrid();

    /// @brief Add a section header.
    int addSection  (const String& title);
    /// @brief Add a string property.
    int addString   (const String& name, const String& value,
                     ct::Function<void(const String&)> onChange = nullptr,
                     const String& desc = "");
    /// @brief Add a float property with range.
    int addFloat    (const String& name, float value, float minVal, float maxVal,
                     ct::Function<void(float)> onChange = nullptr,
                     const String& desc = "");
    /// @brief Add an integer property with range.
    int addInt      (const String& name, int value, int minVal, int maxVal,
                     ct::Function<void(int)> onChange = nullptr,
                     const String& desc = "");
    /// @brief Add a boolean toggle property.
    int addBool     (const String& name, bool value,
                     ct::Function<void(bool)> onChange = nullptr,
                     const String& desc = "");
    /// @brief Add a color property with picker.
    int addColor    (const String& name, const Color& value,
                     ct::Function<void(const Color&)> onChange = nullptr,
                     const String& desc = "");
    /// @brief Add a combo-box property.
    int addCombo    (const String& name, const ct::Vector<String>& options,
                     int selected,
                     ct::Function<void(int)> onChange = nullptr,
                     const String& desc = "");
    /// @brief Add a 2D vector property with range.
    int addVec2     (const String& name, float x, float y,
                     float minVal, float maxVal,
                     ct::Function<void(float,float)> onChange = nullptr,
                     const String& desc = "");
    /// @brief Add a 3D vector property with range.
    int addVec3     (const String& name, float x, float y, float z,
                     float minVal, float maxVal,
                     ct::Function<void(float,float,float)> onChange = nullptr,
                     const String& desc = "");
    /// @brief Add a 4D vector property with range.
    int addVec4     (const String& name, float x, float y, float z, float w,
                     float minVal, float maxVal,
                     ct::Function<void(float,float,float,float)> onChange = nullptr,
                     const String& desc = "");
    /// @brief Add a clickable button property.
    int addButton   (const String& name,
                     ct::Function<void()> onClick = nullptr,
                     const String& desc = "");
    /// @brief Add a visual separator row.
    int addSeparator();
    /// @brief Add a min/max range slider property.
    int addRange    (const String& name, float lo, float hi,
                     float minVal, float maxVal,
                     ct::Function<void(float,float)> onChange = nullptr,
                     const String& desc = "");

    /// @brief Set a string property value by row.
    void setString (int row, const String& v);
    /// @brief Set a float property value by row.
    void setFloat  (int row, float v);
    /// @brief Set an integer property value by row.
    void setInt    (int row, int v);
    /// @brief Set a boolean property value by row.
    void setBool   (int row, bool v);
    /// @brief Set a color property value by row.
    void setColor  (int row, const Color& c);
    /// @brief Set a combo-box selection by row.
    void setCombo  (int row, int idx);
    /// @brief Set a Vec2 property value by row.
    void setVec2   (int row, float x, float y);
    /// @brief Set a Vec3 property value by row.
    void setVec3   (int row, float x, float y, float z);
    /// @brief Set a Vec4 property value by row.
    void setVec4   (int row, float x, float y, float z, float w);
    /// @brief Set a range property value by row.
    void setRange  (int row, float lo, float hi);

    /// @brief Emitted when any property value changes.
    Signal<int> propertyChanged;

    Vec2f sizeHint()                    const override;
    void  paint(PaintContext& ctx)            override;
    void  onMousePress(MouseEvent& e)         override;
    void  onMouseRelease(MouseEvent& e)       override;
    void  onMouseMove(MouseEvent& e)          override;
    void  onMouseScroll(MouseEvent& e)        override;
    void  onKeyPress(KeyEvent& e)             override;
    void  onTextInput(KeyEvent& e)            override;

    // Public for ColorPickerPopup_ internal access
    enum class PropType {
        Section, String, Float, Int, Bool, Color,
        Combo, Vec2, Vec3, Vec4, Button, Separator, Range
    };

    // ── Per-type value structs ───────────────────────────────────────────
    struct PropSection  { bool expanded = true; };
    struct PropString   { String value; ct::Function<void(const String&)> onChange; };
    struct PropFloat    { float value = 0.f, min = 0.f, max = 1.f; ct::Function<void(float)> onChange; };
    struct PropInt      { int value = 0, min = 0, max = 100; ct::Function<void(int)> onChange; };
    struct PropBool     { bool value = false; ct::Function<void(bool)> onChange; };
    struct PropColor    { Color value = {255,255,255,255}; ct::Function<void(const Color&)> onChange; };
    struct PropCombo    { ct::Vector<String> options; int index = 0; ct::Function<void(int)> onChange; };
    struct PropVec      { float value[4] = {0,0,0,0}; int components = 2; float min = 0.f, max = 1.f;
                          ct::Function<void(float,float)> onChange2;
                          ct::Function<void(float,float,float)> onChange3;
                          ct::Function<void(float,float,float,float)> onChange4; };
    struct PropButton   { ct::Function<void()> onClick; };
    struct PropSeparator{};
    struct PropRange    { float lo = 0, hi = 1, min = 0, max = 1; ct::Function<void(float,float)> onChange; };

    using PropData = std::variant<PropSection, PropString, PropFloat, PropInt,
                                  PropBool, PropColor, PropCombo, PropVec,
                                  PropButton, PropSeparator, PropRange>;

    struct PropRow {
        PropType    type;
        String name;
        String description;
        PropData    data;
    };

    // ── Typed accessors (convenience) ────────────────────────────────────
    template<typename T> T&       propData(int row)       { return std::get<T>(rows_[row].data); }
    template<typename T> const T& propData(int row) const { return std::get<T>(rows_[row].data); }

    ct::Vector<PropRow> rows_;  // accessible by ColorPickerPopup_
    /// @brief Set the description panel height.
    void setDescHeight(float h) { descHeight_ = h; markDirty(); }

private:
    void  spinStep(int row, int dir);
    void  startEdit(int row);
    void  commitEdit();
    void  cancelEdit();
    void  paintRow(PaintContext& ctx, const Rect& abs, int idx, float y);
    void  paintDescPanel(PaintContext& ctx, const Rect& abs);
    float gridHeight()           const;
    float maxScroll()            const;
    float visibleTotalHeight()   const;
    float totalHeight()          const;
    ct::Vector<int> visibleRows() const;
    int   hitRow(float localY) const;

    mutable ct::Vector<int> cachedVisibleRows_;
    mutable bool             visDirty_ = true;

    int   selectedRow_   = -1;
    int   hoveredRow_    = -1;
    float scrollOffset_  = 0.f;
    float rowHeight_     = 24.f;
    float labelRatio_    = 0.4f;
    float descHeight_    = 0.f;

    bool  editing_       = false;
    int   activeRow_     = -1;
    String editBuf_;
    int   editCursor_    = 0;

    bool  draggingSlider_ = false;
    float dragStartX_     = 0.f;
    float dragStartVal_   = 0.f;
    int   dragComponent_  = 0;
};

// ═════════════════════════════════════════════════════════════════════════════
//  ColorPicker
// ═════════════════════════════════════════════════════════════════════════════
class ColorPicker : public Widget
{
public:
    ColorPicker();

    /// @brief Set the picker color.
    void  setColor(const Color& c);
    /// @brief Get the current color.
    Color color() const;

    /// @brief Show or hide the alpha channel bar.
    void setShowAlpha(bool show) { showAlpha_ = show; markDirty(); }
    /// @brief Check if the alpha bar is shown.
    bool showAlpha() const { return showAlpha_; }

    /// @brief Emitted when the color changes.
    Signal<Color> colorChanged;

    Vec2f sizeHint()              const override;
    void  paint(PaintContext& ctx)      override;
    void  onMousePress(MouseEvent& e)   override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e)    override;

private:
    enum DragTarget { DragNone, DragWheel, DragValueBar, DragAlphaBar };

    float wheelRadius (const Rect& abs) const;
    float wheelCenterX(const Rect& abs) const;
    float wheelCenterY(const Rect& abs) const;
    Rect  valueBarRect(const Rect& abs) const;
    Rect  alphaBarRect(const Rect& abs) const;
    Rect  previewRect (const Rect& abs) const;
    Rect  hexRect     (const Rect& abs) const;

    void updateFromWheel   (float mx, float my, const Rect& abs);
    void updateFromValueBar(float my, const Rect& abs);
    void updateFromAlphaBar(float my, const Rect& abs);

    static Color hsvToRgb(float h, float s, float v, float a);
    static void  rgbToHsv(const Color& c, float& h, float& s, float& v);

    float      hue_        = 0.f;
    float      sat_        = 0.5f;
    float      val_        = 1.f;
    float      alpha_      = 1.f;
    bool       showAlpha_  = false;
    DragTarget dragTarget_ = DragNone;
};

} // namespace retained
} // namespace ig
