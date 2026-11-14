// A random run against an editor-sized widget tree: events (moves, buttons, drags, wheel, keys,
// typing, file drops, resizes) and tree mutations (widgets added and removed, hidden and disabled,
// moved among their siblings, floats opened and closed, stages switched, timeline clips and
// tracks added and removed), with handlers on the widgets' own signals that remove widgets too -
// often the widget the signal came from - and widgets that change the tree while it is laid out
// and painted. After every step the manager's pointers (focus, popup, drag) must be in the
// tree, the tree must be consistent and the draw data well formed. Run it under ASan/UBSan: a
// pointer left at a deleted widget is a use after free. The same seed twice must give the same
// pictures (nothing depends on an address). `igui_retained_fuzz_tests [seeds] [steps]`.

#include "EditorTree.hpp"

#include <igui/widgets/DialogWidgets.hpp>
#include <igui/widgets/MenuWidgets.hpp>
#include <igui/widgets/SpinSlider.hpp>
#include <igui/widgets/ViewWidgets.hpp>

#include <stdlib.h>

using namespace rh;

namespace
{

class Fuzz;
Fuzz* gFuzz = nullptr;

// ---- the run ---------------------------------------------------------------------------
class Fuzz
{
public:
    Fuzz(uint32_t seed, int clips) : rng_(seed), h_(1280.0f, 720.0f)
    {
        gFuzz = this;
        editor_.build(h_, clips);
        h_.app.addStage("other")->createChild<Label>("the other stage");
        scratch_ = editor_.props;
        h_.relayout();
        hookAll();
    }
    ~Fuzz()
    {
        gFuzz = nullptr;
    }

    Harness& harness()
    {
        return h_;
    }
    uint64_t hash() const
    {
        return hash_;
    }
    int problems() const
    {
        return problems_;
    }

    // One random step.
    void step()
    {
        lastWhat_ = -1;
        const int op = rng_.below(100);
        lastOp_ = op;
        if (op < 24) pointerOp();
        else if (op < 34)
            keyOp();
        else if (op < 40)
            typeOp();
        else if (op < 44)
            wheelOp();
        else if (op < 47)
            dropOp();
        else if (op < 50)
            resizeOp();
        else if (op < 82)
            mutateOp();
        else if (op < 90)
            frameOp();
        else
            dragOp();
        frameAndCheck();
    }

    // Used by the handlers: a random mutation, the widget a signal came from as the target now and then.
    void chaos(Widget* from)
    {
        if (budget_ <= 0) return;
        --budget_;
        const int what = rng_.below(10);
        if (what < 5)
        {
            // remove a widget: the signal's own, or a random one
            Widget* victim = from && rng_.chance(50) ? from : randomRemovable();
            removeWidget(victim);
        }
        else if (what < 6)
        {
            if (Widget* w = randomWidget()) w->setVisible(!w->isVisible());
        }
        else if (what < 7)
        {
            if (Widget* w = randomWidget()) w->setEnabled(!w->isEnabled());
        }
        else if (what < 8)
            addRandom();
        else if (what < 9)
            h_.app.closePopup();
        else
            h_.app.requestLayout();
    }

    // A widget that takes widgets out of the tree while its parent lays out or paints it: a
    // sibling, or itself (what an animation that ends does). Adding widgets there is not
    // asked of the toolkit: a parent walking its children does not expect the list to grow.
    void chaosInLayoutOrPaint(Widget* self)
    {
        if (budget_ <= 0 || !rng_.chance(8)) return;
        --budget_;
        Widget* parent = self->parent();
        if (!parent) return;
        const int what = rng_.below(3);
        if (what == 0 && parent->children().size() > 1)
        {
            Widget* sibling = parent->children()[static_cast<size_t>(rng_.below(static_cast<int>(parent->children().size())))];
            if (sibling != self && removable(sibling)) parent->removeChild(sibling);
        }
        else if (what == 1 && removable(self))
            parent->removeChild(self);
        else
            h_.app.requestLayout();
    }

private:
    // ---- finding widgets ----
    void collect(Widget* w, ct::Vector<Widget*>& out) const
    {
        out.push_back(w);
        for (size_t i = 0; i < w->children().size(); ++i) collect(w->children()[i], out);
    }
    void allWidgets(ct::Vector<Widget*>& out) const
    {
        if (h_.app.root()) collect(h_.app.root(), out);
        for (size_t i = 0; i < h_.app.floats().size(); ++i) collect(h_.app.floats()[i], out);
        if (h_.app.popup()) collect(h_.app.popup(), out);
    }
    Widget* randomWidget()
    {
        ct::Vector<Widget*> all;
        allWidgets(all);
        // never the stage root itself: the run needs somewhere to stand
        if (all.size() < 2) return nullptr;
        Widget* w = all[static_cast<size_t>(1 + rng_.below(static_cast<int>(all.size()) - 1))];
        return w;
    }
    template<typename T>
    T* randomOf()
    {
        ct::Vector<Widget*> all, picked;
        allWidgets(all);
        for (size_t i = 0; i < all.size(); ++i)
            if (T* t = widget_cast<T>(all[i])) picked.push_back(t);
        if (picked.empty()) return nullptr;
        return static_cast<T*>(picked[static_cast<size_t>(rng_.below(static_cast<int>(picked.size())))]);
    }

