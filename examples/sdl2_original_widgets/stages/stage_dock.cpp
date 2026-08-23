#include "WidgetApp.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "DockPanel.hpp"
#include "TreePropertyColorWidgets.hpp"
#include "TextInputWidgets.hpp"
#include "ConsoleWidget.hpp"
using namespace BuGUI;

void registerDockStage(WidgetApp& app)
{
    auto* root  = app.addStage("dock");
    auto* outer = root->createChild<BoxLayout>(LayoutDir::Vertical);
    outer->setStretch(1);
    outer->setSpacing(0);
    outer->setPadding(0);

    // ── Top bar ───────────────────────────────────────────────────────────
    {
        auto* hdr = outer->createChild<BoxLayout>(LayoutDir::Horizontal);
        hdr->setSize(0, 36);
        hdr->setPadding({4, 8, 4, 8});
        hdr->setSpacing(12);
        auto* back = hdr->createChild<Button>("\u2190 Menu");
        back->clicked.connect([&app]() { app.setStage("menu", TransitionType::CoverRight); });
        hdr->createChild<Spacer>(0.0f)->setStretch(1.0f);
        hdr->createChild<Label>("DockPanel \u2014 drag tabs to rearrange")->setAlign(TextAlign::RIGHT);
    }

    // ── DockPanel ─────────────────────────────────────────────────────────
    auto* dock = outer->createChild<DockPanel>();
    dock->setStretch(1);

    // Scene outliner
    {
        auto* tree = new TreeView();
        auto* r    = tree->addRoot("Scene");
        auto* ch1  = r->addChild("Camera");
        auto* ch2  = r->addChild("Lighting");
        ch2->addChild("Sun");
        ch2->addChild("Ambient");
        auto* ch3  = r->addChild("Objects");
        ch3->addChild("Player");
        ch3->addChild("Ground");
        ch3->addChild("Trees");
        r->setExpanded(true);
        ch2->setExpanded(true);
        ch3->setExpanded(true);
        (void)ch1;
        dock->addPanel("Scene", tree);
    }

    // Viewport placeholder
    {
        auto* box = new BoxLayout(LayoutDir::Vertical);
        box->setPadding(20);
        box->setSpacing(8);
        box->createChild<Label>("Viewport")->setColor(Color(100, 160, 240, 255));
        box->createChild<Label>("(drag tabs between panels)");
        dock->addPanel("Viewport", box);
    }

    // Shader editor
    {
        auto* ed = new TextEdit();
        ed->setText(
            "// Simple fragment shader\n"
            "uniform sampler2D uTexture;\n"
            "varying vec2 vUV;\n\n"
            "void main() {\n"
            "    vec4 col = texture2D(uTexture, vUV);\n"
            "    gl_FragColor = col;\n"
            "}\n"
        );
        dock->addPanel("Shader", ed);
    }

    // Properties
    {
        auto* pg = new PropertyGrid();
        pg->addSection("Transform");
        pg->addVec3("Position", 0.f, 0.f, 0.f, -1000.f, 1000.f);
        pg->addVec3("Rotation", 0.f, 0.f, 0.f,  -360.f,  360.f);
        pg->addVec3("Scale",    1.f, 1.f, 1.f,     0.f,  100.f);
        pg->addSection("Material");
        pg->addColor("Albedo",    Color(200, 200, 200, 255));
        pg->addFloat("Roughness", 0.5f, 0.f, 1.f);
        pg->addFloat("Metallic",  0.0f, 0.f, 1.f);
        pg->addBool("Cast Shadow", true);
        dock->addPanel("Properties", pg);
    }

    // Console
    {
        auto* con = new ConsoleWidget();
        con->log(LogLevel::Info,  "Scene loaded in 0.03s");
        con->log(LogLevel::Info,  "Shader compiled OK");
        con->log(LogLevel::Warn,  "Mesh 'tree' has no UV map");
        con->log(LogLevel::Error, "Script 'Player.lua' line 42: nil value");
        con->log(LogLevel::Info,  "Ready.");
        dock->addPanel("Console", con);
    }

    // Initial layout
    dock->splitOff("Scene",      DockSide::Left,   0.20f);
    dock->splitOff("Console",    DockSide::Bottom,  0.25f);
    dock->splitOff("Properties", DockSide::Right,   0.22f);
}
