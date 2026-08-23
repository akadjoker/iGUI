#pragma once

#include "Config.hpp"
#include "Math.hpp"
#include "Color.hpp"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

class RenderBatch;
class Font;

 
 

enum class Orientation
{
    Horizontal,
    Vertical
};

// ========================================
// GUI Theme / Color Scheme
// ========================================
struct GUITheme
{
    Color accent = Color(90, 90, 90, 255);

    Color windowBg = Color(40, 40, 45, 240);
    Color windowBorder = Color(70, 70, 75, 255);
    Color titleBarBg = Color(50, 50, 55, 255);
    Color titleBarActive = Color(60, 60, 65, 255);
    Color titleText = Color(255, 255, 255, 255);

    Color buttonBg = Color(60, 60, 65, 255);
    Color buttonHover = Color(70, 70, 80, 255);
    Color buttonActive = accent;
    Color buttonText = Color(255, 255, 255, 255);

    Color sliderBg = Color(50, 50, 55, 255);
    Color sliderFill = accent;
    Color sliderHandle = Color(190, 190, 195, 255);
    Color sliderHandleHover = Color(230, 230, 235, 255);

    Color checkboxBg = Color(50, 50, 55, 255);
    Color checkboxCheck = accent;
    Color checkboxBorder = Color(100, 100, 105, 255);

    Color inputBg = Color(30, 30, 35, 255);
    Color inputBorder = Color(70, 70, 75, 255);
    Color inputText = Color(255, 255, 255, 255);
    Color inputCursor = accent;

    Color labelText = Color(220, 220, 220, 255);
    Color separatorColor = Color(70, 70, 75, 255);

    float titleBarHeight = 28.0f;
    float windowBorderSize = 1.0f;
    float windowPadding = 8.0f;
    float itemSpacing = 4.0f;
    float scrollbarSize = 12.0f;

    float buttonHeight = 24.0f;
    float sliderHeight = 20.0f;
    float inputHeight = 24.0f;
    float checkboxSize = 16.0f;

    float cornerRadius = 3.0f;

    Color progressBarBg = Color(50, 50, 55, 255);
    Color progressBarFill = Color(90, 90, 90, 255);

    Color dropdownBg = Color(40, 40, 40, 255);
    Color dropdownHover = Color(70, 70, 80, 255);
    Color dropdownBorder = Color(100, 100, 105, 255);
    Color dropdownItemHover = Color(70, 70, 80, 255);

    Color radioBg = Color(50, 50, 55, 255);
    Color radioCheck = accent;
    Color radioBorder = Color(100, 100, 105, 255);

    Color colorPickerBorder = Color(100, 100, 105, 255);

    Color listBoxBg = Color(40, 40, 45, 255);
    Color listBoxItemHover = Color(70, 70, 80, 255);
    Color listBoxItemSelected = accent;
    Color listBoxBorder = Color(100, 100, 105, 255);

    Color textInputBg = Color(30, 30, 35, 255);
    Color textInputBorder = Color(100, 100, 105, 255);
    Color textInputText = Color(255, 255, 255, 255);
    Color textInputCursor = accent;
    Color textInputSelection = Color(70, 160, 190, 128);
    float textInputHeight = 24.0f;

    float dropdownHeight = 24.0f;
    float radioSize = 14.0f;
};

// ========================================
// GUI State
// ========================================
enum class WidgetState
{
    Normal,
    Hovered,
    Active,
    Disabled
};

struct WindowData
{
    std::string id;
    FloatRect bounds{};
    bool isOpen = true;
    bool isMinimized = false;
    bool isDragging = false;
    bool isResizing = false;
    bool isFocused = false;

    Vec2 dragOffset{};
    Vec2 scrollOffset{};
    float contentHeight = 0.0f;
    int zOrder = 0;

    // Window flags
    bool canClose = true;
    bool canMinimize = true;
    bool canResize = true;
    bool canMove = true;
    bool hasScrollbar = true;
};

// ========================================
// GUI Class - Immediate Mode GUI
// ========================================
class GUI
{
public:
    GUI();
    ~GUI();

    // Init / Release
    void Init(RenderBatch *batch, Font *font);
    void Release();

    // Frame
    bool BeginFrame();
    void EndFrame();

    bool IsFocused();

    // Windows
    bool BeginWindow(const char *title, float x, float y, float width, float height, bool *open = nullptr);
    bool BeginWindow(const char *title, const FloatRect &bounds, bool *open = nullptr);
    void EndWindow();
    void SetWindowFlags(bool canClose, bool canMinimize, bool canResize, bool canMove);

    bool Button(const char *label, float x, float y, float w, float h);
    bool Checkbox(const char *label, bool *value, float x, float y, float size);

    bool SliderFloat(const char *label, float *value, float min, float max,
                     float x, float y, float w, float h);
    bool SliderInt(const char *label, int *value, int min, int max,
                   float x, float y, float w, float h);

    bool SliderFloatVertical(const char *label, float *value, float min, float max,
                             float x, float y, float w, float h);
    bool SliderIntVertical(const char *label, int *value, int min, int max,
                           float x, float y, float w, float h);

