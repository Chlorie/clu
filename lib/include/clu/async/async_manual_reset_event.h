#pragma once

#include "execution.h"

namespace clu
{
    namespace detail::async_ev
    {

    }

    class async_manual_reset_event
    {
    public:
        explicit async_manual_reset_event(const bool start_set = false) noexcept {}

        async_manual_reset_event(async_manual_reset_event&&) = delete;
        async_manual_reset_event(const async_manual_reset_event&) = delete;

        ~async_manual_reset_event() noexcept {}

        void set() noexcept; // acq_rel
        bool ready() const noexcept; // acq
        void reset() noexcept; // acq_rel

        [[nodiscard]] auto async_wait() noexcept;

    private:
    };
} // namespace clu
