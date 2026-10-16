#include "pch.hpp"

#include "SpinSlider.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace
{
constexpr float kUpdownWidth = 16.0f;
constexpr int kGrabberWidth = 4;
constexpr int kSeparation = 4;

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

SpinSlider::SpinSlider()
{
    acceptsFocus_ = true;
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
    markDirty();
}

void SpinSlider::setReadOnly(bool enable)
{
    readOnly_ = enable;
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

    if (controlState_ != ControlState::Hide &&
            !(editingInteger_ && controlState_ == ControlState::Default))
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
    }

    if (overrideFont_) ctx.popFont();
    Widget::paint(ctx);
}
