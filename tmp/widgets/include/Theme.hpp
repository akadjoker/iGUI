#pragma once

#include "BuGUI_base.hpp"


namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  Theme - centralized colors/sizes for all widgets
// ═════════════════════════════════════════════════════════════════════════════

struct Theme
{
    // Window / Panel
    Color bgColor        = Color(30, 30, 30, 255);
    Color panelColor     = Color(45, 45, 48, 255);
    Color borderColor    = Color(70, 70, 70, 255);

    // Text
    Color textColor      = Color(220, 220, 220, 255);
    Color textDisabled   = Color(120, 120, 120, 255);

    // Button
    Color buttonNormal   = Color(60, 60, 65, 255);
    Color buttonHover    = Color(75, 75, 80, 255);
    Color buttonPressed  = Color(45, 45, 50, 255);
    Color buttonBorder   = Color(90, 90, 95, 255);

    // Focus / selection
    Color focusColor     = Color(180, 180, 185, 255);
    Color selectionColor = Color(180, 180, 185, 128);

    // Slider
    Color sliderTrack        = Color(55, 55, 58, 255);
    Color sliderTrackHover   = Color(68, 68, 72, 255);
    Color sliderTrackFocused = Color(75, 75, 80, 255);
    Color sliderFill         = Color(140, 140, 145, 255);
    Color sliderFillHover    = Color(165, 165, 170, 255);
    Color sliderThumb        = Color(160, 160, 165, 255);
    Color sliderThumbHover   = Color(195, 195, 200, 255);
    Color sliderThumbPressed = Color(220, 220, 225, 255);

    // Input (checkbox box, text input bg)
    Color inputBg          = Color(35, 35, 38, 255);
    Color inputBgHover     = Color(48, 48, 52, 255);
    Color inputBorder      = Color(80, 80, 85, 255);
    Color inputBorderHover = Color(110, 110, 118, 255);
    Color checkMark        = Color(200, 200, 205, 255);

    // Switch
    Color switchThumb      = Color(220, 220, 225, 255);

    // Tooltip
    Color tooltipBg          = Color(20, 20, 22, 230);
    Color tooltipBorder      = Color(90, 90, 95, 200);
    Color tooltipText        = Color(220, 220, 220, 255);

    // FloatWindow
    Color floatTitleBg       = Color(50, 50, 55, 255);
    Color floatTitleText     = Color(210, 210, 210, 255);
    Color floatBg            = Color(38, 38, 40, 255);
    Color floatBorder        = Color(75, 75, 80, 255);
    Color floatBtnHover      = Color(70, 70, 75, 255);
    Color floatCloseHover    = Color(200, 60, 60, 255);

    // SidePanel / Drawer
    Color drawerBg           = Color(40, 40, 44, 255);
    Color drawerBorder       = Color(70, 70, 75, 255);

    // Dialog
    Color dialogScrim        = Color(0, 0, 0, 140);
    Color dialogBg           = Color(48, 48, 52, 255);
    Color dialogBorder       = Color(80, 80, 85, 255);
    Color dialogTitleText    = Color(220, 220, 225, 255);
    Color dialogText         = Color(190, 190, 195, 255);
    Color dialogBtnBg        = Color(65, 65, 70, 255);
    Color dialogBtnHover     = Color(80, 80, 85, 255);
    Color dialogBtnPrimary   = Color(70, 130, 180, 255);
    Color dialogBtnPrimaryHover = Color(90, 150, 200, 255);
    Color dialogBtnDanger    = Color(180, 60, 60, 255);
    Color dialogBtnDangerHover = Color(200, 80, 80, 255);

    // Editor gutter (line numbers)
    Color gutterBg           = Color(38, 38, 42, 255);
    Color lineNumberColor    = Color(120, 120, 120, 255);
    Color lineNumberActive   = Color(220, 220, 220, 255);

