// Mixer strip in the style of a web DAW: one card per track, each with its own
// accent colour, a thick knob arc with a light pointer, the value and label
// stacked underneath, and an effect dropdown at the bottom. The selected card
// is outlined in its accent colour.
//
// Everything here is application code. The knob and the card are not iGUI
// widgets - they are drawn with the Context's public building blocks
// (drawRectFilledRounded, drawLine, invisibleButton, pointerPosition,
// drawText) so the look belongs to the application rather than to the library.
// ig::retained::Knob has its style fixed in paint(); in immediate mode the
// appearance is the caller's to choose.
//
// Build: cmake -DIGUI_BUILD_RAYLIB_DEMO=ON -DIGUI_BUILD_MIXER_DEMO=ON
// Run:   ./igui_mixer_demo

#include <raylib.h>

#include <math.h>
#include <stdio.h>

#include <igui/Gui.hpp>
#include <igui_raylib/RaylibBackend.hpp>

#include "StepGrid.hpp"

namespace
{

const float kPi = 3.14159265358979323846f;
// Open at the bottom, like a hardware pot: 270 degrees from the lower left.
const float kStartAngle = 135.0f * kPi / 180.0f;
const float kSweepAngle = 270.0f * kPi / 180.0f;

struct KnobStyle
{
    ig::Color track;     // unfilled part of the ring
    ig::Color fill;      // filled part, the track's accent
    ig::Color pointer;   // the needle, kept light so it reads on any accent
    ig::Color value;
    ig::Color label;
    float thickness;

    KnobStyle()
        : track(46u, 52u, 66u, 255u), fill(78u, 201u, 255u, 255u),
          pointer(228u, 236u, 248u, 255u), value(226u, 232u, 242u, 255u),
          label(138u, 148u, 166u, 255u), thickness(5.0f) {}
};

struct KnobDrag
{
    bool active;
    float originX;
    float originY;
    float valueAtPress;

