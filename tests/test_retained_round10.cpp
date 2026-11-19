// Round 10, part 2 (docs/ROUND10.md): the small widget items.
//   - TreeView: a label that does not fit ends in "..." (the same pixels as a label already cut to that room), the second dim text
//     of a row is right-aligned and keeps its room, the measure is kept (no heap in a steady frame) and made again when the width,
//     the font or the theme changes; the icon of a node takes the theme's iconColor, or the tint given with setIconColor;
//   - TreeGrid: nodeActivated on a double click and on Enter in a grid that does not edit, never in one that does, a handler may take
//     the grid out of the tree; the icon tint of a node;
//   - FileDialog in Save mode: the question before replacing a file, the filter's extension appended to a name that has none (and the
//     question asked about that name), the folder remembered by dialog id (opt-in);
//   - the selected row of a ListBox, a ComboBox list and a ListWidget is read in selectionText.
// No standard library: ct and C only.

#include "AllocCounter.hpp"
#include "RasterScreen.hpp"
#include "Round9Window.hpp"

#include <igui/widgets/FileDialog.hpp>
#include <igui/widgets/FileSystem.hpp>

#include <sys/stat.h>
#include <unistd.h>

using namespace rh;

namespace
{

Widget* holderOf(Harness& h, float w, float hh)
{
    Widget* holder = h.root()->createChild<Widget>();
    holder->setRect({ 0, 0, w, hh });
    return holder;
}

// the pixels of a rectangle that are not the same in two shots, and the columns they reach
int differing(const Shot& a, const Shot& b, const Rect& r)
{
    return rh::differing(a, b, static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.x + r.w), static_cast<int>(r.y + r.h));
}

// the last column of a rectangle (x0..x1, y0..y1) with a pixel that is not `bg`; -1 when there is none
int lastInk(const Shot& s, int x0, int y0, int x1, int y1, uint32_t bg)
{
    int last = -1;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (s.px[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)] != bg && x > last) last = x;
    return last;
}
int firstInk(const Shot& s, int x0, int y0, int x1, int y1, uint32_t bg)
{
    for (int x = x0; x < x1; ++x)
        for (int y = y0; y < y1; ++y)
            if (s.px[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)] != bg) return x;
    return -1;
}

constexpr const char* kLong = "A very long node text that cannot possibly fit in this narrow tree view";

// A tree view of one row per entry of `labels` (rows of 24 pixels), `width` wide.
struct TreeScene
{
    Screen screen;
    TreeView* tree = nullptr;
    TreeScene(float width, const char* const* labels, int n, float windowWidth = 0.0f) : screen(windowWidth > 0.0f ? windowWidth : width + 20.0f, 200.0f)
    {
        Theme::instance() = Theme::dark();
        Harness& h = screen.h;
        Widget* holder = holderOf(h, windowWidth > 0.0f ? windowWidth : width + 20.0f, 200.0f);
        tree = holder->createChild<TreeView>();
        for (int i = 0; i < n; ++i) tree->addRoot(labels[i]);
        tree->setRect({ 10, 10, width, 180 });
        h.relayout();
        h.frames(2);
    }
    Rect row(int i) const
    {
        const Rect a = tree->absoluteRect();
        return { a.x + 1.0f, a.y + 24.0f * static_cast<float>(i), a.w - 2.0f, 24.0f };
    }
};

// One scene at a time (a Harness takes the window of the one before): the pixels of a tree of these labels, at a width, a font size
// (0: the theme's) and a preset.
Shot treeShot(float width, const char* const* labels, int n, float fontSize = 0.0f, const char* preset = "dark", float windowWidth = 0.0f)
{
    TreeScene s(width, labels, n, windowWidth);
    Theme::instance() = Theme::byName(preset);
    if (fontSize > 0.0f) Theme::instance().fontSize = fontSize;
    s.screen.h.app.notifyThemeChanged();
    s.screen.h.relayout();
    s.screen.h.frames(2);
    return s.screen.take();
}

// the label cut to a room at a font size, as the painting context cuts it
String cutTo(const char* text, float room, float fontSize)
{
    TreeScene s(100.0f, nullptr, 0);
    PaintContext pc(GetDrawList(), s.screen.h.font());
    pc.font.SetFontSize(fontSize);
    return pc.fitText(String(text), room);
}

