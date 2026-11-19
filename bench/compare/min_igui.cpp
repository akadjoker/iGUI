// The smallest iGUI application on a zen_plataform window: a label. Only for the size of the binary (docs/COMPARACAO.md).
#include <igui_zen/ZenApp.hpp>
#include <Widgets.hpp>

int main()
{
    ig::zen::App::Config c;
    c.title = "min";
    c.width = 400;
    c.height = 300;
    c.showStats = false;
    c.preferOpenGL = false;
    ig::zen::App app;
    if (!app.create(c)) return 1;
    ig::retained::WidgetApp& wa = app.widgets();
    ig::retained::Widget* root = wa.addStage("main")->createChild<ig::retained::Widget>();
    wa.setStage("main");
    auto* l = root->createChild<ig::retained::Label>("Hello");
    l->setPosition(10, 10);
    l->setSize(100, 24);
    app.run();
    return 0;
}
