// O demo do ImGui reescrito com o iGUI.
//
// Cada secção corresponde à do ShowDemoWindow (ver Gallery.hpp): os mesmos
// títulos, na mesma ordem, e dentro de cada uma as mesmas widgets que o iGUI
// sabe exprimir. O que ele não tem fica registado com gallery::reportGap(), que é
// o que a aplicação escreve no relatório - é assim que uma falta aparece em vez
// de passar despercebida.
//
// As contagens de itens por secção estão nos comentários; servem para se ver que
// o espelho acompanha o demo, secção a secção.

#include "Gallery.hpp"
#include "IguiGallery.hpp"

#include <igui/Gui.hpp>

#include <stdio.h>

namespace igui_gallery
{
namespace
{

const ig::StringView kItems5[] = {
    ig::StringView("AAAA"), ig::StringView("BBBB"), ig::StringView("CCCC"),
    ig::StringView("DDDD"), ig::StringView("EEEE")
};

const char *const kItems5Text[] = {"AAAA", "BBBB", "CCCC", "DDDD", "EEEE"};

const ig::StringView kPlotFunctions[] = {ig::StringView("Sin"), ig::StringView("Saw")};

// O mesmo que o ImGuiTextFilter do demo: uma linha de texto, e um item passa se
// a linha for um pedaço dele, sem olhar a maiúsculas.
bool passesFilter(ig::StringView item, const ig::String &filter)
{
    if (filter.empty())
        return true;
    const char lower = 'a' - 'A';
    for (ig::StringView::size_type i = 0u; i + filter.size() <= item.size(); ++i)
    {
        ig::StringView::size_type j = 0u;
        while (j < filter.size())
        {
            char a = item[i + j];
            char b = filter[j];
            if (a >= 'A' && a <= 'Z')
                a = static_cast<char>(a + lower);
            if (b >= 'A' && b <= 'Z')
                b = static_cast<char>(b + lower);
            if (a != b)
                break;
            ++j;
        }
        if (j == filter.size())
            return true;
    }
    return false;
}

struct Panel
{
    gallery::State shared;
    ig::String name;
    ig::String multiline;
    ig::String hint;
    ig::String queryText;
    // O filtro do "combo 2 (w/ filter)" do demo.
    ig::String comboFilter;
    // Que colunas da tabela "Hideable" estao visiveis (o menu de contexto do
    // cabecalho troca-as, como no demo).
    bool tableColumns[3];
    bool dialogOpen;
    bool modalOpen;
    // A messageBox do iGUI em separado do popup modal: são duas widgets diferentes
    // (o demo do ImGui já não tem a caixa de OK/Cancelar, esta é do iGUI) e um clique
    // num botão não pode deixar o outro por abrir.
    bool messageBoxOpen;
    // Os flags da barra de tabs que o demo expõe em checkboxes (o iGUI não tem o
    // CheckboxFlags, por isso é um bool por flag).
    bool tabFlagReorderable;
    bool tabFlagAutoSelect;
    bool tabFlagListPopup;
    bool tabFlagNoCloseMiddle;
    bool tabFlagOverline;
    bool tabFlagFitting;
    // As tabs "Opened:" do demo, que se abrem e fecham pelos checkboxes.
    bool openedTabs[4];
    // As tabs numeradas do "TabItemButton & Leading/Trailing": quem as cria é o "+".
    int activeTabs[8];
    int activeTabCount;
    int nextTabId;
    bool showLeadingTabButton;
    bool showTrailingTabButton;
    bool selectAppearance;
    // Um estado de expansao proprio para o header com botao de fecho: o demo do
    // ImGui guarda a expansao dentro da widget e so o "open" e que vem de fora.
    bool headerCloseExpanded;
    bool headerBulletOpen;
    int selection;
    int sortColumn;
    bool sortAscending;
    int widgets;
    // Delta do frame, para as secções animadas (barras de progresso).
    float deltaSeconds;
    // Um estado por secção: um bool partilhado por todos os headers abria-os e
    // fechava-os todos ao mesmo tempo. O demo abre o "Help" e deixa o resto
    // fechado, por isso é esse o estado inicial.
    bool topOpen[static_cast<int>(gallery::Section::Count)];
    bool subOpen[static_cast<int>(gallery::Section::Count)];