    // Only what the application put there: a composite widget (a timeline, a combo box, a tool
    // bar) keeps pointers to the widgets it made inside itself, and taking those out is not
    // something the application may do.
    bool removable(const Widget* w) const
    {
        const Widget* p = w->parent();
        if (!p) return false;
        return widget_cast<BoxLayout>(p) || widget_cast<Panel>(p) || widget_cast<Splitter>(p) || p == h_.app.root();
    }
    // A removable widget now, or a random one when there is none to be found.
    Widget* randomRemovable()
    {
        ct::Vector<Widget*> all, ok;
        allWidgets(all);
        for (size_t i = 1; i < all.size(); ++i)
            if (removable(all[i]) && all[i] != h_.app.root()) ok.push_back(all[i]);
        if (ok.empty()) return nullptr;
        return ok[static_cast<size_t>(rng_.below(static_cast<int>(ok.size())))];
    }

    void removeWidget(Widget* w)
    {
        if (!w || !w->parent()) return;
        if (w == h_.app.root() || !removable(w)) return;
        w->parent()->removeChild(w);
        ++removed_;
    }

    // ---- handlers on the signals of the widgets ----
    void hook(Widget* w)
    {
        if (!rng_.chance(35)) return;
        Fuzz* self = this;
        Widget* wp = w;
        // The handlers hold the widget they are on: a handler that runs is on a live widget (the
        // signal is the widget's own), so the pointer is good while it runs.
        switch (rng_.below(6))
        {
            case 0: w->clicked.connect([self, wp] { self->chaos(wp); }); break;
            case 1: w->hoverEnter.connect([self, wp] { self->chaos(wp); }); break;
            case 2: w->hoverLeave.connect([self, wp] { self->chaos(wp); }); break;
            case 3: w->pressed.connect([self, wp](int) { self->chaos(wp); }); break;
            case 4: w->released.connect([self, wp](int) { self->chaos(wp); }); break;
            default: w->moved.connect([self, wp](float, float) { if (self->rng_.chance(5)) self->chaos(wp); }); break;
        }
        if (Slider* s = widget_cast<Slider>(w)) s->onValueChanged.connect([self, wp](float) { self->chaos(wp); });
        if (ComboBox* c = widget_cast<ComboBox>(w)) c->selectionChanged.connect([self, wp](int) { self->chaos(wp); });
        if (TextInput* t = widget_cast<TextInput>(w)) t->textChanged.connect([self, wp](const String&) { self->chaos(wp); });
        if (TextEdit* t = widget_cast<TextEdit>(w)) t->textChanged.connect([self, wp] { self->chaos(wp); });
        if (CheckBox* c = widget_cast<CheckBox>(w)) c->toggled.connect([self, wp](bool) { self->chaos(wp); });
        if (Splitter* sp = widget_cast<Splitter>(w)) sp->ratioChanged.connect([self, wp](float) { self->chaos(wp); });
        if (SpinSlider* sp = widget_cast<SpinSlider>(w)) sp->valueChanged.connect([self, wp](double) { self->chaos(wp); });
        if (Timeline* t = widget_cast<Timeline>(w))
        {
            t->onClipSelected.connect([self, wp](int, int) { self->chaos(wp); });
            t->onClipChanged.connect([self, wp](int, int) { if (self->rng_.chance(10)) self->chaos(wp); });
            t->onClipEdited.connect([self, wp](int, int) { self->chaos(wp); });
            t->onPlayheadChanged.connect([self, wp](float) { if (self->rng_.chance(10)) self->chaos(wp); });
            t->onClipContextMenu.connect([self, wp](int, int, float, float) { self->chaos(wp); });
        }
    }
    void hookAll()
    {
        ct::Vector<Widget*> all;
        allWidgets(all);
        for (size_t i = 0; i < all.size(); ++i) hook(all[i]);
    }

