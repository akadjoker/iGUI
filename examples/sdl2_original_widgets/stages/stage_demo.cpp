#include "WidgetApp.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
using namespace BuGUI;

void registerDemoStage(WidgetApp& app)
{
    auto* root = app.addStage("demo");
    auto* vbox = root->createChild<BoxLayout>(LayoutDir::Vertical);
    vbox->setSpacing(8.0f);
    vbox->setPadding(20.0f);

    vbox->createChild<Label>("BuGUI + Raylib");
    vbox->createChild<Line>();

    vbox->createChild<Button>("Click me");
    vbox->createChild<CheckBox>("Option A");
    vbox->createChild<Switch>("Dark mode");

    auto* row = vbox->createChild<BoxLayout>(LayoutDir::Horizontal);
    row->setSpacing(8.0f);
    row->createChild<Button>("OK");
    row->createChild<Button>("Cancel");
}