Rect rowOf(float width, int i)
{
    return { 11.0f, 10.0f + 24.0f * static_cast<float>(i), width - 2.0f, 24.0f };
}

void testTreeEllipsis()
{
    printf("-- TreeView: a label that does not fit ends in an ellipsis\n");
    const float width = 200.0f;
    const char* labels[] = { kLong, "short" };
    // the room of the label of a root without children, with no scroll bar: from 4 + 11 to 4 before the edge
    const String cut = cutTo(kLong, width - 4.0f - 15.0f, Theme::dark().fontSize);
    check(cut.size() < strlen(kLong) && cut.size() > 3 && cut.find(String("...")) != String::npos, "(the label is too wide for the room: fitText cuts it and ends it in dots)");

    // the widget as it changes, kept
    Shot wide, narrow, big, light;
    {
        TreeScene s(width, labels, 2);
        Harness& h = s.screen.h;
        wide = s.screen.take();
        s.tree->setRect({ 10, 10, 140, 180 });
        h.relayout();
        h.frames(2);
        narrow = s.screen.take();
        Theme::instance().fontSize = 20.0f;
        h.app.notifyThemeChanged();
        h.relayout();
        h.frames(2);
        big = s.screen.take();
        Theme::instance() = Theme::light();
        h.app.notifyThemeChanged();
        h.relayout();
        h.frames(2);
        light = s.screen.take();
    }
    Theme::instance() = Theme::dark();
    const char* control[] = { cut.c_str(), "short" };
    const Shot expect = treeShot(width, control, 2);
    char what[160];
    snprintf(what, sizeof what, "the row of a long label draws the pixels of the label already cut to the room (%d differ)", differing(wide, expect, rowOf(width, 0)));
    check(differing(wide, expect, rowOf(width, 0)) == 0, what);
    check(differing(wide, expect, rowOf(width, 1)) == 0, "a short label is untouched");
    // (what the panel of the tree is: a pixel below the rows)
    const uint32_t bg = wide.px[static_cast<size_t>(110) * static_cast<size_t>(wide.w) + static_cast<size_t>(10.0f + width - 8.0f)];
    const int last = lastInk(wide, 12, 12, static_cast<int>(width) + 8, 32, bg);
    check(last >= 0 && static_cast<float>(last) <= 10.0f + width - 4.0f, "the label ends before the right edge (it was cut by the panel's edge before)");

    // the width changes: the label is cut to the new room
    const String cutNarrow = cutTo(kLong, 140.0f - 19.0f, Theme::dark().fontSize);
    check(cutNarrow.size() < cut.size(), "(a narrower room cuts more)");
    const char* controlNarrow[] = { cutNarrow.c_str(), "short" };
    snprintf(what, sizeof what, "a changed width cuts the label to the new room (the kept measure is made again): %d differ", differing(narrow, treeShot(140.0f, controlNarrow, 2, 0.0f, "dark", 220.0f), rowOf(140.0f, 0)));
    check(differing(narrow, treeShot(140.0f, controlNarrow, 2, 0.0f, "dark", 220.0f), rowOf(140.0f, 0)) == 0, what);
    // the theme's font size changes: measured again
    const String cutBig = cutTo(kLong, 140.0f - 19.0f, 20.0f);
    const char* controlBig[] = { cutBig.c_str(), "short" };
    check(differing(big, treeShot(140.0f, controlBig, 2, 20.0f, "dark", 220.0f), rowOf(140.0f, 0)) == 0, "a changed font size cuts the label to what that font fits");
    // a theme change: the colours follow, and the cut is the one of the room
    const char* controlLight[] = { cutNarrow.c_str(), "short" }; // (the light preset's own font size: 16, as the dark one)
    check(differing(light, treeShot(140.0f, controlLight, 2, 0.0f, "light", 220.0f), rowOf(140.0f, 0)) == 0, "a theme change repaints the row as a fresh tree of the new theme");
    check(differing(light, big, rowOf(140.0f, 0)) > 0, "(and it is a different picture from the dark one)");
}