    // ---- making widgets ----
    class Chaos : public Widget
    {
    public:
        Vec2f sizeHint() const override
        {
            if (gFuzz) gFuzz->chaosInLayoutOrPaint(const_cast<Chaos*>(this));
            return { 40.0f, 20.0f };
        }
        void layout() override
        {
            Widget::layout();
        }
        void paint(PaintContext& ctx) override
        {
            if (gFuzz) gFuzz->chaosInLayoutOrPaint(this);
            Widget::paint(ctx);
            ctx.fill.SetColor(200, 40, 40, 255);
            const Rect r = absoluteRect();
            ctx.fillRect(r.x, r.y, r.w, r.h);
        }
    };

    // Widgets that take part in a drag: a source (removed when the drag ends, now and then) and a
    // target (removed when it receives the drop).
    class DragSrc : public Widget
    {
    public:
        DragSrc() { cursor_ = CursorType::Hand; }
        bool isDragSource() const override { return true; }
        DragPayload onDragBegin() override
        {
            DragPayload p;
            p.type = "thing";
            p.data = 1;
            p.source = this;
            return p;
        }
        void onDragEnd(bool) override { if (gFuzz) gFuzz->chaos(this); }
        Vec2f sizeHint() const override { return { 60.0f, 30.0f }; }
    };
    class DropTgt : public Widget
    {
    public:
        bool acceptsDrop(const DragPayload&) override { return true; }
        void onDropReceive(const DragPayload&) override { if (gFuzz) gFuzz->chaos(this); }
        void onDragHover(const DragPayload&, float, float) override {}
        void onFileDrop(const ct::Vector<String>&) override { if (gFuzz) gFuzz->chaos(this); }
        Vec2f sizeHint() const override { return { 60.0f, 30.0f }; }
    };

    const char* randomText()
    {
        static const char* texts[] = { "", "Música", "Vídeos e ação", "x", "a much longer text that will have to be cut with dots somewhere",
            "\xF0\x9F\x8E\xB5 emoji", "\x80\xFF broken", "ççç", "linha\nsegunda", "0.5", "-12" };
        return texts[rng_.below(static_cast<int>(sizeof texts / sizeof texts[0]))];
    }

