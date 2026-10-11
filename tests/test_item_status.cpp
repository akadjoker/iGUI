// Tests for the LastItemData-style queries (isItemHovered, isItemActive,
// isItemClicked, isItemEdited, isItemFocused, isItemDisabled, itemRect,
// itemId). They read state that the widget sets while it is submitted, so the
// only way to check them is to drive the context and read the answers in the
// same frame, right after the widget - which is what these tests do.

#include "InteractionHarness.hpp"

#include <assert.h>
#include <stdio.h>

namespace
{

struct Probe
{
    bool hovered;
    bool active;
    bool clicked;
    bool edited;
    bool focused;
    bool disabled;
    ig::Rect rect;
    ig::WidgetId id;

    Probe()
        : hovered(false), active(false), clicked(false), edited(false), focused(false),
          disabled(false), rect(), id(ig::InvalidWidgetId)
    {
    }

    void read(const ig::Context &context)
    {
        hovered = context.isItemHovered();
        active = context.isItemActive();
        clicked = context.isItemClicked();
        edited = context.isItemEdited();
        focused = context.isItemFocused();
        disabled = context.isItemDisabled();
        rect = context.itemRect();
        id = context.itemId();
    }
};

// The pointer does everything to a slider: hover, hold, release, edit. The
// first frame finds out where the widget actually landed (no hard-coded
// padding/title-bar arithmetic), and the following frames poke that point.
void test_slider_item_status()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float volume = 0.25f;
    Probe probe;
    Probe neighbour;
    const ig::StringView dummyLabel("FPS: 60");

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("status", ig::Rect(0.0f, 0.0f, 320.0f, 200.0f)));
        c.sliderFloat("volume", volume, 0.0f, 1.0f, 200.0f);
        probe.read(c);
        // Um label não identifica item: as consultas continuam a valer para o
        // último widget que se identificou, e o rect é o do label.
        c.label(dummyLabel);
        neighbour.read(c);
        c.endWindow();
    };

    harness.frame(build);
    assert(probe.id != ig::InvalidWidgetId);
    assert(probe.rect.width == 200.0f && probe.rect.height > 0.0f);
    assert(!probe.hovered && !probe.active && !probe.clicked && !probe.edited && !probe.disabled);
    assert(!probe.focused);
    // O label não é um widget: o id fica o do slider e o hover lido depois dele
    // também.
    assert(neighbour.id == probe.id);

    const float x = probe.rect.x + probe.rect.width * 0.5f;
    const float y = probe.rect.y + probe.rect.height * 0.5f;

    context.pushEvent(ig::Event::pointerMove(x, y));
    harness.frame(build);
    assert(probe.hovered);
    assert(!probe.active && !probe.clicked && !probe.edited);
    // Um label submetido a seguir não rouba o hover do slider.
    assert(neighbour.hovered);

    // Premir fica no item: activo, ainda sem clique (o clique é medido na
    // libertação).
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
    harness.frame(build);
    assert(probe.active);
    assert(!probe.clicked);
    assert(probe.focused);

    // Arrastar para a direita muda o valor: passa a edited com o item activo.
    context.pushEvent(ig::Event::pointerMove(x + 30.0f, y));
    harness.frame(build);
    assert(probe.active);
    assert(probe.edited);
    assert(volume > 0.25f);

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x + 30.0f, y));
    harness.frame(build);
    assert(probe.clicked);
    assert(!probe.active);

    // Sem hover nem clique: nada reage fora do widget.
    context.pushEvent(ig::Event::pointerMove(x, y + 120.0f));
    harness.frame(build);
    assert(!probe.hovered && !probe.clicked && !probe.active);
    assert(!probe.edited);
}

// Um clique sem arrasto também conta como edit - o slider saltou para o ponto
// onde se clicou.
void test_click_without_drag_edits()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float volume = 0.0f;
    Probe probe;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("click", ig::Rect(0.0f, 0.0f, 320.0f, 200.0f)));
        c.sliderFloat("volume", volume, 0.0f, 1.0f, 200.0f);
        probe.read(c);
        c.endWindow();
    };

    harness.frame(build);
    // O centro da linha cai na track: um clique sem arrasto salta o valor para
    // lá no próprio premir
    const float x = probe.rect.x + probe.rect.width * 0.5f;
    const float y = probe.rect.y + probe.rect.height * 0.5f;

    context.pushEvent(ig::Event::pointerMove(x, y));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
    harness.frame(build);
    assert(probe.active);
    assert(probe.edited);
    assert(volume > 0.3f && volume < 0.8f);
    assert(!probe.clicked);

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, y));
    harness.frame(build);
    assert(probe.clicked);
    assert(!probe.active);
    // O valor já mudou no premir; a libertação só fecha o clique.
    assert(volume > 0.3f && volume < 0.8f);
}

