// Tests for the LastItemData-style queries (isItemHovered, isItemActive,
// isItemClicked, isItemEdited, isItemFocused, isItemDisabled, itemRect,
// itemId). They read state that the widget sets while it is submitted, so the
// only way to check them is to drive the context and read the answers in the
// same frame, right after the widget - which is what these tests do.

#include "InteractionHarness.hpp"

#include <igui/FileDialog.hpp>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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

// Duas faces de mentira, para o pushFont ter o que escolher: o que o Context
// lhe pede é o tamanho de fabrico da face e as medidas do texto nela.
class TwoFaceProvider : public ig::TextProvider
{
public:
    float bakedSize(ig::FontId font) const override
    {
        if (font.value == 1u)
            return 13.0f;
        if (font.value == 2u)
            return 26.0f;
        return 0.0f;
    }

    ig::TextMetrics measureText(ig::FontId font, ig::StringView text,
                                float logicalSize) const override
    {
        ig::TextMetrics metrics;
        // Sem face não há glifos, como no FontAtlas: o texto mede zero.
        if (font.value != 1u && font.value != 2u)
            return metrics;
        const float factor = font.value == 2u ? 2.0f : 1.0f;
        metrics.width = static_cast<float>(text.size()) * logicalSize * 0.5f * factor;
        metrics.height = logicalSize;
        metrics.ascent = logicalSize * 0.75f;
        metrics.descent = logicalSize * 0.25f;
        return metrics;
    }

    bool appendText(ig::DrawList &, ig::FontId, ig::StringView, const ig::Vec2 &, float,
                    const ig::Color &, const ig::Rect &) const override
    {
        return true;
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

// Uma tabela que rola sozinha (TableStyle::scrollY): tem uma altura fixa, as
// linhas rolam dentro dela com a roda e com a barra, o que sai da área deixa de
// ser desenhado (e de responder) e as primeiras freezeRows linhas ficam presas
// no topo - a linha 0, o cabeçalho, é uma delas.
void test_table_scrolls_its_own_rows()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[2] = {1.0f, 1.0f};
    ig::TableStyle style;
    style.borders = true;
    style.scrollY = true;
    style.height = 160.0f;
    style.freezeRows = 1;
    const int kRows = 6;
    ig::Rect headerRects[2];
    ig::Rect labelRects[kRows];
    bool labelVisible[kRows];
    float labelX[2] = {0.0f, 0.0f};
    float labelY[kRows];

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 2), style, 200.0f))
        {
            if (c.tableNextColumn())
            {
                c.tableHeader("Nome");
                headerRects[0] = c.itemRect();
            }
            if (c.tableNextColumn())
            {
                c.tableHeader("Valor");
                headerRects[1] = c.itemRect();
            }
            for (int row = 0; row < kRows; ++row)
            {
                for (int column = 0; column < 2; ++column)
                {
                    if (!c.tableNextColumn())
                        continue;
                    c.label("celula");
                    labelRects[row] = c.itemRect();
                    if (column == 0)
                        labelY[row] = labelRects[row].y;
                    labelX[column] = labelRects[row].x;
                    labelVisible[row] = c.isItemVisible();
                }
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    // Sem rolagem, as linhas ficam umas debaixo das outras, a começar no
    // cabeçalho.
    const float rowStep = labelY[1] - labelY[0];
    assert(rowStep > 0.0f);
    assert(labelY[0] == headerRects[0].y + headerRects[0].height + 6.0f);
    assert(labelY[2] == labelY[0] + rowStep * 2.0f);
    // As colunas são as duas metades da tabela (menos a barra, que ainda não se
    // sabe se existe no primeiro frame).
    assert(labelX[1] > labelX[0]);

    // A roda por cima da tabela rola as linhas: o cabeçalho não anda, o resto
    // sobe, e o que sai por cima deixa de ser visível.
    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    const float wheelX = 100.0f;
    const float wheelY = labelY[2] + labelRects[2].height * 0.5f;
    const float headerBefore = headerRects[0].y;
    const float firstRowBefore = labelY[0];
    context.pushEvent(ig::Event::pointerMove(wheelX, wheelY));
    context.pushEvent(wheel);
    harness.frame(build);
    // O cabeçalho não anda e as linhas sobem (a roda anda o que mede uma widget).
    assert(headerRects[0].y == headerBefore);
    assert(firstRowBefore - labelY[0] == 28.0f);

    // Rolar até ao fim para: a última linha fica dentro da área que rola e o fundo
    // da tabela é o limite (a altura é a que o style pediu, não a das linhas).
    for (int i = 0; i < 30; ++i)
    {
        context.pushEvent(ig::Event::pointerMove(wheelX, wheelY));
        context.pushEvent(wheel);
        harness.frame(build);
    }
    assert(labelVisible[kRows - 1]);
    assert(labelRects[kRows - 1].y + labelRects[kRows - 1].height <
           headerRects[0].y + style.height + 1.0f);
    assert(!labelVisible[0]);
    // A linha 1 ainda se vê em parte: está cortada por cima pela faixa presa.
    assert(labelY[1] < headerRects[0].y + headerRects[0].height + 6.0f);
    // O cabeçalho continua no sítio, e a última linha fica-lhe abaixo.
    assert(headerRects[0].y == headerBefore);
    assert(labelRects[kRows - 1].y > headerRects[0].y + headerRects[0].height);

    // Rolar para cima volta ao princípio, e o cabeçalho nunca se mexe.
    for (int i = 0; i < 30; ++i)
    {
        wheel.wheelY = 1.0f;
        context.pushEvent(ig::Event::pointerMove(wheelX, wheelY));
        context.pushEvent(wheel);
        harness.frame(build);
    }
    assert(labelVisible[0]);
    assert(labelY[0] == headerRects[0].y + headerRects[0].height + 6.0f);
    assert(labelY[kRows - 1] > headerRects[0].y + style.height);
    assert(headerRects[0].y == headerBefore);

    // A barra de rolagem está no lado direito da tabela, com a altura da área que
    // rola (os vértices com a cor do punho dizem onde ela foi desenhada).
    const ig::DrawData &scrolled = harness.frame(build);
    const ig::Color thumbColor = context.theme().scrollbarThumb;
    bool foundThumb = false;
    float thumbLeft = 0.0f;
    float thumbRight = 0.0f;
    float thumbBottom = 0.0f;
    for (ig::Span<const ig::DrawVertex>::size_type i = 0u; i < scrolled.vertices.size(); ++i)
    {
        const ig::DrawVertex &vertex = scrolled.vertices[i];
        if (vertex.color != thumbColor)
            continue;
        if (!foundThumb)
        {
            thumbLeft = thumbRight = vertex.position.x;
            thumbBottom = vertex.position.y;
            foundThumb = true;
        }
        if (vertex.position.x < thumbLeft)
            thumbLeft = vertex.position.x;
        if (vertex.position.x > thumbRight)
            thumbRight = vertex.position.x;
        if (vertex.position.y > thumbBottom)
            thumbBottom = vertex.position.y;
    }
    assert(foundThumb);
    assert(thumbLeft >= headerRects[1].x + headerRects[1].width);
    assert(thumbBottom <= headerRects[0].y + style.height + 1.0f);
    assert(thumbBottom > headerRects[0].y);
}

// Uma tabela com duas linhas presas: a segunda linha também não anda com a
// rolagem (as presas são as primeiras freezeRows, e a 0 é o cabeçalho).
void test_table_frozen_rows_stay_put()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[2] = {1.0f, 1.0f};
    ig::TableStyle style;
    style.scrollY = true;
    style.height = 160.0f;
    style.freezeRows = 2;
    const int kRows = 5;
    float rowY[kRows];
    float headerY = 0.0f;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 2), style, 200.0f))
        {
            for (int row = -1; row < kRows; ++row)
            {
                for (int column = 0; column < 2; ++column)
                {
                    if (!c.tableNextColumn())
                        continue;
                    c.label(row < 0 ? "cabecalho" : "linha");
                    if (row < 0)
                    {
                        if (column == 0)
                            headerY = c.itemRect().y;
                    }
                    else if (column == 0)
                        rowY[row] = c.itemRect().y;
                }
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    const float step = rowY[1] - rowY[0];
    assert(step > 0.0f);
    assert(rowY[0] == headerY + step);

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    context.pushEvent(ig::Event::pointerMove(100.0f, rowY[3]));
    context.pushEvent(wheel);
    harness.frame(build);
    // O cabeçalho e a linha 0 ficam onde estavam; a linha 1 já subiu.
    assert(headerY == rowY[0] - step);
    assert(rowY[0] == headerY + step);
    assert(rowY[1] < rowY[0] + step);
}

// O TreeStyle do treeNode e do collapsingHeader: o bullet (um ponto em vez da
// seta, que é o ImGuiTreeNodeFlags_Bullet do demo), a linha desenhada como
// escolhida, só a seta a abrir e a linha a apanhar o rato em toda a largura que tem.
void test_tree_style()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool expanded = false;
    bool toggled = false;
    ig::Rect bulletRect;
    ig::Rect plainRect;
    ig::Rect wideRect;
    ig::Rect arrowOnlyRect;
    ig::Rect selectedRect;
    ig::Rect bulletHeaderRect;
    bool arrowToggled = false;

    ig::TreeStyle bullet;
    bullet.bullet = true;
    ig::TreeStyle selected;
    selected.selected = true;
    ig::TreeStyle wide;
    wide.spanAvailWidth = true;
    ig::TreeStyle arrowOnly;
    arrowOnly.openOnArrow = true;

    // Estes rects são no espaço do conteúdo da janela (o mesmo do layoutCursor);
    // aqui são submetidos pelas coordenadas, um por linha, para o itemRect de cada
    // um ser o que a widget desenhou.
    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("arvore", ig::Rect(0.0f, 0.0f, 300.0f, 300.0f)));
        c.treeNode("bullet", expanded, bullet, ig::Rect(0.0f, 0.0f, 120.0f, 20.0f));
        bulletRect = c.itemRect();
        toggled = toggled || c.isItemToggledOpen();
        c.treeNode("simples", expanded, ig::Rect(0.0f, 30.0f, 120.0f, 20.0f));
        plainRect = c.itemRect();
        c.treeNode("larga", expanded, wide, ig::Rect(0.0f, 60.0f, 120.0f, 20.0f));
        wideRect = c.itemRect();
        c.collapsingHeader("seta", expanded, arrowOnly, ig::Rect(0.0f, 90.0f, 120.0f, 20.0f));
        arrowOnlyRect = c.itemRect();
        arrowToggled = c.isItemToggledOpen();
        c.collapsingHeader("escolhida", expanded, selected, ig::Rect(0.0f, 120.0f, 120.0f, 20.0f));
        selectedRect = c.itemRect();
        c.collapsingHeader("cabecalho com bullet", expanded, bullet, ig::Rect(0.0f, 150.0f, 120.0f, 20.0f));
        bulletHeaderRect = c.itemRect();
        c.endWindow();
    };

    const ig::DrawData &frame = harness.frame(build);
    assert(bulletRect.width == 120.0f);
    assert(bulletHeaderRect.width == 120.0f);
    // O spanAvailWidth faz o item apanhar toda a largura que sobra, não só a que lhe
    // foi dada (é o que faz uma linha de árvore responder ao rato em todo o lado).
    assert(wideRect.width > 120.0f);
    // A janela vai de 0 a 300 e a linha começa na margem de dentro dela.
    assert(wideRect.width == 300.0f - context.theme().windowPadding - wideRect.x);

    // A linha escolhida é desenhada com a cor do escolhido e as outras não.
    int selectedVertices = 0;
    int plainVertices = 0;
    for (ig::Span<const ig::DrawVertex>::size_type i = 0u; i < frame.vertices.size(); ++i)
    {
        const ig::DrawVertex &vertex = frame.vertices[i];
        if (vertex.color != context.theme().selectableSelected)
            continue;
        if (vertex.position.x >= selectedRect.x && vertex.position.x <= selectedRect.x + selectedRect.width &&
            vertex.position.y >= selectedRect.y && vertex.position.y <= selectedRect.y + selectedRect.height)
            ++selectedVertices;
        if (vertex.position.x >= plainRect.x && vertex.position.x <= plainRect.x + plainRect.width &&
            vertex.position.y >= plainRect.y && vertex.position.y <= plainRect.y + plainRect.height)
            ++plainVertices;
    }
    assert(selectedVertices > 0);
    assert(plainVertices == 0);

    // O bullet abre e fecha a linha com o clique (a marca é outra, o comportamento
    // é o do treeNode).
    const float cx = bulletRect.x + bulletRect.width * 0.5f;
    const float cy = bulletRect.y + bulletRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(cx, cy));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, cx, cy));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, cx, cy));
    harness.frame(build);
    assert(toggled);
    assert(expanded);

    // Com o openOnArrow, carregar no rótulo não abre a linha: só a seta abre.
    expanded = false;
    toggled = false;
    const float labelX = arrowOnlyRect.x + arrowOnlyRect.width * 0.7f;
    const float labelY = arrowOnlyRect.y + arrowOnlyRect.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(labelX, labelY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, labelX, labelY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, labelX, labelY));
    harness.frame(build);
    assert(!expanded);
    assert(!arrowToggled);

    // E na seta abre.
    const float arrowX = arrowOnlyRect.x + 6.0f;
    context.pushEvent(ig::Event::pointerMove(arrowX, labelY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, arrowX, labelY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, arrowX, labelY));
    harness.frame(build);
    assert(expanded);
    assert(arrowToggled);
}

// O modal de popup (o BeginPopupModal do demo): abre-se como qualquer popup, mas
// sai centrado, com barra de título própria e por cima de tudo - e um clique fora
// não o fecha (só o Escape, o X ou o closeCurrentPopup). É isso que o distingue de
// um popup comum.
void test_popup_modal()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool openModal = false;
    bool showModal = false;
    bool backgroundClicked = false;
    ig::Rect backgroundButton;
    ig::Rect modalLabel;
    ig::Rect modalOk;
    int okClicks = 0;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("modal", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.button("fundo", ig::Rect(0.0f, 0.0f, 90.0f, 26.0f)))
        {
            backgroundClicked = true;
            openModal = true;
            showModal = true;
            c.openPopup("Definicoes");
        }
        backgroundButton = c.itemRect();
        if (showModal && c.beginPopupModal("Definicoes", &openModal))
        {
            c.label("Modal com titulo");
            modalLabel = c.itemRect();
            if (c.button("OK"))
            {
                ++okClicks;
                openModal = false;
                c.closeCurrentPopup();
            }
            modalOk = c.itemRect();
            c.endPopup();
        }
        if (!openModal)
            showModal = false;
        c.endWindow();
    };

    harness.frame(build);
    assert(!context.isPopupOpen("Definicoes"));
    // Um popup que não está aberto some com a chamada (e o conteúdo não é medido).
    assert(modalLabel.width == 0.0f);

    // Abrir pelo botão do fundo.
    const float bx = backgroundButton.x + backgroundButton.width * 0.5f;
    const float by = backgroundButton.y + backgroundButton.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(bx, by));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, bx, by));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, bx, by));
    harness.frame(build);
    assert(backgroundClicked);
    assert(context.isPopupOpen("Definicoes"));
    // O modal aparece no frame seguinte (a caixa é a medida do frame anterior).
    const ig::DrawData &modalFrame = harness.frame(build);
    assert(modalLabel.width > 0.0f);
    assert(modalOk.width > 0.0f);
    // A caixa é o que está pintado com a cor do fundo do menu (é a única coisa no
    // ecrã com ela): dai saem os limites para provar que está centrada.
    ig::Rect box(0.0f, 0.0f, 0.0f, 0.0f);
    bool boxFound = false;
    const ig::Color boxColor = context.theme().menuBg;
    for (ig::Span<const ig::DrawVertex>::size_type i = 0u; i < modalFrame.vertices.size(); ++i)
    {
        const ig::DrawVertex &vertex = modalFrame.vertices[i];
        if (vertex.color != boxColor)
            continue;
        if (!boxFound)
        {
            box = ig::Rect(vertex.position.x, vertex.position.y, 0.0f, 0.0f);
            boxFound = true;
        }
        if (vertex.position.x < box.x)
            box.x = vertex.position.x;
        if (vertex.position.y < box.y)
            box.y = vertex.position.y;
        if (vertex.position.x > box.x + box.width)
            box.width = vertex.position.x - box.x;
        if (vertex.position.y > box.y + box.height)
            box.height = vertex.position.y - box.y;
    }
    assert(boxFound);
    const ig::Vec2 display = context.displaySize();
    const float boxCentreX = box.x + box.width * 0.5f;
    const float boxCentreY = box.y + box.height * 0.5f;
    assert(fabsf(boxCentreX - display.x * 0.5f) <= 1.0f);
    assert(fabsf(boxCentreY - display.y * 0.5f) <= 1.0f);
    // O conteúdo está dentro da caixa e por baixo da barra do título.
    assert(modalLabel.x >= box.x);
    assert(modalLabel.y >= box.y + context.theme().titleBarHeight);
    assert(modalOk.y + modalOk.height <= box.y + box.height + 1.0f);

    // Um clique fora não fecha o modal (é a diferença para um popup comum): o
    // clique é engolido, o botão de trás não conta e o modal continua aberto.
    const int clicksBefore = okClicks;
    backgroundClicked = false;
    context.pushEvent(ig::Event::pointerMove(380.0f, 290.0f));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 380.0f, 290.0f));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 380.0f, 290.0f));
    harness.frame(build);
    assert(context.isPopupOpen("Definicoes"));
    assert(!backgroundClicked);
    assert(okClicks == clicksBefore);
    assert(openModal);

    // O Escape fecha-o (e o conteúdo deixa de ser submetido).
    modalLabel = ig::Rect();
    ig::Event escape;
    escape.type = ig::EventType::KeyDown;
    escape.key = ig::KeyCode::Escape;
    context.pushEvent(escape);
    harness.frame(build);
    assert(!context.isPopupOpen("Definicoes"));
    assert(modalLabel.width == 0.0f);

    // Reabrir e fechar pelo X (o p_open) e pelo botão do modal.
    openModal = true;
    showModal = true;
    context.pushEvent(ig::Event::pointerMove(bx, by));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, bx, by));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, bx, by));
    harness.frame(build);
    harness.frame(build);
    assert(context.isPopupOpen("Definicoes"));
    const ig::Rect okRect = modalOk;
    context.pushEvent(ig::Event::pointerMove(okRect.x + okRect.width * 0.5f,
                                             okRect.y + okRect.height * 0.5f));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, okRect.x + okRect.width * 0.5f,
                                             okRect.y + okRect.height * 0.5f));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, okRect.x + okRect.width * 0.5f,
                                           okRect.y + okRect.height * 0.5f));
    harness.frame(build);
    assert(okClicks == clicksBefore + 1);
    assert(!context.isPopupOpen("Definicoes"));
    assert(!openModal);

    openModal = true;
    showModal = true;
    context.pushEvent(ig::Event::pointerMove(bx, by));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, bx, by));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, bx, by));
    harness.frame(build);
    harness.frame(build);
    assert(context.isPopupOpen("Definicoes"));
    const float titleHeight = context.theme().titleBarHeight;
    const float closeSize = titleHeight - context.theme().padding * 0.5f;
    const float closeX = box.x + box.width - context.theme().padding * 0.25f - closeSize * 0.5f;
    const float closeY = box.y + titleHeight * 0.5f;
    context.pushEvent(ig::Event::pointerMove(closeX, closeY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, closeX, closeY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, closeX, closeY));
    harness.frame(build);
    assert(!context.isPopupOpen("Definicoes"));
    assert(!openModal);
}

// Os flags do openPopup: o NoOpenOverExistingPopup não abre por cima do que já lá
// está, o AtCursor abre encostado ao item que lá está e o isPopupOpen com o
// AnyPopupId pergunta se há algum aberto.
void test_popup_open_flags()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect anchor;
    ig::Rect firstLabel;
    bool tryFirst = false;
    bool trySecond = false;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("flags", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        c.button("ancora");
        anchor = c.itemRect();
        if (tryFirst)
            c.openPopup("um", ig::Context::PopupFlags::AtCursor);
        if (trySecond)
            c.openPopup("dois", ig::Context::PopupFlags::NoOpenOverExistingPopup);
        if (c.beginPopup("um"))
        {
            c.label("um");
            firstLabel = c.itemRect();
            c.endPopup();
        }
        if (c.beginPopup("dois"))
        {
            c.label("dois");
            c.endPopup();
        }
        c.endWindow();
    };

    harness.frame(build);
    assert(!context.isPopupOpen("um"));
    assert(!context.isPopupOpen("qualquer", ig::Context::PopupFlags::AnyPopupId));

    // O AtCursor encosta a caixa ao cursor do layout (na margem de dentro da janela,
    // por baixo do botão), não ao ponteiro - que está longe, no (2,2).
    context.pushEvent(ig::Event::pointerMove(2.0f, 2.0f));
    tryFirst = true;
    harness.frame(build);
    harness.frame(build);
    assert(context.isPopupOpen("um"));
    assert(context.isPopupOpen("qualquer", ig::Context::PopupFlags::AnyPopupId));
    assert(firstLabel.width > 0.0f);
    // A caixa fica na margem de dentro da janela e por baixo do botão (o cursor do
    // layout é onde ele acabou), não no ponteiro.
    assert(firstLabel.x < anchor.x + 30.0f);
    assert(firstLabel.y > anchor.y + anchor.height);

    // Com um popup aberto, o NoOpenOverExistingPopup não abre o segundo.
    trySecond = true;
    harness.frame(build);
    harness.frame(build);
    assert(context.isPopupOpen("um"));
    assert(!context.isPopupOpen("dois"));

    // Fechado o primeiro (um clique fora), a mesma chamada abre o segundo.
    tryFirst = false;
    context.pushEvent(ig::Event::pointerMove(300.0f, 250.0f));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 300.0f, 250.0f));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 300.0f, 250.0f));
    harness.frame(build);
    assert(!context.isPopupOpen("um"));
    assert(context.isPopupOpen("dois"));
    assert(context.isPopupOpen("qualquer", ig::Context::PopupFlags::AnyPopupId));

    // E com nada aberto o AnyPopupId é falso. O popup abriu no ponteiro (o
    // NoOpenOverExistingPopup não muda onde ele abre), por isso o clique que o
    // fecha tem de ser longe dele.
    trySecond = false;
    context.pushEvent(ig::Event::pointerMove(600.0f, 600.0f));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 600.0f, 600.0f));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 600.0f, 600.0f));
    harness.frame(build);
    assert(!context.isPopupOpen("dois"));
    assert(!context.isPopupOpen("qualquer", ig::Context::PopupFlags::AnyPopupId));
}

