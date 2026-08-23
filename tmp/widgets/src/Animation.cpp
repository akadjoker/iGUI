#include "Animation.hpp"
#include "WidgetApp.hpp"  // for EaseType, applyEasing
#include "Widget.hpp"
#include <algorithm>
#include <cmath>

// ═════════════════════════════════════════════════════════════════════════════
//  Animation
// ═════════════════════════════════════════════════════════════════════════════

Animation::Animation(float from, float to, float durationSec, EaseType ease)
    : from_(from), to_(to), duration_(durationSec), ease_(ease), value_(from)
{
}

void Animation::start()
{
    state_     = State::Running;
    elapsed_   = -delay_;
    progress_  = 0.0f;
    value_     = from_;
    dir_       = Direction::Forward;
    loopIndex_ = 0;
}

void Animation::pause()
{
    if (state_ == State::Running) state_ = State::Paused;
}

void Animation::resume()
{
    if (state_ == State::Paused) state_ = State::Running;
}

void Animation::stop()
{
    state_    = State::Finished;
    progress_ = 1.0f;
    value_    = to_;
    onUpdate.emit(value_);
    onFinished.emit();
}

void Animation::restart()
{
    state_     = State::Pending;
    elapsed_   = 0.0f;
    progress_  = 0.0f;
    value_     = from_;
    loopIndex_ = 0;
    start();
}

void Animation::tick(float dt)
{
    if (state_ != State::Running) return;

    elapsed_ += dt;

    // Still in delay
    if (elapsed_ < 0.0f) return;

    float t = (duration_ > 0.0f) ? elapsed_ / duration_ : 1.0f;

    if (t >= 1.0f)
    {
        // End of one cycle
        bool lastLoop = (loopCount_ > 0 && loopIndex_ + 1 >= loopCount_);

        if (loopMode_ == LoopMode::Once || lastLoop)
        {
            progress_ = 1.0f;
            applyValue();
            state_ = State::Finished;
            onUpdate.emit(value_);
            onFinished.emit();
            return;
        }

        // Loop
        onLoopEnd.emit(loopIndex_);
        loopIndex_++;
        elapsed_ -= duration_;

        if (loopMode_ == LoopMode::PingPong)
            dir_ = (dir_ == Direction::Forward) ? Direction::Backward : Direction::Forward;

        t = (duration_ > 0.0f) ? elapsed_ / duration_ : 1.0f;
        t = std::clamp(t, 0.0f, 1.0f);
    }

    progress_ = t;
    applyValue();
    onUpdate.emit(value_);
}

void Animation::applyValue()
{
    float easedT = applyEasing(ease_, progress_);
    if (dir_ == Direction::Backward)
        easedT = 1.0f - easedT;
    value_ = from_ + (to_ - from_) * easedT;
}

// ═════════════════════════════════════════════════════════════════════════════
//  AnimationGroup
// ═════════════════════════════════════════════════════════════════════════════

AnimationGroup::AnimationGroup(Mode mode) : mode_(mode) {}

AnimationGroup::~AnimationGroup()
{
    for (auto* a : anims_) delete a;
}

void AnimationGroup::add(Animation* a)
{
    a->setAutoDelete(false);  // group manages lifetime
    anims_.push_back(a);
}

void AnimationGroup::start()
{
    running_      = true;
    finished_     = false;
    currentIndex_ = 0;

    if (mode_ == Mode::Parallel) {
        for (auto* a : anims_) a->start();
    } else if (!anims_.empty()) {
        anims_[0]->start();
    }
}

void AnimationGroup::stop()
{
    for (auto* a : anims_) a->stop();
    running_  = false;
    finished_ = true;
    onFinished.emit();
}

