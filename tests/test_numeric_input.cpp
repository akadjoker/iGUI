// inputInt / inputFloat editing: what the user types must survive while the
// field is focused, including states that are not a number yet ("-", "1.",
// an empty field), and only valid, finite, in-range values reach the caller.

#include "InteractionHarness.hpp"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace
{

int gFailures = 0;

void check(bool condition, const char *what)
{
    if (!condition)
    {
        ++gFailures;
        printf("FAIL %s\n", what);
    }
}

// O que a widget desenhou como texto neste frame (o valor do campo, com o formato
// dele): é por aqui que se verifica o que o formato faz ao que se lê no ecrã.
bool drewText(const ig::DrawData &data, const char *expected)
{
    for (size_t i = 0u; i < data.commands.size(); ++i)
    {
        const ig::DrawCommand &command = data.commands[i];
        if (command.type != ig::DrawCommandType::Text)
            continue;
        const ig::TextCommand &text = command.payload.text;
        if (static_cast<size_t>(text.textOffset) + text.textSize > data.textBytes.size())
            continue;
        const ig::StringView drawn(data.textBytes.data() + text.textOffset, text.textSize);
        if (drawn == ig::StringView(expected))
            return true;
    }
    return false;
}

// Quantas vezes a widget desenhou este texto no frame.
int countText(const ig::DrawData &data, const char *expected)
{
    int count = 0;
    for (size_t i = 0u; i < data.commands.size(); ++i)
    {
        const ig::DrawCommand &command = data.commands[i];
        if (command.type != ig::DrawCommandType::Text)
            continue;
        const ig::TextCommand &text = command.payload.text;
        if (static_cast<size_t>(text.textOffset) + text.textSize > data.textBytes.size())
            continue;
        const ig::StringView drawn(data.textBytes.data() + text.textOffset, text.textSize);
        if (drawn == ig::StringView(expected))
            ++count;
    }
    return count;
}

struct Field
{
    ig::TestBackend backend;
    ig::Context context;
    ig::Harness harness;
    bool isFloat;
    int intValue;
    float floatValue;
    bool otherShown;
    int changes;
    // O formato do valor (o "%04X" do demo): nulo é o de sempre.
    const char *format;

    Field(bool floating, float initial, const char *valueFormat = nullptr)
        : backend(), context(backend), harness(context), isFloat(floating),
          intValue(static_cast<int>(initial)), floatValue(initial), otherShown(true), changes(0),
          format(valueFormat)
    {
        frame();
        // Focus the field.
        harness.click(150.0f, 24.0f, [this](ig::Context &c) { build(c); });
    }

    void build(ig::Context &c)
    {
        c.beginMainWindow("main");
        const bool changed =
            isFloat ? c.inputFloat("value", floatValue, ig::Rect(10.0f, 10.0f, 300.0f, 28.0f), 3,
                                   format)
                    : c.inputInt("value", intValue, ig::Rect(10.0f, 10.0f, 300.0f, 28.0f), format);
        if (changed)
            ++changes;
        if (otherShown)
            c.button("other", ig::Rect(10.0f, 60.0f, 100.0f, 28.0f));
        c.endWindow();
    }

    void frame() { harness.frame([this](ig::Context &c) { build(c); }); }

    // O frame desenhado, para os testes que olham para o que a widget escreveu.
    const ig::DrawData &draw() { return harness.frame([this](ig::Context &c) { build(c); }); }

    void key(ig::KeyCode code)
    {
        context.pushEvent(ig::Event::keyDown(code));
        frame();
    }

    void type(const char *text)
    {
        for (const char *p = text; *p; ++p)
        {
            const char one[2] = {*p, 0};
            context.pushEvent(ig::Event::textInput(one));
            frame();
        }
    }

    void clear()
    {
        for (int i = 0; i < 20; ++i)
            key(ig::KeyCode::Backspace);
    }
};

void test_int_typing()
{
    {
        Field f(false, 0.0f);
        f.clear();
        f.type("-5");
        check(f.intValue == -5, "inputInt: typing -5 after clearing");
    }
    {
        Field f(false, 0.0f);
        f.clear();
        f.type("123");
        check(f.intValue == 123, "inputInt: typing 123");
    }
    {
        // Erasing "10" passes through "1", a valid number, so the value is
        // 1 by the time the field is empty - and the empty field keeps it.
        Field f(false, 10.0f);
        f.clear();
        check(f.intValue == 1, "inputInt: an empty field keeps the last valid value");
        f.type("7");
        check(f.intValue == 7, "inputInt: typing into a cleared field");
    }
    {
        Field f(false, 0.0f);
        f.clear();
        f.type("99999999999");
        check(f.intValue == INT_MAX, "inputInt: overflow clamps to INT_MAX");
        f.clear();
        f.type("-99999999999");
        check(f.intValue == INT_MIN, "inputInt: underflow clamps to INT_MIN");
    }
    {
        Field f(false, 4.0f);
        f.clear();
        f.type("1x");
        check(f.intValue == 1, "inputInt: trailing garbage does not change the value");
    }
}

void test_float_typing()
{
    {
        Field f(true, 0.0f);
        f.clear();
        f.type("1.5");
        check(f.floatValue == 1.5f, "inputFloat: typing 1.5 after clearing");
    }
    {
        Field f(true, 0.0f);
        f.clear();
        f.type("-2");
        check(f.floatValue == -2.0f, "inputFloat: typing -2 after clearing");
    }
    {
        Field f(true, 0.0f);
        f.clear();
        f.type("-0.25");
        check(f.floatValue == -0.25f, "inputFloat: typing -0.25");
    }
    {
        Field f(true, 0.0f);
        f.clear();
        f.type("1e3");
        check(f.floatValue == 1000.0f, "inputFloat: exponent");
    }
    {
        Field f(true, 3.0f);
        f.clear();
        f.type("nan");
        check(f.floatValue == 3.0f, "inputFloat: 'nan' is rejected");
        f.clear();
        f.type("inf");
        check(f.floatValue == 3.0f, "inputFloat: 'inf' is rejected");
        f.clear();
        // Typed key by key, "1e9" is already valid; "1e99" overflows to
        // infinity and must not replace it.
        f.type("1e99");
        check(f.floatValue == 1e9f, "inputFloat: overflow to infinity is rejected");
        f.clear();
        f.type("0x10");
        check(f.floatValue == 0.0f, "inputFloat: hex is not a float literal here (only the 0 counts)");
    }
}

void test_display_follows_value_when_not_editing()
{
    {
        // Leaving the field normalizes it back to the value's own text.
        Field f(false, 0.0f);
        f.clear();
        f.type("-");
        check(f.intValue == 0, "inputInt: a lone '-' keeps the value");
        f.key(ig::KeyCode::Escape); // focus leaves the field
        f.intValue = 42;           // the application changes it
        f.frame();
        f.harness.click(150.0f, 24.0f, [&f](ig::Context &c) { f.build(c); });
        f.type("1");
        check(f.intValue == 421, "inputInt: text restarts from the value after focus left");
    }
    {
        // The application changing the value while the field is focused
        // replaces the stale text.
        Field f(false, 5.0f);
        f.intValue = 80;
        f.frame();
        f.type("1");
        check(f.intValue == 801, "inputInt: external change while focused resyncs the text");
    }
}

// O formato do valor (o "%04X", o "%.1f grams" do demo): vale para o que o campo
// mostra e para o que ele lê, e é o que faz um campo hexadecimal ser hexadecimal
// dos dois lados.
void test_format_drives_display_and_parse()
{
    {
        // Um inteiro em hexadecimal: escreve-se e lê-se em hexadecimal.
        Field f(false, 0.0f, "%04X");
        f.clear();
        f.type("ff");
        check(f.intValue == 255, "inputInt with %04X: ff is 255");
        // Com o campo focado vê-se o que foi escrito; é quando o foco sai que se vê
        // o valor escrito pelo formato.
        f.key(ig::KeyCode::Escape);
        f.frame();
        check(drewText(f.draw(), "00FF"), "inputInt with %04X: shows the value in hex");
    }
    {
        // A legenda do formato é aceite à entrada (é o que lá está escrito).
        Field f(true, 0.0f, "%.1f grams");
        f.clear();
        f.type("1.5");
        check(f.floatValue > 1.49f && f.floatValue < 1.51f, "inputFloat with a decoration: 1.5");
        f.key(ig::KeyCode::Escape);
        f.frame();
        check(drewText(f.draw(), "1.5 grams"),
              "inputFloat with %.1f grams: the value keeps its legend");
    }
    {
        // Sem formato, o texto é o de sempre e o "1x" continua a não valer 1.
        Field f(false, 4.0f);
        f.clear();
        f.type("1x");
        check(f.intValue == 1, "inputInt: trailing garbage still leaves the number");
    }
}

// O SliderAngle: o valor é em radianos, o intervalo e o texto em graus - o que se
// arrasta e o que se escreve é em graus, e o que fica guardado é em radianos.
// O DragFloatRange2: duas caixas (mínimo e máximo) com o rótulo no fim, e cada uma
// limitada pela outra - arrastar o mínimo para lá do máximo não o passa.
// O texto de um widget de inteiros é o número inteiro (o "5"), e não um real com
// zeros nem o que sai de um formato mal montado - foi por aqui que passou um
// "%.3g" com o comprimento de um inteiro ("%llg"), que escrevia lixo.
void test_int_widgets_write_the_integer()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    int dragValue = 5;
    int sliderValue = 7;
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        c.dragInt("i", dragValue, 0, 10, 1, ig::Rect(10.0f, 10.0f, 260.0f, 28.0f));
        c.sliderInt("s", sliderValue, 0, 10, ig::Rect(10.0f, 42.0f, 260.0f, 28.0f));
        c.endWindow();
    };
    const ig::DrawData &data = harness.frame(build);
    check(drewText(data, "5"), "dragInt: o valor escreve-se como um inteiro");
    check(drewText(data, "7"), "sliderInt: o campo mostra o inteiro");
    check(!drewText(data, "5.000"), "dragInt: e não com três decimais");
}

