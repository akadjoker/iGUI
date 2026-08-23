#include "pch.h"
#include "Batch.hpp"
#include "Color.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "GUI.hpp"
#include "Input.hpp"

Color LerpColor(const Color &a, const Color &b, float t)
{
    t = Clamp(t, 0.0f, 1.0f);
    return Color(
        static_cast<u8>(a.r + (b.r - a.r) * t),
        static_cast<u8>(a.g + (b.g - a.g) * t),
        static_cast<u8>(a.b + (b.b - a.b) * t),
        static_cast<u8>(a.a + (b.a - a.a) * t));
}

 

// ========================================
// Construtor / Destrutor / Init
// ========================================

GUI::GUI()
{
    focusCounter = 0;
}

GUI::~GUI()
{
    Release();
}

void GUI::Init(RenderBatch *batch, Font *font)
{
    m_batch = batch;
    m_font = font;
}

void GUI::Release()
{
    m_windows.clear();
    m_windowOrder.clear();
}

// ========================================
// Helpers de janela (centros / conteúdo)
// ========================================

float GUI::GetWindowCenterX() const
{
    if (!m_currentWindow)
        return 0.0f;
    return m_currentWindow->bounds.width * 0.5f;
}

float GUI::GetWindowCenterY() const
{
    if (!m_currentWindow)
        return 0.0f;
    float contentHeight = m_currentWindow->bounds.height - m_theme.titleBarHeight;
    return m_theme.titleBarHeight + contentHeight * 0.5f;
}

float GUI::GetWindowContentWidth() const
{
    if (!m_currentWindow)
        return 0.0f;
    return m_currentWindow->bounds.width - m_theme.windowPadding * 2.0f;
}

float GUI::GetWindowContentHeight() const
{
    if (!m_currentWindow)
        return 0.0f;
    return m_currentWindow->bounds.height - m_theme.titleBarHeight - m_theme.windowPadding * 2.0f;
}

float GUI::AlignCenter(float itemWidth) const
{
    return GetWindowCenterX() - itemWidth * 0.5f;
}

float GUI::AlignRight(float itemWidth, float margin) const
{
    if (!m_currentWindow)
        return 0.0f;
    return m_currentWindow->bounds.width - itemWidth - margin;
}

float GUI::AlignLeft(float margin) const
{
    if (!m_currentWindow)
        return margin;
    return m_theme.windowPadding + margin;
}

// ========================================
// Frame Management
// ========================================

bool GUI::BeginFrame()
{
    m_nextControlID = 0;
    m_activeControl = 0;
    focusCounter = 0;

    m_mousePos = Input::GetMousePosition();

    ++m_frameCounter;
    m_widgetCounter = 0;

    // Soltou botão → limpa captura e termina drag
    if (Input::IsMouseReleased(MouseButton::LEFT))
    {
        m_activeID.clear();
        if (m_draggingWindow)
        {
            m_draggingWindow->isDragging = false;
            m_draggingWindow = nullptr;
        }
    }

    SortWindowsByZOrder();

    // Atualiza arrasto em progresso
    if (m_draggingWindow && m_draggingWindow->isDragging && Input::IsMouseDown(MouseButton::LEFT))
    {
        m_draggingWindow->bounds.x = m_mousePos.x - m_draggingWindow->dragOffset.x;
        m_draggingWindow->bounds.y = m_mousePos.y - m_draggingWindow->dragOffset.y;
        return true;
    }

    // Descobre a janela topmost sob o rato
    WindowData *topWindow = nullptr;
    for (int i = static_cast<int>(m_windowOrder.size()) - 1; i >= 0; --i)
    {
        WindowData *w = m_windowOrder[i];
        if (!w->isOpen)
            continue;
        const FloatRect r = w->isMinimized
                                ? FloatRect(w->bounds.x, w->bounds.y, w->bounds.width, m_theme.titleBarHeight)
                                : w->bounds;
        if (IsPointInRect(m_mousePos, r))
        {
            topWindow = w;
            break;
        }
    }

    if (topWindow)
    {
        UpdateWindowInteraction(topWindow);
    }
    else if (Input::IsMousePressed(MouseButton::LEFT))
    {
        // Clique no vazio → limpa foco
        for (auto &p : m_windows)
            p.second.isFocused = false;
        m_focusedWindow = nullptr;
    }

    return true;
}

void GUI::EndFrame()
{
    m_hotID.clear();
}

bool GUI::IsFocused()
{
    m_isFocused = (focusCounter > 0);
    return m_isFocused;
}

// ========================================
// Theme Management
// ========================================

void GUI::SetTheme(const GUITheme &theme)
{
    m_theme = theme;
}

GUITheme &GUI::GetTheme()
{
    return m_theme;
}

GUITheme GUI::DarkTheme()
{
    GUITheme t; 
    return t;
}

GUITheme GUI::LightTheme()
{
    GUITheme t;

    // --------------------------
    // FUNDOS (light neutro)
    // --------------------------
    t.windowBg       = Color(230, 230, 235, 255);
    t.windowBorder   = Color(180, 180, 185, 255);
    t.titleBarBg     = Color(245, 245, 248, 255);
    t.titleBarActive = Color(250, 250, 252, 255);
    t.titleText      = Color(40, 40, 40, 255);

    t.labelText      = Color(40, 40, 40, 255);
    t.separatorColor = Color(180, 180, 185, 255);

    // --------------------------
    // BOTÕES (cinza)
    // --------------------------
    t.buttonBg     = Color(210, 210, 215, 255);
    t.buttonHover  = Color(200, 200, 205, 255);
    t.buttonActive = Color(170, 170, 175, 255);
    t.buttonText   = Color(20, 20, 20, 255);

    // --------------------------
    // INPUTS
    // --------------------------
    t.inputBg      = Color(250, 250, 252, 255);
    t.inputBorder  = Color(180, 180, 185, 255);
    t.inputText    = Color(20, 20, 20, 255);
    t.inputCursor  = Color(60, 60, 60, 255);

    t.textInputBg       = t.inputBg;
    t.textInputBorder   = t.inputBorder;
    t.textInputText     = t.inputText;
    t.textInputCursor   = Color(60, 60, 60, 255);
    t.textInputSelection= Color(150, 150, 150, 128);

    // --------------------------
    // SLIDERS (fill cinzento)
    // --------------------------
    t.sliderBg     = Color(200, 200, 205, 255);
    t.sliderFill   = Color(150, 150, 155, 255); 
    t.sliderHandle = Color(240, 240, 245, 255);
    t.sliderHandleHover = Color(255, 255, 255, 255);

    // --------------------------
    // PROGRESS BAR
    // --------------------------
    t.progressBarBg   = Color(200, 200, 205, 255);
    t.progressBarFill = Color(150, 150, 155, 255);

    // --------------------------
    // CHECKBOX / RADIO
    // --------------------------
    t.checkboxBg     = Color(220, 220, 225, 255);
    t.checkboxCheck  = Color(120, 120, 125, 255); 
    t.checkboxBorder = Color(150, 150, 155, 255);

    t.radioBg        = t.checkboxBg;
    t.radioCheck     = t.checkboxCheck;
    t.radioBorder    = t.checkboxBorder;

    // --------------------------
    // LISTBOX
    // --------------------------
    t.listBoxBg           = Color(240, 240, 245, 255);
    t.listBoxItemHover    = Color(220, 220, 225, 255);
    t.listBoxItemSelected = Color(170, 170, 175, 255); 
    t.listBoxBorder       = t.inputBorder;

    // --------------------------
    // DROPDOWN
    // --------------------------
    t.dropdownBg        = Color(240, 240, 245, 255);
    t.dropdownHover     = Color(220, 220, 225, 255);
    t.dropdownBorder    = Color(180, 180, 185, 255);
    t.dropdownItemHover = Color(220, 220, 225, 255);

    // --------------------------
    // COLOR PICKER
    // --------------------------
    t.colorPickerBorder = Color(150, 150, 155, 255);

    // Sizing (mantém default)
    t.titleBarHeight = 28;
    t.windowPadding  = 8;
    t.itemSpacing    = 4;
    t.inputHeight    = 24;
    t.sliderHeight   = 20;

    return t;
}


