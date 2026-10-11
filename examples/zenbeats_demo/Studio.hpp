#pragma once
// Zen Beats studio view, rebuilt in C++ as a portability test: transport bar,
// arrangement with a bar ruler and clips, mixer cards, and the pattern grid.
//
// The question this answers is not "can iGUI draw this" but "does the C++
// version cost more code than the HTML/JS one, and does it look as good".
// Everything is application code over the Context's public drawing and
// interaction primitives.

#include <igui/Gui.hpp>

#include <stdio.h>

namespace zen
{

// ── Model ───────────────────────────────────────────────────────────────────

enum class Cell : unsigned char
{
    Off = 0,
    Soft = 1,
    On = 2
};

struct SoundRow
{
    const char* name;
    ig::Color accent;
    Cell* steps;
    int stepCount;
    bool muted;
    bool soloed;
};

struct ArrangeClip
{
    const char* label;
    int startBar;
    int lengthBars;
    int track;
};

struct ArrangeTrack
{
    const char* name;
    ig::Color accent;
    bool muted;
    bool soloed;
};

struct MixerStrip
{
    const char* name;
    float volume;
    int effect;
    ig::Color accent;
};

struct Transport
{
    bool playing;
    bool recording;
    bool loopMode;      // false = composition
    int bpm;
    int swing;          // percent
    int bar;
    int beat;
    int tick;
};

// ── Shared palette ──────────────────────────────────────────────────────────

struct Palette
{
    ig::Color panel;
    ig::Color panelEdge;
    ig::Color sunken;
    ig::Color text;
    ig::Color dim;
    ig::Color accent;
    ig::Color gridLine;
    ig::Color barLine;