    Widget* makeWidget(Widget* parent, int depth)
    {
        const int kind = rng_.below(depth > 2 ? 11 : 18);
        Widget* w = nullptr;
        switch (kind)
        {
            case 0: w = parent->createChild<Label>(randomText()); break;
            case 1: w = parent->createChild<Button>(randomText()); break;
            case 2: w = parent->createChild<CheckBox>(randomText()); break;
            case 3: w = parent->createChild<Slider>(-1.0f, 1.0f, 0.2f); break;
            case 4:
            {
                ComboBox* c = parent->createChild<ComboBox>();
                for (int i = 0; i < 1 + rng_.below(5); ++i) c->addItem(randomText());
                w = c;
                break;
            }
            case 5: w = parent->createChild<TextInput>(randomText()); break;
            case 6: w = parent->createChild<TextEdit>(randomText()); break;
            case 7:
            {
                SpinSlider* s = parent->createChild<SpinSlider>();
                s->setMax(100.0);
                s->setStep(0.5);
                w = s;
                break;
            }
            case 8: w = parent->createChild<Chaos>(); break;
            case 9:
            {
                ListBox* l = parent->createChild<ListBox>();
                for (int i = 0; i < rng_.below(30); ++i) l->addItem(randomText());
                w = l;
                break;
            }
            case 10: w = parent->createChild<Panel>(); break;
            case 11:
            {
                BoxLayout* b = parent->createChild<BoxLayout>(rng_.chance(50) ? LayoutDir::Vertical : LayoutDir::Horizontal);
                for (int i = 0; i < 1 + rng_.below(4); ++i) makeWidget(b, depth + 1);
                w = b;
                break;
            }
            case 12:
            {
                Splitter* s = parent->createChild<Splitter>(rng_.chance(50) ? LayoutDir::Vertical : LayoutDir::Horizontal);
                makeWidget(s, depth + 1);
                makeWidget(s, depth + 1);
                w = s;
                break;
            }
            case 13:
            {
                ScrollView* sv = parent->createChild<ScrollView>();
                for (int i = 0; i < rng_.below(20); ++i) makeWidget(sv, depth + 1);
                w = sv;
                break;
            }
            case 14:
            {
                Timeline* t = parent->createChild<Timeline>();
                for (int i = 0; i < 2; ++i) t->addTrack("T", i ? TrackKind::Audio : TrackKind::Video);
                for (int i = 0; i < rng_.below(30); ++i) t->addClip(rng_.below(2), rng_.range(0.0f, 10.0f), rng_.range(10.0f, 15.0f));
                t->setTimeRange(0.0f, 12.0f);
                w = t;
                break;
            }
            case 16: w = parent->createChild<DragSrc>(); break;
            case 17: w = parent->createChild<DropTgt>(); break;
            default:
            {
                ToolBar* tb = parent->createChild<ToolBar>();
                for (int i = 0; i < 1 + rng_.below(5); ++i) tb->addButton(IconId::Gear, "tip");
                w = tb;
                break;
            }
        }
        if (w && rng_.chance(15)) w->setTooltip(randomText());
        if (w && rng_.chance(5)) w->setStretch(rng_.range(0.0f, 2.0f));
        if (w) hook(w);
        return w;
    }

    void addRandom()
    {
        ct::Vector<Widget*> all;
        allWidgets(all);
        Widget* parent = nullptr;
        for (int tries = 0; tries < 5 && !parent; ++tries)
        {
            Widget* w = all[static_cast<size_t>(rng_.below(static_cast<int>(all.size())))];
            // containers that lay their children out: a box, a panel, a scroll view, the stage
            if (widget_cast<BoxLayout>(w) || widget_cast<Panel>(w) || widget_cast<ScrollView>(w)) parent = w;
        }
        if (!parent) parent = scratch_ && isIn(scratch_) ? scratch_ : h_.root();
        makeWidget(parent, 0);
        h_.app.requestLayout();
    }
    bool isIn(Widget* w) const
    {
        ct::Vector<Widget*> all;
        allWidgets(all);
        for (size_t i = 0; i < all.size(); ++i)
            if (all[i] == w) return true;
        return false;
    }

    // ---- the operations ----
    void aim(float& x, float& y)
    {
        const int how = rng_.below(10);
        if (how < 3)
        {
            x = rng_.range(-20.0f, h_.io().displayWidth + 20.0f);
            y = rng_.range(-20.0f, h_.io().displayHeight + 20.0f);
            return;
        }
        Widget* w = randomWidget();
        if (!w)
        {
            x = y = 10.0f;
            return;
        }
        const Rect r = w->absoluteRect();
        const int where = rng_.below(5);
        x = r.x + (where == 1 ? 1.0f : where == 2 ? r.w - 1.0f : r.w * rng_.unit());
        y = r.y + (where == 3 ? 1.0f : where == 4 ? r.h - 1.0f : r.h * rng_.unit());
    }

