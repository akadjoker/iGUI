#pragma once
// Zen Beats sound panel: instrument cards with waveform thumbnails, badges, and
// the synth lab with its labelled slider stacks.
//
// The waveform thumbnails are the interesting part for a port: the reference
// draws a real rendered buffer per sound, so the C++ version synthesises a
// short envelope-shaped buffer per card the same way and draws it as a filled
// silhouette.

#include <igui/Gui.hpp>

#include <math.h>
#include <stdio.h>

#include "Studio.hpp"

namespace zen
{

enum class Badge : unsigned char
{
    None = 0,
    ZenBeats,
    Candidate,
    New,
    NewEngine
};

struct SoundCard
{
    const char* name;
    const char* code;      // the monospaced id on the right
    ig::Color accent;
    Badge badge;
    // Thumbnail shape: decay rate and how noisy vs tonal the body is.
    float decay;
    float noisiness;
    float pitch;
};

struct SynthParams
{
    int osc1;
    int osc2;
    float osc2Detune;   // cents
    float osc2Level;
    float subLevel;
    float noise;
    int filterType;
    float cutoff;       // Hz
    float resonance;
    float filterEnv;    // octaves
    float envDecay;     // ms
    float ampAttack;    // ms
    float ampDecay;
    float ampSustain;
    float ampRelease;
    float drive;
    float glide;
    int lfoTarget;
    float lfoRate;
    float lfoDepth;
    int unisonVoices;
    float unisonSpread;

    SynthParams()
        : osc1(1), osc2(2), osc2Detune(8.0f), osc2Level(0.35f), subLevel(0.45f),
          noise(0.0f), filterType(0), cutoff(420.0f), resonance(5.0f),
          filterEnv(3.0f), envDecay(160.0f), ampAttack(3.0f), ampDecay(220.0f),
          ampSustain(0.50f), ampRelease(80.0f), drive(0.25f), glide(0.0f),
          lfoTarget(0), lfoRate(4.0f), lfoDepth(0.0f), unisonVoices(1),
          unisonSpread(0.0f) {}
};

struct SoundPanelState
{
    int selectedPreset;
    float volume;
    SynthParams synth;
    int demoKind;
    int demoBpm;
    float noteLength;   // ms

