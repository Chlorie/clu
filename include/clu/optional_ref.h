#pragma once

#include <optional>

namespace clu
{
    template <typename T>
    class optional_ref final
    {
    private:
        T* ptr_ = nullptr;

    public:
        constexpr optional_ref() noexcept = default;
        constexpr explicit(false) optional_ref(std::nullopt_t) noexcept {}
        constexpr explicit(false) optional_ref(std::nullptr_t) noexcept {}
        constexpr explicit(false) optional_ref(T& reference) noexcept: ptr_(std::addressof(reference)) {}

        constexpr explicit(false) optional_ref(const std::optional<std::remove_const_t<T>>& opt) noexcept
            requires std::is_const_v<T>:
            ptr_(opt ? std::addressof(*opt) : nullptr) {}

        constexpr explicit(false) optional_ref(std::optional<T>& opt) noexcept
            requires (!std::is_const_v<T>):
            ptr_(opt ? std::addressof(*opt) : nullptr) {}

        constexpr optional_ref& operator=(std::nullopt_t) noexcept
        {
            ptr_ = nullptr;
            return *this;
        }

        constexpr optional_ref& operator=(std::nullptr_t) noexcept
        {
            ptr_ = nullptr;
            return *this;
        }

        constexpr optional_ref& operator=(T& reference) noexcept
        {
            ptr_ = std::addressof(reference);
            return *this;
        }

        constexpr T* operator->() const noexcept { return ptr_; }
        constexpr T& operator*() const noexcept { return *ptr_; }

        constexpr explicit operator bool() const noexcept { return ptr_ != nullptr; }
        constexpr bool has_value() const noexcept { return ptr_ != nullptr; }

        constexpr T& value() const
        {
            if (ptr_) return *ptr_;
            throw std::bad_optional_access();
        }

        constexpr T& value_or(T& default_value) const noexcept { return ptr_ ? *ptr_ : default_value; }

        constexpr void swap(optional_ref& other) noexcept { std::swap(ptr_, other.ptr_); }
        friend constexpr void swap(optional_ref& lhs, optional_ref& rhs) noexcept { lhs.swap(rhs); }

        constexpr void reset() noexcept { ptr_ = nullptr; }
        constexpr T& emplace(T& reference) { return *(ptr_ = std::addressof(reference)); }
    };

    template <typename T> using optional_param = optional_ref<const T>;
}