    void pointerOp()
    {
        float x, y;
        aim(x, y);
        const int button = rng_.chance(80) ? 0 : rng_.below(3);
        h_.io().keyCtrl = rng_.chance(10);
        h_.io().keyShift = rng_.chance(10);
        const int kind = rng_.below(10);
        if (kind < 3) h_.moveTo(x, y);
        else if (kind < 7)
            h_.click(x, y, button);
        else if (kind < 9)
        {
            h_.click(x, y, button);
            h_.click(x, y, button); // a double click
        }
        else
        {
            h_.moveTo(x, y);
            h_.down(button);
            float x2, y2;
            aim(x2, y2);
            h_.moveTo(x2, y2);
            h_.up(button);
        }
        h_.io().keyCtrl = h_.io().keyShift = false;
    }
    void dragOp()
    {
        float x, y, x2, y2;
        aim(x, y);
        h_.moveTo(x, y);
        h_.down(0);
        const int moves = 1 + rng_.below(6);
        for (int i = 0; i < moves; ++i)
        {
            aim(x2, y2);
            h_.moveTo(x2, y2);
            if (rng_.chance(15)) chaos(nullptr);
        }
        h_.up(0);
    }
    void keyOp()
    {
        static const int keys[] = { Key::Tab, Key::Return, Key::Escape, Key::Left, Key::Right, Key::Up, Key::Down, Key::Home, Key::End,
            Key::PageUp, Key::PageDown, Key::Backspace, Key::Delete, Key::Space, Key::F1, Key::F5, 'a', 'c', 'v', 'x', 'z', 's', '+', '-' };
        const int key = keys[rng_.below(static_cast<int>(sizeof keys / sizeof keys[0]))];
        h_.key(key, rng_.chance(25), rng_.chance(15), rng_.chance(5));
    }
    void typeOp()
    {
        h_.type(randomText());
    }
    void wheelOp()
    {
        float x, y;
        aim(x, y);
        h_.io().keyCtrl = rng_.chance(15);
        h_.io().keyShift = rng_.chance(15);
        h_.moveTo(x, y);
        h_.wheel(rng_.range(-5.0f, 5.0f), rng_.chance(20) ? rng_.range(-3.0f, 3.0f) : 0.0f);
        h_.io().keyCtrl = h_.io().keyShift = false;
    }
    void dropOp()
    {
        float x, y;
        aim(x, y);
        ct::Vector<String> paths;
        paths.push_back("/tmp/nothing/Música.mp4");
        paths.push_back("/tmp/nothing/b.wav");
        h_.io().addDropEvent(x, y, paths, rng_.chance(30) ? "dropped text" : "");
    }
    void resizeOp()
    {
        const float sizes[][2] = { { 1280.0f, 720.0f }, { 640.0f, 360.0f }, { 1.0f, 1.0f }, { 200.0f, 50.0f }, { 2560.0f, 1440.0f },
            { 0.0f, 0.0f }, { 5.0f, 700.0f }, { 1300.0f, 20.0f } };
        const int i = rng_.below(static_cast<int>(sizeof sizes / sizeof sizes[0]));
        h_.io().displayWidth = sizes[i][0];
        h_.io().displayHeight = sizes[i][1];
    }
    void frameOp()
    {
        h_.io().deltaTime = rng_.chance(20) ? 0.5f : 1.0f / 60.0f; // tooltips and animations need time
        h_.frames(1 + rng_.below(3));
        h_.io().deltaTime = 1.0f / 60.0f;
    }