// O checkbox é um toggle: um clique em cheio muda o valor e marca edited na
// libertação.
void test_checkbox_item_status()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool enabled = false;
    Probe probe;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("check", ig::Rect(0.0f, 0.0f, 320.0f, 200.0f)));
        c.checkbox("enabled", enabled, ig::Rect(0.0f, 0.0f, 200.0f, 24.0f));
        probe.read(c);
        c.endWindow();
    };

    harness.frame(build);
    const float x = probe.rect.x + 10.0f;
    const float y = probe.rect.y + probe.rect.height * 0.5f;

    context.pushEvent(ig::Event::pointerMove(x, y));
    harness.frame(build);
    assert(probe.hovered && !probe.edited);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
    harness.frame(build);
    assert(probe.active && !probe.edited);
    assert(!enabled);

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, y));
    harness.frame(build);
    assert(probe.clicked && probe.edited);
    assert(enabled);
    assert(probe.id != ig::InvalidWidgetId);
}

// Dois widgets na mesma janela: passar de um para o outro muda o que as
// consultas devolvem - não são acumulativas.
void test_hover_follows_the_pointer_between_items()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    Probe first;
    Probe second;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("two", ig::Rect(0.0f, 0.0f, 320.0f, 240.0f)));
        c.button("first", ig::Rect(0.0f, 0.0f, 120.0f, 24.0f));
        first.read(c);
        c.button("second", ig::Rect(0.0f, 40.0f, 120.0f, 24.0f));
        second.read(c);
        c.endWindow();
    };

    harness.frame(build);
    assert(first.id != second.id);
    assert(first.rect.y < second.rect.y);

    context.pushEvent(ig::Event::pointerMove(first.rect.x + 20.0f, first.rect.y + 12.0f));
    harness.frame(build);
    assert(first.hovered && !second.hovered);

    context.pushEvent(ig::Event::pointerMove(second.rect.x + 20.0f, second.rect.y + 12.0f));
    harness.frame(build);
    assert(!first.hovered && second.hovered);
}

// Dentro de beginDisabled nada reage: nem hover nem clique, e o item diz que
// está desativado.
void test_disabled_item_status()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool enabled = false;
    Probe probe;
    Probe live;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("disabled", ig::Rect(0.0f, 0.0f, 320.0f, 240.0f)));
        c.button("live", ig::Rect(0.0f, 0.0f, 120.0f, 24.0f));
        live.read(c);
        c.beginDisabled();
        c.checkbox("enabled", enabled, ig::Rect(0.0f, 40.0f, 200.0f, 24.0f));
        probe.read(c);
        c.endDisabled();
        c.endWindow();
    };

    harness.frame(build);
    assert(probe.disabled);
    assert(!live.disabled);
    assert(probe.id != ig::InvalidWidgetId);

    const float x = probe.rect.x + 10.0f;
    const float y = probe.rect.y + probe.rect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(x, y));
    harness.frame(build);
    assert(!probe.hovered);
    assert(probe.disabled);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, y));
    harness.frame(build);
    assert(!probe.clicked && !probe.active && !probe.edited);
    assert(!enabled);
}

// Um slider desativado não pode mexer-se: a track não é um botão normal, faz o
// teste do ponteiro ela mesma, e sem a guarda do beginDisabled o arrasto
// continuava a mudar o valor.
void test_disabled_slider_does_not_change()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float volume = 0.5f;
    Probe probe;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("disabled slider", ig::Rect(0.0f, 0.0f, 320.0f, 200.0f)));
        c.beginDisabled();
        c.sliderFloat("volume", volume, 0.0f, 1.0f, 200.0f);
        probe.read(c);
        c.endDisabled();
        c.endWindow();
    };

    harness.frame(build);
    assert(probe.disabled);

    const float x = probe.rect.x + probe.rect.width * 0.5f;
    const float y = probe.rect.y + probe.rect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(x, y));
    harness.frame(build);
    assert(!probe.hovered);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
    harness.frame(build);
    assert(!probe.active && !probe.edited);
    assert(volume == 0.5f);

    context.pushEvent(ig::Event::pointerMove(x + 20.0f, y));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x + 20.0f, y));
    harness.frame(build);
    assert(volume == 0.5f);
    assert(!probe.clicked);
}

