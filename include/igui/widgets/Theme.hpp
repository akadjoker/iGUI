#pragma once

#include "RetainedBase.hpp"


namespace ig
{
namespace retained
{
// ═════════════════════════════════════════════════════════════════════════════
//  Theme - centralized colors/sizes for all widgets
// ═════════════════════════════════════════════════════════════════════════════

struct Theme
{
    // Window / Panel
    Color bgColor = Color(30, 30, 30, 255);
    Color panelColor = Color(45, 45, 48, 255);
    Color borderColor = Color(70, 70, 70, 255);

    // Text
    Color textColor = Color(220, 220, 220, 255);
    Color textDisabled = Color(120, 120, 120, 255);

    // Button
    Color buttonNormal = Color(60, 60, 65, 255);
    Color buttonHover = Color(75, 75, 80, 255);
    Color buttonPressed = Color(45, 45, 50, 255);
    Color buttonBorder = Color(90, 90, 95, 255);

    // Focus / selection
    Color focusColor = Color(180, 180, 185, 255);
    Color selectionColor = Color(180, 180, 185, 128);

    // Slider
    Color sliderTrack = Color(55, 55, 58, 255);
    Color sliderTrackHover = Color(68, 68, 72, 255);
    Color sliderTrackFocused = Color(75, 75, 80, 255);
    Color sliderFill = Color(140, 140, 145, 255);
    Color sliderFillHover = Color(165, 165, 170, 255);
    Color sliderThumb = Color(160, 160, 165, 255);
    Color sliderThumbHover = Color(195, 195, 200, 255);
    Color sliderThumbPressed = Color(220, 220, 225, 255);

    // Input (checkbox box, text input bg)
    Color inputBg = Color(35, 35, 38, 255);
    Color inputBgHover = Color(48, 48, 52, 255);
    Color inputBorder = Color(80, 80, 85, 255);
    Color inputBorderHover = Color(110, 110, 118, 255);
    Color checkMark = Color(200, 200, 205, 255);

    // Switch
    Color switchThumb = Color(220, 220, 225, 255);

    // Tooltip
    Color tooltipBg = Color(20, 20, 22, 230);
    Color tooltipBorder = Color(90, 90, 95, 200);
    Color tooltipText = Color(220, 220, 220, 255);

    // FloatWindow
    Color floatTitleBg = Color(50, 50, 55, 255);
    Color floatTitleText = Color(210, 210, 210, 255);
    Color floatBg = Color(38, 38, 40, 255);
    Color floatBorder = Color(75, 75, 80, 255);
    Color floatBtnHover = Color(70, 70, 75, 255);
    Color floatCloseHover = Color(200, 60, 60, 255);

    // SidePanel / Drawer
    Color drawerBg = Color(40, 40, 44, 255);
    Color drawerBorder = Color(70, 70, 75, 255);

    // Dialog
    Color dialogScrim = Color(0, 0, 0, 140);
    Color dialogBg = Color(48, 48, 52, 255);
    Color dialogBorder = Color(80, 80, 85, 255);
    Color dialogTitleText = Color(220, 220, 225, 255);
    Color dialogText = Color(190, 190, 195, 255);
    Color dialogBtnBg = Color(65, 65, 70, 255);
    Color dialogBtnHover = Color(80, 80, 85, 255);
    Color dialogBtnPrimary = Color(70, 130, 180, 255);
    Color dialogBtnPrimaryHover = Color(90, 150, 200, 255);
    Color dialogBtnDanger = Color(180, 60, 60, 255);
    Color dialogBtnDangerHover = Color(200, 80, 80, 255);

    // Editor gutter (line numbers)
    Color gutterBg = Color(38, 38, 42, 255);
    Color lineNumberColor = Color(120, 120, 120, 255);
    Color lineNumberActive = Color(220, 220, 220, 255);

    // Editor
    Color currentLineHighlight = Color(255, 255, 255, 8);
    Color scrollbarThumb = Color(120, 120, 120, 80);

