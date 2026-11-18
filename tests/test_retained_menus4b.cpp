// Menus, second half of round 4 (docs/ROUND4B.md): the icon of an action, the toggled state and the automatic toggle, the
// aboutToShow signal, and popups that hold widgets. Pixels of the software rasteriser, driven through the real frame
// loop (tests/RetainedHarness.hpp). No standard library: ct and C only.

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

#include <igui/widgets/MenuWidgets.hpp>

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
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
