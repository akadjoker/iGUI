// An editor-sized window for tests and benchmarks, built the way ZenVideo builds
// its own: a menu bar, tool bars, a media list and a side panel of labelled
// sliders, combos and check boxes, a monitor, and a timeline of many clips. No
// standard library: ct only.
#pragma once

#include "RetainedHarness.hpp"

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/ComboBox.hpp>
#include <igui/widgets/InputWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/MenuWidgets.hpp>
#include <igui/widgets/ScrollWidgets.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/Timeline.hpp>
#include <igui/widgets/ToolBar.hpp>

namespace rh
{

struct EditorTree
{
    BoxLayout* outer = nullptr;
    MenuBar* menuBar = nullptr;
    ToolBar* topTools = nullptr;
    ToolBar* tools = nullptr;
    Panel* sidePanel = nullptr;
    BoxLayout* props = nullptr;
    Timeline* timeline = nullptr;
    TextEdit* notes = nullptr;
    ListBox* media = nullptr;
    ct::Vector<Label*> labels;
    ct::Vector<Slider*> sliders;
    ct::Vector<ComboBox*> combos;
    ct::Vector<CheckBox*> checks;
    ct::Vector<Button*> buttons;
    int menuActions = 0;

    // `clips` clips spread over four tracks (two video, two audio), the way a cut of a long
    // interview looks: short clips one after another with a few gaps.
    void build(Harness& h, int clips, int sliderRows = 13)
    {
        Widget* stage = h.root();
        outer = stage->createChild<BoxLayout>(LayoutDir::Vertical);
        outer->setPadding(8.0f);
        outer->setSpacing(8.0f);
        outer->setStretch(1);

        menuBar = outer->createChild<MenuBar>();
        const char* titles[] = { "File", "Edit", "View", "Project", "Help" };
        for (int m = 0; m < 5; ++m)
        {
            Menu* menu = menuBar->addMenu(titles[m]);
            for (int a = 0; a < 8; ++a)
            {
                String text = String(titles[m]) + " action " + String::number(a);
                menu->addAction(text, [this] { ++menuActions; })->setShortcut(a % 2 ? "Ctrl+Shift+K" : "");
                if (a == 3) menu->addSeparator();
            }
        }

        BoxLayout* top = outer->createChild<BoxLayout>(LayoutDir::Horizontal);
        top->setSpacing(8.0f);
        top->setStretch(4);

        // Media: a tool bar, a combo, the list, a hint.
        Panel* mediaPanel = top->createChild<Panel>();
        mediaPanel->setStretch(1.2f);
        BoxLayout* mediaBox = mediaPanel->createChild<BoxLayout>(LayoutDir::Vertical);
        mediaBox->setPadding(8.0f);
        mediaBox->setSpacing(6.0f);
        mediaBox->setStretch(1);
        mediaBox->createChild<Label>("Media");
        topTools = mediaBox->createChild<ToolBar>();
        topTools->addButton(IconId::Import, "Import files");
        topTools->addButton(IconId::Folder, "Import a folder");
        ComboBox* library = mediaBox->createChild<ComboBox>();
        library->addItem("Media files");
        library->addItem("Transitions");
        library->addItem("Titles and balloons");
        combos.push_back(library);
        media = mediaBox->createChild<ListBox>();
        media->setStretch(1);
        for (int i = 0; i < 40; ++i)
            media->addItem(String("Entrevista Música ") + String::number(i) + ".mp4");
        labels.push_back(mediaBox->createChild<Label>("40 files, 1h 12m"));

        // Monitor: a panel with a title and a transport row.
        Panel* monitorPanel = top->createChild<Panel>();
        monitorPanel->setStretch(3);
        BoxLayout* monitorBox = monitorPanel->createChild<BoxLayout>(LayoutDir::Vertical);
        monitorBox->setPadding(6.0f);
        monitorBox->setSpacing(6.0f);
        monitorBox->setStretch(1);
        labels.push_back(monitorBox->createChild<Label>("Timeline"));
        Panel* canvas = monitorBox->createChild<Panel>();
        canvas->setStretch(1);
        BoxLayout* transport = monitorBox->createChild<BoxLayout>(LayoutDir::Horizontal);
        ToolBar* transportTools = transport->createChild<ToolBar>();
        transportTools->addButton(IconId::StepBack, "Back");
        transportTools->addButton(IconId::Play, "Play");
        transportTools->addButton(IconId::Stop, "Stop");
        transportTools->addButton(IconId::StepForward, "Forward");
        Label* timecode = transport->createChild<Label>("00:00:00:00");
        labels.push_back(timecode);
        Label* status = transport->createChild<Label>("Ready");
        status->setElide(true);
        status->setStretch(1);
        labels.push_back(status);

        // The properties panel: names above sliders, combos, check boxes.
        sidePanel = top->createChild<Panel>();
        sidePanel->setStretch(1.4f);
        props = sidePanel->createChild<BoxLayout>(LayoutDir::Vertical);
        props->setPadding(8.0f);
        props->setSpacing(6.0f);
        props->setStretch(1);
        Label* title = props->createChild<Label>("Properties");
        labels.push_back(title);
        for (int i = 0; i < sliderRows; ++i)
        {
            BoxLayout* row = props->createChild<BoxLayout>(LayoutDir::Vertical);
            row->setPadding(0.0f);
            row->setSpacing(0.0f);
            BoxLayout* header = row->createChild<BoxLayout>(LayoutDir::Horizontal);
            header->setPadding(0.0f);
            header->setSpacing(4.0f);
            Label* name = header->createChild<Label>(String("Parâmetro número ") + String::number(i));
            name->setElide(true);
            name->setStretch(1.0f);
            Label* value = header->createChild<Label>(String("0.") + String::number(i) + "0");
            value->setAlign(TextAlign::RIGHT);
            Slider* s = row->createChild<Slider>(-1.0f, 1.0f, 0.1f * static_cast<float>(i % 7));
            labels.push_back(name);
            labels.push_back(value);
            sliders.push_back(s);
        }
        for (int i = 0; i < 4; ++i)
        {
            ComboBox* c = props->createChild<ComboBox>();
            c->addItem("Cut");
            c->addItem("Cross dissolve");
            c->addItem("Fade to black");
            c->setSelectedIndex(i % 3);
            combos.push_back(c);
            CheckBox* k = props->createChild<CheckBox>(String("Opção com acentuação ") + String::number(i));
            checks.push_back(k);
        }
        Button* clear = props->createChild<Button>("Clear selected curve");
        buttons.push_back(clear);
        notes = props->createChild<TextEdit>("Notas do projeto\nSegunda linha com ação e vídeo");
        notes->setShowLineNumbers(false);

        tools = outer->createChild<ToolBar>();
        tools->addButton(IconId::Scissors, "Split at the playhead (S)");
        tools->addButton(IconId::Trash, "Delete the selected clip");
        tools->addSeparator();
        tools->addToggle(IconId::Magnet, "Snapping", true);
        tools->addButton(IconId::Plus, "Add a track");
        tools->addButton(IconId::Minus, "Delete the selected track");

        timeline = outer->createChild<Timeline>();
        timeline->setStretch(2);
        timeline->addTrack("Video 2", TrackKind::Video);
        timeline->addTrack("Video 1", TrackKind::Video);
        timeline->addTrack("Audio 1", TrackKind::Audio);
        timeline->addTrack("Audio 2", TrackKind::Audio);
        timeline->setFrameRate(30.0f);
        timeline->setTimeRange(0.0f, 20.0f);
        Rng rng(1234);
        float cursor[4] = { 0, 0, 0, 0 };
        for (int i = 0; i < clips; ++i)
        {
            const int t = i % 4;
            const float length = 0.4f + rng.unit() * 2.0f;
            const float gap = rng.chance(15) ? rng.unit() * 0.5f : 0.0f;
            cursor[t] += gap;
            const int id = timeline->addClip(t, cursor[t], cursor[t] + length,
                    String("Clip ") + String::number(i), Color(80, 120, 180, 200));
            TimelineClip& c = timeline->track(t).clips[id];
            c.media = t >= 2 ? i : -1;
            cursor[t] += length;
        }
    }
};

} // namespace rh