// the steady frame of a tree whose labels are cut allocates nothing: the measure is kept
void testTreeCacheAllocations()
{
    printf("-- TreeView: the measure of the labels is kept\n");
    const char* labels[] = { kLong, "short", kLong, kLong };
    TreeScene s(200.0f, labels, 4);
    s.tree->roots()[0]->setSecondaryText("12 items");
    Harness& h = s.screen.h;
    h.frames(6);
    const unsigned long long before = gAllocs;
    h.frames(20);
    char what[120];
    snprintf(what, sizeof what, "20 frames of a tree with cut labels and a second text allocate nothing (%llu allocations)", gAllocs - before);
    check(gAllocs == before, what);
}

void testTreeSecondaryText()
{
    printf("-- TreeView: a second, dim, right-aligned text\n");
    const char* labels[] = { "plain", "has a count", kLong };
    TreeScene s(240.0f, labels, 3);
    Harness& h = s.screen.h;
    s.tree->roots()[1]->setSecondaryText("42 items");
    s.tree->roots()[2]->setSecondaryText("7");
    h.frames(2);
    const Shot shot = s.screen.take();
    const uint32_t bg = shot.px[static_cast<size_t>(130) * static_cast<size_t>(shot.w) + 10 + 240 - 10]; // (the panel of the tree, below the rows)
    const Rect abs = s.tree->absoluteRect();
    const int x0 = static_cast<int>(abs.x) + 2, x1 = static_cast<int>(abs.x + abs.w) - 2;
    const Rect r1 = s.row(1);
    const int y0 = static_cast<int>(r1.y) + 2, y1 = static_cast<int>(r1.y + r1.h) - 2;
    // the second text ends at the right edge of the room (4 pixels before the edge) and is away from the label
    const int last = lastInk(shot, x0, y0, x1, y1, bg);
    check(last > static_cast<int>(abs.x + abs.w) - 12 && static_cast<float>(last) <= abs.x + abs.w - 4.0f, "the second text ends at the right edge of the row");
    // the ink of the label (from the left) and of the second text (from the right) are separate: a gap between them
    int gapStart = -1, gapEnd = -1;
    for (int x = x0 + 20; x < x1; ++x)
    {
        bool ink = false;
        for (int y = y0; y < y1; ++y) ink = ink || shot.px[static_cast<size_t>(y) * static_cast<size_t>(shot.w) + static_cast<size_t>(x)] != bg;
        if (!ink && gapStart < 0) gapStart = x;
        if (ink && gapStart >= 0 && gapEnd < 0 && x > gapStart + 6) gapEnd = x;
    }
    check(gapStart > 0 && gapEnd > gapStart + 6, "a gap between the label and the second text");
    // the long label of the third row gives way: it ends before the second text starts, with a gap
    const Rect r2 = s.row(2);
    const int ly0 = static_cast<int>(r2.y) + 2, ly1 = static_cast<int>(r2.y + r2.h) - 2;
    const int lastLabel = lastInk(shot, x0, ly0, x0 + static_cast<int>(abs.w) - 40, ly1, bg);
    const int secondStart = firstInk(shot, x1 - 30, ly0, x1, ly1, bg);
    check(secondStart > 0 && lastLabel > 0 && lastLabel < secondStart - 4, "a long label is cut so that it does not touch the second text");
    // the colour of the second text is the theme's textSecondary (dimmer than the label's), and legible
    const Theme& t = Theme::instance();
    bool dim = false;
    const DrawList& dl = GetDrawList();
    for (size_t k = 0; k < dl.vertices().size(); ++k)
    {
        const DrawVertex& v = dl.vertices()[k];
        if (v.y > r1.y && v.y < r1.y + r1.h && v.x > abs.x + abs.w - 60 && v.color.r == t.textSecondary.r && v.color.g == t.textSecondary.g && v.color.b == t.textSecondary.b) dim = true;
    }
    check(dim, "the second text is drawn in the theme's textSecondary");
    // the presets: the second text is read (4.5 against the panel) in all of them
    for (int i = 0; i < 5; ++i)
    {
        const Theme p = Theme::byName(Theme::presetName(i));
        auto lum = [](const Color& col)
        {
            auto ch = [](unsigned char v) { const float f = static_cast<float>(v) / 255.0f; return f <= 0.03928f ? f / 12.92f : powf((f + 0.055f) / 1.055f, 2.4f); };
            return 0.2126f * ch(col.r) + 0.7152f * ch(col.g) + 0.0722f * ch(col.b);
        };
        const float a = lum(p.textSecondary), b = lum(p.panelColor);
        const float ratio = (maxf(a, b) + 0.05f) / (minf(a, b) + 0.05f);
        char what[140];
        snprintf(what, sizeof what, "%s: textSecondary against the panel is %.2f (4.5 or more)", Theme::presetName(i), ratio);
        check(ratio >= 4.5f, what);
    }
    // an empty second text is none; changing it repaints
    s.tree->roots()[1]->setSecondaryText("");
    h.frames(2);
    const Shot none = s.screen.take();
    check(lastInk(none, x0 + 100, y0, x1, y1, bg) < 0, "an empty second text draws nothing");
}