// O rect do item é o rect absoluto do widget, e o mesmo para o id: um item
// resolvido por id composto (o rótulo repetido não colide) devolve esse id.
void test_item_rect_and_id_are_absolute()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    Probe probe;
    ig::Rect windowBounds;

    auto build = [&](ig::Context &c) {
        windowBounds = ig::Rect(40.0f, 60.0f, 320.0f, 200.0f);
        assert(c.beginWindow("absolute", windowBounds));
        c.button("go", ig::Rect(20.0f, 30.0f, 90.0f, 22.0f));
        probe.read(c);
        c.endWindow();
    };

    harness.frame(build);
    // O rect local (20, 30) é somado à origem do conteúdo, que está dentro da
    // janela - nunca é o rect local devolvido em bruto.
    assert(probe.rect.x > windowBounds.x + 20.0f);
    assert(probe.rect.y > windowBounds.y + 30.0f);
    assert(probe.rect.width == 90.0f && probe.rect.height == 22.0f);
    assert(probe.id != ig::InvalidWidgetId);

    // O clique tem de bater com itemRect(), senão as consultas mentem sobre
    // onde o widget está.
    bool clicked = false;
    auto clickBuild = [&](ig::Context &c) {
        assert(c.beginWindow("absolute", windowBounds));
        clicked = c.button("go", ig::Rect(20.0f, 30.0f, 90.0f, 22.0f));
        probe.read(c);
        c.endWindow();
    };

    const float x = probe.rect.x + probe.rect.width * 0.5f;
    const float y = probe.rect.y + probe.rect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(x, y));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
    harness.frame(clickBuild);
    assert(probe.active);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, y));
    harness.frame(clickBuild);
    assert(clicked);
    assert(probe.clicked);
}

// O vSlider mede-se pelo eixo dos y, ao contrário: em cima é o máximo. Um clique
// a meio dá ~0.5, um a um quarto do topo dá ~0.75.
void test_vslider_uses_the_vertical_axis()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float level = 0.0f;
    int steps = 0;
    Probe probe;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("vertical", ig::Rect(0.0f, 0.0f, 320.0f, 300.0f)));
        c.vSliderFloat("level", level, 0.0f, 1.0f, ig::Rect(10.0f, 10.0f, 40.0f, 160.0f));
        probe.read(c);
        c.sameLine();
        c.vSliderInt("steps", steps, 0, 10, ig::Rect(60.0f, 10.0f, 30.0f, 160.0f));
        c.endWindow();
    };

    harness.frame(build);
    assert(probe.id != ig::InvalidWidgetId);
    assert(probe.rect.width == 40.0f && probe.rect.height == 160.0f);

    const float x = probe.rect.x + probe.rect.width * 0.5f;
    const float top = probe.rect.y;
    const float middle = probe.rect.y + probe.rect.height * 0.5f;

    context.pushEvent(ig::Event::pointerMove(x, middle));
    harness.frame(build);
    assert(probe.hovered);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, middle));
    harness.frame(build);
    assert(probe.active);
    assert(probe.edited);
    assert(level > 0.4f && level < 0.6f);

    // Subir dentro da faixa aumenta o valor (em cima está o máximo): um quarto
    // abaixo do topo é 0.75. Na horizontal, andar 40 px dentro da faixa valia
    // 0.25, e é este o teste que diz que o eixo é o y.
    context.pushEvent(ig::Event::pointerMove(x, top + probe.rect.height * 0.25f));
    harness.frame(build);
    assert(level > 0.7f && level < 0.8f);

    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x,
                                            top + probe.rect.height * 0.25f));
    harness.frame(build);
    assert(probe.clicked);

    // O inteiro arredonda e fica dentro dos limites: um clique mesmo no topo dá
    // o máximo, não 10.4.
    Probe intProbe;
    auto intBuild = [&](ig::Context &c) {
        assert(c.beginWindow("vertical", ig::Rect(0.0f, 0.0f, 320.0f, 300.0f)));
        c.vSliderInt("steps", steps, 0, 10, ig::Rect(10.0f, 10.0f, 30.0f, 160.0f));
        intProbe.read(c);
        c.endWindow();
    };

    harness.frame(intBuild);
    const float intX = intProbe.rect.x + intProbe.rect.width * 0.5f;
    context.pushEvent(ig::Event::pointerMove(intX, intProbe.rect.y));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, intX, intProbe.rect.y));
    harness.frame(intBuild);
    assert(intProbe.edited);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, intX, intProbe.rect.y));
    harness.frame(intBuild);
    assert(steps == 10);
}