// As tabs a sério (beginTabBar/beginTabItem): a barra é um contentor e só a tab
// escolhida abre o conteúdo, que vai da linha das tabs até ao endTabItem. A escolha
// é da barra (pelo id) e sobrevive ao frame; a ordem das tabs é da barra e o
// arrasto troca-a.
void test_tabs_with_content()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool closeSecond = true;
    ig::Rect tabRects[3] = {ig::Rect(), ig::Rect(), ig::Rect()};
    ig::Rect contentRects[3] = {ig::Rect(), ig::Rect(), ig::Rect()};
    int submitted = 0;
    ig::Rect afterRect;
    bool afterSeen = false;
    unsigned barFlags = ig::Context::TabBarFlags::None;
    unsigned itemFlags = ig::Context::TabItemFlags::None;
    unsigned thirdFlags = ig::Context::TabItemFlags::None;
    bool thirdOpen = true;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("tabs", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        submitted = 0;
        if (c.beginTabBar("bar", barFlags))
        {
            // O rect da tab lê-se logo a seguir ao beginTabItem (o conteúdo, se ele
            // abrir, passa a ser o último item).
            const bool one = c.beginTabItem("um");
            tabRects[0] = c.itemRect();
            if (one)
            {
                ++submitted;
                c.dummy(40.0f, 20.0f);
                contentRects[0] = c.itemRect();
                c.endTabItem();
            }
            const bool two = c.beginTabItem("dois", closeSecond ? &closeSecond : nullptr, itemFlags);
            tabRects[1] = c.itemRect();
            if (two)
            {
                ++submitted;
                c.dummy(40.0f, 60.0f);
                contentRects[1] = c.itemRect();
                c.endTabItem();
            }
            const bool three = thirdOpen && c.beginTabItem("tres", &thirdOpen, thirdFlags);
            tabRects[2] = c.itemRect();
            if (three)
            {
                ++submitted;
                c.dummy(40.0f, 10.0f);
                contentRects[2] = c.itemRect();
                c.endTabItem();
            }
            c.endTabBar();
        }
        c.label("depois");
        afterRect = c.itemRect();
        afterSeen = true;
        c.endWindow();
    };

    // O primeiro frame: a primeira tab fica escolhida sozinha (é a regra das tabs:
    // a primeira de uma barra nova), e o conteúdo dela é o único submetido.
    harness.frame(build);
    assert(submitted == 1);
    assert(contentRects[0].width > 0.0f);
    assert(contentRects[1].width == 0.0f);
    // A tab escolhida fica no topo da barra e o conteúdo dela por baixo.
    assert(tabRects[0].x == tabRects[1].x - tabRects[0].width);
    assert(contentRects[0].y >= tabRects[0].y + tabRects[0].height - 1.0f);
    // O que vem depois da barra começa por baixo do conteúdo dela.
    assert(afterRect.y > contentRects[0].y + 20.0f);

    // Um clique na segunda tab escolhe-a neste mesmo frame, e o conteúdo passa a ser
    // o dela (mais alto, por isso o que vem depois desce).
    const float beforeY = afterRect.y;
    const float secondX = tabRects[1].x + tabRects[1].width * 0.5f;
    const float tabY = tabRects[1].y + tabRects[1].height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(secondX, tabY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, secondX, tabY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, secondX, tabY));
    harness.frame(build);
    // O conteúdo da tab escolhida agora é do frame seguinte (o realce é já deste):
    // é a mesma regra do ImGui.
    harness.frame(build);
    assert(submitted == 1);
    assert(contentRects[1].width > 0.0f);
    assert(afterRect.y > beforeY + 20.0f);

    // O X da tab fecha-a: o open do chamador fica a false e a tab desaparece (o
    // chamador deixa de a submeter).
    const float closeX = tabRects[1].x + tabRects[1].width - 10.0f;
    context.pushEvent(ig::Event::pointerMove(closeX, tabY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, closeX, tabY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, closeX, tabY));
    harness.frame(build);
    assert(!closeSecond);
    // A tab "dois" já não é submetida (o chamador deixa de a submeter com o open a
    // false) e a barra escolhe a primeira outra vez.
    harness.frame(build);
    harness.frame(build);
    assert(submitted == 1);
    assert(contentRects[0].width > 0.0f);

    // O SetSelected escolhe a terceira tab (o conteúdo dela é do frame seguinte).
    thirdFlags = ig::Context::TabItemFlags::SetSelected;
    harness.frame(build);
    harness.frame(build);
    assert(submitted == 1);
    assert(contentRects[2].width > 0.0f);
    thirdFlags = ig::Context::TabItemFlags::None;

    // Uma barra nova com o AutoSelectNewTabs: uma tab que apareça fica escolhida.
    barFlags = ig::Context::TabBarFlags::AutoSelectNewTabs;
    harness.frame(build);
    harness.frame(build);
    assert(submitted == 1);

    // (O Fitting, que só encolhe quando as tabs não cabem, tem o teste dele.)
    barFlags = ig::Context::TabBarFlags::Reorderable;
    // Arrastar a primeira tab para cima da terceira troca-lhes a ordem: no frame
    // seguinte a primeira fica onde estava a terceira e vice-versa.
    harness.frame(build);
    const float firstX = tabRects[0].x;
    const float thirdX = tabRects[2].x;
    const float firstCentre = tabRects[0].x + tabRects[0].width * 0.5f;
    context.pushEvent(ig::Event::pointerMove(firstCentre, tabY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, firstCentre, tabY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerMove(thirdX + 4.0f, tabY));
    harness.frame(build);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, thirdX + 4.0f, tabY));
    harness.frame(build);
    harness.frame(build);
    assert(tabRects[0].x != firstX);
    assert(tabRects[0].x > firstX);
}

// O Fitting de uma barra de tabs: as tabs que não cabem encolhem para dividirem a
// largura da barra (é o que faz uma barra com muitos documentos continuar a caber).
void test_tab_bar_fitting()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    unsigned barFlags = ig::Context::TabBarFlags::None;
    ig::Rect rects[2];

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("fitting", ig::Rect(0.0f, 0.0f, 400.0f, 200.0f)));
        if (c.beginTabBar("largo", barFlags))
        {
            if (c.beginTabItem("um nome de tab bastante comprido"))
                c.endTabItem();
            rects[0] = c.itemRect();
            if (c.beginTabItem("outro nome de tab igualmente comprido"))
                c.endTabItem();
            rects[1] = c.itemRect();
            c.endTabBar();
        }
        c.endWindow();
    };

    // Sem o Fitting as tabs têm a largura do rótulo (e juntas não cabem nos 384).
    harness.frame(build);
    const float natural = rects[0].width;
    assert(natural > 192.0f);
    assert(rects[0].width + rects[1].width > 384.0f);
    assert(rects[1].x == rects[0].x + rects[0].width);

    // Com ele, cada uma fica com metade da barra.
    barFlags = ig::Context::TabBarFlags::Fitting;
    harness.frame(build);
    harness.frame(build);
    assert(fabsf(rects[0].width - 192.0f) < 1.0f);
    assert(fabsf(rects[1].width - 192.0f) < 1.0f);
    assert(rects[1].x == rects[0].x + rects[0].width);
}

// A rolagem horizontal de uma tabela (TableStyle::scrollX): as colunas são a
// largura pedida (innerWidth), o que não cabe rola dentro da tabela com a roda
// horizontal e com a barra de baixo, e as primeiras freezeCols ficam presas à
// esquerda - o que rola é cortado por elas.
void test_table_scrolls_sideways()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[3] = {1.0f, 1.0f, 1.0f};
    ig::TableStyle style;
    style.borders = true;
    style.scrollX = true;
    style.innerWidth = 600.0f;
    style.freezeCols = 1;
    ig::Rect headerRects[3];
    ig::Rect cellRects[3];
    bool cellVisible[3] = {false, false, false};
    const float tableWidth = 300.0f;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("lado", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 3), style, tableWidth))
        {
            for (int i = 0; i < 3; ++i)
            {
                if (!c.tableNextColumn())
                    continue;
                c.tableHeader(i == 0 ? "A" : (i == 1 ? "B" : "C"));
                headerRects[i] = c.itemRect();
            }
            for (int i = 0; i < 3; ++i)
            {
                if (!c.tableNextColumn())
                    continue;
                // Um dummy da largura da coluna: o itemRect é o da célula (um
                // label é só o texto) e o isItemVisible é o da coluna.
                c.dummy(200.0f, 16.0f);
                cellRects[i] = c.itemRect();
                cellVisible[i] = c.isItemVisible();
            }
            c.endTable();
        }
        c.endWindow();
    };

    const ig::DrawData &firstFrame = harness.frame(build);
    // As colunas são a largura pedida dividida pelos pesos (600/3 = 200), não a
    // largura da tabela: a terceira já sai para fora dela.
    const float columnWidth = 200.0f;
    const float firstHeaderX = headerRects[0].x;
    const float scrolled = 28.0f;   // um entalhe anda o que mede uma widget
    // A largura da coluna lê-se do cabeçalho (a célula é o texto, que é mais
    // estreito): é a coluna que segue a innerWidth, não a tabela.
    assert(fabs(headerRects[0].width - columnWidth) < 1.0f);
    assert(cellRects[0].x == headerRects[0].x);
    assert(cellRects[1].x == headerRects[1].x);
    assert(fabs(cellRects[1].width - columnWidth) < 1.0f);
    assert(headerRects[1].x == firstHeaderX + columnWidth);
    assert(headerRects[2].x == firstHeaderX + columnWidth * 2.0f);
    assert(headerRects[2].x > firstHeaderX + tableWidth);
    // A terceira coluna está fora da tabela, por isso a célula dela não se vê (o clip
    // é a banda que rola); a segunda, que começa onde as presas acabam, vê-se.
    assert(cellVisible[1]);
    assert(!cellVisible[2]);

    // A roda horizontal rola a tabela: a segunda coluna (que rola) anda para a
    // esquerda e a primeira (presa) não se mexe.
    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelX = -1.0f;
    context.pushEvent(ig::Event::pointerMove(100.0f, cellRects[0].y + 4.0f));
    context.pushEvent(wheel);
    harness.frame(build);
    assert(headerRects[0].x == firstHeaderX);
    assert(fabsf(headerRects[1].x - (firstHeaderX + columnWidth - scrolled)) < 1.0f);
    assert(headerRects[2].x == headerRects[1].x + columnWidth);

    // Rolar até ao fim para: a segunda coluna sai de vista (tapada pelas presas) e a
    // terceira entra no lugar dela.
    for (int i = 0; i < 40; ++i)
    {
        context.pushEvent(ig::Event::pointerMove(100.0f, cellRects[0].y + 4.0f));
        context.pushEvent(wheel);
        harness.frame(build);
    }
    assert(headerRects[0].x == firstHeaderX);
    assert(!cellVisible[1]);
    assert(cellVisible[2]);
    // A terceira coluna (natural em x + 2*largura) fica a rolagem máxima à frente.
    assert(fabsf(headerRects[2].x -
                 (firstHeaderX + columnWidth * 2.0f - (600.0f - tableWidth))) < 1.0f);

    // A barra de baixo está no fundo da tabela, com o punho à cor do tema.
    const ig::Color thumbColor = context.theme().scrollbarThumb;
    bool foundThumb = false;
    float thumbY = 0.0f;
    float thumbRight = 0.0f;
    const ig::DrawData &lastFrame = harness.frame(build);
    for (ig::Span<const ig::DrawVertex>::size_type i = 0u; i < lastFrame.vertices.size(); ++i)
    {
        const ig::DrawVertex &vertex = lastFrame.vertices[i];
        if (vertex.color != thumbColor)
            continue;
        if (!foundThumb)
        {
            foundThumb = true;
            thumbY = vertex.position.y;
            thumbRight = vertex.position.x;
        }
        if (vertex.position.y > thumbY)
            thumbY = vertex.position.y;
        if (vertex.position.x > thumbRight)
            thumbRight = vertex.position.x;
    }
    assert(foundThumb);
    assert(thumbY > firstHeaderX + 40.0f);
    // O punho chega ao fim da barra quando a rolagem está no máximo.
    assert(thumbRight >= firstHeaderX + tableWidth - 2.0f);
}

// Uma tabela com scrollX mas sem largura própria (innerWidth 0 ou menor do que a
// tabela) não rola: as colunas continuam a dividir a largura dela.
void test_table_without_sideways_scroll_keeps_the_width()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[2] = {1.0f, 1.0f};
    ig::TableStyle style;
    style.scrollX = true;
    style.innerWidth = 200.0f;   // menos do que a tabela (300)
    ig::Rect rects[2];

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("lado2", ig::Rect(0.0f, 0.0f, 400.0f, 200.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 2), style, 300.0f))
        {
            for (int i = 0; i < 2; ++i)
            {
                if (!c.tableNextColumn())
                    continue;
                c.dummy(150.0f, 16.0f);
                rects[i] = c.itemRect();
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    assert(fabs(rects[0].width - 150.0f) < 1.0f);
    assert(rects[1].x == rects[0].x + rects[0].width);
}

} // namespace

// A face em vigor dentro de um bloco, e o que volta depois dele: a face escolhida
// leva o tamanho de fabrico dela, um id sem face não desenha, e a pilha fica como
// estava no fim do frame mesmo que falte um pop.
// Um popup aberto de dentro de outro (o "Stacked Popup" do demo): os dois ficam
// abertos - é uma pilha -, o filho é quem recebe o ponteiro, o Escape fecha só o de
// cima, e um clique dentro do pai mas fora do filho fecha só o filho.
void test_stacked_popups()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const ig::Vec2 parentPosition(40.0f, 60.0f);
    const ig::Vec2 childPosition(70.0f, 100.0f);
    bool wantParent = false;
    bool wantChild = false;
    bool parentOpen = false;
    bool childOpen = false;
    ig::Rect openButton;
    ig::Rect childButton;
    int hits = 0;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("popups", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.button("abrir o pai"))
            wantParent = true;
        openButton = c.itemRect();
        if (wantParent)
        {
            c.openPopup("pai", parentPosition);
            wantParent = false;
        }
        if (c.beginPopup("pai"))
        {
            c.label("conteudo do pai");
            if (c.button("abrir o filho"))
                wantChild = true;
            childButton = c.itemRect();
            if (wantChild)
            {
                c.openPopup("filho", childPosition);
                wantChild = false;
            }
            if (c.beginPopup("filho"))
            {
                if (c.selectable("escolher", false))
                    ++hits;
                c.endPopup();
            }
            c.endPopup();
        }
        parentOpen = c.isPopupOpen("pai");
        childOpen = c.isPopupOpen("filho");
        c.endWindow();
    };

    harness.frame(build);
    // Abre o pai pelo botão.
    harness.click(openButton.x + openButton.width * 0.5f,
                  openButton.y + openButton.height * 0.5f, build);
    harness.frame(build);
    assert(parentOpen && !childOpen);

    // Abre o filho pelo botão de dentro do pai: os dois ficam abertos (antes disto o
    // filho substituía o pai).
    harness.click(childButton.x + childButton.width * 0.5f,
                  childButton.y + childButton.height * 0.5f, build);
    harness.frame(build);
    assert(parentOpen && childOpen);
    // O pai e o filho desenham-se os dois: as duas caixas estão no ecrã.
    assert(harness.hasVertexNear(parentPosition.x + 1.0f, parentPosition.y + 1.0f, 4.0f));
    assert(harness.hasVertexNear(childPosition.x + 1.0f, childPosition.y + 1.0f, 4.0f));

    // O filho é quem recebe o ponteiro: o selectable dele conta o clique, mesmo com o
    // pai aberto por baixo.
    harness.click(childPosition.x + 20.0f, childPosition.y + 30.0f, build);
    harness.frame(build);
    assert(hits == 1);

    // Escape fecha só o de cima: o pai fica.
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Escape));
    harness.frame(build);
    assert(parentOpen && !childOpen);

    // Abre outra vez o filho e clica dentro do pai mas na margem dele (por cima do
    // conteúdo vazio): fecha só o filho.
    harness.click(childButton.x + childButton.width * 0.5f,
                  childButton.y + childButton.height * 0.5f, build);
    harness.frame(build);
    assert(parentOpen && childOpen);
    harness.click(parentPosition.x + 2.0f, parentPosition.y + 2.0f, build);
    harness.frame(build);
    assert(parentOpen && !childOpen);

    // E um clique fora dos dois fecha a pilha toda.
    harness.click(300.0f, 250.0f, build);
    harness.frame(build);
    assert(!parentOpen && !childOpen);
}

// Uma barra de menus dentro de um popup (o "With a menu.." do demo): o menu abre
// por cima do popup e os itens dele saem fora da caixa do popup. Um clique num
// desses itens é do menu - era o popup a fechar-se com ele e a deixar o menu
// pendurado (e o clique comido duas vezes).
void test_menu_inside_a_popup()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool wantPopup = false;
    bool popupOpen = false;
    ig::Rect openButton;
    ig::Rect fileItem;
    ig::Rect lastItem;
    int clicked = 0;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("popup com menu", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.button("abrir"))
            wantPopup = true;
        openButton = c.itemRect();
        if (wantPopup)
        {
            // O popup nasce no canto de cima: o menu dele cai para fora da caixa.
            c.openPopup("com menu", ig::Vec2(40.0f, 40.0f));
            wantPopup = false;
        }
        if (c.beginPopup("com menu"))
        {
            if (c.beginMenuBar(ig::Rect(0.0f, 0.0f, c.availableWidth(), 26.0f)))
            {
                if (c.beginMenu("File"))
                {
                    // Quatro itens: a caixa do menu é mais alta que a do popup (que
                    // só tem a barra e um rótulo), por isso os últimos itens ficam
                    // fora da caixa do popup.
                    if (c.menuItem("New"))
                        ++clicked;
                    if (c.menuItem("Open"))
                        ++clicked;
                    if (c.menuItem("Save"))
                        ++clicked;
                    if (c.menuItem("Quit"))
                        ++clicked;
                    lastItem = c.itemRect();
                    fileItem = c.itemRect();
                    c.endMenu();
                }
                c.endMenuBar();
            }
            c.label("Hello from popup!");
            c.endPopup();
        }
        popupOpen = c.isPopupOpen("com menu");
        c.endWindow();
    };

    harness.frame(build);
    harness.click(openButton.x + openButton.width * 0.5f,
                  openButton.y + openButton.height * 0.5f, build);
    harness.frame(build);
    assert(popupOpen);
    // Abre o menu "File" (o primeiro item da barra, dentro do popup).
    const float menuX = 40.0f + context.theme().windowPadding +
                        context.theme().menuItemPadX + 4.0f;
    const float menuY = 40.0f + context.theme().windowPadding + 13.0f;
    harness.click(menuX, menuY, build);
    harness.frame(build);
    // O último item do menu já está fora da caixa do popup.
    assert(lastItem.width >= 0.0f);
    const float itemX = lastItem.x + lastItem.width * 0.5f;
    const float itemY = lastItem.y + lastItem.height * 0.5f;
    assert(itemX > 40.0f);
    harness.click(itemX, itemY, build);
    harness.frame(build);
    // O item do menu reagiu e o popup ficou aberto (o menu é dele).
    assert(clicked == 1);
    assert(popupOpen);
    // E o popup continua a responder: clicar "fora" (no canto do écran) fecha-o.
    harness.click(380.0f, 280.0f, build);
    harness.frame(build);
    assert(!popupOpen);
}

// Um submenu dentro de um popup (o "Sub-menu" do "Toggle.." do demo): a caixa do
// submenu abre ao lado do item, FORA da caixa do popup, e um clique nos itens dele
// é do submenu - o popup fica aberto e o item reage. Era aqui que o popup ficava a
// medir a caixa com o layout do submenu e a fechar-se com o clique.
void test_submenu_inside_a_popup()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool wantPopup = false;
    bool popupOpen = false;
    bool submenuOpen = false;
    ig::Rect openButton;
    ig::Rect submenuRow;
    ig::Rect submenuItem;
    bool submenuItemHovered = false;
    int clicked = 0;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("submenu num popup", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.button("abrir"))
            wantPopup = true;
        openButton = c.itemRect();
        if (wantPopup)
        {
            c.openPopup("com submenu", ig::Vec2(40.0f, 40.0f));
            wantPopup = false;
        }
        if (c.beginPopup("com submenu"))
        {
            bool first = true;
            c.menuCheckbox("Bream", first);
            c.menuCheckbox("Haddock", first);
            // O submenu: abre por hover (é um menu) e a caixa dele cai fora do popup.
            submenuOpen = c.beginSubMenu("Sub-menu");
            if (submenuOpen)
            {
                if (c.menuItem("Click me"))
                    ++clicked;
                submenuItem = c.itemRect();
                submenuItemHovered = c.isItemHovered();
                c.endSubMenu();
            }
            c.endPopup();
        }
        popupOpen = c.isPopupOpen("com submenu");
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    harness.click(openButton.x + openButton.width * 0.5f,
                  openButton.y + openButton.height * 0.5f, build);
    harness.frame(build);
    assert(popupOpen && !submenuOpen);

    // O "Sub-menu" é a terceira linha do popup (as duas primeiras são os checkboxes),
    // com a altura de um item de menu.
    const float itemHeight = context.theme().menuItemHeight;
    const float columnX = 40.0f + context.theme().windowPadding + 20.0f;
    const float rowY = 40.0f + context.theme().windowPadding + itemHeight * 2.0f + 8.0f +
                       itemHeight * 0.5f;
    context.pushEvent(ig::Event::pointerMove(columnX, rowY));
    harness.frame(build);
    harness.frame(build);
    assert(popupOpen && submenuOpen);

    // A caixa do submenu é um menu: fica ao lado do item (bem à direita) e passa a
    // borda direita do popup (o popup tem a largura mínima de um menu).
    assert(submenuItem.x > 150.0f);
    assert(submenuItem.x + submenuItem.width > 40.0f + context.theme().menuMinWidth);
    // E o clique no item dele é do submenu: o item reage e o popup fica aberto.
    harness.click(submenuItem.x + 20.0f, submenuItem.y + submenuItem.height * 0.5f, build);
    harness.frame(build);
    assert(clicked == 1);
    assert(popupOpen);
    assert(submenuItemHovered);
    // Sem o submenu aberto, um clique fora dos dois fecha o popup.
    harness.click(360.0f, 260.0f, build);
    harness.frame(build);
    assert(!popupOpen);
}

