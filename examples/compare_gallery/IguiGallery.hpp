#pragma once

#include <igui/FontAtlas.hpp>

#include "Gallery.hpp"

namespace ig
{
class Context;
class Theme;
} // namespace ig

// O lado do iGUI: o demo do ImGui reescrito com as widgets do iGUI.
namespace igui_gallery
{

// Desenha o demo, com o painel de números no topo da própria janela. Tem de ser
// chamado entre beginFrame() e endFrame() do Context. deltaSeconds é o mesmo que
// foi dado ao beginFrame: as secções animadas (barras de progresso) usam-no.
gallery::FrameStats draw(ig::Context& ui, float deltaSeconds);

// Repõe o estado inicial.
void reset();

// As métricas do estilo por omissão do ImGui, mapeadas no tema do iGUI. Sem
// isto o painel do iGUI fica maior linha a linha (fonte 16, item de 28) do que o
// demo (fonte 13, item de 19) e os dois deixam de ser o mesmo ecrã: nem se
// comparam widget a widget, nem os tempos medem o mesmo trabalho.
void applyDemoStyle(ig::Context& ui);

// Abre todas as secções e sub-secções de uma vez, para um relatório de lacunas
// completo sem ter de clicar em todas (é o que o --open-all faz).
void openAllSections();

// Abre as janelas do menu Tools (Style Editor / Metrics / About), que o demo abre
// por fora da janela do demo. É o que o --tools faz, para as poder capturar sem
// ter de clicar no menu.
void setTools(bool styleEditor, bool metrics, bool about);

// Abre uma secção pelo nome, para pôr uma só lado a lado (é o que o --open faz).
bool openSection(const char* name);

// Desenha só essa secção, no topo e sem o header dela (é o que o --only faz): a
// comparação de uma secção de cada vez sem andar à roda dentro do painel.
bool setOnlySection(const char* name);

// ── As faces do atlas (secção "Fonts") ───────────────────────────────────────
// O atlas do painel leva mais do que uma face (FontAtlas::loadFont) e o demo
// mostra-as: o combo "Font" escolhe a que o painel inteiro desenha - o
// io.FontDefault do ImGui - e a lista de fontes desenha uma amostra de cada uma
// no tamanho em que foi cozida. Quem carrega as faces é o main.cpp, que é quem
// tem o atlas e os ficheiros.
struct DemoFont
{
    ig::FontId id;
    // "Cousine 13px", escrito no loadDemoFonts.
    char name[48];
    float size;

    DemoFont()
            : id(),
              name(),
              size(0.0f)
    {
    }
};

const int kMaxDemoFonts = 8;
extern DemoFont gDemoFonts[kMaxDemoFonts];
extern int gDemoFontCount;

// A face que o painel desenha. Zero é "a do atlas" (a primeira, a do painel).
// Como no ImGui, a escolha do combo só vale a partir do frame seguinte.
extern ig::FontId gFontDefault;

// Carrega no atlas as faces extra do demo (as mesmas que o ImGui carrega, ver
// gallery::demoFontFiles) a partir de directory, e enche o gDemoFonts. A face 0
// é a do painel (a que o atlas já tinha). O que faltar - o diretório, um
// ficheiro, uma face que não coza - é ignorado, e o painel fica com o que
// houver.
void loadDemoFonts(ig::FontAtlas& atlas, const char* directory);

} // namespace igui_gallery
