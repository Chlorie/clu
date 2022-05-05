#pragma once

#include "../execution.h"

namespace clu
{
    class CLU_API async_manual_reset_event
    {
    public:
        explicit async_manual_reset_event(const bool start_set = false) noexcept: tail_(start_set ? this : nullptr) {}
        async_manual_reset_event(async_manual_reset_event&&) = delete;
        async_manual_reset_event(const async_manual_reset_event&) = delete;
        ~async_manual_reset_event() noexcept = default;

        void set() noexcept;
        bool ready() const noexcept { return tail_.load(std::memory_order::acquire) == this; }
        void reset() noexcept;

        [[nodiscard]] auto wait_async() noexcept
        {
            const auto wait_and_transfer_to_outer = [this](const auto& schd)
            {
                return exec::schedule(exec::context_scheduler<async_manual_reset_event>(this)) //
                    | exec::transfer(schd);
            };
            return exec::get_scheduler() //
                | exec::let_value(wait_and_transfer_to_outer);
        }

    private:
        using ops_base = exec::context_operation_state_base<async_manual_reset_event>;

        // The tail stores this when the event is set, and stores a pointer to
        // the last inserted pending ops_base* if there is any, or nullptr otherwise.
        std::atomic<void*> tail_{nullptr};

        CLU_API friend void tag_invoke(exec::add_operation_t, async_manual_reset_event& self, ops_base& task);
    };
} // namespace clu
