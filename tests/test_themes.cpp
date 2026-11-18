// The theme presets: every pair of colours a widget paints one over the other
// has to be readable (WCAG contrast), in every preset, and the presets must be
// found by name. The dark and light presets must keep the look fields off, so
// they paint exactly as they did before those fields existed.

#include <igui/widgets/Theme.hpp>

#include <math.h>
#include <stdio.h>
#include <string.h>

using ig::retained::Color;
using ig::retained::Theme;

namespace
{

int failures = 0;

float channel(unsigned char v)
{
    const float c = static_cast<float>(v) / 255.0f;
    return c <= 0.03928f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}

float luminance(const Color& c)
{
    return 0.2126f * channel(c.r) + 0.7152f * channel(c.g) + 0.0722f * channel(c.b);
}

// WCAG: (lighter + 0.05) / (darker + 0.05), 1 to 21.
float contrast(const Color& a, const Color& b)
{
    const float la = luminance(a), lb = luminance(b);
    return (fmaxf(la, lb) + 0.05f) / (fminf(la, lb) + 0.05f);
}

void readable(const char* preset, const char* what, const Color& fg, const Color& bg, float atLeast)
{
    const float ratio = contrast(fg, bg);
    if (ratio < atLeast)
    {
        printf("  FAIL %-10s %-34s contrast %.2f, wanted %.1f\n", preset, what, ratio, atLeast);
        ++failures;
    }
}

void check(bool ok, const char* what)
{
    if (!ok)
    {
        printf("  FAIL %s\n", what);
        ++failures;
    }
}

bool same(const Color& a, const Color& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

void testPreset(const char* name, const Theme& t)
{
    // Text over what it is painted on.
    readable(name, "text / window", t.textColor, t.bgColor, 4.5f);
    readable(name, "text / panel", t.textColor, t.panelColor, 4.5f);
    readable(name, "text / input", t.textColor, t.inputBg, 4.5f);
    readable(name, "text / button", t.textColor, t.buttonNormal, 4.5f);
    readable(name, "text / button hover", t.textColor, t.buttonHover, 4.5f);
    readable(name, "text / button pressed", t.textColor, t.buttonPressed, 4.5f);
    readable(name, "menu text / menu", t.menuItemText, t.menuBg, 4.5f);
    readable(name, "menu text on hover / hover", t.menuItemTextHover, t.menuItemHover, 3.0f);
    readable(name, "float title text / title", t.floatTitleText, t.floatTitleBg, 4.5f);
    readable(name, "dialog text / dialog", t.dialogText, t.dialogBg, 4.5f);
    readable(name, "dialog title / its bar", t.dialogTitleText, t.dialogTitleBg.a > 0 ? t.dialogTitleBg : t.panelColor, 4.5f);
    readable(name, "tooltip text / tooltip", t.tooltipText, t.tooltipBg, 4.5f);
    readable(name, "text / tab bar", t.textColor, t.buttonNormal, 4.5f);
    readable(name, "other tabs' text / tab bar", t.textDisabled, t.buttonNormal, 1.3f);
    readable(name, "selected tab line / tab bar", t.tabAccent, t.buttonNormal, 1.15f);

    // Dimmer on purpose, but still there.
    readable(name, "disabled text / window", t.textDisabled, t.bgColor, 1.5f);
    check(contrast(t.textDisabled, t.bgColor) < contrast(t.textColor, t.bgColor), "disabled text is dimmer than text");
    readable(name, "line numbers / gutter", t.lineNumberColor, t.gutterBg, 2.0f);

    // The tool bar: its icons on its panel, on the checked button, and the dot of a checked button on that button.
    readable(name, "tool bar icon / panel", t.toolbarIcon, t.panelColor, 4.5f);
    readable(name, "tool bar icon / checked button", t.toolbarIcon, t.toolbarChecked, 3.0f);
    readable(name, "checked button / panel", t.toolbarChecked, t.panelColor, 1.1f);
    readable(name, "tool bar dot / checked button", t.toolbarCheckedDot, t.toolbarChecked, 1.5f);
    readable(name, "disabled tool bar icon / panel", t.toolbarIconDisabled, t.panelColor, 1.4f);
    check(contrast(t.toolbarIconDisabled, t.panelColor) < contrast(t.toolbarIcon, t.panelColor), "a disabled tool bar icon is dimmer than an enabled one");

    // Marks and fills against what they sit in.
    readable(name, "check mark / input", t.checkMark, t.inputBg, 2.0f);
    readable(name, "progress fill / track", t.progressFill, t.progressTrack.a > 0 ? t.progressTrack : t.sliderTrack, 1.3f);
    readable(name, "slider fill / track", t.sliderFill, t.sliderTrack, 1.0f);
}

} // namespace

int main()
{
    struct Entry
    {
        const char* name;
        Theme theme;
    } presets[] = {
        {"dark", Theme::dark()}, {"light", Theme::light()}, {"xp", Theme::windowsXP()}, {"macos", Theme::macOS()}, {"macos-dark", Theme::macOSDark()},
    };
    for (const Entry& e : presets)
        testPreset(e.name, e.theme);

    // Names: the order a picker shows, and each one finds its preset.
    check(Theme::kPresetCount == 5, "five presets");
    for (int i = 0; i < Theme::kPresetCount; ++i)
    {
        check(strcmp(Theme::presetName(i), presets[i].name) == 0, "presetName is in the order of the presets");
        check(same(Theme::byName(Theme::presetName(i)).bgColor, presets[i].theme.bgColor) && same(Theme::byName(Theme::presetName(i)).buttonBorder, presets[i].theme.buttonBorder),
              "byName finds the preset presetName names");
    }
    check(strcmp(Theme::presetName(-1), "") == 0 && strcmp(Theme::presetName(Theme::kPresetCount), "") == 0, "presetName is empty out of range");
    check(same(Theme::byName("nonsense").bgColor, Theme::dark().bgColor) && same(Theme::byName(nullptr).bgColor, Theme::dark().bgColor) && same(Theme::byName("").bgColor, Theme::dark().bgColor),
          "an unknown, empty or null name gives dark");

    // dark is what iGUI always had; dark and light keep the look fields off.
    check(same(Theme::dark().bgColor, Theme().bgColor) && same(Theme::dark().textColor, Theme().textColor), "dark is the default theme");
    for (const Entry& e : presets)
    {
        const bool plain = strcmp(e.name, "dark") == 0 || strcmp(e.name, "light") == 0;
        if (!plain) continue;
        const Theme& t = e.theme;
        check(t.controlGradient == 0.0f && t.titleGradient == 0.0f && !t.progressBlocks && t.buttonHoverRing.a == 0 && t.dialogTitleBg.a == 0 && t.progressTrack.a == 0,
              "dark and light leave the look fields off");
        check(same(t.progressFill, Color(140, 140, 145, 255)), "and the progress bar keeps its grey");
    }

    // dark is what the tool bar always painted (the values that were fixed in its paint)
    check(same(Theme::dark().toolbarChecked, Color(60, 100, 160, 255)) && same(Theme::dark().toolbarCheckedDot, Color(100, 200, 255, 255)) &&
                  same(Theme::dark().toolbarIcon, Color(220, 220, 225, 255)) && same(Theme::dark().toolbarIconDisabled, Color(100, 100, 110, 255)),
          "dark keeps the colours of the tool bar it always had");
    // the other presets say theirs: a light preset does not leave the light icons of the dark one on a light bar
    check(!same(Theme::light().toolbarIcon, Theme::dark().toolbarIcon) && !same(Theme::macOS().toolbarChecked, Theme::dark().toolbarChecked) &&
                  !same(Theme::windowsXP().toolbarIcon, Theme::dark().toolbarIcon) && !same(Theme::macOSDark().toolbarChecked, Theme::light().toolbarChecked),
          "light, macos, macos-dark and xp define the colours of the tool bar");

    // The two looks that use them.
    check(Theme::windowsXP().progressBlocks && Theme::windowsXP().controlGradient > 0.0f && Theme::windowsXP().titleGradient > 0.0f && Theme::windowsXP().buttonHoverRing.a > 0, "xp turns the look fields on");
    check(!Theme::macOS().progressBlocks && Theme::macOS().controlGradient == 0.0f, "macos stays flat");
    check(same(Theme::macOS().focusColor, Color(0, 122, 255, 255)) && same(Theme::macOS().progressFill, Color(0, 122, 255, 255)), "macos uses the system blue as accent");

    // shade() moves a colour toward black or white and keeps its alpha.
    const Color mid(100, 100, 100, 77);
    check(same(Theme::shade(mid, 255, 0.0f), mid) && Theme::shade(mid, 255, 1.0f).r == 255 && Theme::shade(mid, 0, 1.0f).g == 0 && Theme::shade(mid, 255, 0.5f).a == 77, "shade moves toward the target and keeps alpha");

    if (failures)
    {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("themes: ok\n");
    return 0;
}