    void mutateOp()
    {
        const int what = rng_.below(24);
        lastWhat_ = what;
        if (what < 6) addRandom();
        else if (what < 11)
            removeWidget(randomRemovable());
        else if (what == 11)
        {
            if (Widget* w = randomWidget()) w->setVisible(!w->isVisible());
        }
        else if (what == 12)
        {
            if (Widget* w = randomWidget()) w->setEnabled(!w->isEnabled());
        }
        else if (what == 13)
        {
            Widget* w = randomRemovable();
            if (w && w->parent() && w->parent()->children().size() > 1)
                w->parent()->moveChild(w, rng_.below(static_cast<int>(w->parent()->children().size()) + 2) - 1);
        }
        else if (what == 14)
        {
            if (Label* l = randomOf<Label>()) l->setText(randomText());
        }
        else if (what == 15)
        {
            if (TextEdit* t = randomOf<TextEdit>()) t->setText(randomText());
            if (TextInput* t = randomOf<TextInput>()) t->setText(randomText());
        }
        else if (what == 16)
        {
            if (Slider* s = randomOf<Slider>()) s->setRange(rng_.range(-5.0f, 5.0f), rng_.range(-5.0f, 5.0f));
        }
        else if (what == 17)
        {
            if (Timeline* t = randomOf<Timeline>()) timelineMutation(t);
        }
        else if (what == 18 && rng_.chance(40))
        {
            // the dialogs of the toolkit: they close themselves on a button or a key
            if (rng_.chance(50))
            {
                MessageBox* mb = h_.app.addFloat<MessageBox>("Question", "Really?", MessageBox::YesNoCancel, MessageBox::Question);
                mb->setFloatPos(rng_.range(0.0f, 900.0f), rng_.range(0.0f, 500.0f));
                mb->result.connect([this](MessageBox::Result) { chaos(nullptr); });
            }
            else
            {
                ChoiceDialog* cd = h_.app.addFloat<ChoiceDialog>("Unsaved", "Save before leaving?");
                cd->addChoice("Save", IconId::Check, ChoiceDialog::Role::Default);
                cd->addChoice("Cancel", IconId::Cross, ChoiceDialog::Role::Cancel);
                cd->setFloatPos(rng_.range(0.0f, 900.0f), rng_.range(0.0f, 500.0f));
                cd->chosen.connect([this](int) { chaos(nullptr); });
            }
        }
        else if (what == 18)
        {
            // a float window with a button that closes it
            FloatWindow* fw = h_.app.addFloat<FloatWindow>("Dialog");
            fw->setFloatPos(rng_.range(0.0f, 900.0f), rng_.range(0.0f, 500.0f));
            fw->setFloatSize(rng_.range(100.0f, 400.0f), rng_.range(60.0f, 300.0f));
            BoxLayout* box = new BoxLayout(LayoutDir::Vertical);
            fw->setContentWidget(box);
            Button* close = box->createChild<Button>("Close");
            Fuzz* self = this;
            close->clicked.connect([self, fw] { self->h_.app.removeFloat(fw); });
            makeWidget(box, 2);
            hook(close);
        }
        else if (what == 19)
        {
            if (!h_.app.floats().empty()) h_.app.removeFloat(h_.app.floats()[static_cast<size_t>(rng_.below(static_cast<int>(h_.app.floats().size())))]);
        }
        else if (what == 20)
            h_.app.setStage(h_.app.currentStageName() == String("harness") ? "other" : "harness", TransitionType::None);
        else if (what == 21)
        {
            // a context menu on a random widget, then a right click
            if (Widget* w = randomWidget())
            {
                Menu* m = new Menu();
                m->addAction("One", [this] { chaos(nullptr); });
                m->addAction("Two");
                w->setContextMenu(m);
            }
        }
        else if (what == 22)
        {
            if (ComboBox* c = randomOf<ComboBox>()) c->openDropdown();
        }
        else
            h_.app.requestLayout();
        budget_ = 4;
    }

    void timelineMutation(Timeline* t)
    {
        switch (rng_.below(8))
        {
            case 0: t->addClip(rng_.below(t->trackCount() > 0 ? t->trackCount() : 1), rng_.range(-1.0f, 20.0f), rng_.range(0.0f, 21.0f)); break;
            case 1:
                if (t->trackCount() > 0)
                {
                    const int tr = rng_.below(t->trackCount());
                    const int n = static_cast<int>(t->track(tr).clips.size());
                    if (n > 0) t->removeClip(tr, rng_.below(n));
                }
                break;
            case 2: t->addTrack("new", rng_.chance(50) ? TrackKind::Video : TrackKind::Audio); break;
            case 3: if (t->trackCount() > 0) t->removeTrack(rng_.below(t->trackCount())); break;
            case 4: t->clearTracks(); break;
            case 5: if (t->trackCount() > 0) t->addKeyframe(rng_.below(t->trackCount()), rng_.range(0.0f, 20.0f)); break;
            case 6: t->setTimeRange(rng_.range(0.0f, 5.0f), rng_.range(1.0f, 40.0f)); break;
            default:
                if (t->trackCount() > 0)
                {
                    const int tr = rng_.below(t->trackCount());
                    const int n = static_cast<int>(t->track(tr).clips.size());
                    if (n > 0) t->splitClip(tr, rng_.below(n), rng_.range(0.0f, 20.0f));
                }
                break;
        }
    }

    // ---- after every step ----
    void frameAndCheck()
    {
        budget_ = 4;
        h_.frame();
        check();
        hashFrame();
    }