bool seenColour(const DrawList& dl, const Rect& r, const Color& col)
{
    for (size_t k = 0; k < dl.vertices().size(); ++k)
    {
        const DrawVertex& v = dl.vertices()[k];
        if (v.x >= r.x && v.x <= r.x + r.w && v.y >= r.y && v.y <= r.y + r.h && v.color.r == col.r && v.color.g == col.g && v.color.b == col.b && v.color.a > 0) return true;
    }
    return false;
}

void testIconTint()
{
    printf("-- the icon of a row: the theme's colour unless a tint is given\n");
    Theme::instance() = Theme::dark();
    Harness h(300, 200);
    Widget* holder = holderOf(h, 300, 200);
    TreeView* tree = holder->createChild<TreeView>();
    TreeNode* node = tree->addRoot("folder");
    node->setIconId(IconId::Folder);
    tree->setRect({ 10, 10, 200, 100 });
    TreeGrid* grid = holder->createChild<TreeGrid>();
    grid->addColumn("Name", 150);
    TreeGrid::Node* gnode = grid->addRoot({ "gridfolder" });
    gnode->iconId = IconId::Folder;
    grid->setRect({ 10, 120, 200, 70 });
    h.relayout();
    h.frames(3);
    const Theme dark = Theme::dark();
    const Rect treeIcon = { 10, 10, 60, 24 };
    const Rect gridIcon = { 10, 120, 80, 50 };
    check(seenColour(GetDrawList(), treeIcon, dark.iconColor), "the icon of a node follows the theme's iconColor by default (dark: white)");
    Theme::instance() = Theme::light();
    h.app.notifyThemeChanged();
    h.frames(3);
    check(seenColour(GetDrawList(), treeIcon, Theme::light().iconColor) && !seenColour(GetDrawList(), treeIcon, dark.iconColor),
            "... and follows it when the theme changes, with no row rebuilt");
    node->setIconColor(Color(220, 30, 30, 255));
    h.frames(3);
    check(seenColour(GetDrawList(), treeIcon, Color(220, 30, 30, 255)), "setIconColor gives the icon its own tint");
    Theme::instance() = Theme::dark();
    h.app.notifyThemeChanged();
    h.frames(3);
    check(seenColour(GetDrawList(), treeIcon, Color(220, 30, 30, 255)) && !seenColour(GetDrawList(), treeIcon, dark.iconColor), "... which a theme change does not touch");
    node->clearIconColor();
    h.frames(3);
    check(seenColour(GetDrawList(), treeIcon, dark.iconColor) && !seenColour(GetDrawList(), treeIcon, Color(220, 30, 30, 255)), "clearIconColor follows the theme again");
    // the tree grid
    check(seenColour(GetDrawList(), gridIcon, dark.iconColor), "a TreeGrid node's icon follows the theme by default");
    gnode->iconColor = Color(30, 160, 30, 255);
    grid->rebuild();
    h.frames(3);
    check(seenColour(GetDrawList(), gridIcon, Color(30, 160, 30, 255)), "TreeGrid::Node::iconColor tints it");
    // a setter that changes what is drawn paints a frame (idle frames): the node tells its view
    h.frames(3);
    const uint32_t before = gWidgetChanges;
    node->setSecondaryText("x");
    check(gWidgetChanges != before, "a node's setter marks its view (a frame is painted for it)");
    Theme::instance() = Theme::dark();
}

