#include "StateMachineEditor.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <cmath>

namespace
{

constexpr float kMarginX = 10.0f;
constexpr float kMarginY = 26.0f;
constexpr float kSep = 3.0f;
constexpr float kPlaySize = 16.0f;
constexpr float kOpenSize = 16.0f;
constexpr float kIconSize = 20.0f;
constexpr float kBidiOffset = 16.0f;
constexpr float kSnapDistance = 5.0f;
constexpr float kDragThreshold = 3.0f;
constexpr float kMinZoom = 0.4f;
constexpr float kMaxZoom = 2.5f;
constexpr float kFitPadding = 90.0f;

using ig::Color;
using ig::Rect;
using ig::Vec2;

Color scaled(const Color& c, float k)
{
    const float a = std::min(255.0f, std::max(0.0f, c.a * k));
    return Color(c.r, c.g, c.b, static_cast<uint8_t>(a));
}

Color lightened(const Color& c, float k)
{
    return Color(static_cast<uint8_t>(c.r + (255 - c.r) * k),
            static_cast<uint8_t>(c.g + (255 - c.g) * k),
            static_cast<uint8_t>(c.b + (255 - c.b) * k), c.a);
}

Color redHue(const Color& c)
{
    const float v = static_cast<float>(std::max(c.r, std::max(c.g, c.b))) / 255.0f;
    const float mn = static_cast<float>(std::min(c.r, std::min(c.g, c.b))) / 255.0f;
    const float s = v > 0.0f ? (v - mn) / v : 0.0f;
    return Color(static_cast<uint8_t>(v * 255.0f), static_cast<uint8_t>(v * (1.0f - s) * 255.0f),
            static_cast<uint8_t>(v * (1.0f - s) * 255.0f), c.a);
}

void normalize(float& x, float& y)
{
    const float l = std::sqrt(x * x + y * y);
    if (l > 1e-6f)
    {
        x /= l;
        y /= l;
    }
    else
    {
        x = 1.0f;
        y = 0.0f;
    }
}

float distanceToSegment(float px, float py, float x0, float y0, float x1, float y1)
{
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float l2 = dx * dx + dy * dy;
    float t = l2 > 1e-9f ? ((px - x0) * dx + (py - y0) * dy) / l2 : 0.0f;
    t = std::min(1.0f, std::max(0.0f, t));
    const float cx = x0 + dx * t;
    const float cy = y0 + dy * t;
    return std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
}

float rayExit(const Rect& r, float x, float y, float dx, float dy)
{
    float t = 1e20f;
    if (dx > 1e-6f) t = std::min(t, (r.right() - x) / dx);
    else if (dx < -1e-6f)
        t = std::min(t, (r.x - x) / dx);
    if (dy > 1e-6f) t = std::min(t, (r.bottom() - y) / dy);
    else if (dy < -1e-6f)
        t = std::min(t, (r.y - y) / dy);
    return t;
}

void clipSource(float& fx, float& fy, float tx, float ty, const Rect& r)
{
    if (tx == fx && ty == fy) return;
    if (!r.contains(fx, fy)) return;
    float nx = tx - fx;
    float ny = ty - fy;
    normalize(nx, ny);
    const float t = rayExit(r, fx, fy, nx, ny);
    if (t > 1e19f) return;
    fx += nx * (t + 0.5f);
    fy += ny * (t + 0.5f);
}

void clipDestination(float fx, float fy, float& tx, float& ty, const Rect& r)
{
    if (tx == fx && ty == fy) return;
    if (!r.contains(tx, ty)) return;
    float nx = tx - fx;
    float ny = ty - fy;
    normalize(nx, ny);
    const float t = rayExit(r, tx, ty, -nx, -ny);
    if (t > 1e19f) return;
    tx -= nx * (t + 0.5f);
    ty -= ny * (t + 0.5f);
}

bool rectsIntersect(const Rect& a, const Rect& b)
{
    return a.x < b.right() && a.right() > b.x && a.y < b.bottom() && a.bottom() > b.y;
}

} // namespace

StateMachineEditor::StateMachineEditor()
{
    acceptsFocus_ = true;
}

float StateMachineEditor::gx(float x) const
{
    return absoluteRect().x + (x - panX_) * zoom_;
}

float StateMachineEditor::gy(float y) const
{
    return absoluteRect().y + (y - panY_) * zoom_;
}

float StateMachineEditor::sx(float x) const
{
    return (x - absoluteRect().x) / zoom_ + panX_;
}

float StateMachineEditor::sy(float y) const
{
    return (y - absoluteRect().y) / zoom_ + panY_;
}

void StateMachineEditor::clearAll()
{
    if (!states_.empty())
    {
        keptSelection_.clear();
        for (const auto& s: states_)
            if (s.selected) keptSelection_.push_back(s.name);
    }
    states_.clear();
    transitions_.clear();
    rects_.clear();
    lines_.clear();
    rectsValid_ = false;
    hover_ = Hit();
    hoverTransition_ = -1;
    markDirty();
}

void StateMachineEditor::reset()
{
    states_.clear();
    transitions_.clear();
    rects_.clear();
    lines_.clear();
    keptSelection_.clear();
    selFrom_ = String();
    selTo_ = String();
    rectsValid_ = false;
    drag_ = Drag::None;
    pressState_ = -1;
    connectFrom_ = -1;
    connectTo_ = -1;
    reconnectIndex_ = -1;
    reconnectTarget_ = -1;
    hover_ = Hit();
    hoverTransition_ = -1;
    snapX_ = -1;
    snapY_ = -1;
    dragX_ = 0;
    dragY_ = 0;
    playback_ = StateMachinePlayback();
    fitPending_ = true;
    markDirty();
}

int StateMachineEditor::addState(const String& name, float x, float y, StateKind kind, bool canOpen)
{
    StateMachineState s;
    s.name = name;
    s.x = x;
    s.y = y;
    s.kind = kind;
    s.canOpen = canOpen;
    for (const auto& kept: keptSelection_)
        if (kept == name) s.selected = true;
    states_.push_back(s);
    rectsValid_ = false;
    markDirty();
    return static_cast<int>(states_.size()) - 1;
}

