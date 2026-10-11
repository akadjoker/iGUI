#pragma once

#include "Gallery.hpp"

// O lado do ImGui: o demo window verdadeiro, o mesmo que o botão "Demo Window" do
// imgui_demo.cpp mostra.
namespace imgui_gallery
{

// O ShowDemoWindow(), com o painel de números numa faixa fixa no topo do painel
// (o ImGui não desenha nada fora de uma janela, e assim os dois lados mostram os
// números no mesmo sítio). Devolve o que o demo submeteu.
gallery::FrameStats draw();

void reset();

} // namespace imgui_gallery