    // Carousel
    Color carouselArrowBg = Color(0, 0, 0, 140);
    Color carouselArrowFg = Color(230, 230, 235, 240);
    Color carouselDotActive = Color(100, 160, 220, 255);
    Color carouselDotInactive = Color(100, 100, 105, 180);

    // Collapsible
    Color collapsibleHeaderBg = Color(50, 52, 58, 255);

    // Menu
    Color menuBarBg = Color(45, 45, 48, 255);
    Color menuBarItemHover = Color(65, 65, 70, 255);
    Color menuBg = Color(45, 45, 48, 255);
    Color menuBorder = Color(70, 70, 75, 255);
    Color menuItemHover = Color(65, 65, 70, 255);
    Color menuItemText = Color(220, 220, 220, 255);
    Color menuItemTextHover = Color(255, 255, 255, 255);
    Color menuItemDisabled = Color(120, 120, 120, 255);
    Color menuSeparator = Color(70, 70, 75, 255);
    Color menuShortcutText = Color(150, 150, 155, 255);
    Color menuShortcutHover = Color(200, 200, 200, 255);
    Color menuCheckMark = Color(100, 160, 220, 255);
    Color menuSubmenuArrow = Color(180, 180, 185, 255);

    // SideTabBar (the rail of icon buttons and the page beside it)
    Color sideTabRailBg = Color(28, 29, 33, 255);
    Color sideTabPageBg = Color(38, 40, 44, 255);
    Color sideTabHoverBg = Color(50, 52, 58, 255);
    Color sideTabActiveBg = Color(32, 48, 52, 255);
    Color sideTabAccent = Color(70, 200, 190, 255);
    Color sideTabText = Color(150, 152, 158, 255);
    Color sideTabTextActive = Color(220, 240, 238, 255);
    Color sideTabBadge = Color(255, 160, 40, 255);

    // Look: how some controls are drawn beyond their colours. The dark and
    // light presets leave these at their defaults, so they paint as they
    // always did; xp and macos switch them on.
    Color progressFill = Color(140, 140, 145, 255); // the bar of a ProgressBar
    Color progressTrack = Color(0, 0, 0, 0);        // alpha 0: sliderTrack
    bool progressBlocks = false;                    // a row of small blocks instead of a smooth bar
    float controlGradient = 0.0f;                   // button faces shade from lighter (top) to darker (bottom) by this much, 0..1; 0 is flat
    Color buttonHoverRing = Color(0, 0, 0, 0);      // alpha 0: none; else a ring inside the border of a hovered button
    Color dialogTitleBg = Color(0, 0, 0, 0);        // alpha 0: panelColor; the title bar of a Dialog
    float titleGradient = 0.0f;                     // title bars (Dialog, FloatWindow) shade from lighter to darker by this much, 0..1
    Color tabAccent = Color(60, 140, 255, 255);     // the line on top of the selected tab (zenforms Tabs)
    bool sliderThumbBox = false;                    // the Slider's thumb is a small box, not a circle

    /// @brief @p c moved toward @p target (0 black .. 255 white) by @p k (0..1).
    static Color shade(const Color& c, int target, float k)
    {
        return Color(static_cast<unsigned char>(c.r + (target - c.r) * k),
                static_cast<unsigned char>(c.g + (target - c.g) * k),
                static_cast<unsigned char>(c.b + (target - c.b) * k), c.a);
    }

    // Sizes
    float fontSize = 16.0f;
    float padding = 6.0f;
    float borderRadius = 4.0f;
    float buttonHeight = 28.0f;
    float sliderHeight = 20.0f;
    float sliderTrackWidth = 4.0f;
    float sliderThumbRadius = 6.5f; // the round thumb of a Slider; never more than half of the slider's thickness
    float inputHeight = 26.0f;
    float scrollbarWidth = 12.0f;
    float scrollbarMinThumb = 16.0f;
    float textEditPadding = 4.0f;
    float gutterPadding = 8.0f;
    float lineSpacing = 2.0f;
    float tooltipPadX = 8.0f;
    float tooltipPadY = 4.0f;
    float menuBarHeight = 26.0f;
    float menuItemHeight = 24.0f;
    float menuItemPadX = 16.0f;
    float menuMinWidth = 160.0f;
    float menuShadowSize = 4.0f;

