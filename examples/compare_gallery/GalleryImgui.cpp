// O lado do ImGui: o demo window a sério.
//
// Não há aqui nenhuma reimplementação: é o ImGui::ShowDemoWindow() do
// imgui_demo.cpp, com tudo o que ele traz (menus, secções, exemplos, as janelas
// de Metrics/Style Editor/About quando abertas). O lado do iGUI é que é o
// espelho, em GalleryIgui.cpp.
//
// Por cima fica uma faixa fixa com os números, para os dois painéis se lerem um
// contra o outro.

#include "Gallery.hpp"
#include "ImguiGallery.hpp"

#include "imgui.h"

#include <stdio.h>

namespace imgui_gallery
{
namespace
{

bool g_open = true;
bool g_geometrySet = false;


// O demo do ImGui pede posição e tamanho por dentro, com ImGuiCond_FirstUseEver,
// e esses pedidos sobrepõem-se aos nossos no mesmo frame. Como o FirstUseEver só
// se aplica à primeira aparição da janela, a forma de a encostar à metade do
// painel é abrir uma vez a janela com o nome do demo já com a geometria que
// queremos: a partir daí a janela existe, o FirstUseEver do demo não tem efeito,
// e o demo ocupa o painel todo.
void placeDemoWindow()
{
    if (g_geometrySet) return;
    g_geometrySet = true;
    const ImVec2 panel = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, gallery::kReadoutHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panel.x, panel.y - gallery::kReadoutHeight), ImGuiCond_Always);
    ImGui::Begin("Dear ImGui Demo", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::End();
}

void readout()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, gallery::kReadoutHeight),
            ImGuiCond_Always);
    const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("##readout", nullptr, flags))
    {
        const gallery::Readout& info = gallery::readout();
        char line[192];
        snprintf(line, sizeof(line),
                "ImGui   submit %.2f ms   render %.2f ms   gpu %.2f ms   %d vtx   %d draw lists",
                static_cast<double>(info.submitMs), static_cast<double>(info.renderMs),
                static_cast<double>(info.gpuMs), info.vertices, info.drawCalls);
        ImGui::SeparatorText(line);
        // O mesmo gráfico que o lado do iGUI desenha à mão, com a escala fixa nos
        // 4 ms, para os dois painéis se lerem um contra o outro.
        ImGui::PlotLines("##historico", info.history, info.historyCount, 0, nullptr, 0.0f, 4.0f,
                ImVec2(-FLT_MIN, 48.0f));
    }
    ImGui::End();
}

} // namespace

void reset()
{
    g_open = true;
    g_geometrySet = false;
    gallery::clearGaps();
}

gallery::FrameStats draw()
{
    readout();

    placeDemoWindow();
    ImGui::ShowDemoWindow(&g_open);
    return gallery::FrameStats();
}

} // namespace imgui_gallery
