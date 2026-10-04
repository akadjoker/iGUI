#include <SDL.h>

#include <igui/Gui.hpp>
#include <igui/CodeEditor.hpp>
#include <igui_sdl2/SdlBackend.hpp>

#include "SynthWidgets.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
const ig::StringView kSections[] = {
    ig::StringView("Basic"), ig::StringView("Inputs"), ig::StringView("Lists"),
    ig::StringView("Layout"), ig::StringView("Windows"), ig::StringView("Menus"),
    ig::StringView("Icons"), ig::StringView("Synth"), ig::StringView("Gizmos"),
    ig::StringView("Code")
};
const ig::StringView kVoicePages[] = {
    ig::StringView("Oscillator"), ig::StringView("Filter"),
    ig::StringView("Envelope"), ig::StringView("FX")
};

const int kIconCount = 6;
const char *const kIconNames[kIconCount] = {
    "icon play", "icon pause", "icon stop", "icon plus", "icon minus", "icon record"
};

// The accent presets the synth section offers, so a voice can pick its colour.
const int kAccentCount = 6;
const char *const kAccentNames[kAccentCount] = {
    "cyan", "teal", "amber", "orange", "violet", "green"
};
const ig::Color kAccents[kAccentCount] = {
    ig::Color(96u, 212u, 255u, 255u), ig::Color(80u, 210u, 180u, 255u),
    ig::Color(240u, 200u, 90u, 255u), ig::Color(245u, 150u, 60u, 255u),
    ig::Color(170u, 130u, 240u, 255u), ig::Color(120u, 230u, 130u, 255u)
};
const ig::StringView kQuality[] = {
    ig::StringView("low"), ig::StringView("medium"), ig::StringView("high")
};
// The mixer strip of the synth section: four voices on vertical faders and the
// master across the bottom, like a desk.
const int kChannelCount = 4;
const char *const kChannelNames[kChannelCount] = {"Kick", "Bass", "Lead", "Hats"};
const ig::StringView kFruit[] = {
    ig::StringView("apple"), ig::StringView("banana"), ig::StringView("cherry"),
    ig::StringView("grape"), ig::StringView("lemon"), ig::StringView("mango"),
    ig::StringView("orange"), ig::StringView("peach")
};

const int kWeight[] = {182, 118, 8, 5, 58, 207, 131, 150};

const char *kSample =
    "#include <cstdio>\n"
    "\n"
    "// iGUI immediate mode code editor\n"
    "int main()\n"
    "{\n"
    "    for (int i = 0; i < 3; ++i)\n"
    "    {\n"
    "        std::printf(\"hello %d\\n\", i);\n"
    "    }\n"
    "    return 0;\n"
    "}\n";

/* A camera at `eye` looking at the origin, in the column-major order the 3D
   gizmo's matrices use. The axis handles are measured from the camera distance,
   so the eye has to be away from the target or they collapse to nothing. */
void lookAtOrigin(float *out, float ex, float ey, float ez)
{
    const float length = std::sqrt(ex * ex + ey * ey + ez * ez);
    const float forward[3] = {ex / length, ey / length, ez / length};
    float right[3] = {forward[2], 0.0f, -forward[0]};
    const float rightLength = std::sqrt(right[0] * right[0] + right[2] * right[2]);
    right[0] /= rightLength;
    right[2] /= rightLength;
    const float up[3] = {forward[1] * right[2] - forward[2] * right[1],
                         forward[2] * right[0] - forward[0] * right[2],
                         forward[0] * right[1] - forward[1] * right[0]};
    out[0] = right[0];  out[4] = right[1];  out[8]  = right[2];
    out[12] = -(right[0] * ex + right[1] * ey + right[2] * ez);
    out[1] = up[0];     out[5] = up[1];     out[9]  = up[2];
    out[13] = -(up[0] * ex + up[1] * ey + up[2] * ez);
    out[2] = forward[0]; out[6] = forward[1]; out[10] = forward[2];
    out[14] = -(forward[0] * ex + forward[1] * ey + forward[2] * ez);
    out[3] = 0.0f;      out[7] = 0.0f;      out[11] = 0.0f;
    out[15] = 1.0f;
}

ig::String numbered(const char *prefix, int value, const char *suffix = "")
{
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%s%d%s", prefix, value, suffix);
    return ig::String(buffer);
}

