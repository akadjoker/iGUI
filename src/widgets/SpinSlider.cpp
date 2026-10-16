#include "pch.hpp"

#include "SpinSlider.hpp"
#include "TextInputWidgets.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace
{
constexpr float kUpdownWidth = 16.0f;
constexpr int kGrabberWidth = 4;
constexpr int kSeparation = 4;
constexpr float kFloatDragSpeed = 5.0f;
constexpr float kIntegerDragSpeed = 0.5f;
constexpr double kDefaultFloatStep = 0.001;
constexpr float kGrabberIconSize = 16.0f;

Color withAlpha(Color c, float alpha)
{
    c.a = static_cast<u8>(std::lround(alpha * 255.0f));
    return c;
}

Color scaledAlpha(Color c, float factor)
{
    c.a = static_cast<u8>(std::lround(c.a * factor));
    return c;
}

bool visibleInTree(const Widget* w)
{
    for (; w; w = w->parent())
        if (!w->isVisible()) return false;
    return true;
}

bool isAccept(const KeyEvent& e)
{
    const bool key = e.key == Key::Return || e.key == Key::KPEnter || e.key == Key::Space;
    return key && !e.shift && !e.ctrl && !e.alt;
}

// Bytes of the longest start of `text` whose glyphs all fit in `room`.
size_t fittingBytes(PaintContext& ctx, const String& text, float room)
{
    size_t fits = 0;
    size_t at = 0;
    while (at < text.size())
    {
        size_t next = at + 1;
        while (next < text.size() && (static_cast<unsigned char>(text[next]) & 0xC0) == 0x80)
            ++next;
        if (ctx.font.GetTextWidth(text.substr(0, next).c_str()) > room) break;
        fits = next;
        at = next;
    }
    return fits;
}
} // namespace

class SpinSlider::Grabber : public Widget
{
public:
    explicit Grabber(SpinSlider* owner)
            : owner_(owner)
    {
    }

    bool highlight = false;

    void paint(PaintContext& ctx) override
    {
        if (!visible_) return;
        const Rect abs = absoluteRect();
        const Vec2 center = { abs.x + abs.w * 0.5f, abs.y + abs.h * 0.5f };
        ctx.drawList.addCircleFilled(center, 3.0f, Color(255, 255, 255, 230));
        ctx.drawList.addCircle(center, 6.0f, Color(224, 224, 224, highlight ? 255 : 74), 2.0f);
    }
    void onMousePress(MouseEvent& e) override
    {
        e.consumed = true;
        owner_->grabberPress(e);
    }
    void onMouseRelease(MouseEvent& e) override
    {
        e.consumed = true;
        owner_->grabberRelease(e);
    }
    void onMouseMove(MouseEvent& e) override
    {
        e.consumed = true;
        owner_->grabberMove(e);
    }
    void onMouseScroll(MouseEvent& e) override
    {
        owner_->grabberScroll(e);
    }
    void onMouseEnter() override
    {
        owner_->grabberMouseEntered();
    }
    void onMouseLeave() override
    {
        owner_->grabberMouseExited();
    }

private:
    SpinSlider* owner_;
};

class SpinSlider::ValueInput : public TextInput
{
public:
    explicit ValueInput(SpinSlider* owner)
            : owner_(owner)
    {
    }

    void onKeyPress(KeyEvent& e) override
    {
        const uint32_t deletions = WidgetApp::instance().deletions();
        owner_->valueInputKey(e);
        if (WidgetApp::instance().deletions() != deletions || !isVisible()) return;
        TextInput::onKeyPress(e);
    }
    void onFocusLost() override
    {
        TextInput::onFocusLost();
        owner_->valueFocusExit();
    }

private:
    SpinSlider* owner_;
};

SpinSlider::SpinSlider()
{
    acceptsFocus_ = true;
    grabbingSpinnerSpeed_ = kFloatDragSpeed;
    grabber_ = createChild<Grabber>(this);
    grabber_->setVisible(false);
}

