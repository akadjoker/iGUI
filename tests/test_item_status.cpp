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

// A pilha do pushItemWidth: o pop de dentro volta à largura de fora, e um push de
// 0 herda a que estava (não é "a que sobra").
void test_item_width_stack()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float value = 0.5f;
    ig::Rect outer;
    ig::Rect inner;
    ig::Rect backToOuter;
    ig::Rect inherited;
    ig::Rect afterAll;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("pilha", ig::Rect(0.0f, 0.0f, 400.0f, 320.0f)));
        c.pushItemWidth(100.0f);
        c.dragFloat("a", value, 0.0f, 1.0f, 0.01f);
        outer = c.itemRect();
        c.pushItemWidth(50.0f);
        c.dragFloat("b", value, 0.0f, 1.0f, 0.01f);
        inner = c.itemRect();
        c.popItemWidth();
        c.dragFloat("c", value, 0.0f, 1.0f, 0.01f);
        backToOuter = c.itemRect();
        c.pushItemWidth(0.0f);
        c.dragFloat("d", value, 0.0f, 1.0f, 0.01f);
        inherited = c.itemRect();
        c.popItemWidth();
        c.popItemWidth();
        c.dragFloat("e", value, 0.0f, 1.0f, 0.01f);
        afterAll = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    assert(outer.width == 100.0f);
    assert(inner.width == 50.0f);
    assert(backToOuter.width == 100.0f);
    assert(inherited.width == 100.0f);
    assert(afterAll.width > 200.0f);
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

// O atalho de um item de menu é desenhado à direita e entra na largura do popup:
// sem isso um menu com atalhos cortava-os, ou alargava o menu a cada frame.
void test_menu_item_draws_its_shortcut()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect barButton;
    ig::Rect plainItem;
    ig::Rect shortcutItem;
    bool clickedShortcut = true;
    float withShortcut = 0.0f;
    float withoutShortcut = 0.0f;

    auto build = [&](ig::Context &c, bool withShortcutItem) {
        assert(c.beginWindow("menus", ig::Rect(0.0f, 0.0f, 320.0f, 260.0f)));
        if (c.beginMenuBar(ig::Rect(0.0f, 0.0f, c.availableWidth(), 26.0f)))
        {
            const bool menuOpen = c.beginMenu("F");
            barButton = c.itemRect();
            if (menuOpen)
            {
                // O atalho é o texto mais comprido do popup, por isso é ele que
                // manda na largura (com rótulos curtos o menu fica na largura
                // mínima e não se vê diferença nenhuma).
                c.menuItem("Undo");
                plainItem = c.itemRect();
                if (withShortcutItem)
                {
                    clickedShortcut = c.menuItem(
                        "Redo", ig::StringView("Ctrl+Shift+Alt+Super+Q e mais teclas"));
                    shortcutItem = c.itemRect();
                    // O rect do item é o da linha do menu, não o do atalho: os dois
                    // itens têm a mesma altura e o mesmo x.
                    assert(shortcutItem.x == plainItem.x);
                    assert(shortcutItem.height == plainItem.height);
                }
                c.endMenu();
            }
            c.endMenuBar();
        }
        c.endWindow();
    };

    // Um frame fechado primeiro: é dele que sai o rect do botão da barra.
    harness.frame([&](ig::Context &c) { build(c, false); });
    assert(barButton.width > 0.0f);

    // Abrir o menu.
    const float barX = barButton.x + barButton.width * 0.5f;
    const float barY = barButton.y + barButton.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(barX, barY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, barX, barY));
    harness.frame([&](ig::Context &c) { build(c, false); });
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, barX, barY));
    harness.frame([&](ig::Context &c) { build(c, false); });

    // O atalho tem de caber: o popup mede a largura dos itens (com o atalho) e
    // usa-a no frame seguinte, por isso a largura do item cresce quando ele lá está.
    for (int i = 0; i < 4; ++i)
        harness.frame([&](ig::Context &c) { build(c, true); });
    withShortcut = shortcutItem.width;
    for (int i = 0; i < 4; ++i)
        harness.frame([&](ig::Context &c) { build(c, false); });
    withoutShortcut = plainItem.width;
    assert(withShortcut > withoutShortcut);

}

// O popup de um combo de conteúdo livre: os itens submetem-se lá dentro como em
// qualquer sítio, o clique escolhe, o popup fecha ao escolher, o que é submetido
// manda no tamanho da caixa, e o layout de fora avança só o cabeçalho.
void test_begin_combo_takes_free_content()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const ig::StringView items[] = {
        ig::StringView("um"), ig::StringView("dois"), ig::StringView("tres")
    };
    int current = 0;
    ig::Rect headerRect;
    ig::Rect itemRects[3];
    ig::Rect afterRect;
    float afterYClosed = 0.0f;
    float afterYOpen = 0.0f;
    bool open = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("combo", ig::Rect(0.0f, 0.0f, 320.0f, 300.0f)));
        open = c.beginCombo("escolha",
                            items[static_cast<ig::Span<const ig::StringView>::size_type>(current)]);
        if (open)
        {
            for (int i = 0; i < 3; ++i)
            {
                if (c.selectable(items[i], current == i))
                    current = i;
                itemRects[i] = c.itemRect();
            }
            c.endCombo();
        }
        else if (headerRect.width <= 0.0f)
        {
            // Fechado, o último item é o cabeçalho: é o primeiro frame que dá o rect.
            headerRect = c.itemRect();
        }
        // Um widget de layout depois do combo: o popup não é do layout de fora, por
        // isso ele tem de cair no mesmo sítio com o popup aberto e fechado.
        c.smallButton("depois");
        afterRect = c.itemRect();
        if (open)
            afterYOpen = afterRect.y;
        else
            afterYClosed = afterRect.y;
        c.endWindow();
    };

    harness.frame(build);
    assert(headerRect.height > 0.0f);
    assert(afterYClosed > headerRect.y + headerRect.height);

    // Abrir: o clique no cabeçalho abre, como no comboBox.
    const float headerX = headerRect.x + headerRect.width * 0.5f;
    const float headerY = headerRect.y + headerRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(headerX, headerY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    assert(open);

    // O primeiro item começa por baixo do cabeçalho e estica até à largura de
    // dentro do popup: como no ImGui, o popup tem a largura do cabeçalho e o
    // conteúdo fica com a margem da janela de fora do popup.
    assert(itemRects[0].width ==
           headerRect.width - context.theme().windowPadding * 2.0f);
    assert(itemRects[0].height == headerRect.height);
    assert(itemRects[1].y > itemRects[0].y + itemRects[0].height - 0.5f);
    // O layout de fora fica no mesmo sítio: o popup não empurra o que vem depois.
    assert(afterYOpen == afterYClosed);
    assert(afterYOpen < itemRects[2].y);

    // Escolher um item com o ponteiro: o selectable devolve o clique e fecha a lista.
    const float itemX = itemRects[1].x + itemRects[1].width * 0.5f;
    const float itemY = itemRects[1].y + itemRects[1].height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(itemX, itemY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, itemX, itemY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, itemX, itemY));
    harness.frame(build);
    assert(current == 1);
    // O frame do clique ainda tem o popup aberto (o beginCombo já respondeu); é o
    // seguinte que o vê fechado.
    harness.frame(build);
    assert(!open);
}

