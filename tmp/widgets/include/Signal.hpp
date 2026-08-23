#pragma once
#include <cstdint>
#include <vector>
#include <type_traits>  // ~6k lines — replaces <functional> (~28k) + <algorithm> (~17k)

// ═════════════════════════════════════════════════════════════════════════════
//  Signal<Args...>  — lightweight signal/slot, Qt-style type erasure.
//
//  No std::function: stores callable as heap-allocated closure + raw fn ptr,
//  same technique Qt's moc generates but in pure C++17, header-only.
//
//  Usage:
//      Signal<int> onValueChanged;
//      auto id = onValueChanged.connect([](int v) { printf("%d\n", v); });
//      onValueChanged.emit(42);
//      onValueChanged.disconnect(id);
// ═════════════════════════════════════════════════════════════════════════════

namespace BuGUI
{

template <typename... Args>
class Signal
{
public:
    using SlotId = uint32_t;

    /// Connect any callable (lambda, function pointer, functor).
    /// Returns a unique ID for later disconnection.
    template <typename F>
    SlotId connect(F&& fn)
    {
        using FD = typename std::decay<F>::type;
        auto* copy = new FD(static_cast<F&&>(fn));
        // Captureless lambdas: FD is baked in at instantiation → valid fn ptr.
        slots_.push_back({
            nextId_++,
            copy,
            [](void* ctx, Args... args) { (*static_cast<FD*>(ctx))(static_cast<Args>(args)...); },
            [](void* ctx)               { delete static_cast<FD*>(ctx); }
        });
        return slots_.back().id;
    }

    void disconnect(SlotId id)
    {
        for (auto it = slots_.begin(); it != slots_.end(); ++it) {
            if (it->id == id) {
                it->destroy(it->ctx);
                slots_.erase(it);
                return;
            }
        }
    }

    void disconnectAll()
    {
        for (auto& s : slots_) s.destroy(s.ctx);
        slots_.clear();
    }

    void emit(Args... args) const
    {
        for (const auto& s : slots_)
            s.call(s.ctx, static_cast<Args>(args)...);
    }

    bool empty() const { return slots_.empty(); }

    ~Signal() { disconnectAll(); }

    // Non-copyable (owns heap allocations)
    Signal() = default;
    Signal(const Signal&)            = delete;
    Signal& operator=(const Signal&) = delete;
    Signal(Signal&&)                 = default;
    Signal& operator=(Signal&&)      = default;

private:
    struct Slot {
        SlotId  id;
        void*   ctx;
        void  (*call)(void*, Args...);
        void  (*destroy)(void*);
    };

    std::vector<Slot> slots_;
    SlotId nextId_ = 1;
};

} // namespace BuGUI
