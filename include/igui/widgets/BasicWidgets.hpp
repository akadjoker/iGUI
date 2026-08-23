#pragma once

#include "Widget.hpp"
#include "Theme.hpp"
#include <string>
#include <functional>

namespace BuGUI
{
class RadioButton;

// ═════════════════════════════════════════════════════════════════════════════

class Label : public Widget
{
public:
    explicit Label(const std::string& text = "");

    /// @brief Set the display text.
    void setText(const std::string& t);
    /// @brief Get the current display text.
    const std::string& text() const { return text_; }

    /// @brief Set the text color.
    void setColor(const Color& c);
    /// @brief Set horizontal text alignment.
    void setAlign(TextAlign a);
    /// @brief Get horizontal text alignment.
    TextAlign align() const { return align_; }

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    std::string text_;
    Color color_ = Theme::instance().textColor;
    TextAlign align_ = TextAlign::LEFT;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Spacer — invisible widget that takes up space in layouts
// ═════════════════════════════════════════════════════════════════════════════

class Spacer : public Widget
{
public:
    explicit Spacer(float size = 0.0f);

    Vec2f sizeHint() const override;
    void paint(PaintContext&) override {}  // invisible

private:
    float size_;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Line — visual separator (horizontal or vertical, adapts to parent layout)
// ═════════════════════════════════════════════════════════════════════════════

class Line : public Widget
{
public:
    explicit Line(float thickness = 1.0f);

    /// @brief Set the line thickness in pixels.
    void setThickness(float t) { thickness_ = t; markDirty(); }
    /// @brief Get the line thickness.
    float thickness() const    { return thickness_; }

    /// @brief Set the line color.
    void setColor(const Color& c) { color_ = c; markDirty(); }
    /// @brief Get the line color.
    const Color& color() const    { return color_; }

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    float thickness_;
    Color color_ = Theme::instance().borderColor;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Button
// ═════════════════════════════════════════════════════════════════════════════

class Button : public Widget
{
public:
    explicit Button(const std::string& text = "");

    /// @brief Set the button label text.
    void setText(const std::string& t);
    /// @brief Get the button label text.
    const std::string& text() const { return text_; }

    /// @brief Set the text alignment within the button.
    void setAlign(TextAlign a);
    /// @brief Get the text alignment.
    TextAlign align() const { return align_; }

    /// @brief Atlas icon drawn before the label (IconId::None = no icon).
    void setIconId(IconId id) { iconId_ = id; markDirty(); }
    /// @brief Get the icon identifier.
    IconId iconId() const { return iconId_; }

    /// @brief Set a custom background color (alpha=0 uses theme).
    void setBgColor(const Color& c) { bgColor_ = c; }
    /// @brief Get the custom background color.
    const Color& bgColor() const { return bgColor_; }

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    std::string text_;
    TextAlign align_ = TextAlign::CENTER;
    Color bgColor_ = Color(0, 0, 0, 0);
    IconId iconId_ = IconId::None;
};

// ═════════════════════════════════════════════════════════════════════════════
//  ImageButton — button that draws a textured region from any atlas/texture
//    auto* btn = parent->createChild<ImageButton>();
//    btn->setTexture(atlas->texture(), atlas->textureWidth(), atlas->textureHeight());
//    btn->setSrcRect(atlas->srcRect(IconId::MyIcon));
//    btn->setTint(Color(255,255,255,255)); // optional tint
// ═════════════════════════════════════════════════════════════════════════════

class ImageButton : public Widget
{
public:
    ImageButton() = default;

    /// @brief Set the texture, dimensions for UV calculation.
    void setTexture(BuGUI::TextureHandle tex, int texW, int texH)
    { tex_ = tex; texW_ = texW; texH_ = texH; markDirty(); }
    /// @brief Get the texture handle.
    BuGUI::TextureHandle texture() const { return tex_; }

    /// @brief Source pixel rect within the texture.
    void setSrcRect(const FloatRect& r) { srcRect_ = r; markDirty(); }
    /// @brief Get the source rect.
    const FloatRect& srcRect() const { return srcRect_; }

    /// @brief Icon padding inside the button (px from each edge).
    void setIconPadding(float p) { iconPad_ = p; markDirty(); }
    /// @brief Get the icon padding.
    float iconPadding() const { return iconPad_; }