// O popup de um combo de conteúdo livre é dele: o que está por baixo não recebe
// o ponteiro, e um item mais largo que o cabeçalho faz o popup crescer no frame
// seguinte (é a medida que o ImGui tira do AutoFitFrames).
void test_begin_combo_popup_owns_the_pointer_and_grows()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const ig::StringView items[] = {
        ig::StringView("um"), ig::StringView("dois")
    };
    const ig::StringView wide("um rotulo bem mais comprido do que o cabecalho do combo");
    int current = 0;
    ig::Rect headerRect;
    ig::Rect underRect;
    ig::Rect itemRects[2];
    bool underHovered = true;
    bool underClicked = false;
    bool wideItem = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("combo", ig::Rect(0.0f, 0.0f, 320.0f, 300.0f)));
        const bool open = c.beginCombo("escolha", items[0]);
        if (open)
        {
            for (int i = 0; i < 2; ++i)
            {
                if (c.selectable(items[i], current == i))
                    current = i;
                itemRects[i] = c.itemRect();
            }
            if (wideItem)
                c.label(wide);
            c.endCombo();
        }
        else if (headerRect.width <= 0.0f)
        {
            headerRect = c.itemRect();
        }
        // Um botão do tamanho do popup, submetido depois dele: é o que está por
        // baixo da lista e o que não pode receber nada enquanto ela está aberta.
        underClicked = c.button("por baixo", ig::Rect(0.0f, 24.0f, 160.0f, 72.0f));
        underRect = c.itemRect();
        underHovered = c.isItemHovered();
        c.endWindow();
    };

    harness.frame(build);

    // Abrir.
    const float headerX = headerRect.x + headerRect.width * 0.5f;
    const float headerY = headerRect.y + headerRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(headerX, headerY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);

    // Um ponto que cai ao mesmo tempo no segundo item e no botão de baixo.
    const float itemY = itemRects[1].y + itemRects[1].height * 0.5f;
    assert(itemY > underRect.y && itemY < underRect.y + underRect.height);
    context.pushEvent(ig::Event::pointerMove(headerX, itemY));
    harness.frame(build);
    assert(!underHovered);

    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, headerX, itemY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, headerX, itemY));
    harness.frame(build);
    assert(!underClicked);
    assert(current == 1);

    // O popup fechou ao escolher: reabri-lo para medir a largura com um item largo.
    wideItem = true;
    context.pushEvent(ig::Event::pointerMove(headerX, headerY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    assert(itemRects[0].width == headerRect.width - context.theme().windowPadding * 2.0f);
    // Um item mais largo que o cabeçalho faz o popup crescer no frame seguinte, e os
    // itens esticam com ele.
    for (int i = 0; i < 3; ++i)
        harness.frame(build);
    assert(itemRects[0].width > headerRect.width);
}

// A barra de menu da aplicação: uma janela no topo do ecrã, fora das janelas do
// utilizador, com o menu aberto por baixo dela e a troca de menu por hover (é o
// que o foco na barra dá).
void test_main_menu_bar()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect fileRect;
    ig::Rect viewRect;
    ig::Rect itemRect;
    ig::Rect viewItemRect;
    ig::Rect windowButtonRect;
    int opened = 0;
    bool fileOpen = false;
    bool viewOpen = false;

    auto build = [&](ig::Context &c) {
        if (c.beginMainMenuBar())
        {
            fileOpen = c.beginMenu("File");
            if (fileOpen)
            {
                if (c.menuItem("Open"))
                    ++opened;
                itemRect = c.itemRect();
                c.endMenu();
            }
            // Com o menu fechado o último item é o botão da barra.
            if (fileRect.width <= 0.0f)
                fileRect = c.itemRect();
            viewOpen = c.beginMenu("View");
            if (viewOpen)
            {
                if (c.menuItem("Zoom"))
                    ++opened;
                viewItemRect = c.itemRect();
                c.endMenu();
            }
            if (viewRect.width <= 0.0f)
                viewRect = c.itemRect();
            c.endMainMenuBar();
        }
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 300.0f, 200.0f)));
        c.button("na janela", ig::Rect(0.0f, 0.0f, 80.0f, 24.0f));
        if (windowButtonRect.width <= 0.0f)
            windowButtonRect = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    // A barra é a faixa do topo, e o que está na janela não é ela.
    assert(fileRect.y == 0.0f);
    assert(fileRect.height == context.mainMenuBarHeight());
    assert(viewRect.x > fileRect.x + fileRect.width - 0.5f);
    assert(windowButtonRect.y >= context.mainMenuBarHeight());

    // Abrir o menu, com o mesmo clique de sempre: carregar e largar.
    const float fileX = fileRect.x + fileRect.width * 0.5f;
    const float fileY = fileRect.y + fileRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(fileX, fileY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, fileX, fileY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, fileX, fileY));
    harness.frame(build);
    assert(fileOpen);
    // O popup sai por baixo da barra, não por cima dela.
    assert(itemRect.y >= fileRect.y + fileRect.height);

    // Passar o rato para o menu ao lado troca de menu aberto: é o foco que a barra
    // ganhou com o clique que o permite. A troca acontece no beginMenu do "View",
    // que é submetido depois do "File" - por isso é no frame seguinte que o "File"
    // já se vê fechado.
    const float viewX = viewRect.x + viewRect.width * 0.5f;
    context.pushEvent(ig::Event::pointerMove(viewX, fileY));
    harness.frame(build);
    harness.frame(build);
    assert(viewOpen);
    assert(!fileOpen);

    // E um item do menu responde ao clique (o popup do View, que é o que está
    // aberto agora).
    const float itemX = viewItemRect.x + viewItemRect.width * 0.5f;
    const float itemY = viewItemRect.y + viewItemRect.height * 0.5f;
    assert(itemY >= viewRect.y + viewRect.height);
    context.pushEvent(ig::Event::pointerMove(itemX, itemY));
    harness.frame(build);
    assert(opened == 0);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, itemX, itemY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, itemX, itemY));
    harness.frame(build);
    assert(opened == 1);
}

// A escolha múltipla: o clique escolhe um e desmarca os outros, o Ctrl liga/desliga
// um sem mexer nos outros, o Shift escolhe o alcance desde o último clique, o Ctrl+A
// escolhe tudo e o Escape limpa.
void test_multi_select()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const ig::StringView items[] = {
        ig::StringView("um"), ig::StringView("dois"), ig::StringView("tres"), ig::StringView("quatro")
    };
    bool selected[4] = {false, false, false, false};
    ig::Rect rects[4];
    bool itemSelected[4] = {false, false, false, false};
    bool itemToggled[4] = {false, false, false, false};
    unsigned flags = ig::Context::MultiSelectFlags::ClearOnEscape;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("lista", ig::Rect(0.0f, 0.0f, 300.0f, 300.0f)));
        assert(c.beginMultiSelect(ig::Span<bool>(selected, 4), flags));
        for (int i = 0; i < 4; ++i)
        {
            c.selectable(items[i], selected[i], 120.0f);
            if (rects[i].width <= 0.0f)
                rects[i] = c.itemRect();
            // As consultas são do item que acabou de passar.
            itemSelected[i] = c.isItemSelected();
            itemToggled[i] = c.isItemToggledSelection();
        }
        c.endMultiSelect();
        c.endWindow();
    };

    auto clickItem = [&](int index, bool control, bool shift) {
        const float x = rects[index].x + rects[index].width * 0.5f;
        const float y = rects[index].y + rects[index].height * 0.5f;
        context.pushEvent(ig::Event::pointerMove(x, y));
        context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y, control, shift));
        harness.frame(build);
        context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, y, control, shift));
        harness.frame(build);
    };

    harness.frame(build);
    for (int i = 0; i < 4; ++i)
        assert(rects[i].width > 0.0f);
    assert(!selected[0] && !selected[1] && !selected[2] && !selected[3]);

    // Um clique sem teclas: este e mais nenhum.
    clickItem(1, false, false);
    assert(!selected[0] && selected[1] && !selected[2] && !selected[3]);
    assert(itemSelected[1]);
    assert(itemToggled[1]);
    assert(!itemToggled[0]);

    // Ctrl: liga outro sem mexer no que estava.
    clickItem(3, true, false);
    assert(selected[1] && selected[3] && !selected[0] && !selected[2]);
    assert(itemSelected[3]);
    // E outra vez no mesmo desliga-o.
    clickItem(3, true, false);
    assert(!selected[3] && selected[1]);

    // Shift: limpa e escolhe o alcance desde o item do último clique (o 3, e só ele).
    clickItem(3, false, true);
    assert(!selected[0] && !selected[1] && !selected[2] && selected[3]);

    // Com o item do último clique escolhido, o alcance é o que fica entre ele e este.
    clickItem(0, false, false);
    clickItem(2, false, true);
    assert(selected[0] && selected[1] && selected[2] && !selected[3]);

    // Com Ctrl o alcance copia o estado desse item: um item do alcance desligado
    // desliga o alcance todo, em vez de o escolher.
    clickItem(2, true, false);
    assert(!selected[2] && selected[1]);
    clickItem(0, true, true);
    assert(!selected[0] && !selected[1] && !selected[2] && !selected[3]);

    // Ctrl+A escolhe tudo e o Escape limpa.
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::A, true));
    harness.frame(build);
    assert(selected[0] && selected[1] && selected[2] && selected[3]);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Escape));
    harness.frame(build);
    assert(!selected[0] && !selected[1] && !selected[2] && !selected[3]);

    // Um item de cada vez: o Ctrl e o Shift deixam de acrescentar.
    flags = ig::Context::MultiSelectFlags::SingleSelect |
            ig::Context::MultiSelectFlags::ClearOnEscape;
    clickItem(0, false, false);
    assert(selected[0] && !selected[1]);
    clickItem(2, true, false);
    assert(!selected[0] && selected[2] && !selected[1] && !selected[3]);
    clickItem(0, false, true);
    assert(selected[0] && !selected[2]);

    // Sem alcance: o Shift é um clique como os outros.
    flags = ig::Context::MultiSelectFlags::NoRangeSelect;
    clickItem(1, false, false);
    clickItem(3, false, true);
    assert(!selected[1] && selected[3]);
}