SpinSlider::~SpinSlider()
{
    if (!grabbingSpinner_) return;
    if (WidgetApp* app = WidgetApp::existing())
    {
        app->setMouseCaptured(false);
        app->warpMouse(grabbingSpinnerMouseX_, grabbingSpinnerMouseY_);
    }
}

String SpinSlider::textValue() const
{
    if (editingInteger_)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld",
                static_cast<long long>(static_cast<int64_t>(value())));
        return buf;
    }
    return numberText(value(), rangeStepDecimals(step()));
}

void SpinSlider::setLabel(const String& label)
{
    if (label_ == label) return;
    label_ = label;
    markDirty();
}

void SpinSlider::setSuffix(const String& suffix)
{
    suffix_ = suffix;
    markDirty();
}

void SpinSlider::setControlState(ControlState state)
{
    controlState_ = state;
    markDirty();
}

void SpinSlider::setEditingInteger(bool editingInteger)
{
    if (editingInteger == editingInteger_) return;
    editingInteger_ = editingInteger;
    grabbingSpinnerSpeed_ = editingInteger_ ? kIntegerDragSpeed : kFloatDragSpeed;
    markDirty();
}

void SpinSlider::setReadOnly(bool enable)
{
    readOnly_ = enable;
    if (readOnly_ && valueInput_ && valueInput_->isFocused())
        WidgetApp::instance().setFocused(nullptr);
    markDirty();
}

void SpinSlider::setFlat(bool enable)
{
    flat_ = enable;
    markDirty();
}

Widget::Vec2f SpinSlider::sizeHint() const
{
    const auto& t = Theme::instance();
    const float width = t.padding * 2.0f + kUpdownWidth;
    const float height = t.padding * 2.0f + t.fontSize;
    return { width, height > t.buttonHeight ? height : t.buttonHeight };
}