// ── TreeGrid activation ─────────────────────────────────────────────────────────────────────────────────────────────────────

void testTreeGridActivation()
{
    printf("-- TreeGrid: nodeActivated\n");
    Theme::instance() = Theme::dark();
    Harness h(400, 260);
    Widget* holder = holderOf(h, 400, 260);
    TreeGrid* grid = holder->createChild<TreeGrid>();
    grid->addColumn("Name", 160);
    grid->addColumn("Size", 80);
    grid->setReadOnly(true);
    TreeGrid::Node* a = grid->addRoot({ "alpha", "1" });
    TreeGrid::Node* b = grid->addRoot({ "beta", "2" });
    a->addChild({ "child", "3" });
    grid->rebuild();
    grid->setRect({ 10, 10, 360, 200 });
    h.relayout();
    h.frames(3);
    TreeGrid::Node* seen = nullptr;
    int column = -2, count = 0;
    grid->nodeActivated.connect([&](TreeGrid::Node* n, int col) { seen = n; column = col; ++count; });
    const Rect abs = grid->absoluteRect();
    // header row is 28 or so: find the rows by the widget's own answer (the second root is the third row after a child: alpha, child, beta)
    const float rowY = abs.y + 44.0f; // below the header, inside the first row
    h.click(abs.x + 200.0f, rowY);
    check(count == 0, "one click does not activate");
    h.click(abs.x + 200.0f, rowY);
    check(count == 1 && seen == a && column == 1, "a double click on a read-only grid activates the node, with the column");
    // Enter: the selected node, column 0
    h.app.setFocused(grid);
    h.key(ig::retained::Key::Down);
    check(grid->selectedNode() != a, "(the arrow moved the selection)");
    TreeGrid::Node* selected = grid->selectedNode();
    h.key(ig::retained::Key::Return);
    check(count == 2 && seen == selected && column == 0, "Enter activates the selected node (column 0)");
    (void) b;
    // an editable grid edits, and does not activate
    grid->setReadOnly(false);
    h.frames(2);
    const int before = count;
    h.click(abs.x + 30.0f, rowY);
    h.click(abs.x + 30.0f, rowY);
    check(count == before, "a double click that starts an edit is not an activation");
    h.key(ig::retained::Key::Escape);
    // a read-only column in an editable grid
    grid->setColumnReadOnly(1, true);
    h.frames(2);
    h.click(abs.x + 200.0f, rowY);
    h.click(abs.x + 200.0f, rowY);
    check(count == before + 1 && column == 1, "a double click on a read-only column activates");

}

void testTreeGridHandlerRemovesGrid()
{
    printf("-- TreeGrid: a handler that takes the grid out of the tree\n");
    Theme::instance() = Theme::dark();
    {
        Harness g(400, 260);
        Widget* hold = holderOf(g, 400, 260);
        TreeGrid* doomed = hold->createChild<TreeGrid>();
        doomed->addColumn("Name", 160);
        doomed->setReadOnly(true);
        doomed->addRoot({ "one" });
        doomed->rebuild();
        doomed->setRect({ 10, 10, 360, 200 });
        g.relayout();
        g.frames(3);
        int calls = 0;
        doomed->nodeActivated.connect([&](TreeGrid::Node*, int) { ++calls; hold->removeChild(doomed); });
        const Rect r = doomed->absoluteRect();
        g.click(r.x + 20.0f, r.y + 44.0f);
        g.click(r.x + 20.0f, r.y + 44.0f);
        g.frames(3);
        check(calls == 1 && hold->children().empty(), "a handler of nodeActivated may take the grid out of the tree");
    }
}

// ── FileDialog, Save mode ───────────────────────────────────────────────────────────────────────────────────────────────────────

struct TempTree
{
    char root[64];
    String path(const char* name) const
    {
        return String(root) + "/" + name;
    }
    TempTree()
    {
        snprintf(root, sizeof root, "/tmp/igui_round10_XXXXXX");
        if (!mkdtemp(root)) root[0] = 0;
        if (root[0])
        {
            FILE* f = fopen(path("exists.txt").c_str(), "wb");
            if (f) fclose(f);
            f = fopen(path("old.png").c_str(), "wb");
            if (f) fclose(f);
            mkdir(path("sub").c_str(), 0700);
            mkdir(path("other").c_str(), 0700);
        }
    }
    ~TempTree()
    {
        if (!root[0]) return;
        unlink(path("exists.txt").c_str());
        unlink(path("old.png").c_str());
        unlink(path("picture.png").c_str());
        rmdir(path("sub").c_str());
        rmdir(path("other").c_str());
        rmdir(root);
    }
};

