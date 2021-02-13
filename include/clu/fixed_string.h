#pragma once

#include <cstddef>

namespace clu
{
    template <typename T, size_t N>
    struct fixed_string
    {
        T data[N + 1];

        constexpr const T* begin() const noexcept { return data; }
        constexpr const T* end() const noexcept { return data + N; }
        constexpr T operator[](const size_t index) const noexcept { return data[index]; }
        constexpr size_t size() const noexcept { return N; }
        constexpr bool operator==(const fixed_string&) const noexcept = default;
        constexpr auto operator<=>(const fixed_string&) const noexcept = default;
    };

    template <typename T, size_t N>
    fixed_string(const T (&)[N]) -> fixed_string<T, N - 1>;
}
