// Zen Beats, rebuilt in C++ over iGUI as a portability test.
//
// Two views, switched with the tabs at the top:
//   Studio      - transport, arrangement, mixer cards, pattern grid
//   Sound panel - instrument cards with waveform thumbnails, and the synth lab
//
// The point is to compare against the HTML/JS original: does the C++ version
// look as good, and does it cost noticeably more code? Everything is drawn
// with the Context's public primitives - none of this is an iGUI widget.
//
// Build: cmake -DIGUI_BUILD_RAYLIB_DEMO=ON -DIGUI_BUILD_ZENBEATS_DEMO=ON
// Run:   ./igui_zenbeats_demo

#include <raylib.h>

#include <math.h>
#include <stdio.h>

#include <igui/Gui.hpp>
#include <igui_raylib/RaylibBackend.hpp>

#include "Studio.hpp"
#include "SoundPanel.hpp"

namespace
{

const int kSteps = 32;

// The knob from the synth demo, kept here so this example stands alone.
const float kPi = 3.14159265358979323846f;
const float kStartAngle = 135.0f * kPi / 180.0f;
const float kSweepAngle = 270.0f * kPi / 180.0f;

struct KnobDrag
{
    bool active;
    float originX;
    float originY;
    float valueAtPress;
    KnobDrag() : active(false), originX(0.0f), originY(0.0f), valueAtPress(0.0f) {}
};

KnobDrag gDrag;

void arcSegments(ig::Context& ui, float cx, float cy, float radius, float fromTurn,
                 float toTurn, const ig::Color& color, float thickness)
{
    if (toTurn <= fromTurn)
        return;
    const int segments = 56;
    const float startStep = fromTurn * segments;
    const float endStep = toTurn * segments;
    int index = static_cast<int>(startStep);
    while (static_cast<float>(index) < endStep)
    {
        const float from = (static_cast<float>(index) > startStep)
                               ? static_cast<float>(index) : startStep;
        const float to = (static_cast<float>(index + 1) < endStep)
                             ? static_cast<float>(index + 1) : endStep;
        const float a0 = kStartAngle + kSweepAngle * from / segments;
        const float a1 = kStartAngle + kSweepAngle * to / segments;
        ui.drawLine(ig::Vec2(cx + radius * cosf(a0), cy + radius * sinf(a0)),
                    ig::Vec2(cx + radius * cosf(a1), cy + radius * sinf(a1)),
                    color, thickness);
        ++index;
    }
}

// Rotating-body knob: the cap turns and carries its notch, the value arc sits
// outside it.
bool knob(ig::Context& ui, ig::StringView id, const char* label,
          const char* displayText, float& value, float minimum, float maximum,
          const ig::Vec2& topLeft, float diameter, const ig::Color& accent,
          const zen::Palette& palette)
{
    const float cx = topLeft.x + diameter * 0.5f;
    const float cy = topLeft.y + diameter * 0.5f;
    const float arcThickness = 4.0f;
    const float arcRadius = diameter * 0.5f - arcThickness * 0.5f;
    const float bodyRadius = diameter * 0.5f - arcThickness - 2.5f;

    const float span = (maximum > minimum) ? (maximum - minimum) : 1.0f;
    float norm = (value - minimum) / span;
    norm = norm < 0.0f ? 0.0f : (norm > 1.0f ? 1.0f : norm);
    const float angle = kStartAngle + kSweepAngle * norm;

    arcSegments(ui, cx, cy, arcRadius, 0.0f, 1.0f, ig::Color(44u, 51u, 66u, 255u),
                arcThickness);
    arcSegments(ui, cx, cy, arcRadius, 0.0f, norm, accent, arcThickness);

    // Domed cap: shrinking circles from the dark bottom tone to the light top.
    const int shadeSteps = 7;
    for (int i = 0; i < shadeSteps; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(shadeSteps - 1);
        const ig::Color shade(static_cast<uint8_t>(38.0f + (74.0f - 38.0f) * t),
                              static_cast<uint8_t>(43.0f + (82.0f - 43.0f) * t),
                              static_cast<uint8_t>(56.0f + (100.0f - 56.0f) * t),
                              255u);
        ui.drawCircleFilled(ig::Vec2(cx, cy - t * bodyRadius * 0.30f),
                            bodyRadius * (1.0f - t * 0.55f), shade);
    }
    ui.drawLine(ig::Vec2(cx + cosf(angle) * bodyRadius * 0.20f,
                         cy + sinf(angle) * bodyRadius * 0.20f),
                ig::Vec2(cx + cosf(angle) * bodyRadius * 0.92f,
                         cy + sinf(angle) * bodyRadius * 0.92f),
                ig::Color(236u, 242u, 252u, 255u), 2.2f);

    const float valueWidth = ui.textWidth(ig::StringView(displayText));
    ui.drawText(ig::StringView(displayText),
                ig::Vec2(cx - valueWidth * 0.5f, topLeft.y + diameter + 3.0f),
                palette.text);
    const float labelWidth = ui.textWidth(ig::StringView(label));
    ui.drawText(ig::StringView(label),
                ig::Vec2(cx - labelWidth * 0.5f, topLeft.y + diameter + 17.0f),
                palette.dim);

    const ig::Rect hit(topLeft.x, topLeft.y, diameter, diameter);
    const ig::Vec2 press = ui.pointerPressedPosition(ig::PointerButton::Left);
    const bool inside = press.x >= hit.x && press.x <= hit.x + hit.width &&
                        press.y >= hit.y && press.y <= hit.y + hit.height;
    bool changed = false;
    if (ui.isPointerButtonDown(ig::PointerButton::Left) && inside)
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

} // namespace

int main()
{
    InitWindow(1560, 980, "Zen Beats - iGUI port test");
    SetTargetFPS(60);

    ig::raylib::Backend backend;
    if (!backend.prepareFontAtlas())
    {
        CloseWindow();
        return 1;
    }

    ig::Context ui(backend, &backend.fontAtlas());
    const zen::Palette palette;

    int view = 0; // 0 = studio, 1 = sound panel

    zen::Transport transport;
    transport.playing = false;
    transport.recording = false;
    transport.loopMode = false;
    transport.bpm = 122;
    transport.swing = 30;
    transport.bar = 1;
    transport.beat = 1;
    transport.tick = 1;

    zen::ArrangeTrack arrangeTracks[] = {
        {"Drums", ig::Color(64u, 224u, 190u, 255u), false, false},
        {"Bass", ig::Color(232u, 188u, 64u, 255u), false, false},
        {"Keys", ig::Color(236u, 86u, 158u, 255u), false, false},
        {"FX", ig::Color(96u, 158u, 248u, 255u), false, false},
    };
    const int arrangeTrackCount = 4;
    zen::ArrangeClip clips[] = {
        {"A Full composition", 0, 28, 0},
    };
    int selectedArrangeTrack = 0;

    zen::MixerStrip mixerStrips[] = {
        {"Drums", 0.90f, 0, ig::Color(64u, 224u, 190u, 255u)},
        {"Bass", 0.90f, 0, ig::Color(232u, 188u, 64u, 255u)},
        {"Keys", 0.90f, 0, ig::Color(236u, 86u, 158u, 255u)},
        {"FX", 0.90f, 0, ig::Color(96u, 158u, 248u, 255u)},
    };
    const int mixerStripCount = 4;
    int selectedStrip = 0;
    float masterVolume = 0.90f;
    float djFilter = 0.0f;
    float masterRes = 2.0f;

    const ig::StringView effectItems[] = {
        ig::StringView("+ Effect"), ig::StringView("Reverb"),
        ig::StringView("Delay"), ig::StringView("Filter"),
    };

    // Pattern: nine sounds, thirty-two steps.
    static zen::Cell patternData[9][kSteps];
    for (int r = 0; r < 9; ++r)
        for (int s = 0; s < kSteps; ++s)
            patternData[r][s] = zen::Cell::Off;
    for (int s = 0; s < kSteps; s += 4) patternData[0][s] = zen::Cell::On;   // kick
    for (int s = 2; s < kSteps; s += 4) patternData[2][s] = zen::Cell::On;   // hi-hat
    for (int s = 0; s < kSteps; ++s)
        patternData[3][s] = (s % 2 == 0) ? zen::Cell::On : zen::Cell::Soft;  // shaker
    for (int s = 3; s < kSteps; s += 6) patternData[4][s] = zen::Cell::Soft; // conga

    zen::SoundRow patternRows[] = {
        {"Kick", ig::Color(232u, 188u, 64u, 255u), patternData[0], kSteps, false, false},
        {"Clap", ig::Color(236u, 110u, 96u, 255u), patternData[1], kSteps, false, false},
        {"Hi-hat", ig::Color(126u, 212u, 236u, 255u), patternData[2], kSteps, false, false},
        {"Shaker", ig::Color(168u, 220u, 118u, 255u), patternData[3], kSteps, false, false},
        {"Conga", ig::Color(224u, 200u, 162u, 255u), patternData[4], kSteps, false, false},
        {"Rim", ig::Color(196u, 156u, 232u, 255u), patternData[5], kSteps, false, false},
        {"Bongo", ig::Color(214u, 178u, 130u, 255u), patternData[6], kSteps, false, false},
        {"Clave", ig::Color(150u, 226u, 190u, 255u), patternData[7], kSteps, false, false},
        {"Sax", ig::Color(236u, 188u, 96u, 255u), patternData[8], kSteps, false, false},
    };
    const int patternRowCount = 9;
    int selectedPatternRow = 0;

    float playClock = 0.0f;
    int playheadStep = 0;

    zen::SoundPanelState soundPanel;

    while (!WindowShouldClose())
    {
        const float dt = GetFrameTime();
        ig::raylib::processInput(ui);
        ui.beginFrame(ig::raylib::frameInfo(dt));

        if (transport.playing)
        {
            playClock += dt;
            const float stepDuration = 60.0f / (static_cast<float>(transport.bpm) * 4.0f);
            while (playClock >= stepDuration)
            {
                playClock -= stepDuration;
                playheadStep = (playheadStep + 1) % kSteps;
                transport.tick = playheadStep % 4 + 1;
                transport.beat = (playheadStep / 4) % 4 + 1;
                if (playheadStep == 0)
                    transport.bar = transport.bar % 28 + 1;
            }
        }

        if (ui.beginMainWindow("Zen Beats"))
        {
            const ig::Vec2 top = ui.cursor();
            const float fullWidth = ui.availableWidth();

            // View tabs.
            if (zen::pill(ui, "view.studio", "Studio",
                          ig::Rect(top.x, top.y, 92.0f, 24.0f), view == 0,
                          palette.accent, palette))
                view = 0;
            if (zen::pill(ui, "view.sound", "Sound panel",
                          ig::Rect(top.x + 100.0f, top.y, 116.0f, 24.0f), view == 1,
                          palette.accent, palette))
                view = 1;

            const float bodyY = top.y + 34.0f;

            if (view == 0)
            {
                // ── Transport ───────────────────────────────────────────
                zen::transportBar(ui, ig::Rect(top.x, bodyY, fullWidth, 44.0f),
                                  transport, palette);

                // ── Arrangement ─────────────────────────────────────────
                const ig::Rect arrangeBox(top.x, bodyY + 54.0f, fullWidth, 290.0f);
                zen::section(ui, arrangeBox, "ARRANGEMENT", "Track: Drums", palette);
                zen::pill(ui, "arr.add", "+ Track",
                          ig::Rect(arrangeBox.x + 200.0f, arrangeBox.y + 6.0f, 68.0f,
                                   22.0f),
                          false, palette.accent, palette);
                zen::pill(ui, "arr.build", "Build A to D structure",
                          ig::Rect(arrangeBox.x + 276.0f, arrangeBox.y + 6.0f, 152.0f,
                                   22.0f),
                          false, palette.accent, palette);
                zen::pill(ui, "arr.clear", "Clear",
                          ig::Rect(arrangeBox.x + 436.0f, arrangeBox.y + 6.0f, 56.0f,
                                   22.0f),
                          false, ig::Color(226u, 96u, 96u, 255u), palette);
                {
                    const float barsWidth = ui.textWidth(ig::StringView("28 bars, 0:55"));
                    ui.drawText(ig::StringView("28 bars, 0:55"),
                                ig::Vec2(arrangeBox.x + arrangeBox.width - barsWidth -
                                             14.0f,
                                         arrangeBox.y + 10.0f),
                                palette.dim);
                }
                zen::arrangement(ui,
                                 ig::Rect(arrangeBox.x + 8.0f, arrangeBox.y + 34.0f,
                                          arrangeBox.width - 16.0f, 248.0f),
                                 arrangeTracks, arrangeTrackCount, clips, 1,
                                 selectedArrangeTrack, 28, palette);

                // ── Mixer ───────────────────────────────────────────────
                const ig::Rect mixerBox(top.x, arrangeBox.y + arrangeBox.height + 12.0f,
                                        fullWidth, 190.0f);
                zen::section(ui, mixerBox, "TRACKS, MIXER AND EFFECTS",
                             "Select a track or drag a knob up and down.", palette);

                const float cardY = mixerBox.y + 36.0f;
                const ig::Rect masterCard(mixerBox.x + 14.0f, cardY, 178.0f, 116.0f);
                ui.drawRectFilledRounded(masterCard, 9.0f, ig::Color(19u, 24u, 36u, 255u));
                ui.drawRectRounded(masterCard, 9.0f, palette.panelEdge, 1.0f);
                ui.drawCircleFilled(ig::Vec2(masterCard.x + 14.0f, masterCard.y + 15.0f),
                                    4.0f, ig::Color(96u, 208u, 232u, 255u));
                ui.drawText(ig::StringView("Master"),
                            ig::Vec2(masterCard.x + 24.0f, masterCard.y + 9.0f),
                            palette.text);
                {
                    char text[16];
                    snprintf(text, sizeof(text), "%d%%",
                             static_cast<int>(masterVolume * 100.0f + 0.5f));
                    knob(ui, "mx.master.vol", "Volume", text, masterVolume, 0.0f, 1.0f,
                         ig::Vec2(masterCard.x + 14.0f, masterCard.y + 30.0f), 42.0f,
                         ig::Color(96u, 208u, 232u, 255u), palette);
                    if (djFilter <= 0.001f)
                        snprintf(text, sizeof(text), "off");
                    else
                        snprintf(text, sizeof(text), "%d%%",
                                 static_cast<int>(djFilter * 100.0f + 0.5f));
                    knob(ui, "mx.master.filter", "DJ filter", text, djFilter, 0.0f, 1.0f,
                         ig::Vec2(masterCard.x + 68.0f, masterCard.y + 30.0f), 42.0f,
                         ig::Color(150u, 160u, 180u, 255u), palette);
                    snprintf(text, sizeof(text), "%.1f", static_cast<double>(masterRes));
                    knob(ui, "mx.master.res", "Res", text, masterRes, 0.0f, 4.0f,
                         ig::Vec2(masterCard.x + 122.0f, masterCard.y + 30.0f), 42.0f,
                         ig::Color(232u, 188u, 64u, 255u), palette);
                }

                for (int i = 0; i < mixerStripCount; ++i)
                {
                    const ig::Rect card(masterCard.x + masterCard.width + 14.0f +
                                            static_cast<float>(i) * 146.0f,
                                        cardY, 136.0f, 140.0f);
                    const bool selected = (i == selectedStrip);
                    ui.drawRectFilledRounded(card, 9.0f, ig::Color(19u, 24u, 36u, 255u));
                    ui.drawRectRounded(card, 9.0f,
                                       selected ? mixerStrips[i].accent
                                                : palette.panelEdge,
                                       selected ? 2.0f : 1.0f);
                    ui.drawCircleFilled(ig::Vec2(card.x + 14.0f, card.y + 15.0f), 4.0f,
                                        mixerStrips[i].accent);
                    ui.drawText(ig::StringView(mixerStrips[i].name),
                                ig::Vec2(card.x + 24.0f, card.y + 9.0f), palette.text);
                    {
                        const float nameWidth =
                            ui.textWidth(ig::StringView(mixerStrips[i].name));
                        ui.drawText(ig::StringView("\xc2\xb7track"),
                                    ig::Vec2(card.x + 28.0f + nameWidth, card.y + 10.0f),
                                    palette.dim);
                    }

                    char id[32];
                    snprintf(id, sizeof(id), "mx.card.%d", i);
                    if (ui.isClicked(ig::StringView(id), card))
                        selectedStrip = i;

                    char text[16];
                    snprintf(text, sizeof(text), "%d%%",
                             static_cast<int>(mixerStrips[i].volume * 100.0f + 0.5f));
                    snprintf(id, sizeof(id), "mx.vol.%d", i);
                    knob(ui, ig::StringView(id), "Vol", text, mixerStrips[i].volume,
                         0.0f, 1.0f,
                         ig::Vec2(card.x + card.width * 0.5f - 21.0f, card.y + 30.0f),
                         42.0f, mixerStrips[i].accent, palette);

                    snprintf(id, sizeof(id), "mx.fx.%d", i);
                    ui.comboBox(ig::StringView(id), mixerStrips[i].effect,
                                ig::Span<const ig::StringView>(effectItems, 4),
                                ig::Rect(card.x + 10.0f, card.y + card.height - 30.0f,
                                         card.width - 20.0f, 22.0f));
                }

                // ── Pattern ─────────────────────────────────────────────
                const ig::Rect patternBox(top.x, mixerBox.y + mixerBox.height + 12.0f,
                                          fullWidth, 386.0f);
                zen::section(ui, patternBox, "PATTERN",
                             "Editing pattern A (Full composition)", palette);
                zen::patternGrid(ui,
                                 ig::Rect(patternBox.x + 10.0f, patternBox.y + 32.0f,
                                          patternBox.width - 20.0f, 340.0f),
                                 patternRows, patternRowCount, 0, kSteps, playheadStep,
                                 selectedPatternRow, palette);

                ui.setCursor(ig::Vec2(top.x, patternBox.y + patternBox.height + 8.0f));
                ui.label("Tap a cell to cycle it: on, soft, off. Drag a knob or the BPM field.");
            }
            else
            {
                zen::soundPanel(ui, ig::Rect(top.x, bodyY, fullWidth, 880.0f),
                                soundPanel, palette);
                ui.setCursor(ig::Vec2(top.x, bodyY + 884.0f));
            }

            ui.endWindow();
        }

        const ig::DrawData& drawData = ui.endFrame();

        BeginDrawing();
        ClearBackground(::Color{9u, 12u, 20u, 255u});
        backend.render(drawData);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
