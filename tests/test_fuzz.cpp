// Randomised input fuzzing for the immediate-mode Context.
//
// Every frame feeds a burst of random events (moves, presses, drags, wheel,
// keys, text - including invalid UTF-8 -, viewport changes, focus loss) into
// a scene that submits every immediate widget across overlapping windows,
// with modals, menus and popups toggled on and off at random. After each
// frame the DrawData and the widget values are checked against invariants.
// Run under ASan/UBSan this finds memory errors and undefined behaviour the
// scripted tests never reach. Seeds are fixed, so a failure reproduces:
//   igui_fuzz_tests <seed> <frames>

#include "InteractionHarness.hpp"

#include <igui/CodeEditor.hpp>
#include <igui/FileDialog.hpp>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{

uint32_t gState = 1u;

uint32_t next()
{
    // xorshift32
    gState ^= gState << 13;
    gState ^= gState >> 17;
    gState ^= gState << 5;
    return gState;
}

int pick(int count)
{
    return static_cast<int>(next() % static_cast<uint32_t>(count));
}

float range(float minimum, float maximum)
{
    return minimum + (maximum - minimum) * static_cast<float>(next() % 100000u) / 100000.0f;
}

class Provider : public ig::FileDialogProvider
{
public:
    bool listDirectory(ig::StringView path, ct::Vector<ig::FileDialogEntry> &entries, bool = true) override
    {
        for (int i = 0; i < 30; ++i)
        {
            ig::FileDialogEntry entry;
            char name[32];
            snprintf(name, sizeof(name), i % 4 == 0 ? "folder%d" : "file%d.png", i);
            entry.name = name;
            entry.path = ig::String(path.data(), path.size()) + "/" + entry.name;
            entry.directory = i % 4 == 0;
            entry.size = static_cast<uint64_t>(i) * 1000u;
            entries.push_back(entry);
        }
        return true;
    }
    ig::String parentDirectory(ig::StringView) override { return "/"; }
    ig::String homeDirectory() override { return "/home/fuzz"; }
    ig::String createDirectory(ig::StringView path, ig::StringView name) override
    {
        return ig::String(path.data(), path.size()) + "/" + ig::String(name.data(), name.size());
    }
};

struct Scene
{
    // Values the widgets edit.
    bool flags[8] = {};
    int radio = 0;
    float sliderF = 0.5f;
    int sliderI = 5;
    float dragF = 0.0f;
    int dragI = 0;
    int stepper = 0;
    float split = 100.0f;
    int combo = 0;
    int list = 0;
    int tab = 0;
    ig::String text = "hello";
    ig::String multiline = "line one\nline two\nline three";
    int inputI = 0;
    float inputF = 0.0f;
    ig::Color color = ig::Color(100, 150, 200, 255);
    ct::Vector<ig::GradientStop> stops;
    int selectedStop = -1;
    ct::Vector<ig::CurvePoint> curve;
    int selectedPoint = -1;
    ct::Vector<ig::TimelineTrack> timeline;
    int timelineFrame = 0;
    int timelineTrack = -1;
    int timelineKey = -1;
    ct::Vector<ig::SequencerTrack> sequence;
    int sequenceFrame = 0;
    int sequenceTrack = -1;
    ig::CodeEditorState code;
    ig::Transform2D t2;
    ig::Transform3D t3;
    float weights[3] = {1.0f, 2.0f, 1.0f};
    int sortColumn = -1;
    bool sortAscending = true;
    int dropped = 0;
    ig::Transform3D t3b;
    int history = 0;
    bool leftPanel = true;
    bool bottomPanel = true;
    float property = 0.0f;

    // Blockers and windows.
    bool message = false;
    bool dialog = false;
    bool files = false;
    bool modalTool = false;
    bool inputMessage = false;
    ig::String messageInput;
    bool toolOpen = true;
    bool panelOpen = true;
    ig::FileDialogState fileState;
    Provider provider;

    Scene()
    {
        stops.push_back(ig::GradientStop(0.0f, ig::Color(0, 0, 0, 255)));
        stops.push_back(ig::GradientStop(1.0f, ig::Color(255, 255, 255, 255)));
        curve.push_back(ig::CurvePoint(0.0f, 0.0f));
        curve.push_back(ig::CurvePoint(1.0f, 1.0f));
        ig::TimelineTrack track;
        track.label = "track";
        track.keys.push_back(10);
        timeline.push_back(track);
        timeline.push_back(track);
        sequence.push_back(ig::SequencerTrack("clip", 5, 30, ig::Color(90, 160, 230)));
        code.setText("int main()\n{\n    return 0;\n}\n");
        code.setHighlighterForFile("a.cpp");
    }
};