struct SaveCase
{
    Harness h;
    FileDialog* fd = nullptr;
    int accepted = 0;
    String path;
    SaveCase(const TempTree& tmp, const char* filter) : h(900, 600)
    {
        fd = h.app.addFloat<FileDialog>("Save as");
        fd->setMode(FileDialog::Mode::Save);
        fd->setPath(tmp.root);
        if (filter) fd->setFilter(filter);
        fd->accepted.connect([this](const String& p) { ++accepted; path = p; });
        h.frames(3);
    }
    // type the name and press Enter in the name field
    void enter(const char* name)
    {
        fd->setFileName(name);
        h.app.setFocused(fd->focusOnOpen());
        h.frames(1);
        h.key(ig::retained::Key::Return);
        h.frames(1);
    }
};

void testFileDialogSave()
{
    printf("-- FileDialog, Save mode\n");
    TempTree tmp;
    check(tmp.root[0] != 0, "(a scratch folder was made)");
    {
        SaveCase c(tmp, "*.png;*.jpg");
        c.enter("picture");
        check(c.accepted == 1 && c.path == tmp.path("picture.png"), "a name with no extension gets the first extension of the filter (.png)");
    }
    {
        SaveCase c(tmp, "*.png;*.jpg");
        c.enter("photo.jpg");
        check(c.accepted == 1 && c.path == tmp.path("photo.jpg"), "a name that has an extension is left as it is");
    }
    {
        SaveCase c(tmp, "*.png;*.jpg");
        c.fd->setDefaultExtension("jpg");
        c.enter("photo");
        check(c.accepted == 1 && c.path == tmp.path("photo.jpg"), "setDefaultExtension picks the extension appended");
    }
    {
        SaveCase c(tmp, "*.png");
        c.fd->setAppendExtension(false);
        c.enter("photo");
        check(c.accepted == 1 && c.path == tmp.path("photo"), "setAppendExtension(false): the name as typed");
    }
    {
        SaveCase c(tmp, nullptr);
        c.enter("photo");
        check(c.accepted == 1 && c.path == tmp.path("photo"), "with no filter nothing is appended");
    }
    {
        // the question before replacing: asked about the name that will be written (with the extension appended)
        SaveCase c(tmp, "*.txt");
        c.enter("exists");
        check(c.accepted == 0, "an existing file is not replaced at the first Enter (the extension was appended, then the file found)");
        c.h.key(ig::retained::Key::Return);
        c.h.frames(1);
        check(c.accepted == 1 && c.path == tmp.path("exists.txt"), "... and is at the second, which is the answer 'replace'");
    }
    {
        // a different name after the question is a different question
        SaveCase c(tmp, "*.png");
        c.enter("old");
        check(c.accepted == 0, "old.png exists: the question is asked");
        c.enter("fresh");
        check(c.accepted == 1 && c.path == tmp.path("fresh.png"), "typing another name drops the question: the new name is saved at once");
    }
    {
        // Open mode never appends
        Harness h(900, 600);
        FileDialog* fd = h.app.addFloat<FileDialog>("Open");
        fd->setMode(FileDialog::Mode::Open);
        fd->setPath(tmp.root);
        fd->setFilter("*.png");
        int accepted = 0;
        String path;
        fd->accepted.connect([&](const String& p) { ++accepted; path = p; });
        h.frames(3);
        fd->setFileName("exists.txt");
        h.app.setFocused(fd->focusOnOpen());
        h.key(ig::retained::Key::Return);
        check(accepted == 1 && path == tmp.path("exists.txt"), "Open mode opens the name as typed");
    }
    {
        // the folder remembered by dialog id, opt-in
        FileDialog::setLastFolder("round10-export", "");
        {
            SaveCase c(tmp, "*.png");
            c.fd->setRememberFolder("round10-export");
            c.fd->setPath(tmp.path("sub"));
            c.h.frames(2);
            c.enter("remembered");
            check(c.accepted == 1 && FileDialog::lastFolder("round10-export") == tmp.path("sub"), "an accepted dialog remembers its folder under its id");
            unlink(tmp.path("sub/remembered.png").c_str());
        }
        {
            SaveCase c(tmp, "*.png");
            check(c.fd->path() == String(tmp.root), "(a dialog that did not ask to remember starts where it was told)");
            c.fd->setRememberFolder("round10-export");
            c.h.frames(2);
            check(c.fd->path() == tmp.path("sub"), "a dialog with the same id starts in the remembered folder");
        }
        {
            SaveCase c(tmp, "*.png");
            c.fd->setRememberFolder("round10-other-id");
            c.h.frames(2);
            check(c.fd->path() == String(tmp.root), "another id has its own (nothing yet: it keeps where it was)");
            c.fd->setPath(tmp.path("other"));
            c.enter("x");
            check(FileDialog::lastFolder("round10-other-id") == tmp.path("other") && FileDialog::lastFolder("round10-export") == tmp.path("sub"), "ids do not share a folder");
            unlink(tmp.path("other/x.png").c_str());
        }
        {
            SaveCase c(tmp, "*.png");
            c.fd->setPath(tmp.path("other"));
            c.enter("y");
            check(FileDialog::lastFolder("") == String(), "no id: nothing remembered");
            unlink(tmp.path("other/y.png").c_str());
        }
    }
}

