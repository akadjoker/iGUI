#include "WidgetApp.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "DialogWidgets.hpp"
#include <cstdio>
using namespace BuGUI;

static Label* s_dlgStatus = nullptr;

void registerDialogsStage(WidgetApp& app)
{
    s_dlgStatus = nullptr;

    auto* root = app.addStage("dialogs");
    auto* vbox = root->createChild<BoxLayout>(LayoutDir::Vertical);
    vbox->setSpacing(0.0f);
    vbox->setPadding(0.0f);

    // ── Top bar ───────────────────────────────────────────────────────────
    auto* topBar = vbox->createChild<BoxLayout>(LayoutDir::Horizontal);
    topBar->setSpacing(8.0f);
    topBar->setPadding(Edges(8.0f, 12.0f));
    auto* back = topBar->createChild<Button>("\u2190 Menu");
    back->clicked.connect([&app]() { app.setStage("menu", TransitionType::CoverRight); });
    topBar->createChild<Spacer>(0.0f)->setStretch(1.0f);
    topBar->createChild<Label>("Dialogs & Toasts")->setAlign(TextAlign::RIGHT);
    vbox->createChild<Line>();

    auto* content = vbox->createChild<BoxLayout>(LayoutDir::Vertical);
    content->setSpacing(14.0f);
    content->setPadding(24.0f);
    content->setStretch(1.0f);

    // ── Dialogs ───────────────────────────────────────────────────────────
    content->createChild<Label>("Modal Dialogs");
    content->createChild<Line>();

    auto* row1 = content->createChild<BoxLayout>(LayoutDir::Horizontal);
    row1->setSpacing(10.0f);

    row1->createChild<Button>("Alert")->clicked.connect([]() {
        AlertDialog::show("Information",
            "The operation completed successfully.\nAll files were processed.",
            []() { if (s_dlgStatus) s_dlgStatus->setText("Alert closed."); });
    });

    row1->createChild<Button>("Confirm")->clicked.connect([]() {
        ConfirmDialog::show("Delete File",
            "Are you sure you want to delete \"document.txt\"?\nThis cannot be undone.",
            []() { if (s_dlgStatus) s_dlgStatus->setText("Confirmed: deleted."); },
            []() { if (s_dlgStatus) s_dlgStatus->setText("Cancelled."); });
    });

    row1->createChild<Button>("Danger")->clicked.connect([]() {
        auto* dlg = new Dialog("Format Drive",
            "You are about to erase ALL data.\nThis CANNOT be undone.");
        dlg->addButton("Cancel",  Dialog::Role::Cancel, []() { if (s_dlgStatus) s_dlgStatus->setText("Cancelled."); });
        dlg->addButton("Format!", Dialog::Role::Danger, []() { if (s_dlgStatus) s_dlgStatus->setText("Drive formatted!"); });
        dlg->show();
    });

    row1->createChild<Button>("Save Changes?")->clicked.connect([]() {
        auto* dlg = new Dialog("Unsaved Changes",
            "You have unsaved changes.\nWhat would you like to do?");
        dlg->addButton("Discard", Dialog::Role::Danger, []() { if (s_dlgStatus) s_dlgStatus->setText("Changes discarded."); });
        dlg->addButton("Cancel",  Dialog::Role::Cancel, []() { if (s_dlgStatus) s_dlgStatus->setText("Cancelled."); });
        dlg->addButton("Save",    Dialog::Role::Accept, []() { if (s_dlgStatus) s_dlgStatus->setText("Saved!"); });
        dlg->show();
    });

    content->createChild<Spacer>(8.0f);

    // ── Toasts ────────────────────────────────────────────────────────────
    content->createChild<Label>("Toasts / Notifications");
    content->createChild<Line>();

    auto* row2 = content->createChild<BoxLayout>(LayoutDir::Horizontal);
    row2->setSpacing(10.0f);

    row2->createChild<Button>("Info")->clicked.connect([]() {
        Toast::show("Loading complete — 42 items imported.");
        if (s_dlgStatus) s_dlgStatus->setText("Toast: Info shown.");
    });
    row2->createChild<Button>("Success")->clicked.connect([]() {
        Toast::show("File saved successfully!", Toast::Type::Success);
        if (s_dlgStatus) s_dlgStatus->setText("Toast: Success shown.");
    });
    row2->createChild<Button>("Warning")->clicked.connect([]() {
        Toast::show("Disk space is running low.", Toast::Type::Warning, 4.0f);
        if (s_dlgStatus) s_dlgStatus->setText("Toast: Warning shown.");
    });
    row2->createChild<Button>("Error")->clicked.connect([]() {
        Toast::show("Connection failed. Please check your network.", Toast::Type::Error, 5.0f);
        if (s_dlgStatus) s_dlgStatus->setText("Toast: Error shown.");
    });

    content->createChild<Spacer>(0.0f)->setStretch(1.0f);
    vbox->createChild<Line>();

    auto* statusRow = vbox->createChild<BoxLayout>(LayoutDir::Horizontal);
    statusRow->setSpacing(0.0f);
    statusRow->setPadding(Edges(5.0f, 12.0f));
    s_dlgStatus = statusRow->createChild<Label>("Last action: \u2014");
}
