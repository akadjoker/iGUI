#include "WidgetApp.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "CodeEditor.hpp"
#include "NodeEditor.hpp"
#include "Timeline.hpp"
#include "GizmoWidgets.hpp"
#include "ConsoleWidget.hpp"
#include "AssetBrowser.hpp"
#include "ThumbnailGrid.hpp"
#include "FileDialog.hpp"
#include "ChartWidgets.hpp"
#include "AudioWidgets.hpp"
#include "AutomotiveWidgets.hpp"
#include "DataWidgets.hpp"
#include "ToolBar.hpp"
#include "AppWidgets.hpp"

#include <cmath>
#include <cmath>
#include <ct/vector.hpp>

using namespace ig::retained;

namespace
{
BoxLayout* makeStage(WidgetApp& app, const char* name, const char* title)
{
    auto* root = app.addStage(name);
    auto* outer = root->createChild<BoxLayout>(LayoutDir::Vertical);
    outer->setSpacing(0.0f);
    outer->setPadding(0.0f);
    outer->setStretch(1.0f);

    auto* header = outer->createChild<BoxLayout>(LayoutDir::Horizontal);
    header->setSize(0.0f, 40.0f);
    header->setPadding(Edges(6.0f, 10.0f));
    header->setSpacing(10.0f);
    auto* back = header->createChild<Button>("\u2190 Menu");
    back->clicked.connect([&app]() { app.setStage("menu", TransitionType::CoverRight); });
    header->createChild<Spacer>(0.0f)->setStretch(1.0f);
    header->createChild<Label>(title)->setAlign(TextAlign::RIGHT);
    outer->createChild<Line>();
    return outer;
}

// A camera at `eye` looking at the origin, in the column-major order the 3D
// gizmo's setViewProjection expects. The gizmo measures its handles from the
// camera distance, so the eye position is what decides how big they are: with
// the identity matrices the demo used before, a target on the origin sat zero
// units from the camera and the handles collapsed to nothing.
void lookAtOrigin(float* out, float ex, float ey, float ez)
{
    const float length = std::sqrt(ex * ex + ey * ey + ez * ez);
    const float f[3] = {ex / length, ey / length, ez / length};
    float r[3] = {f[2], 0.0f, -f[0]};                 // (0,1,0) x f
    const float rl = std::sqrt(r[0] * r[0] + r[2] * r[2]);
    r[0] /= rl; r[2] /= rl;
    const float u[3] = {f[1] * r[2] - f[2] * r[1],
                        f[2] * r[0] - f[0] * r[2],
                        f[0] * r[1] - f[1] * r[0]};   // f x r
    out[0] = r[0]; out[4] = r[1]; out[8]  = r[2];  out[12] = -(r[0] * ex + r[1] * ey + r[2] * ez);
    out[1] = u[0]; out[5] = u[1]; out[9]  = u[2];  out[13] = -(u[0] * ex + u[1] * ey + u[2] * ez);
    out[2] = f[0]; out[6] = f[1]; out[10] = f[2];  out[14] = -(f[0] * ex + f[1] * ey + f[2] * ez);
    out[3] = 0.0f; out[7] = 0.0f; out[11] = 0.0f;  out[15] = 1.0f;
}
}

void showFileDialogDemo(WidgetApp& app)
{
    auto* dialog = app.addFloat<FileDialog>("Open source file");
    dialog->setMode(FileDialog::Mode::Open);
    dialog->setFilter("*.cpp;*.hpp;*.h;*.txt");
    dialog->setPath(".");
    dialog->addBookmark("Project", ".");
    dialog->addBookmark("Widgets", "include/igui/widgets");
    dialog->setRect({160.0f, 80.0f, 920.0f, 560.0f});
}

