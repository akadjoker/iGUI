#pragma once

#include "Range.hpp"
#include <igui/widgets/String.hpp>

namespace ig
{
namespace retained
{
class TextInput;

// ═════════════════════════════════════════════════════════════════════════════
//  SpinSlider — a number field with a slider bar under the number
//
//    auto* x = parent->createChild<SpinSlider>();
//    x->setLabel("x");
//    x->setStep(0.001);
//    x->setAllowGreater(true);
//    x->setAllowLesser(true);
//    x->valueChanged.connect([](double v) { ... });
// ═════════════════════════════════════════════════════════════════════════════

class SpinSlider : public Range
{
public:
    IG_WIDGET_CLASS(SpinSlider, Range);

    /// @brief What shows under the number: the slider bar, the up/down arrows of
    ///        an integer field, or nothing.
    enum class ControlState
    {
        Default,      // the bar, or the arrows when editing an integer
        PreferSlider, // the bar, also when editing an integer
        Hide          // neither
    };

    SpinSlider();
    ~SpinSlider() override;

    /// @brief The value as the field shows it: an integer, or the digits the step needs.
    String textValue() const;

    /// @brief Set the label drawn before the number.
    void setLabel(const String& label);
    /// @brief Get the label.
    const String& label() const
    {
        return label_;
    }
    /// @brief Set the suffix drawn after the number (a unit, for example).
    void setSuffix(const String& suffix);
    /// @brief Get the suffix.
    const String& suffix() const
    {
        return suffix_;
    }
    /// @brief Set what shows under the number.
    void setControlState(ControlState state);
    /// @brief Get what shows under the number.
    ControlState controlState() const
    {
        return controlState_;
    }
    /// @brief Edit an integer: the value shows without decimals.
    void setEditingInteger(bool editingInteger);
    /// @brief Check if the field edits an integer.
    bool editingInteger() const
    {
        return editingInteger_;
    }
    /// @brief Show the value without letting it be changed.
    void setReadOnly(bool enable);
    /// @brief Check if the field is read only.
    bool readOnly() const
    {
        return readOnly_;
    }
    /// @brief Draw without the box, with a background behind the label only.
    void setFlat(bool enable);
    /// @brief Check if the field is flat.
    bool flat() const
    {
        return flat_;
    }
    /// @brief While dragging, change the value without valueChanged and emit it
    ///        once when the drag ends.
    void setDeferredDragMode(bool enable)
    {
        deferredDragMode_ = enable;
    }
    /// @brief Check if the value is emitted only when a drag ends.
    bool deferredDragMode() const
    {
        return deferredDragMode_;
    }
    /// @brief Check if the value is being dragged, on the field or on the grabber.
    bool isGrabbing() const
    {
        return grabbingGrabber_ || grabbingSpinner_;
    }

    /// @brief Emitted when the left button goes down on the field or on the grabber.
    Signal<> grabbed;
    /// @brief Emitted when a drag of the value ends.
    Signal<> ungrabbed;
    /// @brief Emitted when the text field opens to type the value.
    Signal<> valueFocusEntered;
    /// @brief Emitted when the text field loses the focus.
    Signal<> valueFocusExited;

    /// @brief The text field that opens to type the value (created on first use).
    TextInput* lineEdit();

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onKeyPress(KeyEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onMouseEnter() override;
    void onMouseLeave() override;

private:
    class Grabber;
    class ValueInput;

    void grabStart(float x, float y);
    void grabEnd();
    void grabberMouseEntered();
    void grabberMouseExited();
    void grabberPress(MouseEvent& e);
    void grabberRelease(MouseEvent& e);
    void grabberMove(MouseEvent& e);
    void grabberScroll(MouseEvent& e);
    void focusEntered();
    void ensureValueInput();
    void hideValueInput();
    void evaluateInputText();
    void valueInputKey(KeyEvent& e);
    void valueInputSubmitted();
    void valueFocusExit();

    String label_;
    String suffix_;
    ControlState controlState_ = ControlState::Default;
    bool editingInteger_ = false;
    bool readOnly_ = false;
    bool flat_ = false;
    bool deferredDragMode_ = false;
    Grabber* grabber_ = nullptr;
    int grabberRange_ = 1;
    bool mouseOverSpin_ = false;
    bool mouseOverGrabber_ = false;
    bool mousewheelOverGrabber_ = false;
    bool grabbingGrabber_ = false;
    int grabbingFrom_ = 0;
    float grabbingRatio_ = 0.0f;
    ValueInput* valueInput_ = nullptr;
    bool valueInputClosing_ = false;
    bool valueInputDirty_ = false;
    bool grabbingSpinnerAttempt_ = false;
    bool grabbingSpinner_ = false;
    float grabbingSpinnerSpeed_ = 0.0f;
    float grabbingSpinnerDistCache_ = 0.0f;
    float grabbingSpinnerMouseX_ = 0.0f;
    float grabbingSpinnerMouseY_ = 0.0f;
    double preGrabValue_ = 0.0;
};

} // namespace retained
} // namespace ig