/* A white shape on transparent pixels, tinted by the image button. */
bool iconShape(int kind, int x, int y, int size)
{
    const float fx = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
    const float fy = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
    switch (kind)
    {
        case 0: /* play */
            return fx >= 0.30f && fx <= 0.74f && std::fabs(fy - 0.5f) <= (0.74f - fx) * 0.72f;
        case 1: /* pause */
            return ((fx >= 0.30f && fx <= 0.43f) || (fx >= 0.57f && fx <= 0.70f)) &&
                   fy >= 0.24f && fy <= 0.76f;
        case 2: /* stop */
            return fx >= 0.28f && fx <= 0.72f && fy >= 0.28f && fy <= 0.72f;
        case 3: /* plus */
            return (std::fabs(fx - 0.5f) <= 0.32f && std::fabs(fy - 0.5f) <= 0.08f) ||
                   (std::fabs(fy - 0.5f) <= 0.32f && std::fabs(fx - 0.5f) <= 0.08f);
        case 4: /* minus */
            return std::fabs(fx - 0.5f) <= 0.32f && std::fabs(fy - 0.5f) <= 0.08f;
        default: /* record */
        {
            const float dx = fx - 0.5f;
            const float dy = fy - 0.5f;
            return dx * dx + dy * dy <= 0.13f;
        }
    }
}

struct Gallery
{
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    ig::sdl2::Backend *backend = nullptr;
    ig::Context *ui = nullptr;
    uint64_t previousCounter = 0;
    uint64_t frequency = 1;
    bool running = true;

    int section = 0;
    bool enabled = true;
    bool toggled = false;
    bool firstChoice = true;
    int selectedEntry = 0;
    float volume = 0.5f;
    int count = 8;
    float dragValue = 1.0f;
    int quality = 1;
    int listChoice = 2;
    ig::String name = "player";
    ig::String notes = "multi line\ntext field";
    int integer = 42;
    float number = 3.14f;
    float vec3[3] = {1.0f, 2.0f, 3.0f};
    ig::Color color = ig::Color(90, 160, 240, 255);
    bool headerOpen = true;
    bool treeOpen = true;
    bool dialogOpen = false;
    bool floatingOpen = false;
    int menuActions = 0;
    int contextActions = 0;
    int behindClicks = 0;
    int dialogClicks = 0;
    float behindSlider = 0.5f;
    bool behindCheck = false;
    bool dialogCheck = false;
    bool gridChecked = true;
    ig::String lastAction = "none";
    SDL_Texture *icons[kIconCount] = {};
    ig::TextureId iconIds[kIconCount] = {};
    int iconClicks = 0;
    bool playing = false;
    ig::String lastIcon = "none";
    float voiceVolume = 0.9f;
    float voicePan = 0.0f;
    float voiceTune = 0.0f;
    int voicePage = 0;
    ig::Color voiceAccent = ig::Color(96, 212, 255, 255);
    bool accentFromTheme = false;
    float channelLevels[kChannelCount] = {0.82f, 0.64f, 0.48f, 0.30f};
    float masterLevel = 0.75f;
    int sortColumn = -1;
    bool sortAscending = true;
    int firstRow = 0;
    int lastRow = 0;
    int virtualItem = -1;
    ig::Transform3D gizmoTransform;
    ig::Gizmo3DMode gizmoMode = ig::Gizmo3DMode::Translate;
    int gizmoDrags = 0;
    ig::String gizmoLastChange = "none";
    ig::CodeEditorState code;
};

Gallery g;

bool buildIcons(SDL_Renderer *renderer)
{
    const int size = 24;
    std::vector<unsigned char> pixels(static_cast<size_t>(size) * static_cast<size_t>(size) * 4u);
    for (int kind = 0; kind < kIconCount; ++kind)
    {
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                unsigned char *pixel = &pixels[static_cast<size_t>((y * size + x) * 4)];
                const unsigned char value = iconShape(kind, x, y, size) ? 255u : 0u;
                pixel[0] = value;
                pixel[1] = value;
                pixel[2] = value;
                pixel[3] = value;
            }
        }
        SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                                 SDL_TEXTUREACCESS_STATIC, size, size);
        if (!texture)
            return false;
        if (SDL_UpdateTexture(texture, nullptr, pixels.data(), size * 4) != 0 ||
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) != 0)
        {
            SDL_DestroyTexture(texture);
            return false;
        }
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
        g.icons[kind] = texture;
        g.iconIds[kind] = ig::sdl2::textureId(texture);
    }
    return true;
}