// As transições do item: isItemActivated (ficou ativo agora), isItemDeactivated
// (deixou de estar), isItemDeactivatedAfterEdit (e foi editado enquanto esteve) e
// isItemVisible (ainda cai dentro do que se vê).
void test_item_activation_queries()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect buttonRect;
    bool activated = false;
    bool deactivated = false;
    bool afterEdit = false;
    bool visible = true;
    int clicks = 0;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("ativacao", ig::Rect(0.0f, 0.0f, 320.0f, 240.0f)));
        if (c.button("alvo", ig::Rect(0.0f, 0.0f, 120.0f, 28.0f)))
            ++clicks;
        if (buttonRect.width <= 0.0f)
            buttonRect = c.itemRect();
        activated = c.isItemActivated();
        deactivated = c.isItemDeactivated();
        afterEdit = c.isItemDeactivatedAfterEdit();
        visible = c.isItemVisible();
        // Um item bem para fora da janela: o rect dele não cai no clip.
        c.button("fora", ig::Rect(0.0f, 400.0f, 120.0f, 28.0f));
        if (visible)
            visible = c.isItemVisible();
        c.endWindow();
    };

    harness.frame(build);
    assert(!activated && !deactivated && !visible);
    assert(buttonRect.width > 0.0f);

    // Carregar: o item fica ativo neste frame.
    const float x = buttonRect.x + buttonRect.width * 0.5f;
    const float y = buttonRect.y + buttonRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(x, y));
    harness.frame(build);
    assert(!activated);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
    harness.frame(build);
    assert(activated);
    assert(!deactivated);

    // Um frame a segurar: já não está a ficar ativo, e continua ativo.
    harness.frame(build);
    assert(!activated && !deactivated);

    // Largar: deixa de estar ativo neste frame, e o clique conta.
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, y));
    harness.frame(build);
    assert(deactivated);
    assert(clicks == 1);
    // Nada foi editado enquanto esteve ativo: não é um "depois de editar".
    assert(!afterEdit);

    harness.frame(build);
    assert(!deactivated);
    assert(!afterEdit);
}

// O mesmo para um slider: largar depois de mexer é o isItemDeactivatedAfterEdit,
// que é o que um editor usa para gravar o valor.
void test_item_deactivated_after_edit()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float value = 0.5f;
    ig::Rect sliderRect;
    bool afterEdit = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("edicao", ig::Rect(0.0f, 0.0f, 320.0f, 240.0f)));
        c.sliderFloat("v", value, 0.0f, 1.0f, ig::Rect(0.0f, 0.0f, 200.0f, 28.0f));
        if (sliderRect.width <= 0.0f)
            sliderRect = c.itemRect();
        afterEdit = c.isItemDeactivatedAfterEdit();
        c.endWindow();
    };

    harness.frame(build);

    // Carregar na pista e arrastar (é a pista que mexe no valor).
    const float trackX = sliderRect.x + sliderRect.width * 0.6f;
    const float y = sliderRect.y + sliderRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(trackX, y));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, trackX, y));
    harness.frame(build);
    assert(!afterEdit);
    context.pushEvent(ig::Event::pointerMove(trackX + 40.0f, y));
    harness.frame(build);
    assert(value > 0.5f);
    assert(!afterEdit);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, trackX + 40.0f, y));
    harness.frame(build);
    assert(afterEdit);
    harness.frame(build);
    assert(!afterEdit);
}

// O isItemToggledOpen diz que o tree node abriu ou fechou naquele frame.
void test_item_toggled_open()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool expanded = false;
    ig::Rect arrowRect;
    bool toggled = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("arvore", ig::Rect(0.0f, 0.0f, 320.0f, 240.0f)));
        c.treeNode("nodo", expanded, ig::Rect(0.0f, 0.0f, 140.0f, 24.0f));
        toggled = c.isItemToggledOpen();
        if (arrowRect.width <= 0.0f)
            arrowRect = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    assert(!toggled);
    assert(arrowRect.width > 0.0f);

    // O clique no nodo abre-o, e é este frame que o diz.
    const float x = arrowRect.x + arrowRect.width * 0.5f;
    const float y = arrowRect.y + arrowRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(x, y));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
    harness.frame(build);
    assert(!toggled);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, y));
    harness.frame(build);
    assert(expanded);
    assert(toggled);
    harness.frame(build);
    assert(!toggled);
}