GUITheme GUI::BlueTheme()
{
    GUITheme t = DarkTheme();  

    Color bluePrimary   = Color(90, 160, 230, 255);
    Color blueSecondary = Color(70, 130, 200, 255);
    Color blueHover     = Color(55, 85, 130, 255);

    // Fundo mais azulado
    t.windowBg       = Color(20, 28, 40, 240);
    t.windowBorder   = Color(55, 75, 100, 255);
    t.titleBarBg     = Color(28, 42, 60, 255);
    t.titleBarActive = Color(36, 56, 80, 255);
    t.titleText      = Color(230, 235, 245, 255);

    // Botões
    t.buttonBg       = Color(30, 46, 70, 255);
    t.buttonHover    = blueHover;
    t.buttonActive   = bluePrimary;
    t.buttonText     = Color(230, 235, 245, 255);

    // Sliders
    t.sliderBg          = Color(24, 38, 56, 255);
    t.sliderFill        = bluePrimary;
    t.sliderHandle      = Color(200, 210, 225, 255);
    t.sliderHandleHover = Color(235, 240, 250, 255);

    // Checkbox
    t.checkboxBg     = Color(30, 46, 70, 255);
    t.checkboxCheck  = bluePrimary;
    t.checkboxBorder = Color(80, 100, 130, 255);

    // Labels / separadores
    t.labelText      = Color(220, 230, 245, 255);
    t.separatorColor = Color(60, 80, 105, 255);

    // Inputs
    t.inputBg     = Color(18, 26, 38, 255);
    t.inputBorder = Color(70, 90, 120, 255);
    t.inputText   = Color(220, 230, 245, 255);
    t.inputCursor = t.inputText;

    t.textInputBg        = t.inputBg;
    t.textInputBorder    = t.inputBorder;
    t.textInputText      = t.inputText;
    t.textInputCursor    = t.inputCursor;
    t.textInputSelection = Color(90, 160, 230, 80);

    // ProgressBar  
    t.progressBarBg   = Color(24, 38, 56, 255);
    t.progressBarFill = blueSecondary;

    // Dropdown
    t.dropdownBg        = Color(24, 34, 50, 255);
    t.dropdownHover     = blueHover;
    t.dropdownBorder    = Color(70, 90, 120, 255);
    t.dropdownItemHover = Color(32, 48, 70, 255);

    // ListBox
    t.listBoxBg           = Color(24, 32, 46, 255);
    t.listBoxItemHover    = Color(36, 54, 78, 255);
    t.listBoxItemSelected = bluePrimary;
    t.listBoxBorder       = Color(70, 90, 120, 255);

    // Radio
    t.radioBg     = Color(24, 38, 56, 255);
    t.radioCheck  = bluePrimary;
    t.radioBorder = Color(80, 100, 130, 255);

    // ColorPicker
    t.colorPickerBorder = t.listBoxBorder;
 
    return t;
}

// ========================================
// Utility
// ========================================

bool GUI::IsWindowHovered() const
{
    if (!m_currentWindow)
        return false;
    return IsPointInRect(m_mousePos, m_currentWindow->bounds);
}

bool GUI::IsWindowFocused() const
{
    if (!m_currentWindow)
        return false;
    return m_currentWindow->isFocused;
}

void GUI::SetNextWindowPos(float x, float y)
{
    m_nextWindowPos = Vec2(x, y);
    m_hasNextWindowPos = true;
}

void GUI::SetNextWindowSize(float width, float height)
{
    m_nextWindowSize = Vec2(width, height);
    m_hasNextWindowSize = true;
}

// Helpers de conteúdo

FloatRect GUI::MakeContentRect(float x, float y, float w, float h) const
{
    if (!m_currentWindow)
        return FloatRect();
    float ox = m_currentWindow->bounds.x + m_theme.windowPadding;
    float oy = m_currentWindow->bounds.y + m_theme.titleBarHeight + m_theme.windowPadding - m_currentWindow->scrollOffset.y;
    return FloatRect(ox + x, oy + y, w, h);
}

Vec2 GUI::MakeContentPos(float x, float y) const
{
    if (!m_currentWindow)
        return Vec2();
    float ox = m_currentWindow->bounds.x + m_theme.windowPadding;
    float oy = m_currentWindow->bounds.y + m_theme.titleBarHeight + m_theme.windowPadding - m_currentWindow->scrollOffset.y;
    return Vec2(ox + x, oy + y);
}

bool GUI::IsPointInRect(const Vec2 &p, const FloatRect &r) const
{
    return p.x >= r.x && p.x <= r.x + r.width &&
           p.y >= r.y && p.y <= r.y + r.height;
}

WidgetState GUI::GetWidgetState(const FloatRect &rect, const std::string &widgetID)
{
    if (IsPointInRect(m_mousePos, rect))
    {
        m_hotID = widgetID;
        m_lastWidgetID = widgetID;
        if (m_activeID == widgetID)
            return WidgetState::Active;
        return WidgetState::Hovered;
    }
    if (m_activeID == widgetID)
        return WidgetState::Active;
    return WidgetState::Normal;
}

