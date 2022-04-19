#include "clu/concurrency.h"
#include "clu/assertion.h"

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

    namespace detail::lck_ptr
    {
        constexpr std::uintptr_t not_locked = 0;
        constexpr std::uintptr_t locked_no_notify = 1;
        constexpr std::uintptr_t locked_should_notify = 2;
        constexpr std::uintptr_t mask = 3;

        void* lock_and_load(std::atomic_uintptr_t& value) noexcept
        {
            auto loaded = value.load(std::memory_order::relaxed);
            while (true)
                switch (loaded & mask)
                {
                    case not_locked:
                    {
                        // No one is holding the lock, acquire it
                        const auto locked_value = loaded | locked_no_notify;
                        if (value.compare_exchange_weak(
                                loaded, locked_value, std::memory_order::acquire, std::memory_order::relaxed))
                            return reinterpret_cast<void*>(loaded); // NOLINT(performance-no-int-to-ptr)
                        break;
                    }
                    case locked_no_notify:
                    {
                        // No one was waiting, change the state so that
                        // we could be notified
                        const auto notify_value = (loaded & ~mask) | locked_should_notify;
                        if (!value.compare_exchange_weak(loaded, notify_value, std::memory_order::relaxed))
                            break;
                        [[fallthrough]];
                    }
                    case locked_should_notify:
                    {
                        // Join the wait list
                        value.wait(loaded, std::memory_order::relaxed);
                        loaded = value.load(std::memory_order::relaxed);
                        break;
                    }
                    default: unreachable();
                }
        }

        void store_and_unlock(std::atomic_uintptr_t& value, void* ptr) noexcept
        {
            const auto old = value.exchange(reinterpret_cast<std::uintptr_t>(ptr), std::memory_order::release);
            if ((old & mask) == locked_should_notify)
                value.notify_all();
        }
    } // namespace detail::lck_ptr
} // namespace clu