// Uma tabela que rola sem linhas presas (freezeRows = 0): a linha 0 rola como as
// outras, e o clique por cima dela vai para a linha que lá está.
void test_table_without_frozen_rows_scrolls_the_first_row()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[2] = { 1.0f, 1.0f };
    ig::TableStyle style;
    style.scrollY = true;
    style.height = 120.0f;
    style.freezeRows = 0;
    const int kRows = 20;
    ig::Rect rects[kRows];
    bool visible[kRows];
    bool clicked[kRows];

    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 500.0f, 500.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 2), style, 300.0f))
        {
            for (int row = 0; row < kRows; ++row)
            {
                c.tableNextColumn();
                char text[16];
                snprintf(text, sizeof(text), "linha%d", row);
                clicked[row] = c.button(text);
                rects[row] = c.itemRect();
                visible[row] = c.isItemVisible();
                c.tableNextColumn();
                c.label("x");
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    const float step = rects[1].y - rects[0].y;
    assert(step > 0.0f);
    const float firstBefore = rects[0].y;

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    context.pushEvent(ig::Event::pointerMove(50.0f, rects[1].y + 5.0f));
    context.pushEvent(wheel);
    harness.frame(build);
    const float moved = firstBefore - rects[0].y;
    assert(moved > 0.0f);
    assert(rects[1].y - rects[0].y == step);
    assert(rects[2].y - rects[1].y == step);

    for (int i = 0; i < 3; ++i)
    {
        context.pushEvent(ig::Event::pointerMove(50.0f, rects[3].y + 5.0f));
        context.pushEvent(wheel);
        harness.frame(build);
    }
    assert(!visible[0]);
    assert(rects[1].y - rects[0].y == step);

    int target = -1;
    for (int row = 0; row < kRows; ++row)
        if (visible[row] && rects[row].y >= firstBefore)
        {
            target = row;
            break;
        }
    assert(target > 0);
    harness.click(50.0f, rects[target].y + 5.0f, build);
    for (int row = 0; row < kRows; ++row) assert(clicked[row] == (row == target));
}

// Uma tabela que rola e cujo conteúdo encolhe: a rolagem volta para dentro do que
// existe, e as linhas que sobram ficam à vista.
void test_table_scroll_is_clamped_when_rows_shrink()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[2] = { 1.0f, 1.0f };
    ig::TableStyle style;
    style.scrollY = true;
    style.height = 120.0f;
    int rows = 20;
    ig::Rect rects[20];
    bool visible[20];

    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 500.0f, 500.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 2), style, 300.0f))
        {
            for (int row = 0; row < rows; ++row)
            {
                c.tableNextColumn();
                c.label("linha");
                rects[row] = c.itemRect();
                visible[row] = c.isItemVisible();
                c.tableNextColumn();
                c.label("x");
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    const float top = rects[0].y;

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    for (int i = 0; i < 30; ++i)
    {
        context.pushEvent(ig::Event::pointerMove(50.0f, top + 20.0f));
        context.pushEvent(wheel);
        harness.frame(build);
    }
    assert(!visible[0]);
    assert(visible[19]);

    rows = 3;
    harness.frame(build);
    harness.frame(build);
    assert(rects[0].y == top);
    for (int row = 0; row < rows; ++row) assert(visible[row]);

    rows = 8;
    for (int i = 0; i < 30; ++i)
    {
        context.pushEvent(ig::Event::pointerMove(50.0f, top + 20.0f));
        context.pushEvent(wheel);
        harness.frame(build);
    }
    const float bottomWhenScrolled = rects[7].y;
    rows = 6;
    harness.frame(build);
    harness.frame(build);
    assert(visible[5]);
    assert(rects[5].y <= bottomWhenScrolled);
    assert(rects[5].y + rects[5].height <= top + style.height);
}

// Uma tabela que rola nos dois eixos: a barra de baixo vê-se e arrasta-se como numa
// tabela que só rola na horizontal.
void test_table_scrolling_both_ways_drags_the_bottom_bar()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[3] = { 1.0f, 1.0f, 1.0f };
    ig::TableStyle style;
    style.scrollY = true;
    style.height = 200.0f;
    style.freezeRows = 1;
    style.scrollX = true;
    style.innerWidth = 900.0f;
    ig::Rect header[3];

    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 3), style, 300.0f))
        {
            const char* names[3] = { "A", "B", "C" };
            for (int column = 0; column < 3; ++column)
            {
                c.tableNextColumn();
                c.tableHeader(names[column]);
                header[column] = c.itemRect();
            }
            for (int row = 0; row < 20; ++row)
                for (int column = 0; column < 3; ++column)
                {
                    c.tableNextColumn();
                    c.label("x");
                }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    const float top = header[0].y;
    const float barHeight = context.theme().scrollbarWidth * 0.65f;
    const float barY = top + style.height - barHeight * 0.5f;
    const float columnBefore = header[1].x;

    const ig::DrawData& data = harness.frame(build);
    bool barDrawn = false;
    for (ig::Span<const ig::DrawCommand>::size_type i = 0u; i < data.commands.size(); ++i)
    {
        const ig::DrawCommand& command = data.commands[i];
        if (command.type != ig::DrawCommandType::Geometry) continue;
        const ig::Rect clip = command.payload.geometry.clip;
        if (clip.y <= barY && clip.y + clip.height >= barY && clip.height <= barHeight + 0.5f &&
                clip.width > 0.0f && command.payload.geometry.indexCount > 0u)
            barDrawn = true;
    }
    assert(barDrawn);

    harness.drag(header[0].x + 20.0f, barY, header[0].x + 250.0f, barY, build, 4);
    assert(header[1].x < columnBefore);
}

// Uma tabela que rola nos dois eixos, rolada até ao fim: a última linha fica toda
// por cima da barra de baixo e responde ao clique no fundo dela.
void test_table_scrolling_both_ways_shows_the_last_row()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[3] = { 1.0f, 1.0f, 1.0f };
    ig::TableStyle style;
    style.scrollY = true;
    style.height = 200.0f;
    style.freezeRows = 1;
    style.scrollX = true;
    style.innerWidth = 900.0f;
    const int kRows = 20;
    ig::Rect header;
    ig::Rect rects[kRows];
    bool clicked[kRows];

    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 3), style, 300.0f))
        {
            for (int column = 0; column < 3; ++column)
            {
                c.tableNextColumn();
                c.tableHeader("H");
                if (column == 0) header = c.itemRect();
            }
            for (int row = 0; row < kRows; ++row)
            {
                c.tableNextColumn();
                char text[16];
                snprintf(text, sizeof(text), "linha%d", row);
                clicked[row] = c.button(text);
                rects[row] = c.itemRect();
                c.tableNextColumn();
                c.label("x");
                c.tableNextColumn();
                c.label("y");
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    const float top = header.y;
    const float barHeight = context.theme().scrollbarWidth * 0.65f;

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    for (int i = 0; i < 60; ++i)
    {
        context.pushEvent(ig::Event::pointerMove(header.x + 20.0f, top + 60.0f));
        context.pushEvent(wheel);
        harness.frame(build);
    }
    const ig::Rect last = rects[kRows - 1];
    assert(last.y + last.height <= top + style.height - barHeight + 0.01f);
    harness.click(last.x + 10.0f, last.y + last.height - 2.0f, build);
    assert(clicked[kRows - 1]);
}

// Uma tabela com altura própria ocupa essa altura, role ou não na horizontal: o
// item seguinte cai no mesmo sítio.
void test_table_scrolling_both_ways_keeps_its_height()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[3] = { 1.0f, 1.0f, 1.0f };
    ig::TableStyle style;
    style.scrollY = true;
    style.height = 200.0f;
    style.freezeRows = 1;
    style.innerWidth = 900.0f;
    ig::Rect header;
    ig::Rect after;

    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 3), style, 300.0f))
        {
            for (int column = 0; column < 3; ++column)
            {
                c.tableNextColumn();
                c.tableHeader("H");
                if (column == 0) header = c.itemRect();
            }
            for (int row = 0; row < 20; ++row)
                for (int column = 0; column < 3; ++column)
                {
                    c.tableNextColumn();
                    c.label("x");
                }
            c.endTable();
        }
        c.button("depois");
        after = c.itemRect();
        c.endWindow();
    };

    style.scrollX = false;
    harness.frame(build);
    harness.frame(build);
    const float gap = after.y - (header.y + style.height);
    assert(gap > 0.0f);

    style.scrollX = true;
    harness.frame(build);
    harness.frame(build);
    assert(after.y - (header.y + style.height) == gap);
}

// Redimensionar colunas numa tabela que rola na horizontal: arrastar o grip 30 px
// passa 30 px de uma coluna para a seguinte, como sem rolagem.
void test_table_resize_with_sideways_scroll_follows_the_pointer()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float weights[3] = { 1.0f, 1.0f, 1.0f };
    ig::TableStyle style;
    style.scrollX = true;
    style.innerWidth = 600.0f;
    ig::Rect header[3];

    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
        if (c.beginTable("t", ig::Span<float>(weights, 3), style, 300.0f))
        {
            const char* names[3] = { "A", "B", "C" };
            for (int column = 0; column < 3; ++column)
            {
                c.tableNextColumn();
                c.tableHeader(names[column]);
                header[column] = c.itemRect();
            }
            for (int column = 0; column < 3; ++column)
            {
                c.tableNextColumn();
                c.label("x");
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    const float first = header[1].x - header[0].x;
    const float second = header[2].x - header[1].x;
    const float gripX = header[0].x + header[0].width;
    const float gripY = header[0].y + header[0].height * 0.5f;
    harness.drag(gripX, gripY, gripX + 30.0f, gripY, build, 3);
    harness.frame(build);
    assert(fabsf(header[1].x - header[0].x - (first + 30.0f)) < 0.01f);
    assert(fabsf(header[2].x - header[1].x - (second - 30.0f)) < 0.01f);
}

// Um arrasto de cabeçalho que se perde (a tabela não foi submetida durante uns
// frames) larga o ponteiro quando a tabela volta, seja qual for a coluna
// arrastada, mesmo com o botão ainda premido.
void test_table_lost_header_drag_releases_the_pointer()
{
    for (int dragged = 0; dragged < 3; ++dragged)
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);

        bool showTable = true;
        ig::Rect header[3];
        auto build = [&](ig::Context& c)
        {
            assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
            ig::TableStyle style;
            style.reorderable = true;
            const float weights[3] = { 1.0f, 1.0f, 1.0f };
            if (showTable && c.beginTable("t", ig::Span<const float>(weights, 3), style, 300.0f))
            {
                const char* names[3] = { "A", "B", "C" };
                for (int column = 0; column < 3; ++column)
                {
                    c.tableNextColumn();
                    c.tableHeader(names[column]);
                    header[column] = c.itemRect();
                }
                c.endTable();
            }
            c.endWindow();
        };

        harness.frame(build);
        harness.frame(build);
        const float x = header[dragged].x + 20.0f;
        const float y = header[dragged].y + 5.0f;
        context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
        harness.frame(build);
        context.pushEvent(ig::Event::pointerMove(x + 30.0f, y));
        harness.frame(build);
        assert(context.wantsPointer());

        showTable = false;
        harness.frame(build);
        harness.frame(build);
        showTable = true;
        context.pushEvent(ig::Event::pointerMove(500.0f, 400.0f));
        harness.frame(build);
        harness.frame(build);
        assert(!context.wantsPointer());
    }
}

// beginTable(id, int columns, style) segue o style como a versão com pesos: a
// altura própria, a rolagem e as colunas reordenáveis.
void test_table_with_column_count_uses_its_style()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::TableStyle style;
    style.scrollY = true;
    style.height = 100.0f;
    style.freezeRows = 1;
    style.reorderable = true;
    ig::Rect header[2];
    ig::Rect after;
    bool lastVisible = true;
    int firstModel = -1;

    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 600.0f, 1200.0f)));
        if (c.beginTable("t", 2, style, 300.0f))
        {
            const char* names[2] = { "A", "B" };
            for (int column = 0; column < 2; ++column)
            {
                c.tableNextColumn();
                if (column == 0) firstModel = c.tableColumnIndex();
                c.tableHeader(names[column]);
                header[column] = c.itemRect();
            }
            for (int row = 0; row < 20; ++row)
            {
                c.tableNextColumn();
                c.label("r");
                lastVisible = c.isItemVisible();
                c.tableNextColumn();
                c.label("x");
            }
            c.endTable();
        }
        c.button("depois");
        after = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    assert(after.y < header[0].y + style.height + 20.0f);
    assert(!lastVisible);

    harness.drag(header[0].x + 20.0f, header[0].y + 5.0f, header[1].x + 50.0f, header[0].y + 5.0f,
            build, 4);
    harness.frame(build);
    assert(firstModel == 1);
}

// Uma tabela que rola na horizontal e cuja innerWidth diminui: a rolagem volta
// para dentro do que existe e a última coluna fica encostada à direita.
void test_table_sideways_scroll_is_clamped_when_inner_width_shrinks()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[3] = { 1.0f, 1.0f, 1.0f };
    ig::TableStyle style;
    style.scrollX = true;
    style.innerWidth = 900.0f;
    ig::Rect header[3];
    const float tableWidth = 300.0f;

    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 3), style, tableWidth))
        {
            const char* names[3] = { "A", "B", "C" };
            for (int column = 0; column < 3; ++column)
            {
                c.tableNextColumn();
                c.tableHeader(names[column]);
                header[column] = c.itemRect();
            }
            for (int column = 0; column < 3; ++column)
            {
                c.tableNextColumn();
                c.label("x");
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    const float left = header[0].x;

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelX = -1.0f;
    for (int i = 0; i < 40; ++i)
    {
        context.pushEvent(ig::Event::pointerMove(left + 50.0f, header[0].y + 5.0f));
        context.pushEvent(wheel);
        harness.frame(build);
    }
    assert(fabsf(header[0].x - (left - (style.innerWidth - tableWidth))) < 0.01f);

    style.innerWidth = 400.0f;
    harness.frame(build);
    assert(fabsf(header[0].x - (left - (style.innerWidth - tableWidth))) < 0.01f);
    assert(fabsf(header[2].x + header[2].width - (left + tableWidth)) < 0.01f);
}

// SizingFixedFit com itens que ocupam a largura que sobra (o cabeçalho, uma
// progressBar): eles não pedem largura, e as colunas param no que os itens de
// largura fixa pedem.
void test_table_sizing_fixed_fit_ignores_items_that_fill_the_cell()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[3] = { 1.0f, 1.0f, 1.0f };
    ig::TableStyle style;
    style.sizingFixedFit = true;
    ig::Rect header[3];
    ig::Rect bar;
    const float padding = context.theme().windowPadding;

    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 900.0f, 600.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 3), style, 300.0f))
        {
            const char* names[3] = { "A", "B", "C" };
            for (int column = 0; column < 3; ++column)
            {
                c.tableNextColumn();
                c.tableHeader(names[column]);
                header[column] = c.itemRect();
            }
            c.tableNextColumn();
            c.dummy(40.0f, 16.0f);
            c.tableNextColumn();
            c.dummy(60.0f, 16.0f);
            c.progressBar(0.5f, 1.0f);
            bar = c.itemRect();
            c.tableNextColumn();
            c.dummy(20.0f, 16.0f);
            c.endTable();
        }
        c.endWindow();
    };

    for (int frame = 0; frame < 4; ++frame) harness.frame(build);
    float widths[3];
    for (int column = 0; column < 3; ++column) widths[column] = header[column].width;
    assert(widths[0] == 40.0f + padding * 2.0f);
    assert(widths[1] == 60.0f + padding * 2.0f);
    assert(bar.width == widths[1]);

    for (int frame = 0; frame < 8; ++frame) harness.frame(build);
    for (int column = 0; column < 3; ++column) assert(header[column].width == widths[column]);
}

// A roda por cima de uma área que não rola (uma tabela scrollY cujas linhas cabem,
// um child cujo conteúdo cabe) rola o child de fora.
void test_wheel_over_an_area_that_fits_scrolls_the_outer_child()
{
    for (int mode = 0; mode < 2; ++mode)
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);

        ig::Rect first;
        ig::Rect inner;
        auto build = [&](ig::Context& c)
        {
            assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
            if (c.beginChild("fora", 300.0f))
            {
                c.button("primeiro");
                first = c.itemRect();
                if (mode == 0)
                {
                    ig::TableStyle style;
                    style.scrollY = true;
                    style.height = 150.0f;
                    style.freezeRows = 1;
                    const float weights[2] = { 1.0f, 1.0f };
                    if (c.beginTable("t", ig::Span<const float>(weights, 2), style, 300.0f))
                    {
                        for (int row = 0; row < 2; ++row)
                        {
                            c.tableNextColumn();
                            c.label("a");
                            c.tableNextColumn();
                            c.label("b");
                        }
                        c.endTable();
                    }
                }
                else if (c.beginChild("dentro", 150.0f))
                {
                    c.label("a");
                    c.label("b");
                    c.endChild();
                }
                inner = c.itemRect();
                for (int i = 0; i < 30; ++i) c.label("enchimento");
                c.endChild();
            }
            c.endWindow();
        };

        harness.frame(build);
        harness.frame(build);
        const float before = first.y;
        ig::Event wheel;
        wheel.type = ig::EventType::PointerWheel;
        wheel.wheelY = -1.0f;
        for (int i = 0; i < 3; ++i)
        {
            context.pushEvent(ig::Event::pointerMove(inner.x + 20.0f, inner.y + 40.0f));
            context.pushEvent(wheel);
            harness.frame(build);
        }
        assert(before - first.y > 0.0f);
    }
}

// Dois tableTreeCell na mesma linha abrem e fecham cada um o seu.
void test_table_tree_cells_in_one_row_toggle_apart()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[2] = { 1.0f, 1.0f };
    bool expanded[2] = { false, false };
    ig::Rect cells[2];

    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 2), 300.0f))
        {
            for (int column = 0; column < 2; ++column)
            {
                c.tableNextColumn();
                c.tableTreeCell("no", 0, true, expanded[column]);
                cells[column] = c.itemRect();
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    harness.click(cells[1].x + 5.0f, cells[1].y + cells[1].height * 0.5f, build);
    assert(!expanded[0]);
    assert(expanded[1]);
    harness.click(cells[0].x + 5.0f, cells[0].y + cells[0].height * 0.5f, build);
    assert(expanded[0]);
    assert(expanded[1]);
}

// Com rolagem horizontal e colunas presas, a faixa das linhas alternadas cobre a
// largura toda da tabela.
void test_table_alternate_rows_span_the_table_with_frozen_columns()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[6] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    ig::TableStyle style;
    style.alternateRows = true;
    style.scrollX = true;
    style.innerWidth = 600.0f;
    style.freezeCols = 1;
    style.borders = true;
    const float tableWidth = 300.0f;
    ig::Rect firstCell;

    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 6), style, tableWidth))
        {
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 6; ++column)
                {
                    c.tableNextColumn();
                    c.label("x");
                    if (row == 0 && column == 0) firstCell = c.itemRect();
                }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    const ig::DrawData& data = harness.frame(build);
    int bands = 0;
    int scrolledLines = 0;
    for (ig::Span<const ig::DrawCommand>::size_type i = 0u; i < data.commands.size(); ++i)
    {
        const ig::DrawCommand& command = data.commands[i];
        if (command.type != ig::DrawCommandType::Geometry) continue;
        const ig::GeometryCommand& geometry = command.payload.geometry;
        const float pinRight = firstCell.x + 100.0f;
        const ig::Color border = context.theme().borderColor;
        bool band = false;
        for (uint32_t index = 0u; index < geometry.indexCount; ++index)
        {
            const ig::DrawIndex vertex = data.indices[geometry.firstIndex + index];
            const ig::DrawVertex& v =
                    data.vertices[static_cast<uint32_t>(geometry.vertexOffset) + vertex];
            if (v.color.a == 14u) band = true;
            if (v.color.r == border.r && v.color.g == border.g && v.color.b == border.b &&
                    v.color.a == border.a && v.position.x > pinRight + 2.0f &&
                    v.position.x < firstCell.x + tableWidth - 2.0f &&
                    v.position.x >= geometry.clip.x &&
                    v.position.x <= geometry.clip.x + geometry.clip.width)
                ++scrolledLines;
        }
        if (!band) continue;
        ++bands;
        assert(geometry.clip.width >= tableWidth - 0.5f);
    }
    assert(bands > 0);
    assert(scrolledLines > 0);
}

// Uma lista virtual rolada até ao fim que encolhe mostra logo os itens que sobram.
void test_virtual_list_that_shrinks_is_not_empty()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    int count = 100;
    int first = 0;
    int last = 0;
    ig::Rect firstRect;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
        if (c.beginVirtualList("lista", count, 20.0f, 200.0f, first, last))
        {
            for (int i = first; i < last; ++i)
            {
                char text[16];
                snprintf(text, sizeof(text), "i%d", i);
                c.button(text, c.virtualListItemRect(i));
                if (i == 0) firstRect = c.itemRect();
            }
            c.endVirtualList();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    const float top = firstRect.y;
    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    for (int i = 0; i < 100; ++i)
    {
        context.pushEvent(ig::Event::pointerMove(50.0f, top + 50.0f));
        context.pushEvent(wheel);
        harness.frame(build);
    }
    assert(last == count);
    assert(first > 0);

    count = 5;
    harness.frame(build);
    assert(first == 0);
    assert(last == count);
    assert(firstRect.y == top);
}

// Um submenu aberto fecha quando o rato passa para outro item do menu pai, mas não
// quando o rato vai na direção dele por cima desse item.
void test_submenu_closes_when_another_item_is_hovered()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool subOpen = false;
    bool picked = false;
    ig::Rect fileRect;
    ig::Rect newRect;
    ig::Rect recentRect;
    ig::Rect exitRect;
    ig::Rect pickRect;
    auto build = [&](ig::Context& c)
    {
        if (c.beginMainMenuBar())
        {
            const bool fileOpen = c.beginMenu("File");
            fileRect = c.itemRect();
            if (fileOpen)
            {
                c.menuItem("New");
                newRect = c.itemRect();
                subOpen = c.beginSubMenu("Recent");
                recentRect = c.itemRect();
                if (subOpen)
                {
                    c.menuItem("a.txt");
                    if (c.menuItem("b.txt")) picked = true;
                    pickRect = c.itemRect();
                    c.endSubMenu();
                }
                c.menuItem("Exit");
                exitRect = c.itemRect();
                c.endMenu();
            }
            c.endMainMenuBar();
        }
    };

    harness.frame(build);
    harness.frame(build);
    harness.click(fileRect.x + 20.0f, fileRect.y + 10.0f, build);
    const float x = recentRect.x + 10.0f;
    context.pushEvent(ig::Event::pointerMove(x, recentRect.y + 5.0f));
    harness.frame(build);
    harness.frame(build);
    assert(subOpen);

    context.pushEvent(ig::Event::pointerMove(x, exitRect.y + 5.0f));
    harness.frame(build);
    harness.frame(build);
    assert(!subOpen);
    context.pushEvent(ig::Event::pointerMove(x, newRect.y + 5.0f));
    harness.frame(build);
    harness.frame(build);
    assert(!subOpen);

    context.pushEvent(ig::Event::pointerMove(x, recentRect.y + 5.0f));
    harness.frame(build);
    harness.frame(build);
    assert(subOpen);
    assert(pickRect.y >= exitRect.y);
    const float midX = (x + pickRect.x) * 0.5f + 20.0f;
    assert(midX < exitRect.x + exitRect.width);
    context.pushEvent(ig::Event::pointerMove(midX, exitRect.y + 5.0f));
    harness.frame(build);
    harness.frame(build);
    assert(subOpen);
    harness.click(pickRect.x + 10.0f, pickRect.y + 5.0f, build);
    assert(picked);
}

// A barra de menu principal responde em toda a altura dela e nas bordas do item.
void test_main_menu_bar_item_responds_to_its_edges()
{
    const float points[][2] = { { 3.0f, 13.0f }, { 12.0f, 2.0f }, { 12.0f, 24.0f },
        { 30.0f, 13.0f } };
    for (const auto& point: points)
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        bool fileOpen = false;
        auto build = [&](ig::Context& c)
        {
            if (c.beginMainMenuBar())
            {
                fileOpen = c.beginMenu("File");
                if (fileOpen)
                {
                    c.menuItem("New");
                    c.endMenu();
                }
                c.endMainMenuBar();
            }
        };
        harness.frame(build);
        harness.frame(build);
        assert(point[1] < context.mainMenuBarHeight());
        harness.click(point[0], point[1], build);
        harness.frame(build);
        assert(fileOpen);
    }
}