    // ── Theme presets ─────────────────────────────────────────────────────

    /// @brief Get the dark theme preset (default).
    static Theme dark()
    {
        return Theme{}; // defaults are the dark theme
    }

    /// @brief Get the light theme preset.
    static Theme light()
    {
        Theme t;
        // Window / Panel
        t.bgColor = Color(240, 240, 240, 255);
        t.panelColor = Color(250, 250, 250, 255);
        t.borderColor = Color(200, 200, 200, 255);
        // Text
        t.textColor = Color(30, 30, 30, 255);
        t.textDisabled = Color(140, 140, 140, 255);
        // Button
        t.buttonNormal = Color(225, 225, 228, 255);
        t.buttonHover = Color(210, 210, 215, 255);
        t.buttonPressed = Color(195, 195, 200, 255);
        t.buttonBorder = Color(180, 180, 185, 255);
        // Focus / selection
        t.focusColor = Color(0, 120, 215, 255);
        t.selectionColor = Color(0, 120, 215, 80);
        // Slider
        t.sliderTrack = Color(210, 210, 215, 255);
        t.sliderTrackHover = Color(195, 195, 200, 255);
        t.sliderTrackFocused = Color(180, 180, 185, 255);
        t.sliderFill = Color(0, 120, 215, 255);
        t.sliderFillHover = Color(30, 140, 225, 255);
        t.sliderThumb = Color(0, 120, 215, 255);
        t.sliderThumbHover = Color(30, 140, 225, 255);
        t.sliderThumbPressed = Color(0, 90, 180, 255);
        // Input
        t.inputBg = Color(255, 255, 255, 255);
        t.inputBgHover = Color(255, 255, 255, 255);
        t.inputBorder = Color(180, 180, 185, 255);
        t.inputBorderHover = Color(0, 120, 215, 255);
        t.checkMark = Color(0, 120, 215, 255);
        // Switch
        t.switchThumb = Color(255, 255, 255, 255);
        // Tooltip
        t.tooltipBg = Color(255, 255, 255, 240);
        t.tooltipBorder = Color(180, 180, 185, 255);
        t.tooltipText = Color(30, 30, 30, 255);
        // FloatWindow
        t.floatTitleBg = Color(230, 230, 235, 255);
        t.floatTitleText = Color(30, 30, 30, 255);
        t.floatBg = Color(245, 245, 248, 255);
        t.floatBorder = Color(190, 190, 195, 255);
        t.floatBtnHover = Color(210, 210, 215, 255);
        t.floatCloseHover = Color(220, 60, 60, 255);
        // Drawer
        t.drawerBg = Color(245, 245, 248, 255);
        t.drawerBorder = Color(200, 200, 205, 255);
        // Dialog
        t.dialogScrim = Color(0, 0, 0, 80);
        t.dialogBg = Color(250, 250, 252, 255);
        t.dialogBorder = Color(190, 190, 195, 255);
        t.dialogTitleText = Color(30, 30, 30, 255);
        t.dialogText = Color(60, 60, 60, 255);
        t.dialogBtnBg = Color(225, 225, 230, 255);
        t.dialogBtnHover = Color(210, 210, 215, 255);
        t.dialogBtnPrimary = Color(0, 120, 215, 255);
        t.dialogBtnPrimaryHover = Color(30, 140, 225, 255);
        t.dialogBtnDanger = Color(220, 60, 60, 255);
        t.dialogBtnDangerHover = Color(240, 80, 80, 255);
        // Editor gutter
        t.gutterBg = Color(235, 235, 238, 255);
        t.lineNumberColor = Color(140, 140, 145, 255);
        t.lineNumberActive = Color(30, 30, 30, 255);
        // Editor
        t.currentLineHighlight = Color(0, 0, 0, 12);
        t.scrollbarThumb = Color(140, 140, 145, 100);
        // Carousel
        t.carouselArrowBg = Color(255, 255, 255, 180);
        t.carouselArrowFg = Color(30, 30, 30, 240);
        t.carouselDotActive = Color(0, 120, 215, 255);
        t.carouselDotInactive = Color(180, 180, 185, 200);
        // Collapsible
        t.collapsibleHeaderBg = Color(230, 230, 235, 255);
        // Menu
        t.menuBarBg = Color(240, 240, 240, 255);
        t.menuBarItemHover = Color(210, 210, 215, 255);
        t.menuBg = Color(250, 250, 250, 255);
        t.menuBorder = Color(200, 200, 205, 255);
        t.menuItemHover = Color(0, 120, 215, 255);
        t.menuItemText = Color(30, 30, 30, 255);
        t.menuItemTextHover = Color(255, 255, 255, 255);
        t.menuItemDisabled = Color(160, 160, 165, 255);
        t.menuSeparator = Color(210, 210, 215, 255);
        t.menuShortcutText = Color(100, 100, 105, 255);
        t.menuShortcutHover = Color(220, 220, 220, 255);
        t.menuCheckMark = Color(0, 120, 215, 255);
        t.menuSubmenuArrow = Color(60, 60, 65, 255);
        // SideTabBar
        t.sideTabRailBg = Color(232, 232, 236, 255);
        t.sideTabPageBg = Color(248, 248, 250, 255);
        t.sideTabHoverBg = Color(218, 218, 224, 255);
        t.sideTabActiveBg = Color(210, 236, 234, 255);
        t.sideTabAccent = Color(0, 140, 130, 255);
        t.sideTabText = Color(90, 90, 95, 255);
        t.sideTabTextActive = Color(0, 100, 95, 255);
        t.sideTabBadge = Color(235, 130, 20, 255);
        return t;
    }