    KnobDrag() : active(false), originX(0.0f), originY(0.0f), valueAtPress(0.0f) {}
};

KnobDrag gDrag;

void arc(ig::Context& ui, float cx, float cy, float radius, float fromTurn,
         float toTurn, const ig::Color& color, float thickness)
{
    const int segments = 56;
    const int first = static_cast<int>(fromTurn * segments);
    const int last = static_cast<int>(toTurn * segments);
    for (int i = first; i < last; ++i)
    {
        const float a0 = kStartAngle + kSweepAngle * static_cast<float>(i) / segments;
        const float a1 = kStartAngle + kSweepAngle * static_cast<float>(i + 1) / segments;
        ui.drawLine(ig::Vec2(cx + radius * cosf(a0), cy + radius * sinf(a0)),
                    ig::Vec2(cx + radius * cosf(a1), cy + radius * sinf(a1)),
                    color, thickness);
    }
}

// `displayText` is what goes under the dial - a percentage, "off", a plain
// number - so the caller decides how a value reads, as in the reference UI
// where the three master knobs show "90%", "off" and "2.0".
bool knob(ig::Context& ui, ig::StringView id, const char* label,
          const char* displayText, float& value, float minimum, float maximum,
          const ig::Vec2& topLeft, float diameter, const KnobStyle& style)
{
    const float cx = topLeft.x + diameter * 0.5f;
    const float cy = topLeft.y + diameter * 0.5f;
    const float radius = diameter * 0.5f - style.thickness * 0.5f;
    const float span = (maximum > minimum) ? (maximum - minimum) : 1.0f;
    float norm = (value - minimum) / span;
    norm = norm < 0.0f ? 0.0f : (norm > 1.0f ? 1.0f : norm);

    arc(ui, cx, cy, radius, 0.0f, 1.0f, style.track, style.thickness);
    arc(ui, cx, cy, radius, 0.0f, norm, style.fill, style.thickness);

    // Needle from near the centre out to just inside the ring.
    const float angle = kStartAngle + kSweepAngle * norm;
    ui.drawLine(ig::Vec2(cx + cosf(angle) * radius * 0.30f,
                         cy + sinf(angle) * radius * 0.30f),
                ig::Vec2(cx + cosf(angle) * (radius - style.thickness * 0.9f),
                         cy + sinf(angle) * (radius - style.thickness * 0.9f)),
                style.pointer, 2.5f);

    // Value then label, both centred below the dial.
    const float valueWidth = ui.textWidth(ig::StringView(displayText));
    ui.drawText(ig::StringView(displayText),
                ig::Vec2(cx - valueWidth * 0.5f, topLeft.y + diameter + 4.0f),
                style.value);
    const float labelWidth = ui.textWidth(ig::StringView(label));
    ui.drawText(ig::StringView(label),
                ig::Vec2(cx - labelWidth * 0.5f, topLeft.y + diameter + 20.0f),
                style.label);

    // Vertical drag: up raises the value, the convention every DAW uses.
    const ig::Rect hit(topLeft.x, topLeft.y, diameter, diameter);
    const ig::Vec2 press = ui.pointerPressedPosition(ig::PointerButton::Left);
    const bool insideHit = press.x >= hit.x && press.x <= hit.x + hit.width &&
                           press.y >= hit.y && press.y <= hit.y + hit.height;
    const bool held = ui.isPointerButtonDown(ig::PointerButton::Left);
    bool changed = false;

    if (held && insideHit)
    {
        if (!gDrag.active || gDrag.originX != hit.x || gDrag.originY != hit.y)
        {
            gDrag.active = true;
            gDrag.originX = hit.x;
            gDrag.originY = hit.y;
            gDrag.valueAtPress = value;
        }
        const float dy = press.y - ui.pointerPosition().y;
        float next = gDrag.valueAtPress + (dy / 160.0f) * span;
        next = next < minimum ? minimum : (next > maximum ? maximum : next);
        if (next != value)
        {
            value = next;
            changed = true;
        }
    }
    else if (gDrag.active && gDrag.originX == hit.x && gDrag.originY == hit.y)
    {
        gDrag.active = false;
    }

    ui.invisibleButton(id, hit);
    return changed;
}

// The card behind a strip: filled panel, a border that turns the accent colour
// and thickens while selected, and the coloured dot + title along the top.
void stripCard(ig::Context& ui, const ig::Rect& card, const char* title,
               const char* suffix, const ig::Color& accent, bool selected)
{
    ui.drawRectFilledRounded(card, 10.0f, ig::Color(22u, 26u, 38u, 255u));
    if (selected)
        ui.drawRectRounded(card, 10.0f, accent, 2.0f);
    else
        ui.drawRectRounded(card, 10.0f, ig::Color(48u, 56u, 74u, 255u), 1.0f);

    ui.drawCircleFilled(ig::Vec2(card.x + 14.0f, card.y + 15.0f), 4.0f, accent);
    ui.drawText(ig::StringView(title), ig::Vec2(card.x + 24.0f, card.y + 9.0f),
                ig::Color(232u, 238u, 248u, 255u));
    if (suffix)
    {
        const float titleWidth = ui.textWidth(ig::StringView(title));
        ui.drawText(ig::StringView(suffix),
                    ig::Vec2(card.x + 28.0f + titleWidth, card.y + 10.0f),
                    ig::Color(120u, 130u, 150u, 255u));
    }
}

struct Track
{
    const char* name;
    float volume;
    int effect;
    ig::Color accent;
};

} // namespace