// O popup de um combo com mais itens do que os que cabem: o que não cabe é
// cortado, a roda do rato rola a lista, e a lista volta ao topo quando se fecha.
void test_combo_popup_scrolls()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    // 14 itens, que é o "combo 1" do demo: cabem 8 (o HeightRegular do ImGui).
    const ig::StringView items[] = {
        ig::StringView("01"), ig::StringView("02"), ig::StringView("03"), ig::StringView("04"),
        ig::StringView("05"), ig::StringView("06"), ig::StringView("07"), ig::StringView("08"),
        ig::StringView("09"), ig::StringView("10"), ig::StringView("11"), ig::StringView("12"),
        ig::StringView("13"), ig::StringView("14")
    };
    const int count = 14;
    const int visibleItems = 8;
    int current = 0;
    ig::Rect headerRect;
    ig::Rect itemRects[14];
    bool visible[14] = {false};
    bool open = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("combo", ig::Rect(0.0f, 0.0f, 320.0f, 400.0f)));
        open = c.beginCombo("escolha", items[0], 0.0f, visibleItems);
        if (open)
        {
            for (int i = 0; i < count; ++i)
            {
                if (c.selectable(items[i], current == i))
                    current = i;
                itemRects[i] = c.itemRect();
                visible[i] = c.isItemVisible();
            }
            c.endCombo();
        }
        else if (headerRect.width <= 0.0f)
        {
            headerRect = c.itemRect();
        }
        c.endWindow();
    };

    harness.frame(build);
    const float headerX = headerRect.x + headerRect.width * 0.5f;
    const float headerY = headerRect.y + headerRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(headerX, headerY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    assert(open);

    // Alguns frames para o popup medir o conteúdo (a medida é a do frame anterior).
    for (int i = 0; i < 3; ++i)
        harness.frame(build);

    // Só os primeiros oito itens cabem; os últimos estão fora do clip (ou o clip
    // corta-os a zero, ou nem chegam a ser desenhados).
    for (int i = 0; i < visibleItems; ++i)
        assert(visible[i]);
    for (int i = visibleItems; i < count; ++i)
        assert(!visible[i]);
    const float firstY = itemRects[0].y;
    const float popupBottom = itemRects[visibleItems - 1].y + itemRects[visibleItems - 1].height;

    // A roda por cima do popup rola a lista: o primeiro item sobe e os de baixo
    // entram.
    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    const float insideX = itemRects[0].x + itemRects[0].width * 0.5f;
    const float insideY = itemRects[2].y + itemRects[2].height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(insideX, insideY));
    context.pushEvent(wheel);
    harness.frame(build);
    assert(itemRects[0].y < firstY);
    assert(visible[visibleItems]);
    // O que sai por cima deixa de ser visível (o clip é o do popup).
    assert(!visible[0]);
    // A lista não sai da caixa: o último item que se vê acaba dentro do popup.
    assert(itemRects[visibleItems].y + itemRects[visibleItems].height <= popupBottom + 0.5f);

    // Rolar até ao fim para e fica lá: o último item fica dentro do popup e o
    // primeiro de todos já saiu.
    for (int i = 0; i < 40; ++i)
    {
        context.pushEvent(ig::Event::pointerMove(insideX, insideY));
        context.pushEvent(wheel);
        harness.frame(build);
    }
    assert(visible[count - 1]);
    assert(itemRects[count - 1].y + itemRects[count - 1].height <= popupBottom + 0.5f);
    assert(!visible[0]);
    const float lastY = itemRects[count - 1].y;
    harness.frame(build);
    assert(itemRects[count - 1].y == lastY);

    // Fechar e reabrir volta ao topo.
    context.pushEvent(ig::Event::pointerMove(headerX, headerY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    assert(!open);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    assert(open);
    for (int i = 0; i < 3; ++i)
        harness.frame(build);
    assert(itemRects[0].y == firstY);
}

// O setItemDefaultFocus: a lista abre com o item escolhido à vista, mesmo que ele
// esteja lá para baixo - e só na abertura (depois disso a lista rola à vontade).
void test_combo_default_focus_opens_on_the_selection()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const ig::StringView items[] = {
        ig::StringView("01"), ig::StringView("02"), ig::StringView("03"), ig::StringView("04"),
        ig::StringView("05"), ig::StringView("06"), ig::StringView("07"), ig::StringView("08"),
        ig::StringView("09"), ig::StringView("10"), ig::StringView("11"), ig::StringView("12")
    };
    const int count = 12;
    // A escolha está no fim: sem o default focus a lista abria no topo e ela ficava
    // fora de vista.
    int current = 10;
    ig::Rect headerRect;
    ig::Rect itemRects[12];
    bool visible[12] = {false};
    bool open = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("combo", ig::Rect(0.0f, 0.0f, 320.0f, 400.0f)));
        open = c.beginCombo("escolha", items[current], 0.0f, 8);
        if (open)
        {
            for (int i = 0; i < count; ++i)
            {
                if (c.selectable(items[i], current == i))
                    current = i;
                itemRects[i] = c.itemRect();
                visible[i] = c.isItemVisible();
                if (current == i)
                    c.setItemDefaultFocus();
            }
            c.endCombo();
        }
        else if (headerRect.width <= 0.0f)
        {
            headerRect = c.itemRect();
        }
        c.endWindow();
    };

    harness.frame(build);
    const float headerX = headerRect.x + headerRect.width * 0.5f;
    const float headerY = headerRect.y + headerRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(headerX, headerY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    assert(open);

    // A rolagem precisa de um frame com a medida do conteúdo.
    for (int i = 0; i < 3; ++i)
        harness.frame(build);
    assert(visible[10]);
    assert(!visible[0]);

    // Depois de aberta, rolar à mão não é desfeito pelo pedido do item escolhido:
    // ele é pedido a cada frame (é o que o demo faz) e a lista fica onde o mão a
    // deixou.
    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = 1.0f;   // para cima, até ao topo
    const float insideX = itemRects[10].x + itemRects[10].width * 0.5f;
    const float insideY = itemRects[10].y + itemRects[10].height * 0.5f;
    for (int i = 0; i < 20; ++i)
    {
        context.pushEvent(ig::Event::pointerMove(insideX, insideY));
        context.pushEvent(wheel);
        harness.frame(build);
    }
    assert(visible[0]);
    assert(!visible[10]);
    // E aí fica, mesmo com o setItemDefaultFocus a ser pedido outra vez.
    for (int i = 0; i < 3; ++i)
        harness.frame(build);
    assert(visible[0]);
    assert(!visible[10]);
}

// Um beginChild dentro do popup de um combo: o child tem o seu cursor, a sua
// barra e o seu corte, mas quem manda no ponteiro e na lista continua a ser o
// popup (o child não é uma janela, é um bloco de layout).
void test_child_inside_a_combo_popup()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const ig::StringView items[] = {
        ig::StringView("um"), ig::StringView("dois"), ig::StringView("tres"), ig::StringView("quatro"),
        ig::StringView("cinco"), ig::StringView("seis"), ig::StringView("sete"), ig::StringView("oito")
    };
    const int count = 8;
    int current = 0;
    int clicks = 0;
    ig::Rect headerRect;
    ig::Rect itemRects[8];
    ig::Rect childRect;
    bool open = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("combo", ig::Rect(0.0f, 0.0f, 320.0f, 400.0f)));
        open = c.beginCombo("escolha", items[0]);
        if (open)
        {
            // Uma lista com a sua própria caixa dentro do popup, como o demo faz
            // com o "Options" do menu de ficheiros (um child com texto a rolar).
            c.beginChild("child", 70.0f);
            childRect = ig::Rect(0.0f, 0.0f, c.availableWidth(), 70.0f);
            for (int i = 0; i < count; ++i)
            {
                if (c.selectable(items[i], current == i))
                {
                    current = i;
                    ++clicks;
                }
                itemRects[i] = c.itemRect();
            }
            c.endChild();
            c.endCombo();
        }
        else if (headerRect.width <= 0.0f)
        {
            headerRect = c.itemRect();
        }
        c.endWindow();
    };

    harness.frame(build);
    const float headerX = headerRect.x + headerRect.width * 0.5f;
    const float headerY = headerRect.y + headerRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(headerX, headerY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    assert(open);
    for (int i = 0; i < 3; ++i)
        harness.frame(build);

    // Os itens do child começam dentro do popup e descem como sempre: o child tem o
    // próprio cursor, mas é o popup que corta.
    assert(itemRects[0].height > 0.0f);
    assert(itemRects[0].y > headerRect.y + headerRect.height);
    assert(itemRects[1].y > itemRects[0].y + itemRects[0].height - 0.5f);

    // Um item do child responde ao clique como qualquer outro do popup. O clique é
    // no primeiro item: o child é que corta os que não lhe cabem (é para isso que
    // ele tem barra própria), e um item fora do clip do child não é clicável.
    const float itemX = itemRects[1].x + itemRects[1].width * 0.5f;
    const float itemY = itemRects[1].y + itemRects[1].height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(itemX, itemY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, itemX, itemY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, itemX, itemY));
    harness.frame(build);
    assert(current == 1);
    assert(clicks == 1);
    // O frame do clique ainda tem o popup aberto (o beginCombo já respondeu); é o
    // seguinte que o vê fechado.
    harness.frame(build);
    assert(!open);
}

