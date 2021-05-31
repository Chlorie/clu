#pragma once

#include <string>

#include "string_utils.h"

namespace clu
{
    class c_str_view final
    {
    private:
        struct sentinel
        {
            [[nodiscard]] constexpr bool operator==(const char* ptr) const { return *ptr == '\0'; }
        };

        using pointer = const char*;
        using const_pointer = const char*;
        using reference = const char&;
        using const_reference = const char&;
        using iterator = const char*;
        using const_iterator = const char*;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using value_type = char;

        const char* ptr_ = "";

        constexpr const char* one_past_last() const noexcept { return ptr_ + size(); }

    public:
        constexpr c_str_view() noexcept = default;
        constexpr explicit(false) c_str_view(const char* ptr) noexcept: ptr_(ptr) {}
        constexpr explicit(false) c_str_view(const std::string& str) noexcept: ptr_(str.c_str()) {}
        explicit(false) c_str_view(const std::string&&) = delete;

        constexpr c_str_view& operator=(const char* ptr) noexcept
        {
            ptr_ = ptr;
            return *this;
        }

        constexpr c_str_view& operator=(const std::string& str) noexcept
        {
            ptr_ = str.c_str();
            return *this;
        }

        constexpr c_str_view& operator=(const std::string&&) = delete;

        [[nodiscard]] constexpr iterator begin() const noexcept { return ptr_; }
        [[nodiscard]] constexpr sentinel end() const noexcept { return {}; }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return ptr_; }
        [[nodiscard]] constexpr sentinel cend() const noexcept { return {}; }

        [[nodiscard]] constexpr const_reference operator[](const size_t pos) const noexcept { return ptr_[pos]; }
        [[nodiscard]] constexpr const_reference front() const noexcept { return *ptr_; }
        [[nodiscard]] constexpr const char* data() const noexcept { return ptr_; }
        [[nodiscard]] constexpr const char* c_str() const noexcept { return ptr_; }

        [[nodiscard]] constexpr size_t size() const noexcept { return strlen(ptr_); }
        [[nodiscard]] constexpr size_t length() const noexcept { return size(); }
        [[nodiscard]] constexpr static size_t max_size() noexcept { return std::numeric_limits<size_t>::max(); }
        [[nodiscard]] constexpr bool empty() const noexcept { return *ptr_ == '\0'; }

        constexpr void remove_prefix(const size_t n) noexcept { ptr_ += n; }
        constexpr void swap(c_str_view& other) noexcept { std::swap(ptr_, other.ptr_); }
        constexpr friend void swap(c_str_view& lhs, c_str_view& rhs) noexcept { lhs.swap(rhs); }

        [[nodiscard]] constexpr explicit operator std::string_view() const noexcept { return { ptr_ }; }
    };
}

template <> inline constexpr bool std::ranges::enable_view<clu::c_str_view> = true;
template <> inline constexpr bool std::ranges::enable_borrowed_range<clu::c_str_view> = true;
template <> inline constexpr bool std::ranges::disable_sized_range<clu::c_str_view> = true;
