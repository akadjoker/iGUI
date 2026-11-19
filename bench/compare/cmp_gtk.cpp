// The comparison scenes written for GTK 3 (X11, cairo). Outside the no-std rule: bench/compare is built only with
// IGUI_BUILD_COMPARE=ON and holds the code for the other libraries. See cmp_common.h and docs/COMPARACAO.md.
#include "cmp_common.h"

#include <gtk/gtk.h>

struct GtkLib
{
    GtkWidget* win = nullptr;
    GtkWidget* root = nullptr;
    GtkWidget* treeView = nullptr;
    char scene = 'A';
    bool clicked = false, visible = false;
    long long paints = 0;
    long long paintsAtMark = 0;
    bool quitFlag = false;

    static void afterPaint(GdkFrameClock*, gpointer u)
    {
        GtkLib* self = static_cast<GtkLib*>(u);
        ++self->paints;
        if (self->clicked) self->visible = true; // the toggle was handled earlier than this paint: this is the frame that shows it
    }
    static void toggled(GtkToggleButton*, gpointer u) { static_cast<GtkLib*>(u)->clicked = true; }
    static gboolean flag(gpointer p)
    {
        *static_cast<bool*>(p) = true;
        return G_SOURCE_REMOVE;
    }
    static gboolean quit(gpointer u)
    {
        static_cast<GtkLib*>(u)->quitFlag = true;
        gtk_main_quit();
        return G_SOURCE_REMOVE;
    }

    bool init()
    {
        gdk_set_allowed_backends("x11");
        int argc = 0;
        if (!gtk_init_check(&argc, nullptr)) return false;
        win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
        gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
        gtk_window_set_default_size(GTK_WINDOW(win), cmp::kW, cmp::kH);
        gtk_window_move(GTK_WINDOW(win), 0, 0);
        return true;
    }

    GtkWidget* sized(GtkWidget* w, int cw, int ch)
    {
        gtk_widget_set_size_request(w, cw, ch);
        return w;
    }

