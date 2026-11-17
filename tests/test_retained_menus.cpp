// Menus: what an item draws (shortcut, submenu arrow, check mark, icon), who owns the actions and submenus, the signals a
// menu emits. Pixels of the software rasteriser, driven through the real frame loop (tests/RetainedHarness.hpp). No
// standard library: ct and C only.

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

#include <igui/widgets/MenuWidgets.hpp>

using namespace rh;

namespace
{

// ---- shortcut and submenu arrow ----

// A menu with a wide first item that fixes its width, then the item under test. `shortcut` and `submenu` say what it has.
Shot menuShot(const char* shortcut, bool submenu, Rect* itemRect, Rect* menuRect)
{
    Screen screen(420.0f, 160.0f);
    Menu* menu = new Menu();
    menu->addAction("A long entry that sets the width");
    MenuAction* a = menu->addAction("Export");
    if (shortcut && *shortcut) a->setShortcut(shortcut);
    Menu* sub = nullptr;
    if (submenu)
    {
        sub = new Menu();
        sub->addAction("one");
        a->setSubmenu(sub);
    }
    menu->exec(10.0f, 10.0f);
    screen.h.frames(12); // the popup opens in a tenth of a second
    if (itemRect) *itemRect = menu->actionRect(1);
    if (menuRect) *menuRect = menu->rect();
    Shot shot = screen.take();
    WidgetApp::instance().closePopup();
    delete menu;
    delete sub;
    return shot;
}

void testMenuShortcutDoesNotOverlapTheSubmenuArrow()
{
    Rect item, menu;
    const Shot both = menuShot("Ctrl+Shift+N", true, &item, &menu);
    const Shot arrowOnly = menuShot("", true, nullptr, nullptr);
    const Shot shortcutOnly = menuShot("Ctrl+Shift+N", false, nullptr, nullptr);
    // the arrow is drawn within 16 pixels of the right edge of the menu
    const int right = static_cast<int>(menu.x + menu.w);
    const int y0 = static_cast<int>(item.y) + 1, y1 = static_cast<int>(item.y + item.h) - 1;
    const int arrowPixels = differing(both, arrowOnly, right - 16, y0, right - 3, y1);
    if (arrowPixels) printf("    %d pixels of the arrow area differ from the arrow alone\n", arrowPixels);
    check(arrowPixels == 0, "the shortcut of a menu item with a submenu stays clear of the arrow");
    // and the shortcut is still drawn (it moved left, it did not go away)
    check(differing(both, arrowOnly, static_cast<int>(menu.x) + 30, y0, right - 16, y1) > 20,
            "the shortcut of an item with a submenu is drawn");
    // an item with no submenu keeps its shortcut at the right edge, where it always was
    const int edge = differing(shortcutOnly, arrowOnly, right - 30, y0, right - 12, y1);
    check(edge > 10, "the shortcut of an item with no submenu is still at the right edge");
}

// ---- who owns what (meant to run under the address sanitizer: a leak or a double delete is a finding) ----

void testMenuOwnership()
{
    Harness h(420.0f, 300.0f, false);
    // submenus the action owns: a menu rebuilt over and over, then deleted, leaves nothing behind
    Menu* menu = new Menu();
    for (int round = 0; round < 20; ++round)
    {
        menu->clearActions();
        menu->addAction("Novo");
        Menu* recent = menu->addSubmenu("Recentes");
        for (int i = 0; i < 5; ++i) recent->addAction(String("ficheiro") + String::number(i));
        recent->addSubmenu("Mais")->addAction("x");
        menu->addSeparator();
    }
    check(menu->actionCount() == 3 && menu->action(1)->ownsSubmenu(), "a menu rebuilt 20 times has only the last actions");
    delete menu;

    // a submenu the caller owns (setSubmenu with no second argument) is left alone by clearActions and by the destructor
    Menu* mine = new Menu();
    mine->addAction("a");
    Menu* parent = new Menu();
    parent->addAction("Abrir")->setSubmenu(mine);
    check(!parent->action(0)->ownsSubmenu(), "setSubmenu(menu) leaves the submenu to the caller");
    parent->clearActions();
    check(mine->actionCount() == 1, "clearActions leaves a submenu of the caller alive");
    parent->addAction("Abrir")->setSubmenu(mine);
    delete parent;
    check(mine->actionCount() == 1, "so does the destructor of the menu");
    delete mine;

    // setSubmenu(menu, true) takes it, and replacing it deletes the old one
    Menu* owner = new Menu();
    MenuAction* a = owner->addAction("S");
    a->setSubmenu(new Menu(), true);
    a->setSubmenu(new Menu(), true);
    a->setSubmenu(nullptr, true);
    check(a->submenu() == nullptr && !a->ownsSubmenu(), "an owned submenu replaced by nothing is deleted");
    delete owner;

    // a MenuBar deletes the menus it made, with their owned submenus
    MenuBar* bar = new MenuBar();
    Menu* file = bar->addMenu("Ficheiro");
    file->addSubmenu("Recentes")->addAction("um");
    delete bar;

    // rebuilt while it is open and its submenu is open
    Menu* open = new Menu();
    open->addSubmenu("Sub")->addAction("one");
    open->addAction("Outro");
    open->exec(10.0f, 10.0f);
    h.frames(12);
    const Rect row = open->actionRect(0);
    h.moveTo(row.x + row.w * 0.5f, row.y + row.h * 0.5f); // hover opens the submenu
    h.frames(3);
    open->clearActions(); // the submenu goes with its action
    h.frames(3);
    h.moveTo(row.x + 5.0f, row.y + 5.0f);
    h.frames(2);
    open->addAction("Novo");
    h.frames(2);
    WidgetApp::instance().closePopup();
    delete open;
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testMenuShortcutDoesNotOverlapTheSubmenuArrow);
    RUN(testMenuOwnership);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