int main()
{
    InitWindow(1180, 700, "iGUI mixer demo");
    SetTargetFPS(60);

    ig::raylib::Backend backend;
    if (!backend.prepareFontAtlas())
    {
        CloseWindow();
        return 1;
    }

    ig::Context ui(backend, &backend.fontAtlas());

    float masterVolume = 0.9f;
    float djFilter = 0.0f;
    float resonance = 2.0f;

    Track tracks[] = {
        {"Drums", 0.90f, 0, ig::Color(64u, 224u, 190u, 255u)},
        {"Bass", 0.90f, 0, ig::Color(232u, 188u, 64u, 255u)},
        {"Keys", 0.90f, 0, ig::Color(236u, 86u, 158u, 255u)},
        {"FX", 0.90f, 0, ig::Color(96u, 158u, 248u, 255u)},
    };
    const int trackCount = static_cast<int>(sizeof(tracks) / sizeof(tracks[0]));
    int selectedTrack = 0;

    const ig::StringView effectItems[] = {
        ig::StringView("+ Effect"), ig::StringView("Reverb"),
        ig::StringView("Delay"), ig::StringView("Filter"),
    };

    // A 16-step pattern per sound, seeded with a four-on-the-floor beat.
    const int kSteps = 16;
    daw::Step kick[kSteps];
    daw::Step clap[kSteps];
    daw::Step hat[kSteps];
    daw::Step bass[kSteps];
    for (int i = 0; i < kSteps; ++i)
    {
        kick[i] = (i % 4 == 0) ? daw::Step::On : daw::Step::Off;
        clap[i] = (i % 8 == 4) ? daw::Step::On : daw::Step::Off;
        hat[i] = (i % 2 == 1) ? daw::Step::Soft : daw::Step::Off;
        bass[i] = (i % 4 == 2) ? daw::Step::On : daw::Step::Off;
    }
    daw::StepRow patternRows[] = {
        {"Kick", tracks[1].accent, kick, kSteps},
        {"Clap", ig::Color(226u, 96u, 84u, 255u), clap, kSteps},
        {"Hi-hat", tracks[0].accent, hat, kSteps},
        {"808", ig::Color(196u, 96u, 226u, 255u), bass, kSteps},
    };
    const int patternRowCount =
        static_cast<int>(sizeof(patternRows) / sizeof(patternRows[0]));

    float playheadClock = 0.0f;
    int playheadStep = 0;

    while (!WindowShouldClose())
    {
        ig::raylib::processInput(ui);
        ui.beginFrame(ig::raylib::frameInfo(GetFrameTime()));

        // 120 BPM in sixteenths: eight steps a second.
        playheadClock += GetFrameTime();
        while (playheadClock >= 0.125f)
        {
            playheadClock -= 0.125f;
            playheadStep = (playheadStep + 1) % kSteps;
        }

        if (ui.beginMainWindow("Tracks, mixer and effects"))
        {
            ui.label("Select a track or drag a knob up and down.");
            ui.separator();

            const ig::Vec2 origin = ui.cursor();
            const float cardY = origin.y + 10.0f;
            const float cardHeight = 150.0f;
            const float knobSize = 46.0f;

            // Master card: three knobs side by side, no effect slot.
            const ig::Rect masterCard(origin.x + 8.0f, cardY, 200.0f, cardHeight - 34.0f);
            stripCard(ui, masterCard, "Master", 0,
                      ig::Color(96u, 208u, 232u, 255u), false);
            {
                KnobStyle style;
                char text[16];
                snprintf(text, sizeof(text), "%d%%",
                         static_cast<int>(masterVolume * 100.0f + 0.5f));
                knob(ui, "master.vol", "Volume", text, masterVolume, 0.0f, 1.0f,
                     ig::Vec2(masterCard.x + 16.0f, masterCard.y + 32.0f),
                     knobSize, style);

                // "off" rather than "0%" at the bottom of the range, as in the
                // reference UI.
                if (djFilter <= 0.001f)
                    snprintf(text, sizeof(text), "off");
                else
                    snprintf(text, sizeof(text), "%d%%",
                             static_cast<int>(djFilter * 100.0f + 0.5f));
                KnobStyle filterStyle;
                filterStyle.fill = ig::Color(150u, 160u, 180u, 255u);
                knob(ui, "master.filter", "DJ filter", text, djFilter, 0.0f, 1.0f,
                     ig::Vec2(masterCard.x + 76.0f, masterCard.y + 32.0f),
                     knobSize, filterStyle);

                snprintf(text, sizeof(text), "%.1f",
                         static_cast<double>(resonance));
                KnobStyle resStyle;
                resStyle.fill = ig::Color(232u, 188u, 64u, 255u);
                knob(ui, "master.res", "Res", text, resonance, 0.0f, 4.0f,
                     ig::Vec2(masterCard.x + 136.0f, masterCard.y + 32.0f),
                     knobSize, resStyle);
            }

            // One card per track, tinted with the track's accent.
            for (int i = 0; i < trackCount; ++i)
            {
                const ig::Rect card(origin.x + 224.0f + static_cast<float>(i) * 148.0f,
                                    cardY, 138.0f, cardHeight);
                const bool selected = (i == selectedTrack);
                stripCard(ui, card, tracks[i].name, "\xc2\xb7track", tracks[i].accent,
                          selected);

                char cardId[32];
                snprintf(cardId, sizeof(cardId), "card.%d", i);
                if (ui.isClicked(ig::StringView(cardId), card))
                    selectedTrack = i;

                KnobStyle style;
                style.fill = tracks[i].accent;
                char text[16];
                snprintf(text, sizeof(text), "%d%%",
                         static_cast<int>(tracks[i].volume * 100.0f + 0.5f));
                char knobId[32];
                snprintf(knobId, sizeof(knobId), "track.%d.vol", i);
                knob(ui, ig::StringView(knobId), "Vol", text, tracks[i].volume,
                     0.0f, 1.0f,
                     ig::Vec2(card.x + card.width * 0.5f - knobSize * 0.5f,
                              card.y + 34.0f),
                     knobSize, style);

                // Effect slot along the bottom of the card.
                char comboId[32];
                snprintf(comboId, sizeof(comboId), "fx.%d", i);
                ui.comboBox(ig::StringView(comboId), tracks[i].effect,
                            ig::Span<const ig::StringView>(effectItems, 4),
                            ig::Rect(card.x + 10.0f, card.y + card.height - 32.0f,
                                     card.width - 20.0f, 24.0f));
            }

            // Pattern editor below the strips.
            ui.setCursor(ig::Vec2(origin.x, cardY + cardHeight + 22.0f));
            ui.separator();

            const ig::Vec2 gridOrigin = ui.cursor();
            daw::StepGridStyle gridStyle;
            const float rowPitch = gridStyle.cellHeight + 6.0f;
            const float gridWidth = ui.availableWidth() - 100.0f;
            for (int i = 0; i < patternRowCount; ++i)
            {
                const float y = gridOrigin.y + 10.0f + static_cast<float>(i) * rowPitch;
                ui.drawCircleFilled(ig::Vec2(gridOrigin.x + 6.0f, y + 15.0f), 4.0f,
                                    patternRows[i].accent);
                ui.drawText(ig::StringView(patternRows[i].name),
                            ig::Vec2(gridOrigin.x + 16.0f, y + 9.0f),
                            ig::Color(178u, 186u, 202u, 255u));
                char rowId[32];
                snprintf(rowId, sizeof(rowId), "grid.%d", i);
                daw::stepRow(ui, ig::StringView(rowId), patternRows[i],
                             ig::Rect(gridOrigin.x + 84.0f, y, gridWidth,
                                      gridStyle.cellHeight),
                             0, gridStyle);
            }

            const ig::Rect gridBounds(gridOrigin.x + 84.0f, gridOrigin.y + 10.0f,
                                      gridWidth,
                                      static_cast<float>(patternRowCount) * rowPitch - 6.0f);
            daw::stepPlayhead(ui, gridBounds, playheadStep, 0,
                              ig::Color(255u, 255u, 255u, 90u), gridStyle);

            ui.setCursor(ig::Vec2(origin.x, gridBounds.y + gridBounds.height + 18.0f));
            ui.label("Tap a cell to cycle it: on, soft, off.");

            ui.endWindow();
        }

        const ig::DrawData& drawData = ui.endFrame();

        BeginDrawing();
        ClearBackground(::Color{13u, 17u, 28u, 255u});
        backend.render(drawData);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