void basicSection(ig::Context &ui)
{
    ui.separatorText("Buttons");
    if (ui.button("button"))
        ui.showToast("clicked", "button clicked");
    ui.sameLine();
    ui.smallButton("small");
    ui.sameLine();
    ui.checkbox("checkbox", g.enabled);
    ui.sameLine();
    ui.toggleSwitch("switch", g.toggled);

    ui.separatorText("Choice");
    if (ui.radioButton("choice one", g.firstChoice))
        g.firstChoice = true;
    ui.sameLine();
    if (ui.radioButton("choice two", !g.firstChoice))
        g.firstChoice = false;
    for (int i = 0; i < 3; ++i)
    {
        if (ui.selectable(numbered("entry ", i + 1), g.selectedEntry == i, 220.0f))
            g.selectedEntry = i;
    }

    ui.separatorText("Progress");
    ui.progressBar(g.volume, 1.0f, 260.0f);
    ui.sliderFloat("volume", g.volume, 0.0f, 1.0f, 260.0f);

    ui.separatorText("Disclosure");
    if (ui.collapsingHeader("collapsing header", g.headerOpen, 260.0f))
    {
        ui.indent();
        ui.label("content inside the header");
        ui.unindent();
    }
    if (ui.treeNode("tree node", g.treeOpen, 260.0f))
    {
        ui.indent();
        ui.label("child one");
        ui.label("child two");
        ui.unindent();
    }
}

void inputsSection(ig::Context &ui)
{
    ui.separatorText("Sliders and drags");
    ui.sliderFloat("slider float", g.volume, 0.0f, 1.0f, 280.0f);
    ui.sliderInt("slider int", g.count, 0, 20, 280.0f);
    ui.dragFloat("drag float", g.dragValue, 0.0f, 10.0f, 0.02f, 280.0f);
    ui.dragFloat3("drag float3", g.vec3[0], g.vec3[1], g.vec3[2], -10.0f, 10.0f, 0.05f, 280.0f);
    ui.stepperInt("stepper", g.count, 0, 20, 160.0f);

    ui.separatorText("Text and numbers");
    ui.label("text");
    ui.inputText("name", g.name, 280.0f);
    ui.label("integer");
    ui.inputInt("integer", g.integer, 280.0f);
    ui.label("float");
    ui.inputFloat("float", g.number, 280.0f, 3);
    ui.label("multi line");
    ui.inputTextMultiline("notes", g.notes, 280.0f, 90.0f);

    ui.separatorText("Color");
    ui.colorEdit("color", g.color, 280.0f, 128.0f);
}

void listsSection(ig::Context &ui)
{
    ui.separatorText("Selection");
    ui.label("combo box");
    ui.comboBox("quality", g.quality, ig::Span<const ig::StringView>(kQuality), 200.0f);
    ui.label("list box");
    ui.listBox("fruit", g.listChoice, ig::Span<const ig::StringView>(kFruit), 200.0f, 5);

    ui.separatorText("Table");
    int order[6] = {0, 1, 2, 3, 4, 5};
    if (g.sortColumn == 0)
    {
        std::sort(order, order + 6, [](int a, int b)
        {
            const int result = std::strcmp(kFruit[a].data(), kFruit[b].data());
            return g.sortAscending ? result < 0 : result > 0;
        });
    }
    else if (g.sortColumn == 2)
    {
        std::sort(order, order + 6, [](int a, int b)
        {
            return g.sortAscending ? kWeight[a] < kWeight[b] : kWeight[a] > kWeight[b];
        });
    }
    if (ui.beginTable("table", 3, 420.0f))
    {
        ui.tableNextColumn();
        ui.tableHeader("name", g.sortColumn, g.sortAscending);
        ui.tableNextColumn();
        ui.tableHeader("kind", g.sortColumn, g.sortAscending);
        ui.tableNextColumn();
        ui.tableHeader("weight", g.sortColumn, g.sortAscending);
        for (int row = 0; row < 6; ++row)
        {
            const int item = order[row];
            ui.tableNextColumn();
            ui.label(kFruit[item]);
            ui.tableNextColumn();
            ui.label("fruit");
            ui.tableNextColumn();
            ui.label(numbered("", kWeight[item], " g"));
        }
        ui.endTable();
    }

    ui.separatorText("Virtual list, 10000 items");
    // Only the rows on screen are submitted, each one placed with the rect the
    // list hands out. Letting the layout stack them instead pins them to the top
    // of the child, so the list looks frozen while the scrollbar moves.
    if (ui.beginVirtualList("virtual", 10000, 20.0f, 160.0f, g.firstRow, g.lastRow, true, 300.0f))
    {
        for (int i = g.firstRow; i < g.lastRow; ++i)
        {
            ui.pushId(static_cast<uint64_t>(i));
            if (ui.selectable(numbered("item ", i), g.virtualItem == i, ui.virtualListItemRect(i)))
                g.virtualItem = i;
            ui.popId();
        }
        ui.endVirtualList();
    }
    ui.label(numbered("rows drawn: ", g.lastRow - g.firstRow));
    ui.sameLine();
    ui.label(numbered("selected: ", g.virtualItem));
}