void registerEditorStage(WidgetApp& app)
{
    auto* outer = makeStage(app, "editor", "CodeEditor — original completo");
    auto* editor = outer->createChild<CodeEditor>();
    editor->setStretch(1.0f);
    editor->setHighlighterForFile("widget_demo.cpp");
    editor->setShowFolding(true);
    editor->setShowMinimap(true);
    editor->setShowIndentGuides(true);
    editor->setShowScopeLines(true);
    editor->setShowBracketMatch(true);
    editor->setHighlightCurrentLine(true);
    editor->setText(
        "#include <WidgetApp.hpp>\n"
        "#include <CodeEditor.hpp>\n\n"
        "namespace demo\n"
        "{\n"
        "class EditorStage final\n"
        "{\n"
        "public:\n"
        "    void build(ig::retained::WidgetApp& app)\n"
        "    {\n"
        "        auto* root = app.addStage(\"editor\");\n"
        "        auto* editor = root->createChild<ig::retained::CodeEditor>();\n"
        "        editor->setHighlighterForFile(\"main.cpp\");\n"
        "        editor->setShowFolding(true);\n"
        "        editor->setShowMinimap(true);\n"
        "    }\n"
        "};\n"
        "}\n\n"
        "int main()\n"
        "{\n"
        "    // Ctrl+F, Ctrl+H, undo/redo, multi-cursor and folding are active.\n"
        "    return 0;\n"
        "}\n");
}

void registerNodeStage(WidgetApp& app)
{
    auto* outer = makeStage(app, "nodes", "NodeEditor — drag, pan, zoom e links");
    auto* editor = outer->createChild<NodeEditor>();
    editor->setStretch(1.0f);
    editor->setGridSnap(16.0f);

    const int texture = editor->addNode("Texture Sample", 60.0f, 80.0f);
    const int textureUv = editor->addPin(texture, "UV", PinDir::Input, PinType::Vec2);
    const int textureOut = editor->addPin(texture, "RGBA", PinDir::Output, PinType::Color);
    (void)textureUv;
    editor->setNodeHeader(texture, Color(90, 70, 130, 255));

    const int tint = editor->addNode("Multiply", 300.0f, 125.0f);
    const int tintA = editor->addPin(tint, "A", PinDir::Input, PinType::Color);
    const int tintB = editor->addPin(tint, "B", PinDir::Input, PinType::Color);
    const int tintOut = editor->addPin(tint, "Result", PinDir::Output, PinType::Color);
    (void)tintB;
    editor->setNodeHeader(tint, Color(70, 105, 145, 255));

    const int output = editor->addNode("Fragment Output", 545.0f, 170.0f);
    const int outputColor = editor->addPin(output, "Color", PinDir::Input, PinType::Color);
    editor->setNodeHeader(output, Color(135, 70, 70, 255));

    editor->addLink(texture, textureOut, tint, tintA);
    editor->addLink(tint, tintOut, output, outputColor);
    editor->fitView();
}

void registerTimelineStage(WidgetApp& app)
{
    auto* outer = makeStage(app, "timeline", "Timeline — scrub, zoom, clips e keyframes");
    auto* timeline = outer->createChild<Timeline>();
    timeline->setStretch(1.0f);
    timeline->setTimeRange(0.0f, 12.0f);
    timeline->setFrameRate(30.0f);
    timeline->setPlayhead(3.25f);

    const int camera = timeline->addTrack("Camera", Color(90, 150, 220, 255));
    timeline->addClip(camera, 0.5f, 4.5f, "Intro shot", Color(65, 115, 180, 220));
    timeline->addClip(camera, 5.0f, 10.5f, "Tracking shot", Color(70, 135, 170, 220));
    for (float t : {0.5f, 2.0f, 4.5f, 7.0f, 10.5f}) timeline->addKeyframe(camera, t);

    const int character = timeline->addTrack("Character", Color(220, 145, 75, 255));
    timeline->setHeaderWidth(180);
    timeline->setEdgePadding(20);
    const int hips = timeline->addJoint("Hips", character);
    const int spine = timeline->addJoint("Spine", hips);
    const int hand = timeline->addJoint("Hand.L", spine);
    timeline->addKeyframe(hips, timeline->viewStart());
    timeline->addKeyframe(hips, timeline->viewEnd());
    timeline->addKeyframe(spine, 3.0f);
    timeline->addKeyframe(hand, 6.0f);
    timeline->addClip(character, 1.0f, 3.5f, "Idle");
    timeline->addClip(character, 3.5f, 8.0f, "Walk");
    timeline->addClip(character, 8.0f, 11.0f, "Jump");
    for (float t : {1.0f, 3.5f, 5.0f, 8.0f, 9.5f, 11.0f}) timeline->addKeyframe(character, t);

    const int audio = timeline->addTrack("Audio", Color(130, 190, 110, 255));
    timeline->addClip(audio, 0.0f, 12.0f, "music_master.wav", Color(70, 145, 95, 220));
}