// Os flags do isItemHovered: o RectOnly (basta o rect, o clip não conta), o
// AllowWhenBlockedByActiveItem (outro widget ativo), o AllowWhenBlockedByPopup (um
// menu aberto por cima) e o Delay (tempo parado em cima).
void test_item_hovered_flags()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const unsigned rectOnly = ig::Context::ItemHoverFlags::RectOnly;
    const unsigned allowActive = ig::Context::ItemHoverFlags::AllowWhenBlockedByActiveItem;
    const unsigned delay = ig::Context::ItemHoverFlags::Delay;

    // Um botão dentro de um child baixo: uma parte dele fica fora do clip do child.
    float dragged = 0.5f;
    ig::Rect buttonRect;
    bool plainHovered = true;
    bool rectHovered = false;
    bool delayedInFirstFrame = true;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("hover", ig::Rect(0.0f, 0.0f, 320.0f, 300.0f)));
        c.beginChild("child", 40.0f);
        // Alto e no fim do child: metade de fora.
        c.button("dentro", ig::Rect(0.0f, 20.0f, 100.0f, 40.0f));
        if (buttonRect.width <= 0.0f)
            buttonRect = c.itemRect();
        plainHovered = c.isItemHovered();
        rectHovered = c.isItemHovered(rectOnly);
        if (c.isItemHovered(delay))
            delayedInFirstFrame = false;
        c.endChild();
        c.endWindow();
    };

    harness.frame(build);
    assert(buttonRect.width > 0.0f);

    // O ponteiro é posto na parte do botão que o child corta.
    const float x = buttonRect.x + buttonRect.width * 0.5f;
    const float y = buttonRect.y + buttonRect.height - 2.0f;
    context.pushEvent(ig::Event::pointerMove(x, y));
    harness.frame(build);
    assert(!plainHovered);
    assert(rectHovered);

    // O Delay: nos primeiros frames não responde por esta consulta, e passado o
    // tempo responde.
    bool delayed = false;
    bool plain = false;
    auto probeBuild = [&](ig::Context &c) {
        assert(c.beginWindow("hover", ig::Rect(0.0f, 0.0f, 320.0f, 300.0f)));
        c.button("alvo", ig::Rect(0.0f, 0.0f, 120.0f, 28.0f));
        if (buttonRect.width <= 0.0f)
            buttonRect = c.itemRect();
        plain = c.isItemHovered();
        delayed = c.isItemHovered(delay);
        c.endWindow();
    };
    ig::Rect targetRect;
    auto findTarget = [&](ig::Context &c) {
        assert(c.beginWindow("hover", ig::Rect(0.0f, 0.0f, 320.0f, 300.0f)));
        c.button("alvo", ig::Rect(0.0f, 0.0f, 120.0f, 28.0f));
        targetRect = c.itemRect();
        c.endWindow();
    };
    // Os frames do harness não trazem delta: o relógio do hover é o que se lhe der
    // (é o deltaSeconds que o backend traz a sério).
    auto frameWithDelta = [&](float delta) {
        context.beginFrame(ig::FrameInfo(1280.0f, 800.0f, 1.0f, delta));
        probeBuild(context);
        context.endFrame();
    };
    harness.frame(findTarget);
    const float tx = targetRect.x + targetRect.width * 0.5f;
    const float ty = targetRect.y + targetRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(tx, ty));
    frameWithDelta(0.1f);
    assert(plain);
    assert(!delayed);
    // Passado o atraso (0.45 s por omissão), o mesmo hover responde ao Delay.
    for (int i = 0; i < 5; ++i)
        frameWithDelta(0.1f);
    assert(plain);
    assert(delayed);
    // Mexer o ponteiro dentro do item não reinicia o Delay (conta desde que entrou
    // nele), mas reinicia o Stationary (conta desde a última vez que se mexeu).
    bool stationary = false;
    auto probeStationary = [&](ig::Context &c) {
        assert(c.beginWindow("hover", ig::Rect(0.0f, 0.0f, 320.0f, 300.0f)));
        c.button("alvo", ig::Rect(0.0f, 0.0f, 120.0f, 28.0f));
        stationary = c.isItemHovered(ig::Context::ItemHoverFlags::Stationary);
        delayed = c.isItemHovered(delay);
        c.endWindow();
    };
    context.pushEvent(ig::Event::pointerMove(tx + 6.0f, ty));
    context.beginFrame(ig::FrameInfo(1280.0f, 800.0f, 1.0f, 0.1f));
    probeStationary(context);
    context.endFrame();
    assert(delayed);
    assert(!stationary);
    for (int i = 0; i < 5; ++i)
    {
        context.beginFrame(ig::FrameInfo(1280.0f, 800.0f, 1.0f, 0.1f));
        probeStationary(context);
        context.endFrame();
    }
    assert(stationary);

    // O AllowWhenBlockedByActiveItem: com outro widget ativo (um arrasto a passar
    // por cima), o hover normal não responde mas o flag sim.
    bool blocked = true;
    bool allowed = false;
    auto dragBuild = [&](ig::Context &c) {
        assert(c.beginWindow("hover", ig::Rect(0.0f, 0.0f, 320.0f, 300.0f)));
        c.sliderFloat("arrasto", dragged, 0.0f, 1.0f, ig::Rect(0.0f, 0.0f, 200.0f, 28.0f));
        c.button("alvo", ig::Rect(0.0f, 60.0f, 120.0f, 28.0f));
        blocked = c.isItemHovered();
        allowed = c.isItemHovered(allowActive);
        c.endWindow();
    };
    ig::Rect dragRect;
    ig::Rect hoverRect;
    auto findBoth = [&](ig::Context &c) {
        assert(c.beginWindow("hover", ig::Rect(0.0f, 0.0f, 320.0f, 300.0f)));
        c.sliderFloat("arrasto", dragged, 0.0f, 1.0f, ig::Rect(0.0f, 0.0f, 200.0f, 28.0f));
        dragRect = c.itemRect();
        c.button("alvo", ig::Rect(0.0f, 60.0f, 120.0f, 28.0f));
        hoverRect = c.itemRect();
        c.endWindow();
    };
    harness.frame(findBoth);
    // Carregar na pista do slider e arrastar até cima do botão. O itemRect do slider
    // é o widget todo: a pista está no meio, entre o rótulo e o campo do valor (é o
    // meio da largura que cai lá, os extremos são os dois).
    const float dragX = dragRect.x + dragRect.width * 0.5f;
    const float dragY = dragRect.y + dragRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(dragX, dragY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, dragX, dragY));
    harness.frame(dragBuild);
    context.pushEvent(ig::Event::pointerMove(hoverRect.x + hoverRect.width * 0.5f,
                                            hoverRect.y + hoverRect.height * 0.5f));
    harness.frame(dragBuild);
    assert(blocked == false);
    assert(allowed == true);
}

// O corpo de um menu aberto é um contentor de layout: um slider (ou um child, um
// input, um combo) submetido lá dentro fica dentro do popup, e o item que vier
// depois cai por baixo dele. É o "Options" do menu de ficheiros do demo.
void test_widgets_inside_an_open_menu()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float value = 0.5f;
    int separators = 0;
    ig::Rect headerRect;
    ig::Rect firstItemRect;
    ig::Rect sliderRect;
    ig::Rect afterItemRect;
    bool sliderChanged = false;
    bool open = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("menu", ig::Rect(0.0f, 0.0f, 400.0f, 320.0f)));
        if (c.beginMenuBar(ig::Rect(0.0f, 0.0f, c.availableWidth(), 26.0f)))
        {
            open = c.beginMenu("F");
            if (headerRect.width <= 0.0f)
                headerRect = c.itemRect();
            if (open)
            {
                c.menuItem("New");
                firstItemRect = c.itemRect();
                sliderChanged = c.sliderFloat("Value", value, 0.0f, 1.0f);
                sliderRect = c.itemRect();
                c.menuSeparator();
                ++separators;
                c.menuItem("Depois");
                afterItemRect = c.itemRect();
                c.endMenu();
            }
            c.endMenuBar();
        }
        c.endWindow();
    };

    harness.frame(build);
    const float headerX = headerRect.x + headerRect.width * 0.5f;
    const float headerY = headerRect.y + headerRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(headerX, headerY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, headerX, headerY));
    harness.frame(build);
    assert(open);
    harness.frame(build);

    // O slider está dentro do popup: o x é o do menu e o y é por baixo do primeiro
    // item (que começa no topo do popup).
    assert(firstItemRect.height == context.theme().menuItemHeight);
    assert(sliderRect.x == firstItemRect.x);
    assert(sliderRect.width == firstItemRect.width);
    assert(sliderRect.y >= firstItemRect.y + firstItemRect.height - 0.5f);
    // E o item seguinte ao slider cai por baixo dele (o separador e o menuItem
    // seguem o layout).
    assert(afterItemRect.y >= sliderRect.y + sliderRect.height);

    // Carregar na pista do slider (o meio da largura do widget: os extremos são o
    // rótulo e o campo do valor) mexe no valor - é o que prova que o widget recebe o
    // ponteiro dentro do menu.
    const float sliderX = sliderRect.x + sliderRect.width * 0.5f;
    const float sliderY = sliderRect.y + sliderRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(sliderX, sliderY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, sliderX, sliderY));
    harness.frame(build);
    assert(sliderChanged);
    assert(value != 0.5f);
    assert(value > 0.0f && value < 1.0f);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, sliderX, sliderY));
    harness.frame(build);

    // O menu continua aberto (o clique foi no slider, não fora dele).
    assert(open);
    // E o clique não passou para o que está fora do menu: a janela não reagiu.
    assert(separators > 0);
}