    // Editor
    Color currentLineHighlight = Color(255, 255, 255, 8);
    Color scrollbarThumb     = Color(120, 120, 120, 80);

    // Carousel
    Color carouselArrowBg    = Color(0, 0, 0, 140);
    Color carouselArrowFg    = Color(230, 230, 235, 240);
    Color carouselDotActive  = Color(100, 160, 220, 255);
    Color carouselDotInactive = Color(100, 100, 105, 180);

    // Collapsible
    Color collapsibleHeaderBg = Color(50, 52, 58, 255);

    // Menu
    Color menuBarBg          = Color(45, 45, 48, 255);
    Color menuBarItemHover   = Color(65, 65, 70, 255);
    Color menuBg             = Color(45, 45, 48, 255);
    Color menuBorder         = Color(70, 70, 75, 255);
    Color menuItemHover      = Color(65, 65, 70, 255);
    Color menuItemText       = Color(220, 220, 220, 255);
    Color menuItemTextHover  = Color(255, 255, 255, 255);
    Color menuItemDisabled   = Color(120, 120, 120, 255);
    Color menuSeparator      = Color(70, 70, 75, 255);
    Color menuShortcutText   = Color(150, 150, 155, 255);
    Color menuShortcutHover  = Color(200, 200, 200, 255);
    Color menuCheckMark      = Color(100, 160, 220, 255);
    Color menuSubmenuArrow   = Color(180, 180, 185, 255);

    // Sizes
    float fontSize       = 16.0f;
    float padding        = 6.0f;
    float borderRadius   = 4.0f;
    float buttonHeight   = 28.0f;
    float sliderHeight   = 20.0f;
    float sliderTrackWidth = 4.0f;
    float inputHeight    = 26.0f;
    float scrollbarWidth = 12.0f;
    float scrollbarMinThumb = 16.0f;
    float textEditPadding = 4.0f;
    float gutterPadding  = 8.0f;
    float lineSpacing    = 2.0f;
    float tooltipPadX    = 8.0f;
    float tooltipPadY    = 4.0f;
    float menuBarHeight  = 26.0f;
    float menuItemHeight = 24.0f;
    float menuItemPadX   = 16.0f;
    float menuMinWidth   = 160.0f;
    float menuShadowSize = 4.0f;

    // ── Theme presets ─────────────────────────────────────────────────────

    /// @brief Get the dark theme preset (default).
    static Theme dark()
    {
        return Theme{};  // defaults are the dark theme
    }