namespace
{
// The 3D gizmo maps the camera's [-1, 1] over its own rect, so a stage area that
// is not square stretches the world unless the projection takes the aspect back
// out. This one does, from the rect the layout hands it.
class SquareGizmo3D : public Gizmo3D
{
public:
    void setView(const float* view4x4)
    {
        for (int index = 0; index < 16; ++index)
            view_[index] = view4x4[index];
    }

    void layout() override
    {
        Gizmo3D::layout();
        float projection[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        if (rect_.w > 0.0f && rect_.h > 0.0f)
            projection[0] = rect_.h / rect_.w;
        setViewProjection(view_, projection, static_cast<int>(rect_.w), static_cast<int>(rect_.h));
    }

private:
    float view_[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};
}

// One tab per gizmo: sharing the stage between six of them left each one with a
// corner of the window, and a 3D gizmo squeezed into a letterbox reads as
// nothing at all.
void registerGizmosStage(WidgetApp& app)
{
    auto* outer = makeStage(app, "gizmos", "Gizmo2D e Gizmo3D — translate, rotate e scale");
    auto* tabs = outer->createChild<TabLayout>();
    tabs->setStretch(1.0f);

    struct Entry { const char* label; GizmoMode mode; } entries[] = {
        {"Translate 2D", GizmoMode::Translate},
        {"Rotate 2D",    GizmoMode::Rotate},
        {"Scale 2D",     GizmoMode::Scale},
    };
    for (const Entry& entry : entries)
    {
        auto* page = tabs->addTab<Panel>(entry.label);
        auto* gizmo = page->createChild<Gizmo2D>();
        gizmo->setPosition(340.0f, 260.0f);
        gizmo->setMode(entry.mode);
        gizmo->setSnapTranslate(8.0f);
        gizmo->setSnapRotate(15.0f);
        gizmo->setSnapScale(0.1f);
    }

    // The same three modes in 3D, each on the whole stage. The camera looks at
    // the origin from the upper right, so the three axes point three different
    // ways on screen and the handles are big enough to grab.
    float view[16];
    lookAtOrigin(view, 3.0f, 2.2f, 3.0f);
    const float position[3] = {0.0f, 0.0f, 0.0f};
    const float rotation[3] = {0.0f, 0.0f, 0.0f};
    const float scale[3] = {1.0f, 1.0f, 1.0f};

    struct Entry3D { const char* label; GizmoMode3D mode; } entries3D[] = {
        {"Translate 3D", GizmoMode3D::Translate},
        {"Rotate 3D",    GizmoMode3D::Rotate},
        {"Scale 3D",     GizmoMode3D::Scale},
    };
    for (const Entry3D& entry : entries3D)
    {
        auto* page = tabs->addTab<Panel>(entry.label);
        auto* gizmo = page->createChild<SquareGizmo3D>();
        gizmo->setView(view);
        gizmo->setTarget(position, rotation, scale);
        gizmo->setScreenScale(100.0f);
        gizmo->setMode(entry.mode);
        gizmo->setSnapTranslate(0.1f);
        gizmo->setSnapRotate(15.0f);
        gizmo->setSnapScale(0.1f);
    }
}

void registerToolsStage(WidgetApp& app)
{
    auto* outer = makeStage(app, "tools", "Console, Assets, Thumbnails e FileDialog");
    auto* actions = outer->createChild<BoxLayout>(LayoutDir::Horizontal);
    actions->setPadding(Edges(6.0f, 10.0f));
    actions->setSpacing(8.0f);
    auto* openDialog = actions->createChild<Button>("Open FileDialog");
    openDialog->clicked.connect([&app]() { showFileDialogDemo(app); });

    auto* tabs = outer->createChild<TabLayout>();
    tabs->setStretch(1.0f);

    auto* console = tabs->addTab<ConsoleWidget>("Console");
    console->log(LogLevel::Trace, "SDL backend initialized");
    console->log(LogLevel::Info, "Original widgets loaded");
    console->log(LogLevel::Warn, "This is a warning filter test");
    console->log(LogLevel::Error, "This is an error rendering test");
    console->log(LogLevel::Info, "Type commands in the input below");

    auto* assets = tabs->addTab<AssetBrowser>("Assets");
    assets->setPath("Assets/Project");
    assets->addItem({"Scenes", AssetType::Folder, Color(190, 145, 65, 255)});
    assets->addItem({"player.cpp", AssetType::Script, Color(80, 135, 200, 255)});
    assets->addItem({"albedo.png", AssetType::Image, Color(135, 85, 95, 255)});
    assets->addItem({"theme.material", AssetType::Material, Color(125, 90, 155, 255)});
    assets->addItem({"level.scene", AssetType::Scene, Color(75, 145, 110, 255)});
    assets->addItem({"music.wav", AssetType::Audio, Color(100, 155, 115, 255)});

    auto* thumbnails = tabs->addTab<ThumbnailGrid>("Thumbnails");
    thumbnails->setThumbSize(110.0f, 90.0f);
    thumbnails->addItem("forest.png", Color(55, 115, 75, 255));
    thumbnails->addItem("city.png", Color(80, 100, 145, 255));
    thumbnails->addItem("desert.png", Color(165, 125, 65, 255));
    thumbnails->addItem("studio.png", Color(115, 80, 135, 255));
    thumbnails->addItem("ocean.png", Color(55, 115, 160, 255));
    thumbnails->addItem("night.png", Color(55, 60, 100, 255));
}

void registerGalleryStage(WidgetApp& app)
{
    auto* outer = makeStage(app, "gallery", "Charts, Audio, Automotive e Data widgets");
    auto* tabs = outer->createChild<TabLayout>();
    tabs->setStretch(1.0f);

    auto* charts = tabs->addTab<BoxLayout>("Charts", LayoutDir::Vertical);
    charts->setPadding(10.0f);
    charts->setSpacing(8.0f);
    auto* plot = charts->createChild<PlotWidget>();
    plot->setStretch(1.0f);
    plot->setTitle("Frame timings");
    plot->setXLabel("frame");
    plot->setYLabel("ms");
    const int cpu = plot->addSeries("CPU", Color(90, 170, 235, 255), PlotType::Line);
    const int gpu = plot->addSeries("GPU", Color(235, 145, 80, 255), PlotType::Line);
    for (int i = 0; i < 96; ++i)
    {
        plot->addPoint(cpu, 8.0f + std::sin(i * 0.18f) * 2.1f + (i % 17 == 0 ? 3.0f : 0.0f));
        plot->addPoint(gpu, 11.0f + std::cos(i * 0.13f) * 2.8f);
    }
    auto* chartRow = charts->createChild<BoxLayout>(LayoutDir::Horizontal);
    chartRow->setSpacing(8.0f);
    chartRow->setStretch(0.65f);
    auto* histogram = chartRow->createChild<HistogramWidget>();
    histogram->setStretch(1.0f);
    histogram->setTitle("Distribution");
    ct::Vector<float> values;
    for (int i = 0; i < 180; ++i)
        values.push_back(0.5f + std::sin(i * 0.31f) * 0.25f + std::cos(i * 0.07f) * 0.12f);
    histogram->setData(values);
    auto* gradient = chartRow->createChild<GradientEditor>();
    gradient->setStretch(1.0f);
    gradient->clearStops();
    gradient->addStop(0.0f, Color(30, 70, 170, 255));
    gradient->addStop(0.45f, Color(80, 210, 180, 255));
    gradient->addStop(1.0f, Color(245, 170, 50, 255));

    auto* audio = tabs->addTab<BoxLayout>("Audio", LayoutDir::Vertical);
    audio->setPadding(10.0f);
    audio->setSpacing(8.0f);
    auto* audioTop = audio->createChild<BoxLayout>(LayoutDir::Horizontal);
    audioTop->setSpacing(10.0f);
    auto* gain = audioTop->createChild<Knob>();
    gain->setLabel("Gain");
    gain->setRange(-24.0f, 12.0f);
    gain->setValue(-3.0f);
    auto* mix = audioTop->createChild<Knob>();
    mix->setLabel("Mix");
    mix->setValue(0.65f);
    auto* envelope = audioTop->createChild<ADSRWidget>();
    envelope->setStretch(1.0f);
    envelope->setADSR(0.08f, 0.18f, 0.62f, 0.35f);
    auto* meter = audioTop->createChild<VUMeter>();
    meter->setChannelCount(2);
    meter->setLevel(0, 0.72f);
    meter->setLevel(1, 0.58f);
    auto* spectrum = audio->createChild<SpectrumAnalyzer>();
    spectrum->setStretch(1.0f);
    float magnitudes[32];
    for (int i = 0; i < 32; ++i)
        magnitudes[i] = 0.1f + 0.75f * std::fabs(std::sin(i * 0.43f)) * (1.0f - i / 42.0f);
    spectrum->setMagnitudes(magnitudes, 32);
    auto* waveform = audio->createChild<WaveformView>();
    waveform->setStretch(0.7f);
    ct::Vector<float> samples(2048);
    for (int i = 0; i < static_cast<int>(samples.size()); ++i)
        samples[i] = std::sin(i * 0.045f) * (0.55f + 0.35f * std::sin(i * 0.002f));
    waveform->setSamples(samples.data(), static_cast<int>(samples.size()));
    waveform->setPlayhead(0.37f);
    waveform->setLoopRegion(0.18f, 0.72f);

    auto* car = tabs->addTab<BoxLayout>("Automotive", LayoutDir::Vertical);
    car->setPadding(14.0f);
    car->setSpacing(12.0f);
    auto* cluster = car->createChild<BoxLayout>(LayoutDir::Horizontal);
    cluster->setSpacing(16.0f);
    cluster->setStretch(1.0f);
    auto* speedGauge = cluster->createChild<RadialGauge>(0.0f, 260.0f, 118.0f);
    speedGauge->setStretch(1.0f);
    speedGauge->setLabel("SPEED");
    speedGauge->setUnit("km/h");
    speedGauge->setShowNeedle(true);
    auto* speed = cluster->createChild<DigitalSpeed>(118.0f);
    speed->setStretch(0.7f);
    auto* battery = cluster->createChild<RadialGauge>(0.0f, 100.0f, 76.0f);
    battery->setStretch(1.0f);
    battery->setLabel("BATTERY");
    battery->setUnit("%");
    battery->setArcColor(Color(70, 205, 120, 255));
    auto* power = car->createChild<PowerBar>(-100.0f, 200.0f, 84.0f);
    power->setStretch(0.35f);
    auto* carBottom = car->createChild<BoxLayout>(LayoutDir::Horizontal);
    carBottom->setSpacing(16.0f);
    auto* drive = carBottom->createChild<DriveMode>();
    drive->addMode("ECO", Color(70, 180, 105, 255));
    drive->addMode("NORMAL", Color(90, 145, 220, 255));
    drive->addMode("SPORT", Color(225, 85, 65, 255));
    drive->setMode(1);
    carBottom->createChild<BatteryGauge>(76.0f)->setChargingState(true);

    auto* data = tabs->addTab<BoxLayout>("Data", LayoutDir::Vertical);
    data->setPadding(10.0f);
    auto* grid = data->createChild<DataGrid>();
    grid->setStretch(1.0f);
    grid->setShowCheckboxes(true);
    grid->setMultiSelect(true);
    grid->addColumn("Name", 250.0f);
    grid->addColumn("Type", 130.0f);
    grid->addColumn("Size", 100.0f);
    grid->addColumn("Modified", 180.0f);
    grid->addRow({"CodeEditor.cpp", "C++ source", "118 KB", "today"});
    grid->addRow({"NodeEditor.cpp", "C++ source", "21 KB", "today"});
    grid->addRow({"Timeline.cpp", "C++ source", "18 KB", "yesterday"});
    grid->addRow({"theme.json", "JSON", "4 KB", "yesterday"});
    grid->addRow({"Roboto-Regular.ttf", "Font", "164 KB", "last week"});
    grid->setRowChecked(0, true);
    grid->setRowChecked(1, true);
}

void registerSpecialtyStage(WidgetApp& app)
{
    auto* outer = makeStage(app, "specialty", "Curve, Piano, Tree, AppWidgets e ToolBar");
    auto* tabs = outer->createChild<TabLayout>();
    tabs->setStretch(1.0f);

    auto* curves = tabs->addTab<CurveEditor>("Curves");
    const int position = curves->addCurve("Position Y", Color(90, 175, 235, 255));
    curves->addKey(position, 0.0f, 0.1f);
    curves->addKey(position, 0.8f, 1.0f);
    curves->addKey(position, 1.7f, 0.35f);
    curves->addKey(position, 2.8f, 1.4f);
    const int opacity = curves->addCurve("Opacity", Color(235, 150, 75, 255));
    curves->addKey(opacity, 0.0f, 0.0f);
    curves->addKey(opacity, 0.4f, 1.0f);
    curves->addKey(opacity, 2.4f, 1.0f);
    curves->addKey(opacity, 3.0f, 0.0f);
    curves->setSnapTime(0.1f);
    curves->setSnapValue(0.1f);
    curves->setPlayhead(1.15f);
    curves->fitView();

    auto* piano = tabs->addTab<PianoRoll>("Piano Roll");
    piano->setPitchRange(48, 76);
    piano->setTimeRange(0.0f, 16.0f);
    piano->setPlayhead(5.5f);
    piano->addNote(60, 0.0f, 1.5f);
    piano->addNote(64, 1.0f, 1.5f);
    piano->addNote(67, 2.0f, 2.0f);
    piano->addNote(72, 4.0f, 1.0f);
    piano->addNote(71, 5.0f, 1.0f);
    piano->addNote(67, 6.0f, 2.0f);
    piano->addNote(55, 8.0f, 4.0f);

    auto* tree = tabs->addTab<TreeGrid>("Tree Grid");
    tree->addColumn("Object", 320.0f);
    tree->addColumn("Type", 150.0f);
    tree->addColumn("Visible", 110.0f);
    tree->setMultiSelect(true);
    auto* scene = tree->addRoot({"Scene", "Root", "yes"});
    scene->iconId = IconId::Home;
    auto* camera = scene->addChild({"Main Camera", "Camera", "yes"});
    camera->iconId = IconId::Eye;
    auto* lights = scene->addChild({"Lights", "Group", "yes"});
    lights->iconId = IconId::FolderOpen;
    lights->addChild({"Sun", "Directional", "yes"})->iconId = IconId::Star;
    lights->addChild({"Fill", "Point", "no"})->iconId = IconId::Star;
    auto* objects = scene->addChild({"Objects", "Group", "yes"});
    objects->iconId = IconId::FolderOpen;
    objects->addChild({"Player", "Mesh", "yes"})->iconId = IconId::User;
    objects->addChild({"Ground", "Mesh", "yes"})->iconId = IconId::File;
    tree->rebuild();

    auto* appPage = tabs->addTab<BoxLayout>("App Widgets", LayoutDir::Vertical);
    appPage->setPadding(12.0f);
    appPage->setSpacing(10.0f);
    auto* breadcrumbs = appPage->createChild<Breadcrumbs>();
    breadcrumbs->setPath({"Project", "Assets", "Scripts", "editor.cpp"});
    auto* appRow = appPage->createChild<BoxLayout>(LayoutDir::Horizontal);
    appRow->setSpacing(8.0f);
    auto* search = appRow->createChild<SearchBar>();
    search->setStretch(1.0f);
    search->setPlaceholder("Search widgets...");
    auto* split = appRow->createChild<SplitButton>("Build");
    split->addAction("Build Debug", [] {});
    split->addAction("Build Release", [] {});
    split->addSeparator();
    split->addAction("Clean", [] {});
    auto* toolbar = appPage->createChild<ToolBar>();
    toolbar->setDraggable(true);
    toolbar->addButton(IconId::File, "New file");
    toolbar->addButton(IconId::FolderOpen, "Open");
    toolbar->addButton(IconId::Refresh, "Reload");
    toolbar->addSeparator();
    toolbar->addToggle(IconId::Eye, "Preview", true);
    toolbar->addSpacer();
    toolbar->addButton(IconId::Gear, "Settings");
    auto* rich = appPage->createChild<RichText>();
    rich->setStretch(1.0f);
    rich->setMarkdown(
        "# Original AppWidgets\n\n"
        "This page exercises **Breadcrumbs**, `SearchBar`, SplitButton, ToolBar and RichText.\n\n"
        "- Toolbar icons use font glyphs\n"
        "- Items are interactive\n"
        "- [Links](https://example.invalid) expose the original signal API");

}
