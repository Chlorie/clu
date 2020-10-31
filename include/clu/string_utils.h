#pragma once

#include <concepts>

namespace clu
{
    // @formatter:off
    template <typename T>
    concept char_type =
        std::same_as<T, char> ||
        std::same_as<T, unsigned char> ||
        std::same_as<T, signed char> ||
        std::same_as<T, char8_t> ||
        std::same_as<T, char16_t> ||
        std::same_as<T, char32_t>;
    // @formatter:on

    template <typename T>
    concept char_pointer = std::is_pointer_v<T> && char_type<std::remove_pointer_t<T>>;

    template <typename T>
    concept string_view_like = requires(const T& str)
    {
        { str.length() } -> std::convertible_to<size_t>;
        { str.data() } -> char_pointer;
    };

    template <char_type T, size_t N>
    constexpr size_t strlen(const T (&)[N]) { return N - 1; }

    template <char_type T>
    constexpr size_t strlen(const T* str)
    {
        const T* ptr = str;
        while (*ptr != T(0)) ++ptr;
        return ptr - str;
    }

    template <string_view_like T>
    constexpr size_t strlen(const T& str) { return str.length(); }
}