int StateMachineEditor::addTransition(int from, int to, TransitionSwitch mode, bool disabled,
        bool autoAdvance, const String& condition, bool conditionOn)
{
    if (from < 0 || to < 0 || from >= stateCount() || to >= stateCount()) return -1;
    StateMachineTransition t;
    t.from = from;
    t.to = to;
    t.mode = mode;
    t.disabled = disabled;
    t.autoAdvance = autoAdvance;
    t.condition = condition;
    t.conditionOn = conditionOn;
    transitions_.push_back(t);
    rectsValid_ = false;
    markDirty();
    return static_cast<int>(transitions_.size()) - 1;
}

int StateMachineEditor::findState(const String& name) const
{
    for (size_t i = 0; i < states_.size(); ++i)
        if (states_[i].name == name) return static_cast<int>(i);
    return -1;
}

int StateMachineEditor::findTransition(int from, int to) const
{
    for (size_t i = 0; i < transitions_.size(); ++i)
        if (transitions_[i].from == from && transitions_[i].to == to) return static_cast<int>(i);
    return -1;
}

bool StateMachineEditor::isSelected(int index) const
{
    return index >= 0 && index < stateCount() && states_[static_cast<size_t>(index)].selected;
}

void StateMachineEditor::clearSelectionFlags()
{
    for (auto& s: states_) s.selected = false;
    selFrom_ = String();
    selTo_ = String();
}

bool StateMachineEditor::selectionEmpty() const
{
    if (!selFrom_.empty()) return false;
    for (const auto& s: states_)
        if (s.selected) return false;
    return true;
}

void StateMachineEditor::selectOnly(int index)
{
    clearSelectionFlags();
    if (index >= 0 && index < stateCount()) states_[static_cast<size_t>(index)].selected = true;
}

void StateMachineEditor::toggleState(int index)
{
    if (index < 0 || index >= stateCount()) return;
    selFrom_ = String();
    selTo_ = String();
    states_[static_cast<size_t>(index)].selected = !states_[static_cast<size_t>(index)].selected;
}

void StateMachineEditor::selectState(int index, bool additive)
{
    if (additive)
    {
        selFrom_ = String();
        selTo_ = String();
        if (index >= 0 && index < stateCount()) states_[static_cast<size_t>(index)].selected = true;
    }
    else
        selectOnly(index);
    markDirty();
}

void StateMachineEditor::selectTransition(int index)
{
    clearSelectionFlags();
    if (index >= 0 && index < transitionCount())
    {
        const auto& t = transitions_[static_cast<size_t>(index)];
        selFrom_ = states_[static_cast<size_t>(t.from)].name;
        selTo_ = states_[static_cast<size_t>(t.to)].name;
    }
    markDirty();
}

void StateMachineEditor::clearSelection()
{
    clearSelectionFlags();
    markDirty();
}

void StateMachineEditor::selectedStates(ct::Vector<int>& out) const
{
    for (size_t i = 0; i < states_.size(); ++i)
        if (states_[i].selected) out.push_back(static_cast<int>(i));
}

int StateMachineEditor::selectedTransition() const
{
    if (selFrom_.empty()) return -1;
    const int a = findState(selFrom_);
    const int b = findState(selTo_);
    if (a < 0 || b < 0) return -1;
    return findTransition(a, b);
}

void StateMachineEditor::setTool(Tool tool)
{
    if (tool_ == tool) return;
    tool_ = tool;
    drag_ = Drag::None;
    connectFrom_ = -1;
    reconnectIndex_ = -1;
    hoverTransition_ = -1;
    markDirty();
}

void StateMachineEditor::setView(float panX, float panY, float zoom)
{
    panX_ = panX;
    panY_ = panY;
    zoom_ = std::clamp(zoom, kMinZoom, kMaxZoom);
    fitPending_ = false;
    autoFit_ = false;
    markDirty();
}

void StateMachineEditor::fitView()
{
    fitPending_ = true;
    markDirty();
}

void StateMachineEditor::startFit()
{
    const Rect b = absoluteRect();
    float zoom = 1.0f;
    float cx = 0.0f;
    float cy = 0.0f;
    if (!states_.empty())
    {
        float minX = states_[0].x, maxX = states_[0].x, minY = states_[0].y, maxY = states_[0].y;
        for (const auto& s: states_)
        {
            minX = std::min(minX, s.x);
            maxX = std::max(maxX, s.x);
            minY = std::min(minY, s.y);
            maxY = std::max(maxY, s.y);
        }
        cx = (minX + maxX) * 0.5f;
        cy = (minY + maxY) * 0.5f;
        const float w = (maxX - minX) + 2.0f * kFitPadding;
        const float h = (maxY - minY) + 2.0f * kFitPadding;
        zoom = std::min(1.0f, std::min(b.w / w, b.h / h));
    }
    zoom_ = std::clamp(zoom, kMinZoom, kMaxZoom);
    panX_ = cx - b.w * 0.5f / zoom_;
    panY_ = cy - b.h * 0.5f / zoom_;
    fitPending_ = false;
    autoFit_ = true;
    fitW_ = b.w;
    fitH_ = b.h;
}

void StateMachineEditor::setPlayback(const StateMachinePlayback& playback)
{
    playback_ = playback;
    markDirty();
}

void StateMachineEditor::clearPlayback()
{
    if (!playback_.playing && playback_.current < 0 && playback_.fadingFrom < 0) return;
    playback_ = StateMachinePlayback();
    markDirty();
}

Rect StateMachineEditor::stateRect(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= rects_.size()) return Rect();
    return rects_[static_cast<size_t>(index)].box;
}

Rect StateMachineEditor::nameRect(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= rects_.size()) return Rect();
    return rects_[static_cast<size_t>(index)].name;
}

Rect StateMachineEditor::playRect(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= rects_.size()) return Rect();
    return rects_[static_cast<size_t>(index)].play;
}

Rect StateMachineEditor::openRect(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= rects_.size()) return Rect();
    return rects_[static_cast<size_t>(index)].open;
}

bool StateMachineEditor::transitionAnchor(int index, float& x, float& y) const
{
    for (const auto& l: lines_)
    {
        if (l.index != index) continue;
        x = (l.x0 + l.x1) * 0.5f;
        y = (l.y0 + l.y1) * 0.5f;
        return true;
    }
    return false;
}

int StateMachineEditor::stateAt(float x, float y, int except) const
{
    if (rects_.size() != states_.size()) return -1;
    for (int i = static_cast<int>(rects_.size()) - 1; i >= 0; --i)
    {
        if (i == except) continue;
        if (rects_[static_cast<size_t>(i)].box.contains(x, y)) return i;
    }
    return -1;
}