    SoundPanelState()
        : selectedPreset(11), volume(0.8f), synth(), demoKind(0), demoBpm(124),
          noteLength(350.0f) {}
};

// ── Badge ───────────────────────────────────────────────────────────────────

inline void badge(ig::Context& ui, const ig::Vec2& position, Badge kind,
                  const Palette& palette)
{
    const char* label = 0;
    ig::Color color(0u, 0u, 0u, 0u);
    switch (kind)
    {
    case Badge::ZenBeats:  label = "ZEN BEATS";  color = ig::Color(120u, 200u, 236u, 255u); break;
    case Badge::Candidate: label = "CANDIDATE";  color = ig::Color(150u, 224u, 150u, 255u); break;
    case Badge::New:       label = "NEW";        color = ig::Color(120u, 226u, 170u, 255u); break;
    case Badge::NewEngine: label = "NEW ENGINE"; color = ig::Color(236u, 196u, 96u, 255u); break;
    default: return;
    }
    const float width = ui.textWidth(ig::StringView(label)) + 12.0f;
    const ig::Rect pillRect(position.x, position.y, width, 15.0f);
    ui.drawRectRounded(pillRect, 7.5f, color, 1.0f);
    ui.drawText(ig::StringView(label), ig::Vec2(position.x + 6.0f, position.y + 1.0f),
                color);
    (void)palette;
}

// ── Waveform thumbnail ──────────────────────────────────────────────────────
// A decaying, partly noisy body, drawn as a filled silhouette around the
// centre line - the same visual language as the reference cards.
inline void waveThumb(ig::Context& ui, const ig::Rect& bounds, const SoundCard& card,
                      float phase)
{
    const int columns = static_cast<int>(bounds.width / 2.0f);
    const float midY = bounds.y + bounds.height * 0.5f;
    const float maxAmplitude = bounds.height * 0.46f;

    for (int i = 0; i < columns; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(columns - 1);
        // Envelope: fast attack, exponential decay at the card's own rate.
        const float attack = t < 0.02f ? (t / 0.02f) : 1.0f;
        const float envelope = attack * expf(-t * card.decay);
        // Body: a tone at the card's pitch, plus a deterministic pseudo-noise
        // so percussive cards look grainy and tonal ones look smooth.
        const float tone = sinf(t * card.pitch + phase);
        const float grain = sinf(t * 337.0f + phase * 3.1f) *
                            sinf(t * 911.0f + phase * 1.7f);
        const float body = tone * (1.0f - card.noisiness) + grain * card.noisiness;
        float amplitude = envelope * maxAmplitude * (0.35f + 0.65f * fabsf(body));
        if (amplitude < 0.6f)
            amplitude = 0.6f;
        const float x = bounds.x + static_cast<float>(i) * 2.0f;
        ui.drawRectFilledRounded(ig::Rect(x, midY - amplitude, 1.6f, amplitude * 2.0f),
                                 0.0f, card.accent);
    }
}

// ── Labelled slider ─────────────────────────────────────────────────────────
// Row of "Label ............ value" with the track underneath, as in the synth
// lab column. Returns true while being dragged.
inline bool labelledSlider(ig::Context& ui, ig::StringView id, const char* label,
                           const char* valueText, float& value, float minimum,
                           float maximum, const ig::Rect& bounds,
                           const Palette& palette)
{
    ui.drawText(ig::StringView(label), ig::Vec2(bounds.x, bounds.y), palette.dim);
    const float valueWidth = ui.textWidth(ig::StringView(valueText));
    ui.drawText(ig::StringView(valueText),
                ig::Vec2(bounds.x + bounds.width - valueWidth, bounds.y), palette.text);

    const ig::Rect track(bounds.x, bounds.y + 18.0f, bounds.width, 4.0f);
    ui.drawRectFilledRounded(track, 2.0f, ig::Color(38u, 45u, 60u, 255u));
    const float span = (maximum > minimum) ? (maximum - minimum) : 1.0f;
    float norm = (value - minimum) / span;
    norm = norm < 0.0f ? 0.0f : (norm > 1.0f ? 1.0f : norm);
    ui.drawRectFilledRounded(ig::Rect(track.x, track.y, track.width * norm, 4.0f), 2.0f,
                             palette.accent);
    ui.drawCircleFilled(ig::Vec2(track.x + track.width * norm, track.y + 2.0f), 6.0f,
                        palette.accent);

    const ig::Rect grab(track.x - 8.0f, track.y - 10.0f, track.width + 16.0f, 24.0f);
    const ig::Vec2 press = ui.pointerPressedPosition(ig::PointerButton::Left);
    if (ui.isPointerButtonDown(ig::PointerButton::Left) && press.x >= grab.x &&
        press.x <= grab.x + grab.width && press.y >= grab.y &&
        press.y <= grab.y + grab.height)
    {
        float t = (ui.pointerPosition().x - track.x) / track.width;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        value = minimum + t * span;
        ui.invisibleButton(id, grab);
        return true;
    }
    ui.invisibleButton(id, grab);
    return false;
}

// Dropdown-looking field. Cycles through its options on click, which is enough
// for a look-and-feel comparison.
inline void selectField(ig::Context& ui, ig::StringView id, const char* label,
                        int& index, const char* const* options, int optionCount,
                        const ig::Rect& bounds, const Palette& palette)
{
    ui.drawText(ig::StringView(label), ig::Vec2(bounds.x, bounds.y), palette.dim);
    const ig::Rect field(bounds.x, bounds.y + 16.0f, bounds.width, 24.0f);
    ui.drawRectFilledRounded(field, 5.0f, ig::Color(24u, 30u, 43u, 255u));
    ui.drawRectRounded(field, 5.0f, palette.panelEdge, 1.0f);
    ui.drawText(ig::StringView(options[index]),
                ig::Vec2(field.x + 8.0f, field.y + 4.0f), palette.text);
    // Chevron.
    const float cx = field.x + field.width - 14.0f;
    const float cy = field.y + 11.0f;
    ui.drawLine(ig::Vec2(cx - 4.0f, cy - 2.0f), ig::Vec2(cx, cy + 2.0f), palette.dim, 1.6f);
    ui.drawLine(ig::Vec2(cx, cy + 2.0f), ig::Vec2(cx + 4.0f, cy - 2.0f), palette.dim, 1.6f);
    if (ui.isClicked(id, field))
        index = (index + 1) % optionCount;
}

// ── Piano keyboard ──────────────────────────────────────────────────────────

inline int pianoKeys(ig::Context& ui, const ig::Rect& bounds, int firstNote,
                     int whiteKeyCount, int highlightNote, bool showNames,
                     const Palette& palette)
{
    static const int kWhiteOffsets[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int kBlackAfter[7] = {1, 1, 0, 1, 1, 1, 0};
    static const char* kNoteNames[7] = {"C", "D", "E", "F", "G", "A", "B"};

    const float whiteWidth = bounds.width / static_cast<float>(whiteKeyCount);
    int clicked = -1;

    for (int i = 0; i < whiteKeyCount; ++i)
    {
        const int octave = i / 7;
        const int degree = i % 7;
        const int note = firstNote + octave * 12 + kWhiteOffsets[degree];
        const ig::Rect key(bounds.x + static_cast<float>(i) * whiteWidth, bounds.y,
                           whiteWidth - 1.5f, bounds.height);
        char id[32];
        snprintf(id, sizeof(id), "pk.w.%d", note);
        const ig::StringView keyId(id);
        ig::Color color = ui.isHovered(keyId, key) ? ig::Color(206u, 214u, 230u, 255u)
                                                   : ig::Color(238u, 242u, 250u, 255u);
        if (note == highlightNote)
            color = palette.accent;
        ui.drawRectFilledRounded(key, 3.0f, color);
        // Note names on the C of each octave, as the reference labels C1/C2/C3.
        if (showNames && degree == 0)
        {
            char name[8];
            snprintf(name, sizeof(name), "%s%d", kNoteNames[degree],
                     (firstNote + octave * 12) / 12 - 1);
            ui.drawText(ig::StringView(name),
                        ig::Vec2(key.x + 4.0f, key.y + key.height - 15.0f),
                        ig::Color(120u, 130u, 150u, 255u));
        }
        if (ui.isClicked(keyId, key))
            clicked = note;
    }

    const float blackWidth = whiteWidth * 0.60f;
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
        snprintf(id, sizeof(id), "pk.b.%d", note);
        const ig::StringView keyId(id);
        ig::Color color = ui.isHovered(keyId, key) ? ig::Color(48u, 56u, 74u, 255u)
                                                   : ig::Color(20u, 25u, 36u, 255u);
        if (note == highlightNote)
            color = ig::Color(42u, 140u, 190u, 255u);
        ui.drawRectFilledRounded(key, 3.0f, color);
        if (ui.isClicked(keyId, key))
            clicked = note;
    }
    return clicked;
}

// ── The panel ───────────────────────────────────────────────────────────────

inline void soundPanel(ig::Context& ui, const ig::Rect& bounds,
                       SoundPanelState& state, const Palette& palette)
{
    static const SoundCard kDrums[] = {
        {"Kick", "kick", ig::Color(232u, 188u, 64u, 255u), Badge::None, 5.0f, 0.25f, 60.0f},
        {"Clap", "clap", ig::Color(236u, 118u, 96u, 255u), Badge::None, 9.0f, 0.85f, 180.0f},
        {"Snare", "snare", ig::Color(236u, 120u, 150u, 255u), Badge::None, 8.0f, 0.75f, 200.0f},
        {"Hi-hat", "hat", ig::Color(126u, 206u, 232u, 255u), Badge::None, 16.0f, 0.90f, 420.0f},
        {"Open hat", "ohat", ig::Color(110u, 170u, 232u, 255u), Badge::None, 4.0f, 0.90f, 430.0f},
        {"Shaker", "shaker", ig::Color(168u, 220u, 118u, 255u), Badge::None, 11.0f, 0.95f, 500.0f},
        {"Conga", "conga", ig::Color(220u, 198u, 160u, 255u), Badge::None, 7.0f, 0.20f, 150.0f},
        {"Rim", "rim", ig::Color(186u, 158u, 226u, 255u), Badge::None, 14.0f, 0.55f, 260.0f},
        {"Log drum", "logdrum", ig::Color(96u, 158u, 232u, 255u), Badge::None, 6.0f, 0.10f, 120.0f},
        {"808", "bass808", ig::Color(206u, 106u, 226u, 255u), Badge::None, 3.0f, 0.05f, 45.0f},
        {"Low tom", "tomlow", ig::Color(226u, 172u, 108u, 255u), Badge::None, 5.5f, 0.18f, 100.0f},
        {"High tom", "tomhigh", ig::Color(228u, 186u, 120u, 255u), Badge::None, 6.5f, 0.18f, 160.0f},
        {"Bongo", "bongo", ig::Color(214u, 178u, 130u, 255u), Badge::None, 9.0f, 0.22f, 240.0f},
        {"Cowbell", "cowbell", ig::Color(236u, 196u, 96u, 255u), Badge::None, 7.0f, 0.15f, 320.0f},
        {"Clave", "clave", ig::Color(150u, 226u, 190u, 255u), Badge::None, 13.0f, 0.12f, 380.0f},
        {"Tambourine", "tambourine", ig::Color(168u, 220u, 118u, 255u), Badge::None, 10.0f, 0.92f, 520.0f},
        {"Snap", "snap", ig::Color(236u, 130u, 150u, 255u), Badge::None, 15.0f, 0.88f, 300.0f},
        {"Ride", "ride", ig::Color(122u, 170u, 232u, 255u), Badge::None, 2.5f, 0.85f, 460.0f},
        {"Crash", "crash", ig::Color(150u, 160u, 236u, 255u), Badge::None, 2.0f, 0.95f, 480.0f},
        {"Whistle", "whistle", ig::Color(126u, 226u, 190u, 255u), Badge::None, 3.0f, 0.06f, 600.0f},
    };
    const int drumCount = static_cast<int>(sizeof(kDrums) / sizeof(kDrums[0]));

    static const SoundCard kMelodic[] = {
        {"Piano", "piano", ig::Color(206u, 188u, 236u, 255u), Badge::ZenBeats, 2.2f, 0.05f, 90.0f},
        {"Sax", "sax", ig::Color(236u, 166u, 72u, 255u), Badge::ZenBeats, 1.2f, 0.10f, 70.0f},
        {"Organ", "organ", ig::Color(126u, 226u, 190u, 255u), Badge::ZenBeats, 0.8f, 0.05f, 60.0f},
        {"Organ", "organ", ig::Color(126u, 226u, 190u, 255u), Badge::Candidate, 0.9f, 0.35f, 75.0f},
        {"Harmonica", "harmonica", ig::Color(236u, 186u, 72u, 255u), Badge::ZenBeats, 1.0f, 0.12f, 80.0f},
        {"Harmonica", "harmonica", ig::Color(236u, 186u, 72u, 255u), Badge::Candidate, 1.1f, 0.30f, 85.0f},
    };
    const int melodicCount = static_cast<int>(sizeof(kMelodic) / sizeof(kMelodic[0]));

    struct Preset { const char* name; Badge badge; };
    static const Preset kPresets[] = {
        {"Clean sub", Badge::ZenBeats}, {"House pluck", Badge::ZenBeats},
        {"Reese", Badge::ZenBeats},     {"Acid", Badge::ZenBeats},
        {"Round bass", Badge::ZenBeats},{"808 glide", Badge::New},
        {"Dub sub", Badge::New},        {"Chord stab", Badge::New},
        {"Hoover", Badge::New},         {"Glass pluck", Badge::New},
        {"Noise sweep", Badge::New},    {"Soft pad", Badge::New},
        {"Wobble", Badge::NewEngine},   {"Supersaw", Badge::NewEngine},
        {"Trance pad", Badge::NewEngine},{"Vibrato lead", Badge::NewEngine},
        {"Tremolo keys", Badge::NewEngine},
    };
    const int presetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

    const float phase = 0.0f;
    char text[48];

    // ── Header ──────────────────────────────────────────────────────────
    ui.drawText(ig::StringView("Zen Beats \xc2\xb7 Sound panel"),
                ig::Vec2(bounds.x + 4.0f, bounds.y), palette.accent);
    {
        const ig::Rect volumeTrack(bounds.x + bounds.width - 420.0f, bounds.y + 8.0f,
                                   110.0f, 4.0f);
        ui.drawText(ig::StringView("Volume"),
                    ig::Vec2(volumeTrack.x - 54.0f, bounds.y + 2.0f), palette.dim);
        ui.drawRectFilledRounded(volumeTrack, 2.0f, ig::Color(38u, 45u, 60u, 255u));
        ui.drawRectFilledRounded(
            ig::Rect(volumeTrack.x, volumeTrack.y, volumeTrack.width * state.volume,
                     4.0f),
            2.0f, palette.accent);
        ui.drawCircleFilled(
            ig::Vec2(volumeTrack.x + volumeTrack.width * state.volume,
                     volumeTrack.y + 2.0f),
            6.0f, palette.accent);
        pill(ui, "sp.stop", "Stop",
             ig::Rect(bounds.x + bounds.width - 290.0f, bounds.y - 2.0f, 62.0f, 22.0f),
             false, palette.accent, palette);
        pill(ui, "sp.bank", "Bank JSON",
             ig::Rect(bounds.x + bounds.width - 220.0f, bounds.y - 2.0f, 96.0f, 22.0f),
             false, palette.accent, palette);
        pill(ui, "sp.import", "Import JSON",
             ig::Rect(bounds.x + bounds.width - 116.0f, bounds.y - 2.0f, 106.0f, 22.0f),
             false, palette.accent, palette);
    }

    // ── Drums & percussion ──────────────────────────────────────────────
    float y = bounds.y + 34.0f;
    ui.drawText(ig::StringView("Drums & percussion"), ig::Vec2(bounds.x + 4.0f, y),
                palette.text);
    ui.drawText(ig::StringView("Click a card to play. The same synthesis code as Zen Beats."),
                ig::Vec2(bounds.x + 160.0f, y + 1.0f), palette.dim);
    y += 22.0f;

    const int perRow = 7;
    const float cardWidth = (bounds.width - 20.0f) / static_cast<float>(perRow) - 8.0f;
    const float cardHeight = 96.0f;
    for (int i = 0; i < drumCount; ++i)
    {
        const int row = i / perRow;
        const int column = i % perRow;
        const ig::Rect card(bounds.x + 4.0f + static_cast<float>(column) *
                                                  (cardWidth + 8.0f),
                            y + static_cast<float>(row) * (cardHeight + 8.0f),
                            cardWidth, cardHeight);
        ui.drawRectFilledRounded(card, 8.0f, ig::Color(19u, 24u, 36u, 255u));
        ui.drawRectRounded(card, 8.0f, palette.panelEdge, 1.0f);
        ui.drawText(ig::StringView(kDrums[i].name),
                    ig::Vec2(card.x + 10.0f, card.y + 7.0f), palette.text);
        {
            const float codeWidth = ui.textWidth(ig::StringView(kDrums[i].code));
            ui.drawText(ig::StringView(kDrums[i].code),
                        ig::Vec2(card.x + card.width - codeWidth - 10.0f, card.y + 8.0f),
                        palette.dim);
        }
        waveThumb(ui, ig::Rect(card.x + 8.0f, card.y + 26.0f, card.width - 16.0f, 34.0f),
                  kDrums[i], phase);
        ui.drawText(ig::StringView("Pitch"), ig::Vec2(card.x + 10.0f, card.y + 66.0f),
                    palette.dim);
        {
            const ig::Rect track(card.x + 44.0f, card.y + 72.0f, card.width - 84.0f, 3.0f);
            ui.drawRectFilledRounded(track, 1.5f, ig::Color(38u, 45u, 60u, 255u));
            ui.drawCircleFilled(ig::Vec2(track.x + track.width * 0.5f, track.y + 1.5f),
                                5.0f, palette.accent);
            ui.drawText(ig::StringView("0 st"),
                        ig::Vec2(card.x + card.width - 34.0f, card.y + 66.0f),
                        palette.dim);
        }
        char id[32];
        snprintf(id, sizeof(id), "sp.drum.%d", i);
        ui.invisibleButton(ig::StringView(id), card);
    }
    y += static_cast<float>((drumCount + perRow - 1) / perRow) * (cardHeight + 8.0f) + 12.0f;

    // ── Melodic instruments ─────────────────────────────────────────────
    ui.drawText(ig::StringView("Melodic instruments"), ig::Vec2(bounds.x + 4.0f, y),
                palette.text);
    y += 22.0f;
    const float melodicWidth = (bounds.width - 20.0f) / 5.0f - 8.0f;
    const float melodicHeight = 104.0f;
    for (int i = 0; i < melodicCount; ++i)
    {
        const int row = i / 5;
        const int column = i % 5;
        const ig::Rect card(bounds.x + 4.0f + static_cast<float>(column) *
                                                  (melodicWidth + 8.0f),
                            y + static_cast<float>(row) * (melodicHeight + 8.0f),
                            melodicWidth, melodicHeight);
        ui.drawRectFilledRounded(card, 8.0f, ig::Color(19u, 24u, 36u, 255u));
        ui.drawRectRounded(card, 8.0f,
                           kMelodic[i].badge == Badge::Candidate
                               ? ig::Color(150u, 224u, 150u, 140u)
                               : palette.panelEdge,
                           1.0f);
        ui.drawText(ig::StringView(kMelodic[i].name),
                    ig::Vec2(card.x + 10.0f, card.y + 7.0f), palette.text);
        {
            const float nameWidth = ui.textWidth(ig::StringView(kMelodic[i].name));
            badge(ui, ig::Vec2(card.x + 16.0f + nameWidth, card.y + 7.0f),
                  kMelodic[i].badge, palette);
        }
        waveThumb(ui,
                  ig::Rect(card.x + 8.0f, card.y + 28.0f, card.width - 16.0f, 34.0f),
                  kMelodic[i], phase);
        ui.drawText(ig::StringView("Note"), ig::Vec2(card.x + 10.0f, card.y + 70.0f),
                    palette.dim);
        {
            const ig::Rect field(card.x + 44.0f, card.y + 66.0f, 58.0f, 20.0f);
            ui.drawRectFilledRounded(field, 4.0f, ig::Color(24u, 30u, 43u, 255u));
            ui.drawRectRounded(field, 4.0f, palette.panelEdge, 1.0f);
            ui.drawText(ig::StringView("C4"), ig::Vec2(field.x + 8.0f, field.y + 2.0f),
                        palette.text);
        }
        char id[32];
        snprintf(id, sizeof(id), "sp.mel.%d", i);
        ui.invisibleButton(ig::StringView(id), card);
    }
    y += static_cast<float>((melodicCount + 4) / 5) * (melodicHeight + 8.0f) + 12.0f;

    // ── Synth lab ───────────────────────────────────────────────────────
    ui.drawText(ig::StringView("Synth lab"), ig::Vec2(bounds.x + 4.0f, y), palette.text);
    ui.drawText(ig::StringView("The bass synth of Zen Beats, plus an LFO and unison."),
                ig::Vec2(bounds.x + 88.0f, y + 1.0f), palette.dim);
    y += 22.0f;

    // Preset chips.
    float chipX = bounds.x + 4.0f;
    float chipY = y;
    for (int i = 0; i < presetCount; ++i)
    {
        const float width = ui.textWidth(ig::StringView(kPresets[i].name)) + 28.0f;
        if (chipX + width > bounds.x + bounds.width - 10.0f)
        {
            chipX = bounds.x + 4.0f;
            chipY += 48.0f;
        }
        const ig::Rect chip(chipX, chipY, width, 42.0f);
        const bool selected = (i == state.selectedPreset);
        ui.drawRectFilledRounded(chip, 7.0f,
                                 selected ? ig::Color(30u, 40u, 58u, 255u)
                                          : ig::Color(19u, 24u, 36u, 255u));
        ui.drawRectRounded(chip, 7.0f,
                           selected ? palette.accent : palette.panelEdge, 1.0f);
        ui.drawText(ig::StringView(kPresets[i].name),
                    ig::Vec2(chip.x + 10.0f, chip.y + 5.0f), palette.text);
        badge(ui, ig::Vec2(chip.x + 10.0f, chip.y + 23.0f), kPresets[i].badge, palette);
        char id[32];
        snprintf(id, sizeof(id), "sp.preset.%d", i);
        if (ui.isClicked(ig::StringView(id), chip))
            state.selectedPreset = i;
        chipX += width + 8.0f;
    }
    y = chipY + 56.0f;

    // Parameter columns and the keyboard.
    const float columnWidth = 262.0f;
    const ig::Rect paramBox(bounds.x + 4.0f, y, columnWidth * 2.0f + 30.0f, 380.0f);
    ui.drawRectFilledRounded(paramBox, 8.0f, ig::Color(19u, 24u, 36u, 255u));
    ui.drawRectRounded(paramBox, 8.0f, palette.panelEdge, 1.0f);

    static const char* kWaveOptions[5] = {"Sine", "Saw", "Square", "Triangle", "Noise"};
    static const char* kFilterOptions[3] = {"Low-pass", "High-pass", "Band-pass"};
    static const char* kLfoOptions[4] = {"Off", "Pitch", "Cutoff", "Amplitude"};

    SynthParams& synth = state.synth;
    float columnX = paramBox.x + 14.0f;
    float rowY = paramBox.y + 14.0f;

    ui.drawText(ig::StringView("OSCILLATORS"), ig::Vec2(columnX, rowY), palette.dim);
    rowY += 20.0f;
    selectField(ui, "sp.osc1", "Osc 1", synth.osc1, kWaveOptions, 5,
                ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 48.0f;
    selectField(ui, "sp.osc2", "Osc 2", synth.osc2, kWaveOptions, 5,
                ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 48.0f;
    snprintf(text, sizeof(text), "%d ct", static_cast<int>(synth.osc2Detune));
    labelledSlider(ui, "sp.det", "Osc 2 detune", text, synth.osc2Detune, 0.0f, 50.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 36.0f;
    snprintf(text, sizeof(text), "%d%%", static_cast<int>(synth.osc2Level * 100.0f));
    labelledSlider(ui, "sp.o2l", "Osc 2 level", text, synth.osc2Level, 0.0f, 1.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 36.0f;
    snprintf(text, sizeof(text), "%d%%", static_cast<int>(synth.subLevel * 100.0f));
    labelledSlider(ui, "sp.sub", "Sub (octave down)", text, synth.subLevel, 0.0f, 1.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 36.0f;
    snprintf(text, sizeof(text), "%d%%", static_cast<int>(synth.noise * 100.0f));
    labelledSlider(ui, "sp.noise", "Noise", text, synth.noise, 0.0f, 1.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 46.0f;

    ui.drawText(ig::StringView("AMP ENVELOPE"), ig::Vec2(columnX, rowY), palette.dim);
    rowY += 20.0f;
    snprintf(text, sizeof(text), "%d ms", static_cast<int>(synth.ampAttack));
    labelledSlider(ui, "sp.aa", "Attack", text, synth.ampAttack, 0.0f, 500.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 36.0f;
    snprintf(text, sizeof(text), "%d ms", static_cast<int>(synth.ampDecay));
    labelledSlider(ui, "sp.ad", "Decay", text, synth.ampDecay, 0.0f, 1000.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 36.0f;
    snprintf(text, sizeof(text), "%d%%", static_cast<int>(synth.ampSustain * 100.0f));
    labelledSlider(ui, "sp.as", "Sustain", text, synth.ampSustain, 0.0f, 1.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 36.0f;
    snprintf(text, sizeof(text), "%d ms", static_cast<int>(synth.ampRelease));
    labelledSlider(ui, "sp.ar", "Release", text, synth.ampRelease, 0.0f, 1000.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);

    // Second column.
    columnX = paramBox.x + columnWidth + 22.0f;
    rowY = paramBox.y + 14.0f;
    ui.drawText(ig::StringView("FILTER"), ig::Vec2(columnX, rowY), palette.dim);
    rowY += 20.0f;
    selectField(ui, "sp.ftype", "Type", synth.filterType, kFilterOptions, 3,
                ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 48.0f;
    snprintf(text, sizeof(text), "%d Hz", static_cast<int>(synth.cutoff));
    labelledSlider(ui, "sp.cut", "Cutoff", text, synth.cutoff, 20.0f, 12000.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 36.0f;
    snprintf(text, sizeof(text), "%.1f", static_cast<double>(synth.resonance));
    labelledSlider(ui, "sp.res", "Resonance", text, synth.resonance, 0.0f, 20.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 36.0f;
    snprintf(text, sizeof(text), "%.1f oct", static_cast<double>(synth.filterEnv));
    labelledSlider(ui, "sp.fenv", "Filter envelope", text, synth.filterEnv, 0.0f, 8.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 36.0f;
    snprintf(text, sizeof(text), "%d ms", static_cast<int>(synth.envDecay));
    labelledSlider(ui, "sp.fdec", "Envelope decay", text, synth.envDecay, 0.0f, 1000.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 46.0f;

    ui.drawText(ig::StringView("DRIVE & GLIDE"), ig::Vec2(columnX, rowY), palette.dim);
    rowY += 20.0f;
    snprintf(text, sizeof(text), "%d%%", static_cast<int>(synth.drive * 100.0f));
    labelledSlider(ui, "sp.drive", "Drive", text, synth.drive, 0.0f, 1.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 36.0f;
    snprintf(text, sizeof(text), "%d ms", static_cast<int>(synth.glide));
    labelledSlider(ui, "sp.glide", "Glide", text, synth.glide, 0.0f, 500.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 46.0f;

    ui.drawText(ig::StringView("LFO"), ig::Vec2(columnX, rowY), palette.dim);
    badge(ui, ig::Vec2(columnX + 34.0f, rowY), Badge::NewEngine, palette);
    rowY += 20.0f;
    selectField(ui, "sp.lfot", "Target", synth.lfoTarget, kLfoOptions, 4,
                ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 48.0f;
    snprintf(text, sizeof(text), "%.2f Hz", static_cast<double>(synth.lfoRate));
    labelledSlider(ui, "sp.lfor", "LFO rate", text, synth.lfoRate, 0.0f, 20.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);
    rowY += 36.0f;
    snprintf(text, sizeof(text), "%d%%", static_cast<int>(synth.lfoDepth * 100.0f));
    labelledSlider(ui, "sp.lfod", "LFO depth", text, synth.lfoDepth, 0.0f, 1.0f,
                   ig::Rect(columnX, rowY, columnWidth - 20.0f, 0.0f), palette);

    // ── Preview column ──────────────────────────────────────────────────
    const ig::Rect previewBox(paramBox.x + paramBox.width + 14.0f, paramBox.y,
                              bounds.width - paramBox.width - 26.0f, 380.0f);
    ui.drawRectFilledRounded(previewBox, 8.0f, ig::Color(19u, 24u, 36u, 255u));
    ui.drawRectRounded(previewBox, 8.0f, palette.panelEdge, 1.0f);

    const ig::Rect scope(previewBox.x + 14.0f, previewBox.y + 14.0f,
                         previewBox.width - 28.0f, 96.0f);
    ui.drawRectFilledRounded(scope, 6.0f, ig::Color(14u, 18u, 28u, 255u));

    // Note length row.
    float previewY = scope.y + scope.height + 14.0f;
    pill(ui, "sp.oct.down", "-", ig::Rect(previewBox.x + 14.0f, previewY, 26.0f, 22.0f),
         false, palette.accent, palette);
    ui.drawText(ig::StringView("C1\xe2\x80\x93""C3"),
                ig::Vec2(previewBox.x + 48.0f, previewY + 3.0f), palette.text);
    pill(ui, "sp.oct.up", "+", ig::Rect(previewBox.x + 90.0f, previewY, 26.0f, 22.0f),
         false, palette.accent, palette);
    snprintf(text, sizeof(text), "%d ms", static_cast<int>(state.noteLength));
    labelledSlider(ui, "sp.notelen", "Note length", text, state.noteLength, 50.0f,
                   1000.0f,
                   ig::Rect(previewBox.x + 130.0f, previewY - 14.0f,
                            previewBox.width - 160.0f, 0.0f),
                   palette);
    previewY += 34.0f;

    // Keyboard with note names.
    pianoKeys(ui,
              ig::Rect(previewBox.x + 14.0f, previewY, previewBox.width - 28.0f, 112.0f),
              36, 21, -1, true, palette);
    previewY += 124.0f;

    // Demo row.
    ui.drawText(ig::StringView("Demo"), ig::Vec2(previewBox.x + 14.0f, previewY + 4.0f),
                palette.dim);
    static const char* kDemoOptions[3] = {"Bass line", "Chords", "Arpeggio"};
    selectField(ui, "sp.demo", "", state.demoKind, kDemoOptions, 3,
                ig::Rect(previewBox.x + 54.0f, previewY - 12.0f, 130.0f, 0.0f),
                palette);
    ui.drawText(ig::StringView("BPM"), ig::Vec2(previewBox.x + 198.0f, previewY + 4.0f),
                palette.dim);
    {
        const ig::Rect bpmField(previewBox.x + 230.0f, previewY, 58.0f, 24.0f);
        ui.drawRectFilledRounded(bpmField, 5.0f, ig::Color(24u, 30u, 43u, 255u));
        ui.drawRectRounded(bpmField, 5.0f, palette.panelEdge, 1.0f);
        snprintf(text, sizeof(text), "%d", state.demoBpm);
        ui.drawText(ig::StringView(text), ig::Vec2(bpmField.x + 10.0f, bpmField.y + 4.0f),
                    palette.text);
    }
    pill(ui, "sp.playdemo", "Play demo",
         ig::Rect(previewBox.x + 300.0f, previewY, 100.0f, 24.0f), true, palette.accent,
         palette);
    previewY += 40.0f;

    // Name and export row.
    ui.drawText(ig::StringView("Name"), ig::Vec2(previewBox.x + 14.0f, previewY + 4.0f),
                palette.dim);
    {
        const ig::Rect nameField(previewBox.x + 54.0f, previewY,
                                 previewBox.width - 70.0f, 24.0f);
        ui.drawRectFilledRounded(nameField, 5.0f, ig::Color(24u, 30u, 43u, 255u));
        ui.drawRectRounded(nameField, 5.0f, palette.panelEdge, 1.0f);
        ui.drawText(ig::StringView("My sound"),
                    ig::Vec2(nameField.x + 8.0f, nameField.y + 4.0f), palette.dim);
    }
    previewY += 32.0f;
    pill(ui, "sp.wavnote", "WAV \xc2\xb7 note",
         ig::Rect(previewBox.x + 14.0f, previewY, 96.0f, 24.0f), false, palette.accent,
         palette);
    pill(ui, "sp.wavdemo", "WAV \xc2\xb7 demo",
         ig::Rect(previewBox.x + 118.0f, previewY, 100.0f, 24.0f), false,
         palette.accent, palette);
    pill(ui, "sp.json", "JSON",
         ig::Rect(previewBox.x + 226.0f, previewY, 62.0f, 24.0f), false, palette.accent,
         palette);
    pill(ui, "sp.copy", "Copy for Zen Beats",
         ig::Rect(previewBox.x + 296.0f, previewY, 140.0f, 24.0f), false,
         palette.accent, palette);
}

} // namespace zen
