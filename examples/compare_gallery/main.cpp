// O demo do ImGui de um lado, o mesmo demo reescrito com o iGUI do outro, na
// mesma janela SDL2 com contexto OpenGL 3.3.
//
//   painel esquerdo: iGUI   (examples/compare_gallery/GalleryIgui.cpp)
//   painel direito:  ImGui  (ImGui::ShowDemoWindow(), o demo verdadeiro)
//
// Cada lado desenha para um alvo fora do ecrã do tamanho do seu painel (o iGUI
// pelo backend igui::opengl, o ImGui pelo backend dele) e o adaptador copia os
// dois para a janela - assim nenhum dos lados sabe onde está.
//
// Por lado, a cada frame:
//   submit  CPU a construir o frame (beginFrame..endFrame / NewFrame..Render)
//   render  CPU a submeter os draw calls ao GL
//   gpu     tempo de GPU dos draw calls, com uma query GL_TIME_ELAPSED
// A leitura de eventos e as cópias dos alvos para a janela ficam fora das três
// (a cópia é trabalho do adaptador, não da biblioteca), e o painel de números
// entra nos dois lados igual, porque é o mesmo trabalho em ambas as bibliotecas.
//
// Como ler os números da janela: com uma cena pequena (o demo todo fechado) o
// custo fixo de cada frame domina - trocar de alvo, limpar, copiar os dois
// painéis para a janela, e o sincronismo que o driver faz por causa dessas
// cópias, que aparece no `render` do frame seguinte. Para comparar as duas
// bibliotecas com precisão há o bench headless (bench/), que mede só o trabalho
// delas e em tempo de CPU.
//
//   compare_gallery [--width=N] [--height=N] [--frames=N] [--hidden]
//                   [--screenshot=ficheiro.png] [--no-vsync]
//
// Com --frames=N corre N frames, escreve o resumo e sai; --hidden corre sem
// janela visível. Sem --frames fica aberto até se fechar a janela - que é como se
// testa à mão.
//
// Para pôr uma só secção do demo de cada vez lado a lado (sem andar à roda dentro
// do painel) o --only=SECCAO desenha só essa, no topo e sem o header dela; o
// --open=SECCAO (e o --open-all, para o levantamento completo) só a abre, com as
// outras todas por cima.
//
// O levantamento do que o demo tem e o iGUI não exprime (e o TODO da paridade)
// está em GAPS.md, ao lado deste ficheiro.

#include <SDL.h>

#include <stdio.h>
#include <vector>

#include <igui/Gui.hpp>
#include <igui_opengl/OpenglBackend.hpp>
#include <igui_sdl2/SdlBackend.hpp>

#include "Gallery.hpp"
#include "IguiGallery.hpp"
#include "ImguiGallery.hpp"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(CLOCK_PROCESS_CPUTIME_ID)
#  include <time.h>
#endif

namespace compare
{
bool writePng(const char *path, int width, int height, const unsigned char *rgba);
}

namespace
{

// Tempo de CPU, não de parede: numa máquina ocupada o tempo de parede mediria a
// ocupação e não a biblioteca.
double cpuNowUs()
{
#if defined(CLOCK_PROCESS_CPUTIME_ID)
    struct timespec stamp;
    if (::clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stamp) == 0)
        return static_cast<double>(stamp.tv_sec) * 1000000.0 +
               static_cast<double>(stamp.tv_nsec) * 0.001;
#endif
    return static_cast<double>(SDL_GetTicks()) * 1000.0;
}

struct Samples
{
    std::vector<double> values;
    double last;

    Samples() : values(), last(0.0) {}

    void add(double value)
    {
        last = value;
        values.push_back(value);
        if (values.size() > 600u)
            values.erase(values.begin());
    }

    double mean() const
    {
        if (values.empty())
            return 0.0;
        double total = 0.0;
        for (size_t i = 0; i < values.size(); ++i)
            total += values[i];
        return total / static_cast<double>(values.size());
    }

