#pragma once

#include <shared_mutex>
#include <atomic>

#include "assertion.h"

namespace clu
{
    template <typename T>
    class locked
    {
    public:
        // @formatter:off
        locked()
            noexcept(std::is_nothrow_default_constructible_v<T>)
            requires std::default_initializable<T>
        {
            value_.construct();
        }

        explicit locked(const T& value)
            noexcept(std::is_nothrow_copy_constructible_v<T>)
        {
            value_.construct(value);
        }

        explicit locked(T&& value)
            noexcept(std::is_nothrow_move_constructible_v<T>)
        {
            value_.construct(static_cast<T&&>(value));
        }

        template <typename... Ts>
        explicit locked(std::in_place_t, Ts&&... args)
            noexcept(std::is_nothrow_constructible_v<T, Ts...>)
        {
            value_.construct(static_cast<Ts&&>(args)...);
        }

        locked(const locked& other)
        {
            std::shared_lock l(other.mutex_);
            value_.construct(other.value_.value);
        }

        locked(locked&& other) noexcept(false)
        {
            std::unique_lock l(other.mutex_);
            value_.construct(static_cast<T&&>(other.value_.value));
        }

        ~locked() noexcept { value_.destruct(); }

        locked& operator=(const locked& other)
        {
            std::unique_lock l1(mutex_, std::defer_lock);
            std::shared_lock l2(other.mutex_, std::defer_lock);
            std::lock(l1, l2);
            value_.value = other.value_.value;
            return *this;
        }

        locked& operator=(locked&& other) noexcept(false)
        {
            std::unique_lock l1(mutex_, std::defer_lock);
            std::unique_lock l2(other.mutex_, std::defer_lock);
            std::lock(l1, l2);
            value_.value = static_cast<T&&>(other.value_.value);
            return *this;
        }

        void swap(locked& other) noexcept(false)
        {
            std::unique_lock l1(mutex_, std::defer_lock);
            std::unique_lock l2(other.mutex_, std::defer_lock);
            std::lock(l1, l2);
            using std::swap;
            swap(value_.value, other.value_.value);
        }

        friend void swap(locked& lhs, locked& rhs) noexcept(false) { lhs.swap(rhs); }
        // @formatter:on

        [[nodiscard]] T operator*() const &
        {
            std::shared_lock l(mutex_);
            return value_.value;
        }

        [[nodiscard]] T operator*() &&
        {
            std::unique_lock l(mutex_);
            return static_cast<T&&>(value_.value);
        }

        [[nodiscard]] std::shared_mutex& mutex() const noexcept { return mutex_; }

    private:
        struct locked_unique
        {
            std::unique_lock<std::shared_mutex> lock;
            T& value;

            T* operator->() noexcept { return std::addressof(value); }
        };

        struct locked_shared
        {
            std::shared_lock<std::shared_mutex> lock;
            const T& value;

            const T* operator->() const noexcept { return std::addressof(value); }
        };

    public:
        [[nodiscard]] locked_unique lock()
        {
            return {
                std::unique_lock(mutex_),
                value_.value
            };
        }

        [[nodiscard]] locked_shared lock_shared() const
        {
            return {
                std::shared_lock(mutex_),
                value_.value
            };
        }

        [[nodiscard]] locked_unique operator->() { return lock(); }
        [[nodiscard]] locked_shared operator->() const { return lock_shared(); }

        template <std::invocable<T&> F>
        std::invoke_result_t<F, T&> invoke_with_lock(F&& func)
        {
            std::unique_lock l(mutex_);
            return std::invoke(static_cast<F&&>(func), value_.value);
        }

        template <std::invocable<const T&> F>
        std::invoke_result_t<F, const T&> invoke_with_shared_lock(F&& func) const
        {
            std::shared_lock l(mutex_);
            return std::invoke(static_cast<F&&>(func), value_.value);
        }

    private:
        union data_t
        {
            char dummy;
            T value;

            data_t() noexcept: dummy() {}
            ~data_t() noexcept {}

            // @formatter:off
            template <typename... Ts>
            void construct(Ts&&... args)
                noexcept(std::is_nothrow_constructible_v<T, Ts...>)
            {
                new(std::addressof(value)) T(static_cast<Ts&&>(args)...);
            }
            // @formatter:on

            void destruct() noexcept { value.~T(); }
        } value_;

        mutable std::shared_mutex mutex_;
    };

    template <typename T>
    locked(T) -> locked<T>;

    class spinlock
    {
    public:
        void lock() noexcept
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

        void unlock() noexcept
        {
            locked_.clear(std::memory_order::release);
        }

    private:
        static constexpr int spin_count = 20;

        std::atomic_flag locked_;
    };

    template <typename T>
    class locked_ptr
    {
        static_assert(alignof(T) >= 4, "Spare bits of the pointer are needed for tagging");
    public:
        constexpr locked_ptr() noexcept = default;
        explicit(false) locked_ptr(T* ptr) noexcept:
            value_(reinterpret_cast<std::uintptr_t>(ptr)) {}

        T* lock_and_load() noexcept
        {
            auto value = value_.load(std::memory_order::relaxed);
            while (true)
                switch (value & mask)
                {
                    case not_locked:
                    {
                        // No one is holding the lock, acquire it
                        const auto locked_value = value | locked_no_notify;
                        if (value_.compare_exchange_weak(value, locked_value,
                            std::memory_order::acquire, std::memory_order::relaxed))
                            return reinterpret_cast<T*>(value); // NOLINT(performance-no-int-to-ptr)
                        break;
                    }
                    case locked_no_notify:
                    {
                        // No one was waiting, change the state so that
                        // we could be notified
                        const auto notify_value = (value & ~mask) | locked_should_notify;
                        if (!value_.compare_exchange_weak(value, notify_value,
                            std::memory_order::relaxed))
                            break;
                        [[fallthrough]];
                    }
                    case locked_should_notify:
                    {
                        // Join the wait list
                        value_.wait(value, std::memory_order::relaxed);
                        value = value_.load(std::memory_order::relaxed);
                        break;
                    }
                    default: unreachable();
                }
        }

        void store_and_unlock(T* ptr) noexcept
        {
            const auto old = value_.exchange(
                reinterpret_cast<std::uintptr_t>(ptr), std::memory_order::release);
            if ((old & mask) == locked_should_notify)
                value_.notify_all();
        }

        T* unsafe_load_relaxed() noexcept
        {
            const auto value = value_.load(std::memory_order::relaxed);
            return reinterpret_cast<T*>(value & ~mask); // NOLINT(performance-no-int-to-ptr)
        }

    private:
        static constexpr std::uintptr_t not_locked = 0;
        static constexpr std::uintptr_t locked_no_notify = 1;
        static constexpr std::uintptr_t locked_should_notify = 2;
        static constexpr std::uintptr_t mask = 3;

        std::atomic_uintptr_t value_;
    };
}
