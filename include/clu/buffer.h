#pragma once

#include <span>

#include "concepts.h"

namespace clu
{
    template <typename T>
    concept alias_safe = same_as_any_of<T, unsigned char, char, std::byte>;

    // @formatter:off
    template <typename T>
    concept buffer_safe = trivially_copyable<T> ||
        (std::is_array_v<T> && trivially_copyable<std::remove_all_extents_t<T>>);

    template <typename T>
    concept trivial_range = 
        std::ranges::contiguous_range<T> &&
        std::ranges::sized_range<T> &&
        trivially_copyable<std::ranges::range_value_t<T>>;
    // @formatter:on

    template <typename T>
    concept mutable_trivial_range = trivial_range<T> && !std::is_const_v<std::ranges::range_value_t<T>>;

    template <typename T>
    class [[nodiscard]] basic_buffer final
    {
        static_assert(alias_safe<std::remove_const_t<T>>);

    private:
        T* ptr_ = nullptr;
        size_t size_ = 0;

    public:
        constexpr basic_buffer() noexcept = default;
        constexpr basic_buffer(T* ptr, const size_t size) noexcept: ptr_(ptr), size_(size) {}

        // @formatter:off
        template <typename R> requires (
            (std::is_const_v<T> && trivial_range<R>) || 
            (!std::is_const_v<T> && mutable_trivial_range<R>))
        explicit(false) basic_buffer(R&& range) noexcept:
            ptr_(reinterpret_cast<T*>(std::ranges::data(range))),
            size_(std::ranges::size(range) * sizeof(std::ranges::range_value_t<R>)) {}
        
        constexpr explicit(false) basic_buffer(const basic_buffer<std::remove_const_t<T>>& buffer) noexcept
            requires std::is_const_v<T>: ptr_(buffer.ptr()), size_(buffer.size()) {}
        // @formatter:on

        [[nodiscard]] constexpr T* data() const noexcept { return ptr_; }
        [[nodiscard]] constexpr size_t size() const noexcept { return size_; }
        [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
        constexpr void remove_prefix(const size_t size) noexcept { ptr_ += size; }
        constexpr void remove_suffix(const size_t size) noexcept { size_ -= size; }
        [[nodiscard]] constexpr T& operator[](const size_t index) noexcept { return ptr_[index]; }
        [[nodiscard]] constexpr basic_buffer first(const size_t size) const noexcept { return { ptr_, size }; }
        [[nodiscard]] constexpr basic_buffer last(const size_t size) const noexcept { return { ptr_ + (size_ - size), size }; }

        [[nodiscard]] constexpr basic_buffer& operator+=(const size_t size) noexcept
        {
            ptr_ += size;
            return *this;
        }
        [[nodiscard]] friend constexpr basic_buffer operator+(basic_buffer buffer, const size_t size) noexcept { return buffer += size; }

        [[nodiscard]] constexpr std::span<T> as_span() const noexcept { return { ptr_, size_ }; }
    };

    using mutable_buffer = basic_buffer<std::byte>;
    using const_buffer = basic_buffer<const std::byte>;

    template <buffer_safe T>
    mutable_buffer trivial_buffer(T& value) noexcept
    {
        return { reinterpret_cast<std::byte*>(std::addressof(value)), sizeof(T) };
    }

    template <buffer_safe T>
    const_buffer trivial_buffer(const T& value) noexcept
    {
        return { reinterpret_cast<const std::byte*>(std::addressof(value)), sizeof(T) };
    }

    template <typename T> void trivial_buffer(const T&&) = delete;
}