// A família escalar: o mesmo slider/drag/campo para os tipos que não são float
// nem int, com o valor por ponteiro e o formato do tipo (o ImGuiDataType do
// ImGui).
void test_scalar_types()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    uint8_t u8 = 0u;
    int16_t s16 = 0;
    uint32_t u32 = 0u;
    uint64_t u64 = 0ull;
    double f64 = 0.0;
    float f32 = 0.5f;
    const uint8_t u8Min = 0u;
    const uint8_t u8Max = 200u;
    const int16_t s16Min = -1000;
    const int16_t s16Max = 1000;
    const float f32Min = 0.0f;
    const float f32Max = 1.0f;
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        c.sliderScalar("u8", ig::Context::ScalarType::U8, &u8, &u8Min, &u8Max, nullptr, 300.0f);
        c.dragScalar("s16", ig::Context::ScalarType::S16, &s16, 1.0f, &s16Min, &s16Max, nullptr,
                     300.0f);
        c.inputScalar("u32", ig::Context::ScalarType::U32, &u32, "%04X", 300.0f);
        c.inputScalar("u64", ig::Context::ScalarType::U64, &u64, nullptr, 300.0f);
        c.inputScalar("f64", ig::Context::ScalarType::Double, &f64, "%.10f", 300.0f);
        c.sliderScalar("f32", ig::Context::ScalarType::Float, &f32, &f32Min, &f32Max, "%.2f",
                       300.0f);
        c.endWindow();
    };
    const ig::DrawData &data = harness.frame(build);
    // O formato de cada tipo aparece no ecrã: o "%04X" do u32 e os dez decimais do
    // double.
    check(drewText(data, "0000"), "inputScalar u32: o valor sai no formato do chamador");
    check(drewText(data, "0.0000000000"), "inputScalar f64: o formato do double");
    check(drewText(data, "0.50"), "sliderScalar f32: o formato do float");

    // O slider de um u8 tem o intervalo dele: arrastado para o fim, o valor é 200
    // (o valor mais alto que um u8 deste intervalo pode ter) e nunca passa disso.
    harness.drag(30.0f, 20.0f, 4000.0f, 20.0f, build);
    check(u8 == 200u, "sliderScalar u8: a ponta da pista é o máximo do intervalo");

    // O drag de um s16 arrastado para o fim fica no tecto dele.
    harness.drag(30.0f, 52.0f, 4000.0f, 52.0f, build);
    check(s16 == 1000, "dragScalar s16: o arrasto pára no tecto");

    // O campo do u32 é hexadecimal dos dois lados.
    harness.click(150.0f, 84.0f, build);
    for (int i = 0; i < 20; ++i)
    {
        context.pushEvent(ig::Event::keyDown(ig::KeyCode::Backspace));
        harness.frame(build);
    }
    context.pushEvent(ig::Event::textInput("ff"));
    harness.frame(build);
    check(u32 == 255u, "inputScalar u32: o hex escrito à mão vale 255");

    // O campo de um double guarda os dez dígitos (é o que um float não faz). Cada
    // widget daqui está 32 px abaixo da anterior (26 de altura e 6 de espaço).
    harness.click(150.0f, 148.0f, build);
    for (int i = 0; i < 20; ++i)
    {
        context.pushEvent(ig::Event::keyDown(ig::KeyCode::Backspace));
        harness.frame(build);
    }
    context.pushEvent(ig::Event::textInput("0.1"));
    harness.frame(build);
    check(f64 == 0.1, "inputScalar f64: o valor do double fica exato");

    // O campo de um u64 com um valor acima de 2^32 (2^32 cabe em double, é o que
    // estes tipos usam por dentro).
    u64 = 4294967296ull;
    harness.frame(build);
    check(drewText(harness.frame(build), "4294967296"),
          "inputScalar u64: um valor acima de 2^32 escreve-se inteiro");
}

