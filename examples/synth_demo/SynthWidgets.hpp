#pragma once
// Synth-style audio widgets, drawn as application code on top of the Context's
// public building blocks. Nothing here is an iGUI widget: the look belongs to
// the application, which is the whole point of doing these in immediate mode.
//
// The knob is modelled on a real hardware pot rather than on a progress ring:
// a solid body that ROTATES, carrying its indicator mark around with it, with
// the value arc outside it. A ring with a thin needle reads as a gauge; a body
// with a notch reads as something you grab and turn.

#include <igui/Gui.hpp>

#include <math.h>
#include <stdio.h>

namespace synth
{

const float kPi = 3.14159265358979323846f;
// Open at the bottom, like every hardware pot: 270 degrees from the lower left.
const float kStartAngle = 135.0f * kPi / 180.0f;
const float kSweepAngle = 270.0f * kPi / 180.0f;

struct KnobStyle
{
    ig::Color bodyTop;     // body is shaded top-to-bottom so it reads as domed
    ig::Color bodyBottom;
    ig::Color rim;         // thin ring around the body
    ig::Color indicator;   // the mark that rotates with the body
    ig::Color track;       // unfilled part of the value arc
    ig::Color fill;        // filled part; fully transparent = follow the theme accent
    ig::Color value;
    ig::Color label;
    float arcThickness;

    // The same defaults the retained-mode Knob paints with, so one voice reads
    // the same in either API. No accent colour is fixed here: an unset fill
    // takes the theme's, and the voice can pick its own.
    KnobStyle()
        : bodyTop(58u, 62u, 74u, 255u), bodyBottom(34u, 36u, 44u, 255u),
          rim(78u, 84u, 100u, 255u), indicator(240u, 243u, 250u, 255u),
          track(52u, 58u, 82u, 255u), fill(0u, 0u, 0u, 0u),
          value(228u, 234u, 244u, 255u), label(138u, 148u, 166u, 255u),
          arcThickness(4.0f) {}

    /// The default look with one change: the accent of the value arc.
    static KnobStyle accent(const ig::Color& colour)
    {
        KnobStyle style;
        style.fill = colour;
        return style;
    }
};

// One knob may be dragging at a time; the application owns that state.
struct KnobDrag
{
    bool active;
    float originX;
    float originY;
    float valueAtPress;

