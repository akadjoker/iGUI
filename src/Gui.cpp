#include "igui/Gui.hpp"

#include <ct/sort.hpp>

namespace ig
{

namespace
{

Color colorFromHsv(float hue, float saturation, float value, uint8_t alpha)
{
    hue = hue < 0.0f ? hue + 1.0f : (hue >= 1.0f ? hue - 1.0f : hue);
    saturation = clamp(saturation, 0.0f, 1.0f);
    value = clamp(value, 0.0f, 1.0f);
    const float scaled = hue * 6.0f;
    const int sector = static_cast<int>(scaled);
    const float fraction = scaled - static_cast<float>(sector);
    const float p = value * (1.0f - saturation);
    const float q = value * (1.0f - saturation * fraction);
    const float t = value * (1.0f - saturation * (1.0f - fraction));
    float red = value;
    float green = t;
    float blue = p;
    switch (sector % 6)
    {
    case 0: red = value; green = t; blue = p; break;
    case 1: red = q; green = value; blue = p; break;
    case 2: red = p; green = value; blue = t; break;
    case 3: red = p; green = q; blue = value; break;
    case 4: red = t; green = p; blue = value; break;
    default: red = value; green = p; blue = q; break;
    }
    return Color(static_cast<uint8_t>(red * 255.0f + 0.5f),
                 static_cast<uint8_t>(green * 255.0f + 0.5f),
                 static_cast<uint8_t>(blue * 255.0f + 0.5f), alpha);
}

void colorToHsv(const Color &color, float &hue, float &saturation, float &value)
{
    const float red = static_cast<float>(color.r) / 255.0f;
    const float green = static_cast<float>(color.g) / 255.0f;
    const float blue = static_cast<float>(color.b) / 255.0f;
    const float maximum = red > green ? (red > blue ? red : blue) : (green > blue ? green : blue);
    const float minimum = red < green ? (red < blue ? red : blue) : (green < blue ? green : blue);
    const float delta = maximum - minimum;
    value = maximum;
    saturation = maximum > 0.0f ? delta / maximum : 0.0f;
    if (delta == 0.0f)
    {
        hue = 0.0f;
        return;
    }
    if (maximum == red)
        hue = (green - blue) / delta;
    else if (maximum == green)
        hue = 2.0f + (blue - red) / delta;
    else
        hue = 4.0f + (red - green) / delta;
    hue /= 6.0f;
    if (hue < 0.0f)
        hue += 1.0f;
}

} // namespace

Context::PointerState::PointerState()
    : position(), wheelX(0.0f), wheelY(0.0f)
{
    for (uint32_t i = 0; i < 3u; ++i)
    {
        pressedPosition[i] = Vec2();
        releasedPosition[i] = Vec2();
        down[i] = false;
        pressed[i] = false;
        released[i] = false;
    }
}

Context::Context(Backend &backend, TextProvider *textProvider)
    : backend_(backend), textProvider_(textProvider), theme_(), frame_(), pointer_(), layout_(), events_(), textEvents_(),
      windows_(), windowsById_(), listScrolls_(), windowOrder_(), idStack_(), frameDrawList_(), drawData_(),
      currentWindow_(), focusedWindow_(), draggingWindow_(), resizingWindow_(), activeWidget_(InvalidWidgetId),
      hotWidget_(InvalidWidgetId), lastItemId_(InvalidWidgetId),
      focusedWidget_(InvalidWidgetId), textInputWidget_(InvalidWidgetId), openCombo_(InvalidWidgetId),
      textCursor_(0), frameNumber_(0), nextZOrder_(1), windowDragOffset_(),
      wantsKeyboard_(false), wantsTextInput_(false), backspacePressed_(false), enterPressed_(false),
      homePressed_(false), endPressed_(false), upPressed_(false), downPressed_(false),
      copyRequested_(false), pasteRequested_(false)
{
    events_.reserve(32);
    windows_.reserve(8);
    windowsById_.reserve(8);
    listScrolls_.reserve(8);
    windowOrder_.reserve(8);
    idStack_.reserve(8);
    textEvents_.reserve(4);
}

void Context::pushEvent(const Event &event)
{
    events_.push_back(event);
}

void Context::beginFrame(const FrameInfo &frame)
{
    frame_ = frame;
    ++frameNumber_;
    pointer_.wheelX = 0.0f;
    pointer_.wheelY = 0.0f;
    for (uint32_t i = 0; i < 3u; ++i)
    {
        pointer_.pressed[i] = false;
        pointer_.released[i] = false;
    }

    frameDrawList_.clear();
    for (ct::SlotMap<WindowState>::iterator it = windows_.begin(); it != windows_.end(); ++it)
    {
        it->drawList.clear();
        it->overlayDrawList.clear();
    }
    hotWidget_ = InvalidWidgetId;
    lastItemId_ = InvalidWidgetId;
    wantsKeyboard_ = false;
    wantsTextInput_ = false;
    backspacePressed_ = false;
    enterPressed_ = false;
    homePressed_ = false;
    endPressed_ = false;
    upPressed_ = false;
    downPressed_ = false;
    copyRequested_ = false;
    pasteRequested_ = false;
    textEvents_.clear();
    consumeEvents();

    const uint32_t leftButton = buttonIndex(PointerButton::Left);
    if (pointer_.pressed[leftButton])
        focusWindow(topWindowAt(pointer_.pressedPosition[buttonIndex(PointerButton::Left)]));
}

const DrawData &Context::endFrame()
{
    ct::sort(windowOrder_.begin(), windowOrder_.end(), [this](WindowHandle a, WindowHandle b)
    {
        const WindowState *left = windows_.get(a);
        const WindowState *right = windows_.get(b);
        if (!left || !right)
            return left != nullptr;
        return left->zOrder < right->zOrder;
    });

    frameDrawList_.clear();
    for (ct::Vector<WindowHandle>::size_type i = 0; i < windowOrder_.size(); ++i)
    {
        const WindowState *window = windows_.get(windowOrder_[i]);
        if (window && window->open)
        {
            frameDrawList_.append(window->drawList);
            frameDrawList_.append(window->overlayDrawList);
        }
    }

    currentWindow_ = WindowHandle();
    drawData_ = frameDrawList_.data(frame_.displaySize, frame_.dpiScale);
    return drawData_;
}

bool Context::beginWindow(StringView title, const Rect &initialBounds, bool *open)
{
    const WidgetId id = hashText(title);
    WindowState *window = getOrCreateWindow(id, title, initialBounds);
    if (!window)
        return false;

    if (open)
        window->open = *open;

    if (!window->open)
    {
        if (windowsById_[id] == focusedWindow_)
            focusedWindow_ = WindowHandle();
        if (open)
            *open = false;
        return false;
    }

    currentWindow_ = windowsById_[id];
    const WindowState *focused = windows_.get(focusedWindow_);
    if (!focused || !focused->open)
        focusWindow(currentWindow_);
    drawWindow(*window);
    if (open)
        *open = window->open;
    if (!window->open || window->minimized)
    {
        currentWindow_ = WindowHandle();
        idStack_.clear();
        layout_ = LayoutState();
        return false;
    }
    beginLayout(*window);
    idStack_.clear();
    idStack_.push_back(id);
    return true;
}

void Context::endWindow()
{
    WindowState *window = currentWindow();
    if (window && !window->minimized)
    {
        const float gripSize = 12.0f;
        const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
        const float right = window->bounds.x + window->bounds.width;
        const float bottom = window->bounds.y + window->bounds.height;
        window->drawList.addLine(Vec2(right - gripSize, bottom - 2.0f),
                                 Vec2(right - 2.0f, bottom - gripSize),
                                 theme_.borderColor, viewport, 1.0f);
        window->drawList.addLine(Vec2(right - gripSize * 0.55f, bottom - 2.0f),
                                 Vec2(right - 2.0f, bottom - gripSize * 0.55f),
                                 theme_.borderColor, viewport, 1.0f);
    }
    currentWindow_ = WindowHandle();
    idStack_.clear();
    layout_ = LayoutState();
}

void Context::pushId(uint64_t id)
{
    const WidgetId parent = idStack_.empty() ? InvalidWidgetId : idStack_.back();
    idStack_.push_back(combineIds(parent, id));
}

void Context::pushId(StringView id)
{
    pushId(hashText(id));
}

void Context::popId()
{
    if (!idStack_.empty())
        idStack_.pop_back();
}

void Context::setTheme(const Theme &theme)
{
    theme_ = theme;
}

const Theme &Context::theme() const
{
    return theme_;
}

bool Context::wantsPointer() const
{
    return activeWidget_ != InvalidWidgetId || hotWidget_ != InvalidWidgetId;
}

bool Context::wantsKeyboard() const
{
    return wantsKeyboard_;
}

bool Context::wantsTextInput() const
{
    return wantsTextInput_;
}

bool Context::button(StringView labelText, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;
    const bool hovered = itemHovered(rect, clip, id);
    const bool clicked = itemClicked(rect, clip, id);
    const Color background = hovered ? theme_.buttonHovered : theme_.buttonBackground;
    drawList->addRectFilled(rect, background, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const Vec2 textPosition(rect.x + (rect.width - metrics.width) * 0.5f,
                            rect.y + (rect.height - metrics.height) * 0.5f);
    drawText(*drawList, theme_.font, labelText, textPosition, theme_.fontSize,
             theme_.buttonText, clip);
    return clicked;
}

bool Context::checkbox(StringView labelText, bool &value, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect box = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;
    const bool hovered = itemHovered(box, clip, id);
    const bool clicked = itemClicked(box, clip, id);
    if (clicked)
        value = !value;

    const Color checkboxColor = value ? theme_.checkboxChecked
                                      : (hovered ? theme_.buttonHovered : theme_.checkboxBackground);
    drawList->addRectFilled(box, checkboxColor, clip);
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const Vec2 textPosition(box.x + box.width + theme_.windowPadding * 0.5f,
                            box.y + (box.height - metrics.height) * 0.5f);
    drawText(*drawList, theme_.font, labelText, textPosition, theme_.fontSize,
             theme_.labelText, clip);
    return clicked;
}

bool Context::toggleSwitch(StringView labelText, bool &value, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const bool hovered = itemHovered(rect, clip, id);
    const bool clicked = itemClicked(rect, clip, id);
    if (clicked)
        value = !value;

    const float trackHeight = rect.height * 0.62f;
    const float radius = trackHeight * 0.5f;
    const Rect track(rect.x, rect.y + (rect.height - trackHeight) * 0.5f,
                     rect.width, trackHeight);
    const Color trackColor = value ? theme_.checkboxChecked
                                   : (hovered ? theme_.buttonHovered : theme_.checkboxBackground);
    if (track.width > radius * 2.0f)
        drawList->addRectFilled(Rect(track.x + radius, track.y,
                                     track.width - radius * 2.0f, track.height), trackColor, clip);
    drawList->addCircleFilled(Vec2(track.x + radius, track.y + radius), radius, trackColor, clip);
    drawList->addCircleFilled(Vec2(track.x + track.width - radius, track.y + radius), radius,
                              trackColor, clip);

    const float handleX = value ? track.x + track.width - radius : track.x + radius;
    drawList->addCircleFilled(Vec2(handleX, track.y + radius), radius * 0.72f,
                              theme_.switchThumb, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    drawText(*drawList, theme_.font, labelText,
             Vec2(rect.x + rect.width + theme_.windowPadding * 0.5f,
                  rect.y + (rect.height - metrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, clip);
    return clicked;
}

bool Context::radioButton(StringView labelText, bool selected, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect box = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;
    const bool hovered = itemHovered(box, clip, id);
    const bool clicked = itemClicked(box, clip, id);
    const float radius = (box.width < box.height ? box.width : box.height) * 0.5f;
    const Vec2 center(box.x + box.width * 0.5f, box.y + box.height * 0.5f);
    const Color background = hovered ? theme_.buttonHovered : theme_.radioBackground;
    drawList->addCircleFilled(center, radius, background, clip);
    if (selected)
        drawList->addCircleFilled(center, radius * 0.52f, theme_.radioChecked, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const Vec2 textPosition(box.x + box.width + theme_.windowPadding * 0.5f,
                            box.y + (box.height - metrics.height) * 0.5f);
    drawText(*drawList, theme_.font, labelText, textPosition, theme_.fontSize,
             theme_.labelText, clip);
    return clicked;
}

bool Context::selectable(StringView labelText, bool selected, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;

    const bool hovered = itemHovered(rect, clip, id);
    const bool clicked = itemClicked(rect, clip, id);
    const Color background = selected ? theme_.selectableSelected
                                      : (hovered ? theme_.selectableHovered
                                                 : theme_.selectableBackground);
    drawList->addRectFilled(rect, background, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const Vec2 textPosition(rect.x + theme_.windowPadding * 0.5f,
                            rect.y + (rect.height - metrics.height) * 0.5f);
    drawText(*drawList, theme_.font, labelText, textPosition, theme_.fontSize,
             theme_.labelText, clip);
    return clicked;
}

bool Context::collapsingHeader(StringView labelText, bool &expanded, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;

    const bool hovered = itemHovered(rect, clip, id);
    if (itemClicked(rect, clip, id))
        expanded = !expanded;

    drawList->addRectFilled(rect, hovered ? theme_.buttonHovered : theme_.collapsibleHeaderBg, clip);
    const float arrowSize = rect.height * 0.22f;
    const float arrowX = rect.x + theme_.windowPadding;
    const float arrowY = rect.y + rect.height * 0.5f;
    const Vec2 arrow[] = {
        expanded ? Vec2(arrowX, arrowY - arrowSize * 0.5f)
                 : Vec2(arrowX - arrowSize * 0.25f, arrowY - arrowSize),
        expanded ? Vec2(arrowX + arrowSize, arrowY - arrowSize * 0.5f)
                 : Vec2(arrowX - arrowSize * 0.25f, arrowY + arrowSize),
        expanded ? Vec2(arrowX + arrowSize * 0.5f, arrowY + arrowSize * 0.5f)
                 : Vec2(arrowX + arrowSize * 0.75f, arrowY)
    };
    drawList->addPolygonFilled(Span<const Vec2>(arrow), theme_.menuSubmenuArrow, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    drawText(*drawList, theme_.font, labelText,
             Vec2(arrowX + arrowSize + theme_.windowPadding,
                  rect.y + (rect.height - metrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, clip);
    return expanded;
}

bool Context::treeNode(StringView labelText, bool &expanded, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;

    const bool hovered = itemHovered(rect, clip, id);
    if (itemClicked(rect, clip, id))
        expanded = !expanded;

    if (hovered)
        drawList->addRectFilled(rect, theme_.buttonHovered, clip);
    const float arrowSize = rect.height * 0.20f;
    const float arrowX = rect.x + theme_.windowPadding * 0.75f;
    const float arrowY = rect.y + rect.height * 0.5f;
    const Vec2 arrow[] = {
        expanded ? Vec2(arrowX, arrowY - arrowSize * 0.5f)
                 : Vec2(arrowX - arrowSize * 0.25f, arrowY - arrowSize),
        expanded ? Vec2(arrowX + arrowSize, arrowY - arrowSize * 0.5f)
                 : Vec2(arrowX - arrowSize * 0.25f, arrowY + arrowSize),
        expanded ? Vec2(arrowX + arrowSize * 0.5f, arrowY + arrowSize * 0.5f)
                 : Vec2(arrowX + arrowSize * 0.75f, arrowY)
    };
    drawList->addPolygonFilled(Span<const Vec2>(arrow), theme_.menuSubmenuArrow, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    drawText(*drawList, theme_.font, labelText,
             Vec2(arrowX + arrowSize + theme_.windowPadding * 0.75f,
                  rect.y + (rect.height - metrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, clip);
    return expanded;
}

bool Context::tabBar(StringView labelText, int &currentItem, Span<const StringView> items,
                     const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || items.empty())
        return false;

    if (currentItem < 0 || static_cast<Span<const StringView>::size_type>(currentItem) >= items.size())
        currentItem = 0;
    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const float tabWidth = rect.width / static_cast<float>(items.size());
    bool changed = false;
    for (Span<const StringView>::size_type i = 0u; i < items.size(); ++i)
    {
        const Rect tab(rect.x + tabWidth * static_cast<float>(i), rect.y,
                       i + 1u == items.size() ? rect.x + rect.width -
                           (rect.x + tabWidth * static_cast<float>(i)) : tabWidth,
                       rect.height);
        const WidgetId tabId = combineIds(id, static_cast<WidgetId>(i + 1u));
        const bool hovered = itemHovered(tab, clip, tabId);
        if (itemClicked(tab, clip, tabId) && currentItem != static_cast<int>(i))
        {
            currentItem = static_cast<int>(i);
            changed = true;
        }
        const Color background = static_cast<int>(i) == currentItem ? theme_.selectableSelected
                               : (hovered ? theme_.selectableHovered : theme_.buttonBackground);
        drawList->addRectFilled(tab, background, clip);
        const TextMetrics metrics = measureText(theme_.font, items[i], theme_.fontSize);
        drawText(*drawList, theme_.font, items[i],
                 Vec2(tab.x + (tab.width - metrics.width) * 0.5f,
                      tab.y + (tab.height - metrics.height) * 0.5f),
                 theme_.fontSize, theme_.buttonText, clip);
    }
    return changed;
}

bool Context::listBox(StringView labelText, int &currentItem, Span<const StringView> items,
                      const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || items.empty())
        return false;

    if (currentItem < 0 || static_cast<Span<const StringView>::size_type>(currentItem) >= items.size())
        currentItem = 0;
    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    const Rect visible = intersect(rect, clip);
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f ||
        visible.width <= 0.0f || visible.height <= 0.0f)
        return false;

    const int rowCount = static_cast<int>(rect.height / theme_.widgetHeight);
    const int visibleRows = rowCount > 0 ? rowCount : 1;
    const int maximumScroll = static_cast<int>(items.size()) - visibleRows;
    int *scroll = listScrolls_.find(id);
    if (!scroll)
    {
        listScrolls_.put(id, 0);
        scroll = listScrolls_.find(id);
    }
    if (!scroll)
        return false;
    if (*scroll < 0)
        *scroll = 0;
    if (*scroll > maximumScroll)
        *scroll = maximumScroll > 0 ? maximumScroll : 0;

    const bool hovered = currentWindowReceivesPointer() && contains(visible, pointer_.position);
    if (hovered && pointer_.wheelY != 0.0f && maximumScroll > 0)
    {
        const int delta = pointer_.wheelY > 0.0f ? -1 : 1;
        *scroll += delta;
        if (*scroll < 0)
            *scroll = 0;
        if (*scroll > maximumScroll)
            *scroll = maximumScroll;
    }

    bool changed = false;
    if (focusedWidget_ == id && (upPressed_ || downPressed_))
    {
        const int previous = currentItem;
        if (upPressed_ && currentItem > 0)
            --currentItem;
        if (downPressed_ && currentItem + 1 < static_cast<int>(items.size()))
            ++currentItem;
        changed = currentItem != previous;
        if (currentItem < *scroll)
            *scroll = currentItem;
        if (currentItem >= *scroll + visibleRows)
            *scroll = currentItem - visibleRows + 1;
    }

    float scrollbarWidth = 0.0f;
    Rect scrollbar;
    Rect thumb;
    WidgetId scrollbarId = InvalidWidgetId;
    if (maximumScroll > 0)
    {
        scrollbarWidth = theme_.scrollbarWidth > 4.0f ? theme_.scrollbarWidth * 0.35f : 4.0f;
        const float fraction = static_cast<float>(visibleRows) / static_cast<float>(items.size());
        const float minimumThumb = theme_.scrollbarMinThumb < rect.height
                                       ? theme_.scrollbarMinThumb : rect.height;
        const float thumbHeight = rect.height * fraction > minimumThumb
                                      ? rect.height * fraction : minimumThumb;
        const float travel = rect.height - thumbHeight;
        const float offset = travel * static_cast<float>(*scroll) / static_cast<float>(maximumScroll);
        scrollbar = Rect(rect.x + rect.width - scrollbarWidth, rect.y, scrollbarWidth, rect.height);
        thumb = Rect(scrollbar.x, rect.y + offset, scrollbarWidth, thumbHeight);
        scrollbarId = combineIds(id, 0x5343524f4c4cull);
        const uint32_t left = buttonIndex(PointerButton::Left);
        const bool pressedThumb = pointer_.pressed[left] && currentWindow_ == focusedWindow_ &&
                                  activeWidget_ == InvalidWidgetId &&
                                  contains(intersect(thumb, visible), pointer_.pressedPosition[left]);
        if (pressedThumb)
        {
            activeWidget_ = scrollbarId;
            focusedWidget_ = id;
        }
        if (activeWidget_ == scrollbarId)
        {
            if (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left])
            {
                const float normalized = travel > 0.0f
                    ? clamp((pointer_.position.y - rect.y - thumb.height * 0.5f) / travel, 0.0f, 1.0f)
                    : 0.0f;
                const int nextScroll = static_cast<int>(normalized * static_cast<float>(maximumScroll) + 0.5f);
                *scroll = nextScroll < 0 ? 0 : (nextScroll > maximumScroll ? maximumScroll : nextScroll);
            }
            if (pointer_.released[left])
                activeWidget_ = InvalidWidgetId;
        }
        thumb.y = rect.y + travel * static_cast<float>(*scroll) / static_cast<float>(maximumScroll);
    }

    drawList->addRectFilled(rect, theme_.selectableBackground, visible);
    const int itemCount = static_cast<int>(items.size());
    for (int row = 0; row < visibleRows; ++row)
    {
        const int itemIndex = *scroll + row;
        if (itemIndex >= itemCount)
            break;
        const Rect item(rect.x, rect.y + theme_.widgetHeight * static_cast<float>(row),
                        rect.width - scrollbarWidth, theme_.widgetHeight);
        const WidgetId itemId = combineIds(id, static_cast<WidgetId>(itemIndex + 1));
        const bool itemHoveredValue = itemHovered(item, visible, itemId);
        if (itemClicked(item, visible, itemId))
        {
            if (currentItem != itemIndex)
            {
                currentItem = itemIndex;
                changed = true;
            }
            focusedWidget_ = id;
        }
        if (itemIndex == currentItem || itemHoveredValue)
            drawList->addRectFilled(item, itemIndex == currentItem
                                            ? theme_.selectableSelected : theme_.selectableHovered,
                                    visible);
        const TextMetrics metrics = measureText(theme_.font,
                                                 items[static_cast<Span<const StringView>::size_type>(itemIndex)],
                                                 theme_.fontSize);
        drawText(*drawList, theme_.font,
                 items[static_cast<Span<const StringView>::size_type>(itemIndex)],
                 Vec2(item.x + theme_.windowPadding * 0.5f,
                      item.y + (item.height - metrics.height) * 0.5f),
                 theme_.fontSize, theme_.labelText, visible);
    }

    if (maximumScroll > 0)
    {
        drawList->addRectFilled(scrollbar, theme_.inputBg, visible);
        drawList->addRectFilled(thumb, theme_.scrollbarThumb, visible);
    }
    return changed;
}

bool Context::comboBox(StringView labelText, int &currentItem, Span<const StringView> items,
                       const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || items.empty())
        return false;

    if (currentItem < 0 || static_cast<Span<const StringView>::size_type>(currentItem) >= items.size())
        currentItem = 0;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;

    const bool hovered = itemHovered(rect, clip, id);
    if (itemClicked(rect, clip, id))
        openCombo_ = openCombo_ == id ? InvalidWidgetId : id;

    drawList->addRectFilled(rect, hovered ? theme_.buttonHovered : theme_.buttonBackground, clip);
    const TextMetrics selectedMetrics = measureText(theme_.font, items[static_cast<Span<const StringView>::size_type>(currentItem)],
                                                    theme_.fontSize);
    drawText(*drawList, theme_.font, items[static_cast<Span<const StringView>::size_type>(currentItem)],
             Vec2(rect.x + theme_.windowPadding * 0.5f,
                  rect.y + (rect.height - selectedMetrics.height) * 0.5f),
             theme_.fontSize, theme_.buttonText, clip);

    const float arrowSize = rect.height * 0.2f;
    const Vec2 arrow[] = {
        Vec2(rect.x + rect.width - theme_.windowPadding - arrowSize, rect.y + rect.height * 0.42f),
        Vec2(rect.x + rect.width - theme_.windowPadding, rect.y + rect.height * 0.42f),
        Vec2(rect.x + rect.width - theme_.windowPadding - arrowSize * 0.5f, rect.y + rect.height * 0.62f)
    };
    drawList->addPolygonFilled(Span<const Vec2>(arrow), theme_.buttonText, clip);

    if (openCombo_ != id)
        return false;

    const Rect popup(rect.x, rect.y + rect.height, rect.width, rect.height * static_cast<float>(items.size()));
    const Rect popupClip = intersect(popup, clip);
    const uint32_t left = buttonIndex(PointerButton::Left);
    if (pointer_.pressed[left] && currentWindowReceivesPointer() &&
        !contains(rect, pointer_.pressedPosition[left]) &&
        !contains(popupClip, pointer_.pressedPosition[left]))
    {
        openCombo_ = InvalidWidgetId;
        return false;
    }

    // Popups are deferred to the window overlay so controls declared after a
    // combo box cannot paint over its open list.
    DrawList &popupDrawList = window->overlayDrawList;
    popupDrawList.addRectFilled(popup, theme_.selectableBackground, popupClip);
    for (Span<const StringView>::size_type i = 0; i < items.size(); ++i)
    {
        const Rect item(rect.x, rect.y + rect.height * static_cast<float>(i + 1u),
                        rect.width, rect.height);
        const WidgetId itemId = combineIds(id, static_cast<WidgetId>(i + 1u));
        const bool itemHoveredValue = itemHovered(item, popupClip, itemId);
        if (itemClicked(item, popupClip, itemId))
        {
            currentItem = static_cast<int>(i);
            openCombo_ = InvalidWidgetId;
            return true;
        }
        if (itemHoveredValue || static_cast<int>(i) == currentItem)
            popupDrawList.addRectFilled(item, static_cast<int>(i) == currentItem
                                            ? theme_.selectableSelected : theme_.selectableHovered,
                                       popupClip);

        const TextMetrics itemMetrics = measureText(theme_.font, items[i], theme_.fontSize);
        drawText(popupDrawList, theme_.font, items[i],
                 Vec2(item.x + theme_.windowPadding * 0.5f,
                      item.y + (item.height - itemMetrics.height) * 0.5f),
                 theme_.fontSize, theme_.labelText, popupClip);
    }
    return false;
}

bool Context::sliderFloat(StringView labelText, float &value, float minimum, float maximum,
                          const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || maximum <= minimum)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const float labelWidth = metrics.width + theme_.windowPadding;
    const Rect track(rect.x + labelWidth, rect.y + rect.height * 0.4f,
                     rect.width > labelWidth ? rect.width - labelWidth : 0.0f,
                     rect.height * 0.2f);
    const bool changed = sliderValue(track, clip, id, value, minimum, maximum);
    const float normalized = clamp((value - minimum) / (maximum - minimum), 0.0f, 1.0f);
    const float handleX = track.x + track.width * normalized;
    drawText(*drawList, theme_.font, labelText,
             Vec2(rect.x, rect.y + (rect.height - metrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, clip);
    drawList->addRectFilled(track, theme_.sliderBackground, clip);
    drawList->addRectFilled(Rect(track.x, track.y, track.width * normalized, track.height),
                            theme_.sliderFilled, clip);
    drawList->addCircleFilled(Vec2(handleX, track.y + track.height * 0.5f),
                              rect.height * 0.28f, theme_.sliderHandle, clip);
    return changed;
}

bool Context::sliderInt(StringView labelText, int &value, int minimum, int maximum,
                        const Rect &bounds)
{
    if (maximum <= minimum)
        return false;

    float temporary = static_cast<float>(value);
    const bool changed = sliderFloat(labelText, temporary, static_cast<float>(minimum),
                                     static_cast<float>(maximum), bounds);
    if (!changed)
        return false;

    const int rounded = temporary >= 0.0f ? static_cast<int>(temporary + 0.5f)
                                          : static_cast<int>(temporary - 0.5f);
    const int clamped = rounded < minimum ? minimum : (rounded > maximum ? maximum : rounded);
    const bool valueChanged = value != clamped;
    value = clamped;
    return valueChanged;
}

bool Context::stepperInt(StringView labelText, int &value, int minimum, int maximum,
                         const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || maximum < minimum)
        return false;

    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const float buttonWidth = rect.height;
    const float valueWidth = rect.height * 1.7f;
    const float controlsWidth = buttonWidth * 2.0f + valueWidth;
    const float controlsX = rect.x + (rect.width > controlsWidth ? rect.width - controlsWidth : 0.0f);
    const Rect decrement(controlsX, rect.y, buttonWidth, rect.height);
    const Rect valueRect(controlsX + buttonWidth, rect.y, valueWidth, rect.height);
    const Rect increment(controlsX + buttonWidth + valueWidth, rect.y, buttonWidth, rect.height);
    const WidgetId id = makeWidgetId(labelText);
    const WidgetId decrementId = combineIds(id, 1u);
    const WidgetId incrementId = combineIds(id, 2u);
    const bool decrementHovered = itemHovered(decrement, clip, decrementId);
    const bool incrementHovered = itemHovered(increment, clip, incrementId);
    const bool decrementClicked = itemClicked(decrement, clip, decrementId);
    const bool incrementClicked = itemClicked(increment, clip, incrementId);

    bool changed = false;
    if (decrementClicked && value > minimum)
    {
        --value;
        changed = true;
    }
    if (incrementClicked && value < maximum)
    {
        ++value;
        changed = true;
    }

    const TextMetrics labelMetrics = measureText(theme_.font, labelText, theme_.fontSize);
    drawText(*drawList, theme_.font, labelText,
             Vec2(rect.x, rect.y + (rect.height - labelMetrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, clip);
    drawList->addRectFilled(decrement,
                            decrementHovered ? theme_.buttonHovered : theme_.buttonBackground, clip);
    drawList->addRectFilled(valueRect, theme_.inputBg, clip);
    drawList->addRectFilled(increment,
                            incrementHovered ? theme_.buttonHovered : theme_.buttonBackground, clip);

    const String displayed = String::number(value);
    const TextMetrics valueMetrics = measureText(theme_.font, displayed, theme_.fontSize);
    const TextMetrics minusMetrics = measureText(theme_.font, StringView("-"), theme_.fontSize);
    const TextMetrics plusMetrics = measureText(theme_.font, StringView("+"), theme_.fontSize);
    drawText(*drawList, theme_.font, StringView("-"),
             Vec2(decrement.x + (decrement.width - minusMetrics.width) * 0.5f,
                  decrement.y + (decrement.height - minusMetrics.height) * 0.5f),
             theme_.fontSize, theme_.buttonText, clip);
    drawText(*drawList, theme_.font, displayed,
             Vec2(valueRect.x + (valueRect.width - valueMetrics.width) * 0.5f,
                  valueRect.y + (valueRect.height - valueMetrics.height) * 0.5f),
             theme_.fontSize, theme_.buttonText, clip);
    drawText(*drawList, theme_.font, StringView("+"),
             Vec2(increment.x + (increment.width - plusMetrics.width) * 0.5f,
                  increment.y + (increment.height - plusMetrics.height) * 0.5f),
             theme_.fontSize, theme_.buttonText, clip);
    return changed;
}

bool Context::inputText(StringView labelText, String &value, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    const Rect textClip = intersect(rect, clip);
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return false;
    const bool hovered = itemHovered(rect, clip, id);
    itemClicked(rect, clip, id);
    const bool focused = focusedWidget_ == id;
    bool changed = false;
    if (focused)
    {
        if (textInputWidget_ != id)
            textCursor_ = value.size();
        textInputWidget_ = id;
        wantsKeyboard_ = true;
        wantsTextInput_ = true;
        if (textCursor_ > value.size())
            textCursor_ = value.size();
        if (homePressed_)
            textCursor_ = 0u;
        if (endPressed_)
            textCursor_ = value.size();
        if (copyRequested_)
            backend_.setClipboardText(value);
        if (pasteRequested_)
        {
            const String clipboard = backend_.clipboardText();
            if (!clipboard.empty())
            {
                value.insert(textCursor_, clipboard.data(), clipboard.size());
                textCursor_ += clipboard.size();
                changed = true;
            }
        }
        for (ct::Vector<Event>::size_type i = 0; i < textEvents_.size(); ++i)
        {
            const Event &event = textEvents_[i];
            value.insert(textCursor_, event.text, event.textLength);
            textCursor_ += event.textLength;
            changed = event.textLength != 0u || changed;
        }
        if (backspacePressed_ && textCursor_ != 0u)
        {
            String::size_type eraseBegin = textCursor_ - 1u;
            while (eraseBegin != 0u &&
                   (static_cast<uint8_t>(value[eraseBegin]) & 0xC0u) == 0x80u)
                --eraseBegin;
            value.erase(eraseBegin, textCursor_ - eraseBegin);
            textCursor_ = eraseBegin;
            changed = true;
        }
    }

    const Color background = focused ? theme_.buttonHovered
                                     : (hovered ? theme_.buttonHovered : theme_.buttonBackground);
    drawList->addRectFilled(rect, background, clip);
    const TextMetrics metrics = measureText(theme_.font, value, theme_.fontSize);
    const float leftPadding = theme_.windowPadding * 0.5f;
    const float availableWidth = rect.width - theme_.windowPadding;
    float horizontalOffset = metrics.width > availableWidth ? availableWidth - metrics.width : 0.0f;
    const StringView prefix(value.data(), textCursor_);
    const TextMetrics prefixMetrics = measureText(theme_.font, prefix, theme_.fontSize);
    if (focused && prefixMetrics.width + horizontalOffset < 0.0f)
        horizontalOffset = -prefixMetrics.width;
    else if (focused && prefixMetrics.width + horizontalOffset > availableWidth)
        horizontalOffset = availableWidth - prefixMetrics.width;
    const Vec2 textPosition(rect.x + leftPadding + horizontalOffset,
                            rect.y + (rect.height - metrics.height) * 0.5f);
    drawText(*drawList, theme_.font, value, textPosition, theme_.fontSize, theme_.buttonText, textClip);
    if (focused)
    {
        const float caretX = textPosition.x + prefixMetrics.width + 1.0f;
        drawList->addRectFilled(Rect(caretX, rect.y + 5.0f, 1.0f,
                                     rect.height > 10.0f ? rect.height - 10.0f : 0.0f),
                                theme_.buttonText, textClip);
    }
    return changed;
}

bool Context::inputTextMultiline(StringView labelText, String &value, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    const Rect textClip = intersect(rect, clip);
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const bool hovered = itemHovered(rect, clip, id);
    itemClicked(rect, clip, id);
    const bool focused = focusedWidget_ == id;
    bool changed = false;
    if (focused)
    {
        if (textInputWidget_ != id)
            textCursor_ = value.size();
        textInputWidget_ = id;
        wantsKeyboard_ = true;
        wantsTextInput_ = true;
        if (textCursor_ > value.size())
            textCursor_ = value.size();
        if (homePressed_)
            textCursor_ = 0u;
        if (endPressed_)
            textCursor_ = value.size();
        if (copyRequested_)
            backend_.setClipboardText(value);
        if (pasteRequested_)
        {
            const String clipboard = backend_.clipboardText();
            if (!clipboard.empty())
            {
                value.insert(textCursor_, clipboard.data(), clipboard.size());
                textCursor_ += clipboard.size();
                changed = true;
            }
        }
        if (enterPressed_)
        {
            value.insert(textCursor_, "\n", 1u);
            ++textCursor_;
            changed = true;
        }
        for (ct::Vector<Event>::size_type i = 0; i < textEvents_.size(); ++i)
        {
            const Event &event = textEvents_[i];
            value.insert(textCursor_, event.text, event.textLength);
            textCursor_ += event.textLength;
            changed = event.textLength != 0u || changed;
        }
        if (backspacePressed_ && textCursor_ != 0u)
        {
            String::size_type eraseBegin = textCursor_ - 1u;
            while (eraseBegin != 0u &&
                   (static_cast<uint8_t>(value[eraseBegin]) & 0xC0u) == 0x80u)
                --eraseBegin;
            value.erase(eraseBegin, textCursor_ - eraseBegin);
            textCursor_ = eraseBegin;
            changed = true;
        }
    }

    const Color background = focused ? theme_.buttonHovered
                                     : (hovered ? theme_.buttonHovered : theme_.inputBg);
    drawList->addRectFilled(rect, background, clip);
    const float padding = theme_.textEditPadding;
    drawText(*drawList, theme_.font, value, Vec2(rect.x + padding, rect.y + padding),
             theme_.fontSize, theme_.buttonText, textClip);
    if (focused)
    {
        String::size_type lineStart = 0u;
        uint32_t line = 0u;
        for (String::size_type i = 0u; i < textCursor_; ++i)
        {
            if (value[i] == '\n')
            {
                lineStart = i + 1u;
                ++line;
            }
        }
        const StringView linePrefix(value.data() + lineStart, textCursor_ - lineStart);
        const TextMetrics prefixMetrics = measureText(theme_.font, linePrefix, theme_.fontSize);
        const float caretX = rect.x + padding + prefixMetrics.width;
        const float caretY = rect.y + padding + static_cast<float>(line) * theme_.fontSize;
        if (caretY < rect.y + rect.height - padding)
            drawList->addRectFilled(Rect(caretX, caretY, 1.0f, theme_.fontSize),
                                    theme_.buttonText, textClip);
    }
    return changed;
}

bool Context::inputInt(StringView labelText, int &value, const Rect &bounds)
{
    String text = String::number(value);
    if (!inputText(labelText, text, bounds))
        return false;
    value = text.to_int();
    return true;
}

bool Context::inputFloat(StringView labelText, float &value, const Rect &bounds, int precision)
{
    String text = String::number(static_cast<double>(value), precision);
    if (!inputText(labelText, text, bounds))
        return false;
    value = text.to_float();
    return true;
}

bool Context::colorEdit(StringView labelText, Color &value, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const TextMetrics titleMetrics = measureText(theme_.font, labelText, theme_.fontSize);
    const float contentY = rect.y + titleMetrics.height + theme_.itemSpacing;
    const float availableHeight = rect.y + rect.height - contentY - theme_.windowPadding * 0.5f;
    const float maximumPickerWidth = (rect.width - 48.0f) * 0.55f;
    const float pickerSize = availableHeight < maximumPickerWidth ? availableHeight : maximumPickerWidth;
    if (pickerSize <= 0.0f)
        return false;

    const float spacing = theme_.windowPadding;
    const float hueWidth = 14.0f;
    const Rect saturationValue(rect.x, contentY, pickerSize, pickerSize);
    const Rect hueBar(saturationValue.x + saturationValue.width + spacing, contentY,
                      hueWidth, pickerSize);
    const Rect alphaBar(hueBar.x + hueBar.width + spacing, contentY,
                        rect.x + rect.width - (hueBar.x + hueBar.width + spacing), pickerSize);
    float hue = 0.0f;
    float saturation = 0.0f;
    float brightness = 0.0f;
    colorToHsv(value, hue, saturation, brightness);
    float alpha = static_cast<float>(value.a) / 255.0f;
    bool changed = false;
    const uint32_t left = buttonIndex(PointerButton::Left);
    const WidgetId saturationValueId = combineIds(id, 1u);
    const WidgetId hueId = combineIds(id, 2u);
    const WidgetId alphaId = combineIds(id, 3u);
    itemHovered(saturationValue, clip, saturationValueId);
    itemHovered(hueBar, clip, hueId);
    itemHovered(alphaBar, clip, alphaId);

    const bool pressSaturationValue = pointer_.pressed[left] && currentWindow_ == focusedWindow_ &&
                                      activeWidget_ == InvalidWidgetId &&
                                      contains(intersect(saturationValue, clip), pointer_.pressedPosition[left]);
    if (pressSaturationValue)
    {
        activeWidget_ = saturationValueId;
        focusedWidget_ = saturationValueId;
    }
    if (activeWidget_ == saturationValueId)
    {
        if (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left])
        {
            const float nextSaturation = clamp((pointer_.position.x - saturationValue.x) /
                                               saturationValue.width, 0.0f, 1.0f);
            const float nextBrightness = 1.0f - clamp((pointer_.position.y - saturationValue.y) /
                                                       saturationValue.height, 0.0f, 1.0f);
            changed = changed || nextSaturation != saturation || nextBrightness != brightness;
            saturation = nextSaturation;
            brightness = nextBrightness;
        }
        if (pointer_.released[left])
            activeWidget_ = InvalidWidgetId;
    }

    const bool pressHue = pointer_.pressed[left] && currentWindow_ == focusedWindow_ &&
                          activeWidget_ == InvalidWidgetId &&
                          contains(intersect(hueBar, clip), pointer_.pressedPosition[left]);
    if (pressHue)
    {
        activeWidget_ = hueId;
        focusedWidget_ = hueId;
    }
    if (activeWidget_ == hueId)
    {
        if (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left])
        {
            const float nextHue = clamp((pointer_.position.y - hueBar.y) / hueBar.height, 0.0f, 1.0f);
            changed = changed || nextHue != hue;
            hue = nextHue;
        }
        if (pointer_.released[left])
            activeWidget_ = InvalidWidgetId;
    }

    const bool pressAlpha = pointer_.pressed[left] && currentWindow_ == focusedWindow_ &&
                            activeWidget_ == InvalidWidgetId && alphaBar.width > 0.0f &&
                            contains(intersect(alphaBar, clip), pointer_.pressedPosition[left]);
    if (pressAlpha)
    {
        activeWidget_ = alphaId;
        focusedWidget_ = alphaId;
    }
    if (activeWidget_ == alphaId)
    {
        if (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left])
        {
            const float nextAlpha = clamp((pointer_.position.x - alphaBar.x) / alphaBar.width, 0.0f, 1.0f);
            changed = changed || nextAlpha != alpha;
            alpha = nextAlpha;
        }
        if (pointer_.released[left])
            activeWidget_ = InvalidWidgetId;
    }

    if (changed)
        value = colorFromHsv(hue, saturation, brightness,
                             static_cast<uint8_t>(alpha * 255.0f + 0.5f));

    const Color hueColor = colorFromHsv(hue, 1.0f, 1.0f, 255u);
    drawText(*drawList, theme_.font, labelText, Vec2(rect.x, rect.y), theme_.fontSize,
             theme_.labelText, clip);
    drawList->addRectGradient(saturationValue, Color(255u, 255u, 255u, 255u), hueColor,
                              Color(0u, 0u, 0u, 255u), Color(0u, 0u, 0u, 255u), clip);
    for (uint32_t segment = 0u; segment < 6u; ++segment)
    {
        const float segmentHeight = hueBar.height / 6.0f;
        const Rect segmentRect(hueBar.x, hueBar.y + segmentHeight * static_cast<float>(segment),
                               hueBar.width, segmentHeight);
        const Color top = colorFromHsv(static_cast<float>(segment) / 6.0f, 1.0f, 1.0f, 255u);
        const Color bottom = colorFromHsv(static_cast<float>(segment + 1u) / 6.0f, 1.0f, 1.0f, 255u);
        drawList->addRectGradient(segmentRect, top, top, bottom, bottom, clip);
    }
    const float checkerSize = alphaBar.width > 0.0f ? alphaBar.width / 8.0f : 0.0f;
    if (checkerSize > 0.0f)
    {
        for (uint32_t y = 0u; y < 2u; ++y)
        {
            for (uint32_t x = 0u; x < 8u; ++x)
            {
                const Color checker = (x + y) % 2u == 0u ? Color(68u, 68u, 75u, 255u)
                                                           : Color(108u, 108u, 115u, 255u);
                drawList->addRectFilled(Rect(alphaBar.x + checkerSize * static_cast<float>(x),
                                             alphaBar.y + alphaBar.height * 0.5f * static_cast<float>(y),
                                             checkerSize, alphaBar.height * 0.5f), checker, clip);
            }
        }
        const Color transparent(value.r, value.g, value.b, 0u);
        drawList->addRectGradient(alphaBar, transparent, value, value, transparent, clip);
    }

    const Vec2 saturationValueCursor(saturationValue.x + saturation * saturationValue.width,
                                     saturationValue.y + (1.0f - brightness) * saturationValue.height);
    drawList->addCircleFilled(saturationValueCursor, 6.0f, Color(255u, 255u, 255u, 255u), clip);
    drawList->addCircleFilled(saturationValueCursor, 3.5f,
                              colorFromHsv(hue, saturation, brightness, 255u), clip);
    const float hueCursorY = hueBar.y + hue * hueBar.height;
    drawList->addRectFilled(Rect(hueBar.x - 2.0f, hueCursorY - 1.0f, hueBar.width + 4.0f, 2.0f),
                            Color(255u, 255u, 255u, 255u), clip);
    const float alphaCursorX = alphaBar.x + alpha * alphaBar.width;
    drawList->addRectFilled(Rect(alphaCursorX - 1.0f, alphaBar.y - 2.0f, 2.0f, alphaBar.height + 4.0f),
                            Color(255u, 255u, 255u, 255u), clip);
    return changed;
}

void Context::image(TextureId texture, const Rect &bounds, const Vec2 &uvMin,
                    const Vec2 &uvMax, const Color &tint)
{
    if (!currentWindow() || texture.value == 0u)
        return;

    DrawList *drawList = currentDrawList();
    if (!drawList)
        return;

    const Rect rect = contentRect(bounds);
    drawList->addImage(texture, rect, uvMin, uvMax, tint, contentClip());
}

bool Context::imageButton(StringView labelText, TextureId texture, const Rect &bounds,
                          const Vec2 &uvMin, const Vec2 &uvMax, const Color &tint)
{
    WindowState *window = currentWindow();
    if (!window || texture.value == 0u)
        return false;

    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const bool hovered = itemHovered(rect, clip, id);
    const bool clicked = itemClicked(rect, clip, id);
    const float inset = rect.width < rect.height ? rect.width * 0.08f : rect.height * 0.08f;
    const Rect imageRect(rect.x + inset, rect.y + inset,
                         rect.width - inset * 2.0f, rect.height - inset * 2.0f);
    drawList->addRectFilled(rect, hovered ? theme_.buttonHovered : theme_.buttonBackground, clip);
    drawList->addImage(texture, imageRect, uvMin, uvMax, tint, clip);
    if (hovered)
        drawList->addRectFilled(imageRect, Color(255u, 255u, 255u, 24u), clip);
    return clicked;
}

void Context::progressBar(float value, float maximum, const Rect &bounds)
{
    if (!currentWindow())
        return;

    DrawList *drawList = currentDrawList();
    if (!drawList)
        return;

    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    const float normalized = maximum > 0.0f ? clamp(value / maximum, 0.0f, 1.0f) : 0.0f;
    drawList->addRectFilled(rect, theme_.progressBackground, clip);
    drawList->addRectFilled(Rect(rect.x, rect.y, rect.width * normalized, rect.height),
                            theme_.progressFilled, clip);
}

void Context::label(StringView text, const Vec2 &position)
{
    WindowState *window = currentWindow();
    if (!window)
        return;
    DrawList *drawList = currentDrawList();
    if (!drawList)
        return;
    const Rect resolved = contentRect(Rect(position.x, position.y, 0.0f, 0.0f));
    drawText(*drawList, theme_.font, text, Vec2(resolved.x, resolved.y),
             theme_.fontSize, theme_.labelText, contentClip());
}

void Context::tooltip(StringView text)
{
    WindowState *window = currentWindow();
    if (!window || hotWidget_ != lastItemId_ || text.empty())
        return;

    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const TextMetrics metrics = measureText(theme_.font, text, theme_.fontSize);
    const float paddingX = theme_.tooltipPadX;
    const float paddingY = theme_.tooltipPadY;
    float x = pointer_.position.x + 14.0f;
    float y = pointer_.position.y + 18.0f;
    const float width = metrics.width + paddingX * 2.0f;
    const float height = metrics.height + paddingY * 2.0f;
    if (x + width > viewport.width)
        x = viewport.width - width;
    if (y + height > viewport.height)
        y = pointer_.position.y - height - 10.0f;
    if (x < 0.0f)
        x = 0.0f;
    if (y < 0.0f)
        y = 0.0f;
    const Rect bounds(x, y, width, height);
    window->overlayDrawList.addRectFilled(bounds, theme_.tooltipBg, viewport);
    drawText(window->overlayDrawList, theme_.font, text,
             Vec2(x + paddingX, y + paddingY), theme_.fontSize,
             theme_.tooltipText, viewport);
}

bool Context::button(StringView labelText)
{
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, autoButtonWidth(labelText), theme_.widgetHeight);
    const bool clicked = button(labelText, Rect(bounds.x - layout_.origin.x,
                                                bounds.y - layout_.origin.y,
                                                bounds.width, bounds.height));
    advanceLayout(bounds);
    return clicked;
}

bool Context::checkbox(StringView labelText, bool &value)
{
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, theme_.widgetHeight, theme_.widgetHeight);
    const bool clicked = checkbox(labelText, value, Rect(bounds.x - layout_.origin.x,
                                                         bounds.y - layout_.origin.y,
                                                         bounds.width, bounds.height));
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    advanceLayout(Rect(bounds.x, bounds.y,
                       bounds.width + theme_.windowPadding * 0.5f + metrics.width,
                       bounds.height));
    return clicked;
}

bool Context::toggleSwitch(StringView labelText, bool &value)
{
    const float switchWidth = theme_.widgetHeight * 1.8f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, switchWidth, theme_.widgetHeight);
    const bool clicked = toggleSwitch(labelText, value,
                                      Rect(bounds.x - layout_.origin.x,
                                           bounds.y - layout_.origin.y,
                                           bounds.width, bounds.height));
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    advanceLayout(Rect(bounds.x, bounds.y,
                       bounds.width + theme_.windowPadding * 0.5f + metrics.width,
                       bounds.height));
    return clicked;
}

bool Context::radioButton(StringView labelText, bool selected)
{
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, theme_.widgetHeight, theme_.widgetHeight);
    const bool clicked = radioButton(labelText, selected,
                                     Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                          bounds.width, bounds.height));
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    advanceLayout(Rect(bounds.x, bounds.y,
                       bounds.width + theme_.windowPadding * 0.5f + metrics.width,
                       bounds.height));
    return clicked;
}

bool Context::selectable(StringView labelText, bool selected, float width)
{
    const float resolvedWidth = width > 0.0f ? width : autoButtonWidth(labelText);
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool clicked = selectable(labelText, selected,
                                    Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                         bounds.width, bounds.height));
    advanceLayout(bounds);
    return clicked;
}

bool Context::collapsingHeader(StringView labelText, bool &expanded, float width)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool open = collapsingHeader(labelText, expanded,
                                       Rect(bounds.x - layout_.origin.x,
                                            bounds.y - layout_.origin.y,
                                            bounds.width, bounds.height));
    advanceLayout(bounds);
    return open;
}