void GUI::DrawRectFilled(const FloatRect &rect, const Color &color)
{
    if (!m_batch)
        return;
    m_batch->SetColor(color);
    m_batch->Rectangle((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, true);
}

void GUI::DrawRectOutline(const FloatRect &rect, const Color &color, float /*thickness*/)
{
    if (!m_batch)
        return;
    m_batch->SetColor(color);
    m_batch->Rectangle((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, false);
}

void GUI::DrawText(const char *text, float x, float y, const Color &color)
{
    if (!m_font)
        return;
    m_font->SetColor(color);
    m_font->Print(text, x, y);
}

float GUI::GetTextWidth(const char *text)
{
    if (!m_font)
        return 0.0f;
    return m_font->GetTextWidth(text);
}

float GUI::GetTextHeight(const char *text)
{
    if (!m_font)
        return 0.0f;
    return m_font->GetTextHeight(text);
}

bool GUI::IsMouseInWindow() const
{
    if (!m_currentWindow)
        return false;
    return IsPointInRect(m_mousePos, m_currentWindow->bounds);
}

// ========================================
// Controls
// ========================================

GUI::Control *GUI::AddControl()
{
    u32 id = GetNextControlID();
    auto it = m_controls.find(id);
    if (it == m_controls.end())
    {
        Control c;
        c.id = id;
        c.active = false;
        c.ftag = 0.0f;
        c.itag = 0;
        c.changed = false;
        m_controls[id] = c;
        return &m_controls[id];
    }
    return &it->second;
}

u32 GUI::GetNextControlID()
{
    return m_nextControlID++;
}

bool GUI::AnyControlActive() const
{
    for (const auto &kv : m_controls)
    {
        if (kv.second.active)
            return true;
    }
    return false;
}

// ========================================
// Window Management
// ========================================

bool GUI::BeginWindow(const char *title, float x, float y, float w, float h, bool *open)
{
    return BeginWindow(title, FloatRect(x, y, w, h), open);
}

bool GUI::BeginWindow(const char *title, const FloatRect &bounds, bool *open)
{
    m_widgetCounter = 0;
    std::string id = title;
    WindowData *window = GetOrCreateWindow(id);

    // Next-window settings
    if (m_hasNextWindowPos)
    {
        window->bounds.x = m_nextWindowPos.x;
        window->bounds.y = m_nextWindowPos.y;
        m_hasNextWindowPos = false;
    }
    if (m_hasNextWindowSize)
    {
        window->bounds.width = m_nextWindowSize.x;
        window->bounds.height = m_nextWindowSize.y;
        m_hasNextWindowSize = false;
    }
    if (m_windowFlagsSet)
    {
        window->canClose = m_nextCanClose;
        window->canMinimize = m_nextCanMinimize;
        window->canResize = m_nextCanResize;
        window->canMove = m_nextCanMove;
        m_windowFlagsSet = false;
    }

    // Primeira vez
    if (window->bounds.width == 0.0f)
        window->bounds = bounds;

    // Close flag externo
    if (open)
    {
        if (!(*open))
            window->isOpen = false;
        *open = window->isOpen;
        if (!window->isOpen)
            return false;
    }
    else if (!window->isOpen)
    {
        return false;
    }

    m_currentWindow = window;

    // Desenhar janela
    RenderWindow(window, title);

    // Define origem e largura útil (para quem quiser defaults)
    if (!window->isMinimized)
    {
        m_cursorPos.x = window->bounds.x + m_theme.windowPadding;
        m_cursorPos.y = window->bounds.y + m_theme.titleBarHeight + m_theme.windowPadding - window->scrollOffset.y;
        m_maxItemWidth = window->bounds.width - m_theme.windowPadding * 2.0f;
    }

    bool isHover = IsPointInRect(m_mousePos, window->bounds);
    if (isHover)
    {
        focusCounter++;
    }

    return !window->isMinimized;
}

void GUI::EndWindow()
{
    if (!m_currentWindow)
        return;
    m_currentWindow->contentHeight =
        m_cursorPos.y - (m_currentWindow->bounds.y + m_theme.titleBarHeight + m_theme.windowPadding) + m_currentWindow->scrollOffset.y;
    m_currentWindow = nullptr;
}

void GUI::SetWindowFlags(bool canClose, bool canMinimize, bool canResize, bool canMove)
{
    m_nextCanClose = canClose;
    m_nextCanMinimize = canMinimize;
    m_nextCanResize = canResize;
    m_nextCanMove = canMove;
    m_windowFlagsSet = true;
}

WindowData *GUI::GetOrCreateWindow(const std::string &id)
{
    auto it = m_windows.find(id);
    if (it == m_windows.end())
    {
        WindowData w;
        w.id = id;
        w.zOrder = static_cast<int>(m_windows.size());
        m_windows[id] = w;
        m_windowOrder.push_back(&m_windows[id]);
        return &m_windows[id];
    }
    return &it->second;
}

WindowData *GUI::GetCurrentWindow()
{
    return m_currentWindow;
}

// Versão melhorada com AnyControlActive
void GUI::UpdateWindowInteraction(WindowData *window)
{
    if (!window)
        return;

    FloatRect titleBarRect(window->bounds.x, window->bounds.y, window->bounds.width, m_theme.titleBarHeight);
    float buttonX = window->bounds.x + window->bounds.width - m_theme.titleBarHeight;

    FloatRect closeButtonRect(buttonX, window->bounds.y, m_theme.titleBarHeight, m_theme.titleBarHeight);

    FloatRect minimizeButtonRect;
    if (window->canClose && window->canMinimize)
        minimizeButtonRect = FloatRect(buttonX - m_theme.titleBarHeight, window->bounds.y, m_theme.titleBarHeight, m_theme.titleBarHeight);
    else if (window->canMinimize)
        minimizeButtonRect = closeButtonRect;

    const FloatRect interactionRect = window->isMinimized
                                          ? FloatRect(window->bounds.x, window->bounds.y, window->bounds.width, m_theme.titleBarHeight)
                                          : window->bounds;

    const bool mouseInTitleBar = IsPointInRect(m_mousePos, titleBarRect);
    const bool mouseInWindow = IsPointInRect(m_mousePos, interactionRect);

    // Botões
    if (window->canClose && Input::IsMousePressed(MouseButton::LEFT) && IsPointInRect(m_mousePos, closeButtonRect))
    {
        window->isOpen = false;
        return;
    }
    if (window->canMinimize && Input::IsMousePressed(MouseButton::LEFT) && IsPointInRect(m_mousePos, minimizeButtonRect))
    {
        window->isMinimized = !window->isMinimized;
        return;
    }

    // Foco apenas se o clique foi nesta janela
    if (Input::IsMousePressed(MouseButton::LEFT) && mouseInWindow)
    {
        for (auto &p : m_windows)
            p.second.isFocused = false;
        window->isFocused = true;
        m_focusedWindow = window;
        window->zOrder = 1000 + m_frameCounter;
    }

    // Drag — só se nenhum controlo estiver ativo
    if (window->canMove && mouseInTitleBar && !AnyControlActive() && Input::IsMousePressed(MouseButton::LEFT))
    {
        window->isDragging = true;
        m_draggingWindow = window;
        window->dragOffset = Vec2(m_mousePos.x - window->bounds.x, m_mousePos.y - window->bounds.y);
    }
}

void GUI::RenderWindow(WindowData *window, const char *title)
{
    if (!window || !m_batch)
        return;

    // Minimizada → só titlebar
    if (window->isMinimized)
    {
        FloatRect r(window->bounds.x, window->bounds.y, window->bounds.width, m_theme.titleBarHeight);
        Color titleBarColor = window->isFocused ? m_theme.titleBarActive : m_theme.titleBarBg;
        DrawRectFilled(r, titleBarColor);
        DrawRectOutline(r, m_theme.windowBorder, m_theme.windowBorderSize);

        float titleX = window->bounds.x + m_theme.windowPadding;
        float titleY = window->bounds.y + (m_theme.titleBarHeight - GetTextHeight(title)) * 0.5f;
        DrawText(title, titleX, titleY, m_theme.titleText);

        float buttonX = window->bounds.x + window->bounds.width - m_theme.titleBarHeight;

        // Close
        if (window->canClose)
        {
            FloatRect closeButtonRect(buttonX, window->bounds.y, m_theme.titleBarHeight, m_theme.titleBarHeight);
            Color c = IsPointInRect(m_mousePos, closeButtonRect) ? m_theme.buttonHover : titleBarColor;
            DrawRectFilled(closeButtonRect, c);

            float p = 8.0f;
            m_batch->SetColor(m_theme.titleText);
            m_batch->Line2D(Vec2(closeButtonRect.x + p, closeButtonRect.y + p),
                            Vec2(closeButtonRect.x + closeButtonRect.width - p, closeButtonRect.y + closeButtonRect.height - p));
            m_batch->Line2D(Vec2(closeButtonRect.x + closeButtonRect.width - p, closeButtonRect.y + p),
                            Vec2(closeButtonRect.x + p, closeButtonRect.y + closeButtonRect.height - p));
            buttonX -= m_theme.titleBarHeight;
        }

        // Minimize (ícone expand)
        if (window->canMinimize)
        {
            FloatRect minimizeButtonRect(buttonX, window->bounds.y, m_theme.titleBarHeight, m_theme.titleBarHeight);
            Color c = IsPointInRect(m_mousePos, minimizeButtonRect) ? m_theme.buttonHover : titleBarColor;
            DrawRectFilled(minimizeButtonRect, c);

            float p = 8.0f;
            m_batch->SetColor(m_theme.titleText);
            FloatRect icon(minimizeButtonRect.x + p, minimizeButtonRect.y + p,
                           minimizeButtonRect.width - 2 * p, minimizeButtonRect.height - 2 * p);
            m_batch->Rectangle((int)icon.x, (int)icon.y, (int)icon.width, (int)icon.height, false);
        }
        return;
    }

    // Fundo da janela
    DrawRectFilled(window->bounds, m_theme.windowBg);
    DrawRectOutline(window->bounds, m_theme.windowBorder, m_theme.windowBorderSize);

    // Title bar
    FloatRect titleBarRect(window->bounds.x, window->bounds.y, window->bounds.width, m_theme.titleBarHeight);
    Color titleBarColor = window->isFocused ? m_theme.titleBarActive : m_theme.titleBarBg;
    DrawRectFilled(titleBarRect, titleBarColor);

    float titleX = window->bounds.x + m_theme.windowPadding;
    float titleY = window->bounds.y + (m_theme.titleBarHeight - GetTextHeight(title)) * 0.5f;
    DrawText(title, titleX, titleY, m_theme.titleText);

    float buttonX = window->bounds.x + window->bounds.width - m_theme.titleBarHeight;

    // Close
    if (window->canClose)
    {
        FloatRect closeButtonRect(buttonX, window->bounds.y, m_theme.titleBarHeight, m_theme.titleBarHeight);
        Color c = IsPointInRect(m_mousePos, closeButtonRect) ? m_theme.buttonHover : titleBarColor;
        DrawRectFilled(closeButtonRect, c);

        float p = 8.0f;
        m_batch->SetColor(m_theme.titleText);
        m_batch->Line2D(Vec2(closeButtonRect.x + p, closeButtonRect.y + p),
                        Vec2(closeButtonRect.x + closeButtonRect.width - p, closeButtonRect.y + closeButtonRect.height - p));
        m_batch->Line2D(Vec2(closeButtonRect.x + closeButtonRect.width - p, closeButtonRect.y + p),
                        Vec2(closeButtonRect.x + p, closeButtonRect.y + closeButtonRect.height - p));
        buttonX -= m_theme.titleBarHeight;
    }

    // Minimize
    if (window->canMinimize)
    {
        FloatRect minimizeButtonRect(buttonX, window->bounds.y, m_theme.titleBarHeight, m_theme.titleBarHeight);
        Color c = IsPointInRect(m_mousePos, minimizeButtonRect) ? m_theme.buttonHover : titleBarColor;
        DrawRectFilled(minimizeButtonRect, c);

        float p = 8.0f;
        m_batch->SetColor(m_theme.titleText);
        float lineY = minimizeButtonRect.y + minimizeButtonRect.height - p - 2.0f;
        m_batch->Line2D(Vec2(minimizeButtonRect.x + p, lineY),
                        Vec2(minimizeButtonRect.x + minimizeButtonRect.width - p, lineY));
    }
}

void GUI::SortWindowsByZOrder()
{
    std::sort(m_windowOrder.begin(), m_windowOrder.end(),
              [](WindowData *a, WindowData *b)
              { return a->zOrder < b->zOrder; });
}

void GUI::BringCurrentWindowToFront()
{
    if (!m_currentWindow)
        return;
    m_currentWindow->isFocused = true;
    m_currentWindow->zOrder = 1000 + m_frameCounter;
    SortWindowsByZOrder();
}

// ========================================
// Widgets (posicionamento explícito)
// ========================================

bool GUI::Button(const char *label, float x, float y, float w, float h)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return false;

    Control *c = AddControl();
    if (!c)
        return false;

    c->changed = false;

    FloatRect rect = MakeContentRect(x, y, w, h);
    bool hovered = IsPointInRect(m_mousePos, rect);
    bool pressed = hovered && Input::IsMousePressed(MouseButton::LEFT);
    bool released = Input::IsMouseReleased(MouseButton::LEFT);

    if (pressed)
        c->active = true;

    bool clicked = false;

    if (c->active && released)
    {
        if (hovered)
        {
            clicked = true;
            c->changed = true;
        }
        c->active = false;
    }

    Color bg = m_theme.buttonBg;
    if (c->active)
        bg = m_theme.buttonActive;
    else if (hovered)
        bg = m_theme.buttonHover;

    DrawRectFilled(rect, bg);
    DrawRectOutline(rect, m_theme.windowBorder, 1.0f);

    float tw = GetTextWidth(label);
    float th = GetTextHeight(label);
    DrawText(label,
             rect.x + (rect.width - tw) * 0.5f,
             rect.y + (rect.height - th) * 0.5f,
             m_theme.buttonText);

    return clicked;
}

bool GUI::Checkbox(const char *label, bool *value, float x, float y, float size)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return false;

    Control *c = AddControl();
    if (!c)
        return false;

    c->changed = false;

    FloatRect box = MakeContentRect(x, y, size, size);
    bool hovered = IsPointInRect(m_mousePos, box);
    bool pressed = hovered && Input::IsMousePressed(MouseButton::LEFT);

    if (pressed)
    {
        *value = !*value;
        c->changed = true;
    }

    Color boxColor = m_theme.checkboxBg;
    if (hovered)
        boxColor = LerpColor(m_theme.checkboxBg, m_theme.buttonHover, 0.5f);

    DrawRectFilled(box, boxColor);
    DrawRectOutline(box, m_theme.checkboxBorder, 1.0f);

    if (*value)
    {
        float p = 3.0f;
        DrawRectFilled(FloatRect(box.x + p, box.y + p,
                                 box.width - 2 * p,
                                 box.height - 2 * p),
                       m_theme.checkboxCheck);
    }

    float ly = box.y + (box.height - GetTextHeight(label)) * 0.5f;
    DrawText(label, box.x + box.width + m_theme.itemSpacing, ly, m_theme.labelText);

    return c->changed;
}

