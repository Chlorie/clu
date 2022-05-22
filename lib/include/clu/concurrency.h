#pragma once

#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <functional>

#include "concepts.h"
#include "meta_algorithm.h"

namespace clu
{
    template <typename T, lockable L = std::shared_mutex>
    class locked
    {
    public:
        using unique_lock = std::unique_lock<L>;
        using shared_lock = meta::invoke<
            conditional_t<shared_lockable<L>, meta::quote1<std::shared_lock>, meta::quote1<std::unique_lock>>, L>;

        locked() noexcept(std::is_nothrow_default_constructible_v<T>) requires std::default_initializable<T>
        {
            value_.construct();
        }

        explicit locked(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>) { value_.construct(value); }

        explicit locked(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
        {
            value_.construct(static_cast<T&&>(value));
        }

        template <typename... Ts>
        explicit locked(std::in_place_t, Ts&&... args) noexcept(std::is_nothrow_constructible_v<T, Ts...>)
        {
            value_.construct(static_cast<Ts&&>(args)...);
        }

        locked(const locked& other)
        {
            shared_lock l(other.mutex_);
            value_.construct(other.value_.value);
        }

        locked(locked&& other) noexcept(false)
        {
            unique_lock l(other.mutex_);
            value_.construct(static_cast<T&&>(other.value_.value));
        }

        ~locked() noexcept { value_.destruct(); }

        locked& operator=(const locked& other)
        {
            unique_lock l1(mutex_, std::defer_lock);
            shared_lock l2(other.mutex_, std::defer_lock);
            std::lock(l1, l2);
            value_.value = other.value_.value;
            return *this;
        }

        locked& operator=(locked&& other) noexcept(false)
        {
            unique_lock l1(mutex_, std::defer_lock);
            unique_lock l2(other.mutex_, std::defer_lock);
            std::lock(l1, l2);
            value_.value = static_cast<T&&>(other.value_.value);
            return *this;
        }

        void swap(locked& other) noexcept(false)
        {
            unique_lock l1(mutex_, std::defer_lock);
            unique_lock l2(other.mutex_, std::defer_lock);
            std::lock(l1, l2);
            using std::swap;
            swap(value_.value, other.value_.value);
        }

        friend void swap(locked& lhs, locked& rhs) noexcept(false) { lhs.swap(rhs); }

        [[nodiscard]] T operator*() const&
        {
            shared_lock l(mutex_);
            return value_.value;
        }

        [[nodiscard]] T operator*() &&
        {
            unique_lock l(mutex_);
            return static_cast<T&&>(value_.value);
        }

        [[nodiscard]] L& mutex() const noexcept { return mutex_; }

    private:
        struct locked_unique
        {
            unique_lock lock;
            T& value;

            T* operator->() noexcept { return std::addressof(value); }
        };

        struct locked_shared
        {
            shared_lock lock;
            const T& value;

            const T* operator->() const noexcept { return std::addressof(value); }
        };

    public:
        [[nodiscard]] locked_unique lock() { return {unique_lock(mutex_), value_.value}; }
        [[nodiscard]] locked_shared lock_shared() const { return {shared_lock(mutex_), value_.value}; }

        [[nodiscard]] locked_unique operator->() { return lock(); }
        [[nodiscard]] locked_shared operator->() const { return lock_shared(); }

        template <std::invocable<T&> F>
        std::invoke_result_t<F, T&> invoke_with_lock(F&& func)
        {
            unique_lock l(mutex_);
            return std::invoke(static_cast<F&&>(func), value_.value);
        }

        template <std::invocable<const T&> F>
        std::invoke_result_t<F, const T&> invoke_with_shared_lock(F&& func) const
        {
            shared_lock l(mutex_);
            return std::invoke(static_cast<F&&>(func), value_.value);
        }

        [[nodiscard]] T& get_without_lock() noexcept { return value_.value; }
        [[nodiscard]] const T& get_without_lock() const noexcept { return value_.value; }

    private:
        union data_t
        {
            char dummy;
            T value;

            data_t() noexcept: dummy() {}
            ~data_t() noexcept {}

            template <typename... Ts>
            void construct(Ts&&... args) noexcept(std::is_nothrow_constructible_v<T, Ts...>)
            {
                new (std::addressof(value)) T(static_cast<Ts&&>(args)...);
            }

            void destruct() noexcept { value.~T(); }
        } value_;

        mutable L mutex_;
    };

    template <typename T>
    locked(T) -> locked<T>;

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

    namespace detail::lck_ptr
    {
        void* lock_and_load(std::atomic_uintptr_t& value) noexcept;
        void store_and_unlock(std::atomic_uintptr_t& value, void* ptr) noexcept;
    } // namespace detail::lck_ptr

    template <typename T>
    class locked_ptr
    {
        static_assert(alignof(T) >= 4, "Spare bits of the pointer are needed for tagging");

    public:
        constexpr locked_ptr() noexcept = default;
        explicit(false) locked_ptr(T* ptr) noexcept: value_(reinterpret_cast<std::uintptr_t>(ptr)) {}

        T* lock_and_load() noexcept { return static_cast<T*>(detail::lck_ptr::lock_and_load(value_)); }

        void store_and_unlock(T* ptr) noexcept { detail::lck_ptr::store_and_unlock(value_, ptr); }

        T* unsafe_load_relaxed() noexcept
        {
            static constexpr std::uintptr_t mask = 3;
            const auto value = value_.load(std::memory_order::relaxed);
            return reinterpret_cast<T*>(value & ~mask); // NOLINT(performance-no-int-to-ptr)
        }

    private:
        std::atomic_uintptr_t value_;
    };
} // namespace clu