void SpinSlider::paint(PaintContext& ctx)
{
    if (!visible_) return;
    const Rect abs = absoluteRect();
    if (ctx.isClipped(abs)) return;
    const auto& t = Theme::instance();
    const bool redraw = isDirty();
    clearDirty();

    if (overrideFont_) ctx.pushFont(overrideFont_);
    const float pad = t.padding;
    const int sbOffsetX = static_cast<int>(pad);
    const int sbMinWidth = static_cast<int>(pad * 2.0f);

    if (!flat_)
    {
        const Color bg = readOnly_ ? t.panelColor : t.inputBg;
        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fill.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, true);
        ctx.line.SetColor(t.inputBorder.r, t.inputBorder.g, t.inputBorder.b, t.inputBorder.a);
        ctx.line.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, false);
    }

    const int sep = kSeparation + sbOffsetX;
    const int labelWidth = static_cast<int>(ctx.font.GetTextWidth(label_.c_str()));
    const int numberWidth = static_cast<int>(abs.w) - sbMinWidth - labelWidth - sep;
    const String numstr = textValue();
    const int vofs = static_cast<int>((abs.h - ctx.textHeight()) / 2.0f + ctx.font.GetAscender());

    const Color fc = readOnly_ ? t.textDisabled : t.textColor;
    const Color lc = readOnly_ ? t.textDisabled : t.textColor;

    if (flat_ && !label_.empty())
    {
        ctx.fill.SetColor(t.bgColor.r, t.bgColor.g, t.bgColor.b, t.bgColor.a);
        ctx.fill.RoundedRectangle(abs.x, abs.y, static_cast<float>(sbOffsetX * 2 + labelWidth),
                abs.h, t.borderRadius, 6, true);
    }

    if (focused_)
    {
        ctx.line.SetColor(t.focusColor.r, t.focusColor.g, t.focusColor.b, t.focusColor.a);
        ctx.line.RoundedRectangle(abs.x, abs.y, abs.w, abs.h, t.borderRadius, 6, false);
    }

    ctx.font.SetColor(scaledAlpha(lc, 0.5f));
    ctx.font.Print(label_.c_str(), abs.x + static_cast<float>(sbOffsetX),
            abs.y + static_cast<float>(vofs));

    // The number, a thin space and the suffix; only the glyphs that fit whole
    // in the number's room are drawn, the suffix fainter than the number.
    const String shown = numstr + "\xE2\x80\x89" + suffix_;
    const float textStart = std::round(static_cast<float>(sbOffsetX + labelWidth + sep));
    const size_t fits = fittingBytes(ctx, shown, static_cast<float>(numberWidth));
    const size_t numberBytes = fits < numstr.size() ? fits : numstr.size();
    const String numberPart = shown.substr(0, numberBytes);
    ctx.font.SetColor(fc);
    ctx.font.Print(numberPart.c_str(), abs.x + textStart, abs.y + static_cast<float>(vofs));
    if (fits > numberBytes)
    {
        const String suffixPart = shown.substr(numberBytes, fits - numberBytes);
        ctx.font.SetColor(scaledAlpha(fc, 0.4f));
        ctx.font.Print(suffixPart.c_str(),
                abs.x + textStart + ctx.font.GetTextWidth(numberPart.c_str()),
                abs.y + static_cast<float>(vofs));
    }

    if (controlState_ != ControlState::Hide && editingInteger_ &&
            controlState_ == ControlState::Default)
    {
        if (grabber_->isVisible()) grabber_->setVisible(false);
    }
    else if (controlState_ != ControlState::Hide)
    {
        const int width = static_cast<int>(abs.w) - sbMinWidth - kGrabberWidth;
        const int ofs = sbOffsetX;
        const int svofs = static_cast<int>((abs.h + static_cast<float>(vofs)) / 2.0f - 1.0f);

        Color c = withAlpha(fc, 0.2f);
        ctx.fill.SetColor(c.r, c.g, c.b, c.a);
        ctx.fillRect(abs.x + ofs, abs.y + svofs + 1, static_cast<float>(width), 2.0f);

        const int gofs = static_cast<int>(asRatio() * width);
        c = withAlpha(fc, 0.45f);
        ctx.fill.SetColor(c.r, c.g, c.b, c.a);
        ctx.fillRect(abs.x + ofs, abs.y + svofs + 1, static_cast<float>(gofs), 2.0f);

        c = withAlpha(fc, 0.9f);
        ctx.fill.SetColor(c.r, c.g, c.b, c.a);
        ctx.fillRect(abs.x + ofs + gofs, abs.y + svofs, static_cast<float>(kGrabberWidth), 4.0f);

        const float grabberCenterX = static_cast<float>(ofs + gofs) + kGrabberWidth * 0.5f;
        const float grabberCenterY = static_cast<float>(svofs) + 2.0f;
        grabbingSpinnerMouseX_ = abs.x + grabberCenterX;
        grabbingSpinnerMouseY_ = abs.y + grabberCenterY;

        const bool displayGrabber = !readOnly_ &&
                                    (grabbingGrabber_ || mouseOverSpin_ || mouseOverGrabber_) &&
                                    !grabbingSpinner_ && !(valueInput_ && valueInput_->isVisible());
        if (grabber_->isVisible() != displayGrabber) grabber_->setVisible(displayGrabber);

        if (displayGrabber)
        {
            grabber_->highlight = mouseOverGrabber_ || grabbingGrabber_;
            grabber_->setRect({ grabberCenterX - kGrabberIconSize * 0.5f,
                grabberCenterY - kGrabberIconSize * 0.5f, kGrabberIconSize, kGrabberIconSize });
            if (mousewheelOverGrabber_ && redraw)
            {
                const Rect grabberAbs = grabber_->absoluteRect();
                WidgetApp::instance().warpMouse(grabberAbs.x + static_cast<float>(kGrabberWidth),
                        grabberAbs.y + 4.0f);
            }
            grabberRange_ = width;
        }
    }

    if (overrideFont_) ctx.popFont();
    if (valueInput_) valueInput_->setRect({ 0.0f, 0.0f, rect_.w, rect_.h });
    Widget::paint(ctx);
}

