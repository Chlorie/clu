#pragma once

#include <atomic>

namespace clu
{
    namespace detail::lck_ptr
    {
        void* lock_and_load(std::atomic_uintptr_t& value) noexcept;
        void store_and_unlock(std::atomic_uintptr_t& value, void* ptr) noexcept;
    } // namespace detail::lck_ptr

    template <typename T>
    class locked_ptr
    {
    public:
        constexpr locked_ptr() noexcept = default;
        explicit(false) locked_ptr(T* ptr) noexcept: value_(reinterpret_cast<std::uintptr_t>(ptr)) {}

        T* lock_and_load() noexcept
        {
            // Move the static assert here to allow using an incomplete T
            static_assert(alignof(T) >= 4, //
                "Spare bits of the pointer are needed for tagging. "
                "The type T should be complete at the point of calling this function.");
            return static_cast<T*>(detail::lck_ptr::lock_and_load(value_));
        }

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
