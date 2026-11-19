// The comparison scenes written for FLTK 1.4 (X11, Xft). Outside the no-std rule: bench/compare is built only with
// IGUI_BUILD_COMPARE=ON and holds the code for the other libraries. See cmp_common.h and docs/COMPARACAO.md.
#include "cmp_common.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Tree.H>

struct Win : Fl_Double_Window
{
    int draws = 0;
    Win(int w, int h) : Fl_Double_Window(0, 0, w, h, "cmp") {}
    void draw() override
    {
        Fl_Double_Window::draw();
        ++draws;
    }
};

// the checkbox the latency test clicks: remembers it was toggled and when it was drawn after that
struct Probe : Fl_Check_Button
{
    bool* clicked;
    bool* visible;
    Probe(int x, int y, int w, int h, const char* l, bool* c, bool* v) : Fl_Check_Button(x, y, w, h, l), clicked(c), visible(v) {}
    void draw() override
    {
        Fl_Check_Button::draw();
        if (*clicked) *visible = true;
    }
};

struct FltkLib
{
    Win* win = nullptr;
    char scene = 'A';
    bool clicked = false, visible = false;
    long long drawCount = 0;
    Fl_Tree* tree = nullptr;
    Fl_Text_Buffer* tbuf = nullptr;
    char labels[200][32];

    static void toggled(Fl_Widget*, void* u) { *static_cast<bool*>(u) = true; }

    bool init()
    {
        Fl::visual(FL_DOUBLE | FL_RGB);
        win = new Win(cmp::kW, cmp::kH);
        win->border(0);
        win->end();
        win->show();
        return true;
    }

    void build(char s)
    {
        scene = s;
        win->begin();
        switch (s)
        {
        case 'A': break;
        case 'B':
            for (int i = 0; i < cmp::kFormCols * cmp::kFormRows; ++i)
            {
                const int r = i / cmp::kFormCols, c = i % cmp::kFormCols;
                const int x = cmp::kFormX + c * cmp::kCellW, y = cmp::kFormY + r * cmp::kCellH;
                cmp::formText(i, labels[i], sizeof labels[i]);
                switch (cmp::formKind(i))
                {
                case 0:
                {
                    auto* b = new Fl_Box(x, y, cmp::kCellW, cmp::kCellH, labels[i]);
                    b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
                    break;
                }
                case 1: new Fl_Button(x, y, cmp::kCellW, cmp::kCellH, labels[i]); break;
                case 2:
                {
                    if (i == 2)
                    {
                        auto* p = new Probe(x, y, cmp::kCellW, cmp::kCellH, labels[i], &clicked, &visible);
                        p->callback(&FltkLib::toggled, &clicked);
                    }
                    else
                        new Fl_Check_Button(x, y, cmp::kCellW, cmp::kCellH, labels[i]);
                    break;
                }
                case 3:
                {
                    auto* sl = new Fl_Slider(x, y, cmp::kCellW, cmp::kCellH);
                    sl->type(FL_HORIZONTAL);
                    sl->value(0.3);
                    break;
                }
                default:
                {
                    auto* in = new Fl_Input(x, y, cmp::kCellW, cmp::kCellH);
                    in->value(labels[i]);
                    break;
                }
                }
            }
            break;
        case 'C':
        {
            auto* br = new Fl_Browser(0, 0, cmp::kW, cmp::kH);
            char b[64];
            for (int i = 0; i < cmp::kRows; ++i)
            {
                cmp::rowText(i, b, sizeof b);
                br->add(b);
            }
            break;
        }
        case 'D':
        {
            tree = new Fl_Tree(0, 0, cmp::kW, cmp::kH);
            tree->showroot(0);
            char b[64], k[64];
            for (int g = 0; g < cmp::kTreeGroups; ++g)
            {
                cmp::groupText(g, b, sizeof b);
                Fl_Tree_Item* it = tree->add(b);
                for (int n = 0; n < cmp::kTreeKids; ++n)
                {
                    cmp::kidText(g, n, k, sizeof k);
                    tree->add(it, k);
                }
            }
            break;
        }
        case 'E':
        {
            size_t len = 0;
            char* t = cmp::makeText(&len);
            auto* ed = new Fl_Text_Editor(0, 0, cmp::kW, cmp::kH);
            tbuf = new Fl_Text_Buffer();
            tbuf->text(t);
            free(t);
            ed->buffer(tbuf);
            Fl::focus(ed);
            break;
        }
        case 'F':
            for (int k = 0; k < cmp::kWindows; ++k)
            {
                char* title = labels[k % 200];
                snprintf(title, 32, "Window %d", k);
                const int x = cmp::winX(k), y = cmp::winY(k);
                auto* g = new Fl_Group(x, y, cmp::kWinW, cmp::kWinH, title);
                g->box(FL_UP_BOX);
                g->align(FL_ALIGN_TOP_LEFT | FL_ALIGN_INSIDE);
                new Fl_Box(x + 8, y + 26, cmp::kWinW - 16, 22, "A label");
                new Fl_Button(x + 8, y + 52, cmp::kWinW - 16, 24, "A button");
                new Fl_Check_Button(x + 8, y + 82, cmp::kWinW - 16, 24, "A check box");
                g->end();
            }
            break;
        }
        win->end();
        win->redraw();
    }

    void frame(cmp::FrameStats& fs)
    {
        fs = cmp::FrameStats();
        cmp::Span sp;
        sp.begin();
        cmp::regionBegin();
        // what the toolkit has to do: the events that are in, and the drawing they ask for. Nothing in, nothing drawn.
        Fl::wait(0.0);
        if (win->draws == 0)
            while (win->draws == 0) Fl::wait(0.01); // the first frame: until the window has been exposed and drawn
        cmp::regionEnd();
        if (!cmp::icountOn())
        {
            fs.guiUs = fs.totalUs = sp.cpuUs();
            fs.wallUs = sp.wallUs();
            fs.guiAllocs = sp.allocs();
            fs.totalAllocs = sp.allocs();
        }
    }
    void treeOp(bool open)
    {
        if (!tree) return;
        for (Fl_Tree_Item* it = tree->first(); it; it = tree->next(it))
            if (it->has_children() && it->depth() == 1)
            {
                if (open) tree->open(it, 0);
                else tree->close(it, 0);
            }
    }
    // the way an FLTK program ends its Fl::run(): the timeout hides the window, wait() then has nothing to wait for and returns
    static void quit(void* u)
    {
        exit_ = true;
        static_cast<FltkLib*>(u)->win->hide();
    }
    static bool exit_;
    void natural(int seconds, long long& frames)
    {
        exit_ = false;
        Fl::add_timeout(seconds, &FltkLib::quit, this);
        while (!exit_ && Fl::wait(1e20)) {} // what Fl::run() does: sleeps in select() until an event or the timeout
        frames = win->draws;
    }
    unsigned long long drawHash() { return 0; }
    bool changed() const { return visible; }
    void resetChanged() { clicked = visible = false; }
    long long frames() const { return win->draws; }
    void pumpOnce() { Fl::wait(0.005); }
};
bool FltkLib::exit_ = false;

int main(int argc, char** argv)
{
    cmp::t0Wall();
    cmp::t0Cpu();
    FltkLib lib;
    return cmp::run(lib, argc, argv);
}