    double percentile(double fraction) const
    {
        if (values.empty())
            return 0.0;
        std::vector<double> sorted(values);
        std::sort(sorted.begin(), sorted.end());
        const size_t rank =
            static_cast<size_t>(std::ceil(fraction * static_cast<double>(sorted.size())));
        const size_t index = rank == 0 ? 0 : std::min(rank - 1, sorted.size() - 1);
        return sorted[index];
    }

    void clear() { values.clear(); }
};

struct PanelTiming
{
    Samples submit; // microssegundos
    Samples render; // microssegundos
    Samples gpu;    // microssegundos
    std::vector<float> history; // submit em ms, para o gráfico (64 amostras)
    int items;
    int vertices;
    int drawCalls;

    PanelTiming() : submit(), render(), gpu(), history(), items(0), vertices(0), drawCalls(0) {}

    void push(double submitUs, double renderUs, double gpuUs)
    {
        submit.add(submitUs);
        render.add(renderUs);
        if (gpuUs > 0.0)
            gpu.add(gpuUs);
        history.push_back(static_cast<float>(submitUs * 0.001));
        if (history.size() > 64u)
            history.erase(history.begin());
    }

    double hz() const
    {
        const double us = submit.mean() + render.mean();
        return us > 0.0 ? 1000000.0 / us : 0.0;
    }
};

void usage()
{
    printf("usage: compare_gallery [--width=N] [--height=N] [--frames=N] [--hidden]\n"
           "                       [--screenshot=FILE] [--no-vsync] [--font=FILE] [--dump]\n"
           "                       [--open-all | --open=SECCAO] [--click=X,Y] [--hover=X,Y]\n"
           "                       [--wheel=N] [--help]\n"
           "  SECCAO e o nome de uma seccao do demo: 'Combo', 'Basic', 'Tables & Columns'.\n"
           "  --click=X,Y carrega no painel do iGUI nesse ponto (para os popups, que\n"
           "  nao abrem sem rato, aparecerem numa captura), o --hover=X,Y poe o\n"
           "  ponteiro nesse ponto sem clicar (e o que abre um submenu, que abre por\n"
           "  hover) e o --wheel=N manda N entalhes de roda nesse mesmo ponto.\n");
}

// O --click=X,Y: onde carregar uma vez, no painel do iGUI.
bool parsePoint(const char *arg, const char *prefix, float &x, float &y)
{
    const size_t length = strlen(prefix);
    if (strncmp(arg, prefix, length) != 0)
        return false;
    const char *comma = strchr(arg + length, ',');
    if (!comma)
        return false;
    x = static_cast<float>(atof(arg + length));
    y = static_cast<float>(atof(comma + 1));
    return true;
}

bool parseClick(const char *arg, float &x, float &y)
{
    return parsePoint(arg, "--click=", x, y);
}

bool isMouseEvent(Uint32 type)
{
    return type == SDL_MOUSEMOTION || type == SDL_MOUSEBUTTONDOWN || type == SDL_MOUSEBUTTONUP ||
           type == SDL_MOUSEWHEEL;
}

bool isKeyEvent(Uint32 type)
{
    return type == SDL_KEYDOWN || type == SDL_KEYUP || type == SDL_TEXTINPUT;
}

} // namespace