// O menu de contexto é o mesmo contentor de layout: um widget lá dentro também
// fica no popup (é o caminho do endMenu, que o endContextMenu usa).
void test_widgets_inside_a_context_menu()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float value = 0.5f;
    ig::Rect area(0.0f, 0.0f, 0.0f, 0.0f);
    ig::Rect itemRect;
    ig::Rect sliderRect;
    bool changed = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("ctx", ig::Rect(0.0f, 0.0f, 320.0f, 240.0f)));
        if (area.width <= 0.0f)
            area = ig::Rect(0.0f, 0.0f, 200.0f, 120.0f);
        if (c.beginContextMenu("alvo", area))
        {
            c.menuItem("Duplicate");
            itemRect = c.itemRect();
            changed = c.sliderFloat("Nivel", value, 0.0f, 1.0f);
            sliderRect = c.itemRect();
            c.endContextMenu();
        }
        c.endWindow();
    };

    harness.frame(build);
    assert(area.width > 0.0f);

    // Abrir com o botão direito sobre a área.
    const float x = 20.0f;
    const float y = 40.0f;
    context.pushEvent(ig::Event::pointerMove(x, y));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Right, x, y));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Right, x, y));
    harness.frame(build);
    harness.frame(build);

    assert(itemRect.width > 0.0f);
    assert(sliderRect.width > 0.0f);
    // O slider está por baixo do item e dentro da largura do menu.
    assert(sliderRect.x == itemRect.x);
    assert(sliderRect.y >= itemRect.y + itemRect.height - 0.5f);

    // E responde: carregar na pista mexe no valor.
    const float sliderX = sliderRect.x + sliderRect.width * 0.5f;
    const float sliderY = sliderRect.y + sliderRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(sliderX, sliderY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, sliderX, sliderY));
    harness.frame(build);
    assert(changed);
    assert(value != 0.5f);
}

// Dois menus com o mesmo rótulo de widget são widgets diferentes (o id do menu
// entra na pilha de ids).
void test_menu_content_scopes_widget_ids()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float fileValue = 0.2f;
    float editValue = 0.8f;
    ig::Rect fileHeader;
    ig::Rect editHeader;
    ig::Rect fileSlider;
    ig::Rect editSlider;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("menu", ig::Rect(0.0f, 0.0f, 400.0f, 320.0f)));
        if (c.beginMenuBar(ig::Rect(0.0f, 0.0f, c.availableWidth(), 26.0f)))
        {
            const bool fileOpen = c.beginMenu("File");
            if (fileHeader.width <= 0.0f)
                fileHeader = c.itemRect();
            if (fileOpen)
            {
                c.sliderFloat("Value", fileValue, 0.0f, 1.0f);
                fileSlider = c.itemRect();
                c.endMenu();
            }
            const bool editOpen = c.beginMenu("Edit");
            if (editHeader.width <= 0.0f)
                editHeader = c.itemRect();
            if (editOpen)
            {
                c.sliderFloat("Value", editValue, 0.0f, 1.0f);
                editSlider = c.itemRect();
                c.endMenu();
            }
            c.endMenuBar();
        }
        c.endWindow();
    };

    harness.frame(build);

    // Abrir o File: arrastar o slider dele tem de mexer no valor do File, não no do
    // Edit (se os ids colidissem, o slider do Edit ficava com o mesmo id e o clique
    // chegava aos dois).
    const float fileX = fileHeader.x + fileHeader.width * 0.5f;
    const float fileY = fileHeader.y + fileHeader.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(fileX, fileY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, fileX, fileY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, fileX, fileY));
    harness.frame(build);
    harness.frame(build);
    assert(fileSlider.width > 0.0f);
    assert(editSlider.width == 0.0f);

    // O meio do widget é a pista (os extremos são o rótulo e o campo do valor).
    const float sliderX = fileSlider.x + fileSlider.width * 0.5f;
    const float sliderY = fileSlider.y + fileSlider.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(sliderX, sliderY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, sliderX, sliderY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, sliderX, sliderY));
    harness.frame(build);
    assert(fileValue != 0.2f);
    assert(editValue == 0.8f);
}

// Os popups de conteúdo livre: abrem por id, o corpo é um contentor de layout (um
// slider lá dentro responde), um clique fora fecha e o isPopupOpen diz o estado.
void test_own_popups()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const ig::StringView names[] = {
        ig::StringView("Bream"), ig::StringView("Haddock"), ig::StringView("Mackerel")
    };
    int chosen = -1;
    int toggles = 0;
    float level = 0.5f;
    ig::Rect buttonRect;
    ig::Rect firstItemRect;
    ig::Rect sliderRect;
    bool open = false;
    bool reportedOpen = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("popups", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.button("Select..", ig::Rect(0.0f, 0.0f, 90.0f, 26.0f)))
        {
            c.openPopup("escolha");
            ++toggles;
        }
        if (buttonRect.width <= 0.0f)
            buttonRect = c.itemRect();
        reportedOpen = c.isPopupOpen("escolha");
        open = c.beginPopup("escolha");
        if (open)
        {
            c.separatorText("Aquario");
            for (int i = 0; i < 3; ++i)
            {
                if (c.selectable(names[i], chosen == i))
                {
                    chosen = i;
                    // Escolher fecha: é o que o demo faz com o selectable do popup.
                    c.closeCurrentPopup();
                }
                if (i == 0)
                    firstItemRect = c.itemRect();
            }
            c.sliderFloat("Nivel", level, 0.0f, 1.0f);
            sliderRect = c.itemRect();
            c.endPopup();
        }
        c.endWindow();
    };

    harness.frame(build);
    assert(buttonRect.width > 0.0f);
    assert(!reportedOpen && !open);
    assert(!context.isPopupOpen("escolha"));

    // Abrir pelo botão: o popup abre no ponteiro...
    const float buttonX = buttonRect.x + buttonRect.width * 0.5f;
    const float buttonY = buttonRect.y + buttonRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(buttonX, buttonY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, buttonX, buttonY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, buttonX, buttonY));
    harness.frame(build);
    // ...e no frame seguinte já está aberto e com o corpo submetido.
    harness.frame(build);
    assert(open);
    assert(reportedOpen);
    assert(context.isPopupOpen("escolha"));

    // O corpo é um contentor de layout: o slider está dentro da caixa do popup e
    // responde ao clique.
    assert(firstItemRect.width > 0.0f);
    assert(sliderRect.width > 0.0f);
    // A largura da caixa é a pedida (a do tema, sem openPopup com tamanho): os itens
    // esticam-se a ela e não a fazem crescer.
    assert(sliderRect.width <= context.theme().menuMinWidth + 0.5f);
    const float sliderX = sliderRect.x + sliderRect.width * 0.5f;
    const float sliderY = sliderRect.y + sliderRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(sliderX, sliderY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, sliderX, sliderY));
    harness.frame(build);
    assert(level != 0.5f);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, sliderX, sliderY));
    harness.frame(build);
    assert(open);

    // Escolher um item fecha-o (o closeCurrentPopup do corpo).
    const float itemX = firstItemRect.x + firstItemRect.width * 0.5f;
    const float itemY = firstItemRect.y + firstItemRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(itemX, itemY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, itemX, itemY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, itemX, itemY));
    harness.frame(build);
    assert(chosen == 0);
    assert(!context.isPopupOpen("escolha"));
    harness.frame(build);
    assert(!open);

    // Abrir outra vez e fechar com um clique fora.
    const int before = toggles;
    context.pushEvent(ig::Event::pointerMove(buttonX, buttonY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, buttonX, buttonY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, buttonX, buttonY));
    harness.frame(build);
    harness.frame(build);
    assert(toggles == before + 1);
    assert(open);

    // Um clique longe do popup fecha-o e não chega ao que está por baixo (o botão
    // não volta a abrir).
    const float farX = buttonRect.x + 20.0f;
    const float farY = buttonRect.y + 200.0f;
    context.pushEvent(ig::Event::pointerMove(farX, farY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, farX, farY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, farX, farY));
    harness.frame(build);
    assert(!context.isPopupOpen("escolha"));
    assert(toggles == before + 1);
}

