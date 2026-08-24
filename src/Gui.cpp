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
      windows_(), windowsById_(), listScrolls_(), textScrolls_(), colorPickers_(), windowOrder_(), idStack_(), toasts_(), dragDrop_(), frameDrawList_(), dragDropDrawList_(), toastDrawList_(), modalDrawList_(), drawData_(),
      currentWindow_(), focusedWindow_(), draggingWindow_(), resizingWindow_(), activeWidget_(InvalidWidgetId),
      hotWidget_(InvalidWidgetId), lastItemId_(InvalidWidgetId),
      focusedWidget_(InvalidWidgetId), textInputWidget_(InvalidWidgetId), openCombo_(InvalidWidgetId),
      openMenu_(InvalidWidgetId), openContextMenu_(InvalidWidgetId), openSubMenu_(InvalidWidgetId),
      activeMenu_(InvalidWidgetId), subMenuParent_(InvalidWidgetId),
      activeModal_(InvalidWidgetId), dragWidget_(InvalidWidgetId),
      textCursor_(0), frameNumber_(0), nextZOrder_(1), windowDragOffset_(), menuBarBounds_(),
      menuPopupBounds_(), activeMenuBounds_(), subMenuPopupBounds_(), subMenuParentBounds_(),
      menuBarCursorX_(0.0f), menuBarActive_(false),
      dragStartValue_(0.0f), dragStartX_(0.0f),
      wantsKeyboard_(false), wantsTextInput_(false), backspacePressed_(false), enterPressed_(false),
      homePressed_(false), endPressed_(false), upPressed_(false), downPressed_(false),
      copyRequested_(false), pasteRequested_(false)
{
    events_.reserve(32);
    windows_.reserve(8);
    windowsById_.reserve(8);
    listScrolls_.reserve(8);
    textScrolls_.reserve(8);
    colorPickers_.reserve(8);
    windowOrder_.reserve(8);
    idStack_.reserve(8);
    toasts_.reserve(4);
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
    dragDropDrawList_.clear();
    toastDrawList_.clear();
    modalDrawList_.clear();
    for (ct::Vector<ToastState>::size_type i = 0u; i < toasts_.size(); ++i)
    {
        if (toasts_[i].remaining > 0.0f)
        {
            toasts_[i].remaining -= frame_.deltaSeconds;
            if (toasts_[i].remaining < 0.0f)
                toasts_[i].remaining = 0.0f;
        }
    }
    for (ct::SlotMap<WindowState>::iterator it = windows_.begin(); it != windows_.end(); ++it)
    {
        it->drawList.clear();
        it->overlayDrawList.clear();
    }
    hotWidget_ = InvalidWidgetId;
    lastItemId_ = InvalidWidgetId;
    activeMenu_ = InvalidWidgetId;
    menuBarActive_ = false;
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
    drawDragDropPreview();
    frameDrawList_.append(dragDropDrawList_);
    drawToasts();
    frameDrawList_.append(toastDrawList_);
    frameDrawList_.append(modalDrawList_);

    const uint32_t left = buttonIndex(PointerButton::Left);
    if (dragDrop_.armed && pointer_.released[left])
    {
        if (activeWidget_ == dragDrop_.widget)
            activeWidget_ = InvalidWidgetId;
        dragDrop_ = DragDropState();
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
    return activeModal_ != InvalidWidgetId || activeWidget_ != InvalidWidgetId || hotWidget_ != InvalidWidgetId;
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

bool Context::smallButton(StringView labelText, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;
    const WidgetId id = combineIds(makeWidgetId(labelText), 0x534d414c4c42544eull);
    const bool hovered = itemHovered(rect, clip, id);
    const bool clicked = itemClicked(rect, clip, id);
    drawList->addRectFilled(rect, hovered ? theme_.buttonHovered : theme_.buttonBackground, clip);
    const float fontSize = theme_.fontSize * 0.82f;
    const TextMetrics metrics = measureText(theme_.font, labelText, fontSize);
    drawText(*drawList, theme_.font, labelText,
             Vec2(rect.x + (rect.width - metrics.width) * 0.5f,
                  rect.y + (rect.height - metrics.height) * 0.5f),
             fontSize, theme_.buttonText, clip);
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

bool Context::treeItem(StringView labelText, bool &expanded, const TreeItemStyle &style,
                       const Rect &bounds)
{
    return treeItem(makeWidgetId(labelText), labelText, expanded, style, bounds, nullptr);
}

bool Context::treeItem(WidgetId nodeId, StringView labelText, bool &expanded,
                       const TreeItemStyle &style, const Rect &bounds, TreeDrop *drop)
{
    WindowState *window = currentWindow();
    if (!window || nodeId == InvalidWidgetId)
        return false;

    const WidgetId id = combineIds(makeWidgetId(labelText), nodeId);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const float arrowWidth = rect.height * 0.65f;
    const Rect arrowRect(rect.x, rect.y, arrowWidth, rect.height);
    const WidgetId arrowId = combineIds(id, 0x4152524f57ull);
    const uint32_t left = buttonIndex(PointerButton::Left);
    const Rect visible = intersect(rect, clip);
    const bool pressedOnRow = !style.disabled && pointer_.pressed[left] &&
                              currentWindow_ == focusedWindow_ &&
                              activeWidget_ == InvalidWidgetId &&
                              contains(visible, pointer_.pressedPosition[left]) &&
                              !contains(arrowRect, pointer_.pressedPosition[left]);
    if (pressedOnRow)
    {
        // Keep the regular row click available for selection. The item becomes a
        // drag source only if it moves far enough on a subsequent frame.
        dragDrop_.widget = id;
        dragDrop_.payload.type = 0x545245454954454dull;
        dragDrop_.payload.source = nodeId;
        dragDrop_.payload.data = nodeId;
        dragDrop_.preview = String(labelText.data(), labelText.size());
        dragDrop_.pressedPosition = pointer_.pressedPosition[left];
        dragDrop_.armed = true;
        dragDrop_.active = false;
        dragDrop_.accepted = false;
    }
    if (dragDrop_.armed && dragDrop_.widget == id && activeWidget_ == id)
    {
        const float deltaX = pointer_.position.x - dragDrop_.pressedPosition.x;
        const float deltaY = pointer_.position.y - dragDrop_.pressedPosition.y;
        if (pointer_.down[left] && deltaX * deltaX + deltaY * deltaY >= 16.0f)
            dragDrop_.active = true;
    }
    const bool rowHovered = !style.disabled && itemHovered(rect, clip, id);
    const bool arrowHovered = !style.disabled && !style.leaf && itemHovered(arrowRect, clip, arrowId);
    const bool expansionClicked = !style.disabled && !style.leaf &&
                                  itemClicked(arrowRect, clip, arrowId);
    if (expansionClicked)
        expanded = !expanded;
    const bool selected = !style.disabled && !expansionClicked && itemClicked(rect, clip, id);

    const Color rowColor = style.selected ? theme_.selectableSelected
                         : (rowHovered ? theme_.selectableHovered : theme_.inputBg);
    if (style.selected || rowHovered)
        drawList->addRectFilled(rect, rowColor, clip);

    const float centerY = rect.y + rect.height * 0.5f;
    const float iconSize = rect.height * 0.38f;
    const float iconX = rect.x + arrowWidth + theme_.windowPadding * 0.25f;
    if (!style.leaf)
    {
        const float arrowSize = rect.height * 0.20f;
        const float arrowX = rect.x + arrowWidth * 0.35f;
        const Vec2 arrow[] = {
            expanded ? Vec2(arrowX, centerY - arrowSize * 0.5f)
                     : Vec2(arrowX - arrowSize * 0.25f, centerY - arrowSize),
            expanded ? Vec2(arrowX + arrowSize, centerY - arrowSize * 0.5f)
                     : Vec2(arrowX - arrowSize * 0.25f, centerY + arrowSize),
            expanded ? Vec2(arrowX + arrowSize * 0.5f, centerY + arrowSize * 0.5f)
                     : Vec2(arrowX + arrowSize * 0.75f, centerY)
        };
        drawList->addPolygonFilled(Span<const Vec2>(arrow), theme_.menuSubmenuArrow, clip);
    }
    const Color iconColor = style.disabled
        ? Color(style.typeColor.r, style.typeColor.g, style.typeColor.b, 90u) : style.typeColor;
    drawList->addRectFilled(Rect(iconX, centerY - iconSize * 0.5f, iconSize, iconSize), iconColor, clip);

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const Color textColor = style.disabled
        ? Color(theme_.labelText.r, theme_.labelText.g, theme_.labelText.b, 110u) : theme_.labelText;
    drawText(*drawList, theme_.font, labelText,
             Vec2(iconX + iconSize + theme_.windowPadding * 0.5f,
                  rect.y + (rect.height - metrics.height) * 0.5f),
             theme_.fontSize, textColor, clip);
    if (drop && dragDrop_.active && dragDrop_.payload.type == 0x545245454954454dull &&
        dragDrop_.payload.source != nodeId && !style.disabled && contains(visible, pointer_.position))
    {
        const float localY = pointer_.position.y - rect.y;
        const TreeDropPosition position = localY < rect.height * 0.25f ? TreeDropPosition::Before
                                       : (localY >= rect.height * 0.75f ||
                                          (style.leaf && !style.acceptsChildren))
                                           ? TreeDropPosition::After : TreeDropPosition::Inside;
        if (position == TreeDropPosition::Inside)
            drawList->addRect(rect, theme_.dialogBtnPrimary, clip, 2.0f);
        else
        {
            const float lineY = position == TreeDropPosition::Before ? rect.y : rect.y + rect.height;
            drawList->addLine(Vec2(rect.x, lineY), Vec2(rect.x + rect.width, lineY),
                              theme_.dialogBtnPrimary, clip, 2.0f);
        }
        if (pointer_.released[left])
        {
            drop->source = dragDrop_.payload.source;
            drop->target = nodeId;
            drop->position = position;
            dragDrop_.accepted = true;
        }
    }
    (void)arrowHovered;
    return selected;
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

bool Context::beginMenuBar(const Rect &bounds)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList)
        return false;
    const Rect rect = contentRect(bounds);
    if (rect.width <= 0.0f || rect.height <= 0.0f)
        return false;
    menuBarBounds_ = rect;
    menuBarCursorX_ = rect.x;
    menuBarActive_ = true;
    drawList->addRectFilled(rect, theme_.menuBarBg, contentClip());
    return true;
}

void Context::endMenuBar()
{
    menuBarActive_ = false;
}

bool Context::beginMenu(StringView labelText)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList || !menuBarActive_)
        return false;

    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const float width = metrics.width + theme_.menuItemPadX * 2.0f;
    const Rect button(menuBarCursorX_, menuBarBounds_.y, width, menuBarBounds_.height);
    menuBarCursorX_ += width;
    const WidgetId id = combineIds(makeWidgetId(labelText), 0x4d454e55ull);
    const uint32_t left = buttonIndex(PointerButton::Left);
    if (openMenu_ == id && pointer_.pressed[left] && currentWindowReceivesPointer() &&
        !contains(button, pointer_.pressedPosition[left]) &&
        !contains(menuPopupBounds_, pointer_.pressedPosition[left]) &&
        !contains(subMenuPopupBounds_, pointer_.pressedPosition[left]))
    {
        openMenu_ = InvalidWidgetId;
        openSubMenu_ = InvalidWidgetId;
    }

    const bool hovered = itemHovered(button, contentClip(), id);
    if (itemClicked(button, contentClip(), id))
    {
        openMenu_ = openMenu_ == id ? InvalidWidgetId : id;
        openContextMenu_ = InvalidWidgetId;
        openSubMenu_ = InvalidWidgetId;
        if (openMenu_ == id)
            menuPopupBounds_ = Rect(button.x, button.y + button.height, theme_.menuMinWidth, 0.0f);
    }
    drawList->addRectFilled(button, openMenu_ == id || hovered ? theme_.menuBarItemHover : theme_.menuBarBg,
                            contentClip());
    drawText(*drawList, theme_.font, labelText,
             Vec2(button.x + theme_.menuItemPadX,
                  button.y + (button.height - metrics.height) * 0.5f),
             theme_.fontSize, theme_.menuItemText, contentClip());

    if (openMenu_ != id)
        return false;
    activeMenu_ = id;
    activeMenuBounds_ = menuPopupBounds_;
    activeMenuBounds_.x = button.x;
    activeMenuBounds_.y = button.y + button.height;
    activeMenuBounds_.width = theme_.menuMinWidth;
    activeMenuBounds_.height = 0.0f;
    return true;
}

