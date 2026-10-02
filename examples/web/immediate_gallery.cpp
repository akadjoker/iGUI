#include <SDL.h>

#include <igui/Gui.hpp>
#include <igui/CodeEditor.hpp>
#include <igui_sdl2/SdlBackend.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include <cstdio>

namespace
{
const ig::StringView kSections[] = {
    ig::StringView("Basic"), ig::StringView("Inputs"), ig::StringView("Lists"),
    ig::StringView("Layout"), ig::StringView("Windows"), ig::StringView("Code")
};
const ig::StringView kQuality[] = {
    ig::StringView("low"), ig::StringView("medium"), ig::StringView("high")
};
const ig::StringView kFruit[] = {
    ig::StringView("apple"), ig::StringView("banana"), ig::StringView("cherry"),
    ig::StringView("grape"), ig::StringView("lemon"), ig::StringView("mango"),
    ig::StringView("orange"), ig::StringView("peach")
};

const char *kSample =
    "#include <cstdio>\n"
    "\n"
    "// iGUI immediate mode code editor\n"
    "int main()\n"
    "{\n"
    "    for (int i = 0; i < 3; ++i)\n"
    "    {\n"
    "        std::printf(\"hello %d\\n\", i);\n"
    "    }\n"
    "    return 0;\n"
    "}\n";

ig::String numbered(const char *prefix, int value, const char *suffix = "")
{
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%s%d%s", prefix, value, suffix);
    return ig::String(buffer);
}

struct Gallery
{
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    ig::sdl2::Backend *backend = nullptr;
    ig::Context *ui = nullptr;
    uint64_t previousCounter = 0;
    uint64_t frequency = 1;
    bool running = true;