// Um vSlider desativado não se mexe, como o horizontal.
void test_disabled_vslider_does_not_change()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float level = 0.25f;
    Probe probe;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("vertical", ig::Rect(0.0f, 0.0f, 320.0f, 300.0f)));
        c.beginDisabled();
        c.vSliderFloat("level", level, 0.0f, 1.0f, ig::Rect(10.0f, 10.0f, 40.0f, 160.0f));
        probe.read(c);
        c.endDisabled();
        c.endWindow();
    };

    harness.frame(build);
    assert(probe.disabled);

    const float x = probe.rect.x + probe.rect.width * 0.5f;
    const float middle = probe.rect.y + probe.rect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(x, middle));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, middle));
    harness.frame(build);
    assert(!probe.active && !probe.edited);
    assert(level == 0.25f);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, middle));
    harness.frame(build);
    assert(level == 0.25f);
}

// A convenção "##" do ImGui: o texto depois de "##" não se desenha, mas faz
// parte do id - é o que permite ter dois widgets com o mesmo texto visível e
// estados separados, e é o que deixa um campo numérico não desenhar rótulo
// nenhum (o demo usa isto em todo o lado).
void test_label_hides_text_after_double_hash()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    Probe first;
    Probe second;
    Probe hidden;
    bool one = false;
    bool two = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("labels", ig::Rect(0.0f, 0.0f, 400.0f, 320.0f)));
        c.button("Gravar", ig::Rect(0.0f, 0.0f, 120.0f, 24.0f));
        first.read(c);
        // Mesmo texto visível, ids diferentes: o clique tem de ir só para um.
        c.button("Gravar##segundo", ig::Rect(0.0f, 40.0f, 120.0f, 24.0f));
        second.read(c);
        c.checkbox("##so-id", one, ig::Rect(0.0f, 80.0f, 120.0f, 24.0f));
        hidden.read(c);
        c.checkbox("##so-id-dois", two, ig::Rect(0.0f, 120.0f, 120.0f, 24.0f));
        c.endWindow();
    };

    harness.frame(build);
    assert(first.id != second.id);
    assert(hidden.id != ig::InvalidWidgetId);
    // O número de vértices diz se o texto foi desenhado: com "##so-id" o checkbox
    // é só a caixa.
    const size_t withLabel = harness.vertexCount();
    assert(withLabel > 0u);

    // Clicar no primeiro e no segundo com o mesmo texto visível não troca de
    // widget.
    bool clickedFirst = false;
    bool clickedSecond = false;
    auto clickBuild = [&](ig::Context &c) {
        assert(c.beginWindow("labels", ig::Rect(0.0f, 0.0f, 400.0f, 320.0f)));
        clickedFirst = c.button("Gravar", ig::Rect(0.0f, 0.0f, 120.0f, 24.0f));
        clickedSecond = c.button("Gravar##segundo", ig::Rect(0.0f, 40.0f, 120.0f, 24.0f));
        c.endWindow();
    };

    const float x = first.rect.x + 20.0f;
    context.pushEvent(ig::Event::pointerMove(x, first.rect.y + 12.0f));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, first.rect.y + 12.0f));
    harness.frame(clickBuild);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, first.rect.y + 12.0f));
    harness.frame(clickBuild);
    assert(clickedFirst && !clickedSecond);

    context.pushEvent(ig::Event::pointerMove(x, second.rect.y + 12.0f));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, second.rect.y + 12.0f));
    harness.frame(clickBuild);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, second.rect.y + 12.0f));
    harness.frame(clickBuild);
    assert(clickedSecond && !clickedFirst);

    // Um rótulo escondido não ocupa largura no layout: o item seguinte começa
    // logo a seguir à caixa.
    ig::TestBackend backend2;
    ig::Context context2(backend2);
    ig::Harness harness2(context2);
    ig::Rect afterVisible;
    ig::Rect afterHidden;
    bool visibleState = false;
    bool hiddenState = false;
    auto layoutRun = [&](ig::Context &c, bool hide) {
        assert(c.beginWindow("layout", ig::Rect(0.0f, 0.0f, 400.0f, 320.0f)));
        if (hide)
            c.checkbox("##caixa", hiddenState);
        else
            c.checkbox("caixa com rotulo comprido", visibleState);
        c.sameLine();
        c.button("depois");
        ig::Rect bounds = c.itemRect();
        if (hide)
            afterHidden = bounds;
        else
            afterVisible = bounds;
        c.endWindow();
    };

    harness2.frame([&](ig::Context &c) { layoutRun(c, false); });
    harness2.frame([&](ig::Context &c) { layoutRun(c, true); });
    assert(afterHidden.x < afterVisible.x);
}

