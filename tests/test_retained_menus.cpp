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

} // namespace

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testMenuShortcutDoesNotOverlapTheSubmenuArrow);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
