#pragma once

#include "execution.h"

namespace clu
{
    namespace detail
    {

    }

    class async_manual_reset_event
    {
    public:
        explicit async_manual_reset_event(const bool start_set = false) noexcept {}

        async_manual_reset_event(async_manual_reset_event&&) = delete;
        async_manual_reset_event(const async_manual_reset_event&) = delete;

        ~async_manual_reset_event() noexcept {}

        // This method has acquire-release semantics.
        void set() noexcept;

        // This method has acquire semantics.
        bool ready() const noexcept;

        // This method has acquire-release semantics.
        void reset() noexcept;

        // Returns a sender that will complete when the event is "set".
        //
        // The sender will complete immediately if the event is already "set".
        //
        // Regardless of the receiver to which this sender is connected, the sender
        // is unstoppable.
        //
        // Regardless of whether the sender completes immediately or waits first,
        // the completion will first be scheduled onto the receiver's scheduler with
        // schedule().
        [[nodiscard]] auto async_wait() noexcept;

    private:
    };
} // namespace clu