StateMachineEditor::Hit StateMachineEditor::hitState(float x, float y) const
{
    Hit h;
    h.state = stateAt(x, y);
    if (h.state < 0) return h;
    const NodeRect& r = rects_[static_cast<size_t>(h.state)];
    const StateMachineState& s = states_[static_cast<size_t>(h.state)];
    if (r.play.contains(x, y)) h.area = Area::Play;
    else if (s.canOpen && r.open.contains(x, y))
        h.area = Area::Open;
    else if (s.kind == StateKind::Normal && r.name.contains(x, y))
        h.area = Area::Name;
    else
        h.area = Area::Body;
    return h;
}

int StateMachineEditor::hitTransition(float x, float y) const
{
    int closest = -1;
    float closestD = 1e20f;
    ct::Vector<int> candidates;
    for (size_t i = 0; i < lines_.size(); ++i)
    {
        const Line& l = lines_[i];
        if (l.hidden) continue;
        const float d = distanceToSegment(x, y, l.x0, l.y0, l.x1, l.y1);
        if (d > l.width) continue;
        if (std::fabs(d - closestD) < 2.0f) candidates.push_back(static_cast<int>(i));
        else if (d < closestD)
        {
            closestD = d;
            closest = static_cast<int>(i);
            candidates.clear();
            candidates.push_back(closest);
        }
    }
    if (candidates.size() > 1)
    {
        float best = 1e20f;
        for (int c: candidates)
        {
            const Line& l = lines_[static_cast<size_t>(c)];
            const float mx = (l.x0 + l.x1) * 0.5f - x;
            const float my = (l.y0 + l.y1) * 0.5f - y;
            const float d = std::sqrt(mx * mx + my * my);
            if (d < best)
            {
                best = d;
                closest = c;
            }
        }
    }
    return closest < 0 ? -1 : lines_[static_cast<size_t>(closest)].index;
}

void StateMachineEditor::updateHover(float x, float y)
{
    const bool selecting = tool_ == Tool::Select;
    Hit h = hitState(x, y);
    if (!selecting && h.area != Area::None) h.area = Area::Body;
    if (h.state >= 0 && states_[static_cast<size_t>(h.state)].kind != StateKind::Normal &&
            h.area == Area::Name)
        h.area = Area::Body;
    if (h.state != hover_.state || h.area != hover_.area)
    {
        hover_ = h;
        markDirty();
    }

    int endpoint = -1;
    bool endpointStart = false;
    int tooltipIndex = -1;
    if (h.state < 0 && selecting)
    {
        float closestHighlight = 1e20f;
        float closestTooltip = 1e20f;
        for (const Line& l: lines_)
        {
            if (l.hidden) continue;
            const float d = distanceToSegment(x, y, l.x0, l.y0, l.x1, l.y1);
            if (d > l.width) continue;
            if (d < closestTooltip)
            {
                closestTooltip = d;
                tooltipIndex = l.index;
            }
            const float len =
                    std::sqrt((l.x1 - l.x0) * (l.x1 - l.x0) + (l.y1 - l.y0) * (l.y1 - l.y0));
            const float reach = std::min(20.0f * zoom_, len * 0.2f);
            const float ds = std::sqrt((x - l.x0) * (x - l.x0) + (y - l.y0) * (y - l.y0));
            const float de = std::sqrt((x - l.x1) * (x - l.x1) + (y - l.y1) * (y - l.y1));
            const bool nearStart = ds <= reach;
            const bool nearEnd = de <= reach;
            if ((nearStart || nearEnd) && d < closestHighlight)
            {
                closestHighlight = d;
                endpoint = l.index;
                endpointStart = nearStart && (ds < de || !nearEnd);
            }
        }
    }
    if (endpoint != hoverTransition_ || endpointStart != hoverTransitionStart_)
    {
        hoverTransition_ = endpoint;
        hoverTransitionStart_ = endpointStart;
        markDirty();
    }

    String tip;
    CursorType cursor = CursorType::Arrow;
    if (h.state >= 0)
    {
        const String& name = states_[static_cast<size_t>(h.state)].name;
        if (h.area == Area::Play) tip = String("Play ") + name;
        else if (h.area == Area::Open)
            tip = String("Open ") + name;
        else
            tip = name;
        if (h.area == Area::Name) cursor = CursorType::IBeam;
    }
    else if (tooltipIndex >= 0)
    {
        const auto& t = transitions_[static_cast<size_t>(tooltipIndex)];
        tip = states_[static_cast<size_t>(t.from)].name + String(" -> ") +
              states_[static_cast<size_t>(t.to)].name;
        if (!t.condition.empty()) tip += String("  [") + t.condition + String("]");
    }
    setTooltip(tip);
    setCursor(cursor);
}

bool StateMachineEditor::canConnectFrom(int index) const
{
    return index >= 0 && index < stateCount() &&
           states_[static_cast<size_t>(index)].kind != StateKind::End;
}

void StateMachineEditor::onMouseLeave()
{
    hover_ = Hit();
    hoverTransition_ = -1;
    setTooltip(String());
    markDirty();
}

void StateMachineEditor::onMouseScroll(MouseEvent& e)
{
    const float old = zoom_;
    const float factor = e.scrollY > 0 ? 1.1f : 0.9f;
    zoom_ = std::clamp(zoom_ * factor, kMinZoom, kMaxZoom);
    const Rect b = absoluteRect();
    const float mx = (e.x - b.x) / old + panX_;
    const float my = (e.y - b.y) / old + panY_;
    panX_ = mx - (e.x - b.x) / zoom_;
    panY_ = my - (e.y - b.y) / zoom_;
    autoFit_ = false;
    markDirty();
    e.consumed = true;
}

