// The comparison scenes written for iGUI's retained widgets, run in a real zen_plataform window (the software rasteriser or the
// OpenGL backend). No standard library: ct and C only. See cmp_common.h and docs/COMPARACAO.md.
#include "cmp_common.h"

#include <igui_zen/ZenApp.hpp>
#include <Widgets.hpp>

namespace rt = ig::retained;

struct IguiLib
{
    ig::zen::App app;
    bool gl = false;
    bool vsync = false;
    bool noDynamicPage = false; // --nodyn: Config::defaultDynamicPage = false (no 1024 x 1024 RGBA page of glyphs at the exact size asked)
    int samples = 2; // --samples=N: the software rasteriser's anti-aliasing samples per axis (the library's default is 2)
    bool idleSkip = false; // --idleskip: frames in which nothing changed are not painted or presented, and the loop sleeps (the library's default; the
                           // measures of a frame leave it off, so that every frame is one that is painted)
    bool flat = false; // --flat: the theme's corner radius 0 (squared frames), the one setting ImGui's shipped look shares
    cmp::FrameStats* cur = nullptr;
    cmp::Span gui, tot;
    long long frameCount = 0;
    bool clicked = false, visible = false;
    rt::Widget* root = nullptr;
    rt::TreeView* tree = nullptr;
    char scene = 'A';

    static void probe(int phase, void* u) { static_cast<IguiLib*>(u)->onProbe(phase); }
    void onProbe(int phase)
    {
        const bool ic = cmp::icountOn();
        if (phase == 0)
        {
            if (!ic)
            {
                tot.begin();
                gui.begin();
            }
            cmp::regionBegin();
        }
        else if (phase == 1)
        {
            cmp::regionEnd();
            if (!ic && cur)
            {
                cur->guiUs = gui.cpuUs();
                cur->guiAllocs = gui.allocs();
                cur->guiBytes = gui.bytes();
                // what the frame hands the backend: the commands, vertices and indices of every pass
                rt::DrawData* dd = rt::GetDrawData();
                int calls = 0, vtx = 0, idx = 0;
                for (const rt::DrawPass& p : dd->passes)
                {
                    if (!p.list) continue;
                    calls += static_cast<int>(p.list->commands().size());
                    vtx += static_cast<int>(p.list->vertices().size());
                    idx += static_cast<int>(p.list->indices().size());
                }
                cur->drawCalls = calls;
                cur->vertices = vtx;
                cur->indices = idx;
            }
        }
        else if (phase == 3)
        {
            if (!ic && cur)
            {
                cur->totalUs = tot.cpuUs();
                cur->wallUs = tot.wallUs();
                cur->totalAllocs = tot.allocs();
            }
            ++frameCount;
            if (clicked) visible = true;
        }
    }

    bool init()
    {
        ig::zen::App::Config c;
        c.title = "cmp";
        c.width = cmp::kW;
        c.height = cmp::kH;
        c.placement.x = 0;
        c.placement.y = 0;
        c.placement.resizable = false;
        c.placement.decorated = false;
        c.preferOpenGL = gl;
        c.vsync = vsync;
        c.msaa = 0; // the library draws its own anti-aliased edges; ImGui's side has none either
        c.showStats = false;
        c.defaultDynamicPage = !noDynamicPage;
        c.samples = samples;
        if (!app.create(c)) return false;
        if (gl != app.usesOpenGL())
        {
            fprintf(stderr, "cmp_igui: wanted %s, got %s\n", gl ? "OpenGL" : "soft", app.usesOpenGL() ? "OpenGL" : "soft");
            return false;
        }
        if (flat) rt::Theme::instance().borderRadius = 0.0f;
        app.setFrameProbe(&IguiLib::probe, this);
        app.setSkipIdleFrames(idleSkip);
        return true;
    }

