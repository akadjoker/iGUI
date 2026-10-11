#pragma once
// Step-sequencer grid, in the style of a web DAW's pattern editor.
//
// Like the knob in main.cpp this is application code, not an iGUI widget: it is
// built from the Context's public building blocks (drawRectFilledRounded,
// isClicked, isHovered) so the look belongs to the application. The retained
// PianoRoll models notes as pitch/start/length, which is a different instrument
// altogether - a grid of per-step cells is not expressible in it.

#include <igui/Gui.hpp>

namespace daw
{

// A cell is off, soft (ghost note, played quieter) or on. Tapping cycles
// on -> soft -> off, matching the three shades in the reference UI.
enum class Step : unsigned char
{
    Off = 0,
    Soft = 1,
    On = 2
};

struct StepGridStyle
{
    ig::Color cellOff;
    ig::Color cellHover;
    ig::Color gridLine;      // every beat boundary
    ig::Color barLine;       // every bar boundary, stronger
    float cellWidth;
    float cellHeight;
    float gap;
    float radius;
    int stepsPerBeat;
    int beatsPerBar;

    StepGridStyle()
        : cellOff(28u, 34u, 50u, 255u), cellHover(44u, 54u, 76u, 255u),
          gridLine(38u, 46u, 66u, 255u), barLine(70u, 84u, 116u, 255u),
          cellWidth(30.0f), cellHeight(30.0f), gap(4.0f), radius(5.0f),
          stepsPerBeat(4), beatsPerBar(4) {}
};

// One row of the pattern: a sound and its steps.
struct StepRow
{
    const char* name;
    ig::Color accent;
    Step* steps;       // caller-owned, at least stepCount entries
    int stepCount;
};

// Draws one row and handles its clicks. Returns true when a step changed.
// `firstVisibleStep` lets the caller scroll a long pattern horizontally.
inline bool stepRow(ig::Context& ui, ig::StringView idPrefix, const StepRow& row,
                    const ig::Rect& bounds, int firstVisibleStep,
                    const StepGridStyle& style = StepGridStyle())
{
    const float pitch = style.cellWidth + style.gap;
    const int visible = static_cast<int>(bounds.width / pitch) + 1;
    bool changed = false;

    for (int column = 0; column < visible; ++column)
    {
        const int step = firstVisibleStep + column;
        if (step < 0 || step >= row.stepCount)
            continue;

        const ig::Rect cell(bounds.x + static_cast<float>(column) * pitch, bounds.y,
                            style.cellWidth, style.cellHeight);
        if (cell.x + cell.width > bounds.x + bounds.width)
            break;

        char id[64];
        snprintf(id, sizeof(id), "%.*s.%d", static_cast<int>(idPrefix.size()),
                 idPrefix.data(), step);
        const ig::StringView cellId(id);

        // A soft step keeps the row's colour at reduced alpha, so a pattern
        // reads as one instrument at a glance rather than as two colours.
        ig::Color fill = style.cellOff;
        switch (row.steps[step])
        {
        case Step::On:
            fill = row.accent;
            break;
        case Step::Soft:
            fill = ig::Color(row.accent.r, row.accent.g, row.accent.b, 110u);
            break;
        case Step::Off:
            fill = ui.isHovered(cellId, cell) ? style.cellHover : style.cellOff;
            break;
        }

        ui.drawRectFilledRounded(cell, style.radius, fill);

        // The first step of each bar gets a top edge, the way a pattern editor
        // marks its bar lines without drawing a full grid.
        if (step % (style.stepsPerBeat * style.beatsPerBar) == 0)
        {
            ui.drawRectFilledRounded(ig::Rect(cell.x, cell.y - 3.0f, cell.width, 2.0f),
                                     1.0f, style.barLine);
        }
        else if (step % style.stepsPerBeat == 0)
        {
            ui.drawRectFilledRounded(ig::Rect(cell.x, cell.y - 3.0f, cell.width, 1.0f),
                                     0.5f, style.gridLine);
        }

        if (ui.isClicked(cellId, cell))
        {
            // on -> soft -> off -> on
            switch (row.steps[step])
            {
            case Step::On:   row.steps[step] = Step::Soft; break;
            case Step::Soft: row.steps[step] = Step::Off;  break;
            case Step::Off:  row.steps[step] = Step::On;   break;
            }
            changed = true;
        }
    }
    return changed;
}

// Playhead: a vertical line over the grid at the currently playing step.
inline void stepPlayhead(ig::Context& ui, const ig::Rect& gridBounds, int step,
                         int firstVisibleStep, const ig::Color& color,
                         const StepGridStyle& style = StepGridStyle())
{
    const float pitch = style.cellWidth + style.gap;
    const float x = gridBounds.x + static_cast<float>(step - firstVisibleStep) * pitch;
    if (x < gridBounds.x || x > gridBounds.x + gridBounds.width)
        return;
    ui.drawRectFilledRounded(
        ig::Rect(x - 1.0f, gridBounds.y - 4.0f, 2.0f, gridBounds.height + 8.0f),
        1.0f, color);
}

} // namespace daw