const ig::StringView kItems[] = {"alpha", "beta", "gamma", "delta", "epsilon"};
const ig::StringView kTabs[] = {"one", "two", "three"};

void drawScene(ig::Context &c, Scene &s)
{
    const ig::Span<const ig::StringView> items(kItems, 5);
    const ig::Span<const ig::StringView> tabs(kTabs, 3);

    if (c.beginWindow("Basics", ig::Rect(10.0f, 10.0f, 420.0f, 560.0f)))
    {
        if (c.beginMenuBar(ig::Rect(0.0f, 0.0f, 380.0f, 24.0f)))
        {
            if (c.beginMenu("File"))
            {
                if (c.menuItem("Message"))
                    s.message = true;
                if (c.menuItem("Dialog"))
                    s.dialog = true;
                if (c.beginSubMenu("More"))
                {
                    if (c.menuItem("Files"))
                        s.files = true;
                    c.menuCheckbox("Flag", s.flags[7]);
                    c.endSubMenu();
                }
                c.menuSeparator();
                c.menuItem("Disabled", false);
                c.endMenu();
            }
            if (c.beginMenu("Edit"))
            {
                if (c.menuItem("Undo"))
                    c.undo();
                c.endMenu();
            }
            c.endMenuBar();
        }
        c.setCursor(ig::Vec2(0.0f, 30.0f));
        if (c.button("Button"))
            s.message = !s.message;
        c.sameLine();
        c.smallButton("small");
        c.checkbox("check", s.flags[0]);
        c.toggleSwitch("toggle", s.flags[1]);
        for (int i = 0; i < 3; ++i)
        {
            c.pushId(static_cast<uint64_t>(i));
            if (c.radioButton("radio", s.radio == i))
                s.radio = i;
            c.popId();
        }
        c.selectable("selectable", s.flags[2]);
        c.sliderFloat("sf", s.sliderF, 0.0f, 1.0f);
        c.sliderInt("si", s.sliderI, 0, 10);
        c.dragFloat("df", s.dragF, -100.0f, 100.0f, 0.5f);
        c.dragInt("di", s.dragI, -50, 50, 1);
        c.stepperInt("st", s.stepper, -5, 5);
        c.comboBox("combo", s.combo, items);
        c.tabBar("tabs", s.tab, tabs);
        c.inputText("text", s.text);
        c.inputInt("int", s.inputI);
        c.inputFloat("float", s.inputF);
        if (c.collapsingHeader("header", s.flags[3]))
        {
            c.indent();
            c.label("inside header");
            c.progressBar(s.sliderF, 1.0f);
            c.unindent();
        }
        if (c.treeNode("tree", s.flags[4]))
        {
            ig::TreeItemStyle style;
            bool expanded = false;
            c.treeItem("leaf", expanded, style);
        }
        c.separatorText("section");
        c.listBox("list", s.list, items, 0.0f);
        c.tooltip("a tooltip");
        if (c.beginContextMenu("ctx", ig::Rect(0.0f, 0.0f, 400.0f, 500.0f)))
        {
            c.menuItem("Context item");
            c.endContextMenu();
        }
        c.endWindow();
    }

    if (c.beginWindow("Editors", ig::Rect(300.0f, 60.0f, 520.0f, 620.0f)))
    {
        c.inputTextMultiline("ml", s.multiline, ig::Rect(0.0f, 0.0f, 480.0f, 80.0f));
        c.codeEditor("code", s.code, ig::Rect(0.0f, 90.0f, 480.0f, 120.0f));
        c.colorEdit("color", s.color, ig::Rect(0.0f, 220.0f, 240.0f, 110.0f));
        c.gradientEditor("gradient", s.stops, s.selectedStop, ig::Rect(250.0f, 220.0f, 230.0f, 40.0f));
        c.curveEditor("curve", s.curve, s.selectedPoint, ig::Rect(250.0f, 270.0f, 230.0f, 100.0f));
        c.timeline("timeline", s.timeline, 0, 100, s.timelineFrame, s.timelineTrack, s.timelineKey,
                   ig::Rect(0.0f, 380.0f, 480.0f, 90.0f));
        c.sequencer("sequencer", s.sequence, 0, 60, s.sequenceFrame, s.sequenceTrack,
                    ig::Rect(0.0f, 480.0f, 480.0f, 80.0f));
        c.splitter("split", s.split, 0.0f, 480.0f, ig::SplitterAxis::Vertical,
                   ig::Rect(0.0f, 565.0f, 480.0f, 10.0f));
        c.endWindow();
    }

    if (c.beginWindow("Data", ig::Rect(700.0f, 300.0f, 560.0f, 480.0f)))
    {
        if (c.beginTable("table", ig::Span<float>(s.weights, 3)))
        {
            c.tableNextColumn();
            c.tableHeader("A", s.sortColumn, s.sortAscending);
            c.tableNextColumn();
            c.tableHeader("B", s.sortColumn, s.sortAscending);
            c.tableNextColumn();
            c.tableHeader("C", s.sortColumn, s.sortAscending);
            for (int r = 0; r < 4; ++r)
            {
                for (int k = 0; k < 3; ++k)
                {
                    c.tableNextColumn();
                    c.label("cell");
                }
            }
            c.endTable();
        }
        int first = 0;
        int last = 0;
        if (c.beginVirtualList("vlist", 1000, 22.0f, 90.0f, first, last))
        {
            for (int i = first; i < last; ++i)
            {
                char label[16];
                snprintf(label, sizeof(label), "item %d", i);
                c.selectable(label, i == s.list, c.virtualListItemRect(i));
            }
            c.endVirtualList();
        }
        if (c.beginVirtualTable("vtable", 500, 3, 22.0f, 90.0f, first, last))
        {
            for (int r = first; r < last; ++r)
                for (int k = 0; k < 3; ++k)
                    c.label("v", ig::Vec2(c.virtualTableCellRect(r, k).x, c.virtualTableCellRect(r, k).y));
            c.endVirtualTable();
        }
        if (c.beginVirtualTree("vtree", 300, 22.0f, 90.0f, first, last))
        {
            ig::TreeItemStyle style;
            for (int r = first; r < last; ++r)
            {
                bool expanded = r % 3 == 0;
                c.pushId(static_cast<uint64_t>(r));
                ig::TreeDrop drop;
                if (c.treeItem(static_cast<ig::WidgetId>(r + 1), "node", expanded, style,
                               c.virtualTreeItemRect(r, r % 4), &drop) &&
                    drop.target != ig::InvalidWidgetId)
                    ++s.dropped;
                c.popId();
            }
            c.endVirtualTree();
        }
        if (c.beginChild("child", 80.0f))
        {
            for (int i = 0; i < 30; ++i)
            {
                c.pushId(static_cast<uint64_t>(i));
                if (c.beginDragSource(static_cast<ig::WidgetId>(i + 1), 77u, static_cast<uint64_t>(i), "drag",
                                      ig::Rect(0.0f, static_cast<float>(i) * 20.0f, 100.0f, 18.0f)))
                {
                }
                c.popId();
                c.dummy(100.0f, 18.0f);
            }
            c.endChild();
        }
        ig::DragDropPayload payload;
        if (c.acceptDragDropTarget(77u, ig::Rect(300.0f, 300.0f, 200.0f, 100.0f), payload))
            ++s.dropped;
        c.gizmo2D("g2", s.t2, static_cast<ig::Gizmo2DMode>(s.radio), ig::Rect(300.0f, 200.0f, 200.0f, 90.0f));
        c.endWindow();
    }

    if (c.beginWindow("Docks", ig::Rect(40.0f, 420.0f, 620.0f, 360.0f)))
    {
        if (c.beginDockSpace("dock", 24.0f))
        {
            if (c.beginDockPanel("Scene", ig::DockSlot::Center))
            {
                const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
                ig::Gizmo3DOptions options;
                options.axisLength = 0.4f;
                c.gizmo3D("g3", s.t3, static_cast<ig::Gizmo3DMode>(s.radio), ig::Rect(0.0f, 0.0f, 260.0f, 160.0f),
                          identity, identity, options);
                c.image(ig::TextureId(9u), 40.0f, 40.0f);
                c.endDockPanel();
            }
            if (c.beginDockPanel("Inspector", ig::DockSlot::Right))
            {
                if (c.beginPropertyRow("value"))
                {
                    if (c.sliderFloat("##p", s.property, 0.0f, 1.0f))
                    {
                        ++s.history;
                        int *history = &s.history;
                        c.pushUndo("edit", [history]() { --*history; }, [history]() { ++*history; });
                    }
                    c.endPropertyRow();
                }
                c.imageButton("img", ig::TextureId(9u), 32.0f, 32.0f);
                c.endDockPanel();
            }
            if (s.leftPanel && c.beginDockPanel("Outline", ig::DockSlot::Left, &s.leftPanel))
            {
                for (int i = 0; i < 12; ++i)
                    c.selectable("entry", i == s.list);
                c.endDockPanel();
            }
            if (s.bottomPanel && c.beginDockPanel("Console", ig::DockSlot::Bottom, &s.bottomPanel))
            {
                c.label("log line");
                c.endDockPanel();
            }
            if (c.beginDockPanel("Assets", ig::DockSlot::Bottom))
            {
                c.label("assets");
                c.endDockPanel();
            }
            c.endDockSpace();
        }
        c.endWindow();
    }

    if (s.toolOpen && c.beginWindow("Tool", ig::Rect(500.0f, 500.0f, 300.0f, 200.0f), &s.toolOpen))
    {
        c.button("tool button");
        c.endWindow();
    }
    c.setModalWindow("Tool", s.modalTool && s.toolOpen);

    if (c.beginDialog("Settings", s.dialog, ig::Vec2(360.0f, 240.0f)))
    {
        c.inputText("dialog text", s.text);
        if (c.button("Close"))
            s.dialog = false;
        c.endDialog();
    }
    if (s.message)
        c.messageBox("Note", "A message", s.message, true);
    if (s.inputMessage)
    {
        ig::MessageBoxOptions options;
        options.kind = ig::MessageBoxKind::Input;
        options.showCancel = true;
        options.inputValue = &s.messageInput;
        c.messageBox("Rename", "New name", s.inputMessage, options);
    }
    if (s.files)
    {
        ig::FileDialogOptions options;
        options.mode = static_cast<ig::FileDialogMode>(s.radio % 3);
        c.fileDialog("files", s.files, s.fileState, options, s.provider);
    }
}