void Context::endMenu()
{
    if (activeMenu_ == InvalidWidgetId)
        return;
    WindowState *window = currentWindow();
    if (window && activeMenuBounds_.height > 0.0f)
    {
        const Rect clip = contentClip();
        window->overlayDrawList.addRect(activeMenuBounds_, theme_.menuBorder, clip);
        menuPopupBounds_ = activeMenuBounds_;
    }
    activeMenu_ = InvalidWidgetId;
}

bool Context::menuItemInternal(StringView labelText, bool enabled, bool *checked)
{
    WindowState *window = currentWindow();
    if (!window || activeMenu_ == InvalidWidgetId)
        return false;
    const Rect clip = contentClip();
    DrawList &popup = window->overlayDrawList;
    const Rect item(activeMenuBounds_.x, activeMenuBounds_.y + activeMenuBounds_.height,
                    activeMenuBounds_.width, theme_.menuItemHeight);
    activeMenuBounds_.height += item.height;
    const WidgetId id = combineIds(activeMenu_, static_cast<WidgetId>(activeMenuBounds_.height * 100.0f));
    const bool hovered = enabled && itemHovered(item, clip, id);
    const bool clicked = enabled && itemClicked(item, clip, id);
    popup.addRectFilled(item, hovered ? theme_.menuItemHover : theme_.menuBg, clip);
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    const float checkWidth = checked ? theme_.menuItemHeight : 0.0f;
    if (checked && *checked)
    {
        const float checkSize = theme_.menuItemHeight * 0.38f;
        const float checkX = item.x + theme_.menuItemPadX + (theme_.menuItemHeight - checkSize) * 0.5f;
        const float checkY = item.y + (item.height - checkSize) * 0.5f;
        popup.addLine(Vec2(checkX, checkY + checkSize * 0.50f),
                      Vec2(checkX + checkSize * 0.35f, checkY + checkSize),
                      theme_.menuCheckMark, clip, 2.0f);
        popup.addLine(Vec2(checkX + checkSize * 0.35f, checkY + checkSize),
                      Vec2(checkX + checkSize, checkY), theme_.menuCheckMark, clip, 2.0f);
    }
    drawText(popup, theme_.font, labelText,
             Vec2(item.x + theme_.menuItemPadX + checkWidth,
                  item.y + (item.height - metrics.height) * 0.5f),
             theme_.fontSize, enabled ? (hovered ? theme_.menuItemTextHover : theme_.menuItemText)
                                : theme_.menuItemDisabled,
             clip);
    if (clicked)
    {
        if (checked)
            *checked = !*checked;
        openMenu_ = InvalidWidgetId;
        openContextMenu_ = InvalidWidgetId;
        openSubMenu_ = InvalidWidgetId;
    }
    return clicked;
}