void SpinSlider::onMousePress(MouseEvent& e)
{
    e.consumed = true;
    if (!readOnly_ && isEnabledInTree())
    {
        const uint32_t deletions = WidgetApp::instance().deletions();
        if (e.button == 0)
        {
            grabStart(e.x, e.y);
        }
        else if (e.button == 1 && isGrabbing())
        {
            grabEnd();
            if (WidgetApp::instance().deletions() != deletions) return;
            setValue(preGrabValue_);
        }
        if (WidgetApp::instance().deletions() != deletions) return;
    }
    Widget::onMousePress(e);
}

void SpinSlider::onMouseRelease(MouseEvent& e)
{
    e.consumed = true;
    if (!readOnly_ && isEnabledInTree() && e.button == 0)
    {
        const uint32_t deletions = WidgetApp::instance().deletions();
        const bool grabbing = grabbingSpinner_;
        grabEnd();
        if (WidgetApp::instance().deletions() != deletions) return;
        if (deferredDragMode_ && grabbing) notifyValueChanged();
        if (WidgetApp::instance().deletions() != deletions) return;
    }
    Widget::onMouseRelease(e);
}

void SpinSlider::onMouseMove(MouseEvent& e)
{
    e.consumed = true;
    if (readOnly_ || !isEnabledInTree() || !grabbingSpinnerAttempt_) return;

    double diffX = e.relX;
    if (e.shift && grabbingSpinner_) diffX *= 0.1;
    grabbingSpinnerDistCache_ += diffX * grabbingSpinnerSpeed_;

    if (!grabbingSpinner_ && std::fabs(grabbingSpinnerDistCache_) > 4 * grabbingSpinnerSpeed_)
    {
        WidgetApp::instance().setMouseCaptured(true);
        grabbingSpinner_ = true;
    }

    if (grabbingSpinner_)
    {
        if (preGrabValue_ < minValue() && !lesserAllowed()) preGrabValue_ = minValue();
        if (preGrabValue_ > maxValue() && !greaterAllowed()) preGrabValue_ = maxValue();

        const double dragStep = step() > kDefaultFloatStep ? step() : kDefaultFloatStep;
        const double newValue = preGrabValue_ + dragStep * grabbingSpinnerDistCache_;

        const double val = (e.ctrl && !editingInteger_) ? std::round(newValue) : newValue;
        if (deferredDragMode_) setValueNoSignal(val);
        else
            setValue(val);
    }
}

void SpinSlider::onKeyPress(KeyEvent& e)
{
    if (readOnly_) return;
    if (isAccept(e))
    {
        e.consumed = true;
        focusEntered();
        return;
    }
    if (!isGrabbing()) return;
    e.consumed = true;
    if (e.key == Key::Escape && !e.shift && !e.ctrl && !e.alt)
    {
        const uint32_t deletions = WidgetApp::instance().deletions();
        grabEnd();
        if (WidgetApp::instance().deletions() != deletions) return;
        setValue(preGrabValue_);
    }
}

void SpinSlider::grabStart(float x, float y)
{
    grabbingSpinnerAttempt_ = true;
    grabbingSpinnerDistCache_ = 0.0f;
    preGrabValue_ = value();
    grabbingSpinner_ = false;
    grabbingSpinnerMouseX_ = x;
    grabbingSpinnerMouseY_ = y;
    grabbed.emit();
}