int gFailures = 0;

void fail(uint32_t seed, int frame, const char *what)
{
    ++gFailures;
    printf("FAIL seed=%u frame=%d: %s\n", seed, frame, what);
}

bool finite(float value)
{
    return value == value && value < 1e30f && value > -1e30f;
}

void checkDrawData(uint32_t seed, int frame, const ig::DrawData &data)
{
    for (size_t i = 0; i < data.indices.size(); ++i)
    {
        if (data.indices[i] >= data.vertices.size())
        {
            fail(seed, frame, "index past the vertex array");
            return;
        }
    }
    for (size_t i = 0; i < data.vertices.size(); ++i)
    {
        if (!finite(data.vertices[i].position.x) || !finite(data.vertices[i].position.y) ||
            !finite(data.vertices[i].uv.x) || !finite(data.vertices[i].uv.y))
        {
            fail(seed, frame, "non-finite vertex");
            return;
        }
    }
    for (size_t i = 0; i < data.commands.size(); ++i)
    {
        const ig::DrawCommand &command = data.commands[i];
        if (command.type == ig::DrawCommandType::Geometry)
        {
            const ig::GeometryCommand &g = command.payload.geometry;
            if (static_cast<size_t>(g.firstIndex) + g.indexCount > data.indices.size() || g.indexCount % 3u != 0u)
                fail(seed, frame, "geometry command outside the index array");
            if (!finite(g.clip.x) || !finite(g.clip.y) || !finite(g.clip.width) || !finite(g.clip.height))
                fail(seed, frame, "non-finite clip");
        }
        else
        {
            const ig::TextCommand &t = command.payload.text;
            if (static_cast<size_t>(t.textOffset) + t.textSize > data.textBytes.size())
                fail(seed, frame, "text command outside the text bytes");
            if (!finite(t.position.x) || !finite(t.position.y))
                fail(seed, frame, "non-finite text position");
        }
    }
}