void StateMachineEditor::onMousePress(MouseEvent& e)
{
    const float x = e.x;
    const float y = e.y;
    pressShift_ = e.shift;
    if (e.button == 2)
    {
        drag_ = Drag::Pan;
        pressX_ = x;
        pressY_ = y;
        panStartX_ = panX_;
        panStartY_ = panY_;
        e.consumed = true;
        return;
    }

    if (e.button == 1)
    {
        e.consumed = true;
        const Hit h = hitState(x, y);
        if (h.state >= 0)
        {
            const bool changed = !isSelected(h.state) || !selFrom_.empty();
            if (!isSelected(h.state)) selectOnly(h.state);
            markDirty();
            if (changed) onSelectionChanged.emit();
            onStateContext.emit(h.state, x, y);
            return;
        }
        const int t = hitTransition(x, y);
        if (t >= 0)
        {
            selectTransition(t);
            onSelectionChanged.emit();
            onTransitionContext.emit(t, x, y);
            return;
        }
        onCreateRequested.emit(x, y, sx(x), sy(y));
        return;
    }

    if (e.button != 0) return;
    e.consumed = true;
    const Hit h = hitState(x, y);

    if (tool_ == Tool::Create)
    {
        onCreateRequested.emit(x, y, sx(x), sy(y));
        return;
    }

    if (tool_ == Tool::Erase)
    {
        if (h.state >= 0)
        {
            if (states_[static_cast<size_t>(h.state)].kind != StateKind::Normal) return;
            ct::Vector<int> list;
            list.push_back(h.state);
            onStatesDeleteRequested.emit(list);
            return;
        }
        const int t = hitTransition(x, y);
        if (t >= 0) onTransitionDeleteRequested.emit(t);
        return;
    }

    if (tool_ == Tool::Connect || (tool_ == Tool::Select && e.shift))
    {
        if (h.state >= 0 && canConnectFrom(h.state))
        {
            drag_ = Drag::Connect;
            connectFrom_ = h.state;
            connectTo_ = -1;
            connectX_ = x;
            connectY_ = y;
            markDirty();
        }
        return;
    }

    if (hoverTransition_ >= 0 && h.state < 0)
    {
        drag_ = Drag::Reconnect;
        reconnectIndex_ = hoverTransition_;
        reconnectStart_ = hoverTransitionStart_;
        reconnectTarget_ = -1;
        reconnectX_ = x;
        reconnectY_ = y;
        clearSelectionFlags();
        markDirty();
        onSelectionChanged.emit();
        return;
    }

    if (h.state >= 0)
    {
        if (h.area == Area::Play)
        {
            onStatePlayRequested.emit(h.state);
            return;
        }
        if (h.area == Area::Open ||
                (e.clickCount >= 2 && states_[static_cast<size_t>(h.state)].canOpen))
        {
            onStateOpenRequested.emit(h.state);
            return;
        }
        bool changed = false;
        if (e.ctrl || e.shift)
        {
            toggleState(h.state);
            changed = true;
        }
        else if (!isSelected(h.state) || !selFrom_.empty())
        {
            selectOnly(h.state);
            changed = true;
        }
        drag_ = Drag::Press;
        pressX_ = x;
        pressY_ = y;
        pressState_ = h.state;
        pressArea_ = h.area;
        dragX_ = 0;
        dragY_ = 0;
        snapX_ = -1;
        snapY_ = -1;
        markDirty();
        if (changed) onSelectionChanged.emit();
        return;
    }

    const int t = hitTransition(x, y);
    if (t >= 0)
    {
        selectTransition(t);
        onSelectionChanged.emit();
        return;
    }

    boxBase_.clear();
    if (e.ctrl || e.shift)
    {
        for (const auto& s: states_)
            if (s.selected) boxBase_.push_back(s.name);
    }
    else
        clearSelectionFlags();
    drag_ = Drag::Box;
    boxX_ = boxToX_ = x;
    boxY_ = boxToY_ = y;
    markDirty();
    onSelectionChanged.emit();
}

void StateMachineEditor::onMouseMove(MouseEvent& e)
{
    const float x = e.x;
    const float y = e.y;
    switch (drag_)
    {
        case Drag::Pan:
            panX_ = panStartX_ - (x - pressX_) / zoom_;
            panY_ = panStartY_ - (y - pressY_) / zoom_;
            autoFit_ = false;
            markDirty();
            e.consumed = true;
            return;
        case Drag::Press:
        {
            const float dx = x - pressX_;
            const float dy = y - pressY_;
            if (dx * dx + dy * dy < kDragThreshold * kDragThreshold) return;
            drag_ = Drag::Move;
        }
            [[fallthrough]];
        case Drag::Move:
        {
            float ox = (x - pressX_) / zoom_;
            float oy = (y - pressY_) / zoom_;
            snapX_ = -1;
            snapY_ = -1;
            if (pressState_ >= 0 && pressState_ < stateCount())
            {
                const StateMachineState& primary = states_[static_cast<size_t>(pressState_)];
                const float cx = primary.x + ox;
                const float cy = primary.y + oy;
                float bestX = 1e20f;
                float bestY = 1e20f;
                for (int i = 0; i < stateCount(); ++i)
                {
                    if (i == pressState_ || states_[static_cast<size_t>(i)].selected) continue;
                    const StateMachineState& other = states_[static_cast<size_t>(i)];
                    const float ddx = std::fabs(other.x - cx);
                    if (ddx < std::min(kSnapDistance, bestX))
                    {
                        ox -= cx - other.x;
                        bestX = ddx;
                        snapX_ = i;
                    }
                    const float ddy = std::fabs(other.y - cy);
                    if (ddy < std::min(kSnapDistance, bestY))
                    {
                        oy -= cy - other.y;
                        bestY = ddy;
                        snapY_ = i;
                    }
                }
            }
            dragX_ = ox;
            dragY_ = oy;
            markDirty();
            e.consumed = true;
            return;
        }
        case Drag::Box:
        {
            boxToX_ = x;
            boxToY_ = y;
            const Rect box(std::min(boxX_, boxToX_), std::min(boxY_, boxToY_),
                    std::fabs(boxX_ - boxToX_), std::fabs(boxY_ - boxToY_));
            for (size_t i = 0; i < states_.size() && i < rects_.size(); ++i)
            {
                bool base = false;
                for (const auto& n: boxBase_)
                    if (n == states_[i].name) base = true;
                const bool inside = rectsIntersect(rects_[i].box, box);
                states_[i].selected = inside ? !base : base;
            }
            markDirty();
            e.consumed = true;
            return;
        }
        case Drag::Connect:
            connectX_ = x;
            connectY_ = y;
            connectTo_ = stateAt(x, y, connectFrom_);
            markDirty();
            e.consumed = true;
            return;
        case Drag::Reconnect:
        {
            reconnectX_ = x;
            reconnectY_ = y;
            reconnectTarget_ = stateAt(x, y);
            markDirty();
            e.consumed = true;
            return;
        }
        case Drag::None:
            break;
    }
    updateHover(x, y);
}