void test_drag_range2()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    float low = 2.0f;
    float high = 8.0f;
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        c.dragFloatRange2("range", low, high, 0.0f, 10.0f, 0.01f, 300.0f);
        c.endWindow();
    };
    const ig::DrawData &data = harness.frame(build);
    // As duas caixas e os valores delas (o formato por omissão é o %g de três
    // dígitos, o mesmo dos drags).
    check(drewText(data, "2"), "dragFloatRange2: a caixa do mínimo mostra o mínimo");
    check(drewText(data, "8"), "dragFloatRange2: a caixa do máximo mostra o máximo");

    // O rato apanha a caixa do mínimo (o layout começa no canto da janela) e
    // arrasta-o para lá do máximo: fica no máximo, porque isto é uma gama.
    harness.drag(30.0f, 20.0f, 700.0f, 20.0f, build);
    check(low == high, "dragFloatRange2: o mínimo pára no máximo");
    check(high == 8.0f, "dragFloatRange2: o máximo não se mexeu");

    // E ao contrário: o máximo arrastado para a esquerda não passa o mínimo.
    high = 5.0f;
    low = 5.0f;
    harness.drag(150.0f, 20.0f, 0.0f, 20.0f, build);
    check(high == low, "dragFloatRange2: o máximo pára no mínimo");
    check(low == 5.0f, "dragFloatRange2: o mínimo não se mexeu");

    // A gama (0..10) também limita: o mínimo arrastado para a esquerda pára em 0.
    low = 2.0f;
    high = 8.0f;
    // O arrasto vale 0.01 por pixel, por isso o rato tem de andar muito para sair
    // da gama (é um arrasto, não um clique).
    harness.drag(30.0f, 20.0f, -2000.0f, 20.0f, build);
    check(low == 0.0f, "dragFloatRange2: o mínimo pára no limite da gama");

    // Com o intervalo dado ao contrário (minRange >= maxRange) não há limites de
    // gama: o mínimo sai para fora dela, mas continua a não passar o máximo.
    high = 3.0f;
    low = 3.0f;
    auto unbounded = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        c.dragFloatRange2("range", low, high, 1.0f, 0.0f, 1.0f, 300.0f);
        c.endWindow();
    };
    harness.drag(30.0f, 20.0f, -2000.0f, 20.0f, unbounded);
    check(low < 1.0f, "dragFloatRange2: sem limites de gama o valor sai dela");
    check(low <= high, "dragFloatRange2: mas o mínimo continua a não passar o máximo");
}