void checkScene(uint32_t seed, int frame, const Scene &s)
{
    if (!(s.sliderF >= 0.0f && s.sliderF <= 1.0f))
        fail(seed, frame, "sliderFloat out of range");
    if (s.sliderI < 0 || s.sliderI > 10)
        fail(seed, frame, "sliderInt out of range");
    if (!(s.dragF >= -100.0f && s.dragF <= 100.0f))
        fail(seed, frame, "dragFloat out of range");
    if (s.dragI < -50 || s.dragI > 50)
        fail(seed, frame, "dragInt out of range");
    if (s.stepper < -5 || s.stepper > 5)
        fail(seed, frame, "stepperInt out of range");
    if (!(s.split >= 0.0f && s.split <= 480.0f))
        fail(seed, frame, "splitter out of range");
    if (s.combo < 0 || s.combo >= 5)
        fail(seed, frame, "comboBox index out of range");
    if (s.list < 0 || s.list >= 1000)
        fail(seed, frame, "listBox index out of range");
    if (s.tab < 0 || s.tab >= 3)
        fail(seed, frame, "tabBar index out of range");
    if (s.selectedStop >= static_cast<int>(s.stops.size()) || s.stops.size() < 2u)
        fail(seed, frame, "gradient stops invalid");
    if (s.selectedPoint >= static_cast<int>(s.curve.size()) || s.curve.size() < 2u)
        fail(seed, frame, "curve points invalid");
    if (s.timelineFrame < 0 || s.timelineFrame > 100)
        fail(seed, frame, "timeline frame out of range");
    if (s.sequenceFrame < 0 || s.sequenceFrame > 60)
        fail(seed, frame, "sequencer frame out of range");
    if (!(s.inputF == s.inputF))
        fail(seed, frame, "inputFloat became NaN");
    if (s.history < 0)
        fail(seed, frame, "undo ran more often than its edits were pushed");
    if (!finite(s.t2.position.x) || !finite(s.t2.position.y) || !finite(s.t2.rotation) ||
        !finite(s.t2.scale.x) || !finite(s.t2.scale.y))
        fail(seed, frame, "gizmo2D transform not finite");
    if (!finite(s.t3.position.x) || !finite(s.t3.position.y) || !finite(s.t3.position.z) ||
        !finite(s.t3.rotation.x) || !finite(s.t3.scale.x) || !finite(s.t3.scale.y) || !finite(s.t3.scale.z))
        fail(seed, frame, "gizmo3D transform not finite");
    for (size_t i = 0; i + 1u < s.stops.size(); ++i)
        if (!(s.stops[i].position >= 0.0f && s.stops[i].position <= 1.0f))
            fail(seed, frame, "gradient stop outside [0, 1]");
    for (size_t i = 0; i < s.curve.size(); ++i)
        if (!(s.curve[i].x >= 0.0f && s.curve[i].x <= 1.0f && s.curve[i].y >= 0.0f && s.curve[i].y <= 1.0f))
            fail(seed, frame, "curve point outside its range");
}

