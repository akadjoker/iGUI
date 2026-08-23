#include "WidgetApp.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "InputWidgets.hpp"
#include "TextInputWidgets.hpp"
#include <cstdio>
using namespace BuGUI;

void registerInputsStage(WidgetApp& app)
{
    auto* root = app.addStage("inputs");
    auto* vbox = root->createChild<BoxLayout>(LayoutDir::Vertical);
    vbox->setSpacing(12.0f);
    vbox->setPadding(16.0f);

    auto* back = vbox->createChild<Button>("\u2190 Menu");
    back->clicked.connect([&app]() { app.setStage("menu", TransitionType::CoverRight); });
    vbox->createChild<Line>();

    // ── Sliders + ProgressBar ─────────────────────────────────────────────────
    vbox->createChild<Label>("Horizontal Slider → ProgressBar");
    auto* sliderH = vbox->createChild<Slider>(0.0f, 100.0f, 30.0f);
    sliderH->setStretch(1.0f);
    auto* pb = vbox->createChild<ProgressBar>(0.0f, 100.0f, 30.0f);
    pb->setText("30%");
    pb->setStretch(1.0f);
    sliderH->onValueChanged.connect([pb](float v) {
        pb->setValue(v);
        char buf[16]; std::snprintf(buf, sizeof(buf), "%.0f%%", v);
        pb->setText(buf);
    });

    vbox->createChild<Spacer>(8.0f);
    vbox->createChild<Label>("Vertical Sliders");
    auto* hbox = vbox->createChild<BoxLayout>(LayoutDir::Horizontal);
    hbox->setSpacing(12.0f); hbox->setStretch(1.0f);
    for (int i = 0; i < 5; ++i) {
        auto* s = hbox->createChild<Slider>(0.0f, 100.0f, float(i * 20));
        s->setOrientation(LayoutDir::Vertical);
        s->setStretch(1.0f);
    }

    // ── Text inputs ───────────────────────────────────────────────────────────
    vbox->createChild<Spacer>(8.0f);
    vbox->createChild<Label>("Text Inputs");
    auto* form = vbox->createChild<FormLayout>();
    form->setSpacing(10.0f, 6.0f); form->setPadding(4.0f);
    auto* ti1 = form->addRow<TextInput>("Name:", "");
    auto* ti2 = form->addRow<TextInput>("Email:", "");
    ti1->setPlaceholder("Enter name…");
    ti2->setPlaceholder("Enter email…");

    auto* result = vbox->createChild<Label>("");
    auto* submit = vbox->createChild<Button>("Submit");
    submit->clicked.connect([ti1, ti2, result]() {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Name: %s | Email: %s",
            ti1->text().c_str(), ti2->text().c_str());
        result->setText(buf);
    });
}
