#pragma once

#include <string>

#include "string_utils.h"

namespace clu
{
    template <typename Char>
    class basic_c_str_view
    {
    private:
        const Char* ptr_ = "";

        constexpr const Char* one_past_last() const noexcept { return ptr_ + size(); }

    public:
        using pointer = const Char*;
        using const_pointer = const Char*;
        using reference = const Char&;
        using const_reference = const Char&;
        using iterator = const Char*;
        using const_iterator = const Char*;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using value_type = Char;

        struct sentinel
        {
            [[nodiscard]] constexpr bool operator==(const Char* ptr) const { return *ptr == '\0'; }
        };

        constexpr basic_c_str_view() noexcept = default;
        constexpr explicit(false) basic_c_str_view(const Char* ptr) noexcept: ptr_(ptr) {}
        constexpr explicit(false) basic_c_str_view(const std::basic_string<Char>& str) noexcept: ptr_(str.c_str()) {}
        explicit(false) basic_c_str_view(const std::basic_string<Char>&&) = delete;

        constexpr basic_c_str_view& operator=(const Char* ptr) noexcept
        {
            ptr_ = ptr;
            return *this;
        }

        constexpr basic_c_str_view& operator=(const std::basic_string<Char>& str) noexcept
        {
            ptr_ = str.c_str();
            return *this;
        }

        basic_c_str_view& operator=(const std::basic_string<Char>&&) = delete;

        [[nodiscard]] constexpr iterator begin() const noexcept { return ptr_; }
        [[nodiscard]] constexpr sentinel end() const noexcept { return {}; }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return ptr_; }
        [[nodiscard]] constexpr sentinel cend() const noexcept { return {}; }

        [[nodiscard]] constexpr const_reference operator[](const size_t pos) const noexcept { return ptr_[pos]; }
        [[nodiscard]] constexpr const_reference front() const noexcept { return *ptr_; }
        [[nodiscard]] constexpr const Char* data() const noexcept { return ptr_; }
        [[nodiscard]] constexpr const Char* c_str() const noexcept { return ptr_; }

        [[nodiscard]] constexpr size_t size() const noexcept { return clu::strlen(ptr_); }
        [[nodiscard]] constexpr size_t length() const noexcept { return size(); }
        [[nodiscard]] constexpr static size_t max_size() noexcept { return std::numeric_limits<size_t>::max(); }
        [[nodiscard]] constexpr bool empty() const noexcept { return *ptr_ == '\0'; }

        constexpr void remove_prefix(const size_t n) noexcept { ptr_ += n; }
        constexpr void swap(basic_c_str_view& other) noexcept { std::swap(ptr_, other.ptr_); }
        constexpr friend void swap(basic_c_str_view& lhs, basic_c_str_view& rhs) noexcept { lhs.swap(rhs); }

        [[nodiscard]] constexpr explicit operator std::basic_string_view<Char>() const noexcept { return { ptr_ }; }
    };

    using c_str_view = basic_c_str_view<char>;
    using c_wstr_view = basic_c_str_view<wchar_t>;
    using c_u8str_view = basic_c_str_view<char8_t>;
    using c_u16str_view = basic_c_str_view<char16_t>;
    using c_u32str_view = basic_c_str_view<char32_t>;
}

template <typename Char> inline constexpr bool std::ranges::enable_view<clu::basic_c_str_view<Char>> = true;
template <typename Char> inline constexpr bool std::ranges::enable_borrowed_range<clu::basic_c_str_view<Char>> = true;
template <typename Char> inline constexpr bool std::ranges::disable_sized_range<clu::basic_c_str_view<Char>> = true;
