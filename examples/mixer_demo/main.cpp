// Mixer panel in the style of a web DAW: thin open arcs, the value large in
// the middle of the knob, a small label underneath.
//
// Everything here is application code. The knob is not an iGUI widget - it is
// drawn with the Context's public building blocks (drawLine, invisibleButton,
// pointerPosition) exactly as an application would draw its own controls. The
// point of the demo is that the look is the application's to choose, and that
// arcs now come out smooth rather than stair-stepped.
//
// Build: cmake -DIGUI_BUILD_RAYLIB_DEMO=ON -DIGUI_BUILD_MIXER_DEMO=ON
// Run:   ./igui_mixer_demo

#include <raylib.h>

#include <math.h>
#include <stdio.h>

#include <igui/Gui.hpp>
#include <igui_raylib/RaylibBackend.hpp>

namespace
{

const float kPi = 3.14159265358979323846f;
// The gap sits at the bottom, like a hardware pot: sweep 270 degrees starting
// from the lower left.
const float kStartAngle = 135.0f * kPi / 180.0f;
const float kSweepAngle = 270.0f * kPi / 180.0f;

struct KnobStyle
{
    ig::Color track;
    ig::Color fill;
    ig::Color value;
    ig::Color label;
    float thickness;

    KnobStyle()
        : track(44u, 50u, 62u, 255u), fill(78u, 201u, 255u, 255u),
          value(120u, 214u, 255u, 255u), label(150u, 158u, 172u, 255u),
          thickness(3.0f) {}
};

// Drag state lives with the application, not in the Context: one knob can be
// dragging at a time, identified by where its box is.
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
    // 48 segments over a full sweep keeps a 20px knob smooth; the soft edge in
    // DrawList::addLine is what removes the stepping between them.
    const int segments = 48;
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

// Returns true on the frames the value changed.
bool knob(ig::Context& ui, ig::StringView id, const char* label, float& value,
          float minimum, float maximum, const ig::Rect& box,
          const KnobStyle& style = KnobStyle())
{
    const float diameter = box.width;
    const float cx = box.x + diameter * 0.5f;
    const float cy = box.y + diameter * 0.5f;
    const float radius = diameter * 0.5f - style.thickness;
    const float span = (maximum > minimum) ? (maximum - minimum) : 1.0f;
    float norm = (value - minimum) / span;
    norm = norm < 0.0f ? 0.0f : (norm > 1.0f ? 1.0f : norm);

    // Rounded plate behind the knob, the way a web UI would sit it on a card.
    ui.drawRectFilledRounded(ig::Rect(box.x - 6.0f, box.y - 6.0f,
                                      box.width + 12.0f, box.height + 10.0f),
                             8.0f, ig::Color(22u, 27u, 40u, 255u));

    arc(ui, cx, cy, radius, 0.0f, 1.0f, style.track, style.thickness);
    arc(ui, cx, cy, radius, 0.0f, norm, style.fill, style.thickness);

    // Value centred inside the ring, label centred below it.
    char text[16];
    snprintf(text, sizeof(text), "%d%%", static_cast<int>(norm * 100.0f + 0.5f));
    const float valueWidth = ui.textWidth(ig::StringView(text));
    ui.drawText(ig::StringView(text),
                    ig::Vec2(cx - valueWidth * 0.5f, cy - 6.0f), style.value);
    const float labelWidth = ui.textWidth(ig::StringView(label));
    ui.drawText(ig::StringView(label),
                    ig::Vec2(cx - labelWidth * 0.5f, box.y + diameter + 2.0f),
                    style.label);

    // Vertical drag, the convention every DAW uses: up raises the value.
    const ig::Vec2 press = ui.pointerPressedPosition(ig::PointerButton::Left);
    const bool insideBox = press.x >= box.x && press.x <= box.x + box.width &&
                           press.y >= box.y && press.y <= box.y + box.height;
    const bool held = ui.isPointerButtonDown(ig::PointerButton::Left);
    bool changed = false;

    if (held && insideBox)
    {
        if (!gDrag.active || gDrag.originX != box.x || gDrag.originY != box.y)
        {
            gDrag.active = true;
            gDrag.originX = box.x;
            gDrag.originY = box.y;
            gDrag.valueAtPress = value;
        }
        // 160px of travel covers the whole range - fine enough to be precise
        // without needing a modifier key.
        const float dy = press.y - ui.pointerPosition().y;
        float next = gDrag.valueAtPress + (dy / 160.0f) * span;
        next = next < minimum ? minimum : (next > maximum ? maximum : next);
        if (next != value)
        {
            value = next;
            changed = true;
        }
    }
    else if (gDrag.active && gDrag.originX == box.x && gDrag.originY == box.y)
    {
        gDrag.active = false;
    }

    ui.invisibleButton(id, box);
    return changed;
}

struct Track
{
    const char* name;
    float volume;
    ig::Color accent;
};

} // namespace

