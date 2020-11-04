#pragma once

#include <concepts>

#include "concepts.h"

namespace clu
{
    template <typename T>
    concept char_type = same_as_any_of<T, char, unsigned char, signed char, char8_t, char16_t, char32_t>;

    template <typename T>
    concept char_pointer = std::is_pointer_v<T> && char_type<std::remove_cv_t<std::remove_pointer_t<T>>>;

    template <typename T>
    concept string_view_like = requires(const T& str)
    {
        { str.length() } -> std::convertible_to<size_t>;
        { str.data() } -> char_pointer;
    };

    template <char_type T, size_t N>
    constexpr size_t strlen(const T [N]) { return N - 1; }

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