// A largura do item seguinte: um valor, um item, como no ImGui. Depois volta à
// largura que sobra, e pushItemWidth/popItemWidth valem para um bloco.
void test_item_width_controls()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float va = 0.5f;
    float vb = 0.5f;
    float vc = 0.5f;
    ig::Rect first;
    ig::Rect second;
    ig::Rect pushedOne;
    ig::Rect pushedTwo;
    ig::Rect afterPop;
    float computed = 0.0f;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("width", ig::Rect(0.0f, 0.0f, 400.0f, 320.0f)));
        c.setNextItemWidth(120.0f);
        computed = c.calcItemWidth();
        c.dragFloat("a", va, 0.0f, 1.0f, 0.01f, 120.0f);
        first = c.itemRect();
        // Sem setNextItemWidth outra vez, o item seguinte volta à largura toda.
        c.dragFloat("b", vb, 0.0f, 1.0f, 0.01f);
        second = c.itemRect();
        c.pushItemWidth(90.0f);
        c.dragFloat("c", vc, 0.0f, 1.0f, 0.01f);
        pushedOne = c.itemRect();
        c.dragFloat("d", vc, 0.0f, 1.0f, 0.01f);
        pushedTwo = c.itemRect();
        c.popItemWidth();
        c.dragFloat("e", vc, 0.0f, 1.0f, 0.01f);
        afterPop = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    assert(computed == 120.0f);
    assert(first.width == 120.0f);
    assert(second.width > 200.0f);
    assert(pushedOne.width == 90.0f && pushedTwo.width == 90.0f);
    assert(afterPop.width > 200.0f);
}

// newLine fecha a linha: o item seguinte começa na margem, por baixo.
void test_new_line_wraps_to_the_margin()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool value = false;
    ig::Rect side;
    ig::Rect wrapped;
    ig::Rect margin;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("lines", ig::Rect(0.0f, 0.0f, 400.0f, 320.0f)));
        c.button("primeiro");
        margin = c.itemRect();
        c.sameLine();
        c.checkbox("ao lado", value);
        side = c.itemRect();
        c.newLine();
        c.button("depois");
        wrapped = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    assert(side.y == margin.y);
    assert(side.x > margin.x);
    assert(wrapped.y > margin.y);
    assert(wrapped.x == margin.x);
}

// O grupo é UM item lá fora: três widgets empilhados dentro dele e um sameLine a
// seguir põe o item seguinte à direita do bloco todo, não da última linha. Lá
// dentro a margem é a do grupo, e o rect do grupo é a caixa do que ficou dentro.
void test_group_is_one_item_outside()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool value = false;
    ig::Rect firstInside;
    ig::Rect lastInside;
    ig::Rect group;
    ig::Rect beside;
    bool groupHovered = true;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("groups", ig::Rect(0.0f, 0.0f, 500.0f, 400.0f)));
        c.beginGroup();
        c.button("um");
        firstInside = c.itemRect();
        c.button("dois");
        c.checkbox("tres", value);
        lastInside = c.itemRect();
        c.endGroup();
        group = c.itemRect();
        groupHovered = c.isItemHovered();
        c.sameLine();
        c.button("ao lado");
        beside = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    // Os três itens de dentro começam na margem do grupo (o mesmo x).
    assert(firstInside.x == group.x);
    assert(lastInside.x == group.x);
    // O grupo cobre-os todos e é mais alto do que o último.
    assert(group.width >= lastInside.width - 0.5f);
    assert(group.y == firstInside.y);
    assert(group.height >= lastInside.y + lastInside.height - group.y - 0.5f);
    // O item "ao lado" fica à direita do grupo todo, e não por baixo dele.
    assert(beside.x >= group.x + group.width - 0.5f);
    assert(beside.y == group.y);

    // O grupo não tem id nem clique, mas responde ao ponteiro na caixa toda -
    // mesmo num ponto entre widgets, onde não há nada para clicar.
    const float gapY = (firstInside.y + firstInside.height + lastInside.y) * 0.5f;
    context.pushEvent(ig::Event::pointerMove(group.x + 5.0f, gapY));
    harness.frame(build);
    assert(groupHovered);

    context.pushEvent(ig::Event::pointerMove(group.x + group.width + 40.0f, group.y + 2.0f));
    harness.frame(build);
    assert(!groupHovered);
}

