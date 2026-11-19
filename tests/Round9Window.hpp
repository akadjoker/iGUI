// A window with a widget of most types in it, for the tests that switch the theme at run time (tests/test_retained_round9.cpp):
// the draw data of a frame is checksummed, its fills are looked at, its text is looked at against what is under it.
// No standard library: ct and C only.
#pragma once

#include "RetainedHarness.hpp"

#include <igui/widgets/AppWidgets.hpp>
#include <igui/widgets/AssetBrowser.hpp>
#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/CodeEditor.hpp>
#include <igui/widgets/ComboBox.hpp>
#include <igui/widgets/ConsoleWidget.hpp>
#include <igui/widgets/DataWidgets.hpp>
#include <igui/widgets/DockPanel.hpp>
#include <igui/widgets/InputWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/MenuWidgets.hpp>
#include <igui/widgets/NodeEditor.hpp>
#include <igui/widgets/ScrollWidgets.hpp>
#include <igui/widgets/SideTabBar.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/ThumbnailGrid.hpp>
#include <igui/widgets/Timeline.hpp>
#include <igui/widgets/ToolBar.hpp>
#include <igui/widgets/TreePropertyColorWidgets.hpp>

namespace rh
{

// The widgets of the window, the ones a test looks at by name.
struct EveryWidget
{
    Widget* holder = nullptr;
    DockPanel* dock = nullptr;
    ConsoleWidget* console = nullptr;
    TreeView* tree = nullptr;
    ToolBar* toolbar = nullptr;
    Widget* probe = nullptr;
};

// How many times the probe widget of the window was laid out (one counter for the process).
inline int& layoutCalls()
{
    static int n = 0;
    return n;
}

// A widget that counts its layouts.
class LayoutCounter : public Widget
{
public:
    IG_WIDGET_CLASS(LayoutCounter, Widget);
    void layout() override
    {
        Widget::layout();
        ++layoutCalls();
    }
};

// What the scroll bars show in the window: `crowded` gives the console and the tree more rows than they have room for, so
// that their scroll bars are drawn.
inline EveryWidget buildEveryWidget(Harness& h, bool crowded)
{
    EveryWidget w;
    Widget* holder = h.root()->createChild<Widget>();
    holder->setRect({ 0, 0, 1280, 720 });
    w.holder = holder;

    auto at = [](Widget* wd, float x, float y, float ww, float hh) { wd->setRect({ x, y, ww, hh }); return wd; };

    // the tool bar, the menu
    w.toolbar = holder->createChild<ToolBar>();
    w.toolbar->addButton(IconId::Home, "Home");
    w.toolbar->addButton(IconId::Gear, "Settings");
    w.toolbar->addToggle(IconId::Eye, "Show", true);
    w.toolbar->addToggle(IconId::Lock, "Lock", false);
    w.toolbar->addSeparator();
    const int off = w.toolbar->addButton(IconId::Trash, "Delete");
    w.toolbar->setItemEnabled(off, false);
    at(w.toolbar, 0, 0, 1280, 34);

    // the dock: a console and a tree in tabs, a property grid split off
    w.dock = holder->createChild<DockPanel>();
    at(w.dock, 0, 36, 640, 330);
    w.console = new ConsoleWidget();
    w.console->log(LogLevel::Trace, "trace message");
    w.console->log(LogLevel::Info, "an information message");
    w.console->log(LogLevel::Warn, "a warning message");
    w.console->log(LogLevel::Error, "an error message");
    w.console->log(LogLevel::Error, "an error message");
    if (crowded)
        for (int i = 0; i < 40; ++i) w.console->logf(LogLevel::Info, "line number %d of the log", i);
    w.dock->addPanel("Console", w.console);
    w.tree = new TreeView();
    TreeNode* selectedNode = nullptr;
    for (int i = 0; i < (crowded ? 6 : 2); ++i)
    {
        TreeNode* root = w.tree->addRoot(String("Group ") + String::number(i));
        for (int j = 0; j < 4; ++j)
        {
            TreeNode* child = root->addChild(String("Child ") + String::number(j));
            if (i == 0 && j == 1) selectedNode = child;
        }
        root->setExpanded(true);
    }
    w.tree->setSelectedNode(selectedNode); // a selected row: its text over the selection fill is looked at too
    w.dock->addPanel("Tree", w.tree);
    PropertyGrid* grid = new PropertyGrid();
    grid->addSection("Transform");
    grid->addFloat("Scale", 1.5f, 0.0f, 10.0f);
    grid->addInt("Count", 4, 0, 10);
    grid->addBool("Visible", true);
    grid->addString("Name", "Cube");
    w.dock->addPanel("Properties", grid);
    w.dock->splitOff("Properties", DockSide::Right, 0.4f);

    // tabs and a splitter
    TabLayout* tabs = holder->createChild<TabLayout>();
    tabs->addTab<Label>("First", "the first tab");
    tabs->addTab<Label>("Second", "the second tab");
    at(tabs, 646, 36, 300, 100);
    Splitter* split = holder->createChild<Splitter>(LayoutDir::Horizontal);
    split->createChild<Label>("left of the splitter");
    split->createChild<Label>("right of it");
    at(split, 952, 36, 320, 100);

    // basic controls
    at(holder->createChild<Label>("A label"), 646, 144, 120, 24);
    at(holder->createChild<Button>("A button"), 772, 144, 120, 28);
    at(holder->createChild<CheckBox>("A check box"), 898, 144, 140, 24);
    at(holder->createChild<Switch>("A switch"), 1044, 144, 140, 24);
    RadioButton* radio = holder->createChild<RadioButton>("A radio button");
    at(radio, 1190, 144, 80, 24);
    at(holder->createChild<Slider>(), 646, 176, 200, 22);
    at(holder->createChild<SpinBox>(), 852, 176, 120, 26);
    ProgressBar* progress = holder->createChild<ProgressBar>();
    progress->setValue(0.6f);
    at(progress, 978, 176, 200, 22);
    at(holder->createChild<TextInput>("some text"), 646, 208, 200, 26);
    at(holder->createChild<SearchBar>(), 852, 208, 200, 26);
    ComboBox* combo = holder->createChild<ComboBox>();
    combo->addItem("one");
    combo->addItem("two");
    at(combo, 1058, 208, 140, 26);
    at(holder->createChild<TextEdit>("a text edit\nwith lines"), 646, 242, 200, 70);
    ListBox* list = holder->createChild<ListBox>();
    for (int i = 0; i < 4; ++i) list->addItem(String("item ") + String::number(i));
    list->setSelectedIndex(1);
    at(list, 852, 242, 140, 70);
    Collapsible* coll = holder->createChild<Collapsible>(String("A group"), true);
    coll->createChild<Label>("inside the group");
    at(coll, 998, 242, 270, 70);
    Breadcrumbs* crumbs = holder->createChild<Breadcrumbs>();
    at(crumbs, 646, 320, 300, 26);
    at(holder->createChild<StatusBar>(), 952, 320, 320, 26);

    // data
    DataGrid* grid2 = holder->createChild<DataGrid>();
    grid2->addColumn("Name", 90);
    grid2->addColumn("Value", 70);
    grid2->addRow({ "alpha", "1" });
    grid2->addRow({ "beta", "2" });
    grid2->setSelectedRow(1);
    at(grid2, 0, 372, 250, 110);
    TreeGrid* tg = holder->createChild<TreeGrid>();
    tg->addColumn("Name", 120);
    at(tg, 256, 372, 200, 110);
    ThumbnailGrid* thumbs = holder->createChild<ThumbnailGrid>();
    thumbs->addItem("one");
    thumbs->addItem("two");
    at(thumbs, 462, 372, 220, 110);
    AssetBrowser* assets = holder->createChild<AssetBrowser>();
    AssetItem item;
    item.name = "texture.png";
    assets->addItem(item);
    at(assets, 688, 372, 300, 110);
    SideTabBar* side = holder->createChild<SideTabBar>();
    side->addPageWidget(IconId::Home, "Home", new Label("the page"));
    at(side, 994, 372, 280, 110);

    // editors
    CodeEditor* code = holder->createChild<CodeEditor>();
    code->setHighlighter<CppHighlighter>();
    code->setText("#include <stdio.h>\nint main() { return 42; } // done\nconst char* s = \"text\";");
    at(code, 0, 488, 420, 100);
    Timeline* tl = holder->createChild<Timeline>();
    const int track = tl->addTrack("Video", TrackKind::Video);
    tl->addClip(track, 1.0f, 4.0f, "clip");
    at(tl, 426, 488, 440, 100);
    NodeEditor* nodes = holder->createChild<NodeEditor>();
    nodes->addNode("Node", 20, 20);
    at(nodes, 872, 488, 400, 100);

    LayoutCounter* probe = holder->createChild<LayoutCounter>();
    at(probe, 0, 600, 10, 10);
    w.probe = probe;

    h.relayout();
    h.frames(2);
    // a row of the property grid selected by a click on its name (the grid is the panel on the right of the dock)
    {
        const Rect g = grid->absoluteRect();
        if (g.w > 20.0f && g.h > 60.0f) h.click(g.x + 10.0f, g.y + 40.0f);
        h.moveTo(5.0f, 700.0f); // the pointer away from everything
    }
    h.frames(2);
    return w;
}

// The draw list of the last frame as one number: every vertex, index and command (what the bench's draw checksum covers,
// the texture handle and the rounded clips included).
inline uint64_t drawChecksum(uint32_t* vertices = nullptr, uint32_t* commands = nullptr)
{
    const DrawList& dl = GetDrawList();
    uint64_t hash = 1469598103934665603ull;
    auto mix = [&hash](const void* p, size_t n)
    {
        const unsigned char* b = static_cast<const unsigned char*>(p);
        for (size_t i = 0; i < n; ++i) hash = (hash ^ b[i]) * 1099511628211ull;
    };
    mix(dl.vertices().data(), dl.vertices().size() * sizeof(DrawVertex));
    mix(dl.indices().data(), dl.indices().size() * sizeof(uint32_t));
    for (size_t i = 0; i < dl.commands().size(); ++i)
    {
        const DrawCmd& c = dl.commands()[i];
        mix(&c.texture, sizeof c.texture);
        mix(&c.indexOffset, sizeof c.indexOffset);
        mix(&c.indexCount, sizeof c.indexCount);
        mix(&c.clip, sizeof c.clip);
        mix(&c.roundClipCount, sizeof c.roundClipCount);
    }
    if (vertices) *vertices = static_cast<uint32_t>(dl.vertices().size());
    if (commands) *commands = static_cast<uint32_t>(dl.commands().size());
    return hash;
}

} // namespace rh