void layoutSection(ig::Context &ui)
{
    ui.separatorText("Child region");
    if (ui.beginChild("child", 110.0f, true, 320.0f))
    {
        for (int i = 0; i < 8; ++i)
            ui.label(numbered("scrolling line ", i));
        ui.endChild();
    }

    ui.separatorText("Property rows");
    if (ui.beginPropertyRow("position"))
    {
        ui.dragFloat3("position", g.vec3[0], g.vec3[1], g.vec3[2], -10.0f, 10.0f, 0.05f, 220.0f);
        ui.endPropertyRow();
    }
    if (ui.beginPropertyRow("enabled"))
    {
        ui.checkbox("on", g.enabled);
        ui.endPropertyRow();
    }

    ui.separatorText("Spacing");
    ui.label("indent");
    ui.indent(24.0f);
    ui.label("indented text");
    ui.unindent(24.0f);
    ui.spacing(12.0f);
    ui.label("after spacing");
}

void windowsSection(ig::Context &ui)
{
    ui.separatorText("Dialogs and toasts");
    if (ui.button("open dialog"))
        g.dialogOpen = true;
    ui.sameLine();
    if (ui.button("show toast"))
        ui.showToast("demo", "this is a toast", ig::ToastPosition::BottomRight, 2.5f);
    ui.sameLine();
    if (ui.button("floating window"))
        g.floatingOpen = true;

    ui.separatorText("Event leak check");
    ui.label("With the dialog open, click these: the counters must not move.");
    if (ui.button("behind counter"))
        ++g.behindClicks;
    ui.sameLine();
    ui.label(numbered("behind clicks: ", g.behindClicks));
    ui.sliderFloat("behind slider", g.behindSlider, 0.0f, 1.0f, 260.0f);
    ui.checkbox("behind checkbox", g.behindCheck);
    ui.sameLine();
    ui.label(numbered("clicks inside the dialog: ", g.dialogClicks));
}