    void build(char s)
    {
        scene = s;
        rt::WidgetApp& wa = app.widgets();
        root = wa.addStage("main")->createChild<rt::Widget>();
        wa.setStage("main");
        char buf[96];
        switch (s)
        {
        case 'A': break;
        case 'B':
        {
            auto* grid = root->createChild<rt::GridLayout>(cmp::kFormCols);
            grid->setPadding(0.0f);
            grid->setSpacing(0.0f);
            grid->setPosition(static_cast<float>(cmp::kFormX), static_cast<float>(cmp::kFormY));
            grid->setSize(static_cast<float>(cmp::kFormCols * cmp::kCellW), static_cast<float>(cmp::kFormRows * cmp::kCellH));
            for (int i = 0; i < cmp::kFormCols * cmp::kFormRows; ++i)
            {
                cmp::formText(i, buf, sizeof buf);
                switch (cmp::formKind(i))
                {
                case 0: grid->createChild<rt::Label>(rt::String(buf)); break;
                case 1: grid->createChild<rt::Button>(rt::String(buf)); break;
                case 2:
                {
                    auto* cb = grid->createChild<rt::CheckBox>(rt::String(buf));
                    if (i == 2) cb->toggled.connect([this](bool) { clicked = true; });
                    break;
                }
                case 3: grid->createChild<rt::Slider>(0.0f, 1.0f, 0.3f); break;
                default: grid->createChild<rt::TextInput>(rt::String(buf)); break;
                }
            }
            break;
        }
        case 'C':
        {
            auto* lb = root->createChild<rt::ListBox>();
            lb->setPosition(0, 0);
            lb->setSize(static_cast<float>(cmp::kW), static_cast<float>(cmp::kH));
            for (int i = 0; i < cmp::kRows; ++i)
            {
                cmp::rowText(i, buf, sizeof buf);
                lb->addItem(rt::String(buf));
            }
            break;
        }
        case 'D':
        {
            tree = root->createChild<rt::TreeView>();
            tree->setPosition(0, 0);
            tree->setSize(static_cast<float>(cmp::kW), static_cast<float>(cmp::kH));
            for (int g = 0; g < cmp::kTreeGroups; ++g)
            {
                cmp::groupText(g, buf, sizeof buf);
                rt::TreeNode* n = tree->addRoot(rt::String(buf));
                for (int k = 0; k < cmp::kTreeKids; ++k)
                {
                    char kb[64];
                    cmp::kidText(g, k, kb, sizeof kb);
                    n->addChild(rt::String(kb));
                }
            }
            break;
        }
        case 'E':
        {
            size_t len = 0;
            char* text = cmp::makeText(&len);
            auto* te = root->createChild<rt::TextEdit>();
            te->setPosition(0, 0);
            te->setSize(static_cast<float>(cmp::kW), static_cast<float>(cmp::kH));
            te->setText(rt::String(text, len));
            free(text);
            break;
        }
        case 'F':
        {
            for (int k = 0; k < cmp::kWindows; ++k)
            {
                snprintf(buf, sizeof buf, "Window %d", k);
                auto* fw = wa.addFloat<rt::FloatWindow>(rt::String(buf));
                auto* box = fw->setContent<rt::BoxLayout>(rt::LayoutDir::Vertical);
                box->createChild<rt::Label>(rt::String("A label"));
                box->createChild<rt::Button>(rt::String("A button"));
                box->createChild<rt::CheckBox>(rt::String("A check box"));
                fw->setFloatSize(static_cast<float>(cmp::kWinW), static_cast<float>(cmp::kWinH));
                fw->setFloatPos(static_cast<float>(cmp::winX(k)), static_cast<float>(cmp::winY(k)));
            }
            break;
        }
        }
        wa.requestLayout();
    }

    void frame(cmp::FrameStats& fs)
    {
        fs = cmp::FrameStats();
        cur = &fs;
        app.pump();
        cur = nullptr;
    }
    void treeOp(bool open)
    {
        if (!tree) return;
        for (rt::TreeNode* n : tree->roots()) n->setExpanded(open);
    }
    void natural(int seconds, long long& frames)
    {
        const double end = cmp::wallMs() + seconds * 1000.0;
        while (cmp::wallMs() < end && app.pump()) ++frames;
    }
    // FNV-1a of what the last frame handed the backend (vertices, indices, number of commands): two builds of the library that draw
    // the same thing give the same number, so a faster build can be shown to draw exactly what the slower one did.
    unsigned long long drawHash()
    {
        unsigned long long h = 1469598103934665603ull;
        auto mix = [&](const void* p, size_t n) {
            const unsigned char* b = static_cast<const unsigned char*>(p);
            for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
        };
        for (const rt::DrawPass& p : rt::GetDrawData()->passes)
        {
            if (!p.list) continue;
            mix(p.list->vertices().data(), p.list->vertices().size() * sizeof(rt::DrawVertex));
            mix(p.list->indices().data(), p.list->indices().size() * sizeof(uint32_t));
            const size_t nc = p.list->commands().size();
            mix(&nc, sizeof nc);
        }
        return h;
    }
    bool changed() const { return visible; }
    void resetChanged() { clicked = visible = false; }
    long long frames() const { return frameCount; }
    void pumpOnce() { app.pump(); }
};

int main(int argc, char** argv)
{
    cmp::t0Wall();
    cmp::t0Cpu();
    IguiLib lib;
    for (int i = 3; i < argc; ++i)
        if (!strcmp(argv[i], "--gl")) lib.gl = true;
        else if (!strcmp(argv[i], "--flat")) lib.flat = true;
        else if (!strcmp(argv[i], "--idleskip")) lib.idleSkip = true;
        else if (!strcmp(argv[i], "--nodyn")) lib.noDynamicPage = true;
        else if (!strncmp(argv[i], "--samples=", 10)) lib.samples = atoi(argv[i] + 10);
    lib.vsync = argc > 2 && !strcmp(argv[2], "idlecpu");
    return cmp::run(lib, argc, argv);
}