    KnobDrag() : active(false), originX(0.0f), originY(0.0f), valueAtPress(0.0f) {}
};

inline KnobDrag& knobDrag()
{
    static KnobDrag drag;
    return drag;
}

// One continuous band per arc. Stroking it as a fan of short lines leaves every
// joint overlapping its neighbour's soft edge, and the alpha adds up there,
// which reads as notches around the ring.
inline void arcSegments(ig::Context& ui, float cx, float cy, float radius,
                        float fromTurn, float toTurn, const ig::Color& color,
                        float thickness)
{
    if (toTurn <= fromTurn)
        return;
    ui.drawArc(ig::Vec2(cx, cy), radius, kStartAngle + kSweepAngle * fromTurn,
               kStartAngle + kSweepAngle * toTurn, color, thickness);
}

// `displayText` is what appears under the dial, so the caller decides how a
// value reads ("90%", "off", "2.0", "440 Hz").
inline bool knob(ig::Context& ui, ig::StringView id, const char* label,
                 const char* displayText, float& value, float minimum,
                 float maximum, const ig::Vec2& topLeft, float diameter,
                 const KnobStyle& style = KnobStyle())
{
    const float cx = topLeft.x + diameter * 0.5f;
    const float cy = topLeft.y + diameter * 0.5f;
    const float outerRadius = diameter * 0.5f;
    const float arcRadius = outerRadius - style.arcThickness * 0.5f;
    // The body sits inside the value arc with a small gap.
    const float bodyRadius = outerRadius - style.arcThickness - 3.0f;

    const float span = (maximum > minimum) ? (maximum - minimum) : 1.0f;
    float norm = (value - minimum) / span;
    norm = norm < 0.0f ? 0.0f : (norm > 1.0f ? 1.0f : norm);
    const float angle = kStartAngle + kSweepAngle * norm;

    // The value arc's colour is the application's choice: with none set the knob
    // takes the theme accent rather than fixing one of its own.
    const ig::Color accent = style.fill.a == 0u ? ui.theme().focusColor : style.fill;

    // Value arc, outside the body.
    arcSegments(ui, cx, cy, arcRadius, 0.0f, 1.0f, style.track, style.arcThickness);
    arcSegments(ui, cx, cy, arcRadius, 0.0f, norm, accent, style.arcThickness);

    // Body. Drawn as a stack of shrinking circles from bottom colour to top
    // colour: a cheap vertical shade that makes the cap look domed rather than
    // flat, without needing a gradient primitive for circles.
    const int shadeSteps = 7;
    for (int i = 0; i < shadeSteps; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(shadeSteps - 1);
        const ig::Color shade(
            static_cast<uint8_t>(style.bodyBottom.r +
                                 (style.bodyTop.r - style.bodyBottom.r) * t),
            static_cast<uint8_t>(style.bodyBottom.g +
                                 (style.bodyTop.g - style.bodyBottom.g) * t),
            static_cast<uint8_t>(style.bodyBottom.b +
                                 (style.bodyTop.b - style.bodyBottom.b) * t),
            255u);
        // Each layer is slightly smaller and nudged up, so the lighter shades
        // gather toward the top of the cap.
        const float layerRadius = bodyRadius * (1.0f - t * 0.55f);
        const float layerY = cy - t * bodyRadius * 0.30f;
        ui.drawCircleFilled(ig::Vec2(cx, layerY), layerRadius, shade);
    }

    // Rim around the body.
    const int rimSegments = 40;
    for (int i = 0; i < rimSegments; ++i)
    {
        const float a0 = 6.2831853f * static_cast<float>(i) / rimSegments;
        const float a1 = 6.2831853f * static_cast<float>(i + 1) / rimSegments;
        ui.drawLine(ig::Vec2(cx + bodyRadius * cosf(a0), cy + bodyRadius * sinf(a0)),
                    ig::Vec2(cx + bodyRadius * cosf(a1), cy + bodyRadius * sinf(a1)),
                    style.rim, 1.0f);
    }

    // The indicator: a line cut from the centre of the cap out to its edge,
    // rotating with the value. This is what makes the whole knob look turned
    // rather than merely filled.
    ui.drawLine(ig::Vec2(cx + cosf(angle) * bodyRadius * 0.20f,
                         cy + sinf(angle) * bodyRadius * 0.20f),
                ig::Vec2(cx + cosf(angle) * bodyRadius * 0.92f,
                         cy + sinf(angle) * bodyRadius * 0.92f),
                style.indicator, 2.5f);
    // A dot at the tip reads as a moulded marker at any size.
    ui.drawCircleFilled(ig::Vec2(cx + cosf(angle) * bodyRadius * 0.78f,
                                 cy + sinf(angle) * bodyRadius * 0.78f),
                        1.6f, style.indicator);

    // Value then label, centred below.
    const float valueWidth = ui.textWidth(ig::StringView(displayText));
    ui.drawText(ig::StringView(displayText),
                ig::Vec2(cx - valueWidth * 0.5f, topLeft.y + diameter + 5.0f),
                style.value);
    const float labelWidth = ui.textWidth(ig::StringView(label));
    ui.drawText(ig::StringView(label),
                ig::Vec2(cx - labelWidth * 0.5f, topLeft.y + diameter + 21.0f),
                style.label);

    // Vertical drag: up raises the value. The pointer is read in content
    // coordinates, the same space the knob is drawn in, so the hit area stays on
    // the dial when the layout is indented.
    const ig::Rect hit(topLeft.x, topLeft.y, diameter, diameter);
    const ig::Vec2 press = ui.pointerPressedContentPosition(ig::PointerButton::Left);
    const bool insideHit = press.x >= hit.x && press.x <= hit.x + hit.width &&
                           press.y >= hit.y && press.y <= hit.y + hit.height;
    const bool held = ui.isPointerButtonDown(ig::PointerButton::Left);
    KnobDrag& drag = knobDrag();
    bool changed = false;

    if (held && insideHit)
    {
        if (!drag.active || drag.originX != hit.x || drag.originY != hit.y)
        {
            drag.active = true;
            drag.originX = hit.x;
            drag.originY = hit.y;
            drag.valueAtPress = value;
        }
        const float dy = press.y - ui.pointerContentPosition().y;
        float next = drag.valueAtPress + (dy / 160.0f) * span;
        next = next < minimum ? minimum : (next > maximum ? maximum : next);
        if (next != value)
        {
            value = next;
            changed = true;
        }
    }
    else if (drag.active && drag.originX == hit.x && drag.originY == hit.y)
    {
        drag.active = false;
    }

    ui.invisibleButton(id, hit);
    return changed;
}

// ── ADSR envelope ───────────────────────────────────────────────────────────
// Draws the envelope shape and lets each stage be dragged by its breakpoint.
struct Envelope
{
    float attack;   // seconds
    float decay;
    float sustain;  // 0..1
    float release;

