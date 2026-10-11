#pragma once

#include <math.h>
#include <stdint.h>

// Espelho do demo do ImGui.
//
// Este exemplo põe o demo window verdadeiro (ImGui::ShowDemoWindow(), de
// imgui_demo.cpp) de um lado e, do outro, o mesmo demo reescrito com o iGUI.
//
// A lista de secções abaixo é a estrutura do ShowDemoWindow: os CollapsingHeader
// de topo e, dentro de "Widgets", as sub-secções que o demo abre com TreeNode.
// As duas implementações percorrem esta lista pela mesma ordem, por isso o que se
// vê de um lado corresponde ao que se vê do outro. O que o iGUI não souber
// exprimir é registado com reportGap() e sai no relatório do fim, em vez de
// desaparecer sem se dar por isso.
namespace gallery
{

// Altura da faixa de números no topo de cada painel. Os dois lados usam a mesma,
// para o conteúdo dos dois começar à mesma altura.
const float kReadoutHeight = 92.0f;

// Tamanho de letra dos dois painéis: o mesmo do estilo por omissão do ImGui. O
// atlas do iGUI é cozido neste tamanho (um atlas de 16 px desenhado a 13 sai
// desfocado), e o resto das métricas segue-o em igui_gallery::applyDemoStyle.
const float kFontSize = 13.0f;

// A linha de boas-vindas do demo ("dear imgui says hello! (1.92.5) (19258)"),
// escrita pelo main.cpp com a versão do ImGui que está a ser espelhada, para os
// dois lados mostrarem o mesmo texto sem o espelho ter de saber a versão.
extern char gHelloLine[96];


enum class Section
{
    // ── Topo do demo ─────────────────────────────────────────────────────────
    Help,               // CollapsingHeader("Help")
    Configuration,      // CollapsingHeader("Configuration")
    WindowOptions,      // CollapsingHeader("Window options")
    // ── Dentro de CollapsingHeader("Widgets") ────────────────────────────────
    WidgetsBasic,               // TreeNode("Basic")
    WidgetsBullets,             //   "Bullets"
    WidgetsCollapsingHeaders,   //   "Collapsing Headers"
    WidgetsColorAndPickers,     //   "Color/Picker Widgets"
    WidgetsCombo,               //   "Combo"
    WidgetsDataTypes,           //   "Data Types"
    WidgetsDisableBlocks,       //   "Disable Blocks"
    WidgetsDragAndDrop,         //   "Drag and Drop"
    WidgetsDragAndSliderFlags,  //   "Drag/Slider Flags"
    WidgetsFonts,               //   "Fonts"
    WidgetsImages,              //   "Images"
    WidgetsListBoxes,           //   "List Boxes"
    WidgetsMultiComponents,     //   "Multi-component Widgets"
    WidgetsPlotting,            //   "Plotting"
    WidgetsProgressBars,        //   "Progress Bars"
    WidgetsQueryingStatuses,    //   "Querying Item Status" / "Querying Window Status"
    WidgetsSelectables,         //   "Selectables"
    WidgetsSelectionAndMultiSelect, // "Selection State & Multi-Select"
    WidgetsTabs,                //   "Tabs"
    WidgetsTextInput,           //   "Text Input" / multi-line
    WidgetsTrees,               //   "Trees"
    WidgetsTooltips,            //   "Tooltips"
    // ── Restantes CollapsingHeader de topo ───────────────────────────────────
    LayoutAndScrolling,         // CollapsingHeader("Layout & Scrolling")
    PopupsAndModalWindows,      // CollapsingHeader("Popups & Modal Windows")
    TablesAndColumns,           // CollapsingHeader("Tables & Columns")
    InputsNavigationAndFocus,   // CollapsingHeader("Inputs, Navigation & Focus")
    Count
};

inline const Section *sections()
{
    static const Section list[] = {
        Section::Help, Section::Configuration, Section::WindowOptions,
        Section::WidgetsBasic, Section::WidgetsBullets, Section::WidgetsCollapsingHeaders,
        Section::WidgetsColorAndPickers, Section::WidgetsCombo, Section::WidgetsDataTypes,
        Section::WidgetsDisableBlocks, Section::WidgetsDragAndDrop,
        Section::WidgetsDragAndSliderFlags, Section::WidgetsFonts, Section::WidgetsImages,
        Section::WidgetsListBoxes, Section::WidgetsMultiComponents, Section::WidgetsPlotting,
        Section::WidgetsProgressBars, Section::WidgetsQueryingStatuses,
        Section::WidgetsSelectables, Section::WidgetsSelectionAndMultiSelect, Section::WidgetsTabs,
        Section::WidgetsTextInput, Section::WidgetsTrees, Section::WidgetsTooltips,
        Section::LayoutAndScrolling, Section::PopupsAndModalWindows, Section::TablesAndColumns,
        Section::InputsNavigationAndFocus
    };
    static_assert(sizeof(list) / sizeof(list[0]) == static_cast<unsigned>(Section::Count),
                  "sections() tem de cobrir todas as secoes");
    return list;
}

inline int sectionCount()
{
    return static_cast<int>(Section::Count);
}

// As sub-secções de "Widgets" ficam dentro desse CollapsingHeader no demo, e o
// espelho tem de as aninhar da mesma maneira.
inline bool insideWidgets(Section section)
{
    return section >= Section::WidgetsBasic && section <= Section::WidgetsTooltips;
}

inline const char *sectionName(Section section)
{
    switch (section)
    {
    case Section::Help: return "Help";
    case Section::Configuration: return "Configuration";
    case Section::WindowOptions: return "Window options";
    case Section::WidgetsBasic: return "Basic";
    case Section::WidgetsBullets: return "Bullets";
    case Section::WidgetsCollapsingHeaders: return "Collapsing Headers";
    case Section::WidgetsColorAndPickers: return "Color/Picker Widgets";
    case Section::WidgetsCombo: return "Combo";
    case Section::WidgetsDataTypes: return "Data Types";
    case Section::WidgetsDisableBlocks: return "Disable Blocks";
    case Section::WidgetsDragAndDrop: return "Drag and Drop";
    case Section::WidgetsDragAndSliderFlags: return "Drag/Slider Flags";
    case Section::WidgetsFonts: return "Fonts";
    case Section::WidgetsImages: return "Images";
    case Section::WidgetsListBoxes: return "List Boxes";
    case Section::WidgetsMultiComponents: return "Multi-component Widgets";
    case Section::WidgetsPlotting: return "Plotting";
    case Section::WidgetsProgressBars: return "Progress Bars";
    case Section::WidgetsQueryingStatuses: return "Querying Item Status";
    case Section::WidgetsSelectables: return "Selectables";
    case Section::WidgetsSelectionAndMultiSelect: return "Selection State & Multi-Select";
    case Section::WidgetsTabs: return "Tabs";
    case Section::WidgetsTextInput: return "Text Input";
    case Section::WidgetsTrees: return "Trees";
    case Section::WidgetsTooltips: return "Tooltips";
    case Section::LayoutAndScrolling: return "Layout & Scrolling";
    case Section::PopupsAndModalWindows: return "Popups & Modal Windows";
    case Section::TablesAndColumns: return "Tables & Columns";
    case Section::InputsNavigationAndFocus: return "Inputs, Navigation & Focus";
    case Section::Count: break;
    }
    return "?";
}

// O estado que as widgets mostram. Só tipos primitivos e as strings em C: os dois
// lados lêem o mesmo conteúdo (um com ig::String, o outro com char[]).
struct State
{
    bool flag[8];
    int radio;
    int combo;
    // A escolha dos dois combos de conteudo livre (o "combo 1" e o "combo 2
    // (w/ filter)" do demo).
    int comboItem;
    int listBox;
    int tab;
    int i1;
    int i2;
    float f1;
    float f2;
    float vec2[2];
    float vec3[3];
    float vec4[4];
    float rangeMin;
    float rangeMax;
    float col3[3];
    float col4[4];
    float colHdr[3];
    float vSlider;
    // Os VSliderFloat do espelho: três faixas verticais, cada uma com o seu valor.
    float vSliders[3];
    float progress;
    // A barra anima entre -0.1 e 1.1, como a do demo.
    float progressDirection;
    // PlotLines/PlotHistogram: as 7 amostras fixas do demo, a serie em anel (com
    // o índice mais antigo) e os dois geradores Sin/Saw do "Functions".
    float plotFrames[7];
    float plotLines[32];
    int plotOffset;
    int plotFunc;
    int plotCount;
    float plotFuncValues[400];
    float plotPhase;
    bool plotAnimate;
    float plotBars[16];
    bool treeOpen;
    bool headerOpen;
    bool headerWithClose;
    bool disableAll;
    bool option[6];
    char text[64];
    char textMultiline[256];
    char textHint[64];
    // Secção "Querying Status": o tipo de item a submeter, o estado do demo
    // (Item Disabled) e os valores que os itens submetidos usam.
    int queryType;
    bool queryDisabled;
    bool queryBool;
    bool queryTreeOpen;
    int queryCombo;
    float queryFloat;