    /// @brief A light macOS look (Big Sur and later): light grey windows,
    ///        white controls with soft borders, the system blue as accent.
    static Theme macOS()
    {
        Theme t = light();
        const Color accent(0, 122, 255, 255);
        const Color accentHover(30, 140, 255, 255);
        // Window / Panel
        t.bgColor = Color(236, 236, 236, 255);
        t.panelColor = Color(246, 246, 246, 255);
        t.borderColor = Color(214, 214, 214, 255);
        // Text
        t.textColor = Color(34, 34, 34, 255);
        t.textDisabled = Color(170, 170, 170, 255);
        // Button
        t.buttonNormal = Color(255, 255, 255, 255);
        t.buttonHover = Color(250, 250, 250, 255);
        t.buttonPressed = Color(228, 228, 230, 255);
        t.buttonBorder = Color(208, 208, 208, 255);
        // Focus / selection
        t.focusColor = accent;
        t.selectionColor = Color(0, 122, 255, 70);
        // Slider
        t.sliderTrack = Color(206, 206, 210, 255);
        t.sliderTrackHover = Color(198, 198, 203, 255);
        t.sliderTrackFocused = Color(198, 198, 203, 255);
        t.sliderFill = accent;
        t.sliderFillHover = accentHover;
        t.sliderThumb = Color(255, 255, 255, 255);
        t.sliderThumbHover = Color(255, 255, 255, 255);
        t.sliderThumbPressed = Color(236, 236, 238, 255);
        // Input
        t.inputBg = Color(255, 255, 255, 255);
        t.inputBgHover = Color(255, 255, 255, 255);
        t.inputBorder = Color(208, 208, 208, 255);
        t.inputBorderHover = Color(170, 170, 176, 255);
        t.checkMark = accent;
        // Switch
        t.switchThumb = Color(255, 255, 255, 255);
        // Tooltip
        t.tooltipBg = Color(246, 246, 246, 245);
        t.tooltipBorder = Color(200, 200, 200, 255);
        t.tooltipText = Color(34, 34, 34, 255);
        // FloatWindow
        t.floatTitleBg = Color(228, 228, 228, 255);
        t.floatTitleText = Color(34, 34, 34, 255);
        t.floatBg = Color(246, 246, 246, 255);
        t.floatBorder = Color(190, 190, 190, 255);
        t.floatBtnHover = Color(212, 212, 214, 255);
        t.floatCloseHover = Color(255, 95, 87, 255);
        // Drawer
        t.drawerBg = Color(240, 240, 240, 255);
        t.drawerBorder = Color(210, 210, 210, 255);
        // Dialog
        t.dialogScrim = Color(0, 0, 0, 70);
        t.dialogBg = Color(246, 246, 246, 255);
        t.dialogBorder = Color(196, 196, 196, 255);
        t.dialogTitleText = Color(34, 34, 34, 255);
        t.dialogText = Color(60, 60, 60, 255);
        t.dialogBtnBg = Color(255, 255, 255, 255);
        t.dialogBtnHover = Color(240, 240, 240, 255);
        t.dialogBtnPrimary = accent;
        t.dialogBtnPrimaryHover = accentHover;
        t.dialogBtnDanger = Color(255, 59, 48, 255);
        t.dialogBtnDangerHover = Color(255, 89, 78, 255);
        // Editor gutter
        t.gutterBg = Color(240, 240, 240, 255);
        t.lineNumberColor = Color(160, 160, 165, 255);
        t.lineNumberActive = Color(34, 34, 34, 255);
        // Editor
        t.currentLineHighlight = Color(0, 0, 0, 10);
        t.scrollbarThumb = Color(0, 0, 0, 90);
        // Carousel
        t.carouselDotActive = accent;
        t.carouselDotInactive = Color(190, 190, 195, 200);
        // Collapsible
        t.collapsibleHeaderBg = Color(228, 228, 230, 255);
        // Menu
        t.menuBarBg = Color(236, 236, 236, 255);
        t.menuBarItemHover = Color(208, 208, 212, 255);
        t.menuBg = Color(246, 246, 246, 250);
        t.menuBorder = Color(206, 206, 206, 255);
        t.menuItemHover = accent;
        t.menuItemText = Color(34, 34, 34, 255);
        t.menuItemTextHover = Color(255, 255, 255, 255);
        t.menuItemDisabled = Color(170, 170, 170, 255);
        t.menuSeparator = Color(220, 220, 220, 255);
        t.menuShortcutText = Color(120, 120, 125, 255);
        t.menuShortcutHover = Color(225, 235, 255, 255);
        t.menuCheckMark = accent;
        t.menuSubmenuArrow = Color(90, 90, 95, 255);
        // SideTabBar
        t.sideTabRailBg = Color(228, 228, 230, 255);
        t.sideTabPageBg = Color(246, 246, 246, 255);
        t.sideTabHoverBg = Color(214, 214, 218, 255);
        t.sideTabActiveBg = Color(214, 228, 250, 255);
        t.sideTabAccent = accent;
        t.sideTabText = Color(100, 100, 105, 255);
        t.sideTabTextActive = Color(0, 90, 200, 255);
        t.sideTabBadge = Color(255, 59, 48, 255);
        // Look
        t.progressFill = accent;
        t.progressTrack = Color(214, 214, 218, 255);
        t.tabAccent = accent;
        // Sizes
        t.fontSize = 14.0f;
        t.borderRadius = 6.0f;
        t.buttonHeight = 28.0f;
        t.inputHeight = 28.0f;
        t.scrollbarWidth = 10.0f;
        t.menuBarHeight = 24.0f;
        t.menuItemHeight = 24.0f;
        t.menuShadowSize = 8.0f;
        return t;
    }