bool GUI::SliderFloat(const char *label, float *value, float min, float max,
                      float x, float y, float w, float h)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return false;

    Control *c = AddControl();
    if (!c)
        return false;

    c->changed = false;

    FloatRect rect = MakeContentRect(x, y, w, h);
    float lw = GetTextWidth(label) + m_theme.itemSpacing * 2.0f;
    float vw = GetTextWidth("9999.99") + m_theme.itemSpacing * 2.0f;
    float sw = std::max(50.0f, rect.width - lw - vw);
    FloatRect slider(rect.x + lw, rect.y, sw, rect.height);

    bool hovered = IsPointInRect(m_mousePos, slider);
    bool pressed = hovered && Input::IsMousePressed(MouseButton::LEFT);
    bool released = Input::IsMouseReleased(MouseButton::LEFT);

    if (pressed)
        c->active = true;

    if (c->active && Input::IsMouseDown(MouseButton::LEFT))
    {
        float t = Clamp((m_mousePos.x - slider.x) / slider.width, 0.0f, 1.0f);
        float nv = min + t * (max - min);
        if (nv != *value)
        {
            *value = nv;
            c->changed = true;
        }
    }

    if (c->active && released)
        c->active = false;

    DrawRectFilled(slider, m_theme.sliderBg);
    DrawRectOutline(slider, m_theme.windowBorder, 1.0f);

    float t = Clamp((*value - min) / (max - min), 0.0f, 1.0f);
    float fw = t * slider.width;
    if (fw > 0.0f)
        DrawRectFilled(FloatRect(slider.x, slider.y, fw, slider.height), m_theme.sliderFill);

    float handleW = 12.0f;
    float hx = Clamp(slider.x + fw - handleW * 0.5f, slider.x, slider.x + slider.width - handleW);
    FloatRect handle(hx, slider.y - 2.0f, handleW, slider.height + 4.0f);
    Color hc = (hovered || c->active) ? m_theme.sliderHandleHover : m_theme.sliderHandle;
    DrawRectFilled(handle, hc);
    DrawRectOutline(handle, m_theme.windowBorder, 1.0f);

    DrawText(label,
             rect.x,
             rect.y + (rect.height - GetTextHeight(label)) * 0.5f,
             m_theme.labelText);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", *value);
    DrawText(buf,
             slider.x + slider.width + m_theme.itemSpacing,
             rect.y + (rect.height - GetTextHeight(buf)) * 0.5f,
             m_theme.labelText);

    return c->changed;
}

bool GUI::SliderInt(const char *label, int *value, int min, int max,
                    float x, float y, float w, float h)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return false;

    Control *c = AddControl();
    if (!c)
        return false;

    c->changed = false;

    FloatRect rect = MakeContentRect(x, y, w, h);
    float lw = GetTextWidth(label) + m_theme.itemSpacing * 2.0f;
    float vw = GetTextWidth("999999") + m_theme.itemSpacing * 2.0f;
    float sw = std::max(50.0f, rect.width - lw - vw);
    FloatRect slider(rect.x + lw, rect.y, sw, rect.height);

    bool hovered = IsPointInRect(m_mousePos, slider);
    bool pressed = hovered && Input::IsMousePressed(MouseButton::LEFT);
    bool released = Input::IsMouseReleased(MouseButton::LEFT);

    if (pressed)
        c->active = true;

    if (c->active && Input::IsMouseDown(MouseButton::LEFT))
    {
        float t = Clamp((m_mousePos.x - slider.x) / slider.width, 0.0f, 1.0f);
        int nv = min + int(t * (max - min) + 0.5f);
        if (nv != *value)
        {
            *value = nv;
            c->changed = true;
        }
    }

    if (c->active && released)
        c->active = false;

    DrawRectFilled(slider, m_theme.sliderBg);
    DrawRectOutline(slider, m_theme.windowBorder, 1.0f);

    float t = Clamp(float(*value - min) / float(max - min), 0.0f, 1.0f);
    float fw = t * slider.width;
    if (fw > 0.0f)
        DrawRectFilled(FloatRect(slider.x, slider.y, fw, slider.height), m_theme.sliderFill);

    float handleW = 12.0f;
    float hx = Clamp(slider.x + fw - handleW * 0.5f, slider.x, slider.x + slider.width - handleW);
    FloatRect handle(hx, slider.y - 2.0f, handleW, slider.height + 4.0f);
    Color hc = (hovered || c->active) ? m_theme.sliderHandleHover : m_theme.sliderHandle;
    DrawRectFilled(handle, hc);
    DrawRectOutline(handle, m_theme.windowBorder, 1.0f);

    DrawText(label,
             rect.x,
             rect.y + (rect.height - GetTextHeight(label)) * 0.5f,
             m_theme.labelText);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", *value);
    DrawText(buf,
             slider.x + slider.width + m_theme.itemSpacing,
             rect.y + (rect.height - GetTextHeight(buf)) * 0.5f,
             m_theme.labelText);

    return c->changed;
}

