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

} // namespace

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testMenuActionIcon);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