void menusSection(ig::Context &ui)
{
    ui.label("Menu bar: open a menu and click the widgets it covers.");
    const ig::Vec2 bar = ui.cursor();
    if (ui.beginMenuBar(ig::Rect(0.0f, bar.y, 360.0f, 26.0f)))
    {
        if (ui.beginMenu("File"))
        {
            if (ui.beginSubMenu("Export"))
            {
                if (ui.menuItem("PNG sequence"))
                    g.lastAction = "File > Export > PNG sequence";
                if (ui.menuItem("MP4 video"))
                    g.lastAction = "File > Export > MP4 video";
                ui.endSubMenu();
            }
            if (ui.menuItem("New scene"))
            {
                ++g.menuActions;
                g.lastAction = "File > New scene";
            }
            ui.menuSeparator();
            ui.menuItem("Save (disabled)", false);
            ui.endMenu();
        }
        if (ui.beginMenu("Edit"))
        {
            if (ui.menuCheckbox("Grid", g.gridChecked))
                g.lastAction = g.gridChecked ? "Edit > Grid on" : "Edit > Grid off";
            if (ui.menuCheckbox("Inspector", g.behindCheck))
                g.lastAction = "Edit > Inspector toggled";
            ui.endMenu();
        }
        if (ui.beginMenu("Help"))
        {
            if (ui.menuItem("About"))
                g.lastAction = "Help > About";
            ui.endMenu();
        }
        ui.endMenuBar();
    }
    ui.spacing(28.0f);

    ui.separatorText("Event leak check");
    ui.label("The dropdown covers these: nothing here may react while it is open.");
    if (ui.button("behind counter"))
        ++g.behindClicks;
    ui.sameLine();
    ui.label(numbered("behind clicks: ", g.behindClicks));
    ui.sliderFloat("behind slider", g.behindSlider, 0.0f, 1.0f, 260.0f);
    ui.checkbox("behind checkbox", g.behindCheck);
    ui.sameLine();
    if (ui.button("behind button two"))
        ++g.behindClicks;

    ui.separatorText("Context menu");
    ui.label("Right click inside the box below.");
    // Same split as the accent swatches: drawn in content coordinates, hit-tested
    // with the local rect the bounds-based API expects.
    const ig::Vec2 boxOrigin = ui.cursor();
    const ig::Rect box(boxOrigin.x, boxOrigin.y, 320.0f, 80.0f);
    ui.drawRectFilled(box, ig::Color(48, 52, 60, 255));
    ui.drawRect(box, ig::Color(96, 104, 116, 255));
    ui.drawText("right click here", ig::Vec2(box.x + 10.0f, box.y + 32.0f),
                ig::Color(200, 205, 214, 255));
    ui.drawRectFilled(ig::Rect(box.x + 10.0f, box.y + 46.0f, 120.0f, 20.0f),
                      ig::Color(70, 76, 88, 255));
    if (ui.beginContextMenu("box context", ig::Rect(0.0f, boxOrigin.y, 320.0f, 80.0f)))
    {
        if (ui.menuItem("Duplicate"))
        {
            ++g.contextActions;
            g.lastAction = "context > Duplicate";
        }
        ui.menuItem("Delete (disabled)", false);
        ui.endContextMenu();
    }
    ui.spacing(88.0f);

    ui.separatorText("Result");
    ui.label(numbered("menu actions: ", g.menuActions));
    ui.label(numbered("context actions: ", g.contextActions));
    ig::String action = "last action: ";
    action += g.lastAction;
    ui.label(action);
}

void iconsSection(ig::Context &ui)
{
    ui.separatorText("Icon buttons");
    for (int i = 0; i < kIconCount; ++i)
    {
        if (i)
            ui.sameLine();
        if (ui.imageButton(kIconNames[i], g.iconIds[i], 40.0f, 40.0f))
        {
            ++g.iconClicks;
            g.lastIcon = kIconNames[i];
        }
    }

    ui.separatorText("Small icon buttons");
    for (int i = 0; i < kIconCount; ++i)
    {
        if (i)
            ui.sameLine();
        if (ui.smallImageButton(kIconNames[i], g.iconIds[i], 26.0f))
        {
            ++g.iconClicks;
            g.lastIcon = kIconNames[i];
        }
    }

    ui.separatorText("Icon button with two states");
    if (ui.imageButton("play toggle", g.iconIds[g.playing ? 1 : 0], 44.0f, 44.0f))
        g.playing = !g.playing;
    ui.sameLine();
    ui.label(g.playing ? "playing" : "stopped");
    ui.sameLine();
    ui.label(numbered("icon clicks: ", g.iconClicks));

    ig::String line = "last icon: ";
    line += g.lastIcon;
    ui.label(line);
}