    /// @brief Tint color multiplied with texture (white = no tint).
    void setTint(const Color& c) { tint_ = c; markDirty(); }
    /// @brief Get the tint color.
    const Color& tint() const { return tint_; }

    /// @brief Custom background color (alpha=0 uses theme).
    void setBgColor(const Color& c) { bgColor_ = c; }
    /// @brief Get the background color.
    const Color& bgColor() const { return bgColor_; }

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    BuGUI::TextureHandle tex_;
    int       texW_    = 0;
    int       texH_    = 0;
    FloatRect srcRect_ = {0, 0, 0, 0};
    float     iconPad_ = 3.0f;
    Color     tint_    = Color(255, 255, 255, 255);
    Color     bgColor_ = Color(0, 0, 0, 0);
};

// ═════════════════════════════════════════════════════════════════════════════
//  IconButton — button that draws a built-in icon using lines/shapes
//    Supports animated transitions (e.g. hamburger ↔ X)
// ═════════════════════════════════════════════════════════════════════════════

class IconButton : public Widget
{
public:
    enum Icon {
        Hamburger,   // ≡  three horizontal lines
        Close,       // ×  X mark
        ArrowLeft,   // ←
        ArrowRight,  // →
        ArrowUp,     // ↑
        ArrowDown,   // ↓
        Plus,        // +
        Minus,       // −
        Check,       // ✓
        Chevron,     // >  (rotates with direction)
    };

    explicit IconButton(Icon icon = Hamburger);

    /// @brief Set the displayed icon.
    void setIcon(Icon icon);
    /// @brief Get the current icon.
    Icon icon() const { return icon_; }

    /// @brief Enable animated transitions between icons.
    void setAnimated(bool a) { animated_ = a; }
    /// @brief Check if icon transitions are animated.
    bool isAnimated() const  { return animated_; }

    /// @brief Set the icon drawing color.
    void setIconColor(const Color& c) { iconColor_ = c; markDirty(); }
    /// @brief Get the icon color.
    const Color& iconColor() const    { return iconColor_; }

    /// @brief Set the icon stroke width.
    void setLineWidth(float w) { lineWidth_ = w; markDirty(); }
    /// @brief Get the icon stroke width.
    float lineWidth() const    { return lineWidth_; }

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    Icon  icon_       = Hamburger;
    Icon  targetIcon_ = Hamburger;
    bool  animated_   = false;
    float animProgress_ = 1.0f;
    Color iconColor_  = Theme::instance().textColor;
    float lineWidth_  = 2.0f;

    void drawIcon(PaintContext& ctx, const Rect& abs, Icon icon, float t);
};

// ═════════════════════════════════════════════════════════════════════════════
//  Panel — container with background
// ═════════════════════════════════════════════════════════════════════════════

class Panel : public Widget
{
public:
    Panel() = default;

    /// @brief Set the panel background color.
    void setBgColor(const Color& c);
    /// @brief Get the panel background color.
    const Color& bgColor() const { return bgColor_; }

    void layout() override;
    void paint(PaintContext& ctx) override;

private:
    Color bgColor_ = Theme::instance().panelColor;
};

// ═════════════════════════════════════════════════════════════════════════════
//  CheckBox
// ═════════════════════════════════════════════════════════════════════════════

class CheckBox : public Widget
{
public:
    explicit CheckBox(const std::string& text = "");

    /// @brief Set the checkbox label text.
    void setText(const std::string& t);
    /// @brief Get the checkbox label text.
    const std::string& text() const { return text_; }
    /// @brief Check if the checkbox is checked.
    bool isChecked() const { return checked_; }
    /// @brief Set the checked state programmatically.
    void setChecked(bool c);

    /// @brief Emitted when the checked state changes.
    Signal<bool> toggled;

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    std::string text_;
    bool checked_ = false;
};

// ═════════════════════════════════════════════════════════════════════════════
//  RadioGroup — manages mutual exclusion for a set of RadioButtons
// ═════════════════════════════════════════════════════════════════════════════

class RadioGroup
{
public:
    RadioGroup() = default;
    ~RadioGroup();