void StateMachineEditor::onMouseRelease(MouseEvent& e)
{
    const Drag mode = drag_;
    drag_ = Drag::None;
    e.consumed = true;
    switch (mode)
    {
        case Drag::Press:
        {
            const int state = pressState_;
            const Area area = pressArea_;
            pressState_ = -1;
            bool changed = false;
            if (!pressShift_ && !e.ctrl && isSelected(state))
            {
                int count = 0;
                for (const auto& s: states_)
                    if (s.selected) ++count;
                if (count > 1)
                {
                    selectOnly(state);
                    changed = true;
                }
            }
            markDirty();
            if (changed) onSelectionChanged.emit();
            if (area == Area::Name && e.button == 0) onStateRenameRequested.emit(state);
            return;
        }
        case Drag::Move:
        {
            ct::Vector<StateMove> moves;
            for (size_t i = 0; i < states_.size(); ++i)
            {
                if (!states_[i].selected) continue;
                StateMove m;
                m.state = static_cast<int>(i);
                m.x = states_[i].x + dragX_;
                m.y = states_[i].y + dragY_;
                moves.push_back(m);
            }
            pressState_ = -1;
            snapX_ = -1;
            snapY_ = -1;
            dragX_ = 0;
            dragY_ = 0;
            markDirty();
            if (!moves.empty()) onStatesMoved.emit(moves);
            return;
        }
        case Drag::Box:
            markDirty();
            onSelectionChanged.emit();
            return;
        case Drag::Connect:
        {
            const int from = connectFrom_;
            const int to = connectTo_;
            connectFrom_ = -1;
            connectTo_ = -1;
            markDirty();
            if (to >= 0) onTransitionRequested.emit(from, to);
            else if (stateAt(e.x, e.y) < 0)
                onConnectDropped.emit(from, e.x, e.y, sx(e.x), sy(e.y));
            return;
        }
        case Drag::Reconnect:
        {
            const int index = reconnectIndex_;
            const bool start = reconnectStart_;
            const int target = reconnectTarget_;
            reconnectIndex_ = -1;
            reconnectTarget_ = -1;
            markDirty();
            if (target >= 0) onTransitionReconnected.emit(index, start, target);
            return;
        }
        case Drag::Pan:
        case Drag::None:
            return;
    }
}

void StateMachineEditor::onKeyPress(KeyEvent& e)
{
    if (e.key == Key::Delete && tool_ == Tool::Select)
    {
        ct::Vector<int> list;
        for (size_t i = 0; i < states_.size(); ++i)
            if (states_[i].selected && states_[i].kind == StateKind::Normal)
                list.push_back(static_cast<int>(i));
        const int t = selectedTransition();
        e.consumed = true;
        if (!list.empty()) onStatesDeleteRequested.emit(list);
        else if (t >= 0)
            onTransitionDeleteRequested.emit(t);
        return;
    }
    if (e.key == Key::Escape && drag_ != Drag::None)
    {
        drag_ = Drag::None;
        pressState_ = -1;
        connectFrom_ = -1;
        reconnectIndex_ = -1;
        dragX_ = 0;
        dragY_ = 0;
        snapX_ = -1;
        snapY_ = -1;
        markDirty();
        e.consumed = true;
    }
}

float StateMachineEditor::nodeOpacity(int index) const
{
    const int t = selectedTransition();
    if (t >= 0)
    {
        const auto& tr = transitions_[static_cast<size_t>(t)];
        return (index == tr.from || index == tr.to) ? 1.0f : 0.2f;
    }
    int only = -1;
    int count = 0;
    for (size_t i = 0; i < states_.size(); ++i)
    {
        if (!states_[i].selected) continue;
        only = static_cast<int>(i);
        ++count;
    }
    if (count != 1) return 1.0f;
    if (index == only) return 1.0f;
    for (const auto& tr: transitions_)
        if ((tr.from == only && tr.to == index) || (tr.to == only && tr.from == index)) return 1.0f;
    return 0.2f;
}

void StateMachineEditor::layoutNodes(PaintContext& ctx)
{
    const float z = zoom_;
    const float px = std::max(6.0f, Theme::instance().fontSize * z);
    ctx.font.SetFontSize(px);
    const float th = ctx.font.GetFontSize();
    rects_.resize(states_.size());
    for (size_t i = 0; i < states_.size(); ++i)
    {
        const StateMachineState& s = states_[i];
        const float nameW = ctx.font.GetTextWidth(s.name.c_str());
        float w = 2.0f * kMarginX * z + nameW + (kSep + kPlaySize) * z;
        if (s.canOpen) w += (kSep + kOpenSize) * z;
        const float h = kMarginY * z + std::max(th, kPlaySize * z);
        float cx = gx(s.x);
        float cy = gy(s.y);
        if (s.selected && drag_ == Drag::Move)
        {
            cx += dragX_ * z;
            cy += dragY_ * z;
        }
        NodeRect& r = rects_[i];
        r.box = Rect(std::floor(cx - w * 0.5f), std::floor(cy - h * 0.5f), w, h);
        float x = r.box.x + kMarginX * z;
        r.play = Rect(x, r.box.y + std::floor((h - kPlaySize * z) * 0.5f), kPlaySize * z,
                kPlaySize * z);
        x += (kSep + kPlaySize) * z;
        r.name = Rect(x, r.box.y + std::floor((h - th) * 0.5f), nameW, th);
        x += nameW + kSep * z;
        if (s.canOpen)
            r.open = Rect(x, r.box.y + std::floor((h - kOpenSize * z) * 0.5f), kOpenSize * z,
                    kOpenSize * z);
        else
            r.open = Rect();
    }
    rectsValid_ = true;
}

void StateMachineEditor::layoutLines()
{
    lines_.clear();
    const float z = zoom_;
    for (size_t i = 0; i < transitions_.size(); ++i)
    {
        const StateMachineTransition& t = transitions_[i];
        if (static_cast<size_t>(t.from) >= rects_.size() ||
                static_cast<size_t>(t.to) >= rects_.size())
            continue;
        const StateMachineState& a = states_[static_cast<size_t>(t.from)];
        const StateMachineState& b = states_[static_cast<size_t>(t.to)];
        float ofsFromX = 0, ofsFromY = 0, ofsToX = 0, ofsToY = 0;
        if (drag_ == Drag::Move)
        {
            if (a.selected)
            {
                ofsFromX = dragX_ * z;
                ofsFromY = dragY_ * z;
            }
            if (b.selected)
            {
                ofsToX = dragX_ * z;
                ofsToY = dragY_ * z;
            }
        }
        Line l;
        l.index = static_cast<int>(i);
        l.x0 = gx(a.x) + ofsFromX;
        l.y0 = gy(a.y) + ofsFromY;
        l.x1 = gx(b.x) + ofsToX;
        l.y1 = gy(b.y) + ofsToY;
        l.width = kBidiOffset * z;
        if (findTransition(t.to, t.from) >= 0)
        {
            float nx = l.x0 - l.x1;
            float ny = l.y0 - l.y1;
            normalize(nx, ny);
            const float ox = -ny * kBidiOffset * z;
            const float oy = nx * kBidiOffset * z;
            l.x0 += ox;
            l.y0 += oy;
            l.x1 += ox;
            l.y1 += oy;
        }
        clipSource(l.x0, l.y0, l.x1, l.y1, rects_[static_cast<size_t>(t.from)].box);
        clipDestination(l.x0, l.y0, l.x1, l.y1, rects_[static_cast<size_t>(t.to)].box);
        for (const Line& other: lines_)
            if (transitions_[static_cast<size_t>(other.index)].from == t.from &&
                    transitions_[static_cast<size_t>(other.index)].to == t.to)
                l.hidden = true;
        lines_.push_back(l);
    }
}

