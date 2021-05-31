#pragma once

#include <atomic>
#include <coroutine>
#include <utility>

namespace clu
{
    class adapt_async_lock_t {};
    inline constexpr adapt_async_lock_t adapt_async_lock{};

    class async_mutex;

    class async_scoped_lock final
    {
    private:
        async_mutex* mutex_ = nullptr;

    public:
        async_scoped_lock(adapt_async_lock_t, async_mutex& mutex): mutex_(&mutex) {}
        ~async_scoped_lock() noexcept;
        async_scoped_lock(const async_scoped_lock&) = delete;
        async_scoped_lock(async_scoped_lock&&) = delete;
        async_scoped_lock& operator=(const async_scoped_lock&) = delete;
        async_scoped_lock& operator=(async_scoped_lock&&) = delete;
    };

    class async_mutex final
    {
    public:
        class awaiter
        {
            friend class async_mutex;
        private:
            std::coroutine_handle<> handle_{};

        protected:
            async_mutex* mutex_ = nullptr;
            uintptr_t next_ = reinterpret_cast<uintptr_t>(static_cast<awaiter*>(nullptr));

            awaiter* next() const noexcept { return reinterpret_cast<awaiter*>(next_); }
            void set_next(awaiter* ptr) noexcept { next_ = reinterpret_cast<uintptr_t>(ptr); }

        public:
            explicit awaiter(async_mutex& mutex): mutex_(&mutex) {}
            bool await_ready() const noexcept { return false; }
            void await_resume() const noexcept {}

            bool await_suspend(const std::coroutine_handle<> handle) noexcept
            {
                handle_ = handle;
                uintptr_t expected = not_locked;
                while (true)
                {
                    // Not locked before
                    if (mutex_->awaiting_.compare_exchange_weak(
                        expected, locked_no_awaiting,
                        std::memory_order_acquire, // Acquire things that previous lock holder releases
                        std::memory_order_relaxed))
                        return false;
                    else // Locked before, try to push this onto the stack
                    {
                        next_ = expected;
                        if (mutex_->awaiting_.compare_exchange_weak(
                            expected, reinterpret_cast<uintptr_t>(this),
                            std::memory_order_release, // Release the write to the members
                            std::memory_order_relaxed))
                            return true;
                    }
                }
            }
        };

        class awaiter_scoped final : public awaiter
        {
        public:
            using awaiter::awaiter;
            async_scoped_lock await_resume() const noexcept { return { adapt_async_lock, *mutex_ }; }
        };

    private:
        static_assert(alignof(awaiter) > 2);

        static constexpr uintptr_t not_locked = 1;
        static constexpr uintptr_t locked_no_awaiting = 2;

        std::atomic<uintptr_t> awaiting_{ not_locked }; // Awaiter stack (FILO)
        awaiter* awaiters_rev_ = nullptr; // Reversed stack (now it's FIFO)

    public:
        async_mutex() = default;
        ~async_mutex() = default;
        async_mutex(const async_mutex&) = delete;
        async_mutex(async_mutex&&) = delete;
        async_mutex& operator=(const async_mutex&) = delete;
        async_mutex& operator=(async_mutex&&) = delete;

        awaiter async_lock() noexcept { return awaiter(*this); }
        awaiter_scoped async_lock_scoped() noexcept { return awaiter_scoped(*this); }

        void unlock()
        {
            // This function can only be called by one coroutine,
            // no need to worry about concurrent read/write.
            // Only one thread popping the stack means no ABA problems, yeah!
            if (!awaiters_rev_) // No awaiters left on the reversed stack
            {
                uintptr_t expected = locked_no_awaiting;
                if (awaiting_.compare_exchange_strong(
                    expected, not_locked,
                    std::memory_order_release, // Releasing the writes by current lock holder
                    std::memory_order_relaxed))
                    return; // No new awaiters
                else // There're new awaiters
                {
                    uintptr_t top = awaiting_.exchange(locked_no_awaiting,
                        std::memory_order_acquire); // Acquire writes to awaiter members
                    // Reverse into the to-be-resumed stack
                    do
                    {
                        auto* ptr = reinterpret_cast<awaiter*>(top);
                        top = ptr->next_;
                        ptr->set_next(awaiters_rev_);
                        awaiters_rev_ = ptr;
                    } while (top != locked_no_awaiting);
                }
            }
            // Pop front and resume that coroutine
            auto* next = reinterpret_cast<awaiter*>(awaiters_rev_->next_);
            std::exchange(awaiters_rev_, next)->handle_();
        }
    };

    inline async_scoped_lock::~async_scoped_lock() noexcept { mutex_->unlock(); }
}