void test_slider_angle()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    float radians = 0.0f;
    const float half = 1.5707964f; // pi/2, o que vale um ângulo de 90 graus
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        c.sliderAngle("angle", radians, -180.0f, 180.0f, ig::Rect(10.0f, 10.0f, 300.0f, 28.0f));
        c.endWindow();
    };
    const ig::DrawData &data = harness.frame(build);
    check(drewText(data, "0 deg"), "sliderAngle: o valor é escrito em graus");

    // O valor em radianos que a aplicação põe é o que o texto mostra em graus.
    radians = half;
    check(drewText(harness.frame(build), "90 deg"), "sliderAngle: 90 graus são pi/2");

    // Escrever no campo é escrever graus: o que fica guardado é em radianos.
    // O campo do valor fica depois da track (o rótulo "angle" leva 48 px, a track
    // outros 175, e o campo vem a seguir), por isso o clique a 260 cai nele.
    harness.click(260.0f, 24.0f, build);
    for (int i = 0; i < 20; ++i)
    {
        context.pushEvent(ig::Event::keyDown(ig::KeyCode::Backspace));
        harness.frame(build);
    }
    context.pushEvent(ig::Event::textInput("90"));
    harness.frame(build);
    check(radians > 1.57f && radians < 1.58f, "sliderAngle: 90 escrito à mão é pi/2");

    // Um arrasto na pista é em graus: a ponta direita dela é o máximo (180 graus,
    // que são 3.1416 radianos) e a ponta esquerda é o mínimo.
    harness.click(233.0f, 24.0f, build);
    check(radians > 3.12f && radians < 3.15f, "sliderAngle: a ponta da pista são 180 graus");
    harness.click(58.0f, 24.0f, build);
    check(radians < -3.12f && radians > -3.15f, "sliderAngle: a outra ponta são -180 graus");
}

void test_change_reported_only_for_new_values()
{
    Field f(false, 5.0f);
    f.changes = 0;
    f.key(ig::KeyCode::Backspace); // "" - not a number yet
    check(f.changes == 0 && f.intValue == 5, "inputInt: clearing reports no change");
    f.type("5");
    check(f.changes == 0, "inputInt: retyping the same value reports no change");
    f.type("0");
    check(f.changes == 1 && f.intValue == 50, "inputInt: a new value reports one change");
}

// Non-finite values handed in by the application must not turn into
// non-finite geometry (backends would get NaN coordinates), and widgets
// must not rewrite them without user input.
void test_non_finite_values_draw_finite_geometry()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    const float nan = 0.0f / 0.0f;
    const float infinity = 1.0f / 0.0f;
    float values[] = {nan, infinity, -infinity, nan};
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        c.sliderFloat("a", values[0], 0.0f, 1.0f);
        c.sliderFloat("b", values[1], 0.0f, 1.0f);
        c.sliderFloat("c", values[2], 0.0f, 1.0f);
        c.dragFloat("d", values[3], 0.0f, 1.0f, 0.1f);
        c.progressBar(nan, 1.0f);
        c.progressBar(1.0f, nan);
        c.progressBar(infinity, 1.0f);
        c.endWindow();
    };
    harness.frame(build);
    const ig::DrawData &data = harness.frame(build);
    bool finite = true;
    for (size_t i = 0; i < data.vertices.size(); ++i)
    {
        const ig::Vec2 &p = data.vertices[i].position;
        finite = finite && p.x == p.x && p.y == p.y && p.x < 1e30f && p.x > -1e30f && p.y < 1e30f && p.y > -1e30f;
    }
    check(finite, "non-finite widget values produced non-finite vertices");
    check(values[0] != values[0] && values[1] == infinity && values[2] == -infinity && values[3] != values[3],
          "widgets rewrote non-finite values without user input");
}