bool Context::treeNode(StringView labelText, bool &expanded, float width)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool open = treeNode(labelText, expanded,
                               Rect(bounds.x - layout_.origin.x,
                                    bounds.y - layout_.origin.y,
                                    bounds.width, bounds.height));
    advanceLayout(bounds);
    return open;
}

bool Context::tabBar(StringView labelText, int &currentItem, Span<const StringView> items,
                     float width)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = tabBar(labelText, currentItem, items,
                                Rect(bounds.x - layout_.origin.x,
                                     bounds.y - layout_.origin.y,
                                     bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::listBox(StringView labelText, int &currentItem, Span<const StringView> items,
                      float width, int visibleItems)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const int resolvedRows = visibleItems > 0 ? visibleItems : 1;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth,
                      theme_.widgetHeight * static_cast<float>(resolvedRows));
    const bool changed = listBox(labelText, currentItem, items,
                                 Rect(bounds.x - layout_.origin.x,
                                      bounds.y - layout_.origin.y,
                                      bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::comboBox(StringView labelText, int &currentItem, Span<const StringView> items,
                       float width)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = comboBox(labelText, currentItem, items,
                                  Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                       bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::sliderFloat(StringView labelText, float &value, float minimum, float maximum,
                          float width)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = sliderFloat(labelText, value, minimum, maximum,
                                     Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                          bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::sliderInt(StringView labelText, int &value, int minimum, int maximum, float width)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = sliderInt(labelText, value, minimum, maximum,
                                   Rect(bounds.x - layout_.origin.x,
                                        bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::stepperInt(StringView labelText, int &value, int minimum, int maximum, float width)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = stepperInt(labelText, value, minimum, maximum,
                                    Rect(bounds.x - layout_.origin.x,
                                         bounds.y - layout_.origin.y,
                                         bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::inputText(StringView labelText, String &value, float width)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = inputText(labelText, value,
                                   Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::inputTextMultiline(StringView labelText, String &value, float width, float height)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const float resolvedHeight = height > theme_.widgetHeight ? height : theme_.widgetHeight;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, resolvedHeight);
    const bool changed = inputTextMultiline(labelText, value,
                                            Rect(bounds.x - layout_.origin.x,
                                                 bounds.y - layout_.origin.y,
                                                 bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::inputInt(StringView labelText, int &value, float width)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = inputInt(labelText, value,
                                  Rect(bounds.x - layout_.origin.x,
                                       bounds.y - layout_.origin.y,
                                       bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::inputFloat(StringView labelText, float &value, float width, int precision)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = inputFloat(labelText, value,
                                    Rect(bounds.x - layout_.origin.x,
                                         bounds.y - layout_.origin.y,
                                         bounds.width, bounds.height), precision);
    advanceLayout(bounds);
    return changed;
}

bool Context::colorEdit(StringView labelText, Color &value, float width, float height)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const float minimumHeight = theme_.widgetHeight * 3.0f;
    const float resolvedHeight = height > minimumHeight ? height : minimumHeight;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, resolvedHeight);
    const bool changed = colorEdit(labelText, value,
                                   Rect(bounds.x - layout_.origin.x,
                                        bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

void Context::image(TextureId texture, float width, float height,
                    const Vec2 &uvMin, const Vec2 &uvMax, const Color &tint)
{
    if (width <= 0.0f || height <= 0.0f)
        return;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, width, height);
    image(texture, Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                        bounds.width, bounds.height), uvMin, uvMax, tint);
    advanceLayout(bounds);
}

bool Context::imageButton(StringView labelText, TextureId texture, float width, float height,
                          const Vec2 &uvMin, const Vec2 &uvMax, const Color &tint)
{
    if (width <= 0.0f || height <= 0.0f)
        return false;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, width, height);
    const bool clicked = imageButton(labelText, texture,
                                     Rect(bounds.x - layout_.origin.x,
                                          bounds.y - layout_.origin.y,
                                          bounds.width, bounds.height),
                                     uvMin, uvMax, tint);
    advanceLayout(bounds);
    return clicked;
}

void Context::progressBar(float value, float maximum, float width)
{
    const float resolvedWidth = width > 0.0f ? width : 180.0f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    progressBar(value, maximum,
                Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                     bounds.width, bounds.height));
    advanceLayout(bounds);
}

void Context::label(StringView text)
{
    const TextMetrics metrics = measureText(theme_.font, text, theme_.fontSize);
    label(text, Vec2(layout_.cursor.x - layout_.origin.x, layout_.cursor.y - layout_.origin.y));
    advanceLayout(Rect(layout_.cursor.x, layout_.cursor.y, metrics.width, metrics.height));
}

void Context::sameLine(float spacingValue)
{
    if (!layout_.hasLastItem)
        return;
    const float itemSpacing = spacingValue >= 0.0f ? spacingValue : theme_.itemSpacing;
    layout_.cursor.x = layout_.lastItem.x + layout_.lastItem.width + itemSpacing;
    layout_.cursor.y = layout_.lastItem.y;
}

void Context::spacing(float pixels)
{
    if (pixels > 0.0f)
        layout_.cursor.y += pixels;
}

void Context::indent(float pixels)
{
    if (!currentWindow() || pixels <= 0.0f)
        return;
    layout_.origin.x += pixels;
    layout_.cursor.x = layout_.origin.x;
    layout_.hasLastItem = false;
}

void Context::unindent(float pixels)
{
    if (!currentWindow() || pixels <= 0.0f)
        return;
    const float nextOrigin = layout_.origin.x - pixels;
    layout_.origin.x = nextOrigin > layout_.baseOriginX ? nextOrigin : layout_.baseOriginX;
    layout_.cursor.x = layout_.origin.x;
    layout_.hasLastItem = false;
}

void Context::separator(float thickness)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList || thickness <= 0.0f)
        return;
    const Rect clip = contentClip();
    const float right = window->bounds.x + window->bounds.width - theme_.windowPadding;
    const Rect line(layout_.cursor.x, layout_.cursor.y,
                    right > layout_.cursor.x ? right - layout_.cursor.x : 0.0f, thickness);
    drawList->addRectFilled(line, theme_.titleBarBackground, clip);
    advanceLayout(line);
}

void Context::setCursor(const Vec2 &localPosition)
{
    layout_.cursor = Vec2(layout_.origin.x + localPosition.x, layout_.origin.y + localPosition.y);
    layout_.hasLastItem = false;
}

Vec2 Context::cursor() const
{
    return Vec2(layout_.cursor.x - layout_.origin.x, layout_.cursor.y - layout_.origin.y);
}

WidgetId Context::hashText(StringView text)
{
    uint64_t hash = 1469598103934665603ull;
    for (StringView::size_type i = 0; i < text.size(); ++i)
    {
        hash ^= static_cast<uint8_t>(text[i]);
        hash *= 1099511628211ull;
    }
    return hash == InvalidWidgetId ? 1u : hash;
}

WidgetId Context::combineIds(WidgetId a, WidgetId b)
{
    uint64_t hash = a ^ 0x9e3779b97f4a7c15ull;
    hash ^= b + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
    return hash == InvalidWidgetId ? 1u : hash;
}

WidgetId Context::makeWidgetId(StringView labelText) const
{
    const WidgetId parent = idStack_.empty() ? InvalidWidgetId : idStack_.back();
    return combineIds(parent, hashText(labelText));
}

void Context::consumeEvents()
{
    while (!events_.empty())
    {
        const Event event = events_.front();
        events_.pop_front();
        switch (event.type)
        {
        case EventType::PointerMove:
            pointer_.position = event.position;
            break;
        case EventType::PointerDown:
        {
            const uint32_t index = buttonIndex(event.button);
            pointer_.position = event.position;
            pointer_.pressedPosition[index] = event.position;
            pointer_.down[index] = true;
            pointer_.pressed[index] = true;
            break;
        }
        case EventType::PointerUp:
        {
            const uint32_t index = buttonIndex(event.button);
            pointer_.position = event.position;
            pointer_.releasedPosition[index] = event.position;
            pointer_.down[index] = false;
            pointer_.released[index] = true;
            break;
        }
        case EventType::PointerWheel:
            pointer_.wheelX += event.wheelX;
            pointer_.wheelY += event.wheelY;
            break;
        case EventType::KeyDown:
            if (event.key == KeyCode::Backspace)
                backspacePressed_ = true;
            else if (event.key == KeyCode::Enter)
                enterPressed_ = true;
            else if (event.key == KeyCode::Home)
                homePressed_ = true;
            else if (event.key == KeyCode::End)
                endPressed_ = true;
            else if (event.key == KeyCode::Up)
                upPressed_ = true;
            else if (event.key == KeyCode::Down)
                downPressed_ = true;
            else if (event.control && event.key == KeyCode::C)
                copyRequested_ = true;
            else if (event.control && event.key == KeyCode::V)
                pasteRequested_ = true;
            break;
        case EventType::TextInput:
            if (event.textLength != 0u)
                textEvents_.push_back(event);
            break;
        case EventType::FocusLost:
            pointer_.down[0] = false;
            pointer_.down[1] = false;
            pointer_.down[2] = false;
            pointer_.pressed[0] = false;
            pointer_.pressed[1] = false;
            pointer_.pressed[2] = false;
            pointer_.released[0] = false;
            pointer_.released[1] = false;
            pointer_.released[2] = false;
            activeWidget_ = InvalidWidgetId;
            draggingWindow_ = WindowHandle();
            resizingWindow_ = WindowHandle();
            focusedWidget_ = InvalidWidgetId;
            textInputWidget_ = InvalidWidgetId;
            openCombo_ = InvalidWidgetId;
            break;
        case EventType::ViewportChanged:
            frame_.displaySize = event.viewportSize;
            frame_.dpiScale = event.dpiScale;
            break;
        case EventType::None:
            break;
        }
    }
}

WindowState *Context::currentWindow()
{
    return windows_.get(currentWindow_);
}

DrawList *Context::currentDrawList()
{
    WindowState *window = currentWindow();
    return window ? &window->drawList : nullptr;
}

TextMetrics Context::measureText(FontId font, StringView text, float logicalSize) const
{
    return textProvider_ ? textProvider_->measureText(font, text, logicalSize)
                         : backend_.measureText(font, text, logicalSize, frame_.dpiScale);
}

void Context::drawText(DrawList &drawList, FontId font, StringView text, const Vec2 &position,
                       float logicalSize, const Color &color, const Rect &clip)
{
    if (!textProvider_ || !textProvider_->appendText(drawList, font, text, position,
                                                      logicalSize, color, clip))
        drawList.addText(text, position, font, logicalSize, color, clip);
}

void Context::beginLayout(const WindowState &window)
{
    const Vec2 origin(window.bounds.x + theme_.windowPadding,
                      window.bounds.y + theme_.titleBarHeight + theme_.windowPadding);
    layout_.origin = origin;
    layout_.cursor = origin;
    layout_.baseOriginX = origin.x;
    layout_.lastItem = Rect();
    layout_.hasLastItem = false;
}

void Context::advanceLayout(const Rect &item)
{
    layout_.lastItem = item;
    layout_.hasLastItem = true;
    layout_.cursor.x = layout_.origin.x;
    layout_.cursor.y = item.y + item.height + theme_.itemSpacing;
}

float Context::autoButtonWidth(StringView labelText) const
{
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const float width = metrics.width + theme_.windowPadding * 2.0f;
    return width > 64.0f ? width : 64.0f;
}

const WindowState *Context::currentWindow() const
{
    return windows_.get(currentWindow_);
}

WindowState *Context::getOrCreateWindow(WidgetId id, StringView title,
                                        const Rect &bounds)
{
    WindowHandle *existing = windowsById_.find(id);
    if (existing)
        return windows_.get(*existing);

    WindowState state;
    state.id = id;
    state.title = String(title.data(), title.size());
    state.bounds = bounds;
    state.zOrder = nextZOrder_++;
    const WindowHandle handle = windows_.emplace(state);
    windowsById_.put(id, handle);
    windowOrder_.push_back(handle);
    return windows_.get(handle);
}

Rect Context::contentRect(const Rect &local) const
{
    const WindowState *window = currentWindow();
    if (!window)
        return Rect();
    return Rect(window->bounds.x + theme_.windowPadding + local.x,
                window->bounds.y + theme_.titleBarHeight + theme_.windowPadding + local.y,
                local.width, local.height);
}

Rect Context::contentClip() const
{
    const WindowState *window = currentWindow();
    if (!window)
        return Rect();
    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const float contentWidth = window->bounds.width - theme_.windowPadding * 2.0f;
    const float contentHeight = window->bounds.height - theme_.titleBarHeight - theme_.windowPadding * 2.0f;
    const Rect content(window->bounds.x + theme_.windowPadding,
                       window->bounds.y + theme_.titleBarHeight + theme_.windowPadding,
                       contentWidth > 0.0f ? contentWidth : 0.0f,
                       contentHeight > 0.0f ? contentHeight : 0.0f);
    return intersect(content, viewport);
}

bool Context::itemHovered(const Rect &rect, const Rect &clip, WidgetId id)
{
    lastItemId_ = id;
    if (activeWidget_ != InvalidWidgetId && activeWidget_ != id)
        return false;
    const Rect visible = intersect(rect, clip);
    const bool hovered = currentWindowReceivesPointer() && contains(visible, pointer_.position);
    if (hovered)
        hotWidget_ = id;
    return hovered;
}

bool Context::itemClicked(const Rect &rect, const Rect &clip, WidgetId id)
{
    if (activeWidget_ != InvalidWidgetId && activeWidget_ != id)
        return false;
    const uint32_t left = buttonIndex(PointerButton::Left);
    const Rect visible = intersect(rect, clip);
    const bool pressedHere = pointer_.pressed[left] &&
                             currentWindow_ == focusedWindow_ &&
                             contains(visible, pointer_.pressedPosition[left]);
    if (pressedHere)
    {
        activeWidget_ = id;
        focusedWidget_ = id;
    }

    const bool releasedHere = pointer_.released[left] &&
                              contains(visible, pointer_.releasedPosition[left]);
    const bool clicked = releasedHere && activeWidget_ == id;
    if (pointer_.released[left] && activeWidget_ == id)
        activeWidget_ = InvalidWidgetId;
    return clicked;
}

bool Context::sliderValue(const Rect &rect, const Rect &clip, WidgetId id,
                          float &value, float minimum, float maximum)
{
    if (rect.width <= 0.0f || maximum <= minimum)
        return false;
    if (activeWidget_ != InvalidWidgetId && activeWidget_ != id)
        return false;

    const uint32_t left = buttonIndex(PointerButton::Left);
    const Rect visible = intersect(rect, clip);
    const bool pressedHere = pointer_.pressed[left] && currentWindow_ == focusedWindow_ &&
                             contains(visible, pointer_.pressedPosition[left]);
    if (pressedHere)
    {
        activeWidget_ = id;
        focusedWidget_ = id;
    }

    bool changed = false;
    if (activeWidget_ == id && (pointer_.down[left] || pointer_.pressed[left]))
    {
        const float normalized = clamp((pointer_.position.x - rect.x) / rect.width, 0.0f, 1.0f);
        const float nextValue = minimum + (maximum - minimum) * normalized;
        changed = nextValue != value;
        value = nextValue;
    }
    if (pointer_.released[left] && activeWidget_ == id)
        activeWidget_ = InvalidWidgetId;
    return changed;
}

void Context::drawWindow(WindowState &window)
{
    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const float titleHeight = theme_.titleBarHeight;
    const float controlWidth = titleHeight;
    const Rect closeButton(window.bounds.x + window.bounds.width - controlWidth,
                           window.bounds.y, controlWidth, titleHeight);
    const Rect minimizeButton(closeButton.x - controlWidth, window.bounds.y,
                              controlWidth, titleHeight);
    const Rect dragArea(window.bounds.x, window.bounds.y,
                        window.bounds.width > controlWidth * 2.0f
                            ? window.bounds.width - controlWidth * 2.0f : 0.0f,
                        titleHeight);
    const float resizeGripSize = 12.0f;
    const Rect resizeGrip(window.bounds.x + window.bounds.width - resizeGripSize,
                          window.bounds.y + window.bounds.height - resizeGripSize,
                          resizeGripSize, resizeGripSize);
    const WidgetId closeId = combineIds(window.id, 0x434c4f5345ull);
    const WidgetId minimizeId = combineIds(window.id, 0x4d494e494d495a45ull);
    const WidgetId dragId = combineIds(window.id, 0x44524147ull);
    const WidgetId resizeId = combineIds(window.id, 0x524553495a45ull);
    const bool closeHovered = itemHovered(closeButton, viewport, closeId);
    const bool minimizeHovered = itemHovered(minimizeButton, viewport, minimizeId);

    if (itemClicked(closeButton, viewport, closeId))
    {
        window.open = false;
        window.minimized = false;
        draggingWindow_ = WindowHandle();
        resizingWindow_ = WindowHandle();
        focusedWidget_ = InvalidWidgetId;
        textInputWidget_ = InvalidWidgetId;
        openCombo_ = InvalidWidgetId;
        return;
    }
    if (itemClicked(minimizeButton, viewport, minimizeId))
        window.minimized = !window.minimized;

    const uint32_t left = buttonIndex(PointerButton::Left);
    const bool pressedResize = !window.minimized && pointer_.pressed[left] &&
                               currentWindow_ == focusedWindow_ && activeWidget_ == InvalidWidgetId &&
                               contains(intersect(resizeGrip, viewport), pointer_.pressedPosition[left]);
    if (pressedResize)
    {
        activeWidget_ = resizeId;
        resizingWindow_ = currentWindow_;
    }
    if (resizingWindow_ == currentWindow_ && activeWidget_ == resizeId)
    {
        if (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left])
        {
            const float minimumWidth = titleHeight * 4.0f;
            const float minimumHeight = titleHeight + theme_.windowPadding * 2.0f + theme_.widgetHeight;
            const float width = pointer_.position.x - window.bounds.x;
            const float height = pointer_.position.y - window.bounds.y;
            window.bounds.width = width > minimumWidth ? width : minimumWidth;
            window.bounds.height = height > minimumHeight ? height : minimumHeight;
        }
        if (pointer_.released[left])
        {
            activeWidget_ = InvalidWidgetId;
            resizingWindow_ = WindowHandle();
        }
    }
    const bool pressedTitle = pointer_.pressed[left] && currentWindow_ == focusedWindow_ &&
                              activeWidget_ == InvalidWidgetId &&
                              contains(intersect(dragArea, viewport), pointer_.pressedPosition[left]);
    if (pressedTitle)
    {
        activeWidget_ = dragId;
        draggingWindow_ = currentWindow_;
        windowDragOffset_ = Vec2(pointer_.pressedPosition[left].x - window.bounds.x,
                                 pointer_.pressedPosition[left].y - window.bounds.y);
    }
    if (draggingWindow_ == currentWindow_ && activeWidget_ == dragId)
    {
        if (pointer_.down[left] || pointer_.pressed[left])
        {
            window.bounds.x = pointer_.position.x - windowDragOffset_.x;
            window.bounds.y = pointer_.position.y - windowDragOffset_.y;
        }
        if (pointer_.released[left])
        {
            activeWidget_ = InvalidWidgetId;
            draggingWindow_ = WindowHandle();
        }
    }

    const Rect resolvedTitleBar(window.bounds.x, window.bounds.y, window.bounds.width, titleHeight);
    if (!window.minimized)
        window.drawList.addRectFilled(window.bounds, theme_.windowBackground, viewport);
    window.drawList.addRectFilled(resolvedTitleBar, theme_.titleBarBackground, viewport);
    const Rect resolvedCloseButton(window.bounds.x + window.bounds.width - controlWidth,
                                   window.bounds.y, controlWidth, titleHeight);
    const Rect resolvedMinimizeButton(resolvedCloseButton.x - controlWidth, window.bounds.y,
                                      controlWidth, titleHeight);
    if (minimizeHovered)
        window.drawList.addRectFilled(resolvedMinimizeButton, theme_.buttonHovered, viewport);
    if (closeHovered)
        window.drawList.addRectFilled(resolvedCloseButton, theme_.buttonHovered, viewport);
    const TextMetrics metrics = measureText(theme_.font, window.title, theme_.fontSize);
    const Vec2 titlePosition(window.bounds.x + theme_.windowPadding,
                             window.bounds.y + (titleHeight - metrics.height) * 0.5f);
    drawText(window.drawList, theme_.font, window.title, titlePosition, theme_.fontSize,
             theme_.labelText, viewport);
    const TextMetrics minimizeMetrics = measureText(theme_.font, StringView("-"), theme_.fontSize);
    const TextMetrics closeMetrics = measureText(theme_.font, StringView("x"), theme_.fontSize);
    drawText(window.drawList, theme_.font, StringView("-"),
             Vec2(resolvedMinimizeButton.x + (controlWidth - minimizeMetrics.width) * 0.5f,
                  resolvedMinimizeButton.y + (titleHeight - minimizeMetrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, viewport);
    drawText(window.drawList, theme_.font, StringView("x"),
             Vec2(resolvedCloseButton.x + (controlWidth - closeMetrics.width) * 0.5f,
                  resolvedCloseButton.y + (titleHeight - closeMetrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, viewport);
}

WindowHandle Context::topWindowAt(const Vec2 &position) const
{
    WindowHandle result;
    uint64_t highestZ = 0;
    for (ct::Vector<WindowHandle>::size_type i = 0; i < windowOrder_.size(); ++i)
    {
        const WindowState *window = windows_.get(windowOrder_[i]);
        const Rect hitBounds(window ? window->bounds.x : 0.0f, window ? window->bounds.y : 0.0f,
                             window ? window->bounds.width : 0.0f,
                             window && window->minimized ? theme_.titleBarHeight
                                                          : (window ? window->bounds.height : 0.0f));
        if (window && window->open && contains(hitBounds, position) && window->zOrder >= highestZ)
        {
            highestZ = window->zOrder;
            result = windowOrder_[i];
        }
    }
    return result;
}

void Context::focusWindow(WindowHandle handle)
{
    if (!handle)
        return;
    for (ct::SlotMap<WindowState>::iterator it = windows_.begin(); it != windows_.end(); ++it)
        it->focused = false;
    WindowState *window = windows_.get(handle);
    if (!window)
        return;
    window->focused = true;
    window->zOrder = nextZOrder_++;
    focusedWindow_ = handle;
}

bool Context::currentWindowReceivesPointer() const
{
    return currentWindow_ && currentWindow_ == topWindowAt(pointer_.position);
}

uint32_t Context::buttonIndex(PointerButton button)
{
    const uint32_t index = static_cast<uint32_t>(button);
    return index < 3u ? index : 0u;
}

} // namespace ig