// Um popup que deixa de ser submetido fecha-se (não há nada no écra para clicar) e
// o contentor de layout volta ao de fora.
void test_unsubmitted_popup_is_dropped()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool show = true;
    ig::Rect buttonRect;
    ig::Rect afterRect;
    float afterYWithPopup = 0.0f;
    float afterYWithout = 0.0f;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("popups", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.button("abrir", ig::Rect(0.0f, 0.0f, 90.0f, 26.0f)))
            c.openPopup("caixa");
        if (buttonRect.width <= 0.0f)
            buttonRect = c.itemRect();
        if (show && c.beginPopup("caixa"))
        {
            c.label("conteudo");
            c.endPopup();
        }
        // Um botão depois do popup: o popup não é do layout da janela, por isso ele
        // fica sempre no mesmo sítio.
        c.button("depois", ig::Rect(0.0f, 40.0f, 90.0f, 26.0f));
        afterRect = c.itemRect();
        if (show)
            afterYWithPopup = afterRect.y;
        else
            afterYWithout = afterRect.y;
        c.endWindow();
    };

    harness.frame(build);
    const float x = buttonRect.x + buttonRect.width * 0.5f;
    const float y = buttonRect.y + buttonRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(x, y));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, y));
    harness.frame(build);
    harness.frame(build);
    assert(context.isPopupOpen("caixa"));
    assert(afterYWithPopup > 0.0f);

    // Deixar de o submeter: um frame de tolerância (é a mesma regra dos menus: quem
    // submete num frame e não no seguinte ainda conta), e ao segundo já não está.
    show = false;
    harness.frame(build);
    harness.frame(build);
    assert(!context.isPopupOpen("caixa"));
    assert(afterYWithout == afterYWithPopup);
}

// Colunas escondidas (o "Hideable" do demo): a coluna escondida não ocupa largura,
// as seguintes fecham o espaço dela e o tableNextColumn devolve false para a
// saltar. Com ele, o tableHoveredColumn (o que um menu de contexto do cabeçalho
// precisa).
void test_hidden_table_columns()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[3] = {1.0f, 1.0f, 1.0f};
    bool visible[3] = {true, false, true};
    ig::Rect headerRects[3];
    ig::Rect cellRects[3];
    bool columnSubmitted[3] = {false, false, false};
    int rows = 0;
    int hoveredColumn = -1;
    int columnsSeen = 0;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 3),
                         ig::Span<const bool>(visible, 3), 300.0f))
        {
            columnsSeen = c.tableVisibleColumnCount();
            for (int i = 0; i < 3; ++i)
            {
                if (!c.tableNextColumn())
                {
                    columnSubmitted[i] = false;
                    continue;
                }
                columnSubmitted[i] = true;
                c.tableHeader(i == 0 ? "A" : (i == 1 ? "B" : "C"));
                headerRects[i] = c.itemRect();
            }
            hoveredColumn = c.tableHoveredColumn();
            ++rows;
            for (int i = 0; i < 3; ++i)
            {
                if (!c.tableNextColumn())
                    continue;
                c.label("celula");
                cellRects[i] = c.itemRect();
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    assert(columnsSeen == 2);
    assert(columnSubmitted[0] && !columnSubmitted[1] && columnSubmitted[2]);
    // A coluna escondida não ocupa nada: as duas visíveis dividem a largura toda e a
    // terceira começa onde a primeira acaba.
    assert(headerRects[0].width > 140.0f && headerRects[0].width < 160.0f);
    assert(headerRects[2].x == headerRects[0].x + headerRects[0].width);
    assert(headerRects[2].width == headerRects[0].width);
    // As células caem debaixo do cabeçalho respetivo.
    assert(cellRects[0].x == headerRects[0].x);
    assert(cellRects[2].x == headerRects[2].x);

    // O ponteiro sobre o cabeçalho da segunda coluna visível: o tableHoveredColumn
    // diz qual é (2, que é o índice real da coluna).
    context.pushEvent(ig::Event::pointerMove(headerRects[2].x + 10.0f,
                                            headerRects[2].y + headerRects[2].height * 0.5f));
    harness.frame(build);
    assert(hoveredColumn == 2);

    // Longe do cabeçalho: nenhuma.
    context.pushEvent(ig::Event::pointerMove(headerRects[0].x + 10.0f, 280.0f));
    harness.frame(build);
    assert(hoveredColumn == -1);

    // Mostrar a do meio: as três dividem a largura.
    visible[1] = true;
    harness.frame(build);
    assert(columnSubmitted[1]);
    assert(headerRects[0].width > 90.0f && headerRects[0].width < 110.0f);
    assert(headerRects[1].x == headerRects[0].x + headerRects[0].width);

    // Esconder todas: a tabela não se abre (não há nada para mostrar), por isso o
    // corpo não corre e o que ficou é do frame anterior.
    visible[0] = visible[1] = visible[2] = false;
    columnsSeen = -1;
    harness.frame(build);
    assert(columnsSeen == -1);
}

// Colunas reordenáveis (o "Reorderable" do demo): arrastar um cabeçalho para outro
// sítio troca a coluna do modelo de posição, a ordem fica guardada entre frames e o
// tableColumnIndex() diz ao chamador que coluna desenhar em cada sítio.
void test_reorderable_table_columns()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    // Pesos diferentes, para se ver a largura a seguir a coluna do modelo e não a
    // posição no écra.
    const float weights[3] = {1.0f, 2.0f, 3.0f};
    ig::TableStyle style;
    style.reorderable = true;
    ig::Rect headerRects[3];
    int seenOrder[3] = {-1, -1, -1};

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 3), style, 300.0f))
        {
            for (int i = 0; i < 3; ++i)
            {
                if (!c.tableNextColumn())
                    continue;
                seenOrder[i] = c.tableColumnIndex();
                c.tableHeader(seenOrder[i] == 0 ? "A" : (seenOrder[i] == 1 ? "B" : "C"));
                headerRects[i] = c.itemRect();
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    // Sem reordenar, a posição e a coluna do modelo são a mesma.
    assert(seenOrder[0] == 0 && seenOrder[1] == 1 && seenOrder[2] == 2);

    // Arrastar o primeiro cabeçalho para cima do terceiro: a coluna A passa para a
    // terceira posição.
    const float fromX = headerRects[0].x + headerRects[0].width * 0.5f;
    const float fromY = headerRects[0].y + headerRects[0].height * 0.5f;
    const float toX = headerRects[2].x + headerRects[2].width * 0.5f;
    context.pushEvent(ig::Event::pointerMove(fromX, fromY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, fromX, fromY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerMove(toX, fromY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, toX, fromY));
    harness.frame(build);
    harness.frame(build);

    assert(seenOrder[0] == 1 && seenOrder[1] == 2 && seenOrder[2] == 0);

    // A largura é a da coluna do modelo (a de peso 2 ficou onde estava a de peso 1),
    // não a do sítio onde calhou.
    assert(headerRects[1].width > headerRects[0].width);
    assert(headerRects[0].width > headerRects[2].width);

    // E a ordem sobrevive ao frame (é guardada pelo id da tabela).
    harness.frame(build);
    assert(seenOrder[0] == 1 && seenOrder[1] == 2 && seenOrder[2] == 0);

    // Uma tabela que não é reordenável mantém a identidade mesmo ao arrastar um
    // cabeçalho, e isso não mexe na ordem da outra.
    ig::TableStyle plain;
    int plainOrder[3] = {-1, -1, -1};
    auto buildPlain = [&](ig::Context &c) {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 3), plain, 300.0f))
        {
            for (int i = 0; i < 3; ++i)
            {
                if (!c.tableNextColumn())
                    continue;
                plainOrder[i] = c.tableColumnIndex();
                c.tableHeader("x");
                headerRects[i] = c.itemRect();
            }
            c.endTable();
        }
        c.endWindow();
    };
    harness.frame(buildPlain);
    const float plainFromX = headerRects[0].x + headerRects[0].width * 0.5f;
    const float plainY = headerRects[0].y + headerRects[0].height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(plainFromX, plainY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, plainFromX, plainY));
    harness.frame(buildPlain);
    context.pushEvent(ig::Event::pointerMove(headerRects[2].x + 5.0f, plainY));
    harness.frame(buildPlain);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, headerRects[2].x + 5.0f, plainY));
    harness.frame(buildPlain);
    assert(plainOrder[0] == 0 && plainOrder[1] == 1 && plainOrder[2] == 2);

    // Arrastar não pode ter disparado um clique de ordenação na tabela reordenável:
    // a ordem dela continua a ser a que ficou largada.
    harness.frame(build);
    assert(seenOrder[0] == 1 && seenOrder[1] == 2 && seenOrder[2] == 0);
}

