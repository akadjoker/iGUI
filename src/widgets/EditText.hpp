// The text of a field being edited in a grid (a cell of a DataGrid or a TreeGrid, the value of a row of a
// PropertyGrid): cut at the field, moved to the left only as far as the cursor needs. Private to the
// widgets library.
#pragma once

#include "Widget.hpp"

namespace ig
{
namespace retained
{

// Space between the frame of a field being edited and its text, left and right.
inline constexpr float kEditPad = 3.0f;

// The text of a cell being edited, in the field that starts at fieldX and is fieldW wide (the frame
// is drawn already): all of it, never elided, and cut at the field. `scroll` is how far the text is
// moved to the left, kept between frames: it changes only as far as the cursor needs to stay in sight,
// and goes back as the text gets shorter. `cursor` is a byte offset into `text`.
inline void paintEditText(PaintContext& ctx, const String& text, int cursor, float& scroll, float fieldX, float fieldW,
        float rowY, float rowH, float textY, float ascender)
{
    const float left = fieldX + kEditPad;
    const float room = fieldW - 2.0f * kEditPad;
    if (room <= 0.0f) return;
    if (cursor < 0) cursor = 0;
    if (cursor > static_cast<int>(text.size())) cursor = static_cast<int>(text.size());
    const float textW = ctx.font.GetTextWidth(text.c_str());
    const float cursorW = cursor == static_cast<int>(text.size()) ? textW : ctx.font.GetTextWidth(text.substr(0, cursor).c_str());
    // the cursor is a line a pixel wide: it stays inside [0, room - 1] from the left of the text area
    if (cursorW - scroll > room - 1.0f) scroll = cursorW - (room - 1.0f);
    if (cursorW - scroll < 0.0f) scroll = cursorW;
    const float most = textW + 1.0f - room; // no more than the end of the text needs
    if (scroll > most) scroll = most;
    if (scroll < 0.0f) scroll = 0.0f;
    if (!text.empty()) ctx.printWithin({ left, rowY, room, rowH }, text, left - scroll, textW, textY, ascender);
    const float cursorX = left + cursorW - scroll;
    ctx.line.SetColor(255, 255, 255, 200);
    ctx.drawLine(cursorX, rowY + 3, cursorX, rowY + rowH - 3);
}

} // namespace retained
} // namespace ig