    State()
        : flag(), radio(0), combo(1), comboItem(0), listBox(0), tab(0), i1(1), i2(4), f1(0.5f), f2(1.0f),
          rangeMin(0.2f), rangeMax(0.8f), vSlider(0.4f), vSliders(), progress(0.5f),
          progressDirection(1.0f), plotOffset(0), plotFunc(0), plotCount(70), plotPhase(0.0f),
          plotAnimate(true), treeOpen(true),
          headerOpen(true), headerWithClose(true), disableAll(false), option(),
          queryType(3), queryDisabled(false), queryBool(false), queryTreeOpen(true),
          queryCombo(1), queryFloat(0.5f)
    {
        for (int i = 0; i < 8; ++i)
            flag[i] = (i % 2) == 0;
        vec2[0] = 0.10f; vec2[1] = 0.20f;
        vec3[0] = 0.10f; vec3[1] = 0.20f; vec3[2] = 0.30f;
        vec4[0] = 0.10f; vec4[1] = 0.20f; vec4[2] = 0.30f; vec4[3] = 0.40f;
        col3[0] = 0.30f; col3[1] = 0.60f; col3[2] = 0.90f;
        col4[0] = 0.30f; col4[1] = 0.60f; col4[2] = 0.90f; col4[3] = 1.0f;
        colHdr[0] = 0.20f; colHdr[1] = 0.40f; colHdr[2] = 0.60f;
        vSliders[0] = 0.60f; vSliders[1] = 0.35f; vSliders[2] = 0.90f;
        const float frames[7] = {0.6f, 0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f};
        for (int i = 0; i < 7; ++i)
            plotFrames[i] = frames[i];
        for (int i = 0; i < 32; ++i)
            plotLines[i] = cosf(static_cast<float>(i) * 0.35f);
        for (int i = 0; i < 400; ++i)
            plotFuncValues[i] = 0.0f;
        for (int i = 0; i < 16; ++i)
            plotBars[i] = 0.2f + 0.05f * static_cast<float>(i % 9);
        for (int i = 0; i < 64; ++i)
            text[i] = '\0';
        for (int i = 0; i < 256; ++i)
            textMultiline[i] = '\0';
        for (int i = 0; i < 64; ++i)
            textHint[i] = '\0';
    }
};

// O que cada lado fez num frame. O iGUI conta os itens que submeteu; o demo do
// ImGui não tem como ser contado por item, por isso o que os dois lados têm em
// comum é a geometria: vértices e draw calls.
struct FrameStats
{
    int items;
    int vertices;
    int drawCalls;
    int gaps;

    FrameStats() : items(0), vertices(0), drawCalls(0), gaps(0) {}
};

// Uma coisa que o demo do ImGui tem nesta secção e o iGUI não sabe exprimir.
struct Gap
{
    const char *section;
    const char *widget;
};

void reportGap(const char *section, const char *widget);
int gapCount();
const Gap &gap(int index);
void clearGaps();

// Os números do painel de topo, desenhados pela própria biblioteca de cada lado
// (é a única forma de ter painéis iguais sem uma terceira biblioteca por cima). A
// aplicação preenche isto antes de cada lado e desenha-o FORA da medição: mede-se
// o demo, não o relatório.
struct Readout
{
    const float *history; // tempos de submit, em ms, mais antigo primeiro
    int historyCount;
    float submitMs;
    float renderMs;
    float gpuMs;
    float hz;
    int items;
    int vertices;
    int drawCalls;
    int gaps;

    Readout()
        : history(nullptr), historyCount(0), submitMs(0.0f), renderMs(0.0f), gpuMs(0.0f), hz(0.0f),
          items(0), vertices(0), drawCalls(0), gaps(0)
    {
    }
};

void setReadout(const Readout &readout);
const Readout &readout();

} // namespace gallery