// Com a ordem trocada, o que o chamador vê continua a ser a coluna do modelo: o
// tableColumnIndex (o que desenhar), o tableHoveredColumn (o menu de contexto), o
// sortColumn do tableHeader e o peso que o grip do cabeçalho mexe.
void test_reordered_table_columns_are_model_columns()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float weights[3] = {1.0f, 1.0f, 1.0f};
    ig::TableStyle style;
    style.reorderable = true;
    const ig::StringView names[3] = {ig::StringView("A"), ig::StringView("B"), ig::StringView("C")};
    ig::Rect headerRects[3];
    int seenOrder[3] = {-1, -1, -1};
    int hoveredColumn = -1;
    int sortColumn = -1;
    bool sortAscending = true;

    auto submit = [&](ig::Context &c) {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.beginTable("t", ig::Span<float>(weights, 3), style, 300.0f))
        {
            for (int i = 0; i < 3; ++i)
            {
                if (!c.tableNextColumn())
                    continue;
                seenOrder[i] = c.tableColumnIndex();
                c.tableHeader(names[seenOrder[i]], sortColumn, sortAscending);
                headerRects[i] = c.itemRect();
            }
            hoveredColumn = c.tableHoveredColumn();
            c.endTable();
        }
        c.endWindow();
    };
    harness.frame(submit);
    assert(seenOrder[0] == 0 && seenOrder[1] == 1 && seenOrder[2] == 2);

    // Arrastar a coluna B (posição 1) para a frente: a ordem passa a B, A, C.
    const float fromX = headerRects[1].x + headerRects[1].width * 0.5f;
    const float y = headerRects[1].y + headerRects[1].height * 0.5f;
    const float toX = headerRects[0].x + 5.0f;
    context.pushEvent(ig::Event::pointerMove(fromX, y));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, fromX, y));
    harness.frame(submit);
    context.pushEvent(ig::Event::pointerMove(toX, y));
    harness.frame(submit);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, toX, y));
    harness.frame(submit);
    harness.frame(submit);
    assert(seenOrder[0] == 1 && seenOrder[1] == 0 && seenOrder[2] == 2);

    // O ponteiro sobre a posição 2 (a coluna C) diz que é a coluna 2 do modelo.
    context.pushEvent(ig::Event::pointerMove(headerRects[2].x + headerRects[2].width * 0.5f,
                                             headerRects[2].y + headerRects[2].height * 0.5f));
    harness.frame(submit);
    assert(hoveredColumn == 2);
    // E sobre a posição 0 (que agora é a coluna B) diz 1.
    context.pushEvent(ig::Event::pointerMove(headerRects[0].x + headerRects[0].width * 0.5f,
                                             headerRects[0].y + headerRects[0].height * 0.5f));
    harness.frame(submit);
    assert(hoveredColumn == 1);

    // Um clique no cabeçalho da posição 0 ordena a coluna 1 (a B), não a 0: um
    // clique sem andar é um clique, não um arrasto.
    const float clickX = headerRects[0].x + headerRects[0].width * 0.5f;
    const float clickY = headerRects[0].y + headerRects[0].height * 0.5f;
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, clickX, clickY));
    harness.frame(submit);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, clickX, clickY));
    harness.frame(submit);
    assert(sortColumn == 1);
    assert(sortAscending);
    assert(seenOrder[0] == 1 && seenOrder[1] == 0 && seenOrder[2] == 2);
    // Um segundo clique na mesma posição inverte a ordenação da mesma coluna.
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, clickX, clickY));
    harness.frame(submit);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, clickX, clickY));
    harness.frame(submit);
    assert(sortColumn == 1 && !sortAscending);

    // O grip do cabeçalho da posição 0 mexe no peso das duas colunas que ali estão
    // (a 1 e, à direita, a 0), não no peso das posições 0 e 1.
    const float before[3] = {weights[0], weights[1], weights[2]};
    const float gripX = headerRects[0].x + headerRects[0].width - 1.0f;
    context.pushEvent(ig::Event::pointerMove(gripX, clickY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, gripX, clickY));
    harness.frame(submit);
    context.pushEvent(ig::Event::pointerMove(gripX + 30.0f, clickY));
    harness.frame(submit);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, gripX + 30.0f, clickY));
    harness.frame(submit);
    harness.frame(submit);
    assert(weights[1] > before[1]);
    assert(weights[0] < before[0]);
    assert(weights[2] == before[2]);
    // A ordem não mudou (o arrasto do grip não reordena nada).
    assert(seenOrder[0] == 1 && seenOrder[1] == 0 && seenOrder[2] == 2);
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
    test_item_width_stack();
    test_new_line_wraps_to_the_margin();
    test_group_is_one_item_outside();
    test_group_nesting_and_imbalance();
    test_group_columns_line_up();
    test_open_menu_owns_the_pointer_over_it();
    test_menu_bar_reserves_its_strip();
    test_hover_follows_the_window_that_is_raised();
    test_menu_item_draws_its_shortcut();
    test_open_combo_owns_the_pointer_over_its_list();
    test_begin_combo_takes_free_content();
    test_begin_combo_popup_owns_the_pointer_and_grows();
    test_main_menu_bar();
    test_multi_select();
    test_item_activation_queries();
    test_item_deactivated_after_edit();
    test_item_toggled_open();
    test_combo_popup_scrolls();
    test_combo_default_focus_opens_on_the_selection();
    test_child_inside_a_combo_popup();
    test_item_hovered_flags();
    test_widgets_inside_an_open_menu();
    test_menu_content_scopes_widget_ids();
    test_widgets_inside_a_context_menu();
    test_own_popups();
    test_unsubmitted_popup_is_dropped();
    test_hidden_table_columns();
    test_reorderable_table_columns();
    test_reordered_table_columns_are_model_columns();
    printf("test_item_status: all tests passed\n");
    return 0;
}