    void build(char s)
    {
        scene = s;
        char b[96];
        switch (s)
        {
        case 'A': root = gtk_fixed_new(); break;
        case 'B':
        case 'F':
            root = gtk_fixed_new();
            if (s == 'B')
                for (int i = 0; i < cmp::kFormCols * cmp::kFormRows; ++i)
                {
                    const int r = i / cmp::kFormCols, c = i % cmp::kFormCols;
                    const int x = cmp::kFormX + c * cmp::kCellW, y = cmp::kFormY + r * cmp::kCellH;
                    cmp::formText(i, b, sizeof b);
                    GtkWidget* w = nullptr;
                    switch (cmp::formKind(i))
                    {
                    case 0:
                        w = gtk_label_new(b);
                        gtk_label_set_xalign(GTK_LABEL(w), 0.0f);
                        break;
                    case 1: w = gtk_button_new_with_label(b); break;
                    case 2:
                        w = gtk_check_button_new_with_label(b);
                        if (i == 2) g_signal_connect(w, "toggled", G_CALLBACK(&GtkLib::toggled), this);
                        break;
                    case 3: w = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01); gtk_range_set_value(GTK_RANGE(w), 0.3); gtk_scale_set_draw_value(GTK_SCALE(w), FALSE); break;
                    default:
                        w = gtk_entry_new();
                        gtk_entry_set_text(GTK_ENTRY(w), b);
                        break;
                    }
                    gtk_fixed_put(GTK_FIXED(root), sized(w, cmp::kCellW, cmp::kCellH), x, y);
                }
            else
                for (int k = 0; k < cmp::kWindows; ++k)
                {
                    snprintf(b, sizeof b, "Window %d", k);
                    GtkWidget* f = gtk_frame_new(b);
                    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
                    gtk_box_pack_start(GTK_BOX(box), gtk_label_new("A label"), FALSE, FALSE, 0);
                    gtk_box_pack_start(GTK_BOX(box), gtk_button_new_with_label("A button"), FALSE, FALSE, 0);
                    gtk_box_pack_start(GTK_BOX(box), gtk_check_button_new_with_label("A check box"), FALSE, FALSE, 0);
                    gtk_container_add(GTK_CONTAINER(f), box);
                    gtk_fixed_put(GTK_FIXED(root), sized(f, cmp::kWinW, cmp::kWinH), cmp::winX(k), cmp::winY(k));
                }
            break;
        case 'C':
        {
            GtkListStore* st = gtk_list_store_new(1, G_TYPE_STRING);
            GtkTreeIter it;
            for (int i = 0; i < cmp::kRows; ++i)
            {
                cmp::rowText(i, b, sizeof b);
                gtk_list_store_append(st, &it);
                gtk_list_store_set(st, &it, 0, b, -1);
            }
            treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(st));
            g_object_unref(st);
            addColumn();
            root = scrolled(treeView);
            break;
        }
        case 'D':
        {
            GtkTreeStore* st = gtk_tree_store_new(1, G_TYPE_STRING);
            GtkTreeIter g, k;
            for (int gi = 0; gi < cmp::kTreeGroups; ++gi)
            {
                cmp::groupText(gi, b, sizeof b);
                gtk_tree_store_append(st, &g, nullptr);
                gtk_tree_store_set(st, &g, 0, b, -1);
                for (int n = 0; n < cmp::kTreeKids; ++n)
                {
                    char kb[64];
                    cmp::kidText(gi, n, kb, sizeof kb);
                    gtk_tree_store_append(st, &k, &g);
                    gtk_tree_store_set(st, &k, 0, kb, -1);
                }
            }
            treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(st));
            g_object_unref(st);
            gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeView), FALSE);
            addColumn();
            gtk_tree_view_expand_all(GTK_TREE_VIEW(treeView));
            root = scrolled(treeView);
            break;
        }
        case 'E':
        {
            size_t len = 0;
            char* t = cmp::makeText(&len);
            GtkWidget* tv = gtk_text_view_new();
            gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv)), t, static_cast<gint>(len));
            free(t);
            root = scrolled(tv);
            treeView = tv;
            break;
        }
        }
        gtk_container_add(GTK_CONTAINER(win), root);
        gtk_widget_show_all(win);
        gtk_widget_realize(win);
        GdkFrameClock* clock = gtk_widget_get_frame_clock(win);
        if (clock) g_signal_connect(clock, "after-paint", G_CALLBACK(&GtkLib::afterPaint), this);
        if (scene == 'E') gtk_widget_grab_focus(treeView);
    }

    GtkWidget* scrolled(GtkWidget* child)
    {
        GtkWidget* sw = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_kinetic_scrolling(GTK_SCROLLED_WINDOW(sw), FALSE);
        gtk_container_add(GTK_CONTAINER(sw), child);
        gtk_widget_set_size_request(sw, cmp::kW, cmp::kH);
        return sw;
    }
    void addColumn()
    {
        GtkTreeViewColumn* col = gtk_tree_view_column_new_with_attributes("Text", gtk_cell_renderer_text_new(), "text", 0, nullptr);
        gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_fixed_width(col, cmp::kW - 20);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeView), col);
        gtk_tree_view_set_fixed_height_mode(GTK_TREE_VIEW(treeView), TRUE); // the idiom for a long list: rows of one height, none measured
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeView), FALSE);
    }

    // Everything the toolkit does for what is pending, and, when that asks for a redraw, until the frame clock has painted
    // it (it paints on its own timer, at most 60 times a second, so this waits for it: wall time passes, the CPU time does not).
    void frame(cmp::FrameStats& fs)
    {
        fs = cmp::FrameStats();
        cmp::Span sp;
        sp.begin();
        cmp::regionBegin();
        GMainContext* ctx = g_main_context_default();
        const long long p0 = paints;
        while (g_main_context_pending(ctx)) g_main_context_iteration(ctx, FALSE);
        if (paints == 0)
        {
            const double deadline = cmp::wallMs() + 5000.0;
            while (paints == 0 && cmp::wallMs() < deadline) g_main_context_iteration(ctx, TRUE); // the first frame
        }
        else if (cmp::expectRedraw())
        {
            bool timedOut = false;
            const guint id = g_timeout_add(25, &GtkLib::flag, &timedOut);
            while (paints == p0 && !timedOut) g_main_context_iteration(ctx, TRUE); // sleeps until the frame clock paints
            if (!timedOut) g_source_remove(id);
        }
        cmp::regionEnd();
        if (!cmp::icountOn())
        {
            fs.guiUs = fs.totalUs = sp.cpuUs();
            fs.wallUs = sp.wallUs();
            fs.guiAllocs = fs.totalAllocs = sp.allocs();
        }
        if (clicked && paints > p0) visible = true;
    }
    void treeOp(bool open)
    {
        if (scene != 'D') return;
        if (open) gtk_tree_view_expand_all(GTK_TREE_VIEW(treeView));
        else gtk_tree_view_collapse_all(GTK_TREE_VIEW(treeView));
    }
    void natural(int seconds, long long& frames)
    {
        quitFlag = false;
        const long long p0 = paints;
        g_timeout_add_seconds(static_cast<guint>(seconds), &GtkLib::quit, this);
        gtk_main();
        frames = paints - p0;
    }
    unsigned long long drawHash() { return 0; }
    bool changed() const { return visible; }
    void resetChanged() { clicked = visible = false; }
    long long frames() const { return paints; }
    void pumpOnce() { g_main_context_iteration(g_main_context_default(), FALSE); usleep(200); }
};

int main(int argc, char** argv)
{
    cmp::t0Wall();
    cmp::t0Cpu();
    GtkLib lib;
    return cmp::run(lib, argc, argv);
}