    Palette()
        : panel(21u, 26u, 38u, 255u), panelEdge(44u, 52u, 70u, 255u),
          sunken(14u, 18u, 28u, 255u), text(226u, 232u, 244u, 255u),
          dim(126u, 136u, 156u, 255u), accent(64u, 224u, 208u, 255u),
          gridLine(34u, 41u, 56u, 255u), barLine(58u, 68u, 90u, 255u) {}
};

// ── Small shared pieces ─────────────────────────────────────────────────────

// Section panel with a small uppercase caption, as in the reference.
inline void section(ig::Context& ui, const ig::Rect& bounds, const char* caption,
                    const char* hint, const Palette& palette)
{
    ui.drawRectFilledRounded(bounds, 8.0f, palette.panel);
    ui.drawRectRounded(bounds, 8.0f, palette.panelEdge, 1.0f);
    ui.drawText(ig::StringView(caption), ig::Vec2(bounds.x + 14.0f, bounds.y + 10.0f),
                palette.dim);
    if (hint)
    {
        const float captionWidth = ui.textWidth(ig::StringView(caption));
        ui.drawText(ig::StringView(hint),
                    ig::Vec2(bounds.x + 24.0f + captionWidth, bounds.y + 10.0f),
                    palette.dim);
    }
}

// Pill button. `filled` draws it in the accent colour, as the active tab.
inline bool pill(ig::Context& ui, ig::StringView id, const char* label,
                 const ig::Rect& bounds, bool filled, const ig::Color& accent,
                 const Palette& palette)
{
    const bool hot = ui.isHovered(id, bounds);
    ig::Color background = filled ? accent
                                  : (hot ? ig::Color(38u, 46u, 62u, 255u)
                                         : ig::Color(28u, 34u, 48u, 255u));
    ui.drawRectFilledRounded(bounds, 5.0f, background);
    if (!filled)
        ui.drawRectRounded(bounds, 5.0f, palette.panelEdge, 1.0f);
    const float width = ui.textWidth(ig::StringView(label));
    ui.drawText(ig::StringView(label),
                ig::Vec2(bounds.x + (bounds.width - width) * 0.5f, bounds.y + 5.0f),
                filled ? ig::Color(12u, 20u, 28u, 255u) : palette.text);
    return ui.isClicked(id, bounds);
}

// The tiny square M / S / play / link buttons beside each row.
inline bool miniButton(ig::Context& ui, ig::StringView id, const char* glyph,
                       const ig::Rect& bounds, bool active,
                       const ig::Color& activeColor, const Palette& palette)
{
    const bool hot = ui.isHovered(id, bounds);
    const ig::Color background =
        active ? activeColor
               : (hot ? ig::Color(40u, 48u, 64u, 255u) : ig::Color(26u, 32u, 45u, 255u));
    ui.drawRectFilledRounded(bounds, 3.0f, background);
    ui.drawRectRounded(bounds, 3.0f, palette.panelEdge, 1.0f);
    const float width = ui.textWidth(ig::StringView(glyph));
    ui.drawText(ig::StringView(glyph),
                ig::Vec2(bounds.x + (bounds.width - width) * 0.5f, bounds.y + 2.0f),
                active ? ig::Color(12u, 18u, 26u, 255u) : palette.dim);
    return ui.isClicked(id, bounds);
}

// ── Transport bar ───────────────────────────────────────────────────────────

inline void transportBar(ig::Context& ui, const ig::Rect& bounds,
                         Transport& transport, const Palette& palette)
{
    ui.drawText(ig::StringView("ZEN BEATS"), ig::Vec2(bounds.x + 4.0f, bounds.y + 6.0f),
                palette.accent);
    ui.drawText(ig::StringView("Audio off: press play"),
                ig::Vec2(bounds.x + 4.0f, bounds.y + 24.0f), palette.dim);

    float x = bounds.x + 150.0f;
    const float y = bounds.y + 8.0f;

    // Loop / Composition selector.
    ui.drawRectFilledRounded(ig::Rect(x, y, 152.0f, 26.0f), 6.0f,
                             ig::Color(26u, 32u, 46u, 255u));
    if (pill(ui, "tp.loop", "Loop", ig::Rect(x + 3.0f, y + 3.0f, 60.0f, 20.0f),
             transport.loopMode, palette.accent, palette))
        transport.loopMode = true;
    if (pill(ui, "tp.comp", "Composition",
             ig::Rect(x + 67.0f, y + 3.0f, 82.0f, 20.0f), !transport.loopMode,
             palette.accent, palette))
        transport.loopMode = false;
    x += 164.0f;

    // Play / stop / record.
    const ig::Rect playRect(x, y, 38.0f, 26.0f);
    ui.drawRectFilledRounded(playRect, 6.0f,
                             transport.playing ? ig::Color(62u, 200u, 120u, 255u)
                                               : ig::Color(50u, 168u, 104u, 255u));
    // Triangle from three thick lines - addPolygonFilled has no anti-aliasing
    // yet, and at this size the difference shows.
    for (int i = 0; i < 9; ++i)
    {
        const float t = static_cast<float>(i) / 8.0f;
        const float halfHeight = 6.0f * (1.0f - t);
        const float px = playRect.x + 15.0f + t * 9.0f;
        ui.drawLine(ig::Vec2(px, playRect.y + 13.0f - halfHeight),
                    ig::Vec2(px, playRect.y + 13.0f + halfHeight),
                    ig::Color(10u, 24u, 16u, 255u), 1.4f);
    }
    if (ui.isClicked("tp.play", playRect))
        transport.playing = !transport.playing;
    x += 46.0f;

    const ig::Rect stopRect(x, y, 34.0f, 26.0f);
    ui.drawRectFilledRounded(stopRect, 6.0f, ig::Color(32u, 39u, 54u, 255u));
    ui.drawRectFilledRounded(ig::Rect(stopRect.x + 12.0f, stopRect.y + 9.0f, 9.0f, 9.0f),
                             1.5f, palette.text);
    if (ui.isClicked("tp.stop", stopRect))
        transport.playing = false;
    x += 42.0f;

    const ig::Rect recRect(x, y, 66.0f, 26.0f);
    ui.drawRectFilledRounded(recRect, 6.0f, ig::Color(32u, 39u, 54u, 255u));
    ui.drawRectRounded(recRect, 6.0f,
                       transport.recording ? ig::Color(236u, 86u, 86u, 255u)
                                           : palette.panelEdge,
                       1.0f);
    ui.drawCircleFilled(ig::Vec2(recRect.x + 14.0f, recRect.y + 13.0f), 4.5f,
                        ig::Color(236u, 86u, 86u, 255u));
    ui.drawText(ig::StringView("REC"), ig::Vec2(recRect.x + 24.0f, recRect.y + 5.0f),
                palette.text);
    if (ui.isClicked("tp.rec", recRect))
        transport.recording = !transport.recording;
    x += 78.0f;

    // BPM field.
    ui.drawText(ig::StringView("BPM"), ig::Vec2(x, y + 6.0f), palette.dim);
    char text[32];
    snprintf(text, sizeof(text), "%d", transport.bpm);
    const ig::Rect bpmRect(x + 32.0f, y, 52.0f, 26.0f);
    ui.drawRectFilledRounded(bpmRect, 5.0f, palette.sunken);
    ui.drawRectRounded(bpmRect, 5.0f, palette.panelEdge, 1.0f);
    ui.drawText(ig::StringView(text), ig::Vec2(bpmRect.x + 10.0f, bpmRect.y + 5.0f),
                palette.text);
    // Drag the field up or down to change the tempo.
    if (ui.isPointerButtonDown(ig::PointerButton::Left))
    {
        const ig::Vec2 press = ui.pointerPressedPosition(ig::PointerButton::Left);
        if (press.x >= bpmRect.x && press.x <= bpmRect.x + bpmRect.width &&
            press.y >= bpmRect.y && press.y <= bpmRect.y + bpmRect.height)
        {
            const float dy = press.y - ui.pointerPosition().y;
            int next = transport.bpm + static_cast<int>(dy * 0.25f);
            next = next < 40 ? 40 : (next > 240 ? 240 : next);
            transport.bpm = next;
        }
    }
    x += 92.0f;

    pill(ui, "tp.tap", "TAP", ig::Rect(x, y, 44.0f, 26.0f), false, palette.accent,
         palette);
    x += 56.0f;

    // Swing slider.
    ui.drawText(ig::StringView("SWING"), ig::Vec2(x, y + 6.0f), palette.dim);
    const ig::Rect swingTrack(x + 48.0f, y + 11.0f, 96.0f, 4.0f);
    ui.drawRectFilledRounded(swingTrack, 2.0f, ig::Color(40u, 48u, 64u, 255u));
    const float swingFraction = static_cast<float>(transport.swing) / 100.0f;
    ui.drawRectFilledRounded(
        ig::Rect(swingTrack.x, swingTrack.y, swingTrack.width * swingFraction, 4.0f),
        2.0f, palette.accent);
    ui.drawCircleFilled(
        ig::Vec2(swingTrack.x + swingTrack.width * swingFraction, swingTrack.y + 2.0f),
        6.0f, palette.accent);
    if (ui.isPointerButtonDown(ig::PointerButton::Left))
    {
        const ig::Vec2 press = ui.pointerPressedPosition(ig::PointerButton::Left);
        const ig::Rect grab(swingTrack.x - 8.0f, swingTrack.y - 10.0f,
                            swingTrack.width + 16.0f, 24.0f);
        if (press.x >= grab.x && press.x <= grab.x + grab.width &&
            press.y >= grab.y && press.y <= grab.y + grab.height)
        {
            const float t = (ui.pointerPosition().x - swingTrack.x) / swingTrack.width;
            int next = static_cast<int>((t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t)) * 100.0f);
            transport.swing = next;
        }
    }
    snprintf(text, sizeof(text), "%d%%", transport.swing);
    ui.drawText(ig::StringView(text), ig::Vec2(swingTrack.x + 106.0f, y + 6.0f),
                palette.text);
    x += 200.0f;

