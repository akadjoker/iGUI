// The comparison scenes written for Dear ImGui (SDL2 window, OpenGL 3 backend). Outside the no-std rule: bench/compare is
// built only with IGUI_BUILD_COMPARE=ON and holds the code for the other libraries. See cmp_common.h and docs/COMPARACAO.md.
#include "cmp_common.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

struct ImguiLib
{
    SDL_Window* win = nullptr;
    SDL_GLContext gl = nullptr;
    bool vsync = false;
    bool rounded = false; // --rounded: frames rounded by 4 with a 1 px border, as iGUI's shipped theme draws them (ImGui ships both at 0)
    char scene = 'A';
    long long frameCount = 0;
    bool clicked = false, visible = false;
    int treePending = -1; // -1 nothing, 0 close the groups, 1 open them
    bool treeFirst = true;
    char fields[40][32];
    bool checks[40] = {};
    float sliders[40];
    char* editText = nullptr;
    size_t editCap = 0;
    int selected = -1;

    bool init()
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        win = SDL_CreateWindow("cmp", 0, 0, cmp::kW, cmp::kH, SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN);
        if (!win) return false;
        gl = SDL_GL_CreateContext(win);
        if (!gl) return false;
        SDL_GL_SetSwapInterval(vsync ? 1 : 0);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();
        if (rounded)
        {
            ImGuiStyle& st = ImGui::GetStyle();
            st.FrameRounding = st.GrabRounding = st.WindowRounding = 4.0f;
            st.FrameBorderSize = 1.0f;
        }
        ImGui_ImplSDL2_InitForOpenGL(win, gl);
        ImGui_ImplOpenGL3_Init("#version 330");
        return true;
    }

    void build(char s)
    {
        scene = s;
        for (int i = 0; i < 40; ++i)
        {
            char b[48];
            cmp::formText(i * 5 + 4, b, sizeof b);
            snprintf(fields[i], sizeof fields[i], "%.31s", b);
            sliders[i] = 0.3f;
        }
        if (s == 'E')
        {
            size_t len = 0;
            char* t = cmp::makeText(&len);
            editCap = len + 64 * 1024; // room to type: the widget edits a buffer of the application's
            editText = static_cast<char*>(malloc(editCap));
            memcpy(editText, t, len + 1);
            free(t);
        }
    }

    void scenes()
    {
        char buf[96];
        const ImVec2 full(static_cast<float>(cmp::kW), static_cast<float>(cmp::kH));
        const ImGuiWindowFlags frameless = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
        switch (scene)
        {
        case 'A': break;
        case 'F':
            for (int k = 0; k < cmp::kWindows; ++k)
            {
                ImGui::SetNextWindowPos(ImVec2(static_cast<float>(cmp::winX(k)), static_cast<float>(cmp::winY(k))), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(static_cast<float>(cmp::kWinW), static_cast<float>(cmp::kWinH)), ImGuiCond_Always);
                snprintf(buf, sizeof buf, "Window %d", k);
                if (ImGui::Begin(buf, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
                {
                    ImGui::TextUnformatted("A label");
                    ImGui::Button("A button");
                    static bool c[cmp::kWindows];
                    ImGui::Checkbox("A check box", &c[k]);
                }
                ImGui::End();
            }
            break;
        default:
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
            ImGui::SetNextWindowSize(full, ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            const bool open = ImGui::Begin("main", nullptr, frameless);
            ImGui::PopStyleVar(3);
            if (open) body(buf, sizeof buf);
            ImGui::End();
        }
        }
    }

    void body(char* buf, int bn)
    {
        switch (scene)
        {
        case 'B':
            for (int i = 0; i < cmp::kFormCols * cmp::kFormRows; ++i)
            {
                const int r = i / cmp::kFormCols, c = i % cmp::kFormCols;
                ImGui::SetCursorPos(ImVec2(static_cast<float>(cmp::kFormX + c * cmp::kCellW), static_cast<float>(cmp::kFormY + r * cmp::kCellH)));
                ImGui::PushID(i);
                cmp::formText(i, buf, bn);
                const int k = i / 5;
                switch (cmp::formKind(i))
                {
                case 0: ImGui::TextUnformatted(buf); break;
                case 1: ImGui::Button(buf, ImVec2(cmp::kCellW, cmp::kCellH)); break;
                case 2:
                    if (ImGui::Checkbox(buf, &checks[k]) && i == 2) clicked = true;
                    break;
                case 3:
                    ImGui::SetNextItemWidth(cmp::kCellW);
                    ImGui::SliderFloat("##s", &sliders[k], 0.0f, 1.0f, "");
                    break;
                default:
                    ImGui::SetNextItemWidth(cmp::kCellW);
                    ImGui::InputText("##t", fields[k], sizeof fields[k]);
                    break;
                }
                ImGui::PopID();
            }
            break;
        case 'C':
        {
            ImGuiListClipper clipper;
            clipper.Begin(cmp::kRows);
            while (clipper.Step())
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    cmp::rowText(i, buf, bn);
                    if (ImGui::Selectable(buf, selected == i)) selected = i;
                }
            break;
        }
        case 'D':
            for (int g = 0; g < cmp::kTreeGroups; ++g)
            {
                if (treePending >= 0) ImGui::SetNextItemOpen(treePending != 0, ImGuiCond_Always);
                else if (treeFirst) ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                cmp::groupText(g, buf, bn);
                if (ImGui::TreeNodeEx(buf, ImGuiTreeNodeFlags_SpanAvailWidth))
                {
                    for (int k = 0; k < cmp::kTreeKids; ++k)
                    {
                        cmp::kidText(g, k, buf, bn);
                        ImGui::TreeNodeEx(buf, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
                    }
                    ImGui::TreePop();
                }
            }
            treePending = -1;
            treeFirst = false;
            break;
        case 'E':
            ImGui::InputTextMultiline("##e", editText, editCap, ImVec2(static_cast<float>(cmp::kW), static_cast<float>(cmp::kH)), ImGuiInputTextFlags_AllowTabInput);
            break;
        }
    }

    void frame(cmp::FrameStats& fs)
    {
        fs = cmp::FrameStats();
        cmp::Span gui, tot;
        tot.begin();
        gui.begin();
        cmp::regionBegin();
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) ImGui_ImplSDL2_ProcessEvent(&ev);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        scenes();
        ImGui::Render();
        cmp::regionEnd();
        if (!cmp::icountOn())
        {
            fs.guiUs = gui.cpuUs();
            fs.guiAllocs = gui.allocs();
            fs.guiBytes = gui.bytes();
            ImDrawData* dd = ImGui::GetDrawData();
            int calls = 0;
            for (int n = 0; n < dd->CmdListsCount; ++n) calls += dd->CmdLists[n]->CmdBuffer.Size;
            fs.drawCalls = calls;
            fs.vertices = dd->TotalVtxCount;
            fs.indices = dd->TotalIdxCount;
        }
        glViewport(0, 0, cmp::kW, cmp::kH);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(win);
        ++frameCount;
        if (clicked) visible = true;
        if (!cmp::icountOn())
        {
            fs.totalUs = tot.cpuUs();
            fs.wallUs = tot.wallUs();
            fs.totalAllocs = tot.allocs();
        }
    }
    void treeOp(bool open) { treePending = open ? 1 : 0; }
    void natural(int seconds, long long& frames)
    {
        const double end = cmp::wallMs() + seconds * 1000.0;
        while (cmp::wallMs() < end)
        {
            cmp::FrameStats fs;
            frame(fs);
            ++frames;
        }
    }
    unsigned long long drawHash() { return 0; }
    bool changed() const { return visible; }
    void resetChanged() { clicked = visible = false; }
    long long frames() const { return frameCount; }
    void pumpOnce()
    {
        cmp::FrameStats fs;
        frame(fs);
    }
};

int main(int argc, char** argv)
{
    cmp::t0Wall();
    cmp::t0Cpu();
    ImguiLib lib;
    lib.vsync = argc > 2 && !strcmp(argv[2], "idlecpu");
    for (int i = 3; i < argc; ++i)
        if (!strcmp(argv[i], "--rounded")) lib.rounded = true;
    return cmp::run(lib, argc, argv);
}