// Os flags do slider/drag: a escala logarítmica, o WrapAround, o NoInput e o
// AllowOutOfRange (o valor escrito pode sair do intervalo).
void test_slider_flags()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    const unsigned log = ig::Context::SliderFlags::Logarithmic;
    const unsigned wrap = ig::Context::SliderFlags::WrapAround;
    const unsigned noInput = ig::Context::SliderFlags::NoInput;
    const unsigned outOfRange = ig::Context::SliderFlags::AllowOutOfRange;

    // Uma fila de sliders, para o rect de cada um sair do itemRect do respetivo
    // track: o item é a pista, não o slider todo.
    float linear = 1.0f;
    float logarithmic = 1.0f;
    float wrapped = 1.0f;
    float plain = 1.0f;
    ig::Rect linearRect;
    ig::Rect logRect;
    ig::Rect wrapRect;
    ig::Rect noInputRect;
    ig::Rect plainRect;

    auto build = [&](ig::Context &c) {
        c.beginMainWindow("main");
        c.sliderFloat("linear", linear, 1.0f, 100.0f, 0.0f, 0u);
        linearRect = c.itemRect();
        c.sliderFloat("log", logarithmic, 1.0f, 100.0f, 0.0f, log);
        logRect = c.itemRect();
        c.sliderFloat("wrap", wrapped, 1.0f, 100.0f, 0.0f, wrap);
        wrapRect = c.itemRect();
        c.sliderFloat("noinput", plain, 1.0f, 100.0f, 0.0f, noInput);
        noInputRect = c.itemRect();
        c.sliderFloat("plain", plain, 1.0f, 100.0f, 0.0f, 0u);
        plainRect = c.itemRect();
        c.endWindow();
    };

    harness.frame(build);
    check(linearRect.width > 0.0f && logRect.width > 0.0f, "the sliders have a track");

    // O item de um slider é o widget todo (o rótulo, a pista e o campo do valor,
    // como no itemRect do iGUI), por isso o fim da pista só se apanha a arrastar:
    // um clique no canto direito cai no campo do valor.
    auto dragPast = [&](const ig::Rect &rect) {
        const float y = rect.y + rect.height * 0.5f;
        const float x0 = rect.x + rect.width * 0.5f;
        const float x1 = rect.x + rect.width + 40.0f;
        context.pushEvent(ig::Event::pointerMove(x0, y));
        context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x0, y));
        harness.frame(build);
        context.pushEvent(ig::Event::pointerMove(x1, y));
        harness.frame(build);
        context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x1, y));
        harness.frame(build);
    };

    // O mesmo clique nos dois sliders: na escala logarítmica o valor fica muito
    // abaixo do linear (é a média geométrica em vez da aritmética), e os dois
    // chegam ao máximo no fim da pista.
    const float quarterY = linearRect.y + linearRect.height * 0.5f;
    harness.click(linearRect.x + linearRect.width * 0.25f, quarterY,
                  [&](ig::Context &c) { build(c); });
    const float linearQuarter = linear;
    harness.click(logRect.x + logRect.width * 0.25f, quarterY,
                  [&](ig::Context &c) { build(c); });
    check(linearQuarter > 20.0f, "a linear slider follows the position");
    check(logarithmic < linearQuarter * 0.5f,
          "a logarithmic slider stays well below the linear one at the same position");
    dragPast(logRect);
    check(logarithmic > 99.0f, "the end of the track is the maximum, logarithmic or not");

    // WrapAround: com o valor a chegar ao máximo (e a estar abaixo dele) salta para
    // o mínimo.
    wrapped = 50.0f;
    harness.frame(build);
    dragPast(wrapRect);
    check(wrapped < 2.0f, "WrapAround sends a value that reached the top to the bottom");
    // E fica lá: o frame seguinte não volta a saltar.
    harness.frame(build);
    check(wrapped < 2.0f, "WrapAround does not flip back and forth");

    // NoInput: sem o editor de texto ao lado, a pista chega ao fim do widget: um
    // clique no canto direito mexe no valor em vez de abrir o campo.
    plain = 50.0f;
    harness.frame(build);
    const float rightY = noInputRect.y + noInputRect.height * 0.5f;
    harness.click(noInputRect.x + noInputRect.width - 4.0f, rightY,
                  [&](ig::Context &c) { build(c); });
    check(plain > 95.0f, "a NoInput slider has the track all the way to the right");
    for (const char *p = "7"; *p; ++p)
    {
        const char one[2] = {*p, 0};
        context.pushEvent(ig::Event::textInput(one));
        harness.frame(build);
    }
    check(plain > 95.0f, "NoInput: there is no value field to type into");
}