bool GUI::SliderFloatVertical(const char *label, float *value, float min, float max,
                              float x, float y, float w, float h)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return false;

    Control *c = AddControl();
    if (!c)
        return false;

    c->changed = false;

    FloatRect rect = MakeContentRect(x, y, w, h);

    // label por cima
    DrawText(label,
             rect.x + (rect.width - GetTextWidth(label)) * 0.5f,
             rect.y - (GetTextHeight(label) + m_theme.itemSpacing),
             m_theme.labelText);

    bool hovered = IsPointInRect(m_mousePos, rect);
    bool pressed = hovered && Input::IsMousePressed(MouseButton::LEFT);
    bool released = Input::IsMouseReleased(MouseButton::LEFT);

    if (pressed)
        c->active = true;

    if (c->active && Input::IsMouseDown(MouseButton::LEFT))
    {
        float t = Clamp((rect.y + rect.height - m_mousePos.y) / rect.height, 0.0f, 1.0f);
        float nv = min + t * (max - min);
        if (nv != *value)
        {
            *value = nv;
            c->changed = true;
        }
    }

    if (c->active && released)
        c->active = false;

    DrawRectFilled(rect, m_theme.sliderBg);
    DrawRectOutline(rect, m_theme.windowBorder, 1.0f);

    float t = Clamp((*value - min) / (max - min), 0.0f, 1.0f);
    float fh = t * rect.height;
    if (fh > 0.0f)
        DrawRectFilled(FloatRect(rect.x, rect.y + rect.height - fh, rect.width, fh), m_theme.sliderFill);

    float handleH = 12.0f;
    float hy = Clamp(rect.y + rect.height - fh - handleH * 0.5f, rect.y, rect.y + rect.height - handleH);
    FloatRect handle(rect.x - 2.0f, hy, rect.width + 4.0f, handleH);
    Color hc = (hovered || c->active) ? m_theme.sliderHandleHover : m_theme.sliderHandle;
    DrawRectFilled(handle, hc);
    DrawRectOutline(handle, m_theme.windowBorder, 1.0f);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", *value);
    DrawText(buf,
             rect.x + (rect.width - GetTextWidth(buf)) * 0.5f,
             rect.y + rect.height + m_theme.itemSpacing,
             m_theme.labelText);

    return c->changed;
}

bool GUI::SliderIntVertical(const char *label, int *value, int min, int max,
                            float x, float y, float w, float h)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return false;

    Control *c = AddControl();
    if (!c)
        return false;

    c->changed = false;

    FloatRect rect = MakeContentRect(x, y, w, h);

    DrawText(label,
             rect.x + (rect.width - GetTextWidth(label)) * 0.5f,
             rect.y - (GetTextHeight(label) + m_theme.itemSpacing),
             m_theme.labelText);

    bool hovered = IsPointInRect(m_mousePos, rect);
    bool pressed = hovered && Input::IsMousePressed(MouseButton::LEFT);
    bool released = Input::IsMouseReleased(MouseButton::LEFT);

    if (pressed)
        c->active = true;

    if (c->active && Input::IsMouseDown(MouseButton::LEFT))
    {
        float t = Clamp((rect.y + rect.height - m_mousePos.y) / rect.height, 0.0f, 1.0f);
        int nv = min + int(t * (max - min) + 0.5f);
        if (nv != *value)
        {
            *value = nv;
            c->changed = true;
        }
    }

    if (c->active && released)
        c->active = false;

    DrawRectFilled(rect, m_theme.sliderBg);
    DrawRectOutline(rect, m_theme.windowBorder, 1.0f);

    float t = Clamp(float(*value - min) / float(max - min), 0.0f, 1.0f);
    float fh = t * rect.height;
    if (fh > 0.0f)
        DrawRectFilled(FloatRect(rect.x, rect.y + rect.height - fh, rect.width, fh), m_theme.sliderFill);

    float handleH = 12.0f;
    float hy = Clamp(rect.y + rect.height - fh - handleH * 0.5f, rect.y, rect.y + rect.height - handleH);
    FloatRect handle(rect.x - 2.0f, hy, rect.width + 4.0f, handleH);
    Color hc = (hovered || c->active) ? m_theme.sliderHandleHover : m_theme.sliderHandle;
    DrawRectFilled(handle, hc);
    DrawRectOutline(handle, m_theme.windowBorder, 1.0f);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", *value);
    DrawText(buf,
             rect.x + (rect.width - GetTextWidth(buf)) * 0.5f,
             rect.y + rect.height + m_theme.itemSpacing,
             m_theme.labelText);

    return c->changed;
}

void GUI::Label(const char *text, float x, float y)
{
    WindowData *win = GetCurrentWindow();
    if (!win)
        return;
    if (win->isMinimized || !win->isOpen)
        return;

    Vec2 p = MakeContentPos(x, y);
    DrawText(text, p.x, p.y, m_theme.labelText);
}

void GUI::LabelColored(const char *text, const Color &color, float x, float y)
{
    WindowData *win = GetCurrentWindow();
    if (!win)
        return;
    if (win->isMinimized || !win->isOpen)
        return;

    Vec2 p = MakeContentPos(x, y);
    DrawText(text, p.x, p.y, color);
}

void GUI::Text(float x, float y, const char *fmt, ...)
{
    WindowData *win = GetCurrentWindow();
    if (!win)
        return;
    if (win->isMinimized || !win->isOpen)
        return;

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Label(buf, x, y);
}

void GUI::Separator(float x, float y, float w)
{
    WindowData *win = GetCurrentWindow();
    if (!win)
        return;
    if (win->isMinimized || !win->isOpen)
        return;

    DrawRectFilled(MakeContentRect(x, y, w, 2.0f), m_theme.separatorColor);
}

// ========================================
// TextInput com foco por controlo
// ========================================
bool GUI::TextInput(const char *label, char *buffer, size_t bufferSize,
                    float x, float y, float w, float h)
{
    if (!buffer || bufferSize == 0)
        return false;

    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return false;

    if (h <= 0)
        h = m_theme.textInputHeight;

    Control *control = AddControl();
    if (!control)
        return false;

    control->changed = false;

    // Rect do input
    FloatRect rect = MakeContentRect(x, y, w, h);
    bool hovered = IsPointInRect(m_mousePos, rect);

    // Foco pelo rato
    if (Input::IsMousePressed(MouseButton::LEFT))
    {
        if (hovered)
            m_focusedControl = (int)control->id;
        else if (m_focusedControl == (int)control->id)
            m_focusedControl = -1;
    }

    control->active = (m_focusedControl == (int)control->id);

    // Label (se não for vazio)
    if (label && label[0] != '\0')
    {
        float lw = GetTextWidth(label);
        float lh = GetTextHeight(label);
        DrawText(label,
                 rect.x - lw - m_theme.itemSpacing,
                 rect.y + (rect.height - lh) * 0.5f,
                 m_theme.labelText);
    }

    // Garantir cursor dentro dos limites
    size_t len = std::strlen(buffer);
    if (control->itag < 0)            control->itag = 0;
    if (control->itag > (int)len)     control->itag = (int)len;

    // ==========================
    //   INPUT DE TEXTO
    // ==========================
    if (control->active)
    {
        // Caracteres "imprimíveis"
        int ch = Input::GetCharPressed();
        
        if (ch >= 32 && ch < 127)
        {
            if (len < bufferSize - 1)
            {
               
                size_t pos      = (size_t)control->itag;
                size_t moveSize = len - pos;
                if (moveSize > 0)
                    std::memmove(buffer + pos + 1, buffer + pos, moveSize);

                buffer[pos] = (char)ch;
                len++;
                buffer[len] = '\0';

                control->itag++;
                control->changed = true;
            }
        }
    

        // Backspace (apaga antes do cursor)
        if (Input::IsKeyPressed(KeyCode::KEY_BACKSPACE))
        {
            if (control->itag > 0 && len > 0)
            {
                size_t pos      = (size_t)(control->itag - 1);
                size_t moveSize = len - pos;
                if (moveSize > 0)
                    std::memmove(buffer + pos, buffer + pos + 1, moveSize);

                control->itag--;
                len--;
                buffer[len] = '\0';
                control->changed = true;
            }
        }

        // Delete (apaga em cima do cursor)
        if (Input::IsKeyPressed(KeyCode::KEY_DELETE))
        {
            if (control->itag < (int)len)
            {
                size_t pos      = (size_t)control->itag;
                size_t moveSize = len - pos;
                if (moveSize > 0)
                    std::memmove(buffer + pos, buffer + pos + 1, moveSize);

                len--;
                buffer[len] = '\0';
                control->changed = true;
            }
        }

        // Movimentação do cursor
        if (Input::IsKeyPressed(KeyCode::KEY_LEFT) && control->itag > 0)
            control->itag--;

        if (Input::IsKeyPressed(KeyCode::KEY_RIGHT) && control->itag < (int)len)
            control->itag++;

        if (Input::IsKeyPressed(KeyCode::KEY_HOME))
            control->itag = 0;

        if (Input::IsKeyPressed(KeyCode::KEY_END))
            control->itag = (int)len;

        // Enter → sai do foco (opcional)
        if (Input::IsKeyPressed(KeyCode::KEY_ENTER) || Input::IsKeyPressed(KeyCode::KEY_KP_ENTER))
        {
            m_focusedControl = -1;
            control->active = false;
        }
    }

    // ==========================
    //   DESENHO
    // ==========================

    // Background
    Color bgColor = m_theme.textInputBg;
    if (hovered)
        bgColor = Color(40, 40, 50, 255);
    DrawRectFilled(rect, bgColor);

    // Border
    Color borderColor = control->active ? m_theme.buttonActive : m_theme.textInputBorder;
    DrawRectOutline(rect, borderColor, control->active ? 2.0f : 1.0f);

    // Texto
    float textX = rect.x + 6.0f;
    float textY = rect.y + (rect.height - GetTextHeight(buffer)) * 0.5f;
    DrawText(buffer, textX, textY, m_theme.textInputText);

    // Cursor
    if (control->active)
    {
        control->ftag += Device::Instance().GetFrameTime();
        if (control->ftag > 1.0f)
            control->ftag = 0.0f;

        bool showCursor = (control->ftag < 0.5f);
        if (showCursor)
        {
            std::string before(buffer, (size_t)control->itag);
            float cursorX = textX + GetTextWidth(before.c_str());

            DrawRectFilled(FloatRect{cursorX, rect.y + 4.0f, 2.0f, rect.height - 8.0f},
                           m_theme.textInputCursor);
        }
    }

    return control->changed;
}