const char *const kTexts[] = {
    "a", "Z", "7", " ", "ção", "€", "日本", "\xF0\x9F\x98\x80", // valid, up to 4-byte
    "\xC3", "\xE2\x82", "\xFF", "\x80", "\xED\xA0\x80",        // invalid / truncated UTF-8
    "\n", "\t", "-", ".", "1e9", "nan",
};

const ig::KeyCode kKeys[] = {
    ig::KeyCode::Backspace, ig::KeyCode::Enter, ig::KeyCode::Delete, ig::KeyCode::Tab,
    ig::KeyCode::Left, ig::KeyCode::Right, ig::KeyCode::Up, ig::KeyCode::Down,
    ig::KeyCode::Home, ig::KeyCode::End, ig::KeyCode::PageUp, ig::KeyCode::PageDown,
    ig::KeyCode::Escape, ig::KeyCode::A, ig::KeyCode::C, ig::KeyCode::D, ig::KeyCode::F,
    ig::KeyCode::H, ig::KeyCode::S, ig::KeyCode::V, ig::KeyCode::X, ig::KeyCode::Y,
    ig::KeyCode::Z, ig::KeyCode::N, ig::KeyCode::O, ig::KeyCode::F4, ig::KeyCode::F5,
};

void run(uint32_t seed, int frames)
{
    gState = seed * 2654435761u + 1u;
    ig::TestBackend backend;
    ig::Context context(backend);
    Scene scene;
    float width = 1280.0f;
    float height = 800.0f;
    ig::Vec2 pointer(640.0f, 400.0f);
    bool down[3] = {false, false, false};
    // Where the last frame drew something: aiming there hits widgets far
    // more often than uniform positions (small handles, gutters, tabs).
    ct::Vector<ig::Vec2> targets;

    for (int frame = 0; frame < frames; ++frame)
    {
        // Occasionally flip the scene's blockers and window state.
        switch (pick(60))
        {
        case 0: scene.message = !scene.message; break;
        case 1: scene.dialog = !scene.dialog; break;
        case 2: scene.files = !scene.files; break;
        case 3: scene.modalTool = !scene.modalTool; break;
        case 4: scene.inputMessage = !scene.inputMessage; break;
        case 5: scene.toolOpen = true; break;
        case 6: context.minimizeAllWindows(); break;
        case 7: context.restoreAllWindows(); break;
        case 8: context.tileAllWindows(); break;
        case 9: context.cascadeWindows(); break;
        case 10: context.maximizeWindow("Editors"); break;
        case 11: context.showToast("toast", "hello", static_cast<ig::ToastPosition>(pick(9)), 0.5f); break;
        case 12: context.undo(); break;
        case 13: context.redo(); break;
        case 14: scene.leftPanel = true; scene.bottomPanel = true; break;
        case 15: context.clearUndoHistory(); break;
        default: break;
        }

        const int events = pick(5);
        for (int e = 0; e < events; ++e)
        {
            const int kind = pick(100);
            if (kind < 35)
            {
                // Mostly small moves (drags), sometimes jumps, sometimes off screen.
                if (!targets.empty() && pick(2) == 0)
                {
                    const ig::Vec2 &target = targets[static_cast<size_t>(pick(static_cast<int>(targets.size())))];
                    pointer = ig::Vec2(target.x + range(-3.0f, 3.0f), target.y + range(-3.0f, 3.0f));
                }
                else if (pick(4) == 0)
                    pointer = ig::Vec2(range(-50.0f, width + 50.0f), range(-50.0f, height + 50.0f));
                else
                    pointer = ig::Vec2(pointer.x + range(-40.0f, 40.0f), pointer.y + range(-40.0f, 40.0f));
                context.pushEvent(ig::Event::pointerMove(pointer.x, pointer.y));
            }
            else if (kind < 55)
            {
                const int button = pick(10) < 8 ? 0 : 1 + pick(2);
                const ig::PointerButton pb = static_cast<ig::PointerButton>(button);
                if (down[button])
                    context.pushEvent(ig::Event::pointerUp(pb, pointer.x, pointer.y));
                else
                    context.pushEvent(ig::Event::pointerDown(pb, pointer.x, pointer.y));
                down[button] = !down[button];
            }
            else if (kind < 63)
            {
                ig::Event wheel;
                wheel.type = ig::EventType::PointerWheel;
                wheel.wheelY = range(-5.0f, 5.0f);
                wheel.wheelX = pick(4) == 0 ? range(-3.0f, 3.0f) : 0.0f;
                wheel.control = pick(5) == 0;
                context.pushEvent(wheel);
            }
            else if (kind < 80)
            {
                context.pushEvent(ig::Event::keyDown(kKeys[pick(sizeof(kKeys) / sizeof(kKeys[0]))],
                                                     pick(4) == 0, pick(4) == 0));
            }
            else if (kind < 95)
            {
                context.pushEvent(ig::Event::textInput(kTexts[pick(sizeof(kTexts) / sizeof(kTexts[0]))]));
            }
            else if (kind < 98)
            {
                width = range(0.0f, 1600.0f);
                height = range(0.0f, 1000.0f);
                context.pushEvent(ig::Event::viewportChanged(width, height, pick(3) == 0 ? 2.0f : 1.0f));
            }
            else
            {
                context.pushEvent(ig::Event::focusLost());
                down[0] = down[1] = down[2] = false;
            }
        }

        context.beginFrame(ig::FrameInfo(width, height, 1.0f, 1.0f / 60.0f));
        drawScene(context, scene);
        const ig::DrawData &data = context.endFrame();
        checkDrawData(seed, frame, data);
        targets.clear();
        for (size_t i = 0; i < data.vertices.size(); i += 3u)
            targets.push_back(data.vertices[i].position);
        checkScene(seed, frame, scene);
        if (gFailures > 20)
            return;
    }
}

} // namespace

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        const uint32_t seed = static_cast<uint32_t>(strtoul(argv[1], nullptr, 10));
        run(seed, argc > 2 ? atoi(argv[2]) : 2000);
    }
    else
    {
        for (uint32_t seed = 1u; seed <= 12u && gFailures == 0; ++seed)
            run(seed, 600);
    }
    if (gFailures != 0)
    {
        printf("test_fuzz: %d failure(s)\n", gFailures);
        return 1;
    }
    printf("test_fuzz: all tests passed\n");
    return 0;
}