// AllowOutOfRange: o número escrito à mão no drag não é limitado ao intervalo.
void test_drag_allow_out_of_range()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float clamped = 50.0f;
    float free = 50.0f;
    ig::Rect clampedRect;
    ig::Rect freeRect;

    auto build = [&](ig::Context &c) {
        c.beginMainWindow("main");
        c.dragFloat("clamped", clamped, 0.0f, 100.0f, 0.1f, ig::Rect(10.0f, 10.0f, 300.0f, 28.0f));
        clampedRect = c.itemRect();
        c.dragFloat("free", free, 0.0f, 100.0f, 0.1f,
                    ig::Rect(10.0f, 50.0f, 300.0f, 28.0f),
                    ig::Context::SliderFlags::AllowOutOfRange);
        freeRect = c.itemRect();
        c.endWindow();
    };
    harness.frame(build);

    auto typeInto = [&](const ig::Rect &rect, const char *text) {
        const float x = rect.x + rect.width * 0.5f;
        const float y = rect.y + rect.height * 0.5f;
        // Um clique sem arrasto abre o campo de texto (ver dragFloat).
        context.pushEvent(ig::Event::pointerMove(x, y));
        context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
        harness.frame(build);
        context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, y));
        harness.frame(build);
        harness.frame(build);
        for (int i = 0; i < 24; ++i)
        {
            context.pushEvent(ig::Event::keyDown(ig::KeyCode::Backspace));
            harness.frame(build);
        }
        for (const char *p = text; *p; ++p)
        {
            const char one[2] = {*p, 0};
            context.pushEvent(ig::Event::textInput(one));
            harness.frame(build);
        }
        context.pushEvent(ig::Event::keyDown(ig::KeyCode::Enter));
        harness.frame(build);
    };

    typeInto(clampedRect, "150");
    check(clamped == 100.0f, "without the flag the typed value stays inside the range");
    typeInto(freeRect, "150");
    check(free == 150.0f, "AllowOutOfRange keeps the typed value outside the range");
    typeInto(freeRect, "-20");
    check(free == -20.0f, "AllowOutOfRange keeps a typed value below the range");
}

} // namespace

// Um formato só tem uma conversão: um "%" que não seja a do valor sai como um
// por-cento, e um formato que não cabe sai no texto de sempre - nunca uma string
// sem fim nem um snprintf a ler um argumento que não lhe foi passado.
void test_format_with_stray_percent_signs()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float percent = 50.0f;
    int32_t pair = 7;
    int32_t longPrefix = 7;
    const char *longFormat =
        "123456789012345678901234567890123456789012345678901234567890%d";
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        c.inputScalar("percent", ig::Context::ScalarType::Float, &percent, "%.0f%", 300.0f);
        c.inputScalar("pair", ig::Context::ScalarType::S32, &pair, "%d/%d", 300.0f);
        c.inputScalar("long", ig::Context::ScalarType::S32, &longPrefix, longFormat, 300.0f);
        c.endWindow();
    };
    const ig::DrawData &data = harness.frame(build);
    check(drewText(data, "50%"), "format: a stray % after the value is a percent sign");
    check(drewText(data, "7/%d"), "format: a second conversion is printed as text");
    check(drewText(data, "7"), "format: a format that does not fit falls back to the plain value");
}

// O valor de trabalho é um double; o que fica guardado tem de caber no tipo. Um
// slider de u64 ou de s64 arrastado até ao fim guarda o máximo do tipo, um s8
// escrito à mão com "300" guarda 127, e os extremos de 64 bits escrevem-se certos.
void test_scalar_values_stay_in_their_type()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    uint64_t u64 = 0u;
    const uint64_t u64Min = 0u;
    const uint64_t u64Max = UINT64_MAX;
    int64_t s64 = 0;
    const int64_t s64Min = INT64_MIN;
    const int64_t s64Max = INT64_MAX;
    int8_t s8 = 0;
    uint64_t bigU64 = 13835058055282163712ull;
    int64_t bigS64 = INT64_MAX;
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        c.sliderScalar("u64", ig::Context::ScalarType::U64, &u64, &u64Min, &u64Max, nullptr,
                       300.0f);
        c.sliderScalar("s64", ig::Context::ScalarType::S64, &s64, &s64Min, &s64Max, nullptr,
                       300.0f);
        c.inputScalar("s8", ig::Context::ScalarType::S8, &s8, nullptr, 300.0f);
        c.inputScalar("big u64", ig::Context::ScalarType::U64, &bigU64, nullptr, 300.0f);
        c.inputScalar("big s64", ig::Context::ScalarType::S64, &bigS64, nullptr, 300.0f);
        c.endWindow();
    };
    const ig::DrawData &data = harness.frame(build);
    check(drewText(data, "13835058055282163712"), "u64 above 2^63 is written unsigned");
    check(drewText(data, "9223372036854775807"), "INT64_MAX is written as itself");

    harness.drag(60.0f, 20.0f, 4000.0f, 20.0f, build);
    check(u64 == UINT64_MAX, "sliderScalar u64 at the end of the track stores UINT64_MAX");
    harness.drag(60.0f, 52.0f, 4000.0f, 52.0f, build);
    check(s64 == INT64_MAX, "sliderScalar s64 at the end of the track stores INT64_MAX");

    harness.click(150.0f, 84.0f, build);
    for (int i = 0; i < 20; ++i)
    {
        context.pushEvent(ig::Event::keyDown(ig::KeyCode::Backspace));
        harness.frame(build);
    }
    context.pushEvent(ig::Event::textInput("300"));
    harness.frame(build);
    check(s8 == 127, "inputScalar s8: 300 is limited to 127");
    check(bigU64 == 13835058055282163712ull && bigS64 == INT64_MAX,
          "values that were not edited are not written back");
}