void synthSection(ig::Context &ui)
{
    const float diameter = 64.0f;
    const float gap = 18.0f;

    // The accent is the application's choice, not a colour the widget fixes:
    // the picker below drives the voice dot and every value arc.
    const ig::Color accent = g.accentFromTheme ? ui.theme().focusColor : g.voiceAccent;

    // Voice header: the accent dot, the name of the voice and what makes it.
    const ig::Vec2 header = ui.cursor();
    ui.drawCircleFilled(ig::Vec2(header.x + 6.0f, header.y + 9.0f), 6.0f, accent);
    ui.drawText("Kick", ig::Vec2(header.x + 20.0f, header.y), ig::Color(238, 242, 250, 255));
    ui.drawText("synthesized", ig::Vec2(header.x + 66.0f, header.y + 2.0f),
                ig::Color(138, 148, 166, 255));
    ui.spacing(26.0f);

    // The knobs reuse the synth demo's: same body, same needle, same arcs, so
    // the immediate-mode voice and the retained one read as the same control.
    char volumeText[16];
    std::snprintf(volumeText, sizeof volumeText, "%d%%",
                  static_cast<int>(g.voiceVolume * 100.0f + 0.5f));
    char panText[16];
    const int panAmount = static_cast<int>(std::fabs(g.voicePan) * 100.0f + 0.5f);
    if (panAmount == 0)
        std::snprintf(panText, sizeof panText, "C");
    else
        std::snprintf(panText, sizeof panText, "%s %d", g.voicePan < 0.0f ? "L" : "R", panAmount);
    char tuneText[16];
    std::snprintf(tuneText, sizeof tuneText, "%d st", static_cast<int>(g.voiceTune));

    const synth::KnobStyle style = g.accentFromTheme ? synth::KnobStyle()
                                                     : synth::KnobStyle::accent(accent);
    const ig::Vec2 row = ui.cursor();
    synth::knob(ui, "voice volume", "Volume", volumeText, g.voiceVolume, 0.0f, 1.0f,
                row, diameter, style);
    synth::knob(ui, "voice pan", "Pan", panText, g.voicePan, -1.0f, 1.0f,
                ig::Vec2(row.x + diameter + gap, row.y), diameter, style);
    synth::knob(ui, "voice tune", "Tune", tuneText, g.voiceTune, -24.0f, 24.0f,
                ig::Vec2(row.x + (diameter + gap) * 2.0f, row.y), diameter, style);
    ui.spacing(diameter + 30.0f);

    ui.separatorText("Desk");
    // A mixer strip on the same accent as the voice, drawn by the same code on
    // both APIs. The whole control takes the press, which is what a finger
    // needs: it never has to land on the cap.
    const float faderWidth = 46.0f;
    const float faderHeight = 120.0f;
    const ig::FaderStyle faderStyle = g.accentFromTheme
        ? ig::FaderStyle()
        : ig::FaderStyle::accent(accent);
    const ig::Vec2 desk = ui.cursor();
    char levelText[16];
    for (int i = 0; i < kChannelCount; ++i)
    {
        const float x = desk.x + static_cast<float>(i) * (faderWidth + 8.0f);
        ui.fader(kChannelNames[i], g.channelLevels[i], 0.0f, 1.0f,
                 ig::Rect(x, desk.y, faderWidth, faderHeight),
                 ig::FaderOrientation::Vertical, faderStyle);
        std::snprintf(levelText, sizeof levelText, "%d",
                      static_cast<int>(g.channelLevels[i] * 100.0f + 0.5f));
        const ig::StringView name(kChannelNames[i]);
        const ig::StringView level(levelText);
        ui.drawText(name, ig::Vec2(x + (faderWidth - ui.textWidth(name)) * 0.5f,
                                   desk.y + faderHeight + 4.0f),
                    ig::Color(186, 194, 210, 255));
        ui.drawText(level, ig::Vec2(x + (faderWidth - ui.textWidth(level)) * 0.5f,
                                    desk.y + faderHeight + 18.0f),
                    ig::Color(120, 130, 148, 255));
    }
    ui.spacing(faderHeight + 32.0f);

    // The master runs across, so the one control that takes the whole hand is
    // the one that is widest.
    const ig::Vec2 master = ui.cursor();
    ui.fader("master level", g.masterLevel, 0.0f, 1.0f,
             ig::Rect(master.x, master.y, 216.0f, 44.0f), ig::FaderOrientation::Horizontal,
             faderStyle);
    std::snprintf(levelText, sizeof levelText, "master %d",
                  static_cast<int>(g.masterLevel * 100.0f + 0.5f));
    ui.drawText(ig::StringView(levelText), ig::Vec2(master.x, master.y + 48.0f),
                ig::Color(138, 148, 166, 255));
    ui.spacing(66.0f);

    ui.separatorText("Accent colour of the voice");
    for (int i = 0; i < kAccentCount; ++i)
    {
        if (i)
            ui.sameLine();
        ig::String label = kAccentNames[i];
        if (!g.accentFromTheme && g.voiceAccent == kAccents[i])
            label += " *";
        if (ui.smallButton(label))
        {
            g.voiceAccent = kAccents[i];
            g.accentFromTheme = false;
        }
    }
    ui.spacing(4.0f);
    ui.checkbox("follow the theme accent", g.accentFromTheme);
    ui.spacing(6.0f);
    ui.colorEdit("accent", g.voiceAccent, 220.0f, 56.0f);

    ui.separatorText("Pages of the voice");
    ui.tabBar("voice pages", g.voicePage, ig::Span<const ig::StringView>(kVoicePages), 420.0f);
    ui.spacing(6.0f);
    ui.label(g.voicePage == 0 ? "Volume, pan and tune of the synthesized kick."
                              : "Every page reuses the same knob.");
}

