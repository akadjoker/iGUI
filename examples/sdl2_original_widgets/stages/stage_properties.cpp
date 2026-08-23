#include "WidgetApp.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "ScrollWidgets.hpp"
#include "TreePropertyColorWidgets.hpp"
using namespace BuGUI;

void registerPropertiesStage(WidgetApp& app)
{
    auto* root  = app.addStage("properties");
    auto* outer = root->createChild<BoxLayout>(LayoutDir::Vertical);
    outer->setSpacing(0.0f);
    outer->setPadding(0.0f);
    outer->setStretch(1);

    // ── Top bar ───────────────────────────────────────────────────────────
    auto* topBar = outer->createChild<BoxLayout>(LayoutDir::Horizontal);
    topBar->setSpacing(8.0f);
    topBar->setPadding(Edges(8.0f, 12.0f));
    auto* back = topBar->createChild<Button>("\u2190 Menu");
    back->clicked.connect([&app]() { app.setStage("menu", TransitionType::CoverRight); });
    topBar->createChild<Spacer>(0.0f)->setStretch(1.0f);
    topBar->createChild<Label>("PropertyGrid & TreeView")->setAlign(TextAlign::RIGHT);
    outer->createChild<Line>();

    // ── Split: tree left, property grid right ─────────────────────────────
    auto* row = outer->createChild<BoxLayout>(LayoutDir::Horizontal);
    row->setSpacing(0.0f);
    row->setStretch(1);

    // ── Left: TreeView ────────────────────────────────────────────────────
    auto* treePanel = row->createChild<Panel>();
    treePanel->setSize(220, 0);
    auto* treeBox = treePanel->createChild<BoxLayout>(LayoutDir::Vertical);
    treeBox->setPadding(8.0f);
    treeBox->setSpacing(4.0f);
    treeBox->setStretch(1);
    treeBox->createChild<Label>("Scene")->setColor(Color(140, 160, 180, 255));
    treeBox->createChild<Line>();

    auto* sv = treeBox->createChild<ScrollView>();
    sv->setStretch(1);
    auto* tree = sv->setContent<TreeView>();

    auto* r = tree->addRoot("Scene");
    auto* cam = r->addChild("Camera");
    cam->addChild("FrustumGizmo");
    auto* lights = r->addChild("Lighting");
    lights->addChild("Sun");
    lights->addChild("Ambient");
    auto* objs = r->addChild("Objects");
    auto* player = objs->addChild("Player");
    player->addChild("Mesh");
    player->addChild("Collider");
    objs->addChild("Ground");
    auto* trees = objs->addChild("Trees");
    trees->addChild("Tree_01");
    trees->addChild("Tree_02");
    trees->addChild("Tree_03");
    r->setExpanded(true);
    lights->setExpanded(true);
    objs->setExpanded(true);

    // ── Right: PropertyGrid ───────────────────────────────────────────────
    auto* propPanel = row->createChild<Panel>();
    propPanel->setStretch(1);
    auto* propBox = propPanel->createChild<BoxLayout>(LayoutDir::Vertical);
    propBox->setPadding(8.0f);
    propBox->setSpacing(4.0f);
    propBox->setStretch(1);
    propBox->createChild<Label>("Properties")->setColor(Color(140, 160, 180, 255));
    propBox->createChild<Line>();

    auto* sv2 = propBox->createChild<ScrollView>();
    sv2->setStretch(1);
    auto* pg = sv2->setContent<PropertyGrid>();

    pg->addSection("Transform");
    pg->addVec3("Position",  0.f,  0.f,  0.f, -1000.f, 1000.f);
    pg->addVec3("Rotation",  0.f,  0.f,  0.f,  -360.f,  360.f);
    pg->addVec3("Scale",     1.f,  1.f,  1.f,     0.f,  100.f);

    pg->addSection("Mesh Renderer");
    pg->addColor("Albedo",   Color(210, 210, 210, 255));
    pg->addFloat("Roughness", 0.5f, 0.f, 1.f);
    pg->addFloat("Metallic",  0.0f, 0.f, 1.f);
    pg->addBool("Cast Shadow",   true);
    pg->addBool("Receive Shadow", true);

    pg->addSection("Rigidbody");
    pg->addFloat("Mass",         1.0f, 0.f, 1000.f);
    pg->addFloat("Drag",         0.0f, 0.f,   10.f);
    pg->addFloat("Angular Drag", 0.05f,0.f,   10.f);
    pg->addBool("Use Gravity",   true);
    pg->addBool("Is Kinematic",  false);

    pg->addSection("Tags & Layers");
    pg->addBool("Static",   false);
    pg->addBool("Visible",  true);
    pg->addBool("Pickable", true);
}
