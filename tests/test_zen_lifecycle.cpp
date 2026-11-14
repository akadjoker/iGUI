// The zen adapter's window lifecycle, over and over: the application created and destroyed, dialog
// windows opened (modal or not) and closed from their own buttons, a parent closed while its
// dialog is open, the UI scale changed, text drawn at many sizes (the dynamic font page). Real
// windows of zen_plataform (an X server is needed; the test exits 77, skipped, without one). Meant
// for a sanitizer build: a leak (LSan), a use after free, a widget deleted under its own click.
// No standard library.

#include <igui_zen/ZenApp.hpp>

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/ComboBox.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/Theme.hpp>

#include <platform.h>

#include <stdio.h>

using namespace ig::retained;

namespace
{

int gFailures = 0;
void check(bool ok, const char* what)
{
    if (!ok)
    {
        ++gFailures;
        printf("  FAIL %s\n", what);
    }
}

void buildMain(ig::zen::App& zen)
{
    WidgetApp& app = zen.widgets();
    Widget* root = app.addStage("main");
    auto* box = root->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(8.0f);
    box->createChild<Label>("Música e vídeos");
    box->createChild<TextInput>("texto");
    ComboBox* combo = box->createChild<ComboBox>();
    combo->addItem("um");
    combo->addItem("dois");
    box->createChild<Button>("OK");
    app.setStage("main", TransitionType::None);
}

// A dialog window with a label (at a size of its own, so the dynamic page grows) and a close button.
ig::zen::App::Window* openDialog(ig::zen::App& zen, ig::zen::App::Window* parent, bool modal, int n)
{
    ig::zen::App::Window* previous = zen.current();
    // a text size of its own for each dialog: the dynamic font page rasterises it when first drawn
    Theme::instance().fontSize = 11.0f + static_cast<float>(n % 9);
    ig::zen::App::WindowDesc desc;
    desc.title = "Dialog";
    desc.width = 300;
    desc.height = 140;
    desc.dialog = true;
    desc.modal = modal;
    desc.parent = parent;
    ig::zen::App::Window* dialog = zen.openWindow(desc);
    if (!dialog) return nullptr;
    WidgetApp& app = zen.widgets();
    Widget* root = app.addStage("dialog")->createChild<BoxLayout>(LayoutDir::Vertical);
    app.setStage("dialog", TransitionType::None);
    root->createChild<Label>(String("Diálogo ") + String::number(n));
    Button* close = root->createChild<Button>("Close");
    close->clicked.connect([&zen, dialog] { zen.closeWindow(dialog); });
    zen.makeCurrent(previous);
    return dialog;
}

void frames(ig::zen::App& zen, int n)
{
    for (int i = 0; i < n; ++i)
        if (!zen.pump()) break;
}

bool createApp(ig::zen::App& zen, bool openGl)
{
    ig::zen::App::Config config;
    config.title = "lifecycle";
    config.width = 320;
    config.height = 240;
    config.showStats = false;
    config.preferOpenGL = openGl;
    config.vsync = false;
    return zen.create(config);
}

} // namespace

int main()
{
    // The application made and destroyed again and again (the software and the GL path).
    for (int cycle = 0; cycle < 8; ++cycle)
    {
        ig::zen::App zen;
        if (!createApp(zen, cycle % 2 == 0))
        {
            printf("test_zen_lifecycle: no window here, skipped\n");
            return 77;
        }
        buildMain(zen);
        frames(zen, 4);
        zen.setScale(zen.mainWindow(), 1.0f + static_cast<float>(cycle % 3) * 0.5f);
        frames(zen, 3);
    }

    ig::zen::App zen;
    if (!createApp(zen, true))
    {
        printf("test_zen_lifecycle: no window here, skipped\n");
        return 77;
    }
    buildMain(zen);
    frames(zen, 3);
    printf("test_zen_lifecycle: %s drawing, windows %s\n", zen.usesOpenGL() ? "OpenGL" : "software",
            zen.supportsWindows() ? "supported" : "not supported");

    if (zen.supportsWindows())
    {
        // dialogs opened and closed from their own button, modal or not
        for (int i = 0; i < 40; ++i)
        {
            ig::zen::App::Window* d = openDialog(zen, nullptr, i % 2 == 0, i);
            check(d != nullptr, "the dialog opened");
            frames(zen, 2);
            if (d) zen.closeWindow(d);
            frames(zen, 2);
        }
        check(zen.windowCount() == 1, "the dialogs are gone");

        // a parent closed with its dialogs open: they go with it
        for (int i = 0; i < 10; ++i)
        {
            ig::zen::App::Window* parent = openDialog(zen, nullptr, false, i);
            check(parent != nullptr, "the first dialog opened");
            if (!parent) break;
            ig::zen::App::Window* child = openDialog(zen, parent, true, i + 1);
            check(child != nullptr, "the dialog of the dialog opened");
            frames(zen, 2);
            zen.closeWindow(parent);
            frames(zen, 3);
            check(zen.windowCount() == 1, "closing the parent closed the dialog it held open");
        }

        // dialogs open when the application ends: destroyed with it, no leak
        openDialog(zen, nullptr, true, 100);
        openDialog(zen, nullptr, false, 101);
        frames(zen, 2);

        // the UI scale of a window changed while it is shown
        for (int i = 0; i < 6; ++i)
        {
            zen.setScale(zen.mainWindow(), 0.5f + static_cast<float>(i) * 0.5f);
            frames(zen, 2);
        }
        zen.setScale(zen.mainWindow(), 1.0f);
        frames(zen, 2);
    }

    if (gFailures)
    {
        printf("test_zen_lifecycle: %d checks failed\n", gFailures);
        return 1;
    }
    printf("test_zen_lifecycle: ok\n");
    return 0;
}