// A barra de rolagem de um beginCombo arrasta-se.
void test_combo_scrollbar_can_be_dragged()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    char names[14][4];
    ig::StringView items[14];
    for (int i = 0; i < 14; ++i)
    {
        snprintf(names[i], sizeof(names[i]), "%02d", i);
        items[i] = ig::StringView(names[i], 2);
    }
    ig::Rect header;
    ig::Rect first;
    bool open = false;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("combo", ig::Rect(0.0f, 0.0f, 320.0f, 400.0f)));
        open = c.beginCombo("escolha", items[0], 0.0f, 8);
        if (open)
        {
            for (int i = 0; i < 14; ++i)
            {
                c.selectable(items[i], i == 0);
                if (i == 0) first = c.itemRect();
            }
            c.endCombo();
        }
        else
            header = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    harness.click(header.x + header.width * 0.5f, header.y + header.height * 0.5f, build);
    for (int i = 0; i < 3; ++i) harness.frame(build);
    assert(open);
    const ig::Theme& theme = context.theme();
    const float bar = theme.scrollbarWidth > 4.0f ? theme.scrollbarWidth * 0.35f : 4.0f;
    const float barX = header.x + header.width - theme.windowPadding - bar * 0.5f;
    const float barY = header.y + header.height + theme.windowPadding + 3.0f;
    const float before = first.y;
    harness.drag(barX, barY, barX, barY + 150.0f, build, 6);
    harness.frame(build);
    assert(open);
    assert(first.y < before);
}

// Um beginCombo que rola, com conteúdo mais largo do que o cabeçalho, mostra o
// conteúdo inteiro antes da barra.
void test_wide_scrolling_combo_does_not_clip_its_content()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const char* wide = "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWW";
    ig::Rect header;
    ig::Rect wideRect;
    bool open = false;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("combo", ig::Rect(0.0f, 0.0f, 500.0f, 500.0f)));
        open = c.beginCombo("escolha", "x", 100.0f, 4);
        if (open)
        {
            c.label(wide);
            wideRect = c.itemRect();
            for (int i = 0; i < 10; ++i)
            {
                char text[16];
                snprintf(text, sizeof(text), "i%d", i);
                c.selectable(text, false);
            }
            c.endCombo();
        }
        else
            header = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    harness.click(header.x + 10.0f, header.y + header.height * 0.5f, build);
    for (int i = 0; i < 3; ++i) harness.frame(build);
    const ig::DrawData& data = harness.frame(build);
    assert(open);
    assert(wideRect.width > header.width);
    bool found = false;
    for (ig::Span<const ig::DrawCommand>::size_type i = 0u; i < data.commands.size(); ++i)
    {
        const ig::DrawCommand& command = data.commands[i];
        if (command.type != ig::DrawCommandType::Text || command.payload.text.textSize != 30u)
            continue;
        found = true;
        const ig::Rect clip = command.payload.text.clip;
        assert(clip.x + clip.width >= wideRect.x + wideRect.width - 0.01f);
    }
    assert(found);
}

// As linhas do listBox e do comboBox não são paragens do Tab: o Tab salta a lista
// inteira, as setas mexem nela, e depois de escolher uma linha do comboBox as setas
// continuam a mexer no combo.
void test_list_rows_are_not_tab_stops()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const ig::StringView items[] = { "zero", "one", "two", "three", "four" };
    int listCurrent = 0;
    int comboCurrent = 0;
    bool aFocused = false;
    bool bFocused = false;
    ig::Rect aRect;
    ig::Rect comboRect;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 600.0f)));
        c.button("A");
        aFocused = c.isItemFocused();
        aRect = c.itemRect();
        c.listBox("lista", listCurrent, ig::Span<const ig::StringView>(items, 5),
                ig::Rect(0.0f, 40.0f, 200.0f, c.theme().widgetHeight * 3.0f));
        c.button("B");
        bFocused = c.isItemFocused();
        c.comboBox("combo", comboCurrent, ig::Span<const ig::StringView>(items, 5),
                ig::Rect(0.0f, 200.0f, 200.0f, c.theme().widgetHeight));
        comboRect = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    harness.click(aRect.x + 5.0f, aRect.y + 5.0f, build);
    assert(aFocused);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Tab));
    harness.frame(build);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Down));
    harness.frame(build);
    assert(listCurrent == 1);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Tab));
    harness.frame(build);
    harness.frame(build);
    assert(bFocused);

    const ig::Rect header = comboRect;
    harness.click(header.x + 10.0f, header.y + 5.0f, build);
    harness.click(header.x + 10.0f, header.y + header.height * 2.5f, build);
    assert(comboCurrent == 1);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Down));
    harness.frame(build);
    assert(comboCurrent == 2);
}

// A lista de um comboBox com muitos itens fica dentro do ecrã e rola: a roda leva
// até ao último item.
void test_long_combo_box_list_stays_on_screen_and_scrolls()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    harness.setViewport(400.0f, 300.0f);

    char names[20][4];
    ig::StringView items[20];
    for (int i = 0; i < 20; ++i)
    {
        snprintf(names[i], sizeof(names[i]), "%02d", i);
        items[i] = ig::StringView(names[i], 2);
    }
    int current = 0;
    ig::Rect header;
    bool measured = false;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        c.comboBox("combo", current, ig::Span<const ig::StringView>(items, 20), 150.0f);
        if (!measured)
        {
            header = c.itemRect();
            measured = true;
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.click(header.x + 10.0f, header.y + 5.0f, build);
    const ig::DrawData& data = harness.frame(build);
    for (ig::Span<const ig::DrawCommand>::size_type i = 0u; i < data.commands.size(); ++i)
    {
        const ig::DrawCommand& command = data.commands[i];
        if (command.type != ig::DrawCommandType::Text) continue;
        const ig::TextCommand& text = command.payload.text;
        const bool insideClip =
                text.position.y >= text.clip.y && text.position.y < text.clip.y + text.clip.height;
        if (insideClip) assert(text.position.y + text.logicalSize <= 300.0f);
    }

    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    for (int i = 0; i < 30; ++i)
    {
        context.pushEvent(
                ig::Event::pointerMove(header.x + 10.0f, header.y + header.height * 2.5f));
        context.pushEvent(wheel);
        harness.frame(build);
    }
    float lastRowY = header.y + header.height;
    while (lastRowY + header.height * 2.0f <= 300.0f) lastRowY += header.height;
    harness.click(header.x + 10.0f, lastRowY + header.height * 0.5f, build);
    assert(current == 19);
}

// Um comboBox perto do fundo do ecrã abre a lista por cima, e a lista abre com a
// linha escolhida à vista.
void test_combo_box_opens_above_and_shows_the_current_item()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    harness.setViewport(400.0f, 300.0f);

    char names[20][4];
    ig::StringView items[20];
    for (int i = 0; i < 20; ++i)
    {
        snprintf(names[i], sizeof(names[i]), "%02d", i);
        items[i] = ig::StringView(names[i], 2);
    }
    int current = 15;
    int count = 5;
    ig::Rect header;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        c.comboBox("combo", current, ig::Span<const ig::StringView>(items, count),
                ig::Rect(0.0f, 220.0f, 150.0f, 28.0f));
        c.endWindow();
    };
    auto measure = [&](ig::Context& c)
    {
        build(c);
        header = c.itemRect();
    };

    current = 0;
    harness.frame(measure);
    harness.click(header.x + 10.0f, header.y + 5.0f, build);
    harness.click(header.x + 10.0f, header.y - header.height * 0.5f, build);
    assert(current == 4);

    count = 20;
    current = 15;
    harness.frame(measure);
    harness.click(header.x + 10.0f, header.y + 5.0f, build);
    harness.click(header.x + 10.0f, header.y - header.height * 0.5f, build);
    assert(current == 15);
}

// O popup de um menu da barra e o de um menu de contexto ficam dentro do ecrã.
void test_menu_popups_stay_on_screen()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    harness.setViewport(300.0f, 400.0f);

    bool helpOpen = false;
    bool contextOpen = false;
    int picked = 0;
    ig::Rect helpRect;
    ig::Rect aboutRect;
    ig::Rect contextItem;
    auto build = [&](ig::Context& c)
    {
        if (c.beginMainMenuBar())
        {
            const char* names[] = { "File", "Edit", "View" };
            for (const char* name: names)
                if (c.beginMenu(name)) c.endMenu();
            helpOpen = c.beginMenu("Help");
            if (!helpOpen) helpRect = c.itemRect();
            else
            {
                c.menuItem("About this program");
                aboutRect = c.itemRect();
                c.endMenu();
            }
            c.endMainMenuBar();
        }
        assert(c.beginWindow("janela", ig::Rect(0.0f, 40.0f, 300.0f, 360.0f)));
        contextOpen = c.beginContextMenu("ctx", ig::Rect(0.0f, 0.0f, 280.0f, 300.0f));
        if (contextOpen)
        {
            c.menuItem("Copy");
            if (c.menuItem("Paste")) ++picked;
            contextItem = c.itemRect();
            c.endContextMenu();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    harness.click(helpRect.x + 10.0f, helpRect.y + 10.0f, build);
    harness.frame(build);
    harness.frame(build);
    assert(helpOpen);
    assert(aboutRect.x >= 0.0f);
    assert(aboutRect.x + aboutRect.width <= 300.0f);
    harness.click(150.0f, 395.0f, build);
    assert(!helpOpen);

    harness.click(280.0f, 330.0f, build, ig::PointerButton::Right);
    harness.frame(build);
    harness.frame(build);
    assert(contextOpen);
    assert(contextItem.x >= 0.0f);
    assert(contextItem.x + contextItem.width <= 300.0f);
    assert(contextItem.y + contextItem.height <= 400.0f);
    harness.click(contextItem.x + 10.0f, contextItem.y + 5.0f, build);
    assert(picked == 1);
}

// O Ctrl+A de um bloco de escolha múltipla não dispara enquanto se escreve num
// campo de texto; fora dele seleciona tudo.
void test_multi_select_ctrl_a_waits_while_typing()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::String text("ola");
    bool selected[4] = { false, false, false, false };
    ig::Rect field;
    ig::Rect firstRow;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 400.0f)));
        c.inputText("nome", text, 200.0f);
        field = c.itemRect();
        c.beginMultiSelect(ig::Span<bool>(selected, 4));
        const char* names[4] = { "a", "b", "c", "d" };
        for (int i = 0; i < 4; ++i)
        {
            c.selectable(names[i], selected[i]);
            if (i == 0) firstRow = c.itemRect();
        }
        c.endMultiSelect();
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    harness.click(field.x + 20.0f, field.y + field.height * 0.5f, build);
    harness.frame(build);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::A, true, false));
    harness.frame(build);
    for (int i = 0; i < 4; ++i) assert(!selected[i]);

    harness.click(firstRow.x + 5.0f, firstRow.y + 5.0f, build);
    harness.frame(build);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::A, true, false));
    harness.frame(build);
    for (int i = 0; i < 4; ++i) assert(selected[i]);
}

// checkboxFlags com uma máscara de vários bits: marcado só com todos os bits, e um
// clique com só parte deles liga-os todos.
void test_checkbox_flags_with_a_multi_bit_mask()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    unsigned flags = 0x1u;
    unsigned other = 0x1u;
    ig::Rect box;
    ig::Rect otherBox;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 400.0f)));
        c.checkboxFlags("ambos", flags, 0x3u);
        box = c.itemRect();
        c.checkboxFlags("outros", other, 0x3u, ig::Rect(0.0f, 60.0f, 200.0f, 28.0f));
        otherBox = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    harness.click(box.x + 5.0f, box.y + box.height * 0.5f, build);
    assert(flags == 0x3u);
    harness.click(box.x + 5.0f, box.y + box.height * 0.5f, build);
    assert(flags == 0x0u);
    harness.click(otherBox.x + 5.0f, otherBox.y + otherBox.height * 0.5f, build);
    assert(other == 0x3u);
}

// Depois de um listBox, e de um comboBox aberto, as consultas do item falam do
// widget e não da última linha.
void test_list_widgets_report_their_own_item_status()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const ig::StringView items[] = { "zero", "one", "two", "three", "four" };
    int listCurrent = 0;
    int comboCurrent = 0;
    bool listHovered = false;
    bool listEdited = false;
    ig::Rect listRect;
    bool comboHovered = false;
    ig::Rect comboRect;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 600.0f)));
        c.listBox("lista", listCurrent, ig::Span<const ig::StringView>(items, 5),
                ig::Rect(0.0f, 0.0f, 200.0f, c.theme().widgetHeight * 3.0f));
        listHovered = c.isItemHovered();
        listEdited = c.isItemEdited();
        listRect = c.itemRect();
        c.comboBox("combo", comboCurrent, ig::Span<const ig::StringView>(items, 5),
                ig::Rect(0.0f, 200.0f, 200.0f, 28.0f));
        comboHovered = c.isItemHovered();
        comboRect = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    assert(listRect.height == context.theme().widgetHeight * 3.0f);
    context.pushEvent(ig::Event::pointerMove(listRect.x + 20.0f, listRect.y + 5.0f));
    harness.frame(build);
    assert(listHovered);
    harness.click(listRect.x + 20.0f, listRect.y + listRect.height * 0.5f, build);
    assert(listCurrent == 1);
    assert(listEdited);

    const ig::Rect header = comboRect;
    harness.click(header.x + 10.0f, header.y + 5.0f, build);
    harness.frame(build);
    assert(comboRect.y == header.y);
    assert(comboRect.height == header.height);
    context.pushEvent(ig::Event::pointerMove(header.x + 10.0f, header.y + 5.0f));
    harness.frame(build);
    assert(comboHovered);
}

// Um clique direito fora fecha o menu de contexto e o menu da barra.
void test_right_click_outside_closes_menus()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool fileOpen = false;
    bool contextOpen = false;
    ig::Rect fileRect;
    auto build = [&](ig::Context& c)
    {
        if (c.beginMainMenuBar())
        {
            fileOpen = c.beginMenu("File");
            if (!fileOpen) fileRect = c.itemRect();
            else
            {
                c.menuItem("New");
                c.endMenu();
            }
            c.endMainMenuBar();
        }
        assert(c.beginWindow("janela", ig::Rect(0.0f, 40.0f, 600.0f, 500.0f)));
        contextOpen = c.beginContextMenu("ctx", ig::Rect(0.0f, 0.0f, 580.0f, 400.0f));
        if (contextOpen)
        {
            c.menuItem("Copy");
            c.endContextMenu();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    harness.click(100.0f, 150.0f, build, ig::PointerButton::Right);
    harness.frame(build);
    assert(contextOpen);
    harness.click(500.0f, 400.0f, build, ig::PointerButton::Right);
    harness.frame(build);
    assert(!contextOpen);

    harness.click(fileRect.x + 10.0f, fileRect.y + 10.0f, build);
    harness.frame(build);
    assert(fileOpen);
    harness.click(500.0f, 400.0f, build, ig::PointerButton::Right);
    harness.frame(build);
    assert(!fileOpen);
}

// Um popup ou a lista de um combo que passam por cima de outra janela são da
// janela que os abriu: recebem o clique e não trazem a outra janela para a frente.
void test_popup_over_another_window_keeps_its_owner()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool openRequest = false;
    bool popupOpen = false;
    int clicks[4] = { 0, 0, 0, 0 };
    ig::Rect buttons[4];
    bool aFocused = false;
    int bClicks = 0;
    int current = 0;
    const ig::StringView items[6] = { "zero", "one", "two", "three", "four", "five" };
    ig::Rect comboRect;
    bool comboMeasured = false;
    auto build = [&](ig::Context& c)
    {
        if (c.beginWindow("B", ig::Rect(0.0f, 130.0f, 300.0f, 300.0f)))
        {
            if (c.button("bb")) ++bClicks;
            c.endWindow();
        }
        if (c.beginWindow("A", ig::Rect(0.0f, 0.0f, 300.0f, 120.0f)))
        {
            if (openRequest)
            {
                c.openPopup("P", ig::Vec2(20.0f, 80.0f));
                openRequest = false;
            }
            c.comboBox("cb", current, ig::Span<const ig::StringView>(items, 6),
                    ig::Rect(150.0f, 0.0f, 120.0f, 28.0f));
            if (!comboMeasured)
            {
                comboRect = c.itemRect();
                comboMeasured = true;
            }
            aFocused = c.isWindowFocused();
            popupOpen = c.beginPopup("P");
            if (popupOpen)
            {
                for (int i = 0; i < 4; ++i)
                {
                    char name[8];
                    snprintf(name, sizeof(name), "b%d", i);
                    if (c.button(name)) ++clicks[i];
                    buttons[i] = c.itemRect();
                }
                c.endPopup();
            }
            c.endWindow();
        }
    };

    harness.frame(build);
    openRequest = true;
    for (int i = 0; i < 3; ++i) harness.frame(build);
    assert(popupOpen);
    assert(buttons[3].y > 130.0f);
    const float x = buttons[3].x + buttons[3].width * 0.5f;
    const float y = buttons[3].y + buttons[3].height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(x, y));
    harness.frame(build);
    harness.click(x, y, build);
    harness.frame(build);
    assert(clicks[3] == 1);
    assert(aFocused);
    assert(bClicks == 0);

    harness.click(280.0f, 100.0f, build);
    harness.frame(build);
    assert(!popupOpen);
    harness.click(comboRect.x + 10.0f, comboRect.y + 5.0f, build);
    harness.frame(build);
    const float rowY = comboRect.y + comboRect.height * 5.5f;
    assert(rowY > 130.0f);
    harness.click(comboRect.x + 10.0f, rowY, build);
    harness.frame(build);
    assert(current == 4);
    assert(aFocused);
    assert(bClicks == 0);
}

// Com um popup modal aberto, o Tab anda só pelos campos dele, e um campo de trás
// que tinha o foco não recebe o que se escreve.
void test_modal_popup_keeps_the_keyboard()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::String back1("bg1");
    ig::String back2("bg2");
    ig::String first("m1");
    ig::String second("m2");
    bool openRequest = false;
    bool open = false;
    ig::Rect back1Rect;
    ig::Rect secondRect;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("A", ig::Rect(0.0f, 0.0f, 600.0f, 400.0f)));
        c.inputText("fundo1", back1);
        back1Rect = c.itemRect();
        c.inputText("fundo2", back2);
        if (openRequest)
        {
            c.openPopup("Modal");
            openRequest = false;
        }
        open = c.beginPopupModal("Modal");
        if (open)
        {
            c.inputText("modal1", first);
            c.inputText("modal2", second);
            secondRect = c.itemRect();
            c.endPopup();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.click(back1Rect.x + 10.0f, back1Rect.y + 5.0f, build);
    openRequest = true;
    for (int i = 0; i < 3; ++i) harness.frame(build);
    assert(open);
    context.pushEvent(ig::Event::textInput("Q"));
    harness.frame(build);
    assert(back1 == "bg1");

    harness.click(secondRect.x + 10.0f, secondRect.y + secondRect.height * 0.5f, build);
    harness.frame(build);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Tab));
    harness.frame(build);
    harness.frame(build);
    context.pushEvent(ig::Event::textInput("Z"));
    harness.frame(build);
    assert(back1 == "bg1");
    assert(back2 == "bg2");
    assert(first == "m1Z");
}

// O Tab anda pelos widgets da janela onde está o foco e volta ao princípio dela,
// sem saltar para outra janela.
void test_tab_stays_in_its_window()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::String a1("a1");
    ig::String a2("a2");
    ig::String b1("b1");
    ig::Rect a2Rect;
    auto build = [&](ig::Context& c)
    {
        if (c.beginWindow("A", ig::Rect(0.0f, 0.0f, 300.0f, 300.0f)))
        {
            c.inputText("fa1", a1);
            c.inputText("fa2", a2);
            a2Rect = c.itemRect();
            c.endWindow();
        }
        if (c.beginWindow("B", ig::Rect(400.0f, 0.0f, 300.0f, 300.0f)))
        {
            c.inputText("fb1", b1);
            c.endWindow();
        }
    };

    harness.frame(build);
    harness.frame(build);
    harness.click(a2Rect.x + 10.0f, a2Rect.y + a2Rect.height * 0.5f, build);
    harness.frame(build);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Tab));
    harness.frame(build);
    harness.frame(build);
    context.pushEvent(ig::Event::textInput("Z"));
    harness.frame(build);
    assert(b1 == "b1");
    assert(a1 == "a1Z");
}

// A ordem de desenho do primeiro vértice com esta cor (-1 se nenhum): a posição no
// fluxo de índices, comando a comando.
static int firstDrawWithColor(const ig::DrawData& data, const ig::Color& color)
{
    int position = 0;
    for (ig::Span<const ig::DrawCommand>::size_type i = 0u; i < data.commands.size(); ++i)
    {
        const ig::DrawCommand& command = data.commands[i];
        if (command.type != ig::DrawCommandType::Geometry) continue;
        const ig::GeometryCommand& geometry = command.payload.geometry;
        for (uint32_t index = 0u; index < geometry.indexCount; ++index, ++position)
        {
            const ig::DrawVertex& vertex =
                    data.vertices[static_cast<uint32_t>(geometry.vertexOffset) +
                                  data.indices[geometry.firstIndex + index]];
            if (vertex.color.r == color.r && vertex.color.g == color.g &&
                    vertex.color.b == color.b && vertex.color.a == color.a)
                return position;
        }
    }
    return -1;
}

// O scrim do beginDialog e o do beginPopupModal desenham-se: por cima do que está
// atrás e por baixo da caixa.
void test_dialog_and_modal_popup_scrims_are_drawn()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    ig::Theme theme = context.theme();
    theme.dialogScrim = ig::Color(7u, 8u, 9u, 100u);
    theme.windowBackground = ig::Color(21u, 22u, 23u, 255u);
    theme.menuBg = ig::Color(31u, 32u, 33u, 255u);
    context.setTheme(theme);

    bool dialogOpen = true;
    bool popupRequest = false;
    bool usePopup = false;
    auto build = [&](ig::Context& c)
    {
        if (c.beginWindow("fundo", ig::Rect(0.0f, 0.0f, 600.0f, 400.0f)))
        {
            c.label("fundo");
            if (popupRequest)
            {
                c.openPopup("Modal");
                popupRequest = false;
            }
            if (usePopup && c.beginPopupModal("Modal"))
            {
                c.label("modal");
                c.endPopup();
            }
            c.endWindow();
        }
        if (!usePopup && c.beginDialog("Dialogo", dialogOpen, ig::Vec2(300.0f, 200.0f)))
        {
            c.label("dialogo");
            c.endDialog();
        }
    };

    harness.frame(build);
    const ig::DrawData& dialogFrame = harness.frame(build);
    const int dialogScrim = firstDrawWithColor(dialogFrame, theme.dialogScrim);
    assert(dialogScrim >= 0);
    assert(firstDrawWithColor(dialogFrame, theme.windowBackground) < dialogScrim);

    usePopup = true;
    popupRequest = true;
    harness.frame(build);
    harness.frame(build);
    const ig::DrawData& popupFrame = harness.frame(build);
    const int popupScrim = firstDrawWithColor(popupFrame, theme.dialogScrim);
    const int box = firstDrawWithColor(popupFrame, theme.menuBg);
    assert(popupScrim >= 0);
    assert(box > popupScrim);
}