    bool ColorPicker(const char *label, Color *color, float x, float y, float w = 200.0f);

    bool Dropdown(const char *label, int *selectedIndex, const char **items, int itemCount,
                  float x, float y, float w, float h);

    bool ListBox(const char *label, int *selectedIndex, const char **items, int itemCount,
                 float x, float y, float w, float h, int visibleItems = 5);

    void ProgressBar(float progress, float x, float y, float w, float h,
                     Orientation orient = Orientation::Horizontal, const char *overlay = nullptr);

    bool TextInput(const char *label, char *buffer, size_t bufferSize,
                   float x, float y, float w, float h = 0);

    bool ToggleSwitch(const char *label, bool *value, float x, float y, float w = 0.0f, float h = 0.0f);
    void SeparatorText(const char *text, float x, float y, float w);
    void Spinner(float x, float y, float radius);

    bool DragFloat(const char *label, float *value,
                   float speed,
                   float min, float max,
                   float x, float y, float w, float h);

    bool DragFloat(const char *label, float *value,
                   float speed,
                   float min, float max,
                   float x, float y, float w, float h,
                   const Color &accentColor);

    // RadioButton - retorna true se selecionado
    bool RadioButton(const char *label, int *selected, int value, float x, float y, float size = 0);

    void Label(const char *text, float x, float y);
    void LabelColored(const char *text, const Color &color, float x, float y);
    void Text(float x, float y, const char *fmt, ...);
    void Separator(float x, float y, float w);

    float GetWindowCenterX() const;
    float GetWindowCenterY() const;
    float GetWindowContentWidth() const;
    float GetWindowContentHeight() const;

    float AlignCenter(float itemWidth) const;
    float AlignRight(float itemWidth, float margin = 8.0f) const;
    float AlignLeft(float margin = 8.0f) const;

    // Theme
    void SetTheme(const GUITheme &theme);
    GUITheme &GetTheme();
    static GUITheme DarkTheme();
    static GUITheme LightTheme();
    static GUITheme BlueTheme();

    // Utility
    bool IsWindowHovered() const;
    bool IsWindowFocused() const;

    void SetNextWindowPos(float x, float y);
    void SetNextWindowSize(float width, float height);

    FloatRect MakeContentRect(float x, float y, float w, float h) const;
    Vec2 MakeContentPos(float x, float y) const;
    void DrawRectFilled(const FloatRect &rect, const Color &color);
    void DrawRectOutline(const FloatRect &rect, const Color &color, float thickness = 1.0f);
    void DrawText(const char *text, float x, float y, const Color &color);

private:
    // Windows
    WindowData *GetOrCreateWindow(const std::string &id);
    WindowData *GetCurrentWindow();
    void UpdateWindowInteraction(WindowData *window);
    void RenderWindow(WindowData *window, const char *title);
    void SortWindowsByZOrder();
    void BringCurrentWindowToFront();

    // Helpers
    bool IsPointInRect(const Vec2 &point, const FloatRect &rect) const;
    WidgetState GetWidgetState(const FloatRect &rect, const std::string &widgetID);

    float GetTextWidth(const char *text);
    float GetTextHeight(const char *text);

    // Ids
    // std::string GenerateID(const char *label);
    bool IsMouseInWindow() const;

private:
    // ========================================
    // Member Variables
    // ========================================
    RenderBatch *m_batch = nullptr;
    Font *m_font = nullptr;
    GUITheme m_theme{};

    // Input state
    Vec2 m_mousePos{};

    // Windows
    std::unordered_map<std::string, WindowData> m_windows;
    std::vector<WindowData *> m_windowOrder;
    WindowData *m_currentWindow = nullptr;
    WindowData *m_focusedWindow = nullptr;
    WindowData *m_draggingWindow = nullptr;

    // Widget state
    std::string m_activeID;
    std::string m_hotID;
    std::string m_lastWidgetID;

    // Layout base (mantida para referências internas)
    Vec2 m_cursorPos{};
    float m_indentLevel = 0.0f;
    float m_maxItemWidth = 0.0f;

    // Next window settings
    bool m_hasNextWindowPos = false;
    Vec2 m_nextWindowPos{};
    bool m_hasNextWindowSize = false;
    Vec2 m_nextWindowSize{};

    // Next window flags
    bool m_nextCanClose = true;
    bool m_nextCanMinimize = true;
    bool m_nextCanResize = true;
    bool m_nextCanMove = true;
    bool m_windowFlagsSet = false;

    // Frame/IDs
    int m_frameCounter = 0;
    int m_widgetCounter = 0;

    struct Control
    {
        u32 id;
        bool active;
        bool changed;
        int itag;
        float ftag;
        Control()
        {
            id = 0;
            active = false;
            ftag = 0.0f;
            itag = 0;
            changed = false;
        }
    };
    int m_focusedControl = -1;
    // std::vector<Control> m_controls;
    std::unordered_map<u32, Control> m_controls;
    Control *AddControl();
    u32 m_nextControlID = 0;
    u32 GetNextControlID();
    u32 m_activeControl;
    bool m_isFocused = false;
    u32 focusCounter;
    bool AnyControlActive() const;
};