bool Context::menuItem(StringView labelText, bool enabled)
{
    return menuItemInternal(labelText, enabled, nullptr);
}

bool Context::menuCheckbox(StringView labelText, bool &checked, bool enabled)
{
    return menuItemInternal(labelText, enabled, &checked);
}

bool Context::beginSubMenu(StringView labelText, bool enabled)
{
    WindowState *window = currentWindow();
    if (!window || activeMenu_ == InvalidWidgetId || subMenuParent_ != InvalidWidgetId)
        return false;
    const Rect clip = contentClip();
    DrawList &popup = window->overlayDrawList;
    const Rect item(activeMenuBounds_.x, activeMenuBounds_.y + activeMenuBounds_.height,
                    activeMenuBounds_.width, theme_.menuItemHeight);
    activeMenuBounds_.height += item.height;
    const WidgetId parentId = activeMenu_;
    const WidgetId id = combineIds(parentId, combineIds(makeWidgetId(labelText), 0x5355424d454e55ull));
    const bool hovered = enabled && itemHovered(item, clip, id);
    const bool clicked = enabled && itemClicked(item, clip, id);
    if (enabled && (hovered || clicked))
    {
        openSubMenu_ = id;
        float popupX = item.x + item.width;
        const float clipRight = clip.x + clip.width;
        if (popupX + theme_.menuMinWidth > clipRight)
        {
            const float leftPopupX = item.x - theme_.menuMinWidth;
            popupX = leftPopupX >= clip.x ? leftPopupX : clipRight - theme_.menuMinWidth;
        }
        if (popupX < clip.x)
            popupX = clip.x;
        subMenuPopupBounds_ = Rect(popupX, item.y, theme_.menuMinWidth, 0.0f);
    }
    popup.addRectFilled(item, openSubMenu_ == id || hovered ? theme_.menuItemHover : theme_.menuBg, clip);
    const TextMetrics metrics = measureText(theme_.font, labelText, theme_.fontSize);
    drawText(popup, theme_.font, labelText,
             Vec2(item.x + theme_.menuItemPadX, item.y + (item.height - metrics.height) * 0.5f),
             theme_.fontSize, enabled ? theme_.menuItemText : theme_.menuItemDisabled, clip);
    const float arrowSize = item.height * 0.18f;
    const float arrowX = item.x + item.width - theme_.menuItemPadX - arrowSize;
    const float arrowY = item.y + item.height * 0.5f;
    const Vec2 arrow[] = {
        Vec2(arrowX, arrowY - arrowSize), Vec2(arrowX, arrowY + arrowSize),
        Vec2(arrowX + arrowSize, arrowY)
    };
    popup.addPolygonFilled(Span<const Vec2>(arrow), theme_.menuSubmenuArrow, clip);
    if (openSubMenu_ != id)
        return false;

    subMenuParent_ = parentId;
    subMenuParentBounds_ = activeMenuBounds_;
    activeMenu_ = id;
    activeMenuBounds_ = subMenuPopupBounds_;
    activeMenuBounds_.height = 0.0f;
    return true;
}

void Context::endSubMenu()
{
    if (subMenuParent_ == InvalidWidgetId || activeMenu_ != openSubMenu_)
        return;
    WindowState *window = currentWindow();
    if (window && activeMenuBounds_.height > 0.0f)
    {
        window->overlayDrawList.addRect(activeMenuBounds_, theme_.menuBorder, contentClip());
        subMenuPopupBounds_ = activeMenuBounds_;
    }
    activeMenu_ = subMenuParent_;
    activeMenuBounds_ = subMenuParentBounds_;
    subMenuParent_ = InvalidWidgetId;
}

void Context::menuSeparator()
{
    WindowState *window = currentWindow();
    if (!window || activeMenu_ == InvalidWidgetId)
        return;
    const Rect separator(activeMenuBounds_.x, activeMenuBounds_.y + activeMenuBounds_.height,
                         activeMenuBounds_.width, theme_.itemSpacing);
    activeMenuBounds_.height += separator.height;
    window->overlayDrawList.addRectFilled(separator, theme_.menuBg, contentClip());
    window->overlayDrawList.addRectFilled(Rect(separator.x + theme_.menuItemPadX,
                                               separator.y + separator.height * 0.5f,
                                               separator.width - theme_.menuItemPadX * 2.0f, 1.0f),
                                         theme_.menuSeparator, contentClip());
}

bool Context::beginContextMenu(StringView idText, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window)
        return false;
    const WidgetId id = combineIds(makeWidgetId(idText), 0x434f4e54455854ull);
    const Rect target = contentRect(bounds);
    const Rect visible = intersect(target, contentClip());
    const uint32_t right = buttonIndex(PointerButton::Right);
    const uint32_t left = buttonIndex(PointerButton::Left);
    if (pointer_.pressed[right] && currentWindowReceivesPointer() &&
        contains(visible, pointer_.pressedPosition[right]))
    {
        openContextMenu_ = id;
        openMenu_ = InvalidWidgetId;
        openSubMenu_ = InvalidWidgetId;
        menuPopupBounds_ = Rect(pointer_.pressedPosition[right].x, pointer_.pressedPosition[right].y,
                                theme_.menuMinWidth, 0.0f);
    }
    if (openContextMenu_ != id)
        return false;
    if (pointer_.pressed[left] && currentWindowReceivesPointer() &&
        !contains(menuPopupBounds_, pointer_.pressedPosition[left]) &&
        !contains(subMenuPopupBounds_, pointer_.pressedPosition[left]))
    {
        openContextMenu_ = InvalidWidgetId;
        openSubMenu_ = InvalidWidgetId;
        return false;
    }
    activeMenu_ = id;
    activeMenuBounds_ = menuPopupBounds_;
    activeMenuBounds_.height = 0.0f;
    return true;
}

