// Retained widget gallery on zen_plataform, drawn by the software rasteriser.
// Usage: igui_zen_widgets [stage] [samples-per-axis]
// Keys: F2 cycles the anti-aliasing (1x, 2x, 3x, 4x per axis).

#include <igui_zen/ZenApp.hpp>

#include <stdlib.h>

void registerDemoStage(ig::retained::WidgetApp &app);
void registerMenuStage(ig::retained::WidgetApp &app);
void registerBasicStage(ig::retained::WidgetApp &app);
void registerControlsStage(ig::retained::WidgetApp &app);
void registerScrollStage(ig::retained::WidgetApp &app);
void registerInputsStage(ig::retained::WidgetApp &app);
void registerMenusStage(ig::retained::WidgetApp &app);
void registerDialogsStage(ig::retained::WidgetApp &app);
void registerDockStage(ig::retained::WidgetApp &app);
void registerPropertiesStage(ig::retained::WidgetApp &app);
void registerEditorStage(ig::retained::WidgetApp &app);
void registerNodeStage(ig::retained::WidgetApp &app);
void registerTimelineStage(ig::retained::WidgetApp &app);
void registerGizmosStage(ig::retained::WidgetApp &app);
void registerToolsStage(ig::retained::WidgetApp &app);
void registerGalleryStage(ig::retained::WidgetApp &app);
void registerSpecialtyStage(ig::retained::WidgetApp &app);
void registerVideoStage(ig::retained::WidgetApp &app);

int main(int argc, char **argv)
{
    ig::zen::App::Config config;
    config.title = "iGUI on zen_plataform";
    if (argc > 2)
        config.samples = atoi(argv[2]);

    ig::zen::App zen;
    if (!zen.create(config))
        return 1;

    ig::retained::WidgetApp &app = zen.widgets();
    registerDemoStage(app);
    registerMenuStage(app);
    registerBasicStage(app);
    registerControlsStage(app);
    registerScrollStage(app);
    registerInputsStage(app);
    registerMenusStage(app);
    registerDialogsStage(app);
    registerDockStage(app);
    registerPropertiesStage(app);
    registerEditorStage(app);
    registerNodeStage(app);
    registerTimelineStage(app);
    registerGizmosStage(app);
    registerToolsStage(app);
    registerGalleryStage(app);
    registerSpecialtyStage(app);
    registerVideoStage(app);
    app.setStage(argc > 1 ? argv[1] : "menu");

    zen.run();
    return 0;
}