    int section = 0;
    bool enabled = true;
    bool toggled = false;
    bool firstChoice = true;
    int selectedEntry = 0;
    float volume = 0.5f;
    int count = 8;
    float dragValue = 1.0f;
    int quality = 1;
    int listChoice = 2;
    ig::String name = "player";
    ig::String notes = "multi line\ntext field";
    int integer = 42;
    float number = 3.14f;
    float vec3[3] = {1.0f, 2.0f, 3.0f};
    ig::Color color = ig::Color(90, 160, 240, 255);
    bool headerOpen = true;
    bool treeOpen = true;
    bool dialogOpen = false;
    bool floatingOpen = false;
    int sortColumn = -1;
    bool sortAscending = true;
    int firstRow = 0;
    int lastRow = 0;
    ig::CodeEditorState code;
};

Gallery g;

void basicSection(ig::Context &ui)
{
    ui.separatorText("Buttons");
    if (ui.button("button"))
        ui.showToast("clicked", "button clicked");
    ui.sameLine();
    ui.smallButton("small");
    ui.sameLine();
    ui.checkbox("checkbox", g.enabled);
    ui.sameLine();
    ui.toggleSwitch("switch", g.toggled);

    ui.separatorText("Choice");
    if (ui.radioButton("choice one", g.firstChoice))
        g.firstChoice = true;
    ui.sameLine();
    if (ui.radioButton("choice two", !g.firstChoice))
        g.firstChoice = false;
    for (int i = 0; i < 3; ++i)
    {
        if (ui.selectable(numbered("entry ", i + 1), g.selectedEntry == i, 220.0f))
            g.selectedEntry = i;
    }

    ui.separatorText("Progress");
    ui.progressBar(g.volume, 1.0f, 260.0f);
    ui.sliderFloat("volume", g.volume, 0.0f, 1.0f, 260.0f);

    ui.separatorText("Disclosure");
    if (ui.collapsingHeader("collapsing header", g.headerOpen, 260.0f))
    {
        ui.indent();
        ui.label("content inside the header");
        ui.unindent();
    }
    if (ui.treeNode("tree node", g.treeOpen, 260.0f))
    {
        ui.indent();
        ui.label("child one");
        ui.label("child two");
        ui.unindent();
    }
}

void inputsSection(ig::Context &ui)
{
    ui.separatorText("Sliders and drags");
    ui.sliderFloat("slider float", g.volume, 0.0f, 1.0f, 280.0f);
    ui.sliderInt("slider int", g.count, 0, 20, 280.0f);
    ui.dragFloat("drag float", g.dragValue, 0.0f, 10.0f, 0.02f, 280.0f);
    ui.dragFloat3("drag float3", g.vec3[0], g.vec3[1], g.vec3[2], -10.0f, 10.0f, 0.05f, 280.0f);
    ui.stepperInt("stepper", g.count, 0, 20, 160.0f);

    ui.separatorText("Text and numbers");
    ui.label("text");
    ui.inputText("name", g.name, 280.0f);
    ui.label("integer");
    ui.inputInt("integer", g.integer, 280.0f);
    ui.label("float");
    ui.inputFloat("float", g.number, 280.0f, 3);
    ui.label("multi line");
    ui.inputTextMultiline("notes", g.notes, 280.0f, 90.0f);

    ui.separatorText("Color");
    ui.colorEdit("color", g.color, 280.0f, 128.0f);
}

void listsSection(ig::Context &ui)
{
    ui.separatorText("Selection");
    ui.label("combo box");
    ui.comboBox("quality", g.quality, ig::Span<const ig::StringView>(kQuality), 200.0f);
    ui.label("list box");
    ui.listBox("fruit", g.listChoice, ig::Span<const ig::StringView>(kFruit), 200.0f, 5);

    ui.separatorText("Table");
    if (ui.beginTable("table", 3, 420.0f))
    {
        ui.tableNextColumn();
        ui.tableHeader("name", g.sortColumn, g.sortAscending);
        ui.tableNextColumn();
        ui.tableHeader("kind", g.sortColumn, g.sortAscending);
        ui.tableNextColumn();
        ui.tableHeader("size", g.sortColumn, g.sortAscending);
        for (int row = 0; row < 4; ++row)
        {
            ui.tableNextColumn();
            ui.label(kFruit[row]);
            ui.tableNextColumn();
            ui.label("fruit");
            ui.tableNextColumn();
            ui.label(numbered("1", row, " g"));
        }
        ui.endTable();
    }

    ui.separatorText("Virtual list, 10000 items");
    if (ui.beginVirtualList("virtual", 10000, 20.0f, 160.0f, g.firstRow, g.lastRow, true, 300.0f))
    {
        for (int i = g.firstRow; i < g.lastRow; ++i)
        {
            ui.selectable(numbered("item ", i), false, 0.0f);
        }
        ui.endVirtualList();
    }
}

void layoutSection(ig::Context &ui)
{
    ui.separatorText("Child region");
    if (ui.beginChild("child", 110.0f, true, 320.0f))
    {
        for (int i = 0; i < 8; ++i)
            ui.label(numbered("scrolling line ", i));
        ui.endChild();
    }

    ui.separatorText("Property rows");
    if (ui.beginPropertyRow("position"))
    {
        ui.dragFloat3("##pos", g.vec3[0], g.vec3[1], g.vec3[2], -10.0f, 10.0f, 0.05f, 220.0f);
        ui.endPropertyRow();
    }
    if (ui.beginPropertyRow("enabled"))
    {
        ui.checkbox("##enabled", g.enabled);
        ui.endPropertyRow();
    }

    ui.separatorText("Spacing");
    ui.label("indent");
    ui.indent(24.0f);
    ui.label("indented text");
    ui.unindent(24.0f);
    ui.spacing(12.0f);
    ui.label("after spacing");
}

void windowsSection(ig::Context &ui)
{
    ui.separatorText("Dialogs and toasts");
    if (ui.button("open dialog"))
        g.dialogOpen = true;
    ui.sameLine();
    if (ui.button("show toast"))
        ui.showToast("demo", "this is a toast", ig::ToastPosition::BottomRight, 2.5f);
    ui.sameLine();
    if (ui.button("floating window"))
        g.floatingOpen = true;
}

void codeSection(ig::Context &ui, const ig::Rect &bounds)
{
    ui.codeEditor("code", g.code, bounds);
}

void frame(void *)
{
    ig::Context &ui = *g.ui;

#ifdef __EMSCRIPTEN__
    double cssWidth = 0.0;
    double cssHeight = 0.0;
    emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(g.window, &windowWidth, &windowHeight);
    if (cssWidth > 0.0 && cssHeight > 0.0 &&
        (windowWidth != static_cast<int>(cssWidth) || windowHeight != static_cast<int>(cssHeight)))
        SDL_SetWindowSize(g.window, static_cast<int>(cssWidth), static_cast<int>(cssHeight));
#endif

    const uint64_t currentCounter = SDL_GetPerformanceCounter();
    const float deltaSeconds = static_cast<float>(currentCounter - g.previousCounter) /
                               static_cast<float>(g.frequency);
    g.previousCounter = currentCounter;

    SDL_Event nativeEvent;
    while (SDL_PollEvent(&nativeEvent))
    {
        if (nativeEvent.type == SDL_QUIT)
            g.running = false;
        ig::Event event;
        if (ig::sdl2::translateEvent(nativeEvent, event))
            ui.pushEvent(event);
    }

    ui.beginFrame(ig::sdl2::frameInfo(g.window, deltaSeconds));
    if (ui.beginMainWindow("iGUI immediate mode"))
    {
        ui.tabBar("sections", g.section, ig::Span<const ig::StringView>(kSections));
        ui.spacing(6.0f);
        ui.indent(10.0f);
        switch (g.section)
        {
            case 0: basicSection(ui); break;
            case 1: inputsSection(ui); break;
            case 2: listsSection(ui); break;
            case 3: layoutSection(ui); break;
            case 4: windowsSection(ui); break;
            default:
            {
                int width = 0;
                int height = 0;
                SDL_GetWindowSize(g.window, &width, &height);
                codeSection(ui, ig::Rect(12.0f, 40.0f, static_cast<float>(width) - 24.0f,
                                         static_cast<float>(height) - 56.0f));
                break;
            }
        }
        ui.unindent(10.0f);
        ui.endWindow();
    }

    if (g.dialogOpen && ui.beginDialog("Dialog", g.dialogOpen, ig::Vec2(320.0f, 150.0f)))
    {
        ui.label("A modal dialog.");
        ui.spacing(8.0f);
        if (ui.button("close"))
            g.dialogOpen = false;
        ui.endDialog();
    }

    if (g.floatingOpen &&
        ui.beginWindow("Floating window", ig::Rect(520.0f, 140.0f, 280.0f, 180.0f), &g.floatingOpen))
    {
        ui.label("Drag me by the title bar.");
        ui.checkbox("enabled", g.enabled);
        ui.sliderFloat("value", g.volume, 0.0f, 1.0f, 200.0f);
        ui.endWindow();
    }

    const ig::DrawData &drawData = ui.endFrame();

    if (ui.wantsTextInput())
        SDL_StartTextInput();
    else
        SDL_StopTextInput();

    SDL_SetRenderDrawColor(g.renderer, 32u, 34u, 40u, 255u);
    SDL_RenderClear(g.renderer);
    if (!g.backend->render(drawData))
        g.running = false;
    SDL_RenderPresent(g.renderer);
}
}

int main(int, char **)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;
    g.window = SDL_CreateWindow("iGUI immediate mode", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                960, 640, SDL_WINDOW_RESIZABLE);
    if (!g.window)
        return 2;
    g.renderer = SDL_CreateRenderer(g.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g.renderer)
        return 3;

    g.backend = new ig::sdl2::Backend(g.renderer);
    if (!g.backend->prepareFontAtlas())
        return 4;
    g.ui = new ig::Context(*g.backend, &g.backend->fontAtlas());
    g.code.setHighlighterForFile("sample.cpp");
    g.code.setText(kSample);
    g.frequency = SDL_GetPerformanceFrequency();
    g.previousCounter = SDL_GetPerformanceCounter();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(frame, nullptr, 0, 1);
#else
    while (g.running)
        frame(nullptr);
#endif

    delete g.ui;
    delete g.backend;
    SDL_DestroyRenderer(g.renderer);
    SDL_DestroyWindow(g.window);
    SDL_Quit();
    return 0;
}