void AnimationGroup::tick(float dt)
{
    if (!running_ || finished_) return;

    if (mode_ == Mode::Parallel)
    {
        bool allDone = true;
        for (auto* a : anims_) {
            a->tick(dt);
            if (a->state() != Animation::State::Finished)
                allDone = false;
        }
        if (allDone) {
            finished_ = true;
            running_  = false;
            onFinished.emit();
        }
    }
    else // Sequential
    {
        if (currentIndex_ < static_cast<int>(anims_.size()))
        {
            auto* cur = anims_[currentIndex_];
            cur->tick(dt);
            if (cur->state() == Animation::State::Finished) {
                currentIndex_++;
                if (currentIndex_ < static_cast<int>(anims_.size()))
                    anims_[currentIndex_]->start();
            }
        }
        if (currentIndex_ >= static_cast<int>(anims_.size())) {
            finished_ = true;
            running_  = false;
            onFinished.emit();
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Animator (singleton)
// ═════════════════════════════════════════════════════════════════════════════

Animator& Animator::instance()
{
    static Animator inst;
    return inst;
}

void Animator::add(Animation* a)
{
    if (ticking_) {
        pendingAnims_.push_back(a);
        return;
    }
    anims_.push_back(a);
    if (a->state() == Animation::State::Pending)
        a->start();
}

void Animator::add(AnimationGroup* g)
{
    groups_.push_back(g);
}

void Animator::remove(Animation* a)
{
    anims_.erase(std::remove(anims_.begin(), anims_.end(), a), anims_.end());
}

void Animator::remove(AnimationGroup* g)
{
    groups_.erase(std::remove(groups_.begin(), groups_.end(), g), groups_.end());
}

void Animator::cancelAll(const std::string& tag)
{
    if (tag.empty()) {
        // No tag: clear everything
        for (auto* a : anims_) if (a->autoDelete()) delete a;
        anims_.clear();
        for (auto* g : groups_) if (g->autoDelete()) delete g;
        groups_.clear();
    } else {
        // Filter by tag
        for (auto it = anims_.begin(); it != anims_.end();) {
            if ((*it)->tag() == tag) {
                if ((*it)->autoDelete()) delete *it;
                it = anims_.erase(it);
            } else { ++it; }
        }
    }
}

void Animator::tick(float dt)
{
    ticking_ = true;

    // Tick animations (by index — callbacks may add new ones via pendingAnims_)
    for (size_t i = 0; i < anims_.size(); ++i)
        anims_[i]->tick(dt);

    // Clean up finished
    for (auto it = anims_.begin(); it != anims_.end();) {
        if ((*it)->state() == Animation::State::Finished) {
            if ((*it)->autoDelete()) delete *it;
            it = anims_.erase(it);
        } else {
            ++it;
        }
    }

    // Tick groups (by index)
    for (size_t i = 0; i < groups_.size(); ++i)
        groups_[i]->tick(dt);

    for (auto it = groups_.begin(); it != groups_.end();) {
        if ((*it)->isFinished()) {
            if ((*it)->autoDelete()) delete *it;
            it = groups_.erase(it);
        } else {
            ++it;
        }
    }

    ticking_ = false;

    // Flush deferred additions
    for (auto* a : pendingAnims_) {
        anims_.push_back(a);
        if (a->state() == Animation::State::Pending) a->start();
    }
    pendingAnims_.clear();
    for (auto* g : pendingGroups_) groups_.push_back(g);
    pendingGroups_.clear();
}

// ── Convenience factory ──────────────────────────────────────────────────────

Animation* Animator::animate(float from, float to, float duration,
                              EaseType ease,
                              std::function<void(float)> onUpdate,
                              std::function<void()> onDone)
{
    auto* a = new Animation(from, to, duration, ease);
    if (onUpdate) a->onUpdate.connect(std::move(onUpdate));
    if (onDone)   a->onFinished.connect(std::move(onDone));
    instance().add(a);
    return a;
}

Animation* Animator::fadeIn(Widget* w, float duration, EaseType ease)
{
    // Animate by toggling visible at end; no opacity on Widget yet
    if (w) w->setVisible(true);
    return animate(0.0f, 1.0f, duration,
                   ease == EaseType{} ? EaseType::OutCubic : ease,
                   nullptr, nullptr);
}

Animation* Animator::fadeOut(Widget* w, float duration, EaseType ease)
{
    return animate(1.0f, 0.0f, duration,
                   ease == EaseType{} ? EaseType::OutCubic : ease,
                   nullptr,
                   [w] { if (w) w->setVisible(false); });
}

Animation* Animator::slideIn(Widget* w, float fromX, float fromY,
                              float duration, EaseType ease)
{
    if (!w) return nullptr;
    Rect target = w->rect();
    float startX = target.x + fromX;
    float startY = target.y + fromY;
    float endX   = target.x;
    float endY   = target.y;

    auto* gx = new Animation(startX, endX, duration,
                              ease == EaseType{} ? EaseType::OutCubic : ease);
    auto* gy = new Animation(startY, endY, duration,
                              ease == EaseType{} ? EaseType::OutCubic : ease);

    gx->onUpdate.connect([w](float v) {
        Rect r = w->rect(); r.x = v; w->setRect(r);
    });
    gy->onUpdate.connect([w](float v) {
        Rect r = w->rect(); r.y = v; w->setRect(r);
    });

    auto* group = new AnimationGroup(AnimationGroup::Mode::Parallel);
    group->add(gx);
    group->add(gy);
    group->start();
    instance().add(group);
    return nullptr;  // group manages the animations
}
