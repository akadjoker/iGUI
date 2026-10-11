// Synth panel: the audio widgets on their own, with no mixer or sequencer
// around them.
//
// The knob is the point of the demo. A real pot is a body you grab and turn,
// carrying its indicator mark around with it; a ring with a thin needle reads
// as a gauge instead. Here the cap is a shaded circle with a notch that
// rotates, and the value arc sits outside it.
//
// All of it is application code over the Context's public building blocks -
// see SynthWidgets.hpp. Nothing here is an iGUI widget.
//
// Build: cmake -DIGUI_BUILD_RAYLIB_DEMO=ON -DIGUI_BUILD_SYNTH_DEMO=ON
// Run:   ./igui_synth_demo

#include <raylib.h>

#include <math.h>
#include <stdio.h>

#include <igui/Gui.hpp>
#include <igui_raylib/RaylibBackend.hpp>

#include "SynthWidgets.hpp"

namespace
{

const float kTwoPi = 6.28318530717958647692f;

// Panel behind a group of controls, with its title along the top.
void panel(ig::Context& ui, const ig::Rect& bounds, const char* title,
           const ig::Color& accent)
{
    ui.drawRectFilledRounded(bounds, 10.0f, ig::Color(23u, 28u, 40u, 255u));
    ui.drawRectRounded(bounds, 10.0f, ig::Color(46u, 54u, 72u, 255u), 1.0f);
    ui.drawCircleFilled(ig::Vec2(bounds.x + 14.0f, bounds.y + 15.0f), 4.0f, accent);
    ui.drawText(ig::StringView(title), ig::Vec2(bounds.x + 24.0f, bounds.y + 9.0f),
                ig::Color(226u, 232u, 244u, 255u));
}

const char* kWaveNames[4] = {"Sine", "Saw", "Square", "Triangle"};

float waveSample(int wave, float phase)
{
    switch (wave)
    {
    case 1: return 2.0f * (phase - floorf(phase + 0.5f));                 // saw
    case 2: return (phase - floorf(phase)) < 0.5f ? 1.0f : -1.0f;         // square
    case 3:                                                               // triangle
    {
        const float t = phase - floorf(phase);
        return t < 0.5f ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t);
    }
    default: return sinf(phase * kTwoPi);                                 // sine
    }
}

} // namespace