void StateMachineEditor::paintIcon(PaintContext& ctx, float x0, float y0, float x1, float y1,
        TransitionSwitch mode, bool autoAdvance, float opacity, bool enabled)
{
    const EditorColors& ec = Theme::instance().editor;
    const float z = zoom_;
    const float k = (enabled ? 1.0f : 0.2f) * opacity;
    float dx = x1 - x0;
    float dy = y1 - y0;
    normalize(dx, dy);
    const float ox = dy;
    const float oy = -dx;
    const float mx = (x0 + x1) * 0.5f;
    const float my = (y0 + y1) * 0.5f;
    const float size = kIconSize * z;
    const float originX = mx - ox * size * 0.5f - dx * size * 0.5f;
    const float originY = my - oy * size * 0.5f - dy * size * 0.5f;
    const auto at = [&](float u, float v)
    { return Vec2(originX + (u * dx + v * ox) * z, originY + (u * dy + v * oy) * z); };

    const Color fill = scaled(autoAdvance ? ec.smAuto : ec.smIcon, k);
    const Color edge = scaled(ec.smIconEdge, k);
    const float stroke = std::max(1.0f, 1.5f * z);

    const auto triangle = [&](float left, float top, float bottom, float tip)
    {
        const Vec2 a = at(left, top);
        const Vec2 b = at(left, bottom);
        const Vec2 c = at(tip, (top + bottom) * 0.5f);
        ctx.drawList.addTriangleFilled(a, b, c, fill);
        ctx.drawList.addTriangle(a, b, c, edge, stroke);
    };
    const auto bar = [&](float left, float right)
    {
        const float top = 3.19f;
        const float bottom = 17.49f;
        ct::Vector<Vec2> poly;
        poly.push_back(at(left, top));
        poly.push_back(at(right, top));
        poly.push_back(at(right, bottom));
        poly.push_back(at(left, bottom));
        ctx.drawList.addConvexPolyFilled(poly, fill);
        ctx.drawList.addPolyline(poly, edge, stroke, true);
    };

    switch (mode)
    {
        case TransitionSwitch::Immediate:
            triangle(3.56f, 2.3f, 17.7f, 16.4f);
            break;
        case TransitionSwitch::Sync:
            bar(1.46f, 5.82f);
            triangle(7.56f, 3.18f, 17.3f, 18.3f);
            break;
        case TransitionSwitch::AtEnd:
            triangle(1.39f, 3.18f, 17.3f, 12.2f);
            bar(14.41f, 18.77f);
            break;
    }
}

void StateMachineEditor::paintTransition(PaintContext& ctx, float x0, float y0, float x1, float y1,
        const StateMachineTransition* t, bool enabled, bool selected, bool travel, float fadeRatio,
        bool autoAdvance, float opacity, bool endpointHovered, bool endpointHoveredStart)
{
    const EditorColors& ec = Theme::instance().editor;
    const float z = zoom_;
    Color line = enabled ? ec.smLine : scaled(ec.smLine, 0.2f);
    Color highlight = enabled ? ec.smAccent : scaled(ec.smAccent, 0.6f);
    line = scaled(line, opacity);
    highlight = scaled(highlight, opacity);
    if (travel) line = highlight;

    if (endpointHovered)
    {
        float dx = x1 - x0;
        float dy = y1 - y0;
        const float len = std::sqrt(dx * dx + dy * dy);
        normalize(dx, dy);
        const float reach = std::min(20.0f * z, len * 0.2f);
        float gx0, gy0, gx1, gy1;
        if (endpointHoveredStart)
        {
            gx0 = x0;
            gy0 = y0;
            gx1 = x0 + dx * reach;
            gy1 = y0 + dy * reach;
        }
        else
        {
            gx1 = x1;
            gy1 = y1;
            gx0 = x1 - dx * reach;
            gy0 = y1 - dy * reach;
        }
        Color c0 = highlight;
        Color c1 = highlight;
        if (endpointHoveredStart) c1.a = 0;
        else
            c0.a = 0;
        if (selected)
        {
            c0 = lightened(c0, 0.2f);
            c1 = lightened(c1, 0.2f);
        }
        const float w = (selected ? 10.0f : 8.0f) * z * 0.5f;
        const Vec2 pts[4] = { Vec2(gx0 - dy * w, gy0 + dx * w), Vec2(gx1 - dy * w, gy1 + dx * w),
            Vec2(gx1 + dy * w, gy1 - dx * w), Vec2(gx0 + dy * w, gy0 - dx * w) };
        const Color cols[4] = { c0, c1, c1, c0 };
        ctx.drawList.addConvexPolyFilledMultiColor(pts, cols, 4);
    }

    if (selected)
        ctx.drawList.addLine(Vec2(x0, y0), Vec2(x1, y1), highlight, std::max(2.0f, 6.0f * z));
    ctx.drawList.addLine(Vec2(x0, y0), Vec2(x1, y1), line, std::max(1.0f, 2.0f * z));

    if (fadeRatio > 0.0f)
    {
        Color fade = redHue(highlight);
        const float k = std::min(1.0f, fadeRatio);
        ctx.drawList.addLine(Vec2(x0, y0), Vec2(x0 + (x1 - x0) * k, y0 + (y1 - y0) * k), fade,
                std::max(1.0f, 2.0f * z));
    }

    TransitionSwitch mode = t ? t->mode : TransitionSwitch::Immediate;
    paintIcon(ctx, x0, y0, x1, y1, mode, autoAdvance, opacity, enabled);
}