// Colunas lado a lado: cada coluna é um grupo de três sliders empilhados. As
// linhas de dentro de um grupo são as do grupo - se o estado da linha de fora
// vazar para dentro, a segunda linha de cada coluna salta para baixo da altura
// da coluna anterior, e as colunas ficam em escada.
void test_group_columns_line_up()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float values[3] = {0.6f, 0.35f, 0.9f};
    ig::Rect row[3][3];
    ig::Rect column[3];
    ig::Vec2 cursor[3][3];

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("columns", ig::Rect(0.0f, 0.0f, 400.0f, 400.0f)));
        for (int nx = 0; nx < 3; ++nx)
        {
            if (nx > 0)
                c.sameLine();
            c.beginGroup();
            for (int ny = 0; ny < 3; ++ny)
            {
                const ig::Vec2 here = c.cursor();
                cursor[nx][ny] = here;
                c.pushId(nx * 3 + ny);
                c.vSliderFloat("##v", values[nx], 0.0f, 1.0f,
                               ig::Rect(here.x, here.y, 18.0f, 37.0f));
                row[nx][ny] = c.itemRect();
                c.popId();
                c.dummy(18.0f, 37.0f);
            }
            c.endGroup();
            column[nx] = c.itemRect();
        }
        c.endWindow();
    };

    harness.frame(build);
    for (int nx = 0; nx < 3; ++nx)
    {
        // As três colunas têm a mesma altura e o mesmo topo...
        assert(column[nx].y == column[0].y);
        assert(column[nx].height == column[0].height);
        if (nx > 0)
            assert(column[nx].x > column[nx - 1].x);
        // ...e as linhas de dentro caem nos mesmos y em todas.
        for (int ny = 0; ny < 3; ++ny)
        {
            assert(row[nx][ny].y == row[0][ny].y);
            assert(cursor[nx][ny].y == cursor[0][ny].y);
            assert(cursor[nx][ny].x == 0.0f);
            assert(row[nx][ny].x == column[nx].x);
        }
    }
}

// Um grupo dentro de outro e um par desequilibrado não estragam o layout: o
// beginGroup sem endGroup não sobrevive à janela seguinte.
void test_group_nesting_and_imbalance()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect outer;
    ig::Rect innerFirst;
    ig::Rect afterOuter;
    ig::Rect nextWindowItem;
    bool value = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("groups", ig::Rect(0.0f, 0.0f, 500.0f, 400.0f)));
        c.beginGroup();
        c.button("fora");
        c.beginGroup();
        c.button("dentro");
        innerFirst = c.itemRect();
        c.endGroup();
        c.dummy(10.0f, 10.0f);
        c.endGroup();
        outer = c.itemRect();
        c.button("depois do grupo");
        afterOuter = c.itemRect();
        c.endWindow();

        // Segundo container: um beginGroup() sem endGroup() não pode mexer na
        // margem nem no cursor do que vem a seguir.
        assert(c.beginWindow("outro", ig::Rect(520.0f, 0.0f, 300.0f, 200.0f)));
        c.beginGroup();
        c.button("abandonado");
        c.endWindow();
        assert(c.beginWindow("terceiro", ig::Rect(520.0f, 240.0f, 300.0f, 200.0f)));
        c.checkbox("limpo", value);
        nextWindowItem = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    // O grupo de dentro fica dentro do de fora.
    assert(innerFirst.x >= outer.x);
    assert(innerFirst.y >= outer.y);
    assert(outer.height >= innerFirst.y + innerFirst.height - outer.y - 0.5f);
    // E o que vem depois do grupo de fora está por baixo dele, à margem.
    assert(afterOuter.y >= outer.y + outer.height - 0.5f);
    assert(afterOuter.x == outer.x);
    // A janela que ficou com um grupo aberto não contaminou a seguinte.
    assert(nextWindowItem.x == 528.0f);
    assert(nextWindowItem.y == 272.0f);
}