// O Escape fecha o combo ou o popup aberto dentro de um beginDialog, e só o
// seguinte fecha o diálogo.
void test_escape_closes_a_popup_inside_a_dialog_first()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool open = true;
    int current = 0;
    bool popupRequest = false;
    bool popupOpen = false;
    ig::Rect combo;
    const ig::StringView items[3] = { "a", "b", "c" };
    auto build = [&](ig::Context& c)
    {
        popupOpen = false;
        if (c.beginDialog("Dialogo", open, ig::Vec2(300.0f, 200.0f)))
        {
            c.comboBox("cb", current, ig::Span<const ig::StringView>(items, 3));
            combo = c.itemRect();
            if (popupRequest)
            {
                c.openPopup("pp");
                popupRequest = false;
            }
            popupOpen = c.beginPopup("pp");
            if (popupOpen)
            {
                c.label("popup");
                c.endPopup();
            }
            c.endDialog();
        }
    };

    harness.frame(build);
    harness.frame(build);
    harness.click(combo.x + 10.0f, combo.y + combo.height * 0.5f, build);
    harness.frame(build);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Escape));
    harness.frame(build);
    harness.frame(build);
    assert(open);

    popupRequest = true;
    harness.frame(build);
    harness.frame(build);
    assert(popupOpen);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Escape));
    harness.frame(build);
    harness.frame(build);
    assert(open);
    assert(!popupOpen);

    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Escape));
    harness.frame(build);
    assert(!open);
}

// Uma janela sem bool *open não tem X: o clique onde ele estaria não a fecha. Com
// o bool *open o X fecha-a.
void test_window_without_open_flag_has_no_close_button()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool shown = false;
    bool withFlag = false;
    bool open = true;
    auto build = [&](ig::Context& c)
    {
        shown = c.beginWindow("A", ig::Rect(100.0f, 100.0f, 300.0f, 200.0f),
                withFlag ? &open : nullptr);
        if (shown) c.endWindow();
    };

    context.setWindowButtons("A", false, false);
    harness.frame(build);
    const float title = context.theme().titleBarHeight;
    const float x = 100.0f + 300.0f - title * 0.5f;
    const float y = 100.0f + title * 0.5f;
    harness.click(x, y, build);
    harness.frame(build);
    assert(shown);

    withFlag = true;
    harness.frame(build);
    harness.click(x, y, build);
    harness.frame(build);
    assert(!open);
    assert(!shown);
}

// Com a barra de menu principal à vista, uma janela maximizada, as janelas em
// mosaico e a janela principal começam por baixo dela, e o X da maximizada fecha.
void test_maximized_window_stays_below_the_main_menu_bar()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool open = true;
    bool useMain = false;
    ig::Rect mainLabel;
    auto build = [&](ig::Context& c)
    {
        if (c.beginMainMenuBar())
        {
            if (c.beginMenu("File"))
            {
                c.menuItem("Quit");
                c.endMenu();
            }
            c.endMainMenuBar();
        }
        if (useMain)
        {
            if (c.beginMainWindow("principal"))
            {
                c.label("topo");
                mainLabel = c.itemRect();
                c.endWindow();
            }
            return;
        }
        if (c.beginWindow("W", ig::Rect(100.0f, 100.0f, 400.0f, 300.0f), &open))
        {
            c.label("x");
            c.endWindow();
        }
    };

    harness.frame(build);
    harness.frame(build);
    const float bar = context.mainMenuBarHeight();
    context.maximizeWindow("W");
    harness.frame(build);
    harness.frame(build);
    ig::Rect bounds = context.windowBounds("W");
    assert(bounds.y == bar);
    assert(bounds.y + bounds.height <= 800.0f);

    context.restoreWindow("W");
    context.tileAllWindows();
    harness.frame(build);
    bounds = context.windowBounds("W");
    assert(bounds.y >= bar);

    context.maximizeWindow("W");
    harness.frame(build);
    bounds = context.windowBounds("W");
    const float title = context.theme().titleBarHeight;
    harness.click(bounds.x + bounds.width - title * 0.5f, bounds.y + title * 0.5f, build);
    harness.frame(build);
    assert(!open);

    useMain = true;
    harness.frame(build);
    harness.frame(build);
    assert(mainLabel.y >= bar);
}

// Um child que sai de vista continua a ocupar o lugar dele: o que vem a seguir
// anda com a rolagem, sem saltar.
void test_child_out_of_view_keeps_its_place()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect first;
    bool innerShown = false;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("A", ig::Rect(0.0f, 0.0f, 300.0f, 400.0f)));
        if (c.beginChild("fora", 150.0f))
        {
            innerShown = c.beginChild("dentro", 60.0f);
            if (innerShown)
            {
                c.label("dentro");
                c.endChild();
            }
            for (int i = 0; i < 12; ++i)
            {
                char name[8];
                snprintf(name, sizeof(name), "b%d", i);
                c.button(name);
                if (i == 0) first = c.itemRect();
            }
            c.endChild();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    const float start = first.y;
    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    for (int step = 1; step <= 5; ++step)
    {
        context.pushEvent(ig::Event::pointerMove(100.0f, 100.0f));
        context.pushEvent(wheel);
        harness.frame(build);
        harness.frame(build);
        assert(first.y == start - context.theme().widgetHeight * static_cast<float>(step));
    }
    assert(!innerShown);
}

// Um child cuja última linha tem um item alto seguido de um sameLine baixo rola até
// ao fundo do item alto.
void test_child_measures_the_last_line()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect tall;
    ig::Rect child;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("A", ig::Rect(0.0f, 0.0f, 300.0f, 400.0f)));
        if (c.beginChild("ch", 120.0f))
        {
            for (int i = 0; i < 4; ++i)
            {
                char name[8];
                snprintf(name, sizeof(name), "b%d", i);
                c.button(name);
            }
            c.dummy(40.0f, 90.0f);
            tall = c.itemRect();
            c.sameLine();
            c.label("pequeno");
            c.endChild();
        }
        child = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    ig::Event wheel;
    wheel.type = ig::EventType::PointerWheel;
    wheel.wheelY = -1.0f;
    for (int i = 0; i < 30; ++i)
    {
        context.pushEvent(ig::Event::pointerMove(50.0f, 80.0f));
        context.pushEvent(wheel);
        harness.frame(build);
    }
    harness.frame(build);
    const float contentBottom = child.y + child.height - context.theme().padding;
    assert(tall.y + tall.height <= contentBottom + 0.01f);
}

// Um child dentro de um dock panel dá a largura dele, como numa janela simples.
void test_child_inside_a_dock_panel_reports_its_own_width()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float docked = 0.0f;
    float plain = 0.0f;
    auto build = [&](ig::Context& c)
    {
        if (c.beginWindow("A", ig::Rect(0.0f, 0.0f, 800.0f, 600.0f)))
        {
            if (c.beginDockSpace("ds"))
            {
                if (c.beginDockPanel("Painel", ig::DockSlot::Center))
                {
                    if (c.beginChild("filho", 100.0f, true, 150.0f))
                    {
                        docked = c.availableWidth();
                        c.endChild();
                    }
                    c.endDockPanel();
                }
                c.endDockSpace();
            }
            c.endWindow();
        }
        if (c.beginWindow("B", ig::Rect(0.0f, 0.0f, 800.0f, 600.0f)))
        {
            if (c.beginChild("filho", 100.0f, true, 150.0f))
            {
                plain = c.availableWidth();
                c.endChild();
            }
            c.endWindow();
        }
    };

    for (int i = 0; i < 3; ++i) harness.frame(build);
    assert(plain > 0.0f && plain < 150.0f);
    assert(docked == plain);
}

// Um indent, um grupo ou um setCursor não recomeçam a medida do conteúdo: um
// tooltip com uma linha longa no princípio fica com a largura dela.
void test_indent_group_and_set_cursor_keep_the_content_extent()
{
    float widths[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    for (int mode = 0; mode < 4; ++mode)
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Theme theme = context.theme();
        theme.tooltipBg = ig::Color(1u, 2u, 3u, 255u);
        context.setTheme(theme);
        auto build = [&](ig::Context& c)
        {
            assert(c.beginWindow("A", ig::Rect(0.0f, 0.0f, 600.0f, 400.0f)));
            c.button("passa aqui");
            if (c.beginTooltip())
            {
                c.label("Uma linha de tooltip bastante longa");
                if (mode == 1)
                {
                    c.indent(10.0f);
                    c.label("x");
                }
                else if (mode == 2)
                {
                    c.beginGroup();
                    c.label("x");
                    c.endGroup();
                }
                else if (mode == 3)
                {
                    c.setCursor(ig::Vec2(0.0f, 40.0f));
                    c.label("x");
                }
                else
                    c.label("x");
                c.endTooltip();
            }
            c.endWindow();
        };
        context.pushEvent(ig::Event::pointerMove(20.0f, 45.0f));
        const ig::DrawData* data = nullptr;
        for (int frame = 0; frame < 4; ++frame)
        {
            context.beginFrame(ig::FrameInfo(1280.0f, 800.0f, 1.0f, 0.5f));
            build(context);
            data = &context.endFrame();
        }
        float left = 1.0e9f;
        float right = -1.0e9f;
        for (ig::Span<const ig::DrawVertex>::size_type i = 0u; i < data->vertices.size(); ++i)
        {
            const ig::DrawVertex& vertex = data->vertices[i];
            if (vertex.color.r == 1u && vertex.color.g == 2u && vertex.color.b == 3u)
            {
                left = vertex.position.x < left ? vertex.position.x : left;
                right = vertex.position.x > right ? vertex.position.x : right;
            }
        }
        widths[mode] = right - left;
    }
    assert(widths[0] > 100.0f);
    for (int mode = 1; mode < 4; ++mode) assert(widths[mode] >= widths[0]);
}

// Um grupo com um child lá dentro tem a altura da caixa do child, não a do
// conteúdo escondido dele.
void test_group_with_a_child_ignores_the_hidden_content()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect child;
    ig::Rect group;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("A", ig::Rect(0.0f, 0.0f, 400.0f, 700.0f)));
        c.beginGroup();
        if (c.beginChild("ch", 100.0f, true, 150.0f))
        {
            for (int i = 0; i < 15; ++i)
            {
                char name[8];
                snprintf(name, sizeof(name), "b%d", i);
                c.button(name);
            }
            c.endChild();
        }
        child = c.itemRect();
        c.endGroup();
        group = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    assert(group.y == child.y);
    assert(group.y + group.height == child.y + child.height);
}

// newLine depois de um sameLine começa por baixo do item mais alto da linha.
void test_new_line_after_same_line_clears_the_tallest_item()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect tall;
    ig::Rect next;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("A", ig::Rect(0.0f, 0.0f, 400.0f, 400.0f)));
        c.dummy(40.0f, 90.0f);
        tall = c.itemRect();
        c.sameLine();
        c.label("baixo");
        c.sameLine();
        c.newLine();
        c.button("seguinte");
        next = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    assert(next.y == tall.y + tall.height + context.theme().itemSpacing);
}

// O separator e o separatorText vão até à borda do contentor onde estão (um child,
// um popup fora da janela), não até à da janela.
void test_separator_uses_the_available_width()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect childSeparator;
    ig::Rect childText;
    ig::Rect child;
    ig::Rect popupSeparator;
    bool request = false;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("A", ig::Rect(0.0f, 0.0f, 300.0f, 300.0f)));
        if (c.beginChild("ch", 100.0f, true, 120.0f))
        {
            c.label("filho");
            c.separator();
            childSeparator = c.itemRect();
            c.separatorText("titulo");
            childText = c.itemRect();
            c.endChild();
        }
        child = c.itemRect();
        if (request)
        {
            c.openPopup("P", ig::Vec2(350.0f, 50.0f));
            request = false;
        }
        if (c.beginPopup("P"))
        {
            c.label("item do popup");
            c.separator();
            popupSeparator = c.itemRect();
            c.endPopup();
        }
        c.endWindow();
    };

    harness.frame(build);
    request = true;
    for (int i = 0; i < 3; ++i) harness.frame(build);
    assert(childSeparator.x + childSeparator.width <= child.x + child.width);
    assert(childText.x + childText.width <= child.x + child.width);
    assert(popupSeparator.width > 0.0f);
}

// O setNextItemWidth antes de um beginChild é a largura do child, e não passa para
// o primeiro item de dentro.
void test_next_item_width_is_spent_by_the_child()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float first = 0.5f;
    float second = 0.5f;
    ig::Rect child;
    ig::Rect firstRect;
    ig::Rect secondRect;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("A", ig::Rect(0.0f, 0.0f, 500.0f, 400.0f)));
        c.setNextItemWidth(200.0f);
        if (c.beginChild("ch", 100.0f))
        {
            c.sliderFloat("primeiro", first, 0.0f, 1.0f);
            firstRect = c.itemRect();
            c.sliderFloat("segundo", second, 0.0f, 1.0f);
            secondRect = c.itemRect();
            c.endChild();
        }
        child = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    assert(child.width == 200.0f);
    assert(firstRect.width == secondRect.width);
    assert(firstRect.x + firstRect.width <= child.x + child.width);
}

// Dentro de um beginDisabled() nenhum widget muda o valor nem começa um arrasto:
// splitter, colorEdit, gizmos, gradiente, curva, sequencer, timeline e a fonte de
// um arrasto.
void test_disabled_widgets_do_not_edit()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Vec2 o;
    float split = 50.0f;
    ig::Color color(255u, 0u, 0u, 255u);
    ig::Transform2D t2;
    t2.position = ig::Vec2(100.0f, 300.0f);
    ct::Vector<ig::GradientStop> stops;
    stops.push_back(ig::GradientStop(0.0f, ig::Color(0u, 0u, 0u, 255u)));
    stops.push_back(ig::GradientStop(1.0f, ig::Color(255u, 255u, 255u, 255u)));
    int gradientSelected = -1;
    ct::Vector<ig::CurvePoint> points;
    points.push_back(ig::CurvePoint(0.0f, 0.0f));
    points.push_back(ig::CurvePoint(1.0f, 1.0f));
    int curveSelected = -1;
    ct::Vector<ig::SequencerTrack> sequence;
    sequence.push_back(ig::SequencerTrack("a", 10, 20, ig::Color(1u, 2u, 3u)));
    int sequenceFrame = 0;
    int sequenceTrack = -1;
    ct::Vector<ig::TimelineTrack> timeline(1);
    timeline[0].label = ig::String("k");
    int timelineFrame = 0;
    int timelineTrack = -1;
    int timelineKey = -1;
    ig::Transform3D t3;
    const float view[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -5, 1 };
    const float projection[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, -1, 0, 0, -0.2f, 0 };
    bool dragStarted = false;
    int page = 0;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 900.0f, 800.0f)));
        c.dummy(1.0f, 1.0f);
        o = ig::Vec2(c.itemRect().x, c.itemRect().y);
        c.beginDisabled();
        if (page == 0)
        {
            c.splitter("split", split, 0.0f, 200.0f, ig::SplitterAxis::Vertical,
                    ig::Rect(0.0f, 0.0f, 300.0f, 40.0f), 6.0f);
            c.colorEdit("cor", color, ig::Rect(0.0f, 50.0f, 300.0f, 200.0f));
            c.gizmo2D("g2", t2, ig::Gizmo2DMode::Translate, ig::Rect(0.0f, 0.0f, 300.0f, 600.0f));
            c.gradientEditor("grad", stops, gradientSelected,
                    ig::Rect(400.0f, 0.0f, 300.0f, 40.0f));
            dragStarted = c.beginDragSource(42u, 7u, 1u, "item",
                                  ig::Rect(400.0f, 300.0f, 120.0f, 28.0f)) ||
                          dragStarted;
        }
        else
        {
            c.curveEditor("curva", points, curveSelected, ig::Rect(0.0f, 0.0f, 200.0f, 200.0f),
                    ig::Vec2(0.0f, 0.0f), ig::Vec2(1.0f, 1.0f));
            c.sequencer("seq", sequence, 0, 99, sequenceFrame, sequenceTrack,
                    ig::Rect(0.0f, 220.0f, 500.0f, 100.0f));
            c.timeline("tl", timeline, 0, 100, timelineFrame, timelineTrack, timelineKey,
                    ig::Rect(0.0f, 340.0f, 500.0f, 100.0f));
            c.gizmo3D("g3", t3, ig::Gizmo3DMode::Translate, ig::Rect(500.0f, 0.0f, 300.0f, 300.0f),
                    view, projection);
        }
        c.endDisabled();
        c.endWindow();
    };

    harness.frame(build);
    harness.drag(o.x + 50.0f, o.y + 20.0f, o.x + 120.0f, o.y + 20.0f, build);
    harness.drag(o.x + 20.0f, o.y + 150.0f, o.x + 30.0f, o.y + 170.0f, build);
    harness.drag(o.x + 100.0f, o.y + 300.0f, o.x + 150.0f, o.y + 340.0f, build);
    harness.click(o.x + 550.0f, o.y + 15.0f, build);
    harness.drag(o.x + 420.0f, o.y + 310.0f, o.x + 600.0f, o.y + 450.0f, build, 5);
    assert(split == 50.0f);
    assert(color.r == 255u && color.g == 0u && color.b == 0u);
    assert(t2.position.x == 100.0f && t2.position.y == 300.0f);
    assert(stops.size() == 2u);
    assert(!dragStarted);

    page = 1;
    harness.frame(build);
    harness.click(o.x + 100.0f, o.y + 60.0f, build);
    harness.click(o.x + 320.0f, o.y + 230.0f, build);
    harness.click(o.x + 300.0f, o.y + 350.0f, build);
    harness.click(o.x + 300.0f, o.y + 374.0f, build);
    harness.drag(o.x + 650.0f, o.y + 150.0f, o.x + 700.0f, o.y + 120.0f, build);
    assert(points.size() == 2u);
    assert(sequenceFrame == 0);
    assert(timelineFrame == 0);
    assert(timeline[0].keys.size() == 0u);
    assert(t3.position.x == 0.0f && t3.position.y == 0.0f && t3.position.z == 0.0f);
}

// Arrastar uma paragem do gradiente ou um ponto da curva para lá do vizinho mexe só
// no que foi agarrado: o vizinho fica onde estava.
void test_dragging_past_a_neighbour_leaves_it_in_place()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Vec2 o;
    ct::Vector<ig::GradientStop> stops;
    stops.push_back(ig::GradientStop(0.0f, ig::Color(0u, 0u, 0u, 255u)));
    stops.push_back(ig::GradientStop(0.3f, ig::Color(255u, 0u, 0u, 255u)));
    stops.push_back(ig::GradientStop(0.5f, ig::Color(0u, 255u, 0u, 255u)));
    stops.push_back(ig::GradientStop(1.0f, ig::Color(255u, 255u, 255u, 255u)));
    int stop = -1;
    ct::Vector<ig::CurvePoint> points;
    points.push_back(ig::CurvePoint(0.0f, 0.0f));
    points.push_back(ig::CurvePoint(0.3f, 0.5f));
    points.push_back(ig::CurvePoint(0.5f, 0.5f));
    points.push_back(ig::CurvePoint(1.0f, 1.0f));
    int point = -1;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 900.0f, 700.0f)));
        c.dummy(1.0f, 1.0f);
        o = ig::Vec2(c.itemRect().x, c.itemRect().y);
        c.gradientEditor("grad", stops, stop, ig::Rect(0.0f, 0.0f, 412.0f, 40.0f));
        c.curveEditor("curva", points, point, ig::Rect(0.0f, 100.0f, 412.0f, 412.0f),
                ig::Vec2(0.0f, 0.0f), ig::Vec2(1.0f, 1.0f));
        c.endWindow();
    };

    harness.frame(build);
    harness.drag(o.x + 6.0f + 0.3f * 400.0f, o.y + 33.0f, o.x + 6.0f + 0.8f * 400.0f, o.y + 33.0f,
            build, 5);
    harness.frame(build);
    assert(stops.size() == 4u);
    assert(stops[1].color.g == 255u && fabsf(stops[1].position - 0.5f) < 0.001f);
    assert(stops[2].color.r == 255u && stops[2].color.g == 0u &&
            fabsf(stops[2].position - 0.8f) < 0.001f);

    harness.drag(o.x + 6.0f + 0.3f * 400.0f, o.y + 306.0f, o.x + 6.0f + 0.8f * 400.0f, o.y + 306.0f,
            build, 5);
    harness.frame(build);
    assert(points.size() == 4u);
    assert(fabsf(points[1].x - 0.5f) < 0.001f);
    assert(fabsf(points[2].x - 0.8f) < 0.001f);
}

// Um showToast com o id de um toast que ainda está à vista atualiza-o em vez de
// juntar outro.
void test_toast_with_the_same_id_is_updated()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const char* text = "Guardado";
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        c.showToast("gravar", text);
        c.endWindow();
    };
    auto count = [](const ig::DrawData& data, const char* wanted)
    {
        const size_t size = strlen(wanted);
        int found = 0;
        for (ig::Span<const ig::DrawCommand>::size_type i = 0u; i < data.commands.size(); ++i)
        {
            const ig::DrawCommand& command = data.commands[i];
            if (command.type == ig::DrawCommandType::Text &&
                    command.payload.text.textSize == size &&
                    memcmp(data.textBytes.data() + command.payload.text.textOffset, wanted, size) ==
                            0)
                ++found;
        }
        return found;
    };

    for (int i = 0; i < 3; ++i) harness.frame(build);
    assert(count(harness.frame(build), "Guardado") == 1);
    text = "Guardado de novo";
    const ig::DrawData& data = harness.frame(build);
    assert(count(data, "Guardado de novo") == 1);
    assert(count(data, "Guardado") == 0);
}

// textWrapped com espaços antes de um \n no ponto do corte não acrescenta uma linha
// vazia: ocupa o mesmo que o texto sem esses espaços.
void test_wrapped_text_does_not_add_a_line_for_spaces_before_a_newline()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect withSpaces;
    ig::Rect withoutSpaces;
    ig::Rect bullet;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 400.0f)));
        const float width = c.theme().fontSize * 0.5f * 4.0f;
        c.textWrapped("aaaa \nbb", width);
        withSpaces = c.itemRect();
        c.textWrapped("aaaa\nbb", width);
        withoutSpaces = c.itemRect();
        c.bulletText("aaaa  \nbb", width);
        bullet = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    assert(withoutSpaces.height > 0.0f);
    assert(withSpaces.height == withoutSpaces.height);
    assert(bullet.height == withoutSpaces.height);
}

