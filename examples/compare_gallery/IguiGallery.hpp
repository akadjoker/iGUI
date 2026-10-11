#pragma once

#include "Gallery.hpp"

namespace ig
{
class Context;
class Theme;
}

// O lado do iGUI: o demo do ImGui reescrito com as widgets do iGUI.
namespace igui_gallery
{

// Desenha o demo, com o painel de números no topo da própria janela. Tem de ser
// chamado entre beginFrame() e endFrame() do Context. deltaSeconds é o mesmo que
// foi dado ao beginFrame: as secções animadas (barras de progresso) usam-no.
gallery::FrameStats draw(ig::Context &ui, float deltaSeconds);

// Repõe o estado inicial.
void reset();

// As métricas do estilo por omissão do ImGui, mapeadas no tema do iGUI. Sem
// isto o painel do iGUI fica maior linha a linha (fonte 16, item de 28) do que o
// demo (fonte 13, item de 19) e os dois deixam de ser o mesmo ecrã: nem se
// comparam widget a widget, nem os tempos medem o mesmo trabalho.
void applyDemoStyle(ig::Context &ui);

// Abre todas as secções e sub-secções de uma vez, para um relatório de lacunas
// completo sem ter de clicar em todas (é o que o --open-all faz).
void openAllSections();

// Abre uma secção pelo nome, para pôr uma só lado a lado (é o que o --open faz).
bool openSection(const char *name);

} // namespace igui_gallery