int main()
{
    InitWindow(1180, 760, "iGUI synth demo");
    SetTargetFPS(60);

    ig::raylib::Backend backend;
    if (!backend.prepareFontAtlas())
    {
        CloseWindow();
        return 1;
    }

    ig::Context ui(backend, &backend.fontAtlas());

    // Oscillator.
    int waveform = 1;
    float tune = 0.0f;         // semitones
    float detune = 0.12f;
    float oscLevel = 0.85f;

    // Filter.
    float cutoff = 0.62f;
    float resonance = 1.8f;
    float envAmount = 0.45f;

    // Envelope and amp.
    synth::Envelope envelope;
    float drive = 0.25f;
    float masterLevel = 0.9f;

    int lastNote = -1;
    float noteAge = 0.0f;

    float waveSamples[220];
    float spectrumBins[36];
    float meterLevel = 0.0f;
    float meterPeak = 0.0f;
    float clock = 0.0f;

    while (!WindowShouldClose())
    {
        const float dt = GetFrameTime();
        clock += dt;
        noteAge += dt;

        ig::raylib::processInput(ui);
        ui.beginFrame(ig::raylib::frameInfo(dt));

        // Fake but plausible analysis data, so the meters move with the knobs
        // rather than sitting still.
        const float cycles = 2.0f + tune * 0.12f;
        for (int i = 0; i < 220; ++i)
        {
            const float t = static_cast<float>(i) / 219.0f;
            float s = waveSample(waveform, t * cycles + clock * 0.6f);
            // The filter knob rolls off the wave's edges.
            s *= 0.35f + cutoff * 0.65f;
            s += 0.08f * detune * sinf((t * 37.0f) + clock * 3.1f);
            waveSamples[i] = s * oscLevel;
        }
        for (int i = 0; i < 36; ++i)
        {
            const float f = static_cast<float>(i) / 35.0f;
            // A peak that tracks cutoff, with resonance sharpening it.
            const float distance = fabsf(f - cutoff);
            float magnitude = expf(-distance * (6.0f + resonance * 3.0f));
            magnitude *= 0.55f + 0.45f * sinf(clock * 4.0f + f * 9.0f);
            magnitude *= oscLevel;
            spectrumBins[i] = magnitude < 0.0f ? 0.0f : magnitude;
        }

        // Envelope-driven level, retriggered when a key is pressed.
        float envLevel = 0.0f;
        if (noteAge < envelope.attack)
            envLevel = noteAge / (envelope.attack + 0.0001f);
        else if (noteAge < envelope.attack + envelope.decay)
        {
            const float t = (noteAge - envelope.attack) / (envelope.decay + 0.0001f);
            envLevel = 1.0f - (1.0f - envelope.sustain) * t;
        }
        else if (noteAge < 1.2f)
            envLevel = envelope.sustain;
        else
        {
            const float t = (noteAge - 1.2f) / (envelope.release + 0.0001f);
            envLevel = envelope.sustain * (t > 1.0f ? 0.0f : (1.0f - t));
        }
        meterLevel = envLevel * masterLevel * (0.75f + drive * 0.5f);
        if (meterLevel > 1.0f)
            meterLevel = 1.0f;
        meterPeak = meterLevel > meterPeak ? meterLevel : meterPeak - dt * 0.35f;
        if (meterPeak < 0.0f)
            meterPeak = 0.0f;

        if (ui.beginMainWindow("Synth"))
        {
            ui.label("Drag a knob up or down. Click the keyboard to retrigger the envelope.");
            ui.separator();

            const ig::Vec2 origin = ui.cursor();
            const float knobSize = 54.0f;
            const float rowY = origin.y + 8.0f;
            const float panelHeight = 132.0f;
            char text[24];

            // ── Oscillator ──────────────────────────────────────────────
            const ig::Rect oscPanel(origin.x + 4.0f, rowY, 268.0f, panelHeight);
            panel(ui, oscPanel, "Oscillator", ig::Color(96u, 212u, 255u, 255u));
            {
                synth::KnobStyle style;
                style.fill = ig::Color(96u, 212u, 255u, 255u);

                snprintf(text, sizeof(text), "%+.0f st", static_cast<double>(tune));
                synth::knob(ui, "osc.tune", "Tune", text, tune, -24.0f, 24.0f,
                            ig::Vec2(oscPanel.x + 18.0f, oscPanel.y + 30.0f),
                            knobSize, style);

                snprintf(text, sizeof(text), "%.0f c",
                         static_cast<double>(detune * 100.0f));
                synth::knob(ui, "osc.detune", "Detune", text, detune, 0.0f, 1.0f,
                            ig::Vec2(oscPanel.x + 96.0f, oscPanel.y + 30.0f),
                            knobSize, style);

                snprintf(text, sizeof(text), "%d%%",
                         static_cast<int>(oscLevel * 100.0f + 0.5f));
                synth::knob(ui, "osc.level", "Level", text, oscLevel, 0.0f, 1.0f,
                            ig::Vec2(oscPanel.x + 174.0f, oscPanel.y + 30.0f),
                            knobSize, style);
            }

            // ── Filter ──────────────────────────────────────────────────
            const ig::Rect filterPanel(oscPanel.x + oscPanel.width + 12.0f, rowY,
                                       268.0f, panelHeight);
            panel(ui, filterPanel, "Filter", ig::Color(232u, 188u, 64u, 255u));
            {
                synth::KnobStyle style;
                style.fill = ig::Color(232u, 188u, 64u, 255u);

                snprintf(text, sizeof(text), "%d Hz",
                         static_cast<int>(60.0f + cutoff * cutoff * 11000.0f));
                synth::knob(ui, "flt.cutoff", "Cutoff", text, cutoff, 0.0f, 1.0f,
                            ig::Vec2(filterPanel.x + 18.0f, filterPanel.y + 30.0f),
                            knobSize, style);

                snprintf(text, sizeof(text), "%.1f", static_cast<double>(resonance));
                synth::knob(ui, "flt.res", "Res", text, resonance, 0.0f, 4.0f,
                            ig::Vec2(filterPanel.x + 96.0f, filterPanel.y + 30.0f),
                            knobSize, style);

                snprintf(text, sizeof(text), "%d%%",
                         static_cast<int>(envAmount * 100.0f + 0.5f));
                synth::knob(ui, "flt.env", "Env amt", text, envAmount, 0.0f, 1.0f,
                            ig::Vec2(filterPanel.x + 174.0f, filterPanel.y + 30.0f),
                            knobSize, style);
            }

            // ── Amp ─────────────────────────────────────────────────────
            const ig::Rect ampPanel(filterPanel.x + filterPanel.width + 12.0f, rowY,
                                    190.0f, panelHeight);
            panel(ui, ampPanel, "Amp", ig::Color(236u, 86u, 158u, 255u));
            {
                synth::KnobStyle style;
                style.fill = ig::Color(236u, 86u, 158u, 255u);

                snprintf(text, sizeof(text), "%d%%",
                         static_cast<int>(drive * 100.0f + 0.5f));
                synth::knob(ui, "amp.drive", "Drive", text, drive, 0.0f, 1.0f,
                            ig::Vec2(ampPanel.x + 18.0f, ampPanel.y + 30.0f),
                            knobSize, style);

                snprintf(text, sizeof(text), "%d%%",
                         static_cast<int>(masterLevel * 100.0f + 0.5f));
                synth::knob(ui, "amp.master", "Master", text, masterLevel, 0.0f, 1.0f,
                            ig::Vec2(ampPanel.x + 96.0f, ampPanel.y + 30.0f),
                            knobSize, style);
            }

            // ── Output meter ────────────────────────────────────────────
            const ig::Rect meterPanel(ampPanel.x + ampPanel.width + 12.0f, rowY,
                                      104.0f, panelHeight);
            panel(ui, meterPanel, "Out", ig::Color(78u, 214u, 150u, 255u));
            synth::vuMeter(ui, ig::Rect(meterPanel.x + 24.0f, meterPanel.y + 30.0f,
                                        22.0f, panelHeight - 44.0f),
                           meterLevel, meterPeak);
            synth::vuMeter(ui, ig::Rect(meterPanel.x + 58.0f, meterPanel.y + 30.0f,
                                        22.0f, panelHeight - 44.0f),
                           meterLevel * 0.92f, meterPeak * 0.92f);

            // ── Waveform, spectrum, envelope ────────────────────────────
            const float viewY = rowY + panelHeight + 14.0f;
            const float viewHeight = 150.0f;

            const ig::Rect wavePanel(origin.x + 4.0f, viewY, 380.0f, viewHeight);
            panel(ui, wavePanel, kWaveNames[waveform], ig::Color(96u, 212u, 255u, 255u));
            synth::waveformView(ui,
                                ig::Rect(wavePanel.x + 10.0f, wavePanel.y + 30.0f,
                                         wavePanel.width - 20.0f, viewHeight - 44.0f),
                                waveSamples, 220, ig::Color(96u, 212u, 255u, 255u));
            // Waveform selector under the title.
            for (int i = 0; i < 4; ++i)
            {
                const ig::Rect tab(wavePanel.x + 120.0f + static_cast<float>(i) * 62.0f,
                                   wavePanel.y + 6.0f, 58.0f, 20.0f);
                char tabId[32];
                snprintf(tabId, sizeof(tabId), "wave.%d", i);
                const ig::StringView tabIdView(tabId);
                const bool selected = (i == waveform);
                ui.drawRectFilledRounded(tab, 5.0f,
                                         selected ? ig::Color(50u, 110u, 150u, 255u)
                                                  : ig::Color(32u, 38u, 52u, 255u));
                const float w = ui.textWidth(ig::StringView(kWaveNames[i]));
                ui.drawText(ig::StringView(kWaveNames[i]),
                            ig::Vec2(tab.x + (tab.width - w) * 0.5f, tab.y + 3.0f),
                            selected ? ig::Color(226u, 240u, 252u, 255u)
                                     : ig::Color(150u, 160u, 178u, 255u));
                if (ui.isClicked(tabIdView, tab))
                    waveform = i;
            }

            const ig::Rect spectrumPanel(wavePanel.x + wavePanel.width + 12.0f, viewY,
                                         380.0f, viewHeight);
            panel(ui, spectrumPanel, "Spectrum", ig::Color(232u, 188u, 64u, 255u));
            synth::spectrumView(ui,
                                ig::Rect(spectrumPanel.x + 10.0f, spectrumPanel.y + 30.0f,
                                         spectrumPanel.width - 20.0f, viewHeight - 44.0f),
                                spectrumBins, 36, ig::Color(232u, 188u, 64u, 255u));

            const ig::Rect envPanel(spectrumPanel.x + spectrumPanel.width + 12.0f,
                                    viewY, 290.0f, viewHeight);
            panel(ui, envPanel, "Envelope", ig::Color(236u, 86u, 158u, 255u));
            synth::envelopeView(ui,
                                ig::Rect(envPanel.x + 10.0f, envPanel.y + 30.0f,
                                         envPanel.width - 20.0f, viewHeight - 44.0f),
                                envelope, ig::Color(236u, 86u, 158u, 255u),
                                ig::Color(236u, 86u, 158u, 46u));

            // ── ADSR knobs ──────────────────────────────────────────────
            const float adsrY = viewY + viewHeight + 14.0f;
            const ig::Rect adsrPanel(origin.x + 4.0f, adsrY, 380.0f, 112.0f);
            panel(ui, adsrPanel, "ADSR", ig::Color(236u, 86u, 158u, 255u));
            {
                synth::KnobStyle style;
                style.fill = ig::Color(236u, 86u, 158u, 255u);
                const float y = adsrPanel.y + 28.0f;
                const float small = 44.0f;

                snprintf(text, sizeof(text), "%.0f ms",
                         static_cast<double>(envelope.attack * 1000.0f));
                synth::knob(ui, "env.a", "Attack", text, envelope.attack, 0.001f, 2.0f,
                            ig::Vec2(adsrPanel.x + 22.0f, y), small, style);

                snprintf(text, sizeof(text), "%.0f ms",
                         static_cast<double>(envelope.decay * 1000.0f));
                synth::knob(ui, "env.d", "Decay", text, envelope.decay, 0.001f, 2.0f,
                            ig::Vec2(adsrPanel.x + 112.0f, y), small, style);

                snprintf(text, sizeof(text), "%d%%",
                         static_cast<int>(envelope.sustain * 100.0f + 0.5f));
                synth::knob(ui, "env.s", "Sustain", text, envelope.sustain, 0.0f, 1.0f,
                            ig::Vec2(adsrPanel.x + 202.0f, y), small, style);

                snprintf(text, sizeof(text), "%.0f ms",
                         static_cast<double>(envelope.release * 1000.0f));
                synth::knob(ui, "env.r", "Release", text, envelope.release, 0.001f, 3.0f,
                            ig::Vec2(adsrPanel.x + 292.0f, y), small, style);
            }

            // ── Keyboard ────────────────────────────────────────────────
            const ig::Rect keyPanel(adsrPanel.x + adsrPanel.width + 12.0f, adsrY,
                                    684.0f, 112.0f);
            panel(ui, keyPanel, "Keyboard", ig::Color(78u, 214u, 150u, 255u));
            const int note = synth::keyboard(
                ui,
                ig::Rect(keyPanel.x + 10.0f, keyPanel.y + 28.0f,
                         keyPanel.width - 20.0f, 74.0f),
                48, 21, lastNote);
            if (note >= 0)
            {
                lastNote = note;
                noteAge = 0.0f;
            }

            ui.setCursor(ig::Vec2(origin.x, adsrY + 112.0f + 12.0f));
            if (lastNote >= 0)
            {
                static const char* kNames[12] = {"C", "C#", "D", "D#", "E", "F",
                                                 "F#", "G", "G#", "A", "A#", "B"};
                snprintf(text, sizeof(text), "%s%d", kNames[lastNote % 12],
                         lastNote / 12 - 1);
                char line[96];
                snprintf(line, sizeof(line), "Note %s   |   out %d%%", text,
                         static_cast<int>(meterLevel * 100.0f + 0.5f));
                ui.label(ig::StringView(line));
            }
            else
            {
                ui.label("Click a key to trigger the envelope.");
            }

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