void SpinSlider::grabEnd()
{
    const uint32_t deletions = WidgetApp::instance().deletions();
    if (grabbingSpinnerAttempt_)
    {
        grabbingSpinnerAttempt_ = false;
        if (grabbingSpinner_)
        {
            WidgetApp::instance().setMouseCaptured(false);
            WidgetApp::instance().warpMouse(grabbingSpinnerMouseX_, grabbingSpinnerMouseY_);
            mouseOverGrabber_ = true;
            markDirty();
            grabbingSpinner_ = false;
            ungrabbed.emit();
            if (WidgetApp::instance().deletions() != deletions) return;
        }
        else
        {
            focusEntered();
            if (WidgetApp::instance().deletions() != deletions) return;
        }
    }

    if (grabbingGrabber_)
    {
        grabbingGrabber_ = false;
        mousewheelOverGrabber_ = false;
        ungrabbed.emit();
    }
}

TextInput* SpinSlider::lineEdit()
{
    ensureValueInput();
    return valueInput_;
}

void SpinSlider::ensureValueInput()
{
    if (valueInput_) return;
    valueInput_ = createChild<ValueInput>(this);
    valueInput_->setVisible(false);
    valueInput_->setRect({ 0.0f, 0.0f, rect_.w, rect_.h });
    valueInput_->submitted.connect([this](const String&) { valueInputSubmitted(); });
}

void SpinSlider::focusEntered()
{
    if (readOnly_) return;
    ensureValueInput();
    valueInput_->setText(textValue());
    WidgetApp::instance().callDeferred(this,
            [this]()
            {
                valueInput_->setRect({ 0.0f, 0.0f, rect_.w, rect_.h });
                valueInput_->setVisible(true);
                if (!valueInput_->isFocused()) WidgetApp::instance().setFocused(valueInput_);
                valueInput_->selectAll();
            });
    valueFocusEntered.emit();
}

// The field hides: the focus leaves it first, then it reads its text.
void SpinSlider::hideValueInput()
{
    const uint32_t deletions = WidgetApp::instance().deletions();
    valueInput_->setVisible(false);
    if (valueInput_->isFocused())
    {
        WidgetApp::instance().setFocused(valueInputClosing_ ? this : nullptr);
        if (WidgetApp::instance().deletions() != deletions) return;
    }
    evaluateInputText();
}

void SpinSlider::evaluateInputText()
{
    String text = valueInput_->text();
    if (text.empty())
    {
        setValue(0);
        return;
    }
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == ',') text[i] = '.';
        else if (text[i] == ';')
            text[i] = ',';
    }

    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && static_cast<unsigned char>(text[begin]) <= 32) ++begin;
    while (end > begin && static_cast<unsigned char>(text[end - 1]) <= 32) --end;
    if (begin == end) return;
    const String number = text.substr(begin, end - begin);
    char* stop = nullptr;
    const double v = std::strtod(number.c_str(), &stop);
    if (stop != number.c_str() + number.size() || !std::isfinite(v)) return;
    setValue(v);
}

void SpinSlider::valueInputKey(KeyEvent& e)
{
    if (readOnly_) return;
    if (e.key == Key::Up || e.key == Key::Down)
    {
        e.consumed = true;
        double step = this->step();
        if (step < 1)
        {
            const double divisor = 1.0 / step;
            if (std::trunc(divisor) == divisor) step = 1.0;
        }

        if (e.ctrl) step *= 100.0;
        else if (e.shift)
            step *= 10.0;
        else if (e.alt)
            step *= 0.1;

        const uint32_t deletions = WidgetApp::instance().deletions();
        evaluateInputText();
        if (WidgetApp::instance().deletions() != deletions) return;
        const double lastValue = value();
        if (e.key == Key::Down) step *= -1;
        setValue(lastValue + step);
        if (WidgetApp::instance().deletions() != deletions) return;

        valueInputDirty_ = true;
        WidgetApp::instance().callDeferred(this,
                [this]()
                {
                    if (!valueInputDirty_) return;
                    valueInputDirty_ = false;
                    valueInput_->setText(textValue());
                });
    }
    else if (e.key == Key::Escape)
    {
        e.consumed = true;
        valueInputClosing_ = true;
        hideValueInput();
        valueInputClosing_ = false;
    }
}