    static bool finiteRect(const Rect& r)
    {
        return r.x == r.x && r.y == r.y && r.w == r.w && r.h == r.h && r.x - r.x == 0.0f && r.w - r.w == 0.0f;
    }

    void fail(const char* what)
    {
        ++problems_;
        printf("  FAIL %s (frame %d, op %d, mutation %d, stage %s, popup %s)\n", what, h_.frames(), lastOp_, lastWhat_,
                h_.app.currentStageName().c_str(), h_.app.popup() ? h_.app.popup()->typeName() : "none");
    }

    void check()
    {
        ct::Vector<Widget*> all;
        allWidgets(all);
        // consistent tree: parent links, no widget twice, finite rectangles
        for (size_t i = 0; i < all.size(); ++i)
        {
            Widget* w = all[i];
            for (size_t k = 0; k < w->children().size(); ++k)
                if (w->children()[k]->parent() != w) fail("a child's parent is not the widget that holds it");
            if (!finiteRect(w->rect())) fail("a rectangle is not finite");
        }
        // the manager's pointers are at live widgets of the tree
        auto live = [&](Widget* w)
        {
            if (!w) return true;
            for (size_t i = 0; i < all.size(); ++i)
                if (all[i] == w) return true;
            return false;
        };
        if (!live(h_.app.focused())) fail("the focus is at a widget that is not in the tree");
        if (!live(h_.app.popupOwner())) fail("the popup's owner is not in the tree");
        if (!live(h_.app.dragSource())) fail("the drag source is not in the tree");
        // well formed draw data
        const DrawList& dl = GetDrawList();
        for (size_t i = 0; i < dl.indices().size(); ++i)
            if (dl.indices()[i] >= dl.vertices().size())
            {
                fail("an index points past the vertices");
                break;
            }
        for (size_t i = 0; i < dl.commands().size(); ++i)
            if (dl.commands()[i].indexOffset + dl.commands()[i].indexCount > dl.indices().size())
            {
                fail("a command runs past the indices");
                break;
            }
        for (size_t i = 0; i < dl.vertices().size(); ++i)
        {
            const DrawVertex& v = dl.vertices()[i];
            if (!(v.x == v.x && v.y == v.y))
            {
                fail("a vertex is NaN");
                break;
            }
        }
    }

    void hashFrame()
    {
        const DrawList& dl = GetDrawList();
        uint64_t hv = 1469598103934665603ull;
        auto mix = [&hv](const void* p, size_t n)
        {
            const unsigned char* b = static_cast<const unsigned char*>(p);
            for (size_t i = 0; i < n; ++i) hv = (hv ^ b[i]) * 1099511628211ull;
        };
        mix(dl.vertices().data(), dl.vertices().size() * sizeof(DrawVertex));
        mix(dl.indices().data(), dl.indices().size() * sizeof(uint32_t));
        hash_ = hash_ * 31u + hv;
    }

    Rng rng_;
    Harness h_;
    EditorTree editor_;
    Widget* scratch_ = nullptr;
    int budget_ = 4;
    int lastOp_ = -1, lastWhat_ = -1;
    int removed_ = 0;
    int problems_ = 0;
    uint64_t hash_ = 0;
};

// ---- scenarios ----
int gSeeds = 6;
int gSteps = 250;

void testFuzzEditor()
{
    printf("random events and tree mutations on an editor window (%d seeds x %d steps)\n", gSeeds, gSteps);
    for (int seed = 1; seed <= gSeeds; ++seed)
    {
        uint64_t first = 0;
        for (int run = 0; run < 2; ++run)
        {
            Fuzz f(static_cast<uint32_t>(seed) * 2654435761u, 30 + seed * 20);
            for (int i = 0; i < gSteps; ++i)
            {
                f.step();
                if (f.problems() > 3) break;
            }
            check(f.problems() == 0, "the run kept the tree and the draw data sound");
            if (f.problems()) printf("    seed %d, run %d\n", seed, run);
            if (run == 0) first = f.hash();
            else
                check(first == f.hash(), "the same seed gives the same pictures (nothing depends on an address)");
        }
    }
}

} // namespace

// A scenario name, then seeds and steps, on the command line.
int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
    if (argc > 2) gSeeds = atoi(argv[2]);
    if (argc > 3) gSteps = atoi(argv[3]);
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testFuzzEditor);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
