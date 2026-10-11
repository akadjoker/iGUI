// ImGui side of the headless comparison.
//
// Submits the same canonical block as IguiRunner.cpp through ImGui's immediate
// API, and for the same widget count. The differences that remain are the ones
// that cannot be papered over and are called out in bench/README.md:
//   - ImGui's fixed windows are given an explicit position and size every frame
//     (ImGuiCond_Always), because the iGUI side is handed explicit bounds and
//     auto-fit/auto-positioning is not what this measures.
//   - ImGui's TreeNode indents its children itself; iGUI's layout treeNode does
//     not (the equivalent indentation is a separate indent()/unindent() call).
//   - Text is measured by the libraries' own font code: ImGui bakes glyphs on
//     demand, iGUI uses its font atlas (or a fixed-advance fallback when the
//     font module is not built).
//
//   gallery block, per repetition:
//     text   button | sameLine | checkbox   sliderFloat   inputText
//     combo  separator  progressBar
//     collapsingHeader (default open) -> 3 texts
//     treeNode (default open) -> 1 text
//
// No backend is installed: ImGui 1.92 builds draw data without one as long as
// the renderer claims ImGuiBackendFlags_RendererHasTextures, which is the modern
// (1.92+) contract for a backend that owns texture lifecycles.

#include "Bench.hpp"
#include "Clock.hpp"
#include "Stats.hpp"

#include "imgui.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#  include <sys/resource.h>
#endif

namespace bench
{
namespace
{

struct State
{
    bool enabled;
    float volume;
    int quality;
    int plotOffset;
    char name[64];

