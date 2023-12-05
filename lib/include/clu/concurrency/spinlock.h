#pragma once

#include <atomic>

namespace clu
{
    class spinlock
    {
    public:
        void lock() noexcept;
        void unlock() noexcept;
        bool try_lock() noexcept;

    private:
        static constexpr int spin_count = 20;
        std::atomic_flag locked_;
    };
} // namespace clu