// A fonte de um arrasto não tira o press ao item a que pertence: depois de um
// button o arrasto começa, e antes de um selectable o clique ainda chega a ele.
void test_drag_source_leaves_the_press_to_its_item()
{
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        ig::Rect item;
        bool dragging = false;
        auto build = [&](ig::Context& c)
        {
            assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
            c.button("item", ig::Rect(0.0f, 0.0f, 120.0f, 28.0f));
            item = c.itemRect();
            dragging =
                    c.beginDragSource(42u, 7u, 1u, "item", ig::Rect(0.0f, 0.0f, 120.0f, 28.0f)) ||
                    dragging;
            c.endWindow();
        };
        harness.frame(build);
        harness.drag(item.x + 20.0f, item.y + 10.0f, item.x + 200.0f, item.y + 150.0f, build, 6);
        assert(dragging);
    }
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        ig::Rect item;
        bool clicked = false;
        auto build = [&](ig::Context& c)
        {
            assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
            c.beginDragSource(42u, 7u, 1u, "item", ig::Rect(0.0f, 0.0f, 120.0f, 28.0f));
            if (c.selectable("item", false, ig::Rect(0.0f, 0.0f, 120.0f, 28.0f))) clicked = true;
            item = c.itemRect();
            c.endWindow();
        };
        harness.frame(build);
        harness.click(item.x + 20.0f, item.y + 10.0f, build);
        assert(clicked);
    }
}

// Largar sobre uma janela que está por cima de um alvo não entrega o arrasto ao alvo
// de trás.
void test_drop_target_behind_another_window_does_not_accept()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool backDropped = false;
    bool frontDropped = false;
    auto build = [&](ig::Context& c)
    {
        if (c.beginWindow("fonte", ig::Rect(0.0f, 0.0f, 200.0f, 100.0f)))
        {
            c.beginDragSource(42u, 7u, 1u, "item", ig::Rect(0.0f, 0.0f, 120.0f, 28.0f));
            c.endWindow();
        }
        if (c.beginWindow("tras", ig::Rect(300.0f, 0.0f, 300.0f, 300.0f)))
        {
            ig::DragDropPayload payload;
            if (c.acceptDragDropTarget(7u, ig::Rect(0.0f, 0.0f, 280.0f, 260.0f), payload))
                backDropped = true;
            c.endWindow();
        }
        if (c.beginWindow("frente", ig::Rect(350.0f, 50.0f, 200.0f, 200.0f)))
        {
            ig::DragDropPayload payload;
            if (c.acceptDragDropTarget(7u, ig::Rect(0.0f, 0.0f, 100.0f, 40.0f), payload))
                frontDropped = true;
            c.endWindow();
        }
    };

    harness.frame(build);
    harness.click(450.0f, 200.0f, build);
    harness.drag(20.0f, 40.0f, 450.0f, 150.0f, build, 6);
    assert(!backDropped);
    assert(!frontDropped);

    harness.drag(20.0f, 40.0f, 320.0f, 280.0f, build, 6);
    assert(backDropped);
}

// Largar um treeItem sobre uma janela que tapa outro treeItem não o entrega ao de
// trás.
void test_tree_drop_behind_another_window_does_not_accept()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::TreeDrop drop;
    ig::Rect source;
    ig::Rect target;
    auto build = [&](ig::Context& c)
    {
        bool expanded = false;
        ig::TreeItemStyle style;
        style.leaf = true;
        if (c.beginWindow("fonte", ig::Rect(400.0f, 0.0f, 300.0f, 200.0f)))
        {
            ig::TreeDrop ignored;
            c.treeItem(1u, "um", expanded, style, ig::Rect(0.0f, 0.0f, 260.0f, 28.0f), &ignored);
            source = c.itemRect();
            c.endWindow();
        }
        if (c.beginWindow("arvore", ig::Rect(0.0f, 0.0f, 300.0f, 300.0f)))
        {
            c.treeItem(2u, "dois", expanded, style, ig::Rect(0.0f, 120.0f, 260.0f, 28.0f), &drop);
            target = c.itemRect();
            c.endWindow();
        }
        if (c.beginWindow("tampa", ig::Rect(100.0f, 130.0f, 200.0f, 120.0f)))
        {
            c.label("tampa");
            c.endWindow();
        }
    };

    harness.frame(build);
    harness.click(250.0f, 240.0f, build);
    const float y = target.y + target.height * 0.5f;
    harness.drag(source.x + 20.0f, source.y + 10.0f, target.x + 150.0f, y, build, 6);
    harness.frame(build);
    assert(drop.target == ig::InvalidWidgetId);

    harness.drag(source.x + 20.0f, source.y + 10.0f, target.x + 40.0f, y, build, 6);
    harness.frame(build);
    assert(drop.target == 2u);
}

// O Enter que aceita um messageBox não clica também o botão com foco, e o Escape que
// o cancela não fecha também um diálogo submetido depois.
void test_message_box_keys_are_spent_by_the_box()
{
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        bool open = false;
        int fires = 0;
        int accepted = 0;
        ig::Rect button;
        auto build = [&](ig::Context& c)
        {
            if (c.messageBox("Apagar", "Apagar mesmo?", open, true) ==
                    ig::MessageBoxResult::Accepted)
                ++accepted;
            assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
            if (c.button("Apagar", ig::Rect(0.0f, 0.0f, 120.0f, 28.0f)))
            {
                ++fires;
                open = true;
            }
            button = c.itemRect();
            c.endWindow();
        };
        harness.frame(build);
        harness.click(button.x + 20.0f, button.y + 10.0f, build);
        harness.frame(build);
        harness.frame(build);
        assert(fires == 1 && open);
        context.pushEvent(ig::Event::keyDown(ig::KeyCode::Enter));
        harness.frame(build);
        harness.frame(build);
        assert(accepted == 1);
        assert(fires == 1);
        assert(!open);
    }
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        bool boxOpen = true;
        bool dialogOpen = true;
        auto build = [&](ig::Context& c)
        {
            c.messageBox("Aviso", "Continuar?", boxOpen, true);
            if (c.beginDialog("Dialogo", dialogOpen, ig::Vec2(300.0f, 200.0f)))
            {
                c.label("dialogo");
                c.endDialog();
            }
        };
        harness.frame(build);
        harness.frame(build);
        context.pushEvent(ig::Event::keyDown(ig::KeyCode::Escape));
        harness.frame(build);
        assert(!boxOpen);
        assert(dialogOpen);
    }
}

// Encolher a escala com snap pára no menor passo, não em 0: no gizmo2D e no gizmo3D.
void test_gizmo_scale_snap_never_reaches_zero()
{
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        ig::Vec2 o;
        ig::Transform2D transform;
        transform.position = ig::Vec2(150.0f, 150.0f);
        ig::Gizmo2DOptions options;
        options.scaleSnap = 0.5f;
        auto build = [&](ig::Context& c)
        {
            assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 400.0f)));
            c.dummy(1.0f, 1.0f);
            o = ig::Vec2(c.itemRect().x, c.itemRect().y);
            c.gizmo2D("g", transform, ig::Gizmo2DMode::Scale, ig::Rect(0.0f, 0.0f, 300.0f, 300.0f),
                    options);
            c.endWindow();
        };
        harness.frame(build);
        harness.drag(o.x + 205.0f, o.y + 150.0f, o.x + 135.0f, o.y + 150.0f, build, 4);
        assert(transform.scale.x == 0.5f);
    }
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        ig::Vec2 o;
        ig::Transform3D transform;
        const float view[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -5, 1 };
        const float projection[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, -1, 0, 0, -0.2f, 0 };
        ig::Gizmo3DOptions options;
        options.showModel = false;
        options.scaleSnap = 0.5f;
        auto build = [&](ig::Context& c)
        {
            assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
            c.dummy(1.0f, 1.0f);
            o = ig::Vec2(c.itemRect().x, c.itemRect().y);
            c.gizmo3D("g", transform, ig::Gizmo3DMode::Scale, ig::Rect(0.0f, 0.0f, 500.0f, 500.0f),
                    view, projection, options);
            c.endWindow();
        };
        harness.frame(build);
        harness.drag(o.x + 250.0f, o.y + 250.0f, o.x, o.y + 260.0f, build, 4);
        assert(transform.scale.x == 0.5f && transform.scale.y == 0.5f && transform.scale.z == 0.5f);
    }
}

// O punho do plano XZ do gizmo3D move no plano o ponto que está debaixo do ponteiro:
// para baixo no ecrã (o +Z desta câmara) muda o Z, para a direita muda o X, e o Y
// fica. A projeção tem o plano de longe no infinito.
void test_gizmo3d_plane_handle_follows_the_pointer()
{
    const float a = 0.70710678f;
    const float view[16] = { 1, 0, 0, 0, 0, a, a, 0, 0, -a, a, 0, 0, 0, -4.95f, 1 };
    const float projection[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, -1, 0, 0, -0.2f, 0 };
    const auto project = [&](const ig::Rect& viewport, float x, float y, float z)
    {
        const float vx = view[0] * x + view[4] * y + view[8] * z + view[12];
        const float vy = view[1] * x + view[5] * y + view[9] * z + view[13];
        const float vz = view[2] * x + view[6] * y + view[10] * z + view[14];
        const float w = projection[11] * vz + projection[15];
        return ig::Vec2(viewport.x + (projection[0] * vx / w * 0.5f + 0.5f) * viewport.width,
                viewport.y + (0.5f - projection[5] * vy / w * 0.5f) * viewport.height);
    };
    for (int direction = 0; direction < 2; ++direction)
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        ig::Vec2 o;
        ig::Transform3D transform;
        ig::Gizmo3DOptions options;
        options.showModel = false;
        auto build = [&](ig::Context& c)
        {
            assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
            c.dummy(1.0f, 1.0f);
            o = ig::Vec2(c.itemRect().x, c.itemRect().y);
            c.gizmo3D("g", transform, ig::Gizmo3DMode::Translate,
                    ig::Rect(0.0f, 0.0f, 500.0f, 500.0f), view, projection, options);
            c.endWindow();
        };
        harness.frame(build);
        const ig::Vec2 handle = project(ig::Rect(o.x, o.y, 500.0f, 500.0f), 0.45f, 0.0f, 0.45f);
        if (direction == 0)
        {
            harness.drag(handle.x, handle.y, handle.x, handle.y + 40.0f, build, 4);
            assert(transform.position.z > 0.5f);
            assert(fabsf(transform.position.x) < 0.1f);
        }
        else
        {
            harness.drag(handle.x, handle.y, handle.x + 40.0f, handle.y, build, 4);
            assert(transform.position.x > 0.5f);
            assert(fabsf(transform.position.z) < 0.01f);
        }
        assert(transform.position.y == 0.0f);
    }
}

// A rotação dos gizmos acompanha o ponteiro para lá de meia volta, sem saltar 360°.
void test_gizmo_rotation_does_not_jump_a_full_turn()
{
    const float pi = 3.14159265f;
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        ig::Vec2 o;
        ig::Transform2D transform;
        transform.position = ig::Vec2(150.0f, 150.0f);
        auto build = [&](ig::Context& c)
        {
            assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 400.0f)));
            c.dummy(1.0f, 1.0f);
            o = ig::Vec2(c.itemRect().x, c.itemRect().y);
            c.gizmo2D("g", transform, ig::Gizmo2DMode::Rotate,
                    ig::Rect(0.0f, 0.0f, 300.0f, 300.0f));
            c.endWindow();
        };
        harness.frame(build);
        const float r = 60.0f * 0.82f;
        const float cx = o.x + 150.0f;
        const float cy = o.y + 150.0f;
        const float start = 170.0f * pi / 180.0f;
        context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, cx + cosf(start) * r,
                cy + sinf(start) * r));
        harness.frame(build);
        for (int degrees = 175; degrees <= 200; degrees += 5)
        {
            const float angle = static_cast<float>(degrees) * pi / 180.0f;
            context.pushEvent(ig::Event::pointerMove(cx + cosf(angle) * r, cy + sinf(angle) * r));
            harness.frame(build);
            assert(fabsf(transform.rotation - static_cast<float>(degrees - 170)) < 0.5f);
        }
        context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, cx, cy));
        harness.frame(build);
    }
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        ig::Vec2 o;
        ig::Transform3D transform;
        const float view[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -5, 1 };
        const float projection[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, -1, 0, 0, -0.2f, 0 };
        ig::Gizmo3DOptions options;
        options.showModel = false;
        auto build = [&](ig::Context& c)
        {
            assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 400.0f)));
            c.dummy(1.0f, 1.0f);
            o = ig::Vec2(c.itemRect().x, c.itemRect().y);
            c.gizmo3D("g", transform, ig::Gizmo3DMode::Rotate, ig::Rect(0.0f, 0.0f, 300.0f, 300.0f),
                    view, projection, options);
            c.endWindow();
        };
        harness.frame(build);
        const float cx = o.x + 150.0f;
        const float cy = o.y + 150.0f;
        const float r = 30.0f;
        context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, cx + r, cy));
        harness.frame(build);
        float previous = 0.0f;
        for (int degrees = 30; degrees <= 240; degrees += 30)
        {
            const float angle = static_cast<float>(degrees) * pi / 180.0f;
            context.pushEvent(ig::Event::pointerMove(cx + cosf(angle) * r, cy + sinf(angle) * r));
            harness.frame(build);
            assert(transform.rotation.z < previous);
            assert(fabsf(transform.rotation.z - previous) < 60.0f);
            previous = transform.rotation.z;
        }
        assert(fabsf(transform.rotation.z + 240.0f) < 1.0f);
    }
}

// O colorEdit só devolve true quando a cor muda: arrastar o matiz de um cinzento não
// muda a cor, e não conta.
void test_color_edit_reports_only_real_changes()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Color color(128u, 128u, 128u, 255u);
    int changes = 0;
    int real = 0;
    ig::Vec2 o;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 600.0f, 400.0f)));
        c.dummy(1.0f, 1.0f);
        o = ig::Vec2(c.itemRect().x, c.itemRect().y);
        const ig::Color before = color;
        if (c.colorEdit("cor", color, ig::Rect(0.0f, 0.0f, 400.0f, 260.0f)))
        {
            ++changes;
            if (before != color) ++real;
        }
        c.endWindow();
    };

    harness.frame(build);
    const ig::Theme& theme = context.theme();
    const float contentY = 16.0f + theme.itemSpacing;
    const float available = 260.0f - contentY - theme.windowPadding * 0.5f;
    const float picker = available < 193.6f ? available : 193.6f;
    const float hueX = o.x + picker + theme.windowPadding + 7.0f;
    const float y = o.y + contentY + 10.0f;
    harness.drag(hueX, y, hueX, y + 100.0f, build, 6);
    assert(color.r == 128u && color.g == 128u && color.b == 128u);
    assert(changes == 0);

    harness.drag(o.x + 10.0f, y, o.x + picker - 10.0f, y, build, 4);
    assert(real > 0);
    assert(changes == real);
}

// Um currentItem fora do intervalo (-1 = nada escolhido) fica como está: a
// listBox e o comboBox não o põem a 0 às escondidas. Só muda quando o
// utilizador escolhe (com as setas, Up vai para o último e Down para o
// primeiro), e aí devolvem true.
void test_out_of_range_selection_is_left_alone()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const ig::StringView items[] = { "zero", "one", "two", "three", "four" };
    int listCurrent = -1;
    int comboCurrent = 7;
    bool listChanged = false;
    bool comboChanged = false;
    ig::Rect aRect;
    ig::Rect comboRect;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 600.0f)));
        c.button("A");
        aRect = c.itemRect();
        listChanged = c.listBox("lista", listCurrent, ig::Span<const ig::StringView>(items, 5),
                ig::Rect(0.0f, 40.0f, 200.0f, c.theme().widgetHeight * 3.0f));
        comboChanged = c.comboBox("combo", comboCurrent, ig::Span<const ig::StringView>(items, 5),
                ig::Rect(0.0f, 200.0f, 200.0f, c.theme().widgetHeight));
        comboRect = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    assert(listCurrent == -1 && !listChanged);
    assert(comboCurrent == 7 && !comboChanged);

    harness.click(comboRect.x + 10.0f, comboRect.y + 5.0f, build);
    assert(comboCurrent == 7);
    harness.click(comboRect.x + 10.0f, comboRect.y + comboRect.height * 2.5f, build);
    assert(comboCurrent == 1);

    harness.click(aRect.x + 5.0f, aRect.y + 5.0f, build);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Tab));
    harness.frame(build);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Up));
    harness.frame(build);
    assert(listCurrent == 4 && listChanged);

    // Uma tab tem sempre de estar ativa: o tabBar acerta o valor e diz que mudou.
    int tab = 9;
    bool tabChanged = false;
    harness.frame(
            [&](ig::Context& c)
            {
                assert(c.beginWindow("tabs", ig::Rect(0.0f, 0.0f, 400.0f, 100.0f)));
                tabChanged = c.tabBar("barra", tab, ig::Span<const ig::StringView>(items, 3),
                        ig::Rect(0.0f, 0.0f, 300.0f, c.theme().widgetHeight));
                c.endWindow();
            });
    assert(tab == 0 && tabChanged);
}

// Um menuItem desativado também passa a ser o último item: o itemRect é o dele
// e o isItemDisabled diz que está desativado (antes ficava o item anterior).
void test_disabled_menu_item_is_the_last_item()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect barButton;
    ig::Rect first;
    ig::Rect disabled;
    bool disabledFlag = false;
    bool disabledHovered = true;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("menus", ig::Rect(0.0f, 0.0f, 320.0f, 260.0f)));
        if (c.beginMenuBar(ig::Rect(0.0f, 0.0f, c.availableWidth(), 26.0f)))
        {
            const bool open = c.beginMenu("Edit");
            barButton = c.itemRect();
            if (open)
            {
                c.menuItem("Copiar");
                first = c.itemRect();
                c.menuItem("Cortar", false);
                disabled = c.itemRect();
                disabledFlag = c.isItemDisabled();
                disabledHovered = c.isItemHovered();
                c.endMenu();
            }
            c.endMenuBar();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.click(barButton.x + 5.0f, barButton.y + 5.0f, build);
    assert(first.height > 0.0f);
    context.pushEvent(
            ig::Event::pointerMove(disabled.x + 10.0f, disabled.y + disabled.height * 0.5f));
    harness.frame(build);
    assert(disabled.y == first.y + first.height);
    assert(disabledFlag);
    assert(!disabledHovered);
}

// Remover, a meio de outra janela, a janela que está debaixo do ponteiro passa
// logo o hover à de baixo (o ponteiro ficava preso à removida até ao próximo
// beginWindow).
void test_removed_window_lets_go_of_the_pointer()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool removeTop = false;
    bool belowHovered = false;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("baixo", ig::Rect(0.0f, 0.0f, 300.0f, 200.0f)));
        c.button("antes", ig::Rect(0.0f, 120.0f, 50.0f, 20.0f));
        if (removeTop) c.removeWindow("cima");
        c.button("alvo", ig::Rect(0.0f, 0.0f, 200.0f, 100.0f));
        belowHovered = c.isItemHovered();
        c.endWindow();
        if (!removeTop && c.beginWindow("cima", ig::Rect(0.0f, 0.0f, 300.0f, 200.0f)))
        {
            c.button("tampa", ig::Rect(0.0f, 0.0f, 200.0f, 100.0f));
            c.endWindow();
        }
    };

    context.pushEvent(ig::Event::pointerMove(60.0f, 60.0f));
    harness.frame(build);
    harness.frame(build);
    assert(!belowHovered);
    removeTop = true;
    harness.frame(build);
    assert(belowHovered);
}

// O availableHeight conhece onde o conteúdo está: num popup é o espaço até ao
// fundo do ecrã (e não o que sobra da janela que o abriu), no conteúdo de um
// combo é a lista visível, numa tabela com scrollY é o que sobra da caixa da
// tabela, e numa tab é o mesmo que a janela.
void test_available_height_follows_the_container()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    harness.setViewport(400.0f, 300.0f);

    float inWindow = -1.0f;
    float inTab = -1.0f;
    float inPopup = -1.0f;
    float popupTop = -1.0f;
    float inTable = -1.0f;
    float tableTop = -1.0f;
    bool openPopup = false;
    float inCombo = -1.0f;
    ig::Rect comboHeader;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("altura", ig::Rect(0.0f, 0.0f, 300.0f, 120.0f)));
        if (c.beginTabBar("abas"))
        {
            if (c.beginTabItem("uma"))
            {
                inTab = c.availableHeight();
                const bool comboOpen = c.beginCombo("escolha", "x", 120.0f);
                comboHeader = c.itemRect();
                if (comboOpen)
                {
                    inCombo = c.availableHeight();
                    for (int i = 0; i < 20; ++i) c.label("linha");
                    c.endCombo();
                }
                c.endTabItem();
            }
            c.endTabBar();
        }
        c.endWindow();
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 130.0f, 300.0f, 160.0f)));
        inWindow = c.availableHeight();
        ig::TableStyle style;
        style.scrollY = true;
        style.height = 80.0f;
        if (c.beginTable("t", 2, style))
        {
            c.tableNextColumn();
            inTable = c.availableHeight();
            c.label("x");
            tableTop = c.itemRect().y;
            c.endTable();
        }
        if (openPopup)
        {
            c.openPopup("menu", ig::Vec2(10.0f, 150.0f));
            openPopup = false;
        }
        if (c.beginPopup("menu"))
        {
            inPopup = c.availableHeight();
            c.label("item");
            popupTop = c.itemRect().y;
            c.endPopup();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    assert(inTab > 0.0f);
    harness.click(comboHeader.x + 5.0f, comboHeader.y + 5.0f, build);
    harness.frame(build);
    assert(inCombo >= 4.0f * context.theme().widgetHeight);
    assert(inCombo <= 8.0f * (context.theme().widgetHeight + context.theme().itemSpacing));
    harness.click(390.0f, 5.0f, build);
    assert(inTable > 0.0f && inTable <= 80.0f);
    assert(tableTop + inTable <= 130.0f + 160.0f);
    openPopup = true;
    harness.frame(build);
    harness.frame(build);
    assert(popupTop >= 150.0f);
    assert(inPopup > 0.0f);
    const float gap = popupTop + inPopup - 300.0f;
    assert(gap > -1.0f && gap < 1.0f);
}

// As áreas de scroll que a roda conhece são só as do frame anterior: uma área
// que deixou de ser submetida sai, e uma interface com ids que mudam não faz a
// lista crescer sem fim.
void test_wheel_areas_forget_what_is_gone()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    int generation = 0;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("rolar", ig::Rect(0.0f, 0.0f, 300.0f, 200.0f)));
        char id[32];
        snprintf(id, sizeof(id), "area%d", generation);
        if (c.beginChild(id, 80.0f))
        {
            for (int i = 0; i < 20; ++i) c.label("linha");
        }
        c.endChild();
        c.endWindow();
    };

    context.pushEvent(ig::Event::pointerMove(50.0f, 60.0f));
    harness.frame(build);
    harness.frame(build);
    const size_t steady = context.wheelAreaCount();
    assert(steady > 0u);
    for (generation = 1; generation < 50; ++generation)
        for (int frame = 0; frame < 3; ++frame) harness.frame(build);
    assert(context.wheelAreaCount() <= steady + 1u);
}