// Um popup aberto é dono do ponteiro na sua área: o que está por baixo dele -
// o conteúdo da janela cujo rect cai dentro do dropdown - não pode receber o
// hover nem o clique só porque o ponto está na área do popup. Perguntar "este
// ponto está na área do popup?" não chega; a pergunta é "estamos a submeter os
// itens do popup?".
void test_open_menu_owns_the_pointer_over_it()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect barButton;
    ig::Rect firstItem;
    ig::Rect contentRect;
    bool itemHovered = false;
    bool contentHovered = true;
    bool contentClicked = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("menus", ig::Rect(0.0f, 0.0f, 320.0f, 260.0f)));
        const ig::Rect bar(0.0f, 0.0f, c.availableWidth(), 26.0f);
        if (c.beginMenuBar(bar))
        {
            // O rect do botão lê-se logo a seguir ao beginMenu (fechado ou
            // aberto); os itens do dropdown só são submetidos quando ele está
            // aberto e são eles que passam a ser o último item.
            const bool menuOpen = c.beginMenu("File");
            barButton = c.itemRect();
            if (menuOpen)
            {
                c.menuItem("Abrir");
                firstItem = c.itemRect();
                itemHovered = c.isItemHovered();
                c.menuItem("Sair");
                c.endMenu();
            }
            c.endMenuBar();
        }
        // Conteúdo mesmo por baixo do dropdown: é isto que estava a receber o
        // hover do popup por cima dele.
        contentClicked = c.button("conteudo", ig::Rect(0.0f, 30.0f, 200.0f, 24.0f));
        contentRect = c.itemRect();
        contentHovered = c.isItemHovered();
        c.endWindow();
    };

    harness.frame(build);
    assert(barButton.width > 0.0f);
    assert(contentRect.width == 200.0f);
    // O conteúdo está mesmo debaixo de onde o dropdown vai abrir.
    assert(contentRect.y >= barButton.y);
    assert(contentRect.y < barButton.y + barButton.height + 40.0f);

    // Abrir o menu: premir e largar no botão da barra.
    const float barX = barButton.x + barButton.width * 0.5f;
    const float barY = barButton.y + barButton.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(barX, barY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, barX, barY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, barX, barY));
    harness.frame(build);
    assert(firstItem.height > 0.0f);

    // Com o dropdown aberto, o ponteiro em cima de um item dele é do item - e o
    // botão de conteúdo por baixo não é hovered.
    const float itemX = firstItem.x + 10.0f;
    const float itemY = firstItem.y + firstItem.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(itemX, itemY));
    harness.frame(build);
    assert(itemHovered);
    assert(!contentHovered);

    // E um clique lá também não chega ao conteúdo.
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, itemX, itemY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, itemX, itemY));
    harness.frame(build);
    assert(!contentClicked);
}

// A barra de menus é um item do layout (como no ImGui, onde o BeginMenuBar abre
// um grupo): o conteúdo começa por baixo dela, e o rato em cima da barra não cai
// no primeiro widget do conteúdo. Sem isto quem não se lembrasse de deixar 26 px
// à mão tinha o primeiro widget debaixo da barra - e o `over` era dele.
void test_menu_bar_reserves_its_strip()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect bar;
    ig::Rect first;
    bool barHovered = false;
    bool firstHovered = true;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("bar", ig::Rect(0.0f, 0.0f, 320.0f, 220.0f)));
        if (c.beginMenuBar(ig::Rect(0.0f, 0.0f, c.availableWidth(), 26.0f)))
        {
            if (c.beginMenu("F"))
            {
                c.menuItem("x");
                c.endMenu();
            }
            c.endMenuBar();
        }
        bar = c.itemRect();
        barHovered = c.isItemHovered();
        c.button("primeiro");
        first = c.itemRect();
        firstHovered = c.isItemHovered();
        c.endWindow();
    };

    harness.frame(build);
    assert(bar.height == 26.0f);
    assert(!barHovered);
    // O botão do conteúdo começa por baixo da barra.
    assert(first.y >= bar.y + bar.height);
    assert(first.y > bar.y + bar.height - 0.5f);

    // Rato em cima da barra: a barra é o item, o botão não.
    context.pushEvent(ig::Event::pointerMove(bar.x + 40.0f, bar.y + bar.height * 0.5f));
    harness.frame(build);
    assert(barHovered);
    assert(!firstHovered);

    // Rato em cima do botão: o contrário.
    context.pushEvent(ig::Event::pointerMove(first.x + 10.0f, first.y + first.height * 0.5f));
    harness.frame(build);
    assert(firstHovered);
    assert(!barHovered);
}