// ========================================
// ColorPicker
// ========================================
static void RGBtoHSV(float r, float g, float b, float &h, float &s, float &v)
{
    float cmax = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    float cmin = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
    float delta = cmax - cmin;

    v = cmax;
    s = (cmax > 0.0f) ? (delta / cmax) : 0.0f;

    if (delta > 0.0f)
    {
        if (cmax == r)
            h = (g - b) / delta;
        else if (cmax == g)
            h = 2.0f + (b - r) / delta;
        else
            h = 4.0f + (r - g) / delta;

        h *= 60.0f;
        if (h < 0.0f)
            h += 360.0f;
    }
    else
    {
        h = 0.0f;
    }
}

static void HSVtoRGB(float h, float s, float v, float &r, float &g, float &b)
{
    s = Clamp(s, 0.0f, 1.0f);
    v = Clamp(v, 0.0f, 1.0f);

    if (s <= 0.0f)
    {
        r = g = b = v;
        return;
    }

    h = fmodf(h, 360.0f);
    if (h < 0.0f)
        h += 360.0f;

    float hh = h / 60.0f;
    int i = (int)hh;
    float ff = hh - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - (s * ff));
    float t = v * (1.0f - (s * (1.0f - ff)));

    switch (i % 6)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
    }
}

static Color GetHueColor(float hue)
{
    float r = 0.0f, g = 0.0f, b = 0.0f;
    HSVtoRGB(hue, 1.0f, 1.0f, r, g, b);
    return Color((uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255), 255);
}

 

bool GUI::ColorPicker(const char *label, Color *color, float x, float y, float w)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return false;
    if (!color)
        return false;

    Control *control = AddControl();
    if (!control)
        return false;
    control->changed = false;

    float spacing = m_theme.itemSpacing;

    // Label
    if (label && label[0] != '\0')
    {
        Label(label, x, y);
        y += GetTextHeight(label) + spacing;
    }

    // Estado HSV por controlo
    static std::unordered_map<u32, Vec3> hsvState;

    float h0, s0, v0;
    RGBtoHSV(color->r / 255.0f, color->g / 255.0f, color->b / 255.0f, h0, s0, v0);

    if (hsvState.find(control->id) == hsvState.end())
        hsvState[control->id] = Vec3(h0, s0, v0);

    Vec3 &hsv = hsvState[control->id];

    // ========================================
    // SV Box
    // ========================================
    float boxSize = w;
    FloatRect svBox = MakeContentRect(x, y, boxSize, boxSize);

    int gridSize = 32;
    float cellW = svBox.width / gridSize;
    float cellH = svBox.height / gridSize;

    for (int iy = 0; iy < gridSize; ++iy)
    {
        for (int ix = 0; ix < gridSize; ++ix)
        {
            float saturation = ix / (float)(gridSize - 1);
            float value      = 1.0f - (iy / (float)(gridSize - 1));

            float r=0.0f, g=0.0f, b=0.0f;
            HSVtoRGB(hsv.x, saturation, value, r, g, b);

            FloatRect cell{
                svBox.x + ix * cellW,
                svBox.y + iy * cellH,
                cellW + 1.0f,
                cellH + 1.0f};

            DrawRectFilled(cell,
                           Color((uint8_t)(r * 255),
                                 (uint8_t)(g * 255),
                                 (uint8_t)(b * 255),
                                 255));
        }
    }

    DrawRectOutline(svBox, m_theme.colorPickerBorder, 2.0f);

    // Cursor SV
    float cursorX = svBox.x + hsv.y * svBox.width;
    float cursorY = svBox.y + (1.0f - hsv.z) * svBox.height;

    DrawRectOutline(FloatRect{cursorX - 5, cursorY - 5, 10, 10}, Color(255, 255, 255, 255), 2.0f);
    DrawRectOutline(FloatRect{cursorX - 4, cursorY - 4, 8, 8},   Color(0, 0, 0, 255),       1.0f);

    bool svHovered = IsPointInRect(m_mousePos, svBox);

    // Qual SV box está a ser arrastado
    static u32 activeSVId = 0;

    if (svHovered && Input::IsMousePressed(MouseButton::LEFT))
        activeSVId = control->id;

    if (activeSVId == control->id && Input::IsMouseDown(MouseButton::LEFT))
    {
        float sx = (m_mousePos.x - svBox.x) / svBox.width;
        float sy = (m_mousePos.y - svBox.y) / svBox.height;

        sx = Clamp(sx, 0.0f, 1.0f);
        sy = Clamp(sy, 0.0f, 1.0f);

        hsv.y = sx;          // S
        hsv.z = 1.0f - sy;   // V

        float r, g, b;
        HSVtoRGB(hsv.x, hsv.y, hsv.z, r, g, b);
        color->r = (uint8_t)(r * 255);
        color->g = (uint8_t)(g * 255);
        color->b = (uint8_t)(b * 255);
        control->changed = true;
    }

    if (!Input::IsMouseDown(MouseButton::LEFT))
        activeSVId = 0;

    y += boxSize + spacing * 2.0f;

    // ========================================
    // Hue Slider
    // ========================================
    float hueBarH = 20.0f;
    FloatRect hueBar = MakeContentRect(x, y, boxSize, hueBarH);

    int hueSteps = 60;
    float stepW = hueBar.width / hueSteps;

    for (int i = 0; i < hueSteps; ++i)
    {
        float hue = (i / (float)hueSteps) * 360.0f;
        Color hCol = GetHueColor(hue);

        FloatRect step{hueBar.x + i * stepW, hueBar.y, stepW + 1.0f, hueBar.height};
        DrawRectFilled(step, hCol);
    }

    DrawRectOutline(hueBar, m_theme.colorPickerBorder, 2.0f);

    float hueX = hueBar.x + (hsv.x / 360.0f) * hueBar.width;
    DrawRectFilled  (FloatRect{hueX - 2, hueBar.y - 2, 4, hueBar.height + 4}, Color(255, 255, 255, 255));
    DrawRectOutline (FloatRect{hueX - 2, hueBar.y - 2, 4, hueBar.height + 4}, Color(0, 0, 0, 255), 1.0f);

    bool hueHovered = IsPointInRect(m_mousePos, hueBar);
    static u32 activeHueId = 0;

    if (hueHovered && Input::IsMousePressed(MouseButton::LEFT))
        activeHueId = control->id;

    if (activeHueId == control->id && Input::IsMouseDown(MouseButton::LEFT))
    {
        float hx = (m_mousePos.x - hueBar.x) / hueBar.width;
        hx = Clamp(hx, 0.0f, 1.0f);

        hsv.x = hx * 360.0f;

        float r, g, b;
        HSVtoRGB(hsv.x, hsv.y, hsv.z, r, g, b);
        color->r = (uint8_t)(r * 255);
        color->g = (uint8_t)(g * 255);
        color->b = (uint8_t)(b * 255);
        control->changed = true;
    }

    if (!Input::IsMouseDown(MouseButton::LEFT))
        activeHueId = 0;

    y += hueBarH + spacing * 2.0f;

    // Preview
    FloatRect preview = MakeContentRect(x, y, boxSize, 40.0f);
    DrawRectFilled(preview, *color);
    DrawRectOutline(preview, m_theme.colorPickerBorder, 2.0f);

    return control->changed;
}

