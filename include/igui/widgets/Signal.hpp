#pragma once
#include <cstdint>
#include <ct/vector.hpp>
#include <type_traits> // ~6k lines — replaces <functional> (~28k) + <algorithm> (~17k)

// ═════════════════════════════════════════════════════════════════════════════
//  Signal<Args...>  — lightweight signal/slot, Qt-style type erasure.
//
//  No ct::Function: stores callable as heap-allocated closure + raw fn ptr,
//  same technique Qt's moc generates but in pure C++17, header-only.
//
//  Usage:
//      Signal<int> onValueChanged;
//      auto id = onValueChanged.connect([](int v) { printf("%d\n", v); });
//      onValueChanged.emit(42);
//      onValueChanged.disconnect(id);
// ═════════════════════════════════════════════════════════════════════════════

namespace ig
{
namespace retained
{

template<typename... Args>
class Signal
{
public:
    using SlotId = uint32_t;

    /// Connect any callable (lambda, function pointer, functor).
    /// Returns a unique ID for later disconnection.
    template<typename F>
    SlotId connect(F&& fn)
    {
        using FD = typename std::decay<F>::type;
        auto* copy = new FD(static_cast<F&&>(fn));
        // Captureless lambdas: FD is baked in at instantiation → valid fn ptr.
        slots_.push_back({ nextId_++, copy,
            [](void* ctx, Args... args) { (*static_cast<FD*>(ctx))(static_cast<Args>(args)...); },
            [](void* ctx) { delete static_cast<FD*>(ctx); }, false });
        return slots_.back().id;
    }

    // A handler may disconnect any slot, itself included, while the signal is
    // emitting: the slot is only marked dead then, and freed once the outermost
    // emit is done, so the closure that is running is never freed under it.
    void disconnect(SlotId id)
    {
        for (auto it = slots_.begin(); it != slots_.end(); ++it)
        {
            if (it->id == id && !it->dead)
            {
                if (emitting_ > 0)
                {
                    it->dead = true;
                    pendingCleanup_ = true;
                    return;
                }
                it->destroy(it->ctx);
                slots_.erase(it);
                return;
            }
        }
    }

    void disconnectAll()
    {
        if (emitting_ > 0)
        {
            for (auto& s: slots_) s.dead = true;
            pendingCleanup_ = true;
            return;
        }
        for (auto& s: slots_) s.destroy(s.ctx);
        slots_.clear();
    }

    // Slots connected by a handler wait for the next emit; slots are copied before
    // the call because connecting may move the array; and a handler that destroys
    // the signal ends the emit without it being read again. The closures of a
    // signal destroyed that way are freed by the outermost emit, once the closure
    // that was running has returned.
    void emit(Args... args) const
    {
        EmitFrame frame;
        frame.outer = frame_;
        frame_ = &frame;
        ++emitting_;
        const auto count = slots_.size();
        for (decltype(slots_.size()) i = 0; i < count; ++i)
        {
            if (slots_[i].dead) continue;
            const Slot slot = slots_[i];
            slot.call(slot.ctx, static_cast<Args>(args)...);
            if (frame.destroyed)
            {
                if (frame.outer)
                {
                    frame.outer->destroyed = true;
                    for (auto& orphan: frame.orphans) frame.outer->orphans.push_back(orphan);
                }
                else
                {
                    for (auto& orphan: frame.orphans) orphan.destroy(orphan.ctx);
                }
                return;
            }
        }
        frame_ = frame.outer;
        if (--emitting_ == 0 && pendingCleanup_) removeDead();
    }

    bool empty() const
    {
        for (const auto& s: slots_)
            if (!s.dead) return false;
        return true;
    }

    ~Signal()
    {
        if (frame_)
        {
            frame_->destroyed = true;
            for (auto& s: slots_) frame_->orphans.push_back(s);
        }
        else
        {
            for (auto& s: slots_) s.destroy(s.ctx);
        }
        slots_.clear();
    }

    // Non-copyable (owns heap allocations)
    Signal() = default;
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    Signal(Signal&&) = default;
    Signal& operator=(Signal&&) = default;

private:
    struct Slot
    {
        SlotId id;
        void* ctx;
        void (*call)(void*, Args...);
        void (*destroy)(void*);
        bool dead;
    };

    void removeDead() const
    {
        for (auto it = slots_.begin(); it != slots_.end();)
        {
            if (it->dead)
            {
                it->destroy(it->ctx);
                it = slots_.erase(it);
            }
            else
            {
                ++it;
            }
        }
        pendingCleanup_ = false;
    }

    struct EmitFrame
    {
        bool destroyed = false;
        ct::Vector<Slot> orphans;
        EmitFrame* outer = nullptr;
    };

    mutable ct::Vector<Slot> slots_;
    SlotId nextId_ = 1;
    mutable int emitting_ = 0;
    mutable bool pendingCleanup_ = false;
    mutable EmitFrame* frame_ = nullptr;
};

} // namespace retained
} // namespace ig
