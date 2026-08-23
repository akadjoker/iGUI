#pragma once

#include "Signal.hpp"
#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>


namespace BuGUI
{
    
class Widget;

// Forward — EaseType & applyEasing live in WidgetApp.hpp
enum class EaseType;
float applyEasing(EaseType type, float t);

// ═════════════════════════════════════════════════════════════════════════════
//  Animation — a single property tween (float → float)
//
//  Usage:
//      auto* a = new Animation(0.0f, 1.0f, 0.3f, EaseType::OutCubic);
//      a->onUpdate.connect([btn](float v) { btn->setOpacity(v); });
//      a->onFinished.connect([] { printf("done\n"); });
//      Animator::instance().add(a);
//
//  The Animator ticks all active animations each frame.
// ═════════════════════════════════════════════════════════════════════════════

class Animation
{
public:
    /// @brief Animation lifecycle states.
    enum class State { Pending, Running, Paused, Finished };
    /// @brief Playback direction.
    enum class Direction { Forward, Backward };
    /// @brief Loop behavior mode.
    enum class LoopMode { Once, Loop, PingPong };

    Animation(float from, float to, float durationSec, EaseType ease);
    ~Animation() = default;

    // ── Configuration ─────────────────────────────────────────────────────
    /// @brief Set the initial delay before the animation starts.
    void setDelay(float sec)              { delay_ = sec; }
    /// @brief Set the loop mode (Once, Loop, or PingPong).
    void setLoopMode(LoopMode m)          { loopMode_ = m; }
    /// @brief Set the number of loop iterations (0 = infinite).
    void setLoopCount(int n)              { loopCount_ = n; }
    /// @brief Set whether the animation auto-deletes on finish.
    void setAutoDelete(bool d)            { autoDelete_ = d; }
    /// @brief Set a tag string for identification.
    void setTag(const std::string& t)     { tag_ = t; }
    /// @brief Get the tag string.
    const std::string& tag() const        { return tag_; }

    // ── Control ───────────────────────────────────────────────────────────
    /// @brief Start the animation.
    void start();
    /// @brief Pause the animation.
    void pause();
    /// @brief Resume a paused animation.
    void resume();
    /// @brief Stop the animation and jump to the end value.
    void stop();
    /// @brief Reset and restart the animation from the beginning.
    void restart();

    // ── Query ─────────────────────────────────────────────────────────────
    /// @brief Get the current animation state.
    State state()     const { return state_; }
    /// @brief Get the raw progress (0..1).
    float progress()  const { return progress_; }
    /// @brief Get the current eased value (from..to).
    float value()     const { return value_; }
    /// @brief Get the start value.
    float from()      const { return from_; }
    /// @brief Get the end value.
    float to()        const { return to_; }
    /// @brief Get the animation duration in seconds.
    float duration()  const { return duration_; }
    /// @brief Get whether auto-delete is enabled.
    bool  autoDelete()const { return autoDelete_; }

    // ── Signals ───────────────────────────────────────────────────────────
    /// @brief Emitted with the current value each tick.
    Signal<float> onUpdate;
    /// @brief Emitted when fully done (all loops completed).
    Signal<>      onFinished;
    /// @brief Emitted with the loop index at each loop boundary.
    Signal<int>   onLoopEnd;

    /// @brief Advance the animation by the given timestep.
    void tick(float dt);

private:
    void applyValue();

    float from_, to_;
    float duration_;
    EaseType ease_;

    float delay_      = 0.0f;
    float elapsed_    = 0.0f;
    float progress_   = 0.0f;
    float value_;

    State     state_     = State::Pending;
    Direction dir_       = Direction::Forward;
    LoopMode  loopMode_  = LoopMode::Once;
    int       loopCount_ = 1;    // 0 = infinite
    int       loopIndex_ = 0;
    bool      autoDelete_= true;
    std::string tag_;
};

// ═════════════════════════════════════════════════════════════════════════════
//  AnimationGroup — run animations in Parallel or Sequential
// ═════════════════════════════════════════════════════════════════════════════

class AnimationGroup
{
public:
    /// @brief Group execution mode.
    enum class Mode { Parallel, Sequential };

    explicit AnimationGroup(Mode mode = Mode::Parallel);
    ~AnimationGroup();

    /// @brief Add an animation to the group (takes ownership).
    void add(Animation* a);
    /// @brief Start all animations in the group.
    void start();
    /// @brief Stop all animations in the group.
    void stop();
    /// @brief Advance the group by the given timestep.
    void tick(float dt);

    /// @brief Check if all animations have completed.
    bool isFinished() const { return finished_; }
    /// @brief Get whether auto-delete is enabled.
    bool autoDelete() const { return autoDelete_; }
    /// @brief Set whether the group auto-deletes on finish.
    void setAutoDelete(bool d) { autoDelete_ = d; }

    /// @brief Emitted when all animations in the group complete.
    Signal<> onFinished;

private:
    Mode mode_;
    std::vector<Animation*> anims_;
    int  currentIndex_ = 0;
    bool running_      = false;
    bool finished_     = false;
    bool autoDelete_   = true;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Animator — singleton that ticks all active animations
//
//  Called once per frame from WidgetApp::update():
//      Animator::instance().tick(dt_);
// ═════════════════════════════════════════════════════════════════════════════

class Animator
{
public:
    /// @brief Get the singleton Animator instance.
    static Animator& instance();

    /// @brief Add a standalone animation (takes ownership if autoDelete).
    void add(Animation* a);
    /// @brief Add an animation group.
    void add(AnimationGroup* g);

    /// @brief Remove an animation without deleting it.
    void remove(Animation* a);
    /// @brief Remove an animation group without deleting it.
    void remove(AnimationGroup* g);

    /// @brief Cancel all animations matching the given tag.
    void cancelAll(const std::string& tag);

    /// @brief Advance all active animations by the given timestep.
    void tick(float dt);

    /// @brief Get the number of active animations and groups.
    int count() const { return static_cast<int>(anims_.size() + groups_.size()); }

    // ── Convenience factory methods ───────────────────────────────────────
    // These create, configure and register an Animation in one call.

    /// @brief Create and register a float tween animation.
    static Animation* animate(float from, float to, float duration,
                               EaseType ease,
                               std::function<void(float)> onUpdate,
                               std::function<void()> onDone = nullptr);

    /// @brief Fade a widget in (opacity 0 to 1).
    static Animation* fadeIn(Widget* w, float duration = 0.25f, EaseType ease = {});
    /// @brief Fade a widget out (opacity 1 to 0).
    static Animation* fadeOut(Widget* w, float duration = 0.25f, EaseType ease = {});

    /// @brief Slide a widget in from the given offset.
    static Animation* slideIn(Widget* w, float fromX, float fromY,
                               float duration = 0.3f, EaseType ease = {});

private:
    Animator() = default;
    Animator(const Animator&) = delete;
    Animator& operator=(const Animator&) = delete;

    std::vector<Animation*>      anims_;
    std::vector<AnimationGroup*> groups_;

    // Deferred additions during tick to avoid iterator invalidation
    bool                         ticking_ = false;
    std::vector<Animation*>      pendingAnims_;
    std::vector<AnimationGroup*> pendingGroups_;
};

} // namespace BuGUI