void StateMachineEditor::paintFrame(PaintContext& ctx, int index, float opacity)
{
    const EditorColors& ec = Theme::instance().editor;
    const float z = zoom_;
    const StateMachineState& s = states_[static_cast<size_t>(index)];
    const NodeRect& r = rects_[static_cast<size_t>(index)];
    const float radius = std::max(2.0f, Theme::instance().borderRadius * z);
    const bool selected = s.selected;

    if (selected)
    {
        for (int k = 3; k >= 1; --k)
        {
            const float grow = k * 2.5f * z;
            ctx.fill.SetColor(Color(0, 0, 0, static_cast<uint8_t>(26.0f * opacity)));
            ctx.fillRoundedRect(r.box.x - grow, r.box.y - grow + 2.0f * z, r.box.w + 2.0f * grow,
                    r.box.h + 2.0f * grow, radius + grow);
        }
    }

    ctx.fill.SetColor(scaled(selected ? ec.smNodeSelected : ec.smNode, opacity));
    ctx.fillRoundedRect(r.box.x, r.box.y, r.box.w, r.box.h, radius);

    const auto border = [&](const Color& color, float thickness)
    {
        const float inset = thickness * 0.5f;
        ctx.drawList.addRoundRect(
                Rect(r.box.x + inset, r.box.y + inset, r.box.w - thickness, r.box.h - thickness),
                radius, scaled(color, opacity), thickness);
    };

    if (selected) border(ec.smAccent, std::max(2.0f, 3.0f * z));
    else
    {
        border(ec.smNodeEdge, std::max(1.0f, z));
        if (s.kind == StateKind::Start) border(ec.smStart, std::max(1.0f, z));
        if (s.kind == StateKind::End) border(ec.smEnd, std::max(1.0f, z));
    }

    const bool playing =
            playback_.playing && (playback_.current == index || playback_.fadingFrom == index);
    bool traveling = false;
    for (int p: playback_.travelPath)
        if (p == index) traveling = true;
    if (playing || (playback_.playing && traveling))
    {
        const float t = std::max(2.0f, 3.0f * z);
        ctx.fill.SetColor(scaled(ec.smPlaying, 0.15f * opacity));
        ctx.fillRoundedRect(r.box.x - 4.0f * z, r.box.y - 4.0f * z, r.box.w + 8.0f * z,
                r.box.h + 8.0f * z, radius + 4.0f * z);
        border(ec.smPlaying, t);
    }

    const float asc = ctx.font.GetAscender();
    const bool playHover = hover_.state == index && hover_.area == Area::Play;
    const Color iconColor = scaled(playHover ? ec.smAccent : ec.smIcon, opacity);
    {
        const float u = r.play.w / 16.0f;
        ctx.fill.SetColor(iconColor);
        ctx.fillTriangle(r.play.x + 4.0f * u, r.play.y + 4.0f * u, r.play.x + 4.0f * u,
                r.play.y + 12.0f * u, r.play.x + 12.0f * u, r.play.y + 8.0f * u);
    }

    ctx.font.SetColor(scaled(ec.smLine, opacity));
    ctx.font.Print(s.name.c_str(), r.name.x, r.name.y + asc);

    if (s.canOpen)
    {
        const bool openHover = hover_.state == index && hover_.area == Area::Open;
        const Color c = scaled(openHover ? ec.smAccent : ec.smIcon, opacity);
        const float u = r.open.w / 16.0f;
        ctx.fill.SetColor(c);
        ctx.fillRoundedRect(r.open.x + 6.0f * u, r.open.y + 1.0f * u, 4.0f * u, 3.0f * u, 1.0f * u);
        ct::Vector<Vec2> body;
        body.push_back(Vec2(r.open.x + 6.0f * u, r.open.y + 5.0f * u));
        body.push_back(Vec2(r.open.x + 10.0f * u, r.open.y + 5.0f * u));
        body.push_back(Vec2(r.open.x + 10.0f * u, r.open.y + 12.0f * u));
        body.push_back(Vec2(r.open.x + 8.0f * u, r.open.y + 15.0f * u));
        body.push_back(Vec2(r.open.x + 6.0f * u, r.open.y + 12.0f * u));
        ctx.drawList.addConvexPolyFilled(body, c);
        ctx.fill.SetColor(scaled(selected ? ec.smNodeSelected : ec.smNode, 1.0f));
        ctx.fillRect(r.open.x + 7.0f * u, r.open.y + 6.0f * u, 1.0f * u, 5.0f * u);
    }
}

void StateMachineEditor::paintProgress(PaintContext& ctx, int state, float ratio)
{
    if (state < 0 || static_cast<size_t>(state) >= states_.size()) return;
    const StateMachineState& s = states_[static_cast<size_t>(state)];
    if (s.kind != StateKind::Normal || s.canOpen) return;
    const EditorColors& ec = Theme::instance().editor;
    const NodeRect& r = rects_[static_cast<size_t>(state)];
    const float y = (r.play.y + r.play.h + r.box.y + r.box.h) * 0.5f;
    const float x0 = r.play.x;
    const float x1 = r.name.x + r.name.w;
    const float k = std::min(1.0f, std::max(0.0f, ratio));
    const float w = std::max(1.0f, 2.0f * zoom_);
    ctx.drawList.addLine(Vec2(x0, y), Vec2(x1, y), scaled(ec.smLine, 0.3f), w);
    ctx.drawList.addLine(Vec2(x0, y), Vec2(x0 + (x1 - x0) * k, y), ec.smLine, w);
}

void StateMachineEditor::paintGuides(PaintContext& ctx)
{
    if (drag_ != Drag::Move || pressState_ < 0 || pressState_ >= stateCount()) return;
    const EditorColors& ec = Theme::instance().editor;
    const StateMachineState& p = states_[static_cast<size_t>(pressState_)];
    const float fx = gx(p.x + dragX_);
    const float fy = gy(p.y + dragY_);
    const Color c = scaled(ec.smLine, 0.3f);
    if (snapX_ >= 0)
        ctx.drawList.addLine(Vec2(fx, fy),
                Vec2(gx(states_[static_cast<size_t>(snapX_)].x),
                        gy(states_[static_cast<size_t>(snapX_)].y)),
                c, 2.0f);
    if (snapY_ >= 0)
        ctx.drawList.addLine(Vec2(fx, fy),
                Vec2(gx(states_[static_cast<size_t>(snapY_)].x),
                        gy(states_[static_cast<size_t>(snapY_)].y)),
                c, 2.0f);
}

