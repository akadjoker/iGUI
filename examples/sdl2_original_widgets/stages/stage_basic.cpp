#include "WidgetApp.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "InputWidgets.hpp"
using namespace BuGUI;

void registerBasicStage(WidgetApp& app)
{
    auto* root = app.addStage("basic");
    auto* vbox = root->createChild<BoxLayout>(LayoutDir::Vertical);
    vbox->setSpacing(8.0f);
    vbox->setPadding(20.0f);

    auto* back = vbox->createChild<Button>("\u2190 Menu");
    back->clicked.connect([&app]() { app.setStage("menu", TransitionType::CoverRight); });
    vbox->createChild<Line>();

    vbox->createChild<Label>("Labels, Buttons, CheckBox, Switch");
    vbox->createChild<Spacer>(8.0f);

    vbox->createChild<Button>("Normal Button");
    vbox->createChild<CheckBox>("Option A");
    vbox->createChild<CheckBox>("Option B");
    vbox->createChild<Switch>("Dark mode");
    vbox->createChild<Spacer>(8.0f);

    vbox->createChild<ProgressBar>(0.0f, 100.0f, 65.0f)->setText("65%");
    vbox->createChild<Spacer>(8.0f);

    auto* hbox = vbox->createChild<BoxLayout>(LayoutDir::Horizontal);
    hbox->setSpacing(8.0f);
    hbox->createChild<Button>("OK");
    hbox->createChild<Button>("Cancel");
    hbox->createChild<Button>("Apply");
}