// O mesmo para a lista de um combo aberto: os itens da lista cobrem o que está
// por baixo, e o que está por baixo não recebe o ponteiro.
void test_open_combo_owns_the_pointer_over_its_list()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const ig::StringView items[] = {
        ig::StringView("um"), ig::StringView("dois"), ig::StringView("tres")
    };
    int current = 0;
    ig::Rect comboRect;
    ig::Rect contentRect;
    bool contentHovered = true;
    bool contentClicked = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("combos", ig::Rect(0.0f, 0.0f, 320.0f, 300.0f)));
        const bool comboOpen = c.comboBox("escolha", current, ig::Span<const ig::StringView>(items, 3),
                                          ig::Rect(0.0f, 0.0f, 160.0f, 24.0f));
        (void)comboOpen;
        // Com a lista aberta o último item é um item da lista, por isso o rect do
        // cabeçalho só se apanha com ela fechada (é o primeiro frame).
        if (comboRect.width <= 0.0f)
            comboRect = c.itemRect();
        // Por baixo da lista aberta.
        contentClicked = c.button("conteudo", ig::Rect(0.0f, 60.0f, 160.0f, 24.0f));
        contentRect = c.itemRect();
        contentHovered = c.isItemHovered();
        c.endWindow();
    };

    harness.frame(build);
    assert(comboRect.height > 0.0f);

    const float comboX = comboRect.x + comboRect.width * 0.5f;
    const float comboY = comboRect.y + comboRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(comboX, comboY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, comboX, comboY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, comboX, comboY));
    harness.frame(build);

    // Um ponto que está ao mesmo tempo dentro do botão de conteúdo e dentro da
    // lista aberta (é o caso que interessa: o rect de um cai na área do outro).
    const float itemY = contentRect.y + contentRect.height * 0.25f;
    assert(itemY >= contentRect.y && itemY <= contentRect.y + contentRect.height);
    assert(comboRect.y + comboRect.height * 3.0f >= itemY);
    context.pushEvent(ig::Event::pointerMove(comboX, itemY));
    harness.frame(build);
    assert(!contentHovered);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, comboX, itemY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, comboX, itemY));
    harness.frame(build);
    assert(!contentClicked);
    assert(current == 1);
}

// Quem recebe o hover numa sobreposição é a janela de cima, e trazer para a frente
// a de baixo tem de mudar isso no mesmo frame: a resposta a "que janela está
// debaixo do ponteiro" é respondida uma vez por frame (windowAtPointer) e quem
// mexe na ordem tem de a deitar fora.
void test_hover_follows_the_window_that_is_raised()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect lowerButton;
    ig::Rect upperButton;
    bool lowerHovered = false;
    bool upperHovered = false;

    auto build = [&](ig::Context &c) {
        // A janela de baixo, primeiro.
        assert(c.beginWindow("baixo", ig::Rect(0.0f, 0.0f, 260.0f, 200.0f)));
        c.button("em baixo", ig::Rect(0.0f, 0.0f, 120.0f, 40.0f));
        lowerButton = c.itemRect();
        lowerHovered = c.isItemHovered();
        c.endWindow();
        // E a de cima, sobreposta: é criada depois, por isso fica à frente.
        assert(c.beginWindow("cima", ig::Rect(60.0f, 0.0f, 260.0f, 200.0f)));
        c.button("em cima", ig::Rect(0.0f, 0.0f, 120.0f, 40.0f));
        upperButton = c.itemRect();
        upperHovered = c.isItemHovered();
        c.endWindow();
    };

    harness.frame(build);
    // A de cima tapa a parte direita do botão de baixo.
    assert(upperButton.x < lowerButton.x + lowerButton.width);
    assert(lowerButton.x < upperButton.x);

    // Um ponto na sobreposição: é da janela de cima.
    const float overlapX = upperButton.x + 10.0f;
    context.pushEvent(ig::Event::pointerMove(overlapX, lowerButton.y + 10.0f));
    harness.frame(build);
    assert(upperHovered);
    assert(!lowerHovered);

    // Premir na parte do botão de baixo que está à vista traz a janela de baixo
    // para a frente...
    context.pushEvent(ig::Event::pointerMove(lowerButton.x + 5.0f, lowerButton.y + 10.0f));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, lowerButton.x + 5.0f,
                                             lowerButton.y + 10.0f));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, lowerButton.x + 5.0f,
                                           lowerButton.y + 10.0f));
    harness.frame(build);

    // ...e no mesmo frame seguinte a sobreposição já é dela.
    context.pushEvent(ig::Event::pointerMove(overlapX, lowerButton.y + 10.0f));
    harness.frame(build);
    assert(lowerHovered);
    assert(!upperHovered);
}

} // namespace

int main()
{
    test_slider_item_status();
    test_click_without_drag_edits();
    test_checkbox_item_status();
    test_hover_follows_the_pointer_between_items();
    test_disabled_item_status();
    test_disabled_slider_does_not_change();
    test_item_rect_and_id_are_absolute();
    test_vslider_uses_the_vertical_axis();
    test_disabled_vslider_does_not_change();
    test_label_hides_text_after_double_hash();
    test_item_width_controls();
    test_new_line_wraps_to_the_margin();
    test_group_is_one_item_outside();
    test_group_nesting_and_imbalance();
    test_group_columns_line_up();
    test_open_menu_owns_the_pointer_over_it();
    test_menu_bar_reserves_its_strip();
    test_hover_follows_the_window_that_is_raised();
    test_open_combo_owns_the_pointer_over_its_list();
    printf("test_item_status: all tests passed\n");
    return 0;
}
