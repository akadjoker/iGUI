#include "WidgetApp.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "InputWidgets.hpp"
#include <cstdio>
using namespace BuGUI;

void registerControlsStage(WidgetApp& app)
{
    auto* root = app.addStage("controls");
    auto* vbox = root->createChild<BoxLayout>(LayoutDir::Vertical);
    vbox->setSpacing(8.0f);
    vbox->setPadding(20.0f);

    auto* back = vbox->createChild<Button>("\u2190 Menu");
    back->clicked.connect([&app]() { app.setStage("menu", TransitionType::CoverRight); });
    vbox->createChild<Line>();

    vbox->createChild<Label>("Slider, FormLayout, RadioGroup, TabLayout");
    vbox->createChild<Spacer>(8.0f);

    auto* tabs = vbox->createChild<TabLayout>();
    tabs->setStretch(1.0f);

    // ── Tab 1: Form ──────────────────────────────────────────────────────────
    auto* formPage = tabs->addTab<BoxLayout>("Form", LayoutDir::Vertical);
    formPage->setSpacing(10.0f);
    formPage->setPadding(12.0f);

    auto* form = formPage->createChild<FormLayout>();
    form->setSpacing(10.0f, 6.0f);
    form->setPadding(4.0f);

    auto* speedLabel = form->addRow<Label>("Speed:", "0");
    auto* slider     = form->addRow<Slider>("Value:", 0.0f, 100.0f, 40.0f);
    slider->setStretch(1.0f);
    slider->onValueChanged.connect([speedLabel](float v) {
        char buf[16]; std::snprintf(buf, sizeof(buf), "%.0f", v);
        speedLabel->setText(buf);
    });
    speedLabel->setText("40");

    form->addRow<CheckBox>("Enabled:", "Active");
    form->addRow<Switch>("Dark mode:", "");
    form->addRow<ProgressBar>("Progress:", 0.0f, 100.0f, 60.0f)->setText("60%");

    // ── Tab 2: Radio + Flow ──────────────────────────────────────────────────
    auto* flowPage = tabs->addTab<BoxLayout>("Options", LayoutDir::Vertical);
    flowPage->setSpacing(10.0f);
    flowPage->setPadding(12.0f);

    flowPage->createChild<Label>("RadioGroup");
    auto* radioBox = flowPage->createChild<BoxLayout>(LayoutDir::Horizontal);
    radioBox->setSpacing(16.0f);
    static RadioGroup rg;
    auto* rb1 = radioBox->createChild<RadioButton>("Option A", &rg);
    auto* rb2 = radioBox->createChild<RadioButton>("Option B", &rg);
    auto* rb3 = radioBox->createChild<RadioButton>("Option C", &rg);
    rb1->setSelected(true);
    (void)rb2; (void)rb3;

    auto* selLabel = flowPage->createChild<Label>("Selected: Option A");
    rg.selectionChanged.connect([selLabel](int idx) {
        const char* names[] = { "Option A", "Option B", "Option C" };
        if (idx >= 0 && idx < 3) {
            char buf[40]; std::snprintf(buf, sizeof(buf), "Selected: %s", names[idx]);
            selLabel->setText(buf);
        }
    });

    flowPage->createChild<Spacer>(12.0f);
    flowPage->createChild<Label>("FlowLayout tags");
    auto* flow = flowPage->createChild<FlowLayout>();
    flow->setSpacing(6.0f, 6.0f);
    for (const char* t : { "C++", "OpenGL", "Raylib", "BuGUI", "CMake",
                            "Widgets", "Rendering", "Signals", "Layouts" })
        flow->createChild<Button>(t);
}