    /// @brief The macOS look in its dark appearance.
    static Theme macOSDark()
    {
        Theme t = macOS();
        const Color accent(10, 132, 255, 255);
        const Color accentHover(50, 152, 255, 255);
        // Window / Panel
        t.bgColor = Color(40, 40, 42, 255);
        t.panelColor = Color(30, 30, 32, 255);
        t.borderColor = Color(66, 66, 70, 255);
        // Text
        t.textColor = Color(232, 232, 234, 255);
        t.textDisabled = Color(120, 120, 126, 255);
        // Button
        t.buttonNormal = Color(84, 84, 88, 255);
        t.buttonHover = Color(98, 98, 102, 255);
        t.buttonPressed = Color(70, 70, 74, 255);
        t.buttonBorder = Color(54, 54, 58, 255);
        // Focus / selection
        t.focusColor = accent;
        t.selectionColor = Color(10, 132, 255, 110);
        // Slider
        t.sliderTrack = Color(78, 78, 82, 255);
        t.sliderTrackHover = Color(88, 88, 92, 255);
        t.sliderTrackFocused = Color(88, 88, 92, 255);
        t.sliderFill = accent;
        t.sliderFillHover = accentHover;
        t.sliderThumb = Color(236, 236, 238, 255);
        t.sliderThumbHover = Color(246, 246, 248, 255);
        t.sliderThumbPressed = Color(210, 210, 214, 255);
        // Input
        t.inputBg = Color(30, 30, 32, 255);
        t.inputBgHover = Color(36, 36, 38, 255);
        t.inputBorder = Color(76, 76, 80, 255);
        t.inputBorderHover = Color(110, 110, 116, 255);
        t.checkMark = accent;
        // Switch
        t.switchThumb = Color(240, 240, 242, 255);
        // Tooltip
        t.tooltipBg = Color(54, 54, 56, 245);
        t.tooltipBorder = Color(80, 80, 84, 255);
        t.tooltipText = Color(232, 232, 234, 255);
        // FloatWindow
        t.floatTitleBg = Color(58, 58, 60, 255);
        t.floatTitleText = Color(232, 232, 234, 255);
        t.floatBg = Color(40, 40, 42, 255);
        t.floatBorder = Color(76, 76, 80, 255);
        t.floatBtnHover = Color(84, 84, 88, 255);
        // Drawer
        t.drawerBg = Color(36, 36, 38, 255);
        t.drawerBorder = Color(66, 66, 70, 255);
        // Dialog
        t.dialogScrim = Color(0, 0, 0, 120);
        t.dialogBg = Color(44, 44, 46, 255);
        t.dialogBorder = Color(80, 80, 84, 255);
        t.dialogTitleText = Color(240, 240, 242, 255);
        t.dialogText = Color(204, 204, 208, 255);
        t.dialogBtnBg = Color(84, 84, 88, 255);
        t.dialogBtnHover = Color(98, 98, 102, 255);
        t.dialogBtnPrimary = accent;
        t.dialogBtnPrimaryHover = accentHover;
        t.dialogBtnDanger = Color(255, 69, 58, 255);
        t.dialogBtnDangerHover = Color(255, 99, 88, 255);
        // Editor gutter
        t.gutterBg = Color(34, 34, 36, 255);
        t.lineNumberColor = Color(120, 120, 126, 255);
        t.lineNumberActive = Color(232, 232, 234, 255);
        // Editor
        t.currentLineHighlight = Color(255, 255, 255, 10);
        t.scrollbarThumb = Color(255, 255, 255, 90);
        // Carousel
        t.carouselArrowBg = Color(0, 0, 0, 140);
        t.carouselArrowFg = Color(236, 236, 238, 240);
        t.carouselDotActive = accent;
        t.carouselDotInactive = Color(100, 100, 106, 180);
        // Collapsible
        t.collapsibleHeaderBg = Color(54, 54, 56, 255);
        // Menu
        t.menuBarBg = Color(40, 40, 42, 255);
        t.menuBarItemHover = Color(70, 70, 74, 255);
        t.menuBg = Color(46, 46, 48, 250);
        t.menuBorder = Color(76, 76, 80, 255);
        t.menuItemHover = accent;
        t.menuItemText = Color(232, 232, 234, 255);
        t.menuItemDisabled = Color(120, 120, 126, 255);
        t.menuSeparator = Color(72, 72, 76, 255);
        t.menuShortcutText = Color(150, 150, 156, 255);
        t.menuShortcutHover = Color(225, 235, 255, 255);
        t.menuCheckMark = accent;
        t.menuSubmenuArrow = Color(180, 180, 186, 255);
        // SideTabBar
        t.sideTabRailBg = Color(34, 34, 36, 255);
        t.sideTabPageBg = Color(44, 44, 46, 255);
        t.sideTabHoverBg = Color(58, 58, 60, 255);
        t.sideTabActiveBg = Color(28, 52, 86, 255);
        t.sideTabAccent = accent;
        t.sideTabText = Color(160, 160, 166, 255);
        t.sideTabTextActive = Color(214, 230, 255, 255);
        t.sideTabBadge = Color(255, 69, 58, 255);
        // Look
        t.progressFill = accent;
        t.progressTrack = Color(78, 78, 82, 255);
        t.tabAccent = accent;
        return t;
    }