// Numa tabela SizingFixedFit, arrastar a borda de uma coluna muda a largura
// dessa coluna (a medida do conteúdo deixa de mandar nela) e as seguintes andam
// com ela; a largura escolhida fica de um frame para o outro.
void test_table_sizing_fixed_fit_resizes()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float weights[] = { 1.0f, 1.0f, 1.0f };
    ig::Rect header[3];
    ig::Rect cell[3];
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("tabela redimensiona", ig::Rect(0.0f, 0.0f, 480.0f, 300.0f)));
        ig::TableStyle style;
        style.sizingFixedFit = true;
        if (c.beginTable("fit", ig::Span<float>(weights, 3), style, 0.0f))
        {
            for (int column = 0; column < 3; ++column)
            {
                c.tableNextColumn();
                c.tableHeader(column == 0 ? "um" : (column == 1 ? "dois" : "tres"));
                header[column] = c.itemRect();
            }
            for (int column = 0; column < 3; ++column)
            {
                c.tableNextColumn();
                c.dummy(40.0f, 16.0f);
                cell[column] = c.itemRect();
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    harness.frame(build);
    const float before = cell[1].x - cell[0].x;
    const float secondBefore = cell[2].x - cell[1].x;
    const float edgeX = header[0].x + header[0].width;
    const float edgeY = header[0].y + header[0].height * 0.5f;
    harness.drag(edgeX, edgeY, edgeX + 50.0f, edgeY, build, 4);
    harness.frame(build);
    harness.frame(build);
    assert(cell[1].x - cell[0].x == before + 50.0f);
    assert(cell[2].x - cell[1].x == secondBefore);
}

// Largar um reorder muda a ordem no frame seguinte: no frame do largar a
// linha toda (os cabeçalhos e as células) usa ainda a ordem antiga, e nenhuma
// coluna aparece duas vezes.
void test_table_reorder_applies_on_the_next_frame()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[3] = { 1.0f, 1.0f, 1.0f };
    ig::TableStyle style;
    style.reorderable = true;
    ig::Rect headerRects[3];
    int headerOrder[3] = { -1, -1, -1 };
    int cellOrder[3] = { -1, -1, -1 };

    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("tabela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (c.beginTable("t", ig::Span<const float>(weights, 3), style, 300.0f))
        {
            for (int i = 0; i < 3; ++i)
            {
                c.tableNextColumn();
                headerOrder[i] = c.tableColumnIndex();
                c.tableHeader(headerOrder[i] == 0 ? "A" : (headerOrder[i] == 1 ? "B" : "C"));
                headerRects[i] = c.itemRect();
            }
            for (int i = 0; i < 3; ++i)
            {
                c.tableNextColumn();
                cellOrder[i] = c.tableColumnIndex();
                c.label("x");
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
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
    assert(headerOrder[0] == 0 && headerOrder[1] == 1 && headerOrder[2] == 2);
    assert(cellOrder[0] == 0 && cellOrder[1] == 1 && cellOrder[2] == 2);
    harness.frame(build);
    assert(headerOrder[0] == 1 && headerOrder[1] == 2 && headerOrder[2] == 0);
    assert(cellOrder[0] == 1 && cellOrder[1] == 2 && cellOrder[2] == 0);
}

// Uma tabela com scrollY tem a altura que pediu mesmo sem linhas (por exemplo
// uma lista que ficou vazia): o que vem depois dela não sobe para cima dela.
void test_empty_scrolling_table_keeps_its_height()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    int rows = 30;
    ig::Rect before;
    ig::Rect after;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("vazia", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        c.label("antes");
        before = c.itemRect();
        ig::TableStyle style;
        style.scrollY = true;
        style.height = 80.0f;
        if (c.beginTable("t", 2, style))
        {
            for (int row = 0; row < rows; ++row)
            {
                c.tableNextColumn();
                c.label("a");
                c.tableNextColumn();
                c.label("b");
            }
            c.endTable();
        }
        c.label("depois");
        after = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    const float full = after.y - before.y;
    rows = 0;
    harness.frame(build);
    harness.frame(build);
    assert(after.y - before.y == full);
}

// Um objeto atrás da câmara não tem gizmo: projetado, aparecia espelhado no
// ecrã (o w negativo invertia o ponto).
void test_gizmo3d_behind_the_camera_draws_nothing()
{
    const float view[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -5, 1 };
    const float projection[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, -1, 0, 0, -0.2f, 0 };
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    ig::Transform3D transform;
    transform.position = ig::Vec3(0.0f, 0.0f, 8.0f);
    ig::Gizmo3DOptions options;
    options.showModel = false;
    bool withGizmo = false;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
        if (withGizmo)
            c.gizmo3D("g", transform, ig::Gizmo3DMode::Translate,
                    ig::Rect(0.0f, 0.0f, 500.0f, 500.0f), view, projection, options);
        c.endWindow();
    };
    harness.frame(build);
    const size_t without = harness.frame(build).vertices.size();
    withGizmo = true;
    harness.frame(build);
    const size_t behind = harness.frame(build).vertices.size();
    assert(behind == without);

    transform.position = ig::Vec3(0.0f, 0.0f, 0.0f);
    harness.frame(build);
    assert(harness.frame(build).vertices.size() > without);
}

// Um gizmo3D que muda o transform num frame em que o centro não se projeta (o
// objeto ficou atrás da câmara a meio do arrasto) também marca o item como
// editado.
void test_gizmo3d_edit_without_a_center_is_reported()
{
    const float front[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -5, 1 };
    const float behind[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 5, 1 };
    const float projection[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, -1, 0, 0, -0.2f, 0 };
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    ig::Transform3D transform;
    ig::Gizmo3DOptions options;
    options.showModel = false;
    const float* view = front;
    bool changed = false;
    bool edited = false;
    ig::Vec2 o;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 600.0f, 600.0f)));
        c.dummy(1.0f, 1.0f);
        o = ig::Vec2(c.itemRect().x, c.itemRect().y);
        changed = c.gizmo3D("g", transform, ig::Gizmo3DMode::Scale,
                ig::Rect(0.0f, 0.0f, 500.0f, 500.0f), view, projection, options);
        edited = c.isItemEdited();
        c.endWindow();
    };
    harness.frame(build);
    context.pushEvent(ig::Event::pointerMove(o.x + 250.0f, o.y + 250.0f));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, o.x + 250.0f, o.y + 250.0f));
    harness.frame(build);
    view = behind;
    context.pushEvent(ig::Event::pointerMove(o.x + 300.0f, o.y + 250.0f));
    harness.frame(build);
    assert(changed);
    assert(edited);
}

// Carregar no splitter fora do meio da pega não o faz saltar para o ponteiro:
// o arrasto anda a partir de onde se agarrou.
void test_splitter_does_not_jump_on_press()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float value = 100.0f;
    bool changed = false;
    ig::Vec2 o;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        c.dummy(1.0f, 1.0f);
        o = ig::Vec2(c.itemRect().x, c.itemRect().y);
        changed = c.splitter("s", value, 20.0f, 300.0f, ig::SplitterAxis::Vertical,
                          ig::Rect(0.0f, 0.0f, 350.0f, 200.0f), 8.0f) ||
                  changed;
        c.endWindow();
    };
    harness.frame(build);
    const float grabX = o.x + 100.0f + 3.0f;
    const float grabY = o.y + 50.0f;
    context.pushEvent(ig::Event::pointerMove(grabX, grabY));
    context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, grabX, grabY));
    harness.frame(build);
    harness.frame(build);
    assert(value == 100.0f && !changed);
    context.pushEvent(ig::Event::pointerMove(grabX + 20.0f, grabY));
    harness.frame(build);
    assert(value == 120.0f && changed);
    context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, grabX + 20.0f, grabY));
    harness.frame(build);
}

// Um stepperInt com o valor fora do intervalo volta para dentro ao primeiro
// clique (antes andava de 1 a partir de fora, ou não fazia nada).
void test_stepper_int_brings_the_value_into_range()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    int value = 50;
    bool changed = false;
    ig::Vec2 o;
    const float height = 28.0f;
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("janela", ig::Rect(0.0f, 0.0f, 400.0f, 200.0f)));
        c.dummy(1.0f, 1.0f);
        o = ig::Vec2(c.itemRect().x, c.itemRect().y);
        changed = c.stepperInt("passo", value, 0, 10, ig::Rect(0.0f, 0.0f, 300.0f, height));
        c.endWindow();
    };
    harness.frame(build);
    const float controls = height * 2.0f + height * 1.7f;
    const float decrementX = o.x + 300.0f - controls + height * 0.5f;
    const float incrementX = o.x + 300.0f - height * 0.5f;
    const float y = o.y + height * 0.5f;
    harness.frame(build);
    assert(value == 50 && !changed);
    harness.click(decrementX, y, build);
    assert(value == 10);
    value = -5;
    harness.click(incrementX, y, build);
    assert(value == 0);
    harness.click(incrementX, y, build);
    assert(value == 1);
}

// Um valor infinito numa série não entra na escala automática (tal como o NaN):
// as outras amostras continuam a ocupar a altura do gráfico, e o infinito fica
// na borda.
void test_plot_ignores_infinite_values_in_the_scale()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float infinity = 1.0f / 0.0f;
    float values[5] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f };
    auto build = [&](ig::Context& c)
    {
        assert(c.beginWindow("plot", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        c.plotLines("serie", values, 5, ig::Rect(0.0f, 0.0f, 300.0f, 120.0f));
        c.endWindow();
    };
    auto lineHeight = [&]()
    {
        const ig::DrawData& data = harness.frame(build);
        const ig::Color line = context.theme().plotLines;
        float top = 100000.0f;
        float bottom = -100000.0f;
        for (size_t i = 0; i < data.vertices.size(); ++i)
        {
            const ig::DrawVertex& vertex = data.vertices[i];
            if (vertex.color != line) continue;
            top = vertex.position.y < top ? vertex.position.y : top;
            bottom = vertex.position.y > bottom ? vertex.position.y : bottom;
        }
        return bottom - top;
    };
    harness.frame(build);
    const float finite = lineHeight();
    assert(finite > 50.0f);
    values[3] = infinity;
    const float withInfinity = lineHeight();
    assert(withInfinity > 50.0f);
    values[3] = -infinity;
    assert(lineHeight() > 50.0f);

    // Um NaN também fica fora da escala, e não deixa vértices NaN na geometria.
    values[3] = 0.0f / 0.0f;
    assert(lineHeight() > 50.0f);
    const ig::DrawData& data = harness.frame(build);
    for (size_t i = 0; i < data.vertices.size(); ++i)
        assert(data.vertices[i].position.y == data.vertices[i].position.y);
}

// SizingFixedFit: as colunas ficam com a largura do maior conteúdo delas (a do frame
// anterior), em vez de dividirem a largura da tabela pelos pesos.
void test_table_sizing_fixed_fit()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const float weights[] = {1.0f, 1.0f, 1.0f};
    // Larguras diferentes por coluna: com os pesos iguais a divisão dava o mesmo
    // para as três, e com o FixedFit cada uma fica com a sua.
    const float content[3] = {60.0f, 140.0f, 30.0f};
    ig::Rect cellRects[3];
    bool useFixedFit = true;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("tabela fixed fit", ig::Rect(0.0f, 0.0f, 480.0f, 300.0f)));
        ig::TableStyle style;
        style.sizingFixedFit = useFixedFit;
        if (c.beginTable("fit", ig::Span<const float>(weights, 3), style, 0.0f))
        {
            for (int column = 0; column < 3; ++column)
            {
                c.tableNextColumn();
                c.dummy(content[column], 16.0f);
                cellRects[column] = c.itemRect();
            }
            c.endTable();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    harness.frame(build);
    // Cada coluna começa onde a anterior acabou: a segunda a 60 da primeira (a
    // largura do conteúdo dela, com a margem dos dois lados) e a terceira a 140 da
    // segunda - com os três pesos iguais a divisão daria o mesmo passo para as três.
    const float padding = context.theme().windowPadding * 2.0f;
    assert(cellRects[1].x - cellRects[0].x == content[0] + padding);
    assert(cellRects[2].x - cellRects[1].x == content[1] + padding);
    // A coluna do meio (a mais larga) fica mais larga do que a divisão igual daria.
    const float equalShare = context.availableWidth() / 3.0f;
    assert(content[1] > equalShare - 100.0f);

    // Sem o FixedFit as colunas voltam a dividir a largura da tabela: os passos
    // ficam iguais (nada fica preso às medidas do FixedFit).
    useFixedFit = false;
    harness.frame(build);
    harness.frame(build);
    assert(cellRects[1].x - cellRects[0].x == cellRects[2].x - cellRects[1].x);
}

// Os botões da barra (o TabItemButton) e as bandas (Leading/Trailing): as tabs de
// uma ponta encostam-se ao lado dos botões dessa ponta, as do meio ficam no meio, e
// carregar num botão não escolhe tab nenhuma.
void test_tab_item_button_and_bands()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect leadingButton, trailingButton, firstTab, secondTab, trailingTab;
    int clicks = 0;
    float barWidth = 0.0f;
    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("tabs", ig::Rect(0.0f, 0.0f, 480.0f, 300.0f)));
        if (c.beginTabBar("bandas", ig::Context::TabBarFlags::None))
        {
            // Os botões do demo: o "?" Leading (um menu) e o "+" Trailing (uma tab
            // nova), submetidos antes das tabs.
            if (c.tabItemButton("?", ig::Context::TabItemFlags::Leading))
                ++clicks;
            leadingButton = c.itemRect();
            if (c.tabItemButton("+", ig::Context::TabItemFlags::Trailing))
                ++clicks;
            trailingButton = c.itemRect();

            const bool one = c.beginTabItem("um");
            firstTab = c.itemRect();
            if (one)
            {
                c.dummy(40.0f, 10.0f);
                c.endTabItem();
            }
            const bool two = c.beginTabItem("dois");
            secondTab = c.itemRect();
            if (two)
            {
                c.dummy(40.0f, 20.0f);
                c.endTabItem();
            }
            // Uma tab com a banda Trailing: fica à direita das outras, à esquerda do
            // botão "+" (que é a ponta).
            const bool three = c.beginTabItem("tres", nullptr, ig::Context::TabItemFlags::Trailing);
            trailingTab = c.itemRect();
            if (three)
            {
                c.dummy(40.0f, 30.0f);
                c.endTabItem();
            }
            barWidth = c.availableWidth();
            c.endTabBar();
        }
        c.endWindow();
    };

    // Três frames: as posições das tabs são as do frame anterior, e as dos botões
    // são deste (é o que os põe no sítio já no primeiro frame).
    harness.frame(build);
    harness.frame(build);
    harness.frame(build);

    const float barLeft = leadingButton.x; // a barra começa no primeiro botão Leading
    // O "?" abre a faixa e as tabs do meio começam depois dele.
    assert(leadingButton.x == barLeft);
    assert(firstTab.x >= leadingButton.x + leadingButton.width);
    assert(secondTab.x >= firstTab.x + firstTab.width);
    // O "+" fica na ponta direita da barra, depois da tab Trailing.
    assert(trailingButton.x + trailingButton.width == barLeft + barWidth);
    assert(trailingTab.x >= secondTab.x + secondTab.width);
    assert(trailingTab.x + trailingTab.width <= trailingButton.x);
    // As tabs do meio e a Trailing são as mesmas linhas da barra.
    assert(firstTab.y == secondTab.y && firstTab.y == trailingTab.y);
    assert(firstTab.y == leadingButton.y);

    // Carregar num botão devolve o clique e não escolhe tab nenhuma: quem escolhe é
    // o chamador (é o que o "+" do demo faz, uma tab nova).
    const float x = trailingButton.x + trailingButton.width * 0.5f;
    const float y = trailingButton.y + trailingButton.height * 0.5f;
    harness.click(x, y, build);
    assert(clicks == 1);
    // Depois do clique a barra continua com a mesma escolha (a primeira tab) e as
    // posições não se mexeram.
    harness.frame(build);
    assert(clicks == 1);
    assert(leadingButton.x == barLeft);
    assert(trailingButton.x + trailingButton.width == barLeft + barWidth);
}

// O botão da lista de tabs (o TabListPopupButton): abre a lista das tabs da barra e
// escolher uma é o mesmo que carregar nela.
void test_tab_list_popup_button()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::Rect firstTab, secondTab;
    ig::Rect listButton;
    int open = 0;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("tabs", ig::Rect(0.0f, 0.0f, 480.0f, 300.0f)));
        if (c.beginTabBar("lista", ig::Context::TabBarFlags::TabListPopupButton))
        {
            open = 0;
            // O botão da lista é o primeiro item da barra: logo a seguir ao
            // beginTabBar o último item lido é a caixa dele.
            listButton = c.itemRect();
            // O rect da tab lê-se logo a seguir ao beginTabItem: o do conteúdo (que
            // começa no canto da barra) é o último item enquanto ele está aberto.
            const bool one = c.beginTabItem("um");
            firstTab = c.itemRect();
            if (one)
            {
                open = 1;
                c.dummy(40.0f, 10.0f);
                c.endTabItem();
            }
            const bool two = c.beginTabItem("dois");
            secondTab = c.itemRect();
            if (two)
            {
                open = 2;
                c.dummy(40.0f, 20.0f);
                c.endTabItem();
            }
            c.endTabBar();
        }
        c.endWindow();
    };

    harness.frame(build);
    harness.frame(build);
    assert(open == 1);
    // As tabs começam depois do botão da lista, que é a largura de um item, e o
    // botão fica no canto da barra (a margem da janela).
    assert(firstTab.x ==
           context.theme().windowPadding + context.theme().fontSize + context.theme().itemSpacing);
    assert(listButton.x == context.theme().windowPadding);
    assert(secondTab.x >= firstTab.x + firstTab.width);
    const size_t closedVertices = harness.vertexCount();

    // O botão abre a lista: um clique no canto esquerdo da barra.
    const float buttonX = listButton.x + listButton.width * 0.5f;
    const float buttonY = listButton.y + listButton.height * 0.5f;
    context.pushEvent(ig::Event::pointerMove(buttonX, buttonY));
    harness.frame(build);
    harness.click(buttonX, buttonY, build);
    harness.frame(build);
    // A lista abriu (o fundo e as duas linhas são geometria a mais) e a barra
    // continua a desenhar as tabs onde estavam.
    assert(harness.vertexCount() > closedVertices);
    assert(open == 1);
    assert(firstTab.x ==
           context.theme().windowPadding + context.theme().fontSize + context.theme().itemSpacing);

    // Escolher a segunda tab pela lista: o popup abre por baixo do botão e cada
    // linha é um item do tema, por isso a última (a "dois") é a segunda.
    const float itemX = listButton.x + listButton.width * 0.5f;
    const float itemY = listButton.y + listButton.height +
                        context.theme().widgetHeight * 1.5f;
    harness.click(itemX, itemY, build);
    harness.frame(build);
    assert(open == 2);
}

void test_push_font()
{
    ig::TestBackend backend;
    TwoFaceProvider provider;
    ig::Context context(backend, &provider);
    ig::Harness harness(context);

    const ig::FontId panel = ig::FontId(1u);
    const ig::FontId other = ig::FontId(2u);
    assert(context.theme().font == panel);

    float normalWidth = 0.0f;
    float bigWidth = 0.0f;
    float restoredWidth = 0.0f;
    float afterUnknownWidth = 0.0f;
    const ig::StringView text("The quick brown fox");

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("fonts", ig::Rect(0.0f, 0.0f, 480.0f, 240.0f)));
        c.label(text);
        normalWidth = c.itemRect().width;
        assert(c.theme().font == panel && c.theme().fontSize == 16.0f);

        c.pushFont(other);
        // A face nova vale logo ali, com o tamanho dela.
        assert(c.theme().font == other && c.theme().fontSize == 26.0f);
        c.label(text);
        bigWidth = c.itemRect().width;
        c.popFont();
        assert(c.theme().font == panel && c.theme().fontSize == 16.0f);

        c.label(text);
        restoredWidth = c.itemRect().width;

        // Um id sem face: o push não muda o tamanho em vigor nem desenha texto,
        // mas o pop tira o mesmo o que lá está (a face de antes).
        c.pushFont(ig::FontId(9u));
        assert(c.theme().font == ig::FontId(9u) && c.theme().fontSize == 16.0f);
        c.label(text);
        afterUnknownWidth = c.itemRect().width;
        c.popFont();
        assert(c.theme().font == panel);

        c.endWindow();
    };

    harness.frame(build);
    // O provider conta meio tamanho por glifo (e o dobro na segunda face), e o
    // texto é medido ao tamanho em vigor - que o pushFont levou ao da face.
    const float glyphs = static_cast<float>(text.size());
    assert(normalWidth == glyphs * 16.0f * 0.5f);
    assert(bigWidth == glyphs * 26.0f * 0.5f * 2.0f);
    assert(restoredWidth == normalWidth);
    // Sem face não há medidas: o texto fica com zero de largura (mas o item é
    // submetido e a linha avança na mesma).
    assert(afterUnknownWidth == 0.0f);

    // Um push sem pop não se cola ao frame seguinte: o fim do frame repõe a face
    // (e o tamanho) que estava em vigor.
    auto leaking = [&](ig::Context &c) {
        assert(c.beginWindow("leak", ig::Rect(0.0f, 0.0f, 320.0f, 120.0f)));
        c.pushFont(other);
        c.label(text);
        assert(c.theme().font == other);
        c.endWindow();
    };
    harness.frame(leaking);
    assert(context.theme().font == panel && context.theme().fontSize == 16.0f);
}

// A convenção "###" do ImGui: "Gravar como###gravar" e "Guardar como###gravar" são o
// mesmo id (o que muda é só o que se lê), e um título que muda de texto guarda o
// estado. Sem isto, um painel de documento (o nome do ficheiro no título) perdia o
// estado a cada gravação.
void test_hash_marker()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    ig::WidgetId first = ig::InvalidWidgetId;
    ig::WidgetId renamed = ig::InvalidWidgetId;
    ig::WidgetId plain = ig::InvalidWidgetId;
    bool pressed = false;
    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("###doc-widgets", ig::Rect(0.0f, 0.0f, 300.0f, 160.0f)));
        c.button("Gravar como###gravar");
        first = c.itemId();
        c.button("Guardar como###gravar");
        renamed = c.itemId();
        // Sem marcador o id é o da string toda: é o comportamento antigo, que não
        // pode mudar (um rótulo que já lá estava continua a ter o mesmo id).
        c.button("Gravar como");
        plain = c.itemId();
        c.endWindow();
    };
    harness.frame(build);
    assert(first != ig::InvalidWidgetId && renamed != ig::InvalidWidgetId);
    assert(first == renamed);
    assert(plain != first);

    // O mesmo com os títulos: o mesmo "###" é a mesma janela, e o estado (a posição,
    // que é do que a janela se lembra) sobrevive à mudança de texto - é o caso do
    // painel de documento, cujo título leva o nome do ficheiro.
    ig::Rect bounds;
    bool firstFrame = true;
    auto window = [&](ig::Context &c) {
        const char *title = firstFrame ? "Ficheiro: um###docwin" : "Ficheiro: dois###docwin";
        const ig::Rect initial(firstFrame ? 40.0f : 600.0f, firstFrame ? 30.0f : 400.0f,
                               240.0f, 140.0f);
        assert(c.beginWindow(title, initial));
        bounds = c.windowBounds("###docwin");
        firstFrame = false;
        c.endWindow();
    };
    harness.frame(window);
    assert(bounds.x == 40.0f && bounds.y == 30.0f);
    // O segundo frame traz um título e um rect diferentes, e é a mesma janela: fica
    // onde estava, e não onde o rect que ela passou dizia.
    harness.frame(window);
    assert(bounds.x == 40.0f && bounds.y == 30.0f);

    // Uma janela sem o marcador continua a ser uma janela por título: duas strings
    // diferentes são duas janelas (o "###" não pode virar a regra geral).
    ig::Rect plainA;
    ig::Rect plainB;
    auto two = [&](ig::Context &c) {
        assert(c.beginWindow("uma", ig::Rect(0.0f, 300.0f, 200.0f, 100.0f)));
        plainA = c.windowBounds("uma");
        c.endWindow();
        assert(c.beginWindow("outra", ig::Rect(300.0f, 300.0f, 200.0f, 100.0f)));
        plainB = c.windowBounds("outra");
        c.endWindow();
    };
    harness.frame(two);
    assert(plainA.x == 0.0f && plainB.x == 300.0f);
}