int main(int argc, char **argv)
{
    // O demo do ImGui pede 550x680 e uma faixa de números de 92 px acima dele,
    // por isso 560 por painel e 800 de altura mostram os dois lados inteiros.
    int width = 1124;
    int height = 800;
    int frames = 0;
    bool hidden = false;
    bool vsync = true;
    bool openAll = false;
    std::string screenshot;
    std::string fontPath = IGUI_GALLERY_FONT;
    std::string openSection;
    std::string onlySection;
    bool click = false;
    float clickX = 0.0f;
    float clickY = 0.0f;
    int wheel = 0;
    bool hover = false;
    float hoverX = 0.0f;
    float hoverY = 0.0f;
    bool dump = false;

    for (int i = 1; i < argc; ++i)
    {
        const char *arg = argv[i];
        if (strncmp(arg, "--width=", 8) == 0)
            width = atoi(arg + 8);
        else if (strncmp(arg, "--height=", 9) == 0)
            height = atoi(arg + 9);
        else if (strncmp(arg, "--frames=", 9) == 0)
            frames = atoi(arg + 9);
        else if (strncmp(arg, "--screenshot=", 13) == 0)
            screenshot = arg + 13;
        else if (strcmp(arg, "--hidden") == 0)
            hidden = true;
        else if (strcmp(arg, "--no-vsync") == 0)
            vsync = false;
        else if (strcmp(arg, "--open-all") == 0)
            openAll = true;
        else if (strcmp(arg, "--dump") == 0)
            dump = true;
        else if (strncmp(arg, "--open=", 7) == 0)
            openSection = arg + 7;
        else if (strncmp(arg, "--only=", 7) == 0)
            onlySection = arg + 7;
        else if (parseClick(arg, clickX, clickY))
            click = true;
        else if (strncmp(arg, "--wheel=", 8) == 0)
            wheel = atoi(arg + 8);
        else if (parsePoint(arg, "--hover=", hoverX, hoverY))
            hover = true;
        else if (strncmp(arg, "--font=", 7) == 0)
            fontPath = arg + 7;
        else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0)
        {
            usage();
            return 0;
        }
        else
        {
            fprintf(stderr, "opcao desconhecida: %s\n", arg);
            usage();
            return 2;
        }
    }
    if (width < 700 || height < 400)
    {
        fprintf(stderr, "janela demasiado pequena: %dx%d\n", width, height);
        return 2;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (hidden)
        windowFlags |= SDL_WINDOW_HIDDEN;
    SDL_Window *window = SDL_CreateWindow("iGUI vs ImGui  (demo window)", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, width, height, windowFlags);
    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context)
    {
        fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(vsync ? 1 : 0);

    // ── iGUI ─────────────────────────────────────────────────────────────────
    ig::opengl::Backend backend;
    if (!backend.init(SDL_GL_GetProcAddress))
    {
        fprintf(stderr, "o backend OpenGL 3.3 do iGUI nao arrancou (contexto antigo?)\n");
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    // A fonte dos dois painéis: a mesma do demo do ImGui (ProggyClean, em
    // misc/fonts), para a comparação ser do que desenha e não dos glifos. Sem
    // ela - ou sem o ficheiro - fica a fonte de omissão do iGUI, que é Roboto:
    // uma fonte vetorial sai mais pequena e mais clara que uma de pixel pedida no
    // mesmo tamanho, e aí os dois painéis deixam de ser o mesmo ecrã.
    std::vector<unsigned char> fontBytes;
    if (!fontPath.empty())
    {
        std::FILE *file = std::fopen(fontPath.c_str(), "rb");
        if (file)
        {
            std::fseek(file, 0L, SEEK_END);
            const long size = std::ftell(file);
            std::fseek(file, 0L, SEEK_SET);
            if (size > 0)
            {
                fontBytes.resize(static_cast<size_t>(size));
                if (std::fread(fontBytes.data(), 1u, fontBytes.size(), file) != fontBytes.size())
                    fontBytes.clear();
            }
            std::fclose(file);
        }
        if (fontBytes.empty())
            std::fprintf(stderr, "nao consegui ler a fonte '%s'; fica a de omissao\n",
                         fontPath.c_str());
    }
    ig::FontAtlas atlas = fontBytes.empty()
                              ? ig::FontAtlas(ig::defaultFontRanges(), 2048u, 1024u,
                                              gallery::kFontSize)
                              : ig::FontAtlas(ig::Span<const uint8_t>(
                                                  fontBytes.data(), fontBytes.size()),
                                              ig::defaultFontRanges(), 2048u, 1024u,
                                              gallery::kFontSize);
    if (!atlas.valid())
    {
        fprintf(stderr, "o atlas de fontes do iGUI nao foi construido\n");
        return 1;
    }
    backend.setFontAtlas(&atlas);
    ig::Context ui(backend, &atlas);
    igui_gallery::applyDemoStyle(ui);
    snprintf(gallery::gHelloLine, sizeof(gallery::gHelloLine), "dear imgui says hello! (%s) (%d)",
             IMGUI_VERSION, IMGUI_VERSION_NUM);
    igui_gallery::reset();
    if (openAll)
        igui_gallery::openAllSections();
    if (!onlySection.empty() && !igui_gallery::setOnlySection(onlySection.c_str()))
    {
        fprintf(stderr, "seccao desconhecida: %s\n", onlySection.c_str());
        return 1;
    }
    if (!openSection.empty() && !igui_gallery::openSection(openSection.c_str()))
    {
        fprintf(stderr, "seccao desconhecida: %s\n", openSection.c_str());
        return 2;
    }
    std::printf("iGUI   GL %d.%d em %s, atlas %ux%u, fonte %s\n", backend.versionMajor(),
                backend.versionMinor(), backend.renderer(), atlas.width(), atlas.height(),
                fontBytes.empty() ? "de omissao (Roboto)" : fontPath.c_str());

    // ── ImGui ────────────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    ImGui_ImplSDL2_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    imgui_gallery::reset();
    std::printf("ImGui  %s (demo window a serio, via os backends de SDL2 e OpenGL3)\n",
                IMGUI_VERSION);

    PanelTiming iguiTiming;
    PanelTiming imguiTiming;

    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
    float pixelScale = width > 0 ? static_cast<float>(drawableWidth) / static_cast<float>(width)
                                 : 1.0f;
    if (pixelScale <= 0.0f)
        pixelScale = 1.0f;

    const ig::Color background(24u, 26u, 32u, 255u);
    const ig::Color panelBackground(32u, 34u, 40u, 255u);
    const int gap = 4;

    bool running = true;
    int frame = 0;
    int activePanel = 1; // 0 = iGUI, 1 = ImGui: quem tem o teclado
    int iguiItemsLast = 0;
    int imguiVtxLast = 0;
    int imguiDrawsLast = 0;
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    std::vector<unsigned char> pixels;

    while (running)
    {
        const Uint64 counter = SDL_GetPerformanceCounter();
        const float deltaSeconds = static_cast<float>(counter - previousCounter) /
                                   static_cast<float>(frequency);
        previousCounter = counter;

        const int panelLogical = (width - gap) / 2;
        const int panelPixels =
            static_cast<int>(static_cast<float>(panelLogical) * pixelScale + 0.5f);
        const int imguiX = panelLogical + gap;
        const int imguiXPixels =
            static_cast<int>(static_cast<float>(imguiX) * pixelScale + 0.5f);

        int pointerX = 0;
        int pointerY = 0;
        SDL_GetMouseState(&pointerX, &pointerY);
        const bool pointerInIgui = pointerX < panelLogical;
        const bool pointerInImgui = pointerX >= imguiX;
        const bool windowFocused = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            // O ImGui só vê o teclado quando é ele o painel ativo, senão escrever
            // num lado escrevia nos dois. O rato vai sempre: cada painel decide
            // pelo lado em que está.
            if (!(isKeyEvent(event.type) && activePanel != 1))
                ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_WINDOWEVENT &&
                (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                 event.window.event == SDL_WINDOWEVENT_RESIZED))
            {
                width = event.window.data1;
                height = event.window.data2;
                SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
                pixelScale = width > 0 ? static_cast<float>(drawableWidth) /
                                             static_cast<float>(width)
                                       : 1.0f;
                if (pixelScale <= 0.0f)
                    pixelScale = 1.0f;
            }

            // O iGUI leva o rato em coordenadas do seu painel, e só quando está
            // sobre ele.
            if (isKeyEvent(event.type) && activePanel == 0)
            {
                ig::Event translated;
                if (ig::sdl2::translateEvent(event, translated, pixelScale))
                    ui.pushEvent(translated);
            }
            else if (isMouseEvent(event.type) && pointerInIgui)
            {
                ig::Event translated;
                if (ig::sdl2::translateEvent(event, translated, pixelScale))
                    ui.pushEvent(translated);
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
            {
                if (pointerInIgui)
                    activePanel = 0;
                else if (pointerInImgui)
                    activePanel = 1;
            }
        }

        // --click=X,Y: um clique do iGUI a fingir, para os popups (que só abrem com
        // rato) aparecerem numa captura. Os três eventos vão por frames diferentes e
        // no mesmo ponto, como um clique a sério: o popup só abre na largada.
        if (click)
        {
            if (frame == 1)
                ui.pushEvent(ig::Event::pointerMove(clickX, clickY));
            else if (frame == 2)
                ui.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, clickX, clickY));
            else if (frame == 3)
                ui.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, clickX, clickY));
        }
        // --hover=X,Y: põe o ponteiro nesse ponto a partir do frame 5 (sem clicar).
        // É o que abre um submenu, que abre por hover e não por clique.
        if (hover && frame >= 5)
            ui.pushEvent(ig::Event::pointerMove(hoverX, hoverY));

        // --wheel=N: N entalhes de roda, um por frame. O ponto é o do --click 70 px
        // abaixo, que é onde a lista de um popup começa (o clique fica no
        // cabeçalho e a lista abre por baixo dele): é o que faz a lista rolar numa
        // captura.
        if (wheel != 0 && frame >= 6 && frame < 6 + (wheel < 0 ? -wheel : wheel))
        {
            ig::Event wheelEvent;
            wheelEvent.type = ig::EventType::PointerWheel;
            wheelEvent.wheelY = wheel > 0 ? 1.0f : -1.0f;
            ui.pushEvent(ig::Event::pointerMove(clickX, clickY + 70.0f));
            ui.pushEvent(wheelEvent);
        }

        // Os NewFrame dos dois backends do ImGui são obrigatórios (é onde o
        // backend de OpenGL cria o shader e os buffers na primeira vez). Vêm
        // antes de mexer no rato, porque o do SDL2 escreve a posição.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        // O ImGui trabalha dentro do seu painel: fora dele não há ponteiro.
        if (pointerInImgui && windowFocused)
            io.AddMousePosEvent(static_cast<float>(pointerX - imguiX),
                                static_cast<float>(pointerY));
        else
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        io.DisplaySize = ImVec2(static_cast<float>(panelLogical), static_cast<float>(height));
        io.DisplayFramebufferScale = ImVec2(pixelScale, pixelScale);
        io.DeltaTime = deltaSeconds > 0.0f ? deltaSeconds : 1.0f / 60.0f;

        backend.setWindowSize(drawableWidth, drawableHeight);

        // Os números de cada lado são preenchidos antes de o lado correr.
        gallery::Readout iguiReadout;
        iguiReadout.history = iguiTiming.history.empty() ? nullptr : &iguiTiming.history[0];
        iguiReadout.historyCount = static_cast<int>(iguiTiming.history.size());
        iguiReadout.submitMs = static_cast<float>(iguiTiming.submit.mean() * 0.001);
        iguiReadout.renderMs = static_cast<float>(iguiTiming.render.mean() * 0.001);
        iguiReadout.gpuMs = static_cast<float>(iguiTiming.gpu.mean() * 0.001);
        iguiReadout.hz = static_cast<float>(iguiTiming.hz());
        iguiReadout.items = iguiItemsLast;
        iguiReadout.vertices = iguiTiming.vertices;
        iguiReadout.drawCalls = iguiTiming.drawCalls;
        iguiReadout.gaps = gallery::gapCount();

        gallery::Readout imguiReadout = iguiReadout;
        imguiReadout.history = imguiTiming.history.empty() ? nullptr : &imguiTiming.history[0];
        imguiReadout.historyCount = static_cast<int>(imguiTiming.history.size());
        imguiReadout.submitMs = static_cast<float>(imguiTiming.submit.mean() * 0.001);
        imguiReadout.renderMs = static_cast<float>(imguiTiming.render.mean() * 0.001);
        imguiReadout.gpuMs = static_cast<float>(imguiTiming.gpu.mean() * 0.001);
        imguiReadout.hz = static_cast<float>(imguiTiming.hz());
        imguiReadout.items = 0;
        imguiReadout.vertices = imguiVtxLast;
        imguiReadout.drawCalls = imguiDrawsLast;
        imguiReadout.gaps = 0;

        // ── iGUI: o demo reescrito ──────────────────────────────────────────
        ui.beginFrame(ig::FrameInfo(static_cast<float>(panelLogical), static_cast<float>(height),
                                    pixelScale, deltaSeconds));
        // Os números ficam dentro do frame de cada lado, nos dois: se num lado
        // ficassem fora da medição e no outro dentro, os tempos já não seriam
        // comparáveis. O painel de números é uma linha de texto e um gráfico, e
        // custa o mesmo nas duas bibliotecas.
        gallery::setReadout(iguiReadout);
        const double iguiStart = cpuNowUs();
        const gallery::FrameStats iguiStats = igui_gallery::draw(ui, deltaSeconds);
        const double iguiSubmitted = cpuNowUs();
        const ig::DrawData &iguiData = ui.endFrame();

        // ── ImGui: o ShowDemoWindow() a sério ───────────────────────────────
        ImGui::NewFrame();
        gallery::setReadout(imguiReadout);
        const double imguiStart = cpuNowUs();
        const gallery::FrameStats imguiStats = imgui_gallery::draw();
        const double imguiSubmitted = cpuNowUs();
        ImGui::Render();

        ImDrawData *imguiData = ImGui::GetDrawData();
        if (imguiData)
        {
            imguiVtxLast = imguiData->TotalVtxCount;
            imguiDrawsLast = imguiData->CmdListsCount;
        }

        // ── Desenhar os dois painéis ────────────────────────────────────────
        backend.clear(background);

        backend.beginTarget(0, panelPixels, drawableHeight);
        backend.clear(panelBackground);
        backend.gpuTimerBegin(0);
        backend.render(iguiData);
        backend.gpuTimerEnd(0);
        const double iguiRendered = cpuNowUs();
        backend.endTarget(0, 0, 0, panelPixels, drawableHeight);

        backend.beginTarget(1, panelPixels, drawableHeight);
        backend.clear(panelBackground);
        backend.gpuTimerBegin(1);
        ImGui_ImplOpenGL3_RenderDrawData(imguiData);
        backend.gpuTimerEnd(1);
        const double imguiRendered = cpuNowUs();
        backend.endTarget(1, imguiXPixels, 0, panelPixels, drawableHeight);

        iguiTiming.items = iguiStats.items;
        iguiTiming.vertices = backend.uploadedVertices();
        iguiTiming.drawCalls = backend.drawCalls();
        iguiTiming.push(iguiSubmitted - iguiStart, iguiRendered - iguiSubmitted,
                        backend.gpuSeconds(0) * 1e6);
        imguiTiming.items = imguiStats.items;
        imguiTiming.vertices = imguiVtxLast;
        imguiTiming.drawCalls = imguiDrawsLast;
        imguiTiming.push(imguiSubmitted - imguiStart, imguiRendered - imguiSubmitted,
                         backend.gpuSeconds(1) * 1e6);
        iguiItemsLast = iguiStats.items;

        if (frame == 4)
        {
            std::printf("painel: %d px logicos, %d px de framebuffer\n", panelLogical,
                        panelPixels);
            std::printf("iGUI: %d itens, %d vtx, %d draw calls\n", iguiStats.items,
                        backend.uploadedVertices(), backend.drawCalls());
            std::printf("ImGui demo: %d vtx, %d draw lists\n", imguiVtxLast, imguiDrawsLast);
            if (dump)
            {
                // O mesmo diagnóstico que o bench tem (--dump): os comandos do
                // último frame dos dois lados, com o número de índices e a caixa
                // do que desenham - é assim que se vê de onde vêm os vértices.
                std::printf("comandos do iGUI:\n");
                for (size_t i = 0; i < iguiData.commands.size(); ++i)
                {
                    const ig::DrawCommand &command = iguiData.commands[i];
                    if (command.type != ig::DrawCommandType::Geometry)
                    {
                        std::printf("  %zu text\n", i);
                        continue;
                    }
                    const ig::GeometryCommand &g = command.payload.geometry;
                    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
                    for (uint32_t k = 0; k < g.indexCount; ++k)
                    {
                        const ig::Vec2 &p = iguiData.vertices[iguiData.indices[g.firstIndex + k]].position;
                        minX = p.x < minX ? p.x : minX;
                        minY = p.y < minY ? p.y : minY;
                        maxX = p.x > maxX ? p.x : maxX;
                        maxY = p.y > maxY ? p.y : maxY;
                    }
                    std::printf("  %zu geom clip=(%7.1f,%7.1f %7.1fx%7.1f) idx=%-5u "
                                "bbox=(%7.1f,%7.1f)-(%7.1f,%7.1f)\n",
                                i, static_cast<double>(g.clip.x), static_cast<double>(g.clip.y),
                                static_cast<double>(g.clip.width), static_cast<double>(g.clip.height),
                                g.indexCount, static_cast<double>(minX), static_cast<double>(minY),
                                static_cast<double>(maxX), static_cast<double>(maxY));
                }
                // De que cor são os vértices: com o texto todo da mesma cor, é
                // isto que separa "muitos glifos" de "geometria a mais".
                {
                    struct Bucket
                    {
                        ig::Color color;
                        int count;
                    };
                    Bucket buckets[24];
                    int used = 0;
                    for (size_t i = 0; i < iguiData.vertices.size(); ++i)
                    {
                        const ig::Color color = iguiData.vertices[i].color;
                        int found = -1;
                        for (int b = 0; b < used; ++b)
                            if (buckets[b].color == color)
                            {
                                found = b;
                                break;
                            }
                        if (found < 0 && used < 24)
                        {
                            found = used++;
                            buckets[found].color = color;
                            buckets[found].count = 0;
                        }
                        if (found >= 0)
                            ++buckets[found].count;
                    }
                    for (int a = 0; a < used; ++a)
                        for (int b = a + 1; b < used; ++b)
                            if (buckets[b].count > buckets[a].count)
                            {
                                const Bucket swap = buckets[a];
                                buckets[a] = buckets[b];
                                buckets[b] = swap;
                            }
                    std::printf("vertices do iGUI por cor (total %zu, %d cores):\n",
                                iguiData.vertices.size(), used);
                    for (int a = 0; a < used; ++a)
                        std::printf("  #%02x%02x%02x%02x  %6d vtx\n", buckets[a].color.r,
                                    buckets[a].color.g, buckets[a].color.b, buckets[a].color.a,
                                    buckets[a].count);
                }
                if (imguiData)
                {
                    // O mesmo por cor do lado do ImGui: as cores são as do
                    // ImGuiCol_*, por isso o branco é o texto nos dois lados.
                    struct Bucket
                    {
                        ImU32 color;
                        int count;
                    };
                    Bucket buckets[24];
                    int used = 0;
                    for (int l = 0; l < imguiData->CmdListsCount; ++l)
                    {
                        const ImDrawList *list = imguiData->CmdLists[l];
                        for (int v = 0; v < list->VtxBuffer.Size; ++v)
                        {
                            const ImU32 color = list->VtxBuffer[v].col;
                            int found = -1;
                            for (int b = 0; b < used; ++b)
                                if (buckets[b].color == color)
                                {
                                    found = b;
                                    break;
                                }
                            if (found < 0 && used < 24)
                            {
                                found = used++;
                                buckets[found].color = color;
                                buckets[found].count = 0;
                            }
                            if (found >= 0)
                                ++buckets[found].count;
                        }
                    }
                    for (int a = 0; a < used; ++a)
                        for (int b = a + 1; b < used; ++b)
                            if (buckets[b].count > buckets[a].count)
                            {
                                const Bucket swap = buckets[a];
                                buckets[a] = buckets[b];
                                buckets[b] = swap;
                            }
                    std::printf("vertices do ImGui por cor (total %d, %d cores):\n",
                                imguiData->TotalVtxCount, used);
                    for (int a = 0; a < used; ++a)
                        std::printf("  #%08x  %6d vtx\n", buckets[a].color, buckets[a].count);
                    std::printf("comandos do ImGui:\n");
                    for (int l = 0; l < imguiData->CmdListsCount; ++l)
                    {
                        const ImDrawList *list = imguiData->CmdLists[l];
                        for (int c = 0; c < list->CmdBuffer.Size; ++c)
                        {
                            const ImDrawCmd &cmd = list->CmdBuffer[c];
                            std::printf("  lista %d cmd %2d clip=(%7.1f,%7.1f %7.1fx%7.1f) idx=%u "
                                        "vtx=%d\n",
                                        l, c, static_cast<double>(cmd.ClipRect.x),
                                        static_cast<double>(cmd.ClipRect.y),
                                        static_cast<double>(cmd.ClipRect.z - cmd.ClipRect.x),
                                        static_cast<double>(cmd.ClipRect.w - cmd.ClipRect.y),
                                        cmd.ElemCount, list->VtxBuffer.Size);
                        }
                    }
                }
            }
            std::printf("lacunas do iGUI face ao demo: %d\n", gallery::gapCount());
        }

        if (frames > 0 && frame + 1 >= frames)
            running = false;

        if (!running && !screenshot.empty())
        {
            pixels.assign(static_cast<size_t>(drawableWidth) * static_cast<size_t>(drawableHeight) *
                              4u,
                          0u);
            if (backend.readPixels(0, 0, drawableWidth, drawableHeight, pixels.data()))
                compare::writePng(screenshot.c_str(), drawableWidth, drawableHeight,
                                  pixels.data());
        }

        SDL_GL_SwapWindow(window);
        ++frame;
    }

    // ── Resumo ──────────────────────────────────────────────────────────────
    std::printf("\n%d frames\n", frame);
    std::printf("            submit medio   mediana      p95      render medio      gpu medio\n");
    std::printf("iGUI    %13.1f %9.1f %9.1f %14.1f %14.1f   us\n", iguiTiming.submit.mean(),
                iguiTiming.submit.percentile(0.5), iguiTiming.submit.percentile(0.95),
                iguiTiming.render.mean(), iguiTiming.gpu.mean());
    std::printf("ImGui   %13.1f %9.1f %9.1f %14.1f %14.1f   us\n", imguiTiming.submit.mean(),
                imguiTiming.submit.percentile(0.5), imguiTiming.submit.percentile(0.95),
                imguiTiming.render.mean(), imguiTiming.gpu.mean());

    const double iguiTotal = iguiTiming.submit.mean() + iguiTiming.render.mean();
    const double imguiTotal = imguiTiming.submit.mean() + imguiTiming.render.mean();
    std::printf("\nsubmit+render: iGUI %.1f us, ImGui %.1f us -> iGUI e %.2fx o ImGui\n", iguiTotal,
                imguiTotal, imguiTotal > 0.0 ? iguiTotal / imguiTotal : 0.0);
    std::printf("geometria: iGUI %d vtx / %d draw calls, ImGui %d vtx / %d draw lists\n",
                iguiTiming.vertices, iguiTiming.drawCalls, imguiTiming.vertices,
                imguiTiming.drawCalls);
    std::printf("itens submetidos pelo espelho do iGUI: %d\n", iguiTiming.items);

    std::printf("\nO que o demo do ImGui tem e o iGUI nao exprime, nas seccoes desenhadas "
                "neste frame (%d):\n", gallery::gapCount());
    for (int i = 0; i < gallery::gapCount(); ++i)
        std::printf("  %-28s %s\n", gallery::gap(i).section, gallery::gap(i).widget);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    backend.shutdown();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