    // Position readout, in the monospaced style of the reference.
    const ig::Rect positionRect(x, y, 128.0f, 26.0f);
    ui.drawRectFilledRounded(positionRect, 5.0f, palette.sunken);
    ui.drawRectRounded(positionRect, 5.0f, palette.panelEdge, 1.0f);
    snprintf(text, sizeof(text), "%03d : %d : %d", transport.bar, transport.beat,
             transport.tick);
    const float positionWidth = ui.textWidth(ig::StringView(text));
    ui.drawText(ig::StringView(text),
                ig::Vec2(positionRect.x + (positionRect.width - positionWidth) * 0.5f,
                         positionRect.y + 5.0f),
                palette.accent);
    x += 140.0f;

    pill(ui, "tp.undo", "Undo", ig::Rect(x, y, 62.0f, 26.0f), false, palette.accent,
         palette);
    pill(ui, "tp.new", "New project", ig::Rect(x + 70.0f, y, 96.0f, 26.0f), false,
         palette.accent, palette);
    pill(ui, "tp.ai", "AI", ig::Rect(x + 174.0f, y, 36.0f, 26.0f), false,
         palette.accent, palette);
    pill(ui, "tp.project", "Project", ig::Rect(x + 218.0f, y, 74.0f, 26.0f), false,
         palette.accent, palette);
}