    State() : enabled(true), volume(0.35f), quality(1), plotOffset(0)
    {
        std::snprintf(name, sizeof(name), "%s", "player");
    }
};

const char *const kQualityItems[] = { "low", "medium", "high", "very high", "extreme" };

void galleryBlock(State &state, int index, long long &widgets)
{
    ImGui::Text("row %d", index);
    ++widgets;
    ImGui::Button("Apply");
    ++widgets;
    ImGui::SameLine();
    ImGui::Checkbox("Enabled", &state.enabled);
    ++widgets;
    ImGui::SliderFloat("Volume", &state.volume, 0.0f, 1.0f);
    ++widgets;
    ImGui::InputText("Name", state.name, sizeof(state.name));
    ++widgets;
    ImGui::Combo("Quality", &state.quality, kQualityItems, IM_ARRAYSIZE(kQualityItems));
    ++widgets;
    ImGui::Separator();
    // Empty overlay: iGUI's progressBar draws the bar only, with no text on it.
    ImGui::ProgressBar(state.volume, ImVec2(-FLT_MIN, 0.0f), "");
    ++widgets;
    if (ImGui::CollapsingHeader("Advanced", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ++widgets;
        ImGui::TextUnformatted("first option");
        ImGui::TextUnformatted("second option");
        ImGui::TextUnformatted("third option");
        widgets += 3;
    }
    if (ImGui::TreeNodeEx("Node", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ++widgets;
        ImGui::TextUnformatted("leaf");
        ++widgets;
        ImGui::TreePop();
    }
}

void textBlock(int index, long long &widgets)
{
    for (int i = 0; i < 10; ++i)
    {
        ImGui::Text("line %d.%d with some text in it", index, i);
        ++widgets;
    }
}

// A mesma serie do lado do iGUI (ver IguiRunner.cpp), para os dois desenharem
// exatamente os mesmos dados.
const float *plotSamples()
{
    static float values[90];
    static bool filled = false;
    if (!filled)
    {
        for (int i = 0; i < 90; ++i)
            values[i] = std::cos(static_cast<float>(i) * 0.1f);
        filled = true;
    }
    return values;
}

void plotsBlock(int index, int offset, long long &widgets)
{
    char label[32];
    std::snprintf(label, sizeof(label), "Lines %d", index);
    ImGui::PlotLines(label, plotSamples(), 90, offset, "avg 0.500000", -1.0f, 1.0f,
                     ImVec2(400.0f, 128.0f));
    ++widgets;
    std::snprintf(label, sizeof(label), "Bars %d", index);
    ImGui::PlotHistogram(label, plotSamples(), 90, 0, NULL, -1.0f, 1.0f, ImVec2(400.0f, 80.0f));
    ++widgets;
}

void smallBlock(State &state, int index, long long &widgets)
{
    ImGui::Text("panel %d", index);
    ++widgets;
    ImGui::Button("Apply");
    ++widgets;
    ImGui::Checkbox("Enabled", &state.enabled);
    ++widgets;
    ImGui::SliderFloat("Volume", &state.volume, 0.0f, 1.0f);
    ++widgets;
}

void gridPosition(int index, int columns, float width, float height, float gap,
                  ImVec2 &out)
{
    const int column = columns > 0 ? index % columns : 0;
    const int row = columns > 0 ? index / columns : 0;
    out = ImVec2(gap + static_cast<float>(column) * (width + gap),
                 gap + static_cast<float>(row) * (height + gap));
}

void buildScene(State &state, const Config &config, int frame, long long &widgets)
{
    state.volume = 0.5f + 0.4f * std::sin(static_cast<float>(frame) * 0.1f);
    state.plotOffset = frame % 90;

    if (config.scene == "windows")
    {
        int columns = 1;
        while (columns * columns < config.windows)
            ++columns;
        for (int i = 0; i < config.windows; ++i)
        {
            char title[32];
            std::snprintf(title, sizeof(title), "Panel %d", i);
            ImVec2 position;
            gridPosition(i, columns, 280.0f, 180.0f, 8.0f, position);
            ImGui::SetNextWindowPos(position, ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(280.0f, 180.0f), ImGuiCond_Always);
            ImGui::Begin(title);
            smallBlock(state, i, widgets);
            ImGui::End();
        }
        return;
    }

    const int blockCount = config.scene == "basic" ? 1 : config.blocks;
    ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(560.0f, config.height - 80.0f), ImGuiCond_Always);
    ImGui::Begin("Gallery");
    for (int i = 0; i < blockCount; ++i)
    {
        if (config.scene == "text")
            textBlock(i, widgets);
        else if (config.scene == "plots")
            plotsBlock(i, state.plotOffset, widgets);
        else
            galleryBlock(state, i, widgets);
    }
    ImGui::End();
}

long long readPeakRssKb()
{
#if defined(__unix__) || defined(__APPLE__)
    struct rusage usage;
    if (::getrusage(RUSAGE_SELF, &usage) == 0)
        return static_cast<long long>(usage.ru_maxrss);
#endif
    return -1;
}

} // namespace

Result runImgui(const Config &config)
{
    Result result;
    result.lib = "imgui";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(config.width, config.height);
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    // Modern (1.92+) backend contract: the renderer owns texture lifetimes, so
    // ImGui grows its font atlas on demand instead of requiring a built one.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    // Square corners on both sides, so the vertex counts compare the widgets
    // rather than one library's taste in decoration (iGUI's default theme rounds
    // and anti-aliases every frame; its geometry cost is reported separately in
    // bench/README.md).
    ImGuiStyle &style = ImGui::GetStyle();
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;

    State state;
    std::vector<double> submit;
    std::vector<double> render;
    submit.reserve(static_cast<std::size_t>(config.frames));
    render.reserve(static_cast<std::size_t>(config.frames));

    long long widgets = 0;
    long long vertices = 0;
    long long indices = 0;
    long long commands = 0;

    for (int frame = 0; frame < config.warmup + config.frames; ++frame)
    {
        widgets = 0;

        const double start = cpuNowUs();
        ImGui::NewFrame();
        buildScene(state, config, frame, widgets);
        ImGui::Render();
        const double submitted = cpuNowUs();

        // What a backend does with the draw data: walk the draw lists.
        vertices = 0;
        indices = 0;
        commands = 0;
        ImDrawData *draw = ImGui::GetDrawData();
        if (draw)
        {
            for (int list = 0; list < draw->CmdListsCount; ++list)
            {
                const ImDrawList *drawList = draw->CmdLists[list];
                vertices += drawList->VtxBuffer.Size;
                indices += drawList->IdxBuffer.Size;
                commands += drawList->CmdBuffer.Size;
            }
        }
        const double rendered = cpuNowUs();

        if (frame >= config.warmup)
        {
            submit.push_back(submitted - start);
            render.push_back(rendered - submitted);
        }
        if (config.dump && draw && frame == config.warmup + config.frames - 1)
        {
            std::printf("imgui commands: %lld\n", commands);
            for (int list = 0; list < draw->CmdListsCount; ++list)
            {
                const ImDrawList *drawList = draw->CmdLists[list];
                for (int c = 0; c < drawList->CmdBuffer.Size; ++c)
                {
                    const ImDrawCmd &cmd = drawList->CmdBuffer[c];
                    std::printf("  list %d cmd %2d clip=(%7.1f,%7.1f %7.1fx%7.1f) idx=%u\n",
                                list, c, static_cast<double>(cmd.ClipRect.x),
                                static_cast<double>(cmd.ClipRect.y),
                                static_cast<double>(cmd.ClipRect.z - cmd.ClipRect.x),
                                static_cast<double>(cmd.ClipRect.w - cmd.ClipRect.y),
                                cmd.ElemCount);
                }
            }
        }
    }

    ImGui::DestroyContext();

    result.ran = true;
    result.submitMeanUs = mean(submit);
    result.submitMedianUs = percentile(submit, 0.5);
    result.submitP95Us = percentile(submit, 0.95);
    result.submitMinUs = percentile(submit, 0.0);
    result.renderMeanUs = mean(render);
    result.totalMeanUs = result.submitMeanUs + result.renderMeanUs;
    result.vertices = vertices;
    result.indices = indices;
    result.commands = commands;
    result.widgets = widgets;
    result.peakRssKb = readPeakRssKb();
    return result;
}

} // namespace bench
