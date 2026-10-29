// O registo das lacunas (o que o demo do ImGui tem e o iGUI não exprime) e os
// números do painel de topo. É a única parte da galeria com código partilhado:
// cada implementação só inclui a biblioteca da sua secção, para as duas APIs
// nunca serem compiladas no mesmo ficheiro.

#include "Gallery.hpp"

#include <string.h>

namespace gallery
{

char gHelloLine[96] = "dear imgui says hello!";
namespace
{

const int kMaxGaps = 96;

Gap g_gaps[kMaxGaps];
int g_gapCount = 0;
Readout g_readout;

} // namespace

void reportGap(const char* section, const char* widget)
{
    if (!section || !widget || g_gapCount >= kMaxGaps) return;
    for (int i = 0; i < g_gapCount; ++i)
    {
        if (strcmp(g_gaps[i].section, section) == 0 && strcmp(g_gaps[i].widget, widget) == 0)
            return;
    }
    g_gaps[g_gapCount].section = section;
    g_gaps[g_gapCount].widget = widget;
    ++g_gapCount;
}

int gapCount()
{
    return g_gapCount;
}

const Gap& gap(int index)
{
    static const Gap empty = { "", "" };
    if (index < 0 || index >= g_gapCount) return empty;
    return g_gaps[index];
}

void clearGaps()
{
    g_gapCount = 0;
}

void setReadout(const Readout& readout)
{
    g_readout = readout;
}

const Readout& readout()
{
    return g_readout;
}

} // namespace gallery