// ── Arrangement ─────────────────────────────────────────────────────────────

inline void arrangement(ig::Context& ui, const ig::Rect& bounds,
                        ArrangeTrack* tracks, int trackCount, ArrangeClip* clips,
                        int clipCount, int& selectedTrack, int totalBars,
                        const Palette& palette)
{
    const float headerWidth = 168.0f;
    const float rulerHeight = 24.0f;
    const float rowHeight = 56.0f;
    const float laneX = bounds.x + headerWidth;
    const float laneWidth = bounds.width - headerWidth;
    const float barWidth = laneWidth / static_cast<float>(totalBars);

    // Column captions.
    ui.drawText(ig::StringView("TRACKS"), ig::Vec2(bounds.x + 12.0f, bounds.y + 6.0f),
                palette.dim);

    // Bar ruler.
    char text[16];
    for (int bar = 0; bar < totalBars; ++bar)
    {
        const float x = laneX + static_cast<float>(bar) * barWidth;
        ui.drawLine(ig::Vec2(x, bounds.y), ig::Vec2(x, bounds.y + bounds.height),
                    (bar % 4 == 0) ? palette.barLine : palette.gridLine, 1.0f);
        snprintf(text, sizeof(text), "%d", bar + 1);
        ui.drawText(ig::StringView(text), ig::Vec2(x + 5.0f, bounds.y + 5.0f),
                    (bar == 0) ? palette.text : palette.dim);
    }

    // Track rows.
    for (int i = 0; i < trackCount; ++i)
    {
        const float y = bounds.y + rulerHeight + static_cast<float>(i) * rowHeight;
        const ig::Rect header(bounds.x, y, headerWidth - 4.0f, rowHeight - 2.0f);
        const bool selected = (i == selectedTrack);

        ui.drawRectFilledRounded(header, 5.0f,
                                 selected ? ig::Color(28u, 36u, 52u, 255u)
                                          : ig::Color(20u, 25u, 36u, 255u));
        if (selected)
        {
            // Accent bar down the left edge marks the selected track.
            ui.drawRectFilledRounded(ig::Rect(header.x, header.y, 3.0f, header.height),
                                     1.5f, tracks[i].accent);
        }
        ui.drawCircleFilled(ig::Vec2(header.x + 16.0f, header.y + 13.0f), 4.0f,
                            tracks[i].accent);
        ui.drawText(ig::StringView(tracks[i].name),
                    ig::Vec2(header.x + 26.0f, header.y + 7.0f), palette.text);

        char id[48];
        snprintf(id, sizeof(id), "arr.m.%d", i);
        if (miniButton(ui, ig::StringView(id), "M",
                       ig::Rect(header.x + 12.0f, header.y + 28.0f, 20.0f, 18.0f),
                       tracks[i].muted, ig::Color(232u, 188u, 64u, 255u), palette))
            tracks[i].muted = !tracks[i].muted;
        snprintf(id, sizeof(id), "arr.s.%d", i);
        if (miniButton(ui, ig::StringView(id), "S",
                       ig::Rect(header.x + 36.0f, header.y + 28.0f, 20.0f, 18.0f),
                       tracks[i].soloed, ig::Color(96u, 212u, 255u, 255u), palette))
            tracks[i].soloed = !tracks[i].soloed;
        snprintf(id, sizeof(id), "arr.x.%d", i);
        miniButton(ui, ig::StringView(id), "x",
                   ig::Rect(header.x + header.width - 26.0f, header.y + 6.0f, 18.0f,
                            16.0f),
                   false, palette.accent, palette);

        snprintf(id, sizeof(id), "arr.row.%d", i);
        if (ui.isClicked(ig::StringView(id), header))
            selectedTrack = i;

        // Lane background.
        ui.drawLine(ig::Vec2(laneX, y + rowHeight - 1.0f),
                    ig::Vec2(bounds.x + bounds.width, y + rowHeight - 1.0f),
                    palette.gridLine, 1.0f);
    }

    // Clips.
    for (int c = 0; c < clipCount; ++c)
    {
        const ArrangeClip& clip = clips[c];
        if (clip.track < 0 || clip.track >= trackCount)
            continue;
        const float y = bounds.y + rulerHeight + static_cast<float>(clip.track) * rowHeight;
        const ig::Rect body(laneX + static_cast<float>(clip.startBar) * barWidth + 1.0f,
                            y + 2.0f,
                            static_cast<float>(clip.lengthBars) * barWidth - 2.0f,
                            rowHeight - 6.0f);
        ui.drawRectFilledRounded(body, 5.0f, tracks[clip.track].accent);
        ui.drawText(ig::StringView(clip.label), ig::Vec2(body.x + 8.0f, body.y + 5.0f),
                    ig::Color(10u, 26u, 22u, 255u));
        snprintf(text, sizeof(text), "%d bars", clip.lengthBars);
        ui.drawText(ig::StringView(text), ig::Vec2(body.x + 8.0f, body.y + 21.0f),
                    ig::Color(16u, 46u, 40u, 255u));
    }
}

