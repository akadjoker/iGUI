#include <raylib.h>

#include <igui/Gui.hpp>
#include <igui_raylib/RaylibBackend.hpp>

int main()
{
    InitWindow(960, 640, "iGUI Raylib demo");
    SetTargetFPS(60);

    ig::raylib::Backend backend;
    if (!backend.prepareFontAtlas())
    {
        CloseWindow();
        return 1;
    }

    ig::Context ui(backend, &backend.fontAtlas());
    bool enabled = false;
    float volume = 0.5f;
    ig::String name("player");

    while (!WindowShouldClose())
    {
        ig::raylib::processInput(ui);
        ui.beginFrame(ig::raylib::frameInfo(GetFrameTime()));
        if (ui.beginWindow("iGUI + Raylib", ig::Rect(80.0f, 60.0f, 400.0f, 260.0f)))
        {
            ui.label(u8"Olá, Raylib");
            if (ui.button("toggle"))
                enabled = !enabled;
            ui.checkbox("enabled", enabled);
            ui.sliderFloat("volume", volume, 0.0f, 1.0f);
            ui.inputText("name", name);
            ui.progressBar(volume, 1.0f);
            ui.endWindow();
        }
        const ig::DrawData &drawData = ui.endFrame();

        BeginDrawing();
        ClearBackground(::Color{32u, 34u, 40u, 255u});
        backend.render(drawData);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