void Context::endContextMenu()
{
    endMenu();
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

bool Context::dragFloat(StringView labelText, float &value, float minimum, float maximum,
                        float speed, const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || maximum <= minimum || speed <= 0.0f)
        return false;

    const WidgetId id = makeWidgetId(labelText);
    const Rect rect = contentRect(bounds);
    const Rect clip = contentClip();
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    const uint32_t left = buttonIndex(PointerButton::Left);
    const bool hovered = itemHovered(rect, clip, id);
    const bool pressedHere = pointer_.pressed[left] && currentWindow_ == focusedWindow_ &&
                             activeWidget_ == InvalidWidgetId &&
                             contains(intersect(rect, clip), pointer_.pressedPosition[left]);
    if (pressedHere)
    {
        activeWidget_ = id;
        focusedWidget_ = id;
        dragWidget_ = id;
        dragStartValue_ = value;
        dragStartX_ = pointer_.pressedPosition[left].x;
    }

    bool changed = false;
    const bool dragging = activeWidget_ == id && dragWidget_ == id;
    if (dragging && (pointer_.down[left] || pointer_.pressed[left] || pointer_.released[left]))
    {
        const float next = clamp(dragStartValue_ + (pointer_.position.x - dragStartX_) * speed,
                                 minimum, maximum);
        changed = next != value;
        value = next;
    }
    if (dragging && pointer_.released[left])
    {
        activeWidget_ = InvalidWidgetId;
        dragWidget_ = InvalidWidgetId;
    }

    const TextMetrics labelMetrics = measureText(theme_.font, labelText, theme_.fontSize);
    const float valueWidth = rect.width * 0.42f;
    const Rect valueRect(rect.x + rect.width - valueWidth, rect.y, valueWidth, rect.height);
    const Color background = dragging ? theme_.buttonPressed
                                      : (hovered ? theme_.buttonHovered : theme_.inputBg);
    drawText(*drawList, theme_.font, labelText,
             Vec2(rect.x, rect.y + (rect.height - labelMetrics.height) * 0.5f),
             theme_.fontSize, theme_.labelText, clip);
    drawList->addRectFilled(valueRect, background, clip);
    const String displayed = String::number(static_cast<double>(value), 3);
    const TextMetrics valueMetrics = measureText(theme_.font, displayed, theme_.fontSize);
    drawText(*drawList, theme_.font, displayed,
             Vec2(valueRect.x + (valueRect.width - valueMetrics.width) * 0.5f,
                  valueRect.y + (valueRect.height - valueMetrics.height) * 0.5f),
             theme_.fontSize, theme_.buttonText, clip);
    return changed;
}

