// LinkLabel: a click, Enter and Space emit linkClicked with the address, the pointer changes its state, the
// application's openUrl hook is called. No standard library: ct and C only.

#include "RetainedHarness.hpp"

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>

using namespace rh;

namespace
{

struct Scene
{
    Harness h{ 400.0f, 200.0f };
    LinkLabel* link = nullptr;
    Button* other = nullptr;
    int emitted = 0;
    String last;

    Scene()
    {
        BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        link = box->createChild<LinkLabel>("github.com/x/y", "https://github.com/x/y");
        other = box->createChild<Button>("Other");
        h.relayout();
        link->linkClicked.connect(
                [this](const String& u)
                {
                    ++emitted;
                    last = u;
                });
    }
};

void testClick()
{
    Scene s;
    s.h.clickWidget(s.link);
    check(s.emitted == 1, "a click on a LinkLabel emits linkClicked once");
    check(s.last == "https://github.com/x/y", "with the address of the link");
    s.link->setUrl("https://example.com");
    s.h.clickWidget(s.link);
    check(s.last == "https://example.com" && s.link->url() == "https://example.com", "setUrl changes what it emits");
    s.h.clickWidget(s.other);
    check(s.emitted == 2, "a click elsewhere emits nothing");
}

void testHover()
{
    Scene s;
    float x = 0, y = 0;
    Harness::centre(s.link, x, y);
    s.h.moveTo(x, y);
    s.h.frames(2);
    check(s.link->isHovered(), "the pointer over a LinkLabel hovers it");
    check(s.link->cursor() == CursorType::Hand, "its cursor is the hand");
    s.h.moveTo(390.0f, 190.0f);
    s.h.frames(2);
    check(!s.link->isHovered(), "and leaving ends it");
}

void testKeyboard()
{
    Scene s;
    WidgetApp::instance().setFocused(s.link);
    s.h.frames(1);
    s.h.key(Key::Return);
    check(s.emitted == 1, "Enter on a focused LinkLabel activates it");
    s.h.key(Key::Space);
    check(s.emitted == 2, "Space activates it");
    s.h.key(Key::Space, true);
    check(s.emitted == 2, "Shift+Space does not");
}

void testHook()
{
    Scene s;
    int opened = 0;
    String got;
    WidgetApp::instance().setOpenUrl(
            [&](const String& u)
            {
                ++opened;
                got = u;
            });
    s.h.clickWidget(s.link);
    check(opened == 1 && got == "https://github.com/x/y", "the openUrl hook of the application gets the address");
    check(s.emitted == 1, "and the signal is emitted too");
    s.link->setEnabled(false);
    s.h.clickWidget(s.link);
    check(opened == 1, "a disabled link opens nothing");
    WidgetApp::instance().setOpenUrl(ct::Function<void(const String&)>());
    check(!WidgetApp::instance().hasOpenUrl(), "the hook can be cleared");
}

} // namespace

int main()
{
    testClick();
    testHover();
    testKeyboard();
    testHook();
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
