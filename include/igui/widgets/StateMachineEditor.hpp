#pragma once

#include "Signal.hpp"
#include "Widget.hpp"
#include <ct/vector.hpp>
#include <igui/widgets/String.hpp>

namespace ig
{
namespace retained
{

enum class StateKind
{
    Normal,
    Start,
    End
};

enum class TransitionSwitch
{
    Immediate,
    Sync,
    AtEnd
};

struct StateMachineState
{
    String name;
    float x = 0;
    float y = 0;
    StateKind kind = StateKind::Normal;
    bool canOpen = false;
    bool selected = false;
};

struct StateMachineTransition
{
    int from = -1;
    int to = -1;
    TransitionSwitch mode = TransitionSwitch::Immediate;
    bool disabled = false;
    bool autoAdvance = false;
    String condition;
    bool conditionOn = false;
};

struct StateMachinePlayback
{
    bool playing = false;
    int current = -1;
    int fadingFrom = -1;
    ct::Vector<int> travelPath;
    float currentPosition = 0.0f;
    float fadingFromPosition = 0.0f;
    float fadeRatio = 0.0f;
};

struct StateMove
{
    int state = -1;
    float x = 0;
    float y = 0;
};

// ═════════════════════════════════════════════════════════════════════════════
//  StateMachineEditor — states as boxes, transitions as arrows between them
//
//  The widget shows a model the application gives it (addState, addTransition)
//  and never changes that model: every edit is a signal, the application edits
//  its own data and builds the model again (clearAll keeps the selection and
//  the view). Start and End states are drawn apart; two transitions between
//  the same pair of states, one each way, run side by side.
//
//  Select tool:  left drag moves, left drag on the background selects a box,
//                Shift+drag from a state connects, right click asks for a menu
//  Create tool:  left click asks for a new state where it was clicked
//  Connect tool: drag from a state to another
//  Erase tool:   click on a state or a transition asks for it to be deleted
//  Middle drag pans, the wheel zooms.
// ═════════════════════════════════════════════════════════════════════════════

class StateMachineEditor : public Widget
{
public:
    IG_WIDGET_CLASS(StateMachineEditor, Widget);

    enum class Tool
    {
        Select,
        Create,
        Connect,
        Erase
    };

    StateMachineEditor();

    void clearAll();
    void reset();
    int addState(const String& name, float x, float y, StateKind kind = StateKind::Normal,
            bool canOpen = false);
    int addTransition(int from, int to, TransitionSwitch mode = TransitionSwitch::Immediate,
            bool disabled = false, bool autoAdvance = false, const String& condition = String(),
            bool conditionOn = false);

    int stateCount() const
    {
        return static_cast<int>(states_.size());
    }
    const StateMachineState& state(int index) const
    {
        return states_[static_cast<size_t>(index)];
    }
    int transitionCount() const
    {
        return static_cast<int>(transitions_.size());
    }
    const StateMachineTransition& transition(int index) const
    {
        return transitions_[static_cast<size_t>(index)];
    }
    int findState(const String& name) const;
    int findTransition(int from, int to) const;

    void selectState(int index, bool additive = false);
    void selectTransition(int index);
    void clearSelection();
    void selectedStates(ct::Vector<int>& out) const;
    int selectedTransition() const;

    void setTool(Tool tool);
    Tool tool() const
    {
        return tool_;
    }

    void setView(float panX, float panY, float zoom);
    void fitView();
    float panX() const
    {
        return panX_;
    }
    float panY() const
    {
        return panY_;
    }
    float zoom() const
    {
        return zoom_;
    }

    void setPlayback(const StateMachinePlayback& playback);
    void clearPlayback();
    const StateMachinePlayback& playback() const
    {
        return playback_;
    }

    Rect stateRect(int index) const;
    Rect nameRect(int index) const;
    Rect playRect(int index) const;
    Rect openRect(int index) const;
    bool transitionAnchor(int index, float& x, float& y) const;