// ── Pattern grid ────────────────────────────────────────────────────────────

// Only the steps between firstStep and firstStep+visibleSteps are drawn. That
// window is what keeps a long song affordable: a five-minute pattern is around
// 2400 steps per sound, and emitting geometry for all of them costs roughly
// 90 ms a frame - about 11 FPS. Drawing only what fits on screen brings the
// same pattern down to about 1 ms, because the cost follows the window rather
// than the song. The DrawList has no culling of its own; the clip rectangle it
// carries is a scissor for the GPU, not a reason to skip building vertices.
inline bool patternGrid(ig::Context& ui, const ig::Rect& bounds, SoundRow* rows,
                        int rowCount, int firstStep, int visibleSteps,
                        int playheadStep, int& selectedRow, const Palette& palette)
{
    const float headerWidth = 200.0f;
    const float rulerHeight = 20.0f;
    const float rowHeight = 35.0f;
    const float cellGap = 4.0f;
    const float laneX = bounds.x + headerWidth;
    const float cellPitch =
        (bounds.width - headerWidth) / static_cast<float>(visibleSteps);
    const float cellWidth = cellPitch - cellGap;
    bool changed = false;

    // Step ruler: every fourth step numbered, bar starts in the accent colour.
    char text[16];
    for (int i = 0; i < visibleSteps; ++i)
    {
        const int step = firstStep + i;
        if (step % 4 != 0)
            continue;
        snprintf(text, sizeof(text), "%d", step + 1);
        ui.drawText(ig::StringView(text),
                    ig::Vec2(laneX + static_cast<float>(i) * cellPitch + 2.0f,
                             bounds.y + 2.0f),
                    (step % 16 == 0) ? palette.accent : palette.dim);
    }

    for (int r = 0; r < rowCount; ++r)
    {
        const float y = bounds.y + rulerHeight + static_cast<float>(r) * rowHeight;
        SoundRow& row = rows[r];
        const bool selected = (r == selectedRow);

        // Row header: name chip plus the four mini buttons.
        const ig::Rect chip(bounds.x, y + 2.0f, 104.0f, 24.0f);
        ui.drawRectFilledRounded(chip, 5.0f,
                                 selected ? ig::Color(32u, 40u, 58u, 255u)
                                          : ig::Color(20u, 25u, 36u, 255u));
        if (selected)
            ui.drawRectRounded(chip, 5.0f, ig::Color(232u, 188u, 64u, 255u), 1.0f);
        ui.drawCircleFilled(ig::Vec2(chip.x + 12.0f, chip.y + 12.0f), 4.0f, row.accent);
        ui.drawText(ig::StringView(row.name), ig::Vec2(chip.x + 22.0f, chip.y + 5.0f),
                    palette.text);

        char id[48];
        snprintf(id, sizeof(id), "pg.sel.%d", r);
        if (ui.isClicked(ig::StringView(id), chip))
            selectedRow = r;

        const float buttonY = y + 4.0f;
        snprintf(id, sizeof(id), "pg.m.%d", r);
        if (miniButton(ui, ig::StringView(id), "M",
                       ig::Rect(chip.x + 110.0f, buttonY, 20.0f, 20.0f), row.muted,
                       ig::Color(232u, 188u, 64u, 255u), palette))
            row.muted = !row.muted;
        snprintf(id, sizeof(id), "pg.s.%d", r);
        if (miniButton(ui, ig::StringView(id), "S",
                       ig::Rect(chip.x + 134.0f, buttonY, 20.0f, 20.0f), row.soloed,
                       ig::Color(96u, 212u, 255u, 255u), palette))
            row.soloed = !row.soloed;
        snprintf(id, sizeof(id), "pg.p.%d", r);
        miniButton(ui, ig::StringView(id), ">",
                   ig::Rect(chip.x + 158.0f, buttonY, 20.0f, 20.0f), false,
                   palette.accent, palette);
        snprintf(id, sizeof(id), "pg.l.%d", r);
        miniButton(ui, ig::StringView(id), "=",
                   ig::Rect(chip.x + 182.0f, buttonY, 20.0f, 20.0f), false,
                   palette.accent, palette);

        // Cells.
        for (int i = 0; i < visibleSteps; ++i)
        {
            const int step = firstStep + i;
            if (step >= row.stepCount)
                break;
            const ig::Rect cell(laneX + static_cast<float>(i) * cellPitch, y + 3.0f,
                                cellWidth, rowHeight - 9.0f);
            snprintf(id, sizeof(id), "pg.c.%d.%d", r, step);
            const ig::StringView cellId(id);

            ig::Color fill(26u, 32u, 46u, 255u);
            switch (row.steps[step])
            {
            case Cell::On:
                fill = row.accent;
                break;
            case Cell::Soft:
                fill = ig::Color(row.accent.r, row.accent.g, row.accent.b, 105u);
                break;
            case Cell::Off:
                if (ui.isHovered(cellId, cell))
                    fill = ig::Color(40u, 48u, 66u, 255u);
                break;
            }
            ui.drawRectFilledRounded(cell, 4.0f, fill);
            // Bar starts get a brighter edge, as in the reference grid.
            if (step % 16 == 0)
                ui.drawRectRounded(cell, 4.0f, palette.barLine, 1.0f);

            if (ui.isClicked(cellId, cell))
            {
                switch (row.steps[step])
                {
                case Cell::On:   row.steps[step] = Cell::Soft; break;
                case Cell::Soft: row.steps[step] = Cell::Off;  break;
                case Cell::Off:  row.steps[step] = Cell::On;   break;
                }
                changed = true;
            }
        }
    }

    // Playhead over the whole grid.
    const int relative = playheadStep - firstStep;
    if (relative >= 0 && relative < visibleSteps)
    {
        const float x = laneX + static_cast<float>(relative) * cellPitch - 1.0f;
        ui.drawRectFilledRounded(
            ig::Rect(x, bounds.y + rulerHeight - 2.0f, 2.0f,
                     static_cast<float>(rowCount) * rowHeight + 2.0f),
            1.0f, ig::Color(255u, 255u, 255u, 80u));
    }
    return changed;
}

} // namespace zen
