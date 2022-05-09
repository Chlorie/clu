#pragma once

#include "async_manual_reset_event.h"

namespace clu
{
    class async_mutex
    {
    public:
        async_mutex() noexcept = default;

        bool try_lock() noexcept;
        void unlock() noexcept;

    private:
    };
} // namespace clu
