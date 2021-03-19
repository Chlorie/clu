#pragma once

#include <cstdint>

namespace clu::testing
{
    struct LifetimeCounter final
    {
        size_t def_ctor = 0;
        size_t copy_ctor = 0;
        size_t move_ctor = 0;
        size_t copy_assign = 0;
        size_t move_assign = 0;
        size_t dtor = 0;

        size_t total_ctor() const noexcept { return def_ctor + copy_ctor + move_ctor; }
        size_t total_copy() const noexcept { return copy_ctor + copy_assign; }
        size_t total_move() const noexcept { return move_ctor + move_assign; }
        size_t total_assign() const noexcept { return copy_assign + move_assign; }
        ptrdiff_t alive() const noexcept { return static_cast<ptrdiff_t>(total_ctor()) - static_cast<ptrdiff_t>(dtor); }
    };

    class LifetimeNotifier final
    {
        friend class LifetimeCountingScope;
    private:
        inline static LifetimeCounter* counter_ = nullptr;
    public:
        LifetimeNotifier() { if (counter_) counter_->def_ctor++; }
        LifetimeNotifier(const LifetimeNotifier&) { if (counter_) counter_->copy_ctor++; }
        LifetimeNotifier(LifetimeNotifier&&) noexcept { if (counter_) counter_->move_ctor++; }
        LifetimeNotifier& operator=(const LifetimeNotifier&) { if (counter_) counter_->copy_assign++; return *this; }
        LifetimeNotifier& operator=(LifetimeNotifier&&) noexcept { if (counter_) counter_->move_assign++; return *this; }
        ~LifetimeNotifier() noexcept { if (counter_) counter_->dtor++; }
    };

    class LifetimeCountingScope final
    {
    private:
        LifetimeCounter* original_ = nullptr;

    public:
        explicit LifetimeCountingScope(LifetimeCounter& counter)
        {
            original_ = LifetimeNotifier::counter_;
            LifetimeNotifier::counter_ = &counter;
        }

        ~LifetimeCountingScope() noexcept { LifetimeNotifier::counter_ = original_; }

        LifetimeCountingScope(const LifetimeCountingScope&) = delete;
        LifetimeCountingScope(LifetimeCountingScope&&) = delete;
        LifetimeCountingScope& operator=(const LifetimeCountingScope&) = delete;
        LifetimeCountingScope& operator=(LifetimeCountingScope&&) = delete;
    };
}
