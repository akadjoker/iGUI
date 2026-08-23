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

    Color windowBackground;
    Color titleBarBackground;
    Color buttonBackground;
    Color buttonHovered;
    Color buttonText;
    Color checkboxBackground;
    Color checkboxChecked;
    Color labelText;

    Theme()
        : font(1), fontSize(14.0f), titleBarHeight(24.0f), windowPadding(8.0f),
          windowBackground(40, 40, 45, 255),
          titleBarBackground(55, 55, 65, 255),
          buttonBackground(65, 65, 75, 255),
          buttonHovered(85, 85, 100, 255),
          buttonText(255, 255, 255, 255),
          checkboxBackground(55, 55, 65, 255),
          checkboxChecked(90, 160, 230, 255),
          labelText(230, 230, 230, 255)
    {
    }
};

} // namespace ig