void gizmoSection(ig::Context &ui)
{
    ui.separatorText("3D transform gizmo");
    ui.label("The box is the object. Drag a ring to turn it, an arrow to slide it,");
    ui.label("a square to stretch it - the outer ring turns it the way you see it.");

    for (int i = 0; i < 3; ++i)
    {
        if (i)
            ui.sameLine();
        const ig::Gizmo3DMode mode = static_cast<ig::Gizmo3DMode>(i);
        ig::String label = i == 0 ? "translate" : (i == 1 ? "rotate" : "scale");
        if (g.gizmoMode == mode)
            label += " *";
        if (ui.smallButton(label))
            g.gizmoMode = mode;
    }
    ui.spacing(6.0f);

    const float width = 440.0f;
    const float height = 300.0f;
    const ig::Vec2 canvas = ui.cursor();
    const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    // The canvas is wider than it is tall, and the gizmo maps the camera's
    // [-1, 1] over it: without this the world would be stretched sideways and
    // the rings would come out as ellipses.
    float projection[16];
    for (int i = 0; i < 16; ++i)
        projection[i] = identity[i];
    projection[0] = height / width;
    float view[16];
    lookAtOrigin(view, 3.0f, 2.2f, 3.0f);

    // The canvas the gizmo draws on, so the handles are visible against it.
    ui.drawRectFilled(ig::Rect(canvas.x, canvas.y, width, height), ig::Color(24u, 27u, 34u, 255u));
    ui.drawRect(ig::Rect(canvas.x, canvas.y, width, height), ui.theme().borderColor);

    ig::Gizmo3DOptions options;
    options.axisLength = 0.5f;
    options.translateSnap = 0.1f;
    options.rotateSnap = 15.0f;
    options.scaleSnap = 0.1f;
    if (ui.gizmo3D("transform", g.gizmoTransform, g.gizmoMode,
                   ig::Rect(canvas.x, canvas.y, width, height), view, projection, options))
    {
        ++g.gizmoDrags;
        char buffer[96];
        std::snprintf(buffer, sizeof buffer, "pos %.2f %.2f %.2f  rot %.0f %.0f %.0f  scale %.2f %.2f %.2f",
                      static_cast<double>(g.gizmoTransform.position.x),
                      static_cast<double>(g.gizmoTransform.position.y),
                      static_cast<double>(g.gizmoTransform.position.z),
                      static_cast<double>(g.gizmoTransform.rotation.x),
                      static_cast<double>(g.gizmoTransform.rotation.y),
                      static_cast<double>(g.gizmoTransform.rotation.z),
                      static_cast<double>(g.gizmoTransform.scale.x),
                      static_cast<double>(g.gizmoTransform.scale.y),
                      static_cast<double>(g.gizmoTransform.scale.z));
        g.gizmoLastChange = buffer;
    }
    ui.spacing(height + 8.0f);

    ui.label(g.gizmoLastChange);
    ig::String drags = numbered("changes: ", g.gizmoDrags);
    ui.sameLine();
    ui.label(drags);
    if (ui.smallButton("reset"))
    {
        g.gizmoTransform = ig::Transform3D();
        g.gizmoLastChange = "reset";
        g.gizmoDrags = 0;
    }
}

void codeSection(ig::Context &ui, const ig::Rect &bounds)
{
    ui.codeEditor("code", g.code, bounds);
}