    Envelope() : attack(0.02f), decay(0.18f), sustain(0.65f), release(0.35f) {}
};

inline void envelopeView(ig::Context& ui, const ig::Rect& bounds,
                         const Envelope& envelope, const ig::Color& line,
                         const ig::Color& fill)
{
    ui.drawRectFilledRounded(bounds, 6.0f, ig::Color(18u, 22u, 32u, 255u));

    // The four stages share the width in proportion to their times, with the
    // sustain leg given a fixed slice so it stays visible.
    const float total = envelope.attack + envelope.decay + envelope.release;
    const float scale = (total > 0.0f) ? (bounds.width * 0.75f) / total : 0.0f;
    const float x0 = bounds.x + 4.0f;
    const float x1 = x0 + envelope.attack * scale;
    const float x2 = x1 + envelope.decay * scale;
    const float x3 = x2 + bounds.width * 0.25f;
    const float x4 = x3 + envelope.release * scale;
    const float top = bounds.y + 6.0f;
    const float bottom = bounds.y + bounds.height - 6.0f;
    const float sustainY = bottom - (bottom - top) * envelope.sustain;

    // Shade under the curve so the envelope reads as a filled shape.
    const int columns = 48;
    for (int i = 0; i < columns; ++i)
    {
        const float t = static_cast<float>(i) / columns;
        const float x = x0 + (x4 - x0) * t;
        float y = bottom;
        if (x < x1)
            y = bottom - (bottom - top) * ((x - x0) / (x1 - x0 + 0.0001f));
        else if (x < x2)
            y = top + (sustainY - top) * ((x - x1) / (x2 - x1 + 0.0001f));
        else if (x < x3)
            y = sustainY;
        else
            y = sustainY + (bottom - sustainY) * ((x - x3) / (x4 - x3 + 0.0001f));
        ui.drawRectFilledRounded(ig::Rect(x, y, (x4 - x0) / columns + 1.0f, bottom - y),
                                 0.0f, fill);
    }

    ui.drawLine(ig::Vec2(x0, bottom), ig::Vec2(x1, top), line, 2.0f);
    ui.drawLine(ig::Vec2(x1, top), ig::Vec2(x2, sustainY), line, 2.0f);
    ui.drawLine(ig::Vec2(x2, sustainY), ig::Vec2(x3, sustainY), line, 2.0f);
    ui.drawLine(ig::Vec2(x3, sustainY), ig::Vec2(x4, bottom), line, 2.0f);

    // Breakpoints.
    ui.drawCircleFilled(ig::Vec2(x1, top), 3.5f, line);
    ui.drawCircleFilled(ig::Vec2(x2, sustainY), 3.5f, line);
    ui.drawCircleFilled(ig::Vec2(x3, sustainY), 3.5f, line);
}

// ── VU meter ────────────────────────────────────────────────────────────────
// Segmented bar with a peak-hold tick, the way a hardware meter behaves.
inline void vuMeter(ig::Context& ui, const ig::Rect& bounds, float level,
                    float peak)
{
    ui.drawRectFilledRounded(bounds, 4.0f, ig::Color(18u, 22u, 32u, 255u));

    const int segments = 18;
    const float gap = 2.0f;
    const float segmentHeight = (bounds.height - 6.0f - gap * (segments - 1)) / segments;
    for (int i = 0; i < segments; ++i)
    {
        const float t = static_cast<float>(segments - 1 - i) / (segments - 1);
        const float y = bounds.y + 3.0f + static_cast<float>(i) * (segmentHeight + gap);
        // Green below -6 dB, amber approaching clip, red at the top.
        ig::Color color(40u, 46u, 60u, 255u);
        if (t <= level)
        {
            if (t > 0.92f)
                color = ig::Color(238u, 84u, 76u, 255u);
            else if (t > 0.78f)
                color = ig::Color(238u, 186u, 72u, 255u);
            else
                color = ig::Color(78u, 214u, 150u, 255u);
        }
        ui.drawRectFilledRounded(
            ig::Rect(bounds.x + 3.0f, y, bounds.width - 6.0f, segmentHeight),
            1.5f, color);
    }

    // Peak hold.
    const float peakY = bounds.y + 3.0f + (1.0f - peak) * (bounds.height - 6.0f);
    ui.drawRectFilledRounded(
        ig::Rect(bounds.x + 2.0f, peakY - 1.0f, bounds.width - 4.0f, 2.0f), 1.0f,
        ig::Color(236u, 242u, 252u, 220u));
}

// ── Waveform ────────────────────────────────────────────────────────────────
inline void waveformView(ig::Context& ui, const ig::Rect& bounds,
                         const float* samples, int sampleCount,
                         const ig::Color& color)
{
    ui.drawRectFilledRounded(bounds, 6.0f, ig::Color(18u, 22u, 32u, 255u));
    if (sampleCount < 2)
        return;

    const float midY = bounds.y + bounds.height * 0.5f;
    ui.drawLine(ig::Vec2(bounds.x + 4.0f, midY),
                ig::Vec2(bounds.x + bounds.width - 4.0f, midY),
                ig::Color(42u, 50u, 68u, 255u), 1.0f);

    const float usable = bounds.width - 8.0f;
    const float amplitude = bounds.height * 0.42f;
    for (int i = 0; i < sampleCount - 1; ++i)
    {
        const float x0 = bounds.x + 4.0f + usable * static_cast<float>(i) / (sampleCount - 1);
        const float x1 = bounds.x + 4.0f + usable * static_cast<float>(i + 1) / (sampleCount - 1);
        ui.drawLine(ig::Vec2(x0, midY - samples[i] * amplitude),
                    ig::Vec2(x1, midY - samples[i + 1] * amplitude), color, 1.8f);
    }
}

// ── Spectrum ────────────────────────────────────────────────────────────────
inline void spectrumView(ig::Context& ui, const ig::Rect& bounds,
                         const float* bins, int binCount,
                         const ig::Color& color)
{
    ui.drawRectFilledRounded(bounds, 6.0f, ig::Color(18u, 22u, 32u, 255u));
    if (binCount <= 0)
        return;

    const float barWidth = (bounds.width - 8.0f) / static_cast<float>(binCount);
    for (int i = 0; i < binCount; ++i)
    {
        float magnitude = bins[i];
        magnitude = magnitude < 0.0f ? 0.0f : (magnitude > 1.0f ? 1.0f : magnitude);
        const float height = (bounds.height - 8.0f) * magnitude;
        // Fade the top of the spectrum toward the accent colour.
        const ig::Color barColor(
            static_cast<uint8_t>(color.r * (0.45f + 0.55f * magnitude)),
            static_cast<uint8_t>(color.g * (0.45f + 0.55f * magnitude)),
            static_cast<uint8_t>(color.b * (0.45f + 0.55f * magnitude)), 255u);
        ui.drawRectFilledRounded(
            ig::Rect(bounds.x + 4.0f + static_cast<float>(i) * barWidth,
                     bounds.y + bounds.height - 4.0f - height,
                     barWidth - 1.5f, height),
            1.5f, barColor);
    }
}

// ── Keyboard ────────────────────────────────────────────────────────────────
// Returns the MIDI note clicked this frame, or -1.
inline int keyboard(ig::Context& ui, const ig::Rect& bounds, int firstNote,
                    int whiteKeyCount, int highlightNote)
{
    static const int kWhiteOffsets[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int kBlackAfter[7] = {1, 1, 0, 1, 1, 1, 0}; // no black after E or B

    const float whiteWidth = bounds.width / static_cast<float>(whiteKeyCount);
    int clicked = -1;

    // White keys first, so the black ones sit on top.
    for (int i = 0; i < whiteKeyCount; ++i)
    {
        const int octave = i / 7;
        const int degree = i % 7;
        const int note = firstNote + octave * 12 + kWhiteOffsets[degree];
        const ig::Rect key(bounds.x + static_cast<float>(i) * whiteWidth, bounds.y,
                           whiteWidth - 1.0f, bounds.height);
        char id[32];
        snprintf(id, sizeof(id), "key.w.%d", note);
        const ig::StringView keyId(id);
        const bool hot = ui.isHovered(keyId, key);
        ig::Color color = hot ? ig::Color(208u, 216u, 232u, 255u)
                              : ig::Color(232u, 238u, 248u, 255u);
        if (note == highlightNote)
            color = ig::Color(96u, 212u, 255u, 255u);
        ui.drawRectFilledRounded(key, 3.0f, color);
        if (ui.isClicked(keyId, key))
            clicked = note;
    }

    const float blackWidth = whiteWidth * 0.62f;
    const float blackHeight = bounds.height * 0.62f;
    for (int i = 0; i < whiteKeyCount; ++i)
    {
        const int degree = i % 7;
        if (!kBlackAfter[degree])
            continue;
        const int octave = i / 7;
        const int note = firstNote + octave * 12 + kWhiteOffsets[degree] + 1;
        const ig::Rect key(bounds.x + static_cast<float>(i + 1) * whiteWidth -
                               blackWidth * 0.5f,
                           bounds.y, blackWidth, blackHeight);
        char id[32];
        snprintf(id, sizeof(id), "key.b.%d", note);
        const ig::StringView keyId(id);
        const bool hot = ui.isHovered(keyId, key);
        ig::Color color = hot ? ig::Color(52u, 60u, 78u, 255u)
                              : ig::Color(24u, 28u, 40u, 255u);
        if (note == highlightNote)
            color = ig::Color(52u, 150u, 200u, 255u);
        ui.drawRectFilledRounded(key, 3.0f, color);
        if (ui.isClicked(keyId, key))
            clicked = note;
    }
    return clicked;
}

} // namespace synth