void GUI::ProgressBar(float progress, float x, float y, float w, float h,
                      Orientation orient, const char *overlay)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return;

    progress = Clamp(progress, 0.0f, 1.0f);

    FloatRect bgRect = MakeContentRect(x, y, w, h);

    DrawRectFilled(bgRect, m_theme.progressBarBg);
    DrawRectOutline(bgRect, m_theme.windowBorder, 1.0f);

    if (progress > 0.0f)
    {
        FloatRect fillRect = bgRect;

        if (orient == Orientation::Horizontal)
        {
            fillRect.width = bgRect.width * progress;
        }
        else
        {
            float fillHeight = bgRect.height * progress;
            fillRect.y = bgRect.y + bgRect.height - fillHeight;
            fillRect.height = fillHeight;
        }

        DrawRectFilled(fillRect, m_theme.progressBarFill);
    }

    if (overlay)
    {
        float tw = GetTextWidth(overlay);
        float th = GetTextHeight(overlay);
        float tx = bgRect.x + (bgRect.width  - tw) * 0.5f;
        float ty = bgRect.y + (bgRect.height - th) * 0.5f;
        DrawText(overlay, tx, ty, m_theme.labelText);
    }
}

bool GUI::ListBox(const char *label, int *selectedIndex, const char **items, int itemCount,
                  float x, float y, float w, float h, int visibleItems)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return false;
    if (!selectedIndex || !items || itemCount <= 0 || visibleItems <= 0)
        return false;

    Control *control = AddControl();
    if (!control)
        return false;
    control->changed = false;

    if (label && label[0] != '\0')
    {
        Label(label, x, y);
        y += GetTextHeight(label) + m_theme.itemSpacing;
    }

    FloatRect boxRect = MakeContentRect(x, y, w, h);

    DrawRectFilled(boxRect, m_theme.listBoxBg);
    DrawRectOutline(boxRect, m_theme.listBoxBorder, 2.0f);

    float itemH = h / (float)visibleItems;
    if (itemCount < visibleItems)
        itemH = h / (float)itemCount;

    static std::unordered_map<u32, float> scrollMap;
    float &scrollOffset = scrollMap[control->id];

    float maxScroll = (itemCount - visibleItems) * itemH;
    if (maxScroll < 0.0f) maxScroll = 0.0f;
    scrollOffset = Clamp(scrollOffset, 0.0f, maxScroll);

    bool boxHovered = IsPointInRect(m_mousePos, boxRect);
    bool isScrolling = false;

    static u32 draggingScrollbarId = 0;
    static float dragStartY = 0.0f;
    static float dragStartOffset = 0.0f;

    int firstVisible = (int)(scrollOffset / itemH);
    int lastVisible  = firstVisible + visibleItems;
    if (lastVisible > itemCount) lastVisible = itemCount;

    // Scrollbar
    if (itemCount > visibleItems)
    {
        float scrollbarX = boxRect.x + boxRect.width - m_theme.scrollbarSize;
        float scrollbarH = boxRect.height;

        FloatRect trackRect{scrollbarX, boxRect.y, m_theme.scrollbarSize, scrollbarH};
        DrawRectFilled(trackRect, m_theme.sliderBg);

        float handleHeight = (visibleItems / (float)itemCount) * scrollbarH;
        if (handleHeight < 20.0f) handleHeight = 20.0f;

        float handleY = boxRect.y + (scrollOffset / maxScroll) * (scrollbarH - handleHeight);
        FloatRect handleRect{scrollbarX, handleY, m_theme.scrollbarSize, handleHeight};

        bool handleHovered = IsPointInRect(m_mousePos, handleRect);
        Color handleColor = handleHovered ? m_theme.sliderHandleHover : m_theme.sliderHandle;
        DrawRectFilled(handleRect, handleColor);

        if (handleHovered && Input::IsMousePressed(MouseButton::LEFT))
        {
            draggingScrollbarId = control->id;
            dragStartY = m_mousePos.y;
            dragStartOffset = scrollOffset;
        }

        if (draggingScrollbarId == control->id && Input::IsMouseDown(MouseButton::LEFT))
        {
            float deltaY = m_mousePos.y - dragStartY;
            float deltaScroll = (deltaY / (scrollbarH - handleHeight)) * maxScroll;
            scrollOffset = Clamp(dragStartOffset + deltaScroll, 0.0f, maxScroll);
            isScrolling = true;
        }

        if (!Input::IsMouseDown(MouseButton::LEFT))
            draggingScrollbarId = 0;
    }

    for (int i = firstVisible; i < lastVisible; ++i)
    {
        float itemY = boxRect.y + (i - firstVisible) * itemH;

        float itemWidth = boxRect.width;
        if (itemCount > visibleItems)
            itemWidth -= m_theme.scrollbarSize;

        FloatRect itemRect{boxRect.x, itemY, itemWidth, itemH};

        bool itemHovered = IsPointInRect(m_mousePos, itemRect) && boxHovered && !isScrolling;
        bool isSelected  = (i == *selectedIndex);

        Color itemBg = m_theme.listBoxBg;
        if (isSelected)      itemBg = m_theme.listBoxItemSelected;
        else if (itemHovered) itemBg = m_theme.listBoxItemHover;

        DrawRectFilled(itemRect, itemBg);

        const char *txt = items[i] ? items[i] : "";
        float textY = itemRect.y + (itemRect.height - GetTextHeight(txt)) * 0.5f;
        DrawText(txt, itemRect.x + 8.0f, textY, m_theme.buttonText);

        if (itemHovered && Input::IsMousePressed(MouseButton::LEFT))
        {
            *selectedIndex = i;
            control->changed = true;
        }
    }

    return control->changed;
}

bool GUI::Dropdown(const char *label, int *selectedIndex, const char **items, int itemCount,
                   float x, float y, float w, float h)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return false;
    if (!selectedIndex || !items || itemCount <= 0)
        return false;

    Control *control = AddControl();
    if (!control)
        return false;
    control->changed = false;

    FloatRect rect = MakeContentRect(x, y, w, h);

    // Estado aberto/fechado
    static std::unordered_map<u32, bool> openMap;
    bool &isOpen = openMap[control->id];

    // Label opcional por cima
    if (label && label[0] != '\0')
    {
        float lh = GetTextHeight(label);
        DrawText(label, rect.x, rect.y - lh - m_theme.itemSpacing, m_theme.labelText);
    }

    bool buttonHovered = IsPointInRect(m_mousePos, rect);

    if (buttonHovered && Input::IsMousePressed(MouseButton::LEFT))
    {
        isOpen = !isOpen;
    }

    Color bgColor = m_theme.dropdownBg;
    if (buttonHovered) bgColor = m_theme.dropdownHover;
    if (isOpen)        bgColor = m_theme.buttonActive;

    DrawRectFilled(rect, bgColor);
    DrawRectOutline(rect, m_theme.dropdownBorder, 1.0f);

    int idx = (*selectedIndex >= 0 && *selectedIndex < itemCount) ? *selectedIndex : 0;
    const char *currentText = items[idx] ? items[idx] : "Select...";
    float textY = rect.y + (rect.height - GetTextHeight(currentText)) * 0.5f;
    DrawText(currentText, rect.x + 8.0f, textY, m_theme.buttonText);

    const char *arrow = isOpen ? "^" : "v";
    DrawText(arrow,
             rect.x + rect.width - GetTextWidth(arrow) - 8.0f,
             textY,
             m_theme.buttonText);

    if (!isOpen)
        return control->changed;

    // Lista aberta
    float itemH = std::max(h, m_theme.dropdownHeight);
    FloatRect listRect(rect.x, rect.y + rect.height + 2.0f, rect.width, itemH * itemCount);

    DrawRectFilled(listRect, m_theme.windowBg);
    DrawRectOutline(listRect, m_theme.windowBorder, 1.0f);

    for (int i = 0; i < itemCount; ++i)
    {
        FloatRect itemRect(listRect.x, listRect.y + i * itemH, listRect.width, itemH);
        bool itemHovered = IsPointInRect(m_mousePos, itemRect);

        Color itemBg = m_theme.dropdownBg;
        if (i == idx)         itemBg = m_theme.buttonActive;
        else if (itemHovered) itemBg = m_theme.dropdownItemHover;

        DrawRectFilled(itemRect, itemBg);
        DrawRectOutline(itemRect, m_theme.dropdownBorder, 1.0f);

        const char *txt = items[i] ? items[i] : "";
        float itY = itemRect.y + (itemRect.height - GetTextHeight(txt)) * 0.5f;
        DrawText(txt, itemRect.x + 8.0f, itY, m_theme.buttonText);

        if (itemHovered && Input::IsMousePressed(MouseButton::LEFT))
        {
            if (i != *selectedIndex)
            {
                *selectedIndex = i;
                control->changed = true;
            }
            isOpen = false;
        }
    }

    // Clique fora fecha
    if (Input::IsMousePressed(MouseButton::LEFT) &&
        !IsPointInRect(m_mousePos, rect) &&
        !IsPointInRect(m_mousePos, listRect))
    {
        isOpen = false;
    }

    return control->changed;
}