    Panel()
        : shared(), name("texto editavel"), multiline("linha um\nlinha dois"), hint(),
          queryText("texto"), comboFilter(), dialogOpen(false), modalOpen(false),
          messageBoxOpen(false),
          tabFlagReorderable(true), tabFlagAutoSelect(false), tabFlagListPopup(false),
          tabFlagNoCloseMiddle(false), tabFlagOverline(false), tabFlagFitting(false),
          openedTabs(), activeTabs(), activeTabCount(0), nextTabId(0),
          showLeadingTabButton(true), showTrailingTabButton(true),
          selectAppearance(false),
          headerCloseExpanded(true), headerBulletOpen(false),
          selection(0),
          sortColumn(0), sortAscending(true), widgets(0), deltaSeconds(0.0f), topOpen(),
          subOpen()
    {
        // As quatro tabs do "Opened:" do demo nascem abertas, e as numeradas começam
        // com três (é o que o demo faz na primeira vez).
        for (int i = 0; i < 4; ++i)
            openedTabs[i] = true;
        for (int i = 0; i < 3; ++i)
            activeTabs[i] = nextTabId++;
        activeTabCount = 3;
        // Tudo fechado, como o demo do ImGui abre por omissão: enquanto o
        // espelho abria o "Help" e o demo não, os dois painéis desenhavam textos
        // diferentes e a contagem de vértices comparava o "Help" (onze bullets)
        // com nada. Quem quiser as duas versões com tudo aberto usa o --open-all
        // (que só abre o lado do iGUI, e por isso deixa de ser comparável).
    }
};

Panel g_panel;

// O FontSizeBase do demo (ver a secção "Fonts"): o tamanho pedido para o texto,
// à parte do tema.
float g_fontSize = gallery::kFontSize;
// O tamanho do atlas, só para a secção "Fonts" o dizer.
unsigned g_atlasWidth = 0u;
unsigned g_atlasHeight = 0u;

void item()
{
    ++g_panel.widgets;
}

// Uma widget do demo que o iGUI não tem: nota no sítio dela e lacuna no relatório.
// Continua a contar como item, porque no demo ela está lá.
void gap(ig::Context &ui, gallery::Section section, const char *missing)
{
    gallery::reportGap(gallery::sectionName(section), missing);
    ui.label("[iGUI: nao existe]");
    item();
}

// ── Help ─────────────────────────────────────────────────────────────────────
// O texto é o do demo, palavra por palavra: é a secção que fica aberta por
// omissão, é a que se lê nos dois painéis ao mesmo tempo, e enquanto tinha prosa
// em português a contagem de vértices deste lado comparava dois textos
// diferentes em vez de duas bibliotecas.
void sectionHelp(ig::Context &ui, gallery::State &s)
{
    (void)s;
    ui.separatorText("ABOUT THIS DEMO:");
    item();
    ui.bulletText("Sections below are demonstrating many aspects of the library.");
    item();
    ui.bulletText("The \"Examples\" menu above leads to more demo contents.");
    item();
    ui.bulletText("The \"Tools\" menu above gives access to: About Box, Style Editor,\n"
                  "and Metrics/Debugger (general purpose Dear ImGui debugging tool).");
    item();
    ui.separatorText("PROGRAMMER GUIDE:");
    item();
    ui.bulletText("See the ShowDemoWindow() code in imgui_demo.cpp. <- you are here!");
    item();
    ui.bulletText("See comments in imgui.cpp.");
    item();
    ui.bulletText("See example applications in the examples/ folder.");
    item();
    ui.bulletText("Read the FAQ at ");
    ui.sameLine(0.0f);
    // O TextLinkOpenURL do ImGui abre o browser; o iGUI não tem links, por isso
    // fica o texto na cor de link.
    ui.textColored("https://www.dearimgui.com/faq/", ig::Color(102u, 153u, 255u, 255u));
    item();
    ui.bulletText("Set 'io.ConfigFlags |= NavEnableKeyboard' for keyboard controls.");
    item();
    ui.bulletText("Set 'io.ConfigFlags |= NavEnableGamepad' for gamepad controls.");
    item();
    ui.separatorText("USER GUIDE:");
    item();
    gap(ui, gallery::Section::Help, "ShowUserGuide() (as tabelas de ajuda do demo, "
                                    "e o TextLinkOpenURL)");
}

// ── Configuration ────────────────────────────────────────────────────────────
void sectionConfiguration(ig::Context &ui, gallery::State &s)
{
    gap(ui, gallery::Section::Configuration,
        "os flags do ImGuiIO (ConfigFlags, BackendFlags, DPI, config de janelas, "
        "tratamento de erros e debug) - o iGUI nao tem um objeto de configuracao global");
    ui.separatorText("General");
    ui.checkbox("O teu primeiro checkbox", s.flag[0]);
    item();
    ui.checkbox("O teu segundo checkbox", s.flag[1]);
    item();
    ui.sliderFloat("Um slider", s.f1, 0.0f, 1.0f);
    item();
    gap(ui, gallery::Section::Configuration, "Style Editor / Metrics / About / Debug log");
}

// ── Window options ───────────────────────────────────────────────────────────
void sectionWindowOptions(ig::Context &ui, gallery::State &s)
{
    gap(ui, gallery::Section::WindowOptions,
        "os controlos de janela do demo (posicao, tamanho, collapsed, focus, bg alpha, "
        "scroll, limites) - o iGUI gere a janela pelo beginWindow");
    ui.checkbox("Flag da janela 1", s.flag[2]);
    item();
    ui.checkbox("Flag da janela 2", s.flag[3]);
    item();
    ui.label("(o demo usa aqui dezenas de checkboxes de style/IO flags)");
    item();
}

// ── Widgets: Basic ───────────────────────────────────────────────────────────
void sectionWidgetsBasic(ig::Context &ui, gallery::State &s)
{
    ui.separatorText("General");
    ui.label("This is a text");
    item();
    ui.textColored(ig::StringView("This is a colored text"), ig::Color(120u, 200u, 255u));
    item();
    ui.textDisabled(ig::StringView("This is a disabled text"));
    item();
    ui.textWrapped(ig::StringView(
        "The ImGui::TextWrapped() function is used to write long text that wraps around the "
        "window. The text wraps at the right edge of the content area, without a newline in "
        "the source."));
    item();
    ui.label("LabelText:");
    ui.sameLine();
    ui.label("valor");
    item();
    ui.bulletText(ig::StringView("BulletText"));
    item();
    if (ui.button("Button"))
        s.f1 = 1.0f;
    item();
    if (ui.smallButton("SmallButton"))
        s.f1 = 0.0f;
    item();
    {
        const ig::Vec2 cursor = ui.cursor();
        ui.invisibleButton("InvisibleButton", ig::Rect(cursor.x, cursor.y, 80.0f, 22.0f));
        ui.drawRect(ig::Rect(cursor.x, cursor.y, 80.0f, 22.0f), ig::Color(120u, 120u, 140u));
        ui.dummy(80.0f, 22.0f);
    }
    item();
    // O "Hold to repeat:" do demo: o texto alinhado com a moldura dos botões, os
    // dois arrowButton e o contador. Os CheckboxFlags do demo estão nas secções
    // dos flags que eles trocam (io.ConfigFlags, ColorEdit, Combo, Slider), que
    // continuam com a lacuna delas.
    ui.alignTextToFramePadding();
    ui.label("Hold to repeat:");
    ui.sameLine();
    if (ui.arrowButton("##left", ig::ArrowDirection::Left))
        --s.steps;
    item();
    ui.sameLine();
    if (ui.arrowButton("##right", ig::ArrowDirection::Right))
        ++s.steps;
    item();
    ui.sameLine();
    {
        char counter[16];
        snprintf(counter, sizeof(counter), "%d", s.steps);
        ui.label(ig::StringView(counter, strlen(counter)));
    }
    item();
    ui.checkbox("Checkbox", s.flag[0]);
    item();
    if (ui.radioButton("RadioButton a", s.radio == 0))
        s.radio = 0;
    item();
    if (ui.radioButton("RadioButton b", s.radio == 1))
        s.radio = 1;
    item();
    ui.progressBar(s.progress, 1.0f);
    item();

    ui.separatorText("Inputs");
    ui.inputFloat("InputFloat", s.f1, 0.0f, 6);
    item();
    ui.inputFloat3("InputFloat3", s.vec3[0], s.vec3[1], s.vec3[2], 0.0f, 3);
    item();
    ui.inputInt("InputInt", s.i1, 0.0f);
    item();
    gap(ui, gallery::Section::WidgetsBasic, "InputDouble");
    ui.inputText("InputText", g_panel.name);
    item();

    ui.separatorText("Drags");
    ui.dragFloat("DragFloat", s.f1, 0.0f, 1.0f, 0.01f);
    item();
    ui.dragFloat4("DragFloat4", s.vec4[0], s.vec4[1], s.vec4[2], s.vec4[3], 0.0f, 1.0f, 0.01f);
    item();
    ui.dragInt("DragInt", s.i1, 0, 100, 1);
    item();
    gap(ui, gallery::Section::WidgetsBasic, "DragScalar");

    ui.separatorText("Sliders");
    ui.sliderFloat("SliderFloat", s.f1, 0.0f, 1.0f);
    item();
    ui.sliderInt("SliderInt", s.i2, 0, 10);
    item();
    {
        const ig::Vec2 cursor = ui.cursor();
        ui.fader("Fader", s.vSlider, 0.0f, 1.0f, ig::Rect(cursor.x, cursor.y, 40.0f, 100.0f));
        ui.dummy(40.0f, 100.0f);
    }
    item();
    {
        // VSliderFloat/VSliderInt: a faixa estreita do demo (18 px de largura, o
        // valor escrito por cima do grab), em fila e com o rótulo escondido por
        // "##v"/"##int" - como lá. O pushId é o que dá um id a cada uma.
        const ig::Vec2 cursor = ui.cursor();
        const float width = 18.0f;
        const float height = 120.0f;
        const float stride = 40.0f;
        ui.pushId("set1");
        ui.vSliderInt("##int", s.i2, 0, 10, ig::Rect(cursor.x, cursor.y, width, height));
        ui.pushId(0);
        ui.vSliderFloat("##v", s.vSliders[0], 0.0f, 1.0f,
                        ig::Rect(cursor.x + stride, cursor.y, width, height));
        ui.popId();
        ui.pushId(1);
        ui.vSliderFloat("##v", s.vSliders[1], 0.0f, 1.0f,
                        ig::Rect(cursor.x + stride * 2.0f, cursor.y, width, height));
        ui.popId();
        ui.pushId(2);
        ui.vSliderFloat("##v", s.vSliders[2], 0.0f, 1.0f,
                        ig::Rect(cursor.x + stride * 3.0f, cursor.y, width, height));
        ui.popId();
        ui.popId();
        // Os quatro sliders de cima são de bounds: sem este dummy o sameLine()
        // seguinte ainda apontava para o fader, e as colunas saíam por cima.
        ui.dummy(stride * 3.0f + width, height);

        // set2 do demo: quatro colunas de três faixas baixas, cada coluna um
        // grupo - é o que o BeginGroup/EndGroup existe para fazer. Uma coluna
        // começa onde o grupo anterior acabou, não onde acabou a última faixa.
        const float smallHeight = (120.0f - 2.0f * 4.0f) / 3.0f;
        ui.sameLine();
        ui.pushId("set2");
        for (int nx = 0; nx < 4; ++nx)
        {
            if (nx > 0)
                ui.sameLine();
            ui.beginGroup();
            for (int ny = 0; ny < 3; ++ny)
            {
                ui.pushId(nx * 3 + ny);
                ui.vSliderFloat("##v", s.vSliders[nx % 3], 0.0f, 1.0f,
                                ig::Rect(ui.cursor().x, ui.cursor().y, width, smallHeight));
                item();
                ui.dummy(width, smallHeight);
                ui.popId();
            }
            ui.endGroup();
        }
        ui.popId();
    }
    item();
    item();
    item();
    item();
    gap(ui, gallery::Section::WidgetsBasic,
        "SliderAngle / SliderScalar, o formato do valor do VSliderFloat (\"%.2f\\nsec\") "
        "e o GrabMinSize que muda a altura do grab");

    ui.separatorText("Selectors/Pickers");
    ig::Color color = ig::Color::FromFloat(s.col4[0], s.col4[1], s.col4[2], s.col4[3]);
    ui.colorEdit("ColorEdit", color, 0.0f, 24.0f);
    s.col4[0] = static_cast<float>(color.r) / 255.0f;
    s.col4[1] = static_cast<float>(color.g) / 255.0f;
    s.col4[2] = static_cast<float>(color.b) / 255.0f;
    s.col4[3] = static_cast<float>(color.a) / 255.0f;
    item();
    ui.label("Color button:");
    item();
    ui.colorButton("MyColor 1", color);
    item();
    if (ui.colorButton("MyColor 2", color, 90.0f, 40.0f))
        s.f1 = 0.5f;
    item();
    gap(ui, gallery::Section::WidgetsBasic, "ColorEdit flags (RGB, HSV, Alpha bar)");
    ui.comboBox("Combo", s.combo, ig::Span<const ig::StringView>(kItems5, 5));
    item();
}

// ── Widgets: Bullets ─────────────────────────────────────────────────────────
void sectionWidgetsBullets(ig::Context &ui, gallery::State &s)
{
    (void)s;
    ui.bulletText(ig::StringView("Bullet point 1"));
    item();
    ui.bulletText(ig::StringView("Bullet point 2"));
    item();
    ui.bulletText(ig::StringView("Bullet point 3 (two lines) with a bullet that wraps around "
                                 "the content area so the second line lines up under the first"));
    item();
    // O TreeNode de bullet do demo: a marca é o ponto, e a linha abre e fecha na
    // mesma (o TreeStyle::bullet).
    ig::TreeStyle bulletStyle;
    bulletStyle.bullet = true;
    static bool bullet1Open = false;
    static bool bullet2Open = false;
    if (ui.collapsingHeader("Bullet 2", bullet1Open, bulletStyle, 200.0f))
    {
        ui.bulletText(ig::StringView("conteudo do bullet 2"));
        item();
    }
    if (ui.treeNode("Bullet 2.1", bullet2Open, bulletStyle, 200.0f))
    {
        ui.bulletText(ig::StringView("conteudo do bullet 2.1"));
        item();
    }
    gap(ui, gallery::Section::WidgetsBullets,
        "o BulletText sem bullet (o TextColored a imitar a marca do demo)");
}

// ── Widgets: Collapsing Headers ──────────────────────────────────────────────
void sectionWidgetsCollapsingHeaders(ig::Context &ui, gallery::State &s)
{
    if (ui.collapsingHeader("Header", s.headerOpen))
        ui.label("conteudo do header");
    item();
    if (ui.collapsingHeader("Header with a close button", g_panel.headerCloseExpanded,
                            s.headerWithClose))
    {
        ui.label("fecha pelo x ao lado");
        item();
    }
    item();
    // O "Header with a bullet" do demo, com o TreeStyle.
    ig::TreeStyle bulletHeader;
    bulletHeader.bullet = true;
    if (ui.collapsingHeader("Header with a bullet", g_panel.headerBulletOpen, bulletHeader, 200.0f))
    {
        ui.label("conteudo do header com bullet");
        item();
    }
    gap(ui, gallery::Section::WidgetsCollapsingHeaders,
        "CollapsingHeader com o estado do botao de fecho devolvido por ImGuiTreeNodeFlags e "
        "os flags que faltam (ImGuiTreeNodeFlags_DefaultOpen/FramePadding/NoTreePushOnOpen)");
}

// ── Widgets: Color/Picker ────────────────────────────────────────────────────
void sectionWidgetsColorAndPickers(ig::Context &ui, gallery::State &s)
{
    ui.separatorText("Options");
    gap(ui, gallery::Section::WidgetsColorAndPickers,
        "os checkboxes de ColorEditFlags do demo (Alpha, Sub, Input RGB/HSV/HEX, "
        "Picker HueWheel/Bar, etc.)");
    ui.checkbox("ColorEdit option 1", s.option[0]);
    item();
    ui.checkbox("ColorEdit option 2", s.option[1]);
    item();
    ui.separatorText("Inline color editor");
    ig::Color c3 = ig::Color::FromFloat(s.col3[0], s.col3[1], s.col3[2]);
    ui.colorEdit("ColorEdit3/4", c3, 0.0f, 24.0f);
    s.col3[0] = static_cast<float>(c3.r) / 255.0f;
    s.col3[1] = static_cast<float>(c3.g) / 255.0f;
    s.col3[2] = static_cast<float>(c3.b) / 255.0f;
    item();
    ui.separatorText("Color picker");
    ui.colorEdit("ColorPicker", c3, 0.0f, 140.0f);
    item();
    ui.label("Color button:");
    item();
    if (ui.colorButton("color button", ig::Color(80u, 140u, 220u, 255u), 120.0f, 30.0f))
        s.f1 = 0.25f;
    item();
}

// ── Widgets: Combo ───────────────────────────────────────────────────────────
void sectionWidgetsCombo(ig::Context &ui, gallery::State &s)
{
    gap(ui, gallery::Section::WidgetsCombo, "ImGuiComboFlags_* (PopupAlignLeft, "
                                            "NoArrowButton, NoPreview, WidthFitPreview, Height*)");
    // Os 14 itens do demo, e o mesmo desenho: o cabeçalho é o beginCombo, o popup é
    // do chamador, que submete um selectable por item.
    static const ig::StringView kComboItems[] = {
        ig::StringView("AAAA"), ig::StringView("BBBB"), ig::StringView("CCCC"),
        ig::StringView("DDDD"), ig::StringView("EEEE"), ig::StringView("FFFF"),
        ig::StringView("GGGG"), ig::StringView("HHHH"), ig::StringView("IIII"),
        ig::StringView("JJJJ"), ig::StringView("KKKK"), ig::StringView("LLLLLLL"),
        ig::StringView("MMMM"), ig::StringView("OOOOOOO")
    };
    const int comboCount = static_cast<int>(sizeof(kComboItems) / sizeof(kComboItems[0]));
    if (s.comboItem < 0 || s.comboItem >= comboCount)
        s.comboItem = 0;
    // O HeightRegular do ImGui, que é o que vem por omissão: 8 linhas, e a lista
    // rola por dentro.
    if (ui.beginCombo("combo 1", kComboItems[s.comboItem], 240.0f))
    {
        for (int n = 0; n < comboCount; ++n)
        {
            if (ui.selectable(kComboItems[n], s.comboItem == n))
                s.comboItem = n;
            item();
            // Como no demo: o item escolhido é o que a lista mostra ao abrir.
            if (s.comboItem == n)
                ui.setItemDefaultFocus();
        }
        ui.endCombo();
    }
    item();
    // O "combo 2 (w/ filter)" do demo: um campo de texto lá dentro a filtrar os
    // itens. É conteúdo livre - nada disto é do combo, é da aplicação.
    if (ui.beginCombo("combo 2 (w/ filter)", kComboItems[s.comboItem], 240.0f))
    {
        ui.inputText("##Filter", g_panel.comboFilter, 240.0f - 12.0f);
        item();
        for (int n = 0; n < comboCount; ++n)
        {
            if (!passesFilter(kComboItems[n], g_panel.comboFilter))
                continue;
            if (ui.selectable(kComboItems[n], s.comboItem == n))
                s.comboItem = n;
            item();
        }
        ui.endCombo();
    }
    item();
    ui.spacing(6.0f);
    ui.separatorText("One-liner variants");
    item();
    ui.comboBox("combo 3 (one-liner)", s.combo, ig::Span<const ig::StringView>(kItems5, 5));
    item();
    ui.comboBox("combo 4 (array)", s.combo, ig::Span<const ig::StringView>(kItems5, 5));
    item();
    ui.comboBox("combo 5 (function)", s.combo, ig::Span<const ig::StringView>(kItems5, 5));
    item();
}

// ── Widgets: Data Types ──────────────────────────────────────────────────────
void sectionWidgetsDataTypes(ig::Context &ui, gallery::State &s)
{
    ui.separatorText("Drags");
    ui.dragInt("int", s.i1, 0, 10, 1);
    item();
    ui.dragFloat("float", s.f1, 0.0f, 1.0f, 0.01f);
    item();
    ui.dragFloat2("vec2", s.vec2[0], s.vec2[1], 0.0f, 1.0f, 0.01f);
    item();
    ui.dragFloat3("vec3", s.vec3[0], s.vec3[1], s.vec3[2], 0.0f, 1.0f, 0.01f);
    item();
    ui.dragFloat4("vec4", s.vec4[0], s.vec4[1], s.vec4[2], s.vec4[3], 0.0f, 1.0f, 0.01f);
    item();
    ui.separatorText("Sliders");
    ui.sliderInt("slider int", s.i1, 0, 10);
    item();
    ui.sliderFloat("slider float", s.f1, 0.0f, 1.0f);
    item();
    ui.separatorText("Inputs");
    ui.inputInt("input int", s.i1, 0.0f);
    item();
    ui.inputFloat("input float", s.f1, 0.0f, 3);
    item();
    ui.inputFloat2("input vec2", s.vec2[0], s.vec2[1], 0.0f, 3);
    item();
    ui.inputFloat3("input vec3", s.vec3[0], s.vec3[1], s.vec3[2], 0.0f, 3);
    item();
    ui.inputFloat4("input vec4", s.vec4[0], s.vec4[1], s.vec4[2], s.vec4[3], 0.0f, 3);
    item();
    gap(ui, gallery::Section::WidgetsDataTypes,
        "int/unsigned/como vetores de N (DragInt2/3/4, InputInt2/3/4) e os drags "
        "logaritmicos/em wrap");
}

// ── Widgets: Disable Blocks ──────────────────────────────────────────────────
void sectionWidgetsDisableBlocks(ig::Context &ui, gallery::State &s)
{
    ui.beginDisabled();
    ui.checkbox("Checkbox inside a disable block", s.flag[0]);
    item();
    if (ui.button("Button inside a disable block"))
        s.f1 = 0.0f;
    item();
    ui.dragFloat("DragFloat inside a disable block", s.f1, 0.0f, 1.0f, 0.01f);
    item();
    // Um bloco dentro do outro: o interior so sai quando o de fora sai todo.
    ui.beginDisabled();
    ui.textDisabled(ig::StringView("TextDisabled inside the inner block"));
    item();
    ui.endDisabled();
    ui.textDisabled(ig::StringView("TextDisabled inside the outer block"));
    item();
    ui.endDisabled();
    ui.textDisabled(ig::StringView("TextDisabled after the block (volta ao normal)"));
    item();
}

// ── Widgets: Drag and Drop ───────────────────────────────────────────────────
void sectionWidgetsDragAndDrop(ig::Context &ui, gallery::State &s)
{
    (void)s;
    gap(ui, gallery::Section::WidgetsDragAndDrop,
        "os exemplos do demo (arrastar entre itens, copiar/trocar, reordenar) usam "
        "BeginDragDropSource/Target; o iGUI tem beginDragSource/acceptDragDropTarget "
        "mas nao tem preview nem payload tipado com flags");
    {
        const ig::Vec2 cursor = ui.cursor();
        const ig::Rect source(cursor.x, cursor.y, 110.0f, 22.0f);
        ui.drawRectFilled(source, ig::Color(70u, 90u, 130u));
        ui.label("arrasta-me", ig::Vec2(cursor.x + 6.0f, cursor.y + 2.0f));
        ui.dummy(110.0f, 22.0f);
        ui.beginDragSource(1u, 2u, 3u, ig::StringView("a arrastar"), source);
    }
    item();
    {
        const ig::Vec2 cursor = ui.cursor();
        const ig::Rect target(cursor.x, cursor.y, 110.0f, 22.0f);
        ui.drawRect(target, ig::Color(120u, 120u, 140u));
        ui.label("larga aqui", ig::Vec2(cursor.x + 6.0f, cursor.y + 2.0f));
        ui.dummy(110.0f, 22.0f);
        ig::DragDropPayload payload;
        ui.acceptDragDropTarget(2u, target, payload);
    }
    item();
}

// ── Widgets: Drag/Slider Flags ───────────────────────────────────────────────
void sectionWidgetsDragAndSliderFlags(ig::Context &ui, gallery::State &s)
{
    gap(ui, gallery::Section::WidgetsDragAndSliderFlags,
        "CheckboxFlags (o bitfield do demo: os flags do ImGuiSliderFlags_* sao um "
        "checkbox cada) e os flags ClampOnInput/ClampZeroRange/NoRoundToFormat/"
        "NoSpeedTweaks - o iGUI tem Logarithmic, WrapAround, NoInput e "
        "AllowOutOfRange");
    // Os drags/sliders do demo, com os flags que o iGUI tem. No demo os flags sao
    // os mesmos para todos (estao nos checkboxes de cima); aqui cada linha mostra
    // um deles, que é o que se vê de qualquer maneira.
    ui.label("Underlying float value: 0.500");
    item();
    ui.dragFloat("DragFloat (0 -> 1)", s.f1, 0.0f, 1.0f, 0.005f);
    item();
    ui.dragFloat("DragFloat (0 -> 1) WrapAround", s.f1, 0.0f, 1.0f, 0.005f, 0.0f,
                 ig::Context::SliderFlags::WrapAround);
    item();
    ui.dragInt("DragInt (0 -> 100)", s.i2, 0, 100, 1);
    item();
    ui.sliderFloat("SliderFloat (0 -> 1)", s.f1, 0.0f, 1.0f);
    item();
    ui.sliderFloat("SliderFloat (0.001 -> 100) Logarithmic", s.f1, 0.001f, 100.0f, 0.0f,
                   ig::Context::SliderFlags::Logarithmic);
    item();
    ui.sliderFloat("SliderFloat (0 -> 1) NoInput", s.f1, 0.0f, 1.0f, 0.0f,
                   ig::Context::SliderFlags::NoInput);
    item();
    ui.sliderInt("SliderInt (0 -> 100)", s.i2, 0, 100);
    item();
}

// ── Widgets: Fonts ───────────────────────────────────────────────────────────
void sectionWidgetsFonts(ig::Context &ui, gallery::State &s)
{
    (void)s;
    gap(ui, gallery::Section::WidgetsFonts,
        "o inspector do atlas do demo (o loader, compactar/esticar o atlas, as rects "
        "empacotadas, os glifos com zoom e o realce de cada rect) - o iGUI mostra as "
        "faces, as metricas e o combo, mas nao tem o inspetor");

    // O ShowFontSelector: escolhe a face de que o painel inteiro é desenhado (o
    // io.FontDefault do ImGui), a partir do frame seguinte.
    int selected = 0;
    for (int i = 0; i < gDemoFontCount; ++i)
        if (gDemoFonts[i].id == gFontDefault)
            selected = i;
    ig::StringView names[kMaxDemoFonts];
    for (int i = 0; i < gDemoFontCount; ++i)
        names[i] = ig::StringView(gDemoFonts[i].name);
    if (gDemoFontCount > 0 &&
        ui.comboBox("Font", selected, ig::Span<const ig::StringView>(names, gDemoFontCount)))
    {
        gFontDefault = gDemoFonts[selected].id;
    }
    item();

    // O FontSizeBase do demo: um tamanho para o texto todo, sem tocar no tema.
    // Aqui é o pushStyleVar do fontSize, que o atlas desenha escalado.
    float size = g_fontSize;
    if (ui.dragFloat("Font size", size, 5.0f, 100.0f, 0.2f))
        g_fontSize = size;
    ui.sameLine();
    {
        char out[32];
        snprintf(out, sizeof(out), " (out %.0f px)", size);
        ui.label(out);
    }
    item();
    ui.bulletText("a escala e da face, nao do atlas: um tamanho grande sai maior e mais "
                  "desfocado do que a mesma face cozida nesse tamanho");
    item();

    // A Face do painel num tamanho pedido (é o que a escala do fontSize faz).
    ui.pushStyleVar(&ig::Theme::fontSize, size);
    ui.label("Typography sample at the requested size");
    item();
    ui.popStyleVar();
    ui.pushStyleVar(&ig::Theme::fontSize, gallery::kFontSize);
    ui.label("Typography sample at the panel size");
    item();
    ui.popStyleVar();

    ui.separatorText("Font List");
    for (int i = 0; i < gDemoFontCount; ++i)
    {
        const DemoFont &face = gDemoFonts[i];
        char line[96];
        snprintf(line, sizeof(line), "%s - size %.0f px", face.name, face.size);
        ui.label(line);
        item();
        // A amostra sai na face e no tamanho em que ela foi cozida (o pushFont leva
        // o tamanho de fabrico da face): a 1:1, sem reescalonamento.
        ui.pushFont(face.id);
        ui.label("The quick brown fox jumps over the lazy dog 0123456789");
        item();
        const float lineHeight = face.size > 0.0f ? face.size : gallery::kFontSize;
        const float extra = lineHeight - ui.theme().widgetHeight;
        if (extra > 0.0f)
        {
            // O item do layout é da altura do tema, não da fonte em vigor: o que a
            // face tiver a mais tem de ser pedido, senão a linha seguinte passa-lhe
            // por cima.
            ui.dummy(0.0f, extra);
        }
        ui.popFont();
    }
    if (gDemoFontCount > 0)
    {
        char line[64];
        snprintf(line, sizeof(line), "atlas %ux%u, %d face(s)",
                 g_atlasWidth, g_atlasHeight, gDemoFontCount);
        ui.label(line);
        item();
    }
}

// ── Widgets: Images ──────────────────────────────────────────────────────────
void sectionWidgetsImages(ig::Context &ui, gallery::State &s)
{
    (void)s;
    gap(ui, gallery::Section::WidgetsImages,
        "a imagem de teste do demo com tabuleiro, border e tint - o iGUI tem image() "
        "e imageButton(), mas aqui nao ha uma textura para lhe dar");
    {
        const ig::Vec2 cursor = ui.cursor();
        ui.drawRectFilled(ig::Rect(cursor.x, cursor.y, 100.0f, 100.0f), ig::Color(60u, 64u, 72u));
        ui.drawRect(ig::Rect(cursor.x, cursor.y, 100.0f, 100.0f), ig::Color(120u, 120u, 140u));
        ui.dummy(100.0f, 100.0f);
    }
    item();
    gap(ui, gallery::Section::WidgetsImages,
        "ImageButton com tint e o exemplo de textura carregada do disco");
}

// ── Widgets: List Boxes ──────────────────────────────────────────────────────
void sectionWidgetsListBoxes(ig::Context &ui, gallery::State &s)
{
    ui.listBox("ListBox", s.listBox, ig::Span<const ig::StringView>(kItems5, 5), 0.0f, 4);
    item();
    gallery::reportGap(gallery::sectionName(gallery::Section::WidgetsListBoxes),
                       "ListBox com selecao multipla (ImGuiSelectionBasicStorage)");
    if (ui.beginChild("listbox multi", 76.0f))
    {
        for (int i = 0; i < 3; ++i)
            ui.selectable(kItems5[i], s.listBox == i + 1, 160.0f);
        ui.endChild();
    }
    item();
}

// ── Widgets: Multi-component ─────────────────────────────────────────────────
void sectionWidgetsMultiComponents(ig::Context &ui, gallery::State &s)
{
    ui.separatorText("2-wide");
    ui.sliderFloat("float2", s.vec2[0], 0.0f, 1.0f);
    ui.sameLine();
    ui.sliderFloat("float2b", s.vec2[1], 0.0f, 1.0f);  // o iGUI desenha sempre o rotulo
    item();
    ui.dragFloat2("drag float2", s.vec2[0], s.vec2[1], 0.0f, 1.0f, 0.01f);
    item();
    ui.inputFloat2("input float2", s.vec2[0], s.vec2[1], 0.0f, 3);
    item();
    ui.separatorText("3-wide");
    ui.sliderFloat("float3 x", s.vec3[0], 0.0f, 1.0f);
    item();
    ui.dragFloat3("drag float3", s.vec3[0], s.vec3[1], s.vec3[2], 0.0f, 1.0f, 0.01f);
    item();
    ui.inputFloat3("input float3", s.vec3[0], s.vec3[1], s.vec3[2], 0.0f, 3);
    item();
    ui.separatorText("4-wide");
    ui.dragFloat4("drag float4", s.vec4[0], s.vec4[1], s.vec4[2], s.vec4[3], 0.0f, 1.0f, 0.01f);
    item();
    ui.inputFloat4("input float4", s.vec4[0], s.vec4[1], s.vec4[2], s.vec4[3], 0.0f, 3);
    item();
    ui.separatorText("Ranges");
    gallery::reportGap(gallery::sectionName(gallery::Section::WidgetsMultiComponents),
                       "DragFloatRange2 / DragIntRange2 (min e max numa so widget)");
    ui.dragFloat("Min", s.rangeMin, 0.0f, 1.0f, 0.01f);
    item();
    ui.dragFloat("Max", s.rangeMax, 0.0f, 1.0f, 0.01f);
    item();
}

// ── Widgets: Plotting ────────────────────────────────────────────────────────
void sectionWidgetsPlotting(ig::Context &ui, gallery::State &s)
{
    ui.checkbox("Animate", s.plotAnimate);
    item();

    // As duas primeiras series do demo: 7 amostras fixas, uma com escala
    // automatica e outra com escala 0..1.
    ui.plotLines("Frame Times", s.plotFrames, 7);
    item();
    ui.plotHistogram("Histogram", s.plotFrames, 7, 0.0f, 80.0f, 0, ig::StringView(), 0.0f, 1.0f);
    item();

    // Serie continua num anel de 32 amostras: o offset diz qual e a mais antiga,
    // e a legenda mostra a media como no demo.
    if (s.plotAnimate)
    {
        s.plotLines[s.plotOffset] = cosf(s.plotPhase);
        s.plotOffset = (s.plotOffset + 1) % 32;
        s.plotPhase += 0.10f * static_cast<float>(s.plotOffset);
    }
    {
        float average = 0.0f;
        for (int i = 0; i < 32; ++i)
            average += s.plotLines[i];
        char overlay[32];
        snprintf(overlay, sizeof(overlay), "avg %f", static_cast<double>(average / 32.0f));
        ui.plotLines("Lines", s.plotLines, 32, 0.0f, 80.0f, s.plotOffset,
                     ig::StringView(overlay), -1.0f, 1.0f);
        item();
    }

    ui.separatorText("Functions");
    ui.comboBox("func", s.plotFunc, ig::Span<const ig::StringView>(kPlotFunctions, 2));
    item();
    ui.sliderInt("Sample count", s.plotCount, 1, 400);
    item();
    // Sin e Saw gerados por indice, como as duas funcoes do demo.
    {
        const int count = s.plotCount < 1 ? 1 : (s.plotCount > 400 ? 400 : s.plotCount);
        for (int i = 0; i < count; ++i)
            s.plotFuncValues[i] = s.plotFunc == 0
                                      ? static_cast<float>(sinf(static_cast<float>(i) * 0.1f))
                                      : ((i & 1) ? 1.0f : -1.0f);
        ui.plotLines("Sin/Saw lines", s.plotFuncValues, count, 0.0f, 80.0f, 0, ig::StringView(),
                     -1.0f, 1.0f);
        item();
        ui.plotHistogram("Sin/Saw histogram", s.plotFuncValues, count, 0.0f, 80.0f, 0,
                         ig::StringView(), -1.0f, 1.0f);
        item();
    }
}

// ── Widgets: Progress Bars ───────────────────────────────────────────────────
void sectionWidgetsProgressBars(ig::Context &ui, gallery::State &s)
{
    // O demo anima a barra entre -0.1 e 1.1 para mostrar o clamp das pontas.
    s.progress += s.progressDirection * 0.4f * g_panel.deltaSeconds;
    if (s.progress >= 1.1f)
    {
        s.progress = 1.1f;
        s.progressDirection = -1.0f;
    }
    if (s.progress <= -0.1f)
    {
        s.progress = -0.1f;
        s.progressDirection = 1.0f;
    }

    ui.progressBar(s.progress, 1.0f);
    item();
    {
        const float saturated = s.progress < 0.0f ? 0.0f : (s.progress > 1.0f ? 1.0f : s.progress);
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%d/1753", static_cast<int>(saturated * 1753.0f));
        ui.progressBar(s.progress, 1.0f, 0.0f, ig::StringView(buffer));
        item();
    }
    // value < 0 e a barra indeterminada: a faixa anda com o tempo do contexto.
    ui.progressBar(-1.0f, 1.0f, 0.0f, ig::StringView("Searching.."));
    item();
    ui.sliderFloat("progress", s.progress, -0.1f, 1.1f);
    item();
}

// ── Widgets: Querying statuses ───────────────────────────────────────────────
// A secção do demo que responde a "o que se passou com o último item": submete
// um item do tipo escolhido e pergunta-lhe logo a seguir pelo estado. O demo
// faz isto com um BulletText só, para não submeter nada entre as consultas e o
// texto; aqui é a mesma ordem (tirar tudo para variáveis e só depois escrever).
void sectionWidgetsQueryingStatuses(ig::Context &ui, gallery::State &s)
{
    static const ig::StringView kItemNames[] = {
        ig::StringView("Text"), ig::StringView("Button"), ig::StringView("Checkbox"),
        ig::StringView("SliderFloat"), ig::StringView("InputText"),
        ig::StringView("InputTextMultiline"), ig::StringView("Selectable"),
        ig::StringView("TreeNode"), ig::StringView("Combo"), ig::StringView("ListBox")
    };
    static const ig::StringView kComboItems[] = {
        ig::StringView("Apple"), ig::StringView("Banana"), ig::StringView("Cherry"),
        ig::StringView("Kiwi")
    };
    ui.comboBox("Item Type", s.queryType, ig::Span<const ig::StringView>(kItemNames, 10), 240.0f);
    item();
    ui.sameLine();
    ui.checkbox("Item Disabled", s.queryDisabled);
    item();

    // Submete o item escolhido.
    bool ret = false;
    if (s.queryDisabled)
        ui.beginDisabled();
    switch (s.queryType)
    {
    case 0: ui.label("ITEM: Text"); break;
    case 1: ret = ui.button("ITEM: Button"); break;
    case 2: ret = ui.checkbox("ITEM: Checkbox", s.queryBool); break;
    case 3: ret = ui.sliderFloat("ITEM: SliderFloat", s.queryFloat, 0.0f, 1.0f, 240.0f); break;
    case 4: ret = ui.inputText("ITEM: InputText", g_panel.queryText); break;
    case 5:
        ret = ui.inputTextMultiline("ITEM: InputTextMultiline", g_panel.queryText, 0.0f, 60.0f);
        break;
    case 6: ret = ui.selectable("ITEM: Selectable", s.queryBool, 240.0f); break;
    case 7:
        ret = ui.treeNode("ITEM: TreeNode", s.queryTreeOpen, 240.0f);
        if (ret)
            ui.label("child");
        break;
    case 8:
        ret = ui.comboBox("ITEM: Combo", s.queryCombo,
                          ig::Span<const ig::StringView>(kComboItems, 4), 240.0f);
        break;
    default:
        ret = ui.listBox("ITEM: ListBox", s.queryCombo,
                         ig::Span<const ig::StringView>(kComboItems, 4), 240.0f, 4);
        break;
    }
    if (s.queryDisabled)
        ui.endDisabled();
    item();

    const bool itemHovered = ui.isItemHovered();
    const bool itemActive = ui.isItemActive();
    const bool itemClicked = ui.isItemClicked();
    const bool itemEdited = ui.isItemEdited();
    const bool itemFocused = ui.isItemFocused();
    const bool itemDisabledNow = ui.isItemDisabled();
    const ig::Rect itemRect = ui.itemRect();
    char report[512];
    snprintf(report, sizeof(report),
             "Return value = %d\nIsItemHovered() = %d\nIsItemActive() = %d\n"
             "IsItemEdited() = %d\nIsItemClicked() = %d\nIsItemFocused() = %d\n"
             "IsItemDisabled() = %d\nGetItemRectMin() = (%.1f, %.1f)\n"
             "GetItemRectSize() = (%.1f, %.1f)",
             ret ? 1 : 0, itemHovered ? 1 : 0, itemActive ? 1 : 0, itemEdited ? 1 : 0,
             itemClicked ? 1 : 0, itemFocused ? 1 : 0, itemDisabledNow ? 1 : 0,
             static_cast<double>(itemRect.x), static_cast<double>(itemRect.y),
             static_cast<double>(itemRect.width), static_cast<double>(itemRect.height));
    ui.bulletText(ig::StringView(report));
    item();

    gap(ui, gallery::Section::WidgetsQueryingStatuses,
        "IsItemActivated/Deactivated/DeactivatedAfterEdit/Visible/ToggledOpen, "
        "IsItemHovered com flags (Delay/Stationary/AllowWhenBlocked...) e "
        "SetItemAllowOverlap/ItemHoverable");
    // O estado da janela (o demo mostra-os na linha de baixo): as consultas são as
    // do iGUI, e respondem pela janela que está a ser submetida - esta.
    snprintf(report, sizeof(report),
             "Window focused = %d\nWindow hovered = %d\nWindow appearing = %d",
             ui.isWindowFocused() ? 1 : 0, ui.isWindowHovered() ? 1 : 0,
             ui.isWindowAppearing() ? 1 : 0);
    ui.bulletText(ig::StringView(report));
    item();
    gap(ui, gallery::Section::WidgetsQueryingStatuses,
        "o estado de janela 'collapsed' (o iGUI nao tem janelas recolhidas - tem "
        "minimizadas, que sao outra coisa) e o resto das consultas de item");
}

// ── Widgets: Selectables ─────────────────────────────────────────────────────
void sectionWidgetsSelectables(ig::Context &ui, gallery::State &s)
{
    for (int i = 0; i < 3; ++i)
    {
        if (ui.selectable(kItems5[i], s.combo == i, 180.0f))
            s.combo = i;
        item();
    }
    // O duplo clique num item (é o AllowDoubleClick do demo): a linha cai num
    // editor de texto, que é o que um duplo clique numa lista faz.
    ui.separatorText("Duplo clique para mudar o nome");
    static ig::String renameText("item");
    static bool renaming = false;
    if (renaming)
    {
        if (ui.inputText("##rename", renameText))
            item();
        if (ui.isItemDeactivatedAfterEdit() || ui.isKeyPressed(ig::KeyCode::Enter))
            renaming = false;
        item();
    }
    else
    {
        if (ui.selectable(renameText, false, 180.0f))
            item();
        if (ui.isItemDoubleClicked())
            renaming = true;
    }
    gap(ui, gallery::Section::WidgetsSelectables,
        "Selectable com os flags que faltam (SpanAllColumns, Disabled, AllowOverlap) e o "
        "alinhamento a direita - o AllowDoubleClick e o isItemDoubleClicked existem");
    ui.label("Selection state:");
    ui.sameLine();
    if (s.combo == 0)
        ui.label("AAAA");
    else if (s.combo == 1)
        ui.label("BBBB");
    else
        ui.label("CCCC");
    item();
}

// ── Widgets: Selection & Multi-Select ────────────────────────────────────────
void sectionWidgetsSelectionAndMultiSelect(ig::Context &ui, gallery::State &s)
{
    gap(ui, gallery::Section::WidgetsSelectionAndMultiSelect,
        "os ~12 exemplos do demo (clipper, delecao, dual list box, tabela, "
        "checkboxes, multiplos escopos, tiled assets browser, arvores, avancado) e "
        "os flags do BeginMultiSelect (box select, NoAutoSelect, ...)");
    ui.label("Uma lista com escolha multipla (Ctrl liga/desliga, Shift alcance):");
    item();
    // A seleção é do espelho (um bool por item) e o bloco do iGUI só lhe aplica o
    // que o rato e as teclas pediram, como o BeginMultiSelect do demo.
    const unsigned multiFlags = ig::Context::MultiSelectFlags::ClearOnEscape;
    if (ui.beginMultiSelect(ig::Span<bool>(s.multiSelected, 5), multiFlags))
    {
        for (int i = 0; i < 5; ++i)
        {
            ui.selectable(kItems5[i], s.multiSelected[i], 200.0f);
            item();
        }
        ui.endMultiSelect();
    }
    ui.label("Uma lista de um item de cada vez:");
    item();
    if (ui.beginMultiSelect(ig::Span<bool>(s.multiSelected, 5),
                            ig::Context::MultiSelectFlags::SingleSelect))
    {
        for (int i = 0; i < 5; ++i)
        {
            ui.selectable(kItems5[i], s.multiSelected[i], 200.0f);
            item();
        }
        ui.endMultiSelect();
    }
    (void)s;
}

// ── Widgets: Tabs ────────────────────────────────────────────────────────────
void sectionWidgetsTabs(ig::Context &ui, gallery::State &s)
{
    ui.tabBar("Tabs", s.tab, ig::Span<const ig::StringView>(kItems5, 3));
    item();
    if (s.tab == 0)
        ui.label("tab A: conteudo");
    else if (s.tab == 1)
        ui.label("tab B: conteudo");
    else
        ui.label("tab C: conteudo");
    item();
    // As tabs a sério do demo: a barra é um contentor e cada tab abre o conteúdo
    // dela (só a escolhida). A barra diz o que está escolhido por ela (o id), o x
    // fecha a tab (o p_open do chamador) e o ponto assinala o documento por gravar.
    static bool documentOpen = true;
    if (ui.beginTabBar("##Tabs", ig::Context::TabBarFlags::Reorderable |
                                  ig::Context::TabBarFlags::AutoSelectNewTabs))
    {
        if (ui.beginTabItem("Skeleton"))
        {
            ui.label("This is the Skeleton tab.");
            item();
            ui.endTabItem();
        }
        if (ui.beginTabItem("Appearance"))
        {
            ui.label("This is the Appearance tab.");
            item();
            ui.endTabItem();
        }
        if (ui.beginTabItem("Tab com um x", &documentOpen))
        {
            ui.label("Esta tab fecha-se pelo x (abre outra vez com o botao abaixo).");
            item();
            ui.endTabItem();
        }
        if (ui.beginTabItem("Unsaved", nullptr, ig::Context::TabItemFlags::UnsavedDocument))
        {
            ui.label("O ponto diz que o documento nao esta gravado.");
            item();
            ui.endTabItem();
        }
        ui.endTabBar();
    }
    item();
    if (!documentOpen)
    {
        if (ui.button("Abrir outra vez a tab fechada"))
            documentOpen = true;
        item();
    }
    // O segundo nó do "Tabs" do demo ("Advanced & Close Button"): os flags da barra
    // em checkboxes - é a barra que os leva, por isso mudam logo -, as quatro tabs do
    // "Opened:" e a barra delas, em que o x de uma tab a fecha pelo p_open.
    {
        bool reorderable = g_panel.tabFlagReorderable;
        bool autoSelect = g_panel.tabFlagAutoSelect;
        bool listPopup = g_panel.tabFlagListPopup;
        bool noCloseMiddle = g_panel.tabFlagNoCloseMiddle;
        bool overline = g_panel.tabFlagOverline;
        bool fitting = g_panel.tabFlagFitting;
        ui.checkbox("TabBarFlags_Reorderable", reorderable);
        item();
        ui.checkbox("TabBarFlags_AutoSelectNewTabs", autoSelect);
        item();
        ui.checkbox("TabBarFlags_TabListPopupButton", listPopup);
        item();
        ui.checkbox("TabBarFlags_NoCloseWithMiddleMouseButton", noCloseMiddle);
        item();
        ui.checkbox("TabBarFlags_DrawSelectedOverline", overline);
        item();
        ui.checkbox("TabBarFlags_FittingPolicyResizeDown (o Fitting)", fitting);
        item();
        g_panel.tabFlagReorderable = reorderable;
        g_panel.tabFlagAutoSelect = autoSelect;
        g_panel.tabFlagListPopup = listPopup;
        g_panel.tabFlagNoCloseMiddle = noCloseMiddle;
        g_panel.tabFlagOverline = overline;
        g_panel.tabFlagFitting = fitting;

        unsigned barFlags = ig::Context::TabBarFlags::None;
        if (reorderable)
            barFlags |= ig::Context::TabBarFlags::Reorderable;
        if (autoSelect)
            barFlags |= ig::Context::TabBarFlags::AutoSelectNewTabs;
        if (listPopup)
            barFlags |= ig::Context::TabBarFlags::TabListPopupButton;
        if (noCloseMiddle)
            barFlags |= ig::Context::TabBarFlags::NoCloseWithMiddleMouseButton;
        if (overline)
            barFlags |= ig::Context::TabBarFlags::DrawSelectedOverline;
        if (fitting)
            barFlags |= ig::Context::TabBarFlags::Fitting;

        static const char *const kOpenedNames[4] = {"Artichoke", "Beetroot", "Celery", "Daikon"};
        ui.label("Opened:");
        for (int i = 0; i < 4; ++i)
        {
            ui.sameLine();
            ui.checkbox(kOpenedNames[i], g_panel.openedTabs[i]);
        }
        item();
        if (ui.beginTabBar("MyTabBar", barFlags))
        {
            for (int i = 0; i < 4; ++i)
            {
                if (!g_panel.openedTabs[i])
                    continue;
                bool open = true;
                if (ui.beginTabItem(kOpenedNames[i], &open))
                {
                    char line[64];
                    snprintf(line, sizeof(line), "This is the %s tab!", kOpenedNames[i]);
                    ui.label(line);
                    item();
                    if ((i & 1) != 0)
                    {
                        ui.label("I am an odd tab.");
                        item();
                    }
                    ui.endTabItem();
                }
                if (!open)
                    g_panel.openedTabs[i] = false;
            }
            ui.endTabBar();
        }
        item();
    }

    // O terceiro nó do demo ("TabItemButton & Leading/Trailing flags"): o "?" é um
    // botão Leading que abre um menu, o "+" um botão Trailing que cria uma tab, e as
    // tabs numeradas fecham-se pelo x (é a lista que o chamador tem de manter).
    {
        ui.checkbox("Show Leading TabItemButton()", g_panel.showLeadingTabButton);
        item();
        ui.checkbox("Show Trailing TabItemButton()", g_panel.showTrailingTabButton);
        item();
        if (ui.beginTabBar("MyTabBar2", ig::Context::TabBarFlags::AutoSelectNewTabs |
                                           ig::Context::TabBarFlags::Reorderable |
                                           ig::Context::TabBarFlags::Fitting))
        {
            if (g_panel.showLeadingTabButton &&
                ui.tabItemButton("?", ig::Context::TabItemFlags::Leading))
                ui.openPopup("MyHelpMenu");
            item();
            if (ui.beginPopup("MyHelpMenu"))
            {
                ui.selectable("Hello!", false);
                item();
                ui.endPopup();
            }
            // O "+" acrescenta uma tab: o estado é do chamador, como no demo.
            if (g_panel.showTrailingTabButton &&
                ui.tabItemButton("+", ig::Context::TabItemFlags::Trailing) &&
                g_panel.activeTabCount < 8)
                g_panel.activeTabs[g_panel.activeTabCount++] = g_panel.nextTabId++;
            item();
            for (int i = 0; i < g_panel.activeTabCount;)
            {
                bool open = true;
                char name[16];
                snprintf(name, sizeof(name), "%04d", g_panel.activeTabs[i]);
                if (ui.beginTabItem(name, &open))
                {
                    char line[64];
                    snprintf(line, sizeof(line), "This is the %s tab!", name);
                    ui.label(line);
                    item();
                    ui.endTabItem();
                }
                if (!open)
                {
                    for (int move = i; move + 1 < g_panel.activeTabCount; ++move)
                        g_panel.activeTabs[move] = g_panel.activeTabs[move + 1];
                    --g_panel.activeTabCount;
                }
                else
                    ++i;
            }
            ui.endTabBar();
        }
        item();
    }

    // O "SetSelected" do demo: um botao que escolhe uma tab da barra.
    if (ui.button("Escolher a tab 'Appearance'"))
        g_panel.selectAppearance = true;
    item();
    if (ui.beginTabBar("##tabs_forced", ig::Context::TabBarFlags::Fitting))
    {
        const unsigned forced = g_panel.selectAppearance ? ig::Context::TabItemFlags::SetSelected
                                                         : ig::Context::TabItemFlags::None;
        if (ui.beginTabItem("Skeleton", nullptr, forced))
        {
            ui.label("escolhida pelo botao, nao pelo clique");
            item();
            ui.endTabItem();
        }
        if (ui.beginTabItem("Appearance", nullptr, forced))
        {
            ui.label("tab da aparencia");
            item();
            ui.endTabItem();
        }
        ui.endTabBar();
        g_panel.selectAppearance = false;
    }
    item();
    gap(ui, gallery::Section::WidgetsTabs,
        "os CheckboxFlags do demo (os flags da barra são um checkbox cada), o "
        "TabItemFlags_NoTooltip e as tabs de um dock space");
}

// ── Widgets: Text Input ──────────────────────────────────────────────────────
void sectionWidgetsTextInput(ig::Context &ui, gallery::State &s)
{
    ui.inputText("InputText", g_panel.name);
    item();
    ui.inputTextWithHint("InputTextWithHint", ig::StringView("type here to search"), g_panel.hint);
    item();
    ui.inputTextMultiline("InputTextMultiline", g_panel.multiline, 0.0f, 60.0f);
    item();
    gap(ui, gallery::Section::WidgetsTextInput,
        "InputText com CallbackCharFilter / CallbackCompletion e o exemplo de historico");
    g_panel.shared.i1 = g_panel.shared.i1;
    (void)s;
}

// ── Widgets: Trees ───────────────────────────────────────────────────────────
void sectionWidgetsTrees(ig::Context &ui, gallery::State &s)
{
    if (ui.treeNode("Basic tree node", s.treeOpen))
    {
        ui.label("child um");
        ui.label("child dois");
    }
    item();
    {
        ig::TreeItemStyle style;
        style.leaf = true;
        bool expanded = false;
        ui.treeItem("Tree leaf", expanded, style);
    }
    item();
    {
        ig::TreeItemStyle style;
        style.selected = true;
        bool expanded = true;
        ui.treeItem("Tree with selection", expanded, style);
    }
    item();
    gap(ui, gallery::Section::WidgetsTrees,
        "TreeNodeEx com flags (Selected, DefaultOpen, OpenOnArrow, SpanAvailWidth, "
        "SpanFullWidth, Framed, Leaf, Bullet, NavLeftJumpsToParent)");
}

// ── Widgets: Tooltips ────────────────────────────────────────────────────────
void sectionWidgetsTooltips(ig::Context &ui, gallery::State &s)
{
    (void)s;
    if (ui.button("Hover me"))
    {
    }
    ui.tooltip("Tooltip simples");
    item();
    if (ui.button("Hover me (com widgets dentro)"))
    {
    }
    // O tooltip rico do demo: uma caixa com widgets dentro, medida pelo conteudo.
    if (ui.beginTooltip())
    {
        ui.label("Um tooltip com widgets dentro:");
        ui.separator();
        ui.label("um rotulo, um separador");
        ui.sliderFloat("valor", s.f1, 0.0f, 1.0f);
        ui.endTooltip();
    }
    item();
    if (ui.button("Hover me (dois)"))
    {
    }
    ui.tooltip("Tooltip so com texto");
    item();
    gap(ui, gallery::Section::WidgetsTooltips,
        "tooltips com atraso configuravel por item e o exemplo de tooltip em item desativado");
}

// ── Layout & Scrolling ───────────────────────────────────────────────────────
void sectionLayoutAndScrolling(ig::Context &ui, gallery::State &s)
{
    ui.label("SameLine:");
    ui.sameLine();
    ui.label("a");
    ui.sameLine();
    ui.label("b");
    ui.sameLine();
    ui.label("c");
    item();
    gap(ui, gallery::Section::LayoutAndScrolling,
        "BeginChild com ImGuiChildFlags (Borders, ResizeX/Y, AutoResize) e as variacoes de "
        "tamanho; o iGUI tem beginChild(id, altura, border, largura)");
    if (ui.beginChild("child region", 80.0f))
    {
        ui.label("Child region");
        ui.indent();
        ui.label("com indent");
        ui.unindent();
        ui.endChild();
    }
    item();
    gap(ui, gallery::Section::LayoutAndScrolling,
        "BeginGroup/EndGroup (mais perto: beginPropertyRow) e o exemplo de mesmo tamanho");
    if (ui.beginPropertyRow("Group"))
    {
        ui.sliderFloat("valor", s.f1, 0.0f, 1.0f);
        ui.endPropertyRow();
    }
    item();
    gap(ui, gallery::Section::LayoutAndScrolling,
        "as colunas antigas (Columns/BeginColumns)");
    // "Widgets Width" do demo: a largura fixa por item. Os rótulos vão
    // escondidos com "##" como lá, senão o texto muda a largura que se mede.
    ui.separatorText("largura do item");
    ui.setNextItemWidth(100.0f);
    ui.dragFloat("##width1", s.f1, 0.0f, 1.0f, 0.01f);
    item();
    ui.pushItemWidth(100.0f);
    ui.dragFloat("##width2", s.f1, 0.0f, 1.0f, 0.01f);
    item();
    ui.indent();
    ui.dragFloat("##width3", s.f1, 0.0f, 1.0f, 0.01f);
    item();
    ui.unindent();
    ui.popItemWidth();
    ui.newLine();
    ui.label("largura que sobra:");
    ui.sameLine();
    ui.dragFloat("##width4", s.f2, 0.0f, 1.0f, 0.01f);
    item();
    if (ui.beginChild("scroll region", 120.0f))
    {
        char label[32];
        for (int i = 0; i < 40; ++i)
        {
            snprintf(label, sizeof(label), "linha %d", i);
            ui.label(label);
        }
        ui.endChild();
    }
    item();
    gap(ui, gallery::Section::LayoutAndScrolling,
        "ImGuiListClipper (virtualizacao de linhas) - o iGUI tem beginVirtualList");
    {
        int firstVisible = 0;
        int lastVisible = 0;
        if (ui.beginVirtualList("clipper demo", 1000, 20.0f, 120.0f, firstVisible, lastVisible))
        {
            char label[32];
            for (int i = firstVisible; i < lastVisible; ++i)
            {
                const ig::Rect bounds = ui.virtualListItemRect(i);
                snprintf(label, sizeof(label), "item %d", i);
                ui.label(label, ig::Vec2(bounds.x, bounds.y));
            }
            ui.endVirtualList();
        }
    }
    item();
    gap(ui, gallery::Section::LayoutAndScrolling,
        "os exemplos de scroll horizontal, scrollbar customizada e clipping com DrawList");
}

// ── Popups & Modal Windows ───────────────────────────────────────────────────
void sectionPopupsAndModalWindows(ig::Context &ui, gallery::State &s)
{
    (void)s;
    if (ui.button("Open popup"))
        g_panel.dialogOpen = true;
    item();
    if (g_panel.dialogOpen)
    {
        if (ui.beginDialog("Popup do iGUI", g_panel.dialogOpen, ig::Vec2(240.0f, 110.0f)))
        {
            ui.label("conteudo do popup");
            if (ui.button("Close"))
                g_panel.dialogOpen = false;
            ui.endDialog();
        }
    }
    // O modal do demo, agora com o beginPopupModal: título próprio, centrado, por
    // cima de tudo e com um X (o p_open) e o Escape a fecharem-no - o clique fora
    // não fecha, que é o que o distingue de um popup. A messageBox continua a ser a
    // caixa de OK/Cancelar.
    if (ui.button("Open modal"))
    {
        g_panel.modalOpen = true;
        // O id do popup é o do título com que o beginPopupModal o submete (é a
        // mesma regra do ImGui).
        ui.openPopup("Modal Window");
    }
    item();
    ui.sameLine();
    // A caixa de OK/Cancelar do iGUI (o demo do ImGui já não tem uma): um clique e
    // ela aparece enquanto o open for verdadeiro.
    if (ui.button("Open message box"))
        g_panel.messageBoxOpen = true;
    item();
    if (g_panel.modalOpen && ui.beginPopupModal("Modal Window", &g_panel.modalOpen))
    {
        ui.label("Show a modal window with its own title bar, and the ability to close it");
        item();
        ui.label("with the X and by pressing Esc.");
        item();
        ui.separator();
        if (ui.button("Button"))
            ui.closeCurrentPopup();
        if (!g_panel.modalOpen)
            ui.closeCurrentPopup();
        ui.endPopup();
    }
    if (g_panel.messageBoxOpen)
    {
        if (ui.messageBox("Modal", "Continuar?", g_panel.messageBoxOpen) !=
            ig::MessageBoxResult::None)
            g_panel.messageBoxOpen = false;
    }
    // O "Select.." do demo, com o popup de escolha dele: abre no ponteiro, o corpo é
    // conteúdo livre (um separador e os itens) e escolher fecha-o.
    if (ui.button("Select.."))
        ui.openPopup("my_select_popup");
    item();
    ui.sameLine();
    {
        static const ig::StringView kFish[] = {
            ig::StringView("Bream"), ig::StringView("Haddock"), ig::StringView("Mackerel"),
            ig::StringView("Pollock"), ig::StringView("Tilefish")
        };
        ui.label(s.comboItem < 0 ? ig::StringView("<None>") : kFish[s.comboItem]);
    }
    item();
    if (ui.beginPopup("my_select_popup"))
    {
        ui.separatorText("Aquarium");
        for (int i = 0; i < 5; ++i)
        {
            static const ig::StringView kFish[] = {
                ig::StringView("Bream"), ig::StringView("Haddock"), ig::StringView("Mackerel"),
                ig::StringView("Pollock"), ig::StringView("Tilefish")
            };
            if (ui.selectable(kFish[i], s.comboItem == i))
            {
                s.comboItem = i;
                ui.closeCurrentPopup();
            }
            item();
        }
        ui.endPopup();
    }
    // O "Popup Stack" do demo: cada botão abre o seu popup e o popup aberto sabe
    // quem é (o isPopupOpen com o AnyPopupId é o que uma aplicação usa para saber
    // que há um popup pendente).
    if (ui.button("Open Popup 1"))
        ui.openPopup("popup 1", ig::Context::PopupFlags::NoOpenOverExistingPopup);
    item();
    ui.sameLine();
    if (ui.button("Open Popup 2"))
        ui.openPopup("popup 2", ig::Context::PopupFlags::NoOpenOverExistingPopup);
    item();
    if (ui.beginPopup("popup 1"))
    {
        ui.label("Current stack: 'Window/1'");
        item();
        if (ui.button("Close"))
            ui.closeCurrentPopup();
        ui.endPopup();
    }
    if (ui.beginPopup("popup 2"))
    {
        ui.label("Current stack: 'Window/2'");
        item();
        if (ui.button("Close"))
            ui.closeCurrentPopup();
        ui.endPopup();
    }
    // Um popup por documento (o ShowExampleAppDocuments) faz-se com um id por
    // documento - o "##" é o que deixa dois popups partilharem o rótulo. O que falta
    // mesmo é a pilha: um popup aberto de dentro de outro.
    gap(ui, gallery::Section::PopupsAndModalWindows,
        "a pilha de popups (um popup aberto de dentro de outro, como o 'Popup "
        "Stack' do demo): o iGUI tem um popup de cada vez, e o "
        "NoOpenOverExistingPopup e o que evita abrir por cima do que la esta");
    {
        const ig::Vec2 cursor = ui.cursor();
        ui.label("Right-click this line for a context menu");
        const ig::Rect bounds(cursor.x, cursor.y, 260.0f, 20.0f);
        if (ui.beginContextMenu("demo ctx", bounds))
        {
            if (ui.menuItem("Copy"))
                gallery::readout();
            ui.menuSeparator();
            ui.menuItem("Paste", false);
            ui.endContextMenu();
        }
        ui.dummy(260.0f, 20.0f);
    }
    item();
}

// ── Tables & Columns ─────────────────────────────────────────────────────────
void sectionTablesAndColumns(ig::Context &ui, gallery::State &s)
{
    gap(ui, gallery::Section::TablesAndColumns,
        "BeginTable com TableSetupColumn/TableHeadersRow/SortableFlags/Borders/RowBg/"
        "SizingFixedFit e as tabelas aninhadas - o iGUI tem beginTable/tableHeader/"
        "tableNextColumn com pesos mutaveis (redimensionamento pelo grip do cabecalho), "
        "ordenacao pelo cabecalho, colunas escondidas, colunas reordenaveis, rolagem "
        "propria nos dois eixos (ScrollX/ScrollY) com linhas e colunas presas");
    // A tabela do demo ("Hideable"): o cabeçalho tem um menu de contexto com um
    // checkbox por coluna, e a coluna escondida não ocupa largura (as seguintes
    // fecham o espaço dela). O menu é do chamador, com o openPopup.
    static const ig::StringView kColumnNames[] = {
        ig::StringView("Name"), ig::StringView("Value"), ig::StringView("Notes")
    };
    const float weights[3] = {2.0f, 1.0f, 2.0f};
    const ig::Rect tableBounds(0.0f, 0.0f, ui.availableWidth(), 0.0f);
    ig::TableStyle style;
    style.alternateRows = true;
    style.borders = true;
    // Reorderable: arrastar um cabeçalho muda a coluna do modelo de sítio (o
    // tableColumnIndex diz qual desenhar em cada posição, e é com ele que as
    // células têm de escolher o dado).
    style.reorderable = true;
    if (ui.beginTable("table", ig::Span<const float>(weights, 3),
                      ig::Span<const bool>(g_panel.tableColumns, 3), style, tableBounds.width))
    {
        item();
        for (int column = 0; column < 3; ++column)
        {
            // A coluna escondida devolve false: a célula dela não é submetida.
            if (!ui.tableNextColumn())
                continue;
            const int model = ui.tableColumnIndex();
            ui.tableHeader(kColumnNames[model], g_panel.sortColumn, g_panel.sortAscending);
        }
        // Um clique com o botão direito num cabeçalho abre o menu das colunas (é o
        // que o tableHoveredColumn diz: qual delas está debaixo do ponteiro).
        // Um clique com o botão direito no cabeçalho abre o menu: o iGUI não tem um
        // "foi este botão que desceu?", mas o isPointerButtonDown diz que está
        // premido e o isKeyPressed-like test do press vem do próprio menu - por isso
        // abre com o botão direito premido sobre o cabeçalho.
        if (ui.tableHoveredColumn() >= 0 && ui.isPointerButtonDown(ig::PointerButton::Right))
            ui.openPopup("table columns");
        item();
        for (int row = 0; row < 4; ++row)
        {
            char label[24];
            for (int column = 0; column < 3; ++column)
            {
                if (!ui.tableNextColumn())
                    continue;
                // O dado segue a coluna do modelo, não a posição no écra.
                const int model = ui.tableColumnIndex();
                if (model == 1)
                    snprintf(label, sizeof(label), "%d", row * 7);
                else if (model == 0)
                    snprintf(label, sizeof(label), "row %d", row);
                else
                    snprintf(label, sizeof(label), "nota");
                ui.label(ig::StringView(label, strlen(label)));
            }
            item();
        }
        ui.endTable();
    }
    if (ui.beginPopup("table columns"))
    {
        ui.separatorText("Colunas");
        for (int column = 0; column < 3; ++column)
        {
            bool shown = g_panel.tableColumns[column];
            if (ui.menuCheckbox(kColumnNames[column], shown))
                g_panel.tableColumns[column] = shown;
            item();
        }
        ui.endPopup();
    }
    // A tabela de muitas colunas do demo, com as duas primeiras presas (é o
    // TableSetupScrollFreeze(2, 1) dele): as colunas têm a largura que o chamador
    // pede (innerWidth) e o que não couber rola dentro da tabela.
    ui.label("Tabela larga com as duas primeiras colunas presas (rola com a roda horizontal):");
    item();
    const float wideWeights[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    ig::TableStyle wideStyle;
    wideStyle.alternateRows = true;
    wideStyle.borders = true;
    wideStyle.scrollX = true;
    wideStyle.innerWidth = 920.0f;   // 8 colunas de 115 px numa tabela de ~530
    wideStyle.freezeCols = 2;
    if (ui.beginTable("wide table", ig::Span<const float>(wideWeights, 8), wideStyle,
                      ui.availableWidth()))
    {
        item();
        for (int column = 0; column < 8; ++column)
        {
            if (!ui.tableNextColumn())
                continue;
            char label[16];
            snprintf(label, sizeof(label), "Col %d", ui.tableColumnIndex());
            ui.tableHeader(ig::StringView(label, strlen(label)));
        }
        item();
        for (int row = 0; row < 3; ++row)
        {
            char label[16];
            for (int column = 0; column < 8; ++column)
            {
                if (!ui.tableNextColumn())
                    continue;
                snprintf(label, sizeof(label), "%d,%d", row, ui.tableColumnIndex());
                ui.label(ig::StringView(label, strlen(label)));
            }
            item();
        }
        ui.endTable();
    }
    // A tabela que rola sozinha do demo ("Scrolling" com TableSetupScrollFreeze):
    // altura fixa, as linhas rolam dentro dela (roda e barra) e a linha presa fica
    // no topo. Sem scrollY o que rola é a janela, e o cabeçalho vai com ela.
    ui.label("Tabela com rolagem propria (3 linhas presas):");
    item();
    const float scrollWeights[2] = {1.0f, 1.0f};
    ig::TableStyle scrollStyle;
    scrollStyle.alternateRows = true;
    scrollStyle.borders = true;
    scrollStyle.scrollY = true;
    scrollStyle.height = 150.0f;
    scrollStyle.freezeRows = 4;   // a linha 0 é o cabecalho, 1 a 3 ficam presas
    if (ui.beginTable("scroll table", ig::Span<const float>(scrollWeights, 2), scrollStyle,
                      ui.availableWidth()))
    {
        item();
        for (int column = 0; column < 2; ++column)
        {
            if (!ui.tableNextColumn())
                continue;
            ui.tableHeader(column == 0 ? "Nome" : "Valor");
        }
        item();
        for (int row = 0; row < 12; ++row)
        {
            char label[32];
            for (int column = 0; column < 2; ++column)
            {
                if (!ui.tableNextColumn())
                    continue;
                if (column == 0)
                    snprintf(label, sizeof(label), "linha %d", row);
                else
                    snprintf(label, sizeof(label), "%d", row * 3);
                ui.label(ig::StringView(label, strlen(label)));
            }
            item();
        }
        ui.endTable();
    }
    (void)s;
}

// ── Inputs, Navigation & Focus ───────────────────────────────────────────────
void sectionInputsNavigationAndFocus(ig::Context &ui, gallery::State &s)
{
    (void)s;
    gap(ui, gallery::Section::InputsNavigationAndFocus,
        "a navegacao por teclado/gamepad (NavEnable*), o foco programatico, IsKeyPressed/"
        "IsKeyDown/GetKeyPressedAmount, os atalhos por scancode e a captura de teclado/rato - "
        "o iGUI tem isKeyPressed/shortcut/isPointerButtonDown e a captura por janela");
    ui.label("Aqui o demo mostra as teclas e o rato em tempo real:");
    item();
    ui.label(ui.isKeyPressed(ig::KeyCode::Enter) ? "Enter: premido" : "Enter: (carrega nele)");
    item();
    gap(ui, gallery::Section::InputsNavigationAndFocus, "InputText com navegacao por teclado");
    ui.inputText("keyboard input", g_panel.name);
    item();
}

void section(ig::Context &ui, gallery::Section section, gallery::State &s)
{
    switch (section)
    {
    case gallery::Section::Help: sectionHelp(ui, s); break;
    case gallery::Section::Configuration: sectionConfiguration(ui, s); break;
    case gallery::Section::WindowOptions: sectionWindowOptions(ui, s); break;
    case gallery::Section::WidgetsBasic: sectionWidgetsBasic(ui, s); break;
    case gallery::Section::WidgetsBullets: sectionWidgetsBullets(ui, s); break;
    case gallery::Section::WidgetsCollapsingHeaders: sectionWidgetsCollapsingHeaders(ui, s); break;
    case gallery::Section::WidgetsColorAndPickers: sectionWidgetsColorAndPickers(ui, s); break;
    case gallery::Section::WidgetsCombo: sectionWidgetsCombo(ui, s); break;
    case gallery::Section::WidgetsDataTypes: sectionWidgetsDataTypes(ui, s); break;
    case gallery::Section::WidgetsDisableBlocks: sectionWidgetsDisableBlocks(ui, s); break;
    case gallery::Section::WidgetsDragAndDrop: sectionWidgetsDragAndDrop(ui, s); break;
    case gallery::Section::WidgetsDragAndSliderFlags: sectionWidgetsDragAndSliderFlags(ui, s); break;
    case gallery::Section::WidgetsFonts: sectionWidgetsFonts(ui, s); break;
    case gallery::Section::WidgetsImages: sectionWidgetsImages(ui, s); break;
    case gallery::Section::WidgetsListBoxes: sectionWidgetsListBoxes(ui, s); break;
    case gallery::Section::WidgetsMultiComponents: sectionWidgetsMultiComponents(ui, s); break;
    case gallery::Section::WidgetsPlotting: sectionWidgetsPlotting(ui, s); break;
    case gallery::Section::WidgetsProgressBars: sectionWidgetsProgressBars(ui, s); break;
    case gallery::Section::WidgetsQueryingStatuses: sectionWidgetsQueryingStatuses(ui, s); break;
    case gallery::Section::WidgetsSelectables: sectionWidgetsSelectables(ui, s); break;
    case gallery::Section::WidgetsSelectionAndMultiSelect:
        sectionWidgetsSelectionAndMultiSelect(ui, s);
        break;
    case gallery::Section::WidgetsTabs: sectionWidgetsTabs(ui, s); break;
    case gallery::Section::WidgetsTextInput: sectionWidgetsTextInput(ui, s); break;
    case gallery::Section::WidgetsTrees: sectionWidgetsTrees(ui, s); break;
    case gallery::Section::WidgetsTooltips: sectionWidgetsTooltips(ui, s); break;
    case gallery::Section::LayoutAndScrolling: sectionLayoutAndScrolling(ui, s); break;
    case gallery::Section::PopupsAndModalWindows: sectionPopupsAndModalWindows(ui, s); break;
    case gallery::Section::TablesAndColumns: sectionTablesAndColumns(ui, s); break;
    case gallery::Section::InputsNavigationAndFocus:
        sectionInputsNavigationAndFocus(ui, s);
        break;
    case gallery::Section::Count: break;
    }
}

} // namespace

DemoFont gDemoFonts[kMaxDemoFonts];
int gDemoFontCount = 0;
ig::FontId gFontDefault;

void reset()
{
    g_panel = Panel();
    gallery::clearGaps();
}

// A faixa de números, na sua própria janela sem barra de título e sem
// redimensionamento: é a mesma coisa que o lado do ImGui faz com uma janela
// NoTitleBar, e é o que deixa a janela do demo por baixo dela a começar à mesma
// altura nos dois painéis.
void readout(ig::Context &ui)
{
    ig::Context::WindowOptions options;
    options.titleBar = false;
    options.move = false;
    options.resize = false;
    options.saveSettings = false;
    options.fixedBounds = true;
    const ig::Vec2 display = ui.displaySize();
    if (!ui.beginWindow("##numeros", ig::Rect(0.0f, 0.0f, display.x, gallery::kReadoutHeight),
                        options))
        return;
    const gallery::Readout &info = gallery::readout();
    char line[192];
    snprintf(line, sizeof(line),
             "iGUI   submit %.2f ms   render %.2f ms   gpu %.2f ms   %d itens   %d vtx   "
             "%d draw calls   %d lacunas",
             static_cast<double>(info.submitMs), static_cast<double>(info.renderMs),
             static_cast<double>(info.gpuMs), info.items, info.vertices, info.drawCalls,
             info.gaps);
    ui.separatorText(line);
    const ig::Vec2 origin = ui.cursor();
    const float width = ui.availableWidth();
    const float height = 56.0f;
    const float maxMs = 4.0f;
    ui.drawRect(ig::Rect(origin.x, origin.y, width, height), ig::Color(60u, 64u, 72u));
    if (info.history && info.historyCount > 1)
    {
        const int count = info.historyCount > 200 ? 200 : info.historyCount;
        const float step = width / static_cast<float>(count - 1);
        ig::Vec2 previous(origin.x, origin.y + height);
        for (int i = 0; i < count; ++i)
        {
            float clamped = info.history[info.historyCount - count + i] / maxMs;
            if (clamped > 1.0f)
                clamped = 1.0f;
            if (clamped < 0.0f)
                clamped = 0.0f;
            const ig::Vec2 point(origin.x + step * static_cast<float>(i),
                                 origin.y + height - clamped * height);
            ui.drawLine(previous, point, ig::Color(120u, 200u, 255u));
            previous = point;
        }
    }
    ui.dummy(width, height + 6.0f);
    ui.endWindow();
}

void applyDemoStyle(ig::Context &ui)
{
    // O estilo por omissão do ImGui, campo a campo: fonte 13, item 19 (fonte +
    // FramePadding.y*2), espaçamento 4 (ItemSpacing.y), FramePadding.x 4, e as
    // alturas todas no mesmo valor, que é o que ele usa para botões, caixas,
    // campos e sliders. Os cantos ficam direitos como os dele, para as linhas
    // caírem no mesmo pixel nos dois painéis.
    ig::Theme theme = ui.theme();
    theme.fontSize = gallery::kFontSize;
    const float frameHeight = gallery::kFontSize + 3.0f * 2.0f;
    theme.widgetHeight = frameHeight;
    theme.buttonHeight = frameHeight;
    theme.inputHeight = frameHeight;
    theme.sliderHeight = frameHeight;
    theme.titleBarHeight = frameHeight;
    theme.menuBarHeight = frameHeight;
    theme.menuItemHeight = frameHeight;
    theme.itemSpacing = 4.0f;
    theme.padding = 4.0f;
    theme.textEditPadding = 3.0f;
    theme.lineSpacing = 0.0f;
    theme.borderRadius = 0.0f;
    theme.scrollbarWidth = 14.0f;
    theme.scrollbarMinThumb = 20.0f;
    // A cor dos headers das secções é a do ImGui (ImGuiCol_Header), que é o que
    // dá ao demo o aspeto às riscas azuis.
    theme.collapsibleHeaderBg = ig::Color(41u, 74u, 122u, 255u);
    // O texto do demo é branco puro (ImGuiCol_Text por omissão); o tema do iGUI
    // usa um cinzento claro.
    theme.textColor = ig::Color(255u, 255u, 255u, 255u);
    theme.labelText = theme.textColor;
    theme.menuItemPadX = 8.0f;
    theme.tooltipPadX = 8.0f;
    theme.tooltipPadY = 8.0f;
    ui.setTheme(theme);
}

// Só uma secção, no topo: é o que deixa pôr uma secção do demo lado a lado sem
// andar à roda do rato (ver o --only do runner).
static int g_onlySection = -1;

void openAllSections()
{
    for (int i = 0; i < static_cast<int>(gallery::Section::Count); ++i)
    {
        g_panel.topOpen[i] = true;
        g_panel.subOpen[i] = true;
    }
}

// Abre uma secção e só ela, pelo nome (o mesmo de gallery::sectionName): é o que
// deixa pôr lado a lado uma secção do demo de cada vez. Nas sub-secções de
// "Widgets" o header de cima segue a mesma entrada de topOpen (ver draw), por
// isso o "Widgets" abre com ela.
bool setOnlySection(const char *name)
{
    for (int i = 0; i < static_cast<int>(gallery::Section::Count); ++i)
    {
        const gallery::Section section = static_cast<gallery::Section>(i);
        if (strcmp(gallery::sectionName(section), name) != 0)
            continue;
        g_onlySection = i;
        g_panel.topOpen[i] = true;
        g_panel.subOpen[i] = true;
        return true;
    }
    return false;
}

namespace
{

// Lê um ficheiro inteiro para um buffer, ou devolve falso. Não abrir não é uma
// face em falta: quem chama segue sem ela.
bool readFile(const char *path, ct::Vector<uint8_t> &out)
{
    std::FILE *file = std::fopen(path, "rb");
    if (!file)
        return false;
    std::fseek(file, 0L, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0L, SEEK_SET);
    bool ok = size > 0;
    if (ok)
    {
        out.resize(static_cast<ct::Vector<uint8_t>::size_type>(size));
        ok = std::fread(out.data(), 1u, static_cast<size_t>(size), file) ==
             static_cast<size_t>(size);
    }
    std::fclose(file);
    return ok;
}

// O nome da face é o do ficheiro sem o caminho nem a extensão, mais o tamanho
// (".../misc/fonts/Cousine-Regular.ttf" a 13 -> "Cousine-Regular 13px"), que é
// como se lê o combo do demo.
void faceName(const char *file, float size, char *out, int outSize)
{
    const char *base = file;
    for (const char *p = file; *p; ++p)
        if (*p == '/' || *p == '\\')
            base = p + 1;
    int length = 0;
    while (base[length] && base[length] != '.' && length < outSize - 12)
        ++length;
    int written = 0;
    for (int i = 0; i < length; ++i)
        out[written++] = base[i];
    snprintf(out + written, static_cast<size_t>(outSize - written), " %.0fpx",
             static_cast<double>(size));
}

} // namespace

void loadDemoFonts(ig::FontAtlas &atlas, const char *directory)
{
    g_atlasWidth = atlas.width();
    g_atlasHeight = atlas.height();
    gDemoFontCount = 0;
    // A face 0 é a do painel: a que o atlas já tem, e a que o combo mostra como
    // "default".
    gFontDefault = atlas.defaultFont();
    DemoFont &base = gDemoFonts[gDemoFontCount++];
    base.id = atlas.defaultFont();
    base.size = gallery::kFontSize;
    snprintf(base.name, sizeof(base.name), "default %.0fpx",
             static_cast<double>(gallery::kFontSize));
    if (!directory || !*directory)
        return;

    int fileCount = 0;
    const gallery::DemoFontFile *files = gallery::demoFontFiles(fileCount);
    ct::Vector<uint8_t> bytes;
    char path[512];
    for (int i = 0; i < fileCount && gDemoFontCount < kMaxDemoFonts; ++i)
    {
        snprintf(path, sizeof(path), "%s/%s", directory, files[i].file);
        if (!readFile(path, bytes))
        {
            fprintf(stderr, "fonte do demo em falta: %s\n", path);
            continue;
        }
        const ig::FontId id = atlas.loadFont(
            ig::Span<const uint8_t>(bytes.data(), bytes.size()), files[i].size);
        if (id.value == 0u)
        {
            fprintf(stderr, "nao consegui cozer a fonte do demo: %s\n", path);
            continue;
        }
        DemoFont &face = gDemoFonts[gDemoFontCount++];
        face.id = id;
        face.size = files[i].size;
        faceName(files[i].file, files[i].size, face.name,
                 static_cast<int>(sizeof(face.name)));
    }
    // Cozer faces reconstrói o atlas inteiro, por isso o tamanho que a secção
    // "Fonts" mostra é o que ficou no fim.
    g_atlasWidth = atlas.width();
    g_atlasHeight = atlas.height();
}

bool openSection(const char *name)
{
    for (int i = 0; i < static_cast<int>(gallery::Section::Count); ++i)
    {
        const gallery::Section section = static_cast<gallery::Section>(i);
        if (strcmp(gallery::sectionName(section), name) != 0)
            continue;
        g_panel.topOpen[i] = true;
        g_panel.subOpen[i] = true;
        // O CollapsingHeader("Widgets") é o de WidgetsBasic que diz se está aberto
        // (ver draw), por isso uma sub-secção dele tem de o abrir também.
        if (gallery::insideWidgets(section))
        {
            const int widgets = static_cast<int>(gallery::Section::WidgetsBasic);
            g_panel.topOpen[widgets] = true;
        }
        return true;
    }
    return false;
}

gallery::FrameStats draw(ig::Context &ui, float deltaSeconds)
{
    gallery::FrameStats stats;
    g_panel.widgets = 0;
    g_panel.deltaSeconds = deltaSeconds;

    // Duas janelas, como no lado do ImGui: a faixa de números em cima, sem barra
    // de título, e a janela do demo por baixo dela. O Context não desenha nada
    // fora de uma janela, e assim os números saem no mesmo frame que se mede.
    readout(ui);

    const ig::Vec2 display = ui.displaySize();
    const ig::Rect demo(0.0f, gallery::kReadoutHeight, display.x,
                        display.y - gallery::kReadoutHeight);
    if (!ui.beginWindow("Dear iGUI Demo", demo))
        return stats;

    // A face escolhida no combo da secção "Fonts" (o io.FontDefault do ImGui) vale
    // para o painel todo. O combo escreve a escolha e este push lê-a no início do
    // frame com o valor que ela tinha quando o frame começou - como no ImGui, a
    // escolha entra no frame seguinte. O tamanho fica o do painel: a face muda, a
    // escala continua a ser a do tema (é o FontSizeBase que manda).
    const bool pushedFace = gFontDefault.value != 0u && gFontDefault != ui.theme().font;
    if (pushedFace)
    {
        ui.pushFont(gFontDefault);
        ui.pushStyleVar(&ig::Theme::fontSize, gallery::kFontSize);
    }

    // O menu do demo (File/Edit/View/Tools/Help: os menus sao "Menu", "Examples",
    // "Tools", "Help" e abrem apps de exemplo que o iGUI nao tem).
    const ig::Rect bar(0.0f, 0.0f, ui.availableWidth(), 26.0f);
    if (ui.beginMenuBar(bar))
    {
        if (ui.beginMenu("Menu"))
        {
            ui.menuItem("(demo)");
            ui.menuSeparator();
            ui.menuItem("New");
            // Os mesmos atalhos do demo, agora desenhados pelo menuItem.
            ui.menuItem("Open", ig::StringView("Ctrl+O"));
            // O "Open Recent" do demo, com os nomes dos ficheiros.
            if (ui.beginSubMenu("Open Recent"))
            {
                ui.menuItem("fish_hat.c");
                ui.menuItem("fish_hat.inl");
                ui.menuItem("fish_hat.h");
                ui.endSubMenu();
            }
            ui.menuItem("Save", ig::StringView("Ctrl+S"));
            ui.menuItem("Save As..");
            ui.menuSeparator();
            // O "Options" do demo: um menu com widgets lá dentro (um child a rolar,
            // um checkbox, um slider, um input, um combo). Antes isto não cabia no
            // iGUI - o popup de um menu não era um contentor de layout.
            if (ui.beginSubMenu("Options"))
            {
                gallery::State &shared = g_panel.shared;
                bool enabled = shared.flag[0];
                if (ui.menuCheckbox("Enabled", enabled))
                    shared.flag[0] = enabled;
                if (ui.beginChild("child", 60.0f))
                {
                    char scrolling[32];
                    for (int i = 0; i < 10; ++i)
                    {
                        snprintf(scrolling, sizeof(scrolling), "Scrolling Text %d", i);
                        ui.label(ig::StringView(scrolling, strlen(scrolling)));
                        item();
                    }
                    ui.endChild();
                }
                item();
                ui.sliderFloat("Value", shared.f1, 0.0f, 1.0f);
                item();
                ui.inputFloat("Input", shared.f1);
                item();
                ui.comboBox("Combo", shared.combo, ig::Span<const ig::StringView>(kItems5, 5));
                item();
                ui.endSubMenu();
            }
            ui.menuItem("Quit", ig::StringView("Alt+F4"));
            ui.endMenu();
        }
        if (ui.beginMenu("Examples"))
        {
            gallery::reportGap("Menu", "os ~20 apps de exemplo do demo (Main menu bar, "
                                       "Assets Browser, Console, Documents, Log, Property "
                                       "editor, Simple layout, Auto-resizing window, ...)");
            ui.menuItem("Main menu bar");
            ui.menuItem("Console");
            ui.menuItem("Documents");
            ui.menuItem("Property editor");
            ui.endMenu();
        }
        if (ui.beginMenu("Tools"))
        {
            ui.menuItem("Metrics/Debugger");
            ui.menuItem("Style Editor");
            ui.menuItem("About");
            gallery::reportGap("Menu", "Metrics/Debugger, Style Editor, About e os debug "
                                       "options do Tools");
            ui.endMenu();
        }
        ui.beginMenu("Help");
        ui.endMenu();
        ui.endMenuBar();
    }

    if (ui.beginChild("demo", ui.availableHeight(), false))
    {
        gallery::State &s = g_panel.shared;
        const gallery::Section *list = gallery::sections();
        // Com o --only desenha-se só a secção pedida, e sem o header dela: é uma
        // secção sozinha no topo, para comparar com o lado do ImGui.
        if (g_onlySection >= 0)
        {
            for (int i = 0; i < gallery::sectionCount(); ++i)
            {
                if (static_cast<int>(list[i]) == g_onlySection)
                {
                    section(ui, list[i], s);
                    break;
                }
            }
            ui.endChild();
            ui.endWindow();
            stats.items = g_panel.widgets;
            stats.gaps = gallery::gapCount();
            return stats;
        }
        // A primeira linha do ShowDemoWindow, fora das secções (é o Text com a
        // versão do ImGui que está a ser espelhada).
        ui.label(ig::StringView(gallery::gHelloLine));
        item();
        bool widgetsOpen = false;
        for (int i = 0; i < gallery::sectionCount(); ++i)
        {
            const gallery::Section current = list[i];
            const int index = static_cast<int>(current);
            if (gallery::insideWidgets(current))
            {
                if (current == gallery::Section::WidgetsBasic)
                {
                    // Uma vez por frame, como o CollapsingHeader("Widgets") do
                    // demo, que tambem e desenhado antes das suas sub-seccoes.
                    widgetsOpen = ui.collapsingHeader("Widgets", g_panel.topOpen[index]);
                    item();
                    if (widgetsOpen)
                        ui.indent();
                }
                if (!widgetsOpen)
                    continue;
                if (ui.treeNode(gallery::sectionName(current), g_panel.subOpen[index]))
                    section(ui, current, s);
                else
                    item();
                continue;
            }

            if (ui.collapsingHeader(gallery::sectionName(current), g_panel.topOpen[index]))
                section(ui, current, s);
            else
                item();
        }
        if (widgetsOpen)
            ui.unindent();
        ui.endChild();
    }
    if (pushedFace)
    {
        ui.popStyleVar();
        ui.popFont();
    }
    ui.endWindow();

    stats.items = g_panel.widgets;
    stats.gaps = gallery::gapCount();
    return stats;
}

} // namespace igui_gallery
