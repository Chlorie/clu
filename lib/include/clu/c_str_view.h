#pragma once

#include <limits>
#include <string>

#include "string_utils.h"

namespace clu
{
    /**
     * \brief A string view type for null-terminated strings
     * \tparam Char The character type
     */
    template <typename Char>
    class basic_c_str_view
    {
    private:
        static constexpr Char null = static_cast<Char>(0);
        static constexpr Char empty_string[1]{ null };

        const Char* ptr_ = empty_string;

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

        /// Sentinel type
        struct sentinel
        {
            /// Comparison with character pointer, compares equal when the character is null
            [[nodiscard]] constexpr bool operator==(const Char* ptr) const { return *ptr == null; }
        };

        constexpr basic_c_str_view() noexcept = default;
        constexpr explicit(false) basic_c_str_view(const Char* ptr) noexcept: ptr_(ptr) {}
        constexpr explicit(false) basic_c_str_view(const std::basic_string<Char>& str) noexcept: ptr_(str.c_str()) {}

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

        [[nodiscard]] constexpr iterator begin() const noexcept { return ptr_; }
        [[nodiscard]] constexpr sentinel end() const noexcept { return {}; }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return ptr_; }
        [[nodiscard]] constexpr sentinel cend() const noexcept { return {}; }

        [[nodiscard]] constexpr const_reference operator[](const size_t pos) const noexcept { return ptr_[pos]; }
        [[nodiscard]] constexpr const_reference front() const noexcept { return *ptr_; } ///< Get the first character
        [[nodiscard]] constexpr const Char* data() const noexcept { return ptr_; } ///< Get a pointer to the C-string
        [[nodiscard]] constexpr const Char* c_str() const noexcept { return ptr_; } ///< Get a pointer to the C-string

        /// Get the length of the string, excluding the null terminator
        /// \remark This function is not \inlrst :math:`O(1)`. \endrst It calls ``strlen``.
        [[nodiscard]] constexpr size_t size() const noexcept { return clu::strlen(ptr_); }
        [[nodiscard]] constexpr size_t length() const noexcept { return size(); } ///< Same as ``size()``
        [[nodiscard]] constexpr static size_t max_size() noexcept { return std::numeric_limits<size_t>::max(); }
        [[nodiscard]] constexpr bool empty() const noexcept { return *ptr_ == null; } ///< Check if the view is empty

        constexpr void remove_prefix(const size_t n) noexcept { ptr_ += n; } ///< Remove prefix with a certain length from the view
        constexpr void swap(basic_c_str_view& other) noexcept { std::swap(ptr_, other.ptr_); } ///< Swap two views
        constexpr friend void swap(basic_c_str_view& lhs, basic_c_str_view& rhs) noexcept { lhs.swap(rhs); } ///< Swap two views

        /// Convert this null-terminated view into a regular ``std::basic_string_view<Char>``.
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