void frame(void *)
{
    ig::Context &ui = *g.ui;

#ifdef __EMSCRIPTEN__
    double cssWidth = 0.0;
    double cssHeight = 0.0;
    emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(g.window, &windowWidth, &windowHeight);
    if (cssWidth > 0.0 && cssHeight > 0.0 &&
        (windowWidth != static_cast<int>(cssWidth) || windowHeight != static_cast<int>(cssHeight)))
        SDL_SetWindowSize(g.window, static_cast<int>(cssWidth), static_cast<int>(cssHeight));
#endif

    const uint64_t currentCounter = SDL_GetPerformanceCounter();
    const float deltaSeconds = static_cast<float>(currentCounter - g.previousCounter) /
                               static_cast<float>(g.frequency);
    g.previousCounter = currentCounter;

    SDL_Event nativeEvent;
    while (SDL_PollEvent(&nativeEvent))
    {
        if (nativeEvent.type == SDL_QUIT)
            g.running = false;
        ig::Event event;
        if (ig::sdl2::translateEvent(nativeEvent, event))
            ui.pushEvent(event);
    }

    ui.beginFrame(ig::sdl2::frameInfo(g.window, deltaSeconds));
    if (ui.beginMainWindow("iGUI immediate mode"))
    {
        ui.tabBar("sections", g.section, ig::Span<const ig::StringView>(kSections));
        ui.spacing(6.0f);
        ui.indent(10.0f);
        switch (g.section)
        {
            case 0: basicSection(ui); break;
            case 1: inputsSection(ui); break;
            case 2: listsSection(ui); break;
            case 3: layoutSection(ui); break;
            case 4: windowsSection(ui); break;
            case 5: menusSection(ui); break;
            case 6: iconsSection(ui); break;
            case 7: synthSection(ui); break;
            case 8: gizmoSection(ui); break;
            default:
            {
                int width = 0;
                int height = 0;
                SDL_GetWindowSize(g.window, &width, &height);
                codeSection(ui, ig::Rect(12.0f, 40.0f, static_cast<float>(width) - 24.0f,
                                         static_cast<float>(height) - 56.0f));
                break;
            }
        }
        ui.unindent(10.0f);
        ui.endWindow();
    }

    if (g.floatingOpen)
        ui.raiseWindow("Floating window");

    if (g.dialogOpen && ui.beginDialog("Modal dialog", g.dialogOpen, ig::Vec2(380.0f, 250.0f)))
    {
        ui.label("Application-modal: nothing behind it may react.");
        ui.inputText("name", g.name, 300.0f);
        ui.sliderFloat("dialog value", g.volume, 0.0f, 1.0f, 300.0f);
        ui.checkbox("dialog only", g.dialogCheck);
        ui.spacing(6.0f);
        if (ui.button("inside the dialog"))
            ++g.dialogClicks;
        ui.sameLine();
        if (ui.button("close"))
            g.dialogOpen = false;
        ui.endDialog();
    }

    if (g.floatingOpen &&
        ui.beginWindow("Floating window", ig::Rect(520.0f, 140.0f, 280.0f, 180.0f), &g.floatingOpen))
    {
        ui.label("Drag me by the title bar.");
        ui.checkbox("enabled", g.enabled);
        ui.sliderFloat("value", g.volume, 0.0f, 1.0f, 200.0f);
        ui.endWindow();
    }

    const ig::DrawData &drawData = ui.endFrame();

    if (ui.wantsTextInput())
        SDL_StartTextInput();
    else
        SDL_StopTextInput();

    SDL_SetRenderDrawColor(g.renderer, 32u, 34u, 40u, 255u);
    SDL_RenderClear(g.renderer);
    if (!g.backend->render(drawData))
        g.running = false;
    SDL_RenderPresent(g.renderer);
}
}

int main(int, char **)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;
    g.window = SDL_CreateWindow("iGUI immediate mode", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                960, 640, SDL_WINDOW_RESIZABLE);
    if (!g.window)
        return 2;
    g.renderer = SDL_CreateRenderer(g.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g.renderer)
        return 3;

    g.backend = new ig::sdl2::Backend(g.renderer);
    if (!g.backend->prepareFontAtlas())
        return 4;
    g.ui = new ig::Context(*g.backend, &g.backend->fontAtlas());
    if (!buildIcons(g.renderer))
        return 5;
    g.code.setHighlighterForFile("sample.cpp");
    g.code.setText(kSample);
    g.frequency = SDL_GetPerformanceFrequency();
    g.previousCounter = SDL_GetPerformanceCounter();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(frame, nullptr, 0, 1);
#else
    while (g.running)
        frame(nullptr);
#endif

    delete g.ui;
    delete g.backend;
    for (int i = 0; i < kIconCount; ++i)
        SDL_DestroyTexture(g.icons[i]);
    SDL_DestroyRenderer(g.renderer);
    SDL_DestroyWindow(g.window);
    SDL_Quit();
    return 0;
}
