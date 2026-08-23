#include "WidgetApp.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "ScrollWidgets.hpp"
#include <string>
using namespace BuGUI;

void registerScrollStage(WidgetApp& app)
{
    auto* root  = app.addStage("scroll");
    auto* outer = root->createChild<BoxLayout>(LayoutDir::Vertical);
    outer->setPadding(14.0f);
    outer->setSpacing(10.0f);
    outer->setStretch(1);

    auto* back = outer->createChild<Button>("\u2190 Menu");
    back->clicked.connect([&app]() { app.setStage("menu", TransitionType::CoverRight); });
    outer->createChild<Line>();

    auto* status = outer->createChild<Label>("Scroll, ListBox, ListWidget");

    auto* row = outer->createChild<BoxLayout>(LayoutDir::Horizontal);
    row->setSpacing(10.0f);
    row->setStretch(1);

    // ── ScrollView ───────────────────────────────────────────────────────────
    auto* col1 = row->createChild<Panel>();
    col1->setStretch(1);
    auto* c1 = col1->createChild<BoxLayout>(LayoutDir::Vertical);
    c1->setPadding(8.0f); c1->setSpacing(4.0f); c1->setStretch(1);
    c1->createChild<Label>("ScrollView");
    auto* sv = c1->createChild<ScrollView>();
    sv->setStretch(1);
    auto* longCol = sv->setContent<BoxLayout>(LayoutDir::Vertical);
    longCol->setPadding(8.0f); longCol->setSpacing(4.0f);
    for (int i = 0; i < 60; ++i)
        longCol->createChild<Label>("Log line " + std::to_string(i + 1));

    // ── ListBox ──────────────────────────────────────────────────────────────
    auto* col2 = row->createChild<Panel>();
    col2->setStretch(1);
    auto* c2 = col2->createChild<BoxLayout>(LayoutDir::Vertical);
    c2->setPadding(8.0f); c2->setSpacing(4.0f); c2->setStretch(1);
    c2->createChild<Label>("ListBox");
    auto* lb = c2->createChild<ListBox>();
    lb->setStretch(1);
    for (int i = 0; i < 30; ++i)
        lb->addItem("Asset_" + std::to_string(i + 1));
    lb->selectionChanged.connect([status, lb](int idx) {
        if (idx >= 0) status->setText("Selected: " + lb->itemText(idx));
    });

    // ── ListWidget ───────────────────────────────────────────────────────────
    auto* col3 = row->createChild<Panel>();
    col3->setStretch(1);
    auto* c3 = col3->createChild<BoxLayout>(LayoutDir::Vertical);
    c3->setPadding(8.0f); c3->setSpacing(4.0f); c3->setStretch(1);
    c3->createChild<Label>("ListWidget");
    auto* lw = c3->createChild<ListWidget>();
    lw->setStretch(1);
    for (int i = 0; i < 20; ++i) {
        auto* itemRow = lw->addRow<BoxLayout>(LayoutDir::Horizontal);
        itemRow->setSpacing(8.0f);
        itemRow->createChild<Label>("Object " + std::to_string(i + 1));
        itemRow->createChild<Button>("Edit");
    }
    lw->selectionChanged.connect([status](int idx) {
        if (idx >= 0) status->setText("ListWidget row: " + std::to_string(idx));
    });
}