    /// @brief Add a radio button to this group.
    void add(RadioButton* rb);
    /// @brief Remove a radio button from this group.
    void remove(RadioButton* rb);

    /// @brief Get the currently selected radio button.
    RadioButton* selected() const { return selected_; }
    /// @brief Get the index of the selected button (-1 if none).
    int selectedIndex() const;

    /// @brief Emitted when the selected button changes.
    Signal<int> selectionChanged;

private:
    friend class RadioButton;
    std::vector<RadioButton*> buttons_;
    RadioButton* selected_ = nullptr;
    void select(RadioButton* rb);
};

// ═════════════════════════════════════════════════════════════════════════════
//  RadioButton
// ═════════════════════════════════════════════════════════════════════════════

class RadioButton : public Widget
{
public:
    explicit RadioButton(const std::string& text = "", RadioGroup* group = nullptr);
    ~RadioButton() override;

    /// @brief Set the radio button label text.
    void setText(const std::string& t) { text_ = t; markDirty(); }
    /// @brief Get the radio button label text.
    const std::string& text() const    { return text_; }

    /// @brief Check if this radio button is selected.
    bool isSelected() const { return selected_; }
    /// @brief Set the selected state.
    void setSelected(bool s);

    /// @brief Assign this button to a RadioGroup.
    void setGroup(RadioGroup* g);
    /// @brief Get the owning RadioGroup.
    RadioGroup* group() const { return group_; }

    /// @brief Emitted when selection state changes.
    Signal<bool> toggled;

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    friend class RadioGroup;
    std::string text_;
    bool selected_    = false;
    RadioGroup* group_ = nullptr;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Switch — on/off toggle
// ═════════════════════════════════════════════════════════════════════════════

class Switch : public Widget
{
public:
    explicit Switch(const std::string& text = "");

    /// @brief Set the switch label text.
    void setText(const std::string& t) { text_ = t; markDirty(); }
    /// @brief Get the switch label text.
    const std::string& text() const    { return text_; }

    /// @brief Check if the switch is on.
    bool isOn() const { return on_; }
    /// @brief Set the switch on/off state.
    void setOn(bool v);

    /// @brief Set the color when switch is on.
    void setOnColor(const Color& c)  { onColor_ = c; markDirty(); }
    /// @brief Set the color when switch is off.
    void setOffColor(const Color& c) { offColor_ = c; markDirty(); }

    /// @brief Emitted when the switch state changes.
    Signal<bool> toggled;

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    std::string text_;
    bool on_ = false;
    Color onColor_  = Color(80, 160, 80, 255);
    Color offColor_ = Color(70, 70, 75, 255);
};

// ═════════════════════════════════════════════════════════════════════════════
//  Toolbar — horizontal bar of buttons/widgets with optional separators
//    auto* tb = parent->createChild<Toolbar>();
//    tb->addButton("Save", IconButton::Check, [](){ ... });
//    tb->addSeparator();
//    tb->addButton("Undo", IconButton::ArrowLeft, [](){ ... });
//    tb->addWidget(myComboBox);  // arbitrary widget
// ═════════════════════════════════════════════════════════════════════════════

class Toolbar : public Widget
{
public:
    explicit Toolbar(float height = 32.0f);

    /// @brief Add an icon button with callback.
    IconButton* addButton(const std::string& tooltip, IconButton::Icon icon,
                          std::function<void()> onClick = {});

    /// @brief Add an arbitrary widget to the toolbar.
    Widget* addWidget(Widget* w);

    /// @brief Add a vertical separator line.
    void addSeparator();

    /// @brief Set the toolbar height in pixels.
    void setHeight(float h) { height_ = h; markDirty(); }
    /// @brief Get the toolbar height.
    float height() const    { return height_; }

    /// @brief Set spacing between toolbar items.
    void setSpacing(float s) { spacing_ = s; markDirty(); }
    /// @brief Get the spacing.
    float spacing() const    { return spacing_; }

    /// @brief Set the toolbar background color.
    void setBgColor(const Color& c) { bgColor_ = c; markDirty(); }

    void layout() override;
    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    float height_  = 32.0f;
    float spacing_ = 2.0f;
    Color bgColor_ = Color(0, 0, 0, 0);  // 0 alpha = use theme panelColor
};


} // namespace BuGUI
