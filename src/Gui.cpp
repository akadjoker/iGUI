#include "igui/Gui.hpp"

#include <ct/sort.hpp>

namespace ig
{

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
      windows_(), windowsById_(), windowOrder_(), idStack_(), frameDrawList_(), drawData_(),
      currentWindow_(), focusedWindow_(), draggingWindow_(), activeWidget_(InvalidWidgetId), hotWidget_(InvalidWidgetId),
      focusedWidget_(InvalidWidgetId), textInputWidget_(InvalidWidgetId), openCombo_(InvalidWidgetId),
      textCursor_(0), frameNumber_(0), nextZOrder_(1), windowDragOffset_(),
      wantsKeyboard_(false), wantsTextInput_(false), backspacePressed_(false),
      homePressed_(false), endPressed_(false), copyRequested_(false), pasteRequested_(false)
{
    events_.reserve(32);
    windows_.reserve(8);
    windowsById_.reserve(8);
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
    wantsKeyboard_ = false;
    wantsTextInput_ = false;
    backspacePressed_ = false;
    homePressed_ = false;
    endPressed_ = false;
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
            else if (event.key == KeyCode::Home)
                homePressed_ = true;
            else if (event.key == KeyCode::End)
                endPressed_ = true;
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
    const WidgetId closeId = combineIds(window.id, 0x434c4f5345ull);
    const WidgetId minimizeId = combineIds(window.id, 0x4d494e494d495a45ull);
    const WidgetId dragId = combineIds(window.id, 0x44524147ull);
    const bool closeHovered = itemHovered(closeButton, viewport, closeId);
    const bool minimizeHovered = itemHovered(minimizeButton, viewport, minimizeId);

    if (itemClicked(closeButton, viewport, closeId))
    {
        window.open = false;
        window.minimized = false;
        draggingWindow_ = WindowHandle();
        focusedWidget_ = InvalidWidgetId;
        textInputWidget_ = InvalidWidgetId;
        openCombo_ = InvalidWidgetId;
        return;
    }
    if (itemClicked(minimizeButton, viewport, minimizeId))
        window.minimized = !window.minimized;

    const uint32_t left = buttonIndex(PointerButton::Left);
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
