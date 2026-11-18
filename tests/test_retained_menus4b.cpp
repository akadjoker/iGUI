// Menus, second half of round 4 (docs/ROUND4B.md): the icon of an action, the toggled state and the automatic toggle, the
// aboutToShow signal, and popups that hold widgets. Pixels of the software rasteriser, driven through the real frame
// loop (tests/RetainedHarness.hpp). No standard library: ct and C only.

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/MenuWidgets.hpp>
#include <igui/widgets/TextInputWidgets.hpp>

using namespace rh;

namespace
{

// ---- item 12: the icon of an action ----

// One menu with a wide first item that fixes the width, then "Export" with the icon and the state it is given.
Shot iconShot(IconId icon, bool checkable, bool checked, bool enabled, Rect* itemRect, Rect* menuRect)
{
    Screen screen(420.0f, 160.0f);
    Menu* menu = new Menu();
    menu->addAction("A long entry that sets the width");
    MenuAction* a = menu->addAction("Export");
    a->setCheckable(checkable);
    a->setChecked(checked);
    a->setEnabled(enabled);
    a->setIcon(icon);
    menu->exec(10.0f, 10.0f);
    screen.h.frames(12); // the popup opens in a tenth of a second
    if (itemRect) *itemRect = menu->actionRect(1);
    if (menuRect) *menuRect = menu->rect();
    Shot shot = screen.take();
    WidgetApp::instance().closePopup();
    delete menu;
    return shot;
}

void testMenuActionIcon()
{
    Rect item, menu, itemNone, menuNone;
    const Shot none = iconShot(IconId::None, false, false, true, &itemNone, &menuNone);
    const Shot with = iconShot(IconId::Folder, false, false, true, &item, &menu);
    // a new action has no icon, and keeps what it is given
    Menu* probe = new Menu();
    MenuAction* a = probe->addAction("x");
    check(a->icon() == IconId::None, "an action has no icon at first");
    a->setIcon(IconId::Gear);
    check(a->icon() == IconId::Gear, "setIcon keeps the icon");
    a->setIcon(IconId::None);
    check(a->icon() == IconId::None, "setIcon(None) takes it away");
    delete probe;

    // the menu is as wide as it was: the column of the check mark was always there
    check(menu.w == menuNone.w && menu.h == menuNone.h, "an icon does not change the size of the menu");

    // the icon is in the left column of the item (22 pixels), the text where it was
    const int x0 = static_cast<int>(item.x), y0 = static_cast<int>(item.y) + 1, y1 = static_cast<int>(item.y + item.h) - 1;
    const int inColumn = differing(with, none, x0, y0, x0 + 22, y1);
    if (inColumn < 15) printf("    %d pixels differ in the icon column\n", inColumn);
    check(inColumn >= 15, "an action with an icon draws it in the left column");
    check(differing(with, none, x0 + 22, y0, static_cast<int>(item.x + item.w), y1) == 0,
            "the text of an action with an icon is where it was, and nothing else changes");
    check(differing(with, none, static_cast<int>(item.x), static_cast<int>(item.y) - 26, static_cast<int>(item.x + item.w),
                  static_cast<int>(item.y)) == 0,
            "the other items do not change");

    // taking the icon away gives the picture of an action that never had one
    const Shot removed = iconShot(IconId::None, false, false, true, nullptr, nullptr);
    check(differing(removed, none, 0, 0, removed.w, removed.h) == 0, "no icon is the same picture as before");

    // a disabled action draws its icon in the disabled colour
    const Shot disabled = iconShot(IconId::Folder, false, false, false, nullptr, nullptr);
    const int dim = differing(disabled, with, x0, y0, x0 + 22, y1);
    if (dim == 0) printf("    the disabled icon is drawn as the enabled one\n");
    check(dim > 0, "the icon of a disabled action takes the disabled colour");

    // a checked checkable action: a frame around the icon, no tick; the unchecked one has no frame
    const Shot checked = iconShot(IconId::Folder, true, true, true, nullptr, nullptr);
    const Shot unchecked = iconShot(IconId::Folder, true, false, true, nullptr, nullptr);
    check(differing(unchecked, with, 0, 0, with.w, with.h) == 0, "a checkable action that is not checked draws only its icon");
    const int frame = differing(checked, unchecked, x0, y0, x0 + 22, y1);
    if (frame < 8) printf("    %d pixels differ between checked and unchecked\n", frame);
    check(frame >= 8, "a checked action with an icon shows a frame around it");
    check(differing(checked, unchecked, x0 + 22, y0, static_cast<int>(item.x + item.w), y1) == 0,
            "the frame stays in the column of the icon");
}

// ---- item 13: toggled and the automatic toggle ----

// Opens `menu` in the harness and clicks the centre of its item `idx`, as a person does.
void clickItem(Harness& h, Menu* menu, int idx)
{
    menu->exec(10.0f, 10.0f);
    h.frames(12); // the popup opens in a tenth of a second
    const Rect r = menu->actionRect(idx);
    h.click(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
    h.frames(2);
}

// What the signals of one action saw, in order: 'T' for toggled (with its state in `states`), 'G' for triggered.
struct Log
{
    char order[16] = {};
    int n = 0;
    bool states[8] = {};
    int nStates = 0;
    bool checkedInTriggered = false;
    void add(char c)
    {
        if (n < 15) order[n++] = c;
    }
};

void testMenuToggled()
{
    Harness h(420.0f, 300.0f, false);
    Menu* menu = new Menu();
    MenuAction* plain = menu->addAction("Plain");
    MenuAction* box = menu->addCheckable("Box", false);
    Log plainLog, boxLog;
    plain->triggered.connect([&]() { plainLog.add('G'); });
    plain->toggled.connect([&](bool) { plainLog.add('T'); });
    box->toggled.connect(
            [&](bool on)
            {
                boxLog.add('T');
                if (boxLog.nStates < 8) boxLog.states[boxLog.nStates++] = on;
            });
    box->triggered.connect(
            [&]()
            {
                boxLog.add('G');
                boxLog.checkedInTriggered = box->isChecked();
            });

    check(box->autoToggle() && plain->autoToggle(), "a click flips a checkable action unless told not to (the default)");

    // a checkable action flips, tells toggled with the new state, then triggered, which sees the new state too
    clickItem(h, menu, 1);
    check(box->isChecked(), "a click checks a checkable action");
    check(boxLog.n == 2 && boxLog.order[0] == 'T' && boxLog.order[1] == 'G', "toggled comes first, then triggered, once each");
    check(boxLog.nStates == 1 && boxLog.states[0], "toggled says the new state: checked");
    check(boxLog.checkedInTriggered, "triggered sees the new state, as it always did");
    clickItem(h, menu, 1);
    check(!box->isChecked() && boxLog.nStates == 2 && !boxLog.states[1], "the second click unchecks it, and toggled says so");

    // an action that is not checkable only triggers
    clickItem(h, menu, 0);
    check(plainLog.n == 1 && plainLog.order[0] == 'G', "an action that is not checkable emits triggered only");

    // setChecked is silent
    box->setChecked(true);
    check(boxLog.nStates == 2, "setChecked does not emit toggled");

    // setAutoToggle(false): the click leaves the state to the application, and emits triggered only
    box->setChecked(false);
    box->setAutoToggle(false);
    check(!box->autoToggle(), "setAutoToggle(false) is kept");
    boxLog = Log();
    clickItem(h, menu, 1);
    check(!box->isChecked(), "with no auto toggle a click does not flip the action");
    check(boxLog.n == 1 && boxLog.order[0] == 'G', "with no auto toggle only triggered is emitted");
    // and the application can answer the click itself
    box->triggered.connect([&]() { box->setChecked(true); });
    clickItem(h, menu, 1);
    check(box->isChecked() && boxLog.nStates == 0, "the application sets the state from triggered; toggled stays silent");
    delete menu;
}

// a handler of toggled that rebuilds the menu, or deletes it, takes the action away: triggered must not be emitted on it
// (meant to run under the address sanitizer)
void testMenuToggledHandlerRebuildsMenu()
{
    Harness h(420.0f, 300.0f, false);
    {
        Menu* menu = new Menu();
        MenuAction* box = menu->addCheckable("Box", false);
        int triggered = 0;
        box->toggled.connect([&](bool) { menu->clearActions(); });
        box->triggered.connect([&]() { ++triggered; });
        clickItem(h, menu, 0);
        check(menu->actionCount() == 0, "the handler of toggled cleared the menu");
        check(triggered == 0, "triggered is not emitted on an action a handler of toggled deleted");
        delete menu;
    }
    {
        Menu* menu = new Menu();
        MenuAction* box = menu->addCheckable("Box", false);
        int triggered = 0;
        box->toggled.connect([&](bool) { delete menu; });
        box->triggered.connect([&]() { ++triggered; });
        clickItem(h, menu, 0);
        check(triggered == 0, "nor on one whose whole menu a handler of toggled deleted");
        h.frames(20);
    }
    {
        // the same for triggered, as before: the last thing the click does
        Menu* menu = new Menu();
        MenuAction* box = menu->addCheckable("Box", false);
        box->triggered.connect([&]() { delete menu; });
        clickItem(h, menu, 0);
        h.frames(20);
    }
}

// ---- item 14: aboutToShow ----

void testMenuAboutToShow()
{
    Harness h(420.0f, 300.0f, false);
    // exec: emitted once per showing, before the size is worked out, so the items the handler adds are in the menu
    Menu* menu = new Menu();
    menu->addAction("Fixed");
    int shown = 0;
    menu->aboutToShow.connect(
            [&]()
            {
                ++shown;
                menu->clearActions();
                for (int i = 0; i < shown; ++i) menu->addAction(String("item ") + String::number(i));
            });
    check(shown == 0, "adding actions to a menu does not emit aboutToShow");
    menu->exec(10.0f, 10.0f);
    h.frames(12);
    check(shown == 1, "exec emits aboutToShow once");
    check(menu->actionCount() == 1, "the handler rebuilt the menu");
    const float h1 = menu->rect().h;
    h.frames(30);
    check(shown == 1, "frames and hovering do not emit it again");
    h.moveTo(menu->rect().x + 20.0f, menu->rect().y + 10.0f);
    h.frames(3);
    check(shown == 1, "hovering an item does not emit it again");
    WidgetApp::instance().closePopup();
    h.frames(20);
    menu->exec(10.0f, 10.0f);
    h.frames(12);
    check(shown == 2 && menu->actionCount() == 2, "the second showing emits it again, and the menu has the items of the second");
    check(menu->rect().h > h1, "the size of the menu is the one of the items the handler added, not of those it had");
    check(menu->rect().h == menu->sizeHint().y, "the rectangle of the menu is its size hint");
    WidgetApp::instance().closePopup();
    h.frames(20);
    delete menu;

    // a MenuBar opens its menus with exec: a click on the title emits it
    MenuBar* bar = new MenuBar();
    h.root()->addChild(bar);
    bar->setRect(Rect(0.0f, 0.0f, 420.0f, 26.0f));
    Menu* file = bar->addMenu("File");
    int fileShown = 0;
    file->aboutToShow.connect([&]() { ++fileShown; file->addAction("New"); });
    h.relayout();
    h.clickWidget(bar);
    bar->openMenu(0);
    h.frames(3);
    check(fileShown >= 1 && file->actionCount() >= 1, "a menu opened from a MenuBar emits aboutToShow");
    WidgetApp::instance().closePopup();
    h.frames(20);
}  // the bar belongs to the window, which goes with the harness

void testSubmenuAboutToShow()
{
    Harness h(420.0f, 300.0f, false);
    Menu* menu = new Menu();
    menu->addAction("A long entry that sets the width");
    Menu* sub = menu->addSubmenu("Recent");
    int shown = 0;
    sub->aboutToShow.connect(
            [&]()
            {
                ++shown;
                sub->clearActions();
                for (int i = 0; i < 3 * shown; ++i) sub->addAction(String("file") + String::number(i));
            });
    menu->exec(10.0f, 10.0f);
    h.frames(12);
    check(shown == 0, "a submenu that is not open has not emitted aboutToShow");
    const Rect row = menu->actionRect(1);
    h.moveTo(row.x + 20.0f, row.y + row.h * 0.5f);
    h.frames(3);
    check(shown == 1, "hovering the item of a submenu emits its aboutToShow");
    check(sub->actionCount() == 3, "the submenu has the items its handler built");
    check(sub->rect().h == sub->sizeHint().y && sub->rect().h > 3.0f * Theme::instance().menuItemHeight,
            "and it opens with the size of those items");
    h.moveTo(row.x + 30.0f, row.y + row.h * 0.5f);
    h.frames(3);
    check(shown == 1, "moving over the same item does not emit it again");
    // away and back: the submenu closes, and opening it again emits again
    const Rect first = menu->actionRect(0);
    h.moveTo(first.x + 20.0f, first.y + first.h * 0.5f);
    h.frames(3);
    h.moveTo(row.x + 20.0f, row.y + row.h * 0.5f);
    h.frames(3);
    check(shown == 2 && sub->actionCount() == 6, "opening it again emits again, and the items are the new ones");
    WidgetApp::instance().closePopup();
    h.frames(20);
    delete menu;
}

// the handler of a submenu's aboutToShow rebuilds the menu that holds it, which it must not: the menu stays standing and
// opens nothing (meant to run under the address sanitizer)
void testSubmenuAboutToShowRebuildsParent()
{
    Harness h(420.0f, 300.0f, false);
    Menu* menu = new Menu();
    menu->addAction("A long entry that sets the width");
    Menu* sub = menu->addSubmenu("Recent");
    sub->addAction("file");
    sub->aboutToShow.connect([&]() { menu->clearActions(); });
    menu->exec(10.0f, 10.0f);
    h.frames(12);
    const Rect row = menu->actionRect(1);
    h.moveTo(row.x + 20.0f, row.y + row.h * 0.5f);
    h.frames(3);
    check(menu->actionCount() == 0, "the parent was cleared by the handler");
    h.moveTo(row.x + 22.0f, row.y + 3.0f);
    h.frames(3);
    WidgetApp::instance().closePopup();
    h.frames(20);
    delete menu;
}

// ---- item 15: a popup that holds widgets ----

// A panel with a text field, a check box and a button, and a button in the window to open it from.
struct PanelRig
{
    Harness h;
    Button* anchor = nullptr;
    Widget* other = nullptr; // a widget of the window that has the focus before the panel opens
    PopupPanel* panel = nullptr;
    TextInput* input = nullptr;
    CheckBox* box = nullptr;
    Button* ok = nullptr;
    int okClicks = 0, closedCount = 0;
    explicit PanelRig(float w = 420.0f, float hh = 300.0f) : h(w, hh, false)
    {
        anchor = h.root()->createChild<Button>("Open");
        anchor->setRect(Rect(20.0f, 20.0f, 80.0f, 28.0f));
        TextInput* field = h.root()->createChild<TextInput>();
        field->setRect(Rect(20.0f, 80.0f, 160.0f, 26.0f));
        other = field;
        panel = new PopupPanel(LayoutDir::Vertical);
        input = panel->createChild<TextInput>();
        box = panel->createChild<CheckBox>("Loop");
        ok = panel->createChild<Button>("Apply");
        ok->clicked.connect([this]() { ++okClicks; });
        panel->closed.connect([this]() { ++closedCount; });
        h.frames(2);
    }
    ~PanelRig()
    {
        if (panel && panel->isOpen()) panel->close();
        h.frames(20);
        delete panel;
    }
    void settle()
    {
        h.frames(14); // opening and closing take a tenth of a second
    }
    static bool inside(const Rect& inner, const Rect& outer)
    {
        return inner.x >= outer.x - 0.01f && inner.y >= outer.y - 0.01f && inner.x + inner.w <= outer.x + outer.w + 0.01f &&
               inner.y + inner.h <= outer.y + outer.h + 0.01f;
    }
};

void testPopupPanelOpensAndPlaces()
{
    PanelRig r;
    check(!r.panel->isOpen(), "a panel is not open until it is opened");
    r.panel->open(30.0f, 120.0f, r.anchor);
    r.settle();
    check(r.panel->isOpen() && WidgetApp::instance().popup() == r.panel, "open() shows the panel as the popup");
    const Rect pr = r.panel->absoluteRect();
    check(pr.x == 30.0f && pr.y == 120.0f, "it opens where it was told to");
    const Widget::Vec2f hint = r.panel->sizeHint();
    check(pr.w == hint.x && pr.h == hint.y && pr.w > 40.0f && pr.h > 60.0f, "at its size hint");
    check(PanelRig::inside(r.input->absoluteRect(), pr) && PanelRig::inside(r.box->absoluteRect(), pr) &&
                    PanelRig::inside(r.ok->absoluteRect(), pr),
            "its children are laid out inside it");
    check(r.input->absoluteRect().w > 40.0f && r.ok->absoluteRect().h > 10.0f, "and have a size");
    check(r.input->absoluteRect().y < r.box->absoluteRect().y && r.box->absoluteRect().y < r.ok->absoluteRect().y,
            "one under the other, in the order they were added");
    r.panel->close();
    r.settle();
    check(!r.panel->isOpen(), "close() closes it");

    // too near the corner: moved to stay in the window
    r.panel->open(410.0f, 295.0f);
    r.settle();
    const Rect corner = r.panel->absoluteRect();
    check(corner.x + corner.w <= 420.0f && corner.y + corner.h <= 300.0f && corner.x >= 0.0f && corner.y >= 0.0f,
            "a panel opened near the corner of the window is moved to stay in it");
    r.panel->close();
    r.settle();

    // openBelow: under the anchor, left edges aligned; above it when there is no room below
    r.panel->openBelow(r.anchor);
    r.settle();
    const Rect a = r.anchor->absoluteRect();
    check(r.panel->absoluteRect().x == a.x && r.panel->absoluteRect().y == a.y + a.h + 2.0f, "openBelow puts the panel under the widget");
    check(WidgetApp::instance().popupOwner() == r.anchor, "with the widget as the owner of the popup");
    r.panel->close();
    r.settle();
    r.anchor->setRect(Rect(20.0f, 250.0f, 80.0f, 28.0f));
    r.panel->openBelow(r.anchor);
    r.settle();
    check(r.panel->absoluteRect().y + r.panel->absoluteRect().h <= r.anchor->absoluteRect().y,
            "with no room below, openBelow puts it above the widget");
    // opening an open panel moves it
    r.panel->open(200.0f, 20.0f);
    r.settle();
    check(r.panel->isOpen() && r.panel->absoluteRect().x == 200.0f, "opening an open panel moves it");
    r.panel->close();
    r.settle();
    check(r.closedCount == 4, "closed went out once for each of the four openings");
}

void testPopupPanelFocusKeysAndClicks()
{
    PanelRig r;
    WidgetApp& app = WidgetApp::instance();
    r.h.clickWidget(r.other);
    check(app.focused() == r.other, "(the window's text field has the focus)");
    // the panel takes the focus when it opens, and gives it back when it closes
    r.panel->open(30.0f, 120.0f, r.anchor);
    r.settle();
    check(app.focused() == r.panel, "a panel with no focus target takes the focus itself");
    r.panel->close();
    r.settle();
    check(app.focused() == r.other, "closing it gives the focus back to the widget that had it");
    r.panel->setFocusTarget(r.input);
    r.panel->open(30.0f, 120.0f, r.anchor);
    r.settle();
    check(app.focused() == r.input && r.panel->focusTarget() == r.input, "with a focus target the target takes it");
    // typing goes to the text field of the panel, not to the window's
    r.h.type("ab");
    check(r.input->text() == "ab" && r.other == r.other && static_cast<TextInput*>(r.other)->text().empty(),
            "typed text goes to the field of the panel");
    // a press on a widget that takes the focus focuses it: the check box, then the field again
    r.h.clickWidget(r.box);
    check(app.focused() == r.box && r.box->isChecked(), "a click on the check box focuses it and checks it");
    r.h.clickWidget(r.input);
    check(app.focused() == r.input, "a click on the text field focuses it");
    check(r.panel->isOpen(), "clicks inside the panel leave it open");
    // a click on a button works and leaves the panel open
    r.h.clickWidget(r.ok);
    check(r.okClicks == 1 && r.panel->isOpen(), "a button of the panel is clicked, and the panel stays open");
    // Tab walks the children and stays inside the panel
    r.h.clickWidget(r.input);
    r.h.key(ig::retained::Key::Tab);
    check(app.focused() == r.box, "Tab goes to the next widget of the panel");
    r.h.key(ig::retained::Key::Tab);
    check(app.focused() == r.ok, "and to the next");
    r.h.key(ig::retained::Key::Tab);
    check(app.focused() == r.input, "and round to the first: it does not leave the panel (nor stop at the panel itself)");
    r.h.key(ig::retained::Key::Tab, true);
    check(app.focused() == r.ok, "Shift+Tab goes back");
    // Escape closes, whichever child has the focus
    r.h.key(ig::retained::Key::Escape);
    check(!r.panel->isOpen(), "Escape closes the panel");
    r.settle();
    check(app.focused() == r.other, "the focus is back in the window");
    check(r.closedCount == 2, "and closed went out for the second opening");

    // reopened: the children kept their state, and it is a new opening
    r.panel->open(30.0f, 120.0f, r.anchor);
    r.settle();
    check(r.input->text() == "ab" && r.box->isChecked(), "the children keep their state between showings");
    // a click outside closes it (and the press goes on to what is under it)
    r.h.click(380.0f, 250.0f);
    check(!r.panel->isOpen(), "a click outside closes the panel");
    r.settle();
    check(r.closedCount == 3, "closed went out for the third opening too, and only once");
    // a click on the owner does not close it by itself: the owner toggles it
    r.anchor->clicked.connect([&r]() { if (r.panel->isOpen()) r.panel->close(); else r.panel->openBelow(r.anchor); });
    r.h.clickWidget(r.anchor);
    r.settle();
    check(r.panel->isOpen(), "a click on the anchor opens the panel (the application's handler)");
    r.h.clickWidget(r.anchor);
    r.settle();
    check(!r.panel->isOpen(), "and the next click on the anchor closes it, once (the press did not close it first)");
}

// opened over a menu and a menu over it: whichever came last is the popup, the other one goes at once; a panel deleted
// while it is open takes its popup with it (meant to run under the address sanitizer)
void testPopupPanelWithOtherPopupsAndDelete()
{
    PanelRig r;
    Menu* menu = new Menu();
    menu->addAction("one");
    r.panel->open(30.0f, 120.0f, r.anchor);
    r.settle();
    menu->exec(200.0f, 50.0f);
    r.settle();
    check(!r.panel->isOpen() && WidgetApp::instance().popup() == menu, "a menu opened over the panel takes its place");
    check(r.closedCount == 1, "the panel let go: closed went out once, at once");
    r.panel->open(30.0f, 120.0f, r.anchor);
    r.settle();
    check(WidgetApp::instance().popup() == r.panel, "and the panel takes the place of the menu");
    WidgetApp::instance().closePopup();
    r.settle();
    delete menu;
    // deleted while open
    PopupPanel* second = new PopupPanel();
    second->createChild<Button>("x");
    second->open(100.0f, 100.0f);
    r.settle();
    delete second;
    r.h.frames(20);
    check(WidgetApp::instance().popup() == nullptr, "a panel deleted while it is open is not the popup any more");
    // a handler of closed that deletes the panel
    PopupPanel* third = new PopupPanel();
    third->createChild<Button>("y");
    bool handled = false;
    third->closed.connect([&]() { handled = true; delete third; });
    third->open(100.0f, 100.0f);
    r.settle();
    third->close();
    r.settle();
    check(handled, "a handler of closed may delete the panel");
    // the panel is the popup of its window: a window that goes with it open leaves nothing behind (the rig's destructor
    // closes and deletes the first panel)
    r.panel->open(30.0f, 120.0f);
    r.settle();
}

void testPopupPanelPaints()
{
    Screen screen(420.0f, 300.0f);
    PopupPanel* panel = new PopupPanel(LayoutDir::Vertical);
    panel->setPadding(20.0f);
    Button* b = panel->createChild<Button>("Apply");
    (void)b;
    const Shot before = screen.take();
    panel->open(40.0f, 40.0f);
    screen.h.frames(14);
    const Shot after = screen.take();
    const Rect pr = panel->absoluteRect();
    // the padding of the panel is drawn in the panel colour (an empty corner of it)
    const int px = static_cast<int>(pr.x) + 6, py = static_cast<int>(pr.y) + 6;
    const Color c = Theme::instance().panelColor;
    const uint32_t want = 0xff000000u | (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
    check(after.px[static_cast<size_t>(py) * static_cast<size_t>(after.w) + static_cast<size_t>(px)] == want,
            "the panel is drawn with the panel colour of the theme");
    check(differing(before, after, static_cast<int>(pr.x), static_cast<int>(pr.y), static_cast<int>(pr.x + pr.w),
                  static_cast<int>(pr.y + pr.h)) > 200,
            "and its button is drawn");
    check(differing(before, after, 0, 0, after.w, static_cast<int>(pr.y) - 2) == 0, "nothing outside the panel changes");
    panel->close();
    screen.h.frames(20);
    const Shot gone = screen.take();
    check(differing(before, gone, 0, 0, gone.w, gone.h) == 0, "closed, the window is as it was");
    delete panel;
}

// the closed handler of a panel opens another popup: whichever way the panel was let go of, the popup the handler opened is
// the one that is open afterwards, nothing is lost and nothing is shown twice (meant to run under the address sanitizer)
void testPopupPanelClosedHandlerOpensAPopup()
{
    // 1. the panel is closed by a click outside it; after its fade the handler opens a menu
    {
        PanelRig r;
        Menu* menu = new Menu();
        menu->addAction("one");
        int menuOpens = 0;
        r.panel->closed.connect(
                [&]()
                {
                    ++menuOpens;
                    menu->exec(200.0f, 150.0f);
                });
        r.panel->open(30.0f, 120.0f, r.anchor);
        r.settle();
        r.h.click(380.0f, 250.0f); // outside
        r.settle();
        check(menuOpens == 1, "the handler of closed ran once");
        check(WidgetApp::instance().popup() == menu, "and the menu it opened is the popup");
        check(!r.panel->isOpen(), "the panel is not");
        r.h.frames(20);
        check(WidgetApp::instance().popup() == menu && r.closedCount == 1, "later too, and closed went out once");
        WidgetApp::instance().closePopup();
        r.settle();
        delete menu;
    }
    // 2. the panel is let go of because the application opens another popup over it; the handler opens a third
    {
        PanelRig r;
        Menu* chosen = new Menu();
        chosen->addAction("chosen");
        Menu* fromHandler = new Menu();
        fromHandler->addAction("from the handler");
        r.panel->closed.connect([&]() { fromHandler->exec(220.0f, 160.0f); });
        r.panel->open(30.0f, 120.0f, r.anchor);
        r.settle();
        chosen->exec(200.0f, 50.0f);
        r.settle();
        check(r.closedCount == 1, "closed went out once");
        check(WidgetApp::instance().popup() == fromHandler, "the menu the handler opened came after the application's, so it is the popup");
        // it works: it closes and the next popup opens as usual
        WidgetApp::instance().closePopup();
        r.settle();
        check(WidgetApp::instance().popup() == nullptr, "and closes");
        fromHandler->exec(10.0f, 10.0f);
        r.settle();
        check(WidgetApp::instance().popup() == fromHandler, "the next popup opens as usual");
        WidgetApp::instance().closePopup();
        r.settle();
        delete chosen;
        delete fromHandler;
    }
    // 3. the handler opens the panel again (a panel that stays open): one opening for each closing
    {
        PanelRig r;
        int reopened = 0;
        r.panel->closed.connect(
                [&]()
                {
                    if (reopened++ < 2) r.panel->open(40.0f, 130.0f, r.anchor);
                });
        r.panel->open(30.0f, 120.0f, r.anchor);
        r.settle();
        r.h.click(380.0f, 250.0f);
        r.settle();
        check(r.panel->isOpen(), "a handler that opens the panel again leaves it open");
        r.panel->close();
        r.settle();
        check(r.panel->isOpen(), "(a second time)");
        r.panel->close();
        r.settle();
        check(!r.panel->isOpen() && reopened == 3, "and the third time it stays closed");
    }
    // 4. the handler opens a popup and deletes the panel
    {
        PanelRig r;
        PopupPanel* doomed = new PopupPanel();
        doomed->createChild<Button>("x");
        Menu* menu = new Menu();
        menu->addAction("m");
        doomed->closed.connect(
                [&]()
                {
                    menu->exec(100.0f, 100.0f);
                    delete doomed;
                });
        doomed->open(30.0f, 120.0f);
        r.settle();
        r.h.click(380.0f, 250.0f);
        r.settle();
        check(WidgetApp::instance().popup() == menu, "a handler that opens a menu and deletes the panel: the menu is the popup");
        WidgetApp::instance().closePopup();
        r.settle();
        delete menu;
    }
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testMenuActionIcon);
    RUN(testMenuToggled);
    RUN(testMenuToggledHandlerRebuildsMenu);
    RUN(testMenuAboutToShow);
    RUN(testSubmenuAboutToShow);
    RUN(testSubmenuAboutToShowRebuildsParent);
    RUN(testPopupPanelOpensAndPlaces);
    RUN(testPopupPanelFocusKeysAndClicks);
    RUN(testPopupPanelWithOtherPopupsAndDelete);
    RUN(testPopupPanelPaints);
    RUN(testPopupPanelClosedHandlerOpensAPopup);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