// Um popup dentro de outro, ou um modal dentro de um popup fechado pelo Escape: o
// endPopup de fora tem de fechar o seu corpo, e o que vem depois na janela fica no
// layout da janela, com os popups abertos e depois de fechados.
void test_nested_popup_restores_the_window_layout()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool openOuter = false;
    bool openInner = false;
    bool openModal = false;
    bool innerIsModal = false;
    ig::Rect after;

    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("popups", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        c.label("antes");
        if (openOuter)
        {
            c.openPopup("fora", ig::Vec2(40.0f, 60.0f));
            openOuter = false;
        }
        if (c.beginPopup("fora"))
        {
            c.label("conteudo de fora");
            if (openInner)
            {
                c.openPopup(innerIsModal ? "modal" : "dentro", ig::Vec2(70.0f, 100.0f));
                openInner = false;
            }
            if (!innerIsModal && c.beginPopup("dentro"))
            {
                c.label("conteudo de dentro");
                c.endPopup();
            }
            if (innerIsModal && c.beginPopupModal("modal", &openModal))
            {
                c.label("conteudo do modal");
                c.endPopup();
            }
            c.endPopup();
        }
        c.button("depois");
        after = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    const ig::Rect reference = after;
    assert(reference.width > 0.0f);

    openOuter = true;
    harness.frame(build);
    openInner = true;
    harness.frame(build);
    harness.frame(build);
    assert(context.isPopupOpen("fora") && context.isPopupOpen("dentro"));
    assert(after.x == reference.x && after.y == reference.y);
    assert(after.width == reference.width && after.height == reference.height);

    harness.click(380.0f, 280.0f, build);
    harness.frame(build);
    assert(!context.isPopupOpen("fora") && !context.isPopupOpen("dentro"));
    assert(after.x == reference.x && after.y == reference.y);
    assert(after.width == reference.width && after.height == reference.height);

    innerIsModal = true;
    openModal = true;
    openOuter = true;
    harness.frame(build);
    openInner = true;
    harness.frame(build);
    harness.frame(build);
    assert(context.isPopupOpen("fora") && context.isPopupOpen("modal"));
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Escape));
    harness.frame(build);
    harness.frame(build);
    assert(context.isPopupOpen("fora") && !context.isPopupOpen("modal"));
    assert(after.x == reference.x && after.y == reference.y);
    assert(after.width == reference.width && after.height == reference.height);
}

// Uma barra de tabs dentro do conteúdo de uma tab tem as suas tabs: o
// beginTabItem da barra de dentro abre, e um segundo beginTabItem da mesma barra
// antes do endTabItem continua a ser recusado.
void test_nested_tab_bars()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool outerOpen = false;
    bool innerOpen = false;
    bool innerContent = false;
    bool sameBarRefused = true;
    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("tabs", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        outerOpen = false;
        innerOpen = false;
        innerContent = false;
        if (c.beginTabBar("outer"))
        {
            if (c.beginTabItem("Outer A"))
            {
                outerOpen = true;
                if (c.beginTabBar("inner"))
                {
                    if (c.beginTabItem("Inner 1"))
                    {
                        innerOpen = true;
                        sameBarRefused = !c.beginTabItem("Inner 2") && sameBarRefused;
                        c.label("inner content");
                        innerContent = c.itemRect().width > 0.0f;
                        c.endTabItem();
                    }
                    c.endTabBar();
                }
                c.endTabItem();
            }
            c.endTabBar();
        }
        c.endWindow();
    };
    harness.frame(build);
    harness.frame(build);
    assert(outerOpen);
    assert(innerOpen);
    assert(innerContent);
    assert(sameBarRefused);
}

// O duplo clique do diálogo de ficheiros é em tempo, não em frames: dois cliques
// a 0,72 s um do outro não abrem a pasta, nem a 30 fps, e um duplo clique a 1000
// fps abre-a.
class FolderProvider : public ig::FileDialogProvider
{
public:
    bool listDirectory(ig::StringView path, ct::Vector<ig::FileDialogEntry> &entries,
                       bool = true) override
    {
        if (ig::String(path.data(), path.size()) != "/root")
            return true;
        ig::FileDialogEntry entry;
        entry.name = "sub";
        entry.path = "/root/sub";
        entry.directory = true;
        entries.push_back(entry);
        return true;
    }
    ig::String parentDirectory(ig::StringView) override { return "/"; }
    ig::String homeDirectory() override { return "/root"; }
};

struct DialogRun
{
    ig::TestBackend backend;
    ig::Context context;
    FolderProvider provider;
    ig::FileDialogState state;
    bool open;

    DialogRun() : backend(), context(backend), provider(), state(), open(true) {}

    void frame(float seconds)
    {
        context.beginFrame(ig::FrameInfo(1280.0f, 800.0f, 1.0f, seconds));
        ig::FileDialogOptions options;
        options.initialPath = "/root";
        context.fileDialog("files", open, state, options, provider);
        context.endFrame();
    }

    void click(float x, float y, float seconds)
    {
        context.pushEvent(ig::Event::pointerMove(x, y));
        context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
        frame(seconds);
        context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, y));
        frame(seconds);
    }
};

void test_file_dialog_double_click_is_timed()
{
    float rowX = 640.0f;
    float rowY = -1.0f;
    {
        DialogRun run;
        run.frame(0.016f);
        for (float y = 60.0f; y < 760.0f && rowY < 0.0f; y += 4.0f)
        {
            run.click(rowX, y, 0.016f);
            if (run.state.selectedIndex == 0)
                rowY = y;
            // Os cliques do varrimento nunca fazem um duplo clique entre si.
            for (int i = 0; i < 30; ++i)
                run.frame(0.05f);
        }
    }
    assert(rowY >= 0.0f);

    {
        DialogRun run;
        run.frame(0.033f);
        run.click(rowX, rowY, 0.04f);
        for (int i = 0; i < 18; ++i)
            run.frame(0.04f);
        run.click(rowX, rowY, 0.04f);
        assert(run.state.path == "/root");
    }
    {
        DialogRun run;
        run.frame(0.001f);
        run.click(rowX, rowY, 0.001f);
        for (int i = 0; i < 40; ++i)
            run.frame(0.001f);
        run.click(rowX, rowY, 0.001f);
        assert(run.state.path == "/root/sub");
    }
}

// O relógio do Context não pára numa sessão longa: com o relógio já alto, dois
// cliques a um segundo um do outro não são um duplo clique.
void test_clock_keeps_running_in_long_sessions()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    bool doubleClicked = false;
    auto frame = [&](float seconds)
    {
        context.beginFrame(ig::FrameInfo(640.0f, 480.0f, 1.0f, seconds));
        context.beginMainWindow("main");
        context.button("press", ig::Rect(10.0f, 10.0f, 100.0f, 30.0f));
        doubleClicked = doubleClicked || context.isPointerDoubleClicked();
        context.endWindow();
        context.endFrame();
    };
    auto click = [&]()
    {
        context.pushEvent(ig::Event::pointerMove(40.0f, 24.0f));
        context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, 40.0f, 24.0f));
        frame(1.0f / 60.0f);
        context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, 40.0f, 24.0f));
        frame(1.0f / 60.0f);
    };
    frame(1000000.0f);
    click();
    for (int i = 0; i < 60; ++i)
        frame(1.0f / 60.0f);
    click();
    assert(!doubleClicked);
}

// Com um popup modal aberto, um atalho da janela de trás não dispara: o modal é
// dono do teclado, como já era para o isKeyPressed.
void test_shortcut_is_blocked_by_a_modal_popup()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool openModal = false;
    bool modalShown = true;
    bool fired = false;
    bool firedInside = false;
    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("main", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (openModal)
        {
            c.openPopup("Definicoes");
            openModal = false;
        }
        if (c.beginPopupModal("Definicoes", &modalShown))
        {
            c.label("modal");
            firedInside = firedInside || c.shortcut(ig::KeyCode::S, true, false);
            c.endPopup();
        }
        fired = fired || c.shortcut(ig::KeyCode::S, true, false);
        c.endWindow();
    };
    harness.frame(build);
    openModal = true;
    harness.frame(build);
    harness.frame(build);
    assert(context.isPopupOpen("Definicoes"));
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::S, true));
    harness.frame(build);
    assert(!fired);
    assert(firedInside);
}

// Um submenu que deixou de ser submetido deixa de contar como menu aberto: um
// submenu visitado dentro de um popup, com o popup fechado por um clique fora,
// não pode bloquear os cliques da janela (N1), e as bounds de um submenu fechado
// não comem o clique que fecha o menu (N2).
void test_closed_submenus_do_not_block_the_pointer()
{
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        bool wantPopup = false;
        ig::Rect openButton;
        ig::Rect other;
        ig::Rect subItem;
        int otherClicks = 0;
        bool submenuOpen = false;
        auto build = [&](ig::Context &c) {
            assert(c.beginWindow("w", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
            if (c.button("abrir"))
                wantPopup = true;
            openButton = c.itemRect();
            if (c.button("other"))
                ++otherClicks;
            other = c.itemRect();
            if (wantPopup)
            {
                c.openPopup("pp", ig::Vec2(40.0f, 40.0f));
                wantPopup = false;
            }
            if (c.beginPopup("pp"))
            {
                c.menuItem("Bream");
                submenuOpen = c.beginSubMenu("Sub-menu");
                if (!submenuOpen)
                    subItem = c.itemRect();
                if (submenuOpen)
                {
                    c.menuItem("Click me");
                    c.endSubMenu();
                }
                c.endPopup();
            }
            c.endWindow();
        };
        harness.frame(build);
        harness.click(openButton.x + 5.0f, openButton.y + 5.0f, build);
        harness.frame(build);
        context.pushEvent(ig::Event::pointerMove(subItem.x + 10.0f, subItem.y + subItem.height * 0.5f));
        harness.frame(build);
        harness.frame(build);
        assert(submenuOpen);
        harness.click(390.0f, 290.0f, build);
        harness.frame(build);
        assert(!context.isPopupOpen("pp"));
        for (int i = 0; i < 3; ++i)
        {
            harness.click(other.x + 5.0f, other.y + 5.0f, build);
            harness.frame(build);
        }
        assert(otherClicks == 3);
    }
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        bool fileOpen = false;
        bool subOpen = false;
        ig::Rect fileRect;
        ig::Rect recentRect;
        int under = 0;
        ig::Rect underRect;
        auto build = [&](ig::Context &c) {
            if (c.beginMainMenuBar())
            {
                fileOpen = c.beginMenu("File");
                if (!fileOpen)
                    fileRect = c.itemRect();
                if (fileOpen)
                {
                    c.menuItem("New");
                    subOpen = c.beginSubMenu("Recent");
                    if (!subOpen)
                        recentRect = c.itemRect();
                    if (subOpen)
                    {
                        c.menuItem("a.txt");
                        c.menuItem("b.txt");
                        c.endSubMenu();
                    }
                    c.menuItem("Exit");
                    c.endMenu();
                }
                c.endMainMenuBar();
            }
            assert(c.beginWindow("w", ig::Rect(0.0f, 26.0f, 600.0f, 400.0f)));
            if (c.button("under", ig::Rect(180.0f, 0.0f, 120.0f, 30.0f)))
                ++under;
            underRect = c.itemRect();
            c.endWindow();
        };
        harness.frame(build);
        harness.frame(build);
        const float fx = fileRect.x + fileRect.width * 0.5f;
        const float fy = fileRect.y + fileRect.height * 0.5f;
        harness.click(fx, fy, build);
        harness.frame(build);
        context.pushEvent(ig::Event::pointerMove(recentRect.x + 10.0f, recentRect.y + 5.0f));
        harness.frame(build);
        harness.frame(build);
        assert(subOpen);
        harness.click(fx, fy, build);
        harness.frame(build);
        assert(!fileOpen);
        harness.click(fx, fy, build);
        harness.frame(build);
        assert(fileOpen && !subOpen);
        harness.click(underRect.x + 10.0f, underRect.y + 10.0f, build);
        harness.frame(build);
        assert(!fileOpen);
    }
}

// Fechar um popup modal com Escape devolve o teclado à aplicação: o atalho e o
// isKeyPressed voltam a responder (W1).
void test_keyboard_returns_after_a_modal_closes_with_escape()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    bool openModal = false;
    bool modalShown = true;
    bool shortcutFired = false;
    bool keyPressed = false;
    auto build = [&](ig::Context &c) {
        assert(c.beginWindow("main", ig::Rect(0.0f, 0.0f, 400.0f, 300.0f)));
        if (openModal)
        {
            c.openPopup("Modal");
            openModal = false;
        }
        if (c.beginPopupModal("Modal", &modalShown))
        {
            c.label("modal");
            c.endPopup();
        }
        shortcutFired = shortcutFired || c.shortcut(ig::KeyCode::S, true, false);
        keyPressed = keyPressed || c.isKeyPressed(ig::KeyCode::F5);
        c.endWindow();
    };
    harness.frame(build);
    openModal = true;
    harness.frame(build);
    harness.frame(build);
    assert(context.isPopupOpen("Modal"));
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Escape));
    harness.frame(build);
    harness.frame(build);
    assert(!context.isPopupOpen("Modal"));
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::S, true));
    harness.frame(build);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::F5));
    harness.frame(build);
    assert(shortcutFired);
    assert(keyPressed);
}

// Um widget ativo que deixa de ser submetido não fica ativo para sempre: largado o
// botão, a interface volta a receber cliques - depois de arrastar uma janela que
// desaparece a meio, e depois de um slider que desaparece a meio do arrasto (W5).
void test_unsubmitted_active_widget_is_released()
{
    for (int scenario = 0; scenario < 2; ++scenario)
    {
        ig::TestBackend backend;
        ig::Context context(backend);
        ig::Harness harness(context);
        bool showTool = true;
        bool showSlider = true;
        int clicks = 0;
        ig::Rect button;
        ig::Rect slider;
        float value = 0.5f;
        auto build = [&](ig::Context &c) {
            if (c.beginWindow("Main", ig::Rect(0.0f, 0.0f, 300.0f, 300.0f)))
            {
                if (c.button("ok"))
                    ++clicks;
                button = c.itemRect();
                if (showSlider)
                {
                    c.sliderFloat("s", value, 0.0f, 1.0f);
                    slider = c.itemRect();
                }
                c.endWindow();
            }
            if (showTool && c.beginWindow("Tool", ig::Rect(400.0f, 100.0f, 300.0f, 200.0f)))
            {
                c.label("tool");
                c.endWindow();
            }
        };
        harness.frame(build);
        harness.frame(build);
        const ig::Vec2 grab = scenario == 0 ? ig::Vec2(450.0f, 110.0f)
                                            : ig::Vec2(slider.x + 20.0f, slider.y + 5.0f);
        context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, grab.x, grab.y));
        harness.frame(build);
        context.pushEvent(ig::Event::pointerMove(grab.x + 10.0f, grab.y + 10.0f));
        harness.frame(build);
        if (scenario == 0)
            showTool = false;
        else
            showSlider = false;
        harness.frame(build);
        context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, grab.x + 10.0f, grab.y + 10.0f));
        harness.frame(build);
        for (int i = 0; i < 3; ++i)
        {
            context.pushEvent(ig::Event::pointerMove(button.x + 5.0f, button.y + 5.0f));
            harness.click(button.x + 5.0f, button.y + 5.0f, build);
        }
        assert(clicks == 3);
    }
}

// O X de uma tab do dock fecha o painel dessa tab, com o open que o painel
// passou na sua própria chamada (o bool local de cada painel, o padrão do ImGui):
// o dock não guarda o ponteiro de um frame para o outro (D1).
namespace
{
bool gDockClosedA = false;
bool gDockClosedB = false;
bool gDockShowA = true;
bool gDockShowB = true;

void dockPanelWithLocalOpen(ig::Context &c, const char *name, bool &show, bool &closed)
{
    if (!show)
        return;
    bool open = true;
    if (c.beginDockPanel(name, ig::DockSlot::Left, &open))
    {
        c.label(name);
        c.endDockPanel();
    }
    if (!open)
    {
        closed = true;
        show = false;
    }
}
} // namespace

void test_dock_tab_close_uses_its_own_open()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    auto build = [&](ig::Context &c) {
        if (!c.beginWindow("host", ig::Rect(0.0f, 0.0f, 800.0f, 600.0f)))
            return;
        if (c.beginDockSpace("editor"))
        {
            dockPanelWithLocalOpen(c, "Alpha", gDockShowA, gDockClosedA);
            dockPanelWithLocalOpen(c, "Beta", gDockShowB, gDockClosedB);
            c.endDockSpace();
        }
        c.endWindow();
    };
    harness.frame(build);
    harness.frame(build);
    const float fontSize = context.theme().fontSize;
    const float padding = context.theme().windowPadding;
    const float closeSize = context.theme().widgetHeight * 0.55f;
    const float alphaWidth = 5.0f * fontSize * 0.5f + padding * 2.0f + closeSize;
    const float betaWidth = 4.0f * fontSize * 0.5f + padding * 2.0f + closeSize;
    const float x = alphaWidth + betaWidth - closeSize * 0.5f - padding * 0.35f;
    const float y = context.theme().titleBarHeight + context.theme().widgetHeight * 0.5f;
    context.pushEvent(ig::Event::pointerMove(x, y));
    harness.frame(build);
    harness.click(x, y, build);
    harness.frame(build);
    harness.frame(build);
    assert(gDockClosedB);
    assert(!gDockClosedA);
    assert(gDockShowA);
}

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
    test_table_scrolls_its_own_rows();
    test_table_frozen_rows_stay_put();
    test_tree_style();
    test_popup_modal();
    test_popup_open_flags();
    test_tabs_with_content();
    test_nested_tab_bars();
    test_file_dialog_double_click_is_timed();
    test_clock_keeps_running_in_long_sessions();
    test_shortcut_is_blocked_by_a_modal_popup();
    test_closed_submenus_do_not_block_the_pointer();
    test_keyboard_returns_after_a_modal_closes_with_escape();
    test_unsubmitted_active_widget_is_released();
    test_dock_tab_close_uses_its_own_open();
    test_tab_bar_fitting();
    test_table_scrolls_sideways();
    test_table_without_sideways_scroll_keeps_the_width();
    test_push_font();
    test_tab_item_button_and_bands();
    test_tab_list_popup_button();
    test_stacked_popups();
    test_nested_popup_restores_the_window_layout();
    test_menu_inside_a_popup();
    test_submenu_inside_a_popup();
    test_table_sizing_fixed_fit();
    test_table_without_frozen_rows_scrolls_the_first_row();
    test_table_scroll_is_clamped_when_rows_shrink();
    test_table_scrolling_both_ways_drags_the_bottom_bar();
    test_table_scrolling_both_ways_shows_the_last_row();
    test_table_scrolling_both_ways_keeps_its_height();
    test_table_resize_with_sideways_scroll_follows_the_pointer();
    test_table_lost_header_drag_releases_the_pointer();
    test_table_with_column_count_uses_its_style();
    test_table_sideways_scroll_is_clamped_when_inner_width_shrinks();
    test_table_sizing_fixed_fit_ignores_items_that_fill_the_cell();
    test_wheel_over_an_area_that_fits_scrolls_the_outer_child();
    test_table_tree_cells_in_one_row_toggle_apart();
    test_table_alternate_rows_span_the_table_with_frozen_columns();
    test_virtual_list_that_shrinks_is_not_empty();
    test_submenu_closes_when_another_item_is_hovered();
    test_main_menu_bar_item_responds_to_its_edges();
    test_combo_scrollbar_can_be_dragged();
    test_wide_scrolling_combo_does_not_clip_its_content();
    test_list_rows_are_not_tab_stops();
    test_long_combo_box_list_stays_on_screen_and_scrolls();
    test_combo_box_opens_above_and_shows_the_current_item();
    test_menu_popups_stay_on_screen();
    test_multi_select_ctrl_a_waits_while_typing();
    test_checkbox_flags_with_a_multi_bit_mask();
    test_list_widgets_report_their_own_item_status();
    test_right_click_outside_closes_menus();
    test_popup_over_another_window_keeps_its_owner();
    test_disabled_widgets_do_not_edit();
    test_dragging_past_a_neighbour_leaves_it_in_place();
    test_toast_with_the_same_id_is_updated();
    test_wrapped_text_does_not_add_a_line_for_spaces_before_a_newline();
    test_drag_source_leaves_the_press_to_its_item();
    test_drop_target_behind_another_window_does_not_accept();
    test_tree_drop_behind_another_window_does_not_accept();
    test_message_box_keys_are_spent_by_the_box();
    test_gizmo_scale_snap_never_reaches_zero();
    test_gizmo3d_plane_handle_follows_the_pointer();
    test_gizmo_rotation_does_not_jump_a_full_turn();
    test_color_edit_reports_only_real_changes();
    test_modal_popup_keeps_the_keyboard();
    test_tab_stays_in_its_window();
    test_dialog_and_modal_popup_scrims_are_drawn();
    test_escape_closes_a_popup_inside_a_dialog_first();
    test_window_without_open_flag_has_no_close_button();
    test_maximized_window_stays_below_the_main_menu_bar();
    test_child_out_of_view_keeps_its_place();
    test_child_measures_the_last_line();
    test_child_inside_a_dock_panel_reports_its_own_width();
    test_indent_group_and_set_cursor_keep_the_content_extent();
    test_group_with_a_child_ignores_the_hidden_content();
    test_new_line_after_same_line_clears_the_tallest_item();
    test_separator_uses_the_available_width();
    test_next_item_width_is_spent_by_the_child();
    test_hash_marker();
    test_out_of_range_selection_is_left_alone();
    test_disabled_menu_item_is_the_last_item();
    test_removed_window_lets_go_of_the_pointer();
    test_available_height_follows_the_container();
    test_wheel_areas_forget_what_is_gone();
    test_table_sizing_fixed_fit_resizes();
    test_table_reorder_applies_on_the_next_frame();
    test_empty_scrolling_table_keeps_its_height();
    test_gizmo3d_behind_the_camera_draws_nothing();
    test_gizmo3d_edit_without_a_center_is_reported();
    test_splitter_does_not_jump_on_press();
    test_stepper_int_brings_the_value_into_range();
    test_plot_ignores_infinite_values_in_the_scale();
    printf("test_item_status: all tests passed\n");
    return 0;
}
