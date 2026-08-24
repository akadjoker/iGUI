#include "WidgetApp.hpp"
#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "ScrollWidgets.hpp"
using namespace ig::retained;

void registerMenuStage(WidgetApp& app)
{
    auto* root = app.addStage("menu");
    auto* sv   = root->createChild<ScrollView>();
    sv->setStretch(1);

    auto* vbox = sv->setContent<BoxLayout>(LayoutDir::Vertical);
    vbox->setSpacing(10.0f);
    vbox->setPadding(40.0f);

    vbox->createChild<Label>("ig::retained + Raylib — Port Demo");
    vbox->createChild<Spacer>(20.0f);

    struct { const char* label; const char* stage; } entries[] = {
        { "01 - Basic Widgets",    "basic"      },
        { "02 - Controls",         "controls"   },
        { "03 - Scroll & Lists",   "scroll"     },
        { "04 - Inputs & Text",    "inputs"     },
        { "05 - Menus & Context",  "menus"      },
        { "06 - Dialogs & Toasts", "dialogs"    },
        { "07 - Dock Panel",       "dock"       },
        { "08 - Properties",       "properties" },
        { "09 - Code Editor",      "editor"     },
        { "10 - Node Editor",      "nodes"      },
        { "11 - Timeline",         "timeline"   },
        { "12 - Gizmos",           "gizmos"     },
        { "13 - Advanced Tools",   "tools"      },
        { "14 - Widget Gallery",   "gallery"    },
        { "15 - Specialty Widgets","specialty"  },
    };
    for (auto& e : entries) {
        auto* btn = vbox->createChild<Button>(e.label);
        const char* s = e.stage;
        btn->clicked.connect([&app, s]() {
            app.setStage(s, TransitionType::CoverLeft);
        });
    }
}