    Signal<> onSelectionChanged;
    Signal<const ct::Vector<StateMove>&> onStatesMoved;
    Signal<float, float, float, float> onCreateRequested;
    Signal<int, int> onTransitionRequested;
    Signal<int, float, float, float, float> onConnectDropped;
    Signal<int, bool, int> onTransitionReconnected;
    Signal<const ct::Vector<int>&> onStatesDeleteRequested;
    Signal<int> onTransitionDeleteRequested;
    Signal<int> onStateOpenRequested;
    Signal<int> onStateRenameRequested;
    Signal<int> onStatePlayRequested;
    Signal<int, float, float> onStateContext;
    Signal<int, float, float> onTransitionContext;

    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onMouseLeave() override;
    void onKeyPress(KeyEvent& e) override;

private:
    enum class Drag
    {
        None,
        Pan,
        Press,
        Move,
        Box,
        Connect,
        Reconnect
    };

    enum class Area
    {
        None,
        Play,
        Name,
        Open,
        Body
    };

    struct NodeRect
    {
        Rect box;
        Rect play;
        Rect name;
        Rect open;
    };

    struct Line
    {
        int index = -1;
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        float width = 0;
        bool hidden = false;
    };

    struct Hit
    {
        int state = -1;
        Area area = Area::None;
    };

    ct::Vector<StateMachineState> states_;
    ct::Vector<StateMachineTransition> transitions_;
    ct::Vector<String> keptSelection_;
    String selFrom_;
    String selTo_;
    StateMachinePlayback playback_;

    ct::Vector<NodeRect> rects_;
    ct::Vector<Line> lines_;
    bool rectsValid_ = false;

    Tool tool_ = Tool::Select;
    float panX_ = 0;
    float panY_ = 0;
    float zoom_ = 1.0f;
    bool fitPending_ = false;
    bool autoFit_ = true;
    float fitW_ = 0;
    float fitH_ = 0;

    Drag drag_ = Drag::None;
    float pressX_ = 0, pressY_ = 0;
    float panStartX_ = 0, panStartY_ = 0;
    float dragX_ = 0, dragY_ = 0;
    int pressState_ = -1;
    Area pressArea_ = Area::None;
    bool pressShift_ = false;
    int snapX_ = -1;
    int snapY_ = -1;
    float boxX_ = 0, boxY_ = 0, boxToX_ = 0, boxToY_ = 0;
    ct::Vector<String> boxBase_;
    int connectFrom_ = -1;
    int connectTo_ = -1;
    float connectX_ = 0, connectY_ = 0;
    int reconnectIndex_ = -1;
    bool reconnectStart_ = false;
    int reconnectTarget_ = -1;
    float reconnectX_ = 0, reconnectY_ = 0;

    Hit hover_;
    int hoverTransition_ = -1;
    bool hoverTransitionStart_ = false;

    float gx(float x) const;
    float gy(float y) const;
    float sx(float x) const;
    float sy(float y) const;

    Hit hitState(float x, float y) const;
    int hitTransition(float x, float y) const;
    void updateHover(float x, float y);
    void startFit();
    bool isSelected(int index) const;
    void clearSelectionFlags();
    bool selectionEmpty() const;
    void selectOnly(int index);
    void toggleState(int index);
    int stateAt(float x, float y, int except = -1) const;
    bool canConnectFrom(int index) const;
    float nodeOpacity(int index) const;

    void layoutNodes(PaintContext& ctx);
    void layoutLines();
    void paintFrame(PaintContext& ctx, int index, float opacity);
    void paintTransition(PaintContext& ctx, float x0, float y0, float x1, float y1,
            const StateMachineTransition* t, bool enabled, bool selected, bool travel,
            float fadeRatio, bool autoAdvance, float opacity, bool endpointHovered,
            bool endpointHoveredStart);
    void paintIcon(PaintContext& ctx, float x0, float y0, float x1, float y1, TransitionSwitch mode,
            bool autoAdvance, float opacity, bool enabled);
    void paintProgress(PaintContext& ctx, int state, float ratio);
    void paintGuides(PaintContext& ctx);
};

} // namespace retained
} // namespace ig