// Os overloads de layout passam o formato ao widget: o "%.1f grams" do slider,
// o "%04X" do inputInt, o "Min:"/"Max:" do dragFloatRange2 e o formato dos
// inputFloat2 aparecem no ecrã.
void test_layout_overloads_keep_the_format()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float grams = 0.5f;
    int hex = 255;
    float low = 2.0f;
    float high = 8.0f;
    float px = 1.5f;
    float py = 2.5f;
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        c.sliderFloat("sf", grams, 0.0f, 1.0f, 300.0f, 0u, "%.1f grams");
        c.inputInt("ii", hex, 300.0f, "%04X");
        c.dragFloatRange2("range", low, high, 0.0f, 10.0f, 0.1f, 300.0f, "Min: %.1f",
                          "Max: %.1f");
        c.inputFloat2("pair", px, py, 300.0f, 3, "%.2f");
        c.endWindow();
    };
    const ig::DrawData &data = harness.frame(build);
    check(drewText(data, "0.5 grams"), "sliderFloat (width): the format reaches the slider");
    check(drewText(data, "00FF"), "inputInt (width): the format reaches the field");
    check(drewText(data, "Min: 2.0"), "dragFloatRange2: the minimum format");
    check(drewText(data, "Max: 8.0"), "dragFloatRange2: the maximum format");
    check(drewText(data, "1.50") && drewText(data, "2.50"), "inputFloat2: the format");
}

// Sem formato do chamador, o drag e o slider escrevem o inteiro inteiro e o real
// com até três casas, sem expoente; e o editor de um drag abre com esse texto,
// por isso uma edição parte do número que lá está.
void test_default_format_has_no_exponent()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float big = 1234.5f;
    int count = 1500;
    float small = -0.0001f;
    int editable = 1234;
    ig::Rect editableRect;
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        c.dragFloat("big", big, 0.0f, 100000.0f, 1.0f, 300.0f);
        c.dragInt("count", count, 0, 100000, 1, 300.0f);
        c.sliderFloat("small", small, -1.0f, 1.0f, 300.0f);
        c.dragInt("editable", editable, 0, 100000, 1, 300.0f);
        editableRect = c.itemRect();
        c.endWindow();
    };
    const ig::DrawData &data = harness.frame(build);
    check(drewText(data, "1234.5"), "dragFloat: 1234.5 without an exponent");
    check(drewText(data, "1500"), "dragInt: 1500 is written as an integer");
    check(drewText(data, "0"), "sliderFloat: a value that rounds to zero is 0, not -0");

    const float x = editableRect.x + editableRect.width * 0.5f;
    const float y = editableRect.y + editableRect.height * 0.5f;
    harness.click(x, y, build);
    check(drewText(harness.frame(build), "1234"), "dragInt: the editor opens with 1234");
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Backspace));
    harness.frame(build);
    context.pushEvent(ig::Event::textInput("9"));
    harness.frame(build);
    check(editable == 1239, "dragInt: editing starts from the number that was there");
}

// O texto de um campo com formato traz o prefixo do formato ("0 < 0.500 < 1"):
// o número lê-se depois dele, e não do início do texto.
void test_parse_skips_the_format_prefix()
{
    {
        Field f(true, 0.5f, "0 < %.3f < 1");
        f.key(ig::KeyCode::End);
        f.key(ig::KeyCode::Backspace);
        check(f.floatValue == 0.5f, "inputFloat with a prefix: the prefix 0 is not the value");
    }
    {
        Field f(true, 1.0f, "Value: %.1f");
        f.key(ig::KeyCode::End);
        f.key(ig::KeyCode::Backspace);
        f.type("5");
        check(f.floatValue == 1.5f, "inputFloat with a prefix: an edit that keeps it is read");
    }
}

// Com o botão em baixo e o ponteiro parado, um slider ou um drag só muda uma vez:
// o valor guardado num float (ou num int) já é o do ponteiro, e não volta a contar
// como mudança a cada frame (nem o isItemEdited fica ligado).
void test_held_pointer_reports_one_change()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    float slider = 0.0f;
    float drag = 0.0f;
    float vertical = 0.0f;
    int sliderInt = 0;
    int sliderChanges = 0;
    int dragChanges = 0;
    int verticalChanges = 0;
    int intChanges = 0;
    int edited = 0;
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        if (c.sliderFloat("sf", slider, 0.0f, 100.0f, ig::Rect(10.0f, 10.0f, 300.0f, 28.0f)))
            ++sliderChanges;
        if (c.isItemEdited())
            ++edited;
        if (c.dragFloat("df", drag, 0.0f, 100.0f, 0.37f, ig::Rect(10.0f, 50.0f, 300.0f, 28.0f)))
            ++dragChanges;
        if (c.vSliderFloat("vs", vertical, 0.0f, 100.0f, ig::Rect(10.0f, 90.0f, 30.0f, 150.0f)))
            ++verticalChanges;
        if (c.sliderInt("si", sliderInt, 0, 100, ig::Rect(10.0f, 250.0f, 300.0f, 28.0f)))
            ++intChanges;
        if (c.isItemEdited())
            ++edited;
        c.endWindow();
    };
    auto hold = [&](float x, float y, bool move)
    {
        context.pushEvent(ig::Event::pointerMove(x, y));
        context.pushEvent(ig::Event::pointerDown(ig::PointerButton::Left, x, y));
        harness.frame(build);
        if (move)
        {
            context.pushEvent(ig::Event::pointerMove(x + 10.0f, y));
            harness.frame(build);
        }
        for (int i = 0; i < 6; ++i)
            harness.frame(build);
    };
    auto release = [&](float x, float y)
    {
        context.pushEvent(ig::Event::pointerUp(ig::PointerButton::Left, x, y));
        harness.frame(build);
    };

    harness.frame(build);
    hold(150.0f, 24.0f, false);
    check(sliderChanges == 1, "sliderFloat: a held pointer changes the value once");
    check(edited == 1, "sliderFloat: isItemEdited is not on in every frame");
    release(150.0f, 24.0f);

    hold(150.0f, 64.0f, true);
    check(dragChanges == 1, "dragFloat: a held pointer after one move changes the value once");
    release(160.0f, 64.0f);

    hold(25.0f, 160.0f, false);
    check(verticalChanges == 1, "vSliderFloat: a held pointer changes the value once");
    release(25.0f, 160.0f);

    edited = 0;
    hold(150.0f, 264.0f, false);
    check(intChanges == 1, "sliderInt: a held pointer changes the value once");
    check(edited == 1, "sliderInt: isItemEdited is not on in every frame");
    release(150.0f, 264.0f);
}

