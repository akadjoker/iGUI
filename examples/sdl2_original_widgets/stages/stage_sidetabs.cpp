#include "WidgetApp.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "ScrollWidgets.hpp"
#include "SideTabBar.hpp"
#include "TreePropertyColorWidgets.hpp"
#include "ConsoleWidget.hpp"
using namespace ig::retained;

// An editor window: a SideTabBar on the left with the labelled pages of a
// pose tool (Character, Camera, Pose, Projects...), the viewport in the
// middle taking whatever room the sidebars leave, and an icon-only SideTabBar
// on the right. Pressing the active button folds a sidebar to its rail.
void registerSideTabsStage(WidgetApp& app)
{
    auto* root  = app.addStage("sidetabs");
    auto* outer = root->createChild<BoxLayout>(LayoutDir::Vertical);
    outer->setStretch(1);
    outer->setSpacing(0);
    outer->setPadding(0);

    SideTabBar* left  = nullptr;
    SideTabBar* right = nullptr;

    // ── Top bar ───────────────────────────────────────────────────────────
    {
        auto* hdr = outer->createChild<BoxLayout>(LayoutDir::Horizontal);
        hdr->setSize(0, 36);
        hdr->setPadding({4, 8, 4, 8});
        hdr->setSpacing(12);
        auto* back = hdr->createChild<Button>("← Menu");
        back->clicked.connect([&app]() { app.setStage("menu", TransitionType::CoverRight); });

        auto* swap = hdr->createChild<Button>("Swap sides");
        swap->clicked.connect([&left, &right]()
        {
            if (!left || !right) return;
            const SideTabBar::Side a = left->side();
            left->setSide(right->side());
            right->setSide(a);
        });

        auto* badge = hdr->createChild<Button>("Toggle Pose badge");
        badge->clicked.connect([&left]()
        {
            if (left) left->setPageBadge(2, !left->pageBadge(2));
        });

        hdr->createChild<Spacer>(0.0f)->setStretch(1.0f);
        hdr->createChild<Label>("SideTabBar — click a tab, click it again to fold")->setAlign(TextAlign::RIGHT);
    }

    // ── Body: sidebar | viewport | sidebar ───────────────────────────────
    auto* body = outer->createChild<BoxLayout>(LayoutDir::Horizontal);
    body->setStretch(1);
    body->setSpacing(0);
    body->setPadding(0);

    left = body->createChild<SideTabBar>(SideTabBar::Side::Left);
    left->setPageWidth(300.0f);

    // Character: the scene tree
    {
        auto* tree = left->addPage<TreeView>(IconId::User, "Character");
        auto* r    = tree->addRoot("Rig");
        auto* arms = r->addChild("Arms");
        arms->addChild("L.Shoulder");
        arms->addChild("L.Elbow");
        arms->addChild("R.Shoulder");
        arms->addChild("R.Elbow");
        auto* legs = r->addChild("Legs");
        legs->addChild("L.Hip");
        legs->addChild("R.Hip");
        r->setExpanded(true);
        arms->setExpanded(true);
    }

    // Camera: a property grid
    {
        auto* pg = left->addPage<PropertyGrid>(IconId::Eye, "Camera");
        pg->addSection("Lens");
        pg->addFloat("Focal length", 50.0f, 12.0f, 200.0f);
        pg->addFloat("Aperture", 2.8f, 1.0f, 22.0f);
        pg->addSection("Framing");
        pg->addVec3("Target", 0.f, 1.f, 0.f, -100.f, 100.f);
        pg->addBool("Look at target", true);
    }

    // Pose: the controls of the sketch (axes, snap, mirror...)
    {
        auto* pose = left->addPage<BoxLayout>(IconId::Edit, "Pose", LayoutDir::Vertical);
        pose->setPadding(12.0f);
        pose->setSpacing(8.0f);
        pose->createChild<Label>("Click a joint in the viewport, then turn it with the gizmo.")->setColor(Color(150, 152, 158, 255));

        auto* axes = pose->createChild<BoxLayout>(LayoutDir::Horizontal);
        axes->setSpacing(8.0f);
        axes->createChild<Label>("Axes");
        axes->createChild<Spacer>(0.0f)->setStretch(1.0f);
        axes->createChild<Button>("Global");
        axes->createChild<Button>("X-Mirror");

        auto* snap = pose->createChild<BoxLayout>(LayoutDir::Horizontal);
        snap->setSpacing(8.0f);
        snap->createChild<Label>("Snap");
        snap->createChild<Spacer>(0.0f)->setStretch(1.0f);
        snap->createChild<Button>("Off");
        snap->createChild<Button>("5°");
        snap->createChild<Button>("15°");

        pose->createChild<Line>();
        auto* row1 = pose->createChild<BoxLayout>(LayoutDir::Horizontal);
        row1->setSpacing(8.0f);
        row1->createChild<Button>("Reset key");
        row1->createChild<Button>("Copy pose");
        auto* row2 = pose->createChild<BoxLayout>(LayoutDir::Horizontal);
        row2->setSpacing(8.0f);
        row2->createChild<Button>("Mirror L→R");
        row2->createChild<Button>("Mirror R→L");
        left->setPageBadge(2, true);   // keys were set on this frame
        left->setPageTooltip(2, "Pose the selected joints");
    }

    // Projects: a list
    {
        auto* list = left->addPage<ListBox>(IconId::Star, "Projects");
        list->addItem("walk_cycle");
        list->addItem("idle_breathing");
        list->addItem("jump_takeoff");
        list->addItem("wave_hello");
    }

    // Import: no icon, so the rail shows the label's initial; disabled here
    {
        auto* page = left->addPage<BoxLayout>(IconId::None, "Import", LayoutDir::Vertical);
        page->setPadding(12.0f);
        page->createChild<Label>("Drop a .fbx or .bvh on the window.");
        left->setPageEnabled(4, false);
    }
    left->setCurrentIndex(2);

    // Viewport placeholder: it grows when a sidebar folds
    {
        auto* viewport = body->createChild<Panel>();
        viewport->setStretch(1);
        viewport->setBgColor(Color(22, 23, 26, 255));
        auto* box = viewport->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(24.0f);
        box->setSpacing(8.0f);
        box->createChild<Label>("Viewport")->setColor(Color(100, 160, 240, 255));
        box->createChild<Label>("Fold a sidebar (click its active tab) and this area takes the room.");
    }

    // Right rail: icons only, the label is the tooltip
    right = body->createChild<SideTabBar>(SideTabBar::Side::Right);
    right->setShowLabels(false);
    right->setButtonHeight(44.0f);
    right->setPageWidth(260.0f);
    {
        auto* pg = right->addPage<PropertyGrid>(IconId::Gear, "Settings");
        pg->addSection("Display");
        pg->addBool("Show bones", true);
        pg->addBool("Show grid", true);
        pg->addFloat("Gizmo size", 1.0f, 0.2f, 3.0f);
    }
    {
        auto* con = right->addPage<ConsoleWidget>(IconId::Info, "Log");
        con->log(LogLevel::Info, "Rig loaded: 31 bones");
        con->log(LogLevel::Info, "Pose 1: 1 key");
        con->log(LogLevel::Warn, "Import is disabled in this demo");
    }
    {
        auto* help = right->addPage<BoxLayout>(IconId::Book, "Help", LayoutDir::Vertical);
        help->setPadding(12.0f);
        help->setSpacing(6.0f);
        help->createChild<Label>("Shift+click adds joints.");
        help->createChild<Label>("Letting go keys them on this frame.");
    }
    right->collapse();   // starts folded; press an icon to open it
}
