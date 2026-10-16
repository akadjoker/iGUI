#pragma once

#include "Range.hpp"
#include <igui/widgets/String.hpp>

namespace ig
{
namespace retained
{
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

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    String label_;
    String suffix_;
    ControlState controlState_ = ControlState::Default;
    bool editingInteger_ = false;
    bool readOnly_ = false;
    bool flat_ = false;
};

} // namespace retained
} // namespace ig