void StateMachineEditor::paint(PaintContext& ctx)
{
    if (!visible_) return;
    const Rect b = absoluteRect();
    const EditorColors& ec = Theme::instance().editor;
    if (b.w > 8.0f && b.h > 8.0f && (fitPending_ || (autoFit_ && (b.w != fitW_ || b.h != fitH_))))
        startFit();

    ctx.pushClip(b);
    ctx.fill.SetColor(ec.nodeBg);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    layoutNodes(ctx);
    layoutLines();
    paintGuides(ctx);

    if (drag_ == Drag::Connect && connectFrom_ >= 0 && connectFrom_ < stateCount())
    {
        const StateMachineState& a = states_[static_cast<size_t>(connectFrom_)];
        float x0 = gx(a.x);
        float y0 = gy(a.y);
        float x1 = connectX_;
        float y1 = connectY_;
        if (connectTo_ >= 0)
        {
            x1 = gx(states_[static_cast<size_t>(connectTo_)].x);
            y1 = gy(states_[static_cast<size_t>(connectTo_)].y);
        }
        clipSource(x0, y0, x1, y1, rects_[static_cast<size_t>(connectFrom_)].box);
        if (connectTo_ >= 0)
            clipDestination(x0, y0, x1, y1, rects_[static_cast<size_t>(connectTo_)].box);
        paintTransition(ctx, x0, y0, x1, y1, nullptr, true, false, false, 0.0f, false, 1.0f, false,
                false);
    }

    const int selectedT = selectedTransition();
    int onlyState = -1;
    int selectedCount = 0;
    for (size_t i = 0; i < states_.size(); ++i)
    {
        if (!states_[i].selected) continue;
        onlyState = static_cast<int>(i);
        ++selectedCount;
    }

    for (const Line& l: lines_)
    {
        const StateMachineTransition& t = transitions_[static_cast<size_t>(l.index)];
        if (drag_ == Drag::Reconnect && l.index == reconnectIndex_)
        {
            const StateMachineState& a = states_[static_cast<size_t>(t.from)];
            const StateMachineState& b2 = states_[static_cast<size_t>(t.to)];
            float x0, y0, x1, y1;
            int fixedState;
            if (reconnectStart_)
            {
                x0 = reconnectX_;
                y0 = reconnectY_;
                x1 = gx(b2.x);
                y1 = gy(b2.y);
                fixedState = t.to;
            }
            else
            {
                x0 = gx(a.x);
                y0 = gy(a.y);
                x1 = reconnectX_;
                y1 = reconnectY_;
                fixedState = t.from;
            }
            if (reconnectTarget_ != fixedState)
            {
                if (reconnectStart_)
                    clipDestination(x0, y0, x1, y1, rects_[static_cast<size_t>(fixedState)].box);
                else
                    clipSource(x0, y0, x1, y1, rects_[static_cast<size_t>(fixedState)].box);
                if (reconnectTarget_ >= 0)
                {
                    if (reconnectStart_)
                        clipSource(x0, y0, x1, y1,
                                rects_[static_cast<size_t>(reconnectTarget_)].box);
                    else
                        clipDestination(x0, y0, x1, y1,
                                rects_[static_cast<size_t>(reconnectTarget_)].box);
                }
                paintTransition(ctx, x0, y0, x1, y1, &t, !t.disabled, false, false, 0.0f,
                        t.autoAdvance || t.conditionOn, 0.8f, false, false);
            }
            continue;
        }
        if (l.hidden) continue;

        float opacity = 0.2f;
        if (selectedT >= 0)
        {
            const StateMachineTransition& st = transitions_[static_cast<size_t>(selectedT)];
            if ((t.from == st.from && t.to == st.to) || (t.from == st.to && t.to == st.from))
                opacity = 1.0f;
        }
        else if (selectedCount == 1)
        {
            if (t.from == onlyState || t.to == onlyState) opacity = 1.0f;
        }
        else
            opacity = 1.0f;

        bool travel = false;
        float fade = 0.0f;
        if (playback_.playing)
        {
            if (playback_.fadingFrom == t.from && playback_.current == t.to)
            {
                travel = true;
                fade = playback_.fadeRatio;
            }
            const auto& path = playback_.travelPath;
            if (!path.empty())
            {
                if (playback_.current == t.from && path[0] == t.to) travel = true;
                for (size_t j = 0; j + 1 < path.size(); ++j)
                    if (path[j] == t.from && path[j + 1] == t.to) travel = true;
            }
        }

        const bool hovered = hoverTransition_ == l.index;
        paintTransition(ctx, l.x0, l.y0, l.x1, l.y1, &t, !t.disabled, selectedT == l.index, travel,
                fade, t.autoAdvance || t.conditionOn, opacity, hovered, hoverTransitionStart_);
    }

    const float px = std::max(6.0f, Theme::instance().fontSize * zoom_);
    ctx.font.SetFontSize(px);
    ctx.font.SetBatch(&ctx.text);
    for (int i = 0; i < stateCount(); ++i) paintFrame(ctx, i, nodeOpacity(i));

    if (playback_.playing)
    {
        paintProgress(ctx, playback_.current, playback_.currentPosition);
        paintProgress(ctx, playback_.fadingFrom, playback_.fadingFromPosition);
    }

    if (drag_ == Drag::Box)
    {
        ctx.fill.SetColor(Color(178, 178, 255, 77));
        ctx.fillRect(std::min(boxX_, boxToX_), std::min(boxY_, boxToY_), std::fabs(boxX_ - boxToX_),
                std::fabs(boxY_ - boxToY_));
    }

    ctx.fill.SetColor(ec.nodeBorder);
    ctx.fillRect(b.x, b.y, b.w, 1);
    ctx.fillRect(b.x, b.y + b.h - 1, b.w, 1);
    ctx.fillRect(b.x, b.y, 1, b.h);
    ctx.fillRect(b.x + b.w - 1, b.y, 1, b.h);
    if (isFocused())
    {
        ctx.fill.SetColor(scaled(ec.smAccent, 0.8f));
        ctx.fillRect(b.x, b.y, b.w, 1);
        ctx.fillRect(b.x, b.y + b.h - 1, b.w, 1);
        ctx.fillRect(b.x, b.y, 1, b.h);
        ctx.fillRect(b.x + b.w - 1, b.y, 1, b.h);
    }

    ctx.popClip();
    Widget::paint(ctx);
}
