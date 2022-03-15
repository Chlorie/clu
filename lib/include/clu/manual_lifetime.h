#pragma once

#include <type_traits>
#include <utility>

namespace clu
{
    /// Offers utilities for controlling lifetime of an object manually.
    template <typename T>
    class manual_lifetime
    {
    private:
        union
        {
            T value_;
        };

    public:
        manual_lifetime() noexcept {} //< No-op constructor.
        ~manual_lifetime() noexcept {} //< No-op destructor.
        manual_lifetime(const manual_lifetime&) = delete;
        manual_lifetime(manual_lifetime&&) = delete;
        manual_lifetime& operator=(const manual_lifetime&) = delete;
        manual_lifetime& operator=(manual_lifetime&&) = delete;

        template <typename... Args>
        T& construct(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args&&...>)
        {
            return *new (std::addressof(value_)) T(std::forward<Args>(args)...);
        }

        void destruct() noexcept { value_.~T(); } //< Destruct the controlled object.

        [[nodiscard]] T& get() & noexcept { return value_; }
        [[nodiscard]] const T& get() const& noexcept { return value_; }
        [[nodiscard]] T&& get() && noexcept { return std::move(value_); }
        [[nodiscard]] const T&& get() const&& noexcept { return std::move(value_); }
    };

    template <typename T>
    class manual_lifetime<T&>
    {
    private:
        T* ptr_ = nullptr;

    public:
        manual_lifetime() noexcept = default;
        ~manual_lifetime() noexcept = default;
        manual_lifetime(const manual_lifetime&) = delete;
        manual_lifetime(manual_lifetime&&) = delete;
        manual_lifetime& operator=(const manual_lifetime&) = delete;
        manual_lifetime& operator=(manual_lifetime&&) = delete;

        T& construct(T& ref) noexcept { return *(ptr_ = std::addressof(ref)); }
        void destruct() noexcept {}

        [[nodiscard]] T& get() const noexcept { return *ptr_; }
    };

    template <typename T>
    class manual_lifetime<T&&>
    {
    private:
        T* ptr_ = nullptr;

    public:
        manual_lifetime() noexcept = default;
        ~manual_lifetime() noexcept = default;
        manual_lifetime(const manual_lifetime&) = delete;
        manual_lifetime(manual_lifetime&&) = delete;
        manual_lifetime& operator=(const manual_lifetime&) = delete;
        manual_lifetime& operator=(manual_lifetime&&) = delete;

        T&& construct(T&& ref) noexcept { return std::move(*(ptr_ = std::addressof(ref))); }
        void destruct() noexcept {}

        [[nodiscard]] T&& get() const noexcept { return std::move(*ptr_); }
    };

    template <>
    class manual_lifetime<void>
    {
    public:
        manual_lifetime() noexcept = default;
        ~manual_lifetime() noexcept = default;
        manual_lifetime(const manual_lifetime&) = delete;
        manual_lifetime(manual_lifetime&&) = delete;
        manual_lifetime& operator=(const manual_lifetime&) = delete;
        manual_lifetime& operator=(manual_lifetime&&) = delete;

        void construct() noexcept {}
        void destruct() noexcept {}
        void get() const noexcept {}
    };
} // namespace clu