bool GUI::RadioButton(const char *label, int *selected, int value, float x, float y, float size)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return false;
    if (!selected)
        return false;

    if (size <= 0.0f)
        size = m_theme.radioSize;

    FloatRect circleRect = MakeContentRect(x, y, size, size);

    bool hovered = IsPointInRect(m_mousePos, circleRect);
    bool isSelected = (*selected == value);

    Color bgColor = m_theme.radioBg;
    if (hovered)
        bgColor = m_theme.buttonHover;

    DrawRectFilled(circleRect, bgColor);
    DrawRectOutline(circleRect, m_theme.radioBorder, 1.0f);

    if (isSelected)
    {
        FloatRect dotRect = circleRect;
        float margin = size * 0.25f;
        dotRect.x      += margin;
        dotRect.y      += margin;
        dotRect.width  -= margin * 2.0f;
        dotRect.height -= margin * 2.0f;
        DrawRectFilled(dotRect, m_theme.radioCheck);
    }

    float labelX = circleRect.x + circleRect.width + 8.0f;
    float labelY = circleRect.y + (circleRect.height - GetTextHeight(label)) * 0.5f;
    DrawText(label, labelX, labelY, m_theme.labelText);

    if (hovered && Input::IsMousePressed(MouseButton::LEFT))
    {
        *selected = value;
        return true;
    }

    return false;
}

bool GUI::ToggleSwitch(const char *label, bool *value, float x, float y, float w, float h)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return false;
    if (!value)
        return false;

    if (h <= 0.0f)
        h = m_theme.checkboxSize;
    if (w <= 0.0f)
        w = h * 2.0f;

    Control *control = AddControl();
    if (!control)
        return false;

    control->changed = false;

    FloatRect rect = MakeContentRect(x, y, w, h);
    bool hovered = IsPointInRect(m_mousePos, rect);

    if (hovered && Input::IsMousePressed(MouseButton::LEFT))
    {
        *value = !*value;
        control->changed = true;
    }

    Color bgOff = m_theme.checkboxBg;
    Color bgOn  = m_theme.sliderFill;
    Color bg    = *value ? bgOn : bgOff;

    if (hovered)
        bg = LerpColor(bg, m_theme.buttonHover, 0.35f);

    if (!m_batch)
        return control->changed;

    float radius = rect.height * 0.5f;
    float handleR = radius - 2.0f;
    float cxOff   = rect.x + handleR;
    float cxOn    = rect.x + rect.width - handleR;
    float cy      = rect.y + radius;
 
    m_batch->SetColor(bg);
    m_batch->RoundedRectangle((int)rect.x, (int)rect.y,
                              (int)rect.width, (int)rect.height,
                              radius, 8, true);

    m_batch->SetColor(m_theme.checkboxBorder);
    m_batch->RoundedRectangle((int)rect.x, (int)rect.y,
                              (int)rect.width, (int)rect.height,
                              radius, 8, false);



    float cx = *value ? cxOn : cxOff;

    Color handleColor = hovered ? m_theme.sliderHandleHover : m_theme.sliderHandle;
    m_batch->SetColor(handleColor);
    m_batch->Circle((int)cx, (int)cy, handleR, true);

    m_batch->SetColor(m_theme.windowBorder);
    m_batch->Circle((int)cx, (int)cy, handleR, false);

    if (label && label[0] != '\0')
    {
        float textH = GetTextHeight(label);
        float textY = rect.y + (rect.height - textH) * 0.5f;
        DrawText(label, rect.x + rect.width + m_theme.itemSpacing * 2.0f,
                 textY, m_theme.labelText);
    }

    return control->changed;
}

void GUI::SeparatorText(const char *text, float x, float y, float w)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return;

    if (!text || text[0] == '\0')
    {
        Separator(x, y, w);
        return;
    }

    Vec2 base = MakeContentPos(x, y);
    float textW = GetTextWidth(text);
    float textH = GetTextHeight(text);

    float padding = 6.0f;

    float centerX = base.x + w * 0.5f;
    float textX   = centerX - textW * 0.5f;
    float textY   = base.y - textH * 0.5f;

    float lineY = base.y;

    float leftStart  = base.x;
    float leftEnd    = textX - padding;
    float rightStart = textX + textW + padding;
    float rightEnd   = base.x + w;

    if (m_batch)
    {
        m_batch->SetColor(m_theme.separatorColor);
        if (leftEnd > leftStart)
            m_batch->Line2D((int)leftStart, (int)lineY,
                            (int)leftEnd,   (int)lineY);
        if (rightEnd > rightStart)
            m_batch->Line2D((int)rightStart, (int)lineY,
                            (int)rightEnd,  (int)lineY);
    }

    DrawText(text, textX, textY, m_theme.labelText);
}

void GUI::Spinner(float x, float y, float radius)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return;

    Control *control = AddControl();
    if (!control)
        return;

    float dt = Device::Instance().GetFrameTime();
    control->ftag += dt;
    if (control->ftag > 1.0f)
        control->ftag -= 1.0f;

    float startAngleDeg = control->ftag * 360.0f;
    float arcDeg        = 270.0f;
    float endAngleDeg   = startAngleDeg + arcDeg;

    const int segments = 32;
    Vec2 points[segments + 1];

    float startRad = startAngleDeg * 3.14159265f / 180.0f;
    float endRad   = endAngleDeg   * 3.14159265f / 180.0f;
    float step     = (endRad - startRad) / segments;

    float cx, cy;
    {
        Vec2 p = MakeContentPos(x, y);
        cx = p.x;
        cy = p.y;
    }

    for (int i = 0; i <= segments; ++i)
    {
        float a = startRad + step * i;
        points[i].x = cx + std::cos(a) * radius;
        points[i].y = cy + std::sin(a) * radius;
    }

    if (m_batch)
    {
        m_batch->SetColor(m_theme.buttonActive);
        m_batch->Polyline(points, segments + 1);
    }
}
bool GUI::DragFloat(const char *label, float *value,
                    float speed,
                    float min, float max,
                    float x, float y, float w, float h)
{
    return DragFloat(label, value, speed, min, max,
                     x, y, w, h, m_theme.accent);
}

bool GUI::DragFloat(const char *label, float *value,
                    float speed,
                    float min, float max,
                    float x, float y, float w, float h,
                    const Color &accentColor)
{
    WindowData *win = GetCurrentWindow();
    if (!win || win->isMinimized || !win->isOpen)
        return false;
    if (!value)
        return false;

    if (h <= 0.0f)
        h = m_theme.sliderHeight;
    if (w <= 0.0f)
        w = m_maxItemWidth;

    Control *control = AddControl();
    if (!control)
        return false;

    control->changed = false;
    FloatRect rect = MakeContentRect(x, y, w, h);

    float labelW = 0.0f;
    if (label && label[0] != '\0')
        labelW = GetTextWidth(label) + m_theme.itemSpacing * 2.0f;

    float boxW = 80.0f;
    if (labelW + boxW > rect.width)
        boxW = rect.width - labelW;

    FloatRect box(rect.x + labelW, rect.y, boxW, rect.height);

    bool hovered = IsPointInRect(m_mousePos, box);

    if (hovered && Input::IsMousePressed(MouseButton::LEFT))
    {
        control->active = true;
        control->ftag   = *value;
        control->itag   = (int)m_mousePos.x;
    }

    if (control->active && Input::IsMouseDown(MouseButton::LEFT))
    {
        float dx       = m_mousePos.x - (float)control->itag;
        float newValue = control->ftag + dx * speed;

        if (min < max)
            newValue = Clamp(newValue, min, max);

        if (newValue != *value)
        {
            *value = newValue;
            control->changed = true;
        }
    }

    if (control->active && Input::IsMouseReleased(MouseButton::LEFT))
    {
        control->active = false;
    }

    Color bg       = m_theme.textInputBg;
    Color border   = m_theme.textInputBorder;
    Color textCol  = m_theme.textInputText;
    Color labelCol = m_theme.labelText;

    if (hovered)
        bg = LerpColor(bg, m_theme.buttonHover, 0.35f);

    border   = accentColor;
    labelCol = accentColor;
    if (control->active)
    {
    }

    DrawRectFilled(box, bg);
    DrawRectOutline(box, border, 1.0f);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3f", *value);
    float valW = GetTextWidth(buf);
    float valH = GetTextHeight(buf);
    float vx = box.x + (box.width - valW) * 0.5f;
    float vy = box.y + (box.height - valH) * 0.5f;
    DrawText(buf, vx, vy, textCol);

    if (label && label[0] != '\0')
    {
        float lh = GetTextHeight(label);
        float ly = rect.y + (rect.height - lh) * 0.5f;
        DrawText(label, rect.x, ly, labelCol);
    }

    return control->changed;
}
