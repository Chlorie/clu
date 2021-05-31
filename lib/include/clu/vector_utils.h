#pragma once

#include <vector>

namespace clu
{
    template <typename T, typename... Ts>
        requires (std::constructible_from<T, Ts&&> && ...)
    [[nodiscard]] std::vector<T> make_vector(Ts&&... list)
    {
        std::vector<T> vec;
        vec.reserve(sizeof...(list));
        (vec.emplace_back(std::forward<Ts>(list)), ...);
        return vec;
    }
}
