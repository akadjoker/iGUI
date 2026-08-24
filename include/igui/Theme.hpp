#pragma once

#include "Color.hpp"
#include "Types.hpp"

namespace ig
{

struct Theme
{
    FontId font;
    float fontSize;
    float titleBarHeight;
    float windowPadding;
    float widgetHeight;
    float sliderHeight;
    float itemSpacing;

    // Shared sizing fields used by the core immediate-mode controls.
    float padding;
    float borderRadius;
    float buttonHeight;
    float sliderTrackWidth;
    float inputHeight;
    float scrollbarWidth;
    float scrollbarMinThumb;
    float textEditPadding;
    float gutterPadding;
    float lineSpacing;
    float tooltipPadX;
    float tooltipPadY;
    float menuBarHeight;
    float menuItemHeight;
    float menuItemPadX;
    float menuMinWidth;
    float menuShadowSize;

    Color windowBackground;
    Color panelColor;
    Color borderColor;
    Color textColor;
    Color titleBarBackground;
    Color buttonBackground;
    Color buttonHovered;
    Color buttonPressed;
    Color buttonBorder;
    Color buttonText;
    Color checkboxBackground;
    Color checkboxChecked;
    Color radioBackground;
    Color radioChecked;
    Color selectableBackground;
    Color selectableHovered;
    Color selectableSelected;
    Color sliderBackground;
    Color sliderTrackHover;
    Color sliderTrackFocused;
    Color sliderFilled;
    Color sliderFillHover;
    Color sliderHandle;
    Color sliderThumbHover;
    Color sliderThumbPressed;
    Color focusColor;
    Color menuCheckMark;
    Color menuSubmenuArrow;
    Color progressBackground;
    Color progressFilled;
    Color labelText;

    Color bgColor;
    Color textDisabled;
    Color buttonNormal;
    Color buttonHover;
    Color selectionColor;
    Color sliderTrack;
    Color sliderFill;
    Color sliderThumb;
    Color inputBg;
    Color inputBgHover;
    Color inputBorder;
    Color inputBorderHover;
    Color checkMark;
    Color switchThumb;
    Color tooltipBg;
    Color tooltipBorder;
    Color tooltipText;
    Color floatTitleBg;
    Color floatTitleText;
    Color floatBg;
    Color floatBorder;
    Color floatBtnHover;
    Color floatCloseHover;
    Color drawerBg;
    Color drawerBorder;
    Color dialogScrim;
    Color dialogBg;
    Color dialogBorder;
    Color dialogTitleText;
    Color dialogText;
    Color dialogBtnBg;
    Color dialogBtnHover;
    Color dialogBtnPrimary;
    Color dialogBtnPrimaryHover;
    Color dialogBtnDanger;
    Color dialogBtnDangerHover;
    Color gutterBg;
    Color lineNumberColor;
    Color lineNumberActive;
    Color currentLineHighlight;
    Color scrollbarThumb;
    Color carouselArrowBg;
    Color carouselArrowFg;
    Color carouselDotActive;
    Color carouselDotInactive;
    Color collapsibleHeaderBg;
    Color menuBarBg;
    Color menuBarItemHover;
    Color menuBg;
    Color menuBorder;
    Color menuItemHover;
    Color menuItemText;
    Color menuItemTextHover;
    Color menuItemDisabled;
    Color menuSeparator;
    Color menuShortcutText;
    Color menuShortcutHover;