void SpinSlider::valueInputSubmitted()
{
    valueInputClosing_ = true;
    hideValueInput();
    valueInputClosing_ = false;
}

void SpinSlider::valueFocusExit()
{
    if (readOnly_) return;

    const uint32_t deletions = WidgetApp::instance().deletions();
    evaluateInputText();
    if (WidgetApp::instance().deletions() != deletions) return;
    if (!visibleInTree(this) || !valueInputClosing_)
    {
        if (valueInput_->isVisible()) hideValueInput();
        if (WidgetApp::instance().deletions() != deletions) return;
    }
    valueFocusExited.emit();
}

void SpinSlider::onMouseScroll(MouseEvent& e)
{
    (void) e;
    if (readOnly_) return;
    if (grabber_->isVisible()) markDirty();
}

void SpinSlider::onMouseEnter()
{
    mouseOverSpin_ = true;
    markDirty();
    Widget::onMouseEnter();
}

void SpinSlider::onMouseLeave()
{
    mouseOverSpin_ = false;
    markDirty();
    Widget::onMouseLeave();
}

void SpinSlider::grabberMouseEntered()
{
    mouseOverGrabber_ = true;
    markDirty();
}

void SpinSlider::grabberMouseExited()
{
    mouseOverGrabber_ = false;
    markDirty();
}

void SpinSlider::grabberPress(MouseEvent& e)
{
    if (readOnly_ || !isEnabledInTree()) return;
    if (e.button == 0)
    {
        grabbingGrabber_ = true;
        preGrabValue_ = value();
        if (!mousewheelOverGrabber_)
        {
            grabbingRatio_ = static_cast<float>(asRatio());
            grabbingFrom_ = static_cast<int>(e.x - absoluteRect().x);
        }
        if (!isFocused()) WidgetApp::instance().setFocused(this);
        grabbed.emit();
    }
    else if (e.button == 1 && grabbingGrabber_)
    {
        grabbingGrabber_ = false;
        mousewheelOverGrabber_ = false;
        const uint32_t deletions = WidgetApp::instance().deletions();
        setValue(preGrabValue_);
        if (WidgetApp::instance().deletions() != deletions) return;
        ungrabbed.emit();
    }
}

void SpinSlider::grabberRelease(MouseEvent& e)
{
    if (readOnly_ || !isEnabledInTree() || e.button != 0) return;
    if (grabbingGrabber_)
    {
        grabbingGrabber_ = false;
        if (!mouseOverGrabber_) markDirty();
    }
    mousewheelOverGrabber_ = false;
    ungrabbed.emit();
}

void SpinSlider::grabberMove(MouseEvent& e)
{
    if (readOnly_ || !isEnabledInTree() || !grabbingGrabber_ || mousewheelOverGrabber_) return;
    const float grabbingOfs = (e.x - absoluteRect().x - static_cast<float>(grabbingFrom_)) /
                              static_cast<float>(grabberRange_);
    const uint32_t deletions = WidgetApp::instance().deletions();
    setAsRatio(grabbingRatio_ + grabbingOfs);
    if (WidgetApp::instance().deletions() != deletions) return;
    markDirty();
}

void SpinSlider::grabberScroll(MouseEvent& e)
{
    if (readOnly_ || !isEnabledInTree() || !grabbingGrabber_) return;
    if (e.scrollY == 0.0f) return;
    e.consumed = true;
    const uint32_t deletions = WidgetApp::instance().deletions();
    setValue(e.scrollY > 0.0f ? value() + step() : value() - step());
    if (WidgetApp::instance().deletions() != deletions) return;
    mousewheelOverGrabber_ = true;
}