// Um intervalo ao contrário ou vazio não faz o widget desaparecer: o slider
// "s32 reverse" do demo desenha-se e anda ao contrário, e um dragFloatRange2 com
// as duas pontas iguais desenha as duas caixas.
void test_reversed_and_empty_ranges_still_draw()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    int32_t reversed = 25;
    const int32_t fifty = 50;
    const int32_t zero = 0;
    float low = 0.0f;
    float high = 0.0f;
    float stuck = 3.0f;
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        c.sliderScalar("rev", ig::Context::ScalarType::S32, &reversed, &fifty, &zero, nullptr,
                       300.0f);
        c.dragFloatRange2("range", low, high, 0.0f, 10.0f, 0.1f, 300.0f);
        c.sliderFloat("same", stuck, 5.0f, 5.0f, 300.0f);
        c.endWindow();
    };
    const ig::DrawData &data = harness.frame(build);
    check(drewText(data, "25"), "sliderScalar with a reversed range is drawn");
    check(countText(data, "0") >= 2, "dragFloatRange2 with both ends at 0 draws both boxes");
    check(drewText(data, "same"), "sliderFloat with an empty range is drawn");

    harness.drag(60.0f, 20.0f, 4000.0f, 20.0f, build);
    check(reversed == 0, "sliderScalar reversed: the right end is the maximum given (0)");
    harness.drag(60.0f, 20.0f, -4000.0f, 20.0f, build);
    check(reversed == 50, "sliderScalar reversed: the left end is the minimum given (50)");
}

// O que se escreve num campo de float fica: a referência do campo é o valor que
// o float guarda, por isso o texto escrito não é trocado pelo valor reformatado
// a meio ("0.10" não volta a "0.1" e o "5" não vai para o fim).
void test_float_fields_keep_what_is_typed()
{
    {
        Field f(true, 0.0f);
        f.clear();
        f.type("0.105");
        check(f.floatValue == 0.105f, "inputFloat: 0.105 is stored as typed");
    }

    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    float scalar = 0.0f;
    float slider = 0.0f;
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        c.inputScalar("scalar", ig::Context::ScalarType::Float, &scalar, nullptr, 300.0f);
        c.sliderFloat("slider", slider, 0.0f, 1.0f, ig::Rect(10.0f, 40.0f, 300.0f, 28.0f));
        c.endWindow();
    };
    auto typeInto = [&](float x, float y, const char *text)
    {
        harness.click(x, y, build);
        for (int i = 0; i < 20; ++i)
        {
            context.pushEvent(ig::Event::keyDown(ig::KeyCode::Backspace));
            harness.frame(build);
        }
        for (const char *p = text; *p; ++p)
        {
            const char one[2] = {*p, 0};
            context.pushEvent(ig::Event::textInput(one));
            harness.frame(build);
        }
    };
    harness.frame(build);
    typeInto(150.0f, 20.0f, "1.15");
    check(scalar == 1.15f, "inputScalar float: 1.15 is stored as typed");
    typeInto(290.0f, 54.0f, "0.105");
    check(slider == 0.105f, "sliderFloat value field: 0.105 is stored as typed");
}

int main()
{
    test_int_typing();
    test_float_typing();
    test_display_follows_value_when_not_editing();
    test_format_drives_display_and_parse();
    test_drag_range2();
    test_scalar_types();
    test_int_widgets_write_the_integer();
    test_slider_angle();
    test_change_reported_only_for_new_values();
    test_non_finite_values_draw_finite_geometry();
    test_slider_flags();
    test_drag_allow_out_of_range();
    test_format_with_stray_percent_signs();
    test_scalar_values_stay_in_their_type();
    test_layout_overloads_keep_the_format();
    test_default_format_has_no_exponent();
    test_parse_skips_the_format_prefix();
    test_held_pointer_reports_one_change();
    test_reversed_and_empty_ranges_still_draw();
    test_float_fields_keep_what_is_typed();
    if (gFailures != 0)
    {
        printf("test_numeric_input: %d failure(s)\n", gFailures);
        return 1;
    }
    printf("test_numeric_input: all tests passed\n");
    return 0;
}