bool Context::dragInt(StringView labelText, int &value, int minimum, int maximum,
                      int speed, const Rect &bounds)
{
    if (maximum <= minimum || speed <= 0)
        return false;
    float temporary = static_cast<float>(value);
    const bool dragged = dragFloat(labelText, temporary, static_cast<float>(minimum),
                                   static_cast<float>(maximum), static_cast<float>(speed), bounds);
    if (!dragged)
        return false;
    const int rounded = temporary >= 0.0f ? static_cast<int>(temporary + 0.5f)
                                          : static_cast<int>(temporary - 0.5f);
    const int clamped = rounded < minimum ? minimum : (rounded > maximum ? maximum : rounded);
    const bool changed = value != clamped;
    value = clamped;
    return changed;
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
    DrawList *drawList = currentDrawList();
    if (!drawList || rect.width <= 0.0f || rect.height <= 0.0f)
        return false;

    uint32_t lineCount = 1u;
    for (String::size_type i = 0u; i < value.size(); ++i)
    {
        if (value[i] == '\n')
            ++lineCount;
    }
    const TextMetrics lineMetrics = measureText(theme_.font, StringView("M"), theme_.fontSize);
    const float lineHeight = lineMetrics.height > 0.0f ? lineMetrics.height : theme_.fontSize;
    const float padding = theme_.textEditPadding;
    const float innerHeight = rect.height - padding * 2.0f;
    const int visibleLines = innerHeight >= lineHeight
        ? static_cast<int>(innerHeight / lineHeight) : 1;
    const int maximumScroll = static_cast<int>(lineCount) - visibleLines;
    const bool hasScrollbar = maximumScroll > 0;
    const float scrollbarWidth = hasScrollbar ? theme_.scrollbarWidth : 0.0f;
    const Rect textArea(rect.x, rect.y, rect.width - scrollbarWidth, rect.height);
    const Rect textClip = intersect(Rect(textArea.x + padding, textArea.y + padding,
                                         textArea.width - padding * 2.0f,
                                         textArea.height - padding * 2.0f), clip);
    const Rect scrollbar(rect.x + rect.width - scrollbarWidth, rect.y, scrollbarWidth, rect.height);
    const WidgetId scrollbarId = combineIds(id, 0x544558545343524Cull);
    int *scroll = textScrolls_.find(id);
    if (!scroll)
    {
        textScrolls_.put(id, 0);
        scroll = textScrolls_.find(id);
    }
    if (!scroll)
        return false;
    if (*scroll < 0)
        *scroll = 0;
    if (*scroll > maximumScroll)
        *scroll = maximumScroll > 0 ? maximumScroll : 0;

    const bool hovered = itemHovered(textArea, clip, id);
    itemClicked(textArea, clip, id);
    if (hasScrollbar && currentWindowReceivesPointer() &&
        contains(intersect(rect, clip), pointer_.position) && pointer_.wheelY != 0.0f)
    {
        const int delta = pointer_.wheelY > 0.0f ? -1 : 1;
        *scroll += delta;
        if (*scroll < 0)
            *scroll = 0;
        if (*scroll > maximumScroll)
            *scroll = maximumScroll;
    }

    Rect thumb;
    if (hasScrollbar)
    {
        const float requestedThumb = scrollbar.height * static_cast<float>(visibleLines) /
                                     static_cast<float>(lineCount);
        const float thumbHeight = requestedThumb > theme_.scrollbarMinThumb
            ? requestedThumb : theme_.scrollbarMinThumb;
        const float travel = scrollbar.height - thumbHeight;
        const float offset = maximumScroll > 0
            ? travel * static_cast<float>(*scroll) / static_cast<float>(maximumScroll) : 0.0f;
        thumb = Rect(scrollbar.x, scrollbar.y + offset, scrollbar.width, thumbHeight);
        const uint32_t left = buttonIndex(PointerButton::Left);
        const bool pressedThumb = pointer_.pressed[left] && currentWindow_ == focusedWindow_ &&
                                  activeWidget_ == InvalidWidgetId &&
                                  contains(intersect(thumb, clip), pointer_.pressedPosition[left]);
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
                    ? clamp((pointer_.position.y - scrollbar.y - thumb.height * 0.5f) / travel,
                            0.0f, 1.0f) : 0.0f;
                *scroll = static_cast<int>(normalized * static_cast<float>(maximumScroll) + 0.5f);
            }
            if (pointer_.released[left])
                activeWidget_ = InvalidWidgetId;
        }
        const float updatedOffset = maximumScroll > 0
            ? travel * static_cast<float>(*scroll) / static_cast<float>(maximumScroll) : 0.0f;
        thumb.y = scrollbar.y + updatedOffset;
    }

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

    if (focused)
    {
        uint32_t cursorLine = 0u;
        for (String::size_type i = 0u; i < textCursor_; ++i)
        {
            if (value[i] == '\n')
                ++cursorLine;
        }
        if (static_cast<int>(cursorLine) < *scroll)
            *scroll = static_cast<int>(cursorLine);
        else if (static_cast<int>(cursorLine) >= *scroll + visibleLines)
            *scroll = static_cast<int>(cursorLine) - visibleLines + 1;
        if (*scroll > maximumScroll)
            *scroll = maximumScroll > 0 ? maximumScroll : 0;
    }

    const Color background = focused ? theme_.buttonHovered
                                     : (hovered ? theme_.buttonHovered : theme_.inputBg);
    drawList->addRectFilled(rect, background, clip);
    drawText(*drawList, theme_.font, value,
             Vec2(textArea.x + padding, textArea.y + padding - static_cast<float>(*scroll) * lineHeight),
             theme_.fontSize, theme_.buttonText, textClip);
    if (hasScrollbar)
    {
        drawList->addRectFilled(scrollbar, theme_.sliderBackground, clip);
        drawList->addRectFilled(thumb, theme_.scrollbarThumb, clip);
    }
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
        const float caretX = textArea.x + padding + prefixMetrics.width;
        const float caretY = textArea.y + padding +
                             (static_cast<float>(line) - static_cast<float>(*scroll)) * lineHeight;
        if (caretY >= textArea.y + padding && caretY < textArea.y + textArea.height - padding)
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
                        hueWidth, pickerSize);
    ColorPickerState *picker = colorPickers_.find(id);
    if (!picker)
    {
        colorPickers_.put(id, ColorPickerState());
        picker = colorPickers_.find(id);
    }
    if (!picker)
        return false;

    // RGB cannot represent hue for greys and black.  Keep the last hue chosen
    // by the user so dragging through the left/bottom edge of the SV square
    // does not make the next drag jump back to red.
    if (!picker->initialized || picker->lastColor != value)
    {
        float nextHue = 0.0f;
        float nextSaturation = 0.0f;
        float nextBrightness = 0.0f;
        colorToHsv(value, nextHue, nextSaturation, nextBrightness);
        if (!picker->initialized || nextSaturation > 0.0001f)
            picker->hue = nextHue;
        picker->saturation = nextSaturation;
        picker->brightness = nextBrightness;
        picker->lastColor = value;
        picker->initialized = true;
    }

    float hue = picker->hue;
    float saturation = picker->saturation;
    float brightness = picker->brightness;
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
            const float nextAlpha = clamp((pointer_.position.y - alphaBar.y) / alphaBar.height, 0.0f, 1.0f);
            changed = changed || nextAlpha != alpha;
            alpha = nextAlpha;
        }
        if (pointer_.released[left])
            activeWidget_ = InvalidWidgetId;
    }

    if (changed)
    {
        value = colorFromHsv(hue, saturation, brightness,
                             static_cast<uint8_t>(alpha * 255.0f + 0.5f));
        picker->hue = hue;
        picker->saturation = saturation;
        picker->brightness = brightness;
        picker->lastColor = value;
    }

    drawText(*drawList, theme_.font, labelText, Vec2(rect.x, rect.y), theme_.fontSize,
             theme_.labelText, clip);
    // A single quad has a visible diagonal because the GPU splits it into two
    // triangles. Thin horizontal bands preserve the SV curve without a seam.
    const uint32_t saturationValueBands = 32u;
    for (uint32_t band = 0u; band < saturationValueBands; ++band)
    {
        const float topValue = 1.0f - static_cast<float>(band) /
                                       static_cast<float>(saturationValueBands);
        const float bottomValue = 1.0f - static_cast<float>(band + 1u) /
                                          static_cast<float>(saturationValueBands);
        const Rect bandRect(saturationValue.x,
                            saturationValue.y + saturationValue.height * static_cast<float>(band) /
                                                    static_cast<float>(saturationValueBands),
                            saturationValue.width,
                            saturationValue.height / static_cast<float>(saturationValueBands));
        const Color topGray(static_cast<uint8_t>(topValue * 255.0f + 0.5f),
                             static_cast<uint8_t>(topValue * 255.0f + 0.5f),
                             static_cast<uint8_t>(topValue * 255.0f + 0.5f), 255u);
        const Color bottomGray(static_cast<uint8_t>(bottomValue * 255.0f + 0.5f),
                                static_cast<uint8_t>(bottomValue * 255.0f + 0.5f),
                                static_cast<uint8_t>(bottomValue * 255.0f + 0.5f), 255u);
        drawList->addRectGradient(bandRect, topGray,
                                  colorFromHsv(hue, 1.0f, topValue, 255u),
                                  colorFromHsv(hue, 1.0f, bottomValue, 255u), bottomGray, clip);
    }
    for (uint32_t segment = 0u; segment < 6u; ++segment)
    {
        const float segmentHeight = hueBar.height / 6.0f;
        const Rect segmentRect(hueBar.x, hueBar.y + segmentHeight * static_cast<float>(segment),
                               hueBar.width, segmentHeight);
        const Color top = colorFromHsv(static_cast<float>(segment) / 6.0f, 1.0f, 1.0f, 255u);
        const Color bottom = colorFromHsv(static_cast<float>(segment + 1u) / 6.0f, 1.0f, 1.0f, 255u);
        drawList->addRectGradient(segmentRect, top, top, bottom, bottom, clip);
    }
    const float checkerSize = alphaBar.height > 0.0f ? alphaBar.height / 8.0f : 0.0f;
    if (checkerSize > 0.0f)
    {
        for (uint32_t y = 0u; y < 8u; ++y)
        {
            for (uint32_t x = 0u; x < 2u; ++x)
            {
                const Color checker = (x + y) % 2u == 0u ? Color(68u, 68u, 75u, 255u)
                                                           : Color(108u, 108u, 115u, 255u);
                drawList->addRectFilled(Rect(alphaBar.x + alphaBar.width * 0.5f * static_cast<float>(x),
                                             alphaBar.y + checkerSize * static_cast<float>(y),
                                             alphaBar.width * 0.5f, checkerSize), checker, clip);
            }
        }
        const Color transparent(value.r, value.g, value.b, 0u);
        drawList->addRectGradient(alphaBar, transparent, transparent, value, value, clip);
    }

    const Vec2 saturationValueCursor(saturationValue.x + saturation * saturationValue.width,
                                     saturationValue.y + (1.0f - brightness) * saturationValue.height);
    drawList->addCircleFilled(saturationValueCursor, 6.0f, Color(255u, 255u, 255u, 255u), clip);
    drawList->addCircleFilled(saturationValueCursor, 3.5f,
                              colorFromHsv(hue, saturation, brightness, 255u), clip);
    const float hueCursorY = hueBar.y + hue * hueBar.height;
    drawList->addRectFilled(Rect(hueBar.x - 2.0f, hueCursorY - 1.0f, hueBar.width + 4.0f, 2.0f),
                            Color(255u, 255u, 255u, 255u), clip);
    const float alphaCursorY = alphaBar.y + alpha * alphaBar.height;
    drawList->addRectFilled(Rect(alphaBar.x - 2.0f, alphaCursorY - 1.0f, alphaBar.width + 4.0f, 2.0f),
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

bool Context::smallImageButton(StringView labelText, TextureId texture, const Rect &bounds,
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
    const WidgetId id = combineIds(makeWidgetId(labelText), 0x534d414c4c494d47ull);
    const bool hovered = itemHovered(rect, clip, id);
    const bool clicked = itemClicked(rect, clip, id);
    const float inset = rect.width < rect.height ? rect.width * 0.16f : rect.height * 0.16f;
    const Rect imageRect(rect.x + inset, rect.y + inset,
                         rect.width - inset * 2.0f, rect.height - inset * 2.0f);
    drawList->addRectFilled(rect, hovered ? theme_.buttonHovered : theme_.buttonBackground, clip);
    drawList->addImage(texture, imageRect, uvMin, uvMax, tint, clip);
    if (hovered)
        drawList->addRect(imageRect, theme_.focusColor, clip);
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

MessageBoxResult Context::messageBox(StringView title, StringView message, bool &open,
                                     bool showCancel)
{
    MessageBoxOptions options;
    options.showCancel = showCancel;
    return messageBox(title, message, open, options);
}

MessageBoxResult Context::messageBox(StringView title, StringView message, bool &open,
                                     const MessageBoxOptions &options)
{
    const WidgetId id = combineIds(hashText(title), hashText(message));
    if (!open)
    {
        if (activeModal_ == id)
            activeModal_ = InvalidWidgetId;
        return MessageBoxResult::None;
    }

    activeModal_ = id;
    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const TextMetrics titleMetrics = measureText(theme_.font, title, theme_.fontSize);
    const TextMetrics messageMetrics = measureText(theme_.font, message, theme_.fontSize);
    const float padding = theme_.windowPadding * 2.0f;
    const float desiredWidth = messageMetrics.width + padding;
    const float width = desiredWidth < 300.0f ? 300.0f
                      : (desiredWidth > viewport.width - 24.0f ? viewport.width - 24.0f : desiredWidth);
    const bool inputMode = options.kind == MessageBoxKind::Input && options.inputValue;
    const float inputHeight = inputMode ? theme_.widgetHeight + theme_.itemSpacing : 0.0f;
    const float height = titleMetrics.height + messageMetrics.height + inputHeight + theme_.widgetHeight +
                         padding * 2.0f + theme_.itemSpacing * 2.0f;
    const Rect dialog((viewport.width - width) * 0.5f, (viewport.height - height) * 0.5f,
                      width > 0.0f ? width : 0.0f, height > 0.0f ? height : 0.0f);
    const Rect titleBar(dialog.x, dialog.y, dialog.width, titleMetrics.height + theme_.windowPadding * 1.5f);
    const float buttonWidth = 80.0f;
    const float buttonY = dialog.y + dialog.height - theme_.windowPadding - theme_.widgetHeight;
    const Rect acceptButton(dialog.x + dialog.width - theme_.windowPadding - buttonWidth,
                            buttonY, buttonWidth, theme_.widgetHeight);
    const Rect cancelButton(acceptButton.x - theme_.itemSpacing - buttonWidth,
                            buttonY, buttonWidth, theme_.widgetHeight);
    const Rect inputRect(dialog.x + theme_.windowPadding,
                         titleBar.y + titleBar.height + messageMetrics.height + theme_.itemSpacing * 2.0f,
                         dialog.width - theme_.windowPadding * 2.0f, theme_.widgetHeight);
    const WidgetId acceptId = combineIds(id, 0x4f4bull);
    const WidgetId cancelId = combineIds(id, 0x43414e43454cull);
    const uint32_t left = buttonIndex(PointerButton::Left);
    const bool hoveredAccept = contains(acceptButton, pointer_.position);
    const bool hoveredCancel = options.showCancel && contains(cancelButton, pointer_.position);
    if (pointer_.pressed[left] && activeWidget_ == InvalidWidgetId)
    {
        if (contains(acceptButton, pointer_.pressedPosition[left]))
            activeWidget_ = acceptId;
        else if (options.showCancel && contains(cancelButton, pointer_.pressedPosition[left]))
            activeWidget_ = cancelId;
    }

    MessageBoxResult result = MessageBoxResult::None;
    if (pointer_.released[left] && activeWidget_ == acceptId)
    {
        if (contains(acceptButton, pointer_.releasedPosition[left]))
        {
            open = false;
            activeModal_ = InvalidWidgetId;
            result = MessageBoxResult::Accepted;
        }
        activeWidget_ = InvalidWidgetId;
    }
    else if (pointer_.released[left] && activeWidget_ == cancelId)
    {
        if (contains(cancelButton, pointer_.releasedPosition[left]))
        {
            open = false;
            activeModal_ = InvalidWidgetId;
            result = MessageBoxResult::Cancelled;
        }
        activeWidget_ = InvalidWidgetId;
    }

    if (inputMode)
    {
        String &input = *options.inputValue;
        const WidgetId inputId = combineIds(id, 0x494e505554ull);
        if (textInputWidget_ != inputId)
            textCursor_ = input.size();
        textInputWidget_ = inputId;
        wantsKeyboard_ = true;
        wantsTextInput_ = true;
        if (textCursor_ > input.size())
            textCursor_ = input.size();
        if (homePressed_)
            textCursor_ = 0u;
        if (endPressed_)
            textCursor_ = input.size();
        if (copyRequested_)
            backend_.setClipboardText(input);
        if (pasteRequested_)
        {
            const String clipboard = backend_.clipboardText();
            if (!clipboard.empty())
            {
                input.insert(textCursor_, clipboard.data(), clipboard.size());
                textCursor_ += clipboard.size();
            }
        }
        for (ct::Vector<Event>::size_type i = 0u; i < textEvents_.size(); ++i)
        {
            const Event &event = textEvents_[i];
            input.insert(textCursor_, event.text, event.textLength);
            textCursor_ += event.textLength;
        }
        if (backspacePressed_ && textCursor_ != 0u)
        {
            String::size_type eraseBegin = textCursor_ - 1u;
            while (eraseBegin != 0u && (static_cast<uint8_t>(input[eraseBegin]) & 0xC0u) == 0x80u)
                --eraseBegin;
            input.erase(eraseBegin, textCursor_ - eraseBegin);
            textCursor_ = eraseBegin;
        }
    }

    Color kindColor = theme_.dialogBtnPrimary;
    StringView kindText("i");
    if (options.kind == MessageBoxKind::Warning)
    {
        kindColor = Color(235u, 180u, 65u, 255u);
        kindText = StringView("!");
    }
    else if (options.kind == MessageBoxKind::Error)
    {
        kindColor = theme_.dialogBtnDanger;
        kindText = StringView("x");
    }
    else if (options.kind == MessageBoxKind::Input)
    {
        kindColor = Color(160u, 125u, 230u, 255u);
        kindText = StringView(">");
    }

    modalDrawList_.addRectFilled(viewport, theme_.dialogScrim, viewport);
    modalDrawList_.addRectFilled(dialog, theme_.dialogBg, viewport);
    modalDrawList_.addRect(dialog, theme_.dialogBorder, viewport);
    modalDrawList_.addRectFilled(titleBar, theme_.floatTitleBg, viewport);
    modalDrawList_.addRectFilled(Rect(dialog.x, dialog.y, 4.0f, dialog.height), kindColor, viewport);
    modalDrawList_.addCircleFilled(Vec2(dialog.x + theme_.windowPadding * 1.5f,
                                         titleBar.y + titleBar.height * 0.5f),
                                    theme_.fontSize * 0.42f, kindColor, viewport);
    const TextMetrics kindMetrics = measureText(theme_.font, kindText, theme_.fontSize * 0.75f);
    drawText(modalDrawList_, theme_.font, kindText,
             Vec2(dialog.x + theme_.windowPadding * 1.5f - kindMetrics.width * 0.5f,
                  titleBar.y + (titleBar.height - kindMetrics.height) * 0.5f),
             theme_.fontSize * 0.75f, theme_.buttonText, viewport);
    drawText(modalDrawList_, theme_.font, title,
             Vec2(titleBar.x + theme_.windowPadding * 3.0f, titleBar.y + theme_.windowPadding * 0.5f),
             theme_.fontSize, theme_.dialogTitleText, viewport);
    drawText(modalDrawList_, theme_.font, message,
             Vec2(dialog.x + theme_.windowPadding, titleBar.y + titleBar.height + theme_.itemSpacing),
             theme_.fontSize, theme_.dialogText, viewport);
    if (inputMode)
    {
        String &input = *options.inputValue;
        modalDrawList_.addRectFilled(inputRect, theme_.inputBg, viewport);
        modalDrawList_.addRect(inputRect, theme_.inputBorderHover, viewport);
        const float leftPadding = theme_.textEditPadding;
        const StringView prefix(input.data(), textCursor_);
        const TextMetrics allMetrics = measureText(theme_.font, input, theme_.fontSize);
        const TextMetrics prefixMetrics = measureText(theme_.font, prefix, theme_.fontSize);
        const float available = inputRect.width - leftPadding * 2.0f;
        float horizontalOffset = allMetrics.width > available ? available - allMetrics.width : 0.0f;
        if (prefixMetrics.width + horizontalOffset < 0.0f)
            horizontalOffset = -prefixMetrics.width;
        else if (prefixMetrics.width + horizontalOffset > available)
            horizontalOffset = available - prefixMetrics.width;
        const Rect inputClip(inputRect.x + leftPadding, inputRect.y + leftPadding,
                             available, inputRect.height - leftPadding * 2.0f);
        const Vec2 inputTextPosition(inputRect.x + leftPadding + horizontalOffset,
                                     inputRect.y + (inputRect.height - allMetrics.height) * 0.5f);
        drawText(modalDrawList_, theme_.font, input, inputTextPosition, theme_.fontSize,
                 theme_.buttonText, inputClip);
        modalDrawList_.addRectFilled(Rect(inputTextPosition.x + prefixMetrics.width,
                                          inputRect.y + 5.0f, 1.0f, inputRect.height - 10.0f),
                                     theme_.buttonText, inputClip);
    }
    modalDrawList_.addRectFilled(acceptButton,
                                 hoveredAccept ? theme_.dialogBtnPrimaryHover : theme_.dialogBtnPrimary,
                                 viewport);
    const StringView acceptText("OK");
    const TextMetrics acceptMetrics = measureText(theme_.font, acceptText, theme_.fontSize);
    drawText(modalDrawList_, theme_.font, acceptText,
             Vec2(acceptButton.x + (acceptButton.width - acceptMetrics.width) * 0.5f,
                  acceptButton.y + (acceptButton.height - acceptMetrics.height) * 0.5f),
             theme_.fontSize, theme_.buttonText, viewport);
    if (options.showCancel)
    {
        modalDrawList_.addRectFilled(cancelButton,
                                     hoveredCancel ? theme_.dialogBtnHover : theme_.dialogBtnBg, viewport);
        const StringView cancelText("Cancel");
        const TextMetrics cancelMetrics = measureText(theme_.font, cancelText, theme_.fontSize);
        drawText(modalDrawList_, theme_.font, cancelText,
                 Vec2(cancelButton.x + (cancelButton.width - cancelMetrics.width) * 0.5f,
                      cancelButton.y + (cancelButton.height - cancelMetrics.height) * 0.5f),
                 theme_.fontSize, theme_.buttonText, viewport);
    }
    return result;
}

void Context::showToast(StringView idText, StringView text, ToastPosition position, float duration)
{
    if (duration <= 0.0f || text.empty())
        return;
    const WidgetId id = hashText(idText);
    for (ct::Vector<ToastState>::size_type i = 0u; i < toasts_.size(); ++i)
    {
        ToastState &toast = toasts_[i];
        if (toast.id == id)
        {
            toast.text = String(text.data(), text.size());
            toast.position = position;
            toast.remaining = duration;
            return;
        }
    }
    ToastState toast;
    toast.id = id;
    toast.text = String(text.data(), text.size());
    toast.position = position;
    toast.remaining = duration;
    toasts_.push_back(toast);
}

bool Context::beginDragSource(WidgetId source, WidgetId type, uint64_t data, StringView preview,
                              const Rect &bounds)
{
    WindowState *window = currentWindow();
    if (!window || source == InvalidWidgetId || type == InvalidWidgetId)
        return false;
    const Rect rect = contentRect(bounds);
    const Rect visible = intersect(rect, contentClip());
    const uint32_t left = buttonIndex(PointerButton::Left);
    const WidgetId widget = combineIds(makeWidgetId(StringView("drag source")), source);
    const bool pressedHere = pointer_.pressed[left] && currentWindow_ == focusedWindow_ &&
                             activeWidget_ == InvalidWidgetId && contains(visible, pointer_.pressedPosition[left]);
    if (pressedHere)
    {
        activeWidget_ = widget;
        dragDrop_.widget = widget;
        dragDrop_.payload.type = type;
        dragDrop_.payload.source = source;
        dragDrop_.payload.data = data;
        dragDrop_.preview = String(preview.data(), preview.size());
        dragDrop_.pressedPosition = pointer_.pressedPosition[left];
        dragDrop_.armed = true;
        dragDrop_.active = false;
        dragDrop_.accepted = false;
    }

    if (dragDrop_.armed && dragDrop_.widget == widget && activeWidget_ == widget)
    {
        const float deltaX = pointer_.position.x - dragDrop_.pressedPosition.x;
        const float deltaY = pointer_.position.y - dragDrop_.pressedPosition.y;
        if (pointer_.down[left] && deltaX * deltaX + deltaY * deltaY >= 16.0f)
            dragDrop_.active = true;
        return dragDrop_.active;
    }
    return false;
}

bool Context::acceptDragDropTarget(WidgetId acceptedType, const Rect &bounds,
                                   DragDropPayload &payload)
{
    if (!dragDrop_.active || dragDrop_.payload.type != acceptedType)
        return false;
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList)
        return false;
    const Rect rect = contentRect(bounds);
    const Rect visible = intersect(rect, contentClip());
    const bool hovered = contains(visible, pointer_.position);
    if (hovered)
    {
        drawList->addRect(rect, theme_.dialogBtnPrimary, contentClip(), 2.0f);
        const uint32_t left = buttonIndex(PointerButton::Left);
        if (pointer_.released[left])
        {
            payload = dragDrop_.payload;
            dragDrop_.accepted = true;
            return true;
        }
    }
    return false;
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

bool Context::smallButton(StringView labelText)
{
    const float fontSize = theme_.fontSize * 0.82f;
    const TextMetrics metrics = measureText(theme_.font, labelText, fontSize);
    const float paddingX = theme_.padding * 1.25f;
    const float paddingY = theme_.padding * 0.55f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, metrics.width + paddingX * 2.0f,
                      metrics.height + paddingY * 2.0f);
    const bool clicked = smallButton(labelText, Rect(bounds.x - layout_.origin.x,
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
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool clicked = selectable(labelText, selected,
                                    Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                         bounds.width, bounds.height));
    advanceLayout(bounds);
    return clicked;
}

bool Context::collapsingHeader(StringView labelText, bool &expanded, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
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
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool open = treeNode(labelText, expanded,
                               Rect(bounds.x - layout_.origin.x,
                                    bounds.y - layout_.origin.y,
                                    bounds.width, bounds.height));
    advanceLayout(bounds);
    return open;
}

bool Context::treeItem(StringView labelText, bool &expanded, const TreeItemStyle &style,
                       float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool selected = treeItem(labelText, expanded, style,
                                   Rect(bounds.x - layout_.origin.x,
                                        bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height));
    advanceLayout(bounds);
    return selected;
}

bool Context::treeItem(WidgetId nodeId, StringView labelText, bool &expanded,
                       const TreeItemStyle &style, TreeDrop *drop, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool selected = treeItem(nodeId, labelText, expanded, style,
                                   Rect(bounds.x - layout_.origin.x,
                                        bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height), drop);
    advanceLayout(bounds);
    return selected;
}

bool Context::tabBar(StringView labelText, int &currentItem, Span<const StringView> items,
                     float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
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
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
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
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
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
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = sliderFloat(labelText, value, minimum, maximum,
                                     Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                          bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::sliderInt(StringView labelText, int &value, int minimum, int maximum, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = sliderInt(labelText, value, minimum, maximum,
                                   Rect(bounds.x - layout_.origin.x,
                                        bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::dragFloat(StringView labelText, float &value, float minimum, float maximum,
                        float speed, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = dragFloat(labelText, value, minimum, maximum, speed,
                                   Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::dragInt(StringView labelText, int &value, int minimum, int maximum,
                      int speed, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = dragInt(labelText, value, minimum, maximum, speed,
                                 Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                      bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::stepperInt(StringView labelText, int &value, int minimum, int maximum, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
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
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedWidth, theme_.widgetHeight);
    const bool changed = inputText(labelText, value,
                                   Rect(bounds.x - layout_.origin.x, bounds.y - layout_.origin.y,
                                        bounds.width, bounds.height));
    advanceLayout(bounds);
    return changed;
}

bool Context::inputTextMultiline(StringView labelText, String &value, float width, float height)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
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
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
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
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
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
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
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

bool Context::smallImageButton(StringView labelText, TextureId texture, float size,
                               const Vec2 &uvMin, const Vec2 &uvMax, const Color &tint)
{
    const float resolvedSize = size > 0.0f ? size : theme_.fontSize + theme_.padding * 1.4f;
    const Rect bounds(layout_.cursor.x, layout_.cursor.y, resolvedSize, resolvedSize);
    const bool clicked = smallImageButton(labelText, texture,
                                          Rect(bounds.x - layout_.origin.x,
                                               bounds.y - layout_.origin.y,
                                               bounds.width, bounds.height),
                                          uvMin, uvMax, tint);
    advanceLayout(bounds);
    return clicked;
}

void Context::progressBar(float value, float maximum, float width)
{
    const float resolvedWidth = width > 0.0f ? width : availableWidth();
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

void Context::separatorText(StringView text, float width)
{
    WindowState *window = currentWindow();
    DrawList *drawList = currentDrawList();
    if (!window || !drawList)
        return;

    const Rect clip = contentClip();
    const TextMetrics metrics = measureText(theme_.font, text, theme_.fontSize);
    const float available = window->bounds.x + window->bounds.width - theme_.windowPadding - layout_.cursor.x;
    const float resolvedWidth = width > 0.0f && width < available ? width : available;
    const float lineY = layout_.cursor.y + metrics.height * 0.5f;
    drawText(*drawList, theme_.font, text, layout_.cursor, theme_.fontSize, theme_.labelText, clip);
    const float lineX = layout_.cursor.x + metrics.width + theme_.windowPadding;
    if (resolvedWidth > lineX - layout_.cursor.x)
        drawList->addRectFilled(Rect(lineX, lineY, resolvedWidth - (lineX - layout_.cursor.x), 1.0f),
                                theme_.titleBarBackground, clip);
    advanceLayout(Rect(layout_.cursor.x, layout_.cursor.y, resolvedWidth, metrics.height));
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

float Context::availableWidth() const
{
    const WindowState *window = currentWindow();
    if (!window)
        return 0.0f;
    const float right = window->bounds.x + window->bounds.width - theme_.windowPadding;
    return right > layout_.cursor.x ? right - layout_.cursor.x : 0.0f;
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

void Context::drawDragDropPreview()
{
    if (!dragDrop_.active || dragDrop_.preview.empty())
        return;
    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const TextMetrics metrics = measureText(theme_.font, dragDrop_.preview, theme_.fontSize);
    const float padding = theme_.tooltipPadX;
    const Rect preview(pointer_.position.x + 14.0f, pointer_.position.y + 16.0f,
                       metrics.width + padding * 2.0f, metrics.height + theme_.tooltipPadY * 2.0f);
    dragDropDrawList_.addRectFilled(preview, theme_.tooltipBg, viewport);
    dragDropDrawList_.addRect(preview, theme_.dialogBtnPrimary, viewport);
    drawText(dragDropDrawList_, theme_.font, dragDrop_.preview,
             Vec2(preview.x + padding, preview.y + theme_.tooltipPadY),
             theme_.fontSize, theme_.tooltipText, viewport);
}

void Context::drawToasts()
{
    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const float padding = theme_.tooltipPadX;
    for (ct::Vector<ToastState>::size_type i = 0u; i < toasts_.size(); ++i)
    {
        const ToastState &toast = toasts_[i];
        if (toast.remaining <= 0.0f)
            continue;
        const TextMetrics metrics = measureText(theme_.font, toast.text, theme_.fontSize);
        const float width = metrics.width + padding * 2.0f + 4.0f;
        const float height = metrics.height + theme_.tooltipPadY * 2.0f;
        float x = padding;
        float y = padding;
        const uint8_t anchor = static_cast<uint8_t>(toast.position);
        const uint8_t horizontal = anchor % 3u;
        const uint8_t vertical = anchor / 3u;
        if (horizontal == 1u)
            x = (viewport.width - width) * 0.5f;
        else if (horizontal == 2u)
            x = viewport.width - width - padding;
        if (vertical == 1u)
            y = (viewport.height - height) * 0.5f;
        else if (vertical == 2u)
            y = viewport.height - height - padding;
        const Rect bounds(x, y, width, height);
        toastDrawList_.addRectFilled(bounds, theme_.tooltipBg, viewport);
        toastDrawList_.addRect(bounds, theme_.tooltipBorder, viewport);
        toastDrawList_.addRectFilled(Rect(bounds.x, bounds.y, 4.0f, bounds.height),
                                     theme_.dialogBtnPrimary, viewport);
        drawText(toastDrawList_, theme_.font, toast.text,
                 Vec2(bounds.x + padding + 4.0f, bounds.y + theme_.tooltipPadY),
                 theme_.fontSize, theme_.tooltipText, viewport);
    }
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
    return activeModal_ == InvalidWidgetId && currentWindow_ &&
           currentWindow_ == topWindowAt(pointer_.position);
}

uint32_t Context::buttonIndex(PointerButton button)
{
    const uint32_t index = static_cast<uint32_t>(button);
    return index < 3u ? index : 0u;
}

} // namespace ig
