#include "clu/concurrency/spinlock.h"

#include <thread>

namespace clu
{
    void spinlock::lock() noexcept
    {
        int i = 0;
        while (locked_.test_and_set(std::memory_order::acquire))
        {
            if (i++ == spin_count)
            {
                i = 0;
                std::this_thread::yield();
            }
        }
    }

    void spinlock::unlock() noexcept { locked_.clear(std::memory_order::release); }
    bool spinlock::try_lock() noexcept { return !locked_.test_and_set(std::memory_order::acquire); }
} // namespace clu
