#include "igui/Gui.hpp"

#include <stdio.h>
#include <time.h>

#include <ct/sort.hpp>

namespace ig
{

namespace
{

bool sameAsciiIgnoreCase(StringView left, StringView right)
{
    if (left.size() != right.size())
        return false;
    for (StringView::size_type i = 0u; i < left.size(); ++i)
    {
        char a = left[i];
        char b = right[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        if (a != b)
            return false;
    }
    return true;
}

bool matchesFileDialogFilter(StringView name, StringView filter)
{
    if (filter.empty())
        return true;

    StringView::size_type extensionStart = name.size();
    for (StringView::size_type i = name.size(); i != 0u; --i)
    {
        if (name[i - 1u] == '.')
        {
            extensionStart = i - 1u;
            break;
        }
    }
    if (extensionStart == name.size())
        return false;
    const StringView extension(name.data() + extensionStart, name.size() - extensionStart);

    StringView::size_type begin = 0u;
    while (begin < filter.size())
    {
        StringView::size_type end = begin;
        while (end < filter.size() && filter[end] != ';')
            ++end;
        StringView token(filter.data() + begin, end - begin);
        while (!token.empty() && token[0] == '*')
            token = StringView(token.data() + 1u, token.size() - 1u);
        if (sameAsciiIgnoreCase(extension, token))
            return true;
        begin = end + 1u;
    }
    return false;
}

String fileDialogSize(uint64_t bytes)
{
    char text[32];
    if (bytes < 1024u)
        snprintf(text, sizeof(text), "%llu B", static_cast<unsigned long long>(bytes));
    else if (bytes < 1024u * 1024u)
        snprintf(text, sizeof(text), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    else if (bytes < 1024u * 1024u * 1024u)
        snprintf(text, sizeof(text), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    else
        snprintf(text, sizeof(text), "%.1f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    return String(text);
}

String fileDialogTime(uint64_t time)
{
    if (time == 0u)
        return String();
    char text[32];
    const time_t timestamp = static_cast<time_t>(time);
    const struct tm *local = localtime(&timestamp);
    if (!local || strftime(text, sizeof(text), "%Y-%m-%d %H:%M", local) == 0u)
        snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(time));
    return String(text);
}

void sortFileDialogEntries(ct::Vector<FileDialogEntry> &entries, FileDialogSortField field, bool ascending)
{
    ct::sort(entries.begin(), entries.end(), [field, ascending](const FileDialogEntry &left,
                                                                  const FileDialogEntry &right)
    {
        if (left.directory != right.directory)
            return left.directory;
        bool before = false;
        if (field == FileDialogSortField::Size)
            before = left.size < right.size;
        else if (field == FileDialogSortField::Modified)
            before = left.modifiedTime < right.modifiedTime;
        else
            before = left.name < right.name;
        if (!ascending)
        {
            if (field == FileDialogSortField::Size)
                before = left.size > right.size;
            else if (field == FileDialogSortField::Modified)
                before = left.modifiedTime > right.modifiedTime;
            else
                before = left.name > right.name;
        }
        return before;
    });
}

} // namespace

FileDialogResult Context::fileDialog(StringView idText, bool &open, FileDialogState &state,
                                     const FileDialogOptions &options, FileDialogProvider &provider)
{
    FileDialogResult result;
    const WidgetId id = hashText(idText);
    if (!open)
    {
        if (activeModal_ == id)
            activeModal_ = InvalidWidgetId;
        return result;
    }

    activeModal_ = id;
    if (!state.initialized)
    {
        state.path = options.initialPath.empty() ? provider.homeDirectory()
                                                  : String(options.initialPath.data(), options.initialPath.size());
        if (state.path.empty())
            state.path = provider.homeDirectory();
        state.initialized = true;
        state.selectedIndex = -3;
        state.selectedPath.clear();
        state.scrollOffset = 0.0f;
        provider.listDirectory(state.path, state.entries);
        sortFileDialogEntries(state.entries, state.sortField, state.sortAscending);
    }

    const Rect viewport(0.0f, 0.0f, frame_.displaySize.x, frame_.displaySize.y);
    const float preferredWidth = 820.0f;
    const float preferredHeight = 560.0f;
    const float minimumWidth = 460.0f;
    const float minimumHeight = 320.0f;
    const float maximumWidth = viewport.width - 24.0f;
    const float maximumHeight = viewport.height - 24.0f;
    if (state.size.x <= 0.0f || state.size.y <= 0.0f)
    {
        state.size.x = preferredWidth < maximumWidth ? preferredWidth : maximumWidth;
        state.size.y = preferredHeight < maximumHeight ? preferredHeight : maximumHeight;
        state.position = Vec2((viewport.width - state.size.x) * 0.5f,
                              (viewport.height - state.size.y) * 0.5f);
    }
    const float allowedMinimumWidth = minimumWidth < maximumWidth ? minimumWidth : maximumWidth;
    const float allowedMinimumHeight = minimumHeight < maximumHeight ? minimumHeight : maximumHeight;
    state.size.x = clamp(state.size.x, allowedMinimumWidth, maximumWidth);
    state.size.y = clamp(state.size.y, allowedMinimumHeight, maximumHeight);
    state.position.x = clamp(state.position.x, 12.0f, viewport.width - state.size.x - 12.0f);
    state.position.y = clamp(state.position.y, 12.0f, viewport.height - state.size.y - 12.0f);
    Rect dialog(state.position.x, state.position.y, state.size.x, state.size.y);
    const Rect resizeHitHandle(dialog.right() - 18.0f, dialog.bottom() - 18.0f, 18.0f, 18.0f);
    const WidgetId resizeId = combineIds(id, 0x524553495a45ull);
    const uint32_t left = buttonIndex(PointerButton::Left);
    if (pointer_.pressed[left] && activeWidget_ == InvalidWidgetId &&
        contains(resizeHitHandle, pointer_.pressedPosition[left]))
    {
        activeWidget_ = resizeId;
        state.resizing = true;
        state.resizePointerStart = pointer_.pressedPosition[left];
        state.resizeSizeStart = state.size;
    }
    if (state.resizing && pointer_.down[left])
    {
        state.size.x = clamp(state.resizeSizeStart.x + pointer_.position.x - state.resizePointerStart.x,
                             allowedMinimumWidth, maximumWidth);
        state.size.y = clamp(state.resizeSizeStart.y + pointer_.position.y - state.resizePointerStart.y,
                             allowedMinimumHeight, maximumHeight);
        dialog.width = state.size.x;
        dialog.height = state.size.y;
    }
    if (state.resizing && pointer_.released[left])
    {
        state.resizing = false;
        if (activeWidget_ == resizeId)
            activeWidget_ = InvalidWidgetId;
    }
    const Rect resizeHandle(dialog.right() - 18.0f, dialog.bottom() - 18.0f, 18.0f, 18.0f);
    const float padding = theme_.windowPadding;
    const Rect titleBar(dialog.x, dialog.y, dialog.width, theme_.widgetHeight + padding);
    const float toolY = titleBar.bottom() + padding;
    const float toolHeight = theme_.widgetHeight;
    const Rect backButton(dialog.x + padding, toolY, toolHeight, toolHeight);
    const Rect forwardButton(backButton.right() + theme_.itemSpacing, toolY, toolHeight, toolHeight);
    const Rect upButton(forwardButton.right() + theme_.itemSpacing, toolY, toolHeight, toolHeight);
    const Rect newFolderButton(upButton.right() + theme_.itemSpacing, toolY, toolHeight, toolHeight);
    const Rect pathBox(newFolderButton.right() + theme_.itemSpacing, toolY,
                       dialog.width - padding * 2.0f - toolHeight * 7.0f - theme_.itemSpacing * 7.0f,
                       toolHeight);
    const Rect iconsButton(pathBox.right() + theme_.itemSpacing, toolY, toolHeight, toolHeight);
    const Rect listButton(iconsButton.right() + theme_.itemSpacing, toolY, toolHeight, toolHeight);
    const Rect detailsButton(listButton.right() + theme_.itemSpacing, toolY, toolHeight, toolHeight);
    const Rect content(dialog.x + padding, toolY + toolHeight + padding,
                       dialog.width - padding * 2.0f,
                       dialog.height - titleBar.height - toolHeight - theme_.widgetHeight - padding * 4.0f);
    const bool imageDialog = options.mode == FileDialogMode::OpenImage;
    const float requestedPreviewWidth = content.width * 0.34f;
    const float previewWidth = imageDialog ? clamp(requestedPreviewWidth, 180.0f, 300.0f) : 0.0f;
    const Rect fileContent(content.x, content.y,
                           imageDialog ? content.width - previewWidth - theme_.itemSpacing : content.width,
                           content.height);
    const Rect previewPanel(imageDialog ? fileContent.right() + theme_.itemSpacing : 0.0f, content.y,
                            previewWidth, content.height);
    const Rect nameHeader(fileContent.x, fileContent.y, fileContent.width * 0.66f, toolHeight);
    const Rect sizeHeader(nameHeader.right(), fileContent.y, fileContent.width * 0.14f, toolHeight);
    const Rect modifiedHeader(sizeHeader.right(), fileContent.y, fileContent.right() - sizeHeader.right(), toolHeight);
    const float buttonsWidth = 88.0f;
    const Rect acceptButton(dialog.right() - padding - buttonsWidth, dialog.bottom() - padding - theme_.widgetHeight,
                            buttonsWidth, theme_.widgetHeight);
    const Rect cancelButton(acceptButton.x - theme_.itemSpacing - buttonsWidth, acceptButton.y,
                            buttonsWidth, theme_.widgetHeight);
    const Rect newFolderInput(dialog.x + padding, acceptButton.y,
                              cancelButton.x - theme_.itemSpacing - (dialog.x + padding), theme_.widgetHeight);
    const WidgetId backId = combineIds(id, 0x4241434bull);
    const WidgetId forwardId = combineIds(id, 0x464f5257415244ull);
    const WidgetId upId = combineIds(id, 0x5550ull);
    const WidgetId newFolderId = combineIds(id, 0x4e4557464f4c4445ull);
    const WidgetId iconsId = combineIds(id, 0x49434f4e53ull);
    const WidgetId listId = combineIds(id, 0x4c495354ull);
    const WidgetId detailsId = combineIds(id, 0x44455441494cull);
    const WidgetId sortNameId = combineIds(id, 0x534f52544e414d45ull);
    const WidgetId sortSizeId = combineIds(id, 0x534f525453495a45ull);
    const WidgetId sortModifiedId = combineIds(id, 0x534f52544d4f44ull);
    const WidgetId acceptId = combineIds(id, 0x4f4bull);
    const WidgetId cancelId = combineIds(id, 0x43414e43454cull);
    const WidgetId previewPanId = combineIds(id, 0x50524556494557ull);
    ct::Vector<String> breadcrumbPaths;
    ct::Vector<String> breadcrumbLabels;
    ct::Vector<Rect> breadcrumbButtons;
    ct::Vector<WidgetId> breadcrumbIds;
    float breadcrumbX = pathBox.x + 5.0f;
    String accumulatedPath;
    StringView::size_type segmentStart = 0u;
    if (!state.path.empty() && state.path[0] == '/')
    {
        accumulatedPath = "/";
        const TextMetrics rootMetrics = measureText(theme_.font, "/", theme_.fontSize * 0.85f);
        const float rootWidth = rootMetrics.width + 14.0f;
        if (breadcrumbX + rootWidth <= pathBox.right() - 4.0f)
        {
            breadcrumbPaths.push_back(accumulatedPath);
            breadcrumbLabels.push_back("/");
            breadcrumbButtons.push_back(Rect(breadcrumbX, pathBox.y + 3.0f, rootWidth, pathBox.height - 6.0f));
            breadcrumbIds.push_back(combineIds(id, 0x425245414400ull));
            breadcrumbX += rootWidth + 4.0f;
        }
        segmentStart = 1u;
    }
    for (StringView::size_type i = segmentStart; i <= state.path.size(); ++i)
    {
        if (i != state.path.size() && state.path[i] != '/')
            continue;
        if (i > segmentStart)
        {
            const String label(state.path.data() + segmentStart, i - segmentStart);
            if (accumulatedPath.empty())
                accumulatedPath = label;
            else if (accumulatedPath == "/")
                accumulatedPath += label;
            else
            {
                accumulatedPath += "/";
                accumulatedPath += label;
            }
            const TextMetrics labelMetrics = measureText(theme_.font, label, theme_.fontSize * 0.85f);
            const float labelWidth = labelMetrics.width + 18.0f;
            if (breadcrumbX + labelWidth <= pathBox.right() - 4.0f)
            {
                breadcrumbPaths.push_back(accumulatedPath);
                breadcrumbLabels.push_back(label);
                breadcrumbButtons.push_back(Rect(breadcrumbX, pathBox.y + 3.0f, labelWidth, pathBox.height - 6.0f));
                breadcrumbIds.push_back(combineIds(id, 0x425245414400ull + static_cast<WidgetId>(breadcrumbIds.size() + 1u)));
                breadcrumbX += labelWidth + 4.0f;
            }
        }
        segmentStart = i + 1u;
    }
    WidgetId clicked = InvalidWidgetId;
    String breadcrumbNavigation;

    if (pointer_.pressed[left] && activeWidget_ == InvalidWidgetId)
    {
        const Vec2 pressed = pointer_.pressedPosition[left];
        if (contains(backButton, pressed)) activeWidget_ = backId;
        else if (contains(forwardButton, pressed)) activeWidget_ = forwardId;
        else if (contains(upButton, pressed)) activeWidget_ = upId;
        else if (contains(newFolderButton, pressed)) activeWidget_ = newFolderId;
        else if (contains(iconsButton, pressed)) activeWidget_ = iconsId;
        else if (contains(listButton, pressed)) activeWidget_ = listId;
        else if (contains(detailsButton, pressed)) activeWidget_ = detailsId;
        else if (contains(acceptButton, pressed)) activeWidget_ = acceptId;
        else if (contains(cancelButton, pressed)) activeWidget_ = cancelId;
        else if (state.view == FileDialogView::Details && contains(nameHeader, pressed)) activeWidget_ = sortNameId;
        else if (state.view == FileDialogView::Details && contains(sizeHeader, pressed)) activeWidget_ = sortSizeId;
        else if (state.view == FileDialogView::Details && contains(modifiedHeader, pressed)) activeWidget_ = sortModifiedId;
        else if (imageDialog && state.selectedIndex >= 0 &&
                 !state.entries[static_cast<ct::Vector<FileDialogEntry>::size_type>(state.selectedIndex)].directory &&
                 contains(previewPanel, pressed))
        {
            activeWidget_ = previewPanId;
            state.previewPanning = true;
            state.previewPointerStart = pressed;
            state.previewPanStart = state.previewPan;
        }
        else
        {
            for (ct::Vector<Rect>::size_type i = 0u; i < breadcrumbButtons.size(); ++i)
            {
                if (contains(breadcrumbButtons[i], pressed))
                {
                    activeWidget_ = breadcrumbIds[i];
                    break;
                }
            }
        }
    }
    if (pointer_.released[left] && activeWidget_ != InvalidWidgetId)
    {
        const Vec2 released = pointer_.releasedPosition[left];
        if (activeWidget_ == previewPanId)
        {
            state.previewPanning = false;
        }
        else if ((activeWidget_ == backId && contains(backButton, released)) ||
            (activeWidget_ == forwardId && contains(forwardButton, released)) ||
            (activeWidget_ == upId && contains(upButton, released)) ||
            (activeWidget_ == newFolderId && contains(newFolderButton, released)) ||
            (activeWidget_ == iconsId && contains(iconsButton, released)) ||
            (activeWidget_ == listId && contains(listButton, released)) ||
            (activeWidget_ == detailsId && contains(detailsButton, released)) ||
            (activeWidget_ == acceptId && contains(acceptButton, released)) ||
            (activeWidget_ == cancelId && contains(cancelButton, released)) ||
            (activeWidget_ == sortNameId && contains(nameHeader, released)) ||
            (activeWidget_ == sortSizeId && contains(sizeHeader, released)) ||
            (activeWidget_ == sortModifiedId && contains(modifiedHeader, released)))
            clicked = activeWidget_;
        else
        {
            for (ct::Vector<Rect>::size_type i = 0u; i < breadcrumbButtons.size(); ++i)
            {
                if (activeWidget_ == breadcrumbIds[i] && contains(breadcrumbButtons[i], released))
                {
                    breadcrumbNavigation = breadcrumbPaths[i];
                    break;
                }
            }
        }
        activeWidget_ = InvalidWidgetId;
    }
    if (state.previewPanning && pointer_.down[left])
    {
        state.previewPan = Vec2(state.previewPanStart.x + pointer_.position.x - state.previewPointerStart.x,
                                state.previewPanStart.y + pointer_.position.y - state.previewPointerStart.y);
    }

    bool refresh = false;
    String createdDirectory;
    bool handledCreateEnter = false;
    const auto navigateTo = [&state, &refresh](const String &path, bool keepHistory)
    {
        if (path.empty() || path == state.path)
            return;
        if (keepHistory)
        {
            state.backHistory.push_back(state.path);
            state.forwardHistory.clear();
        }
        state.path = path;
        refresh = true;
    };
    if (state.creatingFolder)
    {
        wantsKeyboard_ = true;
        wantsTextInput_ = true;
        if (state.newFolderCursor > state.newFolderName.size())
            state.newFolderCursor = state.newFolderName.size();
        for (ct::Vector<Event>::size_type i = 0u; i < textEvents_.size(); ++i)
        {
            const Event &event = textEvents_[i];
            state.newFolderName.insert(state.newFolderCursor, event.text, event.textLength);
            state.newFolderCursor += event.textLength;
            state.newFolderError.clear();
        }
        if (backspacePressed_ && state.newFolderCursor != 0u)
        {
            --state.newFolderCursor;
            state.newFolderName.erase(state.newFolderCursor, 1u);
            state.newFolderError.clear();
        }
        if (escapePressed_)
        {
            state.creatingFolder = false;
            state.newFolderName.clear();
            state.newFolderError.clear();
            escapePressed_ = false;
        }
    }
    if (clicked == backId && !state.backHistory.empty())
    {
        const String target = state.backHistory.back();
        state.backHistory.pop_back();
        state.forwardHistory.push_back(state.path);
        state.path = target;
        refresh = true;
    }
    else if (clicked == forwardId && !state.forwardHistory.empty())
    {
        const String target = state.forwardHistory.back();
        state.forwardHistory.pop_back();
        state.backHistory.push_back(state.path);
        state.path = target;
        refresh = true;
    }
    else if (clicked == upId)
    {
        const String parent = provider.parentDirectory(state.path);
        navigateTo(parent, true);
    }
    else if (clicked == iconsId)
        state.view = FileDialogView::Icons;
    else if (clicked == listId)
        state.view = FileDialogView::List;
    else if (clicked == detailsId)
        state.view = FileDialogView::Details;
    else if (clicked == sortNameId || clicked == sortSizeId || clicked == sortModifiedId)
    {
        const FileDialogSortField requested = clicked == sortSizeId ? FileDialogSortField::Size
                                            : (clicked == sortModifiedId ? FileDialogSortField::Modified
                                                                         : FileDialogSortField::Name);
        state.sortAscending = state.sortField == requested ? !state.sortAscending : true;
        state.sortField = requested;
        sortFileDialogEntries(state.entries, state.sortField, state.sortAscending);
        state.selectedIndex = -3;
        state.selectedPath.clear();
    }
    else if (clicked == newFolderId && !state.creatingFolder)
    {
        state.creatingFolder = true;
        state.newFolderName.clear();
        state.newFolderError.clear();
        state.newFolderCursor = 0u;
    }
    else if (!breadcrumbNavigation.empty() && breadcrumbNavigation != state.path)
        navigateTo(breadcrumbNavigation, true);
    if (state.creatingFolder && (enterPressed_ || clicked == newFolderId))
    {
        handledCreateEnter = enterPressed_;
        if (state.newFolderName.empty())
            state.newFolderError = "Type a folder name";
        else
        {
            bool invalidName = false;
            for (String::size_type i = 0u; i < state.newFolderName.size(); ++i)
            {
                if (state.newFolderName[i] == '/' || state.newFolderName[i] == '\\')
                    invalidName = true;
            }
            if (invalidName)
                state.newFolderError = "Folder name cannot contain a slash";
            else
            {
                createdDirectory = provider.createDirectory(state.path, state.newFolderName);
                if (createdDirectory.empty())
                    state.newFolderError = "Could not create folder";
                else
                {
                    state.creatingFolder = false;
                    state.newFolderName.clear();
                    state.newFolderError.clear();
                    refresh = true;
                }
            }
        }
    }

    if (imageDialog && contains(previewPanel, pointer_.position) && pointer_.wheelY != 0.0f)
    {
        const float zoomStep = pointer_.wheelY > 0.0f ? 1.15f : (1.0f / 1.15f);
        state.previewZoom = clamp(state.previewZoom * zoomStep, 0.1f, 8.0f);
    }
    else if (contains(fileContent, pointer_.position) && pointer_.wheelY != 0.0f)
    {
        state.scrollOffset -= pointer_.wheelY * theme_.widgetHeight * 2.0f;
        if (state.scrollOffset < 0.0f)
            state.scrollOffset = 0.0f;
    }

    const float rowHeight = theme_.widgetHeight;
    const float headerHeight = state.view == FileDialogView::Details ? rowHeight : 0.0f;
    const Rect entriesArea(fileContent.x, fileContent.y + headerHeight, fileContent.width, fileContent.height - headerHeight);
    ct::Vector<int> visibleEntries;
    visibleEntries.reserve(state.entries.size() + 2u);
    // Match filesystem navigation semantics in every view, not only a toolbar.
    visibleEntries.push_back(-2); // .  current folder
    visibleEntries.push_back(-1); // .. parent folder
    for (ct::Vector<FileDialogEntry>::size_type i = 0u; i < state.entries.size(); ++i)
    {
        const FileDialogEntry &entry = state.entries[i];
        if ((!options.showHidden && entry.hidden) ||
            (!entry.directory && !matchesFileDialogFilter(entry.name, options.filter)))
            continue;
        visibleEntries.push_back(static_cast<int>(i));
    }
    int hitEntry = -3;
    bool activateSelected = false;
    if (pointer_.released[left] && contains(entriesArea, pointer_.releasedPosition[left]))
    {
        const Vec2 released = pointer_.releasedPosition[left];
        if (state.view == FileDialogView::Icons)
        {
            const float cell = 92.0f;
            int columns = static_cast<int>(entriesArea.width / cell);
            if (columns < 1) columns = 1;
            const int column = static_cast<int>((released.x - entriesArea.x) / cell);
            const int row = static_cast<int>((released.y - entriesArea.y + state.scrollOffset) / cell);
            hitEntry = row * columns + column;
        }
        else
            hitEntry = static_cast<int>((released.y - entriesArea.y + state.scrollOffset) / rowHeight);
        if (hitEntry < 0 || hitEntry >= static_cast<int>(visibleEntries.size()))
            hitEntry = -1;
        else
            hitEntry = visibleEntries[static_cast<ct::Vector<int>::size_type>(hitEntry)];
    }
    if (hitEntry >= 0)
    {
        FileDialogEntry &entry = state.entries[static_cast<ct::Vector<FileDialogEntry>::size_type>(hitEntry)];
        const bool doubleClick = state.lastClickedIndex == hitEntry && frameNumber_ - state.lastClickedFrame <= 22u;
        if (state.previewPath != entry.path)
        {
            state.previewPath = entry.path;
            state.previewZoom = 1.0f;
            state.previewPan = Vec2();
        }
        state.selectedIndex = hitEntry;
        state.selectedPath = entry.path;
        state.lastClickedIndex = hitEntry;
        state.lastClickedFrame = frameNumber_;
        if (entry.directory && doubleClick)
        {
            navigateTo(entry.path, true);
        }
        else if (!entry.directory && doubleClick && options.mode != FileDialogMode::ChooseFolder)
        {
            activateSelected = true;
        }
    }
    else if (hitEntry == -2 || hitEntry == -1)
    {
        const bool doubleClick = state.lastClickedIndex == hitEntry && frameNumber_ - state.lastClickedFrame <= 22u;
        state.selectedIndex = hitEntry;
        state.selectedPath = hitEntry == -2 ? state.path : provider.parentDirectory(state.path);
        state.previewPath.clear();
        state.previewZoom = 1.0f;
        state.previewPan = Vec2();
        state.lastClickedIndex = hitEntry;
        state.lastClickedFrame = frameNumber_;
        if (hitEntry == -1 && doubleClick && !state.selectedPath.empty() && state.selectedPath != state.path)
        {
            navigateTo(state.selectedPath, true);
        }
    }

    if (refresh)
    {
        state.entries.clear();
        provider.listDirectory(state.path, state.entries);
        sortFileDialogEntries(state.entries, state.sortField, state.sortAscending);
        state.selectedIndex = -3;
        state.selectedPath.clear();
        state.previewPath.clear();
        state.previewZoom = 1.0f;
        state.previewPan = Vec2();
        state.scrollOffset = 0.0f;
        state.lastClickedIndex = -3;
        visibleEntries.clear();
        visibleEntries.reserve(state.entries.size() + 2u);
        visibleEntries.push_back(-2);
        visibleEntries.push_back(-1);
        for (ct::Vector<FileDialogEntry>::size_type i = 0u; i < state.entries.size(); ++i)
        {
            const FileDialogEntry &entry = state.entries[i];
            if ((!options.showHidden && entry.hidden) ||
                (!entry.directory && !matchesFileDialogFilter(entry.name, options.filter)))
                continue;
            visibleEntries.push_back(static_cast<int>(i));
            if (!createdDirectory.empty() && entry.path == createdDirectory)
            {
                state.selectedIndex = static_cast<int>(i);
                state.selectedPath = entry.path;
            }
        }
    }

    bool accepted = !handledCreateEnter && !state.creatingFolder &&
                    (clicked == acceptId || enterPressed_ || activateSelected);
    const bool cancelled = clicked == cancelId || escapePressed_;
    String chosen = state.selectedPath;
    if (options.mode == FileDialogMode::ChooseFolder)
    {
        if (state.selectedIndex == -2)
            chosen = state.path;
        else if (state.selectedIndex == -1)
            chosen = provider.parentDirectory(state.path);
        else if (state.selectedIndex >= 0 && !state.entries[static_cast<ct::Vector<FileDialogEntry>::size_type>(state.selectedIndex)].directory)
            chosen.clear();
        if (chosen.empty())
            chosen = state.path;
    }
    else if (state.selectedIndex < 0 ||
             state.entries[static_cast<ct::Vector<FileDialogEntry>::size_type>(state.selectedIndex)].directory)
    {
        accepted = false;
    }
    if (accepted)
    {
        open = false;
        activeModal_ = InvalidWidgetId;
        result.kind = FileDialogResultKind::Accepted;
        result.path = chosen;
    }
    else if (cancelled)
    {
        open = false;
        activeModal_ = InvalidWidgetId;
        result.kind = FileDialogResultKind::Cancelled;
    }

    const Color iconColor = theme_.buttonText;
    const Color selection = theme_.selectableSelected;
    const Color hover = theme_.buttonHovered;
    const TextMetrics titleMetrics = measureText(theme_.font, options.title, theme_.fontSize);
    modalDrawList_.addRectFilled(viewport, theme_.dialogScrim, viewport);
    modalDrawList_.addRectFilled(dialog, theme_.dialogBg, viewport);
    modalDrawList_.addRect(dialog, theme_.dialogBorder, viewport);
    modalDrawList_.addRectFilled(titleBar, theme_.floatTitleBg, viewport);
    drawText(modalDrawList_, theme_.font, options.title,
             Vec2(titleBar.x + padding, titleBar.y + (titleBar.height - titleMetrics.height) * 0.5f),
             theme_.fontSize, theme_.dialogTitleText, viewport);

    const bool hoverBack = contains(backButton, pointer_.position) && !state.backHistory.empty();
    const bool hoverForward = contains(forwardButton, pointer_.position) && !state.forwardHistory.empty();
    const bool hoverUp = contains(upButton, pointer_.position);
    const bool hoverNewFolder = contains(newFolderButton, pointer_.position);
    const bool hoverIcons = contains(iconsButton, pointer_.position);
    const bool hoverList = contains(listButton, pointer_.position);
    const bool hoverDetails = contains(detailsButton, pointer_.position);
    const Color disabledIcon = theme_.textDisabled;
    const Color backColor = state.backHistory.empty() ? disabledIcon : iconColor;
    const Color forwardColor = state.forwardHistory.empty() ? disabledIcon : iconColor;
    modalDrawList_.addRectFilled(backButton, hoverBack ? hover : theme_.dialogBtnBg, viewport);
    modalDrawList_.addRect(backButton, theme_.dialogBorder, viewport);
    modalDrawList_.addLine(Vec2(backButton.x + 17.0f, backButton.y + 7.0f), Vec2(backButton.x + 10.0f, backButton.y + 14.0f), backColor, viewport, 1.5f);
    modalDrawList_.addLine(Vec2(backButton.x + 10.0f, backButton.y + 14.0f), Vec2(backButton.x + 17.0f, backButton.y + 21.0f), backColor, viewport, 1.5f);
    modalDrawList_.addLine(Vec2(backButton.x + 10.0f, backButton.y + 14.0f), Vec2(backButton.x + 22.0f, backButton.y + 14.0f), backColor, viewport, 1.5f);
    modalDrawList_.addRectFilled(forwardButton, hoverForward ? hover : theme_.dialogBtnBg, viewport);
    modalDrawList_.addRect(forwardButton, theme_.dialogBorder, viewport);
    modalDrawList_.addLine(Vec2(forwardButton.x + 11.0f, forwardButton.y + 7.0f), Vec2(forwardButton.x + 18.0f, forwardButton.y + 14.0f), forwardColor, viewport, 1.5f);
    modalDrawList_.addLine(Vec2(forwardButton.x + 18.0f, forwardButton.y + 14.0f), Vec2(forwardButton.x + 11.0f, forwardButton.y + 21.0f), forwardColor, viewport, 1.5f);
    modalDrawList_.addLine(Vec2(forwardButton.x + 6.0f, forwardButton.y + 14.0f), Vec2(forwardButton.x + 18.0f, forwardButton.y + 14.0f), forwardColor, viewport, 1.5f);
    modalDrawList_.addRectFilled(upButton, hoverUp ? hover : theme_.dialogBtnBg, viewport);
    modalDrawList_.addRect(upButton, theme_.dialogBorder, viewport);
    modalDrawList_.addLine(Vec2(upButton.x + 9.0f, upButton.y + 15.0f), Vec2(upButton.x + 14.0f, upButton.y + 10.0f), iconColor, viewport, 1.5f);
    modalDrawList_.addLine(Vec2(upButton.x + 14.0f, upButton.y + 10.0f), Vec2(upButton.x + 19.0f, upButton.y + 15.0f), iconColor, viewport, 1.5f);
    modalDrawList_.addLine(Vec2(upButton.x + 14.0f, upButton.y + 10.0f), Vec2(upButton.x + 14.0f, upButton.y + 21.0f), iconColor, viewport, 1.5f);
    modalDrawList_.addRectFilled(newFolderButton, hoverNewFolder ? hover : theme_.dialogBtnBg, viewport);
    modalDrawList_.addRect(newFolderButton, theme_.dialogBorder, viewport);
    const Rect folderTab(newFolderButton.x + 6.0f, newFolderButton.y + 8.0f,
                         newFolderButton.width * 0.37f, 5.0f);
    const Rect folderGlyph(newFolderButton.x + 5.0f, newFolderButton.y + 12.0f,
                           newFolderButton.width - 10.0f, newFolderButton.height * 0.42f);
    modalDrawList_.addRectFilled(folderTab, iconColor, viewport);
    modalDrawList_.addRectFilled(folderGlyph, iconColor, viewport);
    const float plusX = newFolderButton.right() - 8.0f;
    const float plusY = newFolderButton.y + 8.0f;
    modalDrawList_.addLine(Vec2(plusX - 3.0f, plusY), Vec2(plusX + 3.0f, plusY), iconColor, viewport, 1.5f);
    modalDrawList_.addLine(Vec2(plusX, plusY - 3.0f), Vec2(plusX, plusY + 3.0f), iconColor, viewport, 1.5f);
    modalDrawList_.addRectFilled(pathBox, theme_.inputBg, viewport);
    modalDrawList_.addRect(pathBox, theme_.inputBorderHover, viewport);
    for (ct::Vector<Rect>::size_type i = 0u; i < breadcrumbButtons.size(); ++i)
    {
        const bool hoveredBreadcrumb = contains(breadcrumbButtons[i], pointer_.position);
        const bool currentBreadcrumb = i + 1u == breadcrumbButtons.size();
        const Color background = currentBreadcrumb ? theme_.buttonBackground
                               : (hoveredBreadcrumb ? theme_.buttonHovered : theme_.inputBg);
        modalDrawList_.addRectFilled(breadcrumbButtons[i], background, pathBox);
        const TextMetrics metrics = measureText(theme_.font, breadcrumbLabels[i], theme_.fontSize * 0.85f);
        drawText(modalDrawList_, theme_.font, breadcrumbLabels[i],
                 Vec2(breadcrumbButtons[i].x + 7.0f,
                      breadcrumbButtons[i].y + (breadcrumbButtons[i].height - metrics.height) * 0.5f),
                 theme_.fontSize * 0.85f, theme_.dialogText, pathBox);
        if (i + 1u < breadcrumbButtons.size())
        {
            const float arrowX = breadcrumbButtons[i].right() + 1.0f;
            const float arrowY = pathBox.y + pathBox.height * 0.5f;
            modalDrawList_.addLine(Vec2(arrowX, arrowY - 3.0f), Vec2(arrowX + 3.0f, arrowY),
                                   theme_.dialogText, pathBox, 1.0f);
            modalDrawList_.addLine(Vec2(arrowX + 3.0f, arrowY), Vec2(arrowX, arrowY + 3.0f),
                                   theme_.dialogText, pathBox, 1.0f);
        }
    }

    const Rect viewButtons[3] = {iconsButton, listButton, detailsButton};
    const FileDialogView viewModes[3] = {FileDialogView::Icons, FileDialogView::List, FileDialogView::Details};
    const bool viewHover[3] = {hoverIcons, hoverList, hoverDetails};
    for (uint32_t i = 0u; i < 3u; ++i)
    {
        const Color background = state.view == viewModes[i] ? selection : (viewHover[i] ? hover : theme_.dialogBtnBg);
        modalDrawList_.addRectFilled(viewButtons[i], background, viewport);
        modalDrawList_.addRect(viewButtons[i], theme_.dialogBorder, viewport);
        const float x = viewButtons[i].x + 7.0f;
        const float y = viewButtons[i].y + 7.0f;
        if (viewModes[i] == FileDialogView::Icons)
        {
            const float cell = 5.0f;
            modalDrawList_.addRectFilled(Rect(x, y, cell, cell), iconColor, viewport);
            modalDrawList_.addRectFilled(Rect(x + 8.0f, y, cell, cell), iconColor, viewport);
            modalDrawList_.addRectFilled(Rect(x, y + 8.0f, cell, cell), iconColor, viewport);
            modalDrawList_.addRectFilled(Rect(x + 8.0f, y + 8.0f, cell, cell), iconColor, viewport);
        }
        else if (viewModes[i] == FileDialogView::List)
        {
            for (uint32_t row = 0u; row < 3u; ++row)
            {
                const float rowY = y + static_cast<float>(row) * 5.0f;
                modalDrawList_.addRectFilled(Rect(x, rowY, 2.5f, 2.5f), iconColor, viewport);
                modalDrawList_.addLine(Vec2(x + 5.5f, rowY + 1.25f), Vec2(x + 15.0f, rowY + 1.25f),
                                       iconColor, viewport, 1.5f);
            }
        }
        else
        {
            modalDrawList_.addRect(Rect(x, y, 15.0f, 14.0f), iconColor, viewport);
            modalDrawList_.addLine(Vec2(x + 5.0f, y), Vec2(x + 5.0f, y + 14.0f), iconColor, viewport, 1.0f);
            modalDrawList_.addLine(Vec2(x + 10.0f, y), Vec2(x + 10.0f, y + 14.0f), iconColor, viewport, 1.0f);
            modalDrawList_.addLine(Vec2(x, y + 4.5f), Vec2(x + 15.0f, y + 4.5f), iconColor, viewport, 1.0f);
            modalDrawList_.addLine(Vec2(x, y + 9.5f), Vec2(x + 15.0f, y + 9.5f), iconColor, viewport, 1.0f);
        }
    }

    modalDrawList_.addRectFilled(fileContent, theme_.inputBg, viewport);
    modalDrawList_.addRect(fileContent, theme_.dialogBorder, viewport);
    if (state.view == FileDialogView::Details)
    {
        modalDrawList_.addRectFilled(nameHeader, contains(nameHeader, pointer_.position) ? hover : theme_.floatTitleBg, viewport);
        modalDrawList_.addRectFilled(sizeHeader, contains(sizeHeader, pointer_.position) ? hover : theme_.floatTitleBg, viewport);
        modalDrawList_.addRectFilled(modifiedHeader, contains(modifiedHeader, pointer_.position) ? hover : theme_.floatTitleBg, viewport);
        modalDrawList_.addLine(Vec2(nameHeader.right(), nameHeader.y + 4.0f), Vec2(nameHeader.right(), nameHeader.bottom() - 4.0f), theme_.dialogBorder, viewport);
        modalDrawList_.addLine(Vec2(sizeHeader.right(), sizeHeader.y + 4.0f), Vec2(sizeHeader.right(), sizeHeader.bottom() - 4.0f), theme_.dialogBorder, viewport);
        drawText(modalDrawList_, theme_.font, "Name", Vec2(nameHeader.x + 8.0f, nameHeader.y + 4.0f), theme_.fontSize * 0.85f, iconColor, viewport);
        drawText(modalDrawList_, theme_.font, "Size", Vec2(sizeHeader.x + 8.0f, sizeHeader.y + 4.0f), theme_.fontSize * 0.85f, iconColor, viewport);
        drawText(modalDrawList_, theme_.font, "Modified", Vec2(modifiedHeader.x + 8.0f, modifiedHeader.y + 4.0f), theme_.fontSize * 0.85f, iconColor, viewport);
        const Rect activeHeader = state.sortField == FileDialogSortField::Size ? sizeHeader
                                : (state.sortField == FileDialogSortField::Modified ? modifiedHeader : nameHeader);
        const float arrowX = activeHeader.right() - 10.0f;
        const float arrowY = activeHeader.y + activeHeader.height * 0.5f;
        if (state.sortAscending)
        {
            modalDrawList_.addLine(Vec2(arrowX - 3.0f, arrowY + 2.0f), Vec2(arrowX, arrowY - 2.0f), iconColor, viewport, 1.25f);
            modalDrawList_.addLine(Vec2(arrowX, arrowY - 2.0f), Vec2(arrowX + 3.0f, arrowY + 2.0f), iconColor, viewport, 1.25f);
        }
        else
        {
            modalDrawList_.addLine(Vec2(arrowX - 3.0f, arrowY - 2.0f), Vec2(arrowX, arrowY + 2.0f), iconColor, viewport, 1.25f);
            modalDrawList_.addLine(Vec2(arrowX, arrowY + 2.0f), Vec2(arrowX + 3.0f, arrowY - 2.0f), iconColor, viewport, 1.25f);
        }
    }

    const Rect entriesClip = entriesArea;
    const float iconCell = 92.0f;
    int iconColumns = static_cast<int>(entriesArea.width / iconCell);
    if (iconColumns < 1) iconColumns = 1;
    for (ct::Vector<int>::size_type visibleIndex = 0u; visibleIndex < visibleEntries.size(); ++visibleIndex)
    {
        const int entryIndex = visibleEntries[visibleIndex];
        const bool navigationEntry = entryIndex < 0;
        const FileDialogEntry *entry = navigationEntry ? nullptr
            : &state.entries[static_cast<ct::Vector<FileDialogEntry>::size_type>(entryIndex)];
        const StringView entryName = entryIndex == -2 ? StringView(".")
                                 : (entryIndex == -1 ? StringView("..") : StringView(entry->name));
        const bool entryDirectory = navigationEntry || entry->directory;
        Rect item;
        if (state.view == FileDialogView::Icons)
        {
            const int column = static_cast<int>(visibleIndex) % iconColumns;
            const int row = static_cast<int>(visibleIndex) / iconColumns;
            item = Rect(entriesArea.x + static_cast<float>(column) * iconCell,
                        entriesArea.y + static_cast<float>(row) * iconCell - state.scrollOffset,
                        iconCell, iconCell);
        }
        else
            item = Rect(entriesArea.x, entriesArea.y + static_cast<float>(visibleIndex) * rowHeight - state.scrollOffset,
                        entriesArea.width, rowHeight);
        if (item.bottom() <= entriesArea.y || item.y >= entriesArea.bottom())
            continue;
        const bool selected = entryIndex == state.selectedIndex;
        const bool hovered = contains(item, pointer_.position);
        if (selected)
            modalDrawList_.addRectFilled(item, selection, entriesClip);
        else if (hovered)
            modalDrawList_.addRectFilled(item, hover, entriesClip);

        const Color entryColor = entryDirectory ? theme_.dialogBtnPrimary : theme_.dialogText;
        if (state.view == FileDialogView::Icons)
        {
            const float iconSize = 28.0f;
            const Rect glyph(item.x + (item.width - iconSize) * 0.5f, item.y + 10.0f, iconSize, iconSize * 0.72f);
            modalDrawList_.addRectFilled(glyph, entryColor, entriesClip);
            const StringView name(entryName.data(), entryName.size() > 13u ? 13u : entryName.size());
            const TextMetrics metrics = measureText(theme_.font, name, theme_.fontSize * 0.75f);
            drawText(modalDrawList_, theme_.font, name,
                     Vec2(item.x + (item.width - metrics.width) * 0.5f, item.y + 52.0f),
                     theme_.fontSize * 0.75f, theme_.dialogText, entriesClip);
        }
        else
        {
            const Rect glyph(item.x + 7.0f, item.y + (item.height - 12.0f) * 0.5f,
                             entryDirectory ? 15.0f : 10.0f, 12.0f);
            modalDrawList_.addRectFilled(glyph, entryColor, entriesClip);
            drawText(modalDrawList_, theme_.font, entryName,
                     Vec2(item.x + 29.0f, item.y + (item.height - theme_.fontSize) * 0.5f),
                     theme_.fontSize, theme_.dialogText, entriesClip);
            if (state.view == FileDialogView::Details)
            {
                const String size = entryDirectory ? String("Folder") : fileDialogSize(entry->size);
                const String modified = navigationEntry ? String() : fileDialogTime(entry->modifiedTime);
                drawText(modalDrawList_, theme_.font, size,
                         Vec2(item.x + item.width * 0.66f, item.y + (item.height - theme_.fontSize) * 0.5f),
                         theme_.fontSize * 0.85f, theme_.dialogText, entriesClip);
                drawText(modalDrawList_, theme_.font, modified,
                         Vec2(item.x + item.width * 0.80f, item.y + (item.height - theme_.fontSize) * 0.5f),
                         theme_.fontSize * 0.85f, theme_.dialogText, entriesClip);
            }
        }
    }

    if (imageDialog)
    {
        modalDrawList_.addRectFilled(previewPanel, theme_.panelColor, viewport);
        modalDrawList_.addRect(previewPanel, theme_.dialogBorder, viewport);
        const StringView previewTitle("Preview");
        const TextMetrics previewTitleMetrics = measureText(theme_.font, previewTitle, theme_.fontSize * 0.85f);
        drawText(modalDrawList_, theme_.font, previewTitle,
                 Vec2(previewPanel.x + 8.0f, previewPanel.y + 7.0f),
                 theme_.fontSize * 0.85f, theme_.dialogText, previewPanel);
        const Rect imageArea(previewPanel.x + 8.0f, previewPanel.y + previewTitleMetrics.height + 16.0f,
                             previewPanel.width - 16.0f,
                             previewPanel.height - previewTitleMetrics.height - 48.0f);
        modalDrawList_.addRectFilled(imageArea, theme_.inputBg, previewPanel);
        if (state.selectedIndex >= 0 &&
            !state.entries[static_cast<ct::Vector<FileDialogEntry>::size_type>(state.selectedIndex)].directory)
        {
            const FileDialogPreview preview = provider.imagePreview(state.selectedPath);
            if (preview.texture.value != 0u && preview.width > 0.0f && preview.height > 0.0f)
            {
                float scale = imageArea.width / preview.width;
                const float heightScale = imageArea.height / preview.height;
                if (heightScale < scale) scale = heightScale;
                scale *= state.previewZoom;
                const Rect previewBox(imageArea.x + (imageArea.width - preview.width * scale) * 0.5f + state.previewPan.x,
                                      imageArea.y + (imageArea.height - preview.height * scale) * 0.5f + state.previewPan.y,
                                      preview.width * scale, preview.height * scale);
                modalDrawList_.addImage(preview.texture, previewBox, Vec2(), Vec2(1.0f, 1.0f), Color(), imageArea);
                char zoomText[24];
                snprintf(zoomText, sizeof(zoomText), "%.0f%%", state.previewZoom * 100.0f);
                const TextMetrics zoomMetrics = measureText(theme_.font, zoomText, theme_.fontSize * 0.8f);
                drawText(modalDrawList_, theme_.font, zoomText,
                         Vec2(previewPanel.right() - zoomMetrics.width - 8.0f,
                              previewPanel.bottom() - zoomMetrics.height - 7.0f),
                         theme_.fontSize * 0.8f, theme_.textDisabled, previewPanel);
            }
            else
            {
                const StringView unavailable("Preview unavailable");
                const TextMetrics metrics = measureText(theme_.font, unavailable, theme_.fontSize * 0.85f);
                drawText(modalDrawList_, theme_.font, unavailable,
                         Vec2(imageArea.x + (imageArea.width - metrics.width) * 0.5f,
                              imageArea.y + (imageArea.height - metrics.height) * 0.5f),
                         theme_.fontSize * 0.85f, theme_.textDisabled, imageArea);
            }
        }
        else
        {
            const StringView hint("Select an image");
            const TextMetrics metrics = measureText(theme_.font, hint, theme_.fontSize * 0.85f);
            drawText(modalDrawList_, theme_.font, hint,
                     Vec2(imageArea.x + (imageArea.width - metrics.width) * 0.5f,
                          imageArea.y + (imageArea.height - metrics.height) * 0.5f),
                     theme_.fontSize * 0.85f, theme_.textDisabled, imageArea);
        }
    }

    const StringView acceptText = options.mode == FileDialogMode::SaveFile ? StringView("Save")
                              : (options.mode == FileDialogMode::ChooseFolder ? StringView("Choose") : StringView("Open"));
    const bool canAccept = options.mode == FileDialogMode::ChooseFolder || state.selectedIndex >= 0;
    if (state.creatingFolder)
    {
        modalDrawList_.addRectFilled(newFolderInput, theme_.inputBg, viewport);
        modalDrawList_.addRect(newFolderInput, theme_.inputBorderHover, viewport);
        const StringView prompt("New folder");
        const TextMetrics promptMetrics = measureText(theme_.font, prompt, theme_.fontSize * 0.85f);
        drawText(modalDrawList_, theme_.font, prompt,
                 Vec2(newFolderInput.x + 8.0f, newFolderInput.y + (newFolderInput.height - promptMetrics.height) * 0.5f),
                 theme_.fontSize * 0.85f, theme_.dialogText, newFolderInput);
        const float textX = newFolderInput.x + promptMetrics.width + 18.0f;
        const StringView value(state.newFolderName.data(), state.newFolderName.size());
        const TextMetrics valueMetrics = measureText(theme_.font, value, theme_.fontSize);
        if (value.empty())
        {
            const StringView placeholder("Type a name and press Enter");
            drawText(modalDrawList_, theme_.font, placeholder,
                     Vec2(textX, newFolderInput.y + (newFolderInput.height - valueMetrics.height) * 0.5f),
                     theme_.fontSize, theme_.textDisabled, newFolderInput);
        }
        else
        {
            drawText(modalDrawList_, theme_.font, value,
                     Vec2(textX, newFolderInput.y + (newFolderInput.height - valueMetrics.height) * 0.5f),
                     theme_.fontSize, theme_.dialogText, newFolderInput);
        }
        const StringView prefix(state.newFolderName.data(), state.newFolderCursor);
        const TextMetrics prefixMetrics = measureText(theme_.font, prefix, theme_.fontSize);
        modalDrawList_.addRectFilled(Rect(textX + prefixMetrics.width, newFolderInput.y + 6.0f, 1.0f,
                                          newFolderInput.height - 12.0f), theme_.buttonText, newFolderInput);
        if (!state.newFolderError.empty())
        {
            drawText(modalDrawList_, theme_.font, state.newFolderError,
                     Vec2(newFolderInput.x, newFolderInput.y - theme_.fontSize - 3.0f),
                     theme_.fontSize * 0.8f, theme_.dialogBtnDanger, viewport);
        }
    }
    const Color acceptColor = canAccept ? (contains(acceptButton, pointer_.position) ? theme_.dialogBtnPrimaryHover : theme_.dialogBtnPrimary)
                                        : theme_.dialogBtnBg;
    modalDrawList_.addRectFilled(acceptButton, acceptColor, viewport);
    modalDrawList_.addRectFilled(cancelButton, contains(cancelButton, pointer_.position) ? theme_.dialogBtnHover : theme_.dialogBtnBg, viewport);
    const TextMetrics acceptMetrics = measureText(theme_.font, acceptText, theme_.fontSize);
    const StringView cancelText("Cancel");
    const TextMetrics cancelMetrics = measureText(theme_.font, cancelText, theme_.fontSize);
    drawText(modalDrawList_, theme_.font, acceptText,
             Vec2(acceptButton.x + (acceptButton.width - acceptMetrics.width) * 0.5f,
                  acceptButton.y + (acceptButton.height - acceptMetrics.height) * 0.5f), theme_.fontSize, iconColor, viewport);
    drawText(modalDrawList_, theme_.font, cancelText,
             Vec2(cancelButton.x + (cancelButton.width - cancelMetrics.width) * 0.5f,
                  cancelButton.y + (cancelButton.height - cancelMetrics.height) * 0.5f), theme_.fontSize, iconColor, viewport);
    const Color resizeColor = contains(resizeHandle, pointer_.position) || state.resizing ? iconColor : theme_.dialogBorder;
    for (uint32_t line = 0u; line < 3u; ++line)
    {
        const float offset = static_cast<float>(line) * 4.0f;
        modalDrawList_.addLine(Vec2(dialog.right() - 6.0f - offset, dialog.bottom() - 3.0f),
                               Vec2(dialog.right() - 3.0f, dialog.bottom() - 6.0f - offset),
                               resizeColor, viewport, 1.25f);
    }
    if (result.kind != FileDialogResultKind::None)
        state.reset();
    return result;
}

} // namespace ig
