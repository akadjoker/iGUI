// inputInt / inputFloat editing: what the user types must survive while the
// field is focused, including states that are not a number yet ("-", "1.",
// an empty field), and only valid, finite, in-range values reach the caller.

#include "InteractionHarness.hpp"

#include <limits.h>
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

    Field(bool floating, float initial)
        : backend(), context(backend), harness(context), isFloat(floating),
          intValue(static_cast<int>(initial)), floatValue(initial), otherShown(true), changes(0)
    {
        frame();
        // Focus the field.
        harness.click(150.0f, 24.0f, [this](ig::Context &c) { build(c); });
    }

    void build(ig::Context &c)
    {
        c.beginMainWindow("main");
        const bool changed = isFloat ? c.inputFloat("value", floatValue, ig::Rect(10.0f, 10.0f, 300.0f, 28.0f))
                                     : c.inputInt("value", intValue, ig::Rect(10.0f, 10.0f, 300.0f, 28.0f));
        if (changed)
            ++changes;
        if (otherShown)
            c.button("other", ig::Rect(10.0f, 60.0f, 100.0f, 28.0f));
        c.endWindow();
    }

    void frame() { harness.frame([this](ig::Context &c) { build(c); }); }

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

int main()
{
    test_int_typing();
    test_float_typing();
    test_display_follows_value_when_not_editing();
    test_change_reported_only_for_new_values();
    test_non_finite_values_draw_finite_geometry();
    test_slider_flags();
    test_drag_allow_out_of_range();
    if (gFailures != 0)
    {
        printf("test_numeric_input: %d failure(s)\n", gFailures);
        return 1;
    }
    printf("test_numeric_input: all tests passed\n");
    return 0;
}
