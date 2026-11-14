// Text in the retained widgets: accents, emoji and invalid UTF-8 in labels, text inputs and
// text edits, typing more than one event's worth in a frame, pasting, eliding long text,
// and a random key/typing fuzz that keeps checking the cursor and the text. Driven through
// the real frame loop (tests/RetainedHarness.hpp).

#include "RetainedHarness.hpp"

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/ComboBox.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/Utf8.hpp>

#include <time.h>

using namespace rh;

namespace
{

// True when every byte sequence in the text is a complete UTF-8 character (what typing and
// editing valid text must keep true; text that came in invalid stays as invalid as it was).
bool validUtf8(const String& s)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s.c_str());
    const unsigned char* end = p + s.size();
    while (p < end)
    {
        int n = 1;
        if (*p >= 0xF8) return false;
        if (*p >= 0xF0) n = 4;
        else if (*p >= 0xE0)
            n = 3;
        else if (*p >= 0xC0)
            n = 2;
        else if (*p >= 0x80)
            return false;
        if (p + n > end) return false;
        for (int i = 1; i < n; ++i)
            if ((p[i] & 0xC0) != 0x80) return false;
        p += n;
    }
    return true;
}

// A stage with one control filling a column, focused.
template<typename T>
struct Rig
{
    Harness h;
    BoxLayout* box = nullptr;
    T* w = nullptr;

    Rig()
    {
        box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(8.0f);
        w = box->createChild<T>();
        w->setStretch(1.0f);
        h.relayout();
        h.clickWidget(w);
    }
};

// The reference of the elided text: a character is taken off the end until the rest and "..." fit.
String slowFit(PaintContext& ctx, const String& text, float room)
{
    if (text.empty() || ctx.font.GetTextWidth(text.c_str()) <= room) return text;
    String shown = text;
    while (!shown.empty())
    {
        size_t last = shown.size() - 1;
        while (last > 0 && (static_cast<unsigned char>(shown[last]) & 0xC0) == 0x80) --last;
        shown.resize(last);
        if (ctx.font.GetTextWidth((shown + "...").c_str()) <= room) break;
    }
    shown += "...";
    return shown;
}

// ---- scenarios ----

void testFitTextMatchesTheLoop()
{
    printf("elided text equals the character-by-character loop\n");
    Harness h;
    DrawList list;
    PaintContext ctx(list, h.font());
    check(ctx.font.GetTextWidth("Música") > 0.0f, "the font measures");
    Rng rng(11);
    const char* alphabet[] = { "a", "b", "M", "W", "i", "l", " ", "ç", "ã", "é", "Ó", "AV", "To", "...", "\xF0\x9F\x8E\xB5",
        "\xE4\xB8\xAD", "\x80", "\xFF", "\xC3", "\xE2\x82", ".", "/", "-" };
    const int kinds = static_cast<int>(sizeof alphabet / sizeof alphabet[0]);
    int mismatches = 0;
    for (int i = 0; i < 400; ++i)
    {
        String text;
        const int length = rng.below(i < 300 ? 40 : 300);
        for (int k = 0; k < length; ++k) text += alphabet[rng.below(kinds)];
        const float room = rng.chance(10) ? rng.range(-5.0f, 10.0f) : rng.range(0.0f, 600.0f);
        const String fast = ctx.fitText(text, room);
        const String slow = slowFit(ctx, text, room);
        if (!(fast == slow)) ++mismatches;
    }
    check(mismatches == 0, "the same text is shown as the loop shows");
    check(ctx.fitText("", 10.0f).empty(), "nothing stays nothing");
    check(ctx.fitText("short", 1000.0f) == String("short"), "text that fits is untouched");
    check(ctx.fitText("Música e vídeos", 0.0f) == String("..."), "no room, only the dots");
}

// A label can carry a very long text: the frame must not take a quarter of a second.
void testLongElidedLabelIsCheap()
{
    printf("an elided label of 8000 characters does not stall the frame\n");
    Harness h(800.0f, 400.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    String longName;
    for (int i = 0; i < 400; ++i) longName += "Entrevista com Ação ";
    Label* label = box->createChild<Label>(longName);
    label->setElide(true);
    h.relayout();
    h.frames(2);
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (int i = 0; i < 5; ++i) h.frame();
    clock_gettime(CLOCK_MONOTONIC, &b);
    const double ms = ((b.tv_sec - a.tv_sec) * 1e3 + (b.tv_nsec - a.tv_nsec) * 1e-6) / 5.0;
    printf("  %.2f ms per frame\n", ms);
    check(ms < 40.0, "a frame with the long elided label stays under 40 ms (it took 245 ms)");
}

} // namespace

// A scenario name on the command line runs only that one.
int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testFitTextMatchesTheLoop);
    RUN(testLongElidedLabelIsCheap);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
