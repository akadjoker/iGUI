#include "MenuWidgets.hpp"
#include "TextClip.hpp"
#include "WidgetApp.hpp"
#include "pch.hpp"

namespace ig
{
namespace retained
{
namespace textclip
{

Action keyAction(const KeyEvent& e)
{
    namespace Key = ig::retained::Key;
    if (e.alt) return Action::None;
    if (e.ctrl && !e.shift)
    {
        if (e.key == Key::C || e.key == Key::Insert) return Action::Copy;
        if (e.key == Key::X) return Action::Cut;
        if (e.key == Key::V) return Action::Paste;
        if (e.key == Key::A) return Action::SelectAll;
    }
    if (e.shift && !e.ctrl)
    {
        if (e.key == Key::Insert) return Action::Paste;
        if (e.key == Key::Delete) return Action::Cut;
    }
    return Action::None;
}

namespace
{

// What a paste puts in: a one line widget drops the line breaks.
void paste(TextClipTarget& t, const String& from)
{
    if (!t.clipEditable() || from.empty()) return;
    if (!t.clipOneLine())
    {
        t.clipReplaceSelection(from);
        return;
    }
    String text;
    text.reserve(from.size());
    for (size_t i = 0; i < from.size(); ++i)
        if (from[i] != '\n' && from[i] != '\r') text += from[i];
    if (!text.empty()) t.clipReplaceSelection(text);
}

} // namespace

void publishSelection(TextClipTarget& t)
{
    if (!t.clipCanCopy() || !t.clipHasSelection()) return;
    WidgetApp::instance().setPrimaryText(t.clipSelection().c_str());
}

bool run(TextClipTarget& t, Action action)
{
    switch (action)
    {
        case Action::None:
            return false;
        case Action::Copy:
            if (t.clipCanCopy() && t.clipHasSelection()) WidgetApp::instance().setClipboardText(t.clipSelection().c_str());
            return true;
        case Action::Cut:
            if (t.clipCanCopy() && t.clipEditable() && t.clipHasSelection())
            {
                WidgetApp::instance().setClipboardText(t.clipSelection().c_str());
                t.clipReplaceSelection(String()); // (last)
            }
            return true;
        case Action::Paste:
            paste(t, WidgetApp::instance().getClipboardText()); // (last)
            return true;
        case Action::SelectAll:
            t.clipSelectAll();
            publishSelection(t);
            return true;
    }
    return false;
}

bool onKey(TextClipTarget& t, KeyEvent& e)
{
    const Action action = keyAction(e);
    if (action == Action::None) return false;
    e.consumed = true;
    return run(t, action);
}

namespace
{

void showMenu(TextClipTarget& t, const MouseEvent& e)
{
    const bool selected = t.clipHasSelection(), editable = t.clipEditable(), copyable = t.clipCanCopy();
    // a menu for this one right click: the popup owns it and deletes it when it closes; the widget is its owner, so a widget that goes while the menu is
    // open closes it (WidgetApp::notifyWidgetRemoved) and the items never run for a widget that is gone
    Menu* menu = new Menu();
    MenuAction* cut = menu->addAction("Cut", [&t] { run(t, Action::Cut); });
    cut->setShortcut("Ctrl+X");
    cut->setEnabled(editable && copyable && selected);
    MenuAction* copy = menu->addAction("Copy", [&t] { run(t, Action::Copy); });
    copy->setShortcut("Ctrl+C");
    copy->setEnabled(copyable && selected);
    MenuAction* pasteAction = menu->addAction("Paste", [&t] { run(t, Action::Paste); });
    pasteAction->setShortcut("Ctrl+V");
    pasteAction->setEnabled(editable && !WidgetApp::instance().getClipboardText().empty());
    MenuAction* del = menu->addAction("Delete", [&t] { t.clipReplaceSelection(String()); });
    del->setShortcut("Del");
    del->setEnabled(editable && selected);
    menu->addSeparator();
    MenuAction* all = menu->addAction("Select All", [&t] { run(t, Action::SelectAll); });
    all->setShortcut("Ctrl+A");
    all->setEnabled(t.clipHasText());
    menu->execOwned(e.x, e.y, t.clipWidget());
}

} // namespace

bool onMouse(TextClipTarget& t, MouseEvent& e)
{
    if (e.button == 2)
    {
        e.consumed = true;
        if (!t.clipEditable()) return true;
        t.clipPlaceCursor(e);
        paste(t, WidgetApp::instance().getPrimaryText()); // (last)
        return true;
    }
    if (e.button == 1)
    {
        e.consumed = true;
        if (!t.clipPointInSelection(e)) t.clipPlaceCursor(e);
        showMenu(t, e);
        return true;
    }
    return false;
}

} // namespace textclip
} // namespace retained
} // namespace ig