// ── The selected row of a list ──────────────────────────────────────────────────────────────────────────────────────────────

void testListSelectionText()
{
    printf("-- a selected row of a list is read in selectionText\n");
    for (int i = 0; i < 5; ++i)
    {
        Theme::instance() = Theme::byName(Theme::presetName(i));
        const Theme& t = Theme::instance();
        Harness h(500, 300);
        Widget* holder = holderOf(h, 500, 300);
        ListBox* list = holder->createChild<ListBox>();
        for (int k = 0; k < 4; ++k) list->addItem(String("item ") + String::number(k));
        list->setSelectedIndex(1);
        list->setRect({ 10, 10, 140, 100 });
        ListWidget* rows = holder->createChild<ListWidget>();
        for (int k = 0; k < 3; ++k) rows->addRow<Label>(String("row ") + String::number(k));
        rows->setSelectedIndex(1);
        rows->setRect({ 170, 10, 140, 100 });
        ComboBox* combo = holder->createChild<ComboBox>();
        combo->addItem("one");
        combo->addItem("two");
        combo->setSelectedIndex(1);
        combo->setRect({ 330, 10, 140, 26 });
        h.relayout();
        h.frames(3);
        char what[160];
        // ListBox: its second row (y = 10 + 1 row)
        auto seenIn = [&](const Rect& r, const Color& col) { return seenColour(GetDrawList(), r, col); };
        const Rect listRow = { 10, 10, 140, 100 }, rowsRow = { 170, 10, 140, 100 };
        snprintf(what, sizeof what, "%s: the selected row of a ListBox has selectionText", Theme::presetName(i));
        check(seenIn(listRow, t.selectionText), what);
        snprintf(what, sizeof what, "%s: the selected row of a ListWidget (Label rows) has selectionText", Theme::presetName(i));
        check(seenIn(rowsRow, t.selectionText), what);
        combo->openDropdown();
        h.frames(14);
        Widget* popup = h.app.popup();
        check(popup != nullptr, "the combo's list opens");
        if (popup)
        {
            snprintf(what, sizeof what, "%s: the selected row of a ComboBox list has selectionText", Theme::presetName(i));
            check(seenIn(popup->absoluteRect(), t.selectionText), what);
        }
        h.app.closePopup();
        h.frames(14);
    }
    Theme::instance() = Theme::dark();
}

} // namespace

int main()
{
    testTreeEllipsis();
    testTreeCacheAllocations();
    testTreeSecondaryText();
    testIconTint();
    testTreeGridActivation();
    testTreeGridHandlerRemovesGrid();
    testFileDialogSave();
    testListSelectionText();
    Theme::instance() = Theme::dark();
    shutdown();
    if (failures() == 0) printf("PASS\n");
    return failures() == 0 ? 0 : 1;
}