    /// @brief Get the light theme preset.
    static Theme light()
    {
        Theme t;
        // Window / Panel
        t.bgColor        = Color(240, 240, 240, 255);
        t.panelColor     = Color(250, 250, 250, 255);
        t.borderColor    = Color(200, 200, 200, 255);
        // Text
        t.textColor      = Color(30, 30, 30, 255);
        t.textDisabled   = Color(140, 140, 140, 255);
        // Button
        t.buttonNormal   = Color(225, 225, 228, 255);
        t.buttonHover    = Color(210, 210, 215, 255);
        t.buttonPressed  = Color(195, 195, 200, 255);
        t.buttonBorder   = Color(180, 180, 185, 255);
        // Focus / selection
        t.focusColor     = Color(0, 120, 215, 255);
        t.selectionColor = Color(0, 120, 215, 80);
        // Slider
        t.sliderTrack        = Color(210, 210, 215, 255);
        t.sliderTrackHover   = Color(195, 195, 200, 255);
        t.sliderTrackFocused = Color(180, 180, 185, 255);
        t.sliderFill         = Color(0, 120, 215, 255);
        t.sliderFillHover    = Color(30, 140, 225, 255);
        t.sliderThumb        = Color(0, 120, 215, 255);
        t.sliderThumbHover   = Color(30, 140, 225, 255);
        t.sliderThumbPressed = Color(0, 90, 180, 255);
        // Input
        t.inputBg          = Color(255, 255, 255, 255);
        t.inputBgHover     = Color(255, 255, 255, 255);
        t.inputBorder      = Color(180, 180, 185, 255);
        t.inputBorderHover = Color(0, 120, 215, 255);
        t.checkMark        = Color(0, 120, 215, 255);
        // Switch
        t.switchThumb      = Color(255, 255, 255, 255);
        // Tooltip
        t.tooltipBg        = Color(255, 255, 255, 240);
        t.tooltipBorder    = Color(180, 180, 185, 255);
        t.tooltipText      = Color(30, 30, 30, 255);
        // FloatWindow
        t.floatTitleBg     = Color(230, 230, 235, 255);
        t.floatTitleText   = Color(30, 30, 30, 255);
        t.floatBg          = Color(245, 245, 248, 255);
        t.floatBorder      = Color(190, 190, 195, 255);
        t.floatBtnHover    = Color(210, 210, 215, 255);
        t.floatCloseHover  = Color(220, 60, 60, 255);
        // Drawer
        t.drawerBg         = Color(245, 245, 248, 255);
        t.drawerBorder     = Color(200, 200, 205, 255);
        // Dialog
        t.dialogScrim      = Color(0, 0, 0, 80);
        t.dialogBg         = Color(250, 250, 252, 255);
        t.dialogBorder     = Color(190, 190, 195, 255);
        t.dialogTitleText  = Color(30, 30, 30, 255);
        t.dialogText       = Color(60, 60, 60, 255);
        t.dialogBtnBg      = Color(225, 225, 230, 255);
        t.dialogBtnHover   = Color(210, 210, 215, 255);
        t.dialogBtnPrimary = Color(0, 120, 215, 255);
        t.dialogBtnPrimaryHover = Color(30, 140, 225, 255);
        t.dialogBtnDanger    = Color(220, 60, 60, 255);
        t.dialogBtnDangerHover = Color(240, 80, 80, 255);
        // Editor gutter
        t.gutterBg         = Color(235, 235, 238, 255);
        t.lineNumberColor  = Color(140, 140, 145, 255);
        t.lineNumberActive = Color(30, 30, 30, 255);
        // Editor
        t.currentLineHighlight = Color(0, 0, 0, 12);
        t.scrollbarThumb   = Color(140, 140, 145, 100);
        // Carousel
        t.carouselArrowBg  = Color(255, 255, 255, 180);
        t.carouselArrowFg  = Color(30, 30, 30, 240);
        t.carouselDotActive  = Color(0, 120, 215, 255);
        t.carouselDotInactive = Color(180, 180, 185, 200);
        // Collapsible
        t.collapsibleHeaderBg = Color(230, 230, 235, 255);
        // Menu
        t.menuBarBg          = Color(240, 240, 240, 255);
        t.menuBarItemHover   = Color(210, 210, 215, 255);
        t.menuBg             = Color(250, 250, 250, 255);
        t.menuBorder         = Color(200, 200, 205, 255);
        t.menuItemHover      = Color(0, 120, 215, 255);
        t.menuItemText       = Color(30, 30, 30, 255);
        t.menuItemTextHover  = Color(255, 255, 255, 255);
        t.menuItemDisabled   = Color(160, 160, 165, 255);
        t.menuSeparator      = Color(210, 210, 215, 255);
        t.menuShortcutText   = Color(100, 100, 105, 255);
        t.menuShortcutHover  = Color(220, 220, 220, 255);
        t.menuCheckMark      = Color(0, 120, 215, 255);
        t.menuSubmenuArrow   = Color(60, 60, 65, 255);
        return t;
    }

    /// @brief Apply a theme preset to this instance.
    void apply(const Theme& preset)
    {
        *this = preset;
    }

    /// @brief Get the global Theme singleton.
    static Theme& instance()
    {
        static Theme theme;
        return theme;
    }
};

} // namespace BuGUI