int main()
{
    InitWindow(1100, 420, "iGUI mixer demo");
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
    float resonance = 0.5f;

    Track tracks[] = {
        {"Drums", 0.90f, ig::Color(64u, 208u, 150u, 255u)},
        {"Bass", 0.90f, ig::Color(226u, 178u, 64u, 255u)},
        {"Keys", 0.90f, ig::Color(226u, 96u, 160u, 255u)},
        {"FX", 0.90f, ig::Color(96u, 158u, 240u, 255u)},
    };
    const int trackCount = static_cast<int>(sizeof(tracks) / sizeof(tracks[0]));

    while (!WindowShouldClose())
    {
        ig::raylib::processInput(ui);
        ui.beginFrame(ig::raylib::frameInfo(GetFrameTime()));

        if (ui.beginMainWindow("Tracks, mixer and effects"))
        {
            ui.label("Drag a knob up or down.");
            ui.separator();

            const ig::Vec2 origin = ui.cursor();
            const float knobSize = 48.0f;
            const float rowY = origin.y + 16.0f;

            // Master strip.
            KnobStyle masterStyle;
            knob(ui, "master.vol", "Volume", masterVolume, 0.0f, 1.0f,
                 ig::Rect(origin.x + 16.0f, rowY, knobSize, knobSize + 16.0f), masterStyle);
            knob(ui, "master.filter", "DJ filter", djFilter, 0.0f, 1.0f,
                 ig::Rect(origin.x + 84.0f, rowY, knobSize, knobSize + 16.0f), masterStyle);
            knob(ui, "master.res", "Res", resonance, 0.0f, 1.0f,
                 ig::Rect(origin.x + 152.0f, rowY, knobSize, knobSize + 16.0f), masterStyle);

            // One strip per track, each tinted with the track's own colour.
            for (int i = 0; i < trackCount; ++i)
            {
                KnobStyle style;
                style.fill = tracks[i].accent;
                style.value = tracks[i].accent;
                char id[32];
                snprintf(id, sizeof(id), "track.%d.vol", i);
                knob(ui, ig::StringView(id), tracks[i].name, tracks[i].volume,
                     0.0f, 1.0f,
                     ig::Rect(origin.x + 260.0f + static_cast<float>(i) * 76.0f, rowY,
                              knobSize, knobSize + 16.0f),
                     style);
            }

            ui.setCursor(ig::Vec2(origin.x, rowY + knobSize + 48.0f));
            ui.separator();

            char info[160];
            snprintf(info, sizeof(info),
                     "master %d%%  filter %d%%  res %d%%   |   drums %d%%  bass %d%%  keys %d%%  fx %d%%",
                     static_cast<int>(masterVolume * 100.0f + 0.5f),
                     static_cast<int>(djFilter * 100.0f + 0.5f),
                     static_cast<int>(resonance * 100.0f + 0.5f),
                     static_cast<int>(tracks[0].volume * 100.0f + 0.5f),
                     static_cast<int>(tracks[1].volume * 100.0f + 0.5f),
                     static_cast<int>(tracks[2].volume * 100.0f + 0.5f),
                     static_cast<int>(tracks[3].volume * 100.0f + 0.5f));
            ui.label(info);

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
