#include "WidgetApp.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "MenuWidgets.hpp"
#include <cstdio>
using namespace BuGUI;

static Label* s_menuStatus  = nullptr;
static Menu*  s_menuCtxMenu = nullptr;

void registerMenusStage(WidgetApp& app)
{
    s_menuStatus = nullptr;

    auto* root = app.addStage("menus");
    auto* vbox = root->createChild<BoxLayout>(LayoutDir::Vertical);
    vbox->setSpacing(0.0f);
    vbox->setPadding(0.0f);

    // ── Top bar ───────────────────────────────────────────────────────────
    auto* topBar = vbox->createChild<BoxLayout>(LayoutDir::Horizontal);
    topBar->setSpacing(8.0f);
    topBar->setPadding(Edges(8.0f, 12.0f));
    auto* back = topBar->createChild<Button>("\u2190 Menu");
    back->clicked.connect([&app]() { app.setStage("menu", TransitionType::CoverRight); });
    topBar->createChild<Spacer>(0.0f)->setStretch(1.0f);
    topBar->createChild<Label>("Menus & Context")->setAlign(TextAlign::RIGHT);
    vbox->createChild<Line>();

    // ── MenuBar ───────────────────────────────────────────────────────────
    auto* bar = vbox->createChild<MenuBar>();

    // File menu
    {
        Menu* file = bar->addMenu("File");
        file->addAction("New",     []() { if (s_menuStatus) s_menuStatus->setText("File > New");     })->setShortcut("Ctrl+N");
        file->addAction("Open...", []() { if (s_menuStatus) s_menuStatus->setText("File > Open..."); })->setShortcut("Ctrl+O");

        static Menu* recentMenu = nullptr;
        if (!recentMenu) {
            recentMenu = new Menu();
            recentMenu->addAction("project_alpha.bugui", []() { if (s_menuStatus) s_menuStatus->setText("Recent > project_alpha.bugui"); });
            recentMenu->addAction("ui_mockup_v2.bugui",  []() { if (s_menuStatus) s_menuStatus->setText("Recent > ui_mockup_v2.bugui");  });
            recentMenu->addSeparator();
            recentMenu->addAction("Clear Recent",        []() { if (s_menuStatus) s_menuStatus->setText("Recent cleared"); });
        }
        file->addAction("Open Recent")->setSubmenu(recentMenu);
        file->addAction("Save",    []() { if (s_menuStatus) s_menuStatus->setText("File > Save");    })->setShortcut("Ctrl+S");
        file->addSeparator();
        file->addAction("Exit",    []() { if (s_menuStatus) s_menuStatus->setText("File > Exit");    });
    }

    // Edit menu
    {
        Menu* edit = bar->addMenu("Edit");
        edit->addAction("Undo",       []() { if (s_menuStatus) s_menuStatus->setText("Edit > Undo");       })->setShortcut("Ctrl+Z");
        edit->addAction("Redo",       []() { if (s_menuStatus) s_menuStatus->setText("Edit > Redo");       })->setShortcut("Ctrl+Y");
        edit->addSeparator();
        edit->addAction("Cut",        []() { if (s_menuStatus) s_menuStatus->setText("Edit > Cut");        })->setShortcut("Ctrl+X");
        edit->addAction("Copy",       []() { if (s_menuStatus) s_menuStatus->setText("Edit > Copy");       })->setShortcut("Ctrl+C");
        edit->addAction("Paste",      []() { if (s_menuStatus) s_menuStatus->setText("Edit > Paste");      })->setShortcut("Ctrl+V");
        edit->addSeparator();
        auto* find = edit->addAction("Find...");
        find->setShortcut("Ctrl+F");
        find->setEnabled(false);
    }

    // View menu (checkable items)
    {
        Menu* view = bar->addMenu("View");
        auto* grid = view->addCheckable("Show Grid",   true);
        auto* dark = view->addCheckable("Dark Background", true);
        grid->triggered.connect([grid]() {
            if (!s_menuStatus) return;
            char buf[64]; std::snprintf(buf, sizeof(buf), "Show Grid: %s", grid->isChecked() ? "on" : "off");
            s_menuStatus->setText(buf);
        });
        dark->triggered.connect([dark]() {
            if (!s_menuStatus) return;
            char buf[64]; std::snprintf(buf, sizeof(buf), "Dark Background: %s", dark->isChecked() ? "on" : "off");
            s_menuStatus->setText(buf);
        });
        view->addSeparator();
        view->addAction("Zoom In",  []() { if (s_menuStatus) s_menuStatus->setText("View > Zoom In");  })->setShortcut("Ctrl++");
        view->addAction("Zoom Out", []() { if (s_menuStatus) s_menuStatus->setText("View > Zoom Out"); })->setShortcut("Ctrl+-");
    }

    // ── Canvas (right-click target) ───────────────────────────────────────
    auto* canvas = vbox->createChild<Panel>();
    canvas->setStretch(1.0f);
    canvas->setBgColor({28, 28, 32, 255});
    auto* inner = canvas->createChild<BoxLayout>(LayoutDir::Vertical);
    inner->setMainAlign(MainAlign::Center);
    inner->setPadding(8.0f);
    inner->createChild<Label>("Right-click anywhere for context menu")->setColor({90,90,98,255});
    inner->createChild<Label>("")->setAlign(TextAlign::CENTER);

    if (!s_menuCtxMenu) {
        s_menuCtxMenu = new Menu();
        s_menuCtxMenu->addAction("Cut",        []() { if (s_menuStatus) s_menuStatus->setText("Context > Cut");   })->setShortcut("Ctrl+X");
        s_menuCtxMenu->addAction("Copy",       []() { if (s_menuStatus) s_menuStatus->setText("Context > Copy");  })->setShortcut("Ctrl+C");
        s_menuCtxMenu->addAction("Paste",      []() { if (s_menuStatus) s_menuStatus->setText("Context > Paste"); })->setShortcut("Ctrl+V");
        s_menuCtxMenu->addSeparator();
        s_menuCtxMenu->addAction("Select All", []() { if (s_menuStatus) s_menuStatus->setText("Context > Select All"); })->setShortcut("Ctrl+A");
        s_menuCtxMenu->addSeparator();

        static Menu* insertMenu = nullptr;
        if (!insertMenu) {
            insertMenu = new Menu();
            insertMenu->addAction("Label",   []() { if (s_menuStatus) s_menuStatus->setText("Insert > Label");   });
            insertMenu->addAction("Button",  []() { if (s_menuStatus) s_menuStatus->setText("Insert > Button");  });
            insertMenu->addAction("TextBox", []() { if (s_menuStatus) s_menuStatus->setText("Insert > TextBox"); });
        }
        s_menuCtxMenu->addAction("Insert")->setSubmenu(insertMenu);
    }

    canvas->pressed.connect([](int btn) {
        if (btn == 1 && s_menuCtxMenu)
            s_menuCtxMenu->exec(WidgetApp::instance().mouseX(),
                                WidgetApp::instance().mouseY());
    });

    vbox->createChild<Line>();
    // ── Status bar ────────────────────────────────────────────────────────
    auto* statusRow = vbox->createChild<BoxLayout>(LayoutDir::Horizontal);
    statusRow->setSpacing(0.0f);
    statusRow->setPadding(Edges(5.0f, 12.0f));
    s_menuStatus = statusRow->createChild<Label>("Last action: \u2014");
}
