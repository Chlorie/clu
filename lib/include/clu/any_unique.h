#pragma once

#include <utility>

namespace clu
{
    class any_unique
    {
    public:
        constexpr any_unique() noexcept = default;

        constexpr any_unique(const any_unique&) = delete;
        constexpr any_unique(any_unique&& other) noexcept:
            ptr_(std::exchange(other.ptr_, nullptr)),
            deleter_(std::exchange(other.deleter_, nullptr)) {}

        constexpr any_unique& operator=(const any_unique&) = delete;
        constexpr any_unique& operator=(any_unique&& other) noexcept
        {
            if (&other == this) return *this;
            if (deleter_) deleter_(ptr_);
            ptr_ = std::exchange(other.ptr_, nullptr);
            deleter_ = std::exchange(other.deleter_, nullptr);
            return *this;
        }

        template <typename T>
        constexpr explicit(false) any_unique(T&& value):
            ptr_(new T(static_cast<T&&>(value))),
            deleter_(deleter_for<T>) {}

        template <typename T, typename... Args>
        constexpr explicit any_unique(std::in_place_type_t<T>, Args&&... args):
            ptr_(new T(static_cast<Args&&>(args)...)),
            deleter_(deleter_for<T>) {}

        template <typename T, typename U, typename... Args>
        constexpr explicit any_unique(std::in_place_type_t<T>,
            const std::initializer_list<U> ilist, Args&&... args):
            ptr_(new T(ilist, static_cast<Args&&>(args)...)),
            deleter_(deleter_for<T>) {}

        constexpr ~any_unique() noexcept { if (deleter_) deleter_(ptr_); }

        constexpr void swap(any_unique& other) noexcept
        {
            std::swap(ptr_, other.ptr_);
            std::swap(deleter_, other.deleter_);
        }
        constexpr friend void swap(any_unique& lhs, any_unique& rhs) noexcept { lhs.swap(rhs); }

        [[nodiscard]] constexpr bool has_value() const noexcept { return ptr_ != nullptr; }

        constexpr void reset() noexcept
        {
            if (deleter_)
                deleter_(ptr_);
            ptr_ = nullptr;
            deleter_ = nullptr;
        }

        template <typename T, typename... Args>
        std::decay_t<T>& emplace(std::in_place_type_t<T>, Args&&... args)
        {
            reset();
            T* ptr = new T(static_cast<Args&&>(args)...);
            ptr_ = ptr;
            deleter_ = deleter_for<T>;
            return *ptr;
        }

        template <typename T, typename U, typename... Args>
        std::decay_t<T>& emplace(std::in_place_type_t<T>, 
            const std::initializer_list<U> ilist, Args&&... args)
        {
            reset();
            T* ptr = new T(ilist, static_cast<Args&&>(args)...);
            ptr_ = ptr;
            deleter_ = deleter_for<T>;
            return *ptr;
        }

    private:
        void* ptr_ = nullptr;
        void (* deleter_)(void*) noexcept = nullptr;

        template <typename T>
        static void deleter_for(T* ptr) noexcept { delete static_cast<T*>(ptr); }
    };
}