    /// @brief A Windows XP (Luna, blue) look: beige windows, a blue title
    ///        bar and selection, white inputs with a blue-grey border, a
    ///        green check mark.
    static Theme windowsXP()
    {
        Theme t = light();
        const Color blue(49, 106, 197, 255);
        const Color blueLight(80, 130, 220, 255);
        const Color face(236, 233, 216, 255);
        const Color edge(172, 168, 153, 255);
        // Window / Panel
        t.bgColor = face;
        t.panelColor = face;
        t.borderColor = edge;
        // Text
        t.textColor = Color(0, 0, 0, 255);
        t.textDisabled = Color(161, 161, 146, 255);
        // Button
        t.buttonNormal = Color(236, 235, 230, 255);
        t.buttonHover = Color(244, 243, 238, 255);
        t.buttonPressed = Color(214, 210, 194, 255);
        t.buttonBorder = Color(0, 60, 116, 255);
        // Focus / selection
        t.focusColor = blue;
        t.selectionColor = Color(49, 106, 197, 110);
        // Slider
        t.sliderTrack = Color(200, 197, 182, 255);
        t.sliderTrackHover = Color(190, 187, 172, 255);
        t.sliderTrackFocused = Color(190, 187, 172, 255);
        t.sliderFill = Color(200, 197, 182, 255);
        t.sliderFillHover = Color(190, 187, 172, 255);
        t.sliderThumb = Color(236, 235, 230, 255);
        t.sliderThumbHover = Color(255, 240, 207, 255);
        t.sliderThumbPressed = Color(214, 210, 194, 255);
        // Input
        t.inputBg = Color(255, 255, 255, 255);
        t.inputBgHover = Color(255, 255, 255, 255);
        t.inputBorder = Color(127, 157, 185, 255);
        t.inputBorderHover = Color(49, 106, 197, 255);
        t.checkMark = Color(33, 161, 33, 255);
        // Switch
        t.switchThumb = Color(255, 255, 255, 255);
        // Tooltip
        t.tooltipBg = Color(255, 255, 225, 255);
        t.tooltipBorder = Color(0, 0, 0, 255);
        t.tooltipText = Color(0, 0, 0, 255);
        // FloatWindow
        t.floatTitleBg = Color(0, 84, 227, 255);
        t.floatTitleText = Color(255, 255, 255, 255);
        t.floatBg = face;
        t.floatBorder = Color(0, 60, 215, 255);
        t.floatBtnHover = blueLight;
        t.floatCloseHover = Color(224, 80, 50, 255);
        // Drawer
        t.drawerBg = face;
        t.drawerBorder = edge;
        // Dialog
        t.dialogScrim = Color(0, 0, 0, 100);
        t.dialogBg = face;
        t.dialogBorder = Color(0, 60, 215, 255);
        t.dialogTitleText = Color(255, 255, 255, 255);
        t.dialogText = Color(0, 0, 0, 255);
        t.dialogBtnBg = Color(236, 235, 230, 255);
        t.dialogBtnHover = Color(244, 243, 238, 255);
        t.dialogBtnPrimary = blue;
        t.dialogBtnPrimaryHover = blueLight;
        t.dialogBtnDanger = Color(200, 60, 40, 255);
        t.dialogBtnDangerHover = Color(224, 80, 60, 255);
        // Editor gutter
        t.gutterBg = face;
        t.lineNumberColor = Color(130, 128, 118, 255);
        t.lineNumberActive = Color(0, 0, 0, 255);
        // Editor
        t.currentLineHighlight = Color(49, 106, 197, 26);
        t.scrollbarThumb = Color(130, 160, 225, 210);
        // Carousel
        t.carouselDotActive = blue;
        t.carouselDotInactive = edge;
        // Collapsible
        t.collapsibleHeaderBg = Color(198, 211, 247, 255);
        // Menu
        t.menuBarBg = face;
        t.menuBarItemHover = blue;
        t.menuBg = Color(255, 255, 255, 255);
        t.menuBorder = Color(151, 151, 151, 255);
        t.menuItemHover = blue;
        t.menuItemText = Color(0, 0, 0, 255);
        t.menuItemTextHover = Color(255, 255, 255, 255);
        t.menuItemDisabled = Color(161, 161, 146, 255);
        t.menuSeparator = edge;
        t.menuShortcutText = Color(90, 90, 80, 255);
        t.menuShortcutHover = Color(255, 255, 255, 255);
        t.menuCheckMark = Color(0, 0, 0, 255);
        t.menuSubmenuArrow = Color(0, 0, 0, 255);
        // SideTabBar
        t.sideTabRailBg = Color(214, 223, 247, 255);
        t.sideTabPageBg = face;
        t.sideTabHoverBg = Color(198, 211, 247, 255);
        t.sideTabActiveBg = Color(255, 255, 255, 255);
        t.sideTabAccent = blue;
        t.sideTabText = Color(33, 93, 198, 255);
        t.sideTabTextActive = Color(0, 40, 130, 255);
        t.sideTabBadge = Color(224, 80, 50, 255);
        // Look
        t.progressFill = Color(60, 205, 60, 255);
        t.progressTrack = Color(255, 255, 255, 255);
        t.progressBlocks = true;
        t.controlGradient = 0.8f;
        t.buttonHoverRing = Color(250, 195, 90, 255);
        t.dialogTitleBg = Color(0, 84, 227, 255);
        t.titleGradient = 0.35f;
        t.tabAccent = Color(250, 195, 90, 255);
        t.sliderThumbBox = true;
        // Sizes
        t.fontSize = 14.0f;
        t.borderRadius = 3.0f;
        t.buttonHeight = 24.0f;
        t.inputHeight = 24.0f;
        t.scrollbarWidth = 16.0f;
        t.menuBarHeight = 22.0f;
        t.menuItemHeight = 22.0f;
        t.menuShadowSize = 2.0f;
        return t;
    }

    /// @brief The names byName() accepts, in the order a picker shows them.
    static constexpr int kPresetCount = 5;
    static const char* presetName(int index)
    {
        static const char* const names[kPresetCount] = {"dark", "light", "xp", "macos", "macos-dark"};
        return index >= 0 && index < kPresetCount ? names[index] : "";
    }

    /// @brief A preset by name (see presetName); an unknown name gives the dark one.
    static Theme byName(const char* name)
    {
        const auto same = [name](const char* other) {
            const char* a = name ? name : "";
            while (*a && *a == *other)
            {
                ++a;
                ++other;
            }
            return *a == *other;
        };
        if (same("light")) return light();
        if (same("xp")) return windowsXP();
        if (same("macos")) return macOS();
        if (same("macos-dark")) return macOSDark();
        return dark();
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

} // namespace retained
} // namespace ig
