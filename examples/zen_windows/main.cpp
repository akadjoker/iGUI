// Several top-level windows on zen_plataform: the main window has a button
// that opens a modal dialog window; the dialog has a label and a Close
// button. While the dialog is open the main window takes no input.
// Usage: igui_zen_windows [--dialog]   (--dialog opens the dialog at start)
#include <igui_zen/ZenApp.hpp>
#include <Widgets.hpp>

#include <cstring>
#include <stdio.h>

namespace rt = ig::retained;

static void openDialog(ig::zen::App &zen, int n)
{
    ig::zen::App::Window *previous = zen.current();
    ig::zen::App::WindowDesc desc;
    desc.title = "Dialog";
    desc.width = 360;
    desc.height = 160;
    desc.dialog = true;
    desc.modal = true;
    ig::zen::App::Window *dialog = zen.openWindow(desc);
    if (!dialog)
    {
        fprintf(stderr, "no windows here; a dialog would be a panel in the main window\n");
        return;
    }
    // openWindow made the dialog current: widgets() builds its tree.
    rt::WidgetApp &app = zen.widgets();
    rt::Widget *root = app.addStage("dialog")->createChild<rt::Widget>();
    app.setStage("dialog");
    char text[64];
    snprintf(text, sizeof text, "Dialog number %d. The main window waits.", n);
    auto *label = root->createChild<rt::Label>(text);
    label->setPosition(16, 24);
    label->setSize(328, 24);
    auto *close = root->createChild<rt::Button>("Close");
    close->setPosition(130, 90);
    close->setSize(100, 36);
    close->clicked.connect([&zen, dialog]() { zen.closeWindow(dialog); });
    zen.makeCurrent(previous);
}

int main(int argc, char **argv)
{
    ig::zen::App::Config config;
    config.title = "iGUI windows";
    config.width = 480;
    config.height = 320;
    config.showStats = false;

    ig::zen::App zen;
    if (!zen.create(config))
        return 1;

    rt::WidgetApp &app = zen.widgets();
    rt::Widget *root = app.addStage("main")->createChild<rt::Widget>();
    app.setStage("main");
    auto *label = root->createChild<rt::Label>("Main window");
    label->setPosition(16, 16);
    label->setSize(200, 24);
    auto *edit = root->createChild<rt::TextInput>("type here");
    edit->setPosition(16, 56);
    edit->setSize(300, 32);
    auto *open = root->createChild<rt::Button>("Open a modal dialog");
    open->setPosition(16, 110);
    open->setSize(220, 40);
    static int count = 0;
    open->clicked.connect([&zen]() { openDialog(zen, ++count); });

    if (argc > 1 && strcmp(argv[1], "--dialog") == 0)
        openDialog(zen, ++count);

    zen.run();
    return 0;
}