    Theme()
        : font(1), fontSize(16.0f), titleBarHeight(24.0f), windowPadding(8.0f),
          widgetHeight(28.0f), sliderHeight(20.0f), itemSpacing(6.0f),
          padding(6.0f), borderRadius(4.0f), buttonHeight(28.0f),
          sliderTrackWidth(4.0f), inputHeight(26.0f), scrollbarWidth(12.0f),
          scrollbarMinThumb(16.0f), textEditPadding(4.0f), gutterPadding(8.0f),
          lineSpacing(2.0f), tooltipPadX(8.0f), tooltipPadY(4.0f),
          menuBarHeight(26.0f), menuItemHeight(24.0f), menuItemPadX(16.0f),
          menuMinWidth(160.0f), menuShadowSize(4.0f),
          windowBackground(40, 40, 45, 255),
          panelColor(45, 45, 48, 255),
          borderColor(70, 70, 70, 255),
          textColor(220, 220, 220, 255),
          titleBarBackground(55, 55, 65, 255),
          buttonBackground(65, 65, 75, 255),
          buttonHovered(85, 85, 100, 255),
          buttonPressed(45, 45, 50, 255),
          buttonBorder(90, 90, 95, 255),
          buttonText(255, 255, 255, 255),
          checkboxBackground(55, 55, 65, 255),
          checkboxChecked(155, 155, 160, 255),
          radioBackground(55, 55, 65, 255),
          radioChecked(155, 155, 160, 255),
          selectableBackground(55, 55, 65, 255),
          selectableHovered(75, 75, 90, 255),
          selectableSelected(88, 88, 94, 255),
          sliderBackground(55, 55, 58, 255),
          sliderTrackHover(68, 68, 72, 255),
          sliderTrackFocused(75, 75, 80, 255),
          sliderFilled(165, 165, 170, 255),
          sliderFillHover(165, 165, 170, 255),
          sliderHandle(230, 230, 235, 255),
          sliderThumbHover(195, 195, 200, 255),
          sliderThumbPressed(220, 220, 225, 255),
          focusColor(190, 190, 195, 255),
          menuCheckMark(185, 185, 190, 255),
          menuSubmenuArrow(180, 180, 185, 255),
          progressBackground(55, 55, 65, 255),
          progressFilled(165, 165, 170, 255),
          labelText(230, 230, 230, 255),
          bgColor(30, 30, 30, 255), textDisabled(120, 120, 120, 255),
          buttonNormal(60, 60, 65, 255), buttonHover(75, 75, 80, 255),
          selectionColor(180, 180, 185, 128), sliderTrack(55, 55, 58, 255),
          sliderFill(140, 140, 145, 255), sliderThumb(160, 160, 165, 255),
          inputBg(35, 35, 38, 255), inputBgHover(48, 48, 52, 255),
          inputBorder(80, 80, 85, 255), inputBorderHover(110, 110, 118, 255),
          checkMark(200, 200, 205, 255), switchThumb(220, 220, 225, 255),
          tooltipBg(20, 20, 22, 230), tooltipBorder(90, 90, 95, 200),
          tooltipText(220, 220, 220, 255), floatTitleBg(50, 50, 55, 255),
          floatTitleText(210, 210, 210, 255), floatBg(38, 38, 40, 255),
          floatBorder(75, 75, 80, 255), floatBtnHover(70, 70, 75, 255),
          floatCloseHover(200, 60, 60, 255), drawerBg(40, 40, 44, 255),
          drawerBorder(70, 70, 75, 255), dialogScrim(0, 0, 0, 140),
          dialogBg(48, 48, 52, 255), dialogBorder(80, 80, 85, 255),
          dialogTitleText(220, 220, 225, 255), dialogText(190, 190, 195, 255),
          dialogBtnBg(65, 65, 70, 255), dialogBtnHover(80, 80, 85, 255),
          dialogBtnPrimary(118, 118, 125, 255), dialogBtnPrimaryHover(145, 145, 152, 255),
          dialogBtnDanger(180, 60, 60, 255), dialogBtnDangerHover(200, 80, 80, 255),
          gutterBg(38, 38, 42, 255), lineNumberColor(120, 120, 120, 255),
          lineNumberActive(220, 220, 220, 255), currentLineHighlight(255, 255, 255, 8),
          scrollbarThumb(120, 120, 120, 80), carouselArrowBg(0, 0, 0, 140),
          carouselArrowFg(230, 230, 235, 240), carouselDotActive(180, 180, 185, 255),
          carouselDotInactive(100, 100, 105, 180), collapsibleHeaderBg(50, 52, 58, 255),
          menuBarBg(45, 45, 48, 255), menuBarItemHover(65, 65, 70, 255),
          menuBg(45, 45, 48, 255), menuBorder(70, 70, 75, 255),
          menuItemHover(65, 65, 70, 255), menuItemText(220, 220, 220, 255),
          menuItemTextHover(255, 255, 255, 255), menuItemDisabled(120, 120, 120, 255),
          menuSeparator(70, 70, 75, 255), menuShortcutText(150, 150, 155, 255),
          menuShortcutHover(200, 200, 200, 255)
    {
    }
};

enum class ThemePreset : uint8_t
{
    Dark,
    Light,
    Blender,
    VSCode
};

inline Theme makeTheme(ThemePreset preset)
{
    Theme theme;
    if (preset == ThemePreset::Dark)
        return theme;

    if (preset == ThemePreset::Light)
    {
        theme.windowBackground = Color(245, 246, 248, 255);
        theme.panelColor = Color(235, 237, 241, 255);
        theme.borderColor = Color(186, 190, 198, 255);
        theme.textColor = Color(35, 38, 44, 255);
        theme.labelText = theme.textColor;
        theme.buttonText = theme.textColor;
        theme.buttonBackground = Color(224, 227, 232, 255);
        theme.buttonHovered = Color(207, 219, 235, 255);
        theme.buttonBorder = Color(170, 176, 188, 255);
        theme.selectableBackground = Color(247, 248, 250, 255);
        theme.selectableHovered = Color(221, 231, 244, 255);
        theme.selectableSelected = Color(111, 161, 214, 255);
        theme.inputBg = Color(255, 255, 255, 255);
        theme.inputBgHover = Color(247, 249, 252, 255);
        theme.inputBorder = Color(176, 182, 193, 255);
        theme.inputBorderHover = Color(91, 140, 197, 255);
        theme.focusColor = Color(54, 124, 196, 255);
        theme.sliderFilled = theme.focusColor;
        theme.checkboxChecked = theme.focusColor;
        theme.radioChecked = theme.focusColor;
        theme.titleBarBackground = Color(224, 227, 232, 255);
        theme.menuBarBg = Color(232, 234, 238, 255);
        theme.menuBg = Color(250, 251, 253, 255);
        theme.menuBorder = theme.borderColor;
        theme.menuItemText = theme.textColor;
        theme.menuItemTextHover = theme.textColor;
        theme.menuItemHover = Color(218, 229, 243, 255);
        theme.tooltipBg = Color(40, 44, 52, 245);
        theme.tooltipText = Color(245, 246, 248, 255);
        theme.scrollbarThumb = Color(116, 126, 142, 145);
        return theme;
    }

    if (preset == ThemePreset::Blender)
    {
        theme.windowBackground = Color(50, 50, 50, 255);
        theme.panelColor = Color(58, 58, 58, 255);
        theme.borderColor = Color(30, 30, 30, 255);
        theme.buttonBackground = Color(70, 70, 70, 255);
        theme.buttonHovered = Color(91, 91, 91, 255);
        theme.selectableBackground = Color(55, 55, 55, 255);
        theme.selectableHovered = Color(77, 88, 96, 255);
        theme.selectableSelected = Color(54, 124, 176, 255);
        theme.inputBg = Color(42, 42, 42, 255);
        theme.inputBorder = Color(22, 22, 22, 255);
        theme.inputBorderHover = Color(217, 119, 28, 255);
        theme.focusColor = Color(237, 135, 35, 255);
        theme.sliderFilled = theme.focusColor;
        theme.checkboxChecked = theme.focusColor;
        theme.radioChecked = theme.focusColor;
        theme.dialogBtnPrimary = theme.focusColor;
        theme.menuBarBg = Color(48, 48, 48, 255);
        theme.menuBg = Color(58, 58, 58, 255);
        theme.menuItemHover = Color(83, 83, 83, 255);
        return theme;
    }

    theme.windowBackground = Color(30, 30, 30, 255);
    theme.panelColor = Color(37, 37, 38, 255);
    theme.borderColor = Color(60, 60, 60, 255);
    theme.titleBarBackground = Color(37, 37, 38, 255);
    theme.buttonBackground = Color(51, 51, 55, 255);
    theme.buttonHovered = Color(67, 67, 72, 255);
    theme.selectableBackground = Color(37, 37, 38, 255);
    theme.selectableHovered = Color(45, 71, 95, 255);
    theme.selectableSelected = Color(9, 71, 113, 255);
    theme.inputBg = Color(30, 30, 30, 255);
    theme.inputBorder = Color(63, 63, 70, 255);
    theme.inputBorderHover = Color(0, 122, 204, 255);
    theme.focusColor = Color(0, 122, 204, 255);
    theme.sliderFilled = theme.focusColor;
    theme.checkboxChecked = theme.focusColor;
    theme.radioChecked = theme.focusColor;
    theme.menuBarBg = Color(45, 45, 48, 255);
    theme.menuBg = Color(37, 37, 38, 255);
    theme.menuBorder = Color(69, 69, 69, 255);
    theme.menuItemHover = Color(9, 71, 113, 255);
    theme.dialogBtnPrimary = theme.focusColor;
    return theme;
}

} // namespace ig